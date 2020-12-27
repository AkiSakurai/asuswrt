/**
 * @file
 * @brief
 *
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id$
 * <<Broadcom-WL-IPTag/Proprietary:>>
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <typedefs.h>
#include <limits.h>

#include <bcmdefs.h>
#include "acsd_config.h"
#include "acsd_svr.h"

struct entry_s {
	 char *key;
	 char *value;
	 struct entry_s *next;
};

typedef struct entry_s entry_t;

struct acsd_hashtable_s {
	 int size;
	 struct entry_s **table;
};

typedef struct acsd_hashtable_s acsd_hashtable_t;
static acsd_hashtable_t *hashtable = NULL;

static bool acsd_make_hash();
static char* acsd_ht_get(acsd_hashtable_t *hashtable, const char *key);
static void acsd_ht_set(acsd_hashtable_t *hashtable, char *key, char *value);
static entry_t *acsd_ht_newpair(char *key, char *value);
static unsigned int acsd_ht_hash(acsd_hashtable_t *hashtable, const char *key);
static acsd_hashtable_t* acsd_ht_create(unsigned int size);

/* Create a new hashtable. */
static acsd_hashtable_t*
acsd_ht_create(unsigned int size)
{
	 unsigned int i;

	 ACSD_DEBUG("hashtable create \n");

	 /* Allocate the table itself. */
	 if ((hashtable =  malloc(sizeof(acsd_hashtable_t))) == NULL) {
		 ACSD_ERROR("Failed to get Memory for Hashtable struct \n");
		 return NULL;
	 }

	 /* Allocate pointers to the head nodes. */
	 if ((hashtable->table =  malloc(sizeof(entry_t *) *  size)) == NULL) {
		 ACSD_ERROR("Failed to get Memory for Index table \n");
		 free(hashtable);
		 return NULL;
	 }

	 for (i = 0; i < size; i++) {
		 hashtable->table[i] = NULL;
	 }

	 hashtable->size = size;

	 return hashtable;
}

/* Hash a string for a particular hash table. */
static unsigned int
acsd_ht_hash(acsd_hashtable_t *hashtable, const char *key)
{
	 unsigned int value = 0;
	 unsigned int hashval = 0;

	 while (*key)
		 value = (31 * value) + (*key++);

	 hashval =  value & HASH_MAX_ENTRY_MASK;

	 return hashval;
}

/* Create a key-value pair. */
static entry_t
*acsd_ht_newpair(char *key, char *value)
{
	 entry_t *newpair;

	 if ((newpair = (entry_t*)malloc(sizeof(entry_t))) == NULL) {
		 return NULL;
	 }

	 if ((newpair->key = strdup(key)) == NULL) {
		goto cleanup;
	 }

	 if ((newpair->value = strdup(value)) == NULL) {
		free(newpair->key);
		goto cleanup;
	 }

	 newpair->next = NULL;
	 return newpair;

cleanup:
	free(newpair);
	return NULL;
}

/* Insert a key-value pair into a hash table. */
static void
acsd_ht_set(acsd_hashtable_t *hashtable, char *key, char *value)
{
	 unsigned int index = 0;
	 entry_t *newpair = NULL;
	 entry_t *next = NULL;
	 entry_t *last = NULL;
	 unsigned int key_len = 0;

	 index = acsd_ht_hash(hashtable, key);
	 next = hashtable->table[index];
	 key_len = strlen(key);

	 ACSD_INFO("key[%s] value[%s] \n", key, value);

	 /* Traverse the linklist pair	till next is NULL */
	 while ((next != NULL) && (next->key != NULL) && (strncmp(key, next->key, key_len) > 0)) {
		 last = next;
		 next = next->next;
	 }

	 /* In case there are 2 separate values of the same string then replace the string  */
	 if ((next != NULL) && (next->key != NULL) && (strncmp(key, next->key, key_len) == 0)) {
		 free(next->value);
		 ACSD_DEBUG("Duplicate String found so replacing the value of key[%s]\n", key);
		 next->value = strdup(value);
	 }
	 else {
		/* Insert the newpair */
		 newpair = acsd_ht_newpair(key, value);

		 if (next == hashtable->table[index]) {
			/* Add first element at the start of Index  */
			 ACSD_INFO("Inserting element at the START \n");
			 newpair->next = next;
			 hashtable->table[index] = newpair;
		 }
		 else if (next == NULL) {
			/* Add the element at the end of the Linklist. */
			 ACSD_INFO("Inserting element at the END \n");
			 last->next = newpair;
		 }
		 else {
			 /* In case the strncmp < 0 then add the Node at the middle of the list. */
			 ACSD_INFO("Inserting element at the MIDDLE \n");
			 newpair->next = next;
			 last->next = newpair;
		 }
	 }
}

/* Retrieve a key-value pair from a hash table. */
static char*
acsd_ht_get(acsd_hashtable_t *hashtable, const char *key)
{
	 unsigned int index = 0;
	 entry_t *pair;
	 unsigned int key_len = 0;

	 key_len = strlen(key);
	 index = acsd_ht_hash(hashtable, key);
	 ACSD_DEBUG(" key[%s] \n", key);

	 /* Step through the index, looking for our value. */
	 pair = hashtable->table[index];
	 while ((pair != NULL) && (pair->key != NULL) && (strncmp(key, pair->key, key_len) > 0)) {
		 pair = pair->next;
	 }

	 /* No pair found with given string */
	 if ((pair == NULL) || (pair->key == NULL) || (strncmp(key, pair->key, key_len) != 0)) {
		 ACSD_ERROR("Check the key no element found in the file \n");
		 return NULL;
	 }
	 else {
		 ACSD_INFO("Pair matched : value[%s]\n", pair->value);
		 return pair->value;
	 }
}

/* Read the key and value and add make hash table. */
static bool
acsd_make_hash()
{
	 FILE *f = NULL;
	 char line[CONFIG_FILE_LINE_LENGTH];

	 char *token = NULL;
	 char *key = NULL;
	 char *value = NULL;

	 hashtable = acsd_ht_create(HASH_MAX_ENTRY);

	 f = fopen("acsd_config.txt", "r");

	 if (NULL != f) {
		 while (fgets(line, CONFIG_FILE_LINE_LENGTH, f)) {
			char *copy = strdup(line);
			/*
				Reading one line at a time from config file
				And entries in the config file should adher the following protocol
				i.e.
				wl0_acs_scan_entry_expire 3600
				wl0_acs_scan_entry_expire is the key/entry/parameter
				3600 is the value of the key/entry/parameter
			*/

			token = strtok(copy, " \r\n");
			key = token;
			value = strtok(NULL, " \r\n");

			if ((key == NULL) || (value == NULL))
			{
				ACSD_ERROR("Wrong entry in config file \n");
				free(copy);

				if (key != NULL)
					free(key);

				if (value != NULL)
					free(value);

			} else {
				ACSD_INFO("Key[%s] Value[%s] \n", key, value);
				acsd_ht_set(hashtable, key, value);
				key = NULL;
				value = NULL;
				free(copy);
			}
		 }
		 fclose(f);
		 return TRUE;
	}
	else {
		return FALSE;
	}
}

/* Get the value w.r.t key  */
char*
acsd_config_get(const char *name)
{
	 bool chk_hash_create = FALSE;
	 char* ret_token = NULL;

	 if (hashtable == NULL) {
		 chk_hash_create = acsd_make_hash();

		if (chk_hash_create) {
			ACSD_DEBUG("Hash Table creation success\n");
		}
		else {
			ACSD_ERROR("acsd_config.txt file is not present exit the application \n");
			exit(1);
		}
	 }

	 ret_token = acsd_ht_get(hashtable, name);

	 if (ret_token)
		 return ret_token;
	 else
		 return NULL;
}

/* Get the value of an config variable */
char*
acsd_config_safe_get(const char *name)
{
	char *ptr = acsd_config_get(name);
	return ptr ? ptr : "";
}

/* Match an config variable */
int
acsd_config_match(const char *name, const char *match)
{

	 unsigned int name_len = 0;
	 name_len = strlen(name);

	 const char *value = acsd_config_get(name);
	 int cmp = strncmp(value, match, name_len);
	 int ret = value && !cmp;

	 return (ret);
}

/* Free the Hash Table  */
void
acsd_free_mem()
{
	unsigned int i = 0;
	entry_t *next = NULL;
	entry_t *curr = NULL;
	ACSD_DEBUG(" Free Heap \n");

	if (hashtable == NULL)
		return;

	for (i = 0; i < HASH_MAX_ENTRY; i++) {
		next = hashtable->table[i];
		while (next != NULL) {
		curr = next;
		next = next->next;
		free(curr->key);
		free(curr->value);
		free(curr);
		}
	}

	 free(hashtable->table);
	 free(hashtable);
}

/*
 * Basic unit test for ppr module
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: test_ppr.c xxxxxx 2013-10-30 06:00:44Z emanuell,shaib $
 */

/* ******************************************************************************************************************
************* Definitions for module components to be tested with Check  tool ***************************************
********************************************************************************************************************/

#include <typedefs.h>
#include <bcmendian.h>
#include <bcmwifi_channels.h>
#include <wlc_ppr.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <bcmwifi_rates.h>

/* ***************************************************************************************************************
   ************************************* Start of Test Section ***************************************************
   ***************************************************************************************************************/

#include <check.h> /* Includes Check framework */

/*
 * In order to run unit tests with Check, we must create some test cases,
 * aggregate them into a suite, and run them with a suite runner.

 * The pattern of every unit test is as following

 * START_TEST(name_of_test){
 *
 *     perform tests;
 *	       ...assert results
 * }
 * END_TEST

 * Test Case is a set of at least 1 unit test
 * Test Suite is a collection of Test Cases
 * Check Framework can run multiple Test Suites.
 * More details will be on Twiki
 */

/* ------------- Global Definitoions ------------------------- */

/*
 * Global variables definitions, for setup and teardown function.
 */
static ppr_t* pprptr;
static ppr_t* pprptr2;
static wl_tx_bw_t bw;
static osl_t* osh;

/* ------------- Startup and Teardown - Fixtures ---------------
 * Setting up objects for each unit test,
 * it may be convenient to add some setup that is constant across all the tests in a test case
 * rather than setting up objects for each unit test.
 * Before each unit test in a test case, the setup() function is run, if defined.
 */
void setup(void)
{
	// Create ppr pointer
	bw = WL_TX_BW_20;
	pprptr = ppr_create(osh,bw);
}

/*
 * Tear down objects for each unit test,
 * it may be convenient to add teardown that is constant across all the tests in a test case
 * rather than tearing down objects for each unit test.
 * After each unit test in a test case, the setup() function is run, if defined.
 * Note: checked teardown() fixture will not run if the unit test fails.
*/
void teardown(void)
{
	// Delete ppr pointer
	ppr_delete(osh,pprptr);
	ppr_delete(osh,pprptr2);
}

/*
 * The START_TEST/END_TEST pair are macros that setup basic structures to permit testing.
 */

// Assertion of size routine for user alloc/dealloc - bw20
START_TEST(test_ppr_bw20_size){
	const int ppr_bw20_size = 186;
	bw = WL_TX_BW_20;
	ck_assert_int_eq ((int)ppr_size(bw), ppr_bw20_size);
  }
END_TEST

// Assertion of size routine for user alloc/dealloc - bw40
START_TEST(test_ppr_bw40_size){
	const int ppr_bw40_size = 368;
	bw = WL_TX_BW_40;
	ck_assert_int_eq ((int)ppr_size(bw), ppr_bw40_size);
  }
END_TEST

// Assertion of size routine for user alloc/dealloc - bw80
START_TEST(test_ppr_bw80_size){
	const int ppr_bw80_size = 550;
	bw = WL_TX_BW_80;
	ck_assert_int_eq ((int)ppr_size(bw), ppr_bw80_size);
  }
END_TEST

// Assertion of size routine for user serialization alloc (ppr_pwrs_size(pprptr->ch_bw) + SER_HDR_LEN)
START_TEST(test_ppr_ser_size){
	const int ppr_serial_size = 192;
	ck_assert_int_eq ((int)ppr_ser_size(pprptr), ppr_serial_size);
}
END_TEST

// Assertion of size routine for user serialization alloc (ppr_pwrs_size(bw) + SER_HDR_LEN)
START_TEST(test_ppr_ser_size_by_bw){
	const int ppr_bw_size = 192;
	ck_assert_int_eq ((int)ppr_ser_size_by_bw(bw), ppr_bw_size);
}
END_TEST

START_TEST(test_ppr_bw20_delete){

	ppr_t* pprptr2;
	wl_tx_bw_t bw2 = WL_TX_BW_40;
	osl_t* osh2;
	pprptr2 = ppr_create(osh2,bw2);

	int ppr_size20=ppr_size(bw2);

	ppr_delete(osh2,pprptr2);

  }
END_TEST

START_TEST(test_ppr_bw20_clear){

	ppr_t* pprptr2;
	wl_tx_bw_t bw2 = WL_TX_BW_20;
	osl_t* osh2;
	pprptr2 = ppr_create(osh2,bw2);
	int ppr_size20 = 182;

	memset( ((uchar*)pprptr2) + 4, (int8)0x30, ppr_size(bw2)-4);

	int i;

	//ppr_delete(osh2,pprptr2);
	ppr_clear(pprptr2);

	for (i = 0; i < ppr_size20 - 4; i++)
		ck_assert_int_eq ( 0xFF & *(4 + i + ((uchar*)pprptr2)), 0xFF & WL_RATE_DISABLED);

  }
END_TEST

START_TEST(test_ppr_bw40_clear){

	ppr_t* pprptr2;
	wl_tx_bw_t bw2 = WL_TX_BW_40;
	osl_t* osh2;
	pprptr2 = ppr_create(osh2,bw2);
	//int ppr_size40=(uint)ppr_size(bw2);
	int ppr_size40 = 364;

	memset( ((uchar*)pprptr2) + 4, (int8)0x50, ppr_size(bw2)-4);

	int i;

	ppr_clear(pprptr2);

		for (i = 0; i < ppr_size40 - 4; i++)
			ck_assert_int_eq ( 0xFF & *(4 + i + ((uchar*)pprptr2)), 0xFF & WL_RATE_DISABLED);

  }
END_TEST

START_TEST(test_ppr_bw80_clear){

	ppr_t* pprptr2;
	wl_tx_bw_t bw2 = WL_TX_BW_80;
	osl_t* osh2;
	pprptr2 = ppr_create(osh2,bw2);
	//int ppr_size80=(uint)ppr_size(bw);
	int ppr_size80 = 546;
	//int ppr_size80 = 20;
	memset( ((uchar*)pprptr2) + 4, (int8)0x70, ppr_size(bw2)-4);

	int i;

	//ppr_delete(osh2,pprptr2);
	ppr_clear(pprptr2);

	for (i = 0; i < ppr_size80 - 4; i++)
		ck_assert_int_eq ( 0xFF & *(4 + i + ((uchar*)pprptr2)), 0xFF & WL_RATE_DISABLED);

  }
END_TEST

// Comparison of ppr_ser_size
START_TEST(test_ppr_comparison){
// Set bw to WL_TX_BW_40
	bw = WL_TX_BW_40;
// Create new ppr pointer
	ppr_t* pprptr_bw40;
	pprptr_bw40 = ppr_create(osh,bw);

	int pprptr_size_20 = (int)ppr_ser_size(pprptr);
	int pprptr_size_40 = (int)ppr_ser_size(pprptr_bw40);

// Comparison of pprptr_size_20 with pprptr_bw40 ( '<' operator)
	ck_assert_int_lt (pprptr_size_20, pprptr_size_40);

// Delete pprptr_bw40
	ppr_delete(osh,pprptr_bw40);
}
END_TEST

/*
 * Suite of test cases which Asserts the size routine for user alloc/dealloc
 * for bw20, bw40 and bw80 sizes.
 */

Suite * ppr_size_routine1(void)
{
	// Suit creation
	Suite *s = suite_create("PPR - Size routine for user alloc/dealloc");
	// Test case creation
	TCase *tc_size = tcase_create("Test Case - SIZE");
	// Adding unit tests to test case.
	tcase_add_test(tc_size,test_ppr_bw20_size);
	tcase_add_test(tc_size,test_ppr_bw40_size);
	tcase_add_test(tc_size,test_ppr_bw80_size);
	// Adding 'tc_ser_size' test case to a Suite
	suite_add_tcase (s, tc_size);
    return s;
}

/*
 * Suite of test cases which Asserts the size routine for user serialization
 * allocations.
 *
 */
Suite * ppr_size_routine2(void)
{
	// Suite definition - aggregates test cases into a suite, and run them with a suite runner.
	Suite *s2 = suite_create("PPR - Size routine for user serialization allocations");
	// Test case definition
	TCase *tc_ser_size = tcase_create("Test Case - SER SIZE");
	// Checked fixture to current test case
	tcase_add_checked_fixture(tc_ser_size, setup, teardown);
	// Adding unit tests to test case.
	tcase_add_test(tc_ser_size,test_ppr_ser_size);
	tcase_add_test(tc_ser_size,test_ppr_ser_size_by_bw);
	tcase_add_test(tc_ser_size,test_ppr_comparison);
	tcase_add_test(tc_ser_size,test_ppr_bw20_delete);
	tcase_add_test(tc_ser_size,test_ppr_bw20_clear);
	tcase_add_test(tc_ser_size,test_ppr_bw40_clear);
	tcase_add_test(tc_ser_size,test_ppr_bw80_clear);
	// Adding 'tc_ser_size' test case to a suite
	suite_add_tcase(s2, tc_ser_size);
	return s2;
}

/*
 * Main flow:
 * 1. Define SRunner object which will aggregate all suites.
 * 2. Adding all suites to SRunner which enables consecutive suite(s) running.
 * 3. Running all suites contained in SRunner.
 */

int main(void){
	int number_failed; /* Count number of failed tests*/
	//Adding suit to SRunner.
	SRunner *sr = srunner_create(ppr_size_routine1());
	//Adding another suit to SRunner.
	srunner_add_suite (sr,ppr_size_routine2());
	srunner_run_all (sr, CK_VERBOSE); /* Prints the summary, and one message per test (passed or failed) */
	number_failed = srunner_ntests_failed(sr); /* count all the failed tests */
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS: EXIT_FAILURE;
}

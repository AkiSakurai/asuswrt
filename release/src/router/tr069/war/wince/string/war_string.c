/*=======================================================================
  
       Copyright(c) 2009, Works Systems, Inc. All rights reserved.
  
       This software is supplied under the terms of a license agreement 
       with Works Systems, Inc, and may not be copied nor disclosed except 
       in accordance with the terms of that agreement.
  
  =======================================================================*/
/*
 * All rights reserved.
 *
 * Redistribution and use in source code and binary executable file, with or without modification,
 * are prohibited without prior written permission from Works Systems, Inc.
 * The redistribution may be allowed subject to the terms of the License Agreement with Works Systems, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>


/*!
 * \fn war_snprintf.
 * \brief the  functions  snprintf produce output according to a format.
 * \param str: the output buffer.
 * \param size: size allow writting to buffer
 * \param format: format string
 * \param ap: variable list
 * \return success: size written to buffer ; fail or output longer than size: -1 
 */
int war_vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
    int res;
  
    res = _vsnprintf(str, size-1, format, ap);
    if (-1 == res) {
        str[size -1] = '\0';
	    printf("[v]snprintf failed or output longer than buffer!\n");
        res = -1;
    } else {
        str[res] = '\0';
    }

    return res;
}
/*!
 * \fn war_snprintf.
 * \brief the  functions  snprintf produce output according to a format.
 * \param str: the output buffer.
 * \param size: size allow writting to buffer
 * \param format: format string
 * \return success: size written to buffer ; fail: -1 
 */
int war_snprintf(char *str, size_t size, const char *format, ...)
{
    int res;
    va_list ap;

    va_start(ap, format);
    res = war_vsnprintf(str, size, format, ap);
    va_end(ap);

    return res;
}

/*!
 * \fn war_strcasecmp.
 * \brief This function compares the two strings s1 and s2, ignoring the case of the characters.
 * \param s1: Pointer to string 1.
 * \param s2: Pointer to string 2.
 * \return A non-negative integer will be returned if success, otherwise -1 
 */
int war_strcasecmp(const char *s1, const char *s2) 
{
    return _stricmp(s1, s2);
}

/*!
 * \fn war_strncasecmp.
 * \brief This function only compares the first n characters of  two strings s1 and s2, ignoring the case of the characters.
 * \param s1: Pointer to string 1.
 * \param s2: Pointer to string 2.
 * \param n: Size to compare.
 * \return It returns an integer less than,  equal to, or greater than zero if  the first  n  bytes
 *         thereof  is found, respectively, to be less than, to match, or be greater than s2.
 */
int war_strncasecmp(const char *s1, const char *s2, size_t n) 
{
    return _strnicmp(s1, s2, n);
}

#define CREASE_UNIT_SIZE 128
/*!
 * \brief The simulate the getline() rutine of GUN library.
 *
 * \param buf The buffer to hold the line
 * \param len The length of the buffer
 * \param fp The file to be read
 *
 * \return The byte number when success, -1 when any error
 */
static int _getline(char **buf, int *len, FILE *fp)
{
    int i;
    char c;

    if(*buf == NULL) {
        *buf = malloc(CREASE_UNIT_SIZE);
        if(*buf == NULL) {
            printf("Out of memory!\n");
            return -1;
        }

        *len = CREASE_UNIT_SIZE;
    }

    for(i = 0; (c = fgetc(fp)) != EOF;) {
        if(i >= *len) {
            char *tmp;
            tmp = realloc(*buf, (*len) + CREASE_UNIT_SIZE); //resize the buffer if the space is not enough
            if(tmp == NULL) {
                printf("Out of memory!\n");
                return -1;
            }
            *len += CREASE_UNIT_SIZE;
        }

        (*buf)[i++] = c;
        if(c == '\n' || c == '\r')
            break;
    }


    (*buf)[i] = '\0';
     
    return i > 0? i: -1;
}
#undef CREASE_UNIT_SIZE


/*!
 * \fn war_getline.
 * \brief This function read a line from specific file. And use buffer to hold. 
 * \param buf: The buffer to hold the line.
 * \param len: The length of the buffer. 
 * \param fp: The file to be read.
 * \return It returns the byte number when success, -1 when any error.
 */
int war_getline(char **buf, size_t *len, FILE *fp)
{
    return fp == NULL? -1: _getline(buf, len, fp);
}

/*!
 * \fn war_strdup.
 * \brief This function duplicate strings.
 * \param s: The source string.
 * \return It returns a pointer to a new string which is a duplicate of the string s.  
 *         Memory for the new string is obtained with malloc(3), and can be freed with free(3).
 */
char *war_strdup(const char *s)
{
    return s == NULL?NULL:_strdup(s);
}

/*!
 * \fn war_strndup.
 * \brief This function duplicate n bytes strings.
 * \param s: The source string.
 * \param n: Size to duplicate.
 * \return It returns a pointer to a new string which is a duplicate of the string s. 
 *         If s is longer than n, only n characters are  copied, and a terminating NULL is added.
 */
char *war_strndup( const char *s, size_t n)
{
	char *res = NULL;

	if (s != NULL) {
        res = malloc(sizeof(char)*n + 1);
        if(res != NULL) {
            war_snprintf(res, n + 1, "%s", s);
		}
	}
    return res;
}


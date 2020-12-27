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
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>


/*!
 * \fn war_strftime.
 * \brief formats  the  broken-down time tm according to the format specification format
 * \param s: Name of file to remove.
 * \param max: Name of file to remove.
 * \param format: Name of file to remove.
 * \param tm: Name of file to remove.
 * \return the number of characters placed in the array s, not including the terminating NUL character.
 */
size_t war_strftime( char *s, size_t max, const char *format, const struct tm *tm)
{
    return strftime(s, max, format, tm);
}

/*!
 * \fn war_time.
 * \brief This function Get the system time.
 * \param t: Pointer to the storage location for time.
 * \return It returns the number of seconds elapsed since midnight (00:00:00), January 1, 1970, coordinated universal time (UTC), 
 *         according to the system clock., on error ((time_t)-1) is returned.
 */
time_t war_time(time_t *t)
{
    return time(t);
}

/*!
 * \fn war_localtime.
 * \brief This function converts the calendar time timep to broken-time representation.
 * \param t: Pointer to stored time.
 * \return  a pointer to the structure result, NULL when any error.
 */
struct tm * war_localtime(const time_t *t)
{
    return localtime(t);
}

/*!
 * \fn war_gmtime.
 * \brief This function converts the calendar time timep to broken-down time representation.
 * \param t: Pointer to stored time. The time is represented as seconds elapsed since midnight (00:00:00), January 1, 1970, coordinated universal time (UTC)
 * \return It returns a pointer to a structure of type tm. The fields of the returned structure hold the evaluated value of the timer argument in UTC rather than in local time, NULL when any error	 
 */
struct tm * war_gmtime(const time_t *t) 
{
    return gmtime(t);
}

/*!
 * \fn war_mktime.
 * \brief This function Convert the local time to a calendar value.
 * \param t: Pointer to time structure.
 * \return the value described, or( (time_t)-1) in case an error was detected. 
 */
time_t war_mktime(struct tm *tm)
{
    return mktime(tm);
}

/*!
 * \fn war_sleep.
 * \brief This function Suspends the execution of the current thread/process until the time-out interval elapses  or a signal arrives which is not ignored.
 * \param sec: the time interval for which execution is to be suspended, in seconds.
 * \return It does not return a value.
 */
void war_sleep(int sec)
{
    sleep(sec);
}

/*!
 * \fn war_gettimeofday.
 * \brief This function can get the time as well as a timezone.
 * \param tv: Pointer to struct timeval.
 * \param tz: Pointer to struct timezone.
 * \return It return 0 for success, or -1 for failure
 */
int war_gettimeofday(struct timeval *tv, struct timezone *tz)
{
    return gettimeofday(tv, tz);
}

/*!
 * \fn war_gettimeofday.
 * \brief This function converts the character string to values.
 * \param s: Pointer to source string.
 * \param format: Pointer to format.
 * \param tm: Pointer to struct tm.
 * \return The return value of the function is a pointer to the first character not processed in this function call.  In case  the  input  string contains more characters than required by the format string the return value points right after the last consumed input character.  In case the whole input string is consumed the return value points to the NUL byte at the end of the  string.   If  strptime()  fails  to match all of the format string and therefore an error occurred the function returns NULL.
 */
#if 0
char * war_strptime(const char *s, const char *format, struct tm *tm)
{
    return strptime(s, format, tm);
}
#endif

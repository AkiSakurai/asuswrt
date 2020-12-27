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
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/times.h>
#include "war_type.h"

/*!
 * \fn war_strftime.
 * 
 * \brief formats  the  broken-down time tm according to the format specification format
 * 
 * \param s: Name of file to remove.
 * \param max: Name of file to remove.
 * \param format: Name of file to remove.
 * \param tm: Name of file to remove.
 * 
 * \return the number of characters placed in the array s, not including the terminating NUL character.
 * 
 */
size_t war_strftime( char *s, size_t max, const char *format, const struct tm *tm)
{
    //set default format for vxworks because lack of support for certain format "%e %T" for LOG
	if(strstr(format, "%e") || strstr(format, "%T"))
		return strftime(s, max, "%Y-%m-%d %X", tm);
	else
		return strftime(s, max, format, tm);
}

/*!
 * \fn war_time.
 * 
 * \brief This function Get the system time.
 * 
 * \param t: Pointer to the storage location for time.
 * 
 * \return It returns the number of seconds elapsed since midnight (00:00:00), January 1, 1970, coordinated universal time (UTC), 
 *         according to the system clock., on error ((time_t)-1) is returned.
 *         
 */
time_t war_time(time_t *t)
{
    return time(t);
}

/*!
 * \fn war_localtime.
 * 
 * \brief This function converts the calendar time timep to broken-time representation.
 * 
 * \param t: Pointer to stored time.
 * 
 * \return  a pointer to the structure result, NULL when any error.
 * 
 */
struct tm * war_localtime(const time_t *t)
{
    return localtime(t);
}

/*!
 * \fn war_gmtime.
 * 
 * \brief This function converts the calendar time timep to broken-down time representation.
 * 
 * \param t: Pointer to stored time. The time is represented as seconds elapsed since midnight (00:00:00), January 1, 1970, coordinated universal time (UTC)
 * 
 * \return It returns a pointer to a structure of type tm. The fields of the returned structure hold the evaluated value of the timer argument in UTC rather than in local time, NULL when any error
 * 	 
 */
struct tm * war_gmtime(const time_t *t) 
{
    return gmtime(t);
}

/*!
 * \fn war_mktime.
 * 
 * \brief This function Convert the local time to a calendar value.
 * 
 * \param t: Pointer to time structure.
 * 
 * \return the value described, or( (time_t)-1) in case an error was detected. 
 * 
 */
time_t war_mktime(struct tm *tm)
{
    return mktime(tm);
}

/*!
 * \fn war_sleep.
 * 
 * \brief This function Suspends the execution of the current thread/process until the time-out interval elapses  or a signal arrives which is not ignored.
 * 
 * \param sec: the time interval for which execution is to be suspended, in seconds.
 * 
 * \return It does not return a value.
 * 
 */
void war_sleep(int sec)
{
    sleep(sec);
}

/*!
 * \fn getTimeZone.
 * 
 * \brief This function can get the timezone.
 * 
 * \param t: Total time.
 * \param min: Pointer to minutes west of Greenwich.
 * \param dst: Pointer to type of dst correction.
 * 
 * \return It return 0 for success, or -1 for failure.
 * 
 */
static void getTimeZone(time_t t, int *min, int *dst) 
{ 
	struct tm dstTm; 
	char *tz = getenv("TZ");
	
	*dst = (localtime_r(&t, &dstTm) == OK) ? dstTm.tm_isdst : 0; 
	if (tz) { /* see VxWorks timeLib develop guide */ 
		/* mmddhh */ 
		const char *p = tz; 
		int i = 0; 
		while ((i < 2) && (p = strchr(p, ':'))) { 
			++p; 
			++i; 
		} 
		if (p) { 
			if (sscanf(p, "%d", min)!=1) { 
				*min = 0; 
			} 
		} 
	} 
} 

/*!
 * \fn gettimeofday.
 * 
 * \brief This function can get the time as well as a timezone.
 * 
 * \param tv: Pointer to struct timeval.
 * \param tz: Pointer to struct timezone.
 * 
 * \return It return 0 for success, or -1 for failure.
 * 
 */
int gettimeofday(struct timeval *tv, struct timezone *tz) 
{ 
	int ret; 
	struct timespec tp; 

	if ((ret=clock_gettime(CLOCK_REALTIME, &tp))==0) { 
		tv->tv_sec = tp.tv_sec; 
		tv->tv_usec = (tp.tv_nsec + 500) / 1000; //ns->ms

		if (tz != NULL) { 
			getTimeZone(tp.tv_sec, &tz->tz_minuteswest, &tz->tz_dsttime); 
		} 
	} 
	return ret; 
} 

/*!
 * \fn war_gettimeofday.
 * 
 * \brief This function can get the time as well as a timezone.
 * 
 * \param tv: Pointer to struct timeval.
 * \param tz: Pointer to struct timezone.
 * 
 * \return It return 0 for success, or -1 for failure.
 * 
 */
int war_gettimeofday(struct timeval *tv, struct timezone *tz)
{
    return gettimeofday(tv, tz);
}


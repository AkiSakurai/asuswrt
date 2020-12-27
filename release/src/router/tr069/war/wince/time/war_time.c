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
#include <windows.h>
#include <winbase.h>
#include <winsock2.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>

#include "war_type.h"

const __int64 _onesec_in100ns = (__int64)10000000;

/*!
 * \fn strftime
 * \brief  formats the broken-down time tm according to
 *         the format specification format and places the result in the  character
 *         array s of size max.
 * \param s: string
 * \param max: max size
 * \param format: format
 * \param t: tm time
 * \return the number of characters placed in the array s, not including the terminating NUL character.
 */

static unsigned int _strftime(char *s, unsigned int max, const char *format, const struct tm *t)
{
    char str[10];

    memset(str, 0, sizeof(str));
    sprintf(s, "%d-", t->tm_year + 1900);

    if (t->tm_mon + 1 < 10) {
	sprintf(str, "0%d-", t->tm_mon + 1);
    } else {
	sprintf(str, "%d-", t->tm_mon + 1);
    }
    strcat(s, str);

    if (t->tm_mday < 10) {
	sprintf(str, "0%dT", t->tm_mday);
    } else {
	sprintf(str, "%dT", t->tm_mday);
    }
    strcat(s, str);

    if (t->tm_hour < 10) {
	sprintf(str, "0%d:", t->tm_hour);
    } else {
	sprintf(str, "%d:", t->tm_hour);
    }
    strcat(s, str);

    if (t->tm_min < 10) {
	sprintf(str, "0%d:", t->tm_min);
    } else {
	sprintf(str, "%d:", t->tm_min);
    }
    strcat(s, str);

    if (t->tm_sec < 10) {
	sprintf(str, "0%d", t->tm_sec);
    } else {
	sprintf(str, "%d", t->tm_sec);
    }
    strcat(s, str);
    return strlen(s);
}

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
    return _strftime(s, max, format, tm);
}
/*!
 * \fn wce_getFILETIMEFromYear
 * \brief get FILETIME form year
 * \param year: the year
 * \return  FILETIME
 */
static FILETIME wce_getFILETIMEFromYear(WORD year)
{
    SYSTEMTIME s={0};
    FILETIME f;

    s.wYear      = year;
    s.wMonth     = 1;
    s.wDayOfWeek = 1;
    s.wDay       = 1;

    SystemTimeToFileTime( &s, &f );
    return f;
}
/*!
 * \fn wce_FILETIME2int64
 * \brief FILETIME to int64
 * \param f: FILETIME
 * \return  int64 time
 */
static __int64 wce_FILETIME2int64(FILETIME f)
{
    __int64 t;

    t = f.dwHighDateTime;
    t <<= 32;
    t |= f.dwLowDateTime;
    return t;
}

/*!
 * \fn wce_SYSTEMTIME2tm
 * \brief FILETIME to time_t
 * \param s: FILETIME
 * \return  time_t time
 */
#ifndef USEWCECOMPAT
time_t wce_FILETIME2time_t(const FILETIME* f)
{
    FILETIME f1601, f1970;
    __int64 t, offset;

    f1601 = wce_getFILETIMEFromYear(1601);
    f1970 = wce_getFILETIMEFromYear(1970);

    offset = wce_FILETIME2int64(f1970) - wce_FILETIME2int64(f1601);

    t = wce_FILETIME2int64(*f);

    t -= offset;
    return (time_t)(t / _onesec_in100ns);
}
#endif

/*!
 * \fn war_time.
 * \brief This function Get the system time.
 * \param t: Pointer to the storage location for time.
 * \return It returns the number of seconds elapsed since midnight (00:00:00), January 1, 1970, coordinated universal time (UTC), 
 *         according to the system clock., on error ((time_t)-1) is returned.
 */
time_t war_time(time_t *t )
{
    SYSTEMTIME s;
    FILETIME   f;
    static time_t t_tmp;

    GetSystemTime( &s );

    SystemTimeToFileTime( &s, &f );

    t_tmp = wce_FILETIME2time_t(&f);
    if(t != NULL)
	*t = t_tmp;
    return t_tmp;
}
/*!
 * \fn wce_int642FILETIME
 * \brief int64 to FILETIME
 * \param f: int64 time
 * \return  FILETIME
 */
static FILETIME wce_int642FILETIME(__int64 t)
{
    FILETIME f;

    f.dwHighDateTime = (DWORD)((t >> 32) & 0x00000000FFFFFFFF);
    f.dwLowDateTime  = (DWORD)( t        & 0x00000000FFFFFFFF);
    return f;
}

/*!
 * \fn wce_time_t2FILETIME
 * \brief time_t to FILETIME
 * \param s: time_t
 * \return  FILETIME
 */
FILETIME wce_time_t2FILETIME(const time_t t)
{
    FILETIME f, f1970;
    __int64 time;

    f1970 = wce_getFILETIMEFromYear(1970);

    time = t;
    time *= _onesec_in100ns;
    time += wce_FILETIME2int64(f1970);

    f = wce_int642FILETIME(time);

    return f;
}
/*!
 * \fn wce_getYdayFromSYSTEMTIME
 * \brief get Yday from SYSTEMTIME
 * \param s: SYSTEMTIME
 * \return  time_t time
 */
static time_t wce_getYdayFromSYSTEMTIME(const SYSTEMTIME* s)
{
    __int64 t;
    FILETIME f1, f2;

    f1 = wce_getFILETIMEFromYear( s->wYear );
    SystemTimeToFileTime( s, &f2 );

    t = wce_FILETIME2int64(f2)-wce_FILETIME2int64(f1);

    return (time_t)((t/_onesec_in100ns)/(60*60*24));
}

/*!
 * \fn wce_SYSTEMTIME2tm
 * \brief SYSTEMTIME to tm
 * \param s: SYSTEMTIME
 * \return  tm time
 */
static struct tm wce_SYSTEMTIME2tm(SYSTEMTIME *s)
{
    struct tm t;

    t.tm_year  = s->wYear - 1900;
    t.tm_mon   = s->wMonth- 1;
    t.tm_wday  = s->wDayOfWeek;
    t.tm_mday  = s->wDay;
    t.tm_yday  = wce_getYdayFromSYSTEMTIME(s);
    t.tm_hour  = s->wHour;
    t.tm_min   = s->wMinute;
    t.tm_sec   = s->wSecond;
    t.tm_isdst = 0;

    return t;
}

/*!
 * \fn war_localtime.
 * \brief This function converts the calendar time timep to broken-time representation.
 * \param timer: Pointer to stored time.
 * \return  a pointer to the structure result, NULL when any error.
 */
struct tm *war_localtime( const time_t *timer )
{
    SYSTEMTIME ss, ls, s;
    FILETIME   sf, lf, f;
    __int64 t, diff;
    static struct tm tms;

    GetSystemTime(&ss);
    GetLocalTime(&ls);

    SystemTimeToFileTime( &ss, &sf );
    SystemTimeToFileTime( &ls, &lf );

    diff = wce_FILETIME2int64(sf) - wce_FILETIME2int64(lf);

    f = wce_time_t2FILETIME(*timer);
    t = wce_FILETIME2int64(f) - diff;
    f = wce_int642FILETIME(t);

    FileTimeToSystemTime( &f, &s );

    tms = wce_SYSTEMTIME2tm(&s);

    return &tms;
}

/*!
 * \fn war_gmtime.
 * \brief This function converts the calendar time timep to broken-down time representation.
 * \param t: Pointer to stored time. The time is represented as seconds elapsed since midnight (00:00:00), January 1, 1970, coordinated universal time (UTC)
 * \return It returns a pointer to a structure of type tm. The fields of the returned structure hold the evaluated value of the timer argument in UTC rather than in local time, NULL when any error	 
 */
struct tm *war_gmtime(const time_t *t)
{
    FILETIME f;
    SYSTEMTIME s;
    static struct tm tms;

    f = wce_time_t2FILETIME(*t);
    FileTimeToSystemTime(&f, &s);
    tms = wce_SYSTEMTIME2tm(&s);
    return &tms;
}

/*!
 * \fn wce_tm2SYSTEMTIME
 * \brief tm to SYSTEMTIME
 * \param t: tm structure instance
 * \return  SYSTEMTIME
 */
static SYSTEMTIME wce_tm2SYSTEMTIME(struct tm *t)
{
    SYSTEMTIME s;

    s.wYear      = t->tm_year + 1900;
    s.wMonth     = t->tm_mon  + 1;
    s.wDayOfWeek = t->tm_wday;
    s.wDay       = t->tm_mday;
    s.wHour      = t->tm_hour;
    s.wMinute    = t->tm_min;
    s.wSecond    = t->tm_sec;
    s.wMilliseconds = 0;

    return s;
}

/*!
 * \fn war_mktime.
 * \brief This function Convert the local time to a calendar value.
 * \param pt: Pointer to time structure.
 * \return the value described, or( (time_t)-1) in case an error was detected. 
 */
time_t war_mktime(struct tm* pt)
{
    SYSTEMTIME ss, ls, s;
    FILETIME   sf, lf, f;
    __int64 diff;

    GetSystemTime(&ss);
    GetLocalTime(&ls);
    SystemTimeToFileTime( &ss, &sf );
    SystemTimeToFileTime( &ls, &lf );

    diff = (wce_FILETIME2int64(lf)-wce_FILETIME2int64(sf))/_onesec_in100ns;

    s = wce_tm2SYSTEMTIME(pt);
    SystemTimeToFileTime( &s, &f );
    return wce_FILETIME2time_t(&f) - (time_t)diff;
}

/*!
 * \fn war_sleep.
 * \brief This function Suspends the execution of the current thread/process until the time-out interval elapses  or a signal arrives which is not ignored.
 * \param sec: the time interval for which execution is to be suspended, in seconds.
 * \return It does not return a value.
 */
void war_sleep(int sec)
{
    Sleep(sec * 1000);
}


#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif

static int _gettimeofday(struct timeval *t, void *foo)  
{  
    int y,isLeapYear,notLeapYear;   
    SYSTEMTIME   systime;  
    GetSystemTime(&systime);  
    t->tv_sec=0;
    systime.wMilliseconds = GetTickCount();

    for(y = 1970; y < systime.wYear; y++) {  
	if(((0 == (y % 4))&&(0 != (y % 100)))||(0 == (y % 400)))  
	    t->tv_sec += 366*24*3600;  
	else  
	    t->tv_sec += 365*24*3600;  
    }  
    isLeapYear = ((0 == systime.wYear % 4)&&(0 != systime.wYear % 100))||(0 == systime.wYear % 400);  
    notLeapYear = 1 - isLeapYear;  
    switch(systime.wMonth)  
    {  
	case 1:  
	    t->tv_sec+=(systime.wDay-1)*24*3600;  
	    t->tv_sec+=systime.wHour*3600+systime.wMinute*60+systime.wSecond+systime.wMilliseconds/1000;  
	    break;  
	case 2:  
	    t->tv_sec+=(systime.wDay-1)*24*3600;  
	    t->tv_sec+=systime.wHour*3600+systime.wMinute*60+systime.wSecond+systime.wMilliseconds/1000;  
	    t->tv_sec+=31*24*3600;  
	    break;  
	case 3:  
	    t->tv_sec+=(systime.wDay-1)*24*3600;  
	    t->tv_sec+=systime.wHour*3600+systime.wMinute*60+systime.wSecond+systime.wMilliseconds/1000;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=(29-notLeapYear)*24*3600;  
	    break;  
	case 4:  
	    t->tv_sec+=(systime.wDay-1)*24*3600;  
	    t->tv_sec+=systime.wHour*3600+systime.wMinute*60+systime.wSecond+systime.wMilliseconds/1000;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=(29-notLeapYear)*24*3600;  
	    t->tv_sec+=31*24*3600;  
	    break;  
	case 5:  
	    t->tv_sec+=(systime.wDay-1)*24*3600;  
	    t->tv_sec+=systime.wHour*3600+systime.wMinute*60+systime.wSecond+systime.wMilliseconds/1000;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=(29-notLeapYear)*24*3600;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=30*24*3600; 
	    break;  
	case 6:  
	    t->tv_sec+=(systime.wDay-1)*24*3600;  
	    t->tv_sec+=systime.wHour*3600+systime.wMinute*60+systime.wSecond+systime.wMilliseconds/1000;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=(29-notLeapYear)*24*3600;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=30*24*3600;  
	    t->tv_sec+=31*24*3600;  
	    break;  
	case 7:  
	    t->tv_sec+=(systime.wDay-1)*24*3600;  
	    t->tv_sec+=systime.wHour*3600+systime.wMinute*60+systime.wSecond+systime.wMilliseconds/1000;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=(29-notLeapYear)*24*3600;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=30*24*3600;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=30*24*3600;  
	    break;  
	case 8:  
	    t->tv_sec+=(systime.wDay-1)*24*3600;  
	    t->tv_sec+=systime.wHour*3600+systime.wMinute*60+systime.wSecond+systime.wMilliseconds/1000;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=(29-notLeapYear)*24*3600;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=30*24*3600;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=30*24*3600;  
	    t->tv_sec+=31*24*3600;  
	    break;  
	case 9:  
	    t->tv_sec+=(systime.wDay-1)*24*3600;  
	    t->tv_sec+=systime.wHour*3600+systime.wMinute*60+systime.wSecond+systime.wMilliseconds/1000;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=(29-notLeapYear)*24*3600;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=30*24*3600;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=30*24*3600;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=31*24*3600;  
	    break;  
	case 10:  
	    t->tv_sec+=(systime.wDay-1)*24*3600;  
	    t->tv_sec+=systime.wHour*3600+systime.wMinute*60+systime.wSecond+systime.wMilliseconds/1000;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=(29-notLeapYear)*24*3600;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=30*24*3600;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=30*24*3600;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=30*24*3600;  
	    break;  
	case 11:  
	    t->tv_sec+=(systime.wDay-1)*24*3600;  
	    t->tv_sec+=systime.wHour*3600+systime.wMinute*60+systime.wSecond+systime.wMilliseconds/1000;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=(29-notLeapYear)*24*3600;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=30*24*3600;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=30*24*3600;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=30*24*3600;  
	    t->tv_sec+=31*24*3600;  
	    break;  
	case 12:  
	    t->tv_sec+=(systime.wDay-1)*24*3600;  
	    t->tv_sec+=systime.wHour*3600+systime.wMinute*60+systime.wSecond+systime.wMilliseconds/1000;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=(29-notLeapYear)*24*3600;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=30*24*3600;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=30*24*3600;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=30*24*3600;  
	    t->tv_sec+=31*24*3600;  
	    t->tv_sec+=30*24*3600;  
	    break;  
	default://impossible  
	    break;  
    }  

    t->tv_usec = systime.wMilliseconds * 100;  

    return 0;  
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
    return _gettimeofday(tv, tz);
}


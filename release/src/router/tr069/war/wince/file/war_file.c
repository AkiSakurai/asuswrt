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

#include <windows.h>
#include <winbase.h>
#include <winsock2.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>

#include "war_string.h"
/*!
 * \fn wce_mbtowc
 * \brief  char to wchar_t
 * \param a: char string
 * \return  wchar_t string begin address
 */
static wchar_t* wce_mbtowc(const char* a)
{
    int length;
    wchar_t *wbuf;

    length = MultiByteToWideChar(CP_ACP, 0,
	    a, -1, NULL, 0);
    wbuf = (wchar_t*)malloc( (length+1)*sizeof(wchar_t) );
    MultiByteToWideChar(CP_ACP, 0,
	    a, -1, wbuf, length);

    return wbuf;
}

/*!
 * \fn war_unlink.
 * \brief This function Delete a file.
 * \param pathname: Name of file to remove.
 * \return the zero when success, -1 when any error.
 */
int war_unlink(const char *file)
{
    wchar_t *wfile;
    BOOL rc;

    /* replace with DeleteFile. */
    wfile = wce_mbtowc(file);
    rc = DeleteFileW(wfile);
    free(wfile);
    return rc==TRUE ? 0 : -1;
}

/*!
 * \fn war_rename.
 * \brief This function Rename a file or directory.
 * \param oldname: Pointer to old name.
 * \param newname: Pointer to new name.
 * \return the zero when success, -1 when any error.
 */
int war_rename(const char *oldname, const char *newname)
{
    wchar_t *wold, *wnew;
    BOOL rc;

    wold = wce_mbtowc(oldname);
    wnew = wce_mbtowc(newname);

    /* replace with MoveFile. */
    rc = MoveFileW(wold, wnew);

    free(wold);
    free(wnew);

    return rc == 0 ? -1 : 0;

}

/*!
 * \fn war_judge_absolutepath.
 * \brief Judge whether the pathname is absolute.
 * \param pathname: Pointer to pathname.
 * \return the 1 when absolute, -1 when not.
 */
int war_judge_absolutepath(const char *pathname)
{
	return (((pathname[0] >= 'a' && pathname[0] <= 'z') || (pathname[0] >= 'A' && pathname[0] <= 'Z')) && pathname[1] == ':')?1:0 ;
}

/*!
 * \fn init_argument()
 * \brief init argument
 * \param argc: argc
 * \param argv: argv
 * \param p_directory: the pointer to the directory string
 * \return 0; -1
 */
int init_argument(int argc, char **argv, char *p_directory, int size)
{
    //Make sure the work directory end with '/' or '\'
    war_snprintf(p_directory, size, "%s", "\\conf\\");
    return 0;
}

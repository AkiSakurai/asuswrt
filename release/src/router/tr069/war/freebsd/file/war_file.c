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
#include <unistd.h>

#include "war_string.h"
#include "war_type.h"

/*!
 * \fn war_unlink.
 * \brief This function Delete a file.
 * \param pathname: Name of file to remove.
 * \return the zero when success, -1 when any error.
 */
int war_unlink(const char *pathname)
{
    return unlink(pathname);
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
    return rename(oldname, newname);
}

/*!
 * \fn war_judge_absolutepath.
 * \brief Judge whether the pathname is absolute.
 * \param pathname: Pointer to pathname.
 * \return the 1 when absolute, -1 when not.
 */
int war_judge_absolutepath(const char *pathname)
{
    return (*pathname == '/')? 1: 0; 
}

static void usage(const char *command)
{
        fprintf(stderr, "Usage:\n"
                        "\t%s [--directory|-d directoryname]\n"
                        "\t--directory\n\t-d\n"
                        "\t\tTo specify the config and work directory. The default\n"
                        "directory is '/etc/tr/'", command);
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
    char work_dir[256];
    getcwd(work_dir, 256);
    if (argc >= 3 && (war_strcasecmp(argv[1], "--directory") == 0 || war_strcasecmp(argv[1], "-d") == 0)) {
	//Make sure the work directory end with '/' or '\'
	if (argv[2][strlen(argv[2]) - 1] == FILE_PATH_SEP){
	    if(war_judge_absolutepath(argv[2]))
		    war_snprintf(p_directory, size, "%s", argv[2]);
	    else
		    war_snprintf(p_directory, size, "%s%c%s", work_dir, FILE_PATH_SEP, argv[2]);
	}else{
	    if(war_judge_absolutepath(argv[2]))
		    war_snprintf(p_directory, size, "%s%c", argv[2], FILE_PATH_SEP);
	    else
		    war_snprintf(p_directory, size, "%s%c%s%c", work_dir, FILE_PATH_SEP, argv[2], FILE_PATH_SEP);
	}
    } else if (argc != 1) {
	usage(argv[0]);
	return -1;
    }
    return 0;
}



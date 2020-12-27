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
#include <errno.h>


/*!
 * \fn war_geterror.
 * 
 * \brief This function get error code of socket or others.
 * 
 * \return the error code, or 0 if no error 0.
 */
int war_geterror() 
{
    return errno;
}

/*!
 * \fn war_getsockerror.
 * 
 * \brief This function get error code of socket.
 * 
 * \return the error code, or 0 if no error 0.
 * 
 */
int war_getsockerror()
{
    return errno;
}

/*!
 * \fn war_sockstrerror.
 * 
 * \brief This function convert the error code passed in the argument errnum.to a string.
 * 
 * \param errnu: Error number
 * 
 * \return It returns a string describing the error code passed in the argument errnum.
 */
char * war_sockstrerror(int errnum)
{
    return strerror(errnum);
}

/*!
 * \fn war_strerror.
 * 
 * \brief This function convert the error code passed in the argument errnum.to a string.
 * 
 * \param errnu: Error number
 * 
 * \return It returns a string describing the error code passed in the argument errnum.
 * 
 */
char * war_strerror(int errnum)
{
    return strerror(errnum);
}

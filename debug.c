/*static char *sccsid = "%Z% %M% %I% - %G% %U% ";*/
/***************************************************************
*  
*  ARPUS/Ce text editor and terminal emulator modeled after the
*  Apollo(r) Domain systems.
*  Copyright 1988 - 2002 Enabling Technologies Group
*  Copyright 2003 - 2005 Robert Styma Consulting
*  
*  This program is free software; you can redistribute it and/or
*  modify it under the terms of the GNU General Public License
*  as published by the Free Software Foundation; either version 2
*  of the License, or (at your option) any later version.
*  
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*  
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software
*  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*  
*  Original Authors:  Robert Styma and Kevin Plyler
*  Email:  styma@swlink.net
*  
***************************************************************/

/**************************************************************************
   Function:   debug

   INPUTS:

        DEBUG environment variable;

   OUTPUTS:

        external variable debug set to 1/0;

   NOTES:

       formats of debug string:
       1.  DEBUG=<number>,
           sets debug equal to that decimal number
       2.  DEBUG=@<number>[,<number>]...
           where each number is from 1 to 32.
           The corresponding bit in the debug integer is set.  The variable
           is not cleared first.
           This activates the DEBUG1, DEBUG2, etc., macros in debug.h
       3.  >/path/file
           The ">" suffix may be placed on the  DEBUG variable, it causes
           stderr to be repointed to the named file.  If /path/file is
           ommitted, stderr reverts to stdout.  This stays set until another
           > is passed in another call to debug.
       4.  If the path ends in a plus sign, the plus sign is replaced with the current pid.

*************************************************************************/

#ifdef DebuG

#include <stdio.h>       /* /usr/include/stdio.h   */
#include <string.h>      /* /usr/include/string.h  */
#include <stdlib.h>      /* /usr/include/stdlib.h  */
#include <fcntl.h>       /* /usr/include/fcntl.h  */
#include <errno.h>       /* /usr/include/errno.h  */

#ifdef WIN32
#include <windows.h>
#include <io.h>
#include <process.h>
#define getpid _getpid
#include "cepopen.h"     /* needed for ce_popen and ce_pclose */
#endif

#include "debug.h"

char   *getenv(const char *name);


void Debug(char *env_name)
{

char        *ptr;
char        *q;
char        *q2;
char         env_hold[1024];
int          append_mode;
#ifdef WIN32
char         msg[1030];
#endif
char        *null_env;
char         work[1024];
char        *lcl_env = NULL;
static FILE *pstream = NULL;
#ifdef WIN32
FILE        *work_stream;
int          fd;
#endif

if (env_name == NULL)
   env_name = "DEBUG";

ptr = getenv(env_name);

if (ptr)
   {
      q = strchr(ptr, '>');
      if (q != NULL)
         {
            if (*(q+1) == '>')
               append_mode = 1;
            else
               append_mode = 0;
            strncpy(work, ptr, sizeof(work)-6);
            work[sizeof(work)-7] = '\0'; /* make sure string is null terminated */
            q = work + (q - ptr);
            ptr = work;
            *q++ = '\0'; /* blast the arrow and truncate the string. */
            if (append_mode)
               *q++ = '\0'; /* blast the second arrow and truncate the string. */
            for (q2 = q-2; (q2 > work) && (*q2 == ' ' || *q == '\t' || *q == '\0'); q2--) *q2 = '\0'; /* trim blanks in number string  */
            while(*q == ' ' || *q == '\t') q++; /* find name past the > */
            for (q2 = &q[strlen(q)-1]; (q2 > q) && (*q2 == ' ' || *q == '\t'); q2--) *q2 = '\0'; /* trim blanks in file string  */
            if ((*q2 == '$') && (*(q2-1) == '$'))
               sprintf(q2-1,"%d", getpid());
#ifdef WIN32
            if (pstream)
               {
                  ce_pclose(pstream);
                  pstream = NULL;
               }
            else
               fclose(stderr);
            if (*q)
               {
                  if ((work_stream = fopen(q, (append_mode ? "a" : "w"))) == NULL)
                     {
                        _snprintf(msg, sizeof(msg), "fopen of %s failed (%s)", q, strerror(errno));
                        MessageBox(NULL, msg, "Open of debug file failed", MB_OK);
                        dup2(1, 2);
                     }
                  else
                     if (work_stream != stderr)
                        {
                           MessageBox(NULL, "Fopen did not return stderr!\n", "Open of debug file error", MB_OK);
                        }
                     else
                        setbuf(stderr, NULL);
               }
            else
              {
                 fclose(stderr);
                 dup2(1, 2);
              }
#else
            if (pstream)
               {
                  pclose(pstream);
                  pstream = NULL;
               }
            else
               close(2);
            if (*q)
               {
                  if (open(q, (append_mode ? (O_WRONLY | O_CREAT | O_APPEND) : (O_WRONLY | O_CREAT | O_TRUNC)), 0666) < 0)
                     {
                        fprintf(stdout, "%s : %s (%s)\n", q, strerror(errno), (append_mode ? "(O_WRONLY | O_CREAT | O_APPEND)" : "(O_WRONLY | O_CREAT | O_TRUNC)"));
                        dup(1);
                     }
              }
            else
               dup(1);
#endif
         } 
      else
         {
            q = strchr(ptr, '|');
            if (q != NULL)
               {
                  lcl_env = strdup(ptr); /* ptr is clobbered by putenv */
                  /* dummy out the env var to prevent recursive debug calls. */
                  snprintf(env_hold, sizeof(env_hold), "%s=%s", env_name, ptr);
                  snprintf(work, sizeof(work), "%s=", env_name);
                  null_env = strdup(work);
                  if (null_env)
                     putenv(null_env);
                  ptr = lcl_env;
                  q = strchr(ptr, '|');
                  strncpy(work, ptr, sizeof(work));
                  work[sizeof(work)-1] = '\0';
                  q = work + (q - ptr);
                  ptr = work;
                  *q++ = '\0'; /* blast the pipe symbol and truncate the string. */
                  for (q2 = q-2; (q2 > work) && (*q2 == ' ' || *q == '\t'); q2--) *q2 = '\0'; /* trim blanks in number string  */
                  while(*q == ' ' || *q == '\t') q++; /* find name past the > */
                  for (q2 = &q[strlen(q)-1]; (q2 > q) && (*q2 == ' ' || *q == '\t'); q2--) *q2 = '\0'; /* trim blanks in pipe string  */
                  if ((*q2 == '$') && (*(q2-1) == '$'))
                     sprintf(q2-1,"%d", getpid());
                  if (pstream)
                     {
#ifdef WIN32
                        ce_pclose(pstream);
#else
                        pclose(pstream);
#endif
                        pstream = NULL;
                     }
                  else
                     fclose(stderr);
                  if (*q)
                     {
#ifdef WIN32
                        if ((pstream = ce_popen(q, "w")) == NULL)
#else
                        if ((pstream = popen(q, "w")) == NULL)
#endif
                           {
                              fprintf(stdout, "%s : %s", q, strerror(errno));
                              dup(1);
                           }
                        else
                           {
                              setbuf(pstream, NULL);
                              if (pstream != stderr)
                                 fprintf(stdout, "pstream is not stderr!!\n");
#ifdef WIN32
                              fd = fileno(pstream);
                              if (fd != 2)
                                 if (dup2(fd, 2) != 0)
                                    fprintf(stdout, "in debug: dup2 : %s", strerror(errno));
                              SetStdHandle(STD_ERROR_HANDLE, (HANDLE)_get_osfhandle(fd));
#endif
                           }
                      }
                  else
                     dup(1);

                  free(null_env);
                  null_env = strdup(env_hold);
                  if (null_env)
                     putenv(null_env);
               }
         }

      if (ptr[0] == 'y' || ptr[0] == 'Y')
         debug = 1;
      else
         if (ptr[0] == '@')
            for (ptr = strtok(ptr+1,","); ptr != NULL; ptr = strtok(NULL,","))
                debug |= 1 << (atoi(ptr) -1);
         else
            debug = atoi(ptr);
   }
else
   debug = 0;

if (lcl_env)
   free(lcl_env);

setbuf(stderr, NULL);

if (debug) fprintf(stderr,"%s=0x%x\n", env_name, debug);
}




#endif

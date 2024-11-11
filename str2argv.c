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

/**************************************************************
*
*  Routines in str2argv.c
*
*     char_to_argv     -  convert a command line to an argc/argv 
*     free_temp_args   -  Free arguments created by char_to_argv
*
*
***************************************************************/

#include <stdio.h>           /* /usr/include/stdio.h  */
#include <string.h>          /* /usr/include/string.h  */
#include <ctype.h>           /* /usr/include/ctype.h  */

#include "str2argv.h"

#ifdef ARPUS_CE
#include "dmwin.h" 
#include "emalloc.h"
#include "memdata.h"   /* needed for MAX_LINE */
#else
#include <malloc.h>
#define CE_MALLOC(size) malloc(size)
#define dm_error(m,j) fprintf(stderr, "%s\n", m)
#endif

/***************************************************************
*  Make sure these constants are defined.
***************************************************************/
#ifndef MAX_LINE
#define MAX_LINE 4096
#endif
#ifndef True
#define True 1
#endif
#ifndef False
#define False 0
#endif

/*****************************************************************************

  char_to_argv -   break a char string into an argc/argv

PARAMETERS:

   1.  cmd     -  pointer to char  (INPUT)
                  This is an optional command name.  If non-null,
                  This a malloced copy of this string is put in argv[0];

   2.  args    -  pointer to char  (INPUT)
                  This is the string to be broken up into an argc/argc.
                  This string is not modified.

   3.  dm_cmd  -  pointer to char  (INPUT)
                  This is the name of the DM command being processed.
                  It is used in error messages.

   4.  cmd_string -  pointer to char  (INPUT)
                  This is the name full commmand line being processed (as in a
                  full dm command line.  It is used in error messages.

   5.  argc    -  pointer to int (OUTPUT)
                  This is the returned argc.

   6.  argv    -  pointer to pointer to pointer to char (OUTPUT)
                  The malloced argv is returned in this parm.
                  Since argv is a (char **), the place to stick the
                  (char **) is a (char ***)

FUNCTIONS:
   1.  If the command is name is passed in, malloc a copy and put in in argv[0].

   2.  Walk the input string adding elements to argv and incrementing arg.  If a
       malloc fails, free all we got and return failure.

   3.  Zero out argv[argc].

RETURNED VALUE:

   rc    -     int.
               This return code shows the success or lack of success of the parsing.
               0   -  all is ok
               -1  -  failure.  All malloced storage has been freed.

*********************************************************************************/

int  char_to_argv(char     *cmd,
                  char     *args,
                  char     *dm_cmd,
                  char     *cmd_string, 
                  int      *argc,
                  char   ***argv,
                  char      escape_char)
{
char         *temp_argv[256];
char          temp_parm[MAX_LINE+1];
char         *p;
char         *q;
char          dlm_char;
int           done = False;

#define   AV_FIRSTCHAR        0
#define   AV_COPY             1
#define   AV_WHITESPACE       2
#define   AV_IN_STR           3
#define   AV_IN_STR_KEEPQUOTE 4
#define   AV_DONE             5
int       state = AV_FIRSTCHAR;


*argc = 0;
*argv = NULL;

if (cmd != NULL)
   {
      temp_argv[*argc] = CE_MALLOC(strlen(cmd)+1);
      if (!temp_argv[*argc])
         return(-1);
      strcpy(temp_argv[*argc], cmd);
      (*argc)++;
      temp_argv[*argc] = NULL;
   }


p = args;
while ((*p == ' ') || (*p == '\t')) p++;
q = temp_parm;

while(!done)
{
   /***************************************************************
   *  For escaped characters, throw out the escape and 
   *  blindly copy the next character.
   ***************************************************************/
   if (*p == escape_char)
      {
         p++;
         if (*p)
            *q++ = *p++;
         else
            {
               done = True;
               snprintf(temp_parm, sizeof(temp_parm), "(%s) Trailing escape char ignored (%s)", dm_cmd, cmd_string);
               dm_error(temp_parm, DM_ERROR_BEEP);
            }
      }
   else
      switch(state)
      {
      case AV_FIRSTCHAR:
         if ((*p == '\'') || (*p == '"'))
            {
               dlm_char = *p++;
               state = AV_IN_STR;
            }
         else
            if (*p)
               {
                  *q++ = *p++;
                  state = AV_COPY;
               }
            else
               done = True;
         break;

      case AV_COPY:
         if ((*p == ' ') || (*p == '\t') || (*p == '\0'))
            {
               *q = '\0';
               temp_argv[*argc] = CE_MALLOC(strlen(temp_parm)+1);
               if (!temp_argv[*argc])
                  {
                     free_temp_args(temp_argv);
                     *argc = 0;
                     return(-1);
                  }
               strcpy(temp_argv[*argc], temp_parm);
               (*argc)++;
               temp_argv[*argc] = NULL; /* Make there be a null at the end */
               q = temp_parm;
               temp_parm[0] = '\0';
               if (*p)
                  {
                     state = AV_WHITESPACE;
                     p++;
                  }
               else
                  done = True;
            }
         else
            {
               if ((*p == '\'') || (*p == '"'))
                  {
                     dlm_char = *p;
                     state = AV_IN_STR_KEEPQUOTE;
                  }
               *q++ = *p++;
            }
         break;

      case AV_WHITESPACE:
         if ((*p != ' ') && (*p != '\t'))
            state = AV_FIRSTCHAR;
         else
            p++;
         break;

      case AV_IN_STR:
      case AV_IN_STR_KEEPQUOTE:
         if (*p == dlm_char)
            {
               if (state == AV_IN_STR_KEEPQUOTE)
                  *q++ = *p++;
               else
                  p++;
               state = AV_COPY;
            }
         else
            if (*p)
               *q++ = *p++;
            else
               {
                  snprintf(temp_parm, sizeof(temp_parm), "(%s) command end in string (%s)", dm_cmd, cmd_string);
                  dm_error(temp_parm, DM_ERROR_BEEP);
                  free_temp_args(temp_argv);
                  *argc = 0;
                  return(-1);
               }
         break;

      } /* end switch */

} /* end while */

if (*argc > 0)
   {
      *argv = (char **)CE_MALLOC((*argc + 1) * sizeof(p));
      if (!*argv)
         {
            free_temp_args(temp_argv);
            *argc = 0;
            return(-1);
         }
      else
         memcpy((char *)*argv, (char *)temp_argv, *argc * sizeof(p));
      (*argv)[*argc] = NULL;
   }
else
   *argv = NULL; /* RES 6/22/95 Lint change */

return(0);

} /* end of char_to_argv */


/*****************************************************************************

  free_temp_args -   Free arguments created by char_to_argv

PARAMETERS:

   1.  argv    -  pointer to pointer to char  (INPUT)
                  This is the pointer to the argv array created by
                  char_to_argv.  The array is a null terminated list
                  of pointers.  The array is not freed, but a free is
                  done on each of the pointers in the array.  The caller
                  must free the array.

FUNCTIONS:
   1.  Walk the array of pointers freeing each argument.

*********************************************************************************/


void free_temp_args(char **argv)
{
   while(*argv)
   {
      free(*argv);
      argv++;
   }
} /* end of free_temp_args */




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

/***************************************************************
*
*  module normalize.c
*
*  Routines:
*         normalize_file_name - Take the .. and . qualifiers out of a path.
*         get_full_file_name  - Translate a partial file name to a full path
*         search_path         - Search $PATH for a command
*         translate_tilde     - fixup a file name which starts with tilde (~)
*         get_home_dir        - extract the home directory on different systems
*         evaluate_soft_link  - Turn a soft link into a path name
*         substitute_env_vars - Replace $variables in a buffer
*
***************************************************************/

/*******************************************************************
* System include files
*******************************************************************/

#include <stdio.h>                   /* /usr/include/stdio.h             */
#include <string.h>                  /* /usr/include/string.h            */
#include <errno.h>                   /* /usr/include/errno.h             */
#include <stdlib.h>                  /* /usr/include/stdlib.h            */
#ifndef WIN32
#include <unistd.h>                  /* /usr/include/unistd.h            */
#include <pwd.h>                     /* /usr/include/pwd.h               */
#include <sys/param.h>               /* /usr/include/sys/param.h         */
#endif
#include <ctype.h>                   /* /usr/include/ctype.h             */
#include <sys/types.h>               /* /usr/include/sys/types.h         */
#include <sys/stat.h>                /* /usr/include/sys/stat.h          */
#ifndef MAXPATHLEN
#define MAXPATHLEN	1024
#endif
#ifdef WIN32
#include <io.h>
#define access _access
#endif


/*******************************************************************
* Local include files
*******************************************************************/

#ifdef ARPUS_CE
#include "dmwin.h"
#include "emalloc.h"
#else
#include "malloc.h"
#define CE_MALLOC(size) malloc(size)
#define dm_error(m,j) fprintf(stderr, "%s\n", m)
#endif
#include "normalize.h"
#ifdef PAD
#include "pad.h"
#endif

#if  defined(OpenNT) || defined(WIN32)
int    readlink(char *path, char *link, int maxsize);
#endif

/************************************************************************

NAME:      normalize_file_name - Take the .. and . qualifiers out of a path.

PURPOSE:    This routine takes a file name which may have .
            or .. qualifiers in it and evaluates them out.

PARAMETERS:

   1.  name       -  pointer to char  (INPUT)
                     Pointer to name to be processed.
                     This is a NON NULL-terminated string.

   2.  name_len   -  int (INPUT)
                     The length of parameter name.

   3.  norm_name  -  pointer to char (OUTPUT)
                     A pointer to the place to put the nomalized
                     null terminated name.

FUNCTIONS :

   1.   Put the pieces of the name on a stack.  Keep track
        of leading slashes or double slashes (Apollo).

   2.   Unravel the stack back into a file name getting rid of
        . and .. constructs in the name.

*************************************************************************/

void  normalize_file_name(char    *name,
                          short    name_len,
                          char    *norm_name)
{
   int               start;
   int               n;
   char             *cp;
   char             *comp[80];
   int               i;
   int               leading_slash;
   int               leading_double_slash = 0;
   char              tname[MAXPATHLEN+1];


   /***********************************************
   * The file name may contain ../ or ./, so we break
   * it into the constituent parts, put them on a
   * stack, and canonicalize.  Note this doesn't help
   * link text.
   ***********************************************/

   if (name_len > 0 && name[0] == '/')
      {
         if (name_len > 1 && name[1] == '/')
            leading_double_slash = 1;
         else
            leading_double_slash = 0;
         leading_slash = 1;
      }
   else
      leading_slash = 0;

   /*
   * get a work copy of name,
   *  resolve any $ variables,
   *  and append a '/' to make parsing easier
   */
   strncpy(tname, name, name_len);
   tname[sizeof(tname)-1] = '\0';
   strcpy(&tname[name_len], "/");
   /* Put on stack */
   start = 1;
   n = 0;
   for (cp = tname; *cp; cp++) {
      if (start && *cp != '/') {
         start = 0;
         if (n > 0 && !strncmp (cp, "../", 3))
            n--;
         else if (strncmp (cp, "./", 2))
            comp[n++] = cp;
      } else if (!start && *cp == '/') {
         start = 1;
         *cp = '\0';
      }
   }

   /* intialize the target */
   if (leading_double_slash)
      strcpy(norm_name, "/");
   else
      norm_name[0] = '\0';

   /* Reconstruct from stack */
   if (n > 0)
      for (i = 0; i < n; i++)
      {
         if (i != 0 || leading_slash)
            strcat(norm_name, "/");
         strcat(norm_name, comp[i]);
      }
   else
      strcpy(norm_name, "/");

} /* end of normalize_file_name */


/************************************************************************

NAME:      get_full_file_name - Translate a partial file name to a full path

PURPOSE:    This routine will return the file descriptor to read the
            file to be edited or browsed.

PARAMETERS:
   1.  edit_file  -  pointer to char (INPUT/OUTPUT)
                     This is the name of the file to transform.  This may
                     be a full or partial name.  Full names are not changed.

   2.  follow_link - int (INPUT)
                     This flag specifies whether the routine should follow
                     1 level of soft links.
                     False -  Do not follow soft link
                     True  -  Follow one level of soft link

FUNCTIONS :

   1.   If the edit file starts with a '~', prepend the
        home directory.

   2.   If the edit file does not start with a '/', prepend the
        current working directory.

   3.   normalize the file name to get rid of . and .. constructs.

   4.   If the name is a soft link, resolve it.

*************************************************************************/

void get_full_file_name(char    *edit_file,
                        int      follow_link)
{
char                  link[MAXPATHLEN];
char                 *p;
#if defined(CYGNUS) || defined(WIN32)
char                 *q;
#endif
int                   len;

#if defined(CYGNUS) || defined(WIN32)
/* detect drive letter under Windows 95 or NT */
if ((strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZ", (edit_file[0] & 0xDF)) != NULL) &&
    (edit_file[1] == ':'))
   strcpy(link, edit_file);
else
   if ((edit_file[0] != '/') && (edit_file[0] != '\\'))
#else
if (edit_file[0] != '/')
#endif
   {
      if (edit_file[0] == '~')
         {
            p = translate_tilde(edit_file);
            if (p == NULL)
               return;
            else
               strcpy(link, edit_file);
         }
      else
#ifdef PAD
         if (ce_getcwd(link, sizeof(link)) == NULL)
#else
         if (getcwd(link, sizeof(link)) == NULL)
#endif
            {
               snprintf(link, sizeof(link), "Can't get current working dir (%s)", strerror(errno));
               dm_error(link, DM_ERROR_BEEP);
               return;
            }
         else
            {
               if (link[strlen(link)-1] != '/')  /* make sure we are not in the root directory */
                  strcat(link, "/");
               strcat(link, edit_file);
            }
   }
else
   {
      strcpy(link, edit_file);
#if defined(CYGNUS) || defined(WIN32)
      /***************************************************************
      *  Check for CYGNUS style names, like  //C/bin
      *  Convert to DOS form, like           C:\bin
      *
      ***************************************************************/
      if ((link[0] == '/') && (link[1] == '/') && 
          (strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZ", (link[2] & 0xDF)) != NULL) &&
          (link[3] == '/'))
         {
            link[0] = link[2];   /* //C -> C  */
            link[1] = ':';       /*     -> C: */
            p = &link[3];
            q = &link[2];
            while (*p)
               if (*p == '/')
                  *q++ = '\\', p++;
               else
                  *q++ = *p++;
            *q = '\0';
         }
#endif
   }

/***************************************************************
*
*  Get rid of any . or .. constructs in the file name.
*
***************************************************************/

normalize_file_name(link, (short)strlen(link), edit_file);

 /***************************************************************
 *
 *  If the name is a soft link, process it to get the real name.
 *
 ***************************************************************/

if (follow_link)
   {
#if  defined(OpenNT) || defined(WIN32)
      len = -1; /* no soft links */
#else
      len = readlink(edit_file, link, sizeof(link));
#endif
      if (len >= 0)
         {
            link[len] = '\0';
            evaluate_soft_link(edit_file, link); /* modifies edit_file */
         }
   }

} /* end of get_full_file_name */


/************************************************************************

NAME:      search_path -  Search $PATH

PURPOSE:    This routine checks the pieces of $PATH to see if a passed
            command name exists.

PARAMETERS:

   1.  cmd           -  pointer to char (INPUT)
                        This is the command name to look for.
                        It may be a full path, a relative path, or
                        a command name.

   2.  fullpath      -  pointer to char (OUTPUT)
                        The full path name of the command is placed in
                        this variable.  If the command is not found or
                        is not executeable, the string is set to a zero
                        length string.

   3.  env_path_var  -  pointer to char (INPUT)
                        This is the name of the PATH environment variable
                        to search.  Normally it points to the string "PATH".
                        For cmdf processing, it points to "CE_CMDF_PATH".

   4.  separator     -  char (INPUT)
                        This is the separator character.  On UNIX systems
                        this is normally ':'  on windows it is normally ';'.

FUNCTIONS :

   1.   If the passed cmd is a fully qualified path, make sure it is executable
        and not a directory.

   2.   If we do not already have the $PATH variable, get it from the environment.

   3.   Use Malloc to make a copy of this variable for hacking on.

   4.   Chop the Path variable into its pieces and use the pieces to check the
        existence and executablity of the command.

   5.   If the path is found, return it.  Otherwise set the output string to zero length.


*************************************************************************/

void search_path(char    *cmd,
                 char    *fullpath,
                 char    *env_path_var,
                 char     separator)
{
search_path_n(cmd,
              MAXPATHLEN,
              fullpath,
              env_path_var,
              separator);
} /* end of search_path */

void search_path_n(char    *cmd,
                   int      max_fullpath,
                   char    *fullpath,
                   char    *env_path_var,
                   char     separator)
{
static char          *current_working_dir = NULL;
char                 *env_path;
int                   env_path_len;
char                 *work_path;
char                 *start;
char                 *end;
int                   rc;
int                   len;
struct stat           file_stats;
#ifdef WIN32
static char           path_delim = '\\';
#else
static char           path_delim = '/';
#endif

*fullpath = '\0';

/***************************************************************
*
*  Check for a leading Tilde
*
***************************************************************/

if (cmd[0] == '~')
   {
      strcpy(fullpath, cmd);
      translate_tilde(fullpath);
      if (access(fullpath, 01) == 0)
         {
            rc = stat(fullpath, &file_stats);
            if (rc != 0 || ((file_stats.st_mode & S_IFDIR) == S_IFDIR ))
               *fullpath = '\0';
         }
      return;
   }

/***************************************************************
*
*  First check for the case where cmd is a fully qualified path
*  name.  Make sure it is executable and not a directory.
*
***************************************************************/

#if defined(CYGNUS) || defined(WIN32)
/* detect drive letter under Windows 95 or NT */
if ((strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZ", (cmd[0] & 0xDF)) != NULL) &&(cmd[1] == ':'))
   {
      end = strrchr(cmd, '.');
      if ((end && (strcmp(end, ".EXE") == 0)) || (access(cmd, 01) == 0))
         strcpy(fullpath, cmd);
      return;
   }
#endif

/***************************************************************
*
*  First check for the case where cmd is a fully qualified path
*  name.  Make sure it is executable and not a directory.
*
***************************************************************/
#ifdef WIN32
if ((cmd[0] == '/') || (cmd[0] == '\\') || (cmd[0] == '.'))
#else
if ((cmd[0] == '/') || (cmd[0] == '.'))
#endif
   {
      if (access(cmd, 01) == 0)
         {
            rc = stat(cmd, &file_stats);
            if (rc == 0 && !((file_stats.st_mode & S_IFDIR) == S_IFDIR ))
               strcpy(fullpath, cmd);
         }
      return;
   }


/***************************************************************
*
*  Get the current working directory and the path.
*
***************************************************************/

if (current_working_dir == NULL)
   {
#ifdef PAD
      current_working_dir = ce_getcwd(NULL, MAXPATHLEN);
#else
      current_working_dir = getcwd(NULL, MAXPATHLEN);
#endif
      if (current_working_dir == NULL)
         return;
   }

if ((env_path_var == NULL) || (*env_path_var == '\0'))
   env_path_var = "PATH";

env_path = getenv(env_path_var);
if (env_path == NULL)
   return;
env_path_len = strlen(env_path);

/***************************************************************
*
*  Get a copy of the path to chop up.
*
***************************************************************/

work_path = CE_MALLOC(env_path_len + 2);
if (!work_path)
   return;

strcpy(work_path, env_path);
len = strlen(work_path);
work_path[len] = separator;
work_path[len+1] = '\0';

/***************************************************************
*
*  Walk through the path looking for the command being marked
*  executable and not a directory.
*
***************************************************************/

for (start = work_path, end = strchr(start, separator);
     end != NULL;
     start = end + 1,   end = strchr(start, separator))
{
   *end = '\0';

   if (start == end)
      snprintf(fullpath, max_fullpath, "%s%c%s", current_working_dir, path_delim, cmd);
   else
      if (*(end-1) == path_delim)
         snprintf(fullpath, max_fullpath, "%s%s", start, cmd);
      else
         snprintf(fullpath, max_fullpath, "%s%c%s", start, path_delim, cmd);


   if (access(fullpath, 01) == 0)
      {
         rc = stat(fullpath, &file_stats);
         if (rc == 0 && !((file_stats.st_mode & S_IFDIR) == S_IFDIR ))
            break;
         else
            *fullpath = '\0';
      }
   else
      *fullpath = '\0';
}

free(work_path);

}  /* end of search_path_n */


/************************************************************************

NAME:      translate_tilde - translate a name starting with a tilde to start with
                             the home directory.

PURPOSE:    This routine will take a name like ~/.profile and expand it into
            a full path name.


PARAMETERS:

   1.  file      - pointer to char (INPUT / OUTPUT)
                   This is the file name to be processed.  It should start
                   with a tilde (~).  It must be long enough to accomadate
                   the home directory prefixed onto the existing file name.
                   Length MAXPATHLEN or 1024 is recommended.


FUNCTIONS :

   1.   Make sure there is a tilde on the front of the name, if not, just return.

   2.   Get the home directory, if we cannot return null.

   3.   Put the file name at the end of the home directory name and copy the result
        back into the parameter named file.


RETURNED VALUE:
   NULL   -   Could not get a home directory path
   file   -   ON success, parm file is returned.


*************************************************************************/

char *translate_tilde(char  *file)
{

char                  homedir[MAXPATHLEN];
char                 *p;
char                 *user_end;
char                 user[256];

/***************************************************************
*  If the file names starts with a tilde, we are interested in it.
***************************************************************/
if (file[0] == '~')
   {
      if (file[1] == '/')
         {
            /***************************************************************
            *  Normal case ~/.profile - translate the ~ to this users home dir
            ***************************************************************/
            p = get_home_dir(homedir, NULL);
            if (p == NULL)
               {
                  dm_error("Can't get home directory from environment", DM_ERROR_BEEP);
                  file = NULL;
               }
            else
               {
                  if (homedir[strlen(homedir)-1] != '/' && file[1] != '/')  /* make sure we need the slash */
                     strcat(homedir, "/");
                  strcat(homedir, &file[1]);
                  strcpy(file, homedir);
               }
         }
      else
         {
            /***************************************************************
            *  Case ~user/.profile - translate the ~user to the home dir for
            *  user user.
            ***************************************************************/
            user_end = strchr(file, '/');
            if (user_end != NULL)
               {
                  if ((user_end - file) >= sizeof(user))
                      user_end = file + (sizeof(user)-1);
                  strncpy(user, &file[1], (user_end-file)-1);
                  user[(user_end-file)-1] = '\0';
               }
            else
               {
                  strncpy(user, &file[1], sizeof(user));
                  user[sizeof(user)-1] = '\0';
                  user_end = &file[strlen(file)];
               }

            p = get_home_dir(homedir, user);
            if (p == NULL)
               {
                  snprintf(homedir, sizeof(homedir), "Can't get home directory for %s from environment", user);
                  dm_error(homedir, DM_ERROR_BEEP);
                  file = NULL;
               }
            else
               {
                  if (homedir[strlen(homedir)-1] != '/' && *user_end != '/')  /* make sure we need the slash */
                     strcat(homedir, "/");
                  strcat(homedir, user_end);
                  strcpy(file, homedir);
               }
         }
   }

return(file);

} /* end of translate_tilde */


/************************************************************************

NAME:      get_home_dir - extract the home directory on different systems

PURPOSE:    This routine will find the equivalent of $HOME on different
            systems.  It looks at several variables and the /etc/passwd
            data.  If it fails, it returns NULL.


PARAMETERS:

   1.  dest      - pointer to char (OUTPUT)
                   The HOME directory name is copied to this area.

   2.  user      - pointer to char (INPUT)
                   If this pointer is non-null and points to a string,
                   the home directory for this userid is returned.
                   Otherwise the current userid is returned.


FUNCTIONS :

   1.   If environment variable $HOME exists, use it.

   2.   If environment variable $USER exists, use it.

   3.   Look in the passwd information to see if a HOME directory is specified
        there.  If so, use it.


RETURNED VALUE:
   NULL   -   Could not get a home directory path
   dest   -   ON success, parm dest is returned.


*************************************************************************/

char *get_home_dir(char *dest,
                   char *user)
{
   char            *ptr;

#ifndef WIN32
   uid_t            uid;
#ifndef CYGNUS
   extern struct    passwd *getpwuid();  /* have to leave out prototype for IBM */
#endif
   struct passwd   *pw;

   dest[0] = '\0';  /* null it out in case of failure */

   if (user && *user)
      {
         pw = getpwnam(user);
         if (pw)
            strcpy(dest, pw->pw_dir);
         else
            *dest = '\0';
      }
   else
      if ((ptr = getenv("HOME")) != NULL)
         strcpy(dest, ptr);
      else
         {
            if ((ptr = getenv("USER")) != NULL)
               pw = getpwnam(ptr);
            else
               {
                  uid = getuid();
                  pw = getpwuid(uid);
               }

            if (pw)
               strcpy(dest, pw->pw_dir);
            else
               *dest = '\0';
         }

   if (*dest != '\0')
      return(dest);
   else
      return(NULL);
#else
/* On windows NT, the default home directory is the U:
   drive, by default.  If the U: drive does not exist,
   use the C: drive
*/
if ((ptr = getenv("HOME")) != NULL)
   strcpy(dest, ptr);
else
   strcpy(dest, "U:");
return(dest);
#endif

} /* end of get_home_dir  */


/************************************************************************

NAME:      evaluate_soft_link

PURPOSE:   This modifies a path name to follow a soft link.

PARAMETERS:
   1.   real_path  -  pointer to char (INPUT/OUTPUT)
                      This is the path name to be checked.  This
                      name is replaced or modified based on the
                      contents of the soft link being processed.

   2.   link       -  pointer to char (INPUT)
                      This is the contents of the soft link found
                      at the path name found in real_path

FUNCTIONS :

   1.   See if this is an Apollo link to a node, a fully qualified link
        which starts with a / or a relative link.

   2.   For fully qualified links, replace the hard path with the link
        contents.

   3.   For relative links, replace the last qualifier of the hard path
        with the link contents and normalize the name to get rid of
        references to . and .. directories.


*************************************************************************/

void evaluate_soft_link(char  *real_path,  /* input/output */
                        char  *link)       /* input        */
{
char   work_path[MAXPATHLEN];
char  *p;

/***************************************************************
*
*  Check to see if the link is a fully qualified link,
*  or a relative link.
*
*  For a fully qualified link, copy the real path with the
*  link contents.
*
*  For a relative link, replace the last qualifier with the link
*  contents and normalize the file name to take care of . and ..
*  type qualifiers.
*
***************************************************************/

#if defined(CYGNUS) || defined(WIN32)
/* detect drive letter under Windows 95 or NT */
if ((strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZ", (link[0] & 0xDF)) != NULL) &&(link[1] == ':'))
   strcpy(real_path, link);
else
#endif

if (link[0] == '/')
   strcpy(real_path, link);
else
   {
      strcpy(work_path, real_path);
      p = strrchr(work_path, '/');
      if (p != NULL) /* this always works */
         *(++p) = '\0';

      strcat(work_path, link);
      normalize_file_name(work_path, (short)strlen(work_path), real_path);
   }

} /* end of evaluate_soft_link */


/************************************************************************

NAME:      substitute_env_vars

PURPOSE:   This routine performs synbolic substitution of $variables
           in line passed.  Values are taken from the environment

INPUT PARAMETERS:

   1.   buff           - pointer to char string (INPUT/OUTPUT)
                         This is the line in to be processed.
                         It is modified as neccessary.

   2.   max_len       -  int (INPUT)
                         This is the maximum length of the passed input
                         buffer.  If a substitution would cause this
                         value to be exceeded, the substitution is not
                         performed and the overflow flag is returned.

   3.   escape_char   -  pointer to char string (INPUT)
                         This is the value to use as the escape
                         character when parsing the string


FUNCTIONS :

   1.   Scan the string looking for dollar signs.

   2.   If the $sign is preceded by an escape character, ignore it.

   3.   Gather the variable which follows the $ and note its end.

   4.   If the $variable is surrounded by double quotes, include
        these in the $variable area to be replaced.

   5.   Find the value for the named variable using getenv.

   6.   Replace the $variable by its value.


RETURNED VALUE:
   overflow    -   ing flag
                   True  -  Overflow of the target string occurred, one or more substitutions not performed
                   False -  All substitutions performed

*************************************************************************/

int  substitute_env_vars(char     *buff,
                         int       max_len,
                         char      escape_char)
{

/*char        front[4096];  RES 6/23/95 Lint change */
char        back[4096];
char       *dollar_pos;
char       *var_end;
char       *buff_end;
char       *p;
char        var_name[128];
char       *var_ptr;
char       *var_value;
int         var_len;
int         overflow = 0;

dollar_pos = buff;

while ((dollar_pos = strchr(dollar_pos, '$')) != NULL)
{
   buff_end   = &buff[strlen(buff)-1];

   /***************************************************************
   *  If we have \$ then shift out the backslash and ignore the $
   ***************************************************************/
   if (dollar_pos > buff && dollar_pos[-1] == escape_char)
      {
         for (p = dollar_pos; *p != '\0'; p++)
            p[-1] = p[0];
         p[-1] = '\0';
      }
   else
      {
         /***************************************************************
         *  Copy the variable from the buffer.  Watch out for overflow
         ***************************************************************/
         memset(var_name, 0, sizeof(var_name));
         var_ptr = var_name;
         for (var_end = dollar_pos + 1;
              (isalnum(*var_end) || (*var_end == '_')) && (var_ptr-var_name < sizeof(var_name));
              var_end++)
            *var_ptr++ = *var_end;

         /***************************************************************
         *  Get the value of the variable
         ***************************************************************/
          var_value = getenv(var_name);
          if (var_value != NULL)
            {
               /***************************************************************
               *  Watch for $var in double quotes
               ***************************************************************/
               if (dollar_pos > buff && dollar_pos[-1] == '"' && *var_end == '"')
                  {
                     var_end++;
                     dollar_pos--;
                  }

               /***************************************************************
               *  If the string is shorter than the variable name, do the
               *  substitute in place.  Else, copy off the back part of the
               *  buffer, put in the value and append the buffer back on.
               ***************************************************************/
                var_len = strlen(var_value);
                if (var_len <= var_end - dollar_pos)
                   {
                      strncpy(dollar_pos, var_value, var_len);
                      strcpy(dollar_pos + var_len, var_end);
                   }
                else
                   if (((dollar_pos-buff) + var_len + (buff_end-var_end)) < max_len)
                      {
                         strcpy(back, var_end);
                         strcpy(dollar_pos, var_value);
                         strcpy(dollar_pos + var_len, back);
                      }
                   else
                      overflow = -1;

               /***************************************************************
               *  Position past the variable value.
               ***************************************************************/
               dollar_pos += var_len;
            }
         else
            dollar_pos++;
      }
} /* end of while dollar_pos != NULL */

return(overflow);

} /* end of substitute_env_vars */





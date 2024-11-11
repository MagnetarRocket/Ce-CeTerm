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
*  module alias.c
*
*  Routines:
*         lookup_alias         - Check the hash table for an alias name.
*         alias_dollar_vars    - Process the dollar sign variables in an alias.
*         dm_alias             - Do the alias command
*         is_alias_key         - Test is a keykey is an alias keykey
*
*  Local:
*         all_args             - Put all the arguments in one string
*
*  Overview of alias operations
*     The alias command is of the form
*     alias <name> "list of dm commands" ke
*     the list of commands may contain $1, $2, and so on as
*     positional parameters.
*     The alias command is parsed as a normal command and
*     a command structure built.  
*     When the alias command is executed, the name is entered
*     into the hash table with the KEYKEY type set to ALIAS_HASH_TYPE
*     to distinquish it from normal keydefs.
*     It is flattened and put in the X server keydefs just like
*     a new key def.  Alias commands can be put in the .Cekeys file.
*     Thus the format of an alias KEYKEY is 8 bytes of alias name
*     (padded with blanks, not null terminated), 8 bytes of character zeroes,
*     4 types of ALIAS_HASH_TYPE.  The entire string is NUL terminated.
*
*     When parsedm finds a command it does not recognize, it tries
*     to look it up as an alias.  If one is found, a special value
*     for parsing the alias is set and the parsedm code calls a
*     routine which does symbolic substitution on the alias for 
*     the $1, $2, .. variables and calls parsedm_line recursively to
*     parse the new command string.  Recursion is detected when the
*     command string exceeds MAXLINE chars or a recursion depth of 
*     MAX_ALIAS_RECURSION is reached.
*
*     Alias names must be all alphabetic, just like regular DM commands.
*     The maximum length is 8.
*
*     The alias command structure is used 2 ways.  
*     First, when it is executed the first time, dm_alias is called and
*     the alias is installed.  The dm_alias routine installs the
*     actual alias command structure into the hash table.  When the
*     structure is retrieved by the parser, it knows to take the data
*     out of the stucture to process.  This makes flattening of the
*     structure for the X server easier.  The same structure could be
*     under a key definition or an alias.  The alias command gets
*     executed when the user press the key for the key definition.
*     The alias gets expanded when the parser looks for the command
*     created by the alias.
*
*     Parsing of the alias command can hopefully be done via the
*     code which parses the kd command.
*
***************************************************************/


#include <stdio.h>           /* /usr/include/stdio.h  */
#include <string.h>          /* /usr/include/string.h  */
#include <errno.h>           /* /usr/include/errno.h /usr/include/sys/errno.h */
#include <ctype.h>           /* /usr/include/ctype.h */
#if !defined(OMVS) && !defined(WIN32)
#include <sys/param.h>       /* /usr/include/sys/param.h */
#endif
#ifndef MAXPATHLEN
#define MAXPATHLEN	1024
#endif

#ifdef WIN32
#ifdef WIN32_XNT
#include "xnt.h"
#else
#include "X11/Xlib.h"        /* /usr/include/X11/Xlib.h */
#endif
#include <stdlib.h>
#else
#include <X11/Xlib.h>        /* /usr/include/X11/Xlib.h */
#endif


/***************************************************************
*  
*  Special value put in the hash table type field for aliases
*  
***************************************************************/

#define ALIAS_HASH_TYPE   0x7323
#define ALIAS_KEY_FORMAT  "%-8.8s00000000%04X"
#define ALIAS_READ_FORMAT "%8s 00000000%4X"



/* get the real copy of the global variable in kd.h */
#define KD_C 1

#include "alias.h"
#include "debug.h"
#include "dmsyms.h"
#include "dmwin.h"
#include "emalloc.h"
#include "hsearch.h"
#include "kd.h"
#include "parsedm.h"


/************************************************************************

NAME:      lookup_alias  -   Check the hash table for an alias name.



PURPOSE:    This routine handles the lookup of names which may be aliases.
            When the parser cannot find a name, it checks with this routine
            to see if there is an alias defined for this name.

PARAMETERS:

   1.   name     -  pointer to char  (INPUT)
                    This is the alias name to be looked up.

   2.   hash_table - pointer to void (INPUT)
                     This is the hash table pointer for the current display.

FUNCTIONS :

   1.  If the name passed is longer than MAX_ALIAS_NAME characters,
       return NULL.

   2.  Create the hash table key for the alias name.
       name padded to 8 with blanks.
       8 bytes of character zero's
       ALIAS_HASH_TYPE

   3.  Look up the name in the hash table, if the lookup is successful, 
       return the string.  Otherwise return NULL.


RETURNED VALUE:
   expansion  -  pointer to char
                 The returned value is the string the alias translates into.
                 NULL is returned if there is no translation.
                 This is the name from the table and should not be modified.

*************************************************************************/

char     *lookup_alias(char   *name,
                       void   *hash_table)
{


ENTRY                 sym_to_find;
ENTRY                *def_found;
char                  the_key[KEY_KEY_SIZE];
DMC                  *dmc;


/***************************************************************
*  Check for a valid name being passed.
***************************************************************/
if (name && *name)
   {
      if ((int)strlen(name) > MAX_ALIAS_NAME)
         {
            DEBUG11(fprintf(stderr, "Alias name \"%s\" too long\n", name);)
            return(NULL);
         }
   }
else
   return(NULL);

snprintf(the_key, sizeof(the_key), ALIAS_KEY_FORMAT, name, ALIAS_HASH_TYPE);
DEBUG11(fprintf(stderr, "lookup_alias:  key \"%s\" ", the_key);)

sym_to_find.key = the_key;

if ((def_found = (ENTRY *)hsearch(hash_table, sym_to_find, FIND)) != NULL)
   {
      /***************************************************************
      *  Found a definition.
      ***************************************************************/
      if ((def_found->data == NULL) ||
          ((DMC *)def_found->data == DMC_COMMENT))
         {
             DEBUG11(fprintf(stderr, " - null definition found\n");)
             return(NULL);
         }
      else
         {
            dmc = (DMC *)def_found->data;
            if (!(dmc->alias.def) || !(*dmc->alias.def))
               {
                   DEBUG11(fprintf(stderr, " - null alias found\n");)
                   return(NULL);
               }
            else
               {
                   DEBUG11(fprintf(stderr, " - alias found\n");)
                   return(dmc->alias.def);
               }
         }
   }
else
   {
      DEBUG11(fprintf(stderr, " - No translation\n");)
      return(NULL);
   }

} /* end of lookup_alias */


/************************************************************************

NAME:      all_args - Put all the arguments in one string


PURPOSE:    This routine takes the alias expansion string and the argc/argv used
            to call it and replaces the $1, $2, and so on with the parameter
            values.

PARAMETERS:

   1. argc          - int (INPUT)
                      This is the count of arguments passed.

   2. argv          - pointer to pointer to char (INPUT)
                      Thes are the arguments passed to the alias.

   3. quotes        - int (INPUT)
                      This flag specifies that each arg is t

FUNCTIONS :
   1.   Copy each argument to a single character string.  Watch the end of the string

   2.   If the quote flag is true, surround each argument in double quotes

RETURNED VALUE:
   ret          -  char string
                   The concatination of all the arguments is returned in
                   a static character string.

*************************************************************************/

static char  *all_args(int   argc,  char *argv[], int quotes)
{
static char       ret[4096];
char             *end = &ret[4096];
char             *p = ret;
int               len;
int               minus;
int               i;

ret[0] = '\0';
if (quotes)
   minus = 3;
else
   minus = 1;

for (i = 1; i < argc; i++)
{
   len = strlen(argv[i]);
   if ((end - p) > (len - minus))
      {
         if (p != ret)
            *p++ = ' ';
         if (quotes)
            *p++ = '"';
         strcpy(p, argv[i]);
         p += len;
         if (quotes)
            {
               *p++ = '"';
               *p = '\0';
            }
      }
   else
      {
         dm_error("(alias) Total length of args > 4096, truncated", DM_ERROR_BEEP);
         break;
      }
}

return(ret);

}  /* end of all_args */


/************************************************************************

NAME:      alias_dollar_vars - Process the dollar sign variables in an alias.


PURPOSE:    This routine takes the alias expansion string and the argc/argv used
            to call it and replaces the $1, $2, and so on with the parameter
            values.

PARAMETERS:

   1. inbuff       -  pointer to char  (INPUT)
                      This is the alias expansion before substitution.
                      It is not modified.  It contains the $1, $2, and so on.

   2. argc          - int (INPUT)
                      This is the count of arguments passed.

   3. argv          - pointer to pointer to char (INPUT)
                      Thes are the arguments passed to the alias.

   4. escape_char   - char (INPUT)
                      This is the current escap character.

   5. maxout        - int (INPUT)
                      This is the maximum length of the output buffer

   6. outbuff       - pointer to char (OUTPUT)
                      The command string with substititions done is
                      returned in this parameter.  The caller supplies
                      the target buffer.

FUNCTIONS :


   1.   Scan the string looking for dollar signs.

   2.   If the $sign is preceded by an escape character, ignore it.

   3.   Gather the variable which follows the $ and note its end.

   4.   If the $variable is surrounded by double quotes, include
        these in the $variable area to be replaced.

   5.   Find the value for the named variable from the passed argv

   6.   Replace the $variable by its value.

RETURNED VALUE:
   overflow    -   ing flag
                   True  -  Overflow of the target string occurred, one or more substitutions not performed
                   False -  All substitutions performed

*************************************************************************/

int   alias_dollar_vars(char     *inbuff,
                        int       argc,
                        char     *argv[],
                        char      escape_char,
                        int       max_out,
                        char     *outbuff)
{
char        back[4096];
char       *dollar_pos;
char       *var_end;
char       *buff_end;
char       *p;
char        var_name[128];
char       *var_ptr;
char       *var_value;
int         var_idx;
int         var_len;
int         overflow = 0;
char        msg[512];

strncpy(outbuff, inbuff, max_out);
outbuff[max_out-1] = '\0';

dollar_pos = outbuff;

while ((dollar_pos = strchr(dollar_pos, '$')) != NULL)
{
   buff_end   = &outbuff[strlen(outbuff)-1];

   /***************************************************************
   *  If we have \$ then shift out the backslash and ignore the $
   ***************************************************************/
   if (dollar_pos > outbuff && dollar_pos[-1] == escape_char)
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
              isdigit(*var_end) && (var_ptr-var_name < sizeof(var_name));
              var_end++)
            *var_ptr++ = *var_end;

         /***************************************************************
         *  Get the value of the variable
         ***************************************************************/
         if ((var_name[0] == '\0') && ((dollar_pos[1] == '*') || (dollar_pos[1] == '@')))
            {
               var_value = all_args(argc, argv, (dollar_pos[1] == '@'));
               var_end++; /* skip over the * or @ */
            }
         else
            if (var_name[0] != '\0')
               {
                  var_idx = atoi(var_name);
                   if ((var_idx < argc) && (var_idx >= 0))
                      var_value = argv[var_idx];
                   else
                      var_value = "";
                }
             else
                var_value = NULL;
          if (var_value != NULL)
            {
               /***************************************************************
               *  Watch for $var in double quotes
               ***************************************************************/
               if (dollar_pos > outbuff && dollar_pos[-1] == '"' && *var_end == '"')
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
                   if (((dollar_pos-outbuff) + var_len + (buff_end-var_end)) < max_out)
                      {
                         strcpy(back, var_end);
                         strcpy(dollar_pos, var_value);
                         strcpy(dollar_pos + var_len, back);
                      }
                   else
                      {
                         snprintf(msg, sizeof(msg), "(%s) expansion of argument $%s exceeds max of %d characters", argv[0], var_value, max_out);
                         dm_error(msg, DM_ERROR_BEEP);
                         overflow = -1;
                      }

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


}  /* end of alias_dollar_vars */


/************************************************************************

NAME:      dm_alias  - Execute an alias command structure.

PURPOSE:    This routine "does" a alias.  It causes the alias to be installed
            into this Ce session and possibly into the global keydef property.

PARAMETERS:

   1. dmc          -  pointer to DMC  (INPUT)
                      This the first DM command structure for the key
                      definition.  If there are more commands on the
                      list, they are ignored by this routine.  That is this
                      routine does not look at dmc->next.

   2. update_server - int (INPUT)
                      This flag specifies whether the key defintion should be
                      updated in the server.  It is set to False during
                      initial keydef loading and True for interactive key
                      definition commands.

   3. dspl_descr    - pointer to DISPLAY_DESCR (INPUT)
                      This is the display description for the current
                      display.  It is needed for the hash table and to
                      update the server definitions.

FUNCTIONS :
   1.  Parse the keyname to the the keysym, event, and the modifier mask.

   2.  If the data in the key definition is NULL, this is a request for
       data about that key definition.  Look up the definition and output
       it to the DM command window via dump_kd.

   3.  Prescan the defintion to see if it contains any prompts for input.
       such as   kd *h  cv /ce/help &'Help on: '  ke

   4.  If the prescan produced no results, parse the key definition.

   5.  If the parse worked, install the keydef.

RETURNED VALUE:
   rc   -  int
           The returned value shows whether a key definition was successfully
           loaded.
           0   -  keydef loaded
           -1  -  keydef not loaded, message already generated

*************************************************************************/

int   dm_alias(DMC               *dmc,
               int                update_server,
               DISPLAY_DESCR     *dspl_descr)
{
int          rc;
DMC         *new_dmc;
ENTRY       *def_found;
ENTRY        sym_to_find;
char         the_key[KEY_KEY_SIZE];
char         work[80];
char         msg[512];
char        *p;

/***************************************************************
*  Check for a valid name being passed.
***************************************************************/
if (dmc->alias.key && *(dmc->alias.key))
   {
      if ((int)strlen(dmc->alias.key) > MAX_ALIAS_NAME)
         {
            snprintf(msg, sizeof(msg), "(alias) Alias name \"%s\" too long", dmc->alias.key);
            dm_error(msg, DM_ERROR_BEEP);
            return(-1);
         }
      p = dmc->alias.key;
      while(*p && isalpha(*p))
         p++;
      if (*p)
         {
            snprintf(msg, sizeof(msg), "(alias) Alias name \"%s\" contains non-alpha characters", dmc->alias.key);
            dm_error(msg, DM_ERROR_BEEP);
            return(-1);
         }
   }
else
   {
      strcpy(msg, "(alias) Null alias name\n");
      dm_error(msg, DM_ERROR_BEEP);
      return(-1);
   }

/***************************************************************
*  Build the key from the key name.
***************************************************************/

snprintf(the_key, sizeof(the_key), ALIAS_KEY_FORMAT, dmc->alias.key, ALIAS_HASH_TYPE);
DEBUG11(fprintf(stderr, "dm_alias:  key \"%s\" ", the_key);)

if (dmc->alias.def == NULL)  /* request for data */
   {
      sym_to_find.key = the_key;
      def_found = (ENTRY *)hsearch(dspl_descr->hsearch_data, sym_to_find, FIND);
      if ((def_found != NULL) &&
          (def_found->data != NULL) &&
          ((DMC *)def_found->data != DMC_COMMENT))
         {
            new_dmc = (DMC *)def_found->data;
            if (new_dmc->alias.def)
               p =  new_dmc->alias.def;
            else
               p = "";
         }
      else
         p = "";
      dm_error(p, DM_ERROR_MSG);
      return(-1);  /* not really failure, but no alias was loaded */
   }

/***************************************************************
*  Make a copy of the alias DMC to save in the hash table.
***************************************************************/
new_dmc = (DMC *)CE_MALLOC(sizeof(DMC));
if (new_dmc)
   {
      memset((char *)new_dmc, 0, sizeof(DMC));
      new_dmc->alias.cmd     = dmc->alias.cmd;
      new_dmc->alias.temp    = False;
      new_dmc->alias.key     = malloc_copy(dmc->alias.key);
      new_dmc->alias.def     = malloc_copy(dmc->alias.def);
      new_dmc->alias.line_no = dmc->alias.line_no;
      if (!(new_dmc->alias.key) || !(new_dmc->alias.def))
         {
            if (new_dmc->alias.key)
               free((char *)new_dmc->alias.key);
            if (new_dmc->alias.def)
               free((char *)new_dmc->alias.def);
            free((char *)new_dmc);
            new_dmc = NULL;
         }
   }

if (new_dmc != NULL)
   {
      DEBUG4(
         fprintf(stderr, "Installing:  ");
         dump_kd(stderr, new_dmc, 0, False, NULL, dspl_descr->escape_char);
      )
      /***************************************************************
      *  Install the alias in the hash table as if it was a keydef.
      *  Update the server if required.  The hash key prevents
      *  confusion between key definitions and aliases.
      ***************************************************************/
      rc = install_keydef(the_key, new_dmc, update_server, dspl_descr, dmc);
      if (rc != 0)
         {
            snprintf(work, sizeof(work), "(%s) Unable to install key definition, internal error", dmsyms[dmc->kd.cmd].name);
            dm_error(work, DM_ERROR_LOG);
            free_dmc(new_dmc);
            rc = -1;  /* make sure it has the expected value */
         }
   }
else
   rc = -1;

return(rc);

}  /* end of dm_alias */


/************************************************************************

NAME:      is_alias_key  - Test is a keykey is an alias keykey

PURPOSE:    This routine flags the keykey (from the hash table)
            so no attempt is made to use it as a real X keysym.

PARAMETERS:

   1. keykey       -  pointer to char  (INPUT)
                      This the keykey to be tested.

FUNCTIONS :

   1.  Parse the keykeym and verify that the last piece the special value.

RETURNED VALUE:
   rc   -  int
           The returned value shows the keykey was for an alias
           False  -  Not an alias keykey
           True   -  Is an alias keykey

*************************************************************************/

int   is_alias_key(char          *keykey)
{
char         alias_name[10];
int          key_type;
int          i;

i = sscanf(keykey, ALIAS_READ_FORMAT, alias_name, &key_type);
if ((i != 2) || (key_type != ALIAS_HASH_TYPE))
   return(False);
else
   return(True);

} /* end of is_alias_key */


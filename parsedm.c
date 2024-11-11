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
*  Routines in parsedm.c
*
*     parse_dm_line    -  parse a line with DM commands
*     free_dmc         -  Free a DMC command structure.
*     dump_kd          -  Format and print a DMC command structure.
*     iln              -  special itoa for line_numbers
*     char_to_argv     -  convert a command line to an argc/argv 
*     add_escape_chars -  Add escape characters to a line
*     copy_with_delim  -  Copy a file taking into account escape chars and a delimiter (internal)
*
*  INTERNAL
*     parse_dm         -  parse one DM command (internal)
*     copy_kd          -  Copy the data from a key definition.
*     get_num_range    -  Get from,to numbers with [] or ().
*     count_tokens     -  Count the number of tokens on a line
*     comment_scan     -  scan a line for comment start
*     free_temp_args   -  Utility routine used by char_to_argv
*     shift_out_arg    -  Utility to manipulate argc/argv
*     hexparm_to_value -  Convert a parm which is a hex char representation to the value.
*     cse_message      -  Generate a command syntax error message.
*
***************************************************************/

#include <stdio.h>           /* /usr/include/stdio.h  */
#include <string.h>          /* /usr/include/string.h  */
#include <ctype.h>           /* /usr/include/ctype.h  */

#define itoa(v, s) ((sprintf(s, "%d", (v))), (s))

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "alias.h"
#include "debug.h"
#include "dmc.h"
#include "dmsyms.h"
#include "dmwin.h"
#include "emalloc.h"
#include "parsedm.h"
#include "prompt.h"
#include "str2argv.h"
#include "xc.h"

#ifndef HAVE_STRLCPY
#include "strl.h"
#endif

/***************************************************************
*  Use 1 past the last devifed command for processing alias
*  expansions.  This make the parsing code fall into a neater place.
***************************************************************/

#define DM_ALIAS_EXPANSION DM_MAXDEF

/***************************************************************
*  
*  Macro to convert a single char which is a hex digit (0-9,a-f) to
*  its value (0-15)dec.  Returns -1 on failure.
*  
***************************************************************/

#define HEX(a) (isdigit(a) ? (a & 0x0f) : (isxdigit(a) ? ((a & 0x07) + 9) : -1))

#define SKIP_WHITESPACE(p) while((*p == ' ') || (*p == '\t')) p++



/***************************************************************
*  
*  Prototypes for local functions.
*  
***************************************************************/


static char *copy_kd(char    *from,
                     char    *to,
                     char     escape_char,
                     char    *cmd_line,
                     int      line_no);

static DMC  *parse_dm(char   *cmd_line,        /* input  */
                      char  **new_pos,         /* output, can point to cmd_line */
                      int     temp_flag,       /* input  */
                      int     line_no,         /* input  */
                      char    escape_char,     /* input  */
                      void   *hash_table);     /* input  */

static char  *get_num_range(char    *from,
                            int     *first,
                            int     *second,
                            char    *first_relative,
                            char    *second_relative,
                            char     sep);

static int   count_tokens(char   *line);

static char *comment_scan(char    *from,
                          char     escape_char);

static void cse_message(char     *work,              /* output */
                        int       sizeof_work,       /* input  */
                        char     *cmd_name,          /* input  */
                        char     *cmd_line,          /* input  */
                        int       line_no,           /* input  */
                        int      *bad);              /* output */

static void shift_out_arg(int  *argc, char **argv);

static int hexparm_to_value(char  *p,
                            char  *cmd_name,
                            int    line_no,
                            char  *cmd_line,
                            char  *target);

/*****************************************************************************
*
*   parse_dm_line   parse a line with 1 or more DM commands.
*              This routine will parse a command string DM commands
*              in it by repeatedly calling parse_dm.  The commands are placed
*              in a linked list.  Upon failure, NULL is returned.
*
*   PARAMETERS:
*
*      cmd_line   -   pointer to char  (INPUT)
*                     This is the command line.
*
*      temp_flag  -   int  (INPUT)
*                    This flag determines whether this is a temporary DM
*                    structure (command line) or a permanent one (key def).
*
*      line_no    -  int (INPUT)
*                    This is the line number used in error message generation.
*                    For lines typed at the window, this is zero, for lines
*                    being processed from a command file, this is the line
*                    number in the file with the command.  Special routine iln
*                    which is defined in parsedm.c returns a null string for
*                    value zero and a formated line number for non-zero.
*
*      escape_char -  char (INPUT)
*                    This is the escape character used in parsing.  It is
*                    an '@' for Aegis mode and a '\' for Unix mode.
*
*      hash_table -  pointer to void (INPUT)
*                    This is the current hash table being used.  It is 
*                    used by parse_dm for alias processing.
*
*   FUNCTIONS:
*
*   1.  build a linked list of the dm command structures (DMC's) returned
*       by parse_dm.
*
*   RETURNED VALUE:
*
*   first   -   pointer to DMC
*               The pointer to the first DM command structure is returned.
*               Note that the temp flags are not se
*
*
*********************************************************************************/

DMC      *parse_dm_line(char   *cmd_line,    /* input  */
                        int     temp_flag,   /* input  */
                        int     line_no,     /* input  */
                        char    escape_char, /* input  */
                        void   *hash_table)  /* input  */
{
char        *next_cmd;
DMC         *new_dmc;
DMC         *first_dmc = NULL;
DMC         *last_dmc = NULL; /* RES 6/22/95 Lint change */
int          i;

DEBUG4(fprintf(stderr, "parse_dm_line:  Processing line \"%s\"\n", cmd_line); )
next_cmd = cmd_line;
while (*next_cmd == ' ' || *next_cmd == '\t') next_cmd++;  /* trim leading blanks and tabs */
/* RES aded 7/20/94 for Marv to make things work with zmail */
/* this allows an entire line to be quoted with no effect */
if (*next_cmd == '\'' || *next_cmd == '"')
   {
      i = strlen(next_cmd);
      if (next_cmd[i-1] == *next_cmd)
         next_cmd[i-1] = '\0';
      next_cmd++;
   }
if (*next_cmd == '\0')
   return(DMC_COMMENT);

while (next_cmd != NULL && *next_cmd != '\0')
{
   new_dmc = parse_dm(next_cmd, &next_cmd, temp_flag, line_no, escape_char, hash_table);
   if (new_dmc == NULL)
      {
         free_dmc(first_dmc);
         return(NULL);
      }
   else
      {
         if (new_dmc != DMC_COMMENT)  /* comment */
            {
               if (new_dmc->any.cmd == DM_comma && first_dmc == NULL)
                  {
                     /* comma cannot be first */
                     dm_error("Command syntax error", DM_ERROR_BEEP);
                     free_dmc(first_dmc);
                     return(NULL);
                  }
               if (first_dmc == NULL)
                  first_dmc = new_dmc;
               else
                  last_dmc->next = new_dmc;
               last_dmc = new_dmc;
               /***************************************************************
               *  If the parse_dm made a list of dm cmds(alias expansion),
               *  go to the end.
               ***************************************************************/
               while(last_dmc->next != NULL)
                  last_dmc = last_dmc->next;
             }
      }
}

if (first_dmc == NULL)
   first_dmc = DMC_COMMENT;
else
   if (last_dmc->any.cmd == DM_comma)
      {
         /* comma cannot be last */
         dm_error("Command syntax error", DM_ERROR_BEEP);
         free_dmc(first_dmc);
         return(NULL);
      }

return(first_dmc);
}  /* end of parse_dm_line */


/*****************************************************************************
*
*   parse_dm   parse one DM command
*              This routine will parse a command string with one
*              DM command on it.  
*
*   PARAMETERS:
*
*      cmd_line -  pointer to char  (INPUT)
*                  This is the line to parse, it may contain more than one command
*                  on it, but only one will be parsed.
*
*      new_pos  -  Pointer to pointer to char (OUTPUT)
*                  the byte after the command in cmd_line is returned in the pointer
*                  pointed to by this pointer.
*
*      temp_flag  -   int  (INPUT)
*                    This flag determines whether this is a temporary DM
*                    structure (command line) or a permanent one (key def).
*
*      line_no    -  int (INPUT)
*                    This is the line number used in error message generation.
*                    For lines typed at the window, this is zero, for lines
*                    being processed from a command file, this is the line
*                    number in the file with the command.  Special routine iln
*                    which is defined in parsedm.c returns a null string for
*                    value zero and a formated line number for non-zero.
*
*
*   FUNCTIONS:
*
*   1.  Figure out which command name this is.
*
*   2.  allocate space for the cmd structure and fill in the common data.
*
*   3.  Switch on the cmd id to do additional parsing.
*
*   RETURNED VALUE:
*
*   dmc     -   pointer to DMC
*               This is the dmc for the parsed command.  On failure
*               NULL is returned.
*
*
*********************************************************************************/

static DMC  *parse_dm(char   *cmd_line,        /* input  */
                      char  **new_pos,         /* output */
                      int     temp_flag,       /* input  */
                      int     line_no,         /* input  */
                      char    escape_char,     /* input  */
                      void   *hash_table)      /* input  */
{

int                   i,j;
int                   cmd_id = -1;
char                  cmd_name[16];
char                  cmd_name_l[16];
char                  work[MAX_LINE+1];
char                  work_cmd[MAX_LINE+1];
char                  alias_line[MAX_LINE+1];
DMC                  *dmc;
char                 *p, *p2;
char                 *q;
char                 *alias_expansion;
char                  sep;
int                   num_value;
int                   num_value_relative;
int                   dm_bad = False;
char                 *end_of_cmd;
int                   temp_argc;
char                **temp_argvp;


/***************************************************************
*
*  Find the DM command name.
*  First check for the special names, then get the token and
*  search the list.
*
***************************************************************/
strncpy(work_cmd, cmd_line, sizeof(work_cmd)); /* need the null padding */
p = work_cmd;
while (*p == ' ') p++; /* get rid of leading blanks */
end_of_cmd = &cmd_line[strlen(cmd_line)];

switch(*p)
{
case '/':
   cmd_id = DM_find;
   cmd_name[0] = '/';
   cmd_name[1] = '\0';
   break;

case '\\':
   cmd_id = DM_rfind;
   cmd_name[0] = '\\';
   cmd_name[1] = '\0';
   break;

case '?':
   cmd_id = DM_rfind;
   cmd_name[0] = '?';
   cmd_name[1] = '\0';
   break;

case '[':
   cmd_id = DM_markc;
   cmd_name[0] = '[';
   cmd_name[1] = '\0';
   break;

case '{':
   cmd_id = DM_corner;
   cmd_name[0] = '{';
   cmd_name[1] = '\0';
   break;

case '(':
   cmd_id = DM_markp;
   cmd_name[0] = '(';
   cmd_name[1] = '\0';
   break;

case '=':
   cmd_id = DM_equal;
   cmd_name[0] = '=';
   cmd_name[1] = '\0';
   break;

case '$':
   cmd_id = DM_dollar;
   cmd_name[0] = '$';
   cmd_name[1] = '\0';
   break;

case ',':
   cmd_id = DM_comma;
   cmd_name[0] = ',';
   cmd_name[1] = '\0';
   break;

case '!':
   cmd_id = DM_bang;
   cmd_name[0] = '!';
   cmd_name[1] = '\0';
   p++;
   break;

case '#':
   *new_pos = end_of_cmd;
   return(DMC_COMMENT);  /* comment line */

case ';':
   *new_pos = ((p+1) - work_cmd) + cmd_line;
   return(DMC_COMMENT);  /* comment line */

/* RES 10/13/95 added to allow colon on end of find to suppress list termination on find failure */
case ':':
   cmd_id = DM_colon;
   cmd_name[0] = '\0';
   end_of_cmd = ((p+1) - work_cmd) + cmd_line;
   *p = '\0';
   break;

/***************************************************************
*  
*  Default case.  first check to see if this is a number.  If not,
*  grab the command name and check the list of known commands.
*  
***************************************************************/

default:
   if (sscanf(p, "%d%n", &num_value, &i) == 1)
      {
         cmd_id = DM_num;
         if (*p == '-')
            num_value_relative = -1;
         else
            if (*p == '+')
               num_value_relative = 1;
            else
               num_value_relative = 0;
         p += i;
      }
   else
      {
         /***************************************************************
         *  
         *  Copy the first token to a command name area.  All dm commands
         *  other than the special ones we checked for are all Alpha.
         *  Change the command to lower case and then look at the names
         *  listed in the structure of command names.  If we find it, the
         *  index is the command id.
         *  
         ***************************************************************/

         while (*p == ' ' || *p == '\t') p++;  /* skip leading blanks and tabs */
         q = cmd_name;
         while (isalpha(((unsigned char)*p)) && ((q - cmd_name) < sizeof(cmd_name)-1)) *q++ = *p++;
         *q = '\0';
         strncpy(cmd_name_l, cmd_name, sizeof(cmd_name_l));
         (void) case_line('l', cmd_name_l, 0, sizeof(cmd_name_l));

         for (i = 0; i < DM_MAXDEF && cmd_id == -1; i++)
            if (strcmp(dmsyms[i].name, cmd_name_l) == 0)
               cmd_id = i;
         while (*p == ' ' || *p == '\t') p++;  /* skip leading blanks and tabs */
      }
   break;
} /* end of switch statement */

/***************************************************************
*  
*  If this is not a valid command, see if it is an alias.
*  
***************************************************************/

if ((cmd_id == -1) || (dmsyms[cmd_id].supported  == 0))
   {
      alias_expansion = lookup_alias(cmd_name_l, hash_table);
      if (alias_expansion != NULL)
         cmd_id = DM_ALIAS_EXPANSION;
   }

/***************************************************************
*
*  If we have no command id, the command name is invalid,
*  kick it out.
*  If the comnand is not supported, kick it out too.
*
***************************************************************/

if (cmd_id == -1)
   {
      snprintf(work, sizeof(work), "(%s) Unknown command name (%s%s)", cmd_name, iln(line_no), cmd_line);
      dm_error(work, DM_ERROR_BEEP);
      *new_pos = end_of_cmd;
      return(NULL);
   }
if ((cmd_id != DM_ALIAS_EXPANSION) && (dmsyms[cmd_id].supported  == 0))
   {
      snprintf(work, sizeof(work), "(%s) Unsupported CE command (%s%s)", cmd_name, iln(line_no), cmd_line);
      dm_error(work, DM_ERROR_BEEP);
      *new_pos = end_of_cmd;
      return(DMC_COMMENT);
   }

/***************************************************************
*
*  Get the DMC (DM Command) structure to fill out.
*
***************************************************************/

dmc = (DMC *)CE_MALLOC(sizeof(DMC));
if (!dmc)
   return(NULL);
memset((char *)dmc, 0, sizeof(DMC));
dmc->any.next         = NULL;
dmc->any.cmd          = cmd_id;
dmc->any.temp         = temp_flag;


/***************************************************************
*
*  If this is not one of the "funny" commands, "find", "mark",
*  "num", etc, set the end position for the next command.
*  If the command line ends in a semi-colon truncate the work
*  command at this point so we know we have one command.
*
***************************************************************/

if ((cmd_id == DM_ALIAS_EXPANSION) || (!dmsyms[cmd_id].special_delim))
   {
      /***************************************************************
      *  p now points to the first non-blank after the command name.
      *  This is in work_cmd.  Scan to find the end of the command.
      ***************************************************************/
      q = comment_scan(p, escape_char);
      if (*q == ';')
         *q++ = '\0';
      else
         *q = '\0';
      end_of_cmd = cmd_line + (q - work_cmd);
      /* Added 5/25/94 to trim trailing blanks in command strings before the semi colon */
      q = &p[strlen(p)-1];
      while ((q > p) && ((*q == ' ') || (*q == '\t'))) *q-- = '\0'; 
   }

/***************************************************************
*
*  Now parse the command args, if there are any.
*
***************************************************************/

switch(cmd_id)
{
/***************************************************************
*  For find and rfind, p still points to the first non-blank in
*  the buffer.  Get permanent space for the regular expression
*  and copy it getting rid of the slashes and escape chars.
***************************************************************/
case DM_f:
   dmc->any.cmd = cmd_id = DM_find;
   /* FALLTHRU */
case DM_find:
case DM_rfind:
   sep = *p++;
   /* special check for repeat find */
   if (*p == sep)
      {
         dmc->find.buff = NULL;
         end_of_cmd = ((p+1) - work_cmd) + cmd_line;
         break;
      }
   dmc->find.buff = CE_MALLOC(strlen(p)+1);
   if (!dmc->find.buff)
      return(NULL);
   q = copy_with_delim(p, dmc->find.buff, sep, escape_char, 1 /* missing delim  ok (changed to ok 6/13/95 RES) */);
   if (q != NULL)
      end_of_cmd = (q - work_cmd) + cmd_line;
   else
      {
         snprintf(work, sizeof(work), "(%s) Command syntax error - missing delimiter (%s%s)", cmd_name, iln(line_no), cmd_line);
         dm_error(work, DM_ERROR_BEEP);
         free(dmc->find.buff);
         free((char *)dmc);
         dmc = NULL;
      }
   break;

/***************************************************************
*  For markp and markc, p still points to the first non-blank in
*  the buffer.  Skip this char and get the x and y numbers.
*  Then skip past the close bracket or paren.
*  No negative values allowed.
***************************************************************/
case DM_markc:
case DM_corner:
   p++;
   if (cmd_id == DM_markc)
      sep = ']';
   else
      sep = '}';
   q = get_num_range(p, &dmc->markc.row, &dmc->markc.col, &dmc->markc.row_relative, &dmc->markc.col_relative, sep);
   if (q == NULL)
      {
         snprintf(work, sizeof(work), "(%s) Position specification syntax error (%s%s)", cmd_name, iln(line_no), cmd_line);
         dm_error(work, DM_ERROR_BEEP);
         free((char *)dmc);
         dmc = NULL;
      }
   else
      {
         /***************************************************************
         *  Subtract 1 from the row and column because we are zero based
         *  and the display output and the user are one based.
         ***************************************************************/
         if (dmc->markc.row > 0 && !dmc->markc.row_relative)
            (dmc->markc.row)--;
         if (dmc->markc.col > 0 && !dmc->markc.col_relative)
            (dmc->markc.col)--;
         end_of_cmd = (q - work_cmd) + cmd_line;
      }
   break;

case DM_markp:
   p++;
   q = get_num_range(p, &dmc->markp.x, &dmc->markp.y, NULL, NULL, ')');
   if (q == NULL)
      {
         snprintf(work, sizeof(work), "(%s) Position specification syntax error (%s%s)", cmd_name, iln(line_no), cmd_line);
         dm_error(work, DM_ERROR_BEEP);
         free((char *)dmc);
         dmc = NULL;
      }
   else
      end_of_cmd = (q - work_cmd) + cmd_line;

   break;

/***************************************************************
*  For equal, p still points to the equal sign.
*  If the next thing is a semi-colon, skip it.
*
*  For comma, p still points to the equal sign.
*  All we are really here for is to set end of cmd
***************************************************************/
case DM_equal:
case DM_comma:
case DM_dollar:
   p++;  /* skip equal sign */
   if (*p == ';')
      p++;
   end_of_cmd = (p - work_cmd) + cmd_line;
   break;


/***************************************************************
*  For num, the num_value has already been pulled out.  p points
*  just past the number.  This is the start of the next thing.
*  num_value_relative was set at the same time as num_value.
***************************************************************/
case DM_num:
   if (num_value > 0 && !num_value_relative) /* pulled out in early parsing, user is 1 based we are zero based */
      num_value--;
   dmc->num.row = num_value;
   dmc->num.relative = num_value_relative;
   end_of_cmd = (p - work_cmd) + cmd_line;
   break;


case DM_bang:
   while(*p == ' ') p++;
   while (*p == '-' && !dm_bad)
   {
      switch (*(p+1))
      {
         case 'C':
         case 'c':
            dmc->bang.dash_c = True;
            p += 2;
            while(*p == ' ') p++;
            break;
         case 'M':
         case 'm':
            dmc->bang.dash_m = True;
            p += 2;
            while(*p == ' ') p++;
            break;
         case 'E':
         case 'e':
            dmc->bang.dash_e = True;
            p += 2;
            while(*p == ' ') p++;
            break;
         case 'S':
         case 's':
            p += 2; /* skip the -s */
            while(*p == ' ') p++;
            sep = *p++;
            dmc->bang.dash_s = CE_MALLOC(strlen(p)+1);
            if (dmc->bang.dash_s)
               {
                  q = copy_with_delim(p,
                                      dmc->bang.dash_s,
                                      sep,
                                      escape_char,
                                      False /* missing delim not ok */);
                  if (q)
                     {
                        p = q;
                        while(*p == ' ') p++;
                     }
                  else
                     dm_bad = True;
               }
            else
               dm_bad = True;


            break;
         default:
            cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
            break;
      }  /* end of switch */
   }
   if ((*p == '\'') || (*p == '"') && !dm_bad)  /* allow single or double quotes */
      {
         i = strlen(p);
         if (p[i-1] == *p)
            p[i-1] = '\0';
         p++;
      }
   if (!dm_bad)
      {
         dmc->bang.cmdline = malloc_copy(p);
         if (!dmc->bang.cmdline)
            dm_bad = True;
         else
            if (*dmc->bang.cmdline == '\0')
               cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
      }
   if (dm_bad)
      {
         free((char *)dmc);
         dmc = NULL;
      }
   break;


case DM_bl:
   dmc->bl.l_ident = NULL;
   dmc->bl.r_ident = NULL;
   dmc->bl.dash_d  = 0;
   if (char_to_argv(NULL, p, cmd_name, cmd_line, &temp_argc, &temp_argvp, escape_char) == 0)
      {
         if ((temp_argc > 0) && (temp_argvp[0][0] == '-') && ((temp_argvp[0][1] | 0x20) == 'd'))
            {
               if (temp_argvp[0][2]) /* make sure there is a space after the -letter */
                  cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
               dmc->bl.dash_d = True;
               shift_out_arg(&temp_argc, temp_argvp);
            }
      }
   else
      dm_bad = True;

   if (temp_argc == 2)
      {
         dmc->bl.l_ident = temp_argvp[0];
         dmc->bl.r_ident = temp_argvp[1];
         free((char *)temp_argvp);
      }
   else
      if (temp_argc != 0)
         cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);

   if (dm_bad)
      {
         if (temp_argvp)
            {
               free_temp_args(temp_argvp);
               free((char *)temp_argvp);
            }
         free((char *)dmc);
         dmc = NULL;
      }
   break;


case DM_ca:
case DM_nc:
case DM_pc:
   if (char_to_argv(NULL, p, cmd_name, cmd_line, &temp_argc, &temp_argvp, escape_char) == 0)
      {
         if (temp_argc > 0)
            {
               if (temp_argc > 2)
                  {
                     snprintf(work, sizeof(work), "(%s) Too many command arguments (%s%s)", cmd_name, iln(line_no), cmd_line);
                     dm_error(work, DM_ERROR_BEEP);
                     dm_bad = True;
                  }
               else
                  {
                     dmc->ca.fgc = temp_argvp[0];
                     if (*(dmc->ca.fgc) == '\0')
                        {
                           free(dmc->ca.fgc);
                           dmc->ca.fgc = NULL;
                        }

                     if (temp_argc > 1)
                        {
                           dmc->ca.bgc = temp_argvp[1];
                           if (*(dmc->ca.bgc) == '\0')
                              {
                                 free(dmc->ca.bgc);
                                 dmc->ca.bgc = NULL;
                              }
                        }
                     else
                        dmc->ca.bgc = NULL;

                     free(temp_argvp);
                  }
            }
         else
            {
               dmc->ca.bgc = NULL;
               dmc->ca.fgc = NULL;
            }
      }
   else
      dm_bad = True;

   if (dm_bad)
      {
         if (temp_argvp)
            {
               free_temp_args(temp_argvp);
               free((char *)temp_argvp);
            }
         free((char *)dmc);
         dmc = NULL;
      }
   break;


case DM_case:
   dmc->d_case.func = '\0';
   for (p = strtok(p, " \t");
        p != NULL;
        p = strtok(NULL, " \t"))
   {
      if (dmc->d_case.func != '\0')
         {
            cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
            break; /* out of while loop */
         }
      if (*p == '-')
         {
            if (*(p+2)) /* make sure there is a space after the -letter */
               cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
            switch (*(p+1))
            {
               case 'S':
               case 's':
                  dmc->d_case.func = 's';
                  break;
               case 'U':
               case 'u':
                  dmc->d_case.func = 'u';
                  break;
               case 'L':
               case 'l':
                  dmc->d_case.func = 'l';
                  break;
               default:
                  snprintf(work, sizeof(work), "(%s) Bad value for -c parm (%s%s)", cmd_name, iln(line_no), cmd_line);
                  dm_error(work, DM_ERROR_BEEP);
                  dm_bad = True;
                  break;
            }  /* end of switch */
            if (dm_bad)
               break;
         }
      else
         {
            cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
            break;
         }
   }

   if (dm_bad)
      {
         free((char *)dmc);
         dmc = NULL;
      }
   else
      if (dmc->d_case.func == '\0')
         dmc->d_case.func = 's'; /* the default */
   break;

case DM_ce:
case DM_cv:
case DM_cc:
case DM_ceterm:
case DM_cp:
   dmc->cp.argc = 0;
   dmc->cp.argv = NULL;
   if (char_to_argv(cmd_name, p, cmd_name, cmd_line, &dmc->cv.argc, &dmc->cv.argv, escape_char) != 0)
      dm_bad = True;

   if (dmc->cpo.argc > 255)
      {
         snprintf(work, sizeof(work), "(%s) too many args, limit 255 (%s%s)", cmd_name, iln(line_no), cmd_line);
         dm_error(work, DM_ERROR_BEEP);
         dm_bad = True;
      }
   if (dm_bad)
      {
         if (dmc->cv.argv)
            {
               free_temp_args(dmc->cv.argv);
               free((char *)dmc->cv.argv);
            }
         free((char *)dmc);
         dmc = NULL;
      }
   break;


case DM_cmdf:
case DM_rec:
   dmc->cmdf.dash_p = 0;
   dmc->cmdf.path   = NULL;
   for (p = strtok(p, " \t");
        p != NULL;
        p = strtok(NULL, " \t"))
      if (*p == '-')
         {
            if ((*(p+1) == 'p') && (!(*(p+2))))
               dmc->cmdf.dash_p++;
            else
               {
                  cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
                  break;
               }
         }
      else
         {
            if (dmc->cmdf.path != NULL)
               {
                  cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
                  break;
               }
            else
               {
                  dmc->cmdf.path = CE_MALLOC(strlen(p)+1);
                  if (dmc->cmdf.path)
                     strcpy(dmc->cmdf.path, p);
                  else
                     dm_bad = True;
               }
         }
   if (dm_bad)
      {
        if (dmc->cmdf.path != NULL)
            free(dmc->cmdf.path);
         free((char *)dmc);
         dmc = NULL;
      }
   break;

case DM_cpo:
case DM_cps:
   dmc->cpo.argc = 0;
   dmc->cpo.argv = NULL;
   
   if (char_to_argv(NULL, p, cmd_name, cmd_line, &dmc->cpo.argc, &dmc->cpo.argv, escape_char) == 0)
      {
         while(dmc->cpo.argc > 0 && dmc->cpo.argv[0][0] == '-')
         {
            switch (dmc->cpo.argv[0][1])
            {
               case 'W':
               case 'w':
                  dmc->cpo.dash_w++;
                  shift_out_arg(&dmc->cpo.argc, dmc->cpo.argv);
                  break;
               case 'D':
               case 'd':
                  dmc->cpo.dash_d++;
                  shift_out_arg(&dmc->cpo.argc, dmc->cpo.argv);
                  break;
               case 'S':
               case 's':
                  dmc->cpo.dash_s++;
                  shift_out_arg(&dmc->cpo.argc, dmc->cpo.argv);
                  break;
               case 'i':
                  snprintf(work, sizeof(work), "(%s) -i iparm ignored (%s%s)", cmd_name, iln(line_no), cmd_line);
                  dm_error(work, DM_ERROR_BEEP);
                  shift_out_arg(&dmc->cpo.argc, dmc->cpo.argv);
                  break;
               case 'n':
                  snprintf(work, sizeof(work), "(%s) Named process ignored (%s%s)", cmd_name, iln(line_no), cmd_line);
                  dm_error(work, DM_ERROR_BEEP);
                  if (strlen(dmc->cpo.argv[0]) == 2)  /* name is in next parm */
                     shift_out_arg(&dmc->cpo.argc, dmc->cpo.argv); /* eat the next parm */
                  shift_out_arg(&dmc->cpo.argc, dmc->cpo.argv);
                  break;
               case 'c':
                  snprintf(work, sizeof(work), "(%s) -c icon char ignored (%s%s)", cmd_name, iln(line_no), cmd_line);
                  dm_error(work, DM_ERROR_BEEP);
                  if (strlen(dmc->cpo.argv[0]) == 2)  /* name is in next parm */
                     shift_out_arg(&dmc->cpo.argc, dmc->cpo.argv); /* eat the next parm */
                  shift_out_arg(&dmc->cpo.argc, dmc->cpo.argv);
                  break;
               default:
                  cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
                  break;
            }  /* end of switch */
            if (dm_bad)
               break;
         }
      }
   else
      dm_bad = True;

   if (dmc->cpo.argc > 255)
      {
         snprintf(work, sizeof(work), "(%s) too many args, limit 255 (%s%s)", cmd_name, iln(line_no), cmd_line);
         dm_error(work, DM_ERROR_BEEP);
         dm_bad = True;
      }
   if (dmc->cpo.argc == 0)
      cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
   if (dm_bad)
      {
         if (dmc->cpo.argv)
            {
               free_temp_args(dmc->cpo.argv);
               free((char *)dmc->cpo.argv);
            }
         free((char *)dmc);
         dmc = NULL;
      }
   break;


case DM_dq:
   dmc->dq.dash_s = 0;
   dmc->dq.dash_c = 0;
   dmc->dq.dash_b = 0;
   dmc->dq.dash_i = 0;
   dmc->dq.dash_a = 0;
   dmc->dq.name   = NULL;
   for (p = strtok(p, " \t");
        p != NULL;
        p = strtok(NULL, " \t"))
      if (*p == '-')
         {
            if (*(p+2)) /* make sure there is a space after the -letter */
               cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
            switch (*(p+1))
            {
               case 's':
                  dmc->dq.dash_s++;
                  break;
               case 'b':
                  dmc->dq.dash_b++;
                  break;
               case 'i':
                  dmc->dq.dash_i++;
                  break;
               case 'c':
                  p = strtok(NULL, " \t");
                  if (p == NULL)
                     {
                        snprintf(work, sizeof(work), "(%s) Missing value for -c parm (%s%s)", cmd_name, iln(line_no), cmd_line);
                        dm_error(work, DM_ERROR_BEEP);
                        dm_bad = True;
                     }
                  else
                     {
                        if (*p == '\'') /* get rid of quotes in -c '9' */
                           {
                              p++;
                              i = strlen(p) - 1;
                              if ((i >= 0) && (p[i] == '\''))
                                 p[i] = '\0';
                           }
                        if (sscanf(p,"%d%9s", &(dmc->dq.dash_c), work) != 1)
                           {
                              snprintf(work, sizeof(work), "(%s) Invalid decimal signal number in -c parm (%s%s)", cmd_name, iln(line_no), cmd_line);
                              dm_error(work, DM_ERROR_BEEP);
                              dm_bad = True;
                           }
                     }
                  break;
               case 'a':
                  p = strtok(NULL, " \t");
                  if (p == NULL)
                     {
                        snprintf(work, sizeof(work), "(%s) Missing value for -a parm (%s%s)", cmd_name, iln(line_no), cmd_line);
                        dm_error(work, DM_ERROR_BEEP);
                        dm_bad = True;
                      }
                   else
                      dm_bad =  hexparm_to_value(p, cmd_name, line_no, cmd_line, &(dmc->dq.dash_a));
                  break;
               default:
                  cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
                  break;
            }  /* end of switch */
            if (dm_bad)
               break;
         }
      else
         {
            if (dmc->dq.name != NULL)
               {
                  cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
                  break;
               }
            else
               {
                  if (*p == '\'') /* get rid of quotes in -c '9' */
                     {
                        p++;
                        i = strlen(p) - 1;
                        if ((i >= 0) && (p[i] == '\''))
                           p[i] = '\0';
                     }
                  dmc->dq.name = CE_MALLOC(strlen(p)+1);
                  if (dmc->dq.name)
                     strcpy(dmc->dq.name, p);
                  else
                     dm_bad = True;
               }
         }

   if (dm_bad)
      {
        if (dmc->dq.name != NULL)
            free(dmc->dq.name);
         free((char *)dmc);
         dmc = NULL;
      }
   break;


case DM_echo:
   dmc->echo.dash_r = 0;
   for (p = strtok(p, " \t");
        p != NULL;
        p = strtok(NULL, " \t"))
      if (*p == '-')
         {
            switch (*(p+1))
            {
               case 'r':
                  dmc->echo.dash_r++;
                  break;
               default:
                  cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
                  break;
            }  /* end of switch */
            if (dm_bad)
               break;
         }
      else
         {
            cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
            break;
         }

   if (dm_bad)
      {
         free((char *)dmc);
         dmc = NULL;
      }
   break;


case DM_eef:
   dmc->eef.dash_a = 0;
   p = strtok(p, " \t");
   if (p)
      if ((*p == '-') && ((*(p+1) | 0x20) == 'a'))
         {
            p = strtok(NULL, " \t");
            if (p == NULL)
               {
                  snprintf(work, sizeof(work), "(%s) Missing value for -a parm (%s%s)", cmd_name, iln(line_no), cmd_line);
                  dm_error(work, DM_ERROR_BEEP);
                  dm_bad = True;
                }
             else
                dm_bad =  hexparm_to_value(p, cmd_name, line_no, cmd_line, &(dmc->dq.dash_a));
             break;
         }
      else
         cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);

   if (dm_bad)
      {
         free((char *)dmc);
         dmc = NULL;
      }
   break;


case DM_ei:
case DM_inv:
case DM_ro:
case DM_sb:
case DM_sc:
case DM_wa:
case DM_wh:
case DM_wi:
case DM_ws:
case DM_lineno:
case DM_hex:
case DM_vt:
case DM_mouse:
case DM_envar:
case DM_pdm:
case DM_caps:
   dmc->ei.mode = DM_TOGGLE;
   for (p = strtok(p, " \t");
        p != NULL;
        p = strtok(NULL, " \t"))
      if (strcmp(p, "-on") == 0 || strcmp(p, "-ON") == 0)
         dmc->ei.mode = DM_ON;
      else
         if (strcmp(p, "-off") == 0 || strcmp(p, "-OFF") == 0)
            dmc->ei.mode = DM_OFF;
         else
            if (((dmc->ei.cmd == DM_vt) || (dmc->ei.cmd == DM_sb)) &&
                ((strcmp(p, "-auto") == 0) || (strcmp(p, "-AUTO") == 0)))
               dmc->ei.mode = DM_AUTO;
            else
               {
                  cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
                  break;
               }
   if (dm_bad)
      {
         free((char *)dmc);
         dmc = NULL;
      }
   break;


case DM_er:
   dmc->er.string = dmc->er.chars;
   p = strtok(p, " \t");
   if (p)
      dm_bad =  hexparm_to_value(p, cmd_name, line_no, cmd_line, dmc->er.chars);
   else
      {
         snprintf(work, sizeof(work), "(%s) Missing argument - (%s%s)", cmd_name, iln(line_no), cmd_line);
         dm_error(work, DM_ERROR_BEEP);
         dm_bad = True;
      }
   if (dm_bad)
      {
         free((char *)dmc);
         dmc = NULL;
      }
   else
      dmc->er.chars[1] = '\0';
   break;


case DM_es:
   /*  p points to the white space after the es. */
   while(*p == ' ') p++;
   i = strlen(p);
   if (i < ES_SHORT)
      dmc->es.string = dmc->es.chars;
   else
      dmc->es.string = CE_MALLOC(i+1);
   if (!dmc->es.string)
      return(NULL);
   if ((*p != '\'') && (*p != '"') && (*p != '#'))
      {
         snprintf(work, sizeof(work), "(%s) Command syntax error - missing leading delimiter (')   (%s%s)", cmd_name, iln(line_no), cmd_line);
         dm_error(work, DM_ERROR_BEEP);
         dm_bad = True;
      }
   else
      {
         sep = *p;
         p = copy_with_delim(++p, dmc->es.string, sep, escape_char, 0 /* missing delim not ok */);
         if (p == NULL)
            {
               snprintf(work, sizeof(work), "(%s) Command syntax error - missing delimiter (%s%s)", cmd_name, iln(line_no), cmd_line);
               dm_error(work, DM_ERROR_BEEP);
               dm_bad = True;
            }
         else
            {
               while(*p == ' ') p++;
               if (*p != '\0')
                  {
                     snprintf(work, sizeof(work), "(%s) Too many command arguments (%s%s)", cmd_name, iln(line_no), cmd_line);
                     dm_error(work, DM_ERROR_BEEP);
                     dm_bad = True;
                  }
            }
      }

   if (dm_bad)
      {
         if (dmc->es.string != dmc->es.chars)
            free(dmc->es.string);
         free((char *)dmc);
         dmc = NULL;
      }
   break;


case DM_fl:
case DM_geo:
case DM_debug:
case DM_re:
case DM_cd:
case DM_pd:
case DM_env:
case DM_bgc:
case DM_fgc:
case DM_title:
case DM_lbl:
case DM_glb:
   while(*p == ' ') p++;
   if ((*p == '\'') || (*p == '"'))  /* allow single or double quotes */
      {
         i = strlen(p);
         if (p[i-1] == *p)
            p[i-1] = '\0';
         p++;
      }
   i = strlen(p);
   if (i > 0)
      {
         dmc->geo.parm = CE_MALLOC(i+1);
         if (dmc->geo.parm)
            strcpy(dmc->geo.parm, p);
         else
            {
               free((char *)dmc);
               dmc = NULL;
            }
      }
   else
      dmc->geo.parm = NULL;
   break;

case DM_eval:
   while(*p == ' ') p++;
   if (*p == '\0')
      break;  /* eval with no parameters */
   dmc->eval.parm = CE_MALLOC(strlen(p)+1);
   if (!dmc->eval.parm)
      {
         free((char *)dmc);
         dmc = NULL;
         break;
      }
   sep = *p;
   q = copy_with_delim(++p, dmc->eval.parm, sep, escape_char, False /* missing delim not ok */);
   if (q == NULL)
      {
         dm_bad = True;
         snprintf(work, sizeof(work), "(%s) Command syntax error - missing delimiter (%s%s)", cmd_name, iln(line_no), cmd_line);
         dm_error(work, DM_ERROR_BEEP);
      }
   else
      end_of_cmd = (q - work_cmd) + cmd_line;
   if (dm_bad)
      {
         free(dmc->eval.parm);
         free((char *)dmc);
         dmc = NULL;
      }
   break;


case DM_icon:
case DM_al:
case DM_ar:
case DM_sic:
case DM_ee:
   dmc->icon.dash_i = 0;
   dmc->icon.dash_w = 0;
   for (p = strtok(p, " \t");
        p != NULL;
        p = strtok(NULL, " \t"))
      if (*p == '-')
         {
            if (*(p+2)) /* make sure there is a space after the -letter */
               cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
            switch (*(p+1))
            {
               case 'i':
                  dmc->icon.dash_i++;
                  break;
               case 'w':
                  dmc->icon.dash_w++;
                  break;
               default:
                  cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
                  break;
            }  /* end of switch */
            if (dm_bad)
               break;
         }
      else
         {
            cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
            break;
         }

   if (dm_bad)
      {
         free((char *)dmc);
         dmc = NULL;
      }
   break;


case DM_kd:
case DM_lkd:
case DM_alias:
case DM_lmi:
case DM_mi:
case DM_lsf:
   dmc->kd.def = NULL;
   dmc->kd.key = NULL;
   dmc->kd.line_no = line_no;
   p = strtok(p, " \t");
   if (p != NULL)
      {
         dmc->kd.key = malloc_copy(p);
         if (!dmc->kd.key)
            dm_bad = True;
         else
            {
               if ((q = strchr(p, ';')) != NULL)  /* handle "kd m3;ad" */
                  {
                     dmc->kd.key[q-p] = '\0';
                     end_of_cmd = ((q+1) - work_cmd) + cmd_line;
                     break;
                  }
               p = &p[strlen(p)+1];
               while (*p == ' ') p++;
               /* "kd key"  and nothing else is valid */
               if (*p == '\0')
                  break;
               if (*p == ';')  /* handle "kd m3 ;" */
                  {
                     end_of_cmd = (++p - work_cmd) + cmd_line;
                     break;
                  }
               dmc->kd.def = CE_MALLOC(strlen(p)+1);
               if (!dmc->kd.def)
                  return(NULL);
               q = copy_kd(p, dmc->kd.def, escape_char, cmd_line, line_no);
               if (q != NULL)
                  end_of_cmd = (q - work_cmd) + cmd_line;
               else
                  {
                     free((char *)dmc->kd.def);
                     free((char *)dmc);
                     dmc = NULL;
                  }
            }
      }
   else
      dm_bad = True;
   if (dm_bad)
      {
         cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
         free((char *)dmc);
         dmc = NULL;
      }
   break;


case DM_msg:
case DM_prefix:
case DM_sp:
   /*  p points to the white space after the string msg */
   while(*p == ' ') p++;
   if (!(*p)) /* no token, assume empty string */
      p = "''";
   if ((*p != '\'') && (*p != '"'))
      {
         snprintf(work, sizeof(work), "(%s) Command syntax error - missing leading delimiter (')   (%s%s)", cmd_name, iln(line_no), cmd_line);
         dm_error(work, DM_ERROR_BEEP);
         dm_bad = True;
      }
   else
      {
         i = strlen(p);
         dmc->msg.msg = CE_MALLOC(i+1);
         if (!dmc->msg.msg)
            return(NULL);  
         sep = *p;
         p = copy_with_delim(++p, dmc->msg.msg, sep, escape_char, 0 /* missing delim not ok */);
         if (p == NULL)
            {
               snprintf(work, sizeof(work), "(%s) Command syntax error - missing delimiter (%s%s)", cmd_name, iln(line_no), cmd_line);
               dm_error(work, DM_ERROR_BEEP);
               dm_bad = True;
            }
         else
            {
               while(*p == ' ') p++;
               if (*p != '\0')
                  {
                     snprintf(work, sizeof(work), "(%s) Too many command arguments (%s%s)", cmd_name, iln(line_no), cmd_line);
                     dm_error(work, DM_ERROR_BEEP);
                     dm_bad = True;
                  }
            }
      }

   if (dm_bad)
      {
         free(dmc->msg.msg);
         free((char *)dmc);
         dmc = NULL;
      }
   break;


case DM_ph:
case DM_pv:
case DM_bell:
case DM_fbdr:
   p = strtok(p, " \t");
   if (p == NULL)
      if (cmd_id == DM_fbdr)
         dmc->fbdr.chars = -1; /* use -1 as default to trigger printing the value */
      else
         dmc->fbdr.chars = 1;
   else
      if (sscanf(p, "%d%9s", &(dmc->fbdr.chars), work) != 1)
         {
            snprintf(work, sizeof(work), "(%s) Invalid decimal value (%s%s)", cmd_name, iln(line_no), cmd_line);
            dm_error(work, DM_ERROR_BEEP);
            free((char *)dmc);
            dmc = NULL;
         }
   break;

case DM_pn:
   dmc->pn.dash_c = 0;
   dmc->pn.path   = NULL;
   for (p = strtok(p, " \t");
        p != NULL;
        p = strtok(NULL, " \t"))
      if (*p == '-')
         {
            if (*(p+2)) /* make sure there is a space after the -letter */
               cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
            switch (*(p+1))
            {
               case 'c':
                  dmc->pn.dash_c = 'c';
                  break;
               case 'r':
                  dmc->pn.dash_r = 'r';
                  break;
               default:
                  snprintf(work, sizeof(work), "(%s) bad option (%s%s)", cmd_name, iln(line_no), cmd_line);
                  dm_error(work, DM_ERROR_BEEP);
                  dm_bad = True;
                  break;
            }  /* end of switch */
         }
      else
         {
            if (dmc->pn.path != NULL)
               {
                  snprintf(work, sizeof(work), "(%s) Too many command arguments (%s%s)", cmd_name, iln(line_no), cmd_line);
                  dm_error(work, DM_ERROR_BEEP);
                  dm_bad = True;
                  break;
               }
            else
               {
                  dmc->pn.path = CE_MALLOC(strlen(p)+1);
                  if (dmc->pn.path)
                     strcpy(dmc->pn.path, p);
                  else
                     dm_bad = True;
               }
         }
   if (dm_bad)
      {
        if (dmc->pn.path != NULL)
            free(dmc->pn.path);
         free((char *)dmc);
         dmc = NULL;
      }
   break;


case DM_pp:
case DM_sleep:
   p = strtok(p, " \t");
   if (p == NULL)
      dmc->pp.amount = 1.0;
   else
      if (sscanf(p, "%f%9s", &(dmc->pp.amount), work) != 1)
         {
            snprintf(work, sizeof(work), "(%s) Invalid numeric value (%s%s)", cmd_name, iln(line_no), cmd_line);
            dm_error(work, DM_ERROR_BEEP);
            free((char *)dmc);
            dmc = NULL;
         }
   break;


case DM_reload:
   dmc->reload.type   = '\0';
   for (p = strtok(p, " \t");
        p != NULL;
        p = strtok(NULL, " \t"))
   {
      if (dmc->reload.type != '\0')
         {
            cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
            break; /* out of while loop */
         }
      if (*p == '-')
         {
            if (((*(p+1) | 0x20) == 'o') && ((*(p+2) | 0x20) == 'k') && (*(p+3) == '\0'))
               {
                 *(p+1) = 'y';
                 *(p+2) = '\0';
               }
            switch (*(p+1))
            {
               case 'Y':
               case 'y':
                  dmc->reload.type = 'y';
                  break;
               case 'F':
               case 'f':
                  dmc->reload.type = 'f';
                  break;
               case 'N':
               case 'n':
               default:
                  dmc->reload.type = 'n';
                  break;
            }  /* end of switch */
            if (dm_bad)
               break;
         }
      else
         {
            cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
            break;
         }
   }

   if (dm_bad)
      {
         free((char *)dmc);
         dmc = NULL;
      }
   break;


case DM_rs:
   for (p = strtok(p, " \t");
        p != NULL;
        p = strtok(NULL, " \t"))
      if (*p == '-')
         {
            if (*(p+2)) /* make sure there is a space after the -letter */
               cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
            switch (*(p+1))
            {
               case 'f':
                  break;
               case 'n':
                  break;
               case 'o':
                  break;
               case 'r':
                  break;
               default:
                  cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
                  break;
            }  /* end of switch */
            if (dm_bad)
               break;
         }
      else
         {
            cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
            break;
         }

   if (dm_bad)
      {
         free((char *)dmc);
         dmc = NULL;
      }
   break;

case DM_rw:
   dmc->rw.dash_r = 0;
   for (p = strtok(p, " \t");
        p != NULL;
        p = strtok(NULL, " \t"))
      if (*p == '-')
         {
            if (*(p+2)) /* make sure there is a space after the -letter */
               cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
            switch (*(p+1))
            {
               case 'r':
                  dmc->rw.dash_r++;
                  break;
               default:
                  cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
                  break;
            }  /* end of switch */
            if (dm_bad)
               break;
         }
      else
         {
            cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
            break;
         }

   if (dm_bad)
      {
         free((char *)dmc);
         dmc = NULL;
      }
   break;


case DM_s:
case DM_so:
   dmc->s.s1 = NULL;
   dmc->s.s2 = NULL;
   while(*p == ' ') p++;
   if (*p == '\0')
      break;  /* s or so with no parameters */
   dmc->s.s1 = CE_MALLOC(strlen(p)+1);
   if (!dmc->s.s1)
      {
         free((char *)dmc);
         dmc = NULL;
         break;
      }
   sep = *p;
   p = copy_with_delim(++p, dmc->s.s1, sep, escape_char, 0 /* missing delim not ok */);
   if (p == NULL)
      {
         dm_bad = True;
         snprintf(work, sizeof(work), "(%s) Command syntax error - missing delimiter (%s%s)", cmd_name, iln(line_no), cmd_line);
         dm_error(work, DM_ERROR_BEEP);
      }
   else
      {
         dmc->s.s2 = &(dmc->s.s1[strlen(dmc->s.s1)+1]);
         q = copy_with_delim(p, dmc->s.s2, sep, escape_char, 1 /* missing delim is ok */);
         if (q == NULL)
            dm_bad = True;
         else
            end_of_cmd = (q - work_cmd) + cmd_line;
            /***************************************************************
            *  Special case for s/abc/  In this case abc is the substitute
            *  string and the search string is the carry over from prev cmd.
            *  Seeing if copy with delimiter moved anything (p == q) is a way of
            *  differentiating between s/abc/ and s/abc// which are different.
            ***************************************************************/
            if (p == q)
               {
                  dmc->s.s2 = dmc->s.s1;
                  dmc->s.s1 = NULL;
               }
      }
   if (dm_bad)
      {
         free(dmc->s.s1);
         free((char *)dmc);
         dmc = NULL;
      }
   break;


case DM_tf:
   dmc->tf.dash_l = 1;
   dmc->tf.dash_r = 72;
   dmc->tf.dash_c = '\0';
   dmc->tf.dash_b = '\0';
   dmc->tf.dash_p = '\0';
   for (p = strtok(p, " \t");
        p != NULL;
        p = strtok(NULL, " \t"))
   {
      if (*p == '-')
         {
            switch (*(p+1))
            {
               case 'c':
               case 'C':
                  if (dmc->tf.dash_c)
                     {
                        snprintf(work, sizeof(work),  "(%s) Duplicate -c parm specified  (%s%s)", cmd_name, iln(line_no), cmd_line);
                        dm_error(work, DM_ERROR_BEEP);
                        dm_bad = True;
                     }
                  else
                     dmc->tf.dash_c = True;
                  break;
               case 'L':
               case 'l':
                  if (*(p+2) == '\0') /* allow space after -l */
                     p = strtok(NULL, " \t");
                  else
                     p += 2;
                  if ((p == NULL) || (sscanf(p, "%hd%9s", &(dmc->tf.dash_l), work) != 1))
                     cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
                  break;
               case 'R':
               case 'r':
                  if (*(p+2) == '\0') /* allow space after -l */
                     p = strtok(NULL, " \t");
                  else
                     p += 2;
                  if ((p == NULL) || (sscanf(p, "%hd%9s", &(dmc->tf.dash_r), work) != 1))
                     cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
                  if (!dmc->tf.dash_r) 
                     {
                        snprintf(work, sizeof(work),  "(%s) -r may not be 0 (%s%s)", cmd_name, iln(line_no), cmd_line);
                        dm_error(work, DM_ERROR_BEEP);
                        dm_bad = True;
                     }
                  break;
               case 'B':
               case 'b':
                  if (dmc->tf.dash_b)
                     {
                        snprintf(work, sizeof(work),  "(%s) Duplicate -b parm specified  (%s%s)", cmd_name, iln(line_no), cmd_line);
                        dm_error(work, DM_ERROR_BEEP);
                        dm_bad = True;
                     }
                  else
                     dmc->tf.dash_b = True;
                  break;
               case 'P':
               case 'p':
                  if (dmc->tf.dash_p)
                     {
                        snprintf(work, sizeof(work),  "(%s) Duplicate -p parm specified  (%s%s)", cmd_name, iln(line_no), cmd_line);
                        dm_error(work, DM_ERROR_BEEP);
                        dm_bad = True;
                     }
                  else
                     dmc->tf.dash_p = True;
                  break;
               default:
                  cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
                  break;
            }  /* end of switch */
            if (dm_bad)
               break;
         }
      else
         {
            cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
            break;
         }
   }

   if (dm_bad)
      {
         free((char *)dmc);
         dmc = NULL;
      }
   break;

case DM_ts:
   dmc->ts.dash_r = 0;
   dmc->ts.stops  = NULL;
   dmc->ts.count  = count_tokens(p);
   if (dmc->ts.count > 0)
      {
         dmc->ts.stops  = (short int *)CE_MALLOC(dmc->ts.count * sizeof(short int));
         if (!dmc->ts.stops)
            return(NULL);
      }
   else
      dmc->ts.stops  = NULL;
   i = 0;
   for (p = strtok(p, " \t");
        p != NULL;
        p = strtok(NULL, " \t"))
   {
      if (*p == '-')
         {
            if (*(p+2)) /* make sure the '-r' is the last thing */
               cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
            else
               switch (*(p+1))
               {
                  case 'R':
                  case 'r':
                     dmc->ts.dash_r = 1;
                     (dmc->ts.count)--;
                     if (dmc->ts.count < 1)
                        cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
                     break;
                  default:
                     snprintf(work, sizeof(work), "(%s) Bad dash option (%c) - (%s%s)", cmd_name, (*(p+1)), iln(line_no), cmd_line);
                     dm_error(work, DM_ERROR_BEEP);
                     dm_bad = True;
                        break;
               }  /* end of switch */
            if (dm_bad)
               break;
         }
      else
         {
            if (dmc->ts.dash_r)
               {
                  snprintf(work, sizeof(work), "(%s) Too many command arguments - (%s%s)", cmd_name, iln(line_no), cmd_line);
                  dm_error(work, DM_ERROR_BEEP);
                  dm_bad = True;
                  break;
               }
            else
               if (sscanf(p, "%d%9s", &j, work) != 1)
                  {
                     snprintf(work, sizeof(work), "(%s) Invalid integer (%s) - (%s%s)", cmd_name, p, iln(line_no), cmd_line);
                     dm_error(work, DM_ERROR_BEEP);
                     dm_bad = True;
                     break;
                  }
               else
                  {
                     dmc->ts.stops[i] = j;
                     i++;
                  }
         }
   }

   if (dm_bad)
      {
         if (dmc->ts.stops)
            free((char *)dmc->ts.stops);
         free((char *)dmc);
         dmc = NULL;
      }
   else
      {
         if (dmc->ts.dash_r == 0)
            dmc->ts.dash_r = 1;
         else
            if (dmc->ts.count > 1)
               dmc->ts.dash_r = dmc->ts.stops[dmc->ts.count-1] - dmc->ts.stops[dmc->ts.count-2];
            else
               dmc->ts.dash_r = dmc->ts.stops[dmc->ts.count-1];
      }
   break;


case DM_twb:
   dmc->twb.border = '\0';
   for (p = strtok(p, " \t");
        p != NULL;
        p = strtok(NULL, " \t"))
   {
      if (dmc->twb.border != '\0')
         {
            cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
            break; /* out of while loop */
         }
      if (*p == '-')
         {
            if (*(p+2)) /* make sure there is a space after the -letter */
               cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
            switch (*(p+1))
            {
               case 'l':
               case 'L':
                  dmc->twb.border = 'l';
                  break;
               case 'b':
               case 'B':
                  dmc->twb.border = 'b';
                  break;
               case 't':
               case 'T':
                  dmc->twb.border = 't';
                  break;
               case 'r':
               case 'R':
                  dmc->twb.border = 'r';
                  break;
               default:
                  cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
                  break;
            }  /* end of switch */
            if (dm_bad)
               break;
         }
      else
         {
            cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
            break;
         }
   }

   if (dmc->twb.border == '\0')  /* the user had to specify something */
      {
         snprintf(work, sizeof(work), "(%s) Command syntax error, dash option required (%s%s)", cmd_name, iln(line_no), cmd_line);
         dm_error(work, DM_ERROR_BEEP);
         dm_bad = True;
      }
   if (dm_bad)
      {
         free((char *)dmc);
         dmc = NULL;
      }
   break;


case DM_wc:
   dmc->wc.type   = '\0';
   for (p = strtok(p, " \t");
        p != NULL;
        p = strtok(NULL, " \t"))
   {
      if (dmc->wc.type != '\0')
         {
            cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
            break; /* out of while loop */
         }
      if (*p == '-')
         {
            if (((*(p+1) | 0x20) == 'o') && ((*(p+2) | 0x20) == 'k') && (*(p+3) == '\0'))
               {
                 *(p+1) = 'y';
                 *(p+2) = '\0';
               }
#ifdef blah
 res 2/16/2005 - allow them to type yes instead of just y
            if (*(p+2)) /* make sure there is a space after the -letter */
               cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
#endif
            switch (*(p+1))
            {
               case 'Q':
               case 'q':
                  dmc->wc.type = 'q';
                  break;
               case 'F':
               case 'f':
                  dmc->wc.type = 'f';
                  break;
               case 'A':
               case 'a':
                  dmc->wc.type = 'a';
                  break;
               case 'S':
               case 's':
                  dmc->wc.type = 's';
                  break;
               case 'W':
               case 'w':
                  dmc->wc.type = 'w';
                  break;
               case 'Y':
               case 'y':
                  dmc->wc.type = 'y';  /* undocumented option, is a confirmed -q */
                  break;
               case '\0':
               case 'N':
               case 'n':
               default:
                  dmc->wc.type = 'n'; /* undocumented option, is an unconfirmed (canceled) -q */
                  break;
            }  /* end of switch */
            if (dm_bad)
               break;
         }
      else
         {
            cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
            break;
         }
   }

   if (dm_bad)
      {
         free((char *)dmc);
         dmc = NULL;
      }
   break;


case DM_wdc:
   if (char_to_argv(NULL, p, cmd_name, cmd_line, &temp_argc, &temp_argvp, escape_char) == 0)
      {
         if (temp_argc > 0)
            {
               if (sscanf(temp_argvp[0],"%d%9s", &(dmc->wdc.number), work) != 1)
                  {
                     snprintf(work, sizeof(work), "(%s) Invalid decimal wdc number number %s (%s%s)", cmd_name, p, iln(line_no), cmd_line);
                     dm_error(work, DM_ERROR_BEEP);
                     dm_bad = True;
                  }
               else
                  {
                     if (temp_argc > 3)
                        {
                           snprintf(work, sizeof(work), "(%s) Too many command arguments (%s%s)", cmd_name, iln(line_no), cmd_line);
                           dm_error(work, DM_ERROR_BEEP);
                           dm_bad = True;
                        }
                     else
                        {
                           if (temp_argc > 1)
                              dmc->wdc.bgc = temp_argvp[1];
                           else
                              dmc->wdc.bgc = NULL;

                           if (temp_argc > 2)
                              dmc->wdc.fgc = temp_argvp[2];
                           else
                              dmc->wdc.fgc = NULL;

                           free(temp_argvp[0]);
                           free(temp_argvp);
                        }
                  }
            }
         else
            {
               dmc->wdc.number = 0;
               dmc->wdc.bgc = NULL;
               dmc->wdc.fgc = NULL;
            }
      }
   else
      dm_bad = True;

   if (dm_bad)
      {
         if (temp_argvp)
            {
               free_temp_args(temp_argvp);
               free((char *)temp_argvp);
            }
         free((char *)dmc);
         dmc = NULL;
      }
   break;


case DM_wdf:
   p = strtok(p, " \t");
   if (p != NULL)
      if (sscanf(p,"%d%9s", &(dmc->wdf.number), work) != 1)
         {
            snprintf(work, sizeof(work), "(%s) Invalid decimal wdf number number %s (%s%s)", cmd_name, p, iln(line_no), cmd_line);
            dm_error(work, DM_ERROR_BEEP);
            dm_bad = True;
         }
      else
         {
            p = strtok(NULL, " \t");
            if (p != NULL)
               {
                  i = strlen(p);
                  if (i > 0)
                     {
                        dmc->wdf.geometry = CE_MALLOC(i+1);
                        if (dmc->wdf.geometry)
                           strcpy(dmc->wdf.geometry, p);
                        else
                           dm_bad = True;
                     }
                  else
                     dmc->wdf.geometry = NULL;
               }
            else
               dmc->wdf.geometry = NULL;
            p = strtok(NULL, " \t");
            if ((p != NULL) && (*p != ';'))
               {
                  snprintf(work, sizeof(work), "(%s) Too many command arguments (%s%s)", cmd_name, iln(line_no), cmd_line);
                  dm_error(work, DM_ERROR_BEEP);
                  dm_bad = True;
               }
         }
   else
      {
         dmc->wdf.number = 0;
         dmc->wdf.geometry = NULL;
      }
   if (dm_bad)
      {
         if (dmc->wdf.geometry)
            free(dmc->wdf.geometry);
         free((char *)dmc);
         dmc = NULL;
      }
   break;


case DM_wp:
   dmc->wp.mode = DM_TOGGLE;
   for (p = strtok(p, " \t");
        p != NULL;
        p = strtok(NULL, " \t"))
   {
      if (*p == '-')
         {
            if (*(p+2)) /* make sure there is a space after the -letter */
               cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
            switch (*(p+1))
            {
               case 'B':
               case 'b':
                  dmc->wp.mode = DM_OFF;
                  break;
               case 'T':
               case 't':
                  dmc->wp.mode = DM_ON;
                  break;
               default:
                  cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
                  break;
            }  /* end of switch */
            if (dm_bad)
               break;
         }
      else
         {
            cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
            break;
         }
   }
   if (dm_bad)
      {
         free((char *)dmc);
         dmc = NULL;
      }
   break;


case DM_ww:
   dmc->ww.dash_c = 0;
   dmc->ww.dash_a = '\0';
   dmc->ww.dash_i = '\0';
   dmc->ww.mode = DM_TOGGLE;
   for (p = strtok(p, " \t");
        p != NULL;
        p = strtok(NULL, " \t"))
   {
      if (*p == '-')
         {
            switch (*(p+1))
            {
               case 'A':
               case 'a':
                  dmc->ww.dash_a = 'a';
                  break;
               case 'C':
               case 'c':
                  if (*(p+2) == '\0')
                     p = strtok(NULL, " \t");
                  else
                     p += 2;
                  if (sscanf(p, "%d%9s", &(dmc->ww.dash_c), work) != 1)
                     {
                        snprintf(work, sizeof(work), "(%s) Invalid decimal value (%s%s)", cmd_name, iln(line_no), cmd_line);
                        dm_error(work, DM_ERROR_BEEP);
                        free((char *)dmc);
                        dmc = NULL;
                     }
                  break;
               case 'I':
               case 'i':
                  dmc->ww.dash_i = 'i';
                  break;
               case 'O':
               case 'o':
                  if (dmc->ww.mode == DM_TOGGLE)
                     {
                        if (((*(p+2) | 0x20) == 'n') && (*(p+3) == '\0')) 
                           dmc->ww.mode = DM_ON;
                        else
                           if (((*(p+2) | 0x20) == 'f') && ((*(p+3) | 0x20) == 'f') && (*(p+4) == '\0')) 
                              dmc->ww.mode = DM_OFF;
                           else
                              cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
                     }
                  else
                     cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
                  break;
               default:
                  cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
                  break;
            }  /* end of switch */
            if (dm_bad)
               break;
         }
      else
         {
            cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
            break;
         }
   }
   /* semantic check, if -off was specified, -a and -c are invalid */
   if ((dmc->ww.mode == DM_OFF) && (dmc->ww.dash_a || dmc->ww.dash_c))
      cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
   if (dmc->ww.dash_c)
      dmc->ww.mode = DM_ON;
   if (dm_bad)
      {
         free((char *)dmc);
         dmc = NULL;
      }
   break;


case DM_xa:
   for (p = strtok(p, " \t");
        p != NULL && !dm_bad;
        p = strtok(NULL, " \t"))
      if (*p == '-')
         {
            switch (*(p+1))
            {
               case 'F':
               case 'f':
                  if (dmc->xa.path1 == NULL)
                     dmc->xa.dash_f1 = 'f';
                  else
                     dmc->xa.dash_f2 = 'f';
                  break;
               case 'L':
               case 'l':
                  if (dmc->xa.path1 == NULL)
                     dmc->xa.dash_l = 'l';
                  else
                     {
                        snprintf(work, sizeof(work), "(%s) -l only valid before first paste buffer name (%s%s)", cmd_name, iln(line_no), cmd_line);
                        dm_error(work, DM_ERROR_BEEP);
                        dm_bad = True;
                     }
                  break;
               default:
                  snprintf(work, sizeof(work), "(%s) bad option only -f and -l  allowed (%s%s)", cmd_name, iln(line_no), cmd_line);
                  dm_error(work, DM_ERROR_BEEP);
                  dm_bad = True;
                  break;
            }  /* end of switch */
            if (*(p+2) != '\0') /* make sure there is a space after the -letter */
               {
                  *(p+2) = '\0';
                  snprintf(work, sizeof(work), "(%s) Space required after %s (%s%s)", cmd_name, p, iln(line_no), cmd_line);
                  dm_error(work, DM_ERROR_BEEP);
                  dm_bad = True;
               }
         }
      else
         {
            if (dmc->xa.path1 == NULL)
               {
                  if ((*p == '\'') || (*p == '"'))  /* allow single or double quotes */
                     {
                        i = strlen(p);
                        if (p[i-1] == *p)
                           p[i-1] = '\0';
                        p++;
                     }
                  dmc->xa.path1 = malloc_copy((p));
                  if (!dmc->xa.path1)
                     dm_bad = True;
               }
            else
               if (dmc->xa.path2 == NULL)
                  {
                     if ((*p == '\'') || (*p == '"'))  /* allow single or double quotes */
                        {
                           i = strlen(p);
                           if (p[i-1] == *p)
                              p[i-1] = '\0';
                           p++;
                        }
                     dmc->xa.path2 = malloc_copy((p));
                     if (!dmc->xa.path2)
                        dm_bad = True;
                  }
               else
                  {
                     snprintf(work, sizeof(work), "(%s) Too many command arguments (%s%s)", cmd_name, iln(line_no), cmd_line);
                     dm_error(work, DM_ERROR_BEEP);
                     dm_bad = True;
                     break;
                  }
         } /* end of for loop */
   if ((dmc->xa.path1 == NULL) || (dmc->xa.path2 == NULL))
      {
         snprintf(work, sizeof(work), "(%s) Command requires two names <target> and <source> (%s%s)", cmd_name, iln(line_no), cmd_line);
         dm_error(work, DM_ERROR_BEEP);
         dm_bad = True;
      }
   if (dm_bad)
      {
        if (dmc->xa.path1 != NULL)
            free(dmc->xa.path1);
        if (dmc->xa.path2 != NULL)
            free(dmc->xa.path2);
         free((char *)dmc);
         dmc = NULL;
      }
   break;

case DM_xc:
case DM_xd:
case DM_xl:
case DM_xp:
   dmc->xc.dash_a = 0;
   dmc->xc.dash_r = 0;
   dmc->xc.dash_f = 0;
   dmc->xc.dash_l = 0;
   dmc->xc.path   = NULL;
   /***************************************************************
   *  If this is an xl command, strip off the quoted string first.
   ***************************************************************/
   if (dmc->xc.cmd == DM_xl) 
     {
         dmc->xl.data = NULL;
         q = strpbrk(p, "'\"");
         if (q)
            {
               i = strlen(q);
               dmc->xl.data = CE_MALLOC(i+1);
               if (!dmc->xl.data)
                  return(NULL);  
               sep = *q;
               p2 = copy_with_delim(q+1, dmc->xl.data, sep, escape_char, 0 /* missing delim not ok */);
               if (p2 == NULL)
                  {
                     snprintf(work, sizeof(work), "(%s) Command syntax error - missing delimiter (%s%s)", cmd_name, iln(line_no), cmd_line);
                     dm_error(work, DM_ERROR_BEEP);
                     dm_bad = True;
                  }
               else
                  {
                     while(*p2 == ' ') p2++;
                     if (*p2 != '\0')
                        {
                           snprintf(work, sizeof(work), "(%s) Too many command arguments or quoted string is not last argument (%s%s)", cmd_name, iln(line_no), cmd_line);
                           dm_error(work, DM_ERROR_BEEP);
                           dm_bad = True;
                        }
                  }
               *q = '\0'; /* kill off the trailing quoted string */
            } /* end of found the quoted string */
         else
            {
               snprintf(work, sizeof(work), "(%s) Missing quoted literal string (%s%s)", cmd_name, iln(line_no), cmd_line);
               dm_error(work, DM_ERROR_BEEP);
               dm_bad = True;
            }
     } /* special processing for dm_xl */

   /***************************************************************
   *  Common processing for xc, xp, xd, and xl
   ***************************************************************/
   for (p = strtok(p, " \t");
        p != NULL && !dm_bad;
        p = strtok(NULL, " \t"))
      if (*p == '-')
         {
            if (*(p+2)) /* make sure there is a space after the -letter */
               cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
            switch (*(p+1))
            {
               case 'A':
               case 'a':
                  dmc->xc.dash_a = 'a';
                  break;
               case 'F':
               case 'f':
                  dmc->xc.dash_f = 'f';
                  break;
               case 'L':
               case 'l':
                  dmc->xc.dash_l = 'l';
                  break;
               case 'R':
               case 'r':
                  dmc->xc.dash_r = 'r';
                  break;
               default:
                  snprintf(work, sizeof(work), "(%s) bad option -f, -a, -l,  and/or -r allowed (%s%s)", cmd_name, iln(line_no), cmd_line);
                  dm_error(work, DM_ERROR_BEEP);
                  dm_bad = True;
                  break;
            }  /* end of switch */
            if (*(p+2) != '\0')
               {
                  *(p+2) = '\0';
                  snprintf(work, sizeof(work), "(%s) Space required after %s (%s%s)", cmd_name, p, iln(line_no), cmd_line);
                  dm_error(work, DM_ERROR_BEEP);
                  dm_bad = True;
               }
         }
      else
         {
            if (dmc->xc.path != NULL)
               {
                  snprintf(work, sizeof(work), "(%s) Too many command arguments (%s%s)", cmd_name, iln(line_no), cmd_line);
                  dm_error(work, DM_ERROR_BEEP);
                  dm_bad = True;
                  break;
               }
            else
               {
                  dmc->xc.path = CE_MALLOC(strlen(p)+1);
                  if (dmc->xc.path)
                     strcpy(dmc->xc.path, p);
                  else
                     dm_bad = True;
               }
         } /* end of for loop */
   if (dmc->xc.dash_f && (dmc->xc.path == NULL))
      {
         snprintf(work, sizeof(work), "(%s) Missing file name for -f  (%s%s)", cmd_name, iln(line_no), cmd_line);
         dm_error(work, DM_ERROR_BEEP);
         dm_bad = True;
      }
   if (dm_bad)
      {
        if (dmc->xc.path != NULL)
            free(dmc->xc.path);
        if ((dmc->xc.cmd == DM_xl) && (dmc->xl.data != NULL))
            free(dmc->xl.data);
         free((char *)dmc);
         dmc = NULL;
      }
   break;

case DM_cntlc:
   dmc->cntlc.dash_a = 0;
   dmc->cntlc.dash_r = 0;
   dmc->cntlc.dash_f = 0;
   dmc->cntlc.dash_l = 0;
   dmc->cntlc.dash_h = 0;
   dmc->cntlc.path   = NULL;

   /***************************************************************
   *  processing for cntlc
   ***************************************************************/
   for (p = strtok(p, " \t");
        p != NULL && !dm_bad;
        p = strtok(NULL, " \t"))
      if (*p == '-')
         {
            if (*(p+2)) /* make sure there is a space after the -letter */
               cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
            switch (*(p+1))
            {
               case 'A':
               case 'a':
                  dmc->cntlc.dash_a = 'a';
                  break;
               case 'F':
               case 'f':
                  dmc->cntlc.dash_f = 'f';
                  break;
               case 'L':
               case 'l':
                  dmc->cntlc.dash_l = 'l';
                  break;
               case 'R':
               case 'r':
                  dmc->cntlc.dash_r = 'r';
                  break;
               case 'h':
                  p = strtok(NULL, " \t");
                  if (p == NULL)
                     {
                        snprintf(work, sizeof(work), "(%s) Missing value for -h parm (%s%s)", cmd_name, iln(line_no), cmd_line);
                        dm_error(work, DM_ERROR_BEEP);
                        dm_bad = True;
                      }
                   else
                      dm_bad =  hexparm_to_value(p, cmd_name, line_no, cmd_line, &(dmc->cntlc.dash_h));
                  break;
               default:
                  snprintf(work, sizeof(work), "(%s) bad option -f, -a, -l, -h,  and/or -r allowed (%s%s)", cmd_name, iln(line_no), cmd_line);
                  dm_error(work, DM_ERROR_BEEP);
                  dm_bad = True;
                  break;
            }  /* end of switch */
            if (*(p+2) != '\0')
               {
                  *(p+2) = '\0';
                  snprintf(work, sizeof(work), "(%s) Space required after %s (%s%s)", cmd_name, p, iln(line_no), cmd_line);
                  dm_error(work, DM_ERROR_BEEP);
                  dm_bad = True;
               }
         }
      else
         {
            if (dmc->cntlc.path != NULL)
               {
                  snprintf(work, sizeof(work), "(%s) Too many command arguments (%s%s)", cmd_name, iln(line_no), cmd_line);
                  dm_error(work, DM_ERROR_BEEP);
                  dm_bad = True;
                  break;
               }
            else
               {
                  dmc->cntlc.path = CE_MALLOC(strlen(p)+1);
                  if (dmc->cntlc.path)
                     strcpy(dmc->cntlc.path, p);
                  else
                     dm_bad = True;
               }
         } /* end of for loop */
   if (dmc->cntlc.dash_f && (dmc->cntlc.path == NULL))
      {
         snprintf(work, sizeof(work), "(%s) Missing file name for -f  (%s%s)", cmd_name, iln(line_no), cmd_line);
         dm_error(work, DM_ERROR_BEEP);
         dm_bad = True;
      }
   if (dm_bad)
      {
        if (dmc->cntlc.path != NULL)
            free(dmc->cntlc.path);
        if ((dmc->cntlc.cmd == DM_xl) && (dmc->xl.data != NULL))
            free(dmc->xl.data);
         free((char *)dmc);
         dmc = NULL;
      }
   break;

case DM_ALIAS_EXPANSION:
   /***************************************************************
   *  Process arguments passed to the alias into an arvc/argv.
   *  Then symbolically substitute them into the command line.
   ***************************************************************/
   free((char *)dmc); /* won't need the preallocated dmc in this case */
   dmc = NULL;
   if (char_to_argv(cmd_name, p, cmd_name, cmd_line, &temp_argc, &temp_argvp, escape_char) == 0)
      {
         alias_dollar_vars(alias_expansion, temp_argc, temp_argvp, escape_char, sizeof(alias_line), alias_line);
         dmc = prompt_prescan(alias_line, temp_flag, escape_char);
         if (dmc == NULL)
            dmc = parse_dm_line(alias_line, temp_flag, line_no, escape_char, hash_table); 
      }
   else
      cse_message(work, sizeof(work), cmd_name, cmd_line, line_no, &dm_bad);
   if (temp_argvp)
      {
         free_temp_args(temp_argvp);
         free((char *)temp_argvp);
      }
   break;

default:   /* default is for comamands with no parms */
   p = strtok(p, " \t");
   if (p != NULL)
      {
         if (dmc->any.cmd == DM_NULL)
            snprintf(work, sizeof(work), "(%s) Unknown command name (%s%s)", cmd_name, iln(line_no), cmd_line);
         else
            snprintf(work, sizeof(work), "(%s) Too many command arguments (%s%s)", cmd_name, iln(line_no), cmd_line);
         dm_error(work, DM_ERROR_BEEP);
         free((char *)dmc);
         dmc = NULL;
      }
   break;
}  /* end of switch on cmd_id to parse args */

*new_pos = end_of_cmd;
return(dmc);

} /* end of parse_dm */

static char *CSE  = "(%s) Command syntax error (%s%s)";

static void cse_message(char     *work,              /* output */
                        int       sizeof_work,       /* input  */
                        char     *cmd_name,          /* input  */
                        char     *cmd_line,          /* input  */
                        int       line_no,           /* input  */
                        int      *bad)               /* output */
{
snprintf(work, sizeof_work, CSE, cmd_name, iln(line_no), cmd_line);
dm_error(work, DM_ERROR_BEEP);
*bad = True;
}


/*****************************************************************************
*
*   copy_with_delim - Copy a string which is surrounded by a delimiter character.
*
*   PARAMETERS:
*
*   1.  from      -   pointer to char  (INPUT)
*                     This is the line to copy.   The copy starts just past the
*                     leading delimiter.
*
*   2.  to        -   pointer to char  (OUTPUT)
*                     This is the place to copy the line to
*
*   3.  delim     -   char  (INPUT)
*                     This is the delimiter character to watch for.
*
*   4.  escape_char - char  (INPUT)
*                     This is the escape char.  Escaped delimiters don't count.
*
*   5.  missing_end_delim_ok - int  (INPUT)
*                     This flag specifies whether it is ok if the end delimiter
*                     is missing.  If true, and end of line is reached before the
*                     delimiter is found, all is cool.  Otherwise an error is
*                     issued.
*
*
*   FUNCTIONS:
*
*   1.  Walk the string watching for
*       escape characters, delimiters, and escaped delimiters,
*
*   2.  Copy characters to the to string.
*
*   RETURNED VALUE:
*
*   next_char - pointer to char
*               This is the address of the character position just past the
*               end delimiter or the pointer to the null at the end of the
*               string.  On error NULL is returned.
*
*
*********************************************************************************/

char *copy_with_delim(char    *from,
                      char    *to,
                      char     delim,
                      char     escape_char,
                      int      missing_end_delim_ok)
{
char     *p;
char     *q;

p = from;
q = to;

/***************************************************************
*  
*  Walk the string, copying from the from string to the to string.
*  Watch for escaped delimiters and the ending delimiter.
*  
***************************************************************/

while(*p != '\0')
{
   /*
    * For escaped escape chars, cancel the escape
    * characteristic of the second escape but copy them both
    */
   if (*p == escape_char && *(p+1) == escape_char)
      {
         *q++ = *p++;
         *q++ = *p;
      }
   else
      /*
       * For escaped delimiters, use up the escape, and copy 
       * the delimiter.
       */
      if (*p == escape_char && *(p+1) == delim)
         *q++ = *(++p);
      else
         if (*p != delim)
            *q++ = *p;
          else
             break;

      if (*p == '\0')
         break;
      else
         p++;
}

*q = '\0';

/***************************************************************
*  
*  If we found the delimiter or the parm says a missing delimiter
*  is ok, we are done.  Otherwise return null to flag the error.
*  
***************************************************************/

if (*p == delim)
   return(++p);
else
   {
      if (missing_end_delim_ok)
         return(p);
      else
         return(NULL);
   }
} /* end of copy_with_delim */



/*****************************************************************************
*
*   count_tokens - Count the tokens on a line
*              This routine walks the passed line and counts the tokens on the
*              line.  It is used to determine how much space to malloc.
*
*   PARAMETERS:
*
*      line       -   pointer to char  (INPUT)
*                     This is the line to examine
*
*
*   FUNCTIONS:
*
*   1.  Using a finite state machine approach, walk
*       the line and take note of every time a the contents of the
*       line switches from blank to non-blank and back.
*
*   RETURNED VALUE:
*
*   count   -   int
*               This is the number of tokens on the line.
*
*
*********************************************************************************/

static int   count_tokens(char   *line)
{

#define   CT_INIT           0
#define   CT_IN_TOKEN       1
int       state = CT_INIT;

char     *p;
int       count = 0;

p = line;
while (*p)
{
   switch(state)
   {
   case CT_INIT:
      if (*p != ' ')
         state = CT_IN_TOKEN;
      break;

   case CT_IN_TOKEN:
      if (*p == ' ')
         {
            state = CT_INIT;
            count++;
         }
      break;

   } /* end of switch on state */
   p++;
} /* end of walking the line */

if (state == CT_IN_TOKEN)
    count++;

return(count);

} /* end of count_tokens */


/*****************************************************************************
*
*   copy_kd  - Copy the definition from a key defintion.
*
*              This routine copies the defintion from a kd command.
*              It watches for embedded key definitions, and the ke
*              delimiter.
*
*   PARAMETERS:
*
*   1.  from      -   pointer to char  (INPUT)
*                     This is the line containing the key defintion.
*                     It points to the first non-blank in the key defintion.
*
*   2.  to        -   pointer to char  (OUTPUT)
*                     This is the place to copy the line to
*
*   3.  escape_char - char  (INPUT)
*                     This is the escape char.  It is used to watch for
*                     escaped characters and particularly escaped semicolons
*                     following a "ke".
*
*   4.  cmd_line  -   pointer to char  (INPUT)
*                     This is the full command line.  It is used in generating
*                     error messages.
*
*   5. line_no    -  int (INPUT)
*                    This is the line number used in error message generation.
*                    For lines typed at the window, this is zero, for lines
*                    being processed from a command file, this is the line
*                    number in the file with the command.  Special routine iln
*                    which is defined in parsedm.c returns a null string for
*                    value zero and a formated line number for non-zero.
*
*
*   FUNCTIONS:
*
*   1.  Using a finite state machine approach, walk
*       the line and take note of escaped characters.
*
*   2.  Process all escape characters.  Skip the escape and
*       blindly copy the next character (do watch for end of line).
*
*   3.  Watch for single quoted strings and copy the contents.
*
*   4.  Watch for the ke sequence.  If we find one, look for an
*       escaped semi-colon just past it (allow blanks).  If the
*       escaped semi-colon is found, continue the copy.  Otherwise
*       we are done.
*
*   RETURNED VALUE:
*
*   next_char - pointer to char
*               This is the address of the character position just past the
*               ke.  On error NULL is returned.
*
*
*
*********************************************************************************/

static char *copy_kd(char    *from,
                     char    *to,
                     char     escape_char,
                     char    *cmd_line,
                     int      line_no)
{
char     *p;
char     *q;
int       i;
int       j;
char      msg[MAX_LINE+1];

#define   KD_INIT    0
#define   KD_IN_STR  1
#define   KD_FOUND_K 2
#define   KD_FOUND_E 3
#define   KD_DONE    4
int       state = KD_INIT;


/***************************************************************
*  Special case for Kevin.
*  If a keydef specifies \\ and we are in unix mode, change it
*  the slashes to question marks.  This will handle all simple
*  reverse finds.  Compound finds are another story.
***************************************************************/

if (*from == '\\' && escape_char == '\\')
  {
     p = strrchr(from, '\\');
     if (p != NULL && p != from)
        {
           *from = '?';
           *p    = '?';
        }
  }

p = from;
q = to;
while(state != KD_DONE && *p != '\0')
{
   /***************************************************************
   *  For escaped characters, throw out the escape and 
   *  blindly copy the next character.
   ***************************************************************/
   if (*p == escape_char)
      {
         p++;
         *q++ = *p++;
      }
   else
      switch(state)
      {
      case KD_INIT:
         if (*p == 'k')
            state = KD_FOUND_K;  /* do not advance p */
         else
            {
               if (*p == '\'')
                  state = KD_IN_STR;
               *q++ = *p++;
            }
         break;
   
      case KD_IN_STR:
         if ((*p == '\'') && (*(p-1) != escape_char))  /* allow double escaped quotes -> kd m1   /[~a-zA-Z._@@-$0-9@@/@@\@@[@@]~]/dr; \[~a-zA-Z._@@-$0-9@@/@@\@@[@@]~]\;/?/xc cv_file; tdm;tl;xd junk;es 'cv @@'';xp cv_file;tr;es '@@'';en ke */
            state = KD_INIT;
         *q++ = *p++;
         break;
   
      case KD_FOUND_K:
         if (*(p+1) == 'e')
            state = KD_FOUND_E;  /* do not advance p */
         else
            {
               state = KD_INIT;
               *q++ = *p++;
            }
         break;
   
      case KD_FOUND_E:
         /***************************************************************
         *  We have a ke, look for an escaped semicolon just past it.  
         *  If it is there, there is more kd to follow.  If not, 
         *  We have reached the end.
         ***************************************************************/
         i = 2;
         while(p[i] == ' ' || p[i] == '\t') i++;
         if (p[i] == escape_char && p[i+1] == ';')
            {
               for (j = 0;
                    j <= i+1;
                    j++)
                  if (j != i) /* skip the escape before the semicolon */
                     *q++ = p[j];

               p += (i + 2);
               state = KD_INIT;
            }
         else
            {
               if ((p[i] == ';') || (p[i] == '\0') || (p[i] == '#'))
                  {
                     p += i;
                     state = KD_DONE;
                  }
               else
                  {
                     state = KD_INIT;
                     *q++ = *p++;
                  }
            }
         break;
   
      } /* end switch */

}

*q = '\0';
while((q > to) && ((*(q-1) == ' ') || (*(q-1) == '\t')))
   *(--q) = '\0';  /* trim trailing blanks */

if (state == KD_DONE)
   return(p);
else
   {
      snprintf(msg, sizeof(msg), "(kd) Incomplete definition, ke missing (%s%s)", iln(line_no), cmd_line);
      dm_error(msg, DM_ERROR_BEEP);
      return(NULL);
   }
} /* end of copy_kd */

/*****************************************************************************
*
*   get_num_range  - Pick up the values from  [num,num] and (num,num).
*
*              This routine gets the numeric values out of the point
*              definition sequences.  It is used in parsing the num, markp, and markc
*              DM commands.
*
*   PARAMETERS:
*
*   1.  from           - pointer to char  (INPUT)
*                        This is the line containing the number,number sequence.
*                        The pointer points to the first char past the open bracket.
*
*   2.  first          - pointer to int  (OUTPUT)
*                        The first number is from the set is placed in the parameter.
*                        This parameter is can be positive or negative.
*
*   3.  second         - pointer to int  (OUTPUT)
*                        The second number is from the set is placed in the parameter.
*
*   4.  first_relative - pointer to char (really points to a single char)  (OUTPUT)
*                         0 - returned parameter first did not have a sign
*                         1 - returned parameter first had a leading +
*                        -1 - returned parameter first had a leading -
*
*   5.  second_relative - pointer to char (really points to a single char)  (OUTPUT)
*                         0 - returned parameter second did not have a sign
*                         1 - returned parameter second had a leading +
*                        -1 - returned parameter second had a leading -
*
*   6.  sep         - char  (INPUT)
*                     This is the end separator.  It is a blank for the num command,
*                     a ']' for markc and a ')' for markp.
*
*
*   FUNCTIONS:
*
*   1.  Ignore leading blanks.
*
*   2.  If the first character is a comma, there is no first number,
*       default it to zero.
*
*   3.  Convert the first number to a integer.
*
*   4.  If there is a second number, process it.  If not, 
*       default it to zero.
*
*   RETURNED VALUE:
*
*   next_char - pointer to char
*               This is the address of the character position just past the
*               close bracket.  On error NULL is returned.
*
*
*
*********************************************************************************/

static char  *get_num_range(char    *from,
                            int     *first,
                            int     *second,
                            char    *first_relative,
                            char    *second_relative,
                            char     sep)
{
int         i;
int         rel;
char       *p = from;
char        junk1;
char        junk2;

/***************************************************************
*  Initialize output variables to no movement.
***************************************************************/
*first = 0;
*second = 0;
if (first_relative)
   *first_relative = '\0';
else
   first_relative = &junk1;

if (second_relative)
   *second_relative = '\0';
else
   second_relative = &junk2;

i = 0;
while(*p == ' ') p++;  /* skip leading blanks */

/***************************************************************
*  Extract the row number, or x coordinate from the string.
*  Keep track of whether the number is an absolute or relative.
***************************************************************/
if (*p == ']')         /* if no x or y coordinate "[]"  */
   {
      *first_relative = 1;
      *second_relative = 1;
      return(p+1);
   }
if (*p == ',')         /* if no x coordinate  */
   {
      i = 0;
      *first_relative = 1;
   }
else
   {
      if (*p == '+')
         {
            p++;
            rel = 1;
         }
      else
         if (*p == '-')
            {
               p++;
               rel = -1;
            }
         else
            rel = 0;
      while(*p == ' ') p++;  /* skip blanks */
      while(*p >= '0' && *p <= '9')
         i = (i * 10) + (*p++ & 0x0f);
      if (rel)
         {
            i *= rel;
            *first_relative = rel;
            *second_relative = rel;
         }
   }

/***************************************************************
*  Put the first number in the output variable.
*  If this is the num command, there is only one number and
*  the passed separator is blank.  Skip a trailing semicolon,
*  if it exist and return.
***************************************************************/
*first = i;
if (sep == ' ') /* num command, only one number */
   {
      while(*p == ' ') p++;  /* skip blanks */
      if (*p == ';')
         p++;
      return(p);
   }
else
   *second_relative = 1;  /* possibly [5], keep same column */

/***************************************************************
*  Find the begining of the second number.  The next non-blank
*  should be a comma.  Verify it is there and skip it.
***************************************************************/
while((*p == ' ') || (*p == '\t')) p++;  /* skip blanks */
if (*p == sep)       /* one number */
   return(p+1);

if (*p != ',')
   return(NULL);

p++; /* skip the comma */
while(*p == ' ') p++;  /* skip blanks */

/***************************************************************
*  If we are at the end character, the second number is relative
*  movement of zero.  (no movement).
***************************************************************/
if (*p == sep)
   {
      *second_relative = 1;
      return(p+1);   /* one number */
   }

/***************************************************************
*  Collect the second number.
***************************************************************/
i = 0;
if (*p == '+')
   {
      p++;
      rel = 1;
   }
else
   if (*p == '-')
      {
         p++;
         rel = -1;
      }
   else
      rel = 0;
while(*p == ' ') p++;  /* skip blanks */
while(*p >= '0' && *p <= '9')
   i = (i * 10) + (*p++ & 0x0f);
if (rel)
   {
      i *= rel;
      *second_relative = rel;
   }
else
   *second_relative = 0;

*second = i;

/***************************************************************
*  All that should be left is the end character.
***************************************************************/

while(*p == ' ') p++;  /* skip blanks */

if (*p != sep)  /* syntax time */
   return(NULL);
else
   {
      if (*(++p) == ';')
         p++;
      return(p);
   }
} /* end of get_num_range */

/*****************************************************************************
*
*   comment_scan  - Scan a line for a non-escaped comment start which is not in a string.
*
*              This routine is used to stop processing at a comment char.
*
*   PARAMETERS:
*
*   1.  from      -   pointer to char  (INPUT)
*                     This is the line containing the key defintion.
*                     It points to the first non-blank in the key defintion.
*
*   3.  escape_char - char  (INPUT)
*                     This is the escape char.  It is used to watch for
*                     escaped characters.
*
*
*   FUNCTIONS:
*
*   1.  Using a finite state machine approach, walk
*       the line and take note of escaped characters and strings.
*
*   2.  Process all escape characters.  Skip the escape and
*       blindly accept the next character (do watch for end of line).
*
*   3.  Watch for single quoted strings.
*
*   4.  Watch for a normal comment char (#).  When found, we are done.
*
*   RETURNED VALUE:
*
*   cmd_end  - pointer to char
*               This is the address of the end of the parseable cmd.
*               It is either the position of the # or the null at the end of the string.
*
*
*
*********************************************************************************/

static char *comment_scan(char    *from,
                          char     escape_char)
{
char         *p;
char          dlm_char;

#define   CS_INIT    0
#define   CS_IN_STR  1
#define   CS_DONE    2
int       state = CS_INIT;


p = from;
while(state != CS_DONE && *p != '\0')
{
   /***************************************************************
   *  For escaped characters, throw out the escape and 
   *  blindly copy the next character.
   ***************************************************************/
   switch(state)
   {
   case CS_INIT:
      if (*p == '#')
         state = CS_DONE;  /* do not advance p */
      else
         if (*p == ';')
            state = CS_DONE;  /* do not advance p */
         else
            {
               /***************************************************************
               *  Check for quotes strings. 
               ***************************************************************/
               if ((*p == '\'') || (*p == '"'))
                  {
                     dlm_char = *p;
                     state = CS_IN_STR;
                  }
               p++;
            }
      break;
   
   case CS_IN_STR:
      if ((*p == dlm_char) && (*(p-1) != escape_char))  /* allow double escaped quotes -> kd m1   /[~a-zA-Z._@@-$0-9@@/@@\@@[@@]~]/dr; \[~a-zA-Z._@@-$0-9@@/@@\@@[@@]~]\;/?/xc cv_file; tdm;tl;xd junk;es 'cv @@'';xp cv_file;tr;es '@@'';en ke */
         state = CS_INIT;
      p++;
      break;
   
   } /* end switch */

}

return(p);

} /* end of comment_scan */

/*****************************************************************************
*
*   free_dmc   Free a DMC structure and any associated storage.
*              This routine will free any allocated storage stored
*              with a dmc and then free the passed dmc.
*
*   PARAMETERS:
*
*      dmc    -   pointer to DMC (DM Command)  (INPUT)
*
*   FUNCTIONS:
*
*   1.  switch on the command id and free any storage allocated.
*
*********************************************************************************/


void   free_dmc(DMC   *dmc)
{
int          i;

if (dmc == NULL)
   return;

/* free attached cmd structures recursively, last first */ 
if (dmc->next != NULL)
   free_dmc(dmc->next);

switch(dmc->any.cmd)
{
/***************************************************************
*  For find and rfind, p still points to the first non-blank in
*  the buffer.  Get permanent space for the regular expression
*  and copy it getting rid of the slashes and escape chars.
***************************************************************/
case DM_find:
case DM_rfind:
   if (dmc->find.buff)
      free(dmc->find.buff);
   break;

case DM_bang:
   if (dmc->bang.cmdline)
      free(dmc->bang.cmdline);
   if (dmc->bang.dash_s)
      free(dmc->bang.dash_s);
   break;


case DM_bl:
   if (dmc->bl.l_ident)
      free(dmc->bl.l_ident);
   if (dmc->bl.r_ident)
      free(dmc->bl.r_ident);
   break;

case DM_ce:
case DM_cv:
case DM_cc:
   for (i = 0; i < dmc->cv.argc; i++) 
      free(dmc->cv.argv[i]);
   if (dmc->cv.argv)
      free((char *)dmc->cv.argv);
   break;

case DM_cmdf:
case DM_rec:
   if (dmc->cmdf.path)
      free(dmc->cmdf.path);
   break;


case DM_ceterm:
case DM_cp:
case DM_cpo:
case DM_cps:
   for (i = 0; i < dmc->cp.argc; i++) 
      free(dmc->cp.argv[i]);
   if (dmc->cp.argv)
      free((char *)dmc->cp.argv);
   break;


case DM_dq:
   if (dmc->dq.name)
      free(dmc->dq.name);
   break;


case DM_es:
   if (dmc->es.string != dmc->es.chars && dmc->es.string != NULL)
      free(dmc->es.string);
   break;


case DM_fl:
case DM_geo:
case DM_debug:
case DM_re:
case DM_cd:
case DM_pd:
case DM_env:
case DM_bgc:
case DM_fgc:
case DM_title:
case DM_lbl:
case DM_glb:
case DM_eval:
   if (dmc->geo.parm)
      free(dmc->geo.parm);
   break;


case DM_kd:
case DM_lkd:
case DM_alias:
case DM_mi:
case DM_lmi:
case DM_lsf:
   if (dmc->kd.def)
      free(dmc->kd.def);
   if (dmc->kd.key)
      free(dmc->kd.key);
   break;


case DM_prompt:
   if (dmc->prompt.prompt_str)
      free(dmc->prompt.prompt_str);
   if (dmc->prompt.response)
      free(dmc->prompt.response);
   if (dmc->prompt.target_strt && !dmc->prompt.mult_prmt)
      free(dmc->prompt.target_strt);
   if (dmc->prompt.delayed_cmds && dmc->prompt.delayed_cmds->any.temp)
      free_dmc(dmc->prompt.delayed_cmds);
   break;


case DM_msg:
case DM_prefix:
case DM_sp:
   if (dmc->msg.msg)
      free(dmc->msg.msg);
   break;


case DM_pn:
   if (dmc->pn.path)
      free(dmc->pn.path);
   break;


case DM_s:
case DM_so:
   if (dmc->s.s1)  /* s1 and s2 were done with one malloc */
      free(dmc->s.s1);
   break;


case DM_ts:
   if (dmc->ts.stops)
      free((char *)dmc->ts.stops);
   break;


case DM_wdc:
case DM_ca:
case DM_nc:
case DM_pc:
   if (dmc->wdc.bgc)
      free((char *)dmc->wdc.bgc);
   if (dmc->wdc.fgc)
      free((char *)dmc->wdc.fgc);
   break;


case DM_wdf:
   if (dmc->wdf.geometry)
      free((char *)dmc->wdf.geometry);
   break;


case DM_xa:
   if (dmc->xa.path1 != NULL)
       free(dmc->xa.path1);
   if (dmc->xa.path2 != NULL)
       free(dmc->xa.path2);
   break;


case DM_xc:
case DM_xd:
case DM_xp:
   if (dmc->xc.path)
      free(dmc->xc.path);
   break;

case DM_cntlc:
   if (dmc->cntlc.path)
      free(dmc->cntlc.path);
   break;


default:   /* default is for comamands with extra mallocs parms */
   break;
}  /* end of switch of cmd_id to parse args */

free((char *)dmc);


}  /* end of free_dmc */


/************************************************************************

NAME:      dump_kd - Format and print a DMC command structure.

PURPOSE:    This routine converts from internal format of the key definitions
            to normal format.

PARAMETERS:

   1.  stream         - pointer to FILE (INPUT)
                        This is the stream to write to.  If this pointer is
                        not null, the formatted command(s) will be
                        written to this file.  If it is null, the commands will
                        either be put in the passed buffer or output via dm_error.

   2.  first_dmc      - pointer to DMC (INPUT)
                        This is the command structure linked list for
                        the command(s) to be dumped.
                        If it is null, a null line will be output.

   3.  one_only       - int (INPUT)
                        This flag specifies whether to dump all the command
                        structures passed in the linked list or just the first.
                        VALUES:
                        True   -  Only dump one structure
                        False  -  Dump all the structures in the linked list.

   4.  kd_special     - int (INPUT)
                        This flag specifies that special kd flag characters are to
                        be put in front of each definition.  These are processed
                        by routine process_escapes.  They are used to resolve certain
                        problems when dealing with quotes.
                        True   -  Insert special character in front of each command name.
                        False  -  Do not insert special character

   5.  target_str     - pointer to char (OUTPUT)
                        If the stream file is NULL, and this pointer is not NULL,
                        the formatted commands are placed in this buffer.
                        If this pointer is NULL and the stream parameter is NULL,
                        the formatted commands are passed to dm_error.

   5.  escape_char    - char (just 1) (INPUT)
                        This is the current escape character from the display
                        description.


FUNCTIONS :

   1.   Format each command.

   2.   If the one only flag is set, stop after the first loop.

   3.   If the stream parameter is non-NULL, write the formatted
        commands to the file.

   4.   If the stream parm is NULL and the target_str parameter is
        non-NULL, copy the formatted commands to the target string.

   5.   If both the target parms are NULL, call dm_error with the
        formatted commands.


*************************************************************************/

static unsigned char    kd_special_char[2] = { 0x9e, '\0'};

void dump_kd(FILE    *stream,      /* input   */
             DMC     *first_dmc,   /* input   */
             int      one_only,    /* input   */
             int      kd_special,  /* input   */
             char    *target_str,  /* output  */
             char     escape_char) /* input   */
{

int                   i;
char                  def[MAX_LINE+1024];
DMC                  *dmc;
char                  work[80];
int                   cmd_id;
char                 *p, *q;
char                  escaped_chars[10];

def[0] = '\0';
dmc = first_dmc;

while(dmc != NULL)
{

   cmd_id = dmc->any.cmd;

   /***************************************************************
   *
   *  Print the DM command name.
   *
   ***************************************************************/
   if (kd_special)
      strcat(def, (char *)kd_special_char);

   if ((escape_char == '\\') && (cmd_id == DM_rfind))
      strcat(def, "?");
   else
      if (cmd_id < DM_MAXDEF && cmd_id > 0)
         strcat(def, dmsyms[cmd_id].name);

   if (!dmsyms[cmd_id].supported)
      strcat(def, "(unsupported)");

   /***************************************************************
   *
   *  Now output the command args, if there are any.
   *
   ***************************************************************/

   switch(cmd_id)
   {
   case DM_find:
      if (dmc->find.buff)
         {
            p = &def[strlen(def)]; /* point to the null terminator */
            add_escape_chars(p, dmc->find.buff, "/&", escape_char, False, False);
         }
      strcat(def, "/");
      break;

   case DM_rfind:
      if (dmc->find.buff)
         {
            if (escape_char == '@')
               escaped_chars[0] = '\\';
            else
               escaped_chars[0] = '?';
            escaped_chars[1] = '&';
            escaped_chars[2] = '\0';
            p = &def[strlen(def)]; /* point to the null terminator */
            add_escape_chars(p, dmc->find.buff, escaped_chars, escape_char, False, False);
         }
      if (escape_char == '@')
         strcat(def, "\\");
      else
         strcat(def, "?");
      break;

   case DM_markc:
   case DM_corner:
      p = &def[strlen(def)]; /* point to the null terminator */

      if ((dmc->markc.row_relative) && (dmc->markc.row != 0))
         sprintf(p, "%+d", dmc->markc.row);
      else
         if (dmc->markc.row_relative == 0)
            sprintf(p, "%d", dmc->markc.row+1);

      strcat(def, ",");
      p = &def[strlen(def)]; /* point to the null terminator */

      if ((dmc->markc.col_relative) && (dmc->markc.col != 0))
         sprintf(p, "%+d", dmc->markc.col);
      else
         if (dmc->markc.col_relative == 0)
            sprintf(p, "%d", dmc->markc.col+1);

      if (cmd_id == DM_corner)
         strcat(def, "}");
      else
         strcat(def, "]");
      break;

   case DM_markp:
      strcat(def, itoa(dmc->markp.x,work));
      strcat(def, ",");
      strcat(def, itoa(dmc->markp.y,work));
      strcat(def, ")");
      break;

   case DM_num:
      if (dmc->num.relative)
         {
            if (dmc->num.row > 0)
               strcat(def, "+");
            else
               strcat(def, "-");
            strcat(def, itoa(dmc->num.row, work));
         }
      else
         strcat(def, itoa((dmc->num.row)+1, work));
      break;

   case DM_bang:
      if (dmc->bang.dash_c)
         strcat(def, " -c ");
      if (dmc->bang.dash_m)
         strcat(def, " -m ");
      if (dmc->bang.dash_e)
         strcat(def, " -e ");
      if (dmc->bang.dash_s)
         {
            strcat(def, " -s\"");
            strcat(def, dmc->bang.dash_s);
            strcat(def, "\" ");
         }
      if (dmc->bang.cmdline)
         {
            strcat(def, " ");
            strcat(def, dmc->bang.cmdline);
         }
      break;


   case DM_bl:
      sprintf(work, " %s%s %s",
              (dmc->bl.dash_d ? "-d " : ""),
              (dmc->bl.l_ident ? dmc->bl.l_ident : ""),
              (dmc->bl.r_ident ? dmc->bl.r_ident : ""));
      strcat(def, work);
      break;

   case DM_ca:
   case DM_nc:
   case DM_pc:
      if (dmc->ca.bgc || dmc->ca.fgc)
         {
            if (dmc->ca.bgc)
               {
                  strcat(def, " '");
                  strcat(def, dmc->ca.bgc);
                  strcat(def, "'");
               }
            if (dmc->wdc.fgc)
               {
                  strcat(def, " '");
                  strcat(def, dmc->wdc.fgc);
                  strcat(def, "'");
               }
         }
      break;

   case DM_case:
      sprintf(work, " -%c", dmc->d_case.func);
      strcat(def, work);
      break;

   case DM_ce:
   case DM_cv:
   case DM_cc:
      p = &def[strlen(def)]; /* point to the null terminator */
      for (i = 1; i < dmc->cv.argc; i++)
      {
         strcat(p, " ");
         p = &p[strlen(p)]; /* point to the null terminator */
         escaped_chars[0] = escape_char; /* RES 6/22/95 Lint change */
         strcpy(&escaped_chars[1], "\\$[]()*");
         add_escape_chars(p, dmc->cv.argv[i], escaped_chars, escape_char, True, False);
      }
      break;


   case DM_cmdf:
   case DM_rec:
      sprintf(work, " %s", (dmc->cmdf.dash_p ? "-p " : ""));
      strcat(def, work);
      if (dmc->cmdf.path)
         strcat(def, dmc->cmdf.path);
      break;


   case DM_ceterm:
   case DM_cp:
      escaped_chars[0] = escape_char; /* RES 6/22/95 Lint change */
      strcpy(&escaped_chars[1], "\\$[]()*");
      p = &def[strlen(def)]; /* point to the null terminator */
      for (i = 1; i < dmc->cp.argc; i++)
      {
         strcat(p, " ");
         p = &p[strlen(p)]; /* point to the null terminator */
         add_escape_chars(p, dmc->cp.argv[i], escaped_chars, escape_char, True, False);
      }
      break;


   case DM_cpo:
   case DM_cps:
      escaped_chars[0] = escape_char; /* RES 6/22/95 Lint change */
      strcpy(&escaped_chars[1], "\\$[]()*");
      p = &def[strlen(def)]; /* point to the null terminator */
      sprintf(work, " %s%s%s",
              (dmc->cpo.dash_w ? "-w " : ""),
              (dmc->cpo.dash_d ? "-d " : ""),
              (dmc->cpo.dash_s ? "-s " : ""));
      strcat(p, work);
      for (i = 0; i < dmc->cpo.argc; i++)
      {
         strcat(p, " ");
         p = &p[strlen(p)]; /* point to the null terminator */
         add_escape_chars(p, dmc->cpo.argv[i], escaped_chars, escape_char, True, False);
      }
      break;


   case DM_dq:
      sprintf(work, " %s%s",
              (dmc->dq.dash_s ? "-s " : ""),
              (dmc->dq.dash_b ? "-b " : ""));
      strcat(def, work);
      if (dmc->dq.dash_c)
         {
            sprintf(work, " -c %d ", dmc->dq.dash_c);
            strcat(def, work);
         }
      if (dmc->dq.dash_a)
         {
            sprintf(work, " -a %02X", (unsigned char)dmc->dq.dash_a);
            strcat(def, work);
         }
      if (dmc->dq.name)
         {
            sprintf(work, " %s", dmc->dq.name);
            strcat(def, work);
         }
      break;


   case DM_echo:
      if (dmc->echo.dash_r)
         strcat(def, " -r");
      break;


   case DM_eef:
      if (dmc->eef.dash_a)
         {
            sprintf(work, " -a %02X", (unsigned char)dmc->eef.dash_a);
            strcat(def, work);
         }
      break;


   case DM_ei:
   case DM_inv:
   case DM_ro:
   case DM_sc:
   case DM_sb:
   case DM_wa:
   case DM_wh:
   case DM_wi:
   case DM_ws:
   case DM_lineno:
   case DM_hex:
   case DM_vt:
   case DM_mouse:
   case DM_envar:
   case DM_pdm:
   case DM_caps:
      switch(dmc->ei.mode)
      {
      case DM_OFF:
         strcat(def, " -off");
         break;
      case DM_ON:
         strcat(def, " -on");
         break;
      case DM_AUTO:
         strcat(def, " -auto");
         break;
      default:
         break;
      }
      break;


   case DM_er:
      sprintf(work, " %02X", (unsigned char)dmc->es.chars[0]);
      strcat(def, work);
      break;

   case DM_es:
      strcat(def, " '");
      p = &def[strlen(def)]; /* point to the null terminator */
      add_escape_chars(p, dmc->es.string, "'", escape_char, False, False);
      i = strlen(dmc->es.string) - 1;
      if ((i >= 0) && (dmc->es.string[i] == '\\')) /* RES 9/5/95, cannot end an es string with a backslash due to parsing */
         strcat(def, " ';ee");
      else
         strcat(def, "'");
      break;


   case DM_fl:
   case DM_geo:
   case DM_debug:
   case DM_re:
   case DM_cd:
   case DM_pd:
   case DM_env:
   case DM_bgc:
   case DM_fgc:
   case DM_title:
   case DM_lbl:
   case DM_glb:
   case DM_eval:
      if (dmc->geo.parm)
         {
            strcat(def, " '");
            strcat(def, dmc->geo.parm);
            strcat(def, "'");
         }
      break;


   case DM_icon:
   case DM_al:
   case DM_ar:
   case DM_sic:
   case DM_ee:
      sprintf(work, " %s%s",
              (dmc->icon.dash_i ? " -i" : ""),
              (dmc->icon.dash_w ? " -w" : ""));
      strcat(def, work);
      break;


   case DM_kd:
   case DM_lkd:
   case DM_alias:
   case DM_mi:
   case DM_lmi:
   case DM_lsf:
      strcat(def, " ");
      strcat(def, dmc->kd.key);
      if (dmc->kd.def)
         {
            p = &def[strlen(def)]; /* point to the null terminator */
            *p++ = ' ';
            escaped_chars[0] = escape_char;
            escaped_chars[1] = '\0';
            add_escape_chars(p, dmc->kd.def, escaped_chars, escape_char, False, True);
            strcat(p, " ke;");
         }
      break;


   case DM_msg:
   case DM_prefix:
   case DM_sp:
      if (dmc->msg.msg)
         {
            strcat(def, " '");
            p = &def[strlen(def)]; /* point to the null terminator */
            add_escape_chars(p, dmc->msg.msg, "\\'", escape_char, False, False);
            strcat(def, "'");
         }
      break;


   case DM_ph:
   case DM_pv:
   case DM_bell:
   case DM_fbdr:
      sprintf(work, " %d", dmc->ph.chars);
      strcat(def, work);
      break;

   case DM_pn:
      sprintf(work, " %s", (dmc->pn.dash_c ? " -c" : ""));
      strcat(def, work);
      if (dmc->pn.path)
         {
            strcat(def, " ");
            strcat(def, dmc->pn.path);
         }
      break;


   case DM_prompt:
      if (dmc->prompt.target_strt)
         {
            q = &def[strlen(def)];
            for (p = dmc->prompt.target_strt;
                 *p;
                 p++)
            {
               if (p != dmc->prompt.target)
                  *q++ = *p;
               else
                  {
                     *q++ = *p; /* copy the & */
                     if (dmc->prompt.prompt_str && *dmc->prompt.prompt_str)
                        {
                           *q++ = '\'';
                           strcpy(q, dmc->prompt.prompt_str);
                           q += strlen(dmc->prompt.prompt_str);
                           *q++ = '\'';
                        }
                     /* Deal with multiple prompts, second condition added 10/9/95 RES */
                     if ((dmc->next != NULL) && ((dmc->next)->any.cmd == DM_prompt))
                        {
                           i = p - dmc->prompt.target_strt;
                           dmc = dmc->next; /* go to the next prompt */
                           p = dmc->prompt.target_strt + i;
                        }
                  }
            }
            *q = '\0';
         }
      break;


   case DM_pp:
   case DM_sleep:
      sprintf(work, " %f", dmc->pp.amount);
      strcat(def, work);
      break;


   case DM_rw:
      if (dmc->rw.dash_r)
         strcat(def, " -r");
      break;


   case DM_s:
   case DM_so:
      if (dmc->s.s1)
         {
            strcat(def, " /");
            p = &def[strlen(def)]; /* point to the null terminator */
            add_escape_chars(p, dmc->s.s1, "\\/&", escape_char, False, False);
            strcat(def, "/");
         }
      if (dmc->s.s2)
         {
            if (!(dmc->s.s1))  /* if there was no s1, add the leading slash */
               strcat(def, "/");
            p = &def[strlen(def)]; /* point to the null terminator */
            add_escape_chars(p, dmc->s.s2, "\\/&", escape_char, False, False);
            strcat(def, "/");
         }
      else
        if (dmc->s.s1)  /* if there was no s2 and there was an s1, add the trailing slash */
           strcat(def, "/");
      break;


   case DM_tf:
      if (dmc->tf.dash_l)
         {
            sprintf(work, " -l %d", dmc->tf.dash_l);
            strcat(def, work);
         }
      if (dmc->tf.dash_r)
         {
            sprintf(work, " -r %d", dmc->tf.dash_r);
            strcat(def, work);
         }
      if (dmc->tf.dash_b)
         strcat(def, " -b");
      if (dmc->tf.dash_p)
         strcat(def, " -p");
      break;


   case DM_ts:
      if (dmc->ts.stops)
         {
            for (i = 0; i < dmc->ts.count; i++)
            {
               sprintf(work, " %d", dmc->ts.stops[i]);
               strcat(def, work);
            }
         }
      if (dmc->ts.dash_r)
          strcat(def, " -r");
      break;


   case DM_twb:
      sprintf(work, " -%c", dmc->twb.border);
      strcat(def, work);
      break;


   case DM_wc:
   case DM_reload:
      if (dmc->wc.type)
         {
            sprintf(work, " -%c", dmc->wc.type);
            strcat(def, work);
         }
      break;


   case DM_wdc:
      if (dmc->wdc.number != 0)
         {
            sprintf(work, " %d", dmc->wdc.number);
            strcat(def, work);
            if (dmc->wdc.bgc)
               {
                  strcat(def, " '");
                  strcat(def, dmc->wdc.bgc);
                  strcat(def, "'");
               }
            else
               if (dmc->wdc.fgc)
                  strcat(def, " \"\"");
            if (dmc->wdc.fgc)
               {
                  strcat(def, " '");
                  strcat(def, dmc->wdc.fgc);
                  strcat(def, "'");
               }
         }
      break;


   case DM_wdf:
      if (dmc->wdf.number != 0)
         {
            sprintf(work, " %d", dmc->wdf.number);
            strcat(def, work);
            if (dmc->wdf.geometry)
               {
                  strcat(def, " ");
                  strcat(def, dmc->wdf.geometry);
               }
         }
      break;


   case DM_wp:
      strcat(def,
             ((dmc->wp.mode == DM_OFF) ? " -b" : ((dmc->wp.mode == DM_ON) ? " -t" : "")) );
      break;


   case DM_ww:
      sprintf(work, " %s%s",
              (dmc->ww.dash_a ? " -a" : ""),
              (dmc->ww.dash_i ? " -i" : ""));
      strcat(def, work);
      strcat(def,
             ((dmc->ww.mode == DM_OFF) ? " -off" : ((dmc->ww.mode == DM_ON) ? " -on" : "")) );
      if (dmc->ww.dash_c)
         {
            sprintf(work, " -c %d", dmc->ww.dash_c);
            strcat(def, work);
         }
      break;


case DM_xa:
   if (dmc->xa.dash_l)
      strcat(def, " -l");
   if (dmc->xa.dash_f1)
      strcat(def, " -f");
   if (dmc->xa.path1 != NULL)
      {
         strcat(def, " ");
         strcat(def, dmc->xa.path1);
      }
   if (dmc->xa.dash_f2)
      strcat(def, " -f");
   if (dmc->xa.path2 != NULL)
      {
         strcat(def, " ");
         strcat(def, dmc->xa.path2);
      }
   break;


   case DM_xc:
   case DM_xd:
   case DM_xp:
   case DM_xl:
      sprintf(work, " %s%s%s%s",
              (dmc->xc.dash_a ? " -a" : ""),
              (dmc->xc.dash_r ? " -r" : ""),
              (dmc->xc.dash_l ? " -l" : ""),
              (dmc->xc.dash_f ? " -f" : ""));
      strcat(def, work);
      if (dmc->xc.path)
         {
            strcat(def, " ");
            strcat(def, dmc->xc.path);
         }
      if ((cmd_id == DM_xl) && dmc->xl.data)
         {
            strcat(def, " '");
            p = &def[strlen(def)]; /* point to the null terminator */
            add_escape_chars(p, dmc->xl.data, "'", escape_char, False, False);
            strcat(def, "'");
         }
      break;


   case DM_cntlc:
      if (dmc->cntlc.dash_h)
         {
            sprintf(work, " -h %02X", (unsigned char)dmc->cntlc.dash_h);
            strcat(def, work);
         }
      sprintf(work, " %s%s%s%s",
              (dmc->cntlc.dash_a ? " -a" : ""),
              (dmc->cntlc.dash_r ? " -r" : ""),
              (dmc->cntlc.dash_l ? " -l" : ""),
              (dmc->cntlc.dash_f ? " -f" : ""));
      strcat(def, work);
      if (dmc->cntlc.path)
         {
            strcat(def, " ");
            strcat(def, dmc->cntlc.path);
         }
      break;


   default:   /* default is for comamands with no parms */
      break;
   }  /* end of switch on cmd_id  */


/* if the flag is set do just one */
if (one_only)
   break;

dmc = dmc->next;
if (dmc != NULL && !dmsyms[cmd_id].special_delim)
   strcat(def, ";");

}  /* while more definitions */


/***************************************************************
*  Output the definition.
***************************************************************/
if (stream != NULL)
   fprintf(stream, "%s\n", def);
else
   if (target_str != NULL)
      strcpy(target_str, def);
   else
      {
         escaped_chars[0] = escape_char;
         escaped_chars[1] = '\0';
         add_escape_chars(def, def, escaped_chars, escape_char, False, True) ;
         dm_error(def, DM_ERROR_MSG);
      }

}  /* end of dump_kd */



/******************************************************************************/
static int hexparm_to_value(char  *p,
                            char  *cmd_name,
                            int    line_no,
                            char  *cmd_line,
                            char  *target)  /* one char target string */
{
int           i;
int           j;
int           dm_bad = 0;
char          work[128];

*target = '\0';
if (!p)
   {
      snprintf(work, sizeof(work), "(%s) Missing argument - (%s%s)", cmd_name, iln(line_no), cmd_line);
      dm_error(work, DM_ERROR_BEEP);
      return(True);
   }

if (*p == '\'') /* get rid of quotes in -c '9' */
   {
      p++;
      i = strlen(p) - 1;
      if ((i >= 0) && (p[i] == '\''))
         p[i] = '\0';
   }

switch(strlen(p))
{
case 1:
   i = HEX(*p);
   if (i != -1)
      *target = i;
   else
      {
         snprintf(work, sizeof(work), "(%s) Bad hex char (%c) - (%s%s)", cmd_name, *p, iln(line_no), cmd_line);
         dm_error(work, DM_ERROR_BEEP);
         dm_bad = True;
      }
   break;
case 2:
   if ((*p == '\\') || (*p == '@')) /* allow either escape character */
      switch(*(p+1))
      {
      case 'b':
         *target = '\b';
         break;
      case 'f':
         *target = '\f';
         break;
      case 'n':
         *target = '\n';
         break;
      case 'r':
         *target = '\r';
         break;
      case 't':
         *target = '\t';
         break;
      case 'v':
         *target = '\v';
         break;
      default:
         snprintf(work, sizeof(work), "(%s) Unknown escaped char (%s%s)", cmd_name, iln(line_no), cmd_line);
         dm_error(work, DM_ERROR_BEEP);
         dm_bad = True;
         break;
      } 
   else
      if (*p == '^')
         {
            p++;
            if ((*p >= 'a') && (*p <= 'z'))
               *p &= 0xDF;
            *target = ((unsigned char)*p - (unsigned char)'A') + 1;
         } 
      else
         {
            i = HEX(*p);
            j = HEX(*(p+1));
            if ((i != -1) &&(j != -1))
               *target = (i << 4) + j;
            else
               {
                  snprintf(work, sizeof(work), "(%s) Bad hex string (%s) - (%s%s)", cmd_name, p, iln(line_no), cmd_line);
                  dm_error(work, DM_ERROR_BEEP);
                  dm_bad = True;
               }
      }
   break;
default:
   snprintf(work, sizeof(work), "(%s) Bad hex string (%s%s)", cmd_name, iln(line_no), cmd_line);
   dm_error(work, DM_ERROR_BEEP);
   dm_bad = True;
   break;
} /* end of switch statement */

return(dm_bad);

} /* end of hexparm_to_value */
/******************************************************************************/

static void shift_out_arg(int  *argc, char **argv)
{
int     i;

if (argv[0])
   free((char *)argv[0]);
(*argc)--;

for (i = 0; i < *argc; i++)
   argv[i] = argv[i+1];

argv[i] = NULL;

} /* end of shift_out_arg */

static char s[32];

/******************************************************************************/

char *iln(int line_no)
{
char *ptr = &s[sizeof(s) -1];

s[sizeof(s) -1] = '\0';

if (!line_no) return(ptr);

*--ptr = ' ';
*--ptr = ':';

while (line_no){

*--ptr = line_no % 10 + '0';
line_no /= 10;

}

return(ptr);

} /* iln() */


/************************************************************************

NAME:      add_escape_chars - add escape characters to a line

PURPOSE:   This routine adds required escape characters
           to strings which have special characters which must be
           escaped.

PARAMETERS:
   1.   outname     - pointer to char (OUTPUT)
                      This is the string from inname with any escape
                      characters added in.  It may be the same as inname.

   2.   inname      - pointer to char (INPUT)
                      This is the string to be analyzed

   3.   special_chars - pointer to char (INPUT)
                      This is the list of characters to escape.
                      If NULL is passed, a default set is used.

   4.   escape_char - char (INPUT)
                      This is the current escape character

   5.   add_quotes  - int (INPUT)
                      This flag specifies whether the string
                      should be quoted if it contains blanks or tabs
                      VALUES:
                      True  -  Put in double quotes if the string contains
                               blanks or tabs.
                      False -  No quotes.

   6.   kd_special  - int (INPUT)
                      This flag, when true, specifies that the routine
                      is to watch for the ke; sequence and add the
                      escape char before the ';'

FUNCTIONS :

   1.   Copy the string from inname to outname watching for
        special characters which require aegis escapes.

   2.   Put an at escape character before each special character.


*************************************************************************/


void add_escape_chars(char   *outname,
                      char   *inname,
                      char   *special_chars,
                      char    escape_char,
                      int     add_quotes,
                      int     kd_special)
{
static char     default_special_chars[10] = "?*$[];/\\'";
char            work_buff[MAX_LINE+1];
char           *orig_inname = NULL;
int             i;

static struct
{
char    *name;
int      len;
} special_cmd_list[] = {"es", 2, "msg", 3, NULL, 0};

#define   ES_NORMAL           0
#define   ES_FIRSTCHAR        1
#define   ES_KD_SPECIAL       2
int       state = ES_NORMAL;



if (!special_chars)
   special_chars = default_special_chars;

if (inname == outname)
   {
      outname = work_buff;
      work_buff[0] = '\0';
      orig_inname = inname;
   }
else
   outname[0] = '\0';

if (add_quotes)
   {
      if ((strchr(inname, ' ') != NULL) || (strchr(inname, '\t') != NULL))
         *(outname++) = '"';
      else
         add_quotes = False;
   }


while(*inname != '\0')
{
   switch(state)
   {
   case ES_KD_SPECIAL:
   case ES_NORMAL:
      /***************************************************************
      *  Check for the special header character which can be put
      *  on by dump_kd
      ***************************************************************/
      if (kd_special && ((unsigned char)*inname == kd_special_char[0]))
         {
            state = ES_FIRSTCHAR;
            inname++;
         }
      else
         {
            if (kd_special)
               {
                  if ((';' == *inname) && ('e' == *(inname-1)) && ('k' == *(inname-2)))
                     *(outname++) = escape_char;
                  else
                     /* in kd_special mode and not ES_KD_SPECIAL state, escape quote characters */
                     if ((state != ES_KD_SPECIAL) && ('\'' == *inname))
                        *(outname++) = escape_char;

                  if ((strchr(special_chars, *inname) != NULL))
                     *(outname++) = escape_char;
               }
            else
               if ((strchr(special_chars, *inname) != NULL))
                  *(outname++) = escape_char;
            *(outname++) = *(inname++);
         }
      break;

   case ES_FIRSTCHAR:
      state = ES_NORMAL;
      for (i = 0; special_cmd_list[i].name && (state == ES_NORMAL); i++)
          if (strncmp(inname, special_cmd_list[i].name, special_cmd_list[i].len) == 0)
             state = ES_KD_SPECIAL;
      break;

   } /* end of switch */
}

if (add_quotes)
   *(outname++) = '"';

*outname = '\0';

if (orig_inname)
   strcpy(orig_inname, work_buff);

} /* end of add_escape_chars */



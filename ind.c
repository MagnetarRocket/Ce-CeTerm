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
*  module ind.c
*
*  Routines: (external)
*     dm_ind       -   Do indent processing
*     filter_in    -   Returns a file pointer to input stream
*     filter_out   -   Returns a file pointer to output stream
*     ind_stat     -   Return stat struct on file
*     get_ind      -   Get the entire .Cetypes file and return a pointer for X (#includes done)
*     dm_lsf       -   Language Sensitive Filter
*     is_lsf_key   -   Test is a keykey is an lsf keykey
*     lsf_access   -   Does access on virtual files
*
*  Routines (internal)
*     load_ind      -   Open and read a .Cetype file and return it to get_ind
*     add_directive -   Add a directive(DMC) to an indent linked list
*     get_lsf_cmds  -   Retrieve lsf item commands from hash table
*     ptype         -   Print a hash entry type
*     prnt_lsf_hash -   Print the lsf hash table
*     not_empty     -   Filters out blank lines
*     get_rule      -   Get a rule from a hash if present
*
***************************************************************/

#include <stdio.h>        /* /usr/include/stdio.h           */
#include <string.h>         /* /usr/include/string.h        */
#include <errno.h>          /* /usr/include/errno.h         */
#include <sys/types.h>    /* /usr/include/type.h           */
#include <sys/stat.h>     /* /usr/include/sys/stat.h        */
#include <fcntl.h>
#include <sys/stat.h>

#include "buffer.h"
#include "debug.h" 
#include "dmsyms.h" 
#include "dmwin.h" 
#include "emalloc.h"
#include "hsearch.h"
#include "ind.h" 
#include "kd.h" /* needed for KEY_KEY_SIZE */
#include "parsedm.h" 
#include "shmatch.h"
#include "search.h"
#include "memdata.h"

#define CURRENT '-'
#define ABOVE   '^'
#define BELOW   'V'

#ifdef WIN32
#include <io.h>

#define popen _popen
#define pclose _pclose
#endif

/***************************************************************
*  Hash table key format for lsf (language sensitive filter)
***************************************************************/
#define IN_HASH_TYPE   0x7670
#define IN_KEY_FORMAT  "%04d%c%03d%04X0000"


/***************************************************************
*  
*  Constants for the types of rules lines.
*  
***************************************************************/
#define RULE_TYPE_FILEPAT  '1'
#define RULE_TYPE_READ     '2'
#define RULE_TYPE_WRITE    '3'
#define RULE_TYPE_STAT     '4'
#define RULE_TYPE_SEARCH   '5'

/***************************************************************
*  
*  Local Prototypes
*  
***************************************************************/


static DMC *get_lsf_cmds(void                 *hsearch_data,
                         int                   rule_num,
                         char                  rule_type,
                         int                   seq);  

static char   *ptype(char type);
static void   prnt_lsf_hash(void  *hsearch_data);
static int    not_empty(char *);
static void   get_rule(void **data, void *hsearch_data, IND_DATA  **private, char *file);

static char msg[256];

/***********************************************************************
*                    Implementation notes
*
*
*   Rules based indenting:
*   
*   *  '*.[ch]' '*a.mail'
*   <  'optionsal;dm;cmds' zcat {}
*   >  'optionsal;dm;cmds' compress -c > {}.out  -- $CE_FILE instead of {} 
*   =  stat function
*   #  comment
*   ^ /regexp/; dm;cmd
*   ^ \regexp\; dm;cmd
*   V /regexp/; dm;cmd
*   - \regexp\; dm;cmd
*   - /regexp/; dm;cmd
*   V \regexp\; dm;cmd
*                         -- blank lines 
*   ! /tmp/file           -- include processing
*   
*   
*   Notes:
*   1.  PW recursive check
*   2.  ksh matcher for *.[ch]
*   3.  ce -ftype .mail
*   4.  Test prompt in trailer commands
*   examples  {.Z .ps .crypt .c .cxx .html ...}
*   
*   Q&A 
*   a) $CE_FILE instead of {}
*   b) current line number line 73 of ind.c
*   c) compress $CE_INPUT > $CE_FILE (FIFO) or compress $CE_FILE > $CE_TARGET (with .Z)
*   d) multiple *'s EX: * *.z *.mail
*   e) stat why? use in .cetypes file
*   f) #6 below ???
*   g) how do we prevent // from screwing up saved expressions
*   
*   Rules based indenting.  
*   dm command ind (indent)
*   
*   1.  positions cursor on the current line.
*   2.  looks are rules of the form   /regx/  {indent ! "DM;CMDS"" }
*                               or    \regx\  {indent ! "DM;CMDS"" }
*   3.  Finds the preceding and following non-blank lines.
*   4.  Applies the rules in order till a match is found
*   5.  Positions cursor to start position of that line
*       + or minus the indent or runs the list of dm cmds.
*   6.  Indent examples:   +5    indent 5 beyond the start
*                                of the matched line
*                          -3    indent 3 before the start of
*                                the matched line
*                           6    put the cursor in column 6
*   7.  A Null regular expression means match the previous/next
*       non-blank line.
*   8.  The command has an option to ignore rules and position
*       based on the previous or next non-blank line.
*   9.  ind [-p] [-n] [-c] [<indent_num>]
*       specifying no options causes rules to be checked.
*       -p -> position based on the previous non-blank lines start point
*       -n -> position based on the next non-blank lines start point
*       -c -> position based on the current lines start point
*       start point is the first non-blank.
*   
*
* Things that should be done:
*   
*************************************************************************/

/************************************************************************
*
* NAME:      dm_ind  -  DM ind command
*
*
* PURPOSE:    This routine will do indent processing
*
* PARAMETERS:  
*
*
*
* FUNCTIONS :
*
*
*   1.
*
*   2. 
*
*   3. 
*        
*
*************************************************************************/

DMC *dm_ind(DISPLAY_DESCR *dspl_descr)   
{

int sline;                 /* line being scanned */
int redraw_needed = 0;

int rule = 0;
int seq = 0;
int found = True;
int i;
DMC *dmc;

#ifdef ready
DMC *ndmc;
char buff[MAX_LINE+1];
void *sdata;
int from_line, from_col, to_line, to_col; 
int offset; 
char *line;
int direction;
#endif

sline = dspl_descr->cursor_buff->current_win_buff->file_line_no;
if (!(dspl_descr->ind_data)){
    found = False;
    while (dmc = get_lsf_cmds(dspl_descr->hsearch_data, rule, RULE_TYPE_FILEPAT, seq)){
        for (i = 1;i< dmc->cp.argc ;i++ ){
         /*     DEBUG28(fprintf(stderr,"  Ind: shmatch(%s,%s)\n", file, dmc->cp.argv[i]);)*/
         /*     if (shell_match(file, dmc->cp.argv[i])){
                 found = True;
                 break;
             }*/
        }
        if (found) break;
        rule++;
    }
}

if (!found)
    return(0);

#ifdef ready
while (dmc = get_lsf_cmds(dspl_descr->hsearch_data, rule, RULE_TYPE_FILEPAT, seq)){

    offset = sline;
    switch(lsf_direction(dmc->es.string[0]));{
       case ABOVE:
            do{
                line = get_line_by_num(token, offset);
                offset--;
            }while (line && not_empty(line);
            ndmc = dmc->next;
            if (!line || !ndmc || ((ndmc->cmd != DM_rfind) && (ndmc->cmd != DM_find)))
               continue;

            direction = ndmc->DM_find ? 1 : 0;
            /* if the next command is not a search just pass it off to bob */
            break;
            
            
       case BELOW:
            break;

       case CURRENT:
            break;

    } /* switch */
    search(dspl_descr->main_pad->token , 
           from_line,
           from_col, 
           to_line, 
           to_col,
           direction,
           ...,
           ,&sdata);

} /* while more directives and not a successfull match */
#endif

return (0);

}   /* dm_ind() */

/*************************************************************************
*
*   filter_in -
*    $CE_FILENAME refers to the file its self.
*
**************************************************************************/

FILE   *filter_in(void **data, void  *hsearch_data, char *file, char* mode)
{
int seq = 0;
DMC *dmc;
IND_DATA  *private;

DEBUG28(prnt_lsf_hash(hsearch_data);)
DEBUG28(fprintf(stderr, "   Filter_in(%s,%s)\n\n ", file, mode);)

get_rule(data, hsearch_data, &private, file);

dmc = get_lsf_cmds(hsearch_data, private->rule, RULE_TYPE_READ, seq);

if (!dmc){
   DEBUG28(fprintf(stderr, "  Ind: no rule found.\n");) 
   return(fopen(file, mode)); 
}

DEBUG28(fprintf(stderr, "  Ind: matched rule[%d]\n", private->rule);)

if (dmc = get_lsf_cmds(hsearch_data, private->rule, RULE_TYPE_READ, seq)){
    DEBUG28(fprintf(stderr, "  Ind: in-popen(%s,%s)\n", dmc->bang.cmdline, mode);)
    return(private->fpi = popen(dmc->bang.cmdline, mode));
}

return(fopen(file, mode));

}  /* filter_in() */

/*************************************************************************
*
*   filter_out -
*
**************************************************************************/

FILE   *filter_out(void **data, void *hsearch_data, char *file, char* mode)
{
int seq = 0;
DMC *dmc;
IND_DATA *private;

get_rule(data, hsearch_data, &private, file);

dmc = get_lsf_cmds(hsearch_data, private->rule, RULE_TYPE_WRITE, seq);

if (dmc){
  DEBUG28(fprintf(stderr, "  Ind: out-popen(%s,%s)\n", dmc->bang.cmdline, mode);)
  return(private->fpo = popen(dmc->bang.cmdline, mode));
}

return(fopen(file, mode));

} /* filter_out() */

/*************************************************************************
*
*   ind_stat -  Fills in a stat buf for pw.c to examine.
*
**************************************************************************/

int ind_stat(void **data, void *hsearch_data, char *file, struct stat *buf)
{
int seq = 0;
DMC *dmc;
IND_DATA *private;
int i;
FILE *fp;

DEBUG28(fprintf(stderr,"  Ind: ind_stat(%s)\n", file);)

get_rule(data, hsearch_data, &private, file);

dmc = get_lsf_cmds(hsearch_data, private->rule, RULE_TYPE_STAT, seq);

if (dmc){
  DEBUG28(fprintf(stderr, "  Ind: stat-popen(%s)\n", dmc->bang.cmdline);)
  if (!(fp = popen(dmc->bang.cmdline, "r"))) return(-1);
#ifdef WIN32
  i = fscanf(fp, "%d %d %d %d %d %d %d %d %d %d",
         &buf->st_mode,
         &buf->st_ino,
         &buf->st_dev,
         &buf->st_nlink,
         &buf->st_uid,
         &buf->st_gid,
         &buf->st_size,
         &buf->st_atime,
         &buf->st_mtime,
         &buf->st_ctime);
#else 
#ifndef OpenNT
  i = fscanf(fp, "%d %d %d %d %d %d %d %d %d %d %d %d %d",
         &buf->st_mode,
         &buf->st_ino,
         &buf->st_dev,
         &buf->st_rdev,
         &buf->st_nlink,
         &buf->st_uid,
         &buf->st_gid,
         &buf->st_size,
         &buf->st_atime,
         &buf->st_mtime,
         &buf->st_ctime,
         &buf->st_blksize,
         &buf->st_blocks);
#else
  i = fscanf(fp, "%d %d %d %d %d %d %d %d %d %d %d",
         &buf->st_mode,
         &buf->st_ino,
         &buf->st_dev,
         &buf->st_nlink,
         &buf->st_uid,
         &buf->st_gid,
         &buf->st_size,
         &buf->st_atime,
         &buf->st_mtime,
         &buf->st_ctime,
         &buf->st_blksize);
#endif  /* OpenNT */
#endif  /* WIN32  */

  if (i == 13) return(0);
  return(-1);
}

  DEBUG28(fprintf(stderr, "  Ind: stat-normal stat(%s)\n", file);)

return(stat(file, buf));

}   /* ind_stat() */

int lsf_access(void **data, void *hsearch_data, char *file, int mode)
{

return(access(file, mode));

}

/*************************************************************************
*
*   free_ind - dummy
*
**************************************************************************/

void    free_ind(void *data){} 

/*************************************************************************
*
*   get_rule - get the right lsf rule
*
**************************************************************************/

static void get_rule(void **data, void *hsearch_data, IND_DATA  **private, char *file)
{
int found, i, seq = 0;
DMC *dmc;
int rule = 0;

if (!(*data)){
    found = False;
    while (dmc = get_lsf_cmds(hsearch_data, rule, RULE_TYPE_FILEPAT, seq)){
        for (i = 1;i< dmc->cp.argc ;i++ ){
              DEBUG28(fprintf(stderr,"  Ind: shmatch(%s,%s)\n", file, dmc->cp.argv[i]);)
              if (shell_match(file, dmc->cp.argv[i])){
                 found = True;
                 break;
             }
        }
        if (found) break;
        rule++;
    }
    *private = *data = (IND_DATA *)CE_MALLOC(sizeof(struct ind_data));
    (*private)->rule = rule;
}else{
    dmc = get_lsf_cmds(hsearch_data, ((IND_DATA *)*data)->rule, RULE_TYPE_FILEPAT, seq);
    *private = (IND_DATA *)*data;
}

} /* get_rule() */

/*************************************************************************
*
*   lsf_close - close a potentially popend filter
*
**************************************************************************/

void lsf_close(FILE *fp, void *data)
{

DEBUG28(fprintf(stderr, "  Ind: lsf_close()\n");)

if (data && ((fp == ((IND_DATA *)data)->fpi) || 
             (fp == ((IND_DATA *)data)->fpo))){ 
    pclose(fp); 
    DEBUG28(fprintf(stderr, "  Ind:     pclose()\n");)
}
fclose(fp);

}

/*************************************************************************
*
*   print_lsf_hash -  print the lsf hash table for diagnostics
*
**************************************************************************/

static void prnt_lsf_hash(void  *hsearch_data)
{

int rule = 0;
int seq = 0;
DMC *dmc;
char buff[MAX_LINE+1];

while (dmc = get_lsf_cmds(hsearch_data, rule, RULE_TYPE_FILEPAT, seq)){
    dump_kd(NULL, dmc, False, False, buff, '\\');
    fprintf(stdout, "Rule[%d], Type='%s', Command='%s'\n", rule, ptype(RULE_TYPE_FILEPAT), buff);

    dmc = get_lsf_cmds(hsearch_data, rule, RULE_TYPE_READ, seq);
    dump_kd(NULL, dmc, False, False, buff, '\\');
    fprintf(stdout, "   Type='%s', Command='%s'\n", ptype(RULE_TYPE_READ), buff);

    dmc = get_lsf_cmds(hsearch_data, rule, RULE_TYPE_WRITE, seq);
    dump_kd(NULL, dmc, False, False, buff, '\\');
    fprintf(stdout, "   Type='%s', Command='%s'\n", ptype(RULE_TYPE_WRITE), buff);

    dmc = get_lsf_cmds(hsearch_data, rule, RULE_TYPE_STAT, seq);
    dump_kd(NULL, dmc, False, False, buff, '\\');
    fprintf(stdout, "   Type='%s', Command='%s'\n", ptype(RULE_TYPE_STAT), buff);

    seq=0;
    while (dmc = get_lsf_cmds(hsearch_data, rule, RULE_TYPE_SEARCH, seq++)){
        dump_kd(NULL, dmc, False, False, buff, '\\');
        fprintf(stdout, "   Type='%s', Command='%s', Seq=%d\n", ptype(RULE_TYPE_SEARCH), buff, seq);
    }
    fprintf(stdout, "\n");

    seq = 0;
    rule++;
}

} /* prnt_lsf_hash() */

static char *ptype(char type)
{

if (type == RULE_TYPE_FILEPAT) return("File Name");
if (type == RULE_TYPE_READ) return("Read");
if (type == RULE_TYPE_WRITE) return("Write");
if (type == RULE_TYPE_STAT) return("Stat");
if (type == RULE_TYPE_SEARCH) return("Search");
return("Error");

}
/*************************************************************************
*
*   is_virtual -  Determines if a file is a virtual file or not,
*                 assumes ind_stat was called first!
*
**************************************************************************/

int is_virtual(void **data, void *hsearch_data, char *file)
{
struct stat buf;

if (!*data)
   ind_stat(data, hsearch_data, file, &buf);

if (*data)
   return(NULL != get_lsf_cmds(hsearch_data, ((IND_DATA *)*data)->rule, RULE_TYPE_STAT, 0));

return(0);

} /* is_virtual() */

/************************************************************************

NAME:      dm_lsf  - Language Sensitive Filter

PURPOSE:    This routine "does" a lsf cmd.  It causes the filter item to be installed
            into this Ce session and possibly into the global keydef property.

PARAMETERS:

   1. dmc          -  pointer to DMC  (INPUT)
                      This the first DM command structure for the mi 
                      command.  If there are more commands on the
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
   1.  Parse the keyname to the the pulldown name and the menu item number.

   2.  If the data in the key definition is NULL, this is a request for
       data about that key definition.  Look up the definition and output
       it to the DM command window via dump_kd.

   3.  Prescan the defintion to see if it contains any prompts for input.
       such as   mi *h  cv /ce/help &'Help on: '  ke

   4.  If the prescan produced no results, parse the key definition.

   5.  If the parse worked, install the menu item.

RETURNED VALUE:
   rc   -  int
           The returned value shows whether a menu item was successfully
           loaded.
           0   -  keydef loaded
           -1  -  keydef not loaded, message already generated

*************************************************************************/

int   dm_lsf(DMC               *dmc,
             int                update_server,
             DISPLAY_DESCR     *dspl_descr)
{
int                   rc = 0;
/*int                   i;*/
int                   rule_num;
char                  rule_char;
int                   rule_seq = 0;
DMC                  *first_dmc;
DMC                  *new_dmc;
char                  the_key[KEY_KEY_SIZE];
char                  work[80];
char                  msg[512];
char                 *the_def;
char                  work_def[MAX_LINE+20];

/***************************************************************
*  Check for a menu item name being passed.
*  Format is name/number
*  This block of code sets up variables mi_name and mi_num
***************************************************************/
if (dmc->lsf.key && *(dmc->lsf.key))
   {
      if (!strcmp(dmc->mi.key, "print")){
          prnt_lsf_hash(dspl_descr->hsearch_data);
          return(0);
      }
      if (sscanf(dmc->mi.key, "%d/%c", &rule_num, &rule_char) != 2)
         {
            snprintf(msg, sizeof(msg), "(lsf) Rule name is not of the form <number>/<one_char> (%s%s)\n", iln(dmc->lsf.line_no), dmc->mi.key);
            dm_error(msg, DM_ERROR_BEEP);
            rc = -1;
         }
   }
else
   {
      snprintf(msg, sizeof(msg), "(lsf) Null rule name (%s%s)\n", iln(dmc->lsf.line_no), dmc->mi.key);
      dm_error(msg, DM_ERROR_BEEP);
      rc = -1;
   }

if (rc)
   return(rc);


/***************************************************************
*  Check if this is a request to dump an existing lsf.
*  If so, dump it and we are done.
***************************************************************/
if (dmc->lsf.def == NULL)
   {
      dm_error("Can't extract LSF lines yet", DM_ERROR_BEEP);
      return(-1);  /* not really failure, but no menu item was loaded */
   }

/***************************************************************
*  For the file pattern line, use a cp structure so we get back
*  and argc and argv
***************************************************************/
if (rule_char == '*')
   {
      strcpy(work_def, "cp ");
      strcat(work_def, dmc->lsf.def);
      the_def = work_def;
   }
else{
   the_def = dmc->lsf.def;
   if (!get_lsf_cmds(dspl_descr->hsearch_data, rule_num, RULE_TYPE_FILEPAT, 0)){
      snprintf(msg, sizeof(msg), "(lsf) File type rule '*' must proceed other rules, rule #%d (%s%s)\n", rule_num, iln(dmc->lsf.line_no), dmc->mi.key);
      dm_error(msg, DM_ERROR_BEEP);
      return(-1);
   }
}
/***************************************************************
*  Parse the line into a DMC structure. Prompts are not allowed.
***************************************************************/
new_dmc = parse_dm_line(the_def, 0, dmc->lsf.line_no, dspl_descr->escape_char, dspl_descr->hsearch_data);

if (new_dmc != NULL)
   {
      if (new_dmc == DMC_COMMENT)
         {
            /***************************************************************
            *  Special case, install a null command.
            *  Used when with  mi ^f1 ke  (undefining items)
            ***************************************************************/
            new_dmc = (DMC *)CE_MALLOC(sizeof(DMC));
            if (!new_dmc)
               return(-1);
            new_dmc->any.next         = NULL;
            new_dmc->any.cmd          = DM_NULL;
            new_dmc->any.temp         = False;
         }



      switch(rule_char)
      {
      case '*':
         rule_char = RULE_TYPE_FILEPAT;
         break;

      case '<':
         rule_char = RULE_TYPE_READ;
         break;

      case '>':
         rule_char = RULE_TYPE_WRITE;
         break;

      case '=':
         rule_char = RULE_TYPE_STAT;
         break;

      case '^':
      case '-':
      case 'V':
         /***************************************************************
         *  Put the rule char in an 'es' command to go at the front of the list.
         ***************************************************************/
         first_dmc = (DMC *)CE_MALLOC(sizeof(DMC));
         if (!first_dmc)
            return(-1);
         memset((char *)first_dmc, 0, sizeof(DMC));
         first_dmc->any.next         = NULL;
         first_dmc->any.cmd          = DM_es;
         first_dmc->any.temp         = False;
         first_dmc->es.string        = first_dmc->es.chars;
         first_dmc->es.string[0]     = rule_char; /* terminator in place from the memset */

         /***************************************************************
         *  Splice the the es of the menu item text to the front of the list
         ***************************************************************/
         first_dmc->next = new_dmc;
         new_dmc = first_dmc;

         rule_char = RULE_TYPE_SEARCH;
         
         while(get_lsf_cmds(dspl_descr->hsearch_data, rule_num, rule_char, rule_seq) != NULL)
            rule_seq++;

         break;

      default:
         break;
      }

      DEBUG28(
         fprintf(stderr, "(lsf) Installing:  ");
         dump_kd(stderr, new_dmc, 0, False, NULL, dspl_descr->escape_char);
      )
      /***************************************************************
      *  Build the key from the key name.
      ***************************************************************/
      snprintf(the_key, sizeof(the_key), IN_KEY_FORMAT, rule_num, rule_char, rule_seq, IN_HASH_TYPE);
      DEBUG28(fprintf(stderr, "dm_lsf:  key \"%s\" ", the_key);)
      /***************************************************************
      *  Update the X server property if required.  Not required is
      *  when we are doing a -load or -reload (done en masse when done).
      ***************************************************************/
      rc = install_keydef(the_key, new_dmc, update_server, dspl_descr, dmc);
      if (rc != 0)
         {
            snprintf(work, sizeof(work), "(%s) Unable to install language rule, internal error", dmsyms[dmc->kd.cmd].name);
            dm_error(work, DM_ERROR_LOG);
            free_dmc(new_dmc);
            rc = -1;  /* make sure it has the expected value */
         }
   }
else
   rc = -1;

return(rc);

}  /* end of dm_lsf */


/************************************************************************

NAME:      get_lsf_cmds - Retrieve lsf item commands from hash table


PURPOSE:    This routine gets the DMC list from the hash table for
            menu items.

PARAMETERS:
   1.  hsearch_data -  pointer to void (INPUT)
                       This is the hash table which is used to look up
                       key definitions, aliases, and menu items.
                       It is associated with the current display.

   2.  rule_num     -  int (INPUT)
                       This is the rule number for the pulldown.

   3.  type         -  char (INPUT)
                       This is the type of rule,
                       numeric portion of the key used to find the menu item.
                       values:  from the defines
                       RULE_TYPE_FILEPAT
                       RULE_TYPE_READ   
                       RULE_TYPE_WRITE  
                       RULE_TYPE_STAT  
                       RULE_TYPE_SEARCH 

   4.  seq           - int (INPUT)
                       This is the sequence number.  It is always zero except in the
                       case of rule type SEARCH in which case it is the rule.
                       Literals defined at the top of this module

FUNCTIONS : 
   1.   Build the key for the event and look it up.

   2.   Return the list of dm command structures.

RETURNED VALUE:
   new_dmc    -   pointer to DMC
                  These are the DMC's associated with the menu item.
                  NULL can be returned.

*************************************************************************/

static DMC *get_lsf_cmds(void                 *hsearch_data,
                         int                   rule_num,
                         char                  rule_type,
                         int                   seq)  
{
DMC                  *new_dmc;
ENTRY                *def_found;
ENTRY                 sym_to_find;
char                  the_key[KEY_KEY_SIZE];
/*char                  msg[512];*/

snprintf(the_key, sizeof(the_key), IN_KEY_FORMAT, rule_num, rule_type, seq, IN_HASH_TYPE);
/* DEBUG28(fprintf(stderr, "dm_lsf:  key \"%s\" ", the_key);)  */
sym_to_find.key = the_key;
def_found = (ENTRY *)hsearch(hsearch_data, sym_to_find, FIND);
if (def_found != NULL)
   new_dmc = (DMC *)def_found->data;
else
   new_dmc = NULL;

return(new_dmc);

} /* end of get_lsf_cmds */


/************************************************************************

NAME:      is_lsf_key  - Test is a keykey is an lsf keykey

PURPOSE:    This routine checks to see if this is an internal lsf key.

PARAMETERS:
   1. keykey       -  pointer to char  (INPUT)
                      This the keykey to be tested.

FUNCTIONS :

   1.  Parse the keykeym and verify that the last piece the special value.

RETURNED VALUE:
   rc   -  int
           The returned value shows the keykey was for an alias
           False  -  Not an lsf keykey
           True   -  Is an lsf keykey

*************************************************************************/

int   is_lsf_key(char          *keykey)
{
int          rule_num;
char         rule_type;
int          rule_seq;
int          magic;
int          i;

i = sscanf(keykey, IN_KEY_FORMAT, &rule_num, &rule_type, &rule_seq, &magic);
if ((i != 4) || (magic != IN_HASH_TYPE))
   return(False);
else
   return(True);

} /* end of is_lsf_key */

/* this routine burns off blanks and tabs and returns NULL or a char */
/* for true or false                                                 */

static int not_empty(char *line) 
{

while (*line && (*line != '\t') && (*line != ' ')) 
       line ++;

return(*line);

}

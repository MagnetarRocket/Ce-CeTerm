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
*  module dmfind.c
*
*  Routines:
*     dm_find            -  Do a find or reverse find.
*     dm_s               -  Do a substitute or substitute once (s or so)
*     dm_continue_find   -  Continue a find started by dm_start_find. (via dm_find).
*     dm_continue_sub    -  Continue a find started by dm_start_find. (via dm_s)
*     dm_re              -  Convert a regular expression from aegis to unix and display
*     find_border_adjust - Adjust first line to set border around the find.
*
*
*   Internal:
*     dm_start_find      -   Start a find with a cutoff spot.
*                            This allows the find to be done as a
*                            continued as a  background task.
*
*     dm_start_sub       -   Start a substitute with a cutoff spot.
*                            This allows the substitute to be 
*                            continued as a  background task.
*
*     max_line_len       -   Calculate the maximum length of a group of lines
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h      */
#include <limits.h>         /* /usr/include/limits.h     */

#define DMFIND_C 1

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "debug.h"
#include "dmfind.h"
#include "dmsyms.h"
#include "dmwin.h"
#include "emalloc.h"
#include "getevent.h"
#include "mark.h"
#include "memdata.h"
#include "mvcursor.h"
#include "parms.h"
#include "parsedm.h"
#include "search.h"
#include "typing.h"   /* needed for flush */

#ifndef HAVE_STRLCPY
#include "strl.h"
#endif

/***************************************************************
*  
*  This is the number of lines to search for dm_start_find and
*  dm_continue_find before returning DM_FIND_IN_PROGRESS.
*  
***************************************************************/

#define DM_FIND_SEARCH_LINES 512

/***************************************************************
*  
*  Static data hung off the display description.
*
*  The following structure is hung off the private
*  field in the find data structure hung off the display description.
*  Descriptions of each field follow routine dm_s at about line 500 in
*  this file.
*  
***************************************************************/

typedef struct {
   /* variables used in find and substitute */
   int         from_line;
   int         from_col;
   int         to_line;
   int         to_col;
   int         reverse;
   int         case_sensitive;
   PAD_DESCR  *pad;

   /* additional vars used in substitute. */
   int         sub_once;
   int         last_found_line;
   int         last_found_col;
   int         found_once;

   /* special variables used in substitute when newlines get inserted */
   int         cast_in_concrete_from_line;
   int         cast_in_concrete_to_line;
   int         newlines_added;
   int         rectangular;

   int         substitute_changes_count;

   char        last_find_pattern[MAX_LINE+1];
   char        last_sub_pattern[MAX_LINE+1];
/*   char        substitute[MAX_LINE+1];  RES  9/5/95 Seems to be not used */
} FIND_PRIVATE;

/***************************************************************
*  
*  Prototypes for local routines
*  
***************************************************************/

static void  dm_start_find(PAD_DESCR    *pad,              /* input  */
                           FIND_DATA    *find_data,        /* input / output */
                           int           from_line,        /* input  */
                           int           from_col,         /* input  */
                           int           reverse_find,     /* input  */
                           char         *pattern,          /* input  */
                           int           case_sensitive,   /* input  */
                           int           do_it_all,        /* input  */
                           int          *found_line,       /* output */
                           int          *found_col,        /* output */
                           char          escape_char,      /* input  */
                           void        **search_private_ptr);/* input / output  */

static void  dm_start_sub(PAD_DESCR    *pad,              /* input  */
                          FIND_PRIVATE *private,          /* input / output */
                          int           from_line,        /* input  */
                          int           from_col,         /* input  */
                          int           to_line,          /* input  */
                          int           to_col,           /* input  */
                          char         *pattern,          /* input  */
                          char         *substitute,       /* input  */
                          int           do_it_all,        /* input  */
                          int           sub_once,         /* input  */
                          int          *found_line,       /* output */
                          int          *found_col,        /* output */
                          int           rectangular,      /* input  */
                          char          escape_char,      /* input  */
                          void        **search_private_ptr);/* input / output  */

static int max_line_len(DATA_TOKEN   *token,
                        int           top_line,
                        int           bottom_line);

static   void ins(char *string, char *line, char *ptr);




/************************************************************************

NAME:      dm_find  -  DM find // or \\ commands

PURPOSE:    This routine intiate a find.

PARAMETERS:

   1.  dmlist   - pointer to DMC (INPUT)
                  This is the DM command structure for the find
                  to be processed.

   2.  find_data   - pointer to FIND_DATA (INPUT / OUTPUT)
                     This is the static data needed to support find and substitute.

   3.  cursor_buff - pointer to BUFF_DESCR (INPUT / OUTPUT)
                     This is the buffer description which shows the current
                     location of the cursor.

   4.  case_sensitive - int (INPUT)
                        This flag indicates whether the search is case sensitive.
                        0  -  case insensitive
                        1  -  case sensitive


   5.  search_failed - pointer to int (OUTPUT)
                      This flag indicates it a search failure.
                      Nothing was found.  It is not set if the
                      search reverts to background.  It is used
                      to flag failed searches when other commands
                      are stacked behind this search.  It causes
                      the other commands to be aborted.
                      True  -  Search was not satistified.  Either not
                               found or bad regular expression.
                      False -  Search was successful or is "in progress".

FUNCTIONS :

   1.   find the start point for the search.

   2.   If this find is part of a list of commands, do it.  Otherwise start
        it with the possiblity of a continue.

   3.   Process the resultant found, not found, or in progress.


OUTPUTS:
   redraw -  If the screen and cursor need to be redrawn, true is returned,
             Otherwise 0 is returned.
        

*************************************************************************/

int   dm_find(DMC               *dm_list,               /* input   */
              FIND_DATA         *find_data,             /* input / output */
              BUFF_DESCR        *cursor_buff,           /* input / output  */
              int                case_sensitive,        /* input   */
              int               *search_failed,         /* output  */
              char               escape_char)           /* input   */
{

int                      from_line;
int                      from_col;
PAD_DESCR               *from_buffer;
int                      found_line;
int                      found_col;
int                      redraw_needed = 0;
int                      find_dlm;
char                     msg[512];
FIND_PRIVATE            *private;

/***************************************************************
*  Make sure the static private data area has been allocated.
***************************************************************/
if (find_data->private == NULL)
   {
      find_data->private = (void *)CE_MALLOC(sizeof(FIND_PRIVATE));
      if (find_data->private == NULL)
         {
            *search_failed = True;
            return(0);
         }
      memset((char *)find_data->private, 0, sizeof(FIND_PRIVATE));
   }
private = (FIND_PRIVATE *)find_data->private;

/***************************************************************
*  get the start point for the search.
***************************************************************/
from_line   = cursor_buff->current_win_buff->file_line_no;
from_col    = cursor_buff->current_win_buff->file_col_no;
from_buffer = cursor_buff->current_win_buff;

DEBUG2(fprintf(stderr, "(%s) in %s from [%d,%d]  \"%s\"\n",
       dmsyms[dm_list->xc.cmd].name, which_window_names[from_buffer->which_window],
       from_line, from_col,
       (dm_list->find.buff ?dm_list->find.buff  : ""));
)


/***************************************************************
*  If this is not a repeat find, the find buff points to
*  the search string.  Save it away for later messages.
***************************************************************/
if (dm_list->find.buff != NULL && dm_list->find.buff[0] != '\0' )
   strcpy(private->last_find_pattern, dm_list->find.buff);


/***************************************************************
*  If this is part of a complex search, make sure the find
*  finishes before we continue.  Othewise get the find started.
*  and let it run in background.
***************************************************************/
dm_start_find(from_buffer,
              find_data,
              from_line,
              from_col,
              (dm_list->find.cmd == DM_rfind),
              dm_list->find.buff,
              case_sensitive,
              (dm_list->next != NULL),  /* do it all parm */
              &found_line,
              &found_col,
              escape_char,
              &find_data->search_private);


/***************************************************************
*  Set up the find delimiter in case we need it.
***************************************************************/
if (dm_list->find.cmd == DM_find)
   find_dlm = '/';
else
   if (cursor_buff->current_win_buff->display_data->escape_char == '@')
      find_dlm = '\\';
   else
      find_dlm = '?';


/***************************************************************
*  If the find failed, flag the error and clear any remaining cmds.
***************************************************************/

if (found_line == DM_FIND_NOT_FOUND)
   {
      snprintf(msg, sizeof(msg), "No match :  %c%s%c", find_dlm, private->last_find_pattern, find_dlm);
      dm_error(msg, DM_ERROR_BEEP);
      redraw_needed = dm_position(cursor_buff, from_buffer, from_line, from_col);
      *search_failed = True;
   }
else
   {
      /***************************************************************
      *  In progress, will continue find in background mode
      ***************************************************************/
      if (found_line == DM_FIND_IN_PROGRESS)
         {
            snprintf(msg, sizeof(msg), "Searching for %c%s%c", find_dlm, private->last_find_pattern, find_dlm);
            dm_error(msg, DM_ERROR_MSG);
            change_background_work(NULL, BACKGROUND_FIND, True);
            *search_failed = False;
         }
      else
         if (found_line != DM_FIND_ERROR)
            {
               /***************************************************************
               *  We found it.
               ***************************************************************/
               *search_failed = False;
               flush(from_buffer);
               from_buffer->file_line_no = found_line;
               from_buffer->file_col_no  = found_col;
               from_buffer->buff_ptr = get_line_by_num(from_buffer->token, from_buffer->file_line_no);
               redraw_needed = dm_position(cursor_buff, from_buffer, found_line, found_col);
               if ((from_buffer->which_window == MAIN_PAD) &&
                   (from_buffer->display_data->find_border > 0) &&
                   (dm_list->next == NULL) && /* unencumbered find's only */
                   find_border_adjust(from_buffer, from_buffer->display_data->find_border))
                  {
                     redraw_needed |= dm_position(cursor_buff, from_buffer, found_line, found_col);
                     redraw_needed |= (from_buffer->redraw_mask & FULL_REDRAW);
                  }                   
               if (from_buffer->which_window != DMOUTPUT_WINDOW)
                  dm_error("", DM_ERROR_MSG);
               DEBUG2(fprintf(stderr, "dm_find:  found at [%d,%d] in %s\n", found_line, found_col, which_window_names[from_buffer->which_window]);)
            }
         else
            /*  for dmfind error, the message has already been issued */
            *search_failed = True;
   }

return(redraw_needed);

} /* end of dm_find */




/************************************************************************

NAME:      dm_s  -  DM s (substitute) or so (substitute once)

PURPOSE:    This routine intiate a substitute.

PARAMETERS:

   1.  dmlist   - pointer to DMC (INPUT)
                  This is the DM command structure for the find
                  to be processed.

   2.  find_data   - pointer to FIND_DATA (INPUT / OUTPUT)
                     This is the static data needed to support find and substitute.

   3.  mark1       - pointer to MARK_REGION (INPUT)
                     If the substitute is over a range, this is the mark at the
                     set when the dr was executed or is the first mark in the
                     1,$s...

   4.  cursor_buff - pointer to BUFF_DESCR (INPUT)
                     This is the buffer description which shows the current
                     location of the cursor.

   5.  search_failed - pointer to int (OUTPUT)
                      This flag indicates it a search failure.
                      Nothing was found.  It is not set if the
                      search reverts to background.  It is used
                      to flag failed searches when other commands
                      are stacked behind this search.  It causes
                      the other commands to be aborted.
                      True  -  Search was not satistified.  Either not
                               found or bad regular expression.
                      False -  Search was successful or is "in progress".
                        
   6.  rectangular     - int (INPUT)
                         This flag indicates a rectangualar search in a marked region.
                        
FUNCTIONS :

   1.   Get the range to do the substitute on.

   2.   If there are more commands in the stack, to them.  Otherwise start the 
        substitute with the possiblity of a continue.

   3.   Process the result as either a found, not found, or continue.
        


OUTPUTS:
   redraw -  If the screen and cursor need to be redrawn, true is returned,
             Otherwise 0 is returned.
        

*************************************************************************/

int   dm_s(DMC               *dm_list,               /* input   */
           FIND_DATA         *find_data,             /* input / output */
           MARK_REGION       *mark1,                 /* input   */
           BUFF_DESCR        *cursor_buff,           /* input   */
           int               *search_failed,         /* output  */
           int                rectangular,           /* input   */
           char               escape_char)           /* input   */
{

int                      top_line;
int                      top_col;
int                      bottom_line;
int                      bottom_col;
PAD_DESCR               *buffer;
int                      found_line;
int                      found_col;
int                      redraw_needed = 0;
int                      rc;
char                    *line;
char                     msg[512];
FIND_PRIVATE            *private;
char                    *lcl_substitute;


/***************************************************************
*  Make sure the static private data area has been allocated.
***************************************************************/
if (find_data->private == NULL)
   {
      find_data->private = (void *)CE_MALLOC(sizeof(FIND_PRIVATE));
      if (find_data->private == NULL)
         {
            *search_failed = True;
            return(0);
         }
      memset((char *)find_data->private, 0, sizeof(FIND_PRIVATE));
   }
private = (FIND_PRIVATE *)find_data->private;

/***************************************************************
*  range to perform the search over.
***************************************************************/
rc = range_setup(mark1,              /* input   */
                 cursor_buff,        /* input   */
                 &top_line,          /* output  */
                 &top_col,           /* output  */
                 &bottom_line,       /* output  */
                 &bottom_col,        /* output  */
                 &buffer);           /* output  */

/***************************************************************
*  Flag the bad text range.
***************************************************************/
if (rc != 0)
   {
      snprintf(msg, sizeof(msg), "(%s) Invalid Text Range", dmsyms[dm_list->xc.cmd].name);
      dm_error(msg, DM_ERROR_BEEP);
      return(0);
   }

/***************************************************************
*  For a zero size range, treat it as no match.
*  Treat rectangular as a special case when top and bottom columns match.
***************************************************************/
if (rectangular && (top_col == bottom_col))
   {
      if (top_line == bottom_line)
         {
            line = get_line_by_num(buffer->token, top_line);
            if (line != NULL)
               bottom_col = strlen(line);
         }
      else
         bottom_col = max_line_len(buffer->token, top_line, bottom_line);
      if (bottom_col <= top_col)
         {
            dm_error("No match (zero size region)", DM_ERROR_BEEP);
            redraw_needed = dm_position(cursor_buff, buffer, top_line, top_col);
            return(redraw_needed);
         }
   }
else
   if (top_line == bottom_line && top_col == bottom_col)
      {
         dm_error("No match (zero size region)", DM_ERROR_BEEP);
         redraw_needed = dm_position(cursor_buff, buffer, top_line, top_col);
         return(redraw_needed);
      }

/***************************************************************
*  Save the from and to strings for later if we need them.
***************************************************************/
if (dm_list->s.s1 != NULL && dm_list->s.s1[0] != '\0' )
   strcpy(private->last_find_pattern, dm_list->s.s1);

if (dm_list->s.s2 != NULL && dm_list->s.s2[0] != '\0' && dm_list->s.s2[0] != '&')
   strcpy(private->last_sub_pattern, dm_list->s.s2);


/***************************************************************
*  
*  Set the local substitute field from the passed parameter.
*  Changed 9/5/95 for bug in reading keydef
*  kd F3s    tl; s/^    //; [+1] ke                  # Shift line/region left 4 spaces
*  From the X server.  Parameter substitute ends up NULL.
*  Old code initialized lcl_substitute in it's definition.
*  Repeat substitute is only when pattern and substitute are NULL.
*  
***************************************************************/

if (dm_list->s.s2)
   lcl_substitute = dm_list->s.s2;
else
   if (dm_list->s.s1 && *(dm_list->s.s1))
      lcl_substitute = "";
   else
      lcl_substitute = REPEAT_SUBS;



/***************************************************************
*  If this is part of a complex set of cmds, make sure the substitute
*  finishes before we continue.  Othewise get the find started.
*  and let it run in background.
***************************************************************/

DEBUG2(fprintf(stderr, "(%s) in %s from : [%d,%d]  to : [%d,%d]\n",
       dmsyms[dm_list->xc.cmd].name,  which_window_names[buffer->which_window],
       top_line, top_col, bottom_line, bottom_col);
)

private->substitute_changes_count = 0;

dm_start_sub(buffer,
             private,
             top_line,
             top_col,
             bottom_line,
             bottom_col,
             private->last_find_pattern,  /* changed 12/2/94 fixes case when find was case insensitive on a mixed case, followed by s//something/ */
             lcl_substitute,
             (dm_list->next != NULL),  /* do it all parm */
             (dm_list->find.cmd == DM_so),
             &found_line,
             &found_col,
             rectangular,
             escape_char,
             &find_data->search_private);

/***************************************************************
*  Find failed, flag the error and put the cursor back.
***************************************************************/


if (found_line == DM_FIND_NOT_FOUND)
   {
      snprintf(msg, sizeof(msg), "No match :  /%s/%s/", private->last_find_pattern, private->last_sub_pattern);
      dm_error(msg, DM_ERROR_BEEP);
      line = get_line_by_num(buffer->token, bottom_line);
      if (line && ((rc = strlen(line)) < bottom_col))
         bottom_col = rc;
      redraw_needed = dm_position(cursor_buff, buffer, bottom_line, bottom_col);
      *search_failed = True;
   }
else
   {
      /***************************************************************
      *  In progress, will continue find in background mode
      ***************************************************************/
      if (found_line == DM_FIND_IN_PROGRESS)
         {
            snprintf(msg, sizeof(msg), "Substituting /%s/%s/", private->last_find_pattern, private->last_sub_pattern);
            dm_error(msg, DM_ERROR_MSG);
            change_background_work(NULL, BACKGROUND_SUB, True);
            *search_failed = False;
         }
      else
         if (found_line != DM_FIND_ERROR)
            {
               /***************************************************************
               *  We found it - force a redraw of the screen.
               ***************************************************************/
               *search_failed = False;
               (void) dm_position(cursor_buff, buffer, found_line, found_col/*+1  res 6/6/93, commented out +1 */);
               redraw_needed = (cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW);
               snprintf(msg, sizeof(msg), "%d substitution%s made.", private->substitute_changes_count,((private->substitute_changes_count > 1) ? "s" : ""));
               dm_error(msg, DM_ERROR_MSG);
               DEBUG2(   fprintf(stderr, "dm_s:  last found at [%d,%d]\n", found_line, found_col);)
            }
         else
            /* for dmfind error, the message has already been output */
            *search_failed = True;
   }

return(redraw_needed);

} /* end of dm_s */


/***************************************************************
*  
*  static global variables used by this module.
*
*  These variables save the state of the search when dm_start_find
*  returns DM_FIND_IN_PROGRESS and a call to dm_continue_find
*  is required.  They are also used when dm_start_sub encounters the
*  continue condition.  They are stored in the FIND_PRIVATE structure.
*  
*  1.  from_line        - int 
*                                The top (lowest numbered) line in the search.  For a forward search
*                                this is the start, for a backwards search this is the end and
*                                is always zero.  For a forward search, this number is bumped
*                                when the search goes into background mode and is maintained
*                                as the current search start for each segment.  That is if
*                                nothing is found in DM_FIND_SEARCH_LINES, this number is
*                                set to DM_FIND_SEARCH_LINES past the original start.
*
*  2.  from_col         - int
*                                The column corresponding to from_line.  For a reverse
*                                search this is always zero.
*
*  3.  to_line          - int
*                                The bottom (highest numbered) line in the search.  For a forward
*                                search this is always the total lines in the file.  For a backward
*                                search this is the start of the search.  For a backward search,
*                                This number is decremented for background mode finds the same way
*                                as from_line is incremented.
*
*  4.  to_col           - int
*                                This is the column corresponding to to_line.  For
*                                a forward find this is the set to the max line lenght + 1;  For a
*                                reverse find, this is the starting point.
*
*  5.  reverse          - int 
*                                This flag indicates whether the current find is in the forward or
*                                reverse direction.
*                                0  -  forward
*                                1  -  reverse
*
*  6.  case_sensitive   - int
*                                This flag indicates whether the search is case sensitive or insensitive.
*                                This is also used to detect changes in case sensitivity which force the 
*                                search string to be reprocessed on a // call.
*                                0  -  case insensitive
*                                1  -  case sensitive
*
*  7.  pad              - pointer to PAD_DESCR
*                                This is the pad being searched.
*
***************************************************************/


/***************************************************************
*  
*  additional vars used in substitute.
*  These are also stored in the FIND_PRIVATE structure.
*  
*  1.  substitute          - array of char
*                                   This is a saved copy of the substitute string
*  
*  2.  sub_once            - array of char
*                                   This flag indicates whether one or more substitutes are to be performed.
*                                   0  -  Do all substitutes in the range
*                                   1  -  quit after one substitute.
*
*  3.  last_found_line     - int
*                                   This is the last line number where something was found.  It is used
*                                   in positioning the cursor and window after a substitute.
*
*  4.  last_found_col      - int
*                                   This is the column number which matches last_found_line.
*
*  5.  found_once          - int
*                                   This flag is set to true if at least one substitute took place.
*
*  6.  cast_in_concrete_from_line - int
*                                   This is the original from line.  It is like from_line except it
*                                   does not change with continue finds.  It is used when a change adds
*                                   newlines and the routine in search.c to reprocess the new lines needs to
*                                   be called.
*
*  7.  cast_in_concrete_to_line   - int
*                                   This is the original to line.  It is like to_line except that it
*                                   does not change with continue finds.  It is used when a change adds
*                                   newlines and the routine in search.c to reprocess the new lines needs to
*                                   be called.
*           
*  8.  newlines_added      - int
*                                   This is a running count of the number of newlines added by search (/substitute).
*                                   It is used to determine if a call to process_inserted_newlines is required.
*  
*  9.  rectangular         - int
*                                   This flag is used to identify substitutes in a rectangularly marked region.
*  
***************************************************************/



/************************************************************************

NAME:      dm_start_find  -  Initiate a find

PURPOSE:    This routine intiate a find.  It may do all or part of the find.

PARAMETERS:

   1.  pad              - pointer to PAD_DESCR (INPUT)
                          This is the pad description for the pad where the find is being done.

   2.  find_data        - pointer to FIND_DATA (INPUT)
                          This is the address of the static data used by find and substitute.

   3.  from_line        - int (INPUT)
                          This is the line the search starts at.

   4.  from_col         - int (INPUT)
                          This is the column the search starts at.

   5.  reverse_find     - int (INPUT)
                          This flag indicates whether the search is to be performed
                          in the forward or reverse direction in the file.
                          0  -  search forward
                          1  -  search backward through the file

   6.  pattern          - pointer to char (INPUT)
                          This is the pattern to find.  It is a UNIX regular expression
                          in either aegis or unix format.

   7.  case_sensitive   - int (INPUT)
                          This flag indicates whether the search is case sensitive.
                          0  -  case insensitive
                          1  -  case sensitive


   8.  do_it_all        - int (INPUT)
                          This flag specifies that the search is to be performed on
                          all lines, not just the first DM_FIND_SEARCH_LINES.
                          In this case DM_FIND_IN_PROGRESS is never returned.
                        
   9.  found_line       - pointer to int (OUTPUT)
                          This is the line where the pattern was found or one of the 
                          following special values.
                          DM_FIND_NOT_FOUND    -  The pattern was not found. (defined in search.h)
                          DM_FIND_IN_PROGRESS  -  The pattern was not found in the first
                                                  DM_FIND_SEARCH_LINES.  The search will
                                                  be continued in background mode.
                          DM_FIND_ERROR        -  Bad regular expression, search not run.

  10.  found_col        - pointer to int (OUTPUT)
                          This is the column where the pattern was found.  If found_line is
                          one of the special values, this parm has no meaningfull information.

GLOBAL VARIABLES:
   The following global variables are set by this routine for use by dm_continue_find.  They are
   global within this module but not visible from outside this module.

   1.  from_line        - int (OUTPUT)
   2.  from_col         - int (OUTPUT)
   3.  to_line          - int (OUTPUT)
   4.  to_col           - int (OUTPUT)
   5.  reverse          - int (OUTPUT)
   6.  case_sensitive   - int (OUTPUT)
   7.  pad              - int (OUTPUT)


FUNCTIONS :

   1.   Set up the parms to search and call it.




*************************************************************************/

static void  dm_start_find(PAD_DESCR    *pad,              /* input  */
                           FIND_DATA    *find_data,        /* input / output */
                           int           from_line,        /* input  */
                           int           from_col,         /* input  */
                           int           reverse_find,     /* input  */
                           char         *pattern,          /* input  */
                           int           case_sensitive,   /* input  */
                           int           do_it_all,        /* input  */
                           int          *found_line,       /* output */
                           int          *found_col,        /* output */
                           char          escape_char,      /* input  */
                           void        **search_private_ptr)/* input / output  */
{
int                      lcl_to_line;
int                      lcl_to_col;
int                      lcl_from_line;
int                      lcl_from_col;
int                      dummy_parm;
int                      all_data_read;
FIND_PRIVATE            *private = (FIND_PRIVATE *)find_data->private;

/***************************************************************
*  
*  If we have changed case sensitivity and are doing a // (repeat find)
*  We must reprocess the search string.
*  Thus we check, 
*  1.  Case sensitivity has changed since last time.
*  2.  There is no pattern (thus it is a repeat find)
*  3.  There is a last pattern (not the first find)
*  
***************************************************************/

if ((private->case_sensitive != case_sensitive) &&
    ((pattern == NULL) || (pattern[0] == '\0')) &&
    (private->last_find_pattern[0] != '\0'))
   {
      pattern = private->last_find_pattern;
   }

/***************************************************************
*  
*  Save variables which will be needed by continue find.
*  
***************************************************************/

private->reverse          = reverse_find;
private->pad              = pad;
private->case_sensitive   = case_sensitive;

DEBUG2(
   fprintf(stderr, "@start_find[%d,%d] %s \"%s\"\n",
           from_line, from_col,
           (reverse_find ? "Reverse" : "Forward"),
           (pattern ? pattern : ""));
)

if (!reverse_find)
   {
      /***************************************************************
      *  Search to end of file, use lines_read as the high water mark.
      *  In the first pass, we only search a max of DM_FIND_SEARCH_LINES.
      *
      *  from_col is set to zero because we will either finish
      *  the substitute here, in which case the var is irrelevant, or
      *  we will continue the search later in which case the start col
      *  is always zero.
      ***************************************************************/
      lcl_to_line = from_line + DM_FIND_SEARCH_LINES;
      all_data_read = load_enough_data(lcl_to_line);
      if (all_data_read && (lcl_to_line > total_lines(pad->token)))
         lcl_to_line = total_lines(pad->token);

      private->from_col = -1;

      if (all_data_read)
         private->to_line = total_lines(pad->token);
      else
         private->to_line = INT_MAX;
      private->to_col = MAX_LINE+1;
      lcl_to_col  = MAX_LINE+1;

      /***************************************************************
      *  Search up to DM_FIND_SEARCH_LINES lines, then determine
      *  if we found something, failed, or need to continue later.
      ***************************************************************/
      search(private->pad->token,
             from_line,
             from_col,
             lcl_to_line,
             &lcl_to_col, /* pass by reference */
             reverse_find,
             pattern,
             NULL,   /* no substitute string */
             !private->case_sensitive,
             found_line,
             found_col,
             &dummy_parm,
             0 /* not rectangular */,
             0 /* not rectangular */,
             0 /* not rectangular */,
             NULL /* not in substitute mode */,
             0 /* not substitute once */,
             escape_char,
             search_private_ptr);


      private->from_line = lcl_to_line + 1;
      if (*found_line == DM_FIND_NOT_FOUND)
         if (private->from_line  < private->to_line)
            *found_line = DM_FIND_IN_PROGRESS;

   }  /* end of forward find */
else
   {  /* reverse find */

      /***************************************************************
      *  Search to beginning of file, use lines_read as the high water mark.
      *  In the first pass, we only search a max of DM_FIND_SEARCH_LINES.
      ***************************************************************/
      private->to_line   = from_line;
      private->to_col    = 0;
      private->from_line = 0;
      private->from_col  = -1;

      lcl_to_line    = private->to_line;
      lcl_to_col     = from_col;

      if (!do_it_all)
         {
            lcl_from_line  = private->to_line - DM_FIND_SEARCH_LINES;
            if (lcl_from_line <= 0)
               lcl_from_line = 0;
         }
      else
         lcl_from_line = 0;

      lcl_from_col   = 0;


      search(private->pad->token,
             lcl_from_line,
             lcl_from_col,
             lcl_to_line,
             &lcl_to_col, /* pass by reference */
             private->reverse,
             pattern,
             NULL,   /* no substitute string */
             !private->case_sensitive,
             found_line,
             found_col,
             &dummy_parm,
             0 /* not rectangular */,
             0 /* not rectangular */,
             0 /* not rectangular */,
             NULL /* not in substitute mode */,
             0 /* not substitute once */,
             escape_char,
             search_private_ptr);

      private->to_line = lcl_from_line - 1;
      if (*found_line == DM_FIND_NOT_FOUND)
         if (private->to_line  >= private->from_line)
            *found_line = DM_FIND_IN_PROGRESS;
   } /* end of reverse find */

if (do_it_all)
   while(*found_line == DM_FIND_IN_PROGRESS)
     dm_continue_find(find_data, found_line, found_col, &pad, escape_char);

DEBUG2(
if (private->reverse)
   fprintf(stderr, "end start_find[%d,%d] - lcl end [%d,%d]\n", *found_line, *found_col, lcl_from_line, lcl_from_col);
else
   fprintf(stderr, "end start_find[%d,%d] - lcl end [%d,%d]\n", *found_line, *found_col, lcl_to_line, lcl_to_col);
)

} /* end of dm_start_find */


/************************************************************************

NAME:      dm_continue_find  -  Continue a find

PURPOSE:    This routine continues a find started by dm_start_find.  It is
            called as part of background processing.

PARAMETERS:
   1.  found_line       - pointer to int (OUTPUT)
                          This is the line where the pattern was found or one of the 
                          following special values.
                          DM_FIND_NOT_FOUND    -  The pattern was not found.
                          DM_FIND_IN_PROGRESS  -  The pattern was not found in the first
                                                  DM_FIND_SEARCH_LINES.  The search will
                                                  be continued in background mode.

   2.  found_col        - pointer to int (OUTPUT)
                          This is the column where the pattern was found.  If found_line is
                          one of the special values, this parm has no meaningfull information.

   3.  found_pad        - pointer to PAD_DESCR (OUTPUT)
                          This is the pad where the find was being done.


GLOBAL VARIABLES:
   The following global variables are set by this routine for use by dm_continue_find.  They are
   global within this module but not visible from outside this module.

   1.  from_line        - int (INPUT / OUTPUT)
   2.  from_col         - int (INPUT)
   3.  to_line          - int (INPUT / OUTPUT)
   4.  to_col           - int (INPUT)
   5.  reverse          - int (INPUT)
   6.  case_sensitive   - int (INPUT)
   7.  pad              - int (INPUT)

FUNCTIONS :

   1.   Set up the parms to search DM_FIND_SEARCH_LINES in the appropriate direction.


*************************************************************************/

void  dm_continue_find(FIND_DATA  *find_data,        /* input / output */
                       int        *found_line,       /* output */
                       int        *found_col,        /* output */
                       PAD_DESCR **found_pad,        /* output */
                       char        escape_char)      /* input  */
{
int                      lcl_from_line;
int                      lcl_from_col;
int                      lcl_to_line;
int                      lcl_to_col;
int                      dummy_parm;
int                      all_data_read;
char                     msg[256];
char                     find_dlm;
FIND_PRIVATE            *private = (FIND_PRIVATE *)find_data->private;

DEBUG2(fprintf(stderr, "@dm_continue_find[%d,%d] to [%d,%d] %s)\n",
       private->from_line, private->from_col, private->to_line, private->to_col, 
       (private->reverse ? "Reverse" : "Forward"));)

*found_pad       = private->pad;


if (!private->reverse)
   {

      /***************************************************************
      *  Forward search, search DM_FIND_SEARCH_LINES or to the end
      *  whichever is less.
      ***************************************************************/
      lcl_from_col     = private->from_col;
      lcl_from_line    = private->from_line;

      lcl_to_line = private->from_line + DM_FIND_SEARCH_LINES;
      all_data_read = load_enough_data(lcl_to_line);
      if (all_data_read)
         private->to_line = total_lines(private->pad->token);

      if (lcl_to_line > private->to_line)
         {
            lcl_to_line = private->to_line;
            lcl_to_col  = private->to_col;
         }
      else 
         lcl_to_col  = MAX_LINE+1;

      /***************************************************************
      *  Search up to DM_FIND_SEARCH_LINES lines, then determine
      *  if we found something, failed, or need to continue later.
      ***************************************************************/
      search(private->pad->token,
             lcl_from_line,
             lcl_from_col,
             lcl_to_line,
             &lcl_to_col, /* pass by reference */
             private->reverse,
             NULL,   /* use old find string  */
             NULL,   /* no substitute string */
             !private->case_sensitive,
             found_line,
             found_col,
             &dummy_parm,
             0 /* not rectangular */,
             0 /* not rectangular */,
             0 /* not rectangular */,
             NULL /* not in substitute mode */,
             0 /* not substitute once */,
             escape_char,
             &find_data->search_private);

      private->from_line = lcl_to_line + 1;
      if (*found_line == DM_FIND_NOT_FOUND)
         if (private->from_line  < private->to_line)
            *found_line = DM_FIND_IN_PROGRESS;

   }  /* end of forward find */
else
   {  /* reverse find */

      lcl_to_line    = private->to_line;
      lcl_to_col     = private->to_col;
      lcl_from_line  = private->to_line - DM_FIND_SEARCH_LINES;
      if (lcl_from_line <= private->from_line)
         {
            lcl_from_line = private->from_line;
            lcl_from_col  = private->from_col;
         }
      else
         lcl_from_col   = 0;

      search(private->pad->token,
             lcl_from_line,
             lcl_from_col,
             lcl_to_line,
             &lcl_to_col, /* pass by reference */
             private->reverse,
             NULL,   /* use old find string  */
             NULL,   /* no substitute string */
             !private->case_sensitive,
             found_line,
             found_col,
             &dummy_parm,
             0 /* not rectangular */,
             0 /* not rectangular */,
             0 /* not rectangular */,
             NULL /* not in substitute mode */,
             0 /* not substitute once */,
             escape_char,
             &find_data->search_private);

      private->to_line = lcl_from_line - 1;
      if (*found_line == DM_FIND_NOT_FOUND)
         if (private->to_line  >= private->from_line)
            *found_line = DM_FIND_IN_PROGRESS;

   } /* end of reverse find */

if (*found_line != DM_FIND_IN_PROGRESS)
   {
      if (*found_line == DM_FIND_NOT_FOUND)
         {
            /* 9/13/93 RES changed message to make it look like the one from dm_find */
            if (!private->reverse)
               find_dlm = '/';
            else
               if ((*found_pad)->display_data->escape_char == '@')
                  find_dlm = '\\';
               else
                  find_dlm = '?';
            snprintf(msg, sizeof(msg), "No match :  %c%s%c", find_dlm, private->last_find_pattern, find_dlm);
            dm_error(msg, DM_ERROR_BEEP);
         }
      else
         if (*found_line != DM_FIND_ERROR)
            dm_error("", DM_ERROR_MSG);
         /*else
            message already output */
   }   /* end of find is not still in progress */


DEBUG2(
if (private->reverse)
   fprintf(stderr, "end dm_continue_find[%d,%d] - lcl end [%d,%d]\n", *found_line, *found_col, lcl_from_line, lcl_from_col);
else
   fprintf(stderr, "end dm_continue_find[%d,%d] - lcl end [%d,%d]\n", *found_line, *found_col, lcl_to_line, lcl_to_col);
)

} /* end of dm_continue_find */




/************************************************************************

NAME:      dm_start_sub  -  Initiate a substitute

PURPOSE:    This routine intiates a substitute over a region.  It may do all or part of the
            substitute.  If it does part, the rest is done with dm_continue_sub.

PARAMETERS:

   1.  pad              - pointer to PAD_DESCR (INPUT)
                          This is the pad description for the pad where the sub is being done.

   2.  private          - pointer to FIND_PRIVATE (INPUT)
                          This is the address of the static data used by find and substitute.

   3.  from_line        - int (INPUT)
                          This is the line the substitute starts at.

   4.  from_col         - int (INPUT)
                          This is the column the substitute starts at.

   5.  to_line          - int (INPUT)
                          This is the line the substitute ends at.

   6.  to_col           - int (INPUT)
                          This is the column the substitute ends at.

   7.  pattern          - pointer to char (INPUT)
                          This is the pattern to find.  It is a UNIX regular expression
                          in either aegis or unix format.

   8.  substitute       - pointer to char (INPUT)
                          This is the substitute pattern.  It is a UNIX regular expression
                          in either aegis or unix format.

   9.  do_it_all        - int (INPUT)
                          This flag specifies that the search is to be performed on
                          all lines, not just the first DM_FIND_SEARCH_LINES.
                          In this case DM_FIND_IN_PROGRESS is never returned.
                        
  10.  sub_once         - int (INPUT)
                          This flag indicates whether the substitute should be done repeatedly
                          over the entire range or just once.  This takes care of the s and so
                          commands.
                          0  -  do all the subsitutes.
                          1  -  do just one substitute.

  11. found_line        - pointer to int (OUTPUT)
                          This is the line where the pattern was found or one of the 
                          following special values.
                          DM_FIND_NOT_FOUND    -  The pattern was not found.
                          DM_FIND_IN_PROGRESS  -  The pattern was not found in the first
                                                  DM_FIND_SEARCH_LINES.  The search will
                                                  be continued in background mode.
                          DM_FIND_ERROR        -  Bad regular expression, search not run.

  12. found_col         - pointer to int (OUTPUT)
                          This is the column where the pattern was found.  If found_line is
                          one of the special values, this parm has no meaningfull information.

  13.  rectangular     - int (INPUT)
                         This flag indicates a rectangualar search in a marked region.
                        


GLOBAL VARIABLES:
   The following global variables are set by this routine for use by dm_continue_find.  They are
   global within this module but not visible from outside this module.

   1.  from_line           - int (OUTPUT)
   2.  from_col            - int (OUTPUT)
   3.  to_line             - int (OUTPUT)
   4.  to_col              - int (OUTPUT)
   6  pad                  - pointer to PAD_DESCR (OUTPUT)
   7  substitute           - array of char (OUTPUT)
   8  sub_once             - int (OUTPUT)
   9  found_once           - int (OUTPUT)
   10  last_found_line     - int (OUTPUT)
   11  last_found_col      - int (OUTPUT)
   12  newlines_added      - int (OUTPUT)
   13  cast_in_concrete_from_line - int (OUTPUT)
   14  cast_in_concrete_to_line   - int (OUTPUT)


FUNCTIONS :

   1.   Set up the limit on the number of lines to search

   2.   Set up the top and bottom columns for one set of lines
        or the whole range, which ever is smaller.

   3.   Call search to do the substitute.  Search does the
        substitute only once so it must be called in a loop
        if more than one substitute is needed.


*************************************************************************/

static void  dm_start_sub(PAD_DESCR    *pad,              /* input  */
                          FIND_PRIVATE *private,          /* input / output */
                          int           from_line,        /* input  */
                          int           from_col,         /* input  */
                          int           to_line,          /* input  */
                          int           to_col,           /* input  */
                          char         *pattern,          /* input  */
                          char         *substitute,       /* input  */
                          int           do_it_all,        /* input  */
                          int           sub_once,         /* input  */
                          int          *found_line,       /* output */
                          int          *found_col,        /* output */
                          int           rectangular,      /* input  */
                          char          escape_char,      /* input  */
                          void        **search_private_ptr)/* input / output  */
{
int                      lcl_to_line;
int                      lcl_to_col;
int                      lcl_from_line = from_line;
int                      lcl_from_col  = from_col - 1;  /* for substitute, + 1 is added in */
int                      all_data_read;
/*char                    *lcl_substitute  = (substitute ? substitute : REPEAT_SUBS) RES 9/5/95 */;

private->reverse           = 0;
private->pad               = pad;
/*strcpy(private->substitute, (substitute ? substitute : ""));*/
private->sub_once          = sub_once;
private->found_once        = 0;
private->newlines_added    = 0;
private->rectangular       = rectangular;
private->substitute_changes_count = 0;

DEBUG2(fprintf(stderr, "@start_sub[%d,%d] to [%d,%d] \"%s\",\"%s\"\n",
       from_line, from_col, to_line, to_col, 
       (pattern ? pattern : ""), (substitute ? ((substitute != REPEAT_SUBS) ? substitute : "REPEAT_SUBSTITUTE") : ""));
)

/***************************************************************
*  
*  If do_it_all was specified, set the to line to be the real end.
*  
***************************************************************/

if (do_it_all)
   {
      lcl_to_line = to_line;
      load_enough_data(lcl_to_line); /* Changed from INT_MAX 9/5/95 */
   }
else
   {
      lcl_to_line = from_line + DM_FIND_SEARCH_LINES;
      all_data_read = load_enough_data(lcl_to_line);
      if (all_data_read && (lcl_to_line > total_lines(pad->token)))
         lcl_to_line = to_line;
    }

/***************************************************************
*  
*  For a non-rectangular substitute,
*  from_col is set to zero because we will either finish
*  the substitute here, in which case the var is irrelevant, or
*  we will continue the search later in which case the start col
*  is always zero.
*
*  For a rectangular substitute, we need to keep track of
*  the original start col since all searches start there.
*  
***************************************************************/

if (rectangular)
   private->from_col = from_col;
else
   private->from_col = -1;

private->to_line = to_line;
if (lcl_to_line >= to_line)
   {
      lcl_to_line = to_line;
      lcl_to_col  = to_col;
   }
else 
   if (rectangular)
      lcl_to_col  = to_col;
   else
      lcl_to_col  = MAX_LINE+1;
private->to_col = to_col;

private->cast_in_concrete_from_line = from_line;
private->cast_in_concrete_to_line   = private->to_line;

/***************************************************************
*  Search up to DM_FIND_SEARCH_LINES lines, then determine
*  if we found something, failed, or need to continue later.
*  Substitute substitutes once.  We will continue until we 
*  get a not found which will occur.
*  In substitute mode, search returns the character just past
*  the end of the substitution - the new start pos.
***************************************************************/
do
{
   search(private->pad->token,
          lcl_from_line,
          lcl_from_col,
          lcl_to_line,
          &lcl_to_col, /* pass by reference */
          0,
          pattern,
          substitute,
          0, /* substitutes are always case sensitive */
          found_line,
          found_col,
          &private->newlines_added,
          private->rectangular,
          private->from_col,
          private->to_col,
          &private->substitute_changes_count,
          private->sub_once,
          escape_char,
          search_private_ptr);

   pattern = NULL; /* after first call, reuse same compiled pattern */
   substitute = REPEAT_SUBS; /*  optimization 4/12/93 */

   if (*found_line == DM_FIND_ERROR)
      break;

   if (*found_line != DM_FIND_NOT_FOUND)
      {
         private->found_once = 1;
         private->last_found_line = *found_line;
         private->last_found_col  = *found_col;
      }

   /* added conditional 6/13/95, originally was just the else case  */
   if (private->sub_once)
      {
         lcl_from_line = *found_line + 1;
         lcl_from_col  = -1;
      }
   else
      {
         lcl_from_line = *found_line;
         lcl_from_col  = *found_col;
      }

} while(*found_line != DM_FIND_NOT_FOUND);


/***************************************************************
*  
*  First bump the to line to see where we are.  
*  If we found something and are done, set the found line.
*  If we added newlines, do the postprocessing on them and add
*  the added lines to the line number of the last found file.
*  The column number will be trashed out at this point.
*  
***************************************************************/

private->from_line = lcl_to_line + 1;
if (*found_line == DM_FIND_NOT_FOUND)
   if (private->from_line  < private->to_line)
      *found_line = DM_FIND_IN_PROGRESS;
   else
      if (private->found_once)
         {
            *found_line = private->last_found_line;
            *found_col  = private->last_found_col;
         }

DEBUG2(fprintf(stderr, "start_sub, last found pos [%d,%d]\n", *found_line, *found_col);)

if (private->newlines_added && (*found_line != DM_FIND_IN_PROGRESS))
   process_inserted_newlines(private->pad->token, private->cast_in_concrete_from_line, private->cast_in_concrete_to_line, found_line, found_col);


DEBUG2(fprintf(stderr, "end start_sub[%d,%d] - lcl end [%d,%d]\n", *found_line, *found_col, lcl_to_line, lcl_to_col);)

} /* end of dm_start_sub */


/************************************************************************

NAME:      dm_continue_sub  -  Continue a substitute

PURPOSE:    This routine continues a find started by dm_start_find.  It is
            called as part of background processing.

PARAMETERS:
   1.  found_line       - pointer to int (OUTPUT)
                          This is the line where the pattern was found or one of the 
                          following special values.
                          DM_FIND_NOT_FOUND    -  The pattern was not found.
                          DM_FIND_IN_PROGRESS  -  The pattern was not found in the first
                                                  DM_FIND_SEARCH_LINES.  The search will
                                                  be continued in background mode.

   2.  found_col        - pointer to int (OUTPUT)
                          This is the column where the pattern was found.  If found_line is
                          one of the special values, this parm has no meaningfull information.

   3.  found_pad        - pointer to PAD_DESCR (OUTPUT)
                          This is the pad where the sub was being done.


GLOBAL VARIABLES:
   The following global variables are set by this routine for use by dm_continue_find.  They are
   global within this module but not visible from outside this module.

   1.  from_line           - int (INPUT / OUTPUT)
   2.  from_col            - int (INPUT)
   3.  to_line             - int (INPUT)
   4.  to_col              - int (INPUT)
   5   pad                 - pointer to PAD_DESCR (INPUT)
   6   substitute          - array of char (INPUT)
   7   sub_once            - int (INPUT)
   8   found_once          - int (INPUT / OUTPUT)
   9   last_found_line     - int (INPUT / OUTPUT)
   10  last_found_col      - int (INPUT / OUTPUT)
   11  newlines_added      - int (INPUT)
   12  cast_in_concrete_from_line - int (INPUT)
   13  cast_in_concrete_to_line   - int (INPUT)


FUNCTIONS :

   1.   Set up the parms to search and call it.




*************************************************************************/

void  dm_continue_sub(FIND_DATA  *find_data,        /* input / output */
                      int        *found_line,       /* output */
                      int        *found_col,        /* output */
                      PAD_DESCR **found_pad,        /* output */
                      char        escape_char)      /* input  */
{
int                      lcl_from_line;
int                      lcl_from_col;
int                      lcl_to_line;
int                      lcl_to_col;
char                     msg[256];
FIND_PRIVATE            *private = (FIND_PRIVATE *)find_data->private;

DEBUG2(fprintf(stderr, "@dm_continue_sub[%d,%d] to [%d,%d])\n",
       private->from_line, private->from_col, private->to_line, private->to_col);)

/***************************************************************
*  Forward search, search DM_FIND_SEARCH_LINES or to the end
*  whichever is less.
***************************************************************/
lcl_from_col     = private->from_col - 1;   /* -1 for substitute, + 1 is added in */
lcl_from_line    = private->from_line;
*found_pad       = private->pad;

lcl_to_line = private->from_line + DM_FIND_SEARCH_LINES;
if (lcl_to_line >= private->to_line)
   {
      lcl_to_line = private->to_line;
      lcl_to_col  = private->to_col;
   }
else 
   if (private->rectangular)
      lcl_to_col  = private->to_col;
   else
      lcl_to_col  = MAX_LINE+1;

load_enough_data(lcl_to_line);


/***************************************************************
*  Search up to DM_FIND_SEARCH_LINES lines, then determine
*  if we found something, failed, or need to continue later.
***************************************************************/
do
{
   search(private->pad->token,
          lcl_from_line,
          lcl_from_col,
          lcl_to_line,
          &lcl_to_col, /* pass by reference */
          0,
          NULL,   /* reuse old pattern */
          REPEAT_SUBS,
          0, /* substitutes are always case sensitive,*/
          found_line,
          found_col,
          &private->newlines_added,
          private->rectangular,
          private->from_col,
          private->to_col,
          &private->substitute_changes_count,
          private->sub_once,
          escape_char,
          &find_data->search_private);

   if (*found_line == DM_FIND_ERROR)
      break;

   if (*found_line != DM_FIND_NOT_FOUND)
      {
         private->found_once = 1;
         private->last_found_line = *found_line;
         private->last_found_col  = *found_col;
      }

   /* added conditional 6/13/95, originally was just the else case  */
   if (private->sub_once)
      {
         lcl_from_line = *found_line + 1;
         lcl_from_col  = -1;
      }
   else
      {
         lcl_from_line = *found_line;
         lcl_from_col  = *found_col;
      }

} while(*found_line != DM_FIND_NOT_FOUND);


/***************************************************************
*  
*  First bump the to line to see where we are.  
*  If we found something and are done, set the found line.
*  If we added newlines, do the postprocessing on them and add
*  the added lines to the line number of the last found file.
*  The column number will be trashed out at this point.
*  
***************************************************************/

private->from_line = lcl_to_line + 1;
if (*found_line == DM_FIND_NOT_FOUND)
   if (private->from_line  < private->to_line)
      *found_line = DM_FIND_IN_PROGRESS;
   else
      if (private->found_once)
         {
            *found_line = private->last_found_line;
            *found_col  = private->last_found_col;
         }

DEBUG2(fprintf(stderr, "continue_sub, last found pos [%d,%d]\n", *found_line, *found_col);)

if (private->newlines_added && (*found_line != DM_FIND_IN_PROGRESS))
   process_inserted_newlines(private->pad->token, private->cast_in_concrete_from_line, private->cast_in_concrete_to_line, found_line, found_col);

if (*found_line != DM_FIND_IN_PROGRESS)
   {
      if (*found_line == DM_FIND_NOT_FOUND)
         {
            /* 9/13/93 RES, add strings to error message to make it look like the one in dm_s */
            snprintf(msg, sizeof(msg), "No match :  /%s/%s/", private->last_find_pattern, private->last_sub_pattern);
            dm_error(msg, DM_ERROR_BEEP);
         }
      else
         if (*found_line != DM_FIND_ERROR)
            {
               snprintf(msg, sizeof(msg), "%d substitution%s made.", private->substitute_changes_count,((private->substitute_changes_count > 1) ? "s" : ""));
               dm_error(msg, DM_ERROR_MSG);
            }
         /*else
            message already output */
   }   /* end of find is not still in progress */


DEBUG2(fprintf(stderr, "end continue_sub[%d,%d] - lcl end [%d,%d]\n", *found_line, *found_col, lcl_to_line, lcl_to_col);)

} /* end of dm_continue_sub */


/***********************************************************************
*
*  dm_re - translate a RE from aegis to UNIX
*     
***********************************************************************/

void dm_re(DMC  *dmc,
           char  escape_char)
{

char *ptr;
char line[MAX_LINE+1];
char buf[MAX_LINE+1];

if (!dmc->re.parm)
   {
      if (escape_char == '@')
         dm_error("Using Aegis regular expression mode", DM_ERROR_MSG);
      else
         dm_error("Using Unix regular expression mode", DM_ERROR_MSG);
      return;
   }

strcpy(line, dmc->re.parm);
a_to_re(line, 0);

ptr = line;

while (ptr = strpbrk(ptr, "\b\t\n\f")){
      switch(*ptr){
       case '\t' :
                   ins("\t", line, ptr);
                   break;

       case '\n' :
                   ins("\n", line, ptr);
                   break;

       case '\b' :
                   ins("\b", line, ptr);
                   break;

       case '\f' :
                   ins("\f", line, ptr);
                   break;
     }
     ptr++;
}

snprintf(buf, sizeof(buf), "RE: '%s'", line);

dm_error(buf, DM_ERROR_MSG);

}  /* dm_re() */


/***********************************************************************
*
*  ins - insert into a line a string removing the character under the ptr
*     
***********************************************************************/

static void ins(char *string, char *line, char *ptr)
{

char buf[MAX_LINE+1];
int  len;

len = ptr - line;
if (len > MAX_LINE)
   len = MAX_LINE;

strncpy(buf, line, len);
*(buf + (ptr - line)) = '\0';

strlcat(buf, string, sizeof(buf));
strlcat(buf, ptr +1, sizeof(buf));

strcpy(line, buf);

}

/************************************************************************

NAME:      max_line_len  - Calculate the maximum length of a group of lines

PURPOSE:    This routine runs through a line range in a memdata database
            and gets the length of the longest line.  This value is returned.

PARAMETERS:
   1.  token            - pointer to DATA_TOKEN (INPUT)
                          This is the token for the memdata file being processed.

   2.  top_line         - int (INPUT)
                          This is the first line to search.

   3.  bottom_line      - int (INPUT)
                          This is the last line to search.


FUNCTIONS :

   1.   Position for reading the first line.

   2.   Read each line and take the max of the lengths.

RETURNED VALUE:
   max_len     - int
                 This is the length of the longest line in the passed range.


*************************************************************************/

static int max_line_len(DATA_TOKEN   *token,
                        int           top_line,
                        int           bottom_line)
{
int      max_len = 0;
char    *line;

position_file_pointer(token, top_line);
while (((line = next_line(token)) != NULL) && (top_line <= bottom_line))
{
   max_len = MAX(max_len, (int)strlen(line));
   top_line++;
}

return(max_len);

} /* end of max_line_len */


/************************************************************************

NAME:      find_border_adjust - Adjust first line to set border around the find.

PURPOSE:    This routine shifts the window up or down to keep the found
            string from being the first or last line on the window.  It is
            called if there is a non-zero find border and the string was found.
            This routine is called after the call to dm_position on the target.

PARAMETERS:
   1.  main_pad         - pointer to PAD_DESCR (INPUT)
                          This is the token for the memdata file being processed.

   2.  border           - int (INPUT)
                          This is the number of lines between the found line
                          and the edge of the window.

   3.  bottom_line      - int (INPUT)
                          This is the last line to search.


FUNCTIONS :

   1.   Position for reading the first line.

   2.   Read each line and take the max of the lengths.

RETURNED VALUE:
   shifted -  int
              This flag is returned true if the window was shifted.
              We should call dm_position again.

*************************************************************************/

int find_border_adjust(PAD_DESCR   *main_pad,
                       int          border)
{
int      shifted = False;
int      orig_first_line;
int      delta;
int      orig_last_line;

/***************************************************************
*  If the window is smaller than twice the border, cut
*  the border to half the window size.
***************************************************************/
DEBUG2(fprintf(stderr, "find_border_adjust: border =  %d lines\n", border);)
if (main_pad->window->lines_on_screen < (border *2))
   {
      border = main_pad->window->lines_on_screen / 2;
      DEBUG2(fprintf(stderr, "find_border_adjust: border cut to %d lines\n", border);)
   }

if (border == 0)
   return(False);

/***************************************************************
*  If we are within border lines of the top of the window and
*  we are not at the top of the file, adjust the window.
***************************************************************/
if ((main_pad->file_line_no <= main_pad->first_line + border) &&
    (main_pad->first_line > 0))
   {
      orig_first_line =  main_pad->first_line;
      delta = border - (main_pad->file_line_no - main_pad->first_line);
      main_pad->first_line -= delta;
      if (main_pad->first_line < 0)
         main_pad->first_line = 0;
      ff_scan(main_pad, main_pad->first_line);
      if ((main_pad->formfeed_in_window > 0) &&
         (main_pad->formfeed_in_window <= main_pad->file_line_no-main_pad->first_line))
         {
            /* pushed line past a form feed */
            main_pad->first_line = orig_first_line;
            ff_scan(main_pad, main_pad->first_line);
         }
      else
         shifted = True;
   }
else
   {
      /***************************************************************
      *  If we are within border lines of the bottom of the window,
      *  adjust the window.
      ***************************************************************/
      DEBUG0(orig_first_line =  main_pad->first_line;)
      orig_last_line = main_pad->first_line + (main_pad->window->lines_on_screen-1);
      if (main_pad->file_line_no > (orig_last_line - border))
         {
            delta = border - (orig_last_line - main_pad->file_line_no);
            main_pad->first_line += delta;
            shifted = True;
         }
   }

DEBUG2(fprintf(stderr, "find_border_adjust: first line adjusted from %d to %d\n", orig_first_line, main_pad->first_line);)
return(shifted);

} /* end of find_border_adjust */




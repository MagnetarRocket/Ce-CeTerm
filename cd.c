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
*  module cd.c  - color data graphics content
*
*  Routines:
*     get_drawing_linestart   - Find the file lineno to display lines
#ifdef EXCLUDEP
*     cd_count_xed_lines      - Count x'ed lines in agroup.
*     lines_xed               - Calculate the number of x'ed file lines for a passed line
*     win_line_to_file_line   - translate a window line_no to a file line_no
*     file_line_to_win_line   - translate a file line_no to a window line_no
#endif
*     cd_add_remove           - Update cd data in memdata to add or delete chars
*     cdpad_add_remove        - Update pad cd data to add or delete chars
*     cdpad_curr_cdgc         - Get the current cdgc list for drawing
*     cd_join_line            - Update work cd data to accomadate a join
*     pulldown_overflow_color - Preserve color when a line is going away.
*     cd_flush                - Flush color data back to the memdata database
*
* Internal:
*     cdgc_add_del            - Update the cdgc linked list to add or delete chars
*
*     The three routines get_drawing_linestart, win_line_to_file_line, and file_line_to_win_line
*     solve for first_line, file_line_no, and win_line_no respectively given the
*     other two.  They do not use winlines which may not be set up when called.
*
*     The two add_remove are used based on what is available to the calling routine.
*     The cd_add_remove routine reads data from the memdata database and updates it.
*     Any command using this routines must have the flush bit set to True
*     so that nothing is left in the pad's work area.
*     The cd_add_remove routine set up the work cdgc buffer
*     in the pad description.  This is done for efficiency when doing typing.
*     cd_flush (called from regular flush) is used to write the data back to
*     memdata.
*     
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <errno.h>          /* /usr/include/errno.h      */
#include <string.h>         /* /usr/include/string.h     */

#ifdef WIN32
#ifdef WIN32_XNT
#include "xnt.h"
#else
#include "X11/X.h"          /* /usr/include/X11/X.h      */
#include "X11/Xlib.h"       /* /usr/include/X11/Xlib.h   */
#include "X11/Xutil.h"      /* /usr/include/X11/Xutil.h  */
#endif
#else
#include <X11/X.h>          /* /usr/include/X11/X.h      */
#include <X11/Xlib.h>       /* /usr/include/X11/Xlib.h   */
#include <X11/Xutil.h>      /* /usr/include/X11/Xutil.h  */
#endif

#include "debug.h"
#include "cd.h"
#include "cdgc.h"
#include "dmwin.h"
#include "emalloc.h"
#include "xerror.h"
#include "xerrorpos.h"
#include "xutil.h"

/***************************************************************
*  
*  Static data hung off the display description.
*
*  The following structure is hung off the display description
*  it is needed to keep track of the graphic contents (GC's)
*  allocated.  It is a variable size structure.  This is to
*  allow it to be freed during cleanup.  It's size may change.
*  
***************************************************************/

#define DEFAULT_COLOR_ENTRIES 12
#define INCR_COLOR_ENTRIES     5
#define MAX_COLOR_NAME        24

typedef struct {
     char              background[MAX_COLOR_NAME];
     char              foreground[MAX_COLOR_NAME];
     GC                gc;
} COLOR_DATA;

typedef struct {
     int               max_color_entries;
     int               cur_color_entries;
     COLOR_DATA        color[DEFAULT_COLOR_ENTRIES];
} CDGC_PRIVATE;

typedef struct {
     int               cur_line_no;
     int               color_read;
     FANCY_LINE       *cdgc_list;
} CD_ADDEL_PRIVATE;

typedef struct {
     /* elements used in modification (cdpad_add_remove) */
     int               cur_line_no;            /* color line being modified in pad   */
     int               color_read;             /* line has been read (could be null) */
     FANCY_LINE       *cdgc_list;              /* cdgc in linked list form           */

     /* elements used in typing (cd_curr_data) */
    int                 cdgc_lineno;           /* Line number for curr_cdgc (could overflow) */
    int                 lower_lineno;          /* Lower limit for this cdgc range            */
    short int           curr_cdgc_read;        /* True when curr_cdgc has been set up, NULL is valid for curr_cdgc */
    FANCY_LINE         *curr_cdgc;             /* Working cdgc buffer                 */
} CD_PAD_PRIVATE;


/***************************************************************
*  
*  Prototypes for local routines
*  
***************************************************************/

static int   cdgc_add_del(FANCY_LINE      *cdgc_list,   /* input/output */
                          int              col,         /* input */
                          int              chars);      /* input */


#ifdef EXCLUDEP
/************************************************************************

NAME:      get_drawing_linestart - Find the file lineno to display lines

PURPOSE:    This routine takes a file line number and a number of lines
            and determines the file line number needed to display that
            number of lines.  This would be a simple subtraction if it
            weren't for excluded lines.

PARAMETERS:
   1.  token           - pointer opaque (INPUT)
                         This is the memdata token to be examined.

   3.  last_line_no    - int (INPUT)
                         This is the last file line number we want included
                         in the group of displayable lines.  If we wanted
                         one line and of drawing data and this would be the 
                         first line assuming no excludes, we would pass the
                         number and a 1.  This would usually be the expected
                         value for pad->first_line.

   3.  count             int (INPUT)
                         This is the number of drawable lines we want.  A
                         Group of x'ed out lines counts as one line.

FUNCTIONS :
   1.   Starting with last_line_no, get the color data.  If there is none or
        if the color data is more that "count" lines above and is not showing
        that this is an x'ed out line, return last_line_no-(count-1).

   2.   A set of excluded lines counts as one line.  Walk backwards by
        cdgc lines till we have count excluded lines.

   3.   Check the line before the first x'ed out line to see if it is x'ed out
        too.  If so repeat steps 2 and 3 until we find something that is not
        x'ed out.  Stop if we get to file line zero.

   4.   Walk forward looking at color data till we get the next non-x'ed out
        line.

RETURNED VALUE:
   file_lineno  - int
                  This is the first line number to read in order to get "count"
                  drawable lines ending with "last_line_no" as the last displayed
                  line.  If the top of the file is hit and there are not enough
                  displayable lines, -1 is returned.
    
*************************************************************************/

int get_drawing_linestart(DATA_TOKEN      *token,             /* input  */
                          int              last_line_no,      /* input  */
                          int              count)             /* input  */
{
char                 *work_cdgc;
int                   cdgc_line;
int                   display_lines;

work_cdgc = get_color_by_num(token, last_line_no, COLOR_UP, &cdgc_line);
if (work_cdgc == NULL)
   return(last_line_no-(count-1));

while(count)
{
   if (line_is_xed(work_cdgc, cdgc_line, last_line_no))
      {
         count--;
         last_line_no = cdgc_line - 1;
      }
   else
      if (last_line_no-cdgc_line >= (count-1))
         return(last_line_no-(count-1));
      else
         {
            if (cdgc_line == 0)
               return(-1);
            count -= (last_line_no-(cdgc_line-1));
            last_line_no = cdgc_line - 1;
         }

   work_cdgc = get_color_by_num(token, last_line_no, COLOR_UP, &cdgc_line);
   if (work_cdgc == NULL)
      if (last_line_no < (count-1))
         return(-1);
} /* end of while */

return(last_line_no);

} /* end of get_drawing_linestart */


/************************************************************************

NAME:      cd_count_xed_lines - Count x'ed lines in a group.

PURPOSE:    This routine calculates the first line in an x'ed out group
            along with the count of lines.

PARAMETERS:
   1.  cdgc            - pointer to char (INPUT)
                         This is the cdgc text discovered to be
                         in x'ed out mode. (Data returned by get_color_by_num).

   2.  cdgc_line       - int (INPUT)
                         This is the file line number for this cdgc.  That
                         is:  The line number returned from get_color_by_number.

   3.  line_no           int (INPUT)
                         This is the current file line number.  That is:
                         the line we were working on when we discovered
                         The x'ed out lines.

   4.  first_line_no     int (OUTPUT)
                         This is the file line number of first line in the
                         x'ed out grouop.

FUNCTIONS :
   1.   Make sure this is an x'ed out line.  The caller may be mistaken.

   2.   The count between line_no and cdgc_line is always x'ed out.  Include this
        in the count.

   3.   Check the line before the first x'ed out line to see if it is x'ed out
        too.  If so repeat steps 2 and 3 until we find something that is not
        x'ed out.  Stop if we get to file line zero.

   4.   Walk forward looking at color data till we get the next non-x'ed out
        line.

RETURNED VALUE:
   count        - int
                  The count of x'ed out lines is returned.

    
*************************************************************************/

int cd_count_xed_lines(DATA_TOKEN      *token,             /* input  */
                       char            *cdgc,              /* input  */
                       int              cdgc_line,         /* input  */
                       int              line_no,           /* input  */
                       int             *first_line_no)     /* output */
{
int                   count;
GC                    gc;
char                 *work_cdgc;
int                   tmp_first_line_no;
int                   done;
void                 *dummy_parm = NULL;
int                   i;

*first_line_no = line_no;
if (!line_is_xed(cdgc, cdgc_line, line_no))
   return(0);

*first_line_no = cdgc_line;
count = (line_no - cdgc_line) + 1;

/***************************************************************
*  Go upwards through the file looking for a non-x'ed out line.
***************************************************************/
done = False;
while (!done)
{
   work_cdgc = get_color_by_num(token, (*first_line_no)-1, COLOR_UP, &tmp_first_line_no);
   if (line_is_xed(work_cdgc, tmp_first_line_no, (*first_line_no)-1))
      {
         *first_line_no = tmp_first_line_no;
         count = line_no - tmp_first_line_no;
         if (tmp_first_line_no == 0)
            done = True;
      }
   else
      done = True;
}

/***************************************************************
*  Down through the file looking for a non-x'ed out line.
***************************************************************/

for (i = line_no+1; i < total_lines(token); i++)
{
   work_cdgc = get_color_by_num(token, i, COLOR_UP, &tmp_first_line_no);
   if (line_is_xed(work_cdgc, tmp_first_line_no, i))
      count++;
   else
      break;
}
 
return(count);

} /* end of cd_count_xed_lines */


/************************************************************************

NAME:      lines_xed   - Calculate the number of x'ed file lines for a passed line

PURPOSE:    This routine is a shortcut used by cursor movement routines to detect
            window lines with multiple file lines under them.

PARAMETERS:
   1.  token           - pointer opaque (INPUT)
                         This is the memdata token to be examined.

   2.  file_line_no    - int (INPUT)
                         This is the file line number be tested.
                         If it is in a set of x'ed lines, the number of lines
                         between it and the next displayable line is returned.

FUNCTIONS :
   1.   Get the color data for file_line_no.  If it is null or
        the cdgc line is less than first_line and the lines are not excluded,
        return 1

   2.   Count the number of x'ed out lines and return it
        allowing for the passed file number

RETURNED VALUE:
   count        - int
                  This is the number of lines (including the one passed) to
                  get past an excluded group of lines.
    
*************************************************************************/

int lines_xed(DATA_TOKEN      *token,             /* input  */
              int              file_line_no)      /* input  */
{
char                 *work_cdgc;
int                   cdgc_line;
int                   count;
int                   xed_first_line;
int                   xed_count;

work_cdgc = get_color_by_num(token, file_line_no, COLOR_UP, &cdgc_line);
if (!line_is_xed(work_cdgc, cdgc_line, file_line_no))
   return(1);

xed_count = cd_count_xed_lines(token, work_cdgc, cdgc_line, file_line_no, &xed_first_line);
count = (xed_count - (file_line_no - xed_first_line));
return(count);

} /* end of lines_xed */
#endif


#ifdef EXCLUDEP
/************************************************************************

NAME:      win_line_to_file_line   - translate a window line_no to a file line_no

PURPOSE:    This routine takes a window line number and figures out what file
            number it will be based on the first line number displayed and any excludes.

PARAMETERS:
   1.  token           - pointer opaque (INPUT)
                         This is the memdata token to be examined.

   3.  first_line      - int (INPUT)
                         This is the first line displayed.  If it is within
                         an x'ed out group it does not matter.  It counts as
                         one line.  Pass pad->first_line.

   3.  win_line_no     - int (INPUT)
                         This is the window line number to find.
                         This is a zero based number.  The first line on
                         the window is line zero.

FUNCTIONS :
   1.   Get the color data for first_line + win_line_no.  If it is null or
        the cdgc line is less than first_line and the lines are not excluded,
        return first_line + win_line_no.

   2.   Starting with the first line, walk down to the window line number 
        looking for excluded lines.  A group of excluded lines counts as
        one line.

RETURNED VALUE:
   file_lineno  - int
                  This is the line number of the file corresponding to the passed
                  window line number assuming first_line is the first line drawn.
    
*************************************************************************/

int win_line_to_file_line(DATA_TOKEN      *token,             /* input  */
                          int              first_line,        /* input  */
                          int              win_line_no)       /* input  */
{
char                 *work_cdgc;
int                   cdgc_line;
int                   display_lines;
int                   file_line;
int                   xed_first_line;
int                   xed_count;

work_cdgc = get_color_by_num(token, first_line+win_line_no, COLOR_UP, &cdgc_line);
if ((work_cdgc == NULL) || ((cdgc_line <= first_line) && !line_is_xed(work_cdgc, cdgc_line, first_line+win_line_no)))
   return(first_line + win_line_no);

file_line = first_line;

/***************************************************************
*  If we are dealing with window line zero, the actual file line
*  may be less than file line if the line passed is in the middle
*  of x'ed out set.  This should not happen, but if it does, we
*  can deal with it.
***************************************************************/
if ((win_line_no == 0) && (line_is_xed(work_cdgc, cdgc_line, file_line)))
   file_line = cdgc_line;

/***************************************************************
*  Walk down the window counting regular lines and excluded 
*  lines till we get to win_line_no.
***************************************************************/
while(win_line_no > 0)
{
   work_cdgc = get_color_by_num(token, file_line, COLOR_UP, &cdgc_line);
   if (line_is_xed(work_cdgc, cdgc_line, file_line))
      {
         xed_count = cd_count_xed_lines(token, work_cdgc, cdgc_line, file_line, &xed_first_line);
         file_line += (xed_count - (file_line-xed_first_line));
      }
   else
      file_line++;

   win_line_no--;
}

return(file_line);

} /* end of win_line_to_file_line */


/************************************************************************

NAME:      file_line_to_win_line   - translate a file line_no to a window line_no

PURPOSE:    This routine takes a window line number and figures out what file
            number it will be based on the first line number displayed and any excludes.

PARAMETERS:
   1.  token           - pointer opaque (INPUT)
                         This is the memdata token to be examined.

   3.  first_line      - int (INPUT)
                         This is the first line displayed.  If it is within
                         an x'ed out group it does not matter.  It counts as
                         one line.  Pass pad->first_line.

   3.  file_line_no    - int (INPUT)
                         This is the file line number to find on the window.
                         If it is in a set of x'ed lines, the window line number
                         of the x'ed lines will be used.

FUNCTIONS :
   1.   Get the color data for file_line_no.  If it is null or
        the cdgc line is less than first_line and the lines are not excluded,
        return file_line_no - first_line

   2.   Starting with the first line, walk down to the file line number 
        looking for excluded lines.  A group of excluded lines counts as
        one line.

RETURNED VALUE:
   win_lineno   - int
                  This is the zero based window line number where the passed file
                  line will be drawn assuming first_line is the first line drawn.
    
*************************************************************************/

int file_line_to_win_line(DATA_TOKEN      *token,             /* input  */
                          int              first_line,        /* input  */
                          int              file_line_no)      /* input  */
{
char                 *work_cdgc;
int                   cdgc_line;
int                   cur_file_line;
int                   win_line;
int                   xed_first_line;
int                   xed_count;

work_cdgc = get_color_by_num(token, file_line_no, COLOR_UP, &cdgc_line);
if ((work_cdgc == NULL) || ((cdgc_line <= first_line) && !line_is_xed(work_cdgc, cdgc_line, file_line_no)))
   return(file_line_no - first_line);

win_line = 0;
cur_file_line = first_line;

/***************************************************************
*  Walk down the window counting regular lines and excluded 
*  lines till we get to win_line_no.
***************************************************************/
while(cur_file_line < file_line_no)
{
   work_cdgc = get_color_by_num(token, cur_file_line, COLOR_UP, &cdgc_line);
   if (line_is_xed(work_cdgc, cdgc_line, cur_file_line))
      {
         xed_count = cd_count_xed_lines(token, work_cdgc, cdgc_line, cur_file_line, &xed_first_line);
         cur_file_line += (xed_count - (cur_file_line - xed_first_line));
         if (cur_file_line >= file_line_no)  /* file line was embedded in an x'ed out area, don't bump winline */
            break;
      }
   else
      cur_file_line++;

   win_line++;
}

return(win_line);

} /* end of file_line_to_win_line */
#endif


/************************************************************************

NAME:      cd_add_remove - Update cd data in memdata to add or delete chars

PURPOSE:    This routine will arrange color data on a line to compensate for
            adding or deleting text characters in the requested line.
            If you add or delete a character, you want
            color data to the right of where you are to shift appropriately.
            This routine is called when characters are being added or deleted
            and the pad description is not available (search/subsitute).
            The flush bit must be on on any command which uses this routine.

PARAMETERS:
   1.  token           - DATA_TOKEN (INPUT)
                         This is the token for the memdata line being

   2.  curr_line_data  - pointer to pointer to void (INPUT/OUTPUT)
                         The address of a pointer to void is passed
                         to this routine.  This routine puts the
                         address of a private data area in the pointer.
                         This data is kept around between calls.

   3.  lineno          - int (INPUT)
                         This is the zero based file line number.

   4.  col             - int (INPUT)
                         This is the zero based starting column to do the
                         add or delete at.

   5.  chars           - int (INPUT)
                         This is the number of chars being added or deleted.
                         Negative numbers are used for delete.  That is
                         to delete 1 character, pass -1.


FUNCTIONS :
   1.   Get the color data for the passed line and convert it to linkes
        list format.

   2.   Modify the data to show the characters added/removed.

   3.   Put the data back in string format and write it out to 
        the database.


*************************************************************************/

void  cd_add_remove(DATA_TOKEN      *token,          /* input/output */
                    void           **curr_line_data, /* input/output */
                    int              lineno,         /* input */
                    int              col,            /* input */
                    int              chars)          /* input */
{
int                   cdgc_line;
char                 *cdgc;
char                  buff[MAX_LINE+1];
CD_ADDEL_PRIVATE     *private = (CD_ADDEL_PRIVATE *)*curr_line_data;

/***************************************************************
*  If we don't have a private area, get one.
***************************************************************/
DEBUG13( fprintf(stderr, " cd_add_remove(token, %0x, line=%d, col=%d, char=%d\n", *curr_line_data, lineno, col, chars);) 
if (!private)
   {
     /* I believe this one is a memory leak */
      private = (CD_ADDEL_PRIVATE *)CE_MALLOC(sizeof(CD_ADDEL_PRIVATE));
      if (!private)
         return;
      memset((char *)private, 0, sizeof(CD_ADDEL_PRIVATE));
      *curr_line_data = (void *)private;
   }

/***************************************************************
*  If have not read a line (first pass) or have changed lines,
*  flush the old data back to the memdata database and get
*  the data for the new line.
***************************************************************/
if ((lineno != private->cur_line_no) || (!private->color_read))
   {
      if (private->cdgc_list != NULL)
         {
            flatten_cdgc(private->cdgc_list, buff, sizeof(buff), False /* do them all */);
            put_color_by_num(token, private->cur_line_no, buff);
            free_cdgc(private->cdgc_list);
            private->cdgc_list = NULL;
         }

      if (lineno >= 0)  /* lineno = -1 for flush */
         {
            cdgc = get_color_by_num(token, lineno, COLOR_CURRENT, &cdgc_line);
            private->color_read = True;
            DEBUG13(if (cdgc) fprintf(stderr, "old: \"%s\"\n", cdgc);)
         }
      else
         {
            cdgc = NULL;
            private->color_read = False;
         }

      private->cur_line_no = lineno;

      /***************************************************************
      *  Expand the cdgc to linked list format.  We do not care about
      *  the GC's since we do not need them to flatten back to char format.
      *  This list will not be used for drawing.
      ***************************************************************/
      if (cdgc != NULL)
         private->cdgc_list = expand_cdgc(NULL, None, cdgc, None, NULL);
      else
         private->cdgc_list = NULL;
   }

if ((private->cdgc_list != NULL) && (chars != 0))
   cdgc_add_del(private->cdgc_list, col, chars);

DEBUG13(
if (private->cdgc_list != NULL)
   {
      flatten_cdgc(private->cdgc_list, buff, sizeof(buff), False /* do them all */);
      fprintf(stderr, "new: \"%s\"\n", buff);
   }
)


}  /* end of cd_add_remove */


/************************************************************************

NAME:      cdpad_add_remove  - Update pad cd data to add or delete chars

PURPOSE:    This routine will arrange color data on a line to compensate for
            adding or deleting text characters in the requested line.
            If you add or delete a character, you want
            color data to the right of where you are to shift appropriately.
            This routine is similar to cd_add_remove except that
            requested characters.  It is like cd_add_remove except that
            it works with the pads working area and depends on cd_flush to
            write things back to the memdata area.  This delayed write is
            more effcient for things like typing.

PARAMETERS:
   1.  pad             - pointer to PAD_DESCR (INPUT)
                         This is the up to snuff pad we are operating on.

   2.  col             - int (INPUT)
                         This is the zero based starting column to do the
                         add or delete at.

   3.  chars           - int (INPUT)
                         This is the number of chars being added or deleted.
                         Negative numbers are used for delete.  That is
                         to delete 1 character, pass -1.

FUNCTIONS :
   1.   If the cd_data has not been read in, read it in for this line.
        The line is defined in the pad.

   2.   Modify the data.

NOTE:
   This routine is separate from cd_add_remove because it passes
   in enough data to create cdgc lists suitable for doing drawing.

*************************************************************************/

void  cdpad_add_remove(PAD_DESCR      *pad,         /* input/output */
                       int             lineno,      /* input */
                       int             col,         /* input */
                       int             chars)       /* input */
{
int                   cdgc_line;
char                 *cdgc;
char                  buff[MAX_LINE+1];
CD_PAD_PRIVATE       *private = (CD_PAD_PRIVATE *)pad->cdgc_data;

/***************************************************************
*  If we don't have a private area, get one.
***************************************************************/
if (!private)
   {
      private = (CD_PAD_PRIVATE *)CE_MALLOC(sizeof(CD_PAD_PRIVATE));
      if (!private)
         return;
      memset((char *)private, 0, sizeof(CD_PAD_PRIVATE));
      pad->cdgc_data = (void *)private;
   }

if (!COLORED(pad->token))
   return;

/***************************************************************
*  If have not read a line (first pass) or have changed lines,
*  flush the old data back to the memdata database and get
*  the data for the new line.
***************************************************************/
if ((lineno != private->cur_line_no) || (!private->color_read))
   {
      if (private->cdgc_list != NULL)
         {
            flatten_cdgc(private->cdgc_list, buff, sizeof(buff), False /* do them all */);
            put_color_by_num(pad->token, private->cur_line_no, buff);
            free_cdgc(private->cdgc_list);
            private->cdgc_list = NULL;
         }
      if (private->curr_cdgc != NULL)
         {
            free_cdgc(private->curr_cdgc);
            private->curr_cdgc = NULL;
         }
      private->curr_cdgc_read = False;

      if (lineno >= 0)  /* lineno = -1 for flush */
         {
            cdgc = get_color_by_num(pad->token, lineno, COLOR_CURRENT, &cdgc_line);
            private->color_read = True;
         }
      else
         {
            cdgc = NULL;
            private->color_read = False;
         }

      private->cur_line_no = lineno;

      /***************************************************************
      *  Expand the cdgc to linked list format.  We do not care about
      *  the GC's since we do not need them to flatten back to char format.
      *  This list will not be used for drawing.
      ***************************************************************/
      if (cdgc != NULL)
         private->cdgc_list = expand_cdgc(pad->display_data->display,
                                          pad->x_window,
                                          cdgc,
                                          pad->window->font->fid,
                                          &pad->display_data->gc_private_data);
      else
         private->cdgc_list = NULL;
   }

if ((private->cdgc_list != NULL) && (chars != 0))
   cdgc_add_del(private->cdgc_list, col, chars);


}  /* end of cdpad_add_remove */



/************************************************************************

NAME:      cdgc_add_del    - Update the cdgc linked list to add or delete chars

PURPOSE:    This routine will arrange color data on a line to add the
            requested characters.     This routine does
            the actual linked list manipulation.

PARAMETERS:
   1.  cdgc_list       - pointer to FANCY_LINE (INPUT)
                         This is the color data list to be manipulated.
                         This color data is associated with the line
                         rather than overflowing onto the line.  It is a linked
                          list of GC's with column numbers and also the text color names.

   2.  col             - int (INPUT)
                         This is the zero based starting column to do the
                         add or delete.

   3.  chars           - int (INPUT)
                         This is the number of chars begin add or deleted.
                         To delete characters pass a negative value.

FUNCTIONS :
   1.   Walk the linked list looking for end columns less that the
        add/delete point.  Where found, shift the end column right
        or left as appropriate.  Do not shift overflow colors.
        Do not shift the end point left of a delete point.  Do not
        shift the end point past MAX_LINE.

   2.   Walk the linked list looking for start columms less than the
        add/delete point.  Where found, shift the start column right
        or left as appropriate.  Do not shift the start point past
        MAX_LINE.  Do not shift the start point to less than the
        delete point.

RETURNED VALUE:
   changed  -  int
               True  -  The passed cdgc_list was changed.
               False -  The passed cdgc_list was not changed.

*************************************************************************/

static int   cdgc_add_del(FANCY_LINE      *cdgc_list,   /* input/output */
                          int              col,         /* input */
                          int              chars)       /* input */
{
FANCY_LINE           *curr;
int                   changed = False;

if (col < 0)
   col = 0;

for (curr = cdgc_list;
     curr;
     curr = curr->next)
{
   /***************************************************************
   *  If the add/delete point is less than the end col, we will shift 
   *  the end column left or right as appropriate.  Make sure
   *  the right col does not exceed MAX_LINE and does not go below
   *  delete col.
   ***************************************************************/
   if ((col < curr->end_col) && ((curr->end_col != COLOR_OVERFLOW_COL) && (curr->end_col != MAX_LINE)))
      {
         changed = True;
         curr->end_col   += chars;

         if (curr->end_col > MAX_LINE)
            curr->end_col = MAX_LINE; /* limit growth */

         if (curr->end_col < col)
            curr->end_col = col;  /* limit negative growth */ 
      }

   /***************************************************************
   *  If the add/delete point is in front of the color area shift
   *  the front of the color area left or right as appropriate.
   *  Make sure the first column does not exceed MAX_LINE and does
   *  not go below delete col.
   ***************************************************************/
   /* RES 01/09/2003 add test for first col > 0 */
   if ((col <= curr->first_col) && (curr->first_col > 0))
      {
         changed = True;
         curr->first_col += chars; /* shift the color area */

         if (curr->first_col > MAX_LINE)
            curr->first_col = MAX_LINE;  /* limit growth */

         if (curr->first_col < col)
            curr->first_col = col; /* limit negative growth */

      }

   /* This test is theoretically not needed since flatten_cdgc
      can deal with negative size areas */
   if (curr->end_col <= curr->first_col)
      curr->end_col = curr->first_col; /* color went away */
}

return(changed);

}  /* end of cdgc_add_del */


/************************************************************************

NAME:      cdpad_curr_cdgc  - Get the current cdgc list for drawing

PURPOSE:    This routine is used when drawing partial lines after typing.
            It knows about color data in progress and also has the
            current color data.  

PARAMETERS:
   1.  pad             - pointer to PAD_DESCR (INPUT)
                         This is the up to snuff pad we are operating on.

   2.  lineno          - int (INPUT)
                         This is the file line no we are interested in.

   3.  fancy_lineno    - int (Output)
                         This is the file line no for the line which
                         actually goes with the returned FANCY_LINE.
                         This may be less than lineno in the case of
                         overflowing color.

FUNCTIONS :
   1.   If the data being modified is correct, return it.  otherwise
        get and retain the current data for this routine.

RETURNED VALUE:
   curr_cdgc     -  pointer to FANCY_LINE
                    The drawing data for the current line.  It is a linked
                    list of GC's with column numbers and also the text color names.

*************************************************************************/

FANCY_LINE  *cdpad_curr_cdgc(PAD_DESCR      *pad,           /* input  */
                             int             lineno,        /* input  */
                             int            *fancy_lineno)  /* output */
{
int                   cdgc_line;
char                 *cdgc;
CD_PAD_PRIVATE       *private = (CD_PAD_PRIVATE *)pad->cdgc_data;
int                   next_color_lineno;

if (!COLORED(pad->token))
   return(NULL);

/***************************************************************
*  If we don't have a private area, get one.
***************************************************************/
if (!private)
   {
      private = (CD_PAD_PRIVATE *)CE_MALLOC(sizeof(CD_PAD_PRIVATE));
      if (!private)
         return(NULL);
      memset((char *)private, 0, sizeof(CD_PAD_PRIVATE));
      pad->cdgc_data = (void *)private;
   }

/***************************************************************
*  If the current line under modification works, use it.
***************************************************************/
if ((lineno == private->cur_line_no) &&
    (private->color_read) &&
    (private->cdgc_list))
   {
      *fancy_lineno = lineno;
      return(private->cdgc_list);
   }

/***************************************************************
*  If we have already calculated the color data and it has
*  not been invalidated (cur_cdgc_read), use it.
***************************************************************/
if ((private->curr_cdgc_read) &&
    (lineno >= private->cdgc_lineno) &&
    (lineno <  private->lower_lineno))
   {
      *fancy_lineno = private->cdgc_lineno;
      return(private->curr_cdgc);
   }

/***************************************************************
*  Get the color data for the next range of text.
***************************************************************/
if (private->curr_cdgc)
   free_cdgc(private->curr_cdgc);
   

if (lineno < 0)  /* lineno = -1 for flush */
   lineno = 0;

cdgc = get_color_by_num(pad->token, lineno, COLOR_UP, &cdgc_line);
private->color_read = True;

if (cdgc && *cdgc)
   {
      private->curr_cdgc = expand_cdgc(pad->display_data->display,
                                       pad->x_window,
                                       cdgc,
                                       pad->window->font->fid,
                                       &pad->display_data->gc_private_data);
      private->cdgc_lineno = cdgc_line;
   }
else
   {
      private->curr_cdgc = NULL;
      private->cdgc_lineno = 0;
   }
private->curr_cdgc_read = True;

cdgc = get_color_by_num(pad->token, lineno, COLOR_DOWN, &next_color_lineno);
if (!cdgc || !(*cdgc)) 
   private->lower_lineno = total_lines(pad->token) + 1;
else
   private->lower_lineno = next_color_lineno;


*fancy_lineno =  private->cdgc_lineno;
return(private->curr_cdgc);

}  /* end of cdpad_curr_cdgc */


/************************************************************************

NAME:      cd_join_line       - Update cd data to accommodate a join

PURPOSE:    This routine takes joins the color data from the line after
            the passed line number to the end of the line passed.  It
            deletes the data from the line after the line passed.  This
            call must be made before the actual join.

PARAMETERS:
   1.  token           - DATA_TOKEN (INPUT)
                         This is the token for the memdata line being

   2.  lineno          - int (INPUT)
                         This is the zero based file line number.

   3.  line_len        - int (INPUT)
                         The length of the line being joined onto.
                         That is, the length of the first line of the
                         pair of lines which will become 1.

FUNCTIONS :
   1.   Read the color data for the passed line number and the line
        after the passed line number.

   2.   If there is no color data on the line after the passed line
        there is nothing to do.

   3.   Expand both color data items to linked list format.

   4.   Get the data line passed and extract it's length.

   5.   Update the columns in the before and after data to show
        it is at the end of the line.  Add it to the end of the
        first lines linked list.

   6.   Flatten the data and put it away for the first line.

   7.   Delete the color data for the second line.

   8.   Clean up.

    
*************************************************************************/

void  cd_join_line(DATA_TOKEN      *token,       /* input/output */
                   int              lineno,      /* input */
                   int              line_len)    /* input */
{
char                 *first_cdgc;
char                 *sec_cdgc;
FANCY_LINE           *curr;
FANCY_LINE           *first_list;
FANCY_LINE           *sec_list;
int                   cdgc_line;
char                  buff[MAX_LINE];
int                   real_color = False;

sec_cdgc = get_color_by_num(token, lineno+1, COLOR_CURRENT, &cdgc_line);
if (sec_cdgc == NULL)
   return;

sec_list = expand_cdgc(NULL, None, sec_cdgc, None, NULL);
if (!sec_list) /* out of memory */
   return;

first_cdgc = get_color_by_num(token, lineno, COLOR_CURRENT, &cdgc_line);

first_list = expand_cdgc(NULL, None, first_cdgc, None, NULL);

/***************************************************************
*  Bump the offsets in the second line.
***************************************************************/
if (line_len > 0)
   {
      for (curr = sec_list;
           curr;
           curr = curr->next)
      {
         if (curr->first_col != curr->end_col)
            real_color = True;
         curr->first_col += line_len;
         if ((curr->end_col != COLOR_OVERFLOW_COL) && (curr->end_col != MAX_LINE))
            curr->end_col   += line_len;
      }

      /***************************************************************
      *  Find the end of the first line.
      ***************************************************************/
      for (curr = first_list;
           curr;
           curr = curr->next)
      {
         if (real_color && ((curr->end_col == COLOR_OVERFLOW_COL) || (curr->end_col == MAX_LINE)))
            curr->end_col = line_len;
         if (curr->next == NULL)
            break;
      }
   }
else
   {
      if (first_list)
         free_cdgc(first_list);
      curr = NULL;
   }

/* curr is null or points to the last entry in the list */
if (curr)
   curr->next = sec_list;
else
   first_list = sec_list;

/***************************************************************
*  Put away the new color data for the first line.
***************************************************************/
flatten_cdgc(first_list, buff, sizeof(buff), False);
put_color_by_num(token, lineno, buff);
free_cdgc(first_list);

/***************************************************************
*  Remove the color data for the second line.
*  sec_list got freed, because it is now part of first list.
***************************************************************/
put_color_by_num(token, lineno+1, NULL);
 
} /* end of cd_join_line */


/************************************************************************

NAME:      pulldown_overflow_color  -  Preserve color when a line is going away.

PURPOSE:    This routine takes care of the case where the line is colored
            via overflow and the line doing the overflow is going away.  We
            need to copy the overflow data local.

PARAMETERS:

   1.  token      -  pointer to DATA_TOKEN (INPUT / OUTPUT)
                     This is the token for the memdata structure to be pasted into.

   2.  to_line    -  int  (INPUT)
                     This is the target line in the file to colorize.  That is
                     the line past the last line being deleted.

FUNCTIONS :
   1.   Get the color data for the line.

   2.   If there is color data for this line, we need do nothing.

   3.   See if there is anything overflowing into this line.  If so, save it.

*************************************************************************/

void  pulldown_overflow_color(PAD_DESCR      *pad,
                              int             to_line)
{
FANCY_LINE           *cdgc;
FANCY_LINE           *curr;
char                 *color_line;
int                   last_color_lineno;
FANCY_LINE            new;
char                  buff[100]; /* lots of room */


color_line = get_color_by_num(pad->token,
                              to_line,
                              COLOR_UP,
                              &last_color_lineno);

if (color_line &&
    *color_line &&
    (last_color_lineno < to_line))
   {
      cdgc = expand_cdgc(pad->display_data->display,
                         pad->display_data->main_pad->x_window,
                         color_line,
                         pad->window->font->fid,
                         &pad->display_data->gc_private_data);

      for (curr = cdgc; curr; curr = curr->next)
      {
         if (curr->first_col == curr->end_col)
            continue; /* ignore zero sized regions */

         if (curr->end_col == COLOR_OVERFLOW_COL)
            {
               memset((char *)&new, 0, sizeof(new));
               new.end_col = COLOR_OVERFLOW_COL;
               new.gc      = curr->gc;
               new.bgfg    = malloc_copy(curr->bgfg);
               flatten_cdgc(&new,
                            buff,
                            sizeof(buff),
                            False); /* do all the cdgc elements (there is only 1) */
               put_color_by_num(pad->token, to_line, buff);
               break;
            }
      }

      free_cdgc(cdgc);
   }
 
} /* end of pulldown_overflow_color */


/************************************************************************

NAME:      cd_flush           - Flush color data back to the memdata database

PURPOSE:    This routine takes the storage associated with a linked list of
            FANCY_LINES.

PARAMETERS:
   1.  cdgc            - pointer to FANCY_LINE (INPUT)
                         This is the cdgc linked list to free.

FUNCTIONS :
   1.   If the pads work_cdgc has been read in and modified, flatten it
        and write it out to the memdata database.

   2.   Free any work_cdgc elements.

   3.   Clear the work_cdgc flags.

    
*************************************************************************/

void  cd_flush(PAD_DESCR      *pad)           /* input/output */
{

cdpad_add_remove(pad, -1 /* for flush */, 0, 0);

} /* end of cd_flush */



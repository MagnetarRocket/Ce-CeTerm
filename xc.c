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
*  module xc.c
*
*  Routines:
*     dm_xc               -   Do a copy range of text (or cut)
*     dm_xp               -   Do a paste
*     dm_undo             -   Undo processing and redo (un-undo) processing
*     dm_case             -   Case processing on a range of text (upper, lower, swap)
*     case_line           -   change the case (upper, lower, swap) of a line of text
*     dm_xl               -   Copy liternal command to a paste buffer (xl)
*     dm_xa               -   Paste buffer append (xa)
*
* Internal:
*     copy_to_buffer      -   Copy data to a paste buffer
*     copy_one_line       -   Special one line version of copy_to_buffer
*     cut_from_memory     -   Cut a range of lines from memory.
*     cut_from_memory_rect-   Cut a range of lines, using rectangular rules.
*     cut_one_line        -   Special case for cutting one or less lines.
*     rectangular_substr  -   get a substring of a line via rectangular rules.
*     paste_rectangular   -   perform rectangular pasting.
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h   /usr/include/sys/errno.h      */
#include <ctype.h>          /* /usr/include/ctype.h      */
#include <stdlib.h>         /* /usr/include/stdlib.h     */
#include <limits.h>         /* /usr/include/limits.h     */

#include "ca.h"
#include "cd.h"
#include "debug.h"
#include "dmsyms.h"
#include "dmwin.h"
#include "getevent.h"
#include "label.h"
#include "mark.h"
#include "memdata.h"
#ifdef PAD
#include "pad.h"
#endif
#include "parms.h"
#include "pastebuf.h"
#include "tab.h"
#include "typing.h"
#include "txcursor.h"
#include "undo.h"
#ifdef PAD
#include "vt100.h"
#endif
#include "xc.h"


/***************************************************************
*  
*  Prototypes for internal routines.
*  
***************************************************************/


static int   copy_to_buffer(DMC               *dmc,
                            int                top_line,
                            int                top_col,
                            int                bottom_line,
                            int                bottom_col,
                            PAD_DESCR         *buffer,
                            Display           *display,
                            Window             main_x_window,
                            int                echo_mode,
                            Time               timestamp,
                            char               escape_char); 

static void  copy_one_line(char              *line,
                           int                top_col,
                           int                bottom_col,
                           FILE              *paste_stream,
                           int                rect_copy);


static void  cut_from_memory(int                top_line,
                             int                top_col,
                             int                bottom_line,
                             int                bottom_col,
                             PAD_DESCR         *buffer);

static void  cut_from_memory_rect(int                top_line,
                                  int                top_col,
                                  int                bottom_line,
                                  int                bottom_col,
                                  PAD_DESCR         *buffer);

static void  cut_one_line(PAD_DESCR         *buffer,
                          int                line_no,
                          int                top_col,
                          int                bottom_col);

static void rectangular_substr(char     *in_str,
                               int       from_col,
                               int       to_col,
                               char     *out_str);

static void  paste_rectangular(DATA_TOKEN     *token,
                               int             to_line,
                               int             to_col,
                               FILE           *paste_stream,
                               int             overstrike);

#ifdef PAD
static void  paste_vt100(PAD_DESCR          *main_window,
                         FILE               *paste_stream);

#endif


/************************************************************************

NAME:      dm_xc  -  DM Copy command (xc) or Cut command (xd)

PURPOSE:    This routine copies data to a paste buffer.

PARAMETERS:

   1.  dmc      - pointer to DMC (INPUT)
                  This is the DM command structure for the command
                  to be processed.

   2.  mark1      -  pointer to MARK_REGION (INPUT)
                     If set, this is the most current mark.  It is matched
                     with tdm_mark or cursor_buff to get the line range.

   3.  cursor_buff - pointer to BUFF_DESCR (INPUT)
                     This is the buffer description which shows the current
                     location of the cursor.

   4.  main_window - pointer to DRAWABLE_DESCR (INPUT)
                     This is the drawable description for the main window.  It is
                     needed by open_paste_buffer and is passed through.

   5.  echo_mode   - int (INPUT)
                     This is the current echo mode.  An xc or xd acts like an
                     xc -r (or xd -r) if the echo mode is RECTANGULAR_HIGHLIGHT

   6.  timestamp   - Time (INPUT - X lib type)
                     This is the timestamp from the keyboard or mouse event which
                     caused the xc or xd.  It is needed by open_paste_buffer
                     when dealing with the X paste buffers.

   7.  escape_char - int (INPUT)
                     This is the escape character.
                     It is an '@' or a '\'



FUNCTIONS :

   1.   Determine the range of text to copy

   2.   Make sure the two points are in the same window.

   3.   Order the two points from top to bottom on the file.

   4.   Open the paste buffer

   5.   Copy the data to the paste buffer.


RETURNED VALUE:
   redraw -  The window mask anded with the type of redraw needed is returned.


*************************************************************************/

int   dm_xc(DMC               *dmc,                /* input   */
            MARK_REGION       *mark1,              /* input   */
            BUFF_DESCR        *cursor_buff,        /* input   */
            Display           *display,
            Window             main_x_window,
            int                echo_mode,          /* input   */
            Time               timestamp,          /* input - X lib type */
            char               escape_char)        /* input   */
{

int                      top_line;
int                      top_col;
int                      bottom_line;
int                      bottom_col;
PAD_DESCR               *buffer;
int                      rc;
int                      redraw_needed = 0;
char                     msg[100];

rc = range_setup(mark1,              /* input   */
                 cursor_buff,        /* input   */
                 &top_line,          /* output  */
                 &top_col,           /* output  */
                 &bottom_line,       /* output  */
                 &bottom_col,        /* output  */
                 &buffer);           /* output  */

/***************************************************************
*  Make sure the range is in the same window by checking rc
***************************************************************/
if (rc == 0)
   {

      DEBUG2(fprintf(stderr, "(%s) in %s from : [%d,%d]  to : [%d,%d]\n",
             dmsyms[dmc->xc.cmd].name,  which_window_names[buffer->which_window],
             top_line, top_col, bottom_line, bottom_col);
      )
      /* 8/4/94 do not do a zero size copy - Customer reported problem John Hopkins - JPL */
      if ((top_line == bottom_line) && (top_col == bottom_col) && !dmc->xc.dash_r && (echo_mode != RECTANGULAR_HIGHLIGHT))
         {
            DEBUG2(fprintf(stderr, "zero size region\n");)
            stop_text_highlight(mark1->buffer->display_data);
            mark1->mark_set = False;
            return(redraw_needed);
         }

      /***************************************************************
      *  
      *  Do the copy and maybe the cut.  Then set up the type of
      *  redrawing needed on the main screen.
      *  
      ***************************************************************/
      rc = copy_to_buffer(dmc, top_line, top_col, bottom_line, bottom_col, buffer, display, main_x_window, echo_mode, timestamp, escape_char);

      if (rc == 0 && dmc->any.cmd == DM_xd)
         {
            if (dmc->xc.dash_r || echo_mode == RECTANGULAR_HIGHLIGHT)
               cut_from_memory_rect(top_line, top_col, bottom_line, bottom_col, buffer);
            else
               cut_from_memory(top_line, top_col, bottom_line, bottom_col, buffer);
            redraw_needed = (buffer->redraw_mask & PARTIAL_REDRAW);

            cursor_buff->win_line_no = top_line - buffer->first_line; /* RES 6/23/95 */
            redraw_needed |= dm_position(cursor_buff, buffer, top_line, top_col);
            redraw_needed |= redraw_start_setup(cursor_buff);/* in typing.c */
            /* reread the file line number in case it got blasted */
            buffer->buff_ptr = get_line_by_num(buffer->token, buffer->file_line_no);
            buffer->buff_modified = False;
         }

   }
else
   {
      snprintf(msg, sizeof(msg), "(%s) Invalid Text Range", dmsyms[dmc->xc.cmd].name);
      dm_error(msg, DM_ERROR_BEEP);
   }

/***************************************************************
*  Clear the mark and turn off echo mode.
***************************************************************/
stop_text_highlight(mark1->buffer->display_data);
mark1->mark_set = False;

return(redraw_needed);

} /* end of dm_xc */




/************************************************************************

NAME:      dm_xp  -  DM paste command

PURPOSE:    This routine pastes data from a paste buffer or file.

PARAMETERS:

   1.  dmc      - pointer to DMC (INPUT)
                  This is the DM command structure for the command
                  to be processed.

   2.  cursor_buff - pointer to BUFF_DESCR (INPUT)
                     This is the buffer description which shows the current
                     location of the cursor.
   3.  main_window - pointer to DRAWABLE_DESCR (INPUT)
                     This is the drawable description for the main window.  It is
                     needed by open_paste_buffer and is passed through.



FUNCTIONS :

   1.   If we are already at the top of the pad, do nothing.

   2.   Set the pointers to show we are at the first line.


RETURNED VALUE:
   redraw -  The window mask anded with the type of redraw needed is returned.


*************************************************************************/

int   dm_xp(DMC               *dmc,                /* input   */
            BUFF_DESCR        *cursor_buff,        /* input   */
            Display           *display,
            Window             main_x_window,
            char               escape_char)        /* input   */
{

int                      to_line;
int                      to_col;
PAD_DESCR               *to_buffer;
int                      redraw_needed;
int                      line_count;
int                      len;
FILE                    *paste_stream;
DMC_lbl                  lbl_dmc;

/***************************************************************
*  RES 03/01/2005 add -l processing to xp
*  For a non rectangular paste and -l (last) was specified,
*  mark the spot we are pasting in front of.
***************************************************************/
if (!(dmc->xp.dash_r) && (dmc->xp.dash_l) && (cursor_buff->which_window == MAIN_PAD))
   {
      memset(&lbl_dmc, 0, sizeof(lbl_dmc));
      lbl_dmc.cmd  = DM_lbl;
      lbl_dmc.parm = "_XP_";
      dm_lbl((DMC *)&lbl_dmc, cursor_buff, False, False);
   }

/***************************************************************
*  Get the target point.  mark_point_setup in mark.c
***************************************************************/
mark_point_setup(NULL,              /* input  */
                 cursor_buff,       /* input  */
                 &to_line,          /* output  */
                 &to_col,           /* output  */
                 &to_buffer);       /* output  */

DEBUG2( fprintf(stderr,"(xp) in %s  [%d,%d]\n",
                        which_window_names[to_buffer->which_window],
                        to_buffer->file_line_no, to_buffer->file_col_no);
 )

if (cursor_buff->current_win_buff->file_col_no >= MAX_LINE)
   {
      dm_error("Line too long", DM_ERROR_BEEP);
      return(0);
   }

/***************************************************************
* Verify that we are somewhere pasteable
***************************************************************/
if (to_line < 0)
   {
      dm_error("(xp) No text under cursor", DM_ERROR_BEEP);
      return(0);
   }

/***************************************************************
* If we are off the left side of the screen, move the cursor.
***************************************************************/
if (to_col< 0)
   to_col = 0;


/***************************************************************
* open the paste buffer we need to read.
***************************************************************/
paste_stream = open_paste_buffer(dmc, display, main_x_window, 0, escape_char);
if (paste_stream == NULL)
   return(0);  /* msg already sent */

line_count = 2; /* estimate buffer size for memdata routine, not really used */

DEBUG2(
if (to_line > total_lines(to_buffer->token))
   fprintf(stderr, "(xp) Current Last line is %d\n", total_lines(to_buffer->token)-1);
)

#ifdef PAD
/**************************************************************
*  
*  If this is a paste in vt100 mode to the main pad, send
*  everything to the shell
*  
***************************************************************/

if ((*to_buffer->vt100_mode) && (to_buffer->which_window == MAIN_PAD))
   {
      paste_vt100(to_buffer, paste_stream);
      ce_fclose(paste_stream);
      return(0);
   }

#endif


/***************************************************************
*  
*  Add null lines to the end of the file to get to to_line if required.
*  
***************************************************************/

while(to_line >= total_lines(to_buffer->token))
{
   DEBUG2( fprintf(stderr,"(xp) Adding null line %d at eof\n", total_lines(to_buffer->token)); )
   put_line_by_num(to_buffer->token, total_lines(to_buffer->token)-1, "", INSERT);
   to_buffer->buff_ptr = "";
}

/***************************************************************
*  
* Add enough blanks to the line being pasted into to reach to_col
*  if required.
*  
***************************************************************/

len = strlen(to_buffer->buff_ptr);
if (to_col > len)
   {
      insert_in_buff(to_buffer->buff_ptr,
                     "",
                     to_col,
                     1,
                     MAX_LINE);
      DEBUG3( fprintf(stderr," (xp) Saving line %d in %s  \"%s\"\n", to_buffer->file_line_no,  which_window_names[to_buffer->which_window], to_buffer->buff_ptr); )
      put_line_by_num(to_buffer->token, to_line, to_buffer->buff_ptr, OVERWRITE);
      to_buffer->buff_modified = False;
      DEBUG2( fprintf(stderr,"(xp) Extended line %d to len %d  \"%s\"\n", to_buffer->file_line_no,  to_col, to_buffer->buff_ptr); )
   }

/***************************************************************
*  
*  For a non-rectangular paste, put_block_by_num does the work.
*  Otherwise, we do it in paste_rectangular.
*  
***************************************************************/

if (!dmc->xp.dash_r)
   put_block_by_num(to_buffer->token,
                    to_line,
                    line_count,
                    to_col,
                    paste_stream);
else
   paste_rectangular(to_buffer->token,
                     to_line,
                     to_col,
                     paste_stream,
                     dmc->xp.dash_a);


ce_fclose(paste_stream);

to_buffer->buff_ptr = get_line_by_num(to_buffer->token, to_line);
to_buffer->file_line_no = to_line;
to_buffer->buff_modified = False;

/***************************************************************
*  RES 03/01/2005 add -l processing to xp
*  If we created a label at the end of the paste, go there and
*  then delete the label.
***************************************************************/
if (!(dmc->xp.dash_r) && (dmc->xp.dash_l) && (cursor_buff->which_window == MAIN_PAD))
   {
      DMC_glb        glb_dmc;
      DMC           *mark_dmc;
      memset(&glb_dmc, 0, sizeof(glb_dmc));
      glb_dmc.cmd  = DM_glb;
      glb_dmc.parm = "_XP_";
      mark_dmc = dm_glb((DMC *)&glb_dmc, cursor_buff, cursor_buff->current_win_buff);
      if (mark_dmc)
         {
            to_line = mark_dmc->markc.row;
            to_col  = mark_dmc->markc.col;
            free_dmc(mark_dmc);
         }
   }

redraw_needed = redraw_start_setup(cursor_buff);
redraw_needed |= dm_position(cursor_buff, to_buffer, to_line, to_col);
redraw_needed |= redraw_start_setup(cursor_buff);
redraw_needed |= (to_buffer->redraw_mask & PARTIAL_REDRAW);

/***************************************************************
*  RES 03/01/2005 add -l processing to xp
*  Delete the temporary _XP_ label.
***************************************************************/
if (!(dmc->xp.dash_r) && (dmc->xp.dash_l) && (cursor_buff->which_window == MAIN_PAD))
   {
      dm_lbl((DMC *)&lbl_dmc, cursor_buff, False, True); /* last true deletes the label */
   }


return(redraw_needed);

} /* end of dm_xp */




/************************************************************************

NAME:      copy_to_buffer  -  Perform the data movement part of a copy or paste.

PURPOSE:    This routine copies data to a paste buffer.

PARAMETERS:

   1.  dmc        -  pointer to DMC (INPUT)
                     This is the DM command structure for the command
                     being be processed.

   2.  top_line   -  int (INPUT)
                     This is the zero based line number of the first
                     line to put in the paste buffer.

   3.  top_col    -  int (INPUT)
                     This is the zero based col number of the first
                     colunm on the first line to put in the paste buffer.

   4.  bottom_line-  int (INPUT)
                     This is the zero based line number of the last
                     line to put in the paste buffer.

   5.  bottom_col -  int (INPUT)
                     This is the zero based col number of the column just
                     past the last column on the last line to include.  If it
                     is greater than strlen of the line, it will only be greater
                     by 1 and that means that the newline on that line should be
                     included.

   6.  buffer      - pointer to PAD_DESCR (INPUT)
                     This is a pointer to the buffer description of the buffer
                     being copied from.  It is used to determine if this is
                     a copy from the main window, dm input window, or dm output
                     window.

   7.  main_window - pointer to DRAWABLE_DESCR (INPUT)
                     This is the drawable description for the main window.  It is
                     needed by open_paste_buffer and is passed through.

   8.  echo_mode   - int (INPUT)
                     This is the current echo mode.  An xc or xd acts like an
                     xc -r (or xd -r) if the echo mode is RECTANGULAR_HIGHLIGHT

   9.  timestamp   - Time (INPUT - X lib type)
                     This is the timestamp from the keyboard or mouse event which
                     caused the xc or xd.  It is needed by open_paste_buffer
                     when dealing with the X paste buffers.  The value is only needed
                     when doing a copy or cut (write to the paste buffer), so NULL is
                     allowed for paste (read) operations.

FUNCTIONS :

   1.   Open the paste buffer.

   2.   If this is the DM input or output windows copy the data from this buffer.

   3.   If this is the main window, get the first line.  If there is only one
        line process it.

   4.   If this is a regular paste buffer, output a line with the line count in it.

   5.   Copy the first line starting at the starting colunm.

   6.   copy the middle lines.

   7.   Copy the last line ending at the last column.  If the last column is past the
        end of the line, include the newline character.

RETURNED VALUE
   rc        - return code
               This shows whether the data was written out.
               0  -  ok
              -1  -  open Failure, dm_error already output


*************************************************************************/

static int   copy_to_buffer(DMC               *dmc,
                            int                top_line,
                            int                top_col,
                            int                bottom_line,
                            int                bottom_col,
                            PAD_DESCR         *buffer,
                            Display           *display,
                            Window             main_x_window,
                            int                echo_mode,
                            Time               timestamp,
                            char               escape_char) 
{
FILE             *paste_stream;
int               line_count;
int               i;
int               len;
char             *line;
char             *extra_line;
char              rect_buff[MAX_LINE+1];
int               window_top_col;
int               window_bottom_col;
int               rect_top_col;
int               rect_bottom_col;
int               rect_copy;
int               hex_mode = buffer->display_data->hex_mode;


paste_stream = open_paste_buffer(dmc, display, main_x_window, timestamp, escape_char);
if (paste_stream == NULL)
   return(-1);  /* msg already sent */

rect_copy = (dmc->xc.dash_r || (echo_mode == RECTANGULAR_HIGHLIGHT));

if (X_PASTE_STREAM(paste_stream) && !rect_copy)
   {
      /* the dash r parm passes the width of the rectangular area if -r was specified, works correctly with zero width */
      i = setup_x_paste_buffer(paste_stream, buffer->token, top_line, bottom_line, 0);
      if (i != 0)
         return(-1);
   }

position_file_pointer(buffer->token, top_line);
line = next_line(buffer->token);
if (line != NULL)
   if (top_line == bottom_line)
      {
         if (rect_copy)
            {
               if (top_col == bottom_col)
                  bottom_col = MAX_LINE;
               if (X_PASTE_STREAM(paste_stream))
                  {
                     /* the dash r parm passes the width of the rectangular area if -r was specified, works correctly with zero width */
                     i = setup_x_paste_buffer(paste_stream, buffer->token, top_line, bottom_line, strlen(line));
                     if (i != 0)
                        return(-1);
                  }
            }
         copy_one_line(line, top_col, bottom_col, paste_stream, rect_copy);
      }
   else
      {
         line_count = (bottom_line - top_line) + 1; /* a count of lines */
         if (!rect_copy)
            {
               /***************************************************************
               *  First line. skip up to top_col, write out the rest
               ***************************************************************/
               len = strlen(line);
               if (top_col != 0)
                  if (top_col < len)
                     line += top_col;
                  else
                     line += len;  /* point to the null string */

               ce_fputs(line, paste_stream);
               ce_fputs("\n", paste_stream);

               /***************************************************************
               *  Middle lines.  Start at 2 because we already did the first
               *  line and we want to stop before the last line.
               ***************************************************************/
               for (i = 2; i < line_count; i++)
               {
                  line = next_line(buffer->token);
                  if (line == NULL)
                     break;

                  ce_fputs(line, paste_stream);
                  ce_fputs("\n", paste_stream);

               }

               /***************************************************************
               *  last_line. Truncate at from_col
               ***************************************************************/
               line = next_line(buffer->token);
               if (line != NULL && bottom_col != 0)
                  {
                     /* if the mark to end is past the end of line, include the newline */
                     len = strlen(line);
                     DEBUG2(   fprintf(stderr, "xc: end col = %d, strlen = %d\n", bottom_col, len);)

                     if (len >= bottom_col)
                        ce_fwrite(line, 1, bottom_col, paste_stream);
                     else
                        {
                           ce_fputs(line, paste_stream);
                           ce_fputs("\n", paste_stream);
                        }

                  }
               else
                  DEBUG2(   fprintf(stderr, "xc: Last line ends at col 0\n");)
            } /* end of normal copy */
         else
            {

               /***************************************************************
               *  
               *  Rectangular copy.  The buffer is already primed.  Get the
               *  The required piece of text, cutting and padding as needed
               *  and write it out.  Always put on a newline.
               *  
               ***************************************************************/
               window_top_col    = tab_pos(line, top_col, TAB_EXPAND, hex_mode);
               extra_line        = get_line_by_num(buffer->token, bottom_line);
               window_bottom_col = tab_pos(extra_line, bottom_col, TAB_EXPAND, hex_mode);
               if (X_PASTE_STREAM(paste_stream))
                  {
                     /* the dash r parm passes the width of the rectangular area if -r was specified, works correctly with zero width */
                     i = setup_x_paste_buffer(paste_stream, buffer->token, top_line, bottom_line, ABS2(window_top_col, window_bottom_col));
                     if (i != 0)
                        return(-1);
                  }

               while(top_line <= bottom_line)
               {
                  rect_top_col      = tab_pos(line, window_top_col, TAB_CONTRACT, hex_mode);
                  rect_bottom_col   = tab_pos(line, window_bottom_col, TAB_CONTRACT, hex_mode);
                  rectangular_substr(line, rect_top_col, rect_bottom_col, rect_buff);
                  ce_fputs(rect_buff, paste_stream);
                  ce_fputs("\n", paste_stream);
                  line = next_line(buffer->token);
                  top_line++;
               }
            }  /* end of rectangular copy */
      }

ce_fclose(paste_stream);

return(0);

} /* end of copy_to_buffer */


/************************************************************************

NAME:      cut_from_memory  -  Remove one or more lines from the memory map

PURPOSE:    This routine deletes the data between two points in the memory map
            line numbers and columns are taken into account.

PARAMETERS:

   1.  top_line   -  int (INPUT)
                     This is the zero based line number of the first
                     line to be cut out.

   2.  top_col    -  int (INPUT)
                     This is the zero based col number of the first
                     column on the first line to be cut out.

   3.  bottom_line-  int (INPUT)
                     This is the zero based line number of the last
                     line to cut out.

   4.  bottom_col -  int (INPUT)
                     This is the zero based col number of the column just
                     past the last column on the last line to cut.  If it
                     is greater than strlen of the line, it will only be greater
                     by 1 and that means that the newline on that line should be
                     cut and the line joined to the next line.

   5.  buffer      - pointer to PAD_DESCR (INPUT)
                     This is a pointer to the buffer description of the buffer
                     being copied from.  It is used to determine if this is
                     a copy from the main window, dm input window, or dm output
                     window.


FUNCTIONS :

   1.   If there is only one line to be cut, call the routine to handle
        cuts of partial lines.

   2.   Read the first line to be cut.  Notice that we always read the first
        line over an over, because after we delete the line, the next line has
        this line number.

   3.   If the starting column is not zero, then preserve the beginning of the line
        by doing a split at this point and positioning to do the deletes
        at the next line.

   4.   Delete all the lines but the last one.  Notice that while the loop goes
        from top line to bottom line, the line being read stays the same because
        the line numbers migrate up as lines are deleted.

   5.   If the end column is zero, the last line is not deleted.  If the top column
        was zero, the first line was deleted and we are done.  If the top column was
        not zero we need to join the last line to the previous line.  If the end column
        is greater than zero,  we either delete the whole line (if end column is > len)
        or join the line to the previous line.


*************************************************************************/

static void  cut_from_memory(int                top_line,
                             int                top_col,
                             int                bottom_line,
                             int                bottom_col,
                             PAD_DESCR         *buffer)
{
int               line_count;
int               i;
int               len;
char             *line;

if (top_line == bottom_line)
   cut_one_line(buffer, top_line, top_col, bottom_col);
else
   {
      line = get_line_by_num(buffer->token, top_line);
      if (line == NULL)
         return;
      len = strlen(line);

      DEBUG2(
             line_count = (bottom_line - top_line) + 1; /* a count of lines */
             fprintf(stderr, "xd: line_count = %d\n", line_count);
      )

      /***************************************************************
      *  Process the first line.  If the whole line goes away, delete it
      *  along with the rest if the intermedite lines, then get the last
      *  line, which now has line number first line.
      ***************************************************************/
      if (top_col != 0)
         {
            /***************************************************************
            *  If we are not killing off the whole line, make sure we are
            *  getting rid of something.  If so, truncate the line.
            *  Then bump top_line to the first line to be really deleted.
            ***************************************************************/
            if (top_col < len)
               split_line(buffer->token, top_line, top_col, 0 /* throw away newline  - is a truncate line */);
            
            top_line++;
         }

      /***************************************************************
      *  Get rid of the lines up to the last line.  top_line is used
      *  in the delete call because after the delete, everything shifts
      *  up one and the next line to delete is now top line.
      ***************************************************************/

      for (i = top_line; i < bottom_line; i++)
         delete_line_by_num(buffer->token, top_line, 1);

      bottom_line = top_line;


      /***************************************************************
      *  Get the last line and see if we delete it or just remove part
      *  of it.  If we are not deleting any characters (bottom_col = 0),
      *  see if we need to join it to the previous line.
      *  Note that if top_col is greater than zero, we have previously
      *  incremented it.
      ***************************************************************/
      if (bottom_col != 0)
         {
            line = get_line_by_num(buffer->token, bottom_line);
            len = strlen(line);
            if (len > bottom_col)
               {
                  put_line_by_num(buffer->token, bottom_line, &line[bottom_col], OVERWRITE);
                  if (COLORED(buffer->token))
                     {
                        cdpad_add_remove(buffer, bottom_line,  0/*col*/, 0-bottom_col);  /* chars to add (negative) */
                        cd_flush(buffer);
                     }

                  if (top_col != 0)
                     {
                        if (COLORED(buffer->token))
                           cd_join_line(buffer->token, bottom_line-1, strlen(get_line_by_num(buffer->token, bottom_line-1)));
                        join_line(buffer->token, bottom_line-1);
                     }
               }
            else
               delete_line_by_num(buffer->token, bottom_line, 1);
         }
      else
         {
            DEBUG2(   fprintf(stderr, "xd: Last line ends at col 0\n");)
            if (top_col != 0)
               {
                  if (COLORED(buffer->token))
                     cd_join_line(buffer->token, top_line-1, strlen(get_line_by_num(buffer->token, top_line-1)));
                  join_line(buffer->token, top_line-1);
               }
         }
   }

} /* end of cut_from_memory */




/************************************************************************

NAME:      copy_one_line  -  Perform the data movement for a single line.

PURPOSE:    This routine copies one line or less to a paste buffer.

PARAMETERS:

   1.  line       -  pointer to char (INPUT)
                     This is the line to output.

   2.  start_col  -  int (INPUT)
                     This is the zero based column number of the first
                     column on the first line to put in the paste buffer.

   3.  end_col    -  int (INPUT)
                     This is the zero based column number of the column just
                     past the last column on the last line to include.  If it
                     is greater than strlen of the line, it will only be greater
                     by 1 and that means that the newline on that line should be
                     included.

   4.  paste_stream- pointer to FILE (INPUT)
                     This is the open paste buffer to write to.

   5.  rect_copy  -  int (INPUT)
                     This flag, when true specifies a rectangular copy.


FUNCTIONS :

   1.   If this is a regular paste buffer, output a line with the line count in it.

   2.   Copy the line starting at the starting colunm and ending at the end column or
        end of line.

   3.   If the end column is greater than the end of line, include the newline char.


*************************************************************************/

static void  copy_one_line(char              *line,
                           int                start_col,
                           int                end_col,
                           FILE              *paste_stream,
                           int                rect_copy)
{
int               len;
int               chars_out;
char             *p;

/***************************************************************
*  Special case, part of one line;
*  Regular paste buffers get a line count stuck on the front.
*  If the mark to end is past the end of line, include the newline
***************************************************************/

len = strlen(line);
DEBUG2(fprintf(stderr, "xc: end col = %d, strlen = %d\n", end_col, len);)

if (len < end_col)
  chars_out = len;
else
  chars_out = end_col;

DEBUG2(
   if (len >= end_col)
      fprintf(stderr, "xc: single line without newline\n");
   else
      fprintf(stderr, "xc: single line with newline\n");
)

if (start_col > len)
   start_col = len;

p = &line[start_col];
chars_out -= start_col;
ce_fwrite(p, 1, chars_out, paste_stream);
DEBUG2(putc('"', stderr);fwrite(p, 1, chars_out, stderr);putc('"',stderr);putc('\n',stderr);)

if (len < end_col || rect_copy)
   ce_fputs("\n", paste_stream);

} /* end of copy_one_line */



/************************************************************************

NAME:      cut_one_line  -  Perform the data movement for a single line.

PURPOSE:    This routine cuts data from a line.  It is called after the
            data was successfully copied to the paste buffer.

PARAMETERS:

   1.  buffer      -  pointer to PAD_DESCR (INPUT / OUTPUT)
                     This is the buffer being operated on.

   2.  line_no    -  pointer to char (INPUT)
                     This is the line to modified or deleted.

   3.  start_col  -  int (INPUT)
                     This is the zero based col number of the first
                     colunm to be cut.

   4.  end_col    -  int (INPUT)
                     This is the zero based col number of the column just
                     past the last column on the line to cut.  If it
                     is greater than strlen of the line, it will only be greater
                     by 1 and that means that the newline on that line should be
                     included.


FUNCTIONS :

   1.   Read the line to be cut.

   2.   If the cut does not start in col 0, save the portion of the line
        up to the cut point.

   3.   If the end column does not cut the whole string, save the
        remaining portion in the work buffer with the leading portion
        of the line.

   4.   If there is no line left, blast it.

   5.   If the newline was removed at the end of the line, join the next line
        to this line.


*************************************************************************/

static void  cut_one_line(PAD_DESCR         *buffer,
                          int                line_no,
                          int                start_col,
                          int                end_col)
{
int               len;
char              buff[MAX_LINE+1];
char             *line;

/***************************************************************
*  Special case, part of one line;
*  Regular paste buffers get a line count stuck on the front.
*  If the mark to end is past the end of line, include the newline
***************************************************************/


line = get_line_by_num(buffer->token, line_no);
if (line == NULL)
   return;
len = strlen(line);

DEBUG2(fprintf(stderr, "xd: end col = %d, strlen = %d\n", end_col, len);)

/***************************************************************
*  
*  If the start of cut is off the end of the line, pad the line to this
*  spot.  We will be doing a join.
*  
***************************************************************/


/***************************************************************
*  
*  If the cut does not start at column zero, preserve the part
*  of the line we are saving in a work buffer.
*  If the start of cut is off the end of the line, pad the line to this
*  spot. 
*  
***************************************************************/

if (COLORED(buffer->token))
   {
      cdpad_add_remove(buffer, line_no,  start_col, start_col - end_col);  /* chars to add (negative) */
      cd_flush(buffer);
   }

if (start_col > 0)
   {
      strncpy(buff, line, start_col);
      if (start_col > len)
         insert_in_buff(buff, "", start_col, False, MAX_LINE);
      buff[start_col] = '\0';
   }
else
   buff[0] = '\0';


/***************************************************************
*  
*  If the cut does not go to end of line, attach the part we
*  are not cutting to the work buffer.
*  
***************************************************************/

if (end_col < len)
   strcat(buff, &line[end_col]);

/***************************************************************
*  
*  If there is any line left, replace it back in the memdata
*  structure.  If the newline was to be deleted (end col > len),
*  join the next line to this one.  If the line is completely
*  gone, delete it.
*  
***************************************************************/

if ((buff[0] != '\0') || (end_col <= len))
   {
      put_line_by_num(buffer->token, line_no, buff, OVERWRITE);
      if (end_col > len)
         {
            if (COLORED(buffer->token))
               cd_join_line(buffer->token, line_no, strlen(get_line_by_num(buffer->token, line_no)));
            join_line(buffer->token, line_no);
         }
   }
else
   delete_line_by_num(buffer->token, line_no, 1);

} /* end of cut_one_line */



/************************************************************************

NAME:      cut_from_memory_rect  -  Remove one or more lines from the memory map
                                    using rectangular rules.

PURPOSE:    This routine deletes the data between two points in the memory map
            line numbers and columns are taken into account.  Rectangular rules
            are followed. (see functions).

PARAMETERS:

   1.  top_line   -  int (INPUT)
                     This is the zero based line number of the first
                     line to be cut out.

   2.  left_col   -  int (INPUT)
                     This is the zero based col number of the first
                     column on the first line to be cut out.

   3.  bottom_line-  int (INPUT)
                     This is the zero based line number of the last
                     line to cut out.

   4.  right_col  -  int (INPUT)
                     This is the zero based col number of the column just
                     past the last column on the last line to cut.  If it
                     is greater than strlen of the line, it will only be greater
                     by 1 and that means that the newline on that line should be
                     cut and the line joined to the next line.

   5.  buffer      - pointer to PAD_DESCR (INPUT)
                     This is a pointer to the buffer description of the buffer
                     being copied from.  It is used to determine if this is
                     a copy from the main window, dm input window, or dm output
                     window.


FUNCTIONS :

   1.   Position the file to read the first line to be cut up.

   2.   Read each line.  Note that lines are never deleted in rectangular mode.
        At least a null line remains.

   3.   Chop the requested piece out of each line.  If the line is not as long
        as right_col, truncate it, but do not delete it.

   4.   Replace each chopped up line.  Lines which were not modified, do not
        need to be replaced.  This situlation occurs when left_col is greater than
        the length of the line.


*************************************************************************/

static void  cut_from_memory_rect(int                top_line,
                                  int                left_col,
                                  int                bottom_line,
                                  int                right_col,
                                  PAD_DESCR        *buffer)
{
int               len;
int               i;
char              buff[MAX_LINE+1];
char             *line;
char             *extra_line;
int               changed;
int               window_left_col;
int               window_right_col;
int               rect_left_col;
int               rect_right_col;
int               hex_mode = buffer->display_data->hex_mode;


/***************************************************************
*  
*  Position the memdata to get lines from the correct place and
*  figure out where the window cols are.
*  
***************************************************************/

position_file_pointer(buffer->token, top_line);
line = next_line(buffer->token); /* checking done by check_and_add_lines before call to dm_xc */
window_left_col  = tab_pos(line, left_col, TAB_EXPAND, hex_mode);
extra_line       = get_line_by_num(buffer->token, bottom_line);
window_right_col = tab_pos(extra_line, right_col, TAB_EXPAND, hex_mode);

/***************************************************************
*  
*  Process each line.
*  
***************************************************************/

while(top_line <= bottom_line)
{
   if (!line) /* past end of data */
      {
         put_line_by_num(buffer->token, total_lines(buffer->token)-1, "", INSERT);
         line = "";
      }
   changed = False;
   len = strlen(line);
   rect_left_col    = tab_pos(line, window_left_col, TAB_CONTRACT, hex_mode);
   rect_right_col   = tab_pos(line, window_right_col, TAB_CONTRACT, hex_mode);

   /***************************************************************
   *  If left and right columns are backwards, fix them.
   ***************************************************************/
   if (rect_left_col > rect_right_col)
      {
         i = rect_left_col;
         rect_left_col = rect_right_col;
         rect_right_col = i;
      }

   if (COLORED(buffer->token))
      cdpad_add_remove(buffer, top_line,  rect_left_col, rect_left_col - rect_right_col);  /* chars to add (negative) */

   if (rect_left_col > 0)
      {
         strncpy(buff, line, rect_left_col);
         buff[rect_left_col] = '\0';
         if (len > rect_left_col)
            changed = True;
      }
   else
      {
         buff[0] = '\0';
         if (line[0] != '\0')
            changed = True;
      }

   /***************************************************************
   *  If there is something at the end of the line to not cut,
   *  attach it to the end of the work buffer.  The special case of
   *  right_col == left_col causes the line from top col to the end
   *  to be deleted.
   ***************************************************************/
   if (rect_right_col < len && window_left_col != window_right_col)
      strcat(buff, &line[rect_right_col]);

   if (strcmp(buff, line) != 0)
      {
         if (!changed)
            fprintf(stderr, "Changed is wrong (%d) in rect cut for line = %d, left col = %d, right col = %d, len = %d, line = \"%s\"\n",
                            changed, top_line, rect_left_col, rect_right_col, len, line);
         put_line_by_num(buffer->token, top_line, buff, OVERWRITE);
      }
   else
      if (changed)
         fprintf(stderr, "Changed is wrong (%d) in rect cut for line = %d, left col = %d, right col = %d, len = %d, line = \"%s\"\n",
                         changed, top_line, rect_left_col, rect_right_col, len, line);

   line = next_line(buffer->token);
   top_line++;
}

if (COLORED(buffer->token))
   cd_flush(buffer);


} /* end of cut_from_memory_rect */



/************************************************************************

NAME:      dm_undo  -  DM undo command

PURPOSE:    This routine sets up the call to the un_do or re_do and performs the
            cleanup.  un_do and re_do are in undo.c.  redo is a new command.

PARAMETERS:

   1.  dmc           - pointer to DMC (INPUT)
                       This is the command buffer for the undo or redo command
                       it is used to tell which one this is.

   2.  cursor_buff   - pointer to BUFF_DESCR (INPUT / OUTPUT)
                       This is the buffer description which shows the current
                       location of the cursor.


FUNCTIONS :

   1.   If the window is not writable, do nothing.

   2.   Mark the cursor position.  This makes having the command under
        a key and tdm'ing to the command line and typing it equivalent.

   3.   Call un_do or re_do to go do the work.

   4.   If a valid line position was returned, position the cursor in
        col 0 of that line.

   5.   Show we need a full redraw.


RETURNED VALUE:
   redraw -  The window mask anded with the type of redraw needed is returned.



*************************************************************************/

int   dm_undo(DMC               *dmc,                /* input   */
              BUFF_DESCR        *cursor_buff)        /* input / output  */
{
int                   new_line_no = INT_MAX;

if (!WRITABLE(cursor_buff->current_win_buff->token))
   {
     dm_error("Text is read-only", DM_ERROR_BEEP);
     return(0);
   }

if (dmc->any.cmd == DM_undo)
   un_do(cursor_buff->current_win_buff->token, &new_line_no);  /* in undo.c */
else
   re_do(cursor_buff->current_win_buff->token, &new_line_no);  /* in undo.c */
   
DEBUG2(fprintf(stderr, "(%s) position to %d\n", dmsyms[dmc->any.cmd].name, new_line_no); )


/***************************************************************
*  
*  Position the file to the spot specified by undo or redo.
*  
***************************************************************/

if (new_line_no < INT_MAX && new_line_no >= 0)
   (void) dm_position(cursor_buff, cursor_buff->current_win_buff, new_line_no, cursor_buff->current_win_buff->file_col_no);

/***************************************************************
*  
*  Reread the affected line.
*  
***************************************************************/

cursor_buff->current_win_buff->buff_ptr = get_line_by_num(cursor_buff->current_win_buff->token, cursor_buff->current_win_buff->file_line_no);
cursor_buff->up_to_snuff = 0;

return(cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW);

} /* end of dm_undo */



/************************************************************************

NAME:      rectangular_substr  -  Perform a substring function using
                                  rectangular rules.

PURPOSE:    This routine returns the substring of the input string from
            from_col to to_col padding with blanks as necessary.

PARAMETERS:

   1.  in_str     -  pointer to char (INPUT)
                     This is the from which the substring is to be extracted.

   2.  from_col   -  int (INPUT)
                     This is the zero based col number of the first
                     column in the substr.

   3.  to_col     -  int (INPUT)
                     This is the zero based col number of the column just
                     past the last column on the last line to include in the
                     substring.  The lenght of the returned string is always
                     to_col - from_col.

   4.  out_str    -  pointer to char (OUTPUT)
                     This pointer references the target area into which will be
                     placed the substring. It must be at least to_col - from_col
                     long.


FUNCTIONS :

   1.   Get the length of the string.

   2.   If from_col is greater than the length of the string, just copy in the
        appropriate number of blanks.

   3.   If from_col equals to col, this is a special case, we want to copy the
        string from from_col to the end of the string.

   4.   Otherwise, copy characters from in_str to out_str starting at from_col
        and ending when we get to to_col.  If we run out of characters, pad
        with blanks.


*************************************************************************/

static void rectangular_substr(char    *in_str,       /* input  */
                               int       from_col,    /* input  */
                               int       to_col,      /* input  */
                               char     *out_str)     /* output */
{
int       inlen;
int       i;

if (in_str != NULL)
   inlen = strlen(in_str);
else
   inlen = 0;

/***************************************************************
*  
*  If from and to columns are backwards, fix them.
*  
***************************************************************/

if (from_col > to_col)
   {
      i = from_col;
      from_col = to_col;
      to_col = i;
   }


/***************************************************************
*  
*  Check for the case where we are just returning blanks
*  
***************************************************************/

if (from_col >= inlen)
   {
      for (i = from_col; i < to_col; i++)
         *out_str++ = ' ';

      *out_str = '\0';
   }
else
   if (from_col == to_col)
      {
         /***************************************************************
         *  
         *  Special case for zero length width, take from from_col to end
         *  of line.  We already know that from_col is not >= len.
         *  
         ***************************************************************/
         strcpy(out_str, &in_str[from_col]);
      }
   else
      {
         /***************************************************************
         *  
         *  The normal case, take chars from from col to to col.  Watch
         *  out for end of string and pad if needed.
         *  
         ***************************************************************/
         for (i = from_col; i < to_col; i++)
            if (i < inlen)
               *out_str++ = in_str[i];
            else
               *out_str++ = ' ';

         *out_str = '\0';
      }
   
} /* end of rectangular_substr */



/************************************************************************

NAME:      paste_rectangular  -  perform a rectangular paste from a file into a memdata.

PURPOSE:    This routine pastes source from a file into the memdata area in a
            rectangular fashion.

PARAMETERS:

   1.  token      -  pointer to DATA_TOKEN (INPUT / OUTPUT)
                     This is the token for the memdata structure to be pasted into.

   2.  to_line    -  int  (INPUT)
                     This is the target line in the file (zero base) to paste into.

   3.  to_col      - int (INPUT)
                     This is the target column in to_line to paste into (also zero base).

   4.  paste_stream - pointer to FILE (INPUT)
                     This is the open stream with the stuff to paste.

   5.  overstrike  - int (INPUT)
                     If true, the paste is done in overstrike instead of insert mode.


FUNCTIONS :

   1.   Read each line.

   2.   Get rid of any trailing newlines.

   3.   If we are not past the end of the memory file, get the line to paste into.
        Otherwise assume a NULL line.

   4.   Put the source at the correct place in the line.  Insert in buff knows
        how to pad to get the insert in the right place. 

   5.   If we are past end of the memory file, insert the line.  Otherwise overwrite it.


*************************************************************************/

static void  paste_rectangular(DATA_TOKEN     *token,
                               int             to_line,
                               int             to_col,
                               FILE           *paste_stream,
                               int             overstrike)
{
char        insert_buff[MAX_LINE+2];
char        buff2[MAX_LINE+2];
char       *line;
int         overflow = 0;
int         last;

/***************************************************************
*  
*  Read all the lines in the paste buffer and get rid of the
*  newline characters.
*  
***************************************************************/

while (ce_fgets(insert_buff, sizeof(insert_buff),  paste_stream) != NULL)
{
   last = strlen(insert_buff) -1;

   if (insert_buff[last] == '\n')
      insert_buff[last] = '\0';
   
   /***************************************************************
   *  
   *  Get each line to be modified,  The line from the paste buffer
   *  is inserted into the line to be modified.   If we hit end of
   *  the memdata structure, add null lines as needed.
   *  Routine insert_in_buff takes care of padding lines and checking
   *  for overflow conditions.
   *  
   ***************************************************************/

   if (to_line < total_lines(token))
      {
         line = get_line_by_num(token, to_line);
         strcpy(buff2, line);
         overflow |= insert_in_buff(buff2, insert_buff, to_col, !overstrike/* true for insert mode */, MAX_LINE);
         put_line_by_num(token, to_line, buff2, OVERWRITE);
      }
   else
      {
         buff2[0] = '\0';
         overflow |= insert_in_buff(buff2, insert_buff, to_col, !overstrike /* true for insert mode */, MAX_LINE);
         put_line_by_num(token, to_line-1, buff2, INSERT);
      }

   to_line++;
} /* end of while reading lines loop */

if (overflow)
   dm_error("(xp) Line(s) too long", DM_ERROR_BEEP);

} /* end of paste_rectangular */


#ifdef PAD
/************************************************************************

NAME:      paste_vt100  -  perform a paste into a vt100 emulation window

PURPOSE:    This routine handles all pastes when we are in vt100 mode.
            There is no distinction between regular and rectangular
            pastes in this case.  All data is sent to the shell and
            hopefully we are in input mode in vi or the stuff pasted is
            intellegible to the shell.

PARAMETERS:

   1.  main_window - pointer to PAD_DESCR (INPUT)
                     This is the PAD DESCRIPTION for the main window.

   2.  paste_stream - pointer to FILE (INPUT)
                     This is the open stream with the stuff to paste.


FUNCTIONS :

   1.   Read each line.

   2.   Keep the newlines.

   3.   Send each line to the shell.


*************************************************************************/

static void  paste_vt100(PAD_DESCR          *main_window,
                         FILE               *paste_stream)
{
char        insert_buff[MAX_LINE+2];
int         rc;

/***************************************************************
*  
*  Read all the lines in the paste buffer and get rid of the
*  newline characters.
*  
***************************************************************/

while (ce_fgets(insert_buff, sizeof(insert_buff),  paste_stream) != NULL)
{
   rc = vt100_pad2shell(main_window, insert_buff, False);
   if (rc != PAD_SUCCESS)  /* vt100_pad2shell only returns success for failure */
      return;
} /* end of while reading lines loop */


} /* end of paste_vt100 */
#endif

/************************************************************************

NAME:      dm_case  -  Change case of a line of text.

PURPOSE:    This routine changes a marked range of text to all upper case,
            all lower case, or swaps the case.

PARAMETERS:

   1.  dmc      - pointer to DMC (INPUT)
                  This is the DM command structure for the command
                  to be processed.  It contains the flag which says
                  to translate to upper (u), lower (l), or swap (s).
                  Swap is the default.

   2.  mark1      -  pointer to MARK_REGION (INPUT)
                     If set, this is the most current mark.  It is matched
                     with tdm_mark or cursor_buff to get the line range.

   3.  cursor_buff - pointer to BUFF_DESCR (INPUT)
                     This is the buffer description which shows the current
                     location of the cursor.

   4.  echo_mode   - int (INPUT)
                     This is the current echo mode.  If it is set to RECTANGULAR_HIGHLIGHT,
                     we are processing a rectangular region of text.


FUNCTIONS :

   1.   Determine the range of text to process

   2.   Make sure the two points are in the same window.

   3.   Read the lines of text to be processed.

   4.   Change the case of the lines of text as required in the command.
        This is done with routine case_line.


RETURNED VALUE:
   redraw -  If the screen needs to be redrawn, the redraw mask is
             returned.


*************************************************************************/

int   dm_case(DMC               *dmc,                /* input   */
              MARK_REGION       *mark1,              /* input   */
              BUFF_DESCR        *cursor_buff,        /* input   */
              int                echo_mode)          /* input   */
{

int                      top_line;
int                      top_col;
int                      bottom_line;
int                      bottom_col;
PAD_DESCR               *buffer;
int                      rc;
int                      line_changed;
int                      any_change = False;
char                    *line;
char                     work[MAX_LINE+1];
int                      redraw_needed = 0;
char                     msg[100];

/***************************************************************
*  
*  Get the end points of the range of text to process.
*  
***************************************************************/

rc = range_setup(mark1,              /* input   */
                 cursor_buff,        /* input   */
                 &top_line,          /* output  */
                 &top_col,           /* output  */
                 &bottom_line,       /* output  */
                 &bottom_col,        /* output  */
                 &buffer);           /* output  */

/***************************************************************
*  Make sure the range is in the same window by checking rc
***************************************************************/
if (rc == 0)
   {

      DEBUG2(fprintf(stderr, "(%s) in %s from : [%d,%d]  to : [%d,%d]\n",
             dmsyms[dmc->xc.cmd].name,  which_window_names[buffer->which_window],
             top_line, top_col, bottom_line, bottom_col);
      )

      /***************************************************************
      *  Get the first line to process.
      ***************************************************************/
      position_file_pointer(buffer->token, top_line);
      line = next_line(buffer->token);


      if (line != NULL)
         if (top_line == bottom_line)
            {
               /***************************************************************
               *  Special case for one line operations.
               ***************************************************************/
               strcpy(work, line);
               line_changed = case_line(dmc->d_case.func, work, top_col, bottom_col);
               if (line_changed)
                  {
                     put_line_by_num(buffer->token, top_line, work, OVERWRITE);
                     any_change = True;
                  }
            }
         else
            if (echo_mode != RECTANGULAR_HIGHLIGHT)
               {
                  /***************************************************************
                  * 
                  *           NORMAL RANGE OF TEXT
                  * 
                  *  First line. top_col to end of line
                  * 
                  ***************************************************************/
                  strcpy(work, line);
                  line_changed = case_line(dmc->d_case.func, work, top_col, MAX_LINE);
                  if (line_changed)
                     {
                        put_line_by_num(buffer->token, top_line, work, OVERWRITE);
                        any_change = True;
                     }

                  top_line++;
      
                  /***************************************************************
                  *  Middle lines. 
                  ***************************************************************/
                  while (top_line < bottom_line)
                  {
                     line = next_line(buffer->token);
                     if (line == NULL)
                        break;

                     strcpy(work, line);
                     line_changed = case_line(dmc->d_case.func, work, 0, MAX_LINE);
                     if (line_changed)
                        {
                           put_line_by_num(buffer->token, top_line, work, OVERWRITE);
                           any_change = True;
                        }
      
                     top_line++;
                  }
      
                  /***************************************************************
                  *  last_line. Stop at bottom_col
                  ***************************************************************/
                  line = next_line(buffer->token);
                  if (line != NULL && bottom_col != 0)
                     {
                        strcpy(work, line);
                        line_changed = case_line(dmc->d_case.func, work, 0, bottom_col);
                        if (line_changed)
                           {
                              put_line_by_num(buffer->token, bottom_line, work, OVERWRITE);
                              any_change = True;
                           }
                     }
                  else
                     DEBUG2(   fprintf(stderr, "case: Last line ends at col 0\n");)
               } /* end of normal text range */
            else
               {
                  /***************************************************************
                  * 
                  *              RECTANGULAR RANGE OF TEXT
                  *  
                  *  Rectangular region.  The buffer is already primed.  Get the
                  *  The required piece of text, cutting and padding as needed
                  *  and write it out.  Always put on a newline.
                  *  
                  ***************************************************************/

                  while(top_line <= bottom_line)
                  {
                     strcpy(work, line);
                     if (top_col != bottom_col)
                        line_changed = case_line(dmc->d_case.func, work, top_col, bottom_col);
                     else
                        line_changed = case_line(dmc->d_case.func, work, top_col, MAX_LINE);

                     if (line_changed)
                        {
                           put_line_by_num(buffer->token, top_line, work, OVERWRITE);
                           any_change = True;
                        }
                     top_line++;
                     line = next_line(buffer->token);
                     if (line == NULL)
                        break;

                  }
               }  /* end of rectangular case */

   }
else
   {
      snprintf(msg, sizeof(msg), "(%s) Invalid Text Range", dmsyms[dmc->d_case.cmd].name);
      dm_error(msg, DM_ERROR_BEEP);
   }

/***************************************************************
*  Clear the mark and turn off echo mode.
***************************************************************/
stop_text_highlight(mark1->buffer->display_data);
mark1->mark_set = 0;

if (any_change)
   redraw_needed = (buffer->redraw_mask & FULL_REDRAW);

return(redraw_needed);

} /* end of dm_case */


/************************************************************************

NAME:      case_line  -  Change the case of one line of text.

PURPOSE:    This routine changes a passed line or part of it from upper to lower,
            lower to upper, or swap  case.

PARAMETERS:

   1.  func       -  char (INPUT)
                     This is the type of case change to make.
                     values:  l  -  change all UPPER case to lower case.
                              u  -  change all lower case to UPPER case.
                              s  -  change all lower to UPPER and all UPPER to lower.

   2.  work       -  pointer to char (INPUT / OUTPUT)
                     This is the string to operate on.

   3.  from_col    - int (INPUT)
                     This is the first columm (zero base) to start processing.

   4.  to_col      - int (INPUT)
                     This is the end column.  Characters up to but not including
                     this column are processed.


FUNCTIONS :

   1.   Make sure there is something to do (start is not past end of line)

   2.   Determine if this is to be a convert to lower, upper, or swap

   3.   Walk the string making the conversions in place.


RETURNED VALUE:
   changed -  This flag indicates whether changes were made.
              0  -  no changes made
              1  -  one or more changes made.


*************************************************************************/

int case_line(char    func,
              char   *work,
              int     from_col,
              int     to_col)
{

int                      any_change = False;
char                    *p;
char                    *end;
int                      len;

/***************************************************************
*  
*  Make sure there is something to do.
*  
***************************************************************/

len = strlen(work);
if (from_col > len)
   return(0);

/***************************************************************
*  
*  If to_col is past the end of the line, stop at the end of line.
*  
***************************************************************/

if (len < to_col)
   to_col = len;


/***************************************************************
*  
*  Determine whether we are doing a change to lower case, a
*  change to upper case, or a swap case.  The func letters are
*  always lower case, parse_dm saw to that.
*  
***************************************************************/

if (func == 'l')
   {
      end = &work[to_col];
      for (p = &work[from_col]; p < end; p++)
         if (isupper(*p))
            {
               any_change = True;
               *p |= 0x20; /* convert upper case to lower case for this char */
            }
   }
else
   if (func == 'u')
      {
         end = &work[to_col];
         for (p = &work[from_col]; p < end; p++)
            if (islower(*p))
               {
                  any_change = True;
                  *p &= 0xdf; /* convert lower case to upper case for this char */
               }
      }
   else
      {
         /* -s swap case - the default */
         end = &work[to_col];
         for (p = &work[from_col]; p < end; p++)
            if (islower(*p))
               {
                  any_change = True;
                  *p &= 0xdf; /* convert lower case to upper case for this char */
               }
            else
               if (isupper(*p))
                  {
                     any_change = True;
                     *p |= 0x20; /* convert upper case to lower case for this char */
                  }
      }

return(any_change);

} /* end of case_line */


/************************************************************************

NAME:      dm_xl  -  DM Copy liternal command to a paste buffer (xl)

PURPOSE:    This routine copies data to a paste buffer.  It differs from
            xc in that the data to be put in the buffer is supplied by
            the command.

PARAMETERS:

   1.  dmc        -  pointer to DMC (INPUT)
                     This is the DM command structure for the command
                     to be processed.

   2.  main_window - pointer to DRAWABLE_DESCR (INPUT)
                     This is the drawable description for the main window.  It is
                     needed by open_paste_buffer and is passed through.

   3.  timestamp   - Time (INPUT - X lib type)
                     This is the timestamp from the keyboard or mouse event which
                     caused the xc or xd.  It is needed by open_paste_buffer
                     when dealing with the X paste buffers.

   4.  escape_char - int (INPUT)
                     This is the escape character.
                     It is an '@' or a '\'

FUNCTIONS :

   1.   Open the paste buffer

   2.   Copy the data to the paste buffer.


*************************************************************************/

void  dm_xl(DMC               *dmc,                /* input   */
            Display           *display,
            Window             main_x_window,
            Time               timestamp,          /* input - X lib type */
            char               escape_char)        /* input   */
{
int               j;
int               i;
FILE             *paste_stream;


/***************************************************************
*  Open the paste buffer and size it for the data.
***************************************************************/
paste_stream = open_paste_buffer(dmc, display, main_x_window, timestamp, escape_char);
if (paste_stream == NULL)
   return;  /* msg already sent */

if (X_PASTE_STREAM(paste_stream))
   {
      if (dmc->xl.data)
         j = strlen(dmc->xl.data) + 1;
      else
         j = 1;
      i = setup_x_paste_buffer(paste_stream, NULL, 1, 1, j);
      if (i != 0)
         return;  /* msg already sent */
   }

/***************************************************************
*  Copy the liternal string to the buffer.  If -r was specified,
*  add a newline.
***************************************************************/

ce_fputs(dmc->xl.data, paste_stream);
if (dmc->xl.base.dash_r)
    ce_fputs("\n", paste_stream);


ce_fclose(paste_stream);


} /* end of dm_xl */


/************************************************************************

NAME:      dm_xa  -  Paste buffer append (xa)

PURPOSE:    This routine appends the contents of one paste buffer to
            another.

PARAMETERS:
   1.  dmc        -  pointer to DMC (INPUT)
                     This is the DM command structure for the command
                     to be processed.

   2.  main_window - pointer to DRAWABLE_DESCR (INPUT)
                     This is the drawable description for the main window.  It is
                     needed by open_paste_buffer and is passed through.

   3.  timestamp   - Time (INPUT - X lib type)
                     This is the timestamp from the keyboard or mouse event which
                     caused the xc or xd.  It is needed by open_paste_buffer
                     when dealing with the X paste buffers.

   4.  escape_char - int (INPUT)
                     This is the escape character.
                     It is an '@' or a '\'

FUNCTIONS :

   1.   Open the paste buffer paste buffer we are going to read

   2.   Open the paste buffer paste buffer we are going to write
        in append mode.

   3.   Copy the data


*************************************************************************/

void  dm_xa(DMC               *dmc,                /* input   */
            Display           *display,
            Window             main_x_window,
            Time               timestamp,          /* input - X lib type */
            char               escape_char)        /* input   */
{
int               i;
FILE             *read_paste_stream;
FILE             *write_paste_stream;
DMC               work_dmc;
char        insert_buff[MAX_LINE+2];


DEBUG2(fprintf(stderr, "(%s) from : %s  to %s\n",
       dmsyms[dmc->xa.cmd].name, dmc->xa.path2, dmc->xa.path1);
)
/***************************************************************
*  Open the second paste buffer for reading
***************************************************************/
memset((char *)&work_dmc, 0, sizeof(work_dmc));
work_dmc.xp.cmd    = DM_xp;
work_dmc.xp.dash_f = dmc->xa.dash_f2;
work_dmc.xp.path   = dmc->xa.path2;

read_paste_stream = open_paste_buffer(&work_dmc, display, main_x_window, timestamp, escape_char);
if (read_paste_stream == NULL)
   return;  /* msg already sent */

/***************************************************************
*  Open the first paste buffer for writing, append mode
***************************************************************/
memset((char *)&work_dmc, 0, sizeof(work_dmc));
work_dmc.xc.cmd    = DM_xc;
work_dmc.xc.dash_f = dmc->xa.dash_f1;
work_dmc.xc.dash_l = dmc->xa.dash_l;
work_dmc.xc.dash_a = 'a';
work_dmc.xc.path   = dmc->xa.path1;

write_paste_stream = open_paste_buffer(&work_dmc, display, main_x_window, timestamp, escape_char);
if (write_paste_stream == NULL)
   return;  /* msg already sent */

if (X_PASTE_STREAM(write_paste_stream))
   {
      i = setup_x_paste_buffer(write_paste_stream, NULL, 1, 1, get_pastebuf_size(read_paste_stream)+1);
      if (i != 0)
         return;  /* msg already sent */
   }

/***************************************************************
*  Copy the liternal string to the buffer.  If -r was specified,
*  add a newline.
***************************************************************/

while (ce_fgets(insert_buff, sizeof(insert_buff),  read_paste_stream) != NULL)
   ce_fputs(insert_buff, write_paste_stream);

ce_fclose(read_paste_stream);
ce_fclose(write_paste_stream);


} /* end of dm_xa */




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

/*static char *sccsid = "%Z% %M% %I% - %G% %U% ";*/
/***************************************************************
*
*  module mark.c
*
*  Routines:
*     dm_tdm              -   Move cursor to the dm window. Leave mark
*     dm_tdmo             -   Move to the DM output window
*     dm_tmw              -   Move to the main window
*     dm_dr               -   DM dr (define region) command
*     dm_gm               -   Go to the current mark.
*     mark_point_setup    -   Get a position from marks.
*     dm_comma            -   Comma processing for 1,$s etc.
*     range_setup         -   Turn MARK_REGION's into top and bottom rows and cols.
*     toggle_switch       -   Flip Flop a DM_ON, DM_OFF, DM_TOGGLE flag
*     check_and_add_lines -   Add padding lines if neccesary because of past eof or form feeds
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <stdlib.h>         /* /usr/include/stdlib.h     */
#include <errno.h>          /* /usr/include/errno.h   /usr/include/sys/errno.h      */
#include <limits.h>         /* /usr/include/limits.h     */


#include "borders.h"
#include "debug.h"
#include "dmc.h"
#include "dmsyms.h"
#include "dmwin.h"
#include "getevent.h"
#include "mark.h"
#include "memdata.h"
#include "parsedm.h"
#include "mvcursor.h"
#include "redraw.h"
#include "tab.h"
#include "txcursor.h"
#include "window.h"
#include "xerrorpos.h"

/************************************************************************

NAME:      dm_tdm  -  move to the DM window

PURPOSE:    This routine moves the cursor to the DM window and
            establishes a mark in the window it was in.  This mark
            is returned to when enter is pressed.

PARAMETERS:

   1.  tdm_mark      - pointer to MARK_REGION (OUTPUT)
                       This is the mark to be set.
                       If this parm is NULL, no mark is set.

   2.  cursor_buff   - pointer to BUFF_DESCR (INPUT / OUTPUT)
                       This is the current cursor position.
                       It is used to set the mark and then
                       updated to point to the DM window.

   3.  dminput_cur_descr - pointer to BUFF_DESCR (INPUT)
                       This is the buffer description for the
                       DM input window.  It is used to set the
                       new position.

   4.  main_window_cur_descr - pointer to BUFF_DESCR (INPUT)
                       This is the buffer description for the
                       Main window.  It is used for comparison
                       purposes.  The mark only takes place
                       if the cursor was in the main window.



FUNCTIONS :

   1.   If the cursor was in the main window, copy the mark data
        from the current cursor position to the tdm mark.

   2.   Set the current buffer position to the DM input window.

   3.   If the dminput window was off screen, resize the window.


NOTES:
   1.   After this call, an XWarpPointer call is needed.

RETURNED VALUE:
   window_changed  -  int
                      True  -  The window was moved or resized and a configurenotify
                               is forthcoming.  We will want to fix up the cursor.
                      False -  The window was not resized or moved.


*************************************************************************/

int   dm_tdm(MARK_REGION  *tdm_mark,
             BUFF_DESCR   *cursor_buff,
             PAD_DESCR    *dminput_cur_descr,
             PAD_DESCR    *main_pad)
{
int          x;
int          y;
int          width;
int          height;
int          off_by;
int          display_height;
int          need_resize = False;

/***************************************************************
*  
*  Unless we are already in the DM input window, set the mark
*  as to where we are.  If we are past the last line in the file,
*  adjust to the last line in the file.
*  
***************************************************************/
if ((tdm_mark != NULL) && (cursor_buff->current_win_buff->which_window != DMINPUT_WINDOW))
   {
      tdm_mark->mark_set = True;
      tdm_mark->file_line_no = cursor_buff->current_win_buff->file_line_no;
      tdm_mark->col_no = cursor_buff->current_win_buff->file_col_no;
      tdm_mark->buffer = cursor_buff->current_win_buff;

      DEBUG2(fprintf(stderr, "tdm: TDM Mark set to [%d,%d]\n", tdm_mark->file_line_no, tdm_mark->col_no);)


   }

/***************************************************************
*  
*  Move to the DM input window.
*  
***************************************************************/

cursor_buff->which_window     = dminput_cur_descr->which_window;
cursor_buff->current_win_buff = dminput_cur_descr;
cursor_buff->win_line_no      = 0;
cursor_buff->y                = dminput_cur_descr->window->height / 2;
/* if the prompt is too long, scroll the prompt */
if ((cursor_buff->current_win_buff->window->sub_x+cursor_buff->current_win_buff->window->font->max_bounds.width) >= (int)dminput_cur_descr->window->width)
   {
      scroll_dminput_prompt(strlen(get_dm_prompt())-2);
      DEBUG2(fprintf(stderr, "Scrolling DM input prompt to get end of prompt on screen\n");)
   }
set_window_col_from_file_col(cursor_buff);

/***************************************************************
*  
*  Make sure we are on top.
*  
***************************************************************/

if (window_is_obscured(cursor_buff->current_win_buff->display_data)) /* in window.c */
   {
      DEBUG2(fprintf(stderr, "tdm: Raising window\n");)
      DEBUG9(XERRORPOS)
      XRaiseWindow(main_pad->display_data->display, main_pad->x_window);
   }

/***************************************************************
*  
*  Check if the prompt fills the whole dminput window, if so, scroll it.
*  Then check if the window is too small.  If so, make it bigger.
*  
***************************************************************/

x      =  main_pad->window->x;
y      =  main_pad->window->y;
height =  main_pad->window->height;
width  =  main_pad->window->width;

if ((int)(cursor_buff->x+cursor_buff->current_win_buff->window->font->max_bounds.width) >= (int)(dminput_cur_descr->window->width+dminput_cur_descr->window->x))
   {
      need_resize = True;
      width  =  ((cursor_buff->x+cursor_buff->current_win_buff->window->font->max_bounds.width) * 2) + (MARGIN * 3);
      DEBUG2(fprintf(stderr, "Resizing to make dminput window wider, from %d to %d\n", main_pad->window->width, width);)
   }

/***************************************************************
*  
*  If the dm input window is off the screen resize it on.
*  
***************************************************************/

display_height = DisplayHeight(main_pad->display_data->display, DefaultScreen(main_pad->display_data->display));
off_by = (y + dminput_cur_descr->window->y  + dminput_cur_descr->window->height) - display_height;
if (off_by > 0)
   {
      off_by += (main_pad->window->line_height * 3); /* fudge factor to make sure the window is completely on the screen */
      need_resize = True;
      if (off_by < y)
         {
            y -= off_by;
            off_by = 0;
            DEBUG2(fprintf(stderr, "Sliding window up to get dminput window on screen old y: %d, new y %d\n", main_pad->window->y, y);)
         }
      else
         if (y != 0)
            {
               y = 0;
               off_by -= y;
               DEBUG2(fprintf(stderr, "Sliding window to top to get dminput window on screen old y: %d, new y %d\n", main_pad->window->y, y);)
            }
      if (off_by > 0)
         {
            height -= off_by;
            DEBUG2(fprintf(stderr, "Shortening window up to get dminput window on screen old height: %d, new height %d\n", main_pad->window->height, height);)
         }
   }

off_by = (x + dminput_cur_descr->window->sub_x + dminput_cur_descr->window->x);
if (off_by < 0)
   {
      need_resize = True;
      x = 0;
      DEBUG2(fprintf(stderr, "Sliding window right to get dminput window on screen old x: %d, new x %d\n", main_pad->window->x, x);)
   }
off_by -= DisplayWidth(main_pad->display_data->display, DefaultScreen(main_pad->display_data->display));
if (off_by > 0)
   {
      need_resize = True;
      if (off_by < x)
         {
            x -= off_by;
            off_by = 0;
            DEBUG2(fprintf(stderr, "Sliding window left to get dminput window input area on screen old x: %d, new x %d\n", main_pad->window->x, x);)
         }
      else
         if (x != 0)
            {
               x = 0;
               off_by -= x;
               DEBUG2(fprintf(stderr, "Sliding window to left edge to get dminput window input area on screen old x: %d, new x %d\n", main_pad->window->x, x);)
            }
      if (off_by > 0)
         {
            width -= off_by;
            DEBUG2(fprintf(stderr, "Narrowing window up to get dminput window input area on screen old width: %d, new width %d\n", main_pad->window->width, width);)
         }
   }

if (need_resize)
   {
      /***************************************************************
      *  To avoid blowing up Hummingbird Exceed, unmap the windows
      *  before we call XMoveResizeWindow
      *  Changed 9/7/2001 by R. E. Styma
      ***************************************************************/
      DEBUG9(XERRORPOS)
      if (main_pad->display_data->pad_mode)
         XUnmapWindow(main_pad->display_data->display, main_pad->display_data->unix_pad->x_window);
      DEBUG9(XERRORPOS)
      XUnmapWindow(main_pad->display_data->display, main_pad->display_data->dminput_pad->x_window);
      DEBUG9(XERRORPOS)
      XUnmapWindow(main_pad->display_data->display, main_pad->display_data->dmoutput_pad->x_window);
      DEBUG9(XERRORPOS)
      if (main_pad->display_data->sb_data->vert_window != None)
         XUnmapWindow(main_pad->display_data->display, main_pad->display_data->sb_data->vert_window);
      DEBUG9(XERRORPOS)
      if (main_pad->display_data->sb_data->horiz_window != None)
         XUnmapWindow(main_pad->display_data->display, main_pad->display_data->sb_data->horiz_window);
      DEBUG9(XERRORPOS)
      if (main_pad->display_data->pd_data->menubar_x_window != None)
         XUnmapWindow(main_pad->display_data->display, main_pad->display_data->pd_data->menubar_x_window);

#ifdef WIN32
      DEBUG9(XERRORPOS)
      XUnmapWindow(main_pad->display_data->display, main_pad->display_data->main_pad->x_window);
#endif
      XFlush(main_pad->display_data->display);

      DEBUG9(XERRORPOS)
      XMoveResizeWindow(main_pad->display_data->display, main_pad->x_window, x, y, width, height);

#ifdef WIN32
      XMapWindow(main_pad->display_data->display, main_pad->display_data->main_pad->x_window);
#endif
   }

return(need_resize);

} /* end of dm_tdm */


/************************************************************************

NAME:      dm_tdmo  -  move to the DM output window

PURPOSE:    This routine moves the cursor to the DM window.

PARAMETERS:

   1.  cursor_buff   - pointer to BUFF_DESCR (INPUT / OUTPUT)
                       This is the current cursor position.
                       It is used to set the mark and then
                       updated to point to the DM window.

   2.  dmoutput_cur_descr - pointer to BUFF_DESCR (INPUT)
                       This is the buffer description for the
                       DM input window.  It is used to set the
                       new position.

   3.  main_window_cur_descr - pointer to BUFF_DESCR (INPUT)
                       This is the buffer description for the
                       Main window.  It is used for comparison
                       purposes.



FUNCTIONS :

   1.   Move the cursor to the DM output window.

   2.   If the dmoutput window was obscured, raise the window.

   3.   If the dmoutput window was off screen, resize the window.


NOTES:
   1.   After this call, an XWarpPointer call is needed.
        

RETURNED VALUE:
   window_changed  -  int
                      True  -  The window was moved or resized and a configurenotify
                               is forthcoming.  We will want to fix up the cursor.
                      False -  The window was not resized or moved.

*************************************************************************/

int  dm_tdmo(BUFF_DESCR   *cursor_buff,
             PAD_DESCR    *dmoutput_cur_descr,
             PAD_DESCR    *main_pad)
{
int          x;
int          y;
int          width;
int          height;
int          off_by;
int          display_height;
int          need_resize = False;

/***************************************************************
*  
*  Move to the DM output window.
*  
***************************************************************/

cursor_buff->which_window     = dmoutput_cur_descr->which_window;
cursor_buff->current_win_buff = dmoutput_cur_descr;
cursor_buff->win_line_no      = 0;
cursor_buff->y                = dmoutput_cur_descr->window->height / 2;
set_window_col_from_file_col(cursor_buff);

/***************************************************************
*  
*  Make sure we are on top.
*  
***************************************************************/

if (window_is_obscured(cursor_buff->current_win_buff->display_data)) /* in window.c */
   {
      DEBUG2(fprintf(stderr, "tdmo: Raising window\n");)
      DEBUG9(XERRORPOS)
      XRaiseWindow(main_pad->display_data->display, main_pad->x_window);
   }

/***************************************************************
*  
*  Check if the window is too small.  If so, make it bigger.
*  
***************************************************************/

x      =  main_pad->window->x;
y      =  main_pad->window->y;
height =  main_pad->window->height;
width  =  main_pad->window->width;

if ((int)(cursor_buff->x+cursor_buff->current_win_buff->window->font->max_bounds.width) >= (int)(dmoutput_cur_descr->window->width+dmoutput_cur_descr->window->x))
   {
      need_resize = True;
      width  =  ((cursor_buff->x+cursor_buff->current_win_buff->window->font->max_bounds.width) * 2) + (MARGIN * 3);
      DEBUG2(fprintf(stderr, "Resizing to make dmoutput window wider, from %d to %d\n", main_pad->window->width, width);)
   }

/***************************************************************
*  
*  If the dm output window is off the screen resize it on.
*  
***************************************************************/

display_height = DisplayHeight(main_pad->display_data->display, DefaultScreen(main_pad->display_data->display));
off_by = (y + dmoutput_cur_descr->window->y  + dmoutput_cur_descr->window->height) - display_height;
if (off_by > 0)
   {
      need_resize = True;
      if (off_by < y)
         {
            y -= off_by;
            off_by = 0;
            DEBUG2(fprintf(stderr, "Sliding window up to get dmoutput window on screen old y: %d, new y %d\n", main_pad->window->y, y);)
         }
      else
         if (y != 0)
            {
               y = 0;
               off_by -= y;
               DEBUG2(fprintf(stderr, "Sliding window to top to get dmoutput window on screen old y: %d, new y %d\n", main_pad->window->y, y);)
            }
      if (off_by > 0)
         {
            height -= off_by;
            DEBUG2(fprintf(stderr, "Shortening window up to get dmoutput window on screen old height: %d, new height %d\n", main_pad->window->height, height);)
         }
   }

off_by = (x + dmoutput_cur_descr->window->sub_x + dmoutput_cur_descr->window->x);
if (off_by < 0)
   {
      need_resize = True;
      x = 0;
      DEBUG2(fprintf(stderr, "Sliding window right to get dmoutput window on screen old x: %d, new x %d\n", main_pad->window->x, x);)
   }
off_by -= DisplayWidth(main_pad->display_data->display, DefaultScreen(main_pad->display_data->display));
if (off_by > 0)
   {
      need_resize = True;
      if (off_by < x)
         {
            x -= off_by;
            off_by = 0;
            DEBUG2(fprintf(stderr, "Sliding window left to get dmoutput window on screen old x: %d, new x %d\n", main_pad->window->x, x);)
         }
      else
         if (x != 0)
            {
               x = 0;
               off_by -= x;
               DEBUG2(fprintf(stderr, "Sliding window to left edge to get dmoutput window on screen old x: %d, new x %d\n", main_pad->window->x, x);)
            }
      if (off_by > 0)
         {
            width -= off_by;
            DEBUG2(fprintf(stderr, "Narrowing window up to get dmoutput window on screen old width: %d, new width %d\n", main_pad->window->width, width);)
         }
   }

if (need_resize)
   {
      /***************************************************************
      *  To avoid blowing up Hummingbird Exceed, unmap the windows
      *  before we call XMoveResizeWindow
      *  Changed 9/7/2001 by R. E. Styma
      ***************************************************************/
      DEBUG9(XERRORPOS)
      if (main_pad->display_data->pad_mode)
         XUnmapWindow(main_pad->display_data->display, main_pad->display_data->unix_pad->x_window);
      DEBUG9(XERRORPOS)
      XUnmapWindow(main_pad->display_data->display, main_pad->display_data->dminput_pad->x_window);
      DEBUG9(XERRORPOS)
      XUnmapWindow(main_pad->display_data->display, main_pad->display_data->dmoutput_pad->x_window);
      DEBUG9(XERRORPOS)
      if (main_pad->display_data->sb_data->vert_window != None)
         XUnmapWindow(main_pad->display_data->display, main_pad->display_data->sb_data->vert_window);
      DEBUG9(XERRORPOS)
      if (main_pad->display_data->sb_data->horiz_window != None)
         XUnmapWindow(main_pad->display_data->display, main_pad->display_data->sb_data->horiz_window);
      DEBUG9(XERRORPOS)
      if (main_pad->display_data->pd_data->menubar_x_window != None)
         XUnmapWindow(main_pad->display_data->display, main_pad->display_data->pd_data->menubar_x_window);

#ifdef WIN32
      DEBUG9(XERRORPOS)
      XUnmapWindow(main_pad->display_data->display, main_pad->display_data->main_pad->x_window);
#endif
      XFlush(main_pad->display_data->display);

      DEBUG9(XERRORPOS)
      XMoveResizeWindow(main_pad->display_data->display, main_pad->x_window, x, y, width, height);

#ifdef WIN32
      XMapWindow(main_pad->display_data->display, main_pad->display_data->main_pad->x_window);
#endif
   }

return(need_resize);

} /* end of dm_tdmo */


/************************************************************************

NAME:      dm_tmw   -   Move to the main window

PURPOSE:    This routine moves the cursor to the main  window.

PARAMETERS:

   1.  cursor_buff   - pointer to BUFF_DESCR (INPUT / OUTPUT)
                       This is the current cursor position.
                       It is used to set the mark and then
                       updated to point to the DM window.

   2.  main_window_cur_descr - pointer to BUFF_DESCR (INPUT)
                       This is the buffer description for the
                       Main window.    It is used to set the
                       new position.



FUNCTIONS :

   1.   Move the cursor to the main window.


NOTES:
   1.   After this call, an XWarpPointer call is needed.
        

*************************************************************************/

int  dm_tmw(BUFF_DESCR   *cursor_buff,
            PAD_DESCR    *main_window_cur_descr)
{
int          redraw_needed;

/* statement added 6/21/95 */
if ((main_window_cur_descr->file_line_no < main_window_cur_descr->first_line) ||
    (main_window_cur_descr->file_line_no > main_window_cur_descr->first_line+main_window_cur_descr->window->lines_on_screen))
   {
      main_window_cur_descr->file_line_no = main_window_cur_descr->first_line;
      main_window_cur_descr->file_col_no  = main_window_cur_descr->first_char;
   }

redraw_needed = dm_position(cursor_buff, main_window_cur_descr, main_window_cur_descr->file_line_no, main_window_cur_descr->file_col_no);

return(redraw_needed);

} /* end of dm_tmw */



/************************************************************************

NAME:      dm_dr  -  Define Region command

PURPOSE:    This routine will set a mark in a any of the windows controlled
            by the program.

PARAMETERS:

   1.  mark1         - pointer to MARK_REGION (OUTPUT)
                       This is the mark to be set.

   2.  cursor_buff   - pointer to BUFF_DESCR (INPUT)
                       This is a pointer to the buffer / window description
                       showing the current state of the window which contains
                       the cursor.  A mark is set in this window.

   3.  echo_mode     - int (INPUT)
                       This flag shows if echo mode is on.  If so, it is turned off.
                       This handles the case of doing a "dr;echo", moving the cursor
                       and doing another "dr;echo". 


FUNCTIONS :

   1.   Copy the position information from the current cursor buff.
        and flag that the mark is set.

   2.   If the mark was set before column 0, shift it over.

   3.   If echo mode is on, stop it and erase any highlight.


*************************************************************************/

void  dm_dr(MARK_REGION       *mark1,             /* output */
            BUFF_DESCR        *cursor_buff,       /* input  */
            int                echo_mode)         /* input  */
{
int              lines_added;

/* dr where we are */
mark1->mark_set     = True;
mark1->file_line_no = cursor_buff->current_win_buff->file_line_no;
mark1->col_no       = cursor_buff->current_win_buff->file_col_no;
mark1->buffer       = cursor_buff->current_win_buff;
mark1->corner_row   = cursor_buff->current_win_buff->first_line;
mark1->corner_col   = cursor_buff->current_win_buff->first_char;
/***************************************************************
*  The mark must be in the text.  If it is too low, make it
*  be column zero.
***************************************************************/
if (mark1->col_no < 0)
   mark1->col_no = 0;

/***************************************************************
*  If the mark is in the area beyond the end of file or in the
*  area past a form feed, add the required lines.  If the pad
*  is not writable, move the mark to the last available line.
***************************************************************/
lines_added = check_and_add_lines(cursor_buff);
if (!WRITABLE(cursor_buff->current_win_buff->token))
   mark1->file_line_no -= lines_added;
if (mark1->file_line_no < 0)
   mark1->file_line_no = 0;


DEBUG2(fprintf(stderr, "dr: Mark set:  File [%d,%d] in %s\n",
                        mark1->file_line_no, mark1->col_no, which_window_names[cursor_buff->which_window]);
)

if (echo_mode)
   {
      stop_text_highlight(mark1->buffer->display_data);
      highlight_setup(mark1, echo_mode);
   }

#ifdef PAD
if (*cursor_buff->current_win_buff->vt100_mode && (cursor_buff->which_window == MAIN_PAD))
   {
      *cursor_buff->current_win_buff->vt100_mode = False;
      set_event_masks(mark1->buffer->display_data, True);  /* turn the mouse on in the main window */
      *cursor_buff->current_win_buff->vt100_mode = True;
   }
#endif


} /* end of dm_dr */



/************************************************************************

NAME:      dm_gm  -  Go to mark

PURPOSE:    This routine will go to a mark

PARAMETERS:

   1.  mark1         - pointer to MARK_REGION (OUTPUT)
                       This is the mark to be set.

   2.  cursor_buff   - pointer to BUFF_DESCR (INPUT / OUTPUT)
                       This is a pointer to the buffer / window description
                       showing the current state of the window.  It is used
                       to note where we currently are.


FUNCTIONS :

   1.   If the mark1 is set, remember its data.  Otherwise use the
        current cursor location (or tdm mark) to get the point.

   2.   Define a mark where we are now.

   3.   Move the cursor to the target point.


*************************************************************************/

int   dm_gm(MARK_REGION       *mark1,
            BUFF_DESCR        *cursor_buff)
{
int                      from_line;
int                      from_col;
PAD_DESCR               *from_buffer;
int                      redraw_needed;

/***************************************************************
*  Get the spot we will go to.
***************************************************************/
from_line   = mark1->file_line_no;
from_col    = mark1->col_no;
from_buffer = mark1->buffer;
if (from_buffer == NULL)
  return(0);

DEBUG2(fprintf(stderr, "gm: Going to [%d,%d] in %s\n", from_line, from_col, which_window_names[from_buffer->which_window]);)

/***************************************************************
* Set the mark where we are.
***************************************************************/
dm_dr(mark1, cursor_buff, 0);

/***************************************************************
* Move to the target.
***************************************************************/

redraw_needed = dm_position(cursor_buff, from_buffer, from_line, from_col);
return(redraw_needed);

} /* end of dm_gm */



/************************************************************************

NAME:      mark_point_setup

PURPOSE:    This routine gets the current cursor position in the file either
            from the tdm (to dm) mark or the current cursor position.
            This permits dm commands executed from a keystroke and DM commands
            entered in the DM window to behave the same way.  That is, if 
            a command such as xp is put under a keystroke, it executes against the current
            cursor position.  If a tdm is executed (under a key) and then xp
            is typed in the DM input window and enter pressed, the paste should
            act against the cursor position where the tdm was executed.  This routine
            resolves this situation so commands such as xp do not have to be aware
            of the two cases.

PARAMETERS:

   1.  tdm_mark   -  pointer to MARK_REGION (INPUT)
                     This is the current TDM mark.  That is, the mark which
                     was set the last time a tdm was executed.  It is cleared
                     when cursor motion is detected in the main window.

   2.  cursor_buff - pointer to BUFF_DESCR (INPUT)
                     This is the buffer description which shows the current
                     location of the cursor.

   3.  from_line   - pointer to int (OUTPUT)
                     Into this parm is placed the line number with respect to
                     the beginning of the file.

   4.  from_col    - pointer to int (OUTPUT)
                     Into this parm is placed the column number in the file where
                     the mark or cursor is.

   5.  window_buff - pointer to pointer to PAD_DESCR (OUTPUT)
                     Into this parm is placed the pointer to the buffer description
                     for the window the mark or cursor is in.


FUNCTIONS :

   1.   If the tdm mark is set, copy the values from that mark.,

   2.   Fill in the returned parms from the current cursor position.



*************************************************************************/

void  mark_point_setup(MARK_REGION       *tdm_mark,           /* input  */
                       BUFF_DESCR        *cursor_buff,        /* input  */
                       int               *from_line,          /* output */
                       int               *from_col,           /* output */
                       PAD_DESCR        **window_buff)        /* output */
{

if (tdm_mark != NULL && tdm_mark->mark_set)
   {
      *from_line   = tdm_mark->file_line_no;
      *from_col    = tdm_mark->col_no;
      *window_buff = tdm_mark->buffer;
   } /* end of tdm mark set */
else
   {  /* use where we are */
      *from_line   = cursor_buff->current_win_buff->file_line_no;
      if (cursor_buff->win_col_no > 0)  /* is this any good ? */
         *from_col = cursor_buff->current_win_buff->file_col_no;
      else
         *from_col = cursor_buff->current_win_buff->first_char;
      *window_buff = cursor_buff->current_win_buff;
   } /* end of tdm mark NOT set */
} /* end of mark_point_setup */





/************************************************************************

NAME:      range_setup  -  Turn the marks into a range of text. 

PURPOSE:    This routine takes one or two marks and delimits a range
            of text to be acted upon.  The range end points are sorted
            from top (lower numbers) to bottom (higher numbers).

PARAMETERS:

   1.  mark1      -  pointer to MARK_REGION (INPUT)
                     If set, this is the most current mark.  It is matched
                     with tdm_mark or cursor_buff to get the line range.

   2.  cursor_buff - pointer to BUFF_DESCR (INPUT)
                     This is the buffer description which shows the current
                     location of the cursor.

   3.  top_line    - pointer to int (OUTPUT)
                     Into this parm is placed the top line in the range (closest to zero).

   4.  top_col     - pointer to int (OUTPUT)
                     Into this parm is placed the column number that goes with top_line.

   5.  bottom_line - pointer to int (OUTPUT)
                     Into this parm is placed the bottom line in the range larger number than top_line.

   6.  bottom_col  - pointer to int (OUTPUT)
                     Into this parm is placed the column number that goes with bottom_line

   7.  buffer      - pointer to PAD_DESCR (OUTPUT)
                     This is the buffer description for the range of text.




FUNCTIONS :

   1.   If the mark1 mark is set, the range is from the mark to the
        current cursor position.

   2.   If the mark1 mark is not set, the range is either from the
        current cursor position to the end of line or from the current
        cursor position to the end of file.  Which one is determined
        by the calling command.

   3.   Order the two points from top to bottom on the file.


RETURNED VALUE:
   rc   -    return code
             0 - range processed
            -1 - points are in different buffers. (message is not yet issued)


*************************************************************************/

int  range_setup(MARK_REGION       *mark1,              /* input   */
                 BUFF_DESCR        *cursor_buff,        /* input   */
                 int               *top_line,           /* output  */
                 int               *top_col,            /* output  */
                 int               *bottom_line,        /* output  */
                 int               *bottom_col,         /* output  */
                 PAD_DESCR        **buffer)             /* output  */ 
{
int                      from_line;
int                      from_col;
PAD_DESCR               *from_buffer;
int                      to_line;
int                      to_col;
PAD_DESCR               *to_buffer;

mark_point_setup(NULL,               /* input  */
                 cursor_buff,        /* input  */
                 &from_line,         /* output  */
                 &from_col,          /* output  */
                 &from_buffer);      /* output  pointer to BUFF_DESCR */

DEBUG2(
   fprintf(stderr, "@range_setup Cursor: %s [%d,%d] ",
           which_window_names[from_buffer->which_window],
           from_line,from_col);
)

if (mark1->mark_set)
   {
      /***************************************************************
      *  There is a mark set, range is from the mark to the current position.
      ***************************************************************/
      to_line     = from_line;
      to_col      = from_col;
      to_buffer   = from_buffer;
      from_line   = mark1->file_line_no;
      from_col    = mark1->col_no;
      from_buffer = mark1->buffer;
      DEBUG2(
         fprintf(stderr, "Mark: %s [%d,%d] ",
                 which_window_names[from_buffer->which_window],
                 from_line,from_col);
      )
   }
else
   {
      /***************************************************************
      *  There is NO mark set, range is from the current pos to end of line.
      ***************************************************************/
      to_buffer = from_buffer;
      to_col  = MAX_LINE + 2;
      to_line = from_line;
      DEBUG2(
         fprintf(stderr, "NO Mark: %s [%d,%d] ",
                 which_window_names[from_buffer->which_window],
                 to_line,to_col);
      )
   }


/***************************************************************
*  Make sure the range is in the same window.
***************************************************************/
if (to_buffer == from_buffer)
   {
      *buffer = to_buffer;

      /***************************************************************
      *  Arrange the from and to into a physical top and bottom range.
      ***************************************************************/
      if (to_line > from_line)
         {
            *top_line    = from_line;
            *top_col     = from_col;
            *bottom_line = to_line;
            *bottom_col  = to_col;
         }
      else
         if (from_line > to_line)
            {
               *top_line    = to_line;
               *top_col     = to_col;
               *bottom_line = from_line;
               *bottom_col  = from_col;
            }
         else
            if (to_col > from_col)  /* same line */
               {
                  *top_line    = from_line;
                  *top_col     = from_col;
                  *bottom_line = to_line;
                  *bottom_col  = to_col;
               }
            else
               {
                  *top_line    = to_line;
                  *top_col     = to_col;
                  *bottom_line = from_line;
                  *bottom_col  = from_col;
               }
      /***************************************************************
      *  RES 9/5/1997 Deal with substitute over a rectangular region 
      *  This makes rectangular mark always look like top left to
      *  bottom right.
      ***************************************************************/
      if ((*buffer)->display_data->echo_mode == RECTANGULAR_HIGHLIGHT)
         if (*top_col > *bottom_col)
            {
               to_col      = *top_col;
               *top_col    = *bottom_col;
               *bottom_col = to_col;
            }
      return(0);
   }
else
   return(-1);


} /* end of range_setup */



 /************************************************************************

NAME:      dm_comma  -  DM comma command in substitute, xc, or xd

PURPOSE:    This routine is  used when a comma is detected in a
            range specification for a command.
            This routine performs the marking function necessary in
            commands such as:  1,$s/abc/def/
                      and      /abc/,/xyz/xc
            In these situations the number (or find), comma and 
            dollar sign (or second find) are treated as separate
            commands.  The first and third commands set cursor
            positions.  The comma saves the first set cursor position.

PARAMETERS:

   1.  mark1                 - pointer to MARK_REGION (OUTPUT)
                               This mark is set to the current cursor location.

   2.  cursor_buff           - pointer to BUFF_DESCR (INPUT / OUTPUT)
                               This is the buffer description which shows the current
                               location of the cursor.

   3.  comma_mark            - pointer to MARK_REGION (INPUT)
                               This is the mark region set up by bring_up_to_snuff.
                               It has the last place in the main window, any activity ocurred.


FUNCTIONS :

   1.   Copy the current cursor position to the mark

   2.   Make the last position in the main pad the current position
        This is done because in a /abc/,/def/xc command, the second
        find starts at the same place the first find started.


*************************************************************************/

void  dm_comma(MARK_REGION       *mark1,                  /* output   */
               BUFF_DESCR        *cursor_buff,            /* input / output  */
               MARK_REGION       *comma_mark)             /* input    */
{
int      i;

/* dr where we are */
mark1->mark_set = True;
mark1->file_line_no = cursor_buff->current_win_buff->file_line_no;
mark1->col_no       = cursor_buff->current_win_buff->file_col_no;
mark1->buffer       = cursor_buff->current_win_buff;

i = strlen(cursor_buff->current_win_buff->buff_ptr);
if (mark1->col_no >= i)
   mark1->col_no = i;

if (!comma_mark->buffer)
   return; /* avoid memory fault if comma mark was never set up RES 11/8/94 */

cursor_buff->current_win_buff = comma_mark->buffer; /* probably not needed */
cursor_buff->current_win_buff->file_line_no = comma_mark->file_line_no;
cursor_buff->win_line_no  = comma_mark->file_line_no - comma_mark->buffer->first_line;
cursor_buff->current_win_buff->file_col_no  = comma_mark->col_no;
cursor_buff->y            = (cursor_buff->win_line_no * cursor_buff->current_win_buff->window->line_height) + cursor_buff->current_win_buff->window->sub_y + 1; /* +1 makes sure we are inside the character */
set_window_col_from_file_col(cursor_buff);

DEBUG2(fprintf(stderr, "comma: Mark set:  File [%d,%d] in %s, cursor pos set to [%d,%d]\n",
                        mark1->file_line_no, mark1->col_no,
                        which_window_names[cursor_buff->which_window],
                        cursor_buff->current_win_buff->file_line_no, cursor_buff->current_win_buff->file_col_no);
)

} /* end of dm_comma */



/************************************************************************

NAME:      toggle_switch

PURPOSE:    This routine processes the switchs in DM commands like DMC_ei
            which have a -on and -off (default -toggle) flag.

PARAMETERS:

   1.  mode    - int  (INPUT)
                 This it the mode field from the DM command. It is one of
                 the values DM_TOGGLE, DM_ON, or DM_OFF.

   2.  cur_val - int (INPUT)
                 This is the current value of the flag.  It true (1) or false (0).
                 It is used to deal with DM_TOGGLE and to determine if anything changed.

   3.  changed - pointer to int (OUTPUT)
                 This flag is returned as 1 if a change occured and false if there was
                 no change.


FUNCTIONS :

   1.   Set the flag to the new value.

   2.   If the flag changed value, set  the changed flag.

OUTPUTS:
   new_val - Returned value
             The returned value is the new value of the flag.


*************************************************************************/

int  toggle_switch(int      mode,      /* input - DM_TOGGLE, DM_ON, or DM_OFF */
                   int      cur_val,   /* input */
                   int     *changed)   /* output */
{

int           new_val;

*changed = 0;

if (mode == DM_ON ||
    (mode == DM_TOGGLE && cur_val == False))
   {
      if (cur_val == False)
         *changed = True;
      new_val = True;
   }
else
   {
      if (cur_val == True)
         *changed = True;
      new_val = False;
   }

return(new_val);

} /* end of toggle_switch */

/************************************************************************

NAME:      check_and_add_lines  -  see if lines have to be added because
                                   of pending change past eof, or in the
                                   empty area caused by a form feed.

PURPOSE:    This routine checks the current cursor position and makes
            sure there is a line under it.

PARAMETERS:

   1.  cursor_buff - pointer to BUFF_DESCR (INPUT)
                     This is the buffer description which shows the current
                     location of the cursor.


FUNCTIONS :

   1.   Check to see if the current file line number is past the end of
        file.  If so, add empty lines to the file so that the current
        file line number exists.

   2.   If there is a formfeed in the window.  See if the current pos is
        in the empty area past the formfeed line.  If so, add lines before the
        formfeed line so we can access the line without modifying things on
        the next page.


RETURNED VALUE:
   added  -  The number of lines added is returned.


*************************************************************************/

int check_and_add_lines(BUFF_DESCR   *cursor_buff)
{
int       lines_to_add;
int       pos_to_add;
int       i;
int       added = 0;

/***************************************************************
*
*   If the line being modified, is past eof, insert lines as needed. 
*   We will redraw the whole screen in this case.
*
***************************************************************/
if (cursor_buff->current_win_buff->file_line_no >= total_lines(cursor_buff->current_win_buff->token))
   {
      DEBUG7(fprintf(stderr, "check_and_add_lines:  Current Last line is %d\n", total_lines(cursor_buff->current_win_buff->token)-1);)

      lines_to_add = (cursor_buff->current_win_buff->file_line_no - total_lines(cursor_buff->current_win_buff->token)) + 1;
      for (i = 0; i < lines_to_add; i++)
      {
         DEBUG7( fprintf(stderr,"check_and_add_lines:  Adding null line %d in %s\n", total_lines(cursor_buff->current_win_buff->token),  which_window_names[cursor_buff->which_window]); )
         if (WRITABLE(cursor_buff->current_win_buff->token))
            put_line_by_num(cursor_buff->current_win_buff->token,
                            total_lines(cursor_buff->current_win_buff->token)-1,
                            "", INSERT);
         added++;
      }
      if (WRITABLE(cursor_buff->current_win_buff->token))
         {
            cursor_buff->current_win_buff->buff_ptr = "";
         }
   }

/***************************************************************
*  
*  If there is a form feed in the window and we are typing
*  in the empty space left by the form feed, add lines to pad.
*  
***************************************************************/

if (cursor_buff->current_win_buff->formfeed_in_window && (cursor_buff->win_line_no >= cursor_buff->current_win_buff->formfeed_in_window))
   {
      DEBUG7(fprintf(stderr, "check_and_add_lines:  Adding lines because of form feed\n");)

      lines_to_add = (cursor_buff->win_line_no - cursor_buff->current_win_buff->formfeed_in_window) + 1;
      pos_to_add   = (cursor_buff->current_win_buff->first_line + cursor_buff->current_win_buff->formfeed_in_window) - 1;

      for (i = 0; i < lines_to_add; i++)
      {
         DEBUG7( fprintf(stderr,"check_and_add_lines:  Adding null line %d in %s\n", cursor_buff->current_win_buff->formfeed_in_window,  which_window_names[cursor_buff->which_window]); )
         if (WRITABLE(cursor_buff->current_win_buff->token))
            put_line_by_num(cursor_buff->current_win_buff->token,
                            pos_to_add,
                            "", INSERT);
         added++;
      }
      if (WRITABLE(cursor_buff->current_win_buff->token))
         {
            cursor_buff->current_win_buff->buff_ptr = "";
            cursor_buff->current_win_buff->formfeed_in_window += lines_to_add;
         }
   }

return(added);
 
} /* end of check_and_add_lines */





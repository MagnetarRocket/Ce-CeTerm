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
*  module typing.c
*
*  Routines in typing.c
*         dm_es               - es (enter source command)
*         dm_en               - en (enter command)
*         dm_ed               - ed (delete char under the cursor)
*         dm_ee               - ee (delete character before the cursor)
*         insert_in_buff      - put chars in a buffer
*         delete_from_buff    - remove chars from a buffer
*         flush               - flush the work buffer back to the memory map file.
*         redraw_start_setup  - setup start line for partial window redraws
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h      */


#include "borders.h"
#include "buffer.h"
#include "cc.h" /* needed for cc cc_joinln */
#include "cd.h"
#include "debug.h"
#include "dmwin.h"
#include "mark.h"
#include "memdata.h"
#ifdef PAD
#include "pad.h"
#endif
#include "parms.h"
#include "parsedm.h"
#include "prompt.h"
#include "redraw.h"
#include "tab.h"
#include "typing.h"
#include "undo.h"
#ifdef PAD
#include "undo.h"
#include "unixpad.h"
#include "unixwin.h"
#include "vt100.h"
#endif
#include "ww.h"

#define IN_EMPTY_WINDOW_SPACE(pad, file_line_no) ( (pad->formfeed_in_window && ((file_line_no - pad->first_line) > pad->formfeed_in_window) && ((file_line_no - pad->first_line) < pad->window->lines_on_screen) ? True : False)


/************************************************************************

NAME:      dm_es

PURPOSE:    This routine will perform a DM es (enter string)
            command on the window under the cursor.

PARAMETERS:

   1. cursor_buff  - pointer to BUFF_DESCR (INPUT / OUTPUT)
                     This is a pointer to the cursor buff description.
                     The buffer referenced in this description is modified.

   2.  dmc         - pointer to DMC (INPUT)
                     This is the es command structure.


FUNCTIONS :

   1.   Make sure at least one character will fit in the window.  If not,
        flag the fact.

   2.   Add the string into the working buffer making sure the total string
        length does not exceed MAX_LINE.

   3.   Redraw the modified line from the point of insertion to the end of the line.

   4.   Determine how much of the screen will have to be refreshed and set the
        redraw accordingly.

   5.   Determine the new cursor position and set it to be past where the
        string was entered.

RETURNED VALUE:
   redraw -  If the screen needs to be redrawn, the mask of which screen
             and what type of redrawing is returned.


*************************************************************************/

int   dm_es(BUFF_DESCR      *cursor_buff,
            DMC             *dmc)
{

int               end_x;
int               overflow;
int               fixed_font;
int               scroll_chars;
int               i;
int               redraw_needed = 0;
int               left_x;
int               junk_x;
int               junk_y;
int               orig_line_len;
DRAWABLE_DESCR   *main_window;

DEBUG7( fprintf(stderr, "es: %s win[%d,%d] file[%d,%d] \"%s\" into \"%s\"\n", 
        which_window_names[cursor_buff->which_window], cursor_buff->win_line_no, cursor_buff->win_col_no,
        cursor_buff->current_win_buff->file_line_no, cursor_buff->current_win_buff->file_col_no, dmc->es.string, 
        cursor_buff->current_win_buff->buff_ptr);
)

#ifdef PAD
/***************************************************************
*  
*  In vt100 mode under pad_mode, we treat the main pad differently
*  (The unixcmd window is currently unmapped).  We just send the
*  characters to the terminal window.  If the terminal window
*  cannot take them, we will beep at the user.
*  
***************************************************************/

if ((*cursor_buff->current_win_buff->vt100_mode) && (cursor_buff->which_window == MAIN_PAD))
   {
      i = vt100_pad2shell(cursor_buff->current_win_buff, dmc->es.string, False); /* no forced newline */
      if ((i != PAD_SUCCESS) && (i != PAD_FAIL /* message already generated in this case */))
         dm_error("Socket to shell full", DM_ERROR_BEEP);
      return(0);
   }
#endif

/***************************************************************
*
*  If the prompt is wider than the window, scroll the prompt first.
*
***************************************************************/

left_x = cursor_buff->current_win_buff->window->sub_x;
if (left_x >= (int)cursor_buff->current_win_buff->window->width) /* real wide prompt */
   {
      if (cursor_buff->which_window == DMINPUT_WINDOW) 
         scroll_dminput_prompt(strlen(get_dm_prompt()));
#ifdef PAD
      if (cursor_buff->which_window == UNIXCMD_WINDOW) 
         scroll_unix_prompt(strlen(get_drawn_unix_prompt()));
#endif
      left_x = cursor_buff->current_win_buff->window->sub_x;
      cursor_buff->x = left_x;
      cursor_buff->win_col_no = 0;
      if (cursor_buff->current_win_buff->win_lines->tabs)  /* access element zero */
         cursor_buff->current_win_buff->file_col_no = tab_cursor_adjust(cursor_buff->current_win_buff->buff_ptr,
                                                                        0,
                                                                        cursor_buff->current_win_buff->first_char,
                                                                        TAB_LINE_OFFSET,
                                                                        cursor_buff->current_win_buff->display_data->hex_mode);
      else
         cursor_buff->current_win_buff->file_col_no = cursor_buff->current_win_buff->first_char;
      redraw_needed |= (cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW);
   }

/***************************************************************
*
*  If we are off the left edge, move the cursor for the user.
*
***************************************************************/

if (cursor_buff->x < left_x)
   {
      DEBUG7( fprintf(stderr, "es: Cursor to the left of col zero, adjusting to window col zero\n");)
      cursor_buff->x = left_x;
      cursor_buff->win_col_no = 0;
      if (cursor_buff->current_win_buff->win_lines->tabs)  /* access element zero */
         cursor_buff->current_win_buff->file_col_no = tab_cursor_adjust(cursor_buff->current_win_buff->buff_ptr,
                                                                        0,
                                                                        cursor_buff->current_win_buff->first_char,
                                                                        TAB_LINE_OFFSET,
                                                                        cursor_buff->current_win_buff->display_data->hex_mode);
      else
         cursor_buff->current_win_buff->file_col_no = cursor_buff->current_win_buff->first_char;
   }

/***************************************************************
*  
*  Make sure there is room for the string in the window.
*  If not shift the window.
*  This is a deviation from the original DM because it would make
*  sure all the characters would fit and truncate anything which
*  did not fit in the window.
*  
***************************************************************/

fixed_font = cursor_buff->current_win_buff->window->fixed_font;

if (fixed_font)
   end_x = cursor_buff->x + (fixed_font * strlen(dmc->es.string));
else
   end_x = cursor_buff->x + XTextWidth(cursor_buff->current_win_buff->window->font, dmc->es.string, strlen(dmc->es.string));

/*DEBUG7( fprintf(stderr, "es: x = %d, end_x = %d, window_width = %d\n", cursor_buff->x, end_x, (cursor_buff->current_win_buff->window->width - BORDER_WIDTH));)*/
/* RES 9/16/97 DEBUG7( fprintf(stderr, "es: x = %d, end_x = %d, window_width = %d\n", cursor_buff->x, end_x, ((cursor_buff->current_win_buff->window->sub_width - cursor_buff->current_win_buff->window->sub_x) - BORDER_WIDTH));)*/
DEBUG7( fprintf(stderr, "es: x = %d, end_x = %d, window_width = %d\n", cursor_buff->x, end_x, cursor_buff->current_win_buff->window->sub_width);)

/***************************************************************
*  
*  If the end position of the es is off screen, shift the
*  screen to expose the end of the string.
*  RES 7/25/94 - add off screen warp check, when window is partially
*  obscurred off right edge of the physical screen.
***************************************************************/
junk_x = end_x + cursor_buff->current_win_buff->window->font->max_bounds.width;
junk_y = cursor_buff->y;
main_window = cursor_buff->current_win_buff->display_data->main_pad->window; /* I don't like this but it will do for now */

/*if ((end_x >= (cursor_buff->current_win_buff->window->width - BORDER_WIDTH)) ||*/
if (((unsigned)end_x >= ((cursor_buff->current_win_buff->window->sub_width + cursor_buff->current_win_buff->window->sub_x) - BORDER_WIDTH)) ||
    off_screen_warp_check(cursor_buff->current_win_buff->display_data->display, main_window ,cursor_buff->current_win_buff->window, &junk_x, &junk_y))
   {
      if ((end_x - cursor_buff->x) >= (int)cursor_buff->current_win_buff->window->sub_width)
         scroll_chars = strlen(dmc->es.string) - 1;  /* make sure last char of es is on screen */
      else
         scroll_chars = strlen(dmc->es.string) + 5; /* 5 is an abitrary number of characters to scroll onto the right side of the screen */
      DEBUG7( fprintf(stderr, "es: shifting pad %d chars right, old first char = %d\n", scroll_chars, cursor_buff->current_win_buff->first_char);)

      /***************************************************************
      *  For the DM input window or the UNIX command window, try
      *  scrolling the prompt rather than the data.
      ***************************************************************/
      if (cursor_buff->which_window == DMINPUT_WINDOW) 
         {
            if (scroll_chars <= 0) /* RES 9/16/97 we need to scroll some for sure. Takes care of sub_width = 1 due to window size */
               scroll_chars = 2;
            scroll_chars -= scroll_dminput_prompt(scroll_chars);
            if (scroll_chars <= 0)
               scroll_chars = 0;
            DEBUG7( fprintf(stderr, "es: DM prompt shift changed scroll chars to %d\n", scroll_chars);)
         }
#ifdef PAD
      if (cursor_buff->which_window == UNIXCMD_WINDOW) 
         {
            if (scroll_chars <= 0) /* RES 9/16/97 we need to scroll some for sure. Takes care of sub_width = 1 due to long UNIX prompt */
               scroll_chars = 2;
            scroll_chars -= scroll_unix_prompt(scroll_chars);
            if (scroll_chars <= 0)
               scroll_chars = 0;
            DEBUG7( fprintf(stderr, "es: UNIX prompt shift changed scroll chars to %d\n", scroll_chars);)
         }
#endif
      cursor_buff->current_win_buff->first_char += scroll_chars;
      DEBUG7( fprintf(stderr, "es: shifting data %d chars right, first char = %d\n", scroll_chars, cursor_buff->current_win_buff->first_char);)
      redraw_needed |= (cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW);
   }

/***************************************************************
*  
*  Insert the string into the buffer and mark the buffer modified.
*  The overflow flag is set if the totalnumber of characters 
*  on the line exceeded MAX_LINE.  This is different that running
*  out of room on the window.
*  
***************************************************************/

if (cursor_buff->current_win_buff->file_col_no >= MAX_LINE)
   {
      dm_error("Line too long", DM_ERROR_BEEP);
      return(redraw_needed);
   }

orig_line_len = strlen(cursor_buff->current_win_buff->buff_ptr);

overflow = insert_in_buff(cursor_buff->current_win_buff->buff_ptr,
               dmc->es.string,
               cursor_buff->current_win_buff->file_col_no,
               *cursor_buff->current_win_buff->insert_mode,
               MAX_LINE);

cursor_buff->current_win_buff->buff_modified = True;
if (overflow)
   dm_error("Line too long", DM_ERROR_BEEP);

/* RES 2/18/2008 Add test for insert mode to keep color from moving in overstrike mode */
if (COLORED(cursor_buff->current_win_buff->token) && *cursor_buff->current_win_buff->insert_mode)
   cdpad_add_remove(cursor_buff->current_win_buff, 
                    cursor_buff->current_win_buff->file_line_no,
                    cursor_buff->current_win_buff->file_col_no,
                    strlen(dmc->es.string));

/***************************************************************
*  
*  If we are in hex mode, force a full line redraw.
*  
***************************************************************/
if (cursor_buff->current_win_buff->display_data->hex_mode)
   cursor_buff->current_win_buff->redraw_start_col = 0; /* redraw whole line to get rid of extra $ char */
else
   cursor_buff->current_win_buff->redraw_start_col = MIN(cursor_buff->win_col_no, (orig_line_len-cursor_buff->current_win_buff->first_char));

/***************************************************************
*  
*  Figure out what type of redrawing is needed.
*  
***************************************************************/
redraw_needed |= redraw_start_setup(cursor_buff);
if (strchr(dmc->es.string, '\f') != NULL)  /* putting in a form feed forces a partial redraw */
   redraw_needed |= (cursor_buff->current_win_buff->redraw_mask & PARTIAL_REDRAW);

/***************************************************************
*  
*  Move over the entered source, both logically and cursor wise .
*  Do it in the pad too.
*  
***************************************************************/

cursor_buff->current_win_buff->file_col_no += strlen(dmc->es.string);

/***************************************************************
*  
*  See if we need to do a window wrap.  Requirements:
*  1.  The window wrap feature must be on (window_wrap_col != 0).
*  2.  The length of the line must be equal to the window wrap column.
*  Thus, the window wrap triggers on the es which spans the window
*  wrap column.  For normal es's of one char, this is when you
*  get to thw window wrap column.    
*  
***************************************************************/
if ((cursor_buff->current_win_buff->window_wrap_col != 0) &&
    ((int)strlen(cursor_buff->current_win_buff->buff_ptr) > cursor_buff->current_win_buff->window_wrap_col))
   {
      /***************************************************************
      *              WINDOW WRAP CASE
      *  Starting at the window wrap col, walk backwards looking for 
      *  a blank or tab.  If found, do a line split
      *  we blew away the cursor.  Erase the record of it.
      *  
      ***************************************************************/
      DEBUG7( fprintf(stderr, "es: window wrap at %d, len = %d, cursor at %d\n", cursor_buff->current_win_buff->window_wrap_col, strlen(cursor_buff->current_win_buff->buff_ptr), cursor_buff->current_win_buff->file_col_no);)
      redraw_needed |= ww_line(cursor_buff);
   }

set_window_col_from_file_col(cursor_buff); /* input / output */

DEBUG7( fprintf(stderr, "es: Done win[%d,%d] file[%d,%d] (%d,%d) \"%s\"\n", 
        cursor_buff->win_line_no, cursor_buff->win_col_no,
        cursor_buff->current_win_buff->file_line_no, cursor_buff->current_win_buff->file_col_no,
        cursor_buff->x, cursor_buff->y,
        cursor_buff->current_win_buff->buff_ptr
        );
)


return(redraw_needed);

} /* end of dm_es */



/************************************************************************

NAME:      dm_en

PURPOSE:    This routine will take care of an en (enter command)

PARAMETERS:

   1.  dmc         - pointer to DMC (INPUT)
                     This is the en command structure.

   2.  dspl_descr  - pointer to DISPLAY_DESCR (INPUT / OUTPUT)
                     This is a pointer to the current display description.

   3.  new_dmc     - pointer to pointer to DMC (OUTPUT)
                     If this is an enter in the DM command window, one                     
                     or more commands may be generated.  They are parsed
                     and the DM command structure is returned in the new_dmc parm.

FUNCTIONS :

   1.   Find the split position in the line.  We may have to pad with blanks
        to get to this position.

   2.   Split the line.

   3.   If this is the main input window, set the new cursor position

   4.   If this is the DM input window, determine if there is an outstanding
        prompt.  If so, respond to the prompt.

   5.   In the DM input window, parse the line as a command string and then delete
        the line.  The parser returns the list of parsed DM commands.  Put these in
        the returned parm.


OUTPUTS:
   redraw -  If the screen and cursor need to be redrawn, the redraw mask is returned.
             Otherwise 0 is returned.


*************************************************************************/

int    dm_en(DMC            *dmc,           /* input  */
             DISPLAY_DESCR  *dspl_descr,    /* input / output */
             DMC           **new_dmc)       /* output */
{
int                   split_pos;
int                   redraw_needed = 0;
int                   len;
int                   lines;
MARK_REGION           prompt_mark;
int                   lines_added;

*new_dmc = NULL; /* initialization */
prompt_mark.mark_set = False;

/***************************************************************
* negative column numbers are not allowed
***************************************************************/
if (dspl_descr->cursor_buff->current_win_buff->file_col_no < 0)
   dspl_descr->cursor_buff->current_win_buff->file_col_no = 0;

if (dspl_descr->cursor_buff->win_col_no < 0)
   dspl_descr->cursor_buff->win_col_no = 0;

DEBUG7( fprintf(stderr, "en: %s win[%d,%d] file[%d,%d] (tdm is %s)\n", 
        which_window_names[dspl_descr->cursor_buff->which_window], dspl_descr->cursor_buff->win_line_no, dspl_descr->cursor_buff->win_col_no,
        dspl_descr->cursor_buff->current_win_buff->file_line_no, dspl_descr->cursor_buff->current_win_buff->file_col_no,
        (dspl_descr->tdm_mark.mark_set ? "set" : "not set"));
)

/***************************************************************
*  
*  First figure out where we are in the string.
*  We may have to pad the string to the cursor position.
*  
***************************************************************/

if (dspl_descr->cursor_buff->current_win_buff->file_col_no >= MAX_LINE)
   {
      dm_error("Line too long", DM_ERROR_BEEP);
      return(0);
   }

split_pos = dspl_descr->cursor_buff->current_win_buff->file_col_no;
len       = strlen(dspl_descr->cursor_buff->current_win_buff->buff_ptr);

if (split_pos > len)
   {
      insert_in_buff(dspl_descr->cursor_buff->current_win_buff->buff_ptr,
                     "",
                     split_pos,
                     dspl_descr->insert_mode,
                     MAX_LINE);
      dspl_descr->cursor_buff->current_win_buff->buff_modified = True;

   }

#ifdef PAD
/***************************************************************
*  
*  For the unixpad window, if we are in dot mode (password
*  being entered), turn off undo processing.  This way we
*  cannot do an undo and get the password back in plain text.
*  
***************************************************************/

if ((dspl_descr->cursor_buff->which_window == UNIXCMD_WINDOW) && !(TTY_DOT_MODE) && !NODOTS)
   undo_semafor = ON;
#endif

/***************************************************************
*  
*  If we are in overwrite mode get rid of the character at 
*  split pos.
*  
***************************************************************/

if (!(dspl_descr->insert_mode))
   {
      (void) delete_from_buff(dspl_descr->cursor_buff->current_win_buff->buff_ptr, 1, split_pos);
      dspl_descr->cursor_buff->current_win_buff->buff_modified = True;
   }

/***************************************************************
*  
*  Split the line at the cursor position.  This is still for
*  all input/output windows.  We have to put the line back in the memdata
*  structure if it has been modified, othewise the split will not work.
*  
***************************************************************/

if (dspl_descr->cursor_buff->current_win_buff->buff_modified)
   flush(dspl_descr->cursor_buff->current_win_buff);

split_line(dspl_descr->cursor_buff->current_win_buff->token, dspl_descr->cursor_buff->current_win_buff->file_line_no, split_pos, 1 /* create the newline */ );

/***************************************************************
*  
*  The basic stuff is done.  The following is different for the
*  edit window and the command window.  This is because enter
*  causes a newline and nothing else in the edit window and
*  causes a command to be executed in the command window.
*  
***************************************************************/
switch(dspl_descr->cursor_buff->which_window)
{
case DMINPUT_WINDOW:
   /***************************************************************
   *  RES 7/17/94
   *  Odd case where we are not at line zero.  Concatinate all the
   *  lines from zero to where we are with semi colons in between.
   *  This technique is not too fast, but this is a rare case.
   ***************************************************************/
   while (dspl_descr->cursor_buff->current_win_buff->file_line_no > 0)
   {
      DEBUG7(fprintf(stderr, "en: Joining line in dm input window\n");)
      put_line_by_num(dspl_descr->cursor_buff->current_win_buff->token, 0, ";", INSERT);
      /* don't worry about color in the DM window */
      join_line(dspl_descr->cursor_buff->current_win_buff->token, 0); /* add in the semi colon */
      join_line(dspl_descr->cursor_buff->current_win_buff->token, 0); /* attach the next line  */
      dspl_descr->cursor_buff->current_win_buff->file_line_no--;
   }
   /***************************************************************
   *  For the dm input window, the line to process is in 
   *  line zero.  Get the line, process it and delete it.
   ***************************************************************/
   dspl_descr->cursor_buff->current_win_buff->buff_ptr = get_line_by_num(dspl_descr->cursor_buff->current_win_buff->token, 0);
   /***************************************************************
   *  We now have the line to work with.
   *  parse the line and put the results in new_dmc.  This is
   *  processed by main loop temination code to put it on the front of
   *  the list.  We don't do that here because it could involve
   *  modifying DMC structures which are held in static key defs.
   ***************************************************************/
   DEBUG4( fprintf(stderr, "cmd = \"%s\"\n", dspl_descr->cursor_buff->current_win_buff->buff_ptr);)
   if (prompt_in_progress(dspl_descr))
      {
         redraw_needed |= complete_prompt(dspl_descr, dspl_descr->cursor_buff->current_win_buff->buff_ptr, &prompt_mark);
         delete_line_by_num(dspl_descr->cursor_buff->current_win_buff->token, 0, 1);  /* line 0, delete 1 line */
      }
   else
      {
         *new_dmc = parse_dm_line(dspl_descr->cursor_buff->current_win_buff->buff_ptr, True, 0, dspl_descr->escape_char, dspl_descr->hsearch_data);
         redraw_needed |= (dspl_descr->cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW);
         if ((*new_dmc != NULL) && (*new_dmc != DMC_COMMENT))
            {
               delete_line_by_num(dspl_descr->cursor_buff->current_win_buff->token, 0, 1);  /* line 0, delete 1 line */
               dm_error("", DM_ERROR_MSG); /* clear the window */
            }
      }
   scroll_dminput_prompt(-MAX_LINE);
   dspl_descr->cursor_buff->current_win_buff->buff_ptr = get_line_by_num(dspl_descr->cursor_buff->current_win_buff->token, 0);
   dspl_descr->cursor_buff->current_win_buff->buff_modified = False;
   dspl_descr->cursor_buff->current_win_buff->first_char = 0;
   dspl_descr->cursor_buff->current_win_buff->file_col_no = 0;
   dspl_descr->cursor_buff->current_win_buff->file_line_no = 0;
   dspl_descr->cursor_buff->current_win_buff->first_line = 0;

   /***************************************************************
   *  If there is a mark set, go back to it.  Otherwise go to the
   *  start of the DM input line.
   ***************************************************************/
   if ((dspl_descr->tdm_mark.mark_set) && (*new_dmc != NULL))
      {
         if (dspl_descr->tdm_mark.buffer->formfeed_in_window || (dspl_descr->tdm_mark.file_line_no >= total_lines(dspl_descr->tdm_mark.buffer->token)))
            {
               flush(dspl_descr->tdm_mark.buffer);
               dspl_descr->cursor_buff->current_win_buff = dspl_descr->tdm_mark.buffer;
               dspl_descr->cursor_buff->which_window     = dspl_descr->tdm_mark.buffer->which_window;
               dspl_descr->cursor_buff->win_line_no      = dspl_descr->tdm_mark.file_line_no - dspl_descr->tdm_mark.buffer->first_line;
               lines_added = check_and_add_lines(dspl_descr->cursor_buff);
               if (!WRITABLE(dspl_descr->cursor_buff->current_win_buff->token))
                  dspl_descr->tdm_mark.file_line_no -= lines_added;
               dspl_descr->tdm_mark.buffer->buff_ptr = get_line_by_num(dspl_descr->tdm_mark.buffer->token, dspl_descr->tdm_mark.file_line_no);
               redraw_needed |= (dspl_descr->cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW);
            }
         redraw_needed |= dm_position(dspl_descr->cursor_buff, dspl_descr->tdm_mark.buffer, dspl_descr->tdm_mark.file_line_no, dspl_descr->tdm_mark.col_no);
      }                          
   else
      if (prompt_mark.mark_set)
         redraw_needed |= dm_position(dspl_descr->cursor_buff, prompt_mark.buffer, prompt_mark.file_line_no, prompt_mark.col_no);
      else
         redraw_needed |= dm_position(dspl_descr->cursor_buff, dspl_descr->cursor_buff->current_win_buff, 0, 0);
   break;  /* end of dm_input_window */

#ifdef PAD
case UNIXCMD_WINDOW:
   if ((!dspl_descr->hold_mode) || setprompt_mode())
      {
         /***************************************************************
         *  For the unix input window, convert the lines at or above the
         *  current line to sts commands.
         ***************************************************************/
         (dspl_descr->cursor_buff->current_win_buff->file_line_no)++;
         DEBUG22(fprintf(stderr, "dm_en: Want to send %d lines to the shell\n", dspl_descr->cursor_buff->current_win_buff->file_line_no);)
         lines = send_to_shell(dspl_descr->cursor_buff->current_win_buff->file_line_no,
                               dspl_descr->cursor_buff->current_win_buff);
         DEBUG22(fprintf(stderr, "dm_en: sent %d line(s)\n", lines);)
         (dspl_descr->cursor_buff->current_win_buff->file_line_no) -= lines;
         dspl_descr->cursor_buff->current_win_buff->buff_ptr = get_line_by_num(dspl_descr->cursor_buff->current_win_buff->token, dspl_descr->cursor_buff->current_win_buff->file_line_no);


         redraw_needed |= (dspl_descr->cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW);

         scroll_unix_prompt(-MAX_LINE);
         dspl_descr->cursor_buff->current_win_buff->buff_modified = False;
         redraw_needed |= dm_position(dspl_descr->cursor_buff, dspl_descr->cursor_buff->current_win_buff, dspl_descr->cursor_buff->current_win_buff->file_line_no, 0);
      } /* end of not hold mode */
   else
      {  /* hold mode in unixpad window        */
         /***************************************************************
         *  For the hold mode, we will definitely have a partial redraw.
         ***************************************************************/
         redraw_needed |= redraw_start_setup(dspl_descr->cursor_buff);
         redraw_needed |= (dspl_descr->cursor_buff->current_win_buff->redraw_mask & PARTIAL_REDRAW);
         /* go to the next line column zero */
         /***************************************************************
         *  Move to the next line column zero.
         *  If we are on the last line of the window, scroll the window up one.
         *  Unless we can expand the window since we are in hold mode
         *  on the unixcmd window.
         ***************************************************************/
         (dspl_descr->cursor_buff->current_win_buff->file_line_no)++;
         (dspl_descr->cursor_buff->win_line_no)++;
         dspl_descr->cursor_buff->current_win_buff->file_col_no = 0;
         dspl_descr->cursor_buff->win_col_no = 0;
         dspl_descr->cursor_buff->current_win_buff->buff_ptr = get_line_by_num(dspl_descr->cursor_buff->current_win_buff->token, dspl_descr->cursor_buff->current_win_buff->file_line_no);
         if ((dspl_descr->cursor_buff->win_line_no >= dspl_descr->cursor_buff->current_win_buff->window->lines_on_screen - 1) &&
             (dspl_descr->cursor_buff->current_win_buff->window->lines_on_screen >= MAX_UNIX_LINES))
            {
               dspl_descr->cursor_buff->current_win_buff->first_line++;
               redraw_needed = (dspl_descr->cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW);
            }
         set_window_col_from_file_col(dspl_descr->cursor_buff);
         /***************************************************************
         *  cursor_buff->current_win_buff->file_line_no was updated by dm_position
         ***************************************************************/
      }  /* end of hold mode in unixpad window */
#endif
   break;  /* end of unix command window pad */

case MAIN_PAD:
   /***************************************************************
   *  For the main window, we will definitely have a redraw.
   *  That is all.
   ***************************************************************/
   redraw_needed |= redraw_start_setup(dspl_descr->cursor_buff);
   redraw_needed |= (dspl_descr->cursor_buff->current_win_buff->redraw_mask & PARTIAL_REDRAW);
   /* go to the next line column zero */
   /***************************************************************
   *  Move to the next line column zero.
   *  If we are on the last line of the window, scroll the window up one.
   *  Unless this is the unixcmd window in hold mode and we can expand
   *  the window.
   ***************************************************************/
   (dspl_descr->cursor_buff->current_win_buff->file_line_no)++;
   dspl_descr->cursor_buff->current_win_buff->file_col_no = 0;
   if (dspl_descr->cursor_buff->win_line_no == dspl_descr->cursor_buff->current_win_buff->window->lines_on_screen - 1)
      {
         dspl_descr->cursor_buff->current_win_buff->first_line++;
         redraw_needed = (dspl_descr->cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW);
      }
   redraw_needed |= dm_position(dspl_descr->cursor_buff, dspl_descr->cursor_buff->current_win_buff, dspl_descr->cursor_buff->current_win_buff->file_line_no, 0);
   /***************************************************************
   *  cursor_buff->current_win_buff->file_line_no was updated by dm_position
   ***************************************************************/
   dspl_descr->cursor_buff->current_win_buff->buff_ptr = get_line_by_num(dspl_descr->cursor_buff->current_win_buff->token, dspl_descr->cursor_buff->current_win_buff->file_line_no);
   break;  /* end of main pad */

   default:
      DEBUG(fprintf(stderr, "Unknown input window # 0x%X\n", dspl_descr->cursor_buff->which_window);)
      break;

} /* end of switch on which window */


#ifdef PAD
/***************************************************************
*  
*  If we turned undo processing on, reset it to the normal off value.
*  
***************************************************************/

if (undo_semafor == ON)
   undo_semafor = OFF;
#endif


/***************************************************************
*  
*  An enter causes an extra key event unless we are in the
*  middle of a string of commands.  It also invalidates the
*  cursor buffer information.
*  
***************************************************************/

if (dmc->next == NULL)
   {
         event_do(dspl_descr->cursor_buff->current_win_buff->token, KS_EVENT, 0, 0, 0, 0);
         if (dspl_descr->cursor_buff->which_window != MAIN_PAD)
            event_do(dspl_descr->main_pad->token, KS_EVENT, 0, 0, 0, 0);
   }
dspl_descr->cursor_buff->up_to_snuff = 0;


return(redraw_needed);

} /* end of dm_en */


/************************************************************************

NAME:      dm_ed

PURPOSE:    This routine deletes the character under the cursor.

PARAMETERS:

   1.  cursor_buff - pointer to BUFF_DESCR (INPUT / OUTPUT)
                     This is a pointer to the buffer under the cursor.

FUNCTIONS :

   1.   If the cursor is past the current end of line, pad the line to the cursor.

   2.   Delete the character under the cursor in the buffer.

   3.   If the (virtual)newline at the end of the buffer is deleted, join the next
        line to this line.  And flag that the screen has to be redrawn.

   4.   Redraw the partial line.


OUTPUTS:

   redraw -  If the screen and cursor need to be redrawn, the redraw mask is returned.
             Otherwise 0 is returned.
        

*************************************************************************/

int    dm_ed(BUFF_DESCR   *cursor_buff)   /* input / output */
{
int                   redraw_needed = 0;
int                   offset;
int                   newline_removed;
char                 *next_line;
int                   orig_len;

DEBUG7( fprintf(stderr, "ed: %s win[%d,%d] file[%d,%d] \"%s\"\n", 
        which_window_names[cursor_buff->which_window], cursor_buff->win_line_no, cursor_buff->win_col_no,
        cursor_buff->current_win_buff->file_line_no, cursor_buff->current_win_buff->file_col_no, cursor_buff->current_win_buff->buff_ptr);
)

offset   = cursor_buff->current_win_buff->file_col_no;
orig_len = strlen(cursor_buff->current_win_buff->buff_ptr);

/***************************************************************
*  
*  If the cursor is off the end of the line, pad the line to this
*  spot.  We will be doing a join.
*  
***************************************************************/

if (offset > orig_len)
   {
      insert_in_buff(cursor_buff->current_win_buff->buff_ptr, "", offset, 0, MAX_LINE);
      if (COLORED(cursor_buff->current_win_buff->token))
         cdpad_add_remove(cursor_buff->current_win_buff,
                          cursor_buff->current_win_buff->file_line_no,
                          offset,
                          offset-orig_len);
   }

/***************************************************************
*  
*  Delete the character under the cursor.  That IS what this
*  routine is all about.
*  
***************************************************************/

newline_removed = delete_from_buff(cursor_buff->current_win_buff->buff_ptr, 1, offset);
cursor_buff->current_win_buff->buff_modified = True;
if ((!newline_removed) && (COLORED(cursor_buff->current_win_buff->token)))
   cdpad_add_remove(cursor_buff->current_win_buff,
                    cursor_buff->current_win_buff->file_line_no,
                    offset,
                    -1);

/***************************************************************
*  
*  If the delete removed a newline, append the next line.
*  If we do that, we have to do a redraw.
*  
***************************************************************/

if (newline_removed && ((cursor_buff->current_win_buff->file_line_no + 1) < total_lines(cursor_buff->current_win_buff->token)))
   {
      /* need to do cd processing for this block ??? */
      cc_joinln(cursor_buff->current_win_buff->token, cursor_buff->current_win_buff->file_line_no);
      if (COLORED(cursor_buff->current_win_buff->token))
         {
            cd_flush(cursor_buff->current_win_buff);
            cd_join_line(cursor_buff->current_win_buff->token,
                         cursor_buff->current_win_buff->file_line_no,
                         strlen(cursor_buff->current_win_buff->buff_ptr));
         }
      /* this call always works because of the previous check */
      next_line = get_line_by_num(cursor_buff->current_win_buff->token, cursor_buff->current_win_buff->file_line_no + 1);
      (void) insert_in_buff(cursor_buff->current_win_buff->buff_ptr,
                            next_line,
                            strlen(cursor_buff->current_win_buff->buff_ptr),
                            0,  /* not inserting */
                            MAX_LINE);
      delete_line_by_num(cursor_buff->current_win_buff->token, cursor_buff->current_win_buff->file_line_no + 1, 1);
      redraw_needed |= redraw_start_setup(cursor_buff);
      redraw_needed |= (cursor_buff->current_win_buff->redraw_mask & PARTIAL_REDRAW);
   }


cursor_buff->current_win_buff->redraw_start_col = MIN(cursor_buff->win_col_no, orig_len);

/***************************************************************
*  
*  Figure out what type of redrawing is needed if not already set.
*  
***************************************************************/

if (!redraw_needed)
   redraw_needed |= redraw_start_setup(cursor_buff);

return(redraw_needed);

} /* end of dm_ed */



/************************************************************************

NAME:      dm_ee

PURPOSE:    This routine deletes the character just before the cursor.

PARAMETERS:

   1.  cursor_buff - pointer to BUFF_DESCR (INPUT / OUTPUT)
                     This is a pointer to the buffer under the cursor.

   2.  wrap_mode   - int (INPUT)
               This flag indicates the ee command was executed with the
               -w option.  This causes the cursor to wrap to the previous line
               when we backspace in column 0.
FUNCTIONS :
   1.   If we are in window column zero, do nothing unless we are in wrap mode.
        In this case reposition the window if it is scrolled right or move to
        the previous line if possible.

   2.   Calculate the width of the previous character.  The cursor has
        to back up by that amount.

   3.   Delete the character from the buffer.

   4.   Redraw the line.

OUTPUTS:
   redraw -  If the screen and cursor need to be redrawn, true is returned,
             Otherwise 0 is returned.


*************************************************************************/

int     dm_ee(BUFF_DESCR   *cursor_buff,
              int           wrap_mode)
{
int                   i;
char                 *line;
int                   redraw_needed = 0;
int                   offset;
int                   orig_len;

DEBUG7( fprintf(stderr, "ee: %s win[%d,%d] file[%d,%d] \"%s\"\n", 
        which_window_names[cursor_buff->which_window], cursor_buff->win_line_no, cursor_buff->win_col_no,
        cursor_buff->current_win_buff->file_line_no, cursor_buff->current_win_buff->file_col_no, cursor_buff->current_win_buff->buff_ptr);
)

/***************************************************************
*  RES  2/10/1998
*  Added support for -w flag on ee.
*  If we are in window column zero and not using -w, we do nothing
*  If we are using -w, see if we are scrolled right.  If so, scroll
*  left so we are not in window column zero any more.  If we are
*  in file column zero, join with the previous line.
***************************************************************/
if (cursor_buff->win_col_no == 0)
   if (!wrap_mode)
      return(0);
   else
      if (cursor_buff->current_win_buff->first_char > 0)
         {
            redraw_needed |= (cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW);
            i = MIN(cursor_buff->current_win_buff->first_char, 5); /* 5 is an arbitrary number of columns to shift */
            cursor_buff->current_win_buff->first_char -= i;
            cursor_buff->win_col_no += i;
         }
      else
         if (cursor_buff->current_win_buff->file_line_no > 0)
            {
              /***************************************************************
              *  We are in file column zero and not on line zero.  Join the lines.
              ***************************************************************/
              flush(cursor_buff->current_win_buff);
              line = get_line_by_num(cursor_buff->current_win_buff->token, cursor_buff->current_win_buff->file_line_no-1);
              i = strlen(line);
              cc_joinln(cursor_buff->current_win_buff->token, cursor_buff->current_win_buff->file_line_no-1);
              if (COLORED(cursor_buff->current_win_buff->token))
                  {
                     cd_flush(cursor_buff->current_win_buff);
                     cd_join_line(cursor_buff->current_win_buff->token, cursor_buff->current_win_buff->file_line_no-1, i);
                  }
              join_line(cursor_buff->current_win_buff->token, cursor_buff->current_win_buff->file_line_no-1);
              redraw_needed = dm_position(cursor_buff, cursor_buff->current_win_buff, cursor_buff->current_win_buff->file_line_no-1, i);
              redraw_needed |= redraw_start_setup(cursor_buff);
              redraw_needed |= (cursor_buff->current_win_buff->redraw_mask & PARTIAL_REDRAW);
              DEBUG7( fprintf(stderr, "ee: Line join Done win[%d,%d] file[%d,%d] (%d,%d) \"%s\"\n", 
                 cursor_buff->win_line_no, cursor_buff->win_col_no,
                 cursor_buff->current_win_buff->file_line_no, cursor_buff->current_win_buff->file_col_no,
                 cursor_buff->x, cursor_buff->y,
                          cursor_buff->current_win_buff->buff_ptr
                 );
              )
              return(redraw_needed);
            }
         else
            return(0);

(cursor_buff->current_win_buff->file_col_no)--;

if (*cursor_buff->current_win_buff->insert_mode)
   {
      (void) delete_from_buff(cursor_buff->current_win_buff->buff_ptr, 1, cursor_buff->current_win_buff->file_col_no);
      if (COLORED(cursor_buff->current_win_buff->token))
         cdpad_add_remove(cursor_buff->current_win_buff,
                          cursor_buff->current_win_buff->file_line_no,
                          cursor_buff->current_win_buff->file_col_no,
                          -1);
   }
else
   {
      /***************************************************************
      *  RES 2/22/99, 
      *  If the cursor is off the end of the line, don't change the line.
      ***************************************************************/
      offset   = cursor_buff->current_win_buff->file_col_no;
      orig_len = strlen(cursor_buff->current_win_buff->buff_ptr);
      if (offset < orig_len)
         cursor_buff->current_win_buff->buff_ptr[cursor_buff->current_win_buff->file_col_no] = ' ';
   }

cursor_buff->current_win_buff->buff_modified = True;

set_window_col_from_file_col(cursor_buff); /* input / output */
cursor_buff->current_win_buff->redraw_start_col = cursor_buff->win_col_no - 1;

/***************************************************************
*  
*  Figure out what type of redrawing is needed.
*  
***************************************************************/

redraw_needed |= redraw_start_setup(cursor_buff);
set_window_col_from_file_col(cursor_buff); /* input / output */

DEBUG7( fprintf(stderr, "ee: Done win[%d,%d] file[%d,%d] (%d,%d) \"%s\"\n", 
        cursor_buff->win_line_no, cursor_buff->win_col_no,
        cursor_buff->current_win_buff->file_line_no, cursor_buff->current_win_buff->file_col_no,
        cursor_buff->x, cursor_buff->y,
        cursor_buff->current_win_buff->buff_ptr
        );
)

return(redraw_needed);

} /* end of dm_ee */


/************************************************************************

NAME:      insert_in_buff

PURPOSE:    This routine puts characters into a buffer in insert or
            overstrike mode.

PARAMETERS:

   1.  buff        - pointer to char (INPUT / OUTPUT)
                     This is the buffer to be modified.

   2.  insert_str  - pointer to char (INPUT)
                     This is the string to insert.  It can be null.

   3.  offset      - int (INPUT)
                     This is the offset into the buff to put insert_str.

   4.  insert_mode - int (INPUT)
                     If true (non-zero) the string is inserted and the
                     existing data in buff is shifted.  If false (zero)
                     data in buff is overwritten.

   5.  max_len     - int (INPUT)
                     This is the maximum length of buff.


FUNCTIONS :

   1.   If the insert offset is past the end of the string, pad with blanks.

   2.   Make sure the result is not too long.  If so, truncate.

   3.   In insert mode, shift everything over to make room for the new data.
        This includes the null at the end of the string.

   4.   Copy the new data into the buffer.

   5.   For non-insert mode, see if we have to put a new null on the end of the string.


RETURNED VALUE:
        A flag is returned which specifies if the data overflowed the maximum
        length in the buffer and was truncated.


*************************************************************************/

int  insert_in_buff(char  *buff,
                    char  *insert_str,
                    int    offset,
                    int    insert_mode,
                    int    max_len)
{
int   insert_len;
int   orig_len;
int   i;
int   rc = 0;

/***************************************************************
*  Make sure we are inserting in a valid spot.
***************************************************************/
if (offset > max_len)
   return(-1);

orig_len = strlen(buff);
if (insert_str != NULL)
   insert_len = strlen(insert_str);
else
   insert_len = 0;

/***************************************************************
*  If the offset to do the insert at is past the end of the
*  buffer, pad with blanks until we are at the right place.
***************************************************************/
if (offset > orig_len)
   {
      for (i = orig_len;
           i < offset && i < max_len;
           i++)
         buff[i] = ' ';
      buff[i] = '\0';
      orig_len = strlen(buff);
   }

/***************************************************************
*  If the result is too long, truncate.
***************************************************************/
if (orig_len + insert_len > max_len)
   {
      rc = -1;
      insert_len = max_len - orig_len;
   }

/***************************************************************
*  Shift everything over to make room for the new data.
*  Note that is code does move the null character.
***************************************************************/
if (insert_mode)
   for (i = orig_len;
        i >= offset;
        i--)
      buff[i+insert_len] = buff[i];

/***************************************************************
*  Copy the new data into the buffer.
***************************************************************/
for (i = 0;
     i < insert_len;
     i++)
   buff[i + offset] = insert_str[i];

/***************************************************************
*  For non-insert mode, see if we have to put back the null.
***************************************************************/

if (!rc && !insert_mode && (offset + insert_len > orig_len))
   buff[offset + insert_len] = '\0';

return(rc);

} /* end of insert_in_buff */


/************************************************************************

NAME:      delete_from_buff

PURPOSE:    This routine deletes characters from a buffer.

PARAMETERS:

   1.  buff        - pointer to char (INPUT / OUTPUT)
                     This is the buffer to be modified.

   2.  len         - int (INPUT)
                     This is the number of chars to delete.

   3.  offset      - int (INPUT)
                     This is the offset into the buff to do the delete.


FUNCTIONS :

   1.   Make sure there is something to delete.

   2.   If the request is to delete more chars than are left in the buffer,
        truncate the request.

   3.   Delete the requested characters and shift everything over.


RETURNED VALUE:
        A flag is returned which specifies if the newline was deleted.
        For edit windows, this will cause a join.


*************************************************************************/

int  delete_from_buff(char  *buff,
                      int    len,
                      int    offset)
{
int   orig_len;
int   i;
int   rc = 0;

orig_len = strlen(buff);

/***************************************************************
*  Make sure there is something to delete.
***************************************************************/
if (offset >= orig_len)
   return(1);


/***************************************************************
*  Do not delete more than to the end of the buffer.
***************************************************************/
if (offset + len > orig_len)
   {
      len = orig_len - offset;
      rc = 1;
   }

/***************************************************************
*  Shift everything over to get rid of the stuff being deleted.
*  Note that is code does move the null character at the end.
***************************************************************/
for (i = offset;
     i <= orig_len;
     i++)
   buff[i] = buff[i+len];

return(rc);

} /* end of delete_from_buff */


/************************************************************************

NAME:      flush - flush the work buffer back to the memory map file.

PURPOSE:    This routine groups a small set of statements which is
            used often.  It puts the passed buffer's work buffer
            back in the memdata structure and clears the modified
            flag.

PARAMETERS:

   1.  buffer -  pointer to DRAWABLE_DESCR (INPUT / OUTPUT)
               This is the buffer description for the window whose buffer
               is being flushed.  It should be one of main_window_curr_descr,
               dminput_cur_descr, or dmoutput_cur_descr.


FUNCTIONS :

   1.   Copy the buffer back out to the memdata structure.


*************************************************************************/

void flush(PAD_DESCR   *buffer)
{

int             line_no;
WINDOW_LINE    *win_line;

DEBUG7( fprintf(stderr,"flush:  File line %d, modified %d, \"%s\"\n", buffer->file_line_no, buffer->buff_modified, buffer->buff_ptr); )

if (buffer->buff_modified)
   {
      DEBUG3( fprintf(stderr," (flush) Saving line %d in %s  \"%s\"\n", buffer->file_line_no,  which_window_names[buffer->which_window], buffer->buff_ptr); )
      if (total_lines(buffer->token) != 0)
         put_line_by_num(buffer->token, buffer->file_line_no, buffer->buff_ptr, OVERWRITE);
      else
         put_line_by_num(buffer->token, -1, buffer->buff_ptr, INSERT);
      buffer->buff_modified = False;
      buffer->buff_ptr = get_line_by_num(buffer->token, buffer->file_line_no);

   }                     

line_no = buffer->file_line_no - buffer->first_line;
if ((line_no >= 0) && (line_no < buffer->window->lines_on_screen))
   {
      win_line = &buffer->win_lines[line_no];
      if (!win_line || (win_line->line == buffer->work_buff_ptr))
         {
            DEBUG7( fprintf(stderr,"flush:  Fixup winline for win line %d, file line %d\n", line_no, buffer->file_line_no); )
            win_line->line = get_line_by_num(buffer->token, buffer->file_line_no);
         }
   }

if (COLORED(buffer->token))
   cd_flush(buffer);

}  /* end of flush */


/************************************************************************

NAME:      redraw_start_setup

PURPOSE:    This routine does calculations on partial redraws

PARAMETERS:

   1.  buff        - pointer to char (INPUT / OUTPUT)
                     This is the buffer to be modified.

   2.  len         - int (INPUT)
                     This is the number of chars to delete.

   3.  offset      - int (INPUT)
                     This is the offset into the buff to do the delete.


FUNCTIONS :

   1.   If no redraw start line was specified yet (value -1) use the
        current window position.

   2.   If redraw was already set, then this is a partial window redraw.

   3.   If the current line is less than the current redraw position, 
        reset to the lower value.  We redraw from this point on the
        window down.


RETURNED VALUE:
   redraw -  If the screen needs to be redrawn, the mask of which screen
             and what type of redrawing is returned.

*************************************************************************/

int  redraw_start_setup(BUFF_DESCR      *cursor_buff)
{
int                   redraw_needed;

if (cursor_buff->current_win_buff->redraw_start_line == -1)  /* not set yet */
   {
      cursor_buff->current_win_buff->redraw_start_line = cursor_buff->win_line_no;
      redraw_needed = (cursor_buff->current_win_buff->redraw_mask & PARTIAL_LINE);
   }
else
   {
      if (cursor_buff->current_win_buff->redraw_start_line > cursor_buff->win_line_no)
         cursor_buff->current_win_buff->redraw_start_line = cursor_buff->win_line_no;
      redraw_needed = (cursor_buff->current_win_buff->redraw_mask & PARTIAL_REDRAW);
   }

return(redraw_needed);

} /* end of redraw_start_setup */




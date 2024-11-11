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
*  module ww.c
*
*  Routines:
*     dm_ww               - Window Wrap
*     ww_line             - Window wrap a line during interactive (DM_es) processing
*
*  Internal:
      ww_dash_a           - Process ww -a command
*
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h      */

#include "cd.h"
#include "dmsyms.h"
#include "dmwin.h"
#include "memdata.h"
#include "mvcursor.h"
#include "textflow.h"
#include "txcursor.h"
#include "typing.h"
#include "ww.h"


static int ww_dash_a(BUFF_DESCR      *cursor_buff,
                     MARK_REGION     *mark1);



/************************************************************************

NAME:      dm_ww

PURPOSE:    This routine performs the window wrap function.
            It can turn the interactive (typing) window wrap on and off,
            display the current window wrap column, or window wrap one
            pair of lines (-a).

PARAMETERS:

   1.  dmc            - pointer to DMC (INPUT)
                        This is the command structure for the wc command.
                        It contains the data from any dash options.
                        This parm may be null if the confirm_dash_q parm is
                        not null.

   2.  cursor_buff    - pointer to BUFF_DESCR (INPUT / OUTPUT)
                        The current cursor description is used to check which window we are in and
                        to set the window wrap option.

   3.  mark1          - pointer to MARK_REGION (INPUT)
                        If a region is marked, this is the spot marked by the dr (mark)
                        command.  The ww -a can operate over a marked region.


FUNCTIONS :

   1.   If the current window is not the main pad, do nothing.

   2.   If -c was specified, save the new window wrap column.

   3.   If the -on parm was specified and -c was not, calculate the width of the 
        window and set the window wrap width from the window width.

   4.   If the mode is set to toggle, toggle window wrap on and off.

   5.   If ww is on and -a was specified, ww the current line.
        If the line length is less than the ww col, join the line to the
        next line.  Then chop off pieces of the ww col len using split.
        Finally position the cursor on the line past the last line split.

   6.   If -i parm specified, display the current window wrap column.

OUTPUTS:
   redraw -  The mask anded with the type of redraw needed is returned.

*************************************************************************/

int  dm_ww(DMC             *dmc,                    /* input  */
           BUFF_DESCR      *cursor_buff,            /* input  / output */
           MARK_REGION     *mark1)                  /* input  */
{
char             msg[80];
int              redraw_needed;

/***************************************************************
*  
*  Make sure this is the main pad.
*  
***************************************************************/

if (cursor_buff->which_window != MAIN_PAD)
   return(0);

/***************************************************************
*  
*  Check the -c option, which implies -on.  If found, set the
*  column in the pad description.  
*  Otherwise, if -on was specified, make the window width the wrap column.
*  
***************************************************************/

redraw_needed = TITLEBAR_MASK & FULL_REDRAW;

if (dmc->ww.dash_c)
   cursor_buff->current_win_buff->window_wrap_col = dmc->ww.dash_c;
else
   if (dmc->ww.mode == DM_ON)
      cursor_buff->current_win_buff->window_wrap_col = min_chars_on_window(cursor_buff->current_win_buff->window);
   else
      if (dmc->ww.mode == DM_OFF)
         cursor_buff->current_win_buff->window_wrap_col = 0;  /* value zero doubles as being False */
      else
         /***************************************************************
         *  DM toggle mode only takes place if there are no other
         *  parms.
         ***************************************************************/
         if (! (dmc->ww.dash_a || dmc->ww.dash_i))
            if (cursor_buff->current_win_buff->window_wrap_col)
               cursor_buff->current_win_buff->window_wrap_col = 0;
            else
               cursor_buff->current_win_buff->window_wrap_col = min_chars_on_window(cursor_buff->current_win_buff->window);
         else
            redraw_needed = 0;  /* not changing the ww mode */
         

/***************************************************************
*  
*  if -a was specified and we are in "ON" mode, do the text
*  manipulation.
*  
***************************************************************/

if (cursor_buff->current_win_buff->window_wrap_col && dmc->ww.dash_a)
   redraw_needed |= ww_dash_a(cursor_buff, mark1);


/***************************************************************
*  
*  Finally, if there was a -i, generate the message with the
*  ww col in it.  We are zero based, the screen is one based,
*  so we add one to our number for printing.
*  
***************************************************************/

if (dmc->ww.dash_i)
   {
      snprintf(msg, sizeof(msg), "wordwrap character column: %d", cursor_buff->current_win_buff->window_wrap_col + 1);
      dm_error(msg, DM_ERROR_MSG);
   }


return(redraw_needed);

}  /* end of dm_ww */


/************************************************************************

NAME:      ww_dash_a

PURPOSE:    This routine performs -a text manipulation for the command "ww -a".

PARAMETERS:

   1.  cursor_buff    - pointer to BUFF_DESCR (INPUT)
                        The current cursor description is used to check which window we are in and
                        to set the window wrap option.


FUNCTIONS :

   1.   If the length of the current line is less than the wrap col length,
        join lines until it is.

   2.   Walk backwards in the line breaking it into pieces of < window wrap col length.

   3.   Rewrite out the original line.

   4.   Position the cursor in col 0 of the line past the modification.

OUTPUTS:
   redraw -  The mask anded with the type of redraw needed is returned.

*************************************************************************/

static int ww_dash_a(BUFF_DESCR      *cursor_buff,            /* input  / output */
                     MARK_REGION     *mark1)                  /* input  */
{
int        target_lineno;
char      *split_pos;
char      *split_limit;
char      *curr_pos;
char      *next_line;
int        build_len;
DMC        tf_dmc;
int        did_a_split_or_join = False;

flush(cursor_buff->current_win_buff);
target_lineno = cursor_buff->current_win_buff->file_line_no;


/***************************************************************
*  
*  Check for a window wrap over a marked region.  If so, do
*  a text flow instead.
*  
***************************************************************/

if (mark1->mark_set)
   {
      memset((char *)&tf_dmc, 0, sizeof(tf_dmc));
      tf_dmc.tf.cmd    = DM_tf;
      tf_dmc.tf.dash_r = cursor_buff->current_win_buff->window_wrap_col;
      stop_text_highlight(mark1->buffer->display_data);
      mark1->buffer->display_data->echo_mode = NO_HIGHLIGHT;
      return(dm_tf(&tf_dmc, mark1, cursor_buff, mark1->buffer->display_data->echo_mode));
   }

/***************************************************************
*  
*  Make sure the line is long enough.
*  
***************************************************************/

build_len = strlen(cursor_buff->current_win_buff->buff_ptr);

if ((cursor_buff->current_win_buff->file_line_no < total_lines(cursor_buff->current_win_buff->token)-1)
   && (build_len != 0)
   && (build_len <  cursor_buff->current_win_buff->window_wrap_col))
{
   next_line = get_line_by_num(cursor_buff->current_win_buff->token, cursor_buff->current_win_buff->file_line_no+1);
   if (next_line && next_line[0])
      {
         if (cursor_buff->current_win_buff->buff_ptr[build_len-1] != ' ')
            {
               strcpy(cursor_buff->current_win_buff->work_buff_ptr, cursor_buff->current_win_buff->buff_ptr);
               cursor_buff->current_win_buff->buff_ptr = cursor_buff->current_win_buff->work_buff_ptr;
               cursor_buff->current_win_buff->buff_ptr[build_len] = ' ';
               cursor_buff->current_win_buff->buff_ptr[build_len+1] = '\0';
               put_line_by_num(cursor_buff->current_win_buff->token, cursor_buff->current_win_buff->file_line_no, cursor_buff->current_win_buff->buff_ptr, OVERWRITE);
            }
         did_a_split_or_join = True;
         if (COLORED(cursor_buff->current_win_buff->token))
            {
               cd_flush(cursor_buff->current_win_buff);
               cd_join_line(cursor_buff->current_win_buff->token,
                            cursor_buff->current_win_buff->file_line_no,
                            strlen(cursor_buff->current_win_buff->buff_ptr));
            }
         join_line(cursor_buff->current_win_buff->token, cursor_buff->current_win_buff->file_line_no);
         cursor_buff->current_win_buff->buff_ptr = get_line_by_num(cursor_buff->current_win_buff->token, cursor_buff->current_win_buff->file_line_no);
      }
}

/***************************************************************
*  
*  Walk forwards looking for places to split.
*  
***************************************************************/

while (strlen(cursor_buff->current_win_buff->buff_ptr) >  (size_t)cursor_buff->current_win_buff->window_wrap_col)
{
   curr_pos = cursor_buff->current_win_buff->buff_ptr;
   split_limit = curr_pos + cursor_buff->current_win_buff->window_wrap_col;
   split_pos = NULL;
   while((*curr_pos) && ((curr_pos <= split_limit) || (split_pos == NULL)))
   {
      if ((*curr_pos == ' ') || (*curr_pos == '\t'))
         split_pos = curr_pos;
      curr_pos++;
   }

   if (split_pos == NULL)
      break;
   else
      {
         did_a_split_or_join = True;
         split_line(cursor_buff->current_win_buff->token, target_lineno, (split_pos-cursor_buff->current_win_buff->buff_ptr)+1, True);
         target_lineno++;
         cursor_buff->current_win_buff->buff_ptr = get_line_by_num(cursor_buff->current_win_buff->token, target_lineno);
      }
}

if (!did_a_split_or_join)
   target_lineno++;

(void) dm_position(cursor_buff, cursor_buff->current_win_buff, target_lineno, 0);

return(cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW);

} /* end of ww_dash_a */


/************************************************************************

NAME:      ww_line - Window wrap a line during interactive (DM_es) processing

PURPOSE:    This routine performs the normal interactive window wrapping.  It
            is called from dm_es when the typed length exceeds the window wrap
            column.

PARAMETERS:

   1.  cursor_buff    - pointer to BUFF_DESCR (INPUT)
                        The current cursor description.


FUNCTIONS :

   1.   Find a blank or tab to split on walking backwards from the
        window wrap column.

   2.   Split the line and set up the redrawing mask to return.

   3.   If the cursor is past the break pos, move the cursor to the next line.
        Scroll up a line if we are at the last line on the screen already.

   4.   Position the cursor in col 0 of the line past the modification.

OUTPUTS:
   redraw -  The mask anded with the type of redraw needed is returned.

*************************************************************************/

int  ww_line(BUFF_DESCR      *cursor_buff)
{

int       split_pos;
int       redraw_needed = 0;
int       new_col;
int       new_line;
int       new_line_len;
char     *next_line;
int       need_extra_blank;

/***************************************************************
*  
*  Find the place to split the line.
*  
***************************************************************/

flush(cursor_buff->current_win_buff);
for (split_pos = cursor_buff->current_win_buff->window_wrap_col;
     split_pos > 0;
     split_pos--)
   if (cursor_buff->current_win_buff->buff_ptr[split_pos] == ' ' || cursor_buff->current_win_buff->buff_ptr[split_pos] == '\t')
      break;

split_pos++; /* break after the blank */
if (split_pos == (int)strlen(cursor_buff->current_win_buff->buff_ptr))
   return(0);

DEBUG7(fprintf(stderr, "ww_line:  Splitting line %d at col %d\n", cursor_buff->current_win_buff->file_line_no, split_pos);)

if (split_pos > 0)
   {
      /***************************************************************
      *  
      *  Split the line and set up the redraw.
      *  
      ***************************************************************/

      split_line(cursor_buff->current_win_buff->token, 
                 cursor_buff->current_win_buff->file_line_no,
                 split_pos, True);
      cursor_buff->current_win_buff->buff_ptr = get_line_by_num(cursor_buff->current_win_buff->token, 
                                                                cursor_buff->current_win_buff->file_line_no);

      redraw_needed |= redraw_start_setup(cursor_buff); /* in typing.c */
      redraw_needed |= (cursor_buff->current_win_buff->redraw_mask & PARTIAL_REDRAW);

      /***************************************************************
      *  
      *  If the cursor is past the break pos, move the cursor to the next line.
      *  
      ***************************************************************/

      if (split_pos <= cursor_buff->current_win_buff->file_col_no)
         {
            if (cursor_buff->win_line_no == cursor_buff->current_win_buff->window->lines_on_screen - 1)
               {
                  DEBUG7(fprintf(stderr, "ww_line:  Scrolling up one line\n");)
                  cursor_buff->current_win_buff->first_line++;
                  redraw_needed = (cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW);
               }
            new_line = cursor_buff->current_win_buff->file_line_no + 1;
            new_col  = cursor_buff->current_win_buff->file_col_no - split_pos;
            DEBUG7(fprintf(stderr, "ww_line:  Moving cursor to [%d,%d]\n", new_line, new_col);)
            redraw_needed |= dm_position(cursor_buff, cursor_buff->current_win_buff, new_line, new_col);
         }
      else
         new_line = cursor_buff->current_win_buff->file_line_no + 1;

      /***************************************************************
      *  
      *  If the stuff we pushed onto the next line needs to be joined
      *  to the line after that, do the join.
      *  
      ***************************************************************/

      next_line = get_line_by_num(cursor_buff->current_win_buff->token, new_line);
      if (next_line != NULL)  /* this better work since we just did a line split to create this line */
         {
            new_line_len = strlen(next_line);
            if ((next_line[new_line_len-1] != ' ') && (next_line[new_line_len-1] != '\t'))
               need_extra_blank = 1;
            else
               need_extra_blank = 0;
            next_line = get_line_by_num(cursor_buff->current_win_buff->token, new_line+1);
            if ((next_line[0] == ' ') || (next_line[0] == ' '))
               need_extra_blank = 0;
            if ((next_line != NULL) && (next_line[0] != '\0') && (need_extra_blank + new_line_len + (int)strlen(next_line) < cursor_buff->current_win_buff->window_wrap_col))
               {
                  DEBUG7(fprintf(stderr, "ww_line:  Joining line %d to %d, %s to add blank\n", new_line+1, new_line, (need_extra_blank ? "need to" : "do not need to"));)
                  if (need_extra_blank)
                     {  
                        char    work[MAX_LINE+4];
                        work[0] = ' ';
                        work[1] = '\0';
                        strcat(work, next_line);
                        put_line_by_num(cursor_buff->current_win_buff->token, new_line+1, work, OVERWRITE);
                     }
                  if (COLORED(cursor_buff->current_win_buff->token))
                     {
                        cd_flush(cursor_buff->current_win_buff);
                        cd_join_line(cursor_buff->current_win_buff->token, new_line, new_line_len);
                     }
                  join_line(cursor_buff->current_win_buff->token, new_line);
                  if (new_line == cursor_buff->current_win_buff->file_line_no)
                     cursor_buff->current_win_buff->buff_ptr = get_line_by_num(cursor_buff->current_win_buff->token, new_line);
               }
         }
   }

return(redraw_needed);

} /* end of ww_line */
  



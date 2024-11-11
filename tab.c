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
*  module tab.c
*
*  Routines:
*     dm_ts                 -   Set Tab stops
*     dm_th                 -   Tab Horizontal (right and left)
*     untab                 -   expand tab characters for drawing
*     tab_cursor_adjust     -   adjust cursor position withing a window
*     tab_pos               -   Calculate tab position in a line
*     set_window_col_from_file_col - set window col and x value from file col
*     dm_hex                -   Turn hex mode display on an off.
*     dm_untab              -   Change tabs to blanks according to the current tab stops.
*     window_col_to_x_pixel -   Calculate 'x' pixel position for a given window column
*
* Internal:
*     process_char        -   detect and process special characters.
*     
***************************************************************/
    
#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h      */

#ifndef WIN32
#include <X11/Xlib.h>       /* /usr/include/X11/Xlib.h   */
#endif

#include "borders.h"
#include "buffer.h"
#include "dmsyms.h"
#include "dmwin.h"
#include "emalloc.h"
/*#include "memdata.h"   */
#include "mark.h"
#include "tab.h"
#include "txcursor.h"
#include "typing.h"

int process_char(char   c_in,
                 char  *out_str);

/***************************************************************
*  
*  This is the initial default tab setting.  Tabs are set every
*  5 characters.  Note that in this copy only, if a tab stop
*  array is present (due to a ts command), the tab stop positions
*  are zero based instead of 1 based.
*  
***************************************************************/


static DMC_ts   current_ts = {NULL,      /* next pointer      */
                              DM_ts,     /* command id        */
                              0,         /* not temporary     */
                              4,         /* -r every 5 chars  */  /* DEBUGGING, set to 4 to match Bob's node */
                              0,         /* zero stops        */
                              NULL};     /* no tab stop array */



/************************************************************************

NAME:      dm_ts

PURPOSE:    This routine save a new set of tab settings. 

PARAMETERS:

   1.  dmc            - pointer to DMC (INPUT)
                        This is the command structure for the ts command.
                        It contains the data from any dash options and the list of tab stops

GLOBAL DATA

   1.  current_ts     - pointer to DMC_ts (output);
                        These are the current tab settings. They are updated.

FUNCTIONS :


OUTPUTS:
   redraw -  The mask anded with the type of redraw needed is returned.
             A full redraw is always requested.

*************************************************************************/

int  dm_ts(DMC             *dmc)                    /* input  */
{

int         i;

/***************************************************************
*  
*  If there already is a tab stop array allocated, get rid of it.
*  Then copy over the base ts structure.
*  
***************************************************************/

if (current_ts.stops)
   free((char *)current_ts.stops);

memcpy((char *)&current_ts, (char *)dmc, sizeof(DMC_ts));


/***************************************************************
*  
*  Copy over the tab stops.  Subtract 1 to make them zero based
*  instead of 1 based.
*  
***************************************************************/

if (dmc->ts.stops)
   {
      current_ts.stops  = (short int *)CE_MALLOC(dmc->ts.count * sizeof(short int));
      if (!current_ts.stops)
         return(0);
      for (i = 0; i < dmc->ts.count; i++)
         current_ts.stops[i] = dmc->ts.stops[i] - 1;
   }

return(MAIN_PAD_MASK & FULL_REDRAW);

}  /* end of dm_ts */


/************************************************************************

NAME:      untab - expand tab characters and handle other special characters.

PURPOSE:    This routine scans for tabs, form feeds, and special characters.
            If it finds any, it copies parm "from" to parm "to" expanding
            the tabs and converting form feeds.  If global hex_mode
            is true, tabs are represented as \t, special characters are
            represented as \<nn> where <nn> is a two digit hex code.

PARAMETERS:

   1.  from          -  pointer to char (INPUT)
                        This is the input line to be processed.

   2.  to            -  pointer to char (OUTPUT)
                        This is the line with expanded tabs and translated
                        characters.  If there is are none, this parm is not
                        modified.

   3.  limit_len     -  unsigned int (INPUT)
                        This number limits the number of characters to search
                        in the initial search for tabs and other special 
                        characters.  Once tabs are identified, the entire line
                        is processed.  Pass (unsigned int)-1 or MAX_LINE+1
                        to effectively disable this check.

   4.  hex_mode      -  int (INPUT)
                        When True, this flag specifies that we are in hex mode.

FUNCTIONS :

   1.  Check if there are special chars to be translated.
       If not, return zero.

   2.  If the expand tabs flag is on, expand tabs according to the tab stops
       and change form feeds to blanks.

   3.  If the expand tabs flag is off, convert non-ascii characters < x20 and > x7e
       to multiple characters.

RETURNED

converstion_done  -  int
                     If nonzero, the returned flag specifies that a translation
                     was done and parm "to" contains the translated string.
                     0                   - No special characters found
                     TAB_TAB_FOUND       - one or more tabs or special chars were found.
                     TAB_FORMFEED_FOUND  - A formfeed was found

*************************************************************************/

int untab(char          *from,
          char          *to,
          unsigned int   limit_len,
          int            hex_mode)
{
char     *p;
char     *q;
int       pos;
int       i;
int       rc;
int       last_tab;
int       offset_from_last_tab;
int       whole_tabs;
int       pad_chars;
int       tab_stop_idx = 0;

/***************************************************************
*  
*  If there are no tabs, do nothing.
*  If we are not expanding tabs, we will always be putting
*  the $ on at least.
*  
***************************************************************/

if (from == NULL)
   from = "";

if (hex_mode)
   rc = TAB_TAB_FOUND;
else
   {
      pos = 0;
      p = from;
      rc = False;
      while (*p && ((unsigned)pos < limit_len))
      {
         if (*p == '\t')
            rc |= TAB_TAB_FOUND;
         if (*p == '\f')
            {
               rc |= TAB_FORMFEED_FOUND;
               break;
            }
         p++;
         pos++;
      }

      if (!rc)
         return(rc);

   }


/***************************************************************
*  
*  Copy the string expanding tabs.
*  
***************************************************************/

p = from;
q = to;

while(*p)
{
   if (hex_mode)
      /***************************************************************
      *  not expanding tabs. show special characters as \values, copy regular chars.
      ***************************************************************/
      q += process_char(*p, q);
   else
      if (*p != TAB)
         if (*p != FORMFEED)
            *q++ = *p;
         else
            *q++ = ' '; /* replace form feed in display line by blank */
      else
         {
            pos = q - to;         
            last_tab = (current_ts.stops == 0) ? -1 : current_ts.stops[current_ts.count-1];
            if (last_tab <= pos)
               {
                  /***************************************************************
                  *  The tab char is past the last tab stop, use the repeat factor.
                  ***************************************************************/
                  offset_from_last_tab = pos - last_tab;
                  whole_tabs = (offset_from_last_tab / current_ts.dash_r) * current_ts.dash_r;
                  pad_chars = current_ts.dash_r - (offset_from_last_tab - whole_tabs);
               }
            else
               {
                  whole_tabs = 0;
   
                  while (whole_tabs == 0)
                     if (pos < current_ts.stops[tab_stop_idx])
                        whole_tabs = current_ts.stops[tab_stop_idx];
                     else
                        tab_stop_idx++;
   
                  pad_chars = (whole_tabs - pos);
               }
            if ((pad_chars + (q - to)) > MAX_LINE)
               {
                  dm_error("(untab) Line too long", DM_ERROR_BEEP);
                  break;
               }
            for (i = 0; i < pad_chars; i++)
                *q++ = ' ';
            DEBUG8(fprintf(stderr, "(untab) Padding with %d chars to col %d\n", pad_chars, q-to);)
         }

   /***************************************************************
   *  
   *  Move on to the next character
   *  
   ***************************************************************/
   p++;
   if ((q - to) > MAX_LINE)
      {
         dm_error("(untab) Line too long", DM_ERROR_BEEP);
         break;
      }
}

if (hex_mode)
   *q++ = '$';
*q = '\0';

return(rc);

} /* end untab */


int  tab_cursor_adjust(char           *line,            /* input  */
                       int             offset,          /* input  */
                       int             first_char,      /* input  */
                       int             mode,            /* input  TAB_WINDOW_COL, TAB_LINE_OFFSET, TAB_EXPAND_POS, TAB_PADDING */
                       int             hex_mode)
{

char     *p;
int       q_pos;
int       new_q_pos;
int       last_tab;
int       offset_from_last_tab;
int       whole_tabs;
int       pad_chars;
int       tab_start_on_window;
int       tab_end_on_window;
int       tab_stop_idx = 0;
int       adjusted_pos;


if (line == NULL)
   line = "";

/***************************************************************
*  
*  To start with:
*  We have the line of text with the tabs in it and the column
*  number of the cursor on untab'ed text.  If we are over the
*  area expanded to by a tab, we want to put the cursor over
*  the first tab char.
*
*  What we will do is walk the string like we are doing an untab.
*  We will not copy anything, just bump column numbers.
*  If the tab pos plus the tab change spans the current position,
*  we are under a tab.
*  
***************************************************************/


p = line;
q_pos = 0;

while(*p)
{
   if ((mode == TAB_EXPAND_POS) && ((p - line) >= offset))
      return(q_pos-first_char); /* -firstchar added 1/15/97 RES, fix horizontal scrolling of colored areas */

   if (mode != TAB_EXPAND_POS && (q_pos - first_char) >= offset)
      if (mode == TAB_WINDOW_COL)
         return(offset);
      else
         if (mode == TAB_PADDING)
            return(1);
         else
            return(p-line);  /* must be TAB_LINE_OFFSET */

   if (!hex_mode)
      {
         if (*p != TAB)
            q_pos++;
         else
            {
               last_tab = (current_ts.stops == 0) ? -1 : current_ts.stops[current_ts.count-1];
               if (last_tab <= q_pos)
                  {
                     /***************************************************************
                     *  The tab char is past the last tab stop, use the repeat factor.
                     ***************************************************************/
                     offset_from_last_tab = q_pos - last_tab;
                     whole_tabs = (offset_from_last_tab / current_ts.dash_r) * current_ts.dash_r;
                     pad_chars = current_ts.dash_r - (offset_from_last_tab - whole_tabs);
                  }
               else
                  {
                     whole_tabs = 0;

                     /***************************************************************
                     *  Scan looking for the next tab stop past the current position.
                     *  Tab_stop_idx is not reset for each loop because we only
                     *  need to go forward in the loop.  We do not have to check for
                     *  end of list because of the check that last_tab > q_pos for this
                     *  piece of code.
                     ***************************************************************/
                     while (whole_tabs == 0)
                        if (q_pos < current_ts.stops[tab_stop_idx])
                           whole_tabs = current_ts.stops[tab_stop_idx];
                        else
                           tab_stop_idx++;

                     pad_chars = whole_tabs - q_pos;
                  }

               /***************************************************************
               *  Find where the tab starts and ends on the window.  Then
               *  if the window column is in this range, we have found the case
               *  where we are in the middle of a tab and need to adjust to the
               *  beginning of the tab.
               ***************************************************************/
               new_q_pos = q_pos + pad_chars;
               tab_start_on_window = q_pos - first_char;
               tab_end_on_window   = new_q_pos - first_char;
               if (mode != TAB_EXPAND_POS && tab_end_on_window > offset)
                  {
                     DEBUG8(fprintf(stderr, "In a tab, changing col %d to %d, tab end = %d\n", offset, tab_start_on_window, tab_end_on_window);)
                     if (mode == TAB_WINDOW_COL)
                        {
                           /***************************************************************
                           *  This handles the case of a tab which straddles the left boundary
                           *  of the window.  In this case, force the cursor to the next char.
                           ***************************************************************/
                           if (tab_start_on_window >= 0)
                              return(tab_start_on_window);
                           else
                              return(tab_end_on_window);
                        }
                     else
                        if (mode == TAB_PADDING)
                           return(pad_chars);
                        else
                           return(p-line); /* must be TAB_LINE_OFFSET */
                  }
               else
                  q_pos = new_q_pos;
            }  /* else it was a tab */
      }  /* expand tabs is on */
   else
      {
         /***************************************************************
         *  This code is copied from above.  The variable names reflect
         *  tabs, but all characters are being dealt with here.
         ***************************************************************/
         pad_chars = process_char(*p, NULL);
         new_q_pos = q_pos + pad_chars;
         tab_start_on_window = q_pos - first_char;
         tab_end_on_window   = new_q_pos - first_char;
         if (mode != TAB_EXPAND_POS && tab_end_on_window > offset)
            {
               if (mode == TAB_WINDOW_COL)
                  {
                     /***************************************************************
                     *  This handles the case of a special char which straddles the left boundary
                     *  of the window.  In this case, force the cursor to the next char.
                     ***************************************************************/
                     if (tab_start_on_window >= 0)
                        return(tab_start_on_window);
                     else
                        return(tab_end_on_window);
                  }
               else
                  if (mode == TAB_PADDING)
                     return(pad_chars);
                  else
                     return(p-line); /* must be TAB_LINE_OFFSET */
            }
         else
            q_pos = new_q_pos;
      }

   p++;

}

/***************************************************************
*  
*  We ran out of line before we got to the window pos.  
*  assume blanks for the rest
*  
***************************************************************/

switch (mode)
{
case TAB_WINDOW_COL:
   adjusted_pos = offset;
   break;

case TAB_LINE_OFFSET:
   adjusted_pos = (p-line) + (offset - (q_pos - first_char));
   break;

case TAB_EXPAND_POS:
   adjusted_pos = (q_pos + (offset - (p-line)))-first_char; /* -firstchar added 1/15/97 RES, fix horizontal scrolling of colored areas */
   break;

case TAB_PADDING:
   adjusted_pos = 1;
   break;

default:
   dm_error("program error in tab_cursor_adjust", DM_ERROR_LOG);
   adjusted_pos = 1;
   break;

} /* end of switch */

return(adjusted_pos);


} /* end of tab_cursor_adjust */


 

int  tab_pos(char           *line,            /* input  */
             int             offset,          /* input  */
             int             mode,            /* input  TAB_EXPAND or TAB_CONTRACT or TAB_PADDING */
             int             hex_mode)
{

char     *p;
int       q_pos;
int       new_q_pos;
int       last_tab;
int       offset_from_last_tab;
int       whole_tabs;
int       pad_chars;
int       tab_start_on_window;
int       tab_end_on_window;
int       tab_stop_idx = 0;
int       adjusted_pos;

if (line == NULL)
   line = "";

/***************************************************************
*  
*  To start with:
*  We have the line of text with the tabs in it and the column
*  number of the cursor on untab'ed text.  If we are over the
*  area expanded to by a tab, we want to put the cursor over
*  the first tab char.
*
*  What we will do is walk the string like we are doing an untab.
*  We will not copy anything, just bump column numbers.
*  If the tab pos plus the tab change spans the current position,
*  we are under a tab.
*  
***************************************************************/


p = line;
q_pos = 0;

while(*p)
{
   if ((mode == TAB_EXPAND) && ((p - line) >= offset))
      return(q_pos);

   if (mode != TAB_EXPAND && q_pos >= offset)
      if (mode == TAB_CONTRACT)
         return(p-line);
      else
         return(1); /* tab padding */


   if (!hex_mode)
      {
         if (*p != TAB)
            q_pos++;
         else
            {
               last_tab = (current_ts.stops == 0) ? -1 : current_ts.stops[current_ts.count-1];
               if (last_tab <= q_pos)
                  {
                     /***************************************************************
                     *  The tab char is past the last tab stop, use the repeat factor.
                     ***************************************************************/
                     offset_from_last_tab = q_pos - last_tab;
                     whole_tabs = (offset_from_last_tab / current_ts.dash_r) * current_ts.dash_r;
                     pad_chars = current_ts.dash_r - (offset_from_last_tab - whole_tabs);
                  }
               else
                  {
                     whole_tabs = 0;

                     /***************************************************************
                     *  Scan looking for the next tab stop past the current position.
                     *  Tab_stop_idx is not reset for each loop because we only
                     *  need to go forward in the loop.  We do not have to check for
                     *  end of list because of the check that last_tab > q_pos for this
                     *  piece of code.
                     ***************************************************************/
                     while (whole_tabs == 0)
                        if (q_pos < current_ts.stops[tab_stop_idx])
                           whole_tabs = current_ts.stops[tab_stop_idx];
                        else
                           tab_stop_idx++;

                     pad_chars = whole_tabs - q_pos;
                  }

               /***************************************************************
               *  Find where the tab starts and ends on the window.  Then
               *  if the window column is in this range, we have found the case
               *  where we are in the middle of a tab and need to adjust to the
               *  beginning of the tab.
               ***************************************************************/
               new_q_pos = q_pos + pad_chars;
               tab_start_on_window = q_pos;
               tab_end_on_window   = new_q_pos ;
               if ((mode == TAB_EXPAND) && ((p - line) >= offset))
                  return(tab_start_on_window);
               if (mode != TAB_EXPAND && tab_end_on_window > offset)
                  {
                     DEBUG8(fprintf(stderr, "tab_pos: In a tab, changing col %d to %d, tab end = %d\n", offset, tab_start_on_window, tab_end_on_window);)
                     if (mode == TAB_CONTRACT)
                        return(p-line);
                     else
                        return(pad_chars); /* must be tab padding */
                  }
               else
                  q_pos = new_q_pos;

            } /* else it is a tab */
      } /* expand tabs is on */
   else
      {
         /***************************************************************
         *  This code is copied from above.  The variable names reflect
         *  tabs, but all characters are being dealt with here.
         ***************************************************************/
         pad_chars = process_char(*p, NULL);
         new_q_pos = q_pos + pad_chars;
         tab_start_on_window = q_pos;
         tab_end_on_window   = new_q_pos ;
         if ((mode == TAB_EXPAND) && ((p - line) >= offset))
            return(tab_start_on_window);
         if (mode != TAB_EXPAND && tab_end_on_window > offset)
            {
               if (mode == TAB_CONTRACT)
                  return(p-line);
               else
                  return(pad_chars); /* must be tab padding */
            }
         else
            q_pos = new_q_pos;

      }

   p++;
}

/***************************************************************
*  
*  We ran out of line before we got to the offset
*  assume blanks for the rest
*  
***************************************************************/

switch (mode)
{
case TAB_EXPAND:
   adjusted_pos = q_pos + (offset - (p - line));
   break;

case TAB_CONTRACT:
   adjusted_pos = (p - line) + (offset - q_pos);
   break;

case TAB_PADDING:
   adjusted_pos = 1;
   break;

default:
   dm_error("program error in tab_pos", DM_ERROR_LOG);
   adjusted_pos = 1;
   break;

} /* end of switch */

return(adjusted_pos);


} /* end of tab_pos */

int process_char(char   c_in,
                 char  *out_str)
{
int      len;
char     scrap[4];

if (!out_str)
   out_str = scrap;

if (c_in >= 0x20 && c_in <= 0x7E)
   {
      *out_str = c_in;  /* A normal ascii character */
      return(1);
   }
else
   len = 2;  /* start with the common value */

switch(c_in)
{
case '\b':
   strcpy(out_str, "\\b");
   break;
case '\f':
   strcpy(out_str, "\\f");
   break;
case '\n':
   strcpy(out_str, "\\n");
   break;
case '\r':
   strcpy(out_str, "\\r");
   break;
case '\t':
   strcpy(out_str, "\\t");
   break;
case '\v':
   strcpy(out_str, "\\v");
   break;
default:
   sprintf(out_str, "\\%02X", (unsigned char)c_in);
   len = 3;
   break;
} /* end of switch */

return(len);

} /* end of get_non_ascii_char */
 


/************************************************************************

NAME:      set_window_col_from_file_col

PURPOSE:    This routine updates a cursor buff and the current window buff it
            references to contain the correct window_column and x values
            from a file column number.  This means adjusting for the effect
            of tabs.

PARAMETERS:

   1.  cursor_buff    - pointer to BUFF_DESCR (INPUT / OUTPUT)
                        This is the cursor buffer description to be updated.
                        The file_col_no currenly set in the buffer is used to
                        set the window_col number and x values.

FUNCTIONS :

   1.  Translate the file_column number to a column number with tabs expaneded.
       Then subtract off the first char displayed to get the window column number.

   2.  From the window column number, calculate the x value for  this character.

   3.  If the position is off the right edge of the window, position at the right
       edge.

RETURNED VALUE:
   adjusted  -   int   True / False
                 If the column number had to be adjusted because the required
                 X coordinate was off the right side of the window, True is
                 returned.


*************************************************************************/

int set_window_col_from_file_col(BUFF_DESCR      *cursor_buff) /* input / output */
{
int       pos;
int       i;
int       adjusted = False;

/***************************************************************
*  
*  Find the pos of the line in the untabbed line.  
*  Then check to make sure the left_edge_pos is not in the
*  middle of a tab.  If so, adjust the column.
*
*  We set win_col_no and x
*  
***************************************************************/

if (cursor_buff->current_win_buff->buff_ptr == NULL)
   cursor_buff->current_win_buff->buff_ptr =  "";

pos = tab_pos(cursor_buff->current_win_buff->buff_ptr,
              cursor_buff->current_win_buff->file_col_no,
              TAB_EXPAND,
              cursor_buff->current_win_buff->display_data->hex_mode);

cursor_buff->win_col_no = pos - cursor_buff->current_win_buff->first_char;
if (cursor_buff->win_col_no < 0)
   cursor_buff->win_col_no = 0;

/***************************************************************
*  
*  Set the X value from the window column number.
*  
***************************************************************/

if (cursor_buff->current_win_buff->window->fixed_font)
   cursor_buff->x = (cursor_buff->current_win_buff->window->fixed_font * cursor_buff->win_col_no) + cursor_buff->current_win_buff->window->sub_x;
else
   cursor_buff->x = window_col_to_x_pixel(cursor_buff->current_win_buff->buff_ptr,
                                          cursor_buff->win_col_no,
                                          cursor_buff->current_win_buff->first_char,
                                          cursor_buff->current_win_buff->window->sub_x,
                                          cursor_buff->current_win_buff->window->font, 
                                          cursor_buff->current_win_buff->display_data->hex_mode) + 1;

/***************************************************************
*  
*  If we have gone off screen, position to the last char on the
*  screen.
*  
***************************************************************/

if ((unsigned)cursor_buff->x >= ((cursor_buff->current_win_buff->window->sub_width+cursor_buff->current_win_buff->window->sub_x) - BORDER_WIDTH))
   {
      adjusted = True;

      cursor_buff->x = (cursor_buff->current_win_buff->window->sub_width+cursor_buff->current_win_buff->window->sub_x) - 3; /* move to within a letter */
      if (cursor_buff->current_win_buff->window->fixed_font)
         cursor_buff->win_col_no = which_char_on_fixed_line(cursor_buff->current_win_buff->window,
                                                            cursor_buff->x,
                                                            cursor_buff->current_win_buff->window->fixed_font,
                                                            &i);  /* junk parm */
      else
         cursor_buff->win_col_no = which_char_on_line(cursor_buff->current_win_buff->window,
                                                  cursor_buff->current_win_buff->window->font,
                                                  cursor_buff->x,
                                                  cursor_buff->current_win_buff->buff_ptr,
                                                  &i,     /* junk parm */
                                                  &pos);  /* junk parm */

      cursor_buff->current_win_buff->file_col_no = tab_cursor_adjust(cursor_buff->current_win_buff->buff_ptr,
                                                                     cursor_buff->win_col_no, 
                                                                     cursor_buff->current_win_buff->first_char,
                                                                     TAB_LINE_OFFSET,
                                                                     cursor_buff->current_win_buff->display_data->hex_mode);



   }

return(adjusted);

} /* end of set_window_col_from_file_col */


/************************************************************************

NAME:      window_col_to_x_pixel - Calculate 'x' pixel position for a given window column

PURPOSE:    This routine calculates the 'x' pixel position of the left
            edge of a given window column.  This calculation works
            for both fixed and variable fonts.

PARAMETERS:

   1.  buff_ptr       - pointer to char (INPUT)
                        This is the text line at on that line on the screen.

   2.  win_col        - int (INPUT)
                        This is the column on the window we are interested in.
                        This number is always >= 0.

   3.  first_char     - int (INPUT)
                        This is the first column (zero based) displayed in the
                        window.  That is, if we are scrolled over 5 characters
                        to the right, the value will be 5.  This number is
                        always zero or greater.

   4.  subwindow_x    - int (INPUT)
                        This is the x offset into the real window of the subwindow.
                        This number gets added in to the returned value.

   5.  font           - pointer to XFontStruct (INPUT)
                        This is the font data currently in use.

   6.  hex_mode      -  int (INPUT)
                        When True, this flag specifies that we are in hex mode.

FUNCTIONS :

   1.  Untab the line.

   2.  Adjust the passed line start position by the first column displayed in the window.

   3.  Calculate the x pixel position of the window column.  If the position
       is past the end of the line assume blanks.

RETURNED VALUE:
   x    -   int
            The X coordinate for the window is returned.


*************************************************************************/

int window_col_to_x_pixel(char         *buff_ptr,
                          int           win_col,
                          int           first_char,
                          int           subwindow_x,
                          XFontStruct  *font,
                          int           hex_mode)
{
char     *line;
int       len;
char      work[MAX_LINE+1];
int       x;

if (untab(buff_ptr, work, MAX_LINE+1, hex_mode) != 0)
   line = work;
else
   line = buff_ptr;

len = strlen(line) - first_char;
if (len >= 0)
   line += first_char;
else
   {
      line = "";
      len = 0;
   }
if (win_col <= len)
   x = XTextWidth(font, line, win_col) + subwindow_x;
else
   x = XTextWidth(font, line, len)
     + (XTextWidth(font, " ", 1) * (win_col - len)) 
     + subwindow_x;

return(x);

} /* end of window_col_to_x_pixel */


/************************************************************************

NAME:      dm_th - Tab Horizontal (right and left)

PURPOSE:    This routine calculates the 'x' pixel position of the left
            edge of a given window column.  This calculation works
            for both fixed and variable fonts.

PARAMETERS:

   1.  cmd_id         - int (INPUT)
                        This is the command id.  It is either DM_th or DM_thl

   2.  cursor_buff    - pointer to BUFF_DESCR (INPUT / OUTPUT)
                        This is the cursor buffer description to be updated.

FUNCTIONS :

   1.  Calculate the new tab position and adjust the cursor buff to point there

*************************************************************************/

void dm_th(int              cmd_id,
           BUFF_DESCR      *cursor_buff)
{
int       last_tab;
int       q_pos;
int       offset_from_last_tab;
int       whole_tabs;
int       pad_chars;
int       pad_back_chars;
int       tab_stop_idx;
int       tab_end;
int       tab_start;


DEBUG5(fprintf(stderr, "(%s): start: file [%d,%d] win [%d,%d], %s \"%s\"\n",
       dmsyms[cmd_id].name,
       cursor_buff->current_win_buff->file_line_no, cursor_buff->current_win_buff->file_col_no, cursor_buff->win_line_no, cursor_buff->win_col_no,
       which_window_names[cursor_buff->which_window], cursor_buff->current_win_buff->buff_ptr);
)

last_tab = (current_ts.stops == 0) ? -1 : current_ts.stops[current_ts.count-1];
q_pos    = tab_pos(cursor_buff->current_win_buff->buff_ptr,
                   cursor_buff->current_win_buff->file_col_no,
                   TAB_EXPAND,
                   cursor_buff->current_win_buff->display_data->hex_mode);

if (last_tab <= q_pos)
   {
      /***************************************************************
      *  The tab char is past the last tab stop, use the repeat factor.
      ***************************************************************/
      offset_from_last_tab = q_pos - last_tab;
      whole_tabs = (offset_from_last_tab / current_ts.dash_r) * current_ts.dash_r;
      pad_chars      = current_ts.dash_r - (offset_from_last_tab - whole_tabs);
      pad_back_chars = offset_from_last_tab - whole_tabs;
      if (pad_back_chars == 0)
         pad_back_chars = current_ts.dash_r;
      tab_start = cursor_buff->win_col_no - pad_back_chars;
      tab_end   = cursor_buff->win_col_no + pad_chars;
      DEBUG(fprintf(stderr, "(past last) q_pos = %d, pad = %d, pad_back = %d, tab_start = %d, tab_end = %d, whole_tabs = %d\n", q_pos, pad_chars, pad_back_chars, tab_start, tab_end, whole_tabs);)
   }
else
   {
      whole_tabs = 0;
      tab_stop_idx = 0;
      /***************************************************************
      *  Scan looking for the next tab stop past the current position.
      *  Tab_stop_idx is not reset for each loop because we only
      *  need to go forward in the loop.  We do not have to check for
      *  end of list because of the check that last_tab > q_pos for this
      *  piece of code.
      ***************************************************************/
      while (whole_tabs == 0)
         if (q_pos < current_ts.stops[tab_stop_idx])
            whole_tabs = current_ts.stops[tab_stop_idx];
         else
            tab_stop_idx++;

      pad_chars = whole_tabs - q_pos;
      tab_end   = cursor_buff->win_col_no + pad_chars;


      if (tab_stop_idx == 0)
         pad_back_chars = cursor_buff->win_col_no;
      else
         {
            pad_back_chars = q_pos - current_ts.stops[tab_stop_idx-1];
            if (pad_back_chars == 0)
               {
                  tab_stop_idx--;
                  if (tab_stop_idx == 0)
                     pad_back_chars = cursor_buff->win_col_no;
                  else
                     pad_back_chars = q_pos - current_ts.stops[tab_stop_idx-1];
               }
         }
      tab_start = cursor_buff->win_col_no - pad_back_chars;
      DEBUG(fprintf(stderr, "(tab list) q_pos = %d, tab_start = %d, tab_end = %d, idx = %d, whole_tabs = %d\n", q_pos, tab_start, tab_end, tab_stop_idx, whole_tabs);)
   }


if (cmd_id == DM_th)
   cursor_buff->win_col_no = tab_end;
else
   cursor_buff->win_col_no = tab_start;

if (cursor_buff->win_col_no < 0)
   cursor_buff->win_col_no = 0;

/***************************************************************
*  
*  Figure out the file_col_no from where we are on the window.
*  Then get the window position set up including the X coordinate.
*  
***************************************************************/

cursor_buff->current_win_buff->file_col_no = tab_cursor_adjust(cursor_buff->current_win_buff->buff_ptr,
                                                               cursor_buff->win_col_no, 
                                                               cursor_buff->current_win_buff->first_char,
                                                               TAB_LINE_OFFSET,
                                                               cursor_buff->current_win_buff->display_data->hex_mode);

set_window_col_from_file_col(cursor_buff); /* input / output */

DEBUG5(fprintf(stderr, "(%s): end:   file [%d,%d] win [%d,%d], %s\n",
       dmsyms[cmd_id].name,
       cursor_buff->current_win_buff->file_line_no, cursor_buff->current_win_buff->file_col_no, cursor_buff->win_line_no, cursor_buff->win_col_no,
       which_window_names[cursor_buff->which_window]);
)


} /* end of dm_th */


/************************************************************************

NAME:      dm_untab  -  Change tabs to blanks according to the current tab stops.

PURPOSE:    This routine changes a tabs to blanks in a marked range of text.
            The current tab stops are used to control tab conversion.

PARAMETERS:

   1.  mark1      -  pointer to MARK_REGION (INPUT)
                     If set, this is the most current mark.  It is matched
                     with tdm_mark or cursor_buff to get the line range.

   2.  cursor_buff - pointer to BUFF_DESCR (INPUT)
                     This is the buffer description which shows the current
                     location of the cursor.

   3.  echo_mode   - int (INPUT)
                     This is the current echo mode.  If it is set to RECTANGULAR_HIGHLIGHT,
                     we are processing a rectangular region of text.


FUNCTIONS :

   1.   Determine the range of text to process

   2.   Make sure the two points are in the same window.

   3.   Read the lines of text to be processed.

   4.   Expand the tabs in the area required.  Rewrite lines which
        had tabs in them.


RETURNED VALUE:
   redraw -  If the screen needs to be redrawn, the redraw mask is
             returned.


*************************************************************************/

int   dm_untab(MARK_REGION       *mark1,              /* input   */
               BUFF_DESCR        *cursor_buff,        /* input   */
               int                echo_mode)          /* input   */
{

int                      top_line = 0;
int                      top_col;
int                      bottom_line;
int                      bottom_col;
PAD_DESCR               *buffer;
int                      rc;
int                      line_changed;
int                      any_change = False;
char                    *line;
char                     work[MAX_LINE+1];
char                     work2[MAX_LINE+1];
int                      redraw_needed = 0;

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
      DEBUG2(fprintf(stderr, "(untab) in %s from : [%d,%d]  to : [%d,%d]\n",
             which_window_names[buffer->which_window],
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
               strcpy(work, &line[top_col]);
               work[bottom_col-top_col] = '\0';
               line_changed = untab(work, work2, MAX_LINE+1, False /* never tab mode here */);
               if (line_changed)
                  {
                     delete_from_buff(work, bottom_col-top_col, top_col);
                     if (insert_in_buff(work, work2, top_col, True, MAX_LINE))
                        dm_error("Line too long", DM_ERROR_BEEP);
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
                  line = &work[top_col];
                  line_changed = untab(line, work2, MAX_LINE+1, False);
                  if (line_changed)
                     {
                        strncpy(&work[top_col], work2, MAX_LINE-top_col);
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

                     line_changed = untab(line, work, MAX_LINE+1, False);
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
                        work[bottom_col] = '\0';
                        line_changed = untab(work, work2, MAX_LINE+1, False);
                        if (line_changed)
                           {
                              if (bottom_col < (int)strlen(line))
                                 strncat(work2, &line[bottom_col], MAX_LINE-bottom_col);
                              put_line_by_num(buffer->token, bottom_line, work2, OVERWRITE);
                              any_change = True;
                           }
                     }
                  else
                     DEBUG2(   fprintf(stderr, "untab: Last line ends at col 0\n");)
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
                     strcpy(work, &line[top_col]);
                     if (top_col != bottom_col)
                        work[bottom_col-top_col] = '\0';

                    line_changed = untab(work, work2, MAX_LINE+1, False);
                    if (line_changed)
                        {
                           delete_from_buff(work, bottom_col-top_col, top_col);
                           if (insert_in_buff(work, work2, top_col, True, MAX_LINE))
                              dm_error("Line too long", DM_ERROR_BEEP);
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
      dm_error("(untab) Invalid Text Range", DM_ERROR_BEEP);
   }

/***************************************************************
*  Clear the mark and turn off echo mode.
***************************************************************/
stop_text_highlight(mark1->buffer->display_data);
mark1->mark_set = 0;

if (any_change)
   redraw_needed = (buffer->redraw_mask & FULL_REDRAW);

return(redraw_needed);

} /* end of dm_untab */



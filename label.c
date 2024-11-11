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
*  Module label.c
*
*  Routines:
*     dm_lbl                  - Set label at the current location
*     dm_glb                  - Goto a label
*     find_label              - Scan the memdata database for a label
*     dm_dlb                  - Delete labels in an area
*
* Internal:
*     remove_labels           - Remove labels from a color data line
*
* Design Notes:
*     This module is based off the color data code.  The basic technique
*     is to put a zero length color area (start_col=end_col) with a
*     magic background color name LABEL_BG (in cdgc.h) into memdata.
*     The cdgc routines know not to filter this zero length area out.
*     It will also never use this for coloring data since the length
*     is zero.
*
*  
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h   /usr/include/sys/errno.h      */
#include <ctype.h>          /* /usr/include/ctype.h      */
#include <stdlib.h>         /* /usr/include/stdlib.h     */
#include <limits.h>         /* /usr/include/limits.h     */

#include "cd.h"
#include "cdgc.h"
#include "debug.h"
#include "dmsyms.h"
#include "dmwin.h"
#include "emalloc.h"
#include "label.h"
#include "mark.h"
#include "mvcursor.h"
#include "tab.h"
#include "txcursor.h"


/***************************************************************
*  
*  Prototypes for internal routines.
*  
***************************************************************/

static int  remove_labels(DATA_TOKEN   *token,
                          int           lineno,
                          int           left_col,          
                          int           right_col,
                          char         *color_line);



/************************************************************************

NAME:      dm_lbl  -  Set label at the current location

PURPOSE:    This routine creates a label at the current location.

PARAMETERS:
   1.  dmc      - pointer to DMC (INPUT)
                  This is the DM command structure for the command
                  to be processed.

   2.  main_pad    - pointer to PAD_DESCR (INPUT)
                     This is the main pad description.  Lables always refer
                     to the main pad.

   3.  lineno      - int flag (INPUT)
                     This is true if line numbers are on.

   4.  lineno      - int flag (INPUT)
                     If true, just do the delete and return.  It
                     gets rid of a label by name.

FUNCTIONS :
   1.   If this is a delete, see if there is a label under the
        cursor and delete it.

   2.   If this is an add (parm not null), scan the file for this
        label name and delete it.

   3.   If this is an add, create a label under the current position.


RETURNED VALUE:
   redraw -  The window mask anded with the type of redraw needed is returned.


*************************************************************************/

int   dm_lbl(DMC               *dmc,                /* input   */
             BUFF_DESCR        *cursor_buff,        /* input   */
             int                lineno,             /* input flag  */
             int                just_delete)        /* input flag  */
{
char                    *color_line;
int                      color_lineno;
FANCY_LINE              *cdgc = NULL;
FANCY_LINE              *found_label;
int                      found_line;
int                      found_col;
char                     buff[MAX_LINE+1];
char                     bgfg[100]; /* plenty of room */
int                      redraw_needed = 0;
char                    *label_name;
int                      to_line;
int                      to_col;
PAD_DESCR               *to_buffer;

/***************************************************************
*  Check for the the unnamed label.
***************************************************************/
if (dmc->lbl.parm)
   label_name = dmc->lbl.parm;
else
   label_name = "";

/***************************************************************
*  Get the target point.  mark_point_setup in mark.c
***************************************************************/
mark_point_setup(NULL,              /* input  */
                 cursor_buff,       /* input  */
                 &to_line,          /* output  */
                 &to_col,           /* output  */
                 &to_buffer);       /* output  */

DEBUG20( fprintf(stderr,"(lbl) \"%s\" -> [%d,%d] in window %s\n", label_name, to_line, to_col, which_window_names[to_buffer->which_window]);)

if (to_buffer->which_window != MAIN_PAD)
   {
      dm_error("(lbl) Labels may only be place in the edit or transript pads", DM_ERROR_BEEP);
      return(redraw_needed);
   }

/***************************************************************
*  If line numbers are on and we are on that page, redraw the
*  whole screen.
***************************************************************/
if (lineno &&
    (to_buffer->file_line_no >= to_buffer->first_line) &&
    (to_buffer->file_line_no < to_buffer->first_line + to_buffer->lines_displayed))
   redraw_needed = to_buffer->redraw_mask & FULL_REDRAW;

/***************************************************************
*  First see if we have to delete one of
*  the same name.
***************************************************************/
snprintf(bgfg, sizeof(bgfg), "%s/%s", LABEL_BG, label_name);
cdgc = find_cdgc(to_buffer->token,
                 bgfg,
                 0, total_lines(to_buffer->token),
                 True, /* search down */
                 True, /* full match */
                 FIND_CDGC_LABEL,
                 &found_line, &found_col, &found_label);

if (found_label)
   {
      *found_label->bgfg = '#'; /* now it is zero length and not */
      flatten_cdgc(cdgc, buff, sizeof(buff), False /* do them all */);
      put_color_by_num(to_buffer->token,
                       found_line,
                       buff);
      DEBUG20(
         fprintf(stderr, "lbl:  deleted %s at [%d,%d]\n",
                          bgfg, found_line, found_col);
      )
   }

if (cdgc)
   free_cdgc(cdgc);

if (just_delete)
   return(redraw_needed);


/***************************************************************
*  In case we are putting a label in the middle of a colorized area,
*  we need to make sure the color continues past the label.
***************************************************************/
pulldown_overflow_color(to_buffer, to_buffer->file_line_no);

/***************************************************************
*  Add in the the new label.
***************************************************************/
snprintf(buff, sizeof(buff), "%d,%d,%s;",
              to_buffer->file_col_no,
              to_buffer->file_col_no,
              bgfg);
color_line = get_color_by_num(to_buffer->token,
                       to_buffer->file_line_no,
                       COLOR_CURRENT,
                       &color_lineno);
if (color_line && *color_line)
   strcat(buff, color_line);

put_color_by_num(to_buffer->token,
                 to_buffer->file_line_no,
                 buff);
DEBUG20(
   fprintf(stderr, "lbl:  added label \"%s\" at [%d,%d]\n",
                    label_name,
                    to_buffer->file_line_no,
                    to_buffer->file_col_no);
)


return(redraw_needed);

} /* end of dm_lbl */


/************************************************************************

NAME:      dm_glb  -  Goto a label

PURPOSE:    This commmand finds the next colored area, either of a certain
            color or any color.

PARAMETERS:
   1.  dmc         - pointer to DMC (INPUT)
                     This is the DM command structure for the command
                     to be processed.

   2.  cursor_buff - pointer to BUFF_DESCR (INPUT)
                     This is the buffer description which shows the current
                     location of the cursor.

   3.  main_pad    - pointer to PAD_DESCR (INPUT)
                     This is the main pad description.  Lables always refer
                     to the main pad.

FUNCTIONS :
   1.   Determine if we are looking for a specific color or any color.

   2.   Determine if we are searching down (nc) or up (pc).

   3.   Read color information till we find what we are looking for.


RETURNED VALUE:
   new_dmc   -  pointer to DMC
                NULL  -  Label not found
                !NULL -  Pointer to DMC command structure to position the
                         file at the label.


*************************************************************************/

DMC   *dm_glb(DMC               *dmc,                /* input   */
              BUFF_DESCR        *cursor_buff,        /* input   */
              PAD_DESCR         *main_pad)           /* input   */
{
FANCY_LINE              *cdgc = NULL;
FANCY_LINE              *found_label;
int                      found_line;
int                      found_col;
DMC                     *new_dmc;
char                     bgfg[256]; /* plenty of room */
char                    *label_name;

/***************************************************************
*  Check for the the unnamed label.
***************************************************************/
if (dmc->glb.parm)
   label_name = dmc->lbl.parm;
else
   label_name = "";


DEBUG20( fprintf(stderr,"(glb) in window %s \n", label_name);)

/***************************************************************
*  Try to find the label in the main pad.
***************************************************************/
snprintf(bgfg, sizeof(bgfg), "%s/%s", LABEL_BG, label_name);
cdgc = find_cdgc(main_pad->token,
                 bgfg,
                 0, total_lines(main_pad->token),
                 True, /* search down */
                 True, /* full match */
                 FIND_CDGC_LABEL,
                 &found_line, &found_col, &found_label);

/***************************************************************
*  If we found the label, build a [row,col] command.
*  First though, make sure we are in the main window.
***************************************************************/
if (cdgc)
   {
      if (cursor_buff->which_window != main_pad->which_window)
         dm_position(cursor_buff, main_pad, main_pad->file_line_no, main_pad->file_col_no);

      new_dmc = (DMC *)CE_MALLOC(sizeof(DMC));
      memset((char *)new_dmc, 0, sizeof(DMC));
      new_dmc->any.next         = NULL;
      new_dmc->any.cmd          = DM_markc;
      new_dmc->any.temp         = True;
      new_dmc->markc.row = found_line;
      new_dmc->markc.col = found_col;

      free_cdgc(cdgc);
   }
else
   {
      snprintf(bgfg, sizeof(bgfg), "Label %s not found", label_name);
      dm_error(bgfg, DM_ERROR_BEEP);
      new_dmc = NULL;
   }

return(new_dmc);


} /* end of dm_glbl */


/************************************************************************

NAME:       find_label - Scan the memdata database for a label

PURPOSE:    This routine scans the memdata file looking for a label
            or mark data.

PARAMETERS:
   1.  token           - pointer to DATA_TOKEN (INPUT)
                         This is the memdata file to search.
                         Usually from the main pad.

   3.  top_line        - int (INPUT)
                         This is the first line to scan if searching forward
                         and the last line to scan if searching backward.

   4.  bottom_line     - int (INPUT)
                         This is the first line not to scan if searching forward
                         and the first line to scan if searching backward.

   7.  label           - pointer to char (OUTPUT)
                         If the find is successful, the label name is copied to this
                         buffer.

   8.  max_label       - int (INPUT)
                         This is the size of the label buffer.

FUNCTIONS :
   1.   Read color data lines sequentially in the memdata database looking
        for the required data.

   2.   When the data is found, expand the cdgc and find the column.


RETURNED VALUE:
   found_line     - int
                    total_lines(token)+ 1 - No label found in the range given
                    otherwise             - Line number found

    
*************************************************************************/

int find_label(DATA_TOKEN   *token,               /* input   */
               int           top_line,            /* input   */
               int           bottom_line,         /* input   */
               char         *label,               /* output  */
               int           max_label)           /* input   */
{
char                    *p;
int                      found_line;
int                      found_col;
FANCY_LINE              *cdgc = NULL;
FANCY_LINE              *curr;
char                     marker[20];

snprintf(marker, sizeof(marker), "%s/", LABEL_BG);

cdgc = find_cdgc(token,
                 marker,
                 top_line,
                 bottom_line,
                 True,          /* search_down */
                 False,         /* full_match  */
                 FIND_CDGC_LABEL,
                 &found_line,
                 &found_col,
                 &curr);

if (curr)
   {
      p = strchr(curr->bgfg, '/');
      if (p)
         p++;
      else
         p = curr->bgfg;
      strncpy(label, p, max_label);
      free_cdgc(cdgc);

   }
else
   *label = '\0';

if (found_line < 0)
   found_line = total_lines(token) + 1;

return(found_line);

} /* end of find_label */


/************************************************************************

NAME:      dm_dlb -  Delete labels in an area

PURPOSE:    This updates the color data for a marked range of text.

PARAMETERS:

   1.  mark1      -  pointer to MARK_REGION (INPUT)
                     If set, this is the most current mark.  It is matched
                     with tdm_mark or cursor_buff to get the line range.

   2.  cursor_buff - pointer to BUFF_DESCR (INPUT)
                     This is the buffer description which shows the current
                     location of the cursor.

   3.  echo_mode   - int (INPUT)
                     This is the current echo mode.  An xc or xd acts like an
                     xc -r (or xd -r) if the echo mode is RECTANGULAR_HIGHLIGHT

   4.  lineno      - int (INPUT)
                     This is true if line numbers are on.


FUNCTIONS :

   1.   Determine the range of text to process.

   2.   Make sure the two points are in the same window.

   3.   Order the two points from top to bottom on the file.

   4.   If we are deleting color, remove all the color data
        from the start to the end points.

   5.   Color the area we with the desired gc.


RETURNED VALUE:
   redraw -  The window mask anded with the type of redraw needed is returned.


*************************************************************************/

int  dm_dlb(MARK_REGION       *mark1,              /* input   */
            BUFF_DESCR        *cursor_buff,        /* input   */
            int                echo_mode,          /* input   */
            int                lineno)             /* input   */
{

int                      top_line;
int                      top_col;
int                      bottom_line;
int                      bottom_col;
PAD_DESCR               *buffer;
int                      rc;
int                      changed;
int                      redraw_needed = 0;
int                      i;
char                    *line;
char                    *extra_line;
int                      window_left_col;
int                      window_right_col;
int                      rect_left_col;
int                      rect_right_col;
char                    *color_line;
int                      current_lineno;
int                      last_color_lineno;
int                      next_color_lineno = 0;  /* gets rid of lint errors */
int                      hex_mode = cursor_buff->current_win_buff->display_data->hex_mode;

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
if ((rc == 0) && ((top_line != bottom_line) || (top_col != bottom_col) || (echo_mode == RECTANGULAR_HIGHLIGHT)))
   {
      if (buffer->which_window != MAIN_PAD)
         {
            dm_error("(dlb) Labels may only be place in the edit or transript pads", DM_ERROR_BEEP);
            return(redraw_needed);
         }
      /***************************************************************
      *  Position the memdata to get lines from the correct place and
      *  figure out where the window cols are.
      ***************************************************************/
      line = get_line_by_num(buffer->token, top_line);
      window_left_col  = tab_pos(line, top_col, TAB_EXPAND, hex_mode);
      extra_line       = get_line_by_num(buffer->token, bottom_line);
      window_right_col = tab_pos(extra_line, bottom_col, TAB_EXPAND, hex_mode);
      
      /***************************************************************
      *  Setup code for First line.  Try for labels on the top line.
      *  If there are none, search down.
      ***************************************************************/
      current_lineno = top_line;

      color_line = get_color_by_num(buffer->token,
                                    current_lineno,
                                    COLOR_UP,
                                    &last_color_lineno);
      if ((*color_line == '\0') || (last_color_lineno != current_lineno) || (strstr(color_line, LABEL_BG) == NULL))
         {
            color_line = get_color_by_num(buffer->token,
                                          current_lineno,
                                          COLOR_DOWN,
                                          &next_color_lineno);
            if (*color_line == '\0')
               current_lineno = bottom_line + 1; /* no color data anywhere */
            else
               current_lineno = next_color_lineno;
         }

      /***************************************************************
      *  Process each line.
      ***************************************************************/
      while(current_lineno <= bottom_line)
      {
      
         if (echo_mode == RECTANGULAR_HIGHLIGHT)
            {
               line = get_line_by_num(buffer->token, current_lineno);
               rect_left_col    = tab_pos(line, window_left_col, TAB_CONTRACT, hex_mode);
               if (top_col == bottom_col)
                  rect_right_col   = MAX_LINE;
               else
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
            }
         else
            {
               if (current_lineno == top_line)
                  rect_left_col    = top_col;
               else
                  rect_left_col    = 0;
               if (current_lineno == bottom_line)
                  rect_right_col   = bottom_col;
               else
                  rect_right_col    = MAX_LINE;
            }
      
      
         /***************************************************************
         *  Normal code if we are not past the end of the region,
         *  see if we have to delete labels here.
         ***************************************************************/
         if (strstr(color_line, LABEL_BG) != NULL)
            {
               changed = remove_labels(buffer->token,
                                       current_lineno,
                                       rect_left_col,          
                                       rect_right_col,
                                       color_line);
               if (!redraw_needed && lineno && changed &&
                   (current_lineno >= buffer->first_line) &&
                   (current_lineno < buffer->first_line + buffer->lines_displayed))
                  redraw_needed = buffer->redraw_mask & FULL_REDRAW;
            }

         color_line = get_color_by_num(buffer->token,
                                       current_lineno,
                                       COLOR_DOWN,
                                       &next_color_lineno);
            
         if (*color_line == '\0')
            current_lineno = bottom_line + 1; /* no more color data */
         else
            current_lineno = next_color_lineno;
      
      } /* end of while processing more lines */
   }
else
   if (rc != 0)
      dm_error("(ca) Invalid Text Range", DM_ERROR_BEEP);
   else
      {
         DEBUG20(fprintf(stderr, "zero size region\n");)
      }


/***************************************************************
*  Clear the mark and turn off echo mode.
***************************************************************/
stop_text_highlight(mark1->buffer->display_data);
mark1->mark_set = False;


return(redraw_needed);

} /* end of dm_dlb */


/************************************************************************

NAME:       remove_labels - Remove labels from a color data line

PURPOSE:    This routine scans a cdgc and removed labels found.

PARAMETERS:
   1.  token           - pointer to DATA_TOKEN (INPUT/OUTPUT)
                         This is the memdata file.

   2.  lineno          - int (INPUT)
                         This is the line being processed.

   3.  left_col        - int (INPUT)
                         This is the leftmost column we can delete a label in.

   4.  right_col       - int (INPUT)
                         This is the right boundary for deleting labels.  It is
                         the first column to NOT delete labels in.

   5.  color_line      - pointer to char (INPUT)
                         This is the color line for lineno.  It has already been 
                         read in.

FUNCTIONS :
   1.   Convert the color_line to a cdgc linked list.

   2.   Scan the list looking for labels we can delete.

   3.   If we found a label, flatten out the cdgc list and replace
        it in the database.

RETURNED VALUE:
   found   -  int
              True   - Something was deleted
              False  - No changes made

*************************************************************************/

static int remove_labels(DATA_TOKEN   *token,
                          int           lineno,
                          int           left_col,          
                          int           right_col,
                          char         *color_line)
{
FANCY_LINE              *cdgc;
FANCY_LINE              *curr;
int                      found = False;
char                     buff[MAX_LINE+1];

/***************************************************************
*  Expand the cdgc to linked list format.  We do not care about
*  the GC's since we do not need them to flatten back to char format.
*  This list will not be used for drawing.
***************************************************************/
cdgc = expand_cdgc(NULL, None, color_line, None, NULL);

for (curr = cdgc;
     curr;
     curr = curr->next)
{
   if (curr->bgfg &&
       (IS_LABEL(curr->bgfg)) &&
       (curr->first_col >= left_col) &&
       (curr->end_col   <  right_col))
      {
         /***************************************************************
         *  We found something to delete.  Change the data
         *  so it is now a zero length field which is not
         *  the magic LABEL_BG value.  It will go away when we flatten it.
         ***************************************************************/
         *curr->bgfg = '#'; /* now it is zero length and not a label */
         found = True;
         DEBUG20(fprintf(stderr, "lbl:  deleted label at [%d,%d]\n", lineno, curr->first_col);)
      }
}

if (found)
   {
      flatten_cdgc(cdgc, buff, sizeof(buff), False /* do them all */);
      put_color_by_num(token,
                       lineno,
                       buff);
   }

if (cdgc)
   free_cdgc(cdgc);

return(found);

} /* end of remove_labels */

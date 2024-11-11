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
*  Module ca.c
*
*  Routines:
*     dm_ca                   -  The color area command
*     dm_nc                   -  Next color and previous color commands.
*
* Internal:
*     color_area_rect         -  Color a rectangular area
*     color_area              -  Color a normal area
*     cut_color_data_rect     -  Remove color data from a rectangular area
*     cut_color_data          -  Remove color data from an area
*     remove_color_from_cols  -  Default colors for a cdgc
*
*
*  THE ORIGINAL NOTES ABOUT COLOR DATA:
*  THIS INCLUDES THE DISCUSSION OF XX WHICH HAS NOT BEEN IMPLEMENTED:
*  IT IS PRETTY ACURATE:
*  
*  Format of colorization data held in memdata:
*  
*  From the start column to the semi colon we will refer
*  to as a CDGC (Colorization Data Graphics Content)
*  
*  start_col,end_col,bgc/fgc;
*      |        |     |
*      |        |     +- The background and forground colors or 
*      |        |        The literal string "XCLUDE"
*      |        |        the literal string "DEFAULT" means the default colors
*      |        |
*      |        +------- 1 Past the last column (file, not window, unexpanded tabs)
*      |                 MAX_LINE+1 for continue onto next line
*      |                 closing line of multi line has start column of zero
*      |                 MAX_LINE means to end of line, but not flowing over.
*      |
*      +---------------- Zero based start column on file line (file, not window, unexpanded tabs) 
*                        
*  
*  If there is an overlap, the first CDGC takes precedence.
*  For example:
*  5,80,red/blue;75,90,yellow/green
*  
*  The line will use the default colors for columns 0 through 4
*  The line will use blue on red from column 5 through 79
*  The line will use green on yellow from column 80 through 89
*  The line will use the default colors for column 90 and beyond
*  
*  If there was a continuing CDGC flowing onto this line, the line would have this CDGC listed
*  for columns 0 through 5 (0,5,red/blue;) This could appear anywhere in the line.
*  
*  If there was a continuing CDGC flowing onto this line and on to later lines:
*  There would be a full line colorization data item at the end of the line.
*  This would allow the other pieces of colorization data to take precedence.
*  Normal precedence specifies that if there is more than 1 CDGC extending 
*  to MAX_LINE+1, the first is used.
*  
*  5,80,red/blue;75,90,yellow/green;0,4097,black/white
*  
*  This could also be written as:
*  0,5,black/white;5,80,red/blue;75,90,yellow/green;90,4097,black/white
*  
*  
*  Thus, if a colorization line is excluded, or is part of an
*  excluded area, as in 5,80xx, putting the 0,4097,EXCLUDE on the
*  front will exclude the line without damaging the existing
*  colorization.
*  
*  If you are adding color data in the middle of an area which was
*  colorized, you must add in the appropriate CDGC to the line to
*  keep it going.
*  
*  FINDING CDGC DATA:
*  
*  get_color_by_number will return the CDGC line for that line and also
*  the line number.  If there is no CDGC for the line in question,
*  it will return the CDGC for the largest line with a CDGC <= the
*  current line.  This line can be checked for a CDGC which flows
*  past the end of the line.
*  
*  
*  MODIFYING CDGC DATA:
*  
*  A set of 3 of routines will be written:
*  1.  Take a CDGC line and turns it into a linked list
*      of CDGC structures. (Struct FANCY_LINE will do just fine)
*  2.  Flatten the linked list back into the CDGC line.
*  3.  Free the list.
*  Other routines for adding elements to the list, finding
*  if there is an overflow to next line component, and digging
*  out CDGC info for a column will probably be written.
*  
*  The digging routine will take a column number and at linked
*  list of CDGC structures.  It will return the GC to be used
*  and the end col (first column needing a new GC).  This data
*  may be gleaned from several CDGC's.
*  
*  
*  There will have to be a working buffer and buff_ptr for CDGC lines
*  and also a flag saying whether this was an overflow situation.
*  These will be used for typing.  Note that copy and paste will not
*  copy the CDGC lines, but the paste may have to manipulate
*  existing CDGC lines.
*  
*  Will need routines to take a row,col and number of chars
*  inserted also same thing but number of chars deleted.
*  
*  Commands impacted:
*  ed, ee,  er, es, xd, xp, tf(erases color data), untab 
*  s and so ?
*  
*  
*  There will be a GC control module.  It will have a list of
*  GC's for a display.  Each GC will have an in use count.
*  Routine:  Get GC when you need one given foreground and background colors.
*  Rotuine:  Release GC when you are done.   When use count goes to zero, the GC is released.  
*  Routine: Split GC to bump the use count. 
*  Routine:  Convert GC to color pair.  Probably for language sensitive API.
*  
*  
*  Drawing rules:
*  1.  Any line which is partially x'ed out will have the x'ed out
*      region(s) marked with a box with a solid diamond in it.
*  2.  1 or more fully x'ed out lines will be replaced by a boxed symbol
*      the length of the line.  The number of lines x'ed out will be printed
*      on that line.
*  
*  
*  1.  The first character of an x'ed out region will be marked by 
*      a box with an X in it.
*  2.  If the x'ed out region does not cross a line, the line will
*      resume after the x'ed box.
*  3.  If the x'ed out region goes to a new line, the first non-x'ed
*      out character will be shown in column 1.
*  3.  The FANCY_LINE data for excluded data will be stored in the winlines
*      array while the data window is on the screen.  This information will
*      be needed in tab.c to convert window columns to file line number colums.
*  
*  
*  
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h   /usr/include/sys/errno.h      */
#include <ctype.h>          /* /usr/include/ctype.h      */
#include <stdlib.h>         /* /usr/include/stdlib.h     */
#include <limits.h>         /* /usr/include/limits.h     */

#include "ca.h"
#include "cc.h"
#include "cd.h"
#include "cdgc.h"
#include "debug.h"
#include "dmsyms.h"
#include "dmwin.h"
#include "emalloc.h"
#include "mark.h"
#include "parms.h"    /* needed for BACKGROUND_CLR and FOREGROUND_CLR */
#include "tab.h"
#include "txcursor.h"  /* needed for RECTANGULAR_HIGHLIGHT*/


/***************************************************************
*  
*  Prototypes for internal routines.
*  
***************************************************************/


static void  color_area_rect(int                top_line,
                             int                left_col,
                             int                bottom_line,
                             int                right_col,
                             PAD_DESCR         *buffer,
                             char              *bgfg);

static void  color_area(int                top_line,
                        int                top_col,
                        int                bottom_line,
                        int                bottom_col,
                        PAD_DESCR         *buffer,
                        char              *bgfg);

static void  cut_color_data_rect(int                top_line,
                                 int                left_col,
                                 int                bottom_line,
                                 int                right_col,
                                 PAD_DESCR         *buffer);

static void  cut_color_data(int                top_line,
                            int                top_col,
                            int                bottom_line,
                            int                bottom_col,
                            PAD_DESCR         *buffer);

static void remove_color_from_cols(FANCY_LINE       *cdgc,
                                   int               left_col,
                                   int               right_col,
                                   int               default_above,
                                   int               cd_on_next);


/************************************************************************

NAME:      dm_ca  -  DM Color area command

PURPOSE:    This updates the color data for a marked range of text.

PARAMETERS:

   1.  dmc      - pointer to DMC (INPUT)
                  This is the DM command structure for the command
                  to be processed.

   2.  dspl_descr  - pointer to DISPLAY_DESCR (INPUT)
                     This is the current display description.

   3.  mark1      -  pointer to MARK_REGION (INPUT)
                     If set, this is the most current mark.  It is matched
                     with tdm_mark or cursor_buff to get the line range.

   4.  cursor_buff - pointer to BUFF_DESCR (INPUT)
                     This is the buffer description which shows the current
                     location of the cursor.

   5.  echo_mode   - int (INPUT)
                     This is the current echo mode.  An xc or xd acts like an
                     xc -r (or xd -r) if the echo mode is RECTANGULAR_HIGHLIGHT


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

int   dm_ca(DMC               *dmc,                /* input   */
            DISPLAY_DESCR     *dspl_descr,         /* input   */
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
int                      redraw_needed = 0;
char                     bgfg[100]; /* plenty of room */
GC                       new_gc;
char                    *background;
char                    *foreground;


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
      if (bottom_col > MAX_LINE)
         bottom_col = MAX_LINE;

      if ((top_line == bottom_line) && (top_col == bottom_col) && (echo_mode != RECTANGULAR_HIGHLIGHT))
         {
            DEBUG20(fprintf(stderr, "zero size region\n");)
            stop_text_highlight(mark1->buffer->display_data);
            mark1->mark_set = False;
            return(redraw_needed);
         }

      if ((dmc->ca.bgc == NULL) && (dmc->ca.fgc == NULL))
         {
            /***************************************************************
            *  We are deleting color.
            ***************************************************************/
            DEBUG20(
               fprintf(stderr, "(ca) [%d,%d],[%d,%d] REMOVE COLOR in %s \n",
                       top_line, top_col, bottom_line, bottom_col,
                       which_window_names[buffer->which_window]);
            )
            if (echo_mode == RECTANGULAR_HIGHLIGHT)
               cut_color_data_rect(top_line, top_col, bottom_line, bottom_col, buffer);
            else
               cut_color_data(top_line, top_col, bottom_line, bottom_col, buffer);
         }
      else
         {
            /***************************************************************
            *  Validate the colors and make sure the gc can be created. 
            *  If it cannot, the default GC is returned.  In this case
            *  an error message has already been issued.
            ***************************************************************/
            if (dmc->ca.bgc)
               background = dmc->ca.bgc;
            else
               background = DEFAULT_BG;
            if (dmc->ca.fgc)
               foreground = dmc->ca.fgc;
            else
               foreground = DEFAULT_FG;
            snprintf(bgfg, sizeof(bgfg), "%s/%s", background, foreground);
            DEBUG20(
               fprintf(stderr, "(ca) [%d,%d],[%d,%d] %s in %s \n",
                       top_line, top_col, bottom_line, bottom_col,
                       bgfg,
                       which_window_names[buffer->which_window]);
            )
            new_gc = get_gc(dspl_descr->display,
                            dspl_descr->main_pad->x_window,
                            bgfg,
                            dspl_descr->main_pad->window->font->fid,
                            &dspl_descr->gc_private_data);

            if (new_gc == DEFAULT_GC) /* DEFAULT_GC on error */
               {
                  redraw_needed = buffer->redraw_mask & FULL_REDRAW;
                  stop_text_highlight(mark1->buffer->display_data);
                  mark1->mark_set = False;
                  return(redraw_needed);
               }

            if (echo_mode == RECTANGULAR_HIGHLIGHT)
               color_area_rect(top_line,
                               top_col,
                               bottom_line,
                               bottom_col,
                               buffer,
                               bgfg);
            else
               color_area(top_line,
                          top_col,
                          bottom_line,
                          bottom_col,
                          buffer,
                          bgfg);

         }
      redraw_needed = buffer->redraw_mask & FULL_REDRAW;
      if (buffer->which_window == MAIN_PAD)
          cc_ca(dspl_descr, top_line, bottom_line); /* in cc.c */

   }
else
   dm_error("(ca) Invalid Text Range", DM_ERROR_BEEP);


/***************************************************************
*  Clear the mark and turn off echo mode.
***************************************************************/
stop_text_highlight(mark1->buffer->display_data);
mark1->mark_set = False;

return(redraw_needed);

} /* end of dm_ca */


/************************************************************************

NAME:      dm_nc  -  DM Next color and previous color commands.

PURPOSE:    This commmand finds the next colored area, either of a certain
            color or any color.

PARAMETERS:
   1.  dmc         - pointer to DMC (INPUT)
                     This is the DM command structure for the command
                     to be processed.

   2.  cursor_buff - pointer to BUFF_DESCR (INPUT)
                     This is the buffer description which shows the current
                     location of the cursor.

   3.  find_border - int (INPUT)
                     This is the current find border value from the
                     fbdr command or the -findbrdr command line option.

FUNCTIONS :
   1.   Determine if we are looking for a specific color or any color.

   2.   Determine if we are searching down (nc) or up (pc).

   3.   Read color information till we find what we are looking for.


RETURNED VALUE:
   new_dmc   -  pointer to DMC
                NULL  -  No color found
                !NULL -  Pointer to DMC command structure to position the
                         file at the color.


*************************************************************************/

DMC   *dm_nc(DMC               *dmc,                /* input   */
             BUFF_DESCR        *cursor_buff)        /* input   */
{

/*PAD_DESCR               *buffer;*/
int                      rc;
char                     bgfg[100]; /* plenty of room */
char                    *background;
char                    *foreground;
char                    *color_line;
int                      current_lineno;
int                      color_lineno;
FANCY_LINE              *cdgc;
GC                       gc;
int                      full_match;
FANCY_LINE              *found_color;
int                      found_line;
int                      found_col;
DMC                     *new_dmc;

if (dmc->ca.bgc)
   background = dmc->ca.bgc;
else
   background = "";
if (dmc->ca.fgc)
   foreground = dmc->ca.fgc;
else
   foreground = "";

/***************************************************************
*  If the foreground color is -u, extract the color from under
*  the cursor and use it.
***************************************************************/

if ((foreground[0] == '-') && (foreground[1]|0x20 == 'u'))
   {
      bgfg[0] = '\0'; /* set up for no color */
      cdgc = cdpad_curr_cdgc(cursor_buff->current_win_buff,
                             cursor_buff->current_win_buff->file_line_no,
                             &color_lineno);
      if (cdgc)
         {
            gc = extract_gc(cdgc,
                            color_lineno,
                            cursor_buff->current_win_buff->file_line_no,
                            cursor_buff->current_win_buff->file_col_no,
                            &rc); /* junk returned parm */
            if (gc != DEFAULT_GC)
               {
                  color_line =  GC_to_colors(gc, cursor_buff->current_win_buff->display_data->gc_private_data);
                  if (color_line && (strcmp(color_line, DEFAULT_BGFG) != 0))
                     strcpy(bgfg, color_line);
               }
         }
    
   }
else
   snprintf(bgfg, sizeof(bgfg), "%s/%s", background, foreground);

if (*background && *foreground)
   full_match = True;
else
   full_match = False;

/* if there are no colors, use and empty bgfg */
if (strcmp(bgfg, "/") == 0)
   bgfg[0] = '\0';

/***************************************************************
*  Search in the appropriate direction for color.  If a match
*  is found (or no match needed) stop the search.
***************************************************************/
current_lineno = cursor_buff->current_win_buff->file_line_no;

if (dmc->nc.cmd == DM_nc)
   cdgc = find_cdgc(cursor_buff->current_win_buff->token,
                    bgfg,
                    current_lineno+1,
                    total_lines(cursor_buff->current_win_buff->token),
                    True, /* search downward */
                    full_match,
                    FIND_CDGC_COLOR,
                    &found_line, &found_col, &found_color);
else
   cdgc = find_cdgc(cursor_buff->current_win_buff->token,
                    bgfg,
                    0,
                    current_lineno,
                    False, /* search up */
                    full_match,
                    FIND_CDGC_COLOR,
                    &found_line, &found_col, &found_color);

if (cdgc)
   {
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
      new_dmc = NULL;
      dm_error("No more color", DM_ERROR_BEEP);
   }


DEBUG5(
   fprintf(stderr, "%s: Generating cmd [%d,%d] in %s\n",
                   dmsyms[dmc->any.cmd].name,
                   found_line, found_col,
                   which_window_names[cursor_buff->which_window]);
)

return(new_dmc);

} /* end of dm_nc */



/************************************************************************

NAME:      color_area_rect  -  Color a rectangular area

PURPOSE:    This routine colors a rectangular region.  This is done by
            putting a color data element on each line.

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
                     past the last column on the last line to cut.

   5.  buffer      - pointer to PAD_DESCR (INPUT)
                     This is a pointer to the buffer description of the buffer
                     being copied from.  It is used to determine if this is
                     a copy from the main window, dm input window, or dm output
                     window.

   6.  bgfg        - pointer to char (INPUT)
                     This is the background and forground colors in the form
                     background_color/foreground_color


FUNCTIONS :
   1.   Position the file to read the first line to be colored.

   2.   Recalculate the columns for each line.

   3.   Put a color element for the appropriate columns on each line.


*************************************************************************/

static void  color_area_rect(int                top_line,
                             int                left_col,
                             int                bottom_line,
                             int                right_col,
                             PAD_DESCR         *buffer,
                             char              *bgfg)
{
int               i;
char              buff[MAX_LINE+1];
char             *line;
char             *extra_line;
int               window_left_col;
int               window_right_col;
int               rect_left_col;
int               rect_right_col;
char             *color_line;
int               current_lineno = top_line;
int               last_color_lineno;
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

while(current_lineno <= bottom_line)
{
   if (!line) /* past end of data */
      return; /* no color possible past end of data */

   rect_left_col    = tab_pos(line, window_left_col, TAB_CONTRACT, hex_mode);
   if (left_col == right_col)
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

   /***************************************************************
   *  Get the current colorization for this line and put the new
   *  color data on the front.
   ***************************************************************/
   color_line = get_color_by_num(buffer->token,
                                 current_lineno,
                                 COLOR_CURRENT,
                                 &last_color_lineno);

   snprintf(buff, sizeof(buff), "%d,%d,%s;", rect_left_col, rect_right_col, bgfg);
   if (color_line && *color_line)
      strcat(buff, color_line);

   put_color_by_num(buffer->token, current_lineno, buff);

   /***************************************************************
   *  Get the next line of text.
   ***************************************************************/
   line = next_line(buffer->token);
   current_lineno++;

} /* end of while reading lines */

} /* end of color_area_rect */


/************************************************************************

NAME:      color_area  -  Color a normal area

PURPOSE:    This routine deletes the color data between two points in a 
            non-rectangular region.  This is done by putting a color area
            element with overflow on the first line and any other colorized
            lines in the area and terminating it on the last line.

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

   4.  bottom_col  -  int (INPUT)
                     This is the zero based col number of the column just
                     past the last column on the last line to cut.

   5.  buffer      - pointer to PAD_DESCR (INPUT)
                     This is a pointer to the buffer description of the buffer
                     being copied from.  It is used to determine if this is
                     a copy from the main window, dm input window, or dm output
                     window.

   6.  bgfg        - pointer to char (INPUT)
                     This is the background and forground colors in the form
                     background_color/foreground_color


FUNCTIONS :

   1.   Color the first line.

   2.   Read each line.  Note that lines are never deleted in rectangular mode.
        At least a null line remains.

   3.   Chop the requested piece out of each line.  If the line is not as long
        as right_col, truncate it, but do not delete it.

   4.   Replace each chopped up line.  Lines which were not modified, do not
        need to be replaced.  This situlation occurs when left_col is greater than
        the length of the line.


*************************************************************************/

static void  color_area(int                top_line,
                        int                top_col,
                        int                bottom_line,
                        int                bottom_col,
                        PAD_DESCR         *buffer,
                        char              *bgfg)
{
char              buff[MAX_LINE+1];
char             *color_line;
char             *next_color_line;
int               current_lineno;
int               last_color_lineno;
int               next_color_lineno;
int               end_col;


/***************************************************************
*  If we are starting colorization in something other than
*  column zero, we have to worry about color which currently
*  overflows onto this line.  If this case exists, we will
*  pull this color down to this line.  This way we happen
*  to know that there is either color on this line or the current
*  color coming onto this line is the default color.
***************************************************************/
pulldown_overflow_color(buffer, top_line);

/***************************************************************
*  In case we are coloring in the middle of a colorized area,
*  we need to make sure the color continues past what we color.
***************************************************************/
pulldown_overflow_color(buffer, bottom_line);


/***************************************************************
*  Read and colorize the first first line.
***************************************************************/
color_line = get_color_by_num(buffer->token,
                              top_line,
                              COLOR_CURRENT,
                              &last_color_lineno);

/***************************************************************
*  If the area ends in column zero, fully colorize the previous
*  line.  That is [5,0] -> [4,4096] 
***************************************************************/
if (bottom_col == 0)
   {
      bottom_line--;
      bottom_col = MAX_LINE;
   }

/***************************************************************
*  If the top and bottom lines are not the same, the first line
*  will overflow.
***************************************************************/
if (top_line == bottom_line)
   end_col = bottom_col;
else
   end_col = COLOR_OVERFLOW_COL;

snprintf(buff, sizeof(buff), "%d,%d,%s;", top_col, end_col, bgfg);
if (color_line && *color_line)
   strcat(buff, color_line);

put_color_by_num(buffer->token, top_line, buff);
DEBUG20(fprintf(stderr, "     %d(a):%s\n", top_line, buff);)


if (top_line == bottom_line)
   return;

/***************************************************************
*  Find the next color line after the top.
***************************************************************/
next_color_line = get_color_by_num(buffer->token,
                                   top_line,
                                   COLOR_DOWN,
                                   &next_color_lineno);

/***************************************************************
*  If there is no more color, force processing the last line
***************************************************************/

if (!next_color_line || !(*next_color_line))
   {
      current_lineno = bottom_line+1;
      next_color_lineno = total_lines(buffer->token);
   }
else
   current_lineno = next_color_lineno;

/***************************************************************
*  Do everything up till the last line
***************************************************************/
while(current_lineno < bottom_line)
{
   snprintf(buff, sizeof(buff), "%d,%d,%s;%s", 0, COLOR_OVERFLOW_COL, bgfg, next_color_line);
   put_color_by_num(buffer->token, current_lineno, buff);
   DEBUG20(fprintf(stderr, "     %d(b):%s\n", current_lineno, buff);)
   next_color_line = get_color_by_num(buffer->token,
                                      current_lineno,
                                      COLOR_DOWN,
                                      &next_color_lineno);

   if (!next_color_line || !(*next_color_line))
      {
         current_lineno = bottom_line+1;
         next_color_lineno = total_lines(buffer->token);
      }
   else
      current_lineno = next_color_lineno;
}

/***************************************************************
*  If there is no more color lines in the marked range close off
*  the color. on the last line.  If the last line is past eof,
*  set it to overflow.
***************************************************************/
if (next_color_lineno >= bottom_line)
   {
      if (bottom_line >= total_lines(buffer->token))
          bottom_col = COLOR_OVERFLOW_COL;

      snprintf(buff, sizeof(buff), "%d,%d,%s;", 0, bottom_col, bgfg);
      if (next_color_lineno == bottom_line)
         strcat(buff, next_color_line);
      put_color_by_num(buffer->token, bottom_line, buff);
      DEBUG20(fprintf(stderr, "     %d(c):%s\n", bottom_line, buff);)

   }

} /* end of color_area */



/************************************************************************

NAME:      cut_color_data_rect  -  Remove color data from a rectangular area

PURPOSE:    This routine deletes the color data between two points in a 
            rectangular region.  This is either done by removing color
            data or adding default color segments.

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
                     past the last column on the last line to cut.

   5.  buffer      - pointer to PAD_DESCR (INPUT)
                     This is a pointer to the buffer description of the buffer
                     being copied from.  It is used to determine if this is
                     a copy from the main window, dm input window, or dm output
                     window.


FUNCTIONS :
   1.   Position the file to read the first line to be uncolored.

   2.   Find each line that is affected.  These are the lines which are not
        of the default color.

   3.   Clear the color data for the requested piece on each line.


*************************************************************************/

static void  cut_color_data_rect(int                top_line,
                                 int                left_col,
                                 int                bottom_line,
                                 int                right_col,
                                 PAD_DESCR         *buffer)
{
int               i;
char              buff[MAX_LINE+1];
char             *line;
char             *extra_line;
int               window_left_col;
int               window_right_col;
int               rect_left_col;
int               rect_right_col;
char             *color_line;
char             *next_color_line;
FANCY_LINE       *cdgc = NULL;
int               current_lineno = top_line;
int               last_color_lineno;
int               next_color_lineno = 0;  /* gets rid of lint errors */
int               line_overflow_is_default_colors;
int               end_col;
GC                overflow_gc;
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

while(current_lineno <= bottom_line)
{
   if (!line) /* past end of data */
      return; /* no color possible past end of data */

   rect_left_col    = tab_pos(line, window_left_col, TAB_CONTRACT, hex_mode);
   if (left_col == right_col)
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

   /***************************************************************
   *  Code for First time through or where we reach the next color line.
   *
   *  Get the previous and next color
   *  data.  Deal with the special cases of no color at all
   *  and no color above the top line.
   ***************************************************************/
   if ((current_lineno == top_line) || (current_lineno == next_color_lineno))
      {
         if (cdgc != NULL)
            {
               free_cdgc(cdgc);
               cdgc = NULL;
            }
         color_line = get_color_by_num(buffer->token,
                                       current_lineno,
                                       COLOR_UP,
                                       &last_color_lineno);
         next_color_line = get_color_by_num(buffer->token,
                                            current_lineno,
                                            COLOR_DOWN,
                                            &next_color_lineno);
         if ((*color_line == '\0') && (*next_color_line == '\0'))
            return; /* no color anywhere */
         if (*color_line == '\0')
            {
               /***************************************************************
               *  There is color below but none above, skip forward.
               ***************************************************************/
               current_lineno = next_color_lineno;
               position_file_pointer(buffer->token, current_lineno);
               line = next_line(buffer->token);
               line_overflow_is_default_colors = True;
               continue;
            }
      }

   /***************************************************************
   *  Normal code. We are in a color area.  Either through
   *  overflow from a previous line or because of color on the current line.
   *  First check if this is overflow.  If so, and this is the default
   *  color already, we can skip forward.
   ***************************************************************/
   if (cdgc == NULL)
      cdgc = expand_cdgc(buffer->display_data->display,            /* in cdgc.c */
                         buffer->display_data->main_pad->x_window,
                         color_line,
                         buffer->window->font->fid,
                         &buffer->display_data->gc_private_data);

   if (last_color_lineno < current_lineno)
      {
         if ((overflow_gc = extract_gc(cdgc, last_color_lineno, current_lineno, MAX_LINE, &end_col)) == DEFAULT_GC)
            {
               /***************************************************************
               *  There is color below but none above, skip forward.
               ***************************************************************/
               if (*next_color_line == '\0')
                  return; /* no color below */
               current_lineno = next_color_lineno;
               line_overflow_is_default_colors = True;
            }
         else
            {
               /***************************************************************
               *  Mark a square of default color. 
               ***************************************************************/
               snprintf(buff, sizeof(buff), "%d,%d,%s;0,%d,%s;", rect_left_col, rect_right_col, DEFAULT_BGFG, MAX_LINE+1, GC_to_colors(overflow_gc, buffer->display_data->gc_private_data));
               if ((*next_color_line == '\0') || (next_color_lineno > bottom_line))
                  next_color_lineno = bottom_line + 1;
               while(current_lineno < next_color_lineno)
                  put_color_by_num(buffer->token, current_lineno++, buff);
               line_overflow_is_default_colors = False;
            }
         position_file_pointer(buffer->token, current_lineno);
         line = next_line(buffer->token);
      }
   else
      {
         if (last_color_lineno == current_lineno)
            {
               remove_color_from_cols(cdgc,
                                      rect_left_col,
                                      rect_right_col,
                                      line_overflow_is_default_colors,
                                      (current_lineno+1 == next_color_lineno));
               line = next_line(buffer->token);
               current_lineno++;
            }
         else
            {
               snprintf(buff, sizeof(buff), "(cut_color_data_rect) Internal error, last_color_lineno (%d) is greater than current line (%d)", last_color_lineno, current_lineno);
               dm_error(buff, DM_ERROR_LOG);
               return;
            }
      }

} /* end of while processing more lines */

if (cdgc != NULL)
   free_cdgc(cdgc);

} /* end of cut_color_data_rect */


/************************************************************************

NAME:      cut_color_data  -  Remove color data from an area

PURPOSE:    This routine deletes the color data between two points in a 
            non-rectangular region.  This is either done by removing color
            data.

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

   4.  bottom_col  -  int (INPUT)
                     This is the zero based col number of the column just
                     past the last column on the last line to cut.

   5.  buffer      - pointer to PAD_DESCR (INPUT)
                     This is a pointer to the buffer description of the buffer
                     being copied from.  It is used to determine if this is
                     a copy from the main window, dm input window, or dm output
                     window.


FUNCTIONS :

   1.   See if color overflows past the end of the region being uncolorized.
        If so, pull down the color data to this line so we will not impact it.

   2.   Read the color data for the first line.  If it is already default color,
        don't bother with it.  Otherwise make it the default color.

   3.   Remove colorization data from each line up to the last line.

   4.   Remove colorization data for the affected columns on the last line.


*************************************************************************/

static void  cut_color_data(int                top_line,
                            int                top_col,
                            int                bottom_line,
                            int                bottom_col,
                            PAD_DESCR         *buffer)
{
int               i;
char              buff[MAX_LINE+1];
char             *color_line;
char             *next_color_line;
FANCY_LINE       *cdgc = NULL;
int               current_lineno;
int               last_color_lineno;
int               next_color_lineno;
int               overflow_line;
char             *line;
int               line_overflow_is_default_colors = True;
/*GC                overflow_gc;*/

/***************************************************************
*  See if there is color overflowing into this area.
***************************************************************/
if (top_line > 0)
   {
      color_line = get_color_by_num(buffer->token,
                                    top_line-1,
                                    COLOR_UP,
                                    &last_color_lineno);
      if (color_line && *color_line)
         {
            cdgc = expand_cdgc(buffer->display_data->display,            /* in cdgc.c */
                               buffer->display_data->main_pad->x_window,
                               color_line,
                               buffer->window->font->fid,
                               &buffer->display_data->gc_private_data);
            if (extract_gc(cdgc, last_color_lineno, top_line, MAX_LINE, &i) != DEFAULT_GC)
               line_overflow_is_default_colors = False;
            free_cdgc(cdgc);
         }
   }


/***************************************************************
*  See if overflow data needs to be pulled down past the cut region.
*  If not, then there is no color data which will be deleted.
*  There is nothing to do.  If the bottom is at bottom of file,
*  we will not do a pull down.
***************************************************************/

line = get_line_by_num(buffer->token, bottom_line);
if ((bottom_col >= (int)strlen(line)) && (bottom_line >= (total_lines(buffer->token)-1)))
   {
      /* range goes past eof, do not pull down color */
      if (!COLORED(buffer->token))
         return; 
   }
else
   {
      if ((bottom_col >= (int)strlen(line)) && (bottom_line < (total_lines(buffer->token)-1)))
         overflow_line = bottom_line+1;   /* color on last line is eliminated */
      else
         overflow_line = bottom_line;     /* some color could remain on last line */

      color_line = get_color_by_num(buffer->token,
                                    overflow_line,
                                    COLOR_UP,
                                    &last_color_lineno);

      if (color_line && *color_line && (last_color_lineno >= top_line))
         {
            if (overflow_line < (total_lines(buffer->token)))
               pulldown_overflow_color(buffer, overflow_line); /* preserve any overflow */
         }
      else
         return;  /* there is no color in the range */
   }


/***************************************************************
*  Process the top line, maybe the only line.
***************************************************************/

color_line = get_color_by_num(buffer->token,
                              top_line,
                              COLOR_UP,
                              &last_color_lineno);

if (color_line && *color_line)
   if (last_color_lineno == top_line)
      {
         cdgc = expand_cdgc(buffer->display_data->display,
                            buffer->display_data->main_pad->x_window,
                            color_line,
                            buffer->window->font->fid,
                            &buffer->display_data->gc_private_data);

         if (top_line == bottom_line)
            i = bottom_col;
         else
            i = COLOR_OVERFLOW_COL;

         remove_color_from_cols(cdgc,
                                top_col,
                                i,
                                line_overflow_is_default_colors,
                                True); /* killing color below */
         flatten_cdgc(cdgc,
                      buff,
                      sizeof(buff),
                      False); /* do all the cdgc elements (there is only 1) */

         if (cdgc != NULL)
            {
               free_cdgc(cdgc);
               cdgc = NULL;
            }

         put_color_by_num(buffer->token, top_line, buff);
      }
   else
      {
         snprintf(buff, sizeof(buff), "%d,%d,%s;", top_col, COLOR_OVERFLOW_COL, DEFAULT_BGFG);
         put_color_by_num(buffer->token, top_line, buff);
      }

/***************************************************************
*  Find the next color line after the top.
***************************************************************/
next_color_line = get_color_by_num(buffer->token,
                                   top_line,
                                   COLOR_DOWN,
                                   &next_color_lineno);

if (!next_color_line || !(*next_color_line) || (next_color_lineno > bottom_line))
   return;  /* no more color */

/***************************************************************
*  Do everything up till the last line
***************************************************************/

current_lineno = next_color_lineno;

while(current_lineno < bottom_line)
{
      if (strstr(next_color_line, LABEL_BG) == NULL)
         put_color_by_num(buffer->token, current_lineno, NULL);
      else
         {
            cdgc = expand_cdgc(buffer->display_data->display,
                               buffer->display_data->main_pad->x_window,
                               next_color_line,
                               buffer->window->font->fid,
                               &buffer->display_data->gc_private_data);

            remove_color_from_cols(cdgc,
                                   0,
                                   MAX_LINE+1,
                                   True, /* default color above, for sure */
                                   False); /* Don't know if there is color data below */
            flatten_cdgc(cdgc,
                         buff,
                         sizeof(buff),
                         False);

            if (cdgc != NULL)
               {
                  free_cdgc(cdgc);
                  cdgc = NULL;
               }

            put_color_by_num(buffer->token, current_lineno, buff);
         }

      next_color_line = get_color_by_num(buffer->token,
                                         current_lineno,
                                         COLOR_DOWN,
                                         &next_color_lineno);
     if (!next_color_line || !(*next_color_line))
        current_lineno = bottom_line+1;
     else
        current_lineno = next_color_lineno;
}

if ((current_lineno == bottom_line) && (bottom_col > 0))
   {
      cdgc = expand_cdgc(buffer->display_data->display,
                         buffer->display_data->main_pad->x_window,
                         next_color_line,
                         buffer->window->font->fid,
                         &buffer->display_data->gc_private_data);

      remove_color_from_cols(cdgc,
                             0,
                             bottom_col,
                             True, /* default color above, for sure */
                             True);
      flatten_cdgc(cdgc,
                   buff,
                   sizeof(buff),
                   False); /* do all the cdgc elements (there is only 1) */

      if (cdgc != NULL)
         free_cdgc(cdgc);

      put_color_by_num(buffer->token, bottom_line, buff);
   }


} /* end of cut_color_data */



/************************************************************************

NAME:      remove_color_from_cols  -  Default colors for a cdgc

PURPOSE:    This routine modifies a cdgc linked list to show default colors
            in the passed columns.

PARAMETERS:

   1.  cdgc          - pointer to FANCY_LINE (INPUT/OUTPUT)
                       This is the cdgc list to be manipulated.

   2.  left_col      - int (INPUT)
                       This is the leftmost (numerically lowest) column
                       to process.

   3.  right_col     - int (INPUT)
                       This is the end column.  It is the column number
                       (zero based) of the first column we are not
                       processing.

   4.  default_above - int (INPUT)
                       This flag indicates that the area above this
                       line is painted in the default colors.  This
                       fact can be used to throw away lines when all
                       relevant data is gone.

   5.  cd_on_next    - int (INPUT)
                       This flag indicates that the next line has
                       color data on it.  It is used to help clear
                       out unused color data.


FUNCTIONS :
   1.   Examine each element of the cdgc list to see if we impact it.

   2.   Mark the cursor position.  This makes having the command under
        a key and tdm'ing to the command line and typing it equivalent.

   3.   Call un_do or re_do to go do the work.

   4.   If a valid line position was returned, position the cursor in
        col 0 of that line.

   5.   Show we need a full redraw.

*************************************************************************/

static void remove_color_from_cols(FANCY_LINE       *cdgc,
                                   int               left_col,
                                   int               right_col,
                                   int               default_above,
                                   int               cd_on_next)
{
FANCY_LINE           *curr;
FANCY_LINE           *new;
int                   nonzero_element_left = False;

/***************************************************************
*  In the following comments, the region is the area between
*  parameters left_col and right_col.  The element is the area
*  bounded by the first and end columns of the fancy line element.
***************************************************************/

for (curr = cdgc;
     curr;
     curr = curr->next)
{
   /***************************************************************
   *  Ignore label markers
   ***************************************************************/
   if (IS_LABEL(curr->bgfg))
      continue;
   /***************************************************************
   *  First check for some overlap between the region and the element
   ***************************************************************/
   if ((curr->first_col < right_col) && (curr->end_col > left_col))
      {
         /***************************************************************
         *  Overlap exists:
         *  Check for an overlap at the front of the element.
         ***************************************************************/
         if (curr->first_col >= left_col)                  
            {
               /***************************************************************
               *  Overlap on front.
               *  Check for total coverage of the element.
               ***************************************************************/
               if (curr->end_col <= right_col)
                  curr->end_col = curr->first_col; /* kill element by making it zero size */
               else
                  {
                     curr->first_col = right_col; /* chop off front of element */
                     nonzero_element_left = True;
                     /***************************************************************
                     *  Overlap on front.
                     *  Check for total coverage of the element.
                     ***************************************************************/
                     if ((curr->first_col == MAX_LINE) && (curr->end_col == MAX_LINE+1) && cd_on_next)
                        curr->end_col = curr->first_col; /* kill element by making it zero size */
                  }
            }
         else
            {
               /***************************************************************
               *  Overlap must be at end or middle.
               *  Check for region inside of element.
               ***************************************************************/
               if (curr->end_col > right_col)
                  {
                     nonzero_element_left = True;
                     /***************************************************************
                     *  Region is inside element, split into two elements.
                     ***************************************************************/
                     new = (FANCY_LINE *)CE_MALLOC(sizeof(FANCY_LINE));
                     if (new)
                        {
                           memcpy((char *)new, (char *)curr, sizeof(FANCY_LINE));
                           curr->next = new;
                           curr->end_col = left_col;
                           new->first_col = right_col;
                        }
                  }
               else
                  curr->end_col = left_col; /* chop off back of element */
            }
      }
}  /* end of walking the cdgc list */

if (cdgc)
   {
      if ((!nonzero_element_left) && !default_above)
         {
            /***************************************************************
            *  Reuse first element to define default colored area: 0,COLOR_OVERFLOW_COL
            ***************************************************************/
            cdgc->first_col  = 0;
            cdgc->end_col    = COLOR_OVERFLOW_COL;
            cdgc->gc         = DEFAULT_GC;
            cdgc->bgfg       = malloc_copy(DEFAULT_BGFG);
         }
   }
else
   DEBUG20(fprintf(stderr, "remove_color_from_cols: NULL cdgc passed");)

} /* end of remove_color_from_cols */






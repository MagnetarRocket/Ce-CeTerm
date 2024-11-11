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
*  module color.c
*
*  Routines:
*         dm_inv              - Invert the foreground and background window colors
*         dm_bgc              - Change the background or forground color of the windows.
*         dm_fl               - Load a new font
*         set_initial_colors  - Turn the initial background and foreground colors to pixels
*         change_title        - Change the window manager title and icon name.
*
*  Internal routines:
*         build_expose_event  - build expose event for whole screen
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h      */


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

#include "borders.h"
#include "color.h"
#include "cdgc.h"
#include "debug.h"
#include "dmsyms.h"
#include "dmwin.h"
#include "emalloc.h"
#include "parms.h"
#include "pd.h"
#include "sendevnt.h"
#include "wdf.h"
#include "window.h"
#include "xutil.h"
#include "xerrorpos.h"

/***************************************************************
*  
*  Declares for internal routines
*  
***************************************************************/

static void build_expose_event(DRAWABLE_DESCR     *window,
                               Display            *display,
                               Window              x_window);


/************************************************************************

NAME:      dm_inv   -  reverse foreground and background colors

PURPOSE:    This routine will modify the gc's and border colors for
            the Ce windows.  The function is to reverse the foreground
            and background colors.  This includes the graphics content
            and the window border colors.  The GC's are shared between the 
            the main, dminput, and dmoutput windows, so they only have to be
            done once.  

PARAMETERS:
   1.   dspl_descr -  pointer to DISPLAY_DESCR (INPUT)
                    The display description being processed.

FUNCTIONS :

   1.   Get the foreground and background colors

   2.   Modify the normal and reverse gc's to swap the colors.

NOTES:
   The xor GC is not impacted by this swap because it's foreground
   color is the XOR of the forground and background colors which remains
   the same.


*************************************************************************/

void dm_inv(DISPLAY_DESCR  *dspl_descr)
{
unsigned long         temp_pixel;
XGCValues             gcvalues;
unsigned long         valuemask;
char                 *work;

/***************************************************************
*  
*  Get the existing forground and background values
*  
***************************************************************/

valuemask = GCForeground
          | GCBackground;

DEBUG9(XERRORPOS)
if (!XGetGCValues(dspl_descr->display,
                  dspl_descr->main_pad->window->gc,
                  valuemask,
                  &gcvalues))
{
   dm_error("XGetGCValues failure", DM_ERROR_LOG);
   return;
}

/* let the color area data know about the switch (cdgc.c) */
cdgc_bgc(dspl_descr->display, gcvalues.foreground, gcvalues.background, &dspl_descr->gc_private_data);
 
/****************************************************************
*
*   Put the main gc values in the reverse gc and the titlebar
*   regular gc.  The titlebar is reverse video to start with.
*
****************************************************************/

DEBUG9(XERRORPOS)
XChangeGC(dspl_descr->display,
          dspl_descr->main_pad->window->reverse_gc,
          valuemask,
          &gcvalues);

DEBUG9(XERRORPOS)
if (dspl_descr->title_subarea->gc !=  dspl_descr->main_pad->window->reverse_gc)
   XChangeGC(dspl_descr->display,
             dspl_descr->title_subarea->gc,
             valuemask,
             &gcvalues);

/***************************************************************
*  
*  Set the background and border colors in each window.
*  
***************************************************************/


DEBUG9(XERRORPOS)
XSetWindowBorder(dspl_descr->display,
                 dspl_descr->main_pad->x_window,
                 gcvalues.background);

DEBUG9(XERRORPOS)
XSetWindowBackground(dspl_descr->display,
                     dspl_descr->main_pad->x_window,
                     gcvalues.foreground);

DEBUG9(XERRORPOS)
XSetWindowBorder(dspl_descr->display,
                 dspl_descr->dminput_pad->x_window,
                 gcvalues.background);

DEBUG9(XERRORPOS)
XSetWindowBackground(dspl_descr->display,
                    dspl_descr->dminput_pad->x_window,
                    gcvalues.foreground);

DEBUG9(XERRORPOS)
XSetWindowBorder(dspl_descr->display,
                 dspl_descr->dmoutput_pad->x_window,
                 gcvalues.background);

DEBUG9(XERRORPOS)
XSetWindowBackground(dspl_descr->display,
                    dspl_descr->dmoutput_pad->x_window,
                    gcvalues.foreground);

DEBUG9(XERRORPOS)
XSetWindowBorder(dspl_descr->display,
                 dspl_descr->pd_data->menubar_x_window,
                 gcvalues.foreground);

DEBUG9(XERRORPOS)
XSetWindowBackground(dspl_descr->display,
                    dspl_descr->pd_data->menubar_x_window,
                    gcvalues.background);

#ifdef PAD
if (dspl_descr->pad_mode)
   {
      DEBUG9(XERRORPOS)
      XSetWindowBorder(dspl_descr->display,
                       dspl_descr->unix_pad->x_window,
                       gcvalues.background);

      DEBUG9(XERRORPOS)
      XSetWindowBackground(dspl_descr->display,
                          dspl_descr->unix_pad->x_window,
                          gcvalues.foreground);
   }
#endif


/****************************************************************
*
*   Swap the foreground and background and put them in the normal
*   gc and the titlebar reverse gc.
*
****************************************************************/

temp_pixel          = gcvalues.background;
gcvalues.background = gcvalues.foreground;
gcvalues.foreground = temp_pixel;

DEBUG9(XERRORPOS)
XChangeGC(dspl_descr->display,
          dspl_descr->main_pad->window->gc,
          valuemask,
          &gcvalues);

DEBUG9(XERRORPOS)
if (dspl_descr->title_subarea->reverse_gc !=  dspl_descr->main_pad->window->gc)
   XChangeGC(dspl_descr->display,
             dspl_descr->title_subarea->reverse_gc,
             valuemask,
             &gcvalues);

/***************************************************************
*  
*  Swap the current color names in the main pararameter values
*  
***************************************************************/

work = BACKGROUND_CLR; /* macro in parms.h */
BACKGROUND_CLR = FOREGROUND_CLR;
FOREGROUND_CLR = work;
OPTION_FROM_PARM[FOREGROUND_IDX] = True;
OPTION_FROM_PARM[BACKGROUND_IDX] = True;


/***************************************************************
*  
*  Reexpose the whole window.
*  
***************************************************************/

build_expose_event(dspl_descr->main_pad->window, dspl_descr->display, dspl_descr->main_pad->x_window);

}  /* end of dm_inv */



/************************************************************************

NAME:      dm_bgc   -  change a window's background or foreground color.

PURPOSE:    This routine will modify the gc's and border colors for
            the Ce windows.  The function is to change either the background
            or foreground colors.  This includes the graphics content
            and the window border colors.  The GC's are shared between the 
            the main, dminput, and dmoutput windows, so they only have to be
            done once.  

PARAMETERS:
   1.   dmc      -  pointer to DMC (INPUT)
                    The command structure is passed in as the parm.  It is needed
                    to determine whether this is a bgc or fgc command and
                    to get the name of the color passed.

   2.   dspl_descr -  pointer to DISPLAY_DESCR (INPUT)
                    The display description being processed.

FUNCTIONS :

   1.   Get the foreground and background colors

   2.   Convert the new color to a pixel value.

   3.   Modify the gc's and window colors to have the new value.

   4.   modify the Xor gc to have the new value.


*************************************************************************/

void dm_bgc(DMC            *dmc,
            DISPLAY_DESCR  *dspl_descr)
{
unsigned long         temp_pixel;
XGCValues             gcvalues;
unsigned long         valuemask;
char                  msg[256];
unsigned long         old_pixel;


/***************************************************************
*  
*  If a null parm is passed, print the current color
*  
***************************************************************/

if ((dmc->bgc.parm == NULL) || (dmc->bgc.parm[0] == '\0'))
   {
      if (dmc->any.cmd == DM_bgc)
         dm_error(BACKGROUND_CLR, DM_ERROR_MSG);
      else
         dm_error(FOREGROUND_CLR, DM_ERROR_MSG);
      return;
   }


/***************************************************************
*  
*  Get the existing forground and background values
*  
***************************************************************/

valuemask = GCForeground
          | GCBackground;

DEBUG9(XERRORPOS)
if (!XGetGCValues(dspl_descr->display,
                  dspl_descr->main_pad->window->gc,
                  valuemask,
                  &gcvalues))
{
   dm_error("XGetGCValues failure", DM_ERROR_LOG);
   return;
}
 
/***************************************************************
*  
*  Convert the passed color to a pixel id.
*  
***************************************************************/

temp_pixel = colorname_to_pixel(dspl_descr->display, dmc->bgc.parm);
if (temp_pixel == BAD_COLOR)
   {
      snprintf(msg, sizeof(msg), "Cannot allocate color %s", dmc->bgc.parm);
      dm_error(msg, DM_ERROR_BEEP);
      return;  /* message already issued */
   }

/***************************************************************
*  
*  Put the pixel value in the GC area providing:
*  1.  It is not the color we have already, in this case do nothing.
*  2.  It is not the same as the other (foreground or background color).
*      In this case flag an error.
*  
***************************************************************/

if (dmc->any.cmd == DM_bgc)
   {
      if (gcvalues.background == temp_pixel)
         return;  /* nothing to do. */
      if (gcvalues.foreground == temp_pixel)
         {
            snprintf(msg, sizeof(msg), "Cannot change background color to be the same as the foreground (%s)",  dmc->bgc.parm);
            dm_error(msg, DM_ERROR_BEEP);
            return;  /* message already issued */
         }
      old_pixel = gcvalues.background;
      gcvalues.background = temp_pixel;
      /* let the color area data know about the switch (cdgc.c) */
      cdgc_bgc(dspl_descr->display, temp_pixel, BAD_COLOR, &dspl_descr->gc_private_data);
   }
else
   {
      if (gcvalues.foreground == temp_pixel)
         return;  /* nothing to do. */
      if (gcvalues.background == temp_pixel)
         {
            snprintf(msg, sizeof(msg), "Cannot change foreground color to be the same as the background (%s)",  dmc->bgc.parm);
            dm_error(msg, DM_ERROR_BEEP);
            return;  /* message already issued */
         }
      old_pixel = gcvalues.foreground;
      gcvalues.foreground = temp_pixel;
      /* let the color area data know about the switch (cdgc.c) */
      cdgc_bgc(dspl_descr->display, BAD_COLOR, temp_pixel, &dspl_descr->gc_private_data);
   }


/***************************************************************
*  
*  Save the new color name in the option values
*  
***************************************************************/

if (dmc->any.cmd == DM_bgc)
   {
      if (BACKGROUND_CLR) /* macro in parms.h */
         free(BACKGROUND_CLR);
      BACKGROUND_CLR = malloc_copy(dmc->bgc.parm);
      OPTION_FROM_PARM[BACKGROUND_IDX] = True;
   }
else
   {
      if (FOREGROUND_CLR)
         free(FOREGROUND_CLR);
      FOREGROUND_CLR = malloc_copy(dmc->bgc.parm);
      OPTION_FROM_PARM[FOREGROUND_IDX] = True;
   }

/****************************************************************
*
*   Put the colors in the main gc;.
*
****************************************************************/


DEBUG9(XERRORPOS)
XChangeGC(dspl_descr->display,
          dspl_descr->main_pad->window->gc,
          valuemask,
          &gcvalues);

DEBUG9(XERRORPOS)
if (dspl_descr->title_subarea->reverse_gc !=  dspl_descr->main_pad->window->gc)
   XChangeGC(dspl_descr->display,
             dspl_descr->title_subarea->reverse_gc,
             valuemask,
             &gcvalues);


/***************************************************************
*  
*  Set the background and border colors in each window.
*  
***************************************************************/


DEBUG9(XERRORPOS)
XSetWindowBorder(dspl_descr->display,
                 dspl_descr->main_pad->x_window,
                 gcvalues.foreground);

DEBUG9(XERRORPOS)
XSetWindowBackground(dspl_descr->display,
                     dspl_descr->main_pad->x_window,
                     gcvalues.background);

DEBUG9(XERRORPOS)
XSetWindowBorder(dspl_descr->display,
                 dspl_descr->dminput_pad->x_window,
                 gcvalues.foreground);

DEBUG9(XERRORPOS)
XSetWindowBackground(dspl_descr->display,
                    dspl_descr->dminput_pad->x_window,
                    gcvalues.background);

DEBUG9(XERRORPOS)
XSetWindowBorder(dspl_descr->display,
                 dspl_descr->dmoutput_pad->x_window,
                 gcvalues.foreground);

DEBUG9(XERRORPOS)
XSetWindowBackground(dspl_descr->display,
                    dspl_descr->dmoutput_pad->x_window,
                    gcvalues.background);

DEBUG9(XERRORPOS)
XSetWindowBorder(dspl_descr->display,
                 dspl_descr->pd_data->menubar_x_window,
                 gcvalues.background);

DEBUG9(XERRORPOS)
XSetWindowBackground(dspl_descr->display,
                    dspl_descr->pd_data->menubar_x_window,
                    gcvalues.foreground);

#ifdef PAD
if (dspl_descr->pad_mode)
   {
      DEBUG9(XERRORPOS)
      XSetWindowBorder(dspl_descr->display,
                       dspl_descr->unix_pad->x_window,
                       gcvalues.foreground);

      DEBUG9(XERRORPOS)
      XSetWindowBackground(dspl_descr->display,
                          dspl_descr->unix_pad->x_window,
                          gcvalues.background);
   }
#endif


/****************************************************************
*
*   Swap the foreground and background and put them in the reverse
*   gc's
*
****************************************************************/

temp_pixel          = gcvalues.background;
gcvalues.background = gcvalues.foreground;
gcvalues.foreground = temp_pixel;


DEBUG9(XERRORPOS)
XChangeGC(dspl_descr->display,
          dspl_descr->main_pad->window->reverse_gc,
          valuemask,
          &gcvalues);

DEBUG9(XERRORPOS)
if (dspl_descr->title_subarea->gc !=  dspl_descr->main_pad->window->reverse_gc)
   XChangeGC(dspl_descr->display,
             dspl_descr->title_subarea->gc,
             valuemask,
             &gcvalues);

/***************************************************************
*  
*  Set the colors for the xor gc.
*  
***************************************************************/

gcvalues.foreground = gcvalues.background ^ gcvalues.foreground;
DEBUG9(XERRORPOS)
XChangeGC(dspl_descr->display,
          dspl_descr->main_pad->window->xor_gc,
          GCForeground,
          &gcvalues);

DEBUG9(XERRORPOS)
if (dspl_descr->title_subarea->xor_gc !=  dspl_descr->main_pad->window->xor_gc)
   XChangeGC(dspl_descr->display,
             dspl_descr->title_subarea->xor_gc,
             GCForeground,
             &gcvalues);




/***************************************************************
*  
*  Reexpose the whole window.
*  This is done here, with global access to the dspl descr,
*  so emergency routines like malloc_error can call.
*  
***************************************************************/

build_expose_event(dspl_descr->main_pad->window, dspl_descr->display, dspl_descr->main_pad->x_window);

/***************************************************************
*  
*  Free the old pixel value.
*  
***************************************************************/

DEBUG1(fprintf(stderr, "New Pixel is %d, Old Pixel is %d\n", temp_pixel, old_pixel);)
#ifdef blah
/*This causes errors when ca's are being used.*/
DEBUG9(XERRORPOS)
XFreeColors(dspl_descr->display,
            DefaultColormap(dspl_descr->display, DefaultScreen(dspl_descr->display)),
            &old_pixel,
            1,                      /* one pixel value to free */
            0);
#endif


}  /* end of dm_bgc */


/************************************************************************

NAME:      dm_fl   -  change a window's font

PURPOSE:    This routine will modify the gc's and DRAWABLE_DESCRIPTIONS to 
            load a new font.

PARAMETERS:
   1.   dmc      -  pointer to DMC (INPUT)
                    The command structure is passed in as the parm.  It is needed
                    to get the new font name.

   2.   dspl_descr -  pointer to DISPLAY_DESCR (INPUT)
                    The display description being processed.

FUNCTIONS :

   1.   Get access to the new font.  If it cannot be loaded, issue 
        an error message and return.

   2.   If the title subarea is using the same font and the main window,
        change its gc's.

   3.   Free the old font data structure.

   4.   Copy the new font data into the DRAWABLE DESCRIPTIONS.

   5.   Force a window resize to get lines per screen and some such recalculated.


*************************************************************************/

void dm_fl(DMC            *dmc,
           DISPLAY_DESCR  *dspl_descr)
{
#define               MAX_NAMES_TO_RETURN 20
XGCValues             gcvalues;
char                  msg[256];
XFontStruct          *font_data = NULL;
int                   fixed_font;
int                   line_height;
int                   i;
char                 *all_exists_str;
/*XEvent                configure_event;*/
int                   actual_font_count;
char                **font_list;

DEBUG9(XERRORPOS)
if (dmc->fl.parm)
   {
      DEBUG9(XERRORPOS)
      font_list = XListFonts(dspl_descr->display, dmc->fl.parm, (int)MAX_NAMES_TO_RETURN, &actual_font_count);
      if (actual_font_count > 0)
         {
            for (i = 0; i < actual_font_count && font_data == NULL; i++)
               font_data = XLoadQueryFont(dspl_descr->display, font_list[i]);

            i--; /* back off to the entry found */
            DEBUG1(fprintf(stderr, "(fl) Matched fonts: %d, Font chosen #%d (%s)\n", actual_font_count, i, font_list[i]);)
         }
   }
else
   {
     dm_error(dspl_descr->current_font_name, DM_ERROR_MSG);
     return;
   }

if (font_data == NULL)
   {
      snprintf(msg, sizeof(msg), "(fl) Cannot find font %s", dmc->fl.parm);
      dm_error(msg, DM_ERROR_BEEP);
      return;
   }
else
   {
      if (dspl_descr->current_font_name)
         free(dspl_descr->current_font_name);
      dspl_descr->current_font_name = malloc_copy(font_list[i]);
      /* change the font name in the parms */
      if (FONT_NAME)
         free(FONT_NAME);
      FONT_NAME = malloc_copy(dmc->fl.parm);
      OPTION_FROM_PARM[FONT_IDX] = True;
   }

gcvalues.font = font_data->fid;

if ((font_data->min_bounds.width == font_data->max_bounds.width) && font_data->all_chars_exist)
   fixed_font = font_data->min_bounds.width;
else
   fixed_font = 0;

if (font_data->all_chars_exist)
   all_exists_str = ", all chars exist";
else
   all_exists_str = "";

line_height = font_data->ascent + font_data->descent + (((font_data->ascent + font_data->descent) * dspl_descr->padding_between_line) / 100);

if (font_data->min_bounds.width == font_data->max_bounds.width)
   snprintf(msg, sizeof(msg), "Fixed width, %d pixels, height = %d pixels%s", font_data->min_bounds.width, line_height, all_exists_str);
else
   snprintf(msg, sizeof(msg), "Variable width %d - %d pixels, height = %d pixels%s", font_data->min_bounds.width, font_data->max_bounds.width, line_height, all_exists_str);

dm_error(msg, DM_ERROR_MSG);

DEBUG1(
fprintf(stderr, "MIN: ascent %d, descent %d, lbearing %d, rbearing %d\n", font_data->min_bounds.ascent, font_data->min_bounds.descent, font_data->min_bounds.lbearing, font_data->min_bounds.rbearing);
fprintf(stderr, "MAX: ascent %d, descent %d, lbearing %d, rbearing %d\n", font_data->max_bounds.ascent, font_data->max_bounds.descent, font_data->max_bounds.lbearing, font_data->max_bounds.rbearing);
fprintf(stderr, "min char = 0x%02x (%c), max char = 0x%02x (%c)\n", 
                font_data->min_char_or_byte2, font_data->min_char_or_byte2,
                font_data->max_char_or_byte2, font_data->max_char_or_byte2);
fprintf(stderr, "min row  = 0x%02x,      max row  = 0x%02x\n", 
                font_data->min_byte1,
                font_data->max_byte1);

fprintf(stderr, "all_chars_exist = %d, default char = 0x%02x (%c)\n",
                font_data->all_chars_exist, font_data->default_char, font_data->default_char);

)


/***************************************************************
*  
*  Change the titlebar gc if needed.
*  
***************************************************************/

if (dspl_descr->title_subarea->font == dspl_descr->main_pad->window->font)
   {
      dspl_descr->title_subarea->font = font_data;
      DEBUG9(XERRORPOS)
      dspl_descr->title_subarea->line_height = line_height;
      dspl_descr->title_subarea->fixed_font  = fixed_font;
      dspl_descr->title_subarea->lines_on_screen = 1;
   }


/***************************************************************
*  Get rid of the old font.
***************************************************************/
DEBUG9(XERRORPOS)
XFreeFont(dspl_descr->display, dspl_descr->main_pad->window->font);

/***************************************************************
*  
*  Change the GC's once.  The same GC's get used in the main
*  pad, dminput, dmoutput, and unix pads.
*  
***************************************************************/

DEBUG9(XERRORPOS)
XChangeGC(dspl_descr->display,
       dspl_descr->main_pad->window->gc,
       GCFont,
       &gcvalues);
DEBUG9(XERRORPOS)
XChangeGC(dspl_descr->display,
       dspl_descr->main_pad->window->reverse_gc,
       GCFont,
       &gcvalues);
DEBUG9(XERRORPOS)
XChangeGC(dspl_descr->display,
       dspl_descr->main_pad->window->xor_gc,
       GCFont,
       &gcvalues);

change_pd_font(dspl_descr->display,
               dspl_descr->pd_data,
               gcvalues.font);

/***************************************************************
*  Reset the main pad data
***************************************************************/
dspl_descr->main_pad->window->font        = font_data;
dspl_descr->main_pad->window->line_height = line_height;
dspl_descr->main_pad->window->fixed_font  = fixed_font;
dspl_descr->main_pad->window->lines_on_screen = lines_on_drawable(dspl_descr->main_pad->window);

dspl_descr->main_pad->pix_window->font        = font_data;
dspl_descr->main_pad->pix_window->line_height = line_height;
dspl_descr->main_pad->pix_window->fixed_font  = fixed_font;
dspl_descr->main_pad->pix_window->lines_on_screen = lines_on_drawable(dspl_descr->main_pad->pix_window);

/***************************************************************
*  Reset the dminput pad data
***************************************************************/
dspl_descr->dminput_pad->window->font        = font_data;
dspl_descr->dminput_pad->window->line_height = line_height;
dspl_descr->dminput_pad->window->fixed_font  = fixed_font;
dspl_descr->dminput_pad->window->lines_on_screen = 1; /* always 1 */

dspl_descr->dminput_pad->pix_window->font        = font_data;
dspl_descr->dminput_pad->pix_window->line_height = line_height;
dspl_descr->dminput_pad->pix_window->fixed_font  = fixed_font;
dspl_descr->dminput_pad->pix_window->lines_on_screen = 1; /* always 1 */

/***************************************************************
*  Reset the dmoutput pad data
***************************************************************/
dspl_descr->dmoutput_pad->window->font        = font_data;
dspl_descr->dmoutput_pad->window->line_height = line_height;
dspl_descr->dmoutput_pad->window->fixed_font  = fixed_font;
dspl_descr->dmoutput_pad->window->lines_on_screen = 1; /* always 1 */

dspl_descr->dmoutput_pad->pix_window->font        = font_data;
dspl_descr->dmoutput_pad->pix_window->line_height = line_height;
dspl_descr->dmoutput_pad->pix_window->fixed_font  = fixed_font;
dspl_descr->dmoutput_pad->pix_window->lines_on_screen = 1; /* always 1 */

/***************************************************************
*  Reset the lineno_subarea 
***************************************************************/
dspl_descr->lineno_subarea->font        = font_data;
dspl_descr->lineno_subarea->line_height = line_height;
dspl_descr->lineno_subarea->fixed_font  = fixed_font;
dspl_descr->lineno_subarea->lines_on_screen = lines_on_drawable(dspl_descr->lineno_subarea);

/***************************************************************
*  Reset the menubar window
***************************************************************/
dspl_descr->pd_data->menu_bar.font        = font_data;
dspl_descr->pd_data->menu_bar.line_height = line_height;
dspl_descr->pd_data->menu_bar.fixed_font  = fixed_font;
dspl_descr->pd_data->menu_bar.lines_on_screen =  1; /* always 1 */

#ifdef PAD
if (dspl_descr->pad_mode)
   {
      /***************************************************************
      *  Reset the unix pad data, if needed
      ***************************************************************/
      dspl_descr->unix_pad->window->font        = font_data;
      dspl_descr->unix_pad->window->line_height = line_height;
      dspl_descr->unix_pad->window->fixed_font  = fixed_font;
      /*dspl_descr->unix_pad->window->lines_on_screen  not controlled here */

      dspl_descr->unix_pad->pix_window->font        = font_data;
      dspl_descr->unix_pad->pix_window->line_height = line_height;
      dspl_descr->unix_pad->pix_window->fixed_font  = fixed_font;
      /*dspl_descr->unix_pad->pix_window->lines_on_screen   not controlled here */

   }
#endif

/***************************************************************
*  update any color data (ca) information (cdgc.c)
***************************************************************/
new_font_gc(dspl_descr->display, font_data->fid, &dspl_descr->gc_private_data);

/***************************************************************
*  create a configure event to recalculate the window geometry.
***************************************************************/
reconfigure(dspl_descr->main_pad, dspl_descr->display);

}  /* end of dm_fl */


/************************************************************************

NAME:      set_initial_colors   -  Turn the initial background and foreground colors to pixels

PURPOSE:    This routine is called during initial window setup to set the colors
            for the window.

PARAMETERS:
   1.   dspl_descr -  pointer to DISPLAY_DESCR (INPUT)
                    The display description being processed.

   2.   back_color -  pointer to char (INPUT)
                    This is the background color specified by the
                    user.  It could be NULL or a zero length string.

   3.   back_pixel -  pointer to unsigned long (OUTPUT)
                    This is the background color pixel value to use.

   4.   fore_color -  pointer to char (INPUT)
                    This is the foreground color specified by the
                    user.  It could be NULL or a zero length string.

   5.   fore_pixel -  pointer to unsigned long (OUTPUT)
                    This is the foreground color pixel value to use.

FUNCTIONS :

   1.   Try to set the background and foreground colors from the parameters.

   2.   If this fails use black and white.

   3.   If the foreground and background colors are the same, change one of
        them.  If only the background or only the foreground was specified,
        save this one.  Otherwise, save the background color.

   4.   Save the color names in the local static variables.


*************************************************************************/

void set_initial_colors(DISPLAY_DESCR  *dspl_descr,  /* input  */
                        char           *back_color,  /* input  */
                        unsigned long  *back_pixel,  /* output */
                        char           *fore_color,  /* input  */
                        unsigned long  *fore_pixel)  /* output */
{
#define MAX_COLOR_NAME_LEN 80
char   current_background[MAX_COLOR_NAME_LEN];
char   current_foreground[MAX_COLOR_NAME_LEN];

/***************************************************************
*  
*  If no values were specified, try to get the colors from
*  the wdc list.
*  
***************************************************************/

if ((!back_color || !*back_color) && (!fore_color || !*fore_color))
   {
      wdc_get_colors(dspl_descr->display, &back_color, &fore_color, dspl_descr->properties);
      OPTION_FROM_PARM[FOREGROUND_IDX] = True;
      OPTION_FROM_PARM[BACKGROUND_IDX] = True;
   }


/****************************************************************
*
*   Get the foreground and background colors.  If they are the
*   same, fix one.  If one was not defaulted, try to keep this
*   color.  If both were specified, perserve the background.
*   If neither were specified there can be no problem as the defaults
*   are bg=white and fg=black.
*
****************************************************************/
*back_pixel  = colorname_to_pixel(dspl_descr->display, back_color); /* in xutil.c */
if (*back_pixel == BAD_COLOR)
   {
      *back_pixel = WhitePixel(dspl_descr->display, DefaultScreen(dspl_descr->display));
      strcpy(current_background, "white");
      fore_color = "black";  /* force black on white */
   }
else
   strcpy(current_background, back_color);

*fore_pixel  = colorname_to_pixel(dspl_descr->display, fore_color);
if (*fore_pixel == BAD_COLOR)
   {
      /***************************************************************
      *  In the event of failure, revert to black on white.
      ***************************************************************/
      *fore_pixel = BlackPixel(dspl_descr->display, DefaultScreen(dspl_descr->display));
      strcpy(current_foreground, "black");
      if (!back_color || !*back_color)
         {
            if ((*back_pixel != WhitePixel(dspl_descr->display, DefaultScreen(dspl_descr->display))) &&
                (*back_pixel != BlackPixel(dspl_descr->display, DefaultScreen(dspl_descr->display))))
               {
                  DEBUG9(XERRORPOS)
                  XFreeColors(dspl_descr->display,
                              DefaultColormap(dspl_descr->display, DefaultScreen(dspl_descr->display)),
                              back_pixel,
                              1,                      /* one pixel value to free */
                              0);
               }
            *back_pixel = WhitePixel(dspl_descr->display, DefaultScreen(dspl_descr->display));
            strcpy(current_background, "white");
         }
   }
else
   strcpy(current_foreground, fore_color);

if (*fore_pixel == *back_pixel)
   if (back_color && *back_color)
      if (*fore_pixel == BlackPixel(dspl_descr->display, DefaultScreen(dspl_descr->display)))
         {
            *fore_pixel = WhitePixel(dspl_descr->display, DefaultScreen(dspl_descr->display));
            strcpy(current_foreground, "white");
         }
      else
         {
            *fore_pixel = BlackPixel(dspl_descr->display, DefaultScreen(dspl_descr->display));
            strcpy(current_foreground, "black");
         }
   else
      if (*back_pixel == BlackPixel(dspl_descr->display, DefaultScreen(dspl_descr->display)))
         {
            *back_pixel = WhitePixel(dspl_descr->display, DefaultScreen(dspl_descr->display));
            strcpy(current_background, "white");
         }
      else
         {
            *back_pixel = BlackPixel(dspl_descr->display, DefaultScreen(dspl_descr->display));
            strcpy(current_background, "black");
         }

/***************************************************************
*  
*  Set the options in the parameter values in parms.h.  
*  The addresses we were passed were most likely BACKGROUND_CLR
*  and FOREGROUND_CLR, so we wait till the end to change the parms.
*  This is not true in the case of a cc window.
*  
***************************************************************/


if (BACKGROUND_CLR) /* macro in parms.h */
   free(BACKGROUND_CLR);
BACKGROUND_CLR = malloc_copy(current_background);

if (FOREGROUND_CLR)
   free(FOREGROUND_CLR);
FOREGROUND_CLR = malloc_copy(current_foreground);

/* let the color area data know about the colors (cdgc.c) */
/* this is where the private area will probably get allocated */
cdgc_bgc(dspl_descr->display, *back_pixel, *fore_pixel, &dspl_descr->gc_private_data);


} /* end of set_initial_colors */


/************************************************************************

NAME:      change_title - change the window and icon titles

PURPOSE:    This routine changes the window manager properties needed
            to change the window manager title and the icon title.

PARAMETERS:

   1. dspl_descr     - pointer to DISPLAY_DESCR (INPUT)
                       The current display description

   2. window_name    - pointer to char (INPUT)
                       This is the name of the to be put in the
                       window.  If it is null, the current name
                       is displayed in the DM output window.

FUNCTIONS :
    1.  Use the Xlib call to chane the window manager and icon titles.

OUTPUTS:
   The window manager properties for the passed window are set by this routine.

*************************************************************************/

void change_title(DISPLAY_DESCR     *dspl_descr,
                  char              *window_name)
{
XTextProperty   returned_window_name;

if (window_name && *window_name)
   {
      DEBUG9(XERRORPOS)
      XSetStandardProperties(dspl_descr->display,
                             dspl_descr->main_pad->x_window,
                             window_name,
                             window_name,
                             None, /* pixmap for icon */
                             NULL, /* argv */
                             0,    /* argc */
                             NULL);
      if (WM_TITLE)
         free((char *)WM_TITLE);
      WM_TITLE = malloc_copy(window_name);
      OPTION_FROM_PARM[TITLE_IDX] = True;
   }
else
   {
      DEBUG9(XERRORPOS)
      XGetWMName(dspl_descr->display,
                 dspl_descr->main_pad->x_window,
                 &returned_window_name);
      dm_error((char *)returned_window_name.value, DM_ERROR_MSG);
   }

} /* end of change_title */


/************************************************************************

NAME:      build_expose_event

PURPOSE:    This routine build an X expose event for the entire window
            passed in. 

PARAMETERS:

   1.  window  - pointer to DRAWABLE_DESCR (INPUT)
                 This is the ce drawable description for the window
                 to have an expose event generated for it.
                 For example: dspl_descr->main_pad->window


FUNCTIONS :

   1.   Copy the data from the parm into the event structure.

   2.   Fill in the data in the event which is not copied from
        the drawable description with constants.

   3.   Send the event on it's way


*************************************************************************/

static void build_expose_event(DRAWABLE_DESCR     *window,
                               Display            *display,
                               Window              x_window)
{
XEvent          event;

/***************************************************************
*
*  Set up the expose event for forcing the entire window to be
*  redrawn.
*
***************************************************************/

memset((char *)&event, 0, sizeof(event));
event.xexpose.type = Expose;
event.xexpose.serial = 0;
event.xexpose.send_event = True;
event.xexpose.display = display;
event.xexpose.window  = x_window;
event.xexpose.x = 0;
event.xexpose.y = 0;
event.xexpose.width  = window->width;
event.xexpose.height = window->height;
event.xexpose.count = 0;

ce_XSendEvent(display, x_window, True, ExposureMask, &event);

} /* end build_expose_event  */




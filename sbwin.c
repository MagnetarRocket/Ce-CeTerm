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
*  module sbwin.c
*
*  Routines:
*         get_sb_windows        - Create the horizontal and vertical scroll bars
*         resize_sb_windows     - Reposition scroll bar windows after main window resize
*         draw_vert_sb          - Draw the vertical scroll bar
*         draw_horiz_sb         - Draw the horizontal scroll bar
*         vert_button_press     - Process a button press in the vertical sb window
*         horiz_button_press    - Process a button press in the horizontal sb window
*         move_vert_slider      - Move vertical slider because of positioning
*         move_horiz_slider     - Move horizontal slider because of positioning
*         build_in_arrow_press  - Build a button press which appears to be in one of the 4 arrows
*         build_in_arrow_rlse   - Build a button release to turn off scrolling from build_in_arrow_press
*
*  Internal:
*         size_vert_slider      - Size and position the vertical slider
*         size_horiz_slider     - Size and position the horizontal slider
*         max_text_pixels       - Find maximum line width for horiz sb.
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h         */
#include <string.h>         /* /usr/include/string.h        */
#include <errno.h>          /* /usr/include/errno.h         */
#include <time.h>           /* /usr/include/time.h          */

#ifdef WIN32
#ifdef WIN32_XNT
#include "xnt.h"
#else
#include "X11/X.h"          /* /usr/include/X11/X.h      */
#include "X11/Xlib.h"       /* /usr/include/X11/Xlib.h   */
#include "X11/Xutil.h"      /* /usr/include/X11/Xutil.h  */
#include "X11/keysym.h"     /* /usr/include/X11/keysym.h */
#endif
#else
#include <X11/X.h>          /* /usr/include/X11/X.h      */
#include <X11/Xlib.h>       /* /usr/include/X11/Xlib.h   */
#include <X11/Xutil.h>      /* /usr/include/X11/Xutil.h  */
#include <X11/keysym.h>     /* /usr/include/X11/keysym.h */
#endif

#include "borders.h"
#include "dmsyms.h"
#include "dmwin.h"
#include "debug.h"
#include "emalloc.h"
#include "memdata.h"
#include "mvcursor.h"
#include "parms.h"
#include "redraw.h"
#include "sbwin.h"
#include "tab.h"
#include "timeout.h"
#include "window.h"
#include "xerrorpos.h"
#include "xutil.h"

/*char  *getenv(char *name);  Removed for FREEBSD, don't seem to use getenv */

/***************************************************************
*
*  This is the list of colors used for drawing the scroll bars.
*  The indexes are referenced by the arrow maps.
*
***************************************************************/

#define   BAR_COLORS   4

/* [0] = "gray59" "#979797" */
/* [1] = "gray87" "#dddddd" */    
/* [2] = "gray70" "#b2b2b2" */
/* [3] = "gray37" "#5e5e5e" */

static char   *bar_colors[BAR_COLORS] = {
              "#979797",
              "#E0E0E0",
              "#b2b2b2",
              "#5e5e5e" };

static char   *bw_colors[BAR_COLORS] = {
              "black",
              "white",
              "white",
              "black" };


/***************************************************************
*  
*  SB_AREA  - enum type
*  When a button is pressed down, this value is used to tell where in
*  the scroll bar the pointer is.  Values exist for both the vertical
*  and horizontal scroll bars because the pointer can only be in one place.
*
*  ARROW_IDX - enum
*  This type is used as an index into the arrows array in the private
*  data structure area.  Each arrow contains the image for one of the
*  arrows.  It is easier to initialize them as an array, but nicer to
*  access them via mnewmonic.
*  
***************************************************************/

typedef enum {
   BUTTON_NOT_DOWN        = 0,
   PTR_IN_ARROW_UP,
   PTR_IN_ARROW_DOWN,
   PTR_IN_ARROW_LEFT,
   PTR_IN_ARROW_RIGHT,
   PTR_IN_EMPTY_TOP,
   PTR_IN_EMPTY_BOTTOM,
   PTR_IN_EMPTY_LEFT,
   PTR_IN_EMPTY_RIGHT,
   PTR_IN_VERT_SLIDER,
   PTR_IN_HORIZ_SLIDER
} SB_AREA;

enum {
   ARROW_UP              = 0,
   ARROW_DOWN            = 1,
   ARROW_LEFT            = 2,
   ARROW_RIGHT           = 3,
   ARROW_UP_PRESSED      = 4,
   ARROW_DOWN_PRESSED    = 5,
   ARROW_LEFT_PRESSED    = 6,
   ARROW_RIGHT_PRESSED   = 7
} ARROW_IDX;

#ifdef DebuG
char *sb_area_names[] = {
   "BUTTON_NOT_DOWN",
   "PTR_IN_ARROW_UP",
   "PTR_IN_ARROW_DOWN",
   "PTR_IN_ARROW_LEFT",
   "PTR_IN_ARROW_RIGHT",
   "PTR_IN_EMPTY_TOP",
   "PTR_IN_EMPTY_BOTTOM",
   "PTR_IN_EMPTY_LEFT",
   "PTR_IN_EMPTY_RIGHT",
   "PTR_IN_VERT_SLIDER",
   "PTR_IN_HORIZ_SLIDER" };

#endif

#define TOTAL_ARROWS 8

/***************************************************************
*  
*  Static data hung off the display description.
*
*  The following structure is hung off the sb_data->sb_private field
*  in the display description.
*  
***************************************************************/

typedef struct {
   unsigned int       vert_window_height;
   unsigned int       horiz_window_width;
   unsigned int       sb_disabled;
   unsigned int       drag_out_of_window_scrolling;
   GC                 bg_gc;
   GC                 darkline_gc;
   GC                 lightline_gc;
   GC                 slidertop_gc;
   unsigned long      pixels[BAR_COLORS];
   SB_AREA            sb_area;
   int                vert_slider_top;
   int                vert_slider_hotspot;
   int                vert_slider_height;
   int                vert_last_firstline;
   int                horiz_slider_left;
   int                horiz_slider_hotspot;
   int                horiz_slider_width;
   int                horiz_max_chars;
   int                horiz_base_chars;
   int                horiz_last_firstchar;
   int                slider_last_pos;
   int                slider_pointer_offset;
   int                slider_delta_tweek;
} SB_PRIVATE;




/***************************************************************
*  
*  Min height for vertical slider and width for horizontal.
*  Smallest size is a square
*  
***************************************************************/

#define MIN_SLIDER_WIDTH  VERT_SCROLL_BAR_WIDTH-5
#define MIN_SLIDER_HEIGHT HORIZ_SCROLL_BAR_HEIGHT-5

/***************************************************************
*  
*  Maps for drawing the arrows at the end of the bars
*  
***************************************************************/

#define  ARROW_WIDTH   (VERT_SCROLL_BAR_WIDTH-3)
#define  ARROW_HEIGHT  (VERT_SCROLL_BAR_WIDTH-3)

/***************************************************************
*  
*  Local Prototypes
*  
***************************************************************/

static void size_vert_slider(int              lines_on_screen,
                             int              total_lines,
                             int              first_line,
                             SB_PRIVATE      *private);

static void size_horiz_slider(int              screen_width,
                              int              first_char,
                              XFontStruct     *font_data,
                              SB_PRIVATE      *private);
                        
static int  max_text_pixels(PAD_DESCR       *pad,
                            int              horiz_window_on,
                            int             *max_len);

static SB_PRIVATE *check_private_data(SCROLLBAR_DESCR  *sb_win);


/************************************************************************

NAME:      get_sb_windows        - Create the horizontal and vertical scroll bars

PURPOSE:    This routine creates the scroll bar windows when scroll bars are
            turned on.  This is only done once.  If scroll bars are turned off,
            the windows are simply unmapped.

PARAMETERS:

   1.  main_pad      - pointer to PAD_DESCR (INPUT)
                       This is a pointer to the main window drawable description.
                       The sb windows will be children of this window.

   2.  display       - pointer to Display (Xlib type) (INPUT)
                       This is the current display pointer.

   3.  sb_win        - pointer to SCROLLBAR_DESCR (INPUT/OUTPUT)
                       This is where this module stores its data about the
                       windows.  This data is divided into public and private
                       sections.  The private section  is described by type
                       SB_PRIVATE.

   3.  sb_colors     - pointer to char (INPUT)
                       This string is extracted from the scrollBarColors
                       X resource.  It contains the 4 colors used in
                       drawing the scroll bars in the order, gutter color,
                       highlight color, slider to color, shadow color.
                       The colors are separated by blanks, commas, and/or
                       tabs.  If NULL or a null string the defaults are
                       used.

   3.  sb_horiz_base_width  - pointer to char (INPUT)
                       This is a numeric characters string.
                       This string is extracted from the  .scrollBarWidth
                       X resource.  It contains the base width for the
                       X horizontal scroll bar gutter in chars.  That is,
                       when calculating the size and position of the horizontal
                       scroll bar, the maximum of the longest line displayed
                       and this numeric value is used.

FUNCTIONS :
   1.   If the windows already exist, return.  There is nothing to do.
        Otherwise allocate and clear the private data.

   2.   Allocate the color pixels needed to draw the scroll bars.
        Be wary of depth 1 displays.

   3.   Create the horizontal and vertical windows.  Leave them unmmapped.       

   4.   Create the gc's used for drawing the scroll bars.

   5.   If a serious failure occurs, set the sb_disabled flag to show
        that scroll bars are out of commission for this run.

*************************************************************************/

void  get_sb_windows(PAD_DESCR        *main_pad,            /* input  */
                     Display          *display,             /* input  */
                     SCROLLBAR_DESCR  *sb_win,              /* input / output */
                     char             *sb_colors,           /* input */
                     char             *sb_horiz_base_width) /* input */


{
SB_PRIVATE           *private;
int                   i;

XGCValues             gcvalues;
unsigned long         valuemask;
char                  msg[512];
char                  work_colors[512];
XSetWindowAttributes  window_attrs;
unsigned long         window_mask;
unsigned long         event_mask;
char                 *color;

int                   vert_x;
int                   vert_y;
int                   vert_width;
int                   vert_height;
int                   horiz_x;
int                   horiz_y;
int                   horiz_width;
int                   horiz_height;

/***************************************************************
*  Make sure the scroll bar windows don't already exist.
***************************************************************/
if (sb_win->vert_window != None)
   return;


if ((private = check_private_data(sb_win)) == NULL)
   {
      sb_win->sb_mode = SCROLL_BAR_OFF;
      return;
   }

if (private->sb_disabled)
   {
      dm_error_dspl("Scroll bars disabled, (could not be created)", DM_ERROR_LOG, main_pad->display_data); 
      sb_win->sb_mode = SCROLL_BAR_OFF;
      return;
   }

/***************************************************************
*
*  Allocate the colors needed for the scroll bars
*
***************************************************************/

if (sb_colors && *sb_colors)
   {
      strncpy(work_colors, sb_colors, sizeof(work_colors));
      work_colors[sizeof(work_colors)-1] = '\0';
      color = strtok(work_colors, "\t ,");
   }
else
   color = NULL;

   /* color screen */
   /* [0] = "gray59" "#979797" */
   /* [1] = "gray87" "#dddddd" */    
   /* [2] = "gray70" "#b2b2b2" */
   /* [3] = "gray37" "#5e5e5e" */
   
   for (i = 0; i < BAR_COLORS; i++)
   {
      private->pixels[i] = BAD_COLOR;
      if (color)
         {
            private->pixels[i] = colorname_to_pixel(display, color);
            if (private->pixels[i] == BAD_COLOR)
               {
                  snprintf(msg, sizeof(msg), "Could not find color %s, trying %s", color, bar_colors[i]); 
                  dm_error_dspl(msg, DM_ERROR_BEEP, main_pad->display_data);
                }
            color = strtok(NULL, "\t ,");
         }

      if (DefaultDepth(display, DefaultScreen(display)) != 1) 
         {
            if (private->pixels[i] == BAD_COLOR)
               {
                  private->pixels[i] = colorname_to_pixel(display, bar_colors[i]);
                  if (private->pixels[i] == BAD_COLOR)
                     {
                        snprintf(msg, sizeof(msg), "Could not find color %s, using %s", bar_colors[i], bw_colors[i]); 
                        dm_error_dspl(msg, DM_ERROR_BEEP, main_pad->display_data);
                        if (strcmp(bw_colors[i], "black") == 0)
                           private->pixels[i] = BlackPixel(display, DefaultScreen(display));
                        else
                           private->pixels[i] = WhitePixel(display, DefaultScreen(display));
                      }
               }
         }
      else
         if (strcmp(bw_colors[i], "black") == 0)
            private->pixels[i] = BlackPixel(display, DefaultScreen(display));
         else
            private->pixels[i] = WhitePixel(display, DefaultScreen(display));
   }

/***************************************************************
*
*  Get the pixel values from the main window gc.
*
***************************************************************/

valuemask = GCForeground
          | GCBackground
          | GCLineWidth
          | GCCapStyle
          | GCGraphicsExposures
          | GCFont;

DEBUG9(XERRORPOS)
if (!XGetGCValues(display,
                  main_pad->window->gc,
                  valuemask,
                  &gcvalues))
   {
      DEBUG(   fprintf(stderr, "get_dm_windows:  XGetGCValues failed for main window gc = 0x%X\n", main_pad->window->gc);)
      gcvalues.foreground = BlackPixel(display, DefaultScreen(display));
      gcvalues.background = WhitePixel(display, DefaultScreen(display));
   }

/***************************************************************
*
*  Determine the height, width, X and Y of the windows.
*
***************************************************************/

private->vert_window_height = main_pad->window->sub_height;
private->horiz_window_width = main_pad->window->sub_width - VERT_SCROLL_BAR_WIDTH;

vert_x        = main_pad->window->width-VERT_SCROLL_BAR_WIDTH;
vert_y        = main_pad->window->sub_y;
vert_width    = VERT_SCROLL_BAR_WIDTH;
vert_height   = private->vert_window_height;

horiz_x       = main_pad->window->sub_x;
horiz_y       = main_pad->window->sub_height-HORIZ_SCROLL_BAR_HEIGHT;
horiz_width   = main_pad->window->sub_width-VERT_SCROLL_BAR_WIDTH;
horiz_height  = HORIZ_SCROLL_BAR_HEIGHT;
                

/***************************************************************
*  
*  Set the event mask and do not propagate mask.
*  
***************************************************************/

event_mask = ExposureMask
             | KeyPressMask
             | KeyReleaseMask
             | ButtonPressMask
             | ButtonReleaseMask
             | Button1MotionMask
             | Button2MotionMask
             | Button3MotionMask
             | Button4MotionMask
             | Button5MotionMask;


/***************************************************************
*
*  Create the vertical scroll bar window
*
***************************************************************/

window_attrs.background_pixel = gcvalues.background;
window_attrs.border_pixel     = gcvalues.background;
gcvalues.foreground           = private->pixels[0];
window_attrs.win_gravity      = UnmapGravity;
window_attrs.do_not_propagate_mask = PointerMotionMask;

window_mask = CWBackPixel
            | CWBorderPixel
            | CWWinGravity
            | CWDontPropagate;

DEBUG9(XERRORPOS)
sb_win->vert_window = XCreateWindow(
                                display,
                                main_pad->x_window,
                                vert_x, vert_y,
                                vert_width, vert_height,
                                0,                /* no border */
                                DefaultDepth(display, DefaultScreen(display)),
                                InputOutput,                    /*  class        */
                                CopyFromParent,                 /*  visual       */
                                window_mask,
                                &window_attrs
    );

DEBUG9(XERRORPOS)
XSelectInput(display,
             sb_win->vert_window,
             event_mask);


/***************************************************************
*
*  Create the horizontal scroll bar window.
*
***************************************************************/


DEBUG9(XERRORPOS)
sb_win->horiz_window = XCreateWindow(
                                display,
                                main_pad->x_window,
                                horiz_x, horiz_y,
                                horiz_width, horiz_height,
                                0,                /* no border */
                                DefaultDepth(display, DefaultScreen(display)),
                                InputOutput,                    /*  class        */
                                CopyFromParent,                 /*  visual       */
                                window_mask,
                                &window_attrs
    );


DEBUG9(XERRORPOS)
XSelectInput(display,
             sb_win->horiz_window,
             event_mask);

gcvalues.cap_style  = CapButt;
gcvalues.line_width = 1;

DEBUG9(XERRORPOS)
private->bg_gc = XCreateGC(display,
                        main_pad->x_window,
                        valuemask,
                        &gcvalues
                       );

gcvalues.foreground           = private->pixels[3];
DEBUG9(XERRORPOS)
private->darkline_gc = XCreateGC(display,
                        main_pad->x_window,
                        valuemask,
                        &gcvalues
                       );

gcvalues.foreground           = private->pixels[1];
DEBUG9(XERRORPOS)
private->lightline_gc = XCreateGC(display,
                        main_pad->x_window,
                        valuemask,
                        &gcvalues
                       );

gcvalues.foreground           = private->pixels[2];
DEBUG9(XERRORPOS)
private->slidertop_gc = XCreateGC(display,
                        main_pad->x_window,
                        valuemask,
                        &gcvalues
                       );


/***************************************************************
*  
*  Get the base horizontal width from the passed parameter.
*  It is a numeric characters string.
*  
***************************************************************/

if (sb_horiz_base_width && *sb_horiz_base_width)
   if (sscanf(sb_horiz_base_width, "%d%9s", &private->horiz_base_chars, msg) != 1)
      {
         snprintf(msg, sizeof(msg), "Invalid .scrollBarWidth resource (-sbwidth parm) \"%s\", mus be numeric, default of 256 assumed", sb_horiz_base_width);
         dm_error_dspl(msg, DM_ERROR_LOG, main_pad->display_data); 
         private->horiz_base_chars = 256;
      }


} /* end of get_sb_windows  */


/************************************************************************

NAME:      resize_sb_windows     - Reposition scroll bar windows after main window resize

PURPOSE:    This routine will resize and remap the scroll bar windows
            when the windows are being turned on or when the
            main window changes size.  Remapping is only done if the
            private data flags indicate that it should be.  The sb_windows
            are resized after the main pad text area is sized.  Since the
            scroll bars are fixed width and height, they can be accounted
            for statically when forming the main window.

PARAMETERS:
   1.  sb_win        - pointer to SCROLLBAR_DESCR (INPUT/OUTPUT)
                       This is where this module stores its data about the
                       windows.  This data is divided into public and private
                       sections.  The private section  is described by type
                       SB_PRIVATE.

   2.  main_window   - pointer to DRAWABLE_DESCR (INPUT)
                       This is a pointer to the main window drawable description.
                       The DM windows, will be children of this window.
                       The main window has already been sized allowing for
                       VERT_SCROLL_BAR_WIDTH and  HORIZ_SCROLL_BAR_HEIGHT
                       pixels being available for the scroll bars if the scroll
                       bars are to be used.

   3.  display       - pointer to Disply (Xlib type) (INPUT)
                       This is the current display pointer.



FUNCTIONS :

   1.   Determine the shape of the new scroll bar windows.  For the vertical
        window this is the height of the drawable area in the main window.
        For the horizontal window this is the width of the drawable area in
        the main window minus the width of the vertical scroll bar.

   2.   If the window is to be mapped, configure it to it's new shape.

   3.   Map the window if required.

NOTES:

   1.   If the scroll bar mode is auto, a fair amount of rework is possible.
        This is especially true for the horizontal scroll bar.  After drawing
        the main window, a check is made to see if the scroll bar is needed.
        If so, and it is off, resize dm_windows has to be called to create
        the windows and then the screen has to be redrawn.  As the scroll bar
        exists only when data is wider than the window, this can flop back
        and forth pretty regularly.


*************************************************************************/

void  resize_sb_windows(SCROLLBAR_DESCR  *sb_win,           /* input / output  */
                        DRAWABLE_DESCR   *main_window,      /* input   */
                        Display          *display)          /* input   */
{
int                   vert_x;
int                   vert_y;
int                   vert_width;
int                   vert_height;
int                   horiz_x;
int                   horiz_y;
int                   horiz_width;
int                   horiz_height;
SB_PRIVATE           *private;
unsigned long         window_mask;
XWindowChanges        window_attrs;


if (((private = check_private_data(sb_win)) == NULL) || (private->sb_disabled))
   return;


/***************************************************************
*
*  Determine the height, width, X and Y of the windows.
*
***************************************************************/

if (!sb_win->vert_window_on && !sb_win->horiz_window_on)
   return;


vert_x        = main_window->width-VERT_SCROLL_BAR_WIDTH;
vert_y        = main_window->sub_y-MARGIN;
vert_width    = VERT_SCROLL_BAR_WIDTH;
if (sb_win->horiz_window_on)
   vert_height   = main_window->sub_height+MARGIN+HORIZ_SCROLL_BAR_HEIGHT+1;
else
   vert_height   = main_window->sub_height+MARGIN+1;

horiz_x       = main_window->sub_x-MARGIN;
horiz_y       = main_window->sub_height+main_window->sub_y+1;
horiz_width   = main_window->sub_width+MARGIN;
horiz_height  = HORIZ_SCROLL_BAR_HEIGHT;

/* public dimensions */
private->vert_window_height = vert_height;
private->horiz_window_width = horiz_width;


DEBUG1(
   fprintf(stderr, "resize_sb_windows:  main:  %dx%d%+d%+d(%dx%d%+d%+d), vert:  %dx%d%+d%+d, horiz:  %dx%d%+d%+d\n",
                   main_window->width, main_window->height, main_window->x, main_window->y,
                   main_window->sub_width, main_window->sub_height, main_window->sub_x, main_window->sub_y,
                   vert_width, vert_height, vert_x, vert_y,
                   horiz_width, horiz_height, horiz_x, horiz_y);
)

/***************************************************************
*
*  Resize and remap the currently unmapped windows
*
***************************************************************/

if (sb_win->vert_window_on)
   {
      window_attrs.height           = vert_height;
      window_attrs.width            = vert_width;
      window_attrs.x                = vert_x;
      window_attrs.y                = vert_y;
      
      window_mask = CWX
                  | CWY
                  | CWWidth
                  | CWHeight;
      
      DEBUG9(XERRORPOS)
      XConfigureWindow(display, sb_win->vert_window, window_mask, &window_attrs);
      DEBUG9(XERRORPOS)
      XMapWindow(display, sb_win->vert_window);
      private->vert_last_firstline = -1;
   }

if (sb_win->horiz_window_on)
   {
      window_attrs.height           = horiz_height;
      window_attrs.width            = horiz_width;
      window_attrs.x                = horiz_x;
      window_attrs.y                = horiz_y;
      
      window_mask = CWX
                  | CWY
                  | CWWidth
                  | CWHeight;
      
      DEBUG9(XERRORPOS)
      XConfigureWindow(display, sb_win->horiz_window, window_mask, &window_attrs);
      DEBUG9(XERRORPOS)
      XMapWindow(display, sb_win->horiz_window);
      private->horiz_last_firstchar = -1;
   }

} /* end of resize_sb_windows  */


/************************************************************************

NAME:   draw_vert_sb          - Draw the vertical scroll bar

PURPOSE:    This routine draws the vertical scroll bar.

PARAMETERS:
   1.  sb_win        - pointer to SCROLLBAR_DESCR (INPUT/OUTPUT)
                       This is where this module stores its data about the
                       windows.  This data is divided into public and private
                       sections.  The private section  is described by type
                       SB_PRIVATE.

   2.  main_pad      - pointer to PAD_DESCR (INPUT)
                       This is a pointer to the main pad description.
                       It is needed to get to the main token and the, current
                       position in the file, and the drawing data form the
                       main window.

   3.  display       - pointer to Display (Xlib type) (INPUT)
                       This is the current display pointer.

FUNCTIONS :
   1.   If the sb_mode is AUTO, see if a scroll bar is needed.  If the need
        does not match the current state, set the vert_window_on flag and
        generate a configurenotify to recalculate the windows.  If we have
        to reconfigure and the window is on, we will manually unmap it to
        make it behave like a real configurenotify.

   2.   Draw the arrows and fixed portion of the scroll bar gutter.

   3.   Calculate the size of the scroll bar box and draw it with a
        series of lines and boxes.

RETURNED VALUE:
   reconfigure   -    int flag
                      True  -  A configure was generated, will have to redraw.
                      False -  Everything was ok

NOTES:
   1.  This routine is called after the main pad drawing is done.

*************************************************************************/

int  draw_vert_sb(SCROLLBAR_DESCR  *sb_win,
                  PAD_DESCR        *main_pad,
                  Display          *display)
{
SB_PRIVATE           *private;
int                   should_be_on;
XPoint                shadow[12];
GC                    gc1;
GC                    gc2;

if (((private = check_private_data(sb_win)) == NULL) || (private->sb_disabled))
   return(False);

/***************************************************************
*  
*  See if we should be on or off, if we are in the wrong state,
*  Fix it and send a configuration notify to recalculate all the
*  window sizes.
*  
***************************************************************/

if ((sb_win->sb_mode == SCROLL_BAR_OFF) ||
    ((sb_win->sb_mode == SCROLL_BAR_AUTO) && (total_lines(main_pad->token) <= main_pad->window->lines_on_screen)))
   should_be_on = False;
else
   should_be_on = True;

if (should_be_on != sb_win->vert_window_on)
   {
      sb_win->vert_window_on = should_be_on;
      if (!should_be_on)
         {
            DEBUG9(XERRORPOS)
            XUnmapWindow(display, sb_win->vert_window);
         }

      reconfigure(main_pad, display); /* in windows.c */
      return(True);
   }

if (!should_be_on)
   {
      private->vert_last_firstline = -1;
      return(False);
   }


/***************************************************************
*  
*  Draw the fixed portion of the scroll bar, the background,
*  
***************************************************************/

DEBUG9(XERRORPOS)
XFillRectangle(display,
               sb_win->vert_window,
               private->bg_gc,
               0, 0,  /* x=0 y=0 relative to the window */
               VERT_SCROLL_BAR_WIDTH-1,
               private->vert_window_height);

/***************************************************************
*  The dark line for the outer gutter (left and top)
***************************************************************/
shadow[0].x = 0;
shadow[0].y = private->vert_window_height-1;
shadow[1].x = 0;
shadow[1].y = 0;
shadow[2].x = VERT_SCROLL_BAR_WIDTH-1;
shadow[2].y = 0;

DEBUG9(XERRORPOS)
XDrawLines(display,
           sb_win->vert_window,
           private->darkline_gc,
           shadow,
           3, /* number of points */
           CoordModeOrigin);

/***************************************************************
*  The light line for the outer gutter  (right and bottom)
***************************************************************/
shadow[0].x = VERT_SCROLL_BAR_WIDTH-2;
shadow[0].y = 1;
shadow[1].x = VERT_SCROLL_BAR_WIDTH-2;
shadow[1].y = private->vert_window_height-1;
shadow[2].x = 0;
shadow[2].y = private->vert_window_height-1;

DEBUG9(XERRORPOS)
XDrawLines(display,
           sb_win->vert_window,
           private->lightline_gc,
           shadow,
           3, /* number of points */
           CoordModeOrigin);

/***************************************************************
*  Calculate the size and shape of the slider.
***************************************************************/
size_vert_slider(main_pad->window->lines_on_screen,
                 total_lines(main_pad->token),
                 main_pad->first_line,
                 private);


/***************************************************************
*  The table top of the slider
***************************************************************/
DEBUG9(XERRORPOS)
XFillRectangle(display,
               sb_win->vert_window,
               private->slidertop_gc,
               2, private->vert_slider_top+1,  /* x, y */
               VERT_SCROLL_BAR_WIDTH-5, /* 2 for x col, 1 for bg, 1 for gutter shadow, 1 for slider shadow */
               private->vert_slider_height-1);

/***************************************************************
*  If any debug is on, show where the hot spot on the slider is
***************************************************************/
DEBUG(
   DEBUG9(XERRORPOS)
   XDrawLine(display,
             sb_win->vert_window,
             private->darkline_gc,
             2,                       private->vert_slider_hotspot,
             VERT_SCROLL_BAR_WIDTH-5, private->vert_slider_hotspot);
)

/***************************************************************
*  The light line for the slider edge.
***************************************************************/
shadow[0].x = 1;
shadow[0].y = private->vert_slider_top+private->vert_slider_height;
shadow[1].x = 1;
shadow[1].y = private->vert_slider_top;
shadow[2].x = VERT_SCROLL_BAR_WIDTH-2;
shadow[2].y = private->vert_slider_top;

DEBUG9(XERRORPOS)
XDrawLines(display,
           sb_win->vert_window,
           private->lightline_gc,
           shadow,
           3, /* number of points */
           CoordModeOrigin);

/***************************************************************
*  The dark line for the slider edge.
***************************************************************/
shadow[0].x = VERT_SCROLL_BAR_WIDTH-3;
shadow[0].y = private->vert_slider_top+1;
shadow[1].x = VERT_SCROLL_BAR_WIDTH-3;
shadow[1].y = private->vert_slider_top+(private->vert_slider_height-1);
shadow[2].x = 2;
shadow[2].y = private->vert_slider_top+(private->vert_slider_height-1);

DEBUG9(XERRORPOS)
XDrawLines(display,
           sb_win->vert_window,
           private->darkline_gc,
           shadow,
           3, /* number of points */
           CoordModeOrigin);


/***************************************************************
*  The top arrow
***************************************************************/
if (private->sb_area == PTR_IN_ARROW_UP)
   {
      gc1 = private->darkline_gc;
      gc2 = private->lightline_gc;
   }
else
   {
      gc1 = private->lightline_gc;
      gc2 = private->darkline_gc;
   }


/***************************************************************
*  The flat area.
***************************************************************/
shadow[0].x = 1;
shadow[0].y = ARROW_HEIGHT;
shadow[1].x = (ARROW_WIDTH+1)/2,
shadow[1].y = 1;
shadow[2].x = ARROW_WIDTH+1;
shadow[2].y = ARROW_HEIGHT;

DEBUG9(XERRORPOS)
XFillPolygon(display, sb_win->vert_window, private->slidertop_gc,
             shadow,
             3, /* number of points */
             Convex,
             CoordModeOrigin);

/***************************************************************
*  The bottom shadow/highlight
***************************************************************/
DEBUG9(XERRORPOS)
XDrawLine(display, sb_win->vert_window, gc2,
            2,             ARROW_HEIGHT,
            ARROW_WIDTH+1, ARROW_HEIGHT);

/***************************************************************
*  The right/top shadow/highlight
***************************************************************/
shadow[0].x = ARROW_WIDTH;
shadow[0].y = ARROW_HEIGHT-1;
shadow[1].x = ARROW_WIDTH-1;
shadow[1].y = ARROW_HEIGHT-3;
shadow[2].x = ARROW_WIDTH-2;
shadow[2].y = ARROW_HEIGHT-5;
shadow[3].x = ARROW_WIDTH-3;
shadow[3].y = ARROW_HEIGHT-7;
shadow[4].x = ARROW_WIDTH-4;
shadow[4].y = ARROW_HEIGHT-9;

DEBUG9(XERRORPOS)
XDrawPoints(display, sb_win->vert_window, gc2,
            shadow,
            5, /* number of points */
            CoordModeOrigin);

/***************************************************************
*  The left/top highlight/shadow
***************************************************************/
shadow[0].x = 1;
shadow[0].y = ARROW_HEIGHT;
shadow[1].x = 1;
shadow[1].y = ARROW_HEIGHT-1;
shadow[2].x = 2;
shadow[2].y = ARROW_HEIGHT-3;
shadow[3].x = 3;
shadow[3].y = ARROW_HEIGHT-5;
shadow[4].x = 4;
shadow[4].y = ARROW_HEIGHT-7;
shadow[5].x = 5;
shadow[5].y = ARROW_HEIGHT-9;
shadow[6].x = 6;
shadow[6].y = ARROW_HEIGHT-10;

DEBUG9(XERRORPOS)
XDrawPoints(display, sb_win->vert_window, gc1,
           shadow,
           7, /* number of points */
           CoordModeOrigin);

/***************************************************************
*  The bottom arrow
***************************************************************/
if (private->sb_area == PTR_IN_ARROW_DOWN)
   {
      gc1 = private->darkline_gc;
      gc2 = private->lightline_gc;
   }
else
   {
      gc1 = private->lightline_gc;
      gc2 = private->darkline_gc;
   }

/***************************************************************
*  The flat area.
***************************************************************/
shadow[0].x = 1;
shadow[0].y = private->vert_window_height-(ARROW_HEIGHT);
shadow[1].x = (ARROW_WIDTH+1)/2;
shadow[1].y = private->vert_window_height-1;
shadow[2].x = ARROW_WIDTH;
shadow[2].y = private->vert_window_height-(ARROW_HEIGHT);

DEBUG9(XERRORPOS)
XFillPolygon(display, sb_win->vert_window, private->slidertop_gc,
             shadow,
             3, /* number of points */
             Convex,
             CoordModeOrigin);

/***************************************************************
*  The top shadow/highlight
***************************************************************/
DEBUG9(XERRORPOS)
XDrawLine(display, sb_win->vert_window, gc1,
            1,             private->vert_window_height-(ARROW_HEIGHT+1),
            ARROW_WIDTH+1, private->vert_window_height-(ARROW_HEIGHT+1));

/***************************************************************
*  The left/bottom shadow/highlight
***************************************************************/
shadow[0].x = 1;
shadow[0].y = (private->vert_window_height-(ARROW_HEIGHT+1))+1;
shadow[1].x = 2;
shadow[1].y = (private->vert_window_height-(ARROW_HEIGHT+1))+3;
shadow[2].x = 3;
shadow[2].y = (private->vert_window_height-(ARROW_HEIGHT+1))+5;
shadow[3].x = 4;
shadow[3].y = (private->vert_window_height-(ARROW_HEIGHT+1))+7;
shadow[4].x = 5;
shadow[4].y = (private->vert_window_height-(ARROW_HEIGHT+1))+9;

DEBUG9(XERRORPOS)
XDrawPoints(display, sb_win->vert_window, gc1,
            shadow,
            5, /* number of points */
            CoordModeOrigin);

/***************************************************************
*  The left/top highlight/shadow
***************************************************************/
shadow[0].x = ARROW_WIDTH;
shadow[0].y = (private->vert_window_height-(ARROW_HEIGHT+1))+1;
shadow[1].x = ARROW_WIDTH-1;
shadow[1].y = (private->vert_window_height-(ARROW_HEIGHT+1))+3;
shadow[2].x = ARROW_WIDTH-2;
shadow[2].y = (private->vert_window_height-(ARROW_HEIGHT+1))+5;
shadow[3].x = ARROW_WIDTH-3;
shadow[3].y = (private->vert_window_height-(ARROW_HEIGHT+1))+7;
shadow[4].x = ARROW_WIDTH-4;
shadow[4].y = (private->vert_window_height-(ARROW_HEIGHT+1))+9;
shadow[5].x = ARROW_WIDTH-5;
shadow[5].y = (private->vert_window_height-(ARROW_HEIGHT+1))+10;

DEBUG9(XERRORPOS)
XDrawPoints(display, sb_win->vert_window, gc2,
           shadow,
           6, /* number of points */
           CoordModeOrigin);

return(False);

} /* end of draw_vert_sb */


/************************************************************************

NAME:   size_vert_slider           - Size and position the vertical slider

PURPOSE:    This routine calculates where the top of the slider should go
            in the scroll bar gutter and the size of the slider.

PARAMETERS:
   1.  lines_on_screen  - int (INPUT)
                          total number of lines currently displayed

   2.  total_lines      - int (INPUT)
                          total lines in the file.

   2.  first_line       - int (INPUT)
                          Line number of the first line in the main window.

   3.  private          - pointer to void (INPUT/OUTPUT)
                          This is where this module stores its data about the
                          windows.  This data is not visible outsidet this module.
                          This pointer must not be null.
                          It is described by type SB_PRIVATE.
                          fields modified: vert_slider_top 
                                           vert_slider_hotspot
                                           vert_slider_height
                                           vert_last_firstline

FUNCTIONS :
   1.   Calculate the height of the slider.

   2.   Determine where the hot spot for the slider goes in the scroll bar
        window.  The hot spot on the slider is at the same percent down
        the window that the line on the screen is.

   3.   Calculate the top of the slider.  The hot spot is the same percentage
        into the slider that the hot spot is into the file.

                                                                                       UNITS P=PIXELS
                                 -----------------+                                          L=Lines
                                 ^                | ^   ^    A total lines in file             (L)
                                 |                | |   |    B lines on screen                 (L)
                                 H                |          C gutter height                   (P)
                                 |                | |   |    D offset hotspot to top of gutter (P)
                                 v                |          E offset hostpot to top of slider (P)
      +-------------------------------+           | |   |    F slider height                   (P)
    ^ |      ^                        |           |          G winline hot spot points at      (L)
    | |      |       ---    +-----+   | --- ---   | |   |    H first line on screen            (P)
      |               ^     |     |   | ^    ^    |          J total line space (A + B)        (P)
    | |      |        |     |     |   | |    |    | |   |    K total virtual gutter area (C+F) (P)
      |      G              |     |   |           |
    | |      |        D     |     |   | |    |    | |   |
      |               |   - |+---+| - |           |          RATIOS:
    | |      |            ^ ||   || ^ | |    |    | |   |
      |               |   E ||   || | |           | A   J    G+H    D   E   G
    | |      V        v   v ||   ||   | |    |    | |   |    --- =  - = - = -
      |     ---       ----- ||---|| F | C    K    |           J     C   F   B
    | |                     ||   ||   |      |    | |   |
    B |                     ||   || | | |         |
    | |                     ||   || v |      |    | |   |        B*C
      |                     |+---+|   | |         |          F = ---
    | |                     |     |   |      |    | |   |         A
      |                     |     |   | |         |
    | |                     |     |   |      |    | |   |        B*H
      |                     |     |   | |         |          G = ---
    | |                     |     |   |      |    | |   |         A
      |                     |     |   | v         |
    | |                     +-----+   | ---  |    | |   |        (G + H) * K
      |                               |           |          D = -----------
    V |                               |      |    | |   |            J
      +-------------------------------+           |
                                             |    | |   |        (G + H) * F
                                                  |          E = -----------
                                             |    | |   |            J
                                             v    |
                                            ---   | |   |
                                                  |
                                                  | |   |
                                                  | v
                                         ---------+     |
                                                    ^
                                                    |   |

                                                    |   |
                                                    B
                                                    |   |

                                                    |   |
                                                    v   v

*************************************************************************/

static void size_vert_slider(int              lines_on_screen, /*  B  */
                             int              total_lines,     /*  A  */
                             int              first_line,      /*  H  */
                             SB_PRIVATE      *private)
{
unsigned int          usable_pixels;           /*  C  */
unsigned int          hot_spot;                /*  D  */
unsigned int          slider_top_to_hot_spot;  /*  E  */
unsigned int          slider_height;           /*  F  */
unsigned int          lines_to_hot_spot;       /*  G  */
unsigned int          slider_top;              /*  D - E */
unsigned int          max_lines;               /*  J = A + B */
unsigned int          max_gutter;              /*  K = C + F */
unsigned int          i;

/***************************************************************
*  Calculate C : Usable pixels in the slider gutter
*  Calculate J : Total lines on screen plus lines on window.
***************************************************************/
usable_pixels = private->vert_window_height-((2*ARROW_HEIGHT)+2);  /* +2-> 1 each for top and bottom shadows */
max_lines     = total_lines + lines_on_screen;

/***************************************************************
*  Calculate F
*  Take care of the case where the window is bigger than the
*  file.  The slider will take up the whole screen.
*  Then adjust the size if this is not the case.
***************************************************************/

slider_height = usable_pixels;

if (lines_on_screen < total_lines)   /* if B < A then F = (B * F) / A */
   slider_height =  (lines_on_screen * slider_height) / total_lines;

DEBUG23(
   fprintf(stderr, "@size_vert_slider: Raw slider_height=%d, ", slider_height);
)

/***************************************************************
*  Avoid sliders which are below minimum size or above the size
*  of the gutter.
***************************************************************/
if (slider_height <= MIN_SLIDER_HEIGHT)
   slider_height = MIN_SLIDER_HEIGHT;

if (slider_height > usable_pixels)
   slider_height = usable_pixels;

/***************************************************************
*  Calculate K: Total virtual gutter.  Gutter Plus slider since
*  The slider can go past the bottom of the gutter.
***************************************************************/
max_gutter = usable_pixels + slider_height;

/***************************************************************
*  Figure out how far into the usable gutter the hot spot should go.
*  We want the hot spot in the slide bar to be the same percent
*  into the scroll window gutter as the line it points to is
*  into the total line of the file.  Thus if the line is 1/4
*  of the way into the file, the hot spot will be 1/4 of the way
*  down the window and 1/4 of the way down the slider.
***************************************************************/

if (total_lines == 0)
   total_lines = 1;  /* avoid zero divide */

/***************************************************************
*  Calculate G : Window lines to hot spot
*  The total_lines / 2 causes rounding.
*  G = (B * H) / A
***************************************************************/
/*lines_to_hot_spot = ((lines_on_screen * first_line) + (total_lines/2)) / total_lines don't want rounding*/
lines_to_hot_spot = (lines_on_screen * first_line) / total_lines;

/***************************************************************
*  Set the hot spot in the window.  (Calculate D)
*  ARROW_HEIGHT+1 -> +1 is for the top shadow.
*  D = ((G + H) * K) / J
***************************************************************/
hot_spot = (ARROW_HEIGHT+1) + (((lines_to_hot_spot + first_line) * max_gutter) / max_lines);

/***************************************************************
*  Don't let the hot spot slide off the bottom of the window
*  unles lines to hot spot went off the window.
***************************************************************/
if ((hot_spot > (usable_pixels + ARROW_HEIGHT)) && ((lines_to_hot_spot + first_line) < (unsigned)total_lines))
   hot_spot = usable_pixels + ARROW_HEIGHT;

/***************************************************************
*  Set the offset from the hot spot to the top of the slider
*  (Calculate E)
*  E = ((G + H) * F) / J
***************************************************************/
slider_top_to_hot_spot = (((lines_to_hot_spot + first_line) * slider_height) / max_lines);

/***************************************************************
*  Calculate location of the top of the slider
*  slider_top = D - E
***************************************************************/
slider_top = hot_spot - slider_top_to_hot_spot;

/***************************************************************
*  Avoid the odd case where you scroll with keys and the slider
*  moves the wrong way because of rounding.
***************************************************************/
if (private->vert_last_firstline != -1)
   if ((private->vert_last_firstline < first_line) && ((unsigned)private->vert_slider_top > slider_top))
      slider_top = private->vert_slider_top;
   else
      if ((private->vert_last_firstline > first_line) && ((unsigned)private->vert_slider_top < slider_top))
         slider_top = private->vert_slider_top;

DEBUG23(
   fprintf(stderr, "slider_height=%d, first_line=%d, lines_to_hot_spot=%d, total_lines=%d, usable_pixels=%d, lines_on_screen=%d, hot_spot=%d, slider_top=%d, slider_top_to_hot_spot=%d\n",
           slider_height, first_line, lines_to_hot_spot, total_lines, usable_pixels, lines_on_screen, hot_spot, slider_top, slider_top_to_hot_spot);
)

/***************************************************************
*  Update the elements of the structure to describe the scroll bar.
***************************************************************/
private->vert_slider_top     = slider_top;
private->vert_slider_hotspot = hot_spot;
private->vert_slider_height  = slider_height;
private->vert_last_firstline = first_line;

/***************************************************************
*  Avoid having the pointer drift in the slider while moving the
*  slider due to rounding errors.
***************************************************************/
if (private->sb_area == PTR_IN_VERT_SLIDER)
   {
      i = private->slider_last_pos - private->vert_slider_top;
      if ((int)i != private->slider_pointer_offset)
         private->slider_delta_tweek = i - private->slider_pointer_offset;
   }

} /* end of size_vert_slider */


/************************************************************************

NAME:   draw_horiz_sb          - Draw the horizontal scroll bar

PURPOSE:    This routine draws the vertical scroll bar.

PARAMETERS:
   1.  sb_win        - pointer to SCROLLBAR_DESCR (INPUT/OUTPUT)
                       This is where this module stores its data about the
                       windows.  This data is divided into public and private
                       sections.  The private section  is described by type
                       SB_PRIVATE.

   2.  main_pad      - pointer to PAD_DESCR (INPUT)
                       This is a pointer to the main pad description.
                       It is needed to get to the main token and the, current
                       position in the file, and the drawing data form the
                       main window.

   3.  display       - pointer to Disply (Xlib type) (INPUT)
                       This is the current display pointer.


FUNCTIONS :
   1.   If the sb_mode is AUTO, see if a scroll bar is needed.  If the need
        does not match the current state, set the vert_window_on flag and
        generate a configurenotify to recalculate the windows.  If we have
        to reconfigure and the window is on, we will manually unmap it to
        make it behave like a real configurenotify.
        The maximum line width is taken from the winlines data.

   2.   Draw the arrows and fixed portion of the scroll bar gutter.

   3.   Calculate the size of the scroll bar box and draw it with a
        series of lines and boxes.

RETURNED VALUE:
   reconfigure   -    int flag
                      True  -  A configure was generated, will have to redraw.
                      False -  Everything was ok

NOTES:
   1.  This routine is called after the main pad drawing is done.  Thus 
       the winlines structure is up to date.

   2.  If the mode is auto and the scroll bar is currently on, check the
       two lines past the bottom of the displayed window to see if they
       would call for a scroll bar.  This is to avoid the situation where
       the last line on the window needs the horizontal scroll bar, but
       this line is not displayed when the scroll bars are added in.

*************************************************************************/

int  draw_horiz_sb(SCROLLBAR_DESCR  *sb_win,
                   PAD_DESCR        *main_pad,
                   Display          *display)
{
SB_PRIVATE           *private;
int                   should_be_on;
int                   widest_line;
XPoint                shadow[8];
GC                    gc1;
GC                    gc2;

if (((private = check_private_data(sb_win)) == NULL) || (private->sb_disabled))
   return(False);

/***************************************************************
*  
*  See if we should be on or off, if we are in the wrong state,
*  Fix it and send a configuration notify to recalculate all the
*  window sizes.
*  
***************************************************************/

if ((sb_win->sb_mode == SCROLL_BAR_OFF) || (private->horiz_base_chars == 0))
   should_be_on = False;
else
   {
      widest_line = max_text_pixels(main_pad,
                                    sb_win->horiz_window_on,
                                    &private->horiz_max_chars);

      if ((sb_win->sb_mode == SCROLL_BAR_AUTO) && ((unsigned)widest_line < main_pad->window->sub_width) && (main_pad->first_char == 0))
         should_be_on = False;
      else
         should_be_on = True;
   }

if ((should_be_on != sb_win->horiz_window_on) && (private->sb_area != PTR_IN_VERT_SLIDER))
   {
      sb_win->horiz_window_on = should_be_on;
      if (should_be_on)
         {
            /***************************************************************
            *  RES 11/6/97
            *  If we are on the line to be covered by the scroll bar, move
            *  up off the scroll bar unless this is the last line on the file
            *  in which case we bump the screen down one line.
            ***************************************************************/

            /* if we are on the line to be covered by the scroll bar, move up off the scroll bar */
            if (main_pad->file_line_no == main_pad->first_line+(main_pad->window->lines_on_screen-1))
               {
                 if (main_pad->file_line_no == (total_lines(main_pad->token) - 1))
                    {
                       DEBUG23(fprintf(stderr, "draw_horiz_sb:  Shifting file up 1 line\n");)
                       main_pad->first_line++;
                    }
                 else
                    {
                       DEBUG23(fprintf(stderr, "draw_horiz_sb:  Shifting cursor up 1 line\n");)
                       main_pad->file_line_no--;
                    }
                 main_pad->display_data->cursor_buff->win_line_no--;
                 main_pad->display_data->cursor_buff->y -= HORIZ_SCROLL_BAR_HEIGHT;
                 main_pad->display_data->redo_cursor_in_configure = REDRAW_FORCE_REAL_WARP;
               }

            /***************************************************************
            *  RES 12/10/97
            *  In pad mode, move up one line if we are about to cover up
            *  the last line with the scroll bar, move up one line.
            ***************************************************************/
            if (main_pad->display_data->pad_mode && total_lines(main_pad->token) == main_pad->first_line+(main_pad->window->lines_on_screen))
               {
                 DEBUG23(fprintf(stderr, "draw_horiz_sb:  Shifting transcript pad up 1 line\n");)
                 main_pad->first_line++;
               }
         }
      else
         {
            DEBUG9(XERRORPOS)
            XUnmapWindow(display, sb_win->horiz_window);
         }

      reconfigure(main_pad, display); /* in windows.c */
      return(True);
   }

if (!should_be_on)
   {
      private->horiz_last_firstchar = -1;
      return(False);
   }

/***************************************************************
*  
*  Draw the fixed portion of the scroll bar, the background,
*  
***************************************************************/

DEBUG9(XERRORPOS)
XFillRectangle(display,
               sb_win->horiz_window,
               private->bg_gc,
               0, 0,  /* x=0 y=0 relative to the window */
               private->horiz_window_width,
               HORIZ_SCROLL_BAR_HEIGHT);

/***************************************************************
*  The dark line for the outer gutter
***************************************************************/
shadow[0].x = 0;
shadow[0].y = HORIZ_SCROLL_BAR_HEIGHT-1;
shadow[1].x = 0;
shadow[1].y = 0;
shadow[2].x = private->horiz_window_width;
shadow[2].y = 0;

DEBUG9(XERRORPOS)
XDrawLines(display,
           sb_win->horiz_window,
           private->darkline_gc,
           shadow,
           3, /* number of points */
           CoordModeOrigin);

/***************************************************************
*  The light line for the outer gutter
***************************************************************/
shadow[0].x = private->horiz_window_width-1;
shadow[0].y = 1;
shadow[1].x = private->horiz_window_width-1;
shadow[1].y = HORIZ_SCROLL_BAR_HEIGHT-1;
shadow[2].x = 0;
shadow[2].y = HORIZ_SCROLL_BAR_HEIGHT-1;

DEBUG9(XERRORPOS)
XDrawLines(display,
           sb_win->horiz_window,
           private->lightline_gc,
           shadow,
           3, /* number of points */
           CoordModeOrigin);

/***************************************************************
*  
*  Calculate the size and shape of the slider and draw it.
*  
***************************************************************/

size_horiz_slider(main_pad->window->sub_width,
                  main_pad->first_char,
                  main_pad->window->font,
                  private);

/***************************************************************
*  The table top of the slider
***************************************************************/
DEBUG9(XERRORPOS)
XFillRectangle(display,
               sb_win->horiz_window,
               private->slidertop_gc,
               private->horiz_slider_left+1, 2, /* x, y */
               private->horiz_slider_width-2,
               HORIZ_SCROLL_BAR_HEIGHT-4);

/***************************************************************
*  If any debug is on, show where the hot spot on the slider is
***************************************************************/
DEBUG(
   DEBUG9(XERRORPOS)
   XDrawLine(display,
             sb_win->horiz_window,
             private->darkline_gc,
             private->horiz_slider_hotspot, 2,
             private->horiz_slider_hotspot, HORIZ_SCROLL_BAR_HEIGHT-4);
)

/***************************************************************
*  The light line for the slider edge.
***************************************************************/
shadow[0].x = private->horiz_slider_left;
shadow[0].y = HORIZ_SCROLL_BAR_HEIGHT-1;
shadow[1].x = private->horiz_slider_left;
shadow[1].y = 1;
shadow[2].x = private->horiz_slider_left+private->horiz_slider_width;
shadow[2].y = 1;

DEBUG9(XERRORPOS)
XDrawLines(display,
           sb_win->horiz_window,
           private->lightline_gc,
           shadow,
           3, /* number of points */
           CoordModeOrigin);

/***************************************************************
*  The dark line for the slider edge.
***************************************************************/
shadow[0].x = private->horiz_slider_left+(private->horiz_slider_width-1);
shadow[0].y = 2;
shadow[1].x = private->horiz_slider_left+(private->horiz_slider_width-1);
shadow[1].y = HORIZ_SCROLL_BAR_HEIGHT-2;
shadow[2].x = private->horiz_slider_left+1;
shadow[2].y = HORIZ_SCROLL_BAR_HEIGHT-2;

DEBUG9(XERRORPOS)
XDrawLines(display,
           sb_win->horiz_window,
           private->darkline_gc,
           shadow,
           3, /* number of points */
           CoordModeOrigin);

/***************************************************************
*  The left arrow
***************************************************************/
if (private->sb_area == PTR_IN_ARROW_LEFT)
   {
      gc1 = private->darkline_gc;
      gc2 = private->lightline_gc;
   }
else
   {
      gc1 = private->lightline_gc;
      gc2 = private->darkline_gc;
   }


/***************************************************************
*  The flat area.
***************************************************************/
shadow[0].x = ARROW_WIDTH;
shadow[0].y = 1;
shadow[1].x = 1;
shadow[1].y = (ARROW_HEIGHT+1)/2;
shadow[2].x = ARROW_WIDTH;
shadow[2].y = ARROW_HEIGHT;

DEBUG9(XERRORPOS)
XFillPolygon(display, sb_win->horiz_window, private->slidertop_gc,
             shadow,
             3, /* number of points */
             Convex,
             CoordModeOrigin);

/***************************************************************
*  The right shadow/highlight
***************************************************************/
DEBUG9(XERRORPOS)
XDrawLine(display, sb_win->horiz_window, gc2,
            ARROW_WIDTH, 2,
            ARROW_WIDTH, ARROW_HEIGHT+1);

/***************************************************************
*  The left/bottom shadow/highlight
***************************************************************/
shadow[0].x = ARROW_WIDTH-1;
shadow[0].y = ARROW_HEIGHT;
shadow[1].x = ARROW_WIDTH-3;
shadow[1].y = ARROW_HEIGHT-1;
shadow[2].x = ARROW_WIDTH-5;
shadow[2].y = ARROW_HEIGHT-2;
shadow[3].x = ARROW_WIDTH-7;
shadow[3].y = ARROW_HEIGHT-3;
shadow[4].x = ARROW_WIDTH-9;
shadow[4].y = ARROW_HEIGHT-4;

DEBUG9(XERRORPOS)
XDrawPoints(display, sb_win->horiz_window, gc2,
            shadow,
            5, /* number of points */
            CoordModeOrigin);

/***************************************************************
*  The left/top highlight/shadow
***************************************************************/
shadow[0].x = ARROW_WIDTH;
shadow[0].y = 1;
shadow[1].x = ARROW_WIDTH-1;
shadow[1].y = 1;
shadow[2].x = ARROW_WIDTH-3;
shadow[2].y = 2;
shadow[3].x = ARROW_WIDTH-5;
shadow[3].y = 3;
shadow[4].x = ARROW_WIDTH-7;
shadow[4].y = 4;
shadow[5].x = ARROW_WIDTH-9;
shadow[5].y = 5;
shadow[6].x = ARROW_WIDTH-10;
shadow[6].y = 6;

DEBUG9(XERRORPOS)
XDrawPoints(display, sb_win->horiz_window, gc1,
           shadow,
           7, /* number of points */
           CoordModeOrigin);

/***************************************************************
*  The right arrow
***************************************************************/
if (private->sb_area == PTR_IN_ARROW_RIGHT)
   {
      gc1 = private->darkline_gc;
      gc2 = private->lightline_gc;
   }
else
   {
      gc1 = private->lightline_gc;
      gc2 = private->darkline_gc;
   }


/***************************************************************
*  The flat area.
***************************************************************/
shadow[0].x = private->horiz_window_width-(ARROW_WIDTH);
shadow[0].y = 1;
shadow[1].x = private->horiz_window_width-2;
shadow[1].y = (ARROW_HEIGHT+1)/2;
shadow[2].x = private->horiz_window_width-(ARROW_WIDTH);
shadow[2].y = ARROW_HEIGHT;

DEBUG9(XERRORPOS)
XFillPolygon(display, sb_win->horiz_window, private->slidertop_gc,
             shadow,
             3, /* number of points */
             Convex,
             CoordModeOrigin);

/***************************************************************
*  The left shadow/highlight
***************************************************************/
DEBUG9(XERRORPOS)
XDrawLine(display, sb_win->horiz_window, gc1,
            private->horiz_window_width-(ARROW_WIDTH+1), 1,
            private->horiz_window_width-(ARROW_WIDTH+1), ARROW_HEIGHT+1);

/***************************************************************
*  The right/top shadow/highlight
***************************************************************/
shadow[0].x = private->horiz_window_width-(ARROW_WIDTH+1)+1;
shadow[0].y = 1;
shadow[1].x = private->horiz_window_width-(ARROW_WIDTH+1)+3;
shadow[1].y = 2;
shadow[2].x = private->horiz_window_width-(ARROW_WIDTH+1)+5;
shadow[2].y = 3;
shadow[3].x = private->horiz_window_width-(ARROW_WIDTH+1)+7;
shadow[3].y = 4;
shadow[4].x = private->horiz_window_width-(ARROW_WIDTH+1)+9;
shadow[4].y = 5;

DEBUG9(XERRORPOS)
XDrawPoints(display, sb_win->horiz_window, gc1,
            shadow,
            5, /* number of points */
            CoordModeOrigin);

/***************************************************************
*  The right/bottom highlight/shadow
***************************************************************/
shadow[0].x = private->horiz_window_width-(ARROW_WIDTH+1)+1;
shadow[0].y = ARROW_HEIGHT;
shadow[1].x = private->horiz_window_width-(ARROW_WIDTH+1)+3;
shadow[1].y = ARROW_HEIGHT-1;
shadow[2].x = private->horiz_window_width-(ARROW_WIDTH+1)+5;
shadow[2].y = ARROW_HEIGHT-2;
shadow[3].x = private->horiz_window_width-(ARROW_WIDTH+1)+7;
shadow[3].y = ARROW_HEIGHT-3;
shadow[4].x = private->horiz_window_width-(ARROW_WIDTH+1)+9;
shadow[4].y = ARROW_HEIGHT-4;
shadow[5].x = private->horiz_window_width-(ARROW_WIDTH+1)+10;
shadow[5].y = ARROW_HEIGHT-5;

DEBUG9(XERRORPOS)
XDrawPoints(display, sb_win->horiz_window, gc2,
           shadow,
           6, /* number of points */
           CoordModeOrigin);

return(False);

} /* end of draw_horiz_sb */


/************************************************************************

NAME:   size_horiz_slider          - Size and position the horizontal slider

PURPOSE:    This routine calculates where the left side of the slider should go
            in the scroll bar gutter and the size of the slider.

PARAMETERS:
   1.  screen_width     - int (INPUT)
                          The pixel width of the screen.

   2.  first_char       - int (INPUT)
                          First character displayed on the left edge.  This
                          is taken from the main pad description.

   3.  font_data        - pointer to XFontStruct (INPUT) (Xlib type)
                          The width in pixels of the characters is derived
                          from this data.

   4.  private          - pointer to void (INPUT)
                          This is where this module stores its data about the
                          windows.  This data is not visible outsidet this module.
                          This pointer must not be null.
                          It is described by type SB_PRIVATE.
                          fields modified: horiz_slider_left 
                                           horiz_slider_hotspot
                                           horiz_slider_width

FUNCTIONS :
   1.   Calculate the width of the slider.

   2.   Determine where the hot spot for the slider goes in the scroll bar
        window.  The hot spot on the slider is at the same percent across
        the window that the first character displayed is.

   3.   Calculate left edge of the slider.  The hot spot is the same percentage
        into the slider that the hot spot is into the line.

NOTE:
   The calculations for sizing the horizontal slide bar are done like the
   vertical slide bar, except everything is turned on it's ear and we
   convert all units to pixels right at the front.

*************************************************************************/

static void size_horiz_slider(int              screen_width, /*  B  */
                              int              first_char,
                              XFontStruct     *font_data,
                              SB_PRIVATE      *private)
{
unsigned int          longest_line_pix;        /*  A  */
unsigned int          usable_pixels;           /*  C  */
unsigned int          hot_spot;                /*  D  */
unsigned int          slider_left_to_hot_spot; /*  E  */
unsigned int          slider_width;            /*  F  */
unsigned int          hot_spot_char_pix;       /*  G  */
unsigned int          first_char_pix;          /*  H  */
unsigned int          slider_left;             /*  D - E */
unsigned int          max_pix_space;           /*  J = A + B */
unsigned int          max_gutter;              /*  K = C + F */
unsigned int          i;

/***************************************************************
*  Calculate A : Max pixels on any displayed line or the base size
*  Calculate H : Pixel offset to first char on screen
***************************************************************/
longest_line_pix   = MAX(private->horiz_max_chars, private->horiz_base_chars) * font_data->max_bounds.width;
first_char_pix     = first_char * font_data->max_bounds.width;

/***************************************************************
*  Calculate C : Usable pixels in the slider gutter
*  Calculate J : Total chars on line (A) plus chars on window (B);
***************************************************************/
usable_pixels    = private->horiz_window_width-((2*ARROW_HEIGHT)+2);  /* +2-> 1 each for left and right shadows */
max_pix_space    = longest_line_pix + screen_width;

/***************************************************************
*  Calculate F
*  Take care of the case where the window is bigger than the
*  Max width.  The slider will take up the whole screen.
*  Then adjust the size if this is not the case.
***************************************************************/

slider_width = usable_pixels;

if ((unsigned)screen_width < longest_line_pix) /* if B < A then F = (B * F) / A */
   slider_width =  (screen_width * slider_width) / longest_line_pix;

DEBUG23(
   fprintf(stderr, "size_horiz_slider: Raw slider_width=%d, ", slider_width);
)

/***************************************************************
*  Avoid sliders which are below minimum size or above the size
*  of the gutter.
***************************************************************/
if (slider_width <= MIN_SLIDER_WIDTH)
   slider_width = MIN_SLIDER_WIDTH;

if (slider_width > usable_pixels)
   slider_width = usable_pixels;

/***************************************************************
*  Calculate K: Total virtual gutter.  Gutter Plus slider since
*  The slider can go past the bottom of the gutter.
***************************************************************/
max_gutter = usable_pixels + slider_width;

/***************************************************************
*  Figure out how far into the usable gutter the hot spot should go.
*  We want the hot spot in the slide bar to be the same percent
*  into the scroll window gutter as the char it points to is
*  into the total with of the max line.  Thus if the line is 1/4
*  of the way into the line, the hot spot will be 1/4 of the way
*  down the window and 1/4 of the way along the slider.
***************************************************************/

if (longest_line_pix == 0)
   longest_line_pix = 10;  /* avoid zero divide */

/***************************************************************
*  Calculate G : Window lines to hot spot
*  The longest_line_pix / 2 causes rounding.
*  G = (B * H) / A
***************************************************************/
hot_spot_char_pix = ((screen_width * first_char_pix) + (longest_line_pix/2)) / longest_line_pix;

/***************************************************************
*  Set the hot spot in the window.  (Calculate D)
*  ARROW_WIDTH+1 -> +1 is for the top shadow.
*  D = ((G + H) * K) / J
***************************************************************/
hot_spot = (ARROW_WIDTH+1) + (((hot_spot_char_pix + first_char_pix) * max_gutter) / max_pix_space);

/***************************************************************
*  Set the offset from the hot spot to the top of the slider
*  (Calculate E)
*  E = ((G + H) * F) / J
***************************************************************/
slider_left_to_hot_spot = (((hot_spot_char_pix + first_char_pix) * slider_width) / max_pix_space);

/***************************************************************
*  Calculate location of the left edge of the slider
*  slider_left = D - E
***************************************************************/
slider_left = hot_spot - slider_left_to_hot_spot;

/***************************************************************
*  Do let the hot spot slide off the bottom of the window?
*  We won't do the test found in the vertical slider routine.
***************************************************************/

/***************************************************************
*  Avoid the odd case where you scroll with keys and the slider
*  moves the wrong way because of rounding.
***************************************************************/
if (private->horiz_last_firstchar != -1)
   if ((private->horiz_last_firstchar < first_char) && ((unsigned)(private->horiz_slider_left) > slider_left))
      slider_left = private->horiz_slider_left;
   else
      if ((private->horiz_last_firstchar > first_char) && ((unsigned)private->horiz_slider_left < slider_left))
         slider_left = private->horiz_slider_left;


DEBUG23(
   fprintf(stderr, "slider_width=%d, first_char=%d, first_char_pix=%d, usable_pixels=%d, hot_spot=%d, slider_left=%d, hot_spot_char_pix=%d, slider_left_to_hot_spot=%d, longest_line_pix=%d, max_pix_space=%d, max_gutter=%d\n",
           slider_width, first_char, first_char_pix, usable_pixels, hot_spot, slider_left, hot_spot_char_pix, slider_left_to_hot_spot, longest_line_pix, max_pix_space, max_gutter);
)

/***************************************************************
*  Update the elements of the structure to describe the scroll bar.
***************************************************************/
private->horiz_slider_left    = slider_left;
private->horiz_slider_hotspot = hot_spot;
private->horiz_slider_width   = slider_width;

/***************************************************************
*  Avoid having the pointer drift in the slider while moving the
*  slider due to rounding errors.
***************************************************************/
if (private->sb_area == PTR_IN_HORIZ_SLIDER)
   {
      i = private->slider_last_pos - private->horiz_slider_left;
      if ((int)i != private->slider_pointer_offset)
         private->slider_delta_tweek = i - private->slider_pointer_offset;
   }

} /* end of size_horiz_slider */


/************************************************************************

NAME:   max_text_pixels       - Find maximum line width for horiz sb.

PURPOSE:    This routine calculates the widest line on the screen
            in pixels. 

PARAMETERS:
   1.  pad              - PAD_DESCR (INPUT)
                          This is the pad description for the main pad.
                          It contains the token for the data and the 
                          size and shape of the window.  It also shows
                          what lines are currently displayed on the window.

   2.  horiz_window_on  - int (INPUT)
                          This is the flag which shows if the horizontal
                          scroll bar is currently on.  If so, we will
                          get the length from 2 extra lines.  This is
                          to avoid the situation where the last line
                          on the window without the scroll bar
                          is the one which turns the scroll bar on.
                          This would cause an osillation loop turning
                          the scroll bar on and off.

   2.  max_len          - pointer to int (OUTPUT)
                          This is the max stringlen for the window.

FUNCTIONS :
   1.   Get the text width of each full line and find the maximum.
        We go two beyond the number of lines on the screen to avoid
        the situation where the last line on the screen without the 
        scroll bar is the one which turns the scroll bar on.
        The win_lines structure will not help in this case because
        if the window is scrolled over, it does not include the width
        of the part scrolled off the left side of the screen.

RETURNED VALUE:
   widest_line -   int
                   The maximum pixel length of the line.
                   This is the full pixel length of the line.

*************************************************************************/

static int  max_text_pixels(PAD_DESCR       *pad,
                            int              horiz_window_on,
                            int             *max_len)
{
int                   widest_line = 1; /* never allow zero, used in divides */
int                   i;
int                   last_line;
char                 *cur_line;
int                   cur_chars;
int                   max_chars = 0;
int                   first_line;
int                   max_chars_displayable;
char                  work[MAX_LINE+1];
char                 *untabbed_line;

/***************************************************************
*  Figure out the maximum number of characters which could fit
*  in the window.  No sense drawing more than this.
*  RES 3/23/1998 - Add max chars displayable to limit value passed
*  to XDrawString.
***************************************************************/
if (pad->pix_window->font->min_bounds.width > 0)
   max_chars_displayable = (pad->pix_window->sub_width / pad->pix_window->font->min_bounds.width) + 15; /* 15 is a fudge padding */
else
   max_chars_displayable = (pad->pix_window->sub_width / 2) + 15; /* assume 2 pixel wide characters */



last_line = pad->first_line + pad->window->lines_on_screen;
first_line = pad->first_line;
if (horiz_window_on)
   {
      /***************************************************************
      *  Avoid dancing window (oscilating horizontal scroll bar)
      *  when wide line just scrolls off bottom of the window.
      ***************************************************************/
      last_line += 2;
      /***************************************************************
      *  Avoid dancing window when wide lines just scrolls off the top
      *  in ceterm mode.
      ***************************************************************/
      if (pad->display_data->pad_mode)
         {
            first_line -= 2;
            if (first_line < 0)
               first_line = 0;
         }
   }
if (last_line > total_lines(pad->token))
   last_line = total_lines(pad->token); 

position_file_pointer(pad->token, first_line);

/***************************************************************
*  Take care of the case where the window is bigger than the
*  file.  The slider will take up the whole screen.
***************************************************************/

for (i = first_line;
     i < last_line && ((cur_line = next_line(pad->token)) != NULL);
     i++)
{
   if (untab(cur_line, work, max_chars_displayable+pad->first_char, pad->display_data->hex_mode))
      untabbed_line = work;
   else
      untabbed_line = cur_line;

   cur_chars = strlen(untabbed_line);
   if (cur_chars > max_chars)
      {
         max_chars = cur_chars;
         widest_line = cur_chars * pad->window->font->max_bounds.width;
      }
}

*max_len = max_chars;
return(widest_line);

} /* end of max_text_pixels */


/************************************************************************

NAME:   vert_button_press     - Process a button press in the vertical sb window

PURPOSE:    This routine is called when a button press or release is performed
            in the scroll bar window.

PARAMETERS:
   1.  event            - pointer to XEvent (INPUT)
                          This is the event to be processed.
                          This is either a ButtonPress or ButtonRelease

   2.  dspl_descr       - pointer to DISPLAY_DESCR (INPUT)
                          This is the current display description.  It is
                          the one we are displaying to.  It is needed to 
                          redraw the main pad and the scroll bar.

FUNCTIONS :
   1.   Determine if the button press occured in the top arrow, bottom arrow
        slider, top gutter area, or bottom gutter area.

   2.   For the top or bottom arrows scroll the screen 1 line.
        Copy the XEvent to the sb_data area and set the timer for 
        multiClickTime milliseconds.

   3.   If the button press is in the area between the slider and the
        scroll bar, scroll one page.
        Copy the XEvent to the sb_data area and set the timer for 
        multiClickTime milliseconds.

   4.   If the button press is on the slider, save the current cursor
        location and do not set the timer.  Delta's in motion events
        will be calculated in vert_sb_motion.

   5.   If this is a button release, clear the timer flag.

RETURNED VALUE:
   redraw -  If the screen needs to be redrawn, the mask of which screen
             and what type of redrawing is returned.

*************************************************************************/

int  vert_button_press(XEvent           *event,
                       DISPLAY_DESCR    *dspl_descr)
{
SB_PRIVATE           *private = (SB_PRIVATE *)dspl_descr->sb_data->sb_private;
int                   redraw = 0;
int                   scroll_amount = 0;
int                   accelerated_scroll_amount = 0;
int                   page = False;
int                   junk;
int                   new_first;
DMC                   scroll_dmc;
BUFF_DESCR            fake_cursor_buff;
int                   m2_button_press = False;

if (!private)  /* I don't trust anyone, but this should never be NULL */
   return(0);

if (!private->drag_out_of_window_scrolling)
   if ((dspl_descr->sb_data->sb_mode == SCROLL_BAR_OFF) ||
       (private->sb_disabled) ||
       !dspl_descr->sb_data->vert_window_on)
      return(0);

/***************************************************************
*  If this is a button press, figure out where in the window it is.
***************************************************************/
if (event->type == ButtonPress)
   {
      if (event->xbutton.y < ARROW_HEIGHT)
         {
            private->sb_area = PTR_IN_ARROW_UP; 
            scroll_amount = -1;
         }
      else
         if (event->xbutton.y >= (int)(private->vert_window_height-ARROW_HEIGHT))
            {
               private->sb_area = PTR_IN_ARROW_DOWN; 
               scroll_amount = 1;
            }
         else
            if (event->xbutton.y < private->vert_slider_top)
               {
                  private->sb_area = PTR_IN_EMPTY_TOP; 
                  scroll_amount = -1;
                  page = True;
               }
            else
               if (event->xbutton.y >= (private->vert_slider_top+private->vert_slider_height))
                  {
                     private->sb_area = PTR_IN_EMPTY_BOTTOM; 
                     scroll_amount = 1;
                     page = True;
                  }
               else
                  private->sb_area = PTR_IN_VERT_SLIDER; 

      /***************************************************************
      *  For mouse button 2, move the slider so the hot spot is on
      *  the mouse position.  The calculation reads:  Take the ratio
      *  of the mouse position in the scroll bar window to the
      *  height of the scroll bar window and apply it to the number
      *  of lines in the file.
      ***************************************************************/
      if ((event->xbutton.button == 2) && ((private->sb_area == PTR_IN_EMPTY_TOP) || (private->sb_area == PTR_IN_EMPTY_BOTTOM)))
         {
            new_first = ((event->xbutton.y - ARROW_HEIGHT) * total_lines(dspl_descr->main_pad->token)) / (private->vert_window_height-(2*ARROW_HEIGHT));
            if (new_first < 0)
               new_first = 0;
            scroll_amount  = new_first - dspl_descr->main_pad->first_line;
            page = False;
            m2_button_press = True;
         }
      else
         if (((event->xbutton.x < 0) || (event->xbutton.x > VERT_SCROLL_BAR_WIDTH)) &&
             ((private->sb_area == PTR_IN_ARROW_UP) || (private->sb_area == PTR_IN_ARROW_DOWN)))
            {
               /* slid sideways while holding the button down */
               accelerated_scroll_amount = scroll_amount * (-event->xbutton.x / 16);
               if (ABSVALUE(accelerated_scroll_amount) > ABSVALUE(scroll_amount))
                  scroll_amount = accelerated_scroll_amount;
            }


      DEBUG23(fprintf(stderr, "vert_button_press: press, sb_area=%s, scroll_amount=%d, page=%d\n", sb_area_names[private->sb_area], scroll_amount, page);)
      /***************************************************************
      *  Scroll the window, or set up the slider.
      ***************************************************************/
      switch(private->sb_area)
      {
      case PTR_IN_ARROW_UP:
      case PTR_IN_ARROW_DOWN:
      case PTR_IN_EMPTY_TOP:
      case PTR_IN_EMPTY_BOTTOM:
         memset((char *)&fake_cursor_buff, 0, sizeof(fake_cursor_buff));
         fake_cursor_buff.current_win_buff = dspl_descr->main_pad;
         fake_cursor_buff.which_window     = MAIN_PAD;
         scroll_dmc.pv.next   = NULL; 
         scroll_dmc.pv.cmd    = (page ? DM_pp : DM_pv);
         scroll_dmc.pv.temp   = False;
         if (page)
            scroll_dmc.pp.amount = (float)scroll_amount * (float)0.99;
         else
            scroll_dmc.pv.chars  = scroll_amount;

         redraw = dm_pv(&fake_cursor_buff, &scroll_dmc, &junk);
         if (!m2_button_press)
            if (dspl_descr->sb_data->button_push.xany.window == 0) /* first call on button push */
               {
                  dspl_descr->sb_data->sb_microseconds = 500 * 1000;
                  memcpy((char *)&dspl_descr->sb_data->button_push, (char *)event, sizeof(XEvent));
               }
            else
               if (page)
                  dspl_descr->sb_data->sb_microseconds = 250 * 1000;
               else
                  dspl_descr->sb_data->sb_microseconds = 100 * 1000;
         break;

      case PTR_IN_VERT_SLIDER:
         private->slider_last_pos = event->xbutton.y;
         private->slider_pointer_offset = event->xbutton.y - private->vert_slider_top;
         dspl_descr->sb_data->sb_microseconds = 0;
         break;

      default:
         DEBUG(fprintf(stderr, "vert_button_press: sb_area = %d???\n", private->sb_area);;)
         break;
      } /* end of switch on sb_area */

   }
else
   if (event->type == ButtonRelease)
      {
         /***************************************************************
         *  For a button release, turn everything off.
         ***************************************************************/
         DEBUG23(fprintf(stderr, "vert_button_press: release\n");)
         dspl_descr->sb_data->sb_microseconds = 0;
         memset((char *)&dspl_descr->sb_data->button_push, 0, sizeof(XEvent));
         private->sb_area = BUTTON_NOT_DOWN;
         private->slider_delta_tweek = 0;
         redraw = MAIN_PAD_MASK & SCROLL_REDRAW; /* zero length scroll gets the sb windows redrawn */
         /* possibly clear the timeout if nothing else was going on */
         timeout_set(dspl_descr); /* in timeout.c */
      }
   else
      DEBUG(fprintf(stderr, "Unexpected event type %d, passed to vert_button_press\n", event->type);)

return(redraw);

} /* end of vert_button_press */


/************************************************************************

NAME:   horiz_button_press     - Process a button press in the horizontal sb window

PURPOSE:    This routine is called when a button press or release is performed
            in the scroll bar window.

PARAMETERS:
   1.  event            - pointer to XEvent (INPUT)
                          This is the event to be processed.
                          This is either a ButtonPress or ButtonRelease

   2.  dspl_descr       - pointer to DISPLAY_DESCR (INPUT)
                          This is the current display description.  It is
                          the one we are displaying to.  It is needed to 
                          redraw the main pad and the scroll bar.

FUNCTIONS :
   1.   Determine if the button press occured in the left arrow, right arrow
        slider, left gutter area, or right gutter area.

   2.   For the top or bottom arrows scroll the screen character left or right.
        Copy the XEvent to the sb_data area and set the timer for 
        multiClickTime milliseconds.

   3.   If the button press is in the area between the slider and the
        scroll bar, scroll approximately 1/4 of the screen left or right.
        Copy the XEvent to the sb_data area and set the timer for 
        multiClickTime milliseconds.

   4.   If the button press is on the slider, save the current cursor
        location and do not set the timer.  Delta's in motion events
        will be calculated in vert_sb_motion.

   5.   If this is a button release, clear the timer flag.

RETURNED VALUE:
   redraw -  If the screen needs to be redrawn, the mask of which screen
             and what type of redrawing is returned.

*************************************************************************/

int  horiz_button_press(XEvent           *event,
                        DISPLAY_DESCR    *dspl_descr)
{
SB_PRIVATE           *private = (SB_PRIVATE *)dspl_descr->sb_data->sb_private;
int                   redraw = 0;
int                   scroll_amount = 0;
int                   accelerated_scroll_amount = 0;
int                   new_first;
int                   m2_button_press = False;
DMC                   scroll_dmc;
BUFF_DESCR            fake_cursor_buff;

if (!private)  /* I don't trust anyone, but this should never be NULL */
   return(0);

if (!private->drag_out_of_window_scrolling)
if ((dspl_descr->sb_data->sb_mode == SCROLL_BAR_OFF) ||
    (private->sb_disabled) ||
    !dspl_descr->sb_data->horiz_window_on)
   return(0);


/***************************************************************
*  If this is a button press, figure out where in the window it is.
***************************************************************/
if (event->type == ButtonPress)
   {
      if (event->xbutton.x < ARROW_WIDTH)
         {
            private->sb_area = PTR_IN_ARROW_LEFT; 
            DEBUG5(fprintf(stderr, "Scroll left 1 char\n");)
            scroll_amount = -1;
         }
      else
         if (event->xbutton.x >= ((int)private->horiz_window_width-ARROW_WIDTH))
            {
               private->sb_area = PTR_IN_ARROW_RIGHT; 
               DEBUG5(fprintf(stderr, "Right left 1 char\n");)
               scroll_amount = 1;
            }
         else
            if (event->xbutton.x < private->horiz_slider_left)
               {
                  private->sb_area = PTR_IN_EMPTY_LEFT; 
                  scroll_amount = ((dspl_descr->main_pad->window->sub_width/2)/dspl_descr->main_pad->window->font->max_bounds.width);
                  DEBUG5(fprintf(stderr, "Scroll left %d chars\n", scroll_amount);)
                  scroll_amount = -scroll_amount;
               }
            else
               if (event->xbutton.x >= (private->horiz_slider_left+private->horiz_slider_width))
                  {
                     private->sb_area = PTR_IN_EMPTY_RIGHT; 
                     scroll_amount = ((dspl_descr->main_pad->window->sub_width/2)/dspl_descr->main_pad->window->font->max_bounds.width);
                     DEBUG5(fprintf(stderr, "Scroll right %d chars\n", scroll_amount);)
                  }
               else
                  private->sb_area = PTR_IN_HORIZ_SLIDER; 

      DEBUG23(
         fprintf(stderr, "horiz_button_press: press, sb_area=%s, scroll_amount=%d, %s\n",
                 sb_area_names[private->sb_area], scroll_amount,
                 (dspl_descr->sb_data->button_push.xany.window == 0) ? "First Call" : "Repeat Call");
      )

      /***************************************************************
      *  For mouse button 2, move the slider so the hot spot is on
      *  the mouse position.  The calculation reads:  Take the ratio
      *  of the mouse position in the scroll bar window to the
      *  width of the scroll bar window and apply it to the number
      *  of characters we are scrolling over.
      ***************************************************************/
      if ((event->xbutton.button == 2) && ((private->sb_area == PTR_IN_EMPTY_LEFT) || (private->sb_area == PTR_IN_EMPTY_RIGHT)))
         {
            new_first = ((event->xbutton.x - ARROW_WIDTH) * (MAX(private->horiz_max_chars, private->horiz_base_chars))) /
                         (private->horiz_window_width-(2*ARROW_WIDTH));
            if (new_first < 0)
               new_first = 0;
            scroll_amount  = new_first - dspl_descr->main_pad->first_char;
            m2_button_press = True;
         }
      else
         if ((event->xbutton.y < 0) && ((private->sb_area == PTR_IN_ARROW_LEFT) || (private->sb_area == PTR_IN_ARROW_RIGHT)))
            {
               /* slid up on holding the button down */
               accelerated_scroll_amount = scroll_amount * (-event->xbutton.y / 16);
               if (ABSVALUE(accelerated_scroll_amount) > ABSVALUE(scroll_amount))
                  scroll_amount = accelerated_scroll_amount;
            }


      /***************************************************************
      *  Scroll the window, or set up the slider.
      ***************************************************************/
      switch(private->sb_area)
      {
      case PTR_IN_ARROW_LEFT:
      case PTR_IN_ARROW_RIGHT:
      case PTR_IN_EMPTY_LEFT:
      case PTR_IN_EMPTY_RIGHT:
         memset((char *)&fake_cursor_buff, 0, sizeof(BUFF_DESCR));
         fake_cursor_buff.current_win_buff = dspl_descr->main_pad;
         fake_cursor_buff.which_window     = MAIN_PAD;
         scroll_dmc.ph.next   = NULL; 
         scroll_dmc.ph.cmd    = DM_ph;
         scroll_dmc.ph.temp   = False;
         scroll_dmc.ph.chars  = scroll_amount;
         redraw = dm_ph(&fake_cursor_buff, &scroll_dmc);
         if (!m2_button_press)
            if (dspl_descr->sb_data->button_push.xany.window == 0) /* first call on button push */
               {
                  dspl_descr->sb_data->sb_microseconds = 500 * 1000;
                  memcpy((char *)&dspl_descr->sb_data->button_push, (char *)event, sizeof(XEvent));
               }
            else
               dspl_descr->sb_data->sb_microseconds = 200 * 1000;
         break;

      case PTR_IN_HORIZ_SLIDER:
         private->slider_last_pos = event->xbutton.x;
         private->slider_pointer_offset = event->xbutton.x - private->horiz_slider_left;
         dspl_descr->sb_data->sb_microseconds = 0;
         break;

      default:
         DEBUG(fprintf(stderr, "horiz_button_press: sb_area = %d???\n", private->sb_area);;)
         break;
      } /* end of switch on sb_area */

   }
else
   if (event->type == ButtonRelease)
      {
         /***************************************************************
         *  For a button release, turn everything off.
         ***************************************************************/
         dspl_descr->sb_data->sb_microseconds = 0;
         memset((char *)&dspl_descr->sb_data->button_push, 0, sizeof(XEvent));
         private->sb_area = BUTTON_NOT_DOWN;
         private->slider_delta_tweek = 0;
         redraw = MAIN_PAD_MASK & SCROLL_REDRAW; /* zero length scroll gets the sb windows redrawn */
         DEBUG23(fprintf(stderr, "horiz_button_press: release, sb_area=%s\n", sb_area_names[private->sb_area]);)
         /* possibly clear the timeout if nothing else was going on */
         timeout_set(dspl_descr); /* in timeout.c */
      }
   else
      DEBUG(fprintf(stderr, "Unexpected event type %d, passed to horiz_button_press", event->type);)

return(redraw);

} /* end of horiz_button_press */


/************************************************************************

NAME:   move_vert_slider      - Move vertical slider because of positioning

PURPOSE:    This routine is called for motion events when the mouse button
            is down in the scroll bar window.

PARAMETERS:
   1.  event            - pointer to XEvent (INPUT)
                          This is the event to be processed.
                          This is a MotionNotify

   2.  dspl_descr       - pointer to DISPLAY_DESCR (INPUT)
                          This is the current display description.  It is
                          the one we are displaying to.  It is needed to 
                          reposition the main pad and the scroll bar.

FUNCTIONS :
   1.   Make sure the active sb_area is the slider.  Motion events show up
        for the arrows and the gutter portion of the window, but are not processed.

   2.   Calculate the delta in vertical motion since the last positioning
        and add that to the hot spot in the slider.  Recalculate the new
        hot spot location and use that to calculate the new line number.

   3.   If the line number changed, scroll the window accordingly.

RETURNED VALUE:
   redraw -  If the screen needs to be redrawn, the mask of which screen
             and what type of redrawing is returned.

*************************************************************************/

int  move_vert_slider(XEvent           *event,
                      DISPLAY_DESCR    *dspl_descr)
{
SB_PRIVATE           *private = (SB_PRIVATE *)dspl_descr->sb_data->sb_private;
int                   redraw = 0;
int                   usable_pixels;
int                   new_first;
int                   delta;
int                   delta_lines;
int                   junk;
DMC                   scroll_dmc;
BUFF_DESCR            fake_cursor_buff;

if (!private ||
    (private->sb_area == BUTTON_NOT_DOWN) ||
    (dspl_descr->sb_data->sb_mode == SCROLL_BAR_OFF) ||
    !dspl_descr->sb_data->vert_window_on ||
    (event->type != MotionNotify) ||
    (private->sb_disabled) ||
    (event->xmotion.window != dspl_descr->sb_data->vert_window))
   return(0);

if (private->sb_area != PTR_IN_VERT_SLIDER)
   {  /* kevin code, accellerator */
      dspl_descr->sb_data->button_push.xbutton.x = event->xmotion.x;
      return(0);
   }

/***************************************************************
*  First figure out how far we moved.
*  Tweek the pointer movement to keep the pointer at the same
*  offset into the slider.
***************************************************************/
delta = event->xmotion.y - private->slider_last_pos;
if (private->slider_delta_tweek)
   {
      delta += private->slider_delta_tweek;
      private->slider_delta_tweek = 0;
   }

if (delta != 0)
   {
      usable_pixels = private->vert_window_height-((2*ARROW_HEIGHT)+2);  /* +2-> 1 each for top and bottom shadow */
      delta_lines =  (int)(((float)(total_lines(dspl_descr->main_pad->token)) * (float)delta) / (float)usable_pixels);
      DEBUG23(fprintf(stderr, "@move_vert_slider: delta_pix=%d, usable_pix=%d, delta_lines=%d, total_lines=%d, ", delta, usable_pixels, delta_lines, total_lines(dspl_descr->main_pad->token));)
      new_first = dspl_descr->main_pad->first_line + delta_lines; /* delta could be negative */
      if (new_first < 0)
         new_first = 0;
      else
         if (new_first >= total_lines(dspl_descr->main_pad->token))
            new_first = total_lines(dspl_descr->main_pad->token) - 1;
      delta_lines =  new_first - dspl_descr->main_pad->first_line;
      DEBUG23(fprintf(stderr, "new_first=%d, adj_delta_lines=%d\n", new_first, delta_lines);)
      if (delta_lines != 0)
         {
            memset((char *)&fake_cursor_buff, 0, sizeof(fake_cursor_buff));
            fake_cursor_buff.current_win_buff = dspl_descr->main_pad;
            fake_cursor_buff.which_window     = MAIN_PAD;
            scroll_dmc.pv.next   = NULL; 
            scroll_dmc.pv.cmd    = DM_pv;
            scroll_dmc.pv.temp   = False;
            scroll_dmc.pv.chars  = delta_lines;
            redraw = dm_pv(&fake_cursor_buff, &scroll_dmc, &junk);
            private->slider_last_pos = event->xmotion.y;
         }
   }
else
   DEBUG23(fprintf(stderr, "@move_vert_slider: delta_pix=%d\n", delta);)

return(redraw);

} /* end of move_vert_slider */


/************************************************************************

NAME:   move_horiz_slider      - Move horizontal slider because of positioning

PURPOSE:    This routine is called for motion events when the mouse button
            is down in the scroll bar window.

PARAMETERS:
   1.  event            - pointer to XEvent (INPUT)
                          This is the event to be processed.
                          This is a MotionNotify

   2.  dspl_descr       - pointer to DISPLAY_DESCR (INPUT)
                          This is the current display description.  It is
                          the one we are displaying to.  It is needed to 
                          reposition the main pad and the scroll bar.

FUNCTIONS :
   1.   Make sure the active sb_area is the slider.  Motion events show up
        for the arrows and the gutter portion of the window, but are not processed.

   2.   Calculate the delta in vertical motion since the last positioning
        and add that to the hot spot in the slider.  Recalculate the new
        hot spot location and use that to calculate the new line number.

   3.   If the line number changed, scroll the window accordingly.

RETURNED VALUE:
   redraw -  If the screen needs to be redrawn, the mask of which screen
             and what type of redrawing is returned.

*************************************************************************/

int  move_horiz_slider(XEvent           *event,
                       DISPLAY_DESCR    *dspl_descr)
{
SB_PRIVATE           *private = (SB_PRIVATE *)dspl_descr->sb_data->sb_private;
int                   redraw = 0;
int                   delta;
int                   usable_pixels;
int                   delta_chars;
DMC                   scroll_dmc;
BUFF_DESCR            fake_cursor_buff;
int                   working_chars;

if (!private ||
    (private->sb_area == BUTTON_NOT_DOWN) ||
    /*(private->sb_area != PTR_IN_HORIZ_SLIDER) ||*/
    (dspl_descr->sb_data->sb_mode == SCROLL_BAR_OFF) ||
    !dspl_descr->sb_data->horiz_window_on ||
    (event->type != MotionNotify) ||
    (private->sb_disabled) ||
    (event->xmotion.window != dspl_descr->sb_data->horiz_window))
   return(0);

if (private->sb_area != PTR_IN_HORIZ_SLIDER)
   {  /* kevin code, accellerator */
      dspl_descr->sb_data->button_push.xbutton.y = event->xmotion.y;
      return(0);
   }

/***************************************************************
*  First figure out how far we moved.
*  Tweek the pointer movement to keep the pointer at the same
*  offset into the slider.
***************************************************************/
delta = event->xmotion.x - private->slider_last_pos;
if (private->slider_delta_tweek)
   {
      delta += private->slider_delta_tweek;
      private->slider_delta_tweek = 0;
   }

if (delta != 0)
   {
      usable_pixels = private->horiz_window_width-((2*ARROW_HEIGHT)+2);  /* +2-> 1 each for left and right shadows */
      working_chars = MAX((unsigned)private->horiz_max_chars, (dspl_descr->main_pad->window->sub_width / dspl_descr->main_pad->window->font->min_bounds.width));
      delta_chars =  (int)(((float)working_chars * (float)delta) / (float)usable_pixels);
      DEBUG23(fprintf(stderr, "@move_horiz_slider: delta_pix=%d, usable_pix=%d, delta_chars=%d, working_chars=%d, ", delta, usable_pixels, delta_chars, working_chars);)
      if (delta_chars != 0)
         {
            memset((char *)&fake_cursor_buff, 0, sizeof(fake_cursor_buff));
            fake_cursor_buff.current_win_buff = dspl_descr->main_pad;
            fake_cursor_buff.which_window     = MAIN_PAD;
            scroll_dmc.ph.next   = NULL; 
            scroll_dmc.ph.cmd    = DM_pv;
            scroll_dmc.ph.temp   = False;
            scroll_dmc.ph.chars  = delta_chars;
            redraw = dm_ph(&fake_cursor_buff, &scroll_dmc);
            private->slider_last_pos = event->xmotion.x;
         }
   }
else
   DEBUG23(fprintf(stderr, "@move_horiz_slider: delta_pix=%d\n", delta);)

return(redraw);

} /* end of move_horiz_slider */


/************************************************************************

NAME:      check_private_data - get the sb private data

PURPOSE:    This routine returns the existing dmwin private area
            from the display description or initializes a new one.

PARAMETERS:
   1.  sb_win  -  pointer to SCROLLBAR_DESCR (in drawable.h)
                  This is where the private data for this window set
                  is hung.

FUNCTIONS :

   1.   If the private area has already been allocated, return it.

   2.   Malloc a new area.  If the malloc fails, return NULL

   3.   Initialize the new private area and return it.

RETURNED VALUE
   private  -  pointer to SB_PRIVATE
               This is the private static area used by the
               routines in this module.
               NULL is returned on malloc failure only.

*************************************************************************/

static SB_PRIVATE *check_private_data(SCROLLBAR_DESCR  *sb_win)
{
SB_PRIVATE    *private;
int sbp_size;

if (sb_win->sb_private)
   return((SB_PRIVATE *)sb_win->sb_private);

sbp_size = sizeof(SB_PRIVATE);
private = (SB_PRIVATE *)CE_MALLOC(sbp_size);
if (!private)
   return(NULL);   
else
   {
      memset((char *)private, 0, sbp_size);
      private->vert_last_firstline = -1;
      private->horiz_last_firstchar = -1;
   }

sb_win->sb_private = (void *)private;

return(private);

} /* end of check_private_data */



/************************************************************************

NAME:  build_in_arrow_press  - Build a button press event which appears to be in one of the 4 arrows


PURPOSE:    This routine is called when a mouse button tied to highlighting
            is pressed and the cursor drug out of the window to start scrolling.

PARAMETERS:
   1.  event            - pointer to XEvent (INPUT)
                          This is the event to be processed.
                          This is always a LeaveNotify

   2.  dspl_descr       - pointer to DISPLAY_DESCR (INPUT)
                          This is the current display description.  It is
                          the one we are displaying to.  It is passed on to other
                          routines.

FUNCTIONS :
   1.   Build the guts of a buttonpress.

   2.   Determine if the cursor left the main window from the top, bottom, left
        side or right side and set up the button press appropriately.

   3.   Send the button press to the button press handler routine in this module.

RETURNED VALUE:
   redraw -  If the screen needs to be redrawn, the mask of which screen
             and what type of redrawing is returned.


*************************************************************************/

int  build_in_arrow_press(XEvent           *event,
                          DISPLAY_DESCR    *dspl_descr)
{
SB_PRIVATE           *private = (SB_PRIVATE *)dspl_descr->sb_data->sb_private;
int                   redraw = 0;
XButtonEvent          button_event;


if (!private)
   {
      get_sb_windows(dspl_descr->main_pad,
                     dspl_descr->display,
                     dspl_descr->sb_data,
                     SBCOLORS,
                     SBWIDTH);
      private = (SB_PRIVATE *)dspl_descr->sb_data->sb_private;
   }

if (!private)
   return(0);

private->drag_out_of_window_scrolling = True;

/***************************************************************
*  Set up the parts of the button event we can do blindly.
***************************************************************/
memset(&button_event, 0, sizeof(button_event));
button_event.type           = ButtonPress;
button_event.serial         = event->xcrossing.serial;
button_event.send_event     = event->xcrossing.send_event;
button_event.display        = event->xcrossing.display;
/* button_event.window    later */
button_event.root            = event->xcrossing.root;
button_event.subwindow       = event->xcrossing.subwindow;
button_event.time            = event->xcrossing.time;
/*button_event. x, y  later */
button_event.x_root          = 0; /* not used in this module */
button_event.y_root          = 0; /* not used in this module */
button_event.state           = event->xcrossing.state;
button_event.button          = 1;
button_event.same_screen     = event->xcrossing.same_screen;

/***************************************************************
*  Figure out which border is being crossed. 
*  On vertical scrolling, the X value can be used to control the
*  speed of scrolling.  Search for variable accelerated_scroll_amount
*  which is in routine vert_button_press.  If you are holding the
*  mouse down in the arrow up or down and slide the mouse to the
*  left, the speed of scrolling increases.  We are making use
*  of this to pick the scrolling speed.
***************************************************************/
if (event->xcrossing.y <= 0)
   {
      button_event.x = -48; /* 48 / 16 = scroll 3 lines at a time */
      button_event.y = ARROW_HEIGHT - 1;
      button_event.window = dspl_descr->sb_data->vert_window;
      redraw = vert_button_press((XEvent *)&button_event,dspl_descr);
   }
else
   if (event->xcrossing.y >= dspl_descr->main_pad->window->height)
      {
         button_event.x = -48; /* 48 / 16 = scroll 3 lines at a time */
         /*button_event.x = 1*/
         button_event.y = (int)(private->vert_window_height-ARROW_HEIGHT);
         button_event.window = dspl_descr->sb_data->vert_window;
         redraw = vert_button_press((XEvent *)&button_event,dspl_descr);
      }
   else
      if (event->xcrossing.x <= 0)
         {
            button_event.x = ARROW_WIDTH - 1;
            button_event.y = 1;
            button_event.window = dspl_descr->sb_data->horiz_window;
            redraw = horiz_button_press((XEvent *)&button_event,dspl_descr);
         }
      else
         if (event->xcrossing.x >= dspl_descr->main_pad->window->width)
            {
               button_event.x = (int)(private->horiz_window_width-ARROW_WIDTH);
               button_event.y = 1;
               button_event.window = dspl_descr->sb_data->horiz_window;
               redraw = horiz_button_press((XEvent *)&button_event,dspl_descr);
            }
         else
            {
               DEBUG(fprintf(stderr, "Thought we had a button press window crossing, could not decide which window, crossing at (%d,%d)\n",
                                     event->xcrossing.x, event->xcrossing.y);
               )
               private->drag_out_of_window_scrolling = False;
               return(0);
            }

return(redraw);

} /*end of build_in_arrow_press */



/************************************************************************

NAME:  build_in_arrow_rlse  - Build a button release to turn off scrolling from build_in_arrow_press


PURPOSE:    This routine is called when a focus is lost and 
            "drag out of the window with the mouse (highlighting) down" is set as seen
            in dspl_descr drag_vert_scrolling and drag_horiz_scrolling

PARAMETERS:
   1.  dspl_descr       - pointer to DISPLAY_DESCR (INPUT)
                          This is the current display description.  It is
                          the one we are displaying to.  It is passed on to other
                          routines and used to extract a button event.

FUNCTIONS :
   1.   Convert the scrollbar data buttonpress to a button release.

   2.   Call the release button routine for vertical.  Both the horizontal and
        vertical do the same thing on button release.

RETURNED VALUE:
   redraw -  If the screen needs to be redrawn, the mask of which screen
             and what type of redrawing is returned.


*************************************************************************/

int  build_in_arrow_rlse(DISPLAY_DESCR    *dspl_descr)
{
SB_PRIVATE           *private = (SB_PRIVATE *)dspl_descr->sb_data->sb_private;
int                   redraw = 0;
XButtonEvent          button_event;

if (!private)
   return(0);

/***************************************************************
*  Set up the buttonrelease event.
***************************************************************/
memcpy(&button_event, &dspl_descr->pd_data->button_push, sizeof(button_event));
button_event.type           = ButtonRelease;
redraw = vert_button_press((XEvent *)&button_event,dspl_descr);

private->drag_out_of_window_scrolling = False;

return(redraw);

} /* end of build_in_arrow_rlse */


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
*  module pd.c   -  Pull Downs
*
*  Routines:
*         get_menubar_window     - Create the menubar X window
*         resize_menu_bar        - Resize the menu bar
*         dm_mi                  - Menu Item command (builds menus)
*         dm_pd                  - Pull Down command (initiates a pulldown)
*         dm_pdm                 - Toggle the menu bar on and off
*         pd_remove              - Get rid of one or more pulldowns.
*         pd_window_to_idx       - Find pulldown entry given the X window id.
*         change_pd_font         - Change the font in the pulldown to match the main window.
*         draw_menu_bar          - draw the top menu area
*         draw_pulldown          - draw a pulldown window
*         pd_adjust_motion       - Adjust motion events when pulldown is active
*         pd_button              - Process a button press or release when pulldowns are active
*         pd_button_motion       - Process button motion events when pulldowns are active
*         check_menu_items       - Make sure certain menu items exist
*         is_menu_item_key       - Test is a keykey is a menu item keykey
*         dm_tmb                 - To menu bar
*
*  Internal:
*         get_pulldown_colors    - Set up colors for the pulldowns.
*         next_avail_mi_num      - Find unused Menu item in a pulldown
*         merge_duplicate_titles - Make sure same title is not in pulldown twice
*         get_pd_window          - Create a new pulldown window
*         get_loc                - Extract location info from an X event.
*         get_private_data       - Get the pulldow private data
*         highlight_line         - Highlight or Un-Highlight a Pulldown line.
*         get_menu_cmds          - Retrieve menu item commands from hash table
*         pd_in_list             - Check for pulldown command in list of DMC's
*         sched_button_push      - Schedule a delayed button push to pop up dependant pulldowns.
* 
* 
* Notes for pulldowns.
* 
* new cmd  mi (menu item) used to build the pulldown
* mi <pd>/<num> "title";cmds  ke
* <pd> is the pull down name (predefined names a,b,c, & d)
* <num> is where in the list it goes, starts at zero.
* "title" is what goes in the window
* new cmd pd (pull down) activates a pull down
* pd <pd>
* pdm -off / -on
* 
* pulldowns a-d predefined in .Cekeys.
* also as defaults in programs
* a -> File
* b -> Edit
* c -> macros
* d -> help
* These are fixed names.
* 
* Top 4 pulldowns in titlebar(menu_bar) area.  Titlebar gets larger in menu bar mode=on.
* 
* titlebar.c detects menu bar on and calls menubar drawing routine.
*            menu bar goes <above?/below?> above existing line.
*            resize detects and calculates in the size.
* txcursor.c motion events detect mouse in menubar area and does not translate
*            to a valid character position if the menu bar is active.  
*            In this case, pulldown mode is turned on so a button press
*            fires up a pulldown.
* 
* crpad.c   When button press or keypress occur, detect non-zero pulldown flag
*           and call pulldown routine.  Unused space is value -1 and gets
*           ignored.
*           Expose of a pulldown window calls pull down drawing routine.
*           In pulldown mode (eg pulldown active or cursor in menubar),
*           motion events are prefiltered by pd_adjust_motion.  If the
*           button is not, down, no change is made.
* 
* 
* resizing:
*    If you get to small start dropping boxes from the right.
*    Don't bother with boxes, do it like netscape
* 
* button press.
*    Button press in menu area clears any existing pulldowns and
*    executes pulldown command to create a pulldown window.
*    button release in menu area keeps pulldown in place.
*    Outside of pulldown closes out pulldown.
*    If button press is in a pulldown, run the command string.
* 
* button up.
*    If in menubar or pulldown window which cascades (executes a pulldown)
*    lock in the displayed pulldowns.
*    If in pulldown window which does not cascade, run the function.
* 
* button motion. do translation first.  If in a menu item, highlight the
* menu item.  If it has a pulldown on it, schedule a callback just
* like scroll bars.  We will send ourselves
* a "sent" motion event for the current location.  That will trigger expanding
* The pulldown.  This occurs only if there is a single pulldown 
* command after the es.
* 
* make the scroll bar window pixels visible.  Use them for both scroll bars
* and pull downs.  The allocation of the colors becomes a separate routine.
* Will have to pass them in to the drawing routines.
*  
* 
* +----- OLD
* |  Top 4 pulldowns in titlebar area.  Titlebar gets larger in pull down mode=on.
* |  
* |  titlebar.c detects pull downs on and calls pull down drawing routine.
* |             pull downs go above existing line.
* |             resize detects and calculates in the size.
* |  txcursor.c motion events detect mouse in pulldown area and does not translate
* |             to a valid character.  Instead calc what pulldown it is in
* |             and set pulldown flag.  (done in pulldown.c)
* |  when button press or keypress occur, detect non-zero pulldown flag
* |  and call pulldown routine.  Unused space is value -1 and gets
* |  ignored.
* |  
* |  resizing:
* |     If you get to small start dropping boxes from the right.
* |     Don't bother with boxes, do it like netscape
* |  
* |  button press.
* |     Button press in pulldown clears any existing pulldowns and
* |     executes pulldown command to create a pulldown window.
* |     button release in pulldown keeps pulldown in place.
* +-------
* 
* 
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h         */
#include <string.h>         /* /usr/include/string.h        */
#include <errno.h>          /* /usr/include/errno.h         */

#ifdef WIN32
#ifdef WIN32_XNT
#include "xnt.h"
#else
#include "X11/X.h"          /* /usr/include/X11/X.h      */
#include "X11/Xlib.h"       /* /usr/include/X11/Xlib.h   */
#include "X11/Xutil.h"      /* /usr/include/X11/Xutil.h  */
#include "X11/cursorfont.h" /* /usr/include/X11/cursorfont.h */
#endif
#else
#include <X11/X.h>          /* /usr/include/X11/X.h      */
#include <X11/Xlib.h>       /* /usr/include/X11/Xlib.h   */
#include <X11/Xutil.h>      /* /usr/include/X11/Xutil.h  */
#include <X11/cursorfont.h> /* /usr/include/X11/cursorfont.h */
#endif


#define MAX_PD_NAME 16
#define MI_HASH_TYPE   0x6044
#define MI_KEY_FORMAT  "%-16.16s%04d%04X"
#define MI_READ_FORMAT "%16s%4d%4X"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "borders.h"
#include "buffer.h"
#include "dmsyms.h"
#include "dmwin.h"
#include "debug.h"
#include "emalloc.h"
#include "gc.h"
#include "hsearch.h"
#include "kd.h" /* needed for KEY_KEY_SIZE */
#include "mark.h"
#include "parms.h"
#include "parsedm.h"
#include "pd.h"
#include "prompt.h"
#include "redraw.h" /* needed for REDRAW_FORCE_REAL_WARP */
#include "timeout.h"
#include "window.h"
#include "xerrorpos.h"
#include "xutil.h"

#ifndef HAVE_STRLCPY
#include "strl.h"
#endif

#if !defined(FREEBSD) && !defined(WIN32) && !defined(_OSF_SOURCE) && !defined(OMVS)
char  *getenv(char *name);
#endif
#define   MENUBAR_HEIGHT_PADDING   40
#define   PD_SHADOW_LINE_WIDTH      1
#define   ARROW_WIDTH 15
#define   MENUBAR_PULLDOWN_NAME    "Menubar"

/***************************************************************
*
*  This is the list of colors used for drawing the menus.
*
***************************************************************/

#define   BAR_COLORS   4

/* [0] = "gray59" "#979797" */
/* [1] = "gray87" "#dddddd" */    
/* [3] = "gray37" "#5e5e5e" */
/* [4] = "black"  "#000000" */

static char   *bar_colors[BAR_COLORS] = {
              "#979797",
              "#E0E0E0",
              "#5e5e5e",
              "black" };

static char   *bw_colors[BAR_COLORS] = {
              "black",
              "white",
              "white",
              "white" };


/***************************************************************
*  
*  Static data hung off the display description.
*
*  The following structure is hung off the pd_data->pd_private field
*  in the display description.
*  
*  Field Notes:
*  current_cursor_w_idx - Current pulldown nest level where the
*                         cursor is.  -1 for outside any pulldown
*                         or in menu bar area.
*  current_cursor_menu_item - Current line in the pulldown or
*                             current item in the menu bar.
*                             -1 for no highlighting.  
***************************************************************/

#define MAX_PD_WINS 10
#define MAX_BAR_ITEMS 10

typedef struct {
   Window             pd_win;
   char               pd_name[MAX_PD_NAME+1];
   DRAWABLE_DESCR     win_shape;                /* the gc's in this structure are not used */
   int                lines_in_pulldown;

} PULLDOWN;

typedef struct {
   PULLDOWN           pd[MAX_PD_WINS];
   int                button_window;
   int                win_nest_level;             /* next window id to use, zero means no popups up. */
   Window             main_window;
   int                bar_l_offset[MAX_BAR_ITEMS];
   int                bar_r_offset[MAX_BAR_ITEMS];
   int                current_bar_items;          /* current number of menubar items showing    */
   int                help_bar_pos;               /* index of the right justified help pulldown */
   unsigned long      pixels[BAR_COLORS];
   GC                 bg_gc;
   GC                 darkline_gc;
   GC                 lightline_gc;
   GC                 text_gc;
   int                current_cursor_w_idx;
   int                current_cursor_menu_item;
   Cursor             pointer_cursor;

} PD_PRIVATE;


/***************************************************************
*  Constant offsets into bar_l_offset and bar_r_offset
***************************************************************/
#define BAR_ITEM_FILE   0
#define BAR_ITEM_EDIT   1
#define BAR_ITEM_MACRO  2
#define BAR_ITEM_MODES  3
#define BAR_ITEM_HELP   4



/***************************************************************
*  
*  Constants for the last argument to get_menu_cmds
*  MENU_ITEM_TEXT    -  Return pointer to the leading 'es' dmc
*  MENU_ITEM_CMDS    -  Return pointer to item past leading 'es' dmc
*  MENU_ITEM_PD_ONLY -  Return pointer to the pulldown immediately after
*                       the leading 'es' dmc.  If not a pull down,       
*                       return NULL
*  
***************************************************************/
#define MENU_ITEM_TEXT    0
#define MENU_ITEM_CMDS    1
#define MENU_ITEM_PD_ONLY 2

/***************************************************************
*  
*  Local Prototypes
*  
***************************************************************/

static void  get_pulldown_colors(Display          *display,             /* input  */
                                 Window            main_x_window,       /* input  */
                                 XFontStruct      *font,                /* input  */
                                 char             *pd_colors,           /* input  */
                                 PD_DATA          *pd_data);             /* input / output */

static int next_avail_mi_num(void    *hsearch_data,
                             char    *pd_name);

static int merge_duplicate_titles(void    *hsearch_data,
                                  char    *pd_name,
                                  char    *title_string,
                                  int      current_mi_num);

static void empty_pulldown(DISPLAY_DESCR    *dspl_descr,
                           char             *pd_name,
                           int               cmd_id);

static void  get_loc(XEvent            *event,
                     int                root,
                     int               *x,
                     int               *y,
                     Window            *w);

static void get_pd_window(DISPLAY_DESCR     *dspl_descr,
                          int                x,
                          int                y,
                          unsigned int       width,
                          int                lines);

static PD_PRIVATE *get_private_data(PD_DATA  *pd_win);

static void  highlight_line(int                 nest_level,    /* input  */
                            int                 lineno,        /* input  */
                            int                 highlight_on,  /* input  */
                            PD_PRIVATE         *private,       /* input / output */
                            Display            *display);      /* input  */


static DMC *get_menu_cmds(void                 *hsearch_data,
                          char                 *pd_name,
                          int                   menu_item,
                          int                   type);

static int pd_in_list(DMC   *dmc);


static void  sched_button_push(XEvent             *event,
                               PD_DATA           *pd_data);            /* input / output */


/************************************************************************

NAME:      get_menubar_window  - Create the menubar X window

PURPOSE:    This routine does the initial create of the window used for
            the menu bar.  Note that this does not map the window.  It
            is called if menu's are on by default or the first time the
            user switched on menu mode.

PARAMETERS:
   1.  display       -  pointer to Display (Xlib type) (INPUT)
                        This token is needed for the X calls.

   2.  pd_data       -  pointer to PD_DATA (INPUT/OUTPUT)
                        This is all the public and private data for the menu bar.

   3.  mousecursor   -  pointer to char (INPUT)
                        This is argement to Ce from which we can override the
                        cursor used in the menubar window.

   4.  main_window    - pointer to DRAWABLE_DESCR (INPUT)
                        This is a pointer to the main window drawable description.
                        The menubar window, will be its child.

   5.  main_x_window -  Window (Xlib type) (INPUT)
                        This is the window id for the main ce window.  It becomes
                        the parent of the menubar window.


FUNCTIONS :

   1.   Set up the mouse cursor for the menu bar.

   2.   Estimate the size of the menubar window.

   3.   Create the menubar window.

   4.   Set the event mask for this window.


*************************************************************************/

void  get_menubar_window(Display          *display,
                         PD_DATA          *pd_data,
                         char             *mousecursor,
                         DRAWABLE_DESCR   *main_window,
                         Window            main_x_window)
{
PD_PRIVATE           *private;
int                   window_height;
XSetWindowAttributes  window_attrs;
unsigned long         window_mask;
XGCValues             gcvalues;
unsigned long         valuemask;
unsigned long         event_mask;
unsigned int          main_cursor = 0;
unsigned int          pulldown_cursor = 0;

/***************************************************************
*  Access the private data.
***************************************************************/
private = get_private_data(pd_data);
if (!private) 
   return; /* message about out of memory already output */

/***************************************************************
*  Make sure we're not called twice.
***************************************************************/
if (pd_data->menubar_x_window != 0)
   return;

/***************************************************************
*  Get the mouse cursor for the menu bar window.
***************************************************************/
if (private->pointer_cursor == 0)
   {
      if (mousecursor && *mousecursor)
         sscanf(mousecursor, "%d %d", &main_cursor, &pulldown_cursor);

      if (pulldown_cursor == 0)
         pulldown_cursor = XC_crosshair;

      DEBUG9(XERRORPOS)
      private->pointer_cursor = XCreateFontCursor(display, pulldown_cursor);
      if (private->pointer_cursor == 0)
         {
            DEBUG9(XERRORPOS)
            private->pointer_cursor = XCreateFontCursor(display, XC_crosshair);
         }
   }

/***************************************************************
*  Do the fixed stuff first:
***************************************************************/

memcpy((char *)&pd_data->menu_bar,  (char *)main_window, sizeof(DRAWABLE_DESCR));
pd_data->menu_bar.lines_on_screen  = 1;

/***************************************************************
*
*  GC's along with font data will be taken from the titlebar.
*  use the main window data as an estimate for sizeing.  The
*  resize is always called first, thus we can ignore the
*  GC's and font data.
*
*  Determine the height, width, X and Y of the window.
*  This is an initial estimate only.  The resize routine
*  does the final calculation.
*
***************************************************************/

window_height = main_window->font->ascent + main_window->font->descent;
window_height += (window_height * MENUBAR_HEIGHT_PADDING) / 100;

pd_data->menu_bar.height  = window_height;
pd_data->menu_bar.x       = 1;
pd_data->menu_bar.y       = 1;
pd_data->menu_bar.width   = main_window->width - 4;

pd_data->menu_bar.sub_height  = window_height;
pd_data->menu_bar.sub_x       = 2;
pd_data->menu_bar.sub_y       = 1;
pd_data->menu_bar.sub_width   = main_window->width - 4;

/***************************************************************
*  Get the pixel values from the main window gc.
***************************************************************/
valuemask = GCForeground
          | GCBackground;

DEBUG9(XERRORPOS)
if (!XGetGCValues(display,
                  main_window->gc,
                  valuemask,
                  &gcvalues))
   {
      DEBUG(   fprintf(stderr, "get_menubar_window:  XGetGCValues failed for gc = 0x%X\n", main_window->gc);)
      gcvalues.foreground = BlackPixel(display, DefaultScreen(display));
      gcvalues.background = WhitePixel(display, DefaultScreen(display));
   }

/***************************************************************
*
*  Create the menubar window.
*
***************************************************************/

window_attrs.background_pixel      = gcvalues.foreground;
window_attrs.border_pixel          = gcvalues.background;
window_attrs.win_gravity           = UnmapGravity;
window_attrs.do_not_propagate_mask = PointerMotionMask;
window_attrs.cursor                = private->pointer_cursor;


window_mask = CWBackPixel
            | CWBorderPixel
            | CWWinGravity
            | CWDontPropagate
            | CWCursor;

DEBUG9(XERRORPOS)
pd_data->menubar_x_window = XCreateWindow(
                                display,
                                main_x_window,                  /* parent */
                                pd_data->menu_bar.x,     pd_data->menu_bar.y,
                                pd_data->menu_bar.width, pd_data->menu_bar.height,
                                BORDER_WIDTH,
                                CopyFromParent,                 /*  depth        */
                                InputOutput,                    /*  class        */
                                CopyFromParent,                 /*  visual       */
                                window_mask,
                                &window_attrs
    );

/***************************************************************
*
*  Set the event mask and do not propagate mask.
*
***************************************************************/

if (pd_data->menubar_x_window != None)
   {
      event_mask = ExposureMask
                   | EnterWindowMask
                   | LeaveWindowMask
                   | KeyPressMask
                   | KeyReleaseMask
                   | ButtonPressMask
                   | ButtonReleaseMask
                   | Button1MotionMask
                   | Button2MotionMask
                   | Button3MotionMask
                   | Button4MotionMask
                   | Button5MotionMask;

      XSelectInput(display,
                   pd_data->menubar_x_window,
                   event_mask);
   }
else
   {
      dm_error("Cannot create menu bar window, menubar option canceled", DM_ERROR_LOG);
      pd_data->menubar_on = False;
   }



} /* end of get_menubar_window  */


/************************************************************************

NAME:      get_pulldown_colors  - Set up colors for the pulldowns.

PURPOSE:    This routine creates sets up the gc's for the pulldowns.

PARAMETERS:

   1.  main_pad      - pointer to PAD_DESCR (INPUT)
                       This is a pointer to the main window drawable description.
                       The sb windows will be children of this window.

   2.  display       - pointer to Display (Xlib type) (INPUT)
                       This is the current display pointer.

   3.  pd_colors     - pointer to char (INPUT)
                       This string is extracted from the scrollBarColors
                       X resource.  It contains the 4 colors used in
                       drawing the scroll bars in the order, gutter color,
                       highlight color, slider to color, shadow color.
                       The colors are separated by blanks, commas, and/or
                       tabs.  If NULL or a null string the defaults are
                       used.

   4.   pd_data      - pointer to PD_DATA (INPUT/OUTPUT)
                       This is the pull down data which contains the private
                       data for pulldowns.  The GC's and color information is
                       stored here.
FUNCTIONS :
   1.   Determine whether to use the passed colors or one of the sets of
        default colors.

   2.   Allocate the color pixels needed to draw the pulldowns.
        Be wary of depth 1 displays.

   3.   Create the gc's used for drawing the pulldowns.


*************************************************************************/

static void  get_pulldown_colors(Display          *display,             /* input  */
                                 Window            main_x_window,       /* input  */
                                 XFontStruct      *font,                /* input  */
                                 char             *pd_colors,           /* input  */
                                 PD_DATA          *pd_data)             /* input / output */
{
PD_PRIVATE           *private;
int                   i;

XGCValues             gcvalues;
unsigned long         valuemask;
char                  msg[512];
char                  work_colors[512];
char                 *color;
int                   colormap_depth = DefaultDepth(display, DefaultScreen(display));


/***************************************************************
*  Access the private data.
***************************************************************/
private = get_private_data(pd_data);
if (!private) 
   return; /* message about out of memory already output */

/***************************************************************
*  Make sure the colors have not already been set.
***************************************************************/
if (private->pixels[0] || private->pixels[1] || private->pixels[2] || private->pixels[3])
   return;

/***************************************************************
*  Allocate the colors needed for the pull downs
***************************************************************/
if (pd_colors && *pd_colors && (colormap_depth != 1))
   {
      strlcpy(work_colors, pd_colors, sizeof(work_colors));
      color = strtok(work_colors, "\t ,");
   }
else
   color = NULL;

/* color screen */
/* [0] = "gray59" "#979797" */
/* [1] = "gray87" "#dddddd" */    
/* [2] = "gray37" "#5e5e5e" */
/* [3] = "black"  "#000000" */

for (i = 0; i < BAR_COLORS; i++)
{
   private->pixels[i] = BAD_COLOR;
   if (color)
      {
         private->pixels[i] = colorname_to_pixel(display, color);
         if (private->pixels[i] == BAD_COLOR)
            {
               snprintf(msg, sizeof(msg), "Could not find color %s, trying %s", color, bar_colors[i]); 
               dm_error(msg, DM_ERROR_BEEP);
             }
         color = strtok(NULL, "\t ,");
      }

   if (colormap_depth != 1) 
      {
         if (private->pixels[i] == BAD_COLOR)
            {
               private->pixels[i] = colorname_to_pixel(display, bar_colors[i]);
               if (private->pixels[i] == BAD_COLOR)
                  {
                     /*snprintf(msg, sizeof(msg), "Could not find color %s, using %s", bar_colors[i], bw_colors[i]); */
                     snprintf(msg, sizeof(msg), "Could not find color %s, switching to black and white mode", bar_colors[i]); 
                     dm_error(msg, DM_ERROR_BEEP);

                     colormap_depth = 1; /* use the black and white colors */
                     i = -1; /* restart the loop */
#ifdef blah
                     if (strcmp(bw_colors[i], "black") == 0)
                        private->pixels[i] = BlackPixel(display, DefaultScreen(display));
                     else
                        private->pixels[i] = WhitePixel(display, DefaultScreen(display));
#endif
                   }
            }
      }
   else
      if (strcmp(bw_colors[i], "black") == 0)
         private->pixels[i] = BlackPixel(display, DefaultScreen(display));
      else
         private->pixels[i] = WhitePixel(display, DefaultScreen(display));

   DEBUG29(fprintf(stderr, "Pulldown Pixel[%d] = %d\n", i, private->pixels[i]);)
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

gcvalues.cap_style  = CapButt;
gcvalues.line_width = PD_SHADOW_LINE_WIDTH;
gcvalues.graphics_exposures = False;
gcvalues.font       = font->fid;

/***************************************************************
*
*  Create the gc's for the pulldown windows
*
***************************************************************/

gcvalues.background           = private->pixels[0];
gcvalues.foreground           = private->pixels[0];

DEBUG9(XERRORPOS)
private->bg_gc = XCreateGC(display,
                        main_x_window,
                        valuemask,
                        &gcvalues
                       );

gcvalues.foreground           = private->pixels[2];
DEBUG9(XERRORPOS)
private->darkline_gc = XCreateGC(display,
                        main_x_window,
                        valuemask,
                        &gcvalues
                       );

gcvalues.foreground           = private->pixels[1];
DEBUG9(XERRORPOS)
private->lightline_gc = XCreateGC(display,
                        main_x_window,
                        valuemask,
                        &gcvalues
                       );

gcvalues.foreground           = private->pixels[3];
DEBUG9(XERRORPOS)
private->text_gc = XCreateGC(display,
                   main_x_window,
                   valuemask,
                   &gcvalues
                   );


} /* end of get_pulldown_colors  */


/************************************************************************

NAME:      resize_menu_bar       - Resize the menu bar

PURPOSE:    This routine will resize the menu bar to fit in the
            main window.
            NOTE:  This routine should be called after the main window
            data has been updated to reflect the resize.  It is called
            before resizing the titlebar.

PARAMETERS:
   1.  display          - pointer to Display (Xlib type) (INPUT)
                          This is the token needed to make X calls.  It
                          represents the window being written to.

   2.  menubar_x_window -  Window (INPUT)
                           This is the menubar window which is a child
                           of the main window.  It is the window being
                           resized.

   3.  main_window      -  pointer to DRAWABLE_DESCR (INPUT)
                           This structure describes the size and shape
                           of the main window.  It also contains font
                           data and gc's for the window.
                           It is used for size information.

   4.  title_subarea    -  pointer to DRAWABLE_DESCR (INPUT)
                           This structure describes the size and shape
                           of the titlebar area of the main window. 
                           It also contains font data and gc's for the window.
                           It is used for the font data and gc's

   5.  menubar_win      -  pointer to DRAWABLE_DESCR (INPUT/OUTPUT)
                           This structure describes the size and shape
                           of the menubar window.  It is the structure
                           being updated.


FUNCTIONS :
   1.   Determine the new width of the windows. They are the
        the width of the main window - 4.

   2.   Determine the heigth of the window.  It is the height
        of one titlebar line

   3.   Resize and remap the menubar window.


*************************************************************************/

void  resize_menu_bar(Display          *display,              /* input   */
                      Window            menubar_x_window,     /* input   */
                      DRAWABLE_DESCR   *main_window,          /* input   */
                      DRAWABLE_DESCR   *title_subarea,        /* input   */
                      DRAWABLE_DESCR   *menubar_win)          /* input / output  */
{
int                   window_height;
XWindowChanges        window_attrs;
unsigned long         window_mask;
		
/***************************************************************
*
*  Copy stuff from the titlebar window in case we have not
*  aready done this.
*
***************************************************************/

menubar_win->gc         =  title_subarea->gc;
menubar_win->reverse_gc =  title_subarea->reverse_gc;
menubar_win->xor_gc     =  title_subarea->xor_gc;
menubar_win->font       =  title_subarea->font;


/***************************************************************
*
*  Determine the new width of the window.
*  The X, Y, and height stay the same.
*
***************************************************************/

window_height = menubar_win->font->ascent + menubar_win->font->descent;
window_height += (window_height * MENUBAR_HEIGHT_PADDING) / 100;

menubar_win->height    = window_height;
menubar_win->width     = main_window->width - 4;
menubar_win->sub_width = main_window->sub_width - 4;
menubar_win->x       = 1;
menubar_win->y       = 1;

menubar_win->sub_height  = window_height;
menubar_win->sub_x       = 2;
menubar_win->sub_y       = 1;
menubar_win->sub_width   = main_window->width - 4;

/***************************************************************
*
*  Resize the currently unmapped window
*
***************************************************************/

window_attrs.height           = menubar_win->height;
window_attrs.width            = menubar_win->width;
window_attrs.x                = menubar_win->x;
window_attrs.y                = menubar_win->y;

window_mask = CWX
            | CWY
            | CWWidth
            | CWHeight;

DEBUG9(XERRORPOS)
XConfigureWindow(display, menubar_x_window, window_mask, &window_attrs);

/***************************************************************
*
*  Remap the windows and we are done.
*
***************************************************************/

DEBUG9(XERRORPOS)
XMapWindow(display, menubar_x_window);


DEBUG1(
   fprintf(stderr, "resize_menu_bar:  main:  %dx%d%+d%+d, menubar:  %dx%d%+d%+d\n",
                   main_window->width, main_window->height, main_window->x, main_window->y,
                   menubar_win->width, menubar_win->height, menubar_win->x, menubar_win->y);
)

} /* end of resize_menu_bar  */


/************************************************************************

NAME:      dm_mi  - Menu Item command (builds menus)

PURPOSE:    This routine "does" a menu item cmd.  It causes the menu item to be installed
            into this Ce session and possibly into the global keydef property.

PARAMETERS:

   1. dmc          -  pointer to DMC  (INPUT)
                      This the first DM command structure for the mi 
                      command.  If there are more commands on the
                      list, they are ignored by this routine.  That is this
                      routine does not look at dmc->next.

   2. update_server - int (INPUT)
                      This flag specifies whether the key defintion should be
                      updated in the server.  It is set to False during
                      initial keydef loading and True for interactive key
                      definition commands.

   3. dspl_descr    - pointer to DISPLAY_DESCR (INPUT)
                      This is the display description for the current
                      display.  It is needed for the hash table and to
                      update the server definitions.

FUNCTIONS :
   1.  Parse the keyname to the the pulldown name and the menu item number.

   2.  If the data in the key definition is NULL, this is a request for
       data about that key definition.  Look up the definition and output
       it to the DM command window via dump_kd.

   3.  Prescan the defintion to see if it contains any prompts for input.
       such as   mi Help/$  'General'cv /ce/help &'Help on: '  ke

   4.  If the prescan produced no results, parse the key definition.

   5.  If the parse worked, install the menu item.

RETURNED VALUE:
   rc   -  int
           The returned value shows whether a menu item was successfully
           loaded.
           0   -  keydef loaded
           -1  -  keydef not loaded, message already generated

*************************************************************************/

int   dm_mi(DMC               *dmc,
            int                update_server,
            DISPLAY_DESCR     *dspl_descr)
{
int                   rc = 0;
int                   i;
int                   mi_num;
char                  pd_name[MAX_PD_NAME+1];
DMC                  *first_dmc;
DMC                  *new_dmc;
char                  the_key[KEY_KEY_SIZE];
char                  work[512];
char                  msg[512];
char                 *p;
char                 *past_menu_text;
char                  work_def[MAX_LINE+1];
char                  sep;

/***************************************************************
*  Check for a menu item name being passed.
*  Format is name/number
*  This block of code sets up variables pd_name and mi_num
***************************************************************/
if (dmc->mi.key && *(dmc->mi.key))
   {
      strlcpy(work, dmc->mi.key, sizeof(work));
      if ((p = strchr(work, '/')) != NULL)
         {
            *p++ = '\0';
            /***************************************************************
            *  Save the pull down name
            ***************************************************************/
            if ((int)strlen(work) <= MAX_PD_NAME)
               strlcpy(pd_name, work, sizeof(pd_name));
            else
               {
                  snprintf(msg, sizeof(msg), "(mi) Pulldown name too long (%s%s)", iln(dmc->mi.line_no), work);
                  dm_error_dspl(msg, DM_ERROR_BEEP, dspl_descr);
                  rc = -1;
               }
            /***************************************************************
            *  Extract the number, $ means find the first unused number.
            *  We will count non-drawn items as unused.
            ***************************************************************/
            if ((*p == '$') && (*(p+1) == '\0'))
               mi_num = next_avail_mi_num(dspl_descr->hsearch_data, pd_name);
            else
               if (sscanf(p, "%d%9s", &mi_num, msg) != 1)
                  {
                     snprintf(msg, sizeof(msg), "(mi) Part of menu item name past the slash is not numeric (%s%s)", iln(dmc->mi.line_no), dmc->mi.key);
                     dm_error_dspl(msg, DM_ERROR_BEEP, dspl_descr);
                     rc = -1;
                  }
         }
      else
         {
            snprintf(msg, sizeof(msg), "(mi) Menu item name does not contain a slash (/)(%s%s)", iln(dmc->mi.line_no), dmc->mi.key);
            dm_error_dspl(msg, DM_ERROR_BEEP, dspl_descr);
            rc = -1;
         }
   }
else
   {
      snprintf(msg, sizeof(msg), "(mi) Null menu item name (%s%s)", iln(dmc->mi.line_no), dmc->mi.key);
      dm_error_dspl(msg, DM_ERROR_BEEP, dspl_descr);
      rc = -1;
   }

if (rc)
   return(rc);

DEBUG29(fprintf(stderr, "Pulldown %s, line %d\n", pd_name, mi_num);)

/***************************************************************
*  If the menu item number is < 0 (-1) and there is a null
*  definition, delete all the menu items.
***************************************************************/
if (mi_num < 0)
   {
      if ((dmc->mi.def == NULL) || (dmc->mi.def[0] == '\0'))
         empty_pulldown(dspl_descr, pd_name, dmc->any.cmd);
      else
         {
            snprintf(msg, sizeof(msg), "(mi) Invalid menu item line number for defining line (%s%s)", iln(dmc->mi.line_no), dmc->mi.key);
            dm_error_dspl(msg, DM_ERROR_BEEP, dspl_descr);
         }
      return(-1);
   }


/***************************************************************
*  Check if this is a request to dump an existing mi.
*  If so, dump it and we are done.
***************************************************************/
if (dmc->mi.def == NULL)
   {
      new_dmc  = get_menu_cmds(dspl_descr->hsearch_data,
                               pd_name,
                               mi_num,
                               MENU_ITEM_CMDS);  
      if (new_dmc != NULL)
         dump_kd(NULL, new_dmc, False /* list them all */, False, NULL, dspl_descr->escape_char);
      else
         dm_error_dspl("", DM_ERROR_MSG, dspl_descr);
      return(-1);  /* not really failure, but no menu item was loaded */
   }

/***************************************************************
*  The first thing in the menu item definition is a quoted
*  string single quotes, double quotes, or grave quotes.  We
*  parse it out into a dm_es command structure.  It is the title
*  for the line.  If the first character is not a quote character,
*  we will use the menu item number.
***************************************************************/
first_dmc = (DMC *)CE_MALLOC(sizeof(DMC));
if (!first_dmc)
   return(-1);
memset((char *)first_dmc, 0, sizeof(DMC));
first_dmc->any.next         = NULL;
first_dmc->any.cmd          = DM_es;
first_dmc->any.temp         = False;

strlcpy(work_def, dmc->mi.def, sizeof(work_def));
p = work_def;

/***************************************************************
*  Parse out the quotes text string at the front of the menu
*  item defintion.  This is done up front.  We could have defined
*  using an 'es' as the first element, but that would complicate
*  strings with prompts.  Thus we have:
*  mi  <pd_name>/<menu_line_num> "text"command1;command2;...
***************************************************************/
if (*p != '\0')
   if ((*p == '\'') || (*p == '"') || (*p == '`'))
      {
         sep = *p;
         past_menu_text = copy_with_delim(++p, msg, sep, dspl_descr->escape_char, False /* missing delim not ok */);
         if (!past_menu_text)
            {
               snprintf(msg, sizeof(msg), "(mi) Command syntax error - missing Text string delimiter (%s%s)", iln(dmc->mi.line_no), work_def);
               dm_error_dspl(msg, DM_ERROR_BEEP, dspl_descr);
               return(-1);
            }
      }
   else
      {
         snprintf(msg, sizeof(msg), "(mi) Missing Text string for menu item (%s%s)", iln(dmc->mi.line_no), work_def);
         dm_error_dspl(msg, DM_ERROR_BEEP, dspl_descr);
         snprintf(msg, sizeof(msg), "%d", mi_num);
         past_menu_text = work_def;
      }
else
   {
      msg[0] = '\0';
      past_menu_text = work_def;
   }

i = strlen(msg);
if (i < ES_SHORT)
   first_dmc->es.string = first_dmc->es.chars;
else
   first_dmc->es.string = CE_MALLOC(i+1);
if (!first_dmc->es.string)
   return(-1);
strcpy(first_dmc->es.string, msg);

/***************************************************************
*  Scan the list to see if the same text string is used in
*  another menu item in this pulldown.  If so, give a warning
*  message and set the menu item to the earlier value.
***************************************************************/
mi_num = merge_duplicate_titles(dspl_descr->hsearch_data, pd_name, first_dmc->es.string, mi_num);


/***************************************************************
*  See if any prompts need to be processed.  If so, build a
*  prompt type DMC.  Otherwise parse the line into a DMC structure.
***************************************************************/
new_dmc = prompt_prescan(past_menu_text, False, dspl_descr->escape_char); /* in prompt.c */
if (new_dmc == NULL)
   new_dmc = parse_dm_line(past_menu_text, 0, dmc->mi.line_no, dspl_descr->escape_char, dspl_descr->hsearch_data);

/***************************************************************
*  Make sure the pulldown does not have a mix of pulldown and 
*  non-pulldown commands.  If there is a pulldown, there better
*  be exactly one command.
***************************************************************/
if ((new_dmc != DMC_COMMENT) && pd_in_list(new_dmc) && ((new_dmc->any.cmd != DM_pd) || (new_dmc->next != NULL)))
   {
      snprintf(msg, sizeof(msg), "(mi) Cannot mix pulldown and non-pulldown items in the same menu item (%s%s)", iln(dmc->mi.line_no), work_def);
      dm_error_dspl(msg, DM_ERROR_BEEP, dspl_descr);
      free_dmc(new_dmc);
      return(-1);
   }


if (new_dmc != NULL)
   {
      if (new_dmc == DMC_COMMENT)
         {
            /***************************************************************
            *  Special case, install a null command.
            *  Used when with  mi ^f1 ke  (undefining items)
            ***************************************************************/
            new_dmc = (DMC *)CE_MALLOC(sizeof(DMC));
            if (!new_dmc)
               return(-1);
            new_dmc->any.next         = NULL;
            new_dmc->any.cmd          = DM_NULL;
            new_dmc->any.temp         = False;
         }

      DEBUG4(
         fprintf(stderr, "(mi) Installing:  ");
         dump_kd(stderr, new_dmc, 0, False, NULL, dspl_descr->escape_char);
      )
      /***************************************************************
      *  Splice the the es of the menu item text to the front of the list
      ***************************************************************/
      first_dmc->next = new_dmc;
      new_dmc = first_dmc;
      /***************************************************************
      *  Build the key from the key name.
      ***************************************************************/
      snprintf(the_key, sizeof(the_key), MI_KEY_FORMAT, pd_name, mi_num, MI_HASH_TYPE);
      DEBUG11(fprintf(stderr, "dm_mi:  key \"%s\" ", the_key);)
      /***************************************************************
      *  Update the X server property if required.  Not required is
      *  when we are doing a -load or -reload (done en masse when done).
      ***************************************************************/
      rc = install_keydef(the_key, new_dmc, update_server, dspl_descr, dmc);
      if (rc != 0)
         {
            snprintf(work, sizeof(work), "(%s) Unable to install key definition, internal error", dmsyms[dmc->kd.cmd].name);
            dm_error_dspl(work, DM_ERROR_LOG, dspl_descr);
            free_dmc(new_dmc);
            rc = -1;  /* make sure it has the expected value */
         }
   }
else
   rc = -1;

return(rc);

}  /* end of dm_mi */

/************************************************************************

NAME:      next_avail_mi_num - Find unused Menu item in a pulldown

PURPOSE:    This routine scans the list of menu items in the passed
            pull down to find the next empty slot.
            The empty slot may be a previously deleted line.

PARAMETERS:
   1.  hsearch_data -  pointer to void (INPUT)
                       This is the hash table which is used to look up
                       key definitions, aliases, and menu items.
                       It is associated with the current display.

   2.  pd_name    - pointer to char (INPUT)
                    This is the name of the pulldown to search.

FUNCTIONS :
   1.   Attempt to retrieve each menu item for the pulldown.
        When an empty one is found or we run off the end of the
        list, return that index.


RETURNED VALUE:
   The next available line in the menu timem is returned.

*************************************************************************/

static int next_avail_mi_num(void    *hsearch_data,
                             char    *pd_name)
{
int                   mi_num = 0;
DMC                  *new_dmc;

/* CONSTCOND */
while(1)
{
   new_dmc  = get_menu_cmds(hsearch_data,
                            pd_name,
                            mi_num,
                            MENU_ITEM_TEXT);  
   if ((new_dmc == NULL) || ((new_dmc->any.cmd == DM_es) && (new_dmc->es.string[0] == '\0')))
      break;
   else
      mi_num++;
}

return(mi_num);

} /* end of next_avail_mi_num */


/************************************************************************

NAME:      merge_duplicate_titles - Make sure same title is not in pulldown twice

PURPOSE:    This routine scans the list of menu items in the passed
            pull down to make sure the same title does not exist twice.
            If the same title is found, the menu item number of the
            pulldown is returned.

PARAMETERS:
   1.  hsearch_data   - pointer to void (INPUT)
                        This is the hash table which is used to look up
                        key definitions, aliases, and menu items.
                        It is associated with the current display.

   2.  pd_name        - pointer to char (INPUT)
                        This is the name of the pulldown to search.

   3.  title_string   - pointer to char (INPUT)
                        This is the string to check for.

   4.  current_mi_num - int (INPUT)
                        This is the current menu item number.

FUNCTIONS :
   1.   Attempt to retrieve each menu item for the pulldown.
        When we run off the end of the list, return the the
        current index.

   2.   If the title string passed matches one in the menu
        item and this is not already the current menu item,
        return the current menu item.


RETURNED VALUE:
   current_mi_num   - int
                      The menu item number of the matching title
                      or the passed current menu item is returned.

*************************************************************************/

static int merge_duplicate_titles(void    *hsearch_data,
                                  char    *pd_name,
                                  char    *title_string,
                                  int      current_mi_num)
{
int                   mi_num = 0;
DMC                  *new_dmc;
char                  msg[256];

/* CONSTCOND */
while(1)
{
   new_dmc  = get_menu_cmds(hsearch_data,
                            pd_name,
                            mi_num,
                            MENU_ITEM_TEXT);  
   if (new_dmc == NULL)
      break;
   else
      if ((mi_num != current_mi_num) && (new_dmc->any.cmd == DM_es) && (strcmp(new_dmc->es.string, title_string) == 0))
         {
            snprintf(msg, sizeof(msg), "pd: duplicate memu item title in pulldown %s,  %d changed to %d", pd_name, current_mi_num, mi_num);
            dm_error(msg, DM_ERROR_BEEP);
            current_mi_num = mi_num;
            break;
         }
      else
         mi_num++;
}

return(current_mi_num);

} /* end of merge_duplicate_titles */


/************************************************************************

NAME:      empty_pulldown - Find unused Menu item in a pulldown

PURPOSE:    This routine scans the list of menu items in the passed
            pull downt to find the next empty slot.
            The empty slot may be a previously deleted line.

PARAMETERS:
   1. dspl_descr    - pointer to DISPLAY_DESCR (INPUT)
                      This is the display description for the current
                      display.  It is needed for the hash table and to
                      update the server definitions.

   2.  pd_name      - pointer to char (INPUT)
                      This is the name of the pulldown to empty.

   3.  cmd_id       - pointer to int (INPUT)
                      This is the command id.  It is either DM_mi or
                      DM_lmi.  In the first case, we will update the
                      server definitions.

FUNCTIONS :
   1.   Initialize a DMC to empty a menu item.

   2.   Walk the list of menu items for the passed pulldown and empty
        any which are not emptied.


*************************************************************************/

static void empty_pulldown(DISPLAY_DESCR    *dspl_descr,
                           char             *pd_name,
                           int               cmd_id)
{
int                   mi_num = 0;
DMC                  *new_dmc;
DMC                   kill_dmc;
char                  msg[128];

memset((char *)&kill_dmc, 0, sizeof(DMC));
kill_dmc.mi.cmd          = cmd_id;
kill_dmc.mi.def          = "";
kill_dmc.mi.key          = msg;

/* CONSTCOND */
while(1)
{
   new_dmc  = get_menu_cmds(dspl_descr->hsearch_data,
                            pd_name,
                            mi_num,
                            MENU_ITEM_TEXT);  
   if (new_dmc == NULL)
      break;
   else
      {
         if ((new_dmc->any.cmd == DM_es) && (new_dmc->es.string[0] != '\0'))
            {
               snprintf(msg, sizeof(msg), "%s/%d", pd_name, mi_num);
               dm_mi(&kill_dmc, cmd_id == DM_mi, dspl_descr);
            }
         mi_num++;
      }
}

snprintf(msg, sizeof(msg), "%d menu Items emptied", mi_num);
dm_error_dspl(msg, DM_ERROR_MSG, dspl_descr);


} /* end of empty_pulldown */



/************************************************************************

NAME:      dm_pd  - Pull Down command (initiates a pulldown)

PURPOSE:    This routine sets up a pulldown window.  It allocates the resources,
            determines the size, determines the location, and maps the window.
            actual drawing is done by another routine.

PARAMETERS:
   1. dmc          -  pointer to DMC  (INPUT)
                      This the first command structure for the pulldown command.
                      The parm field has the name of the pulldown we want
                      to activate.

   2. event         - pointer to XEvent (INPUT)
                      This is the X event which caused the pulldown command
                      to be executed.  It is used to get the cursor
                      position to position the pulldown.

   3. dspl_descr    - pointer to DISPLAY_DESCR (INPUT)
                      This is the display description for the current
                      display.

FUNCTIONS :
   1.  Verify that element 1 in the pulldown exists in the hash table.
       this checks that the pulldown is using a valid name.  Verify that
       we have not exceeded the max pulldown nesting.

   2.  Determine where the cursor is.  It can be in the title bar area
       or on the main window.  If it is in the main window, check to see
       if a pulldown is active.  Use the items in it to position the new
       pull down window.

   3.  Count the items in the window and there width's.  Use this to
       size the window.  If necessary, adjust the position to keep from 
       going off screen.

   4.  Get the window id from the stack of windows.

   5.  Map the window.

*************************************************************************/

void  dm_pd(DMC               *dmc,
            XEvent            *event,
            DISPLAY_DESCR     *dspl_descr)
{
int                   i;
int                   done;
DMC                  *new_dmc;
char                  msg[512];
int                   item_count;
int                   null_line;
PD_PRIVATE           *private;
int                   x;
int                   w_idx;
int                   y;
int                   pd_has_submenu = False;
Window                window;
unsigned int          width = 0;

if ((!dmc->pd.parm) || (*(dmc->pd.parm) == '\0'))
   {
      dm_error_dspl("(pd) Null pulldown name", DM_ERROR_BEEP, dspl_descr);
      return;
   }

if (strlen(dmc->pd.parm) > MAX_PD_NAME)
   {
      snprintf(msg, sizeof(msg), "(pd) Pulldown name too long (%s)", dmc->pd.parm);
      dm_error_dspl(msg, DM_ERROR_BEEP, dspl_descr);
      return;
   }

/***************************************************************
*  Make sure menu item 0 exists.
***************************************************************/
new_dmc  = get_menu_cmds(dspl_descr->hsearch_data,
                         dmc->pd.parm,
                         0,
                         MENU_ITEM_TEXT);  
if (new_dmc == NULL)
   {
      snprintf(msg, sizeof(msg), "(pd) Cannot find menu item 0 for Pulldown name %s", dmc->pd.parm);
      dm_error_dspl(msg, DM_ERROR_BEEP, dspl_descr);
      return;
   }

/***************************************************************
*  Get access to the private data and check the nesting level.
***************************************************************/
private = get_private_data(dspl_descr->pd_data);
if (private->win_nest_level >= MAX_PD_WINS)
   {
      snprintf(msg, sizeof(msg), "(pd) Max pulldown nesting %d exceeded in Pulldown %s", MAX_PD_WINS, dmc->pd.parm);
      dm_error_dspl(msg, DM_ERROR_BEEP, dspl_descr);
      return;
   }

/***************************************************************
*  If the colors have not already been set, do it now.
***************************************************************/
if (!private->pixels[0] && !private->pixels[1] && !private->pixels[2] && !private->pixels[3])
   get_pulldown_colors(dspl_descr->display,
                       dspl_descr->main_pad->x_window,
                       dspl_descr->main_pad->window->font,
                       SBCOLORS,
                       dspl_descr->pd_data);

/***************************************************************
*  Get the x and y coordinates of the cursor.  This will be used
*  to place the pulldown window.
***************************************************************/
get_loc(event, False, &x, &y, &window);
DEBUG29(fprintf(stderr, "dm_pd: Location (%d,%d) window 0x%X\n", x, y, window);)

/***************************************************************
*  Set up initial placement on the window.
*  This is based on the current cursor position.
***************************************************************/
if (window == dspl_descr->pd_data->menubar_x_window)
   {
      /***************************************************************
      *  If we are in the menu bar window we had first off better be at
      *  nesting level 0.  If not, make it so.  Then we are either
      *  place the pulldown under a menu bar item or wherever the 
      *  cursor is.
      ***************************************************************/
      if (private->win_nest_level > 0)
         pd_remove(dspl_descr, 0);

      if (y < (int)dspl_descr->pd_data->menu_bar.height)
         {
           for (i = 0; i < private->current_bar_items; i++)
               if ((x >= private->bar_l_offset[i]) &&
                   (x <  private->bar_r_offset[i]))
                  {
                     x = private->bar_l_offset[i] + dspl_descr->main_pad->window->x;
                     y = dspl_descr->pd_data->menu_bar.height + dspl_descr->main_pad->window->y;
                     break;
                  }
         }
      DEBUG29(fprintf(stderr, "dm_pd: pd placement off main window Root(%d,%d)\n", x, y);)

   } /* is menu bar window */
else
   if ((w_idx = pd_window_to_idx(dspl_descr->pd_data, window)) != -1)
      {
         /***************************************************************
         *  If we are in a pulldown window, align the new window with
         *  the current line.
         ***************************************************************/
         if (private->win_nest_level > w_idx+1)
            pd_remove(dspl_descr, w_idx+1);
         x = private->pd[w_idx].win_shape.x+private->pd[w_idx].win_shape.width-1;
         y = ((y-private->pd[w_idx].win_shape.sub_y)/private->pd[w_idx].win_shape.line_height)*private->pd[w_idx].win_shape.line_height;
         y += private->pd[w_idx].win_shape.y+2;
         DEBUG29(fprintf(stderr, "dm_pd: pd placement off menu nest %d window Root(%d,%d)\n", w_idx, x, y);)
      }
   else
      {
         /***************************************************************
         *  Some other window.  Assume upper left of main drawing area.
         ***************************************************************/
         x = dspl_descr->main_pad->window->sub_x + dspl_descr->main_pad->window->x;
         y = dspl_descr->main_pad->window->sub_y + dspl_descr->main_pad->window->y;
         window = dspl_descr->main_pad->x_window;
         DEBUG29(fprintf(stderr, "dm_pd: pd placement off unknown window, using main Root(%d,%d)\n", x, y);)
      }

/***************************************************************
*  Count the items in the menu to size the window.
***************************************************************/
done = False;
null_line = 0;
item_count = 0;
while(!done)
{
   new_dmc  = get_menu_cmds(dspl_descr->hsearch_data,
                            dmc->pd.parm,
                            item_count,
                            MENU_ITEM_TEXT);  
   if (new_dmc != NULL)
      {
         if (new_dmc->any.cmd == DM_es)
            if (new_dmc->es.string[0] != '\0')
               {
                  width = MAX((int)width, XTextWidth(dspl_descr->main_pad->window->font,
                                                new_dmc->es.string,
                                                strlen(new_dmc->es.string)));
                  /* note the submenu so we can save space for the little arrow */
                  if ((new_dmc->next)->any.cmd == DM_pd)
                     pd_has_submenu = True;
               }
            else
               null_line++;

         item_count++;
      }
   else
      done = True;
}

/***************************************************************
*  Save the number of menu items, but subtract out the null
*  ones when sizing the window.
***************************************************************/
private->pd[private->win_nest_level].lines_in_pulldown = item_count;
item_count -= null_line;  /* don't count null items */
width += MARGIN*2;

if (item_count > 0)
   {
      if (pd_has_submenu)
         width += (ARROW_WIDTH + PD_SHADOW_LINE_WIDTH);

      DEBUG29(fprintf(stderr, "dm_pd: %d items in pulldown\n", item_count);)

      /***************************************************************
      *  Build and configure the window.
      ***************************************************************/
      strlcpy(private->pd[private->win_nest_level].pd_name, dmc->pd.parm, MAX_PD_NAME+1);
      get_pd_window(dspl_descr, x, y, width, item_count);
   }

}  /* end of dm_pd */


/************************************************************************

NAME:      dm_pdm  - Toggle the menu bar on and off

PURPOSE:    This routine turns the menu bar on and off.

PARAMETERS:
   1. dmc          -  pointer to DMC  (INPUT)
                      This the first command structure for the pdm command.

   2. event         - pointer to XEvent (INPUT)
                      This is the X event which caused the pulldown command
                      to be executed.  It is used to get the cursor
                      position to position the pulldown.

   3. dspl_descr    - pointer to DISPLAY_DESCR (INPUT)
                      This is the display description for the current
                      display.

FUNCTIONS :
   1.  Verify that element 1 in the pulldown exists in the hash table.
       this checks that the pulldown is using a valid name.  Verify that
       we have not exceeded the max pulldown nesting.

   2.  Determine where the cursor is.  It can be in the title bar area
       or on the main window.  If it is in the main window, check to see
       if a pulldown is active.  Use the items in it to position the new
       pull down window.

   3.  Count the items in the window and there width's.  Use this to
       size the window.  If necessary, adjust the position to keep from 
       going off screen.

   4.  Get the window id from the stack of windows.

   5.  Map the window.

*************************************************************************/

void  dm_pdm(DMC               *dmc,
             DISPLAY_DESCR     *dspl_descr)
{
int                   changed;


dspl_descr->pd_data->menubar_on = toggle_switch(dmc->pdm.mode, /* in mark.c */
                                        dspl_descr->pd_data->menubar_on,
                                        &changed); /* shows change */
if (changed)
   dspl_descr->redo_cursor_in_configure = REDRAW_FORCE_REAL_WARP;

if (changed && !dspl_descr->pd_data->menubar_on)
   {
      DEBUG9(XERRORPOS)
      XUnmapWindow(dspl_descr->display, dspl_descr->pd_data->menubar_x_window);
   }
   

reconfigure(dspl_descr->main_pad, dspl_descr->display); /* in window.c */


}  /* end of dm_pdm */


/************************************************************************

NAME:      get_loc  - Extract location info from an X event.

PURPOSE:    This routine switches on the event type and returns the
            correct fields.

PARAMETERS:
   1.   event       - pointer to XEvent (INPUT)
                      This is the event to be examined.
                      Only certain types are supported.  See the code.

   2.   root        - int (INPUT)
                      This flag says get root window coordinates.

   3.   x           - pointer to int (OUTPUT)
                      The mouse position X coordinate

   4.   y           - pointer to int (OUTPUT)
                      The mouse position Y coordinate

   5.   w           - pointer to Window (OUTPUT)
                      The window the mouse was in.

FUNCTIONS :
   1.  Switch on the event type.

   2.  Return the appropriate X, Y, and window values.

*************************************************************************/

static void  get_loc(XEvent            *event,
                     int                root,
                     int               *x,
                     int               *y,
                     Window            *w)
{
char                  msg[512];

*x = 0;
*y = 0;
*w = None;

switch (event->type)
{
case KeyPress:
case KeyRelease:
   if (!root)
      {
         *x = event->xkey.x;
         *y = event->xkey.y;
      }
   else
      {
         *x = event->xkey.x_root;
         *y = event->xkey.y_root;
      }
   *w = event->xkey.window;
   break;
case ButtonPress:
case ButtonRelease:
   if (!root)
      {
         *x = event->xbutton.x;
         *y = event->xbutton.y;
      }
   else
      {
         *x = event->xbutton.x_root;
         *y = event->xbutton.y_root;
      }
   *w = event->xbutton.window;
   break;
case MotionNotify:
   if (!root)
      {
         *x = event->xmotion.x;
         *y = event->xmotion.y;
      }
   else
      {
         *x = event->xmotion.x_root;
         *y = event->xmotion.y_root;
      }
   *w = event->xmotion.window;
   break;
case EnterNotify:
case LeaveNotify:
   if (!root)
      {
         *x = event->xcrossing.x;
         *y = event->xcrossing.y;
      }
   else
      {
         *x = event->xcrossing.x_root;
         *y = event->xcrossing.y_root;
      }
   *w = event->xcrossing.window;
   break;
default:
   snprintf(msg, sizeof(msg), "Error: Unexpected event type (%d) passed to get_loc, internal error", event->type);
   dm_error(msg, DM_ERROR_LOG);
   break;
}

} /* end of get_loc */


/************************************************************************

NAME:      pd_remove  - Remove pulldowns

PURPOSE:    This routine unmaps pulldown menu's down to the requested
            nesting level.

PARAMETERS:
   1.   dspl_descr    - pointer to DISPLAY_DESCR (INPUT)
                        This is the display description for the current
                        display.  It is needed for the pulldown data an
                        the window id's for all the windows.

   2.   new_nest_level - int(INPUT)
                         This is the last nesting level to be
                         removed.

FUNCTIONS :
   1.  Walk the nesting array backwards unmapping windows.


*************************************************************************/

void pd_remove(DISPLAY_DESCR     *dspl_descr,
               int                new_nest_level)
{
int          i;
PD_DATA     *pd_data = dspl_descr->pd_data;
PD_PRIVATE  *private;
Window       focus_window;
int          junk;

private = pd_data->pd_private;
	
DEBUG29(fprintf(stderr, "pd_remove: New nest level %d\n", new_nest_level);)
if (new_nest_level < 0)
   new_nest_level = 0;

/***************************************************************
*  If we are scraping the window, the cursor is not there anymore.
***************************************************************/
if (new_nest_level <= private->current_cursor_w_idx)
   {
      private->current_cursor_w_idx = -1;
      private->current_cursor_menu_item = -1;
   }

if (private)
   for (i = private->win_nest_level-1; i >= new_nest_level; i--)
   {
      DEBUG9(XERRORPOS)
      XUnmapWindow(dspl_descr->display, private->pd[i].pd_win);
   }

private->win_nest_level = new_nest_level;

if (private->win_nest_level == 0)
   pd_data->pulldown_active = False;

/***************************************************************
*  If the pulldowns are all gone, check the input focus.  If we
*  still have it, show it in the internal flag so the text 
*  cursor gets drawn correctly.
***************************************************************/
if (pd_data->pulldown_active)
   {
      DEBUG9(XERRORPOS)
      XFlush(dspl_descr->display);
   }
else
   {
      DEBUG9(XERRORPOS)
      XGetInputFocus(dspl_descr->display,
                     &focus_window,
                     &junk);
      DEBUG29(fprintf(stderr, "pd_remove: Focus window is 0x%X\n", focus_window);)
      if ((focus_window == dspl_descr->main_pad->x_window) ||
          (focus_window == dspl_descr->dminput_pad->x_window) ||
          (focus_window == dspl_descr->dmoutput_pad->x_window) ||
          (focus_window == dspl_descr->unix_pad->x_window))
         force_in_window(dspl_descr);
   }

}  /* end of pd_remove */


/************************************************************************

NAME:      pd_window_to_idx  - Find pulldown entry given the X window id.

PURPOSE:    This routine finds a the passed window in the array of
            pulldown windows and returns the index.  The returned
            value is not useful outside this module except that a -1
            means the passed window is not a pulldown window.

PARAMETERS:
   1.   pd_data     - pointer to PD_DATA (INPUT)
                      This is the pull down data which contains
                      the private data which contains
                      the current list of pulldown windows.

   2.   w           - Window (INPUT)
                      This is the window we are looking for.

FUNCTIONS :
   1.  Scan the array of active window id's looking for the
       passed window.  If it is found return the index into
       the array.

   2.  If the window is not found, return -1;

RETURNED VALUE:
   idx    -   int
              This index into the array of pulldow windows with
              the window id matching the passed window.
              -1 is returned on no match.

*************************************************************************/

int  pd_window_to_idx(PD_DATA     *pd_data,
                      Window       w)
{
PD_PRIVATE           *private = pd_data->pd_private;
int                   i;

if (private)
   for (i = 0; i < private->win_nest_level; i++)
      if (w == private->pd[i].pd_win)
         return(i);

return(-1);

} /* end of pd_window_to_idx */


/************************************************************************

NAME:      get_pd_window  - Create a new pulldown window

PURPOSE:    This routine configures and maps the pulldown window.
            It either uses an existing window or creates a new window.
            Bounds checking is already done.  This is an internal
            routine used by dm_pd.

PARAMETERS:
   1.   dspl_descr  - pointer to DISPLAY_DESCR (INPUT)
                      This is the display description for the current
                      display.

   2.   x           - int (INPUT)
                      Position information for the window

   3.   y           - int (INPUT)
                      Position information for the window

   4.   width       - int (INPUT)
                      Position information for the window

   5.   lines       - int (INPUT)
                      Number of lines in the pulldown.  This
                      was already used for sizing the window.

FUNCTIONS :
   1.  Put the size information in the display description for
       the new window.

   2.  If the next window on the stack does not exist, create one.
       if it does, reuse it by resizing it.

   3.  Map the window.

*************************************************************************/

static void get_pd_window(DISPLAY_DESCR     *dspl_descr,
                          int                x,
                          int                y,
                          unsigned int       width,
                          int                lines)
{
PD_PRIVATE           *private = dspl_descr->pd_data->pd_private;
DRAWABLE_DESCR       *new_pd;
XGCValues             gcvalues;
unsigned long         valuemask;
XSetWindowAttributes  window_attrs;
XWindowChanges        changed_window_attrs;
unsigned long         window_mask;
unsigned long         event_mask;
int                   overflow;

/***************************************************************
*  Shape the new window.
***************************************************************/

new_pd = &private->pd[private->win_nest_level].win_shape;

new_pd->font        = dspl_descr->main_pad->window->font;
new_pd->fixed_font  = dspl_descr->main_pad->window->fixed_font;

new_pd->line_height = new_pd->font->ascent + new_pd->font->descent +
                      (PD_SHADOW_LINE_WIDTH * 2);

/***************************************************************
*  Check for going off the physical window.
***************************************************************/
overflow = (y + new_pd->height) - DisplayHeight(dspl_descr->display, DefaultScreen(dspl_descr->display));
if (overflow > 0)
   {
      y -= overflow;
      if (y < 0)
         y = 0;
   }

overflow = (x + width) - DisplayWidth(dspl_descr->display, DefaultScreen(dspl_descr->display));
if (overflow > 0)
   {
      x -= overflow;
      if (x < 0)
         x = 0;
   }


/***************************************************************
*  Set the pulldown window geometry
***************************************************************/
new_pd->x           = x;
new_pd->y           = y;
new_pd->width       = width;
new_pd->height      = (lines * new_pd->line_height) + (MARGIN*2);

new_pd->sub_x       = MARGIN;
new_pd->sub_y       = MARGIN;
new_pd->sub_width   = width - (MARGIN*2);
new_pd->sub_height  = new_pd->height - (MARGIN*2);

new_pd->lines_on_screen = lines;



/***************************************************************
*  If the window does not already exist, create one.
*  Otherwise, reconfigure the existing window.
***************************************************************/
if (private->pd[private->win_nest_level].pd_win == None)
   {
      /***************************************************************
      *  New window
      *  Get the pixel values from the main window gc.
      ***************************************************************/
      valuemask = GCForeground
                | GCBackground
                | GCLineWidth
                | GCCapStyle
                | GCGraphicsExposures
                | GCFont;

      DEBUG9(XERRORPOS)
      if (!XGetGCValues(dspl_descr->display,
                        dspl_descr->main_pad->window->gc,
                        valuemask,
                        &gcvalues))
         {
            DEBUG(   fprintf(stderr, "get_pd_window:  XGetGCValues failed for gc = 0x%X\n", dspl_descr->main_pad->window->gc);)
            gcvalues.foreground = BlackPixel(dspl_descr->display, DefaultScreen(dspl_descr->display));
            gcvalues.background = WhitePixel(dspl_descr->display, DefaultScreen(dspl_descr->display));
         }

      /***************************************************************
      *  Set the event mask and do not propagate mask.
      ***************************************************************/

      event_mask = ExposureMask
                   | EnterWindowMask
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
      *  Create the new window as override direct
      ***************************************************************/

      window_attrs.background_pixel      = gcvalues.background;
      window_attrs.border_pixel          = gcvalues.background;
      gcvalues.foreground                = private->pixels[0];
      window_attrs.win_gravity           = UnmapGravity;
      window_attrs.do_not_propagate_mask = PointerMotionMask;
      window_attrs.override_redirect     = True;
      window_attrs.cursor                = private->pointer_cursor;

      window_mask = CWBackPixel
                  | CWBorderPixel
                  | CWWinGravity
                  | CWDontPropagate
                  | CWCursor
                  | CWOverrideRedirect;

      DEBUG29(fprintf(stderr, "Getting new window for nest level %d -- ", private->win_nest_level);)
      DEBUG9(XERRORPOS)
      private->pd[private->win_nest_level].pd_win = XCreateWindow(
                                dspl_descr->display,
                                RootWindow(dspl_descr->display, DefaultScreen(dspl_descr->display)),
                                x, y,
                                width, new_pd->height,
                                1,                /* no border */
                                DefaultDepth(dspl_descr->display, DefaultScreen(dspl_descr->display)),
                                InputOutput,                    /*  class        */
                                CopyFromParent,                 /*  visual       */
                                window_mask,
                                &window_attrs
      );

      DEBUG29(fprintf(stderr, "0x%X\n", private->pd[private->win_nest_level].pd_win);)
      if (private->pd[private->win_nest_level].pd_win != None)
         {
            DEBUG9(XERRORPOS)
            XSelectInput(dspl_descr->display,
                         private->pd[private->win_nest_level].pd_win,
                         event_mask);
         }

   }
else
   {
      /***************************************************************
      *  Existing window
      *  Set the shape.
      ***************************************************************/
      changed_window_attrs.height           = new_pd->height;
      changed_window_attrs.width            = width;
      changed_window_attrs.x                = x;
      changed_window_attrs.y                = y;

      window_mask = CWX
                  | CWY
                  | CWWidth
                  | CWHeight;

      DEBUG29(fprintf(stderr, "Reconfiguring existing window 0x%X at nest level %d\n", private->pd[private->win_nest_level].pd_win, private->win_nest_level);)
      DEBUG9(XERRORPOS)
      XConfigureWindow(dspl_descr->display, private->pd[private->win_nest_level].pd_win, window_mask, &changed_window_attrs);
   }


if (private->pd[private->win_nest_level].pd_win != None)
   {
      DEBUG9(XERRORPOS)
      XMapWindow(dspl_descr->display, private->pd[private->win_nest_level].pd_win);

      DEBUG9(XERRORPOS)
      XRaiseWindow(dspl_descr->display, private->pd[private->win_nest_level].pd_win);

      private->win_nest_level++;
      dspl_descr->pd_data->pulldown_active = True;

   }
else
   dm_error("create of pulldown window failed", DM_ERROR_LOG);

DEBUG29(fprintf(stderr, "get_pd_window: window size 0x%X, %dx%d%+d%+d\n", private->pd[private->win_nest_level].pd_win, width, new_pd->height, x, y);)



} /* end of get_pd_window */


/************************************************************************

NAME:      change_pd_font  - Change the font in the pulldown to match the main window.

PURPOSE:    This routine updates the private GC's used in pulldowns
            in response to a font change.

PARAMETERS:
   1.   dspl_descr  - pointer to DISPLAY_DESCR (INPUT)
                      This is the display description for the current
                      display.

   2.   x           - int (INPUT)
                      Position information for the window

   3.   y           - int (INPUT)
                      Position information for the window

   4.   width       - int (INPUT)
                      Position information for the window

   5.   lines       - int (INPUT)
                      Number of lines in the pulldown.  This
                      was already used for sizing the window.

FUNCTIONS :
   1.  Put the size information in the display description for
       the new window.

   2.  If the next window on the stack does not exist, create one.
       if it does, reuse it by resizing it.

   3.  Map the window.

*************************************************************************/

void change_pd_font(Display          *display,
                    PD_DATA          *pd_data,
                    Font              fid)
{
PD_PRIVATE           *private = pd_data->pd_private;
XGCValues             gcvalues;

if (!private || (private->text_gc == None))
   return;

gcvalues.font = fid;

DEBUG9(XERRORPOS)
XChangeGC(display,
          private->text_gc,
          GCFont,
          &gcvalues);


} /* end of get_pd_window */


/************************************************************************

NAME:      draw_menu_bar         - draw the top menu area

PURPOSE:    This routine will Draw the menu bar with the
            File, Edit, Macro, and Help pulldown headers.

PARAMETERS:

   1.  display    -  pointer to Display (Xlib type) (INPUT)
                     This is the display being drawn on.

   2.  x_pixmap   - Pixmap (OUTPUT)
                    This is the backing pixmap which is drawn onto.
                    The pixmap is the size and shape of the main
                    window.  The titlebar, main subarea, dm subareas,
                    and so forth are drawn here.

   3.  hsearch_data -  pointer to void (INPUT)
                       This is the hash table which is used to look up
                       key definitions, aliases, and menu items.
                       It is associated with the current display.

   4.  pd_data    - pointer to PD_DATA (INPUT)
                    This is the pulldown data from the display description.
                    There is a public and private part to this data.


FUNCTIONS :
   1.   Clear out the menu bar to blank it.

   2.   Calculate the y coordinate of the baseline for the text.
        (see note below)

   2.   Calculate the x coordiate of each pulldown.  Throw away
        any which do not fit.

   3.   Write the text strings.


NOTES:

To center the font in the window, the font baseline must be offset from
the center of the window by the amount the baseline of the font is offcenter
from the center of the font.  The following diagram should help explain.

  +---------                        --+
  |                                   |
  |                                   |
  |                                   |
  |                                   |
  |                            ascent |
  |                                   |
  +--------- center of font           | ----+
  |                                   |     |
  |                                   |     | font_offcenter
  +------    baseline of font       --+ ----+
  |                                   |
  |                           descent |
  |                                   |
  +---------                        --+

A similar calculation is done to center the Letters in the window boxes.

*************************************************************************/

void  draw_menu_bar(Display          *display,            /* input   */
#ifdef WIN32
                    Drawable          x_pixmap,
#else
                    Pixmap            x_pixmap,           /* input   */
#endif
                    void             *hsearch_data,       /* input   */
                    PD_DATA          *pd_data)

{
DRAWABLE_DESCR       *menubar_win = &pd_data->menu_bar;
PD_PRIVATE           *private = pd_data->pd_private;
int                   baseline_y;
int                   font_offcenter;
int                   i;
int                   title_start;
char                 *text_list[MAX_BAR_ITEMS];
int                   space;
int                   pix;
int                   item_count;
DMC                  *new_dmc;
char                  msg[128];
int                   done;

if (!private)
   return;

/***************************************************************
*  First clear out the menu bar
***************************************************************/

DEBUG9(XERRORPOS)
XFillRectangle(display,
               x_pixmap,
               menubar_win->reverse_gc,
               menubar_win->x,     menubar_win->y,
               menubar_win->width, menubar_win->height);



/***************************************************************
*  Figure out where the line will be drawn.
***************************************************************/
font_offcenter = menubar_win->font->ascent - ((menubar_win->font->ascent + menubar_win->font->descent) / 2);
baseline_y = (menubar_win->height / 2) + font_offcenter;

title_start = menubar_win->sub_x + MARGIN;
space = XTextWidth(menubar_win->font, " ", 1);


/***************************************************************
*  Find the window elements which work from left to right.
***************************************************************/
done = False;
i = 0;
while(!done)
{
   new_dmc  = get_menu_cmds(hsearch_data,
                            MENUBAR_PULLDOWN_NAME,
                            i,
                            MENU_ITEM_TEXT);  
   if (new_dmc != NULL)
      {
         if (new_dmc->any.cmd == DM_es)
            {
               pix = XTextWidth(menubar_win->font,
                                new_dmc->es.string,
                                strlen(new_dmc->es.string));

               text_list[i] = new_dmc->es.string;
            }
         else
            {
               pix = 0;
               text_list[i] = "???";
            }

         private->bar_l_offset[i] = (i ? (private->bar_r_offset[i-1] + (3*space)) : title_start);
         private->bar_r_offset[i] = private->bar_l_offset[i] + pix;

         i++;
         if (i >= MAX_BAR_ITEMS)
            {
               snprintf(msg, sizeof(msg), "Max of %d Menubar items exceeded, truncating", MAX_BAR_ITEMS);
               dm_error(msg, DM_ERROR_BEEP);
               done = True;
            }
      }
   else
      done = True;
}

/***************************************************************
*  Now add in the right adjusted help pulldown.
***************************************************************/
private->help_bar_pos = -1;
if (i < MAX_BAR_ITEMS)
   {
      pix = XTextWidth(menubar_win->font,
                       "Help", 4);
      private->bar_r_offset[i] = menubar_win->sub_width - MARGIN;
      private->bar_l_offset[i] = private->bar_r_offset[i] - pix;
      text_list[i] = "Help";
      if ((i != 0) && (private->bar_l_offset[i] > private->bar_r_offset[i-1]))
         {
            private->help_bar_pos = i;
            i++;
         }
   }

item_count = i;

/***************************************************************
*  If the window is too small, start throwing away menu items.
*  Help goes away first, then starting at the right.
***************************************************************/
while ((item_count > 0) && (private->bar_l_offset[item_count-1] > (int)(menubar_win->sub_x + menubar_win->sub_width)))
   item_count--;

/***************************************************************
*  
*  Draw the menu bar names
*  
***************************************************************/

for (i = 0; i < item_count; i++)
{  
   DEBUG9(XERRORPOS)
   XDrawString(display,
               x_pixmap,
               menubar_win->gc,
               private->bar_l_offset[i], baseline_y,
               text_list[i], strlen(text_list[i]));
}

private->current_bar_items = item_count;

} /* end draw_menu_bar  */


/************************************************************************

NAME:      draw_pulldown         - draw a pulldown window


PURPOSE:    This routine will Draw the pulldown window.  It needs
            to be called whenever the pulldown is exposed

PARAMETERS:

   1.  display      - pointer to Display (Xlib type) (INPUT)
                      This is the display being drawn on.

   2.  hsearch_data -  pointer to void (INPUT)
                       This is the hash table which is used to look up
                       key definitions, aliases, and menu items.
                       It is associated with the current display.

   3.  nest_level   - int (INPUT)
                      This is the index into the array of pulldown windows
                      of the window to draw.  It is the value returned
                      from  pd_window_to_idx

   4.  pd_data      - pointer to PD_DATA (INPUT)
                      This is the pulldown data from the display description.
                      There is a public and private part to this data.


FUNCTIONS :
   1.   Clear out the menu bar to blank it.

   2.   Calculate the y coordinate of the baseline for the text.
        (see note below)

   2.   Calculate the x coordiate of each pulldown.  Throw away
        any which do not fit.

   3.   Write the text strings.


NOTES:

*************************************************************************/

void  draw_pulldown(Display          *display,            /* input   */
                    void             *hsearch_data,       /* input   */
                    int               nest_level,         /* input   */
                    PD_DATA          *pd_data)            /* input   */
{
PD_PRIVATE           *private = pd_data->pd_private;
int                   baseline_y;
int                   item_count;
DMC                  *new_dmc;
int                   arrow_y;
XPoint                shadow[12];
char                  msg[128];

if (!private)
   return;

if ((nest_level < 0) || (nest_level >= MAX_PD_WINS))
   {
      snprintf(msg, sizeof(msg), "Error: Unexpected nest level (%d) passed to draw_pulldown, internal error", nest_level);
      dm_error(msg, DM_ERROR_LOG);
      return;
   }

/***************************************************************
*  First clear out the pulldown 
***************************************************************/

DEBUG9(XERRORPOS)
XFillRectangle(display,
               private->pd[nest_level].pd_win,
               private->bg_gc,
               0, 0,
               private->pd[nest_level].win_shape.width, private->pd[nest_level].win_shape.height);



/***************************************************************
*  Figure out where the first line will be drawn.
***************************************************************/
baseline_y = private->pd[nest_level].win_shape.sub_y +  private->pd[nest_level].win_shape.font->ascent;


/***************************************************************
*  Draw the lines for each menu item.  The first dmc in the
*  menu item list is an es command with the string to draw.
***************************************************************/
for (item_count = 0; item_count < private->pd[nest_level].lines_in_pulldown;  item_count++)
{
   new_dmc  = get_menu_cmds(hsearch_data,
                            private->pd[nest_level].pd_name,
                            item_count,
                            MENU_ITEM_TEXT);  
   if (new_dmc != NULL)
      {
         /***************************************************************
         *  Draw the line in the menu item unless it is zero length.
         *  In that case skip it.  To get a blank line, code the string
         *  to contain a blank.
         ***************************************************************/
         if (new_dmc->any.cmd == DM_es)
            if (new_dmc->es.string[0] != '\0') 
               XDrawString(display, private->pd[nest_level].pd_win, private->text_gc,
                           private->pd[nest_level].win_shape.sub_x,
                           baseline_y,
                           new_dmc->es.string, strlen(new_dmc->es.string));
            else
               continue;

         if ((new_dmc->next)->any.cmd == DM_pd)
            {
               arrow_y = (baseline_y - private->pd[nest_level].win_shape.font->ascent) + (private->pd[nest_level].win_shape.line_height / 2);
               /***************************************************************
               *  Draw the arrow to show a next level pulldown
               ***************************************************************/
               shadow[0].x = private->pd[nest_level].win_shape.width - ((ARROW_WIDTH + PD_SHADOW_LINE_WIDTH));
               shadow[0].y = arrow_y;
               shadow[1].x = ARROW_WIDTH;
               shadow[1].y = 0;
               shadow[2].x = -5;
               shadow[2].y = (private->pd[nest_level].win_shape.line_height / 2) - 1;
               shadow[3].x = 5;
               shadow[3].y = -((private->pd[nest_level].win_shape.line_height / 2) - 1);
               shadow[4].x = -5;
               shadow[4].y = -((private->pd[nest_level].win_shape.line_height / 2) - 1);

               DEBUG9(XERRORPOS)
               XDrawLines(display,
                          private->pd[nest_level].pd_win,
                          private->darkline_gc,
                          shadow,
                          5, /* number of points */
                          CoordModePrevious);

            }

         baseline_y += private->pd[nest_level].win_shape.line_height;
      }
}

/***************************************************************
*  If the line was highlighted before the draw, rehighlight it.
***************************************************************/
if ((private->current_cursor_w_idx == nest_level) &&
    (private->current_cursor_menu_item >= 0))
   highlight_line(nest_level, 
                  private->current_cursor_menu_item,
                  True,  /*  highlight_on */
                  private,
                  display);


} /* end draw_pulldown  */


/************************************************************************

NAME:      get_private_data - Get the pulldow private data

PURPOSE:    This routine returns the existing dmwin private area
            from the display description or initializes a new one.

PARAMETERS:
   1.  pd_data -  pointer to PD_DATA (in buffer.h)
                  This is where the private data for this module
                  is hung.

FUNCTIONS :

   1.   If the private area has already been allocated, return it.

   2.   Malloc a new area.  If the malloc fails, return NULL

   3.   Initialize the new private area and return it.

RETURNED VALUE
   private  -  pointer to PD_PRIVATE
               This is the private static area used by the
               routines in this module.
               NULL is returned on malloc failure only.

*************************************************************************/

static PD_PRIVATE *get_private_data(PD_DATA  *pd_data)
{
PD_PRIVATE    *private;

if (pd_data->pd_private)
   return((PD_PRIVATE *)pd_data->pd_private);

private = (PD_PRIVATE *)CE_MALLOC(sizeof(PD_PRIVATE));
if (!private)
   return(NULL);   
else
   {
      memset((char *)private, 0, sizeof(PD_PRIVATE));
      private->current_cursor_w_idx = -1;
      private->current_cursor_menu_item = -1;
   }

pd_data->pd_private = (void *)private;

return(private);

} /* end of get_private_data */



/************************************************************************

NAME:      pd_adjust_motion      - Adjust motion events when pulldown is active

PURPOSE:    This when a button is held down, event positions are all
            relative to that window.  This routine figures out what window
            the event is in and adjusts it accordingly.  Only the main window
            and the pulldown menu's are used.

PARAMETERS:
   1.   event       - pointer to XEvent (INPUT/OUTPUT)
                      This is the event to be examined.
                      This is expected to be a button event or a motion event.

FUNCTIONS :
   1.  Switch on the event type.

   2.  Return the appropriate X, Y, and window values.

*************************************************************************/

void  pd_adjust_motion(XEvent            *event,
                       PAD_DESCR         *main_pad,
                       PD_DATA           *pd_data)
{
PD_PRIVATE           *private;
char                  msg[512];
int                   x;
int                   y;
int                   i;
Window                w = None;

/***************************************************************
*  Access the private data.
***************************************************************/
private = get_private_data(pd_data);
if (!private) 
   return; /* message about out of memory already output */

switch (event->type)
{
case KeyPress:
case KeyRelease:
   x = event->xkey.x_root;
   y = event->xkey.y_root;
   break;
case ButtonPress:
case ButtonRelease:
   x = event->xbutton.x_root;
   y = event->xbutton.y_root;
   break;
case MotionNotify:
   x = event->xmotion.x_root;
   y = event->xmotion.y_root;
   break;
case EnterNotify:
case LeaveNotify:
   x = event->xcrossing.x_root;
   y = event->xcrossing.y_root;
   break;
default:
   snprintf(msg, sizeof(msg), "Error: Unexpected event type (%d) passed to pd_adjust_motion, internal error", event->type);
   dm_error(msg, DM_ERROR_LOG);
   return;
}

/***************************************************************
*  First check the list of popup windows
***************************************************************/
for (i = private->win_nest_level-1; i >= 0; i--)
   if (((int)(private->pd[i].win_shape.x + private->pd[i].win_shape.width) < x) || (private->pd[i].win_shape.x > x))
      continue;
   else
      if (((int)(private->pd[i].win_shape.y + private->pd[i].win_shape.height) < y) || (private->pd[i].win_shape.y > y))
         continue;
      else
         break;

/***************************************************************
*  Check the main window if we did not find it in a pull down.
*  If we have a match in the main window or a pull down, assign
*  the window and adjust the x and y values to be relative to
*  that window.
***************************************************************/
DEBUG29(fprintf(stderr, "pd_adjust_motion: Window index %d\n", i);)
if (i < 0) /* not found */
   {
      w =  main_pad->x_window;
      x -= main_pad->window->x;
      y -= main_pad->window->y;

      if (((int)(pd_data->menu_bar.x + pd_data->menu_bar.width)  > x) && (pd_data->menu_bar.x <= x) &&
          ((int)(pd_data->menu_bar.y + pd_data->menu_bar.height) > y) && (pd_data->menu_bar.y <= y))
         {
            w =  pd_data->menubar_x_window;
            x -= pd_data->menu_bar.x;
            y -= pd_data->menu_bar.y;
         }
   }
else
   {
      w = private->pd[i].pd_win;
      x -= private->pd[i].win_shape.x;
      y -= private->pd[i].win_shape.y;
   }

DEBUG29(if (w != event->xbutton.window) fprintf(stderr, "pd_adjust_motion: Window adjustment made 0x%X to 0x%X\n", event->xbutton.window, w);)

/***************************************************************
*  If we had a match, we set the window (w).  Put the data back.
***************************************************************/
if (w != None)
   switch (event->type)
   {
   case KeyPress:
   case KeyRelease:
      event->xkey.x = x;
      event->xkey.y = y;
      event->xkey.window = w;
      break;
   case ButtonPress:
   case ButtonRelease:
      event->xbutton.x = x;
      event->xbutton.y = y;
      event->xbutton.window = w;
      break;
   case MotionNotify:
      event->xmotion.x = x;
      event->xmotion.y = y;
      event->xmotion.window = w;
      break;
   case EnterNotify:
   case LeaveNotify:
      event->xcrossing.x = x;
      event->xcrossing.y = y;
      event->xcrossing.window = w;
      break;
   default:
      snprintf(msg, sizeof(msg), "Error: Very unexpected event type (%d) passed to pd_adjust_motion, internal error", event->type);
      dm_error(msg, DM_ERROR_LOG);
      break;
   }


} /* end of pd_adjust_motion */


/************************************************************************

NAME:      pd_button - Process a button press or release when pulldowns are active


PURPOSE:    This routine handles button press'es and releases when pulldowns are
            active.  Pulldowns are active when either the mouse cursor is in the
            menu bar area or a pulldown window is displayed.  In either of these
            two cases, the flag pulldown_active in PD_DATA is set.

PARAMETERS:
   1.   event       - pointer to XEvent (INPUT)
                      This is the event to be examined.
                      This is expected to be a button event.

   2.  dspl_descr   - pointer to DISPLAY_DESCR (INPUT/OUTPUT)
                      This is the display description for the display
                      being processed.  It is the root structure for
                      all the windows and sub-windows.

FUNCTIONS :
   1.   Determine whether this is a button press or a button release and
        that pulldowns are indeed active.  Also determine which menu we
        are in.

   2.   For a button release, check if this is the special "sent" event
        which occurs when the button his held down.  If so, put up the
        next pulldown. (may not need to detect this as a special case)

   3.   For a button press or release, see if we have to take any windows down.

   4.   For button release, check if we are in the menu area.  If so, put
        up the appropriate pulldown if any.  If not, just return.
        If we are in a pulldown window, execute the commands that we find
        there.

   5.   For a button press, highlight the menu item and schedule
        a fake button release for 1/2 second from now if the mouse does not
        move out of the menu item and the first executable command
        associated with the menu item is a pulldown command.  Note that the
        first command in a menu item is usually an 'es' command with the
        label for the menu item.  So we really look at the first two commands.

   6.   Set the flag to show a popup is active.

RETURNED VALUE:
   new_dmc    -   pointer to DMC
                  If a button release causes commands to be executed, return
                  the list.  Otherwise return null.

*************************************************************************/

DMC *pd_button(XEvent           *event,            /* input  */
               DISPLAY_DESCR    *dspl_descr)       /* input / output */
{
PD_DATA              *pd_data = dspl_descr->pd_data;
PD_PRIVATE           *private;
char                  msg[512];
int                   i;
DMC                  *new_dmc = NULL;

/***************************************************************
*  Access the private data.
***************************************************************/
private = get_private_data(pd_data);
if (!private) 
   return(NULL); /* message about out of memory already output */

if ((event->type != ButtonPress) && (event->type != ButtonRelease))
   {
      snprintf(msg, sizeof(msg), "Internal error:  Unexpected event type %d passed to pd_button", event->type);
      dm_error_dspl(msg, DM_ERROR_LOG, dspl_descr);
      return(NULL);
   }

if (!pd_data->pulldown_active && (event->xmotion.window != pd_data->menubar_x_window))
   {
      snprintf(msg, sizeof(msg), "Internal error:  pd_button called when pulldowns not active", event->type);
      dm_error_dspl(msg, DM_ERROR_LOG, dspl_descr);
      return(NULL);
   }

/***************************************************************
*  Adust the motion for where we really are.
***************************************************************/
pd_adjust_motion(event, dspl_descr->main_pad, pd_data);

/***************************************************************
*  Which window are we in?  -1 is returned on no match for
*  the pulldown menus.  Thus it must be in the menu bar area
*  since pulldowns are active.
***************************************************************/
private->current_cursor_w_idx = pd_window_to_idx(pd_data, event->xbutton.window);
DEBUG29(fprintf(stderr, "pd_button: window index = %d\n", private->current_cursor_w_idx);)
if (private->current_cursor_w_idx >= 0)
   {
      /***************************************************************
      *  We are in a pulldown window.  Figure out which line we
      *  are on.
      ***************************************************************/
      private->current_cursor_menu_item = (event->xbutton.y - private->pd[private->current_cursor_w_idx].win_shape.sub_y) /
                                          private->pd[private->current_cursor_w_idx].win_shape.line_height;
      /* make sure we have a valid entry */
      if (private->current_cursor_menu_item < 0)
         private->current_cursor_menu_item = 0;
      else
         if (private->current_cursor_menu_item >= private->pd[private->current_cursor_w_idx].lines_in_pulldown)
            private->current_cursor_menu_item = private->pd[private->current_cursor_w_idx].lines_in_pulldown - 1;

      if (event->type == ButtonPress)
         {
            /***************************************************************
            *  For a button press, drop any extra pulldowns.
            ***************************************************************/
            if (private->win_nest_level > private->current_cursor_w_idx+1)
               pd_remove(dspl_descr, private->current_cursor_w_idx+1);

            /***************************************************************
            *  Button press in a pulldown item
            *  If there is a submenu below this one, activate it.
            *  Otherwise highlight it and schedule a fake button release.
            ***************************************************************/
            new_dmc = get_menu_cmds(dspl_descr->hsearch_data,
                                    private->pd[private->current_cursor_w_idx].pd_name,
                                    private->current_cursor_menu_item,
                                    MENU_ITEM_PD_ONLY);

            highlight_line(private->current_cursor_w_idx, 
                           private->current_cursor_menu_item,
                           True,  /*  highlight_on */
                           private,
                           dspl_descr->display);
         }
      else
         {
            /***************************************************************
            *  Button release in a pulldown item.
            *  For pulldowns, the pulldown is already done.
            ***************************************************************/
            new_dmc = get_menu_cmds(dspl_descr->hsearch_data,
                                    private->pd[private->current_cursor_w_idx].pd_name,
                                    private->current_cursor_menu_item,
                                    MENU_ITEM_CMDS);

            if (new_dmc && (new_dmc->any.cmd == DM_pd))
               {
                  new_dmc = NULL;

                  highlight_line(private->current_cursor_w_idx, 
                                 private->current_cursor_menu_item,
                                 False,  /*  highlight_off */
                                 private,
                                 dspl_descr->display);
#ifdef grabpointer
                  DEBUG9(XERRORPOS)
                  XGrabPointer(dspl_descr->display,
                               private->pd[private->current_cursor_w_idx].pd_win,
                               False, /* pass events only to this window */
                               ButtonPressMask | ButtonReleaseMask,    /* no event mask, only button push/release */
                               GrabModeAsync,
                               GrabModeAsync,
                               None,
                               None,  /* same cursor as we had */
                               CurrentTime);
#endif
               }
            else
               {
                  pd_remove(dspl_descr, 0);
#ifdef grabpointer
                  DEBUG9(XERRORPOS)
                  XUngrabPointer(dspl_descr->display, CurrentTime);
#endif
               }

            pd_data->pd_microseconds = 0; /* cancel any pending button push */
            /* possibly clear the timeout if nothing else was going on */
            timeout_set(dspl_descr); /* in timeout.c */

         }
   }
else
   /***************************************************************
   *  See if we are in the menubar.
   ***************************************************************/
   if (event->xbutton.window == pd_data->menubar_x_window)
      {
         /***************************************************************
         *  We are in the menubar, see if we are in a menu item.
         ***************************************************************/
         private->current_cursor_menu_item = -1;
         for (i = 0; i < private->current_bar_items && private->current_cursor_menu_item < 0; i++)
            if ((event->xbutton.x >= private->bar_l_offset[i]) &&
                (event->xbutton.x <  private->bar_r_offset[i]))
               private->current_cursor_menu_item = i;

         DEBUG29(fprintf(stderr, "pd_button: menubar menu index = %d\n", private->current_cursor_menu_item);)

         if (event->type == ButtonPress)
            {
               /***************************************************************
               *  Get rid of any extra windows.
               ***************************************************************/
               if (private->win_nest_level > 0)
                  pd_remove(dspl_descr, 0);

               /***************************************************************
               *  We are in a menu item.  Put up the pulldown.  Detect the
               *  right adjusted "Help" pulldown as a special case.
               ***************************************************************/
               if ((private->help_bar_pos != -1) && (private->help_bar_pos == private->current_cursor_menu_item))
                  {
                     new_dmc = (DMC *)CE_MALLOC(sizeof(DMC));
                     if (new_dmc)
                        {
                           memset((char *)new_dmc, 0, sizeof(DMC));
                           new_dmc->next  = NULL;
                           new_dmc->pd.cmd   = DM_pd;
                           new_dmc->pd.temp  = True;
                           new_dmc->pd.parm  = malloc_copy("Help");
                           if (!new_dmc->pd.parm)
                              {
                                 free((char *)new_dmc);
                                 new_dmc = NULL;
                              }
                        }
                  }
               else
                  new_dmc = get_menu_cmds(dspl_descr->hsearch_data,
                                       MENUBAR_PULLDOWN_NAME,
                                       private->current_cursor_menu_item,
                                       MENU_ITEM_PD_ONLY);
            }
         else
            {
               /***************************************************************
               *  Button release in a the menu bar.  User could have stored
               *  a non-pulldown here.
               ***************************************************************/
               new_dmc = get_menu_cmds(dspl_descr->hsearch_data,
                                       MENUBAR_PULLDOWN_NAME,
                                       private->current_cursor_menu_item,
                                       MENU_ITEM_CMDS);

               if (new_dmc && (new_dmc->any.cmd == DM_pd))
                  {
                     new_dmc = NULL;
#ifdef grabpointer
                     DEBUG9(XERRORPOS)
                     XGrabPointer(dspl_descr->display,
                                  pd_data->menubar_x_window,
                                  False, /* pass events only to this window */
                                  ButtonPressMask | ButtonReleaseMask,    /* no event mask, only button push/release */
                                  GrabModeAsync,
                                  GrabModeAsync,
                                  None,
                                  None,  /* same cursor as we had */
                                  CurrentTime);
#endif
                  }
#ifdef grabpointer
               else
                  {
                     DEBUG9(XERRORPOS)
                     XUngrabPointer(dspl_descr->display, CurrentTime);
                  }
#endif

               pd_data->pd_microseconds = 0; /* cancel any pending button push */
               /* possibly clear the timeout if nothing else was going on */
               timeout_set(dspl_descr); /* in timeout.c */
            }
      }
   else
      {
         /***************************************************************
         *  Not in a pulldown or the menu
         *  Get rid of any extra windows.
         ***************************************************************/
         pd_remove(dspl_descr, 0);
#ifdef grabpointer
         DEBUG9(XERRORPOS)
         XUngrabPointer(dspl_descr->display, CurrentTime);
#endif
      }

return(new_dmc);

} /* end of pd_button */


/************************************************************************

NAME:      get_menu_cmds - Retrieve menu item commands from hash table


PURPOSE:    This routine gets the DMC list from the hash table for
            menu items.

PARAMETERS:
   1.  hsearch_data -  pointer to void (INPUT)
                       This is the hash table which is used to look up
                       key definitions, aliases, and menu items.
                       It is associated with the current display.

   2.  pd_name      -  pointer to char (INPUT)
                       This is the name of the pulldown we are working with.
                       It is used to look up the menu item.

   3.  menu_item    -  int (INPUT)
                       This is the line in the pulldown.  It translates to the
                       numeric portion of the key used to find the menu item.

   4.  type          - int (INPUT)
                       values:  
                       MENU_ITEM_TEXT    -  Return pointer to the leading 'es' dmc
                       MENU_ITEM_CMDS    -  Return pointer to item past leading 'es' dmc
                       MENU_ITEM_PD_ONLY -  Return pointer to the pulldown immediately after
                                            the leading 'es' dmc.  If not a pull down,       
                                            return NULL
                       Literals defined at the top of this module

FUNCTIONS : 
   1.   Build the key for the event and look it up.

   2.   If the first item is a 'es' and the second is a pulldown
        skip over the 'es'.  It is the menu title for the menu item
        which invokes the pulldown.
        
   3.   If we are only interested in pulldowns and this is not a pulldown,
        set the new dmc list to null.

   4.   Return the list of dm command structures.

RETURNED VALUE:
   new_dmc    -   pointer to DMC
                  These are the DMC's associated with the menu item.
                  NULL can be returned.  If asking for a pulldown and
                  this is not a pulldown menu, NULL is returned.
                  For pulldown menu items, the 'es' with the menu item
                  string is never returned.

*************************************************************************/

static DMC *get_menu_cmds(void                 *hsearch_data,
                          char                 *pd_name,
                          int                   menu_item,
                          int                   type)  
{
DMC                  *new_dmc;
ENTRY                *def_found;
ENTRY                 sym_to_find;
char                  the_key[KEY_KEY_SIZE];
char                  msg[512];

snprintf(the_key, sizeof(the_key), MI_KEY_FORMAT, pd_name, menu_item, MI_HASH_TYPE);
DEBUG29(fprintf(stderr, "get_menu_cmds:  key \"%s\"\n", the_key);)
sym_to_find.key = the_key;
def_found = (ENTRY *)hsearch(hsearch_data, sym_to_find, FIND);
if (def_found != NULL)
   {
      new_dmc = (DMC *)def_found->data;
      if ((new_dmc != NULL) && (new_dmc != DMC_COMMENT))
         {
            if (new_dmc->any.cmd != DM_es)
               {
                  snprintf(msg, sizeof(msg), "First menu item command not a text string  %s/%d", pd_name, menu_item);
                  dm_error(msg, DM_ERROR_BEEP);
               }
            else
               if (type != MENU_ITEM_TEXT)
                  new_dmc = new_dmc->next;

            if ((type == MENU_ITEM_PD_ONLY) && new_dmc && (new_dmc->any.cmd != DM_pd))
               new_dmc = NULL;
         }
      else
         new_dmc = NULL;
   }
else
   {
      DEBUG29(fprintf(stderr, "get_menu_cmds: Could not find menu item for \"%s/%d\"\n", pd_name, menu_item); )
      new_dmc = NULL;
   }

return(new_dmc);

} /* end of get_menu_cmds */


/************************************************************************

NAME:      pd_in_list - Check for pulldown command in list of DMC's


PURPOSE:    This routine scans a linked list of DMC's to see if there
            is a PD command in it.

PARAMETERS:
   1.  dmc          -  pointer to DMC (INPUT)
                       This is the first element in the list to be scanned.

FUNCTIONS : 
   1.   Walk the list looking for a pulldown DMC

RETURNED VALUE:
   Found     -   int
                 True  - There is a pulldown DMC in the list
                 False - There is NOT a pulldown DMC in the list.

*************************************************************************/

static int pd_in_list(DMC   *dmc)
{
while(dmc)
   if (dmc->any.cmd == DM_pd)
      return(True);
   else
      dmc = dmc->next;

return(False);

}


/************************************************************************

NAME:      pd_button_motion - Process button motion events when pulldowns are active


PURPOSE:    This routine handles motion events when a button is pressed down
            and the events are in the menu area or a pulldown window.
            It is called in place of the normal event processing.

PARAMETERS:
   1.   event       - pointer to XEvent (INPUT)
                      This is the event to be examined.
                      This is expected to be a motion event.

   2.  dspl_descr   - pointer to DISPLAY_DESCR (INPUT/OUTPUT)
                      This is the display description for the display
                      being processed.  It is the root structure for
                      all the windows and sub-windows.

FUNCTIONS : 
   1.   The event has been adjusted to determine that this is in the
        menu area or a pulldown.  Determine which window it is in.

   2.   Determine which line on which menu the button is on.  If the line
        has changed since the last one, move the highlighting to that
        line.

   3.   If the menu item changed and there is a pending fake button up
        event, cancel it.

   2.   If the menu item changed, schedule the fake button up event to
        cause the next pulldown to be activated.  Only do this if the
        current menu item has a pulldown associated with it.


*************************************************************************/

void  pd_button_motion(XEvent             *event,            /* input  */
                       DISPLAY_DESCR      *dspl_descr)       /* input / output */
{
PD_DATA              *pd_data = dspl_descr->pd_data;
PD_PRIVATE           *private = dspl_descr->pd_data->pd_private;
int                   new_cursor_w_idx = -1;
int                   new_cursor_menu_item = -1;
DMC                  *new_dmc;
char                  msg[256];
int                   i;

/***************************************************************
*  Can't have a pulldown if we have never initialized anything.
***************************************************************/
if (!private)
   return;

if (event->type != MotionNotify)
   {
      snprintf(msg, sizeof(msg), "Internal error:  Unexpected event type %d passed to pd_button_motion", event->type);
      dm_error_dspl(msg, DM_ERROR_LOG, dspl_descr);
      return;
   }

if (!pd_data->pulldown_active && (event->xmotion.window != pd_data->menubar_x_window))
   {
      snprintf(msg, sizeof(msg), "Internal error:  pd_button_motion called when pulldowns not active");
      dm_error_dspl(msg, DM_ERROR_LOG, dspl_descr);
      return;
   }

/***************************************************************
*  Adust the motion for where we really are.
***************************************************************/
pd_adjust_motion(event, dspl_descr->main_pad, pd_data);

/***************************************************************
*  See if we are in the menubar window.  If so, process it as
*  such.
***************************************************************/
if (event->xbutton.window == pd_data->menubar_x_window)
   {
      /***************************************************************
      *  We are in the main window, see if we are in a menu item
      ***************************************************************/
      if (event->xbutton.window == pd_data->menubar_x_window)
         for (i = 0; i < private->current_bar_items && new_cursor_menu_item < 0; i++)
            if ((event->xbutton.x >= private->bar_l_offset[i]) &&
                (event->xbutton.x <  private->bar_r_offset[i]))
               new_cursor_menu_item = i;

      /***************************************************************
      *  Get rid of the old highlighting if any.
      ***************************************************************/
      if ((private->current_cursor_w_idx >= 0) && (private->current_cursor_menu_item >= 0))
         highlight_line(private->current_cursor_w_idx, 
                        private->current_cursor_menu_item,
                        False,  /*  highlight_off */
                        private,
                        dspl_descr->display);

      if ((private->current_cursor_menu_item != new_cursor_menu_item) ||
          (private->current_cursor_w_idx     != new_cursor_w_idx))
         {
            DEBUG29(
               fprintf(stderr, "pd_button_motion(a): new window index = %d, old window index = %d, new menu index = %d, old menu index = %d\n",
                                new_cursor_w_idx, private->current_cursor_w_idx, new_cursor_menu_item, private->current_cursor_menu_item);
            )
            /***************************************************************
            *  Get rid of any extra pulldowns since we are within the title
            *  bar area.
            ***************************************************************/
            if (private->win_nest_level > 0)
               pd_remove(dspl_descr, 0);

            /***************************************************************
            *  If the mouse is over a name, push the button
            ***************************************************************/
            if (new_cursor_menu_item >= 0)
               sched_button_push(event, pd_data);

            private->current_cursor_menu_item = new_cursor_menu_item;
            private->current_cursor_w_idx     = new_cursor_w_idx;
         }

   }
else
   {
      /***************************************************************
      *  Which window are we in?  -1 is returned on no match for
      *  the pulldown menus.  This would be either the menu bar
      *  or someplace away from menubar processing.
      ***************************************************************/
      new_cursor_w_idx = pd_window_to_idx(pd_data, event->xbutton.window);
      if (new_cursor_w_idx >= 0)
         {
            new_cursor_menu_item = (event->xbutton.y - private->pd[new_cursor_w_idx].win_shape.sub_y) /
                                   private->pd[new_cursor_w_idx].win_shape.line_height;

            /* make sure we have a valid entry */
            if (new_cursor_menu_item < 0)
               new_cursor_menu_item = 0;
            else
               if (new_cursor_menu_item >= private->pd[new_cursor_w_idx].lines_in_pulldown)
                  new_cursor_menu_item = private->pd[new_cursor_w_idx].lines_in_pulldown - 1;
         }

      /***************************************************************
      *  See if we changed area in the pulldown.
      ***************************************************************/
      if ((new_cursor_w_idx     != private->current_cursor_w_idx)      ||
          (new_cursor_menu_item != private->current_cursor_menu_item))
         {
            DEBUG29(
               fprintf(stderr, "pd_button_motion(b): new window index = %d, old window index = %d, new menu index = %d, old menu index = %d\n",
                                new_cursor_w_idx, private->current_cursor_w_idx, new_cursor_menu_item, private->current_cursor_menu_item);
            )

            /***************************************************************
            *  Get rid of any extra pulldowns unless we moved outside the.
            *  pulldown window.
            ***************************************************************/
            if ((new_cursor_w_idx >= 0) && (private->win_nest_level > new_cursor_w_idx))
               pd_remove(dspl_descr, new_cursor_w_idx+1);

            /***************************************************************
            *  See if there is a pulldown under this menu.  If so schedule
            *  a button press to activate the menu.
            ***************************************************************/
            if ((new_cursor_w_idx >= 0) && (new_cursor_menu_item >= 0))
               new_dmc = get_menu_cmds(dspl_descr->hsearch_data,
                                       private->pd[new_cursor_w_idx].pd_name,
                                       new_cursor_menu_item,
                                       MENU_ITEM_PD_ONLY);
            else
               new_dmc = NULL; /*DMC_COMMENT??*/

            if (new_dmc)
               sched_button_push(event, pd_data);

            /***************************************************************
            *  Move the highlighting
            ***************************************************************/
            if ((private->current_cursor_w_idx >= 0) && (private->current_cursor_menu_item >= 0))
               highlight_line(private->current_cursor_w_idx, 
                              private->current_cursor_menu_item,
                              False,  /*  highlight_off */
                              private,
                              dspl_descr->display);

            if ((new_cursor_w_idx >= 0) && (new_cursor_menu_item >= 0))
               highlight_line(new_cursor_w_idx, 
                              new_cursor_menu_item,
                              True,  /*  highlight_on */
                              private,
                              dspl_descr->display);

            private->current_cursor_menu_item = new_cursor_menu_item;
            private->current_cursor_w_idx     = new_cursor_w_idx;
         }
   }

} /* end of pd_button_motion */


/************************************************************************

NAME:      highlight_line        - Highlight or Un-Highlight a Pulldown line.


PURPOSE:    This routine draws the highlight around a line in a pull
            down to make it looked depressed via shadowing.  It also
            erases these lines.

PARAMETERS:
   1.  nest_level    - int (INPUT)
                       This identifies the pulldown to be acted upon.

   2.  lineno        - int (INPUT)
                       This is the line within the pulldown to be acted upon.

   3.  highlight_on  - int (INPUT)
                       This flag is True to turn the highlight on and False
                       to turn it off.

   4.  private       - pointer to PD_PRIVATE (INPUT/OUTPUT)
                       This is the description of the data which is private
                       within this module.  It describes the windows to be
                       acted uopn.

   5.  display       - pointer to Display (Xlib type) (INPUT)
                       This is the current display pointer.

FUNCTIONS : 
   1.   Set the gc's to use based on whether we are turning the highlight
        on or off.

   2.   Draw the two lines to do the highlighting.


*************************************************************************/

static void  highlight_line(int                 nest_level,    /* input  */
                            int                 lineno,        /* input  */
                            int                 highlight_on,  /* input  */
                            PD_PRIVATE         *private,       /* input / output */
                            Display            *display)       /* input  */
{
XPoint                shadow[12];
GC                    gc1;
GC                    gc2;
int                   top_y;
int                   bottom_y;

/***************************************************************
*  If we are in the menu area (nest level == -1, do nothing.
***************************************************************/
if (nest_level < 0)
   return;

/***************************************************************
*  Determine which GC's to use for drawing.
***************************************************************/
if (highlight_on)
   {
      DEBUG29(fprintf(stderr, "highlight_line: ON, nest %d, line %d\n", nest_level, lineno);)
      gc1 = private->lightline_gc;
      gc2 = private->darkline_gc;
   }
else
   {
      DEBUG29(fprintf(stderr, "highlight_line: OFF, nest %d, line %d\n", nest_level, lineno);)
      gc1 = private->bg_gc;
      gc2 = private->bg_gc;
   }

top_y    = (private->pd[nest_level].win_shape.line_height * lineno) + private->pd[nest_level].win_shape.sub_y;
bottom_y = (top_y + private->pd[nest_level].win_shape.line_height)  - PD_SHADOW_LINE_WIDTH;

/***************************************************************
*  The line for the bottom right edge.
***************************************************************/
shadow[0].x = 0;
shadow[0].y = bottom_y;
shadow[1].x = private->pd[nest_level].win_shape.width - PD_SHADOW_LINE_WIDTH;
shadow[1].y = bottom_y;
shadow[2].x = private->pd[nest_level].win_shape.width - PD_SHADOW_LINE_WIDTH;
shadow[2].y = top_y;

DEBUG9(XERRORPOS)
XDrawLines(display,
           private->pd[nest_level].pd_win,
           gc1,
           shadow,
           3, /* number of points */
           CoordModeOrigin);

/***************************************************************
*  The line for the top left edge
***************************************************************/
shadow[0].x = 0;
shadow[0].y = bottom_y;
shadow[1].x = 0;
shadow[1].y = top_y;
shadow[2].x = private->pd[nest_level].win_shape.width - PD_SHADOW_LINE_WIDTH;
shadow[2].y = top_y;

DEBUG9(XERRORPOS)
XDrawLines(display,
           private->pd[nest_level].pd_win,
           gc2,
           shadow,
           3, /* number of points */
           CoordModeOrigin);


} /* end of highlight_line */


/************************************************************************

NAME:   sched_button_push     - Schedule a delayed button push to pop up dependant pulldowns.


PURPOSE:    This routine schedules a button press which is processed by
            timeout_set (timeout.c).  The button press cycles back to
            this routine to cause a pull down to appear.

PARAMETERS:
   1.  event         - pointer to XEvent (Xlib type)(INPUT)
                       This motion event is used to build the button press.

   2.  pd_data -  pointer to PD_DATA (in buffer.h)
                  This is where the visible data about pulldowns is placed
                  in the display description.

FUNCTIONS : 
   1.   Copy the fields to build the button press out of the motion
        event.

   2.   Set the timeout value in the pd data so the event is scheduled.


*************************************************************************/

static void  sched_button_push(XEvent             *event,
                               PD_DATA           *pd_data)             /* input / output */
{

pd_data->button_push.xbutton.type        = ButtonPress;
pd_data->button_push.xbutton.serial      = event->xmotion.serial;
pd_data->button_push.xbutton.send_event  = True;
pd_data->button_push.xbutton.display     = event->xmotion.display;
pd_data->button_push.xbutton.window      = event->xmotion.window;
pd_data->button_push.xbutton.root        = event->xmotion.root;
pd_data->button_push.xbutton.subwindow   = event->xmotion.subwindow;
pd_data->button_push.xbutton.time        = event->xmotion.time;
pd_data->button_push.xbutton.x           = event->xmotion.x;
pd_data->button_push.xbutton.y           = event->xmotion.y;
pd_data->button_push.xbutton.x_root      = event->xmotion.x_root;
pd_data->button_push.xbutton.y_root      = event->xmotion.y_root;
pd_data->button_push.xbutton.state       = event->xmotion.state;
pd_data->button_push.xbutton.button      = 1;  /* show left mouse button */
pd_data->button_push.xbutton.same_screen = event->xmotion.same_screen;

pd_data->pd_microseconds = 100 * 1000; /* .3 second delay */


} /* end of sched_button_push */


/************************************************************************

NAME:      check_menu_items - Make sure certain menu items exist

PURPOSE:    This routine make sure that there are menu items defined
            for the basic pulldowns.  It looks up each menu item
            and adds it if it does not exist.

PARAMETERS:
   1.  dspl_descr   - pointer to DISPLAY_DESCR (INPUT)
               This is the display description for the display
               being processed.  It is the root structure for
               all the windows and sub-windows.

FUNCTIONS :
   1.   Loop through the menu item list and make sure there is some
        definition for each one.

   2.   If a keysym is undefined, force the definition.

OUTPUTS:
        The table of key defintions is modified both in ths program and in the server.

NOTES:
   1.   This routine is called after all the keysyms have been read in.

*************************************************************************/

typedef struct {
   char        *pd_name;
   int          idx;
   char        *defn;
} DEF_MI;

static DEF_MI   forced_mi[] = {"File",      0,  "mi File/0 'Save'pw;msg 'File saved' ke",
                               "File",      1,  "mi File/1 'Save As'pn -c -r &'New Name:';ro -off;pw ke",
                               "File",      2,  "mi File/2 'Close/Save'wc ke",
                               "File",      3,  "mi File/3 'Close/NoSave'wc -q ke",
                               "File",      4,  "mi File/4 'Icon'icon ke",
                               "File",      5,  "mi File/5 ' ' ke",
                               "File",      6,  "mi File/6 'Exit'wc -f ke",

                               "Edit",      0,  "mi Edit/0 'Copy'xc ke",
                               "Edit",      1,  "mi Edit/1 'Cut'xd ke",
                               "Edit",      2,  "mi Edit/2 'Paste'xp ke",
                               "Edit",      3,  "mi Edit/3 'Undo'undo ke",
                               "Edit",      4,  "mi Edit/4 'Redo'redo ke",
                               "Edit",      5,  "mi Edit/5 'Select All'1,$echo ke",
                               "Edit",      6,  "mi Edit/6 'Text Flow'tf ke",
                               "Edit",      7,  "mi Edit/7 'Find Matching Delim'bl -d ke",

                               "Modes",     0,  "mi Modes/0 'Case'pd Case ke",
                               "Modes",     1,  "mi Modes/1 'Insert'pd Insert ke",
                               "Modes",     2,  "mi Modes/2 'hex'pd Hex ke",
                               "Modes",     3,  "mi Modes/3 'Lineno'pd Lineno ke",
                               "Modes",     4,  "mi Modes/4 'ScrollBar'pd ScrollBr ke",
                               "Modes",     5,  "mi Modes/5 'Vt100'pd vt ke",
                               "Modes",     6,  "mi Modes/6 'Track Mouse'pd Mouse ke",
                               "Modes",     7,  "mi Modes/7 'Read-Write'ro ke",

                               "Macro",     0,  "mi Macro/0 'No Macros Defined' ke",

                               "Help",      0,  "mi Help/0 'Menubar'cv -envar y $CEHELPDIR/menubarCon.hlp ke",
                               "Help",      1,  "mi Help/1 'Intro'cv -envar y $CEHELPDIR/.hlp ke",
                               "Help",      2,  "mi Help/2 'Commands'cv -envar y $CEHELPDIR/commands.hlp ke",
                               "Help",      3,  "mi Help/3 'options'cv -envar y $CEHELPDIR/xresources.hlp ke",

                               "Case",      0,  "mi Case/0 'Sensitive find'sc -on ke",
                               "Case",      1,  "mi Case/1 'Insensitive find'sc -off ke",

                               "Insert",    0,  "mi Insert/0 'Insert'ei -on ke",
                               "Insert",    1,  "mi Insert/1 'Overstrike'ei -off ke",

                               "Hex",       0,  "mi Hex/0 'On'hex -on ke",
                               "Hex",       1,  "mi Hex/1 'Off'hex -off ke",

                               "Lineno",    0,  "mi Lineno/0 'On'lineno -on ke",
                               "Lineno",    1,  "mi Lineno/1 'Off'lineno -off ke",

                               "ScrollBr",  0,  "mi ScrollBr/0 'On'sb -on ke",
                               "ScrollBr",  1,  "mi ScrollBr/1 'Off'sb -off ke",
                               "ScrollBr",  2,  "mi ScrollBr/2 'Auto'sb -auto ke",

                               "vt",        0,  "mi vt/0 'On'vt -on ke",
                               "vt",        1,  "mi vt/1 'Off'vt -off ke",
                               "vt",        2,  "mi vt/2 'Auto'vt -auto ke",

                               "Mouse",     0,  "mi Mouse/0 'On'mouse -on ke",
                               "Mouse",     1,  "mi Mouse/1 'Off'mouse -off ke",

                               "Menubar",   0,  "mi Menubar/0 'File'pd File ke",
                               "Menubar",   1,  "mi Menubar/1 'Edit'pd Edit ke",
                               "Menubar",   2,  "mi Menubar/2 'Modes'pd Modes ke",
                               "Menubar",   3,  "mi Menubar/3 'Macro'pd Macro ke",

                               NULL,        0,  NULL};

void check_menu_items(DISPLAY_DESCR     *dspl_descr)
{

int                   i;
DMC                  *new_dmc;

for (i = 0; forced_mi[i].pd_name; i++)
{
   new_dmc  = get_menu_cmds(dspl_descr->hsearch_data,
                         forced_mi[i].pd_name,
                         forced_mi[i].idx,
                         MENU_ITEM_TEXT);  
   if (new_dmc == NULL)
      {
         DEBUG29(fprintf(stderr, "Adding menu item: \"%s\"\n", forced_mi[i].defn);)
         new_dmc = parse_dm_line(forced_mi[i].defn,
                                 True,  /* temp DMC structures */
                                 0,     /* no line number      */
                                 dspl_descr->escape_char,
                                 dspl_descr->hsearch_data);
         if (new_dmc != NULL)
            {
               dm_mi(new_dmc, False, dspl_descr); /* always a kd */
               free_dmc(new_dmc);
            }

      }
}

} /* end of check_menu_items */


/************************************************************************

NAME:      is_menu_item_key  - Test is a keykey is a menu item keykey

PURPOSE:    This routine flags the keykey (from the hash table)
            so no attempt is made to use it as a real X keysym.

PARAMETERS:

   1. keykey       -  pointer to char  (INPUT)
                      This the keykey to be tested.

FUNCTIONS :

   1.  Parse the keykeym and verify that the last piece the special value.

RETURNED VALUE:
   rc   -  int
           The returned value shows the keykey was for an alias
           False  -  Not an menu keykey
           True   -  Is an menu keykey

*************************************************************************/

int   is_menu_item_key(char          *keykey)
{
char         pd_name[10];
int          menu_num;
int          magic;
int          i;

i = sscanf(keykey, MI_READ_FORMAT, pd_name, &menu_num, &magic);
if ((i != 3) || (magic != MI_HASH_TYPE))
   return(False);
else
   return(True);

} /* end of is_menu_item_key */


/************************************************************************

NAME:      dm_tmb  - To menu bar

PURPOSE:    This routine executes the Ce command 'tmb'.   It moves the
            cursor to the menu bar and locks the screen or moves it
            to a dm window.

PARAMETERS:
   1.  display      - pointer to Display (Xlib type) (INPUT)
                      This is the display being drawn on.

   2.  pd_data      - pointer to PD_DATA (INPUT)
                      This is the pulldown data from the display description.
                      There is a public and private part to this data.


FUNCTIONS :
   1.  If the menu bar is on, warp the cursor to that window and set the
       pulldown active flag to freeze the main window.

   2.  If the menubar is off and there is a pulldown to go to, go to it.

RETURNED VALUE:
   warp_done  -  int (flag)
                 The returned value shows it this routine warped the pointer.
                 False  -  No pointer warp done
                 True   -  Pointer warped.

*************************************************************************/

int   dm_tmb(Display          *display,            /* input   */
             PD_DATA          *pd_data)            /* input   */
{
Window                window = None;
int                   warped = False;
PD_PRIVATE           *private = (PD_PRIVATE *)pd_data->pd_private;

if (pd_data->menubar_on)
   {
      window =  pd_data->menubar_x_window;
      pd_data->pulldown_active = True;
   }
else
   if ((pd_data->pulldown_active) && (private->win_nest_level > 0))
      window =  private->pd[private->win_nest_level-1].pd_win;
  
if (window != None)
   {
      DEBUG29(fprintf(stderr, "dm_tmb: Warping to window 0x%X (10,4)\n", window);)
      DEBUG9(XERRORPOS)
      XWarpPointer(display, None, window,
                   0, 0, 0, 0,
                   10, 4); /* target location in the window */
      warped = True;
   }

return(warped);

} /* end of dm_tmb */







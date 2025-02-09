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
*  module window.c
*
*  Routines:
*     dm_geo                  - Resize and move a window, new command
*     dm_icon                 - iconify the window
*     dm_wp                   - window pop
*     dm_rs                   - refresh screen
*     process_geometry        - Process a geometry string, set window values.
*     window_is_obscured      - Return flag showing if the window is at least partially obscured.
*     raised_window           - Sets flag to show window is raised
*     obscured_window         - Sets flag to show window is at least partially obscured.
*     main_subarea            - Calculates size of text region of the main window
*     window_crossing         - Mark the passage of the cursor into and out of the window structure
*     focus_change            - Mark the gaining and losing the input focus.
*     in_window               - Show which window the mouse cursor is in.
*     force_in_window         - Force the in window flag to true.
*     force_out_window        - Force the in window flag to False
*     get_root_location       - Get the root x and y coordinates for a window
*     reconfigure             - Generate a configurenotify
*     fake_focus              - Generate a fake focuse event
*
*  Internal:
*     check_private_data      - Get the window private data from the display description.
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h   /usr/include/sys/errno.h      */
#include <limits.h>         /* /usr/include/limits.h     */
#include <ctype.h>          /* /usr/include/ctype.h      */

#include "borders.h"
#include "cc.h"
#include "debug.h"
#include "emalloc.h"
#include "dmwin.h"
#include "getevent.h"
#include "parms.h"
#include "parms.h"
#include "sbwin.h"
#include "sendevnt.h"
#include "xerrorpos.h"
#include "window.h"

#define  ABS(a)    ((a) > 0 ? (a) : -(a))


/***************************************************************
*  
*  Static data hung off the display description.
*
*  The following structure is hung off the display_data field
*  in the display description.
*  
*  These static flags keeps track of whether the mouse cursor is
*  in the window set.  If focus events are available, the focus
*  flag is used, on the apollo, the window mask is used.
*
*   window_mask - int
*                 This int is used in the routines window_crossing,
*                 in_window, and force_in_window.  It is the variable
*                 or'ed and anded to show what window the mouse cursor is
*                 in.  Routines window_crossing and force_in_window set it.
*                 Routine in_window reads it.  The flag describes which
*                 window we are in, but it is just used as a zero / non-zero flag.
*                 This flag is only used by in_window when focus change events
*                 are not available
*
*   focus_flag  - int
*                 This int is used in the routines focus_change,
*                 in_window, and force_in_window. The flag is set to true
*                 when a focusin event arrives and false for a focus out.
*                 Force in window sets it to true.  It has a special initial
*                 value of FOCUS_UNKNOWN because not all X servers send 
*                 focus events.  In particular, the Apollo shared mode server.
*                 If not focus events ever arrive, the window mask is used.
*                 in.  Routines focus_change and force_in_window set it.
*                 Routine in_window reads it.
*                 Values:  True          -  This program has the input focus for the display
*                          False         -  This program does not have the input focus for the display
*                          FOCUS_UNKNOWN -  No FocusIn or FocusOut events have been observed.
*
*   forced_focus_flag  - int
*                 This int is used in the routines in_window and force_in_window.
*                 The flag is set to true is set to true in force_in_window and
*                 cleared in in_window.  It guarentees that when force_in_window
*                 is called, the next call to in_window will show true.
*                 Values:  True          -  This program has the input focus for the display
*                          False         -  This program does not have the input focus for the display
*
*   window_obscured  - int
*                 This int is used in the routines window_is_obscured,
*                 raised_window, and obscured_window.  It is set to true
*                 if the window is at least partially obscurred.
*
***************************************************************/

#define FOCUS_UNKNOWN -1

typedef struct {
    int window_mask;
    int focus_flag;
    int window_obscured;
    int forced_focus_flag;
} FOCUS_PRIVATE;

static FOCUS_PRIVATE *check_private_data(DISPLAY_DESCR       *dspl_descr);


/************************************************************************

NAME:      dm_geo  -  geometry

PURPOSE:    This routine will take an x windows geometry string and
            resize the window accordingly.

PARAMETERS:

   1.  dmc           - pointer to DMC (INPUT)
                       This is the command structure for the region.

   2.  main_window   - pointer to DRAWABLE_DESCR (INPUT / OUTPUT)
                       This is the window description for the main window.
                       It is updated to the new geometry.


FUNCTIONS :

   1.   Use the Xlib routine to parse the geometry string.

   2.   For each value not specified, fill in the current window value.

   3.   Issue a move/resize window request to move the window.
        The configure event processing will take care of everything else.

RETURNED VALUE:
   resized   -  int flag
                True  - Window was resized
                False - Window was not resized

*************************************************************************/

int   dm_geo(DMC             *dmc,
             DISPLAY_DESCR   *dspl_descr)
{

int             bad;
char            msg[80];
int             hold_width;
int             hold_height;
int             hold_x;
int             hold_y;
int             resized = False;

if ((dmc->geo.parm == NULL) || (dmc->geo.parm[0] == '\0') || (dmc->geo.parm[0] == '*'))
   {
      if ((dmc->geo.parm != NULL) && (dmc->geo.parm[0] == '*'))
         get_root_location(dspl_descr->display,
                           dspl_descr->main_pad->x_window,
                           &dspl_descr->main_pad->window->x,
                           &dspl_descr->main_pad->window->y);

      snprintf(msg, sizeof(msg), "%dx%d%+d%+d",
                   dspl_descr->main_pad->window->width, dspl_descr->main_pad->window->height,
                   dspl_descr->main_pad->window->x, dspl_descr->main_pad->window->y);
      dm_error(msg, DM_ERROR_MSG);
      return(False);
   }
hold_width  = dspl_descr->main_pad->window->width;
hold_height = dspl_descr->main_pad->window->height;
hold_x      = dspl_descr->main_pad->window->x;
hold_y      = dspl_descr->main_pad->window->y;

bad = process_geometry(dspl_descr, dmc->geo.parm, 0);

if (!bad)
   {
      if ((hold_width == (int)dspl_descr->main_pad->window->width) && (hold_height == (int)dspl_descr->main_pad->window->height) && (hold_x == dspl_descr->main_pad->window->x) && (hold_y == dspl_descr->main_pad->window->y))
         resized = False;
      else
         resized = True;

      DEBUG5(fprintf(stderr, "(geo) Geometry was %dx%d%+d%+d, changed to %dx%d%+d%+d window %schanged\n",
             hold_width, hold_height, hold_x, hold_y,
             dspl_descr->main_pad->window->width, dspl_descr->main_pad->window->height, dspl_descr->main_pad->window->x, dspl_descr->main_pad->window->y,
             (resized ? "" : "did not "));
      )

      /***************************************************************
      *  To avoid blowing up Hummingbird Exceed, unmap the windows
      *  before we call XMoveResizeWindow
      *  Changed 9/7/2001 by R. E. Styma
      ***************************************************************/
      DEBUG9(XERRORPOS)
      if (dspl_descr->pad_mode)
         XUnmapWindow(dspl_descr->display, dspl_descr->unix_pad->x_window);
      DEBUG9(XERRORPOS)
      XUnmapWindow(dspl_descr->display, dspl_descr->dminput_pad->x_window);
      DEBUG9(XERRORPOS)
      XUnmapWindow(dspl_descr->display, dspl_descr->dmoutput_pad->x_window);
      DEBUG9(XERRORPOS)
      if (dspl_descr->sb_data->vert_window != None)
         XUnmapWindow(dspl_descr->display, dspl_descr->sb_data->vert_window);
      DEBUG9(XERRORPOS)
      if (dspl_descr->sb_data->horiz_window != None)
         XUnmapWindow(dspl_descr->display, dspl_descr->sb_data->horiz_window);
      DEBUG9(XERRORPOS)
      if (dspl_descr->pd_data->menubar_x_window != None)
         XUnmapWindow(dspl_descr->display, dspl_descr->pd_data->menubar_x_window);

#ifdef WIN32
      DEBUG9(XERRORPOS)
      XUnmapWindow(dspl_descr->display, dspl_descr->main_pad->x_window);
#endif
      XFlush(dspl_descr->display);

      DEBUG9(XERRORPOS)
      XMoveResizeWindow(dspl_descr->display, dspl_descr->main_pad->x_window,
                        dspl_descr->main_pad->window->x,     dspl_descr->main_pad->window->y,
                        dspl_descr->main_pad->window->width, dspl_descr->main_pad->window->height);

#ifdef WIN32
      XMapWindow(dspl_descr->display, dspl_descr->main_pad->x_window);
      XFlush(dspl_descr->display);
#endif

      /***************************************************************
      *  
      *  Put the dimensions back till they are changed by the configuration 
      *  notify event.
      *  
      ***************************************************************/
      dspl_descr->main_pad->window->width  = hold_width;
      dspl_descr->main_pad->window->height = hold_height;
      dspl_descr->main_pad->window->x      = hold_x;
      dspl_descr->main_pad->window->y      = hold_y;
   }

return(resized);

} /* end of dm_geo */


/************************************************************************

NAME:      dm_icon  -  iconify a window.

PURPOSE:    This routine will iconify the current ce.  There is no
            de-iconify since this is done by the window manager.

PARAMETERS:

   1.  dmc           - pointer to DMC (INPUT)
                       This is the command structure for the region.

   2.  main_window   - pointer to DRAWABLE_DESCR (INPUT)
                       This is the window description for the main window.
                       It is needed for the window id and display pointer.


FUNCTIONS :

   1.   Call XIconifyWindow to iconify the window.


*************************************************************************/

/* ARGSUSED */
void  dm_icon(DMC              *dmc,
              Display          *display,
              Window            main_x_window)
{
int     rc;

if (!dmc->icon.dash_w && !dmc->icon.dash_i)
   {
      if (cc_is_iconned())
         {
            DEBUG1(fprintf(stderr, "attempting un-icon\n");)
            DEBUG9(XERRORPOS)
            XMapWindow(display, main_x_window);
         }
      else
         {
            DEBUG9(XERRORPOS)
            rc = XIconifyWindow(display,
                                main_x_window,
                                DefaultScreen(display));

            if (rc == 0) /* zero on failure */
               dm_error("(icon) Could not iconify", DM_ERROR_LOG);
         }
   }
else
   {
      if (dmc->icon.dash_w)
         {
            DEBUG1(fprintf(stderr, "attempting un-icon\n");)
            DEBUG9(XERRORPOS)
            XMapWindow(display, main_x_window);
         }
      else  /* must be -i */
         {
            DEBUG9(XERRORPOS)
            rc = XIconifyWindow(display,
                                main_x_window,
                                DefaultScreen(display));

            if (rc == 0) /* zero on failure */
               dm_error("(icon) Could not iconify", DM_ERROR_LOG);
         }
   }




} /* end of dm_icon */


/************************************************************************

NAME:      dm_wp  -  window pop

PURPOSE:    This routine will move a corner of the window according to
            dm rules and take the window with it..

PARAMETERS:

   1.  dmc           - pointer to DMC (INPUT)
                       This is the command structure for the wp command
                       The mode field is used.
                       VALUES:
                          DM_ON     -  -t (move to the top)
                          DM_OFF    -  -b (move to the bottom)
                          DM_TOGGLE -  If obscured, move to top, else move to bottom.

   2.  main_window_cur_descr - pointer to BUFF_DESCR (INPUT)
                       This is the buffer description for the
                       Main window.  It is used for comparison
                       purposes.  The mark only takes place
                       if the cursor was in the main window.


FUNCTIONS :

   1.   Determine if the window is on top.

   2.   If on top make it on bottom.

   3.   If onbottom make on top

*************************************************************************/


void  dm_wp(DMC          *dmc,
            PAD_DESCR    *main_window_cur_descr)
{

if (dmc->wp.mode == DM_ON)
   {
      DEBUG9(XERRORPOS)
      XRaiseWindow(main_window_cur_descr->display_data->display, main_window_cur_descr->x_window);
   }
else
   if (dmc->wp.mode == DM_OFF)
      {
         DEBUG9(XERRORPOS)
         XLowerWindow(main_window_cur_descr->display_data->display, main_window_cur_descr->x_window);
      }
   else
      if (window_is_obscured(main_window_cur_descr->display_data))
         {
            DEBUG9(XERRORPOS)
            XRaiseWindow(main_window_cur_descr->display_data->display, main_window_cur_descr->x_window);
         }
      else
         {
            DEBUG9(XERRORPOS)
            XLowerWindow(main_window_cur_descr->display_data->display, main_window_cur_descr->x_window);
         }

} /* end of dm_wp */


/************************************************************************

NAME:      dm_rs  -  refresh screen

PURPOSE:    This routine will cause expose events on all the windows on
            the display (including non-ce) windows by mapping an invisible
            window over the whole root window and then destroying it.

PARAMETERS:

    1.  Display      - pointer to Display (Xlib type) (INPUT)
                       This is the display pointer needed in Xlib calls.

FUNCTIONS :

   1.   Create a new window the size of the whole screen and map it.

   2.   Destroy the window.

*************************************************************************/

void  dm_rs(Display   *display)
{
Window                window;
unsigned int          display_width;
unsigned int          display_height;
XSetWindowAttributes  window_attrs;     /* setup structure for building the main window                  */
unsigned long         window_mask = 0;  /* bitmask to support window_attrs structure          */


display_width  = DisplayWidth(display, DefaultScreen(display)) ;
display_height = DisplayHeight(display, DefaultScreen(display)) ;

window_attrs.background_pixmap = None;
/*window_attrs.background_pixmap = ParentRelative;*/
window_mask |= CWBackPixmap;

window_attrs.override_redirect = True;
window_attrs.backing_store = NotUseful;
window_attrs.save_under = False;
window_mask |= (CWOverrideRedirect | CWBackingStore | CWSaveUnder);

window = XCreateWindow(display,
                       RootWindow(display, DefaultScreen(display)),
                       0, 0, display_width, display_height,
                       0,                   /*  border_width */
                       DefaultDepth(display, DefaultScreen(display)),  /*  depth        */
                       InputOutput,                    /*  class        */
                       CopyFromParent,                 /*  visual       */
                       window_mask,
                        &window_attrs
);

DEBUG5(fprintf(stderr, "(rs) Created window 0x%X  %dx%d+0+0\n", window, display_width, display_height);)


XMapWindow(display, window);

XFlush(display);

XDestroyWindow(display, window);

XFlush(display);

} /* end of dm_rs */


/************************************************************************

NAME:      process_geometry  -  process an X geometry string

PURPOSE:    This routine take a geomentry string and set the passed 
            windows x, y, width, and height from it.  Syntax checking
            is done for off screen movement and zero size.

PARAMETERS:

   1.  dspl_descr      - pointer to DISPLAY_DESCR (INPUT/OUTPUT)
                         This is the display description for the current display.
                         It is used to access the main window (INPUT/OUTPUT) and
                         and parameters.

   2.  geometry_string - pointer to MARK_REGION (INPUT)
                         This is the X geometry string to parse.
                         It is of the form <width>x<height>+<x>+<y>
                         -<x> may be substituted to refer to a offset
                         from the right hand side of the screen.
                         -<y> may be substituted to refer to a offset
                         from the bottom of the screen.
                         The geometry string may be prefixed with a 'C' to show
                         the width and height are in characters as describled by
                         member font in parm main window.

   3.  default_size  - int (INPUT)
                       This flag indicates whether defaults should be set
                       For unspecified or invalid values.
                       Values:  True   -  Do set defaults
                                False  -  Do not set defaults, leave window values alone.


FUNCTIONS :

   1.   Make sure a geometry string was passed in.  If not, do nothing
        unless we can set the defaults.

   2.   Use the Xlib routine to parse the geometry string.

   3.   Get the x, y, width, and height values from the parm or from defaults.

   4.   Syntax check the results and put in the window values if they are valid.
        If the results are invalid and we are allowed to use defaults, use default
        values.  Otherwise leave the values unchanged.

RETURNED VALUE:

   bad  -   int
            False  -  geometry processed successfully.
            True   -  geometry bad.  If the default parm was specified, 
                      a valid geometry was set up.

*************************************************************************/

int  process_geometry(DISPLAY_DESCR    *dspl_descr,       /* input / output */
                      char             *geometry_string,  /* input */
                      int               default_size)     /* input */
{
int             which_geometry_values = 0;
int             x;
int             y;
int             i;
int             bad = False;
unsigned int    width;
unsigned int    height;
Display        *display;
unsigned int    display_width;
unsigned int    display_height;
int             window_x;
int             window_y;
unsigned int    window_width;
unsigned int    window_height;
char            msg[256];
int             use_character_values = 0;


if ((geometry_string == NULL || geometry_string[0] == '\0') && !default_size)
   return(False);

display = dspl_descr->display;
display_width  = DisplayWidth(display, DefaultScreen(display)) ;
display_height = DisplayHeight(display, DefaultScreen(display)) ;


/***************************************************************
*  
*  First check for the 'C' or 'c' prefix to show that we should
*  calculate the width and height in characters.
*  
***************************************************************/

if (geometry_string != NULL)
   {
      if ((*geometry_string | 0x20) == 'c')
         {
            geometry_string++;
            use_character_values = 1;
         }

      if (!isdigit(*geometry_string) && (*geometry_string != '+') && (*geometry_string != '-') && (*geometry_string != 'x'))
         {
            snprintf(msg, sizeof(msg), "(geo) Invalid geometry string, first char not numeric (%s)", geometry_string);
            dm_error(msg, DM_ERROR_BEEP);
            if (!default_size)
               return(True);
         }
   }

/***************************************************************
*  
*  Parse the string with the standard X call.
*  
***************************************************************/

DEBUG9(XERRORPOS)
if (geometry_string != NULL && geometry_string[0] != '\0')
   {
      which_geometry_values = XParseGeometry(geometry_string,    /* geometry string */
                                             &x, &y,             /* location */
                                             &width, &height);   /* size */

      if (which_geometry_values == 0)  /* no valid data */
         {
            snprintf(msg, sizeof(msg), "(geo) Invalid geometry string (%s)", geometry_string);
            dm_error(msg, DM_ERROR_BEEP);
            if (!default_size)
               return(True);
         }
   }



/***************************************************************
*  
*  If the geometry value was not specified, either leave it be
*  or set it to a default value.
*  
***************************************************************/

if (which_geometry_values & WidthValue)
   if (use_character_values)
      window_width = (width * dspl_descr->main_pad->window->font->max_bounds.width) + (MARGIN * 2);
   else
      window_width = width;
else
   if (default_size)
      window_width = display_width / 2;
   else
      window_width = dspl_descr->main_pad->window->width;


if (which_geometry_values & HeightValue)
   if (use_character_values)
      window_height = ((height + 3) * dspl_descr->main_pad->window->line_height) + (MARGIN * 2); /* plus 3 compensates for the DM input window and the titlebar */
   else
      window_height = height;
else
   if (default_size)
      window_height = display_height / 2;
   else
      window_height = dspl_descr->main_pad->window->height;


if (which_geometry_values & XValue)
   {
      window_x = x;
      if (which_geometry_values & XNegative)
         window_x = display_width - (window_width + ABS(window_x));

   }
else
   if (default_size)
      window_x = 0;
   else
      window_x = dspl_descr->main_pad->window->x;


if (which_geometry_values & YValue)
   {
      window_y = y;
      if (which_geometry_values & YNegative)
         window_y = display_height - (window_height + ABS(window_y));
   }
else
   if (default_size)
      window_y = 0;
   else
      window_y = dspl_descr->main_pad->window->y;


/***************************************************************
*  
*  Check for moving the window off screen entirely, this is bad.
*  If defaults are allowed, change to the default value.
*  
***************************************************************/

if ((window_x >= (int)display_width) && !(OFFDSPL))
   {
      bad = True;
      if (default_size)
         dspl_descr->main_pad->window->x = 0;
   }
else
   dspl_descr->main_pad->window->x = window_x;
if ((window_y >= (int)display_height) && !(OFFDSPL))
   {
      bad = True;
      if (default_size)
         dspl_descr->main_pad->window->y = 0;
   }
else
   dspl_descr->main_pad->window->y = window_y;

if ((window_x + window_width <= 0) && !(OFFDSPL))
   {
      bad = True;
      if (default_size)
         dspl_descr->main_pad->window->x = 0;
   }
if ((window_y + window_height <= 0) && !(OFFDSPL))
   {
      bad = True;
      if (default_size)
         dspl_descr->main_pad->window->y = 0;
   }

if ((int)window_width <= 0)
   {
      bad = True;
      dspl_descr->main_pad->window->width = display_width / 2;
   }
else
   dspl_descr->main_pad->window->width = window_width;
if ((int)window_height <= 0)
   {
      bad = True;
      dspl_descr->main_pad->window->height = display_height / 2;
   }
else
   dspl_descr->main_pad->window->height = window_height;

if (bad)
   {
      snprintf(msg, sizeof(msg), "(geo) Illegal grow coordinates (window off screen or zero dimension) - %dx%d%+d%+d", window_width, window_height, window_x, window_y);
      dm_error(msg, DM_ERROR_BEEP);
   }
else
   {
      /***************************************************************
      *  
      *  If we need to apply a window manager adjustment, do so now.
      *  option_values is from parms.h and was set most likely from
      *  the Xdefaults file.  This compensates for window managers which
      *  shift window locations by some fixed amount.
      *  
      *  WMADJX_PARM is a macro in parms.h
      *
      ***************************************************************/
      if (WMADJX_PARM && (sscanf(WMADJX_PARM, "%d%9s", &i, msg) == 1))
         dspl_descr->main_pad->window->x += i;
      if (WMADJY_PARM && (sscanf(WMADJY_PARM, "%d%9s", &i, msg) == 1))
         dspl_descr->main_pad->window->y += i;

   }

return(bad);

} /* end of process_geometry */


/***************************************************************
*  
*  The following flag keeps track of whether the window is
*  partially or fully raised.
*  
***************************************************************/

int window_is_obscured(DISPLAY_DESCR       *dspl_descr)  /* returns True or False */
{
FOCUS_PRIVATE    *private;

if ((private = check_private_data(dspl_descr)) == NULL)
   return(0);

return(private->window_obscured);
}

void obscured_window(DISPLAY_DESCR       *dspl_descr)
{
FOCUS_PRIVATE    *private;

if ((private = check_private_data(dspl_descr)) == NULL)
   return;

if (!(private->window_obscured))
   cc_obscured(True);
private->window_obscured = True;
}

void raised_window(DISPLAY_DESCR       *dspl_descr) 
{
FOCUS_PRIVATE    *private;

if ((private = check_private_data(dspl_descr)) == NULL)
   return;

if (private->window_obscured)
   cc_obscured(False);
private->window_obscured = False;
}


/************************************************************************

NAME:      main_subarea

PURPOSE:    This routine sets up the main window text subarea during
            initialization and window resizing.

PARAMETERS:

   1.  main_window -  pointer to DRAWABLE_DESCR (INPUT)
               This is the main window description.

   2.  title_subarea -  pointer to DRAWABLE_DESCR (INPUT)
               This is the description of the titlebar sub area.
               Its size needs to be removed from the drawable area
               and the top of the drawable area needs to be moved
               down by this areas height.

   3.  lineno_subarea -  pointer to DRAWABLE_DESCR (INPUT)
               This optional area takes up room on the left
               side of the text subarea.

   4.  dminput_window -  pointer to DRAWABLE_DESCR (INPUT)
               This is the description of the dm input window.
               If NULL, the dminput window is not local.
               Otherwise the dm_input window takes up space at the
               bottom of the text subarea.

   4.  unixcmd_window -  pointer to DRAWABLE_DESCR (INPUT)
               This is the description of the unixcmd window.
               If NULL, then we are not in pad mode is not local.
               Otherwise the unixcmd window takes up space at the
               bottom of the text subarea.

   5.  text_subarea -  pointer to DRAWABLE_DESCR (INPUT)
               This is the subarea description to be updated.


FUNCTIONS :

   1.   Copy the standard stuff accross.

   2.   Carve out the margin from the global constant.

   3.   Calculate the width and height accouting for the DM window
        and the margin.

   4.   Calculate the number of lines that fit on the screen and
        adjust to height to be that exact size.

RETURNED VALUE:
   bad_size -  if the window is too short or too long, the window is resized
               and True is returned.
               False  -  Window is ok, continue.
               True   -  Window too small, resize is done.

NOTES:
   1.  Width and height are used in calculations because text_subarea->width
       and text_subarea->height are unsigned and useless if the 
       subarea width goes negative.


*************************************************************************/

int  main_subarea(DISPLAY_DESCR    *dspl_descr)
{
int                   x;
int                   y;
int                   width;
int                   height;
int                   fix_width = False;
int                   fix_height = False;

DRAWABLE_DESCR       *main_window  = dspl_descr->main_pad->window;
DRAWABLE_DESCR       *title_subarea = dspl_descr->title_subarea;
DRAWABLE_DESCR       *lineno_subarea;
DRAWABLE_DESCR       *dminput_window  = dspl_descr->dminput_pad->window;
DRAWABLE_DESCR       *unixcmd_window;

lineno_subarea = (dspl_descr->show_lineno ? dspl_descr->lineno_subarea : NULL);
unixcmd_window = ((dspl_descr->pad_mode && !(dspl_descr->vt100_mode)) ? dspl_descr->unix_pad->window : NULL);


/***************************************************************
*
*  Do the fixed stuff first:
*
***************************************************************/


DEBUG1(
fprintf(stderr, "main_subarea:  main window:  %dx%d%+d%+d\n",
main_window->width, main_window->height, main_window->x, main_window->y);
)

/***************************************************************
*
*  Calculate the area which actually has data in it.
*  For the DM input window, the prompt is part of the window
*  but not the writable subarea.
*
***************************************************************/

x = MARGIN;
if (lineno_subarea)
   x += lineno_subarea->x + lineno_subarea->width; 

width = main_window->width - x;

if (dspl_descr->sb_data->vert_window_on)
   width -= VERT_SCROLL_BAR_WIDTH;

y = MARGIN;
if (title_subarea != NULL)
   y  += title_subarea->y + title_subarea->height;

height = main_window->height - y;

if (dminput_window != NULL)
   height -= (dminput_window->height + BORDER_WIDTH);

if (unixcmd_window != NULL)
   height -= (unixcmd_window->height + BORDER_WIDTH);

if (dspl_descr->sb_data->horiz_window_on)
   height -= HORIZ_SCROLL_BAR_HEIGHT;


/***************************************************************
*  
*  Check if the window is too small.  If so, make it bigger.
*  Minimum for the text subarea (not counting the line numbers)
*  is 2 characters wide by 1 line tall.
*  
***************************************************************/

if (width < (MARGIN + main_window->font->max_bounds.width * 2))
   fix_width = True;
if (height < (main_window->line_height + MARGIN))
   fix_height = True;

if (fix_width || fix_height)
   {
      if (fix_height)
         height =  (main_window->line_height * 2) + (MARGIN * 2) + BORDER_WIDTH +
                   (dspl_descr->title_subarea->height + BORDER_WIDTH) +
                   (dminput_window ? (dminput_window->height + BORDER_WIDTH) : 0) +
                   (unixcmd_window ? (unixcmd_window->height + BORDER_WIDTH) : 0) +
                   (dspl_descr->pd_data->menubar_on ? (dspl_descr->pd_data->menu_bar.height + BORDER_WIDTH) : 0) +
                   (dspl_descr->sb_data->horiz_window_on ? HORIZ_SCROLL_BAR_HEIGHT : 0);
      else
         height =  main_window->height;

      if (fix_width)
         width  =  ((main_window->font->max_bounds.width) * 2) + (MARGIN * 3) + x +
                    (lineno_subarea ? (lineno_subarea->x + lineno_subarea->width) : 0) +
                    (dspl_descr->sb_data->vert_window_on ? VERT_SCROLL_BAR_WIDTH : 0);
      else
         width  =  main_window->width;

      x      =  main_window->x;
      y      =  main_window->y;

      /***************************************************************
      *  To avoid blowing up Hummingbird Exceed, unmap the windows
      *  before we call XMoveResizeWindow
      *  Changed 9/7/2001 by R. E. Styma
      ***************************************************************/
      DEBUG9(XERRORPOS)
      if (dspl_descr->pad_mode)
         XUnmapWindow(dspl_descr->display, dspl_descr->unix_pad->x_window);
      DEBUG9(XERRORPOS)
      XUnmapWindow(dspl_descr->display, dspl_descr->dminput_pad->x_window);
      DEBUG9(XERRORPOS)
      XUnmapWindow(dspl_descr->display, dspl_descr->dmoutput_pad->x_window);
      DEBUG9(XERRORPOS)
      if (dspl_descr->sb_data->vert_window != None)
         XUnmapWindow(dspl_descr->display, dspl_descr->sb_data->vert_window);
      DEBUG9(XERRORPOS)
      if (dspl_descr->sb_data->horiz_window != None)
         XUnmapWindow(dspl_descr->display, dspl_descr->sb_data->horiz_window);

#ifdef WIN32
      DEBUG9(XERRORPOS)
      XUnmapWindow(dspl_descr->display, dspl_descr->main_pad->x_window);
#endif
      XFlush(dspl_descr->display);

      DEBUG9(XERRORPOS)
      XMoveResizeWindow(dspl_descr->display, dspl_descr->main_pad->x_window, x, y, width, height);
      DEBUG1(fprintf(stderr, "main_subarea:  Bad size, (%s%s) resizing to:  %dx%d%+d%+d\n",
                    (fix_height ? "height" : ""), (fix_width ? "width" : ""), width, height, x, y);
      )

#ifdef WIN32
      XMapWindow(dspl_descr->display, dspl_descr->main_pad->x_window);
#endif
   }

/***************************************************************
*  Save the calculated values in the text subarea.
***************************************************************/
main_window->sub_x        = x;
main_window->sub_y        = y;
main_window->sub_width    = width;
main_window->sub_height   = height;


/***************************************************************
*  get the lines on screen and then adjust the Height of the
*  subarea to be an even number of lines.  Then copy the
*  lines on screen to the main window area.
***************************************************************/
main_window->lines_on_screen  = lines_on_drawable(main_window);
if (main_window->lines_on_screen == 0)
   main_window->lines_on_screen = 1;
/* These two are always the same */
if (lineno_subarea)
   lineno_subarea->lines_on_screen = main_window->lines_on_screen;

/***************************************************************
*  
*  The main subarea overlays the pixmap directly.
*  
***************************************************************/

memcpy((char *)dspl_descr->main_pad->pix_window, (char *)dspl_descr->main_pad->window, sizeof(DRAWABLE_DESCR));
dspl_descr->main_pad->pix_window->x = 0;
dspl_descr->main_pad->pix_window->y = 0;


DEBUG1(
   fprintf(stderr, "main_subarea:  main:  %dx%d%+d%+d, text_subarea:  %dx%d%+d%+d\n",
                   main_window->width, main_window->height, main_window->x, main_window->y,
                   main_window->sub_width, main_window->sub_height, main_window->sub_x, main_window->sub_y);
)

return(fix_width || fix_height);

} /* end of main_subarea  */


/************************************************************************

NAME:      window_crossing - Mark the passage of the cursor into and out of the window structure

PURPOSE:    This routine marks the passage of the mouse cursor into and
            out of the windows as caused by enter and leave events.

PARAMETERS:

   1.  event - pointer to XEvent (INPUT)
               This is the event which caused this routine to be called.
               It is always either an EnterNotify or LeaveNotifiy event.

FUNCTIONS :

   1.   Convert the window id in the event to the pad buffer

   2.   Switch on the which window field in the pad buffer.

   3.   For enter events, or in the mask for that window into the
        private area hung off the display description.

   4.   For leave events and out the mask for that window into the
        private area hung off the display description.


*************************************************************************/

void window_crossing(XEvent              *event,
                     DISPLAY_DESCR       *dspl_descr)
{
PAD_DESCR            *buffer;
FOCUS_PRIVATE    *private;


/***************************************************************
*  Make sure the private data area is available
***************************************************************/
if ((private = check_private_data(dspl_descr)) == NULL)
   return;

buffer = window_to_buff_descr(dspl_descr, event->xany.window);

switch(buffer->which_window)
{
case MAIN_PAD:
   if (event->type == EnterNotify)
      private->window_mask |=  MAIN_PAD_MASK;
   else
      private->window_mask &= ~MAIN_PAD_MASK;
   break;

case DMINPUT_WINDOW:
   if (event->type == EnterNotify)
      private->window_mask |=  DMINPUT_MASK | MAIN_PAD_MASK;
   else
      private->window_mask &= ~DMINPUT_MASK;
   break;

case DMOUTPUT_WINDOW:
   if (event->type == EnterNotify)
      private->window_mask |=  DMOUTPUT_MASK | MAIN_PAD_MASK;
   else
      private->window_mask &= ~DMOUTPUT_MASK;
   break;

case UNIXCMD_WINDOW:
   if (event->type == EnterNotify)
      private->window_mask |=  UNIXCMD_MASK | MAIN_PAD_MASK;
   else
      private->window_mask &= ~UNIXCMD_MASK;
   break;              

default:
   break;
}

DEBUG8(fprintf(stderr, "window_crossing: %s %s mask: 0x%X, display %d\n", ((event->type == EnterNotify) ? "Enter" : "Leave"), which_window_names[buffer->which_window], private->window_mask, dspl_descr->display_no);)

} /* end of window_crossing */


/************************************************************************

NAME:      focus_change - Mark the gaining and losing the input focus.

PURPOSE:    This routine marks the when the input focus enters the
            main window and when it leaves.

PARAMETERS:

   1.  event - pointer to XEvent (INPUT)
               This is the event which caused this routine to be called.
               It is always either an FocusIn or FocusOut event.

   2.  dspl_descr  -  pointer to DISPLAY_DESC
                  This is where the private data for this window set
                  is hung.


FUNCTIONS :

   1.   For FocusIn events, set the flag in the
        private area hung off the display description.

   2.   For FocuOutn events, clear the flag in the
        private area hung off the display description.


*************************************************************************/

void focus_change(XEvent              *event,
                  DISPLAY_DESCR       *dspl_descr)
{
FOCUS_PRIVATE        *private;

/***************************************************************
*  Make sure the private data area is available
***************************************************************/
if ((private = check_private_data(dspl_descr)) == NULL)
   return;

if (event->type == FocusIn)
   private->focus_flag = True;
else
   private->focus_flag = False;

DEBUG8(fprintf(stderr, "focus_change: Input Focus is %s, display %d\n", ((event->type == FocusIn) ? "True" : "False"), dspl_descr->display_no);)

} /* end of focus_change */


/************************************************************************

NAME:      in_window - Show which window the mouse cursor is in.

PURPOSE:    Report on whether this ce/ceterm session is the active
            program on the display.

PARAMETERS:

   dspl_descr  -  pointer to DISPLAY_DESCR
                  This is where the private data for this window set
                  is hung.


FUNCTIONS :

   1.   If the focus flag is not in its initial state, return its value.

   2.   If the focus flag is in its initial state, return the window mask.

RETURNED VALUE:
   in_window  -  int
                 This flag is true if the cursor or focus is in the
                 window.


*************************************************************************/

int in_window(DISPLAY_DESCR       *dspl_descr)
{
FOCUS_PRIVATE    *private;
int               in_window = False;

/***************************************************************
*  Make sure the private data area is available
***************************************************************/
if ((private = check_private_data(dspl_descr)) == NULL)
   return(in_window);

/* 11/2/93 dynamically work out whether there are focus events available RES */
if (private->focus_flag != FOCUS_UNKNOWN)
   {
      DEBUG8(fprintf(stderr, "in_window: focus %s, display %d,  mask 0x%X\n", (private->focus_flag ? "True" : "False"), dspl_descr->display_no, private->window_mask);)
      /******************************************************************************
      * RES 12/16/2002 use both focus_flag and window_mask to avoid problems
      * displaying to Linux with XFree86.  XFree86 give you the focus when you are
      * just in the handles to pull the window.  It also sends multiple Configurenotifies.
      * If Ce things you are in the window and doing a resize, it repositions the cursor
      * which moves the cursor which slams the window to a new size.
      ******************************************************************************/
      in_window = (private->focus_flag && private->window_mask) || private->forced_focus_flag;
#ifdef blah
      RES 8/22/2004 
      if (dspl_descr->XFree86_flag)
         return(private->focus_flag && private->window_mask);
      else    test RES 8/22/2004
         return(private->focus_flag);
#endif
   }
else
   {
      DEBUG8(fprintf(stderr, "in_window: mask 0x%X, display %d\n", private->window_mask, dspl_descr->display_no);)
      in_window = private->window_mask || private->forced_focus_flag;
   }

private->forced_focus_flag = False;  /* RES 9/22/2004 */
return(in_window);

} /* end of in_window */


/************************************************************************

NAME:      force_in_window - Force the in window flag to true.

PURPOSE:    Force the flag returned by in_window to resolve to true.
            This is used by commands such as geo and ti when we want
            to grab the input focus and move the cursor into this window.

PARAMETERS:

   dspl_descr  -  pointer to DISPLAY_DESCR
                  This is where the private data for this window set
                  is hung.

FUNCTIONS :

   1.   If the focus flag is not in its initial state, set it to True.

   2.   If the focus flag is in its initial state, set the window mask to non-zero.

*************************************************************************/

void force_in_window(DISPLAY_DESCR       *dspl_descr)
{
FOCUS_PRIVATE    *private;

/***************************************************************
*  Make sure the private data area is available
***************************************************************/
if ((private = check_private_data(dspl_descr)) == NULL)
   return;

private->forced_focus_flag = True;  /* RES 9/22/2004 */

/* 11/2/93 dynamically work out whether there are focus events available RES */
if (private->focus_flag != FOCUS_UNKNOWN)
   {
      DEBUG10(fprintf(stderr, "force_in_window: current focus %s, display %d\n", (private->focus_flag ? "True" : "False"), dspl_descr->display_no);)
      private->focus_flag = True;
#ifdef blah
      if (dspl_descr->XFree86_flag)
         private->window_mask |=  MAIN_PAD_MASK;
#endif
   }
else
   {
      DEBUG10(fprintf(stderr, "force_in_window: current mask 0x%X, display %d\n", private->window_mask, dspl_descr->display_no);)
      private->window_mask |=  MAIN_PAD_MASK;
   }
} /* end of force_in_window */


/************************************************************************

NAME:      force_out_window - Force the in window flag to False

PURPOSE:    Force the flag returned by in_window to resolve to true.
            This is used by commands such as geo and ti when we want
            to grab the input focus and move the cursor into this window.

PARAMETERS:

   dspl_descr  -  pointer to DISPLAY_DESCR
                  This is where the private data for this window set
                  is hung.

FUNCTIONS :

   1.   If the focus flag is not in its initial state, set it to False

   2.   If the focus flag is in its initial state, set the window mask to zero.

*************************************************************************/

void force_out_window(DISPLAY_DESCR       *dspl_descr)
{
FOCUS_PRIVATE    *private;

/***************************************************************
*  Make sure the private data area is available
***************************************************************/
if ((private = check_private_data(dspl_descr)) == NULL)
   return;

private->forced_focus_flag = False;  /* RES 9/22/2004 */

/* 11/2/93 dynamically work out whether there are focus events available RES */
if (private->focus_flag != FOCUS_UNKNOWN)
   {
      DEBUG10(fprintf(stderr, "force_out_window: current focus %s, display %d\n", (private->focus_flag ? "True" : "False"), dspl_descr->display_no);)
      private->focus_flag = False;
#ifdef blah
      if (dspl_descr->XFree86_flag)
         private->window_mask = 0;
#endif
   }
else
   {
      DEBUG10(fprintf(stderr, "force_out_window: current mask 0x%X, display %d\n", private->window_mask, dspl_descr->display_no);)
      private->window_mask = 0;
   }
} /* end of force_out_window */


/************************************************************************

NAME:      check_private_data - Get the window private data from the display description.

PURPOSE:    This routine returns the existing dmwin private area
            from the display description or initializes a new one.

PARAMETERS:

   dspl_descr  -  pointer to DISPLAY_DESCR (in windowdefs.h)
                  This is where the private data for this window set
                  is hung.

FUNCTIONS :

   1.   If the private area has already been allocated, return it.

   2.   Malloc a new area.  If the malloc fails, return NULL

   3.   Initialize the new private area and return it.

RETURNED VALUE
   private  -  pointer to static
               This is the private static area used by the
               routines in this module.
               NULL is returned on malloc failure only.

*************************************************************************/

static FOCUS_PRIVATE *check_private_data(DISPLAY_DESCR       *dspl_descr)
{
FOCUS_PRIVATE    *private;

if (dspl_descr->focus_data)
   return((FOCUS_PRIVATE *)dspl_descr->focus_data);

private = (FOCUS_PRIVATE *)CE_MALLOC(sizeof(FOCUS_PRIVATE));
if (!private)
   return(NULL);   
else
   memset((char *)private, 0, sizeof(FOCUS_PRIVATE));

private->focus_flag = FOCUS_UNKNOWN;

dspl_descr->focus_data = (void *)private;

return(private);

} /* end of check_private_data */


/************************************************************************

NAME:      get_root_location - Get the root x and y coordinates for a window

PURPOSE:    This routine follows the parent window pointers and figures
            the x and y coordinates of the upper left corner of the window
            with respect to the root.

PARAMETERS:
   1.  display     -  pointer to Display (Xlib type) (INPUT)
                      This is the display token used for making Xlib calls.

   2.  window      -  Window (Xlib type) (INPUT)
                      This is the window for which we are calulating the location.

   3.  root_x      -  pointer to int (OUTPUT)
                      This is the root X coordinate for the upper left
                      corner of the window.

   4.  root_y      -  pointer to int (OUTPUT)
                      This is the root Y coordinate for the upper left
                      corner of the window.

FUNCTIONS :
   1.   Get the geometry of the passed window.

   2.   Get the parent window id.

   3.   Repeatedly get the geometry of the parent and factor in the position
        till the parent is the root window.

*************************************************************************/

void  get_root_location(Display      *display,
                        Window        window,
                        int          *root_x,
                        int          *root_y)
{
int               ok;
Window            root_return;
Window            parent_return;
Window           *child_windows = NULL;
unsigned int      nchildren;

int               x_return;
int               y_return;
unsigned int      width_return;
unsigned int      height_return;
unsigned int      base_width;
unsigned int      base_height;

unsigned int      border_width_return;
unsigned int      depth_return;


DEBUG9(XERRORPOS)
XGetGeometry(display,
             window,
             &root_return,
             root_x,
             root_y,
             &base_width,
             &base_height,
             &border_width_return,
             &depth_return);

DEBUG1(fprintf(stderr, "get_root_location:  Ce window: 0x%08X %dx%d%+d%+d\n", window, base_width, base_height, *root_x, *root_y);)

/***************************************************************
*  
*  Find the window whose parent is the root window.  This may
*  be the passed window or a parent of the passed window.
*  
***************************************************************/

DEBUG9(XERRORPOS)
ok = XQueryTree(display,                 /* input */
                window,                  /* input */
                &root_return,            /* output */
                &parent_return,          /* output */
                &child_windows,          /* output */
                &nchildren);             /* output */

while(ok && (root_return != parent_return))
{

   DEBUG9(XERRORPOS)
   XGetGeometry(display,
                parent_return,
                &root_return,
                &x_return,
                &y_return,
                &width_return,
                &height_return,
                &border_width_return,
                &depth_return);

   DEBUG1(fprintf(stderr, "get_root_location:  parent:    0x%08X %dx%d%+d%+d\n", parent_return, width_return, height_return, x_return, y_return);)

   (*root_x)       += x_return;
   (*root_y)       += y_return;


   if (child_windows)
      XFree((char *)child_windows);
   child_windows = NULL;
   window = parent_return;

   DEBUG9(XERRORPOS)
   ok = XQueryTree(display,                 /* input */
                   window,                  /* input */
                   &root_return,            /* output */
                   &parent_return,          /* output */
                   &child_windows,          /* output */
                   &nchildren);             /* output */
}

DEBUG1(fprintf(stderr, "get_root_location:  Ce window: 0x%08X %dx%d%+d%+d\n", window, base_width, base_height, *root_x, *root_y);)

if (child_windows)
   XFree((char *)child_windows);

} /* end of get_root_location */


/************************************************************************

NAME:      reconfigure - Generate a configurenotify

PURPOSE:    This routine generates a configurnotify which says the
            window stayed the same shape.  It is used to deal with 
            scroll bars and the menu bar coming and going.

PARAMETERS:
   1.  main_pad    -  pointer to PAD_DESCR
                      The main pad is needed for the values to put in
                      the configure enent.

   2.  display     -  pointer to Display (Xlib type)
                      This is the display pointer needed for X calls.


FUNCTIONS :
   1.   Build the configure event to show the window did not
        really change.

   2.   Send it, using the ce shortcut.  Code in sendevnt.c

*************************************************************************/

void reconfigure(PAD_DESCR        *main_pad,
                 Display          *display)
{
XEvent                configure_event;

/***************************************************************
*  Force the screen to be reconfigured.
***************************************************************/
memset((char *)&configure_event, 0, sizeof(configure_event));
configure_event.xconfigure.type               = ConfigureNotify;
configure_event.xconfigure.serial             = 0;
configure_event.xconfigure.send_event         = True;
configure_event.xconfigure.display            = display;
configure_event.xconfigure.event              = main_pad->x_window;
configure_event.xconfigure.window             = main_pad->x_window;
configure_event.xconfigure.x                  = main_pad->window->x;
configure_event.xconfigure.y                  = main_pad->window->y;
configure_event.xconfigure.width              = main_pad->window->width;
configure_event.xconfigure.height             = main_pad->window->height;
configure_event.xconfigure.border_width       = 1;
configure_event.xconfigure.above              = None;
configure_event.xconfigure.override_redirect  = False;

DEBUG9(XERRORPOS)
ce_XSendEvent(display, main_pad->x_window, True, StructureNotifyMask, (XEvent *)&configure_event);


}  /* end of reconfigure */


/************************************************************************

NAME:      fake_focus - Generate a fake focuse event

           On linux, using a KVM, if you go away from the linux screen
           with the KVM and come back after the screen saver starts,
           you need to let ce know it has the focus.  This is done
           with the focus flag in the EnterNotify message.

PARAMETERS:

   1.  dspl_descr - pointer to DISPLAY_DESCR (INPUT)
                    This is the display description to
                    use to build the fake keystroke. 

   2.  focus_in   - int flag (INPUT)
                    True for focusin.

FUNCTIONS :

   1.   Build a fake focus event.

   2.   send it to the main window.

*************************************************************************/

void fake_focus(DISPLAY_DESCR    *dspl_descr, int focus_in)
{
XEvent                event_union;      /* The event structure filled in by X as events occur            */

DEBUG15(fprintf(stderr, "Generating fake focus %s event\n", (focus_in ? "in" : "out"));)

if (focus_in)
   event_union.xfocus.type     = FocusIn;
else
   event_union.xfocus.type     = FocusOut;
event_union.xfocus.serial      = 0;
event_union.xfocus.send_event  = True;
event_union.xfocus.display     = dspl_descr->display;
event_union.xfocus.window      = dspl_descr->main_pad->x_window;
event_union.xfocus.mode        = NotifyNormal;
event_union.xfocus.detail      = NotifyDetailNone;

DEBUG9(XERRORPOS)
ce_XSendEvent(dspl_descr->display,
              dspl_descr->main_pad->x_window,
              True,
              KeyPressMask,
              &event_union);

} /* end of fake_focus */




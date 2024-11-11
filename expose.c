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
*  module expose.c
*
*  Routine:
*     exposed_window          - Process expose event from X
*
***************************************************************/


#include <stdio.h>          /* /usr/include/stdio.h      */
#include <errno.h>          /* /usr/include/errno.h      */
#include <string.h>         /* /usr/include/string.h   */
#include <limits.h>         /* /usr/include/limits.h   */
#include <sys/types.h>      /* "/usr/include/sys/types.h"     */

#ifdef WIN32
#ifdef WIN32_XNT
#include "xnt.h"
#else
#include "X11/Xlib.h"       /* /usr/include/X11/Xlib.h   */
#endif
#else
#include <X11/Xlib.h>       /* /usr/include/X11/Xlib.h   */
#endif


#include "debug.h"
#include "display.h"
#include "expose.h"
#include "dumpxevent.h"
#include "getevent.h"
#include "parms.h"
#include "pd.h"
#include "redraw.h"
#include "sbwin.h"
#include "txcursor.h"
#include "window.h"
#include "xerror.h"
#include "xerrorpos.h"


/************************************************************************

NAME:      exposed_window  - Process expose event from X


PURPOSE:    This routine copies data from the pixmap to the window 
            when an expose event occurs.  It is called from the
            main event loop.

PARAMETERS:
   1.  dspl_descr      - pointer to DISPLAY_DESCR  (INPUT/OUTPUT)
                         This is the current display description.
                            
   2.  event_union     - pointer to XEvent (INPUT)
                         This is the event which caused the DM commands to get
                         executed.  It the event structure for a:
                         KeyPress, KeyRelease, ButtonPress, ButtonRelease


FUNCTIONS :
   1.   Do any redrawing for pulldown or scroll bar windows and copy		
        data from the pixmap as needed.


*************************************************************************/

void  exposed_window(DISPLAY_DESCR  *dspl_descr,         /* in/out */
                     XEvent         *event_union)        /* input  Expose event */
{
int                   pixmap_x;         /* offset of window in pixmap, used in expose event processing                */
int                   pixmap_y;         /* main window is at 0,0, other windows are elsewhere on the pixmap           */
PAD_DESCR            *from_buffer;      /*     "                         "                       "       */
int                   i;
int                   in_area;

DEBUG(
if (event_union->type != Expose)
   {
      fprintf(stderr, "non-expose event passed to exposed_window, type = %d\n", event_union->type);
      return;
   }
)

/***************************************************************
*  
*  Check which window is impacted by the expose event
*  The scroll bar windows are a special case since they are
*  not in the pixmap.
*  
***************************************************************/
if (event_union->xany.window == dspl_descr->sb_data->vert_window)
   {
      DEBUG12(fprintf(stderr, "Draw vertical scrollbar due to expose\n");)
      draw_vert_sb(dspl_descr->sb_data, dspl_descr->main_pad, dspl_descr->display);
      return;
   }

if (event_union->xany.window == dspl_descr->sb_data->horiz_window)
   {
      DEBUG12(fprintf(stderr, "Draw horizontal scrollbar due to expose\n");)
      draw_horiz_sb(dspl_descr->sb_data, dspl_descr->main_pad, dspl_descr->display);
      return;
   }

/***************************************************************
*  If we have active pulldowns, and this is an expose for
*  one, draw the pulldown into the window.
***************************************************************/
if (dspl_descr->pd_data->pulldown_active && ((i = pd_window_to_idx(dspl_descr->pd_data, event_union->xany.window)) >= 0))
   {
      DEBUG12(fprintf(stderr, "Draw pulldown %d due to expose\n", i);)
      draw_pulldown(dspl_descr->display,            /* input   */
                    dspl_descr->hsearch_data,       /* input   */
                    i,                              /* input   */
                    dspl_descr->pd_data);           /* input   */
      return;
   }

/***************************************************************
*  If the menu bar is on, and this is an expose for it, 
*  copy the data from the pixmap to the window.
***************************************************************/
if (dspl_descr->pd_data->menubar_on && (event_union->xany.window == dspl_descr->pd_data->menubar_x_window))
   {
      DEBUG9(XERRORPOS)
      while (XCheckTypedWindowEvent(dspl_descr->display,
                                    dspl_descr->pd_data->menubar_x_window,
                                    Expose,
                                    event_union));
      DEBUG12(
      fprintf(stderr, "Menubar  xcopy from %dx%d+0+0 to %dx%d+0+0, %s\n",
                dspl_descr->pd_data->menu_bar.width,       /* width to copy        */
                dspl_descr->pd_data->menu_bar.height,      /* height to copy       */
                dspl_descr->pd_data->menu_bar.width,       /* width to copy        */
                dspl_descr->pd_data->menu_bar.height,      /* height to copy       */
                "menubar window");
      )
      DEBUG9(XERRORPOS)
      XCopyArea(dspl_descr->display,                    /* display */
                dspl_descr->x_pixmap,                   /* from pixmap */
                dspl_descr->pd_data->menubar_x_window,  /* to window   */
                dspl_descr->pd_data->menu_bar.gc,       /* the graphics context */
                dspl_descr->pd_data->menu_bar.x,        /* from origin x        */
                dspl_descr->pd_data->menu_bar.y,        /* from origin y        */
                dspl_descr->pd_data->menu_bar.width,    /* width to copy        */
                dspl_descr->pd_data->menu_bar.height,   /* height to copy       */
                0, 0);                                  /* target origin        */
      return;
   }

from_buffer = window_to_buff_descr(dspl_descr, event_union->xany.window); /* in getevent.c */
if (from_buffer->which_window == MAIN_PAD)
   {
      pixmap_x = 0;
      pixmap_y = 0;
   }
else
   {
      pixmap_x = MAX(from_buffer->window->x, 0);
      pixmap_y = MAX(from_buffer->window->y, 0);
   }
if (dspl_descr->x_pixmap == None)
   {
      DEBUG15(fprintf(stderr, "Expose of %s window ignored becase of no pixmap available\n", which_window_names[from_buffer->which_window]);)
      return;
   }

if (dspl_descr->echo_mode && (dspl_descr->mark1.buffer->which_window == from_buffer->which_window))
   {
      DEBUG9(XERRORPOS)
      while (XCheckTypedWindowEvent(dspl_descr->display,
                                    event_union->xexpose.window,
                                    Expose,
                                    event_union));
      DEBUG12(
         fprintf(stderr, "Expose highlight  xcopy from %dx%d%+d%+d to %dx%d+0+0, %s\n",
                      from_buffer->window->width,       /* width to copy        */
                      from_buffer->window->height,      /* height to copy       */
                      pixmap_x,
                      pixmap_y,
                      from_buffer->window->width,       /* width to copy        */
                      from_buffer->window->height,      /* height to copy       */
                      which_window_names[from_buffer->which_window]);
      )
      DEBUG9(XERRORPOS)
      XCopyArea(dspl_descr->display,                    /* display */
                dspl_descr->x_pixmap,                   /* from pixmap */
                from_buffer->x_window,                  /* to window   */
                from_buffer->window->gc,                /* the graphics context */
                pixmap_x, pixmap_y,                     /* from origin          */
                from_buffer->window->width,             /* width to copy        */
                from_buffer->window->height,            /* height to copy       */
                0, 0);                                  /* target origin        */
      clear_text_cursor(from_buffer->x_window, dspl_descr);
      clear_text_highlight(dspl_descr);
      text_rehighlight(from_buffer->x_window, from_buffer->window, dspl_descr);
   }
else
   {
      in_area = cursor_in_area(event_union->xexpose.window,
                               event_union->xexpose.x, event_union->xexpose.y,
                               event_union->xexpose.width, event_union->xexpose.height,
                               dspl_descr);

      /***************************************************************
      *  If the cursor is in the area being exposed.  Convert the
      *  expose to a full window expose.  This is to from generating
      *  mouse droppings when the cursor overlaps the expose boundary.
      ***************************************************************/
      if (in_area)
         {
            DEBUG12(
            fprintf(stderr, "Cursor in area, converting expose from: %dx%d%+d%+d to: %dx%d%+d%+d\n",
                      event_union->xexpose.width,
                      event_union->xexpose.height,
                      event_union->xexpose.x,
                      event_union->xexpose.y,
                      0, 0,
                      from_buffer->window->width,
                      from_buffer->window->height);
            )
            /***************************************************************
            *  Eat any extra exposes, since we are doing the whole thing.
            *  Then resize the expose area to the whole window.
            ***************************************************************/
            DEBUG9(XERRORPOS)
            while (XCheckTypedWindowEvent(dspl_descr->display,
                                          event_union->xexpose.window,
                                          Expose,
                                          event_union))
               DEBUG15( fprintf(stderr, "Previous event IGNORED\n\n");dump_Xevent(stderr, event_union, debug);)
            event_union->xexpose.x = 0;
            event_union->xexpose.y = 0;
            event_union->xexpose.width  = from_buffer->window->width;
            event_union->xexpose.height = from_buffer->window->height;
         }

      DEBUG12(
      fprintf(stderr, "Expose xcopy from %dx%d%+d%+d to %dx%d%+d%+d, %s\n",
              event_union->xexpose.width,
              event_union->xexpose.height,
              event_union->xexpose.x+pixmap_x,
              event_union->xexpose.y+pixmap_y,
              event_union->xexpose.width,
              event_union->xexpose.height,
              event_union->xexpose.x,
              event_union->xexpose.y,
              which_window_names[from_buffer->which_window]);
      )
      DEBUG9(XERRORPOS)
      XCopyArea(dspl_descr->display,                    /* display */
                dspl_descr->x_pixmap,                   /* from pixmap */
                from_buffer->x_window,                  /* to window   */
                from_buffer->window->gc,                /* the graphics context */
                event_union->xexpose.x+pixmap_x,
                event_union->xexpose.y+pixmap_y,         /* window offset in the pixmap */
                event_union->xexpose.width,              /* width to copy        */
                event_union->xexpose.height,             /* height to copy       */
                event_union->xexpose.x,
                event_union->xexpose.y);

      if (in_area && in_window(dspl_descr))
         redo_cursor(dspl_descr, False);

   }
} /* end of exposed_window */



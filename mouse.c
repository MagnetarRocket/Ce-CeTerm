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
*  module mouse.c
*
*  Routines:
*     build_cursors    -   Build the normal arrow cursor and the blank cursor
*     normal_mcursor   -   Switch to the normal cursor
*     blank_mcursor    -   Switch to the blank mouse cursor
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h   /usr/include/sys/errno.h      */
#include <limits.h>         /* /usr/include/limits.h     */

#ifdef WIN32
#ifdef WIN32_XNT
#include "xnt.h"
#else
#include "X11/Xlib.h"       /* /usr/include/X11/Xlib.h   */
#include "X11/cursorfont.h" /* /usr/include/X11/cursorfont.h */
#endif
#else
#include <X11/Xlib.h>       /* /usr/include/X11/Xlib.h   */
#include <X11/cursorfont.h> /* /usr/include/X11/cursorfont.h */
#endif

/* get the real copy of which_cursor */
#define _MOUSE_C_ 1

#include "debug.h"
#include "emalloc.h"
#include "mouse.h"
#include "parms.h"
#include "xerrorpos.h"

/***************************************************************
*  
*  Static data hung off the display description.
*
*  The following structure is hung off the display_data field
*  in the display description.
*  
***************************************************************/

typedef struct {
     Cursor               saved_pointer_cursor;
     Cursor               saved_blank_cursor;
} CURSOR_PRIVATE;

static CURSOR_PRIVATE *check_private_data(DISPLAY_DESCR       *dspl_descr);

#define blank_width 16
#define blank_height 16
static char blank_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};



/************************************************************************

NAME:      build_cursors    -   Build the normal arrow cursor and the blank cursor

PURPOSE:    This routine does the initial create of the blank and 
            pointer cursors.

PARAMETERS:
   1.  dspl_descr     -  pointer to DISPLAY_DESCR (INPUT/OUTPUT)
                         This is the display description for the current display.
                         Cursor data both private and public and the window
                         information for the main window is used.
FUNCTIONS :
   1.   Create the mouse cursor from the build in cursor set.

   2.   Create a blank pixmap

   3.   Convert the pixmap to a cursor.


*************************************************************************/

void  build_cursors(DISPLAY_DESCR       *dspl_descr)
{

Pixmap           blank_cursor_pixmap;
XColor           foreground_color;
XColor           background_color;
XColor           rgb_db_def;
CURSOR_PRIVATE  *private;
unsigned int     main_cursor = 0;

/***************************************************************
*  Make sure the private data area is available
***************************************************************/
if ((private = check_private_data(dspl_descr)) == NULL)
   return;

if (MOUSECURSOR)
   sscanf(MOUSECURSOR, "%d", &main_cursor);

if (main_cursor == 0)
   main_cursor = XC_top_left_arrow;

/***************************************************************
*  Set up the built in arrow cursor.  This is the easy part.
*  Use the passed value,  If this fails, use the default.
***************************************************************/
DEBUG9(XERRORPOS)
private->saved_pointer_cursor = XCreateFontCursor(dspl_descr->display, main_cursor);
if (private->saved_pointer_cursor == 0)
   {
      DEBUG9(XERRORPOS)
      private->saved_pointer_cursor = XCreateFontCursor(dspl_descr->display, XC_top_left_arrow);
   }

/***************************************************************
*  build a blank pixmap cursor.
***************************************************************/
DEBUG9(XERRORPOS)
blank_cursor_pixmap = XCreateBitmapFromData(dspl_descr->display,
                                            dspl_descr->main_pad->x_window,
                                            (char *)blank_bits,
                                            blank_width,
                                            blank_height);

/***************************************************************
*  
*  Use the pixmap to make a cursor
*  
***************************************************************/

DEBUG9(XERRORPOS)
XAllocNamedColor(dspl_descr->display,
                 DefaultColormap(dspl_descr->display, DefaultScreen(dspl_descr->display)),
                 "Black",
                 &foreground_color,
                 &rgb_db_def);

DEBUG9(XERRORPOS)
XAllocNamedColor(dspl_descr->display,
                 DefaultColormap(dspl_descr->display, DefaultScreen(dspl_descr->display)),
                 "White",
                 &background_color,
                 &rgb_db_def);

DEBUG9(XERRORPOS)
private->saved_blank_cursor = XCreatePixmapCursor(dspl_descr->display,
                                         blank_cursor_pixmap,
                                         blank_cursor_pixmap,
                                         &foreground_color,
                                         &background_color,
                                         0,  /* x hot */
                                         0); /* y hot */

XFreePixmap(dspl_descr->display, blank_cursor_pixmap);

dspl_descr->which_cursor = CURSOR_NOT_SET;

} /* end of build_cursors */


/************************************************************************

NAME:      normal_mcursor   -   Switch to the normal cursor

PURPOSE:    This routine will switch to the saved normal cursor.

PARAMETERS:

   1.  dspl_descr     -  pointer to DISPLAY_DESCR (INPUT)
                         This is the display description for the current display.
                         Cursor data both private and public and the window
                         information for the main window is used.
FUNCTIONS :

   1.   Change the window referenced by the description to the saved normal cursor.


*************************************************************************/

void  normal_mcursor(DISPLAY_DESCR       *dspl_descr)
{
XSetWindowAttributes    win_attr;
CURSOR_PRIVATE         *private;

/***************************************************************
*  Make sure the private data area is available
***************************************************************/
if ((private = check_private_data(dspl_descr)) == NULL)
   return;

win_attr.cursor = private->saved_pointer_cursor;

DEBUG9(XERRORPOS)
XChangeWindowAttributes(dspl_descr->display,
                        dspl_descr->main_pad->x_window,
                        CWCursor,
                        &win_attr);

dspl_descr->which_cursor = NORMAL_CURSOR;

} /* end of normal_mcursor */


/************************************************************************

NAME:      blank_mcursor   -   Switch to the blank cursor

PURPOSE:    This routine will switch to the saved normal cursor.

PARAMETERS:

   1.  dspl_descr     -  pointer to DISPLAY_DESCR (INPUT)
                         This is the display description for the current display.
                         Cursor data both private and public and the window
                         information for the main window is used.
FUNCTIONS :

   1.   Change the window referenced by the description to the saved normal cursor.


*************************************************************************/

void  blank_mcursor(DISPLAY_DESCR       *dspl_descr)
{
XSetWindowAttributes    win_attr;
CURSOR_PRIVATE         *private;

/***************************************************************
*  Make sure the private data area is available
***************************************************************/
if ((private = check_private_data(dspl_descr)) == NULL)
   return;

win_attr.cursor = private->saved_blank_cursor;

DEBUG9(XERRORPOS)
XChangeWindowAttributes(dspl_descr->display,
                        dspl_descr->main_pad->x_window,
                        CWCursor,
                        &win_attr);

dspl_descr->which_cursor = BLANK_CURSOR;

} /* end of blank_mcursor */


/************************************************************************

NAME:      check_private_data - get the cursor private data from the display description.

PURPOSE:    This routine returns the existing cursor private area
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
   private  -  pointer to CURSOR_PRIVATE
               This is the private static area used by the
               routines in this module.
               NULL is returned on malloc failure only.

*************************************************************************/

static CURSOR_PRIVATE *check_private_data(DISPLAY_DESCR       *dspl_descr)
{
CURSOR_PRIVATE    *private;

if (dspl_descr->cursor_data)
   return((CURSOR_PRIVATE *)dspl_descr->cursor_data);

private = (CURSOR_PRIVATE *)CE_MALLOC(sizeof(CURSOR_PRIVATE));
if (!private)
   return(NULL);   
else
   memset((char *)private, 0, sizeof(CURSOR_PRIVATE));

dspl_descr->cursor_data = (void *)private;

return(private);

} /* end of check_private_data */




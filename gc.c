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
*  module gc.c
*
*  Routines:
*         create_gcs          - Create the GC's for a window.
*         dup_gc              - Duplicate a GC with changes
*         dump_gc             - Dump Graphics Content info.
*         pix_value           - Get the pixel value under an X Y coordinate
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

#include "debug.h"
#include "gc.h"
#include "xerrorpos.h"


/************************************************************************

NAME:      create_gcs

PURPOSE:    This routine will create the GCs (graphics contents) used by
            a window description.  These are the basic gc,
            the reverse_video gc (clearing areas),
            and the XOR gc (for text cursor stuff).

PARAMETERS:


   1.  display       - Display (Xlib type) (INPUT)
                       This is the display token needed for Xlib calls.

   2.  main_x_window - Window (Xlib type) (INPUT)
                       This is the X window id of the main window.
                       The new gc's are associated with this window.

   3.  window        - pointer to DRAWABLE_DESCR (OUTPUT)
                       This is the window to be updated.
                       It is the main window description.

   4.  gcvalues_parm -  pointer to XGCValues (INPUT)
                        This is the data needed to load into the gc's created.
                        At minimum, the following fields must be filled in:
                        background, foreground, linewidth, graphics_exposures, and font.


FUNCTIONS :

   1.   Build the main gc.

   2.   Build the reverse gc.

   3.   Build the XOR GC.

*************************************************************************/

void   create_gcs(Display         *display,
                  Window           main_x_window,
                  DRAWABLE_DESCR  *window,
                  XGCValues       *gcvalues_parm)
{
unsigned long         temp_pixel;
unsigned long         gc_mask;
XGCValues             gcvalues;

/***************************************************************
*  
*  Some sanity checking.  Make sure foreground and background are different.
*  
***************************************************************/

if (gcvalues_parm->foreground == gcvalues_parm->background)
   if (gcvalues_parm->foreground == 0)
      gcvalues_parm->foreground = 1;
   else
      gcvalues_parm->foreground = 0;


memcpy((char *)&gcvalues, (char *)gcvalues_parm, sizeof(XGCValues));

/****************************************************************
*
*   Build the gc for the screen and pixmap.
*
****************************************************************/

gc_mask = GCForeground
        | GCBackground
        | GCLineWidth
        | GCGraphicsExposures
        | GCFont;


DEBUG9(XERRORPOS)
window->gc = XCreateGC(display,
                      main_x_window,
                      gc_mask,
                      &gcvalues
                    );


temp_pixel          = gcvalues.foreground;
gcvalues.foreground = gcvalues.background;
gcvalues.background = temp_pixel;

DEBUG9(XERRORPOS)
window->reverse_gc = XCreateGC(display,
                      main_x_window,
                      gc_mask,
                      &gcvalues
                    );

memcpy((char *)&gcvalues, (char *)gcvalues_parm, sizeof(XGCValues));
gcvalues.foreground = gcvalues.background ^ gcvalues.foreground;
DEBUG12(fprintf(stderr, "create_gcs:  Xor background pix value: %d, Xor foreground pix value: %d\n",
                       gcvalues.background, gcvalues.foreground);
)


/***************************************************************
*  Build the xor GC.
***************************************************************/
gc_mask = GCFunction
        | GCForeground
        | GCBackground
        | GCGraphicsExposures;

gcvalues.graphics_exposures = False;
gcvalues.function = GXxor;

DEBUG9(XERRORPOS)
window->xor_gc = XCreateGC(display,
                         main_x_window,
                         gc_mask,
                         &gcvalues);


}  /* end of create_gcs */




/************************************************************************

NAME:      dup_gc  -  Duplicate a gc with changes

PURPOSE:    This routine will duplicate a gc and add in the changes passed.

PARAMETERS:

   1.  display   -  Pointer to Display (INPUT)
                    This is the Display which is open.

   2.  drawable -   drawable (INPUT)
                    This is the drawable the GC will be associated with.

   3.  old_gc   -   GC (INPUT)
                    This is the source GC to be duplicated

   4.  new_valuemask   -   unsigned long (INPUT)
                    This is the valuemask which marks which new values are
                    being supplied.

   5.  new_gcvalues   -   pointer to XGCValues (INPUT)
                    This parm contains the new gc values being supplied.


FUNCTIONS :


   1.   Get the values from the GC.

   2.   Copy any values in the new_gcvalues to the existing one

   3.   Create a new GC with the merge of the old and new values.


OUTPUTS:
    new_gc   -   GC (RETURNED VALUE)
                 The new GC is returned.  On error, a value
                 of (GC)-1 is returned.



    
*************************************************************************/

GC   dup_gc(Display       *display,
            Drawable       drawable,
            GC             old_gc,
            unsigned long  new_valuemask,
            XGCValues     *new_gcvalues)
{

XGCValues      gcvalues;
unsigned long  valuemask;
GC             new_gc;

valuemask = GCFunction
          | GCPlaneMask
          | GCForeground
          | GCBackground
          | GCLineWidth
          | GCLineStyle
          | GCCapStyle
          | GCJoinStyle
          | GCFillStyle
          | GCFillRule
          | GCTile
          | GCStipple
          | GCTileStipXOrigin
          | GCTileStipYOrigin
          | GCFont
          | GCSubwindowMode
          | GCGraphicsExposures
          | GCClipXOrigin
          | GCClipYOrigin
          | GCDashOffset
          | GCArcMode;


if (!XGetGCValues(display,
                  old_gc,
                  valuemask,
                  &gcvalues))
   return((GC)-1);

if (new_valuemask & GCFunction)
   gcvalues.function = new_gcvalues->function;
if (new_valuemask & GCPlaneMask)
   gcvalues.plane_mask = new_gcvalues->plane_mask;
if (new_valuemask & GCForeground)
   gcvalues.foreground = new_gcvalues->foreground;
if (new_valuemask & GCBackground)
   gcvalues.background = new_gcvalues->background;
if (new_valuemask & GCLineWidth)
   gcvalues.line_width = new_gcvalues->line_width;
if (new_valuemask & GCLineStyle)
   gcvalues.line_style = new_gcvalues->line_style;
if (new_valuemask & GCCapStyle)
   gcvalues.cap_style = new_gcvalues->cap_style;
if (new_valuemask & GCJoinStyle)
   gcvalues.join_style = new_gcvalues->join_style;
if (new_valuemask & GCFillStyle)
   gcvalues.fill_style = new_gcvalues->fill_style;
if (new_valuemask & GCFillRule)
   gcvalues.fill_rule = new_gcvalues->fill_rule;
if (new_valuemask & GCTile)
   gcvalues.tile = new_gcvalues->tile;
else
   if (gcvalues.tile == (Pixmap)-1)
      valuemask &= ~GCTile;
if (new_valuemask & GCStipple)
   gcvalues.stipple = new_gcvalues->stipple;
else
   if (gcvalues.stipple == (Pixmap)-1)
      valuemask &= ~GCStipple;
if (new_valuemask & GCTileStipXOrigin)
   gcvalues.ts_x_origin = new_gcvalues->ts_x_origin;
if (new_valuemask & GCTileStipYOrigin)
   gcvalues.ts_y_origin = new_gcvalues->ts_y_origin;
if (new_valuemask & GCFont)
   gcvalues.font = new_gcvalues->font;
if (new_valuemask & GCSubwindowMode)
   gcvalues.subwindow_mode = new_gcvalues->subwindow_mode;
if (new_valuemask & GCGraphicsExposures)
   gcvalues.graphics_exposures = new_gcvalues->graphics_exposures;
if (new_valuemask & GCClipXOrigin)
   gcvalues.clip_x_origin = new_gcvalues->clip_x_origin;
if (new_valuemask & GCClipYOrigin)
   gcvalues.clip_y_origin = new_gcvalues->clip_y_origin;
if (new_valuemask & GCDashOffset)
   gcvalues.dash_offset = new_gcvalues->dash_offset;
if (new_valuemask & GCArcMode)
   gcvalues.arc_mode = new_gcvalues->arc_mode;

DEBUG9(XERRORPOS)
new_gc = XCreateGC(display, drawable, valuemask, &gcvalues);

return(new_gc);
 
} /* end of dup_gc */


#ifdef  DebuG
/************************************************************************

NAME:      dump_gc  -  debugging routine

PURPOSE:    This routine will format and dump the values in a GC.

PARAMETERS:

   1.  stream   -   Pointer to File (INPUT)
                    This is the stream file to write to.
                    It must already be open.  Values such
                    as stdout or stderr work fine.  Or you
                    may fopen a file and pass the returned
                    pointer.

   2.  display  -   Pointer to Display (INPUT)
                    This is the pointer to the current display.
                    The value returned by XOpenDisplay.

   3.  gc       -   GC (INPUT)
                    This is the graphics content to be dumped.

   2.  header   -   Pointer to char (INPUT)
                    This is a pointer to a string to be output at
                    the head of the dump.



FUNCTIONS :


   1.   Get the values from the GC.

   2.   Print all the values.



    
*************************************************************************/

void dump_gc(FILE         *stream,
             Display      *display,
             GC            gc,
             char         *header)
{

XGCValues      gcvalues;
unsigned long  valuemask;
char           temp1[40];

memset((char *)&gcvalues, 0, sizeof(gcvalues));
valuemask = GCFunction
          | GCPlaneMask
          | GCForeground
          | GCBackground
          | GCLineWidth
          | GCLineStyle
          | GCCapStyle
          | GCJoinStyle
          | GCFillStyle
          | GCFillRule
          | GCTile
          | GCStipple
          | GCTileStipXOrigin
          | GCTileStipYOrigin
          | GCFont
          | GCSubwindowMode
          | GCGraphicsExposures
          | GCClipXOrigin
          | GCClipYOrigin
          | GCDashOffset
          | GCArcMode;


DEBUG9(XERRORPOS)
if (!XGetGCValues(display,
                  gc,
                  valuemask,
                  &gcvalues))
{
   fprintf(stream, "XGetGCValues failure for GC = 0x%08X\n", gc);
   return;
}
 
translate_gc_function(gcvalues.function, temp1);

if (header != NULL)
   fprintf(stream, "%s\n", header);
fprintf(stream, "     background = %d   foreground = %d  function = %s (0x%x)  gc = 0x%X  plane_mask = 0x%X\n",
                gcvalues.background, gcvalues.foreground, temp1, gcvalues.function, gc, gcvalues.plane_mask);

fprintf(stream, "     line_width = %d  line_style = %s (0x%X)\n     cap_style = %s (0x%X)  join_style = %s (0x%X)\n",
    gcvalues.line_width, 
    ((gcvalues.line_style == LineSolid) ? "LineSolid" : ((gcvalues.line_style == LineOnOffDash) ? "LineOnOffDash" : ((gcvalues.line_style == LineOnOffDash) ? "LineDoubleDash" : "Unknown") ) ), gcvalues.line_style,
    ((gcvalues.cap_style == CapNotLast) ? "CapNotLast" : ((gcvalues.cap_style == CapButt) ? "CapButt" : ((gcvalues.cap_style == CapRound) ? "CapRound" : ((gcvalues.cap_style == CapProjecting) ? "CapProjecting" : "Unknown") ) ) ), gcvalues.cap_style,
    ((gcvalues.join_style == JoinMiter) ? "JoinMiter" : ((gcvalues.join_style == JoinRound) ? "JoinRound" : ((gcvalues.join_style == JoinBevel) ? "JoinBevel" : "Unknown") ) ), gcvalues.join_style
       );
fprintf(stream, "     fill_style = %s (0x%X)  fill_rule = %s (0x%X)  arc_mode = %s (0x%X)\n",
 ((gcvalues.fill_style == FillSolid) ? "FillSolid" : ((gcvalues.fill_style == FillTiled) ? "FillTiled" : ((gcvalues.fill_style == FillStippled) ? "FillStippled" : ((gcvalues.fill_style == FillOpaqueStippled) ? "FillOpaqueStippled" : "Unknown")))),
 gcvalues.fill_style,
 ((gcvalues.fill_rule == EvenOddRule) ? "EvenOddRule" : ((gcvalues.fill_rule == WindingRule) ? "WindingRule" : "Unknown" ) ),
  gcvalues.fill_rule,
 ((gcvalues.arc_mode == ArcChord) ? "ArcChord" : ((gcvalues.arc_mode == ArcPieSlice) ? "ArcPieSlice" : "Unknown" ) ),
 gcvalues.arc_mode
       );
fprintf(stream, "     tile = 0x%X  stipple = 0x%X  ts_x_origin = %d  ts_y_origin = %d  font = 0x%08X\n",
    gcvalues.tile, gcvalues.stipple, gcvalues.ts_x_origin, gcvalues.ts_y_origin, gcvalues.font
       );
fprintf(stream, "     subwindow_mode = %s (0x%X)  graphics_exposures = %s  clip_x_origin = %d  clip_y_origin = %d\n     dash_offset = %d\n",
    ((gcvalues.subwindow_mode == ClipByChildren) ? "ClipByChildren" : ((gcvalues.subwindow_mode == IncludeInferiors) ? "IncludeInferiors" : "Unknown" ) ), gcvalues.subwindow_mode,
    ((gcvalues.graphics_exposures) ? "True" : "False"), gcvalues.clip_x_origin,  gcvalues.clip_y_origin, gcvalues.dash_offset
       );

} /* end of dump_gc */


/************************************************************************

NAME:      pix_value

PURPOSE:    This routine get the pixel value from an XY coordinate on
            a window or pixmap.

PARAMETERS:

   1.  display       - Display (Xlib type) (INPUT)
                       This is the display token needed for Xlib calls.

   2.  main_x_window - Window (Xlib type) (INPUT)
                       This is the X window id of the main window.
                       The new gc's are associated with this window.

   2.  x       - int (INPUT)
                 This is the x coordiate of the pixel to extract.

   3.  y       - int (INPUT)
                 This is the y coordiate of the pixel to extract.


FUNCTIONS :

   1.   Get the image from the server.

   2.   Extract the pix value.

   3.   Get rid of the image.

RETURNED VALUE:

   pixel  -  unsigned int
             The returned value is the pixel value.


*************************************************************************/

unsigned long pix_value(Display         *display,
                        Window           main_x_window,
                        int              x,
                        int              y)
{

static   XImage      *work_image = NULL;
unsigned long         pixel;


/***************************************************************
*  
*  Get the image from the window.
*  
***************************************************************/

DEBUG9(XERRORPOS)
work_image = XGetImage(display, 
                       main_x_window,
                       x, y,
                       1, 1,
                       AllPlanes,
                       ZPixmap);


if (work_image == None)
   return((unsigned long)-1);

DEBUG9(XERRORPOS)
pixel = XGetPixel(work_image, 0, 0);

DEBUG9(XERRORPOS)
XDestroyImage(work_image);

return(pixel);

}  /* end of pix_value */
#endif







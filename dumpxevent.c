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
*  module dumpxevent.c
*
*  Routines:
*         dump_Xevent                 - Format and dump an X event, debugging only
*         translate_buttonmask        - Translate a button mask into a string
*
*  Local:
*         translate_detail            - Translate notify X event data into a string
*         translate_configure_detail  - Translate detail field from a configure request into a string
*         translate_mode              - Translate xcrossing data into a string 
*         translate_visibility        - Translate visibility state data into a string
* 
*
***************************************************************/

#include <stdio.h>       /* /usr/include/stdio.h      */
#include <string.h>      /* /bsd4.3/usr/include/string.h     */

#ifdef WIN32
#ifdef WIN32_XNT
#include "xnt.h"
#else
#include "X11/X.h"       /* /usr/include/X11/X.h      */
#include "X11/Xlib.h"    /* /usr/include/X11/Xlib.h   */
#include "X11/Xutil.h"   /* /usr/include/X11/Xutil.h  */
#include "X11/Xproto.h"  /* /usr/include/X11/Xproto.h */
#endif
#else
#include <X11/X.h>       /* /usr/include/X11/X.h      */
#include <X11/Xlib.h>    /* /usr/include/X11/Xlib.h   */
#include <X11/Xutil.h>   /* /usr/include/X11/Xutil.h  */
#include <X11/Xproto.h>  /* /usr/include/X11/Xproto.h */
#endif

#include "dumpxevent.h"

/***************************************************************
*  
*  dump_Xevent and the local routines are only needed when
*  compiling for debug.
*  
***************************************************************/

#ifdef DebuG

/**************************************************************
*  
*  Routine dump_Xevent
*
*   This routine will format and dump an X window system
*   event structure to the file descriptor passed.
*
* PARAMETERS:
*
*   stream      -   Pointer to File (INPUT)
*                   This is the stream file to write to.
*                   It must already be open.  Values such
*                   as stderr or stdout work fine.  Or you
*                   may fopen a file and pass the returned
*                   pointer.
*
*   event       -   Pointer to XEvent (INPUT)
*                   The event to dump.  This is the event
*                   structure returned bu a call to XNextEvent,
*                   An event handler in motif, or the event
*                   structure from a calldata parm in a motif
*                   widget callback routine.
*
*   level       -   int (INPUT)
*                   This parm indicates how much stuff to dump
*                   Values:
*                   0  -  Do nothing, just return
*                   1  -  Dump just the name of the event
*                   2  -  Dump the major interesting fields
*                   3  -  Dump all the fields
*
*  FUNCTIONS:
*
*  1.   If the level is zero or less, just return.
*
*  2.   Switch on the event type and print the name of the event
*
*  3.   Based on the level parm format and print some or all
*       of the fields.  In general, level 3 prints the
*       serial number, display token, and whether this event
*       was generated by send event.
*       
*
*  OUTPUTS:
*   Data is written to the file referenced by input parm stream.
*
*
*  SAMPLE USAGE:
*
*     Display  *display;
*     XEvent    event_union;
*     int       debug;
*
*     while (1)
*     {
*         XNextEvent (display, &event_union);
*         if (debug)
*            dump_Xevent(stderr, &event_union, debug);
*         switch (event_union.type)
*         {
*            ...
*  
***************************************************************/

static void translate_detail(int              detail,
                             char            *work);

static void translate_configure_detail(int              detail,
                                       char            *work);

static void translate_mode(int              mode,
                           char            *work);

static void translate_visibility(int              state,
                                 char            *work);

#define itoa(v, s) ((sprintf(s, "UNKNOWN VALUE (%d)", (v))), (s))



void dump_Xevent(FILE     *stream,
                 XEvent   *event,
                 int       level)
{
char     work[256];
char     work2[256];
char     work3[256];
int      len;
char    *x_key_name;
KeySym   keysym;

if (level <= 0)
   return;

switch(event->type)
{
case  KeyPress:
case  KeyRelease:
   if (event->type == KeyPress)
      fprintf(stream, "event  KeyPress\n");
   else
      fprintf(stream, "event  KeyRelease\n");
   if (level > 1)
      {
         translate_buttonmask(event->xkey.state, work);
         len = XLookupString((XKeyEvent *)event, work2, sizeof(work2), &keysym,  NULL);
         work2[len] = '\0';
         x_key_name = XKeysymToString(keysym);
         if (x_key_name == NULL)
            x_key_name = "";
         fprintf(stream, "     keysym = \"%s\" (0x%02X), state = %s (0x%02X)\n     X key name = \"%s\",  hardware keycode = 0x%02X\n     x = %d, y = %d\n     x_root = %d, y_root = %d\n     window = 0x%02X, same_screen = %s\n",
                        work2, keysym,
                        work, event->xkey.state,
                        x_key_name, event->xkey.keycode,
                        event->xkey.x, event->xkey.y,
                        event->xkey.x_root, event->xkey.y_root,
                        event->xkey.window,
                        (event->xkey.same_screen ? "True" : "False"));
      }
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, root = 0x%X, subwindow = 0x%02X, time = %d, serial = %u\n",
                     event->xkey.display,
                     (event->xkey.send_event ? "True" : "False"),
                     event->xkey.root,
                     event->xkey.subwindow,
                     event->xkey.time,
                     event->xkey.serial);
   break;

case  ButtonPress:
case  ButtonRelease:
   if (event->type == ButtonPress)
      fprintf(stream, "event  ButtonPress\n");
   else
      fprintf(stream, "event  ButtonRelease\n");
   if (level > 1)
      {
         translate_buttonmask(event->xkey.state, work);
         fprintf(stream, "     button = %u, state = %s (0x%02X)\n     x = %d, y = %d\n     x_root = %d, y_root = %d\n     window = 0x%02X, same_screen = %s\n",
                        event->xbutton.button,
                        work, event->xbutton.state,
                        event->xbutton.x, event->xbutton.y,
                        event->xbutton.x_root, event->xbutton.y_root,
                        event->xbutton.window,
                        (event->xbutton.same_screen ? "True" : "False"));
      }
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, root = 0x%X, subwindow = 0x%02X, time = %d, serial = %u\n",
                     event->xbutton.display,
                     (event->xbutton.send_event ? "True" : "False"),
                     event->xbutton.root,
                     event->xbutton.subwindow,
                     event->xbutton.time,
                     event->xbutton.serial);
   break;

case  MotionNotify:
   fprintf(stream, "event  MotionNotify\n");
   if (level > 1)
      {
         translate_buttonmask(event->xkey.state, work);
         fprintf(stream, "     is_hint = 0x%02x, state = %s (0x%02X)\n     x = %d, y = %d\n     x_root = %d, y_root = %d\n     window = 0x%02X, same_screen = %s\n",
                        event->xmotion.is_hint,
                        work, event->xmotion.state,
                        event->xmotion.x, event->xmotion.y,
                        event->xmotion.x_root, event->xmotion.y_root,
                        event->xmotion.window,
                        (event->xmotion.same_screen ? "True" : "False"));
      }
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, root = 0x%X, subwindow = 0x%02X, time = %u, serial = %u\n",
                     event->xmotion.display,
                     (event->xmotion.send_event ? "True" : "False"),
                     event->xmotion.root,
                     event->xmotion.subwindow,
                     event->xmotion.time,
                     event->xmotion.serial);
   break;

case  EnterNotify:
case  LeaveNotify:
   if (event->type == EnterNotify)
      fprintf(stream, "event  EnterNotify\n");
   else
      fprintf(stream, "event  LeaveNotify\n");
   if (level > 1)
      {
         translate_buttonmask(event->xcrossing.state, work);
         translate_detail(event->xcrossing.detail, work2);
         translate_mode(event->xcrossing.mode, work3);
         fprintf(stream, "     mode = %s (0x%02X),  state = %s (0x%02X), detail = %s (0x%02X)\n     x = %d, y = %d\n     x_root = %d, y_root = %d\n     focus = %s, window = 0x%02X, same_screen = %s\n",
                        work3, event->xcrossing.mode,
                        work, event->xcrossing.state,
                        work2, event->xcrossing.detail,
                        event->xcrossing.x, event->xcrossing.y,
                        event->xcrossing.x_root, event->xcrossing.y_root,
                        (event->xcrossing.focus ? "True" : "False"),
                        event->xcrossing.window,
                        (event->xcrossing.same_screen ? "True" : "False"));
      }
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, root = 0x%X, subwindow = 0x%X, time = %d, serial = %u\n",
                     event->xcrossing.display,
                     (event->xcrossing.send_event ? "True" : "False"),
                     event->xcrossing.root,
                     event->xcrossing.subwindow,
                     event->xcrossing.time,
                     event->xcrossing.serial);
   break;

case  FocusIn:
case  FocusOut:
   if (event->type == FocusIn)
      fprintf(stream, "event  FocusIn\n");
   else
      fprintf(stream, "event  FocusOut\n");
   if (level > 1)
      {
         translate_detail(event->xfocus.detail, work2);
         translate_mode(event->xfocus.mode, work3);
         fprintf(stream, "     mode = %s (0x%02X), detail = %s (0x%02X), window = 0x%02X\n",
                        work3, event->xfocus.mode,
                        work2, event->xfocus.detail,
                        event->xfocus.window);
      }
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xfocus.display,
                     (event->xfocus.send_event ? "True" : "False"),
                     event->xfocus.serial);
   break;

case  KeymapNotify:
   fprintf(stream, "event  KeymapNotify\n");
   if (level > 1)
      {
         fprintf(stream, "     key_vector = %08X %08X %08X %08X %08X %08X %08X %08X, window = 0x%02X\n",
                        *(int *)&event->xkeymap.key_vector[0],
                        *(int *)&event->xkeymap.key_vector[4],
                        *(int *)&event->xkeymap.key_vector[8],
                        *(int *)&event->xkeymap.key_vector[12],
                        *(int *)&event->xkeymap.key_vector[16],
                        *(int *)&event->xkeymap.key_vector[20],
                        *(int *)&event->xkeymap.key_vector[24],
                        *(int *)&event->xkeymap.key_vector[28],
                        event->xkeymap.window);
      }
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xkeymap.display,
                     (event->xkeymap.send_event ? "True" : "False"),
                     event->xkeymap.serial);
   break;

case  Expose:
   fprintf(stream, "event  Expose\n");
   if (level > 1)
      fprintf(stream, "     x = %d, y = %d, width = %d, height = %d\n     window = 0x%02X, count = %d\n",
                     event->xexpose.x, event->xexpose.y,
                     event->xexpose.width, event->xexpose.height,
                     event->xexpose.window,
                     event->xexpose.count);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xexpose.display,
                     (event->xexpose.send_event ? "True" : "False"),
                     event->xexpose.serial);
   break;

#ifndef WIN32
case  GraphicsExpose:
   fprintf(stream, "event  GraphicsExpose\n");
   if (level > 1)
      fprintf(stream, "     x = %d, y = %d, width = %d, height = %d\n     drawable = 0x%02X, count = %d\n     major_code = %s, minor_code = %d\n",
                     event->xgraphicsexpose.x, event->xgraphicsexpose.y,
                     event->xgraphicsexpose.width, event->xgraphicsexpose.height,
                     event->xgraphicsexpose.drawable,
                     event->xgraphicsexpose.count,
                     ((event->xgraphicsexpose.major_code == X_CopyArea) ? "CopyArea" : ((event->xgraphicsexpose.major_code == X_CopyPlane) ? "CopyPlane" : itoa(event->xgraphicsexpose.major_code, work))),
                     event->xgraphicsexpose.minor_code);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xgraphicsexpose.display,
                     (event->xgraphicsexpose.send_event ? "True" : "False"),
                     event->xgraphicsexpose.serial);
   break;

case  NoExpose:
   fprintf(stream, "event  NoExpose\n");
   if (level > 1)
      fprintf(stream, "     drawable = 0x%X\n     major_code = %s, minor_code = %d\n",
                     event->xnoexpose.drawable,
                     ((event->xnoexpose.major_code == X_CopyArea) ? "CopyArea" : ((event->xnoexpose.major_code == X_CopyArea) ? "CopyPlane" : itoa(event->xnoexpose.major_code, work))),
                     event->xnoexpose.minor_code);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xnoexpose.display,
                     (event->xnoexpose.send_event ? "True" : "False"),
                     event->xnoexpose.serial);
   break;
#endif

case  VisibilityNotify:
   fprintf(stream, "event  VisibilityNotify\n");
   if (level > 1)
      {
         translate_visibility(event->xvisibility.state, work);
         fprintf(stream, "     state = %s (0x%02X), window = 0x%02X\n",
                        work, event->xvisibility.state,
                        event->xvisibility.window);
      }
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xvisibility.display,
                     (event->xvisibility.send_event ? "True" : "False"),
                     event->xvisibility.serial);
   break;

case  CreateNotify:
   fprintf(stream, "event  CreateNotify\n");
   if (level > 1)
      fprintf(stream, "     x = %d, y = %d, width = %d, height = %d\n     Border width = %d, override_redirect = %s\n     window = 0x%X, Parent = 0x%X\n",
                     event->xcreatewindow.x, event->xcreatewindow.y,
                     event->xcreatewindow.width, event->xcreatewindow.height,
                     event->xcreatewindow.border_width,
                     (event->xcreatewindow.override_redirect ? "True" : "False"),
                     event->xcreatewindow.window,
                     event->xcreatewindow.parent);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xcreatewindow.display,
                     (event->xcreatewindow.send_event ? "True" : "False"),
                     event->xcreatewindow.serial);
   break;

case  DestroyNotify:
   fprintf(stream, "event  DestroyNotify\n");
   if (level > 1)
      fprintf(stream, "     event = 0x%X, window = 0x%X\n",
                     event->xdestroywindow.event,
                     event->xdestroywindow.window);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xdestroywindow.display,
                     (event->xdestroywindow.send_event ? "True" : "False"),
                     event->xdestroywindow.serial);
   break;

case  UnmapNotify:
   fprintf(stream, "event  UnmapNotify\n");
   if (level > 1)
      fprintf(stream, "     event = 0x%X, window = 0x%X, from_configure = %s\n",
                     event->xunmap.event,
                     event->xunmap.window,
                     (event->xunmap.from_configure ? "True" : "False"));
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xunmap.display,
                     (event->xunmap.send_event ? "True" : "False"),
                     event->xunmap.serial);
   break;

case  MapNotify:
   fprintf(stream, "event  MapNotify\n");
   if (level > 1)
      fprintf(stream, "     event = 0x%X, window = 0x%X, override_redirect = %s\n",
                     event->xmap.event,
                     event->xmap.window,
                     (event->xmap.override_redirect ? "True" : "False"));
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xmap.display,
                     (event->xmap.send_event ? "True" : "False"),
                     event->xmap.serial);
   break;

case  MapRequest:
   fprintf(stream, "event  MapRequest\n");
   if (level > 1)
      fprintf(stream, "     parent = 0x%X, window = 0x%02X\n",
                     event->xmaprequest.parent,
                     event->xmaprequest.window);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xmaprequest.display,
                     (event->xmaprequest.send_event ? "True" : "False"),
                     event->xmaprequest.serial);
   break;

case  ReparentNotify:
   fprintf(stream, "event  ReparentNotify\n");
   if (level > 1)
      fprintf(stream, "     event = 0x%X, window = 0x%02X, parent = 0x%X\n     x = %d, y = %d, override_redirect = %s\n",
                     event->xreparent.event,
                     event->xreparent.window,
                     event->xreparent.parent,
                     event->xreparent.x,
                     event->xreparent.y,
                     (event->xreparent.override_redirect ? "True" : "False"));
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xreparent.display,
                     (event->xreparent.send_event ? "True" : "False"),
                     event->xreparent.serial);
   break;

case  ConfigureNotify:
   fprintf(stream, "event  ConfigureNotify\n");
   if (level > 1)
      fprintf(stream, "     x = %d, y = %d, width = %d, height = %d\n     Border width = %d, override_redirect = %s\n     Event = 0x%X, window = 0x%X, above = 0x%X\n",
                     event->xconfigure.x, event->xconfigure.y,
                     event->xconfigure.width, event->xconfigure.height,
                     event->xconfigure.border_width,
                     (event->xconfigure.override_redirect ? "True" : "False"),
                     event->xconfigure.event,
                     event->xconfigure.window,
                     event->xconfigure.above);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xconfigure.display,
                     (event->xconfigure.send_event ? "True" : "False"),
                     event->xconfigure.serial);
   break;

case  ConfigureRequest:
   fprintf(stream, "event  ConfigureRequest\n");
   if (level > 1)
      {
      translate_configure_detail(event->xconfigurerequest.detail, work);
      fprintf(stream, "     x = %d, y = %d, width = %d, height = %d\n     Border width = %d, detail = %s, value_mask = 0x%08X\n     parent = 0x%X, window = 0x%X, above = 0x%X\n",
                     event->xconfigurerequest.x, event->xconfigurerequest.y,
                     event->xconfigurerequest.width, event->xconfigurerequest.height,
                     event->xconfigurerequest.border_width,
                     work,
                     event->xconfigurerequest.value_mask,
                     event->xconfigurerequest.parent,
                     event->xconfigurerequest.window,
                     event->xconfigurerequest.above);
      }
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xconfigurerequest.display,
                     (event->xconfigurerequest.send_event ? "True" : "False"),
                     event->xconfigurerequest.serial);
   break;

case  GravityNotify:
   fprintf(stream, "event  GravityNotify\n");
   if (level > 1)
      fprintf(stream, "     x = %d, y = %d     Event = %d, window = 0x%02X\n",
                     event->xgravity.x, event->xgravity.y,
                     event->xgravity.event,
                     event->xgravity.window);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xgravity.display,
                     (event->xgravity.send_event ? "True" : "False"),
                     event->xgravity.serial);
   break;

case  ResizeRequest:
   fprintf(stream, "event  ResizeRequest\n");
   if (level > 1)
      fprintf(stream, "     width = %d, height = %d\n     window = 0x%X\n",
                     event->xresizerequest.width, event->xresizerequest.height,
                     event->xconfigurerequest.window);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xresizerequest.display,
                     (event->xresizerequest.send_event ? "True" : "False"),
                     event->xresizerequest.serial);
   break;

case  CirculateNotify:
   fprintf(stream, "event  CirculateNotify\n");
   if (level > 1)
      fprintf(stream, "     window = 0x%X, event = 0x%X, place = %s\n",
                     event->xcirculate.window,
                     event->xcirculate.event,
                     ((event->xcirculate.place == PlaceOnTop) ? "PlaceOnTop" : ((event->xcirculate.place == PlaceOnBottom) ? "PlaceOnBottom" : itoa(event->xcirculate.place, work))));
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xcirculate.display,
                     (event->xcirculate.send_event ? "True" : "False"),
                     event->xcirculate.serial);
   break;

case  CirculateRequest:
   fprintf(stream, "event  CirculateRequest\n");
   if (level > 1)
      fprintf(stream, "     window = 0x%X, parent = 0x%X, place = %s\n",
                     event->xcirculaterequest.window,
                     event->xcirculaterequest.parent,
                     ((event->xcirculaterequest.place == PlaceOnTop) ? "PlaceOnTop" : ((event->xcirculaterequest.place == PlaceOnBottom) ? "PlaceOnBottom" : itoa(event->xcirculaterequest.place, work))));
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xcirculaterequest.display,
                     (event->xcirculaterequest.send_event ? "True" : "False"),
                     event->xcirculaterequest.serial);
   break;

case  PropertyNotify:
   fprintf(stream, "event  PropertyNotify\n");
   if (level > 1)
      fprintf(stream, "     window = 0x%X, atom = %d, state = %s, time = %d\n",
                     event->xproperty.window,
                     event->xproperty.atom,
                     ((event->xproperty.state == PropertyNewValue) ? "PropertyNewValue" : ((event->xproperty.state == PropertyDelete) ? "PropertyDelete" : itoa(event->xproperty.state, work))),
                     event->xproperty.time);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xproperty.display,
                     (event->xproperty.send_event ? "True" : "False"),
                     event->xproperty.serial);
   break;

case  SelectionClear:
   fprintf(stream, "event  SelectionClear\n");
   if (level > 1)
      fprintf(stream, "     window = 0x%02X, selection = %d, time = %d\n",
                     event->xselectionclear.window,
                     event->xselectionclear.selection,
                     event->xselectionclear.time);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xselectionclear.display,
                     (event->xselectionclear.send_event ? "True" : "False"),
                     event->xselectionclear.serial);
   break;

case  SelectionRequest:
   fprintf(stream, "event  SelectionRequest\n");
   if (level > 1)
      fprintf(stream, "     owner = 0x%X, requestor = 0x%X, selection = %d\n     target = 0x%02X, property = 0X%02X, time = %d\n",
                     event->xselectionrequest.owner,
                     event->xselectionrequest.requestor,
                     event->xselectionrequest.selection,
                     event->xselectionrequest.target,
                     event->xselectionrequest.property,
                     event->xselectionrequest.time);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xselectionrequest.display,
                     (event->xselectionrequest.send_event ? "True" : "False"),
                     event->xselectionrequest.serial);
   break;

case  SelectionNotify:
   fprintf(stream, "event  SelectionNotify\n");
   if (level > 1)
      fprintf(stream, "     requestor = 0x%X, selection = %d,  target = %d\n     property = 0X%02X, time = %d\n",
                     event->xselection.requestor,
                     event->xselection.selection,
                     event->xselection.target,
                     event->xselection.property,
                     event->xselection.time);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xselection.display,
                     (event->xselection.send_event ? "True" : "False"),
                     event->xselection.serial);
   break;

case  ColormapNotify:
   fprintf(stream, "event  ColormapNotify\n");
   if (level > 1)
      fprintf(stream, "     colormap = %s, new = %s, state = %s window = 0x%02X\n",
                     ((event->xcolormap.colormap == None) ? "None" : itoa(event->xcolormap.colormap, work)),
                     (event->xcolormap.new ? "True" : "False"),
                     ((event->xcolormap.state == ColormapInstalled) ? "ColormapInstalled" : ((event->xcolormap.state == ColormapUninstalled) ? "ColormapUninstalled" : itoa(event->xcolormap.state, work))),
                     event->xcolormap.window);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xcolormap.display,
                     (event->xcolormap.send_event ? "True" : "False"),
                     event->xcolormap.serial);
   break;

case  ClientMessage:
   fprintf(stream, "event  ClientMessage\n");
   if (level > 1)
      fprintf(stream, "     type = 0x%02X, format = 0x%02X,\n     data = %08X %08X %08X %08X %08X\n     window = 0x%02X\n",
                     event->xclient.message_type,
                     event->xclient.format,
                     event->xclient.data.l[0],
                     event->xclient.data.l[1],
                     event->xclient.data.l[2],
                     event->xclient.data.l[3],
                     event->xclient.data.l[4],
                     event->xclient.window);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xclient.display,
                     (event->xclient.send_event ? "True" : "False"),
                     event->xclient.serial);
   break;

case  MappingNotify:
   fprintf(stream, "event  MappingNotify\n");
   if (level > 1)
      fprintf(stream, "     request = %s, first_keycode = %d (0x%x), count = %d\n",
                     (event->xmapping.request == MappingModifier ? "MappingModifer" :
                      (event->xmapping.request == MappingKeyboard ? "MappingKeyboard" :
                       (event->xmapping.request == MappingPointer ? "MappingPointer" :
                       itoa(event->xmapping.request, work)))),
                       event->xmapping.first_keycode, event->xmapping.first_keycode,
                     event->xmapping.count);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xmapping.display,
                     (event->xmapping.send_event ? "True" : "False"),
                     event->xmapping.serial);
   break;

default:
   fprintf(stream, "Unknown event type %02X (%d)\n", event->type, event->type);
   break;

}  /* end of switch statement */

if (level > 1)
   fprintf(stream, "\n");

} /* end of dump_Xevent */

static void translate_detail(int              detail,
                             char            *work)
{

if (detail == NotifyAncestor)
   strcpy(work, "NotifyAncestor");
else
   if (detail == NotifyInferior)
      strcpy(work, "NotifyInferior");
   else
      if (detail == NotifyNonlinear)
         strcpy(work, "NotifyNonlinear");
      else
         if (detail == NotifyNonlinearVirtual)
            strcpy(work, "NotifyNonlinearVirtual");
         else
            if (detail == NotifyVirtual)
               strcpy(work, "NotifyVirtual");
            else
               if (detail == NotifyPointer)
                  strcpy(work, "NotifyPointer");
               else
                  if (detail == NotifyPointerRoot)
                     strcpy(work, "NotifyPointerRoot");
                  else
                     if (detail == NotifyDetailNone)
                        strcpy(work, "NotifyDetailNone");
                     else
                        sprintf(work, "UNKNOWN VALUE 0x%02X", detail);
} /* end of translate_detail */


static void translate_configure_detail(int              detail,
                                       char            *work)
{

if (detail == None)
   strcpy(work, "None");
else
   if (detail == Above)
      strcpy(work, "Above");
   else
      if (detail == Below)
         strcpy(work, "Below");
      else
         if (detail == TopIf)
            strcpy(work, "TopIf");
         else
            if (detail == BottomIf)
               strcpy(work, "BottomIf");
            else
               if (detail == Opposite)
                  strcpy(work, "Opposite");
               else
                  sprintf(work, "UNKNOWN VALUE 0x%02X", detail);
} /* end of translate_configure_detail */


static void translate_mode(int              mode,
                           char            *work)
{

if (mode == NotifyNormal)
   strcpy(work, "NotifyNormal");
else
   if (mode == NotifyGrab)
      strcpy(work, "NotifyGrab");
   else
      if (mode == NotifyUngrab)
         strcpy(work, "NotifyUngrab");
      else
         sprintf(work, "UNKNOWN VALUE 0x%02X", mode);
} /* end of translate_mode */


static void translate_visibility(int              state,
                                 char            *work)
{

if (state == VisibilityFullyObscured)
   strcpy(work, "VisibilityFullyObscured");
else
   if (state == VisibilityPartiallyObscured)
      strcpy(work, "VisibilityPartiallyObscured");
   else
      if (state == VisibilityUnobscured)
         strcpy(work, "VisibilityUnobscured");
      else
         sprintf(work, "UNKNOWN VISIBILITY VALUE 0x%02X", state);
} /* end of translate_visibility */

#endif

void translate_buttonmask(unsigned int     state,
                          char            *work)
{
work[0] = '\0';
if (state & ShiftMask)                strcat(work, " Shift");
if (state & ControlMask)              strcat(work, " Cntl");
if (state & LockMask)                 strcat(work, " Shift-Lock");
if (state & Mod1Mask)                 strcat(work, " Mod1");
if (state & Mod2Mask)                 strcat(work, " Mod2");
if (state & Mod3Mask)                 strcat(work, " Mod3");
if (state & Mod4Mask)                 strcat(work, " Mod4");
if (state & Mod5Mask)                 strcat(work, " Mod5");
if (state & Button1Mask)              strcat(work, " Button1");
if (state & Button2Mask)              strcat(work, " Button2");
if (state & Button3Mask)              strcat(work, " Button3");
if (state & Button4Mask)              strcat(work, " Button4");
if (state & Button5Mask)              strcat(work, " Button5");
if (work[0] == '\0') strcpy(work, "(NO MODIFIERS)");
} /* end of translate_buttonmask */






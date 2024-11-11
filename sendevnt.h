#ifndef _SENDEVNT__INCLUDED
#define _SENDEVNT__INCLUDED

/* "%Z% %M% %I% - %G% %U% " */

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

/**************************************************************
*
*  Routines in sendevnt.c
*     ce_XSendEvent           - Send an event to ourselves using a local queue
*     ce_XNextEvent           - Get the next event off the queue or call XNextEvent
*     ce_XIfEvent             - Perform XIfEvent processing searching our queue first
*     ce_XCheckIfEvent        - Perform XCheckIfEvent processing searching our queue first
*     queued_event_ready      - Check if there is anything on the event queue.
*
*  These routines take the same parameters as the like named
*  Xlib routines.
*
***************************************************************/


Status ce_XSendEvent(Display   *display,
                     Window     window,
                     Bool       propagate,
                     long       event_mask,
                     XEvent    *event);

void ce_XNextEvent(Display    *display,
                   XEvent     *event_return);


void ce_XIfEvent(Display     *display,
                 XEvent      *event_return,
                 Bool       (*chk_rtn)( Display   *    /* display */,
                                        XEvent    *    /* event */,
                                        char      *    /* arg */
                                      )                /* predicate */,
                 char        *arg);

Bool ce_XCheckIfEvent(Display     *display,
                      XEvent      *event_return,
                      Bool       (*chk_rtn)( Display   *    /* display */,
                                             XEvent    *    /* event */,
                                             char      *    /* arg */
                                           )                /* predicate */,
                      char        *arg);

Bool queued_event_ready(void);

#endif


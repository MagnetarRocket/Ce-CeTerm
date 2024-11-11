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
*  module sendevnt.c
*
*  Routines:
*     ce_XSendEvent           - Send an event to ourselves using a local queue
*     ce_XNextEvent           - Get the next event off the queue or call XNextEvent
*     ce_XIfEvent             - Perform XIfEvent processing searching our queue first
*     ce_XCheckIfEvent        - Perform XCheckIfEvent processing searching our queue first
*     queued_event_ready      - Check if there is anything on the event queue.
*
* Internal:
*     ce_IfEvent             -   Perform IfEvent searching our local queue.
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /bsd4.3/usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h   /usr/include/sys/errno.h      */

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
#include "emalloc.h"
#include "sendevnt.h"
#include "xerrorpos.h"


#define SUNHELPKEY 0x7d


/***************************************************************
*  
*  Linked list of events.  Our own internal event queue.
*  This list is the whole purpose of the module.  When
*  sending events to ourselves, we put the events here
*  rather than send them on a round trip to the server.
*  
***************************************************************/

struct CE_XEVENT  ;

struct CE_XEVENT {
        struct CE_XEVENT       *next;              /* next data in list                          */
        XEvent                  event_union;       /* paste buffer name                          */
  };

typedef struct CE_XEVENT CE_XEVENT;


static CE_XEVENT    *event_list_head = NULL;
static CE_XEVENT    *event_list_tail = NULL;


/***************************************************************
*  
*  Prototypes for local routines
*  
***************************************************************/

static Bool ce_ifevent(Display     *display,
                       XEvent      *event_return,
                       Bool       (*chk_rtn)( Display   *    /* display */,
                                              XEvent    *    /* event */,
                                              char      *    /* arg */
                                            )                /* predicate */,
                       char        *arg);

/************************************************************************

NAME:      ce_XSendEvent   - Send an event to ourselves using a local queue

PURPOSE:    This routine puts an element on the end of the linked list.

PARAMETERS:

   Same as XSendEvent - all input


FUNCTIONS :

   1.   Malloc space for the event and copy the passed data
        into the area.

   2.   Add the event to the end of the list.

RETURNED VALUE:

   Status -  type status (really an int)
             True - (1)  always returned
             False - (0) Out of memory


*************************************************************************/

/* ARGSUSED */
Status ce_XSendEvent(Display   *display,
                     Window     window,
                     Bool       propagate,
                     long       event_mask,
                     XEvent    *event)
{
CE_XEVENT        *new_event;

/***************************************************************
*  
*  Look at the command id to determine whether we want to open
*  the paste buffer for read or write.  copy(xc) and cut(xd)
*  need write, paste(xp) and command file (cmdf) need read.
*  
***************************************************************/

new_event = (CE_XEVENT *)CE_MALLOC(sizeof(CE_XEVENT));
if (!new_event)
   return(False);

new_event->next = NULL;
memcpy((char *)&new_event->event_union, (char *)event, sizeof(XEvent));

if (event_list_head == NULL)
   event_list_head = new_event;
else
   event_list_tail->next = new_event;

event_list_tail = new_event;
return(True);

} /* end of ce_XSendEvent */


/************************************************************************

NAME:      ce_XNextEvent - Get the next event off the queue or call XNextEvent

PURPOSE:    This routine takes the next event off the queue for the passed 
            display or calls the Xlib routine XNextEvent if the queue is empty.

PARAMETERS:

   Same as Xlib routine XNextEvent


FUNCTIONS :

   1.   Scan the linked list looking for the first element which matches the
        passed display.

   2.   If an event is found on the linked list, remove it from the list,
        copy it to the returned parameter and free it.

   3.   If no matching event is found, call XNextEvent.

RETURNED VALUE:

   The stream pointer (FILE *) is returned.  On failure, it is NULL.
   For this routine, it is actually a pointer to the PASTEBUF_LIST entry
   we are using.


*************************************************************************/

void ce_XNextEvent(Display    *display,
                   XEvent     *event_return)
{
CE_XEVENT        *curr_event;
CE_XEVENT        *prev_event = NULL;
int               ok_event = False;

for (curr_event = event_list_head;
     curr_event != NULL;
     curr_event = curr_event->next)
   if (curr_event->event_union.xany.display == display)
      break;
   else
      prev_event = curr_event;

/***************************************************************
*  curr_event is non-NULL if we found a suitable event.
***************************************************************/
if (curr_event)
   {
      /**********************************************************
      *  Copy the data to the returned parm.
      ***********************************************************/
      memcpy((char *)event_return, (char *)&curr_event->event_union, sizeof(XEvent));
      /**********************************************************
      *  Remove the element and free it.
      ***********************************************************/
      if (prev_event)
         prev_event->next = curr_event->next;
      else
         event_list_head  = curr_event->next;

      if (curr_event == event_list_tail)
         if (prev_event)
            event_list_tail = prev_event;
         else
            event_list_tail = event_list_head;

      DEBUG15(fprintf(stderr, "ce_XNextEvent:  Event taken from list\n");)

      free((char *)curr_event);
   }
else
   {
      while(!ok_event)
      {
         DEBUG9(XERRORPOS)
         XNextEvent(display, event_return);
         /* do not allow outside people to send us keystroke events or button events */
         if (!event_return->xany.send_event ||
             ((event_return->xany.type != KeyPress) &&
              (event_return->xany.type != KeyRelease) &&
              (event_return->xany.type != ButtonPress) &&
              (event_return->xany.type != ButtonRelease)) ||
               event_return->xkey.keycode == SUNHELPKEY)
           ok_event = True;
       
      }
   }

} /* end of ce_XNextEvent */


/************************************************************************

NAME:      ce_XIfEvent  - Perform XIfEvent processing searching our queue first

PURPOSE:    This routine scans the local event queue using the
            rules of XIfEvent and returns the queue'd element
            if one is found.  Otherwise it calls XIfEvent to
            perform the processing.

PARAMETERS:
   Same as Xlib routine XIfEvent

FUNCTIONS :

   1.   Scan the linked list looking for an event the passed
        event checking routine likes.

   2.   If an event is found, return it.

   3.   If no event is found in the queue, call XIfEvent to
        search for it.


*************************************************************************/

void ce_XIfEvent(Display     *display,
                 XEvent      *event_return,
                 Bool       (*chk_rtn)( Display   *    /* display */,
                                 XEvent    *    /* event */,
                                 char      *    /* arg */
                               )                /* predicate */,
                 char        *arg)
{
int               ok_event = False;

if (ce_ifevent(display, event_return, chk_rtn, arg))
   return;
else
   while(!ok_event)
   {
      DEBUG9(XERRORPOS)
      XIfEvent(display, event_return, chk_rtn, arg);
      /* do not allow outside people to send us keystroke events or button events */
      if (!event_return->xany.send_event ||
          ((event_return->xany.type != KeyPress) &&
           (event_return->xany.type != KeyRelease) &&
           (event_return->xany.type != ButtonPress) &&
           (event_return->xany.type != ButtonRelease)) ||
            event_return->xkey.keycode == SUNHELPKEY)
        ok_event = True;
       
   }

} /* end of ce_XIfEvent */


/************************************************************************

NAME:      ce_XCheckIfEvent  - Perform XCheckIfEvent processing searching our queue first

PURPOSE:    This routine scans the local event queue using the
            rules of XIfEvent and returns the queue'd element
            if one is found.  Otherwise it calls XIfEvent to
            perform the processing.  It is like 

PARAMETERS:
   Same as Xlib routine XIfEvent

FUNCTIONS :

   1.   Scan the linked list looking for an event the passed
        event checking routine likes.

   2.   If an event is found, return it and the Flag True.

   3.   If no event is found in the queue, call XCheckIfEvent to
        search for it.


*************************************************************************/

Bool ce_XCheckIfEvent(Display     *display,
                 XEvent      *event_return,
                 Bool       (*chk_rtn)( Display   *    /* display */,
                                 XEvent    *    /* event */,
                                 char      *    /* arg */
                               )                /* predicate */,
                 char        *arg)
{
Bool     got_event;
Bool     ok_event = False;

if (ce_ifevent(display, event_return, chk_rtn, arg))
   return(True);
else
   while(!ok_event)
   {
      DEBUG9(XERRORPOS)
      got_event = XCheckIfEvent(display, event_return, chk_rtn, arg);
      /* do not allow outside people to send us keystroke events or button events */
      if (!event_return->xany.send_event ||
          !got_event ||
          ((event_return->xany.type != KeyPress) &&
           (event_return->xany.type != KeyRelease) &&
           (event_return->xany.type != ButtonPress) &&
           (event_return->xany.type != ButtonRelease)) ||
            event_return->xkey.keycode == SUNHELPKEY)
        ok_event = True;
   }

return(got_event);

} /* end of ce_XCheckIfEvent */


/************************************************************************

NAME:      ce_IfEvent  - Perform IfEvent searching our local queue.

PURPOSE:    This routine scans the local event queue using the
            rules of XIfEvent and returns the queue'd element
            if one is found.  If one is found, it is used.

PARAMETERS:
   Same as Xlib routine XIfEvent

FUNCTIONS :

   1.   Scan the linked list looking for an event the passed
        event checking routine likes.

   2.   If an event is found, return it.

RETURNED VALUE:
   Found -  Bool (really an int)
        True   - Event found and copied into parmaeter
        False  - No matching event found

*************************************************************************/

static Bool ce_ifevent(Display     *display,
                       XEvent      *event_return,
                       Bool       (*chk_rtn)( Display   *    /* display */,
                                              XEvent    *    /* event */,
                                              char      *    /* arg */
                                            )                /* predicate */,
                       char        *arg)
{
CE_XEVENT        *curr_event;
CE_XEVENT        *prev_event = NULL;

for (curr_event = event_list_head;
     curr_event != NULL;
     curr_event = curr_event->next)
   if (curr_event->event_union.xany.display == display)
         if (chk_rtn(display, &curr_event->event_union, arg))
            break;
         else /* RES 9/15/95 else clause added, was throwing away events fix for 2.4 */
            prev_event = curr_event;
   else
      prev_event = curr_event;

/***************************************************************
*  curr_event is non-NULL if we found a suitable event.
***************************************************************/
if (curr_event)
   {
      /**********************************************************
      *  Copy the data to the returned parm.
      ***********************************************************/
      memcpy((char *)event_return, (char *)&curr_event->event_union, sizeof(XEvent));
      /**********************************************************
      *  Remove the element and free it.
      ***********************************************************/
      if (prev_event)
         prev_event->next = curr_event->next;
      else
         event_list_head  = curr_event->next;

      if (curr_event == event_list_tail)
         if (prev_event)
            event_list_tail = prev_event;
         else
            event_list_tail = event_list_head;

      free((char *)curr_event);
      DEBUG15(fprintf(stderr, "ce_IfEvent: Event taken from list\n");)
      return(True);
   }
else
   return(False);

} /* end of ce_IfEvent */


/************************************************************************

NAME:      queued_event_ready      - Check if there is anything on the event queue.

PURPOSE:    This routine is an external check just to see if there is
            something on the queue.  It is used in cases where we would
            wait on the X socket with a select.

PARAMETERS:
   Same as Xlib routine XIfEvent

FUNCTIONS :

   1.   Scan the linked list looking for an event the passed
        event checking routine likes.

   2.   If an event is found, return it and the Flag True.

   3.   If no event is found in the queue, call XCheckIfEvent to
        search for it.


*************************************************************************/

Bool queued_event_ready(void)
{
return(event_list_head != NULL);

} /* end of ce_XCheckIfEvent */





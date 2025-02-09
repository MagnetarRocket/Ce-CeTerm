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
*  module xsmp.c
*
*  This module provides the interface to xsmp X Session Manager Protocol
*  using libSM and libICE.  XSMP is the replacement for the WM_SAVE_YOURSELF
*  protocol from older X11 implementations which is not supported by 
*  newer versions of Linux (Gnome).
*
*  The two interesting things the Sesson Manager can tell us to do are
*  "Save yourself" which involves possibly a pad write and always a construction
*  of an argc/argv needed to restart the program and die, which tells us to
*  save data, close all windows and exit.
*
*  Routines:
*     xsmp_init              -   Initial connection to xsmp
*     xsmp_display_check     -   Check saved dspl_descr when one is being deleted.
*     xsmp_fdset             -   Update an fdset for the xsmp connections
*     xsmp_fdisset           -   Check the fdset and process
*     xsmp_close             -   Close the xsmp connection and free the private data
*     xsmp_register          -   Register the with the X server that XSMP is being used
*
* Internal:
*     ice_watch_proc         -   Watch for ICE connections to be created and save the fd
*     SaveYourselfProc       -   Respond to a "save yourself" order.
*     DieProc                -   Respond to an XSMP order to save and terminate
*     SaveCompleteProc       -   Respond to a "save complete" order. - no op.
*     ShutdownCancelledProc  -   Respond to a "shutdown canceled" request - no op.
*     child_of_root          -   Get the window which is a child of the root window, the window manager window
*     get_leader_window      -   Get a window to use as the "leader window".
*     xsmp_create_cc_cmds    -   Build the cmdf file with cc commands
*
* OVERVIEW of XSMP operations:
*   xsmp_init gets called shortly after the display is opened.  If there
*   is no Session Manager available, False is returned and this module is
*   called no more.  The initial xsmp call allows the system to use the
*   default environment variable SESSION_MANAGER to determine if and how
*   to connect to the session manager.  If there is a Session Manager
*   available, the private data is salted away and true is returned.
*
*   If there is a session manager, ice_watch_proc will get called so it
*   can extract the file descriptor for the xsmc session.  There is 
*   probably only one.  The main select calls xsmp_fdset to include any
*   xsmp connections in the select.  It then calls xsmp_fdisset to test
*   these and process any orders.
*
*   If a die order comes in, wc on each display description (cc window).
*   When the last one closes, dm_wc calls 
*   del_display which calls xsmp_close to shutdown the connection.  The
*   dm_wc for the last window does not return. 
*
*   The save yourself order causes use to do a save if the global flag is
*   set.  If the fast flag is set, we create a crash file.  We also build
*   the properties to create the command line to restart the program.
*
* XSMP to WM operational notes:  
*   xsmp_register interfaces with the window manager.  It must be called before the
*   main window is mapped.  It must be called after xsmp_init.  That narrows it down.
*   This was tested with the metacity window manager on Linux FC3 and FC4.
*
*
* CE INTEGRATION/DESIGN NOTES:
*   xsmp to ce
*   Need new parm -sm_client_id <value>
*   parms.h
*
*   dspl_descr
*   2 fields:
*   void   *xsmp_private_data
*   int     xsmp_active
*
* CE NOTES
*   in display.c
*   In del_display, only free xsmp_private_data if this is the last display.  There is
*   only accross all dspl_descr's.  Use xsmp_close so the connection is shutdown.
*   If this is not the last display, call xsmp_display_check.
*
*   in cc.c
*   near line 4321 copy the xsmp_private_data pointer to the new
*   display description just like the token pointer.  The xsmp_active 
*   display too.
*
*   when processing a die message, call wc on each window in turn.
*   When only 1 is left, there will be no more private data and wc
*   will not return.
*
*   when saving yourself, cc windows have to be taken into account.
*   dspl_descr->titlebar_host_name with a value of '\0' should be the
*   original ce with the others being -cmd "cc -display ..." values.
*   If this is a ceterm, use the pad mode display description.
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h   /usr/include/sys/errno.h      */
#ifdef  HAVE_X11_SM_SMLIB_H
#include <time.h>           /* /usr/include/time.h      */
#include <pwd.h>
#include <limits.h>         /* /usr/include/limits.h     */
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/SM/SMlib.h>
#include <X11/ICE/ICElib.h>
#ifndef FD_ZERO
#include <sys/select.h>     /* /usr/include/sys/select.h    */
#endif
#endif

#include "buffer.h"
#include "debug.h"
#include "dmc.h"
#include "dmsyms.h"
#include "init.h"
#include "emalloc.h"
#include "pad.h"          /* needed for ce_getcwd */
#include "pw.h"
#include "parms.h"
#include "pastebuf.h"
#include "wc.h"
#include "xerror.h"
#include "xerrorpos.h"
#include "xsmp.h"

#ifndef HAVE_STRLCPY
#include "strl.h"
#endif
#ifndef MAXPATHLEN
#define MAXPATHLEN	1024
#endif


/***************************************************************
*  
*  In places where we rebuild argc/argv, this is the size of the
*  local argv.  It does not need to be changed unless we get close
*  to MAX_ARGV_SIZE actual arguments.
*  
***************************************************************/

#define MAX_ARGV_SIZE 256

/***************************************************************
*  
*  Structure used to describe the xsmp.c private data area
*  This gets hung off the dspl_descr if xsmp is active.
*  
*  MAX_ICE_FDS         Number of ICE connctions we can handle.  I
*                      believe there will actually only be one.
*
*  saved_dspl_descr    The dspl_descr from the call to xsmp_init.
*  client_id_ptr       The client ID from the call to SmcOpenConnection.
*  prev_id_ptr         The client ID passed in the parm if this is a
*                      restart by the session manager.  Otherwise NULL.
*  edit_file           pointer to the global edit_file var.
*  arg0                ponter to argv[0] from the original call.  
*  sm_connection       Value returned by SmcOpenConnection
*  ice_fd_count        Number of ICE connections, usually 1.
*  ice_fds             File descriptors from the ICE connections for the select.
*  ice_connections     ICE connection tokens matching ice_fds.
*
***************************************************************/

#ifdef  HAVE_X11_SM_SMLIB_H

#define MAX_ICE_FDS 20

typedef struct
{
   Display                *display;
   DISPLAY_DESCR          *saved_dspl_descr;
   char                   *client_id_ptr;
   char                   *prev_id_ptr;
   char                   *edit_file;
   char                   *arg0;
   SmcConn                 sm_connection;
   Window                  leader_window;
   Window                  leader_wm_window;
   Atom                    wmClientLeaderAtom;
   Atom                    wmWindowRoleAtom;
   int                     ice_fd_count;
   int                     ice_fds[MAX_ICE_FDS];
   IceConn                 ice_connections[MAX_ICE_FDS];
}  XSMP_CALLBACK_DATA;
#endif

/***************************************************************
*  
*  Prototypes for local routines.
*  
***************************************************************/
#ifdef  HAVE_X11_SM_SMLIB_H

static void ice_watch_proc(IceConn      ice_conn,
                           IcePointer   client_data,
                           Bool         opening, 
                           IcePointer  *watch_data);

static void SaveYourselfProc(SmcConn    smc_conn,
                             SmPointer  client_data,
                             int        save_type,
                             Bool       shutdown,
                             int        interact_style,
                             Bool       fast);

static void DieProc(SmcConn    smc_conn,
                    SmPointer  client_data);

static void SaveCompleteProc(SmcConn    smc_conn,
                             SmPointer   client_data);

static void ShutdownCancelledProc(SmcConn    smc_conn,
                                  SmPointer   client_data);

static void get_userid(char   *userid, int  sizeof_userid);

static char  *quoted_parm(char *the_parm);

static Window  child_of_root(Display   *display,
                             Window     window);

static int  get_leader_window(Display   *display,     /* input  */
                              Window    *our_window,  /* output */
                              Window    *wm_window);  /* output */

static char *xsmp_create_cc_cmds(DISPLAY_DESCR      *dspl_descr,
                                 const char         *client_id);

#ifdef blah
static void wait_for_save_reply(XSMP_CALLBACK_DATA        *callback_data);
#endif

#endif


/************************************************************************

NAME:      xsmp_init              -   Initial connection to xsmp


PURPOSE:    This routine gets a connection to the X Session Manager
            via the X Session Manager Protcol (XSMP) via libSM.
       
PARAMETERS:
   1.  dspl_descr -  pointer to DISPLAY_DESCR (INPUT/OUTPUT)
                     This is the current initial display description.
                     It ends up holding the callback data.

   2.  edit_file  -  pointer to char (INPUT)
                     This is the current edit file.  Since it point
                     to the global edit file name, it changes with
                     edit file.

   3.  arg0       -  pointer to char (INPUT)
                     This is the argv[0]  it is needed for various
                     calls made by the callback routines.

FUNCTIONS :
   1.   Get the local data area and initialize it.

   2.   Register a connection watcher with ICE.

   3.   Connect to the Session Manager (via SmcOpenConnection)

RETURN VALUE:
   ok      -    int flag
                True  - Connection to SM established
                False - No Session Manager available.

*************************************************************************/

int  xsmp_init(DISPLAY_DESCR     *dspl_descr,  /* returns true if xsmp is active */
               char              *edit_file,   /* creates xsmp_private_data_p */
               char              *arg0)
{
#ifdef  HAVE_X11_SM_SMLIB_H

XSMP_CALLBACK_DATA   *callback_data;
char                  msg[1024];
SmcCallbacks          smc_callback_funcs;
Bool                  ok;

/***************************************************************
*  First get the private area which is passed to all the callback
*  functions and initialize it.
***************************************************************/
callback_data = (XSMP_CALLBACK_DATA  *)CE_MALLOC(sizeof(XSMP_CALLBACK_DATA));
if (!callback_data)
   return(False);

memset(callback_data, 0, sizeof(XSMP_CALLBACK_DATA));
callback_data->saved_dspl_descr = dspl_descr;
callback_data->arg0             = arg0;
callback_data->edit_file        = edit_file;

/***************************************************************
*  Set the ICE connection watcher.  This is routine gets to know
*  what socket file descriptors are used for xsmp data.
***************************************************************/
ok = IceAddConnectionWatch(ice_watch_proc, callback_data);
if (!ok)
   {
      free(callback_data);
      DEBUG1(fprintf(stderr, "xsmp_init:  Call to IceAddConnectionWatch failed\n");)
      return(False);
   }

/***************************************************************
*  Set up all the callback routine pointers in the parm to 
*  SmcOpenConnection.
***************************************************************/
smc_callback_funcs.save_yourself.callback         = SaveYourselfProc;
smc_callback_funcs.save_yourself.client_data      = (SmPointer)callback_data;
smc_callback_funcs.die.callback                   = DieProc;
smc_callback_funcs.die.client_data                = (SmPointer)callback_data;
smc_callback_funcs.save_complete.callback         = SaveCompleteProc;
smc_callback_funcs.save_complete.client_data      = (SmPointer)callback_data;
smc_callback_funcs.shutdown_cancelled.callback    = ShutdownCancelledProc;
smc_callback_funcs.shutdown_cancelled.client_data = (SmPointer)callback_data;

if (SM_CLIENT_ID && SM_CLIENT_ID[0])
   callback_data->prev_id_ptr = SM_CLIENT_ID;
else
   callback_data->prev_id_ptr = NULL;

DEBUG1(fprintf(stderr, "xsmp_init:  Using XSMP prev_id_ptr : %s\n", (callback_data->prev_id_ptr ? callback_data->prev_id_ptr : "<NULL>"));)

/***************************************************************
*  See if there is a session manager to talk to.  If not, the
*  call will give a connection of NULL.
***************************************************************/
callback_data->sm_connection =  SmcOpenConnection(
                      NULL,  /* networkIdsList, NULL means use env var SESSION_MANAGER */
                      NULL,  /* SmPointer      context */
                      SmProtoMajor,            /* xsmpMajorRev */
                      SmProtoMinor,            /* xsmpMinorRev */
                      SmcSaveYourselfProcMask | SmcDieProcMask | SmcSaveCompleteProcMask | SmcShutdownCancelledProcMask,  /* mask */
                      &smc_callback_funcs,     /* SmcCallbacks * callbacks */
                      callback_data->prev_id_ptr,              /* char *      previousId */
                      &(callback_data->client_id_ptr),         /* char **        /* clientIdRet */
                      sizeof(msg),           /* errorLength */
                      msg);


if (callback_data->sm_connection == NULL)
   {
      free(callback_data);
      DEBUG1(fprintf(stderr, "xsmp_init:  Call to SmcOpenConnection failed (%s)\n", msg);)
      return(False);
  }

dspl_descr->xsmp_private_data = callback_data;

return(True);

#else

/***************************************************************
*  If we are not compiling in XSMP (libSM) support, just return
*  false saying there is no session manager.
***************************************************************/
return(False);

#endif
} /* end of xsmp_init */


/************************************************************************

NAME:      xsmp_display_check              -   Check saved dspl_descr when one is being deleted.


PURPOSE:    This routine is called when a display description is being
            deleted by del_display and it is not the last one.  In case
            it is the one we have saved, we need to swap in a new saved
            value.  This happens if you open a ce window, do a cc and
            then close the original window.
       
PARAMETERS:
   1.  dspl_descr_going_away  -  pointer to DISPLAY_DESCR (INPUT/OUTPUT)
                                  This is the display description which is
                                going away.

   3.  Replacement_dspl_descr -  pointer to DISPLAY_DESCR (INPUT/OUTPUT)
                                 This is the display description to be
                                 used as a replacement if the saved display
                                 description is the one going away.

FUNCTIONS :
   1.   Check the saved value against the passed value.

   2.   If they are the same, replace the saved value with the
        replacement.


*************************************************************************/

void   xsmp_display_check(DISPLAY_DESCR     *dspl_descr_going_away,
                          DISPLAY_DESCR     *Replacement_dspl_descr)
{
#ifdef  HAVE_X11_SM_SMLIB_H

XSMP_CALLBACK_DATA   *callback_data;

callback_data = dspl_descr_going_away->xsmp_private_data;

if (callback_data->saved_dspl_descr == dspl_descr_going_away)
   callback_data->saved_dspl_descr  = Replacement_dspl_descr;

#endif
} /* end of xsmp_display_check */


/************************************************************************

NAME:      xsmp_fdset             -   Update an fdset for the xsmp connections


PURPOSE:    This routine sets the correct bits in an fdset structure destined 
            to be used in a select() call.

PARAMETERS:
   1.  xsmp_private_data_p - pointer to void (INPUT)
                             This is the dspl_descr->xsmp_private_data field.

   2.  ice_fdset           - pointer to fd_set (OUTPUT)
                             This is a read fdset which will be passed to
                             select.  It is modified by this routine.

FUNCTIONS :
   1.   Walk the list of socket fd's and set the fd_set bit for each one.

   2.   Keep track of the max fd and return it.

RETURN VALUE:
   max_fd      -  int
                  The max file descriptor to merge in with the other fd's
                  going to the select.

*************************************************************************/

int   xsmp_fdset(void        *xsmp_private_data_p,  /* input */
                fd_set       *ice_fdset)            /* output */
{
#ifdef  HAVE_X11_SM_SMLIB_H

XSMP_CALLBACK_DATA   *callback_data = (XSMP_CALLBACK_DATA *)xsmp_private_data_p;
int                   i;
int                   max_fd = -1;

if (!xsmp_private_data_p)
   return(max_fd);

for (i = 0; i < callback_data->ice_fd_count; i++)
{
   if (callback_data->ice_fds[i] >= 0)
      {
         FD_SET(callback_data->ice_fds[i], ice_fdset);
         if (callback_data->ice_fds[i] > max_fd)
            max_fd = callback_data->ice_fds[i];
      }
}

return(max_fd);

#else
return(-1);
#endif
} /* end of xsmp_fdset */


/************************************************************************

NAME:      xsmp_fdisset           -   Check the fdset and process


PURPOSE:    This routine tests the fd(s) for the ICE connection(s).
            If the ICE fd has data ready to read, the ICE Process Messages
            routine is called, which cases the XSMP code to call one of the
            callback routines in this module.

PARAMETERS:
   1.  xsmp_private_data_p - pointer to void (INPUT)
                             This is the dspl_descr->xsmp_private_data field.

   2.  ice_fdset           - pointer to fd_set (INPUT)
                             This is a read fdset which will be passed to
                             select.

FUNCTIONS :
   1.   Walk the list of ICE connections looking to see if any are ready
        with an order.

   2.   Call the library handler IceProcessMessages for each ready fd.

*************************************************************************/

void   xsmp_fdisset(void        *xsmp_private_data_p,  /* input */
                    fd_set      *ice_fdset)            /* input */
{
#ifdef  HAVE_X11_SM_SMLIB_H

XSMP_CALLBACK_DATA   *callback_data = (XSMP_CALLBACK_DATA *)xsmp_private_data_p;
int                   i;

if (!xsmp_private_data_p)
   return;

for (i = 0; i < callback_data->ice_fd_count; i++)
   {
       if ((callback_data->ice_fds[i] >= 0) && FD_ISSET(callback_data->ice_fds[i], ice_fdset))
         {
            DEBUG22(fprintf(stderr, "%s *** ice_commands for ice_fds[%d] = %d\n", CURRENT_TIME, i, callback_data->ice_fds[i]);)
            IceProcessMessages(callback_data->ice_connections[i], NULL, NULL);
         }
   }
 
#endif
} /* end of xsmp_fdisset */


/************************************************************************

NAME:      xsmp_close             -   Close the xsmp connection and free the private data


PURPOSE:    This routine closes the connection to the xsmp server.

PARAMETERS:
   1.  xsmp_private_data_p - pointer to void (INPUT)
                             This is the dspl_descr->xsmp_private_data field.
                             After this call the address is no good and should be nulled out.

FUNCTIONS :
   1.   Turn off the ICE connection watcher.

   2.   Shut down the SMC connection.

   3.   Free the private data.


*************************************************************************/

void   xsmp_close(void       **xsmp_private_data_p)  /* input/output */
{
#ifdef  HAVE_X11_SM_SMLIB_H

XSMP_CALLBACK_DATA   *callback_data = (XSMP_CALLBACK_DATA *)(*xsmp_private_data_p);

if (xsmp_private_data_p == NULL)
   return;

DEBUG1(fprintf(stderr, "xsmp_close called\n");)

IceRemoveConnectionWatch(ice_watch_proc, callback_data);
SmcCloseConnection(callback_data->sm_connection, 0, NULL);

if (callback_data->leader_window != None)
   {
      DEBUG9(XERRORPOS)
      XUnmapWindow(callback_data->display, callback_data->leader_window);
      DEBUG9(XERRORPOS)
      XDestroyWindow(callback_data->display, callback_data->leader_window);
   }

DEBUG9(XERRORPOS)
XCloseDisplay(callback_data->display);

memset(callback_data, 0, sizeof(XSMP_CALLBACK_DATA));
free(callback_data);
*xsmp_private_data_p = NULL;

#endif
} /* end of xsmp_close */


/************************************************************************

NAME:      xsmp_register  -   Register the with the X server that XSMP is being used


PURPOSE:    This routine gets the next geometry off the queue and returns it.

PARAMETERS:
   1.  dspl_descr          - pointer to DISPLAY_DESCR (INPUT)
                             This is the dspl_descr.  Field xsmp_private_data field
                             points to the data described by XSMP_CALLBACK_DATA

FUNCTIONS :
   1.   Build the X property structure to show we have an SM_CLIENT_ID.

   2.   Get the Client ID Atom from the X server

   3.   Set the property for this atom on the window manager window.

*************************************************************************/

void   xsmp_register(DISPLAY_DESCR       *dspl_descr)
{
#ifdef  HAVE_X11_SM_SMLIB_H
XSMP_CALLBACK_DATA   *callback_data = (XSMP_CALLBACK_DATA *)dspl_descr->xsmp_private_data;
Atom                  smClientIdAtom;
XTextProperty         smClientIDText;
XTextProperty         smLeaderID;
XTextProperty         wmWindowRole;
Atom                  actual_type;
int                   actual_format;
unsigned long         nitems;
unsigned long         bytesafter;
unsigned long        *datap = NULL;
Window                leader_window;
static Window         None_window = None;
char                  role[512];

if (callback_data->display == NULL)
   {
      DEBUG9(XERRORPOS)
      callback_data->display = XOpenDisplay(DisplayString(dspl_descr->display));
      if (callback_data->display == NULL)
         {
            DEBUG1(fprintf(stderr, "xsmp_register: cannot connect to X server %s (%s), session manager will not recognise this window\n",
                   DisplayString(dspl_descr), strerror(errno));
            )
            return;
            
         }
   }

if (callback_data->wmClientLeaderAtom == None)
   {
      DEBUG9(XERRORPOS)
      callback_data->wmClientLeaderAtom = XInternAtom(callback_data->display, "WM_CLIENT_LEADER", False);
      if (callback_data->wmClientLeaderAtom == None)
         {
            DEBUG1(fprintf(stderr, "xsmp_register: Cannot get atom id for WM_CLIENT_LEADER, session manager will not recognise this window\n");)
            return;
         }
   }

if (callback_data->wmWindowRoleAtom == None)
   {
      DEBUG9(XERRORPOS)
      callback_data->wmWindowRoleAtom = XInternAtom(callback_data->display, "WM_WINDOW_ROLE", False);
      if (callback_data->wmWindowRoleAtom == None)
         {
            DEBUG1(fprintf(stderr, "xsmp_register: Cannot get atom id for WM_WINDOW_ROLE, session manager may not recognise this window\n");)
            return;
         }
   }


if (callback_data->leader_window == None)
   {
      if (get_leader_window(callback_data->display,                /* input  */
                            &callback_data->leader_window,         /* output */
                            &callback_data->leader_wm_window) < 0) /* output */
         return;


      /***************************************************************
      *  Let the window manager know we are processing XSMP protocol.
      *  Get the Client Leader atom and get the value which is a window
      *  id.  Then set the SM_CLIENT_ID property on this window.
      ***************************************************************/

      DEBUG9(XERRORPOS)
      smClientIdAtom = XInternAtom(callback_data->display, "SM_CLIENT_ID", False);
      if (smClientIdAtom != None)
         {
            smClientIDText.value    = (unsigned char *)callback_data->client_id_ptr;
            smClientIDText.encoding = XA_STRING;
            smClientIDText.format   = 8;
            smClientIDText.nitems   = strlen(callback_data->client_id_ptr);

            DEBUG9(XERRORPOS)
            if (callback_data->leader_window)
               XSetTextProperty(callback_data->display, callback_data->leader_window, &smClientIDText, smClientIdAtom);
            DEBUG9(XERRORPOS)
            if ((callback_data->leader_wm_window) && (callback_data->leader_wm_window != callback_data->leader_window))
               XSetTextProperty(callback_data->display, callback_data->leader_wm_window, &smClientIDText, smClientIdAtom);

            DEBUG1(
               fprintf(stderr, "xsmp_register: SM_CLIENT_ID property set for leader window 0x%X, val = %s\n",
                       callback_data->leader_window, callback_data->client_id_ptr);
            )

            /*smLeaderID.value    = (unsigned char *)&callback_data->leader_window;*/
            smLeaderID.value    = (unsigned char *)&None_window;
            smLeaderID.encoding = XA_WINDOW;
            smLeaderID.format   = 32;
            smLeaderID.nitems   = 1;

            DEBUG9(XERRORPOS)
            if (callback_data->leader_window)
               XSetTextProperty(callback_data->display, callback_data->leader_window, &smLeaderID, callback_data->wmClientLeaderAtom);

            DEBUG9(XERRORPOS)
            if ((callback_data->leader_wm_window) && (callback_data->leader_wm_window != callback_data->leader_window))
               XSetTextProperty(callback_data->display, callback_data->leader_wm_window, &smLeaderID, callback_data->wmClientLeaderAtom);
         }

   }

if (callback_data->wmClientLeaderAtom != None)
   {
      smLeaderID.value    = (unsigned char *)&callback_data->leader_window;
      smLeaderID.encoding = XA_WINDOW;
      smLeaderID.format   = 32;
      smLeaderID.nitems   = 1;

      DEBUG9(XERRORPOS)
      if (dspl_descr->main_pad->x_window)
         XSetTextProperty(callback_data->display, dspl_descr->main_pad->x_window, &smLeaderID, callback_data->wmClientLeaderAtom);

      DEBUG1(
         fprintf(stderr, "xsmp_register: WM_CLIENT_LEADER property set to 0x%X for main window 0x%X\n",
                 callback_data->leader_window, dspl_descr->main_pad->x_window);
      )

   }

if (callback_data->wmWindowRoleAtom != None)
   {
      snprintf(role, sizeof(role), "%s-%d", callback_data->client_id_ptr, dspl_descr->display_no);
      wmWindowRole.value    = (unsigned char *)role;
      wmWindowRole.encoding = XA_STRING;
      wmWindowRole.format   = 8;
      wmWindowRole.nitems   = strlen(role);

      DEBUG9(XERRORPOS)
      if (dspl_descr->main_pad->x_window)
         XSetTextProperty(callback_data->display, dspl_descr->main_pad->x_window, &wmWindowRole, callback_data->wmWindowRoleAtom);

      DEBUG1(
         fprintf(stderr, "xsmp_register: WM_WINDOW_ROLE property set to \"%s\" for main window 0x%X\n",
                 role, dspl_descr->main_pad->x_window);
      )

   }


if (callback_data->display != NULL)
   XFlush(callback_data->display);

#endif
} /* end of xsmp_register */


#ifdef  HAVE_X11_SM_SMLIB_H

/************************************************************************

NAME:      ice_watch_proc  -  Monitor the creation and deletion of ICE connections.


PURPOSE:    This routine closes the connection to the xsmp server.

PARAMETERS:
   1.  ice_conn       - IceConn type (opaque) (INPUT)
                        This token represents the ICE connection
                        just created or about to be destroyed.

   2.  client_data    - IcePointer type (INPUT/OUTPUT)
                        This is the callback data described by
                        XSMP_CALLBACK_DATA

   3.  opening        - int flag (INPUT)
                        True  - An ICE connection is being created.
                        False - An ICE connection is being destroyed.

   4.  watch_data     - pointer to IcePointer (OUTPUT)
                        The ICE doc says to set this to the value of client data.

FUNCTIONS :
   1.   If this is an opening, get the file descriptor and add it to
        the list saved in the callback data.

   2.   If this is a destroy, set the value in the file descriptor to
        -1 to show it is no longer in use.

NOTE:
   I have only seen one call to this routine right after the call to SmcOpenConnection

*************************************************************************/

static void ice_watch_proc(IceConn      ice_conn,
                           IcePointer   client_data,
                           Bool         opening, 
                           IcePointer  *watch_data)
{
XSMP_CALLBACK_DATA        *callback_data = (XSMP_CALLBACK_DATA *)client_data;
int                        i;
int                        temp_fd;

DEBUG1(
fprintf(stderr, "%s *** ice_watch_proc called opening = %s\n ", CURRENT_TIME,
        (opening ? "True" : "False"));
)

temp_fd = IceConnectionNumber(ice_conn);

if (opening)
   {
      for (i = 0; i < callback_data->ice_fd_count; i++)
      {
         if (callback_data->ice_fds[i] == -1)
            {
               callback_data->ice_fds[i] =  temp_fd;
               DEBUG1(fprintf(stderr, "    ice_fds[%d] = %d\n", i, callback_data->ice_fds[i]);)
               break;
            }
      }
      if (i >= callback_data->ice_fd_count)
         {
            callback_data->ice_fds[callback_data->ice_fd_count] =  temp_fd;
            callback_data->ice_connections[callback_data->ice_fd_count] = ice_conn;
            DEBUG1(fprintf(stderr, "    ice_fds[%d] = %d\n", callback_data->ice_fd_count, callback_data->ice_fds[callback_data->ice_fd_count]);)
         }
      callback_data->ice_fd_count++;
      *watch_data = client_data; /* ice doc tells us to do this */
   }
else
   {
      for (i = 0; i < callback_data->ice_fd_count; i++)
      {
         if (temp_fd == callback_data->ice_fds[i])
            {
               callback_data->ice_fds[i] = -1;
               DEBUG1(fprintf(stderr, "    REMOVING ice_fds[%d] = %d\n", i, callback_data->ice_fds[i]);)
               if (i == (callback_data->ice_fd_count - 1))
                  callback_data->ice_fd_count--;
            }
      }
         
   }

} /* end of ice_watch_proc */


/************************************************************************

NAME:      SaveYourselfProc  -  Process xsmp SaveYourself command.


PURPOSE:    This routine closes the connection to the xsmp server.

PARAMETERS:
   1.  smc_conn       - SmcConn type (opaque) (INPUT)
                        This token represents the Session Manager Connection/

   2.  client_data    - SmPointer type (INPUT)
                        This is the dspl_descr->xsmp_private_data
                        field described by XSMP_CALLBACK_DATA.

   3.  save_type      - int (enum) (INPUT)
                        SmSaveGlobal - Save the data to disk (dm_pw).
                        SmSaveLocal  - Set the properties needed to restart the program
                        SmSaveBoth   - Do both of the above, the pw first.

   4.  shutdown       - Bool (int) flag (INPUT)
                        True - A shutdown is being performed. See Notes:
                        
   5.  interact_style - Bool (int) flag (INPUT)
                        
   6.  fast           - Bool (int) flag (INPUT)
                        True - Something is terrible wrong, do the save fast.
                               We will create a crash file instead of doing a pw.

FUNCTIONS :
   1.   If this is an opening, get the file descriptor and add it to
        the list saved in the callback data.

   2.   If this is a destroy, set the value in the file descriptor to
        -1 to show it is no longer in use.

NOTES:
   FROM THE SPEC:
   The shutdown argument specifies whether the system is being shut
   down. The interaction is different depending on whether or not
   shutdown is set. If not shutting down, the client should save its
   state and wait for a 'Save Complete' message. If shutting down,
   the client must save state and then prevent interaction until it
   receives either a 'Die' or a 'Shutdown Cancelled.'


*************************************************************************/

static void SaveYourselfProc(SmcConn    smc_conn,
                             SmPointer  client_data,
                             int        save_type,
                             Bool       shutdown,
                             int        interact_style,
                             Bool       fast)
{
XSMP_CALLBACK_DATA        *callback_data = (XSMP_CALLBACK_DATA *)client_data;
int                        smc_property_count;
char                      *temp;
char                       work_geometry[128];
DISPLAY_DESCR             *dspl_descr;
char                       lineno_str[20];
char                       work_dir[MAXPATHLEN+10];
int                        argc;
char                      *argv[MAX_ARGV_SIZE];
char                       user_id_name[32];
char                       process_id_str[16];
SmPropValue                discard_argv[2];
SmPropValue                clone_argv[MAX_ARGV_SIZE];
SmPropValue                restart_argv[MAX_ARGV_SIZE];
int                        malloced_parm[MAX_ARGV_SIZE];
SmPropValue                userid_val;
SmPropValue                progname_val;
SmPropValue                current_dir_val;
SmPropValue                processid_val;
int                        i;
int                        clone_count;
int                        restart_count;
char                      *progname;
char                      *the_arg;
SmProp                     restart_properties[7];                    
SmProp                    *restart_properites_p[7];
int                        restart_properties_count;
char                      *cc_cmdf_file;

DEBUG1(
fprintf(stderr, "%s *** SaveYourselfProc called Save type = %s, shutdown = %s, interact_style = %s, fast = %s\n ",
        CURRENT_TIME,
        ((save_type = SmSaveGlobal) ? "SmSaveGlobal" : ((save_type = SmSaveLocal) ? "SmSaveLocal" : "SmSaveBoth")),
        (shutdown ? "True" : "False"),
        ((interact_style = SmInteractStyleNone) ? "SmInteractStyleNone" : ((interact_style = SmInteractStyleErrors) ? "SmInteractStyleErrors" : "SmInteractStyleAny")),
        (fast ? "True" : "False"));
)

dspl_descr = callback_data->saved_dspl_descr;

/***************************************************************
*  If this shutdown type requires a save do it first.  If the
*  fast flag is set something bad has happened and we create
*  a crash file.
***************************************************************/
if (save_type != SmSaveLocal)
   {
      if (fast)
         {
#ifdef PAD
            if ((dirty_bit(dspl_descr->main_pad->token) && !dspl_descr->pad_mode) || (dspl_descr->edit_file_changed))
#else
            if (dirty_bit(dspl_descr->main_pad->token))
#endif
               create_crash_file();
         }
      else
         {
#ifdef PAD
            if ((dirty_bit(dspl_descr->main_pad->token) && !dspl_descr->pad_mode) || (dspl_descr->edit_file_changed))
#else
            if (dirty_bit(dspl_descr->main_pad->token))
#endif
               dm_pw(dspl_descr, callback_data->edit_file, False);
         }
   }

/***************************************************************
*  If this shutdown type requires us to set up the restart command
*  do so.
***************************************************************/
if (save_type != SmSaveGlobal)
   {
      /***************************************************************
      *  First, get the command line arguments for this display
      *  descriptor.  This creates an argc and argv.
      ***************************************************************/
      temp = OPTION_VALUES[GEOMETRY_IDX];
      snprintf(work_geometry, sizeof(work_geometry), "%dx%d%+d%+d",
                             dspl_descr->main_pad->window->width,
                             dspl_descr->main_pad->window->height,
                             dspl_descr->main_pad->window->x,
                             dspl_descr->main_pad->window->y);
      OPTION_VALUES[GEOMETRY_IDX] = work_geometry;
      build_cmd_args(dspl_descr, callback_data->edit_file, callback_data->arg0, &argc, argv);
      OPTION_VALUES[GEOMETRY_IDX] = temp;

      /***************************************************************
      *  Copy all but the last parm (edit file) to properties lists
      *  We need to stick the -sm_client_id parm in the restart_argv
      *  before tacking on the last piece.
      ***************************************************************/
      for (i = 0; i < argc-1; i++)
      {
         if (strpbrk(argv[i], " >#<	;\'\"$") == NULL)
            {
               the_arg = argv[i];
               malloced_parm[i] = False;
            }
         else
            {
               the_arg = quoted_parm(argv[i]);
               malloced_parm[i] = True;
            }

         restart_argv[i].value  = the_arg;
         restart_argv[i].length = strlen(restart_argv[i].value);

         clone_argv[i].value  = the_arg;
         clone_argv[i].length = strlen(clone_argv[i].value);

      }
      clone_count = i;
      restart_count = i;

      /***************************************************************
      *  Add in the client id parm to the list 
      ***************************************************************/
      restart_argv[restart_count].value  = malloc_copy("-sm_client_id");
      restart_argv[restart_count].length = strlen(restart_argv[restart_count].value);
      malloced_parm[restart_count]       = True;
      restart_count++;

      restart_argv[restart_count].value  = malloc_copy(callback_data->client_id_ptr);
      restart_argv[restart_count].length = strlen(restart_argv[restart_count].value);
      malloced_parm[restart_count]       = True;
      restart_count++;

      /***************************************************************
      *  If we are located at some line number, pass the parm to postion
      *  the file in the correct place.
      ***************************************************************/
      if (dspl_descr->main_pad->file_line_no > 0)
         {
            snprintf(lineno_str, sizeof(lineno_str),  "%+d", dspl_descr->main_pad->file_line_no);
            restart_argv[restart_count].value  = malloc_copy(lineno_str);
            restart_argv[restart_count].length = strlen(restart_argv[restart_count].value);
            malloced_parm[restart_count]       = True;
            restart_count++;
         }

      /***************************************************************
      *  If there are cc windows, build a cmd file with the cc commands in it
      *  and pass the cmdf for this file to the program.
      ***************************************************************/
      cc_cmdf_file = xsmp_create_cc_cmds(dspl_descr,  callback_data->client_id_ptr);
      if (cc_cmdf_file != NULL)
         {
            /***************************************************************
            *  Add in the -cmd "cmdf <file>" parm to the list 
            ***************************************************************/
            restart_argv[restart_count].value  = malloc_copy("-cmd");
            restart_argv[restart_count].length = strlen(restart_argv[restart_count].value);
            malloced_parm[restart_count]       = True;
            restart_count++;

            snprintf(work_dir, sizeof(work_dir), "\"cmdf %s\"", cc_cmdf_file);
            restart_argv[restart_count].value  = malloc_copy(work_dir);
            restart_argv[restart_count].length = strlen(restart_argv[restart_count].value);
            malloced_parm[restart_count]       = True;
            restart_count++;

            discard_argv[0].value          = "/bin/rm";
            discard_argv[0].length         = strlen(discard_argv[0].value);
            discard_argv[1].value          = cc_cmdf_file;
            discard_argv[1].length         = strlen(discard_argv[1].value);

            restart_properties[6].name     = SmDiscardCommand;
            restart_properties[6].type     = SmLISTofARRAY8;
            restart_properties[6].num_vals = 2;
            restart_properties[6].vals     = discard_argv;

            restart_properties_count = 7;
         }
      else
         restart_properties_count = 6;

      /***************************************************************
      *  Add in the name.  If this was a ceterm, it will just be the
      *  last parm from the list.
      ***************************************************************/
      the_arg = malloc_copy(argv[argc-1]);
      restart_argv[restart_count].value  = the_arg;
      restart_argv[restart_count].length = strlen(restart_argv[restart_count].value);
      malloced_parm[restart_count]       = False;
      restart_count++;

      clone_argv[clone_count].value  = the_arg;
      clone_argv[clone_count].length = strlen(clone_argv[clone_count].value);
      clone_count++;

      /***************************************************************
      *   Set the data for other xsmp variables as prescribed in
      *  the spec.  These are the program name, userid, working directory,
      *  and process id.
      ***************************************************************/
      progname = argv[0];
      progname_val.value  = progname;
      progname_val.length = strlen(progname_val.value);

      get_userid(user_id_name, sizeof(user_id_name));
      userid_val.value  = user_id_name;
      userid_val.length = strlen(userid_val.value);

      work_dir[0] = '\0';
      getcwd(work_dir, sizeof(work_dir));
      current_dir_val.value  = work_dir;
      current_dir_val.length = strlen(current_dir_val.value);

      sprintf(process_id_str, "%d", getpid());
      processid_val.value  = process_id_str;
      processid_val.length = strlen(processid_val.value);

      /***************************************************************
      *  Set up the parm to SmcSetProperties.  We are setting 4
      *  properites, CloneCommand, Program, RestartCommand, and Userid.
      *  The call takes an arry of pointers to the restart properties.
      ***************************************************************/
      restart_properties[0].name     = SmCloneCommand;
      restart_properties[0].type     = SmLISTofARRAY8;
      restart_properties[0].num_vals = clone_count;
      restart_properties[0].vals     = clone_argv;

      restart_properties[1].name     = SmProgram;
      restart_properties[1].type     = SmARRAY8;
      restart_properties[1].num_vals = 1;
      restart_properties[1].vals     = &progname_val;

      restart_properties[2].name     = SmRestartCommand;
      restart_properties[2].type     = SmLISTofARRAY8;
      restart_properties[2].num_vals = restart_count;
      restart_properties[2].vals     = restart_argv;

      restart_properties[3].name     = SmUserID;
      restart_properties[3].type     = SmARRAY8;
      restart_properties[3].num_vals = 1;
      restart_properties[3].vals     = &userid_val;

      restart_properties[4].name     = SmCurrentDirectory;
      restart_properties[4].type     = SmARRAY8;
      restart_properties[4].num_vals = 1;
      restart_properties[4].vals     = &current_dir_val;

      restart_properties[5].name     = SmProcessID;
      restart_properties[5].type     = SmARRAY8;
      restart_properties[5].num_vals = 1;
      restart_properties[5].vals     = &processid_val;

      restart_properites_p[0] = &restart_properties[0];
      restart_properites_p[1] = &restart_properties[1];
      restart_properites_p[2] = &restart_properties[2];
      restart_properites_p[3] = &restart_properties[3];
      restart_properites_p[4] = &restart_properties[4];
      restart_properites_p[5] = &restart_properties[5];
      restart_properites_p[6] = &restart_properties[6];

      SmcSetProperties(smc_conn, 
                       restart_properties_count,  /* numProps */
                       restart_properites_p);     /* ptr to props array */

      DEBUG1(
         fputs("XSMP SaveYourselfProc restart command:  ", stderr);

         for (i = 0; i < restart_count; i++)
            fprintf(stderr, "%s ", restart_argv[i].value);

         fputc('\n', stderr);
         fputs("XSMP SaveYourselfProc clone command:  ", stderr);

         for (i = 0; i < clone_count; i++)
            fprintf(stderr, "%s ", clone_argv[i].value);

         fputc('\n', stderr);
      )

      /***************************************************************
      *  If we malloc'ed the parms, clear them out
      ***************************************************************/
      for (i = 0; i < restart_count; i++)
         if (malloced_parm[i])
            free(restart_argv[i].value);

   } /* save_type needs resources set */

/***************************************************************
*  Tell the xsmp server we are done.
***************************************************************/
SmcSaveYourselfDone(smc_conn, True); /* True is sucess */


} /* end of SaveYourselfProc */


/************************************************************************

NAME:      DieProc -   Process xsmp Die (exit) command.


PURPOSE:    This routine is called if the X Session Manager sends a
            SaveComplete Message.

PARAMETERS:
   1.  smc_conn       - SmcConn type (opaque) (INPUT)
                        This token represents the Session Manager Connection/

   2.  client_data    - SmPointer type (INPUT)
                        This is the dspl_descr->xsmp_private_data
                        field described by XSMP_CALLBACK_DATA.


FUNCTIONS :
   1.   Turn off the ICE connection watcher.

*************************************************************************/

static void DieProc(SmcConn    smc_conn,
                    SmPointer  client_data)
{
XSMP_CALLBACK_DATA        *callback_data = (XSMP_CALLBACK_DATA *)client_data;
DMC_wc                     fake_window_close;

DEBUG1(fprintf(stderr, "*** DieProc called\n");)

while (callback_data->saved_dspl_descr)
{
   memset(&fake_window_close, 0, sizeof(DMC_wc));
   fake_window_close.next   = NULL;
   fake_window_close.cmd    = DM_wc;
   fake_window_close.temp   = False;
   fake_window_close.type   = 'y';
   flush(callback_data->saved_dspl_descr->main_pad);
   dm_wc((DMC *)&fake_window_close,
                callback_data->saved_dspl_descr,
                callback_data->edit_file,
                NULL,
                True,          /* no prompt */
                False);
   /* does not return when the last one goes away. */
   /* xsmp_close gets called when the last display description is deleted */
   /* xsmp_display_check is called for other than the last display description
      and saved_dspl_descr gets updated */

}

} /* end of DieProc */


/************************************************************************

NAME:      SaveCompleteProc -   Process xsmp shutdown canceled.


PURPOSE:    This routine is called if the X Session Manager sends a
            SaveComplete Message.

PARAMETERS:
   1.  smc_conn       - SmcConn type (opaque) (INPUT)
                        This token represents the Session Manager Connection/

   2.  client_data    - SmPointer type (INPUT)
                        This is the dspl_descr->xsmp_private_data
                        field described by XSMP_CALLBACK_DATA.


FUNCTIONS :
   This is a null function.  It does not do anything.

*************************************************************************/

static void SaveCompleteProc(SmcConn     smc_conn,
                             SmPointer   client_data)
{
XSMP_CALLBACK_DATA        *callback_data = (XSMP_CALLBACK_DATA *)client_data;

DEBUG1(fprintf(stderr, "*** SaveCompleteProc called\n");)


} /* end of SaveCompleteProc */


/************************************************************************

NAME:      ShutdownCancelledProc -   Process xsmp shutdown canceled.


PURPOSE:    This routine is called if the X Session Manager sends a
            ShutdownCanelled Message.

PARAMETERS:
   1.  smc_conn       - SmcConn type (opaque) (INPUT)
                        This token represents the Session Manager Connection/

   2.  client_data    - SmPointer type (INPUT)
                        This is the dspl_descr->xsmp_private_data
                        field described by XSMP_CALLBACK_DATA.


FUNCTIONS :
   This is a null function.  It does not do anything.

*************************************************************************/

static void ShutdownCancelledProc(SmcConn     smc_conn,
                                  SmPointer   client_data)
{
XSMP_CALLBACK_DATA        *callback_data = (XSMP_CALLBACK_DATA *)client_data;

DEBUG1(fprintf(stderr, "*** ShutdownCancelledProc called\n");)


} /* end of ShutdownCancelledProc */


static void get_userid(char   *userid, int  sizeof_userid)
{
struct passwd *pw_ptr;
int    len;
char  *ptr;


if (userid)
   {
      setpwent();  /* make sure password file is rewound, cp, processing could cause this to be needed */

      pw_ptr = getpwuid(getuid());
      if (!pw_ptr)
         strlcpy(userid, pw_ptr->pw_name, sizeof_userid);
      else
         {
            ptr = getenv("USER");
            if (ptr)
               strlcpy(userid, ptr, sizeof_userid);
            else
               {
                  ptr = getenv("LOGNAME");
                  if (ptr)
                     strlcpy(userid, ptr, sizeof_userid);
                  else
                     {
                        ptr = getenv("USERNAME");
                        if (ptr)
                           strlcpy(userid, ptr, sizeof_userid);
                        else
                           strlcpy(userid, "**UNKNOWN**", sizeof_userid);
                     }
               }
         }
      endpwent();
   }
}

static char  *quoted_parm(char *the_parm)
{
char                    *new_parm;
char                     quote;
int                      new_len = strlen(the_parm) + 3; /* two quotes and a nul */
char                    *p;
char                    *q;
char                    *end;
char                    *start = the_parm;

end = start + strlen(the_parm);

/*******************************************************************
*  If the parm is already quoted, skip the quotes.
*******************************************************************/
if ((*start == '"') || (*start == '\''))
   start++;
if ((*(end-1) == '"') || (*(end-1) == '\''))
   end--;

/*******************************************************************
* res 11/11/2005 for -bgc #a0a0a0 use -bgc @a0a0a0 which is supported
* in xutil.c as an extension, -fgc too 
*  Special case for the #hexcolor -> @hexcolor
*******************************************************************/
if ((*start == '#') && (strspn(start, "@#0123456789abcdefABCDEF") == strlen(start)))
   {
      new_parm = malloc_copy(start);
      *new_parm = '@';
      return(new_parm);
   }

/*******************************************************************
*  Count the things which need to be escaped.
*******************************************************************/
for (p = start; p < end; p++)
   if ((*p == '\\') || (*p == '"'))
      new_len++;

/*******************************************************************
*  Malloc the returned string
*******************************************************************/
if ((new_parm = (char *)CE_MALLOC(new_len)) == NULL)
   return(NULL);

*new_parm = '"';
p = start;
q = new_parm+1;

while(p < end)
{
   if ((*p == '\\') || (*p == '"'))
      *q++ = '\\';
   *q++ = *p++;
}

*q++ = '"';
*q = '\0';

return(new_parm);

} /* end of quoted_parm */


#ifdef blah
{
/* for debugging use */
FILE  *test1;
test1 = fopen("/tmp/zce.txt", "a");
if (test1)
   {
      fprintf(test1, "quoted_parm = |%|!n", the_parm);
      fclose(test1);
   }
}

#endif

/************************************************************************

NAME:      child_of_root       -   Find the window to register the CC list property on

PURPOSE:    This routine finds the window whose parent is the root window starting
            at the passed window.  This may the the passed window or one of
            its ancestors.
            This routine copied from cc.c

PARAMETERS:
   1.  display     - pointer to Display (INPUT)
                     This is the display name needed for X calls.

   2.  window      - Window (INPUT - X type)
                     This is the window to start with.

FUNCTIONS :
   1.   Call XQueryTree to find the parent of the passed window.

   2.   Follow this list of parents till the window whose parent
        is the root window is found.  This compensates for window managers.

RETURNED VALUE:
   wm_window   -  Window
                  The window whose window is the root window is returned.

*************************************************************************/

static Window  child_of_root(Display   *display,
                             Window     window)
{
int               ok;
Window            root_return;
Window            parent_return;
Window           *child_windows = NULL;
unsigned int      nchildren;

DEBUG1(fprintf(stderr, "@child_of_root in xsmp:  our window: 0x%X ", window);)

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

DEBUG1(fprintf(stderr, "child_of_root in xsmp:  WM window: 0x%X\n", window);)

if (child_windows)
   XFree((char *)child_windows);
return(window);

} /* end of child_of_root */



/************************************************************************

NAME:      get_leader_window   -   Get a window to use as the "leader window".

PURPOSE:    This routine gets a non-mapped window to use as the XSMP "leader_window".
            This window never gets mapped and as cc windows come and go, we do not have
            to deal with the changes.  This is similar to what gnome termina does.

PARAMETERS:
   1.  display     - pointer to Display (INPUT)
                     This is the display name needed for X calls.

   2.  our_window  - pointer to Window (OUTPUT - X type)
                     This is the window window we create.

   3.  wm_window   - pointer to Window (OUTPUT - X type)
                     This is the outermost window, the window manager window.

FUNCTIONS :
   1.   Create a simple window which we will not map.

   2.   Find the window manager window for this window.

RETURNED VALUE:
   rc          -  int
                  0   -  Success
                  -1  -  Could not create a window

*************************************************************************/

static int  get_leader_window(Display   *display,     /* input  */
                              Window    *our_window,  /* output */
                              Window    *wm_window)   /* output */
{
XEvent           event_union;      /* The event structure filled in by X as events occur            */

*our_window  = None;
*wm_window   = None;

DEBUG9(XERRORPOS)
*our_window = XCreateSimpleWindow(display, 
                                  RootWindow(display, DefaultScreen(display)),
                                  -1000, -1000,  /* x and y */
                                  1, 1,    /* w and h */
                                  0, 0, 0);  /* border width, border, background */
if (*our_window == None)
   {
      DEBUG1(fprintf(stderr, "xsmp.c: get_leader_window: Could not create a window for xsmp\n");)
      return(-1);
   }

*wm_window = child_of_root(display, *our_window);
DEBUG1(fprintf(stderr, "xsmp.c: get_leader_window: leader window: 0x%X, leader WM window 0x%X\n", *our_window, *wm_window);)
   

return(0);

} /* end of get_leader_window */


/************************************************************************

NAME:      xsmp_create_cc_cmds  - build the cmdf file with cc commands


PURPOSE:    This routine takes care of building the cc windows if there
            are more than one open.

PARAMETERS:
   1.  dspl_descr          - pointer to DISPLAY_DESCR (INPUT)
                             This is the dspl_descr. 

FUNCTIONS :
   1.   See if there are any cc windows.  If not, return NULL.

   2.   Get the Client ID Atom from the X server

   3.   Set the property for this atom on the window manager window.

RETURNED_VALUE:
   cc_file      pointer to char
                NULL - No cc command file built.

*************************************************************************/


static char *xsmp_create_cc_cmds(DISPLAY_DESCR      *dspl_descr,
                                 const char         *client_id)
{
XSMP_CALLBACK_DATA        *callback_data = (XSMP_CALLBACK_DATA *)dspl_descr->xsmp_private_data;
static char                cc_file[MAXPATHLEN];
FILE                      *cc_stream;
DISPLAY_DESCR             *walk_dspl;
const char                *paste_buff_dir;
char                      *temp;
int                        argc;
int                        i;
char                      *argv[MAX_ARGV_SIZE];
char                      *p;
char                       work_geometry[128];

if (!dspl_descr->next || (dspl_descr->next == dspl_descr))
   return(NULL);

paste_buff_dir = setup_paste_buff_dir("xsmp_create_cc_cmds");
if (!paste_buff_dir)
   return(NULL);

snprintf(cc_file, sizeof(cc_file), "%s/xsmp_cc_cmdf.%s", paste_buff_dir, client_id);
DEBUG1(fprintf(stderr, "xsmp_create_cc_cmds: cc file : %s\n", cc_file);)

if ((cc_stream = fopen(cc_file, "w")) == NULL)
   {
      DEBUG1(fprintf(stderr, "xsmp_create_cc_cmds: Cannot open %s for output (%s)\n", cc_file, strerror(errno));)
      return(NULL);
   }

for (walk_dspl = dspl_descr->next; walk_dspl != dspl_descr; walk_dspl = walk_dspl->next)
{
   /***************************************************************
   *  First, get the command line arguments for this display
   *  descriptor.  This creates an argc and argv.
   ***************************************************************/
   temp = OPTION_VALUES[GEOMETRY_IDX];
   snprintf(work_geometry, sizeof(work_geometry), "%dx%d%+d%+d",
                          walk_dspl->main_pad->window->width,
                          walk_dspl->main_pad->window->height,
                          walk_dspl->main_pad->window->x,
                          walk_dspl->main_pad->window->y);
   OPTION_VALUES[GEOMETRY_IDX] = work_geometry;
   build_cmd_args(walk_dspl, callback_data->edit_file, callback_data->arg0, &argc, argv);
   OPTION_VALUES[GEOMETRY_IDX] = temp;

   fputs("cc ", cc_stream);

   for (i = 1; i < argc-1; i++) /* skip the command name or shell name */
   {
      if (argv[i][0] == '-')
         fprintf(cc_stream, " %s ", argv[i]);
      else
         fprintf(cc_stream, " \"%s\"", argv[i]);
   }
   fputc('\n', cc_stream);
}

fclose(cc_stream);

return(cc_file);

} /* end of xsmp_create_cc_cmds */



void  debug_request_save_yourself(DISPLAY_DESCR      *dspl_descr)
{
XSMP_CALLBACK_DATA        *callback_data = (XSMP_CALLBACK_DATA *)dspl_descr->xsmp_private_data;

SmcRequestSaveYourself (
    callback_data->sm_connection		/* smcConn */,
    SmSaveLocal			/* saveType */,
    False   		/* shutdown */,
    SmInteractStyleAny			/* interactStyle */,
    False		/* fast */,
    False		/* global */
);

}


#endif

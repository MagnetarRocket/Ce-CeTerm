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
*  module ceapi.c
*
*  This module provides the application program interface to Ce.
*  It is compiled into libCeapi.a and allows client programs to
*  interface with Ce.
*
*  Routines:
*     ceApiInit             -   Initialization routine to find the ce window.
*     ceGetDisplay          -   Return the initialized display pointer
*     ceXdmc                -   Send a dm command to a window
*     ceGetLines            -   Get one or more lines
*     cePutLines            -   Insert or replace one or more lines
*     ceDelLines            -   Delete one or more lines
*     ceGetMsg              -   Get the contents of the message window.
*     ceGetStats            -   Get the current ce stats (line count, current position, etc)
*     ceReadPasteBuff       -   Get the current value of a Ce paste buffer
*
* Internal:
*     selection_request      -   Process an anticipated SelectionRequest
*     find_select_request    -   Look for the selection request
*     get_ce_env_var         -   Get the value of an environment variable from ce
*     grab_baton             -   Get control of the selection for the a paste buffer
*     wait_for_baton_loss    -   Wait for the baton to be grabbed by ce
*     find_selection_clear   -   Look for an anticipated SelectionClear
*     wait_for_stats         -   Wait for the client message from Ce with the stats.
*     find_client_message    -   Look for the client message from Ce
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /bsd4.3/usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h   /usr/include/sys/errno.h      */
#include <ctype.h>          /* /usr/include/ctype.h      */
#include <malloc.h>         /* /usr/include/malloc.h     */
#include <sys/types.h>      /* /usr/include/sys/types.h  */
#ifdef WIN32
#include <winsock.h>
#ifndef MAXPATHLEN
#define MAXPATHLEN	 256
#include <time.h>       /* /usr/include/sys/time.h      */
#endif
#else
#include <netinet/in.h>     /* /bsd4.3/usr/include/netinet/in.h */
#ifndef MAXPATHLEN
#define MAXPATHLEN	1024
#endif
#include <sys/time.h>       /* /usr/include/sys/time.h      */
#endif
#ifndef FD_ZERO
#include <sys/select.h>     /* /usr/include/sys/select.h  */
#endif

#include <X11/X.h>         /* /usr/include/X11/X.h    */
#include <X11/Xlib.h>      /* /usr/include/X11/Xlib.h */

/* main needed to get cc.h constants to be declared */
#define _MAIN_ 1
#include "apistats.h"
#include "cc.h"
/* on Solaris 2.3 the memdata macro total_lines conflicts with CeStats.total lines */
#undef total_lines
#include "ceapi.h"
#include "debug.h"
#include "dmsyms.h"
#include "drawable.h"
#include "pastebuf.h"
#include "pad.h"     /* needed for macro HT */
#include "xerror.h"
#include "xerrorpos.h"

#define CE_MALLOC(size) malloc(size)
#define dm_error(m,j) fprintf(stderr, "%s\n", m)

void   sleep(int secs);
void   usleep(int usecs);
#ifndef WIN32
void   srand(int seed);
int    rand(void);
#endif

/***************************************************************
*
*  Static data initialized by ceApiInit
*
*  display    - The display opened to communicate with X.
*  ce_window  - This is the window id of the window we will be
*               communicating with.
*  paste_name - The window id in hex character string format.  This
*               name is used as the private paste buffer between this
*               set of routines and the ce window.
*  paste_atom - The atom id which corresponds to the paste_name.
*
***************************************************************/

/*static DRAWABLE_DESCR  lcl_window;*/
static Window          lcl_window_id;
static Window          ce_window = None;
static char            paste_name[20];
static Atom            paste_atom = 0;
static Display        *display;

#define DEFAULT_TIMEOUT 10
#define DEBUG_ENV_NAME "CEDEBUG"

/***************************************************************
*
*  Prototypes for local routines.
*
***************************************************************/

static char   *get_ce_env_var(char   *var);

static int   grab_baton(char *name);

static int   wait_for_baton_loss(Atom   atom,
                                 int    timeout_secs);

static int selection_request(Atom   paste_atom,
                             int    timeout_secs);

static Bool find_select_request(Display        *display,
                                XEvent         *event,
                                char           *arg);

static Bool find_selection_clear(Display        *display,
                                XEvent         *event,
                                char           *arg);

static int   wait_for_stats(Atom    atom,
                            XEvent *event,
                            int     timeout_secs);

static Bool find_client_message(Display        *display,
                                XEvent         *event,
                                char           *arg);

/***************************************************************
*
*  System routines which have no built in defines
*
***************************************************************/

char   *getenv(const char *name);
int    fork(void);
int    getpid(void);
int    setpgrp(int pid, int pgrp);  /* bsd version */
int    isatty(int fd);
#ifndef WIN32
int    gethostname(char  *target, int size);
#endif
static char *copyright = "Copyright (c) 2001, A. G. Communication Systems Inc and Emerging Technologies Group, Inc.  All rights reserved.";
char *getcwd(char *buf, int size);


#ifdef DebuG
/***************************************************************
*  
*  macro to translate event to its name
*  The unsigned in the macro takes care of invalid negative values.
*  
***************************************************************/

#define EVENT_NAME(event) (((unsigned)event < LASTEvent) ? event_names[event] : "Out of Range value")

char *event_names[] = {
"0",
"1",
"KeyPress",
"KeyRelease",
"ButtonPress",
"ButtonRelease",
"MotionNotify",
"EnterNotify",
"LeaveNotify",
"FocusIn",
"FocusOut",
"KeymapNotify",
"Expose",
"GraphicsExpose",
"NoExpose",
"VisibilityNotify",
"CreateNotify",
"DestroyNotify",
"UnmapNotify",
"MapNotify",
"MapRequest",
"ReparentNotify",
"ConfigureNotify",
"ConfigureRequest",
"GravityNotify",
"ResizeRequest",
"CirculateNotify",
"CirculateRequest",
"PropertyNotify",
"SelectionClear",
"SelectionRequest",
"SelectionNotify",
"ColormapNotify",
"ClientMessage",
"MappingNotify"};
#endif



/************************************************************************

NAME:      ceApiInit             -   Initialization routine to find the ce window.


PURPOSE:    This routine performs initialization and finds the ce window we need to
            communicate with.

PARAMETERS:

   1.  cePid     -  int (INPUT)
                    This is the pid of the ce process to look for.  It is used
                    when a process starts a ce and then goes on to use the
                    api to communicate with it or when the api is used from
                    a process not started by the ce cpo process.
                    Note that if the CE_WINDOW environment is set, specifying 
                    this parameter to a value other than zero will cause the
                    CE_WINDOW value to be ignored.
                    VALUES:
                    0   -  Used when the application is run from cpo
                    pid - the pid of the ce process when the api is not called from a cpo.

GLOBAL DATA:

lcl_window       -  DRAWABLE_DESCR (OUTPUT)
                    This structure contains the display pointer and
                    the window id of the local window we create but
                    do not map.
                    The display is opened by this routine.
                    It is initially NULL, but is set to -1 on
                    failure.

ce_window        -  int (OUTPUT)
                    The window id we will be communicating with

paste_name       -  int (OUTPUT)
                    The window id in hex character string format.  This
                    name is used as the private paste buffer between this
                    set of routines and the ce window.

paste_atom       -  int (OUTPUT)
                    This is the atom used for the paste name.


FUNCTIONS :

   1.   Check the environment variables for the CE_WINDOW and the CEHOST
        names.  If CE_WINDOW is not set, we are not being called via the
        dm cpo, cps, or !(bang) commands.

   2.   Open the display.  If DISPLAY is available, use it.  Otherwise
        if the CEHOST variable is available, use it as
        the display name.  Otherwise, fail.

   3.   Create a window to use with the x requests.  We will never map
        this window.

   4.   Call ccxdmc with a null command to find and validate the window id.
        If it fails, issue an error message and quit.

   5.   Create the paste buffer name and atom.


RETURNED VALUE:
   rc  -  return code
          0  -  Successfull initialization
         -1  -  Error detected, message already output.  must abort.

*************************************************************************/

int  ceApiInit(int  cePid)
{
int               rc;
char             *cehost;
char             *window_string;
char             *display_name;
char              display_buff[80];

cehost = getenv("CEHOST");
window_string = getenv("CE_WINDOW");
display_name  = getenv("DISPLAY");
/*lcl_window.display = &display;*/

/***************************************************************
*
*  Get debugging flag from the environment.  If it is high
*  enough turn on X syncronization with the server.
*
***************************************************************/

DEBUG0(Debug(DEBUG_ENV_NAME);)

DEBUG18(
   if (!cehost)
      fprintf(stderr, "CEHOST was not set in the environment\n");
   else
      fprintf(stderr, "CEHOST=\"%s\"\n", cehost);

   if (!window_string)
      fprintf(stderr, "CE_WINDOW was not set in the environment\n");
   else
      fprintf(stderr, "CE_WINDOW=\"%s\"\n", window_string);

   if (!display_name)
      fprintf(stderr, "DISPLAY was not set in the environment\n");
   else
      fprintf(stderr, "DISPLAY=\"%s\"\n", display_name);


)

if (window_string)
   sscanf(window_string, "%lX", &ce_window);
else
   ce_window = None;

if (!display_name)
   if (cehost)
      {
         snprintf(display_buff, sizeof(display_buff), "%s", cehost);
         display_name = display_buff;
      }
   else
      {
         errno = ENXIO;
         display = (Display *)-1;
         DEBUG18(fprintf(stderr, "No display name available\n");)
         return(-1);
      }

display = XOpenDisplay(display_name);
if (!display)
    return(-1);

lcl_window_id = XCreateSimpleWindow(
                       display,
                       RootWindow(display, DefaultScreen(display)),
                       0, 0, 100, 100, 1, /* arbitrary position and size since we never map it */
                       BlackPixel(display, DefaultScreen(display)),
                       WhitePixel(display, DefaultScreen(display)));


 if (lcl_window_id == None)
    {
       DEBUG18(fprintf(stderr, "Could not create window (%s)\n", strerror(errno));)
       return(-1);
    }

/* needed to make ceGetLines work */
XSelectInput(display,
             lcl_window_id,
             PropertyChangeMask);


rc = cc_xdmc(display,
             cePid,
             ";",
             &ce_window,
             False);  /* quiet mode */

if (rc)
   return(rc);

snprintf(paste_name, sizeof(paste_name), "%X", ce_window);

paste_atom = XInternAtom(display, paste_name, False);

return(0);

} /* end of ceApiInit */


/************************************************************************

NAME:      ceGetDisplay - Return the initialized display pointer


PURPOSE:    This routine is used if the Api user wants to do X windows stuff.

PARAMETERS:

   None

GLOBAL DATA:

display          -  pointer to Display (Xlib type) (INPUT)
                    The display value is returned.


FUNCTIONS :

   1.   Return the display value.


RETURNED VALUE:
   display  -   Display (Xlib type)
                NULL  - The display is not open
                !NULL - The display pointer

*************************************************************************/

Display   *ceGetDisplay(void)
{
   if (display == (Display *)-1)
      return(NULL);
   else
      return(display);

} /* end of ceGetDisplay */


/************************************************************************

NAME:      ceXdmc                -   Send a dm command to a window


PURPOSE:    This routine sends the passed string to ce window.

PARAMETERS:

   1.  display     - pointer to Display (INPUT)
                     This is the display handle needed for X calls.


GLOBAL DATA:
display          -  pointer to Display (Xlib type) (INPUT)
                    The display is opened by ceApiInit.
                    It is initially NULL, but is set to -1 on
                    failure.  It is needed for all the X calls.

ce_window        -  int (INPUT)
                    The window id we will be communicating with.
                    It is initialized by ceApiInit.

paste_name       -  int (INPUT)
                    The window id in hex character string format.  This
                    name is used as the private paste buffer between this
                    set of routines and the ce window.

paste_atom       -  int (INPUT)
                    This is the atom used for the paste name.


FUNCTIONS :
   1.   Make sure the initialization routine was called successfully.

   2.   Pass the command to cc_xdmc.


RETURNED VALUE:
   rc  -  return code
          0  -  Command sent
         -1  -  Error detected, message already output.

*************************************************************************/

int  ceXdmc(char    *cmdlist,
            int      synchronous)
{
int                   rc;
char                 *cmds_line;
int                   cmds_line_len;

/***************************************************************
*
*  Make sure the init routine was called and worked.
*
***************************************************************/

if (display == NULL)
   {
      fprintf(stderr, "ceXdmc:  ceApiInit has not been called\n");
      return(-1);
   }
else
   if (display == (Display *)-1)
      {
         fprintf(stderr, "ceXdmc:  ceApiInit initialization failed\n");
         return(-1);
      }

if (synchronous)
   {
      cmds_line_len = strlen(cmdlist) + 25;
      cmds_line = malloc(cmds_line_len);
      if (!cmds_line)
         {
            fprintf(stderr, "ceXdmc:  Out of memory, synchronous option canceled\n");
            synchronous = False;
            cmds_line = cmdlist;
         }
      else
         {
            snprintf(cmds_line, cmds_line_len, "%s;xc %s", cmdlist, paste_name);
            rc = grab_baton(paste_name);
            if (rc)
               {
                  fprintf(stderr, "ceXdmc:  Synchronous option canceled\n");
                  synchronous = False;
                  free(cmds_line);
                  cmds_line = cmdlist;
               }
         }
   }
else
   cmds_line = cmdlist;

rc = cc_xdmc(display,
             0,
             cmds_line,
             &ce_window,
             False);  /* quiet mode */

if ((rc == 0) && synchronous)
   {
      DEBUG18(fprintf(stderr, "ceXdmc:  Waiting for loss of baton %s\n", paste_name);)
      rc = wait_for_baton_loss(paste_atom, DEFAULT_TIMEOUT);
   }

if (cmds_line != cmdlist)
   free(cmds_line);

return(rc);

} /* end of ceXdmc */


/************************************************************************

NAME:      ceGetLines            -   Get one or more lines


PURPOSE:    This routine gets a set of lines from the calling program
            It has to be done at an odd time in the intialization sequence
            (especially in pad mode), so we have a special call for it.

PARAMETERS:

   1.  lineno         - int (INPUT)
                        This is the first line number to be retrieved.  One
                        based line numbers are used.

   2.  count          - int (INPUT)
                        This is the number of lines to be extracted.
                        The special value -1 means to end of data.


GLOBAL DATA:

    Same as ceXdmc



FUNCTIONS :
   1.   Verify that the init function has been called.

   2.   Grab the special paste buffer selection using grab_batton.
        We are using the selection as both the baton and the
        vehicle for carrying data.  If the grab fails, return failure.,

   3.   Build and transmit the following command string:
        <lineno>,<lineno+count>xc <winbuffname>

   4.   Wait for ce to grab the baton to signal it is done.  If ce
        does not grab the baton, the get fails.

   5.   Extract the paste buff data to a malloc'ed area using
        ce_ReadPasteBuf

   6.   Take ownership of the paste special paste buffer name
        using grab baton to free the storage in ce.


RETURNED VALUE:
   buff  -  pointer to char
            NULL      -  Could not retrieve lines
            otherwise -  Pointer to buffer containing the lines.

*************************************************************************/

char  *ceGetLines(int        lineno,
                   int        count)
{
int                   rc;
char                  cmdline[80];
int                   timeout;
char                 *area;

/***************************************************************
*
*  Make sure the init routine was called and worked.
*
***************************************************************/

if (display == NULL)
   {
      fprintf(stderr, "ceGetLines:  ceApiInit has not been called\n");
      return(NULL);
   }
else
   if (display == (Display *)-1)
      {
         fprintf(stderr, "ceGetLines:  ceApiInit initialization failed\n");
         return(NULL);
      }

rc = grab_baton(paste_name);
if (rc)
   return(NULL);

if (count < 0)
   snprintf(cmdline, sizeof(cmdline), "tmw;%d,$xc %s", lineno, paste_name);
else
   if (count == 0)
      snprintf(cmdline, sizeof(cmdline), "tmw;%dxc %s", lineno, paste_name);
   else
      snprintf(cmdline, sizeof(cmdline), "tmw;%d,%dxc %s", lineno, lineno+count, paste_name);

rc = cc_xdmc(display,
             0,
             cmdline,
             &ce_window,
             False);  /* quiet mode */

if (rc)
   return(NULL);

timeout = count / 100;
if (timeout == 0)
   timeout = DEFAULT_TIMEOUT;  /* arbitrary default */

DEBUG18(fprintf(stderr, "ceGetLines:  Waiting for loss of baton %s\n", paste_name);)
rc = wait_for_baton_loss(paste_atom, timeout);
if (rc)
   return(NULL);

area = ceReadPasteBuff(paste_name);

grab_baton(paste_name);

return(area);

} /* end of ceGetLines */


/************************************************************************

NAME:      cePutLines            -   Send lines back to ce


PURPOSE:    This routine sends a block of lines back to ce to either
            be inserted or replace an existing line.

PARAMETERS:

   1.  lineno         - int (INPUT)
                        This is the first line number to be replaced or One
                        the line number to be inserted before.

   2.  count          - int (INPUT)
                        This is the number of lines to be replaced.
                        This line is only interesting in replace mode.
                        It is the number of lines to delete.
                        The special value -1 means to end of data.

   3.  insert         - int (INPUT)
                        True  -  Insert the data
                        False -  Delete the requested number of lines
                                 before sending the data for insert.

   4.  data           - pointer to char (INPUT)
                        This parameter points to the data to send to
                        ce.  It is a null terminated string with
                        embedded newlines.


GLOBAL DATA:

    Same as ceXdmc



FUNCTIONS :
   1.   Verify that the init function has been called.

   2.   Open the special paste buffer for output and
        initialize it's size.

   3.   Use ce_fputs to copy the data to the paste buffer.

   4.   If replace was specified, start creating the command string
        with dr;<lineno>,<lineno+count>xd -l junk;
        Otherwise start the command string with:
        <lineno>

   5.   Attach the command xp <windbuffname>

   7.   Using select and XCheckEvent, wait for the request to transmit
        the paste data.  If we time out, return failure.

   8.   Use the pastebuf routine to send the data.


RETURNED VALUE:
   rc   -  int
           0  -  data sent
          -1  -  Data could not be sent, request for to get data
                 from ce never arrived.

*************************************************************************/

int    cePutLines(int        lineno,
                   int        count,
                   int        insert,
                   char      *data)
{
DMC_xc                xc_dmc;
int                   rc;
char                  cmdline[80];
FILE                 *paste_stream;

/***************************************************************
*
*  Make sure the init routine was called and worked.
*
***************************************************************/

if (display == NULL)
   {
      fprintf(stderr, "cePutLines:  ceApiInit has not been called\n");
      return(-1);
   }
else
   if (display == (Display *)-1)
      {
         fprintf(stderr, "cePutLines:  ceApiInit initialization failed\n");
         return(-1);
      }

xc_dmc.next   = NULL;
xc_dmc.cmd    = DM_xc;
xc_dmc.temp   = False;
xc_dmc.dash_r = True;
xc_dmc.dash_f = False;
xc_dmc.dash_a = False;
xc_dmc.dash_l = False;
xc_dmc.path   = paste_name;


paste_stream = open_paste_buffer((DMC *)&xc_dmc, display, lcl_window_id, CurrentTime, '\\');
if (!paste_stream)
   return(-1);

rc = setup_x_paste_buffer(paste_stream, NULL, 1, 1, strlen(data)+1);
if (rc)
   return(rc);

ce_fputs(data, paste_stream);

ce_fclose(paste_stream);

if (insert)
   snprintf(cmdline, sizeof(cmdline), "tmw;%d;xp %s", lineno, paste_name);
else
   if (count < 0)
      snprintf(cmdline, sizeof(cmdline), "tmw;%d,$xd -l junk;xp %s", lineno, paste_name);
   else
      if (count == 0)
         snprintf(cmdline, sizeof(cmdline), "tmw;%dxd -l junk;xp %s", lineno, paste_name);
      else
         snprintf(cmdline, sizeof(cmdline), "tmw;%d,%dxd -l junk;xp %s", lineno, lineno+count, paste_name);

rc = cc_xdmc(display,
             0,
             cmdline,
             &ce_window,
             False);  /* quiet mode */

if (rc)
   return(rc);

rc = selection_request(paste_atom, DEFAULT_TIMEOUT);
return(rc);

} /* end of cePutLines */

/************************************************************************

NAME:      selection_request    -   Process an anticipated SelectionRequest


PURPOSE:    This routine watches for the SelectionRequest which will be sent
            as a result of the xp command sent to the ce window.  When it
            arrives, it calls the pastebuf routine to process the request.

PARAMETERS:

   1.  atom           - Atom (Xlib type)(INPUT)
                        This is the atom for the paste buffer we are    One
                        waiting to get the selection request on.

   2.  timeout_secs   - int (INPUT)
                        This is the maximum number of seconds to sleep
                        before giving up.

GLOBAL DATA:

    Same as ceXdmc



FUNCTIONS :
   1.   Perform selects on the Xserver socket to watch for
        information.  If the select times out, we have failed.

   2.   Use XCHeckIfEvent to search for the expected SelectionRequest.

   3.   If found, process it with the routines in pastebuf.c.

RETURNED VALUE:
   rc   -  int
           0  -  data sent
          -1  -  Data could not be sent, request for to get data
                 from ce never arrived.

*************************************************************************/

static int selection_request(Atom   paste_atom,
                             int    timeout_secs)
{
struct timeval        time_out;
fd_set                readfds;
int                   nfound;
int                   done = False;
int                   Xserver_socket = ConnectionNumber(display);
XEvent                event_union;


while(!done)
{
   FD_ZERO(&readfds);
   FD_SET(Xserver_socket, &readfds);
   time_out.tv_sec = timeout_secs;
   time_out.tv_usec = 0;
   nfound = select(Xserver_socket+1, HT &readfds, NULL, NULL, &time_out);
   if (nfound)
      {
         done = XCheckIfEvent(display, &event_union, find_select_request, (char *)&paste_atom);
      }
   else
      {
         DEBUG18(fprintf(stderr, "selection_request:  Timed out after %d seconds\n", timeout_secs);)
         done = True;
      }

} /* end of while not done */

/***************************************************************
*  
*  To get here, done was set to true.  Either we timed out or
*  we found the matching event.  The nfound value will tell
*  us this.
*  
***************************************************************/

if (nfound) /* good case */
   {
      send_pastebuf_data(&event_union);  /* in pastebuf.c */
      return(0);
   }
else
   return(-1);


} /* end of selection_request */


/************************************************************************

NAME:      find_select_request - Look for the selection request for the
                                 special paste buffer.


PURPOSE:    This routine is the test routine used in a call to XCheckIfEvent
            It watches for the SelectionRequest for the xp (paste) command
            sent by cePutLines.

PARAMETERS:

   1.  display     - pointer to Display (INPUT)
                     This is the pointer to the current display.

   2.  event       - pointer to XEvent (INPUT)
                     This is the pointer to event to be looked at.

   3.  arg         - pointer to char (really pointer to Atom) (INPUT)
                     The atom we are looking for.

FUNCTIONS :

   1.   Unwrap the user parm.

   2.   If this event is a SelectionNotify on the main window for the
        correct atom, return True (we found it).  Otherwise return False.


*************************************************************************/

/* ARGSUSED */
static Bool find_select_request(Display        *display,
                                XEvent         *event,
                                char           *arg)
{
Atom       search_atom;

search_atom = *((Atom *)arg);

DEBUG15(fprintf(stderr, "find_select_request:  got %s event\n", EVENT_NAME(event->type));)

if ((event->type                        == SelectionRequest) &&
    (event->xselectionrequest.requestor == ce_window)        &&
    (event->xselectionrequest.selection == search_atom))
   return(True);
else
   return(False);

} /* end of find_select_request */


/************************************************************************

NAME:      ceDelLines            -   Send a delete request to ce


PURPOSE:    This routine sends a delete request for a block of lines.

PARAMETERS:

   1.  lineno         - int (INPUT)
                        This is the first line number to be deleted.

   2.  count          - int (INPUT)
                        This is the number of lines to be deleted.
                        It is the number of lines to delete.
                        The special value -1 means to end of data.
                        A count of 0 or 1 means to delete just the
                        line specified.


GLOBAL DATA:

    Same as ceXdmc



FUNCTIONS :
   1.   Verify that the init function has been called.

   2.   Grab the special paste buffer.

   3.   Build and send the command:
        <lineno>,<lineno+count>xd <paste_name>

   4.   Wait for ce to grab the paste_name to show it got the command.

RETURNED VALUE:
   rc   -  int
           0  -  command sent
          -1  -  command could not be sent.

*************************************************************************/

int    ceDelLines(int        lineno,
                   int        count)
{
int                   rc;
char                  cmdline[80];

/***************************************************************
*
*  Make sure the init routine was called and worked.
*
***************************************************************/

if (display == NULL)
   {
      fprintf(stderr, "ceDelLines:  ceApiInit has not been called\n");
      return(-1);
   }
else
   if (display == (Display *)-1)
      {
         fprintf(stderr, "ceDelLines:  ceApiInit initialization failed\n");
         return(-1);
      }

rc = grab_baton(paste_name);
if (rc)
   return(rc);

if (count < 0)
   snprintf(cmdline, sizeof(cmdline), "tmw;%d,$xd %s", lineno, paste_name);
else
   if (count == 0)
      snprintf(cmdline, sizeof(cmdline), "tmw;%dxd -l %s", lineno, paste_name);
   else
      snprintf(cmdline, sizeof(cmdline), "tmw;%d,%dxd %s", lineno, lineno+count, paste_name);

rc = cc_xdmc(display,
             0,
             cmdline,
             &ce_window,
             False);  /* quiet mode */

if (rc)
   return(rc);

DEBUG18(fprintf(stderr, "ceDelLines:  Waiting for loss of baton %s\n", paste_name);)
rc = wait_for_baton_loss(paste_atom, DEFAULT_TIMEOUT);

return(rc);

} /* end of ceDelLines */


/************************************************************************

NAME:      ceGetMsg  -   Get the contents of the message window.


PURPOSE:    This routine gets the contents of the message window.
            ce session we are talking to.

PARAMETERS:

   1.  cmdline     - pointer to char (INPUT)
                     Optional pointer to char of commands to send
                     which will generate the message.



GLOBAL DATA:

    Same as ceXdmc



FUNCTIONS :
   2.   Grab the special paste buffer selection using grab_batton.
        We are using the selection as both the baton and the
        vehicle for carrying data.  If the grab fails, return -1.

   3.   Add any passed commands to the front of the command string:
        tdmo;tl;dr;tr;xc %s;msg ' '
        The commands are expected to generate something in the command window.
        The msg command clears the message window.

   4.   Send the command string to ce.

   5.   Wait for ce to grab the batton when it does the xc.

   6.   Get the data from the paste buffer.

RETURNED VALUE:
   value - pointer to char
           NULL  -  could not get the value
           !NULL -  pointer to the message in malloc'ed storage.

*************************************************************************/

char   *ceGetMsg(char   *cmdlist)
{
int                   rc;
char                 *cmds_line;
int                   cmds_line_len;
char                 *area;

rc = grab_baton(paste_name);
if (rc)
   return(NULL);

if (!cmdlist)
   cmdlist = "";

cmds_line_len = strlen(cmdlist) + 80;
cmds_line = malloc(cmds_line_len);
if (!cmds_line)
   {
      fprintf(stderr, "ceGetMsg:  Out of memory, malloc failed\n");
      return(NULL);
   }
else
   {
      snprintf(cmds_line, cmds_line_len, "%s;tdmo;tl;dr;tr;xc %s", cmdlist, paste_name);
      if (*cmdlist != '\0')
         strcat(cmds_line, ";msg ' '");
   }

rc = cc_xdmc(display,
             0,
             cmds_line,
             &ce_window,
             False);  /* quiet mode */

free(cmds_line);

if (rc)
   return(NULL);


DEBUG18(fprintf(stderr, "ceGetMsg:  Waiting for loss of baton %s\n", paste_name);)
rc = wait_for_baton_loss(paste_atom, DEFAULT_TIMEOUT);
if (rc)
   return(NULL);

area = ceReadPasteBuff(paste_name);

return(area);

} /* end of ceGetMsg */


#ifdef blah
/************************************************************************

NAME:      get_ce_env_var      -   Get an environment variable from ce


PURPOSE:    This gets the value of an environment variable from the 
            ce session we are talking to.

PARAMETERS:

   1.  var         - pointer to char (INPUT)
                     The ce environment variable we want.



GLOBAL DATA:

    Same as ceXdmc



FUNCTIONS :
   2.   Grab the special paste buffer selection using grab_batton.
        We are using the selection as both the baton and the
        vehicle for carrying data.  If the grab fails, return -1.

   3.   Build and transmit the following command string:
        env <passed_name>;tdmo;tl;/=/ar;dr;tr;xc <paste_name>;msg ' '

   4.   Wait for ce to grab the baton to signal it is done.  If ce
        does not grab the baton, the get fails.

   5.   Read the special paste buffer.

   6.   Get the single line with ce_fgets

   7.   Return the address of the value.

RETURNED VALUE:
   value - pointer to char
           NULL  -  could not get the value
           !NULL -  pointer to the string value of the variable
                    in malloc'ed storage.

*************************************************************************/

static char   *get_ce_env_var(char   *var)
{
int                   rc;
char                  cmdline[80];
int                   timeout;
char                 *area;

rc = grab_baton(paste_name);
if (rc)
   return(NULL);

sprintf(cmdline, sizeof(cmdline), "env %s;tdmo;tl;/=/ar;dr;tr;xc %s;msg ' '", var, paste_name);

rc = cc_xdmc(display,
             0,
             cmdline,
             &ce_window,
             False);  /* quiet mode */

if (rc)
   return(NULL);


DEBUG18(fprintf(stderr, "get_ce_env_var:  Waiting for loss of baton %s\n", paste_name);)
rc = wait_for_baton_loss(paste_atom, DEFAULT_TIMEOUT);
if (rc)
   return(NULL);

area = ceReadPasteBuff(paste_name);

return(area);

} /* end of get_ce_env_var */
#endif


/************************************************************************

NAME:      ceGetStats      -   et the current ce stats (line count, current position, etc)


PURPOSE:    This routine sends a clientmessage to ce to get the current stats.

PARAMETERS:

   none

GLOBAL DATA:

    Same as ceXdmc



FUNCTIONS :

   1.   Format a clientmessage event and send it to ce.

   2.   wait for the return.

   3.   Copy the data to a malloced returned structure.

RETURNED VALUE:
   value - pointer to CeStats
           NULL  -  could not get the value
           !NULL -  pointer to a static of the stats
                    described in ceapi.h

*************************************************************************/

CeStats  *ceGetStats(void)
{
int                   rc;
XEvent                message;
static CeStats        returned_stats;
static Atom           ce_cclist_atom = None;


/***************************************************************
*
*  Make sure the init routine was called and worked.
*
***************************************************************/

if (display == NULL)
   {
      fprintf(stderr, "ceGetStats:  ceApiInit has not been called\n");
      return(NULL);
   }
else
   if (display == (Display *)-1)
      {
         fprintf(stderr, "ceGetStats:  ceApiInit initialization failed\n");
         return(NULL);
      }


ce_cclist_atom  = XInternAtom(display, "CE_CCLIST", True);

if (ce_cclist_atom == None)
   {
      fprintf(stderr, "ceGetStats:  Unable to get stats\n");
      return(NULL);
   }

/***************************************************************
*
*  Set up the outbound message and send it
*
***************************************************************/

message.xclient.type = ClientMessage;
message.xclient.serial = 0;
message.xclient.send_event = True;
message.xclient.display = display;
message.xclient.window = lcl_window_id;
message.xclient.message_type = ce_cclist_atom;
message.xclient.format = 8;

XSendEvent(display,
           ce_window,
           False,
           None,
           (XEvent *)&message);

XFlush(display);

/***************************************************************
*
*  Get the inbound message.
*
***************************************************************/

rc = wait_for_stats(ce_cclist_atom,  &message, DEFAULT_TIMEOUT);

if (rc == 0)
   {
      returned_stats.total_lines  =  ntohl(message.TOTAL_LINES_INFILE); 
      returned_stats.current_line =  ntohl(message.CURRENT_LINENO);
      returned_stats.current_col  =  ntohs(message.CURRENT_COLNO);
      returned_stats.writable     =  ntohs(message.PAD_WRITABLE);
      return(&returned_stats);
   }
else
   return(NULL);


} /* end of ceGetStats */


/************************************************************************

NAME: ceReadPasteBuff       -   Get the current value of a Ce paste buffer


PURPOSE:    This routine gets a set of lines from the calling program
            It has to be done at an odd time in the initialization sequence
            (especially in pad mode), so we have a special call for it.

PARAMETERS:

   1.  paste_name     - pointer to char (INPUT)
                        This is the name of the X paste buffer to read. One


GLOBAL DATA:

    Same as ceXdmc



FUNCTIONS :
   1.   Verify that the init function has been called.

   2.   Read the passed paste buffer.

   3.   Get the total pastebuf size (new pastebuf.c function).

   4.   Malloc space for the paste buff area.

   5.   Copy all the data into the malloced area using ce_fgets.


RETURNED VALUE:
   buff  -  pointer to char
            NULL      -  Could not retrieve lines
            otherwise -  Pointer to buffer containing the lines.

*************************************************************************/

char  *ceReadPasteBuff(char  *name)
{
DMC_xp       xp_dmc;
FILE        *paste_stream;
int          area_size;
char        *area;
char        *p;
int          i;

if (display == NULL)
   {
      fprintf(stderr, "cePutLines:  ceApiInit has not been called\n");
      return(NULL);
   }
else
   if (display == (Display *)-1)
      {
         fprintf(stderr, "cePutLines:  ceApiInit initialization failed\n");
         return(NULL);
      }

xp_dmc.next   = NULL;
xp_dmc.cmd    = DM_xp;
xp_dmc.temp   = False;
xp_dmc.dash_r = False;
xp_dmc.dash_f = False;
xp_dmc.dash_a = False;
xp_dmc.dash_l = False;
xp_dmc.path   = name;

paste_stream = open_paste_buffer((DMC *)&xp_dmc, display, lcl_window_id, CurrentTime, '\\');

DEBUG18(fprintf(stderr, "ceReadPasteBuff: %s paste buffer %s\n", (paste_stream ? "Opened " : "Could not open "), (name ? name : "<default>"));)

if (paste_stream == NULL)
   return(NULL);

area_size = get_pastebuf_size(paste_stream) + 1; /* add in for the null terminator */
if (area_size < 0)
   return(NULL);
area_size += (sizeof(int) - (area_size % sizeof(int))); /* round up to an even number */
DEBUG18(fprintf(stderr, "ceReadPasteBuff: paste buffersize = %d\n", area_size);)

area = malloc(area_size);
if (area == NULL)
   {
      fprintf(stderr, "ceReadPasteBuff:  Could not malloc %d bytes\n", area_size+1);
      return(NULL);
   }

p = area;
while(ce_fgets(p, area_size, paste_stream) != NULL)
{
   i = strlen(p);
   p += i;
   area_size -= i;
}

DEBUG18(fprintf(stderr, "ceReadPasteBuff: Read %d bytes from paste buffer %s = \"%s\"\n",(p - area), (name ? name : "<default>"), area);)
return(area);

} /* end of ceReadPasteBuff */


/************************************************************************

NAME:      grab_baton    -  Get control of the selection for the a paste buffer


PURPOSE:    This routine gets ownership of the baton selection.

PARAMETERS:
   1.  name           - pointer to char (INPUT)
                        This is the name of the paste buffer to be
                        grabbed.  It may be the main one used to talk
                        to ce or the baton version.


FUNCTIONS :
   1.   Open the paste buffer with the baton name for output to get
        control of the baton selection.

   2.   Size the buffer to a small size.

   3.   Close the buffer to perform housekeeping cleanup.


RETURNED VALUE:
   rc   -  int
           0  -  open worked
          -1  -  Could not get control of paste buffer selection.

*************************************************************************/

static int   grab_baton(char *name) 
{
DMC_xc       xc_dmc;
FILE        *paste_stream;
int          rc;

xc_dmc.next   = NULL;
xc_dmc.cmd    = DM_xc;
xc_dmc.temp   = False;
xc_dmc.dash_r = True;
xc_dmc.dash_f = False;
xc_dmc.dash_a = False;
xc_dmc.dash_l = False;
xc_dmc.path   = name;

paste_stream = open_paste_buffer((DMC *)&xc_dmc, display, lcl_window_id, CurrentTime, '\\');

if (paste_stream == NULL)
   return(-1);

rc = setup_x_paste_buffer(paste_stream, NULL, 1, 1, 4 /* 4 is an arbitrary size */);
DEBUG18(
   if (rc == 0)
      fprintf(stderr, "Baton %s grabbed\n", (name ? name : "<default>"));
   else
      fprintf(stderr, "Unable to grab baton %s\n", (name ? name : "<default>"));
)
if (rc)
   return(rc);

ce_fclose(paste_stream);

return(0);

} /* end of grab_baton */


/************************************************************************

NAME:   wait_for_baton_loss    -   Wait for the baton to be grabbed
                                   by ce


PURPOSE:    This command waits for ce to grab the baton to show the
            command has completed.

PARAMETERS:
   1.  atom           - Atom (Xlib type)(INPUT)
                        This is the atom for the paste buffer we are    One
                        waiting to get the selection request on.

   2.  timeout_secs   - int (INPUT)
                        This is the number of seconds to wait before
                        assuming the request will not arrive.


FUNCTIONS :
   1.   Use select to wait for input on the display socket with a
        timeout.  If the timeout occurs, fail.

   2.   Use XCheckIfEvent to look for the selection notify that
        says the event has been found.

   3.   Repeat until a timeout occurs or the selection is found.


RETURNED VALUE:
   rc   -  int
           0  -  Baton passed to ce
          -1  -  Timed out.

*************************************************************************/

static int   wait_for_baton_loss(Atom   atom,
                                 int    timeout_secs)
{
struct timeval        time_out;
fd_set                readfds;
int                   nfound;
int                   done = False;
int                   Xserver_socket = ConnectionNumber(display);
XEvent                event_union;


while(!done)
{
   FD_ZERO(&readfds);
   FD_SET(Xserver_socket, &readfds);
   time_out.tv_sec = timeout_secs;
   time_out.tv_usec = 0;
   nfound = select(Xserver_socket+1, HT &readfds, NULL, NULL, &time_out);
   if (nfound)
      {
         done = XCheckIfEvent(display, &event_union, find_selection_clear, (char *)&atom);
      }
   else
      {
         DEBUG18(fprintf(stderr, "selection_request:  Timed out after %d seconds\n", timeout_secs);)
         done = True;
      }

} /* end of while not done */

/***************************************************************
*  
*  To get here, done was set to true.  Either we timed out or
*  we found the matching event.  The nfound value will tell
*  us this.
*  
***************************************************************/

if (nfound) /* good case */
   {
      clear_pastebuf_owner(&event_union);  /* in pastebuf.c */
      DEBUG18(fprintf(stderr, "wait_for_baton_loss:  got selection clear for baton #%d\n", atom);)
      return(0);
   }
else
   {
      DEBUG18(fprintf(stderr, "wait_for_baton_loss:  timed out for baton #%d\n", atom);)
      return(-1);
   }

} /* end of wait_for_baton_loss */


/************************************************************************

NAME:      find_selection_clear - Look for the selection clear for a paste buffer


PURPOSE:    This routine is the test routine used in a call to XCheckIfEvent
            It watches for the SelectionClear which is receieved when we send an
            xc command to the ce window.

PARAMETERS:

   1.  display     - pointer to Display (INPUT)
                     This is the pointer to the current display.

   2.  event       - pointer to XEvent (INPUT)
                     This is the pointer to event to be looked at.

   3.  arg         - pointer to char (really pointer to Atom) (INPUT)
                     The atom we are looking for.

FUNCTIONS :

   1.   Unwrap the user parm.

   2.   If this event is a SelectionClear on the main window for the
        correct atom, return True (we found it).  Otherwise return False.


*************************************************************************/

/* ARGSUSED */
static Bool find_selection_clear(Display        *display,
                                XEvent         *event,
                                char           *arg)
{
Atom       search_atom;

search_atom = *((Atom *)arg);

DEBUG15(
   fprintf(stderr, "find_selection_clear:  got %s event\n", EVENT_NAME(event->type));
   if (event->type == SelectionClear)
      fprintf(stderr, "window = 0x%X, ce_window = 0x%X\nselection = %d, search_atom = %d\n",
              event->xselectionclear.window, ce_window, event->xselectionclear.selection, search_atom);
)

if ((event->type                      == SelectionClear)   &&
    (event->xselectionclear.selection == search_atom))
   return(True);
else
   return(False);


} /* end of find_selection_clear */


/************************************************************************

NAME:   wait_for_stats   -   Wait for the client message from Ce with the stats.


PURPOSE:    This command waits for ce to return the stats buffer.
            command has completed.

PARAMETERS:
   1.  atom           - Atom (Xlib type)(INPUT)
                        This is the atom for the paste buffer we are    One
                        waiting to get the selection request on.

   2.  event          - pointer to XEvent (OUTPUT)
                        The returning client message event is
                        put in this parameter when and if it arrives.

   3.  timeout_secs   - int (INPUT)
                        This is the number of seconds to wait before
                        assuming the request will not arrive.


FUNCTIONS :
   1.   Use select to wait for input on the display socket with a
        timeout.  If the timeout occurs, fail.

   2.   Use XCheckIfEvent to look for the selection notify that
        says the event has been found.

   3.   Repeat until a timeout occurs or the selection is found.


RETURNED VALUE:
   rc   -  int
           0  -  Got the event
          -1  -  Timed out.

*************************************************************************/

static int   wait_for_stats(Atom    atom,
                            XEvent *event,
                            int     timeout_secs)
{
struct timeval        time_out;
fd_set                readfds;
int                   nfound;
int                   done = False;
int                   Xserver_socket = ConnectionNumber(display);


while(!done)
{
   FD_ZERO(&readfds);
   FD_SET(Xserver_socket, &readfds);
   time_out.tv_sec = timeout_secs;
   time_out.tv_usec = 0;
   nfound = select(Xserver_socket+1, HT &readfds, NULL, NULL, &time_out);
   if (nfound)
      {
         done = XCheckIfEvent(display, event, find_client_message, (char *)&atom);
      }
   else
      {
         DEBUG18(fprintf(stderr, "wait_for_stats:  Timed out after %d seconds\n", timeout_secs);)
         done = True;
      }

} /* end of while not done */

/***************************************************************
*  
*  To get here, done was set to true.  Either we timed out or
*  we found the matching event.  The nfound value will tell
*  us this.
*  
***************************************************************/

if (nfound) /* good case */
   return(0);
else
   return(-1);

} /* end of wait_for_stats */


/************************************************************************

NAME:      find_client_message - Look for the client message from Ce


PURPOSE:    This routine is the test routine used in a call to XCheckIfEvent
            It watches for the ClientMessage which is send by Ce to us.

PARAMETERS:

   1.  display     - pointer to Display (INPUT)
                     This is the pointer to the current display.

   2.  event       - pointer to XEvent (INPUT)
                     This is the pointer to event to be looked at.

   3.  arg         - pointer to char (really pointer to Atom) (INPUT)
                     The atom we are looking for.

FUNCTIONS :

   1.   Unwrap the user parm.

   2.   If this event is a ClientMessage for the
        correct atom, return True (we found it).  Otherwise return False.


*************************************************************************/

/* ARGSUSED */
static Bool find_client_message(Display        *display,
                                XEvent         *event,
                                char           *arg)
{
Atom       search_atom;

DEBUG15(fprintf(stderr, "find_client_message:  got %s event\n", EVENT_NAME(event->type));)

search_atom = *((Atom *)arg);

if ((event->type                        == ClientMessage)   &&
    (event->xclient.message_type        == search_atom))
   return(True);
else
   return(False);

} /* end of find_client_message */




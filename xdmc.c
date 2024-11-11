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

/**************************************************************
*
*  MODULE xdmc - Arpus/Ce execute dm command routine
*
*  OVERVIEW
*      This program can be called from a shell to execute dm
*      commands in the current ceterm or in a different ceterm.
*      It can also be used to list the current windows.
*
*  ARGUMENTS:
*      xdmc [-p <pid>] [-q] [-w <windowid>] [-display <node:0.0>] [-list [-cmd]] [-ver]  \"dm;cmd;list\
*      -p <pid>      -   send the commands to the Ce window whose shell
*                        has the specified pid.
*      -w <windowid> -   send the commands to the Ce window with
*                        the specified hex window id (use -list to find it)
*      -display <node:0.0> -  open the specified node.
*      -list         -   List all the Arpus/Ce windows
*      -cmd          -   Valid only with -list, list the command property
*                        used by a session manager.
*      -ver          -   print the version and quit
*      "cmds"        -   List of dm commands enclosed in quotes to
*                        be a single argument to this routine.
*
***************************************************************/


#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <limits.h>         /* /usr/include/limits.h     */
#include <errno.h>          /* /usr/include/errno.h     */
#ifdef WIN32
typedef char *caddr_t;
#endif


#include <X11/Xlib.h>       /* /usr/include/X11/Xlib.h   */
#include <X11/Xutil.h>      /* /usr/include/X11/Xutil.h  */
#include <X11/keysym.h>     /* /usr/include/X11/keysym.h */
#include <X11/Xatom.h>      /* /usr/include/X11/Xatom.h */

#define _MAIN_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "cc.h"
#include "debug.h"
#include "getxopts.h"
#ifndef HAVE_CONFIG_H
#include "help.h"
#endif
#include "xerror.h"  /* note we have our own ce_xerror */
#include "xerrorpos.h"

#ifndef linux
char *malloc(int size);
#endif
char *getenv(char *name);
int  ce_error_bad_window_flag;

static int _XPrintDefaultError (Display     *dpy,
                                XErrorEvent *event,
                                FILE        *fp);

extern char   *compile_date;
extern char   *compile_time;

#include <X11/Xresource.h>     /* "/usr/include/X11/Xresource.h" */


#define OPTION_COUNT 7


/***************************************************************
*  
*  The following is the Xlib description of a parm list.
*  <X11/Xresource.h> must be included, but only in the main.
*  
***************************************************************/

XrmOptionDescRec cmd_options[OPTION_COUNT] = {
/* option             resource name                   type                  used only if type is XrmoptionNoArg */

{"-p",              ".internalp",                   XrmoptionSepArg,        (caddr_t) NULL},    /*  0   */
{"-display",        ".internaldpy",                 XrmoptionSepArg,        (caddr_t) NULL},    /*  1   */
{"-w",              ".internalwin",                 XrmoptionSepArg,        (caddr_t) NULL},    /*  2   */
{"-q",              ".internalq",                   XrmoptionNoArg,         (caddr_t) "yes"},   /*  3   */
{"-list",           ".internallst",                 XrmoptionNoArg,         (caddr_t) "yes"},   /*  4   */
{"-version",        ".internalver",                 XrmoptionNoArg,         (caddr_t) "none"},  /*  5   */
{"-cmd",            ".internalcmd",                 XrmoptionNoArg,         (caddr_t) "yes"},   /*  6   */
};

char  *defaults[OPTION_COUNT] = {
             NULL,            /* 0  default for -p        */
             NULL,            /* 1  default for display   */
             NULL,            /* 2  default for -w window */
             NULL,            /* 3  default for -q        */
             NULL,            /* 4  default for -list     */
             NULL,            /* 5  default for -ver      */
             NULL,            /* 6  default for -cmd      */
             };      

char                *option_values[OPTION_COUNT];

#define P_IDX           0
#define DISPLAY_IDX     1
#define WINDOW_IDX      2
#define Q_IDX           3
#define LIST_IDX        4
#define VER_IDX         5
#define CMD_IDX         6

char                *cmdname;

#define USAGE "xdmc [-p <pid>] [-q] [-w <windowid>] [-display <node:0.0>] [-list [-cmd]] [-ver]  \"dm;cmd;list\"\n"

/************************************************************************

NAME:      xdmc (main) -   xdmc program


PURPOSE:    This routine will pass dm commands to a specified ce or ceterm session.

PARAMETERS:

   1.  -p <num>    - process id 
                     The -p option allows the specification of the process number
                     of the shell or ceterm to send the dm commands to.
                     Default: the ceterm associated with the current shell executing xdmc.

   2.  -display <name:0.0> -  name of display to open.
                     This is the name of the display to open in normal X format.
                     Default:  The $DISPLAY variable.

   3.  -w <window> - window specification.
                     The -w option allows the specification of the actual hex window id
                     as dumped by the -list option to send the data to.
                     <window> may have the 0x at the front, for example 0x3000002 and 3000002 are
                     the same.

   4.  -q          - quiet
                     The -q option specifies that failures are quiet.

   5.  -l          - list
                     The -list option specifies that the cclist is to be dumped to stdout.


FUNCTIONS :

   1.   Parse the parameters and open the display.

   2.   If -list was specified, process it.

   3.   Get the passed process id or window id.

   4.   Call the cc_xdmc routine (in cc.c) to process the request.

EXIT CODE:
   0   -  success
  !0   -  failure

*************************************************************************/

int main (int argc, char *argv[])
{

Display              *display = NULL;
int                   i;
int                   len;
int                   lcl_dash_q = 0;
int                   passed_pid = 0;
char                  buff[512];
Window                window;
char                 *p;
/* junk variables needed in call to XQueryPointer */
Window                root_window;
int                   root_x;
int                   root_y;
int                   target_x;
int                   target_y;
unsigned int          mask_return;
char                  options_from_parm[OPTION_COUNT];

/***************************************************************
*  
*  Scan the options list for the -display parameter.  If it was
*  not specified, do a quick check to see if DISPLAY is null
*  or not defined.  If so, exit quickly.  This is useful when
*  xdmc is used to test if this is a ceterm.
*  
***************************************************************/

for (i = 1; i < argc; i++)
{
   len = strlen(argv[i]);
   if ((len <= 8) && (strncmp(argv[i], "-display", len) == 0))
      break;
   /* Don't care if -display was before -q, if -display is found, -q is not needed */
   if ((argv[i][0] == '-') && (argv[i][1] == 'q'))
      lcl_dash_q = 1;
}
if (i == argc) /* -display not found in argv */
   {
      p = getenv("DISPLAY");
      if ((p == NULL) || (*p == '\0'))
         {
            if (!lcl_dash_q)
               fprintf(stderr, "Cannot open display, DISPLAY environment variable not set\n");
            return(1); /* will not be able to open display */
         }
   }



/****************************************************************
*
*   Get the options from the parm string.
*
****************************************************************/

#ifdef DebuG
Debug("CEDEBUG");
#endif

getxopts(cmd_options, OPTION_COUNT, defaults, &argc, argv, option_values, options_from_parm, &display, DISPLAY_IDX, -1, NULL);

if (option_values[VER_IDX])
   {
      fprintf(stderr, PACKAGE_STRING, VERSION);
      fprintf(stderr, ",  Compiled: %s %s\n", compile_date, compile_time);
      return(0);
   }

/****************************************************************
*
*   getxopts opens the display, make sure it is indeed open.
*
****************************************************************/

 if (display == NULL)
    {
       DEBUG9(XERRORPOS)
       fprintf(stderr, "%s: cannot connect to X server %s (%s)\n",
               argv[0], XDisplayName(option_values[DISPLAY_IDX]), strerror(errno));
       return(1);
    }

XSetErrorHandler(ce_xerror);


DEBUG9(XERRORPOS)
DEBUG9( (void)XSynchronize(display, True);)

/***************************************************************
*  
*  If the list option was specified, do this.  -list overrides
*  all else.
*  
***************************************************************/

if (option_values[LIST_IDX])
   {
      ccdump(display, (option_values[CMD_IDX] != NULL));
      return(0);
   }
else
   if (option_values[CMD_IDX])
      fprintf(stderr, "-cmd ignored without -list being specified\n");

/***************************************************************
*  
*  Make sure there is only one arg left.
*  
***************************************************************/

if (argc > 2)
   {
      fprintf(stderr, "Too many arguments\n");
      fprintf(stderr, USAGE);
      return(1);
   }
else
   if (argc < 2)
      {
         fprintf(stderr, "Missing required argument\n");
         fprintf(stderr, USAGE);
         return(1);
      }
 

if (option_values[P_IDX])
   {
      if (sscanf(option_values[P_IDX], "%d%s", &passed_pid, buff) == 1)  /* buff is a scrap variable here */
         {
            if (passed_pid < 0)
               {
                  fprintf(stderr, "%s: Warning: Invalid -p value %d\n", argv[0], passed_pid);
                  return(1);
               }
         }
      else
         {
            fprintf(stderr, "%s: Warning: invalid number for -p pid: %s\n", argv[0], option_values[P_IDX]);
            return(1);
         }
   }

if (option_values[WINDOW_IDX])
   {
      if (strcmp(option_values[WINDOW_IDX], "any") == 0)
         window = ANY_WINDOW;
      else
         if (strcmp(option_values[WINDOW_IDX], ".") == 0)
            {
               XQueryPointer(display,
                             RootWindow(display, DefaultScreen(display)),
                             &root_window, &window,
                             &root_x, &root_y, &target_x, &target_y, &mask_return);
            }
         else
            {
               p = option_values[WINDOW_IDX];
               if ((*p == '0') && ((*(p+1) | 0x20) == 'x')) /* look for 0x or 0X and skip */
                        p += 2;
               if (sscanf(p, "%lX%s", &window, buff) == 1)  /* buff is a scrap variable here */
                  {
                     if ((int)window < 0)
                        {
                           fprintf(stderr, "%s: Warning: Invalid -w value 0x%X\n", argv[0], window);
                           return(1);
                        }
                  }
               else
                  {
                     fprintf(stderr, "%s: Warning: invalid number for -w window: %s\n", argv[0], option_values[WINDOW_IDX]);
                     return(1);
                  }
            }
   }
else
   window = None;

i = cc_xdmc(display, passed_pid, argv[1], &window, (option_values[Q_IDX] != NULL));

return(i);

}  /* end main   */

/************************************************************************

NAME:      ce_xerror

PURPOSE:    This routine is identified by a call to XSetErrorHandler
            to be the error handler for protocol requests.
            It currently just calls the message printer and continues.

PARAMETERS:

   1.  display    - pointer to Display (INPUT)
                    This is the current display parm needed in X calls.

   2.  err_event  - pointer to XErrorEvent (INPUT)
                    This is the Xevent structure for the error event 
                    which caused this routine to get called.

FUNCTIONS :

   1.   Set the badwindow flag if this is a badwindow error.

   2.   Print the message relating to the text so long as the message has not been suppressed.
 
   3.   Save the errno from the X error globally.



*************************************************************************/

int  ce_xerror(Display      *display,
               XErrorEvent  *err_event)
{

DEBUG(fprintf(stderr, "@ce_xerror:  error code = %d\n", err_event->error_code);)

if (err_event->error_code == BadWindow)
   {
      ce_error_bad_window_flag = True;
      ce_error_bad_window_window = err_event->resourceid;
   }
else
   ce_error_bad_window_flag = False;

ce_error_errno = err_event->error_code;

if (ce_error_generate_message)
   {
      _XPrintDefaultError(display, err_event, stderr);
#ifdef DebuG
      debug |= D_BIT9;
#endif
   }
else
   DEBUG9(fprintf(stderr, "  From line %d in file %s\n", Xcall_lineno, (Xcall_file ? Xcall_file : ""));)

return(0);

} /* end of ce_xerror */





/*
 *	NAME
 *		_XPrintDefaultError
 *
 *	SYNOPSIS
 */
/************************************************************************

NAME:      _XPrintDefaultError

PURPOSE:    This routine is formats the data from an X error event and
            outputs the data to the passed file. 

PARAMETERS:

   1.  display    - pointer to Display (INPUT)
                    This is the current display parm needed in X calls.

   2.  err_event  - pointer to XErrorEvent (INPUT)
                    This is the Xevent structure for the error event 
                    which caused this routine to get called.

   3.  fp         - pointer to FILE (INPUT)
                    This is the stream to write to. 

FUNCTIONS :

   1.   Print the message relating to the text.

NOTES:
   This routine paraphased from the Xlib source


*************************************************************************/

static int _XPrintDefaultError (Display     *dpy,
                                XErrorEvent *event,
                                FILE        *fp)
{
char                    buffer[BUFSIZ];
char                    mesg[BUFSIZ];
char                    number[32];
char                   *mtype = "XlibMessage";

DEBUG9(fprintf(fp, "  From line %d in file %s\n", Xcall_lineno, (Xcall_file ? Xcall_file : ""));)

XGetErrorText(dpy, event->error_code, buffer, BUFSIZ);
XGetErrorDatabaseText(dpy, mtype, "Ce", "X Error", mesg, BUFSIZ);
#ifdef WIN32
fprintf(fp, "%s:  %s\n  ", mesg, buffer);
#else
fprintf(fp, "%s:  %s (pid %d)\n  ", mesg, buffer, getpid());
#endif

XGetErrorDatabaseText(dpy, mtype, "MajorCode", "Request Major code %d", mesg, BUFSIZ);
fprintf(fp, mesg, event->request_code);

sprintf(number, "%d", event->request_code);
XGetErrorDatabaseText(dpy, "XRequest", number, "", buffer, BUFSIZ);

fprintf(fp, " (%s)\n  ", buffer);
if (event->request_code >= 128)
   {
      XGetErrorDatabaseText(dpy, mtype, "MinorCode", "Request Minor code %d", mesg, BUFSIZ);
      fprintf(fp, mesg, event->minor_code);
      fputs("\n  ", fp);
   }

if ((event->error_code == BadWindow) ||
    (event->error_code == BadPixmap) ||
    (event->error_code == BadCursor) ||
    (event->error_code == BadFont) ||
    (event->error_code == BadDrawable) ||
    (event->error_code == BadColor) ||
    (event->error_code == BadGC) ||
    (event->error_code == BadIDChoice) ||
    (event->error_code == BadValue) ||
    (event->error_code == BadAtom))
   {
      if (event->error_code == BadValue)
         XGetErrorDatabaseText(dpy, mtype, "Value", "Value 0x%X", mesg, BUFSIZ);
      else
         if (event->error_code == BadAtom)
            XGetErrorDatabaseText(dpy, mtype, "AtomID", "AtomID 0x%X", mesg, BUFSIZ);
         else
            XGetErrorDatabaseText(dpy, mtype, "ResourceID", "ResourceID 0x%X", mesg, BUFSIZ);
      fprintf(fp, mesg, event->resourceid);
      fputs("\n  ", fp);
   }

XGetErrorDatabaseText(dpy, mtype, "ErrorSerial", "Error Serial #%d", mesg, BUFSIZ);
fprintf(fp, mesg, event->serial);
fputs("\n  ", fp);

XGetErrorDatabaseText(dpy, mtype, "CurrentSerial", "Current Serial #%d", mesg, BUFSIZ);
fprintf(fp, mesg, NextRequest(dpy)-1);
fputs("\n", fp);

if (event->error_code == BadImplementation)
   return(0);
else
   return(1);

} /* end of _XPrintDefaultError */



/***************************************************************
*  
*  Subsituted copies of dm_error and dm_error_dspl for
*  use in xdmc.  This way a separate ifdef'ed object does
*  not have to be created for a module in xdmc which only
*  uses dm_error.
*  
***************************************************************/

/* ARGSUSED1 */
void  dm_error_dspl(char             *text,         /* input  */
                    int               level,        /* input  */
                    DISPLAY_DESCR    *dspl_descr)   /* input  */
{
   if (text)
      fprintf(stderr, "%s\n", text);

} /* end of fake dm_error_dspl */


/* ARGSUSED1 */
void  dm_error(char             *text,         /* input  */
               int               level)        /* input  */
{
   if (text)
      fprintf(stderr, "%s\n", text);

} /* end of dm_error  */


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
*  Email:  stymar@cox.net
*  
***************************************************************/

/***************************************************************
*
*  xerror.c
*
*  Routines:
*         ce_xerror           - Handle X protocol errors
*         ce_xioerror         - Handle fatal system errors detected by X
*         create_crash_file   - Create a .CRA file during failure.
*         catch_quit          - quit signal handler
*
*  Internal:
*         _XPrintDefaultError - Dig up X error information and print it.
*
***************************************************************/


#include <stdio.h>          /* /usr/include/stdio.h     */
#include <string.h>         /* /usr/include/string.h    */
#include <time.h>           /* /usr/include/time.h      */
#include <errno.h>          /* /usr/include/errno.h     */
#include <stdlib.h>
#ifdef WIN32
#include <direct.h>
#include <io.h>
#include <process.h>

#define getuid() -1
#define getpid _getpid
#else
#include <sys/param.h>      /* /usr/include/sys/param.h */
#endif

#ifndef MAXPATHLEN
#ifdef WIN32
#define MAXPATHLEN	256
#else
#define MAXPATHLEN	1024
#endif
#endif

#ifndef WIN32
#include <X11/X.h>
#include <X11/Xlib.h>
#endif

#include "cc.h"
#include "debug.h"
#include "display.h"
#ifdef PAD
#include "dmc.h"
#include "dmsyms.h"
#endif
#include "dmwin.h"
#include "getevent.h"
#include "memdata.h"
#include "normalize.h"
#ifdef PAD
#include "pad.h"
#endif
#include "parms.h"
#include "pw.h"
#include "windowdefs.h"
#include "xerror.h"
#include "xerrorpos.h"

#include <signal.h>

#ifndef  BUFSIZ
#define BUFSIZ 1024
#endif

#ifndef WIN32
char   *getenv(const char *name);
#endif

#ifdef WIN32
int symlink(char *link_text, char *new_path_name){ return(0);}
#else
#ifndef linux
int symlink(char *link_text, char *new_path_name);
#endif
#endif


static int _XPrintDefaultError (Display     *dpy,
                                XErrorEvent *event,
                                FILE        *fp);


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
FILE  *stream;

DEBUG(fprintf(stderr, "@ce_xerror:  error code = %d\n", err_event->error_code);)

/***************************************************************
*  If this is the unixcmd window and the error says this is
*  no longer a window.  Don't output a message.  There is sometimes
*  events which come in after the window has been destroyed.
*  ce_error_unixcmd_window is global data in xerror.h which is
*  set in crpad.c
***************************************************************/
if (ce_error_unixcmd_window == err_event->resourceid)
   {
      if ((err_event->error_code == BadWindow) ||
          (err_event->error_code == BadDrawable) ||
          (err_event->error_code == BadPixmap))
         return(0);
   }

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
      (void)XSynchronize(display, True);
#endif
   }
else
   DEBUG9(fprintf(stderr, "  From line %d in file %s\n", Xcall_lineno, (Xcall_file ? Xcall_file : ""));)

stream = fopen(ERROR_LOG, "a");
if (stream != NULL)
   {
      fprintf(stream, "%s XIOERROR PID(%d) UID(%d) in file: %s\n", CURRENT_TIME, getpid(), getuid(), edit_file); 
      _XPrintDefaultError(display, err_event, stream);
      fclose(stream);
   }

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
fprintf(fp, "%s:  %s (pid %d)\n  ", mesg, buffer, getpid());

XGetErrorDatabaseText(dpy, mtype, "MajorCode", "Request Major code %d", mesg, BUFSIZ);
fprintf(fp, mesg, event->request_code);

snprintf(number, sizeof(number), "%d", event->request_code);
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
   return 0;
else
   return 1;

} /* end of _XPrintDefaultError */



/************************************************************************

NAME:      ce_xioerror

PURPOSE:    This routine is identified by a call to XSetXIOErrorHandler
            to be the error handler for fatal server errors.

PARAMETERS:

   1.  display    - pointer to Display (INPUT)
                    This is the current display parm needed in X calls.

GLOBAL DATA:
   errno          - Errno is set by the system before this is called.

   windodefs      - The window definitions are accessed globally


FUNCTIONS :

   1.   If this is a server shutdown or a server kill, attempt to save the
        file if we can.  Print a message and quit.

   2.   If this is some other sort of error, or the save fails, create a
        crash file.

   3.   Exit code errno.



*************************************************************************/

int  ce_xioerror(Display      *display)
{

int   lcl_errno = errno;
int   rc;
FILE *stream;
char  msg[1024];
DISPLAY_DESCR        *walk_dspl;
DISPLAY_DESCR        *hold_dspl;

DEBUG9(fprintf(stderr, "ce_xioerror:   From line %d in file %s, passed display 0x%X, current display 0x%X\n", Xcall_lineno, (Xcall_file ? Xcall_file : ""), display, dspl_descr->display);)

/***************************************************************
*  If there is more than one display open, or the display passed
*  is not the current display, we are not dead yet.
***************************************************************/

#ifdef PAD
      if ((dspl_descr->display == display) && dspl_descr->pad_mode)
         {
#ifndef WIN32
            if (MAKE_UTMP)
               kill_utmp();
            kill_shell(SIGTERM);
            kill_shell(SIGKILL);
#else
            kill_shell(0);
#endif
         }
#endif

if (lcl_errno == EPIPE)
   {
      DEBUG(fprintf(stderr, "Dirty bit is %s\n", (dirty_bit(dspl_descr->main_pad->token) ? "True" : "False"));)
#ifdef PAD
      if (!dspl_descr->pad_mode && dirty_bit(dspl_descr->main_pad->token))
#else
      if (dirty_bit(dspl_descr->main_pad->token))
#endif
         {
            rc = dm_pw(dspl_descr, edit_file, True);
            if (rc != 0)
               create_crash_file();
         }
      snprintf(msg, sizeof(msg), "%s PID(%d) UID(%d) in file: %s\n      X connection to %s broken (explicit kill or server shutdown).\n",
              CURRENT_TIME, getpid(), getuid(), edit_file,
              DisplayString(display), getpid());
   }
else
   {
      snprintf(msg, sizeof(msg), "%s PID(%d) UID(%d) in file: %s\n      Ce:  fatal IO error %d (%s) on X server \"%s\"\n      after %lu requests (%lu known processed) with %d events remaining.\n",
              CURRENT_TIME, getpid(), getuid(), edit_file,
              lcl_errno, strerror(lcl_errno), DisplayString(display),
              NextRequest(display) - 1, LastKnownRequestProcessed(display), QLength(display));
      DEBUG(fprintf(stderr, "Dirty bit is %s\n", (dirty_bit(dspl_descr->main_pad->token) ? "True" : "False"));)
      if (dirty_bit(dspl_descr->main_pad->token))
         create_crash_file();

   }

#ifdef ECONNRESET
/***************************************************************
*  2/9/1999 RES.  Don't dump a message to the log file on
*  X server shutdown.  Too much clutter from logging off.
***************************************************************/
if ((lcl_errno  != ECONNRESET) && (lcl_errno != EPIPE))
   {
#endif
      fprintf(stderr, msg);
      stream = fopen(ERROR_LOG, "a");
      if (stream != NULL)
         {
            fprintf (stream, msg);
            fclose(stream);
         }
#ifdef ECONNRESET
   }
#endif

/***************************************************************
*  Exit if this is the last display description.
***************************************************************/
if ((dspl_descr->next == dspl_descr) && (dspl_descr->display == display))
   {
      DEBUG(fprintf(stderr, "Last Display Description, exiting program\n");)
      exit(lcl_errno);
   }
else
   {
      /***************************************************************
      *  Find the bad display description, and eliminate it.
      ***************************************************************/
      walk_dspl = dspl_descr;
      do
      {
         if (walk_dspl->display == display)
            break;
         walk_dspl = walk_dspl->next;
      } while(walk_dspl != dspl_descr);

      if (walk_dspl->display == display)
         {
            hold_dspl = walk_dspl->next;
            del_display(walk_dspl);
            set_global_dspl(hold_dspl);
         }
      else
         {
            DEBUG(fprintf(stderr, "Cannot find display description for display 0x%X, exiting\n", display);)
            exit(lcl_errno);
         }

      /***************************************************************
      *  Give the message to each of the remaining windows.
      ***************************************************************/
      walk_dspl = dspl_descr;
      do
      {
         /*hold_dspl = dspl_descr;      3/9/95 RES */
         /*set_global_dspl(walk_dspl);  3/9/95 RES */
         ce_XBell(walk_dspl, 100);
         dm_error_dspl(msg, DM_ERROR_MSG, walk_dspl);
         /*set_global_dspl(hold_dspl);  3/9/95 RES */
         walk_dspl = walk_dspl->next;
      } while(walk_dspl != dspl_descr);

      if (dspl_descr->next == dspl_descr)
          cc_ce = False;       /* turn off cc processing when only one window left. */
      DEBUG(fprintf(stderr, "Returning to Main Event Loop for other windows\n");)
      longjmp(setjmp_env, 1);  /* return to the main routine, at the top of the getevent loop */
   }

return(0); /* never reached */

} /* end of ce_xioerror */


/************************************************************************

NAME:      create_crash_file

PURPOSE:    This routine attempts to create crash file in the event of
            trouble.

PARAMETERS:

   none

GLOBAL DATA:
   edit_file      - The name of the file currently being edited.

   windodefs      - The window definitions are accessed globally


FUNCTIONS :

   1.   Add .CRA to the end of the edit file name and attempt to save the file.

   2.   If this fails, try to save the file in the users $HOME directory.

   3.   If this fails, create a directory called /tmp/Ce_CRASH and attempt
        to save the crash file in this directory.  

   4.   If either of the later two cases succeed, create a soft link from the
        first choice place to create the crash file to the place it was created.

RETURNED VALUE:

   rc   -   int
            0  -  crash file created
           !0  -  crash file creation failed.



*************************************************************************/

int create_crash_file(void)
{
int           rc;
char         *p;
static char   crash_file[MAXPATHLEN];      /* static to avoid using stack space */
static char   crash_file_link[MAXPATHLEN]; /* static to avoid using stack space */
FILE         *stream;

#ifdef PAD
/***************************************************************
*  
*  No crash files in pad mode.
*  
***************************************************************/

if (dspl_descr->pad_mode)
   return(0);
#endif

/***************************************************************
*  
*  Turn off backups
*  
*   BACKUP_TYPE is a macro in parms.h
*
***************************************************************/

BACKUP_TYPE = "n";
OPTION_VALUES[LOCKF_IDX] = "no"; /* don't worry about cleaning up at this point */

/***************************************************************
*  
*  First create a crash file in the current directory.  If that
*  does not work, try to create one in /tmp/Ce_CRASH.  We use a
*  directory so that reboot will not clean up the crash file.
*  
***************************************************************/

strcpy(crash_file, edit_file);
strcat(crash_file, ".CRA");

/* first the .CRA file in the current directory */
rc = dm_pw(dspl_descr, crash_file, True);
if (rc != 0)
   {
      strcpy(crash_file_link, crash_file);

      /* now the crash file in the home directory */
      p = strchr(edit_file, '/');
      if (p == NULL)
         p = edit_file;
      else
         p++;

      /* 2/9/99 RES, Reverseal, try /usr/tmp before trying $HOME as directory for crash file */
      strcpy(crash_file, "/usr/tmp/");
      strcat(crash_file, p);
      strcat(crash_file, ".CRA");
      rc = dm_pw(dspl_descr, crash_file, True);
      if (rc == 0)
         {
            chmod(crash_file, 0700);
            symlink(crash_file, crash_file_link);
         }
      else
         {
            get_home_dir(crash_file, NULL);
            if (crash_file[0] != '\0')
            strcat(crash_file, "/");
            strcat(crash_file, p);
            strcat(crash_file, ".CRA");
            rc = dm_pw(dspl_descr, crash_file, True);
            if (rc == 0)
               symlink(crash_file, crash_file_link);
            else
               {
                  strcpy(crash_file, "/tmp/Ce_CRASH");
#ifdef WIN32
                  _mkdir(crash_file);
#else
                  mkdir(crash_file, 0700);
#endif
                  chmod(crash_file, 0700);
                  strcat(crash_file, "/");
                  strcat(crash_file, p);
                  strcat(crash_file, ".CRA");
                  rc = dm_pw(dspl_descr, crash_file, True);
                  if (rc == 0)
                     symlink(crash_file, crash_file_link);
               }
         }
   }

stream = fopen(ERROR_LOG, "a");
if (stream != NULL)
   {
      if (rc == 0)
         fprintf(stream, "%s XIOERROR PID(%d) UID(%d) Crash_file created: %s\n", CURRENT_TIME, getpid(), getuid(), crash_file);
      else
         fprintf(stream, "%s XIOERROR PID(%d) UID(%d) Crash_file creation failed for file %s\n", CURRENT_TIME, getpid(), getuid(), edit_file);
      fclose(stream);
   }


return(rc);

} /* end of create_crash_file */



/************************************************************************

NAME:      catch_quit

PURPOSE:    This routine is the signal handler for SIGQUIT, SIGTERM, and SIGINT
            signals.  It attempts to create a crash file and quits.

PARAMETERS:

   1.             - This is the signal number supplied by the system.

GLOBAL DATA:
   edit_file      - The name of the file currently being edited.


FUNCTIONS :

   1.   Call create_crash_file to save what we can.

   2.   Print a message to stderr identifying the signal.

   3.   Exit with the signal number as the exit code.


*************************************************************************/

void catch_quit(int sig)
{
FILE                 *stream;
#ifndef WIN32
int                   temp_fd;
char                  path[256];
DMC                   pn_dmc;
char                 *tmp;
#endif

DEBUG(fprintf(stderr, "catch_signal:  Dirty bit is %s\n", (dirty_bit(dspl_descr->main_pad->token) ? "True" : "False"));)
#ifdef PAD
#ifndef WIN32
/***************************************************************
*  No crash files in pad mode unless we are hit with a sigquit.
***************************************************************/
if ((dspl_descr->pad_mode) && (sig == SIGQUIT))
   {
      /***************************************************************
      *  RES 03/28/2001, If you hit a ceterm with a SIGQUIT, we will
      *  Save the Transcript Pad in /tmp.
      ***************************************************************/
      tmp = getenv("TEMP");
      if ((!tmp) || (!(*tmp)))
         tmp = "/tmp";
      snprintf(path, sizeof(path), "%s/cetermpad.XXXXXX", tmp);  /* RES 9/9/2004 changed from mktemp */
      temp_fd = mkstemp(path);
      if (temp_fd >= 0)
         {
            close(temp_fd); /* RES 9/9/2004 added */
            memset((char *)&pn_dmc, 0, sizeof(DMC));
            pn_dmc.pn.cmd = DM_pn;
            pn_dmc.pn.dash_c = 'c';
            pn_dmc.pn.dash_r = 'r';
            pn_dmc.pn.path = path;
            dm_pn(&pn_dmc, edit_file, dspl_descr);
            dspl_descr->pad_mode = False;
            create_crash_file();
            dspl_descr->pad_mode = True;
         }
   }
else
   if (dirty_bit(dspl_descr->main_pad->token))
      create_crash_file();
#else
/* PAD ON and WIN32 ON */
if (!(dspl_descr->pad_mode) && (dirty_bit(dspl_descr->main_pad->token)))
   create_crash_file();
#endif /* else of ifndef WIN32 */
#else

if (dirty_bit(dspl_descr->main_pad->token))
   create_crash_file();
#endif

fprintf(stderr, "Signal %d received, quitting\n\n", sig);

signal(sig, SIG_DFL);

stream = fopen(ERROR_LOG, "a");
if (stream != NULL)
   {
      fprintf(stream, "%s Signal %d received PID(%d) UID(%d) in file: %s\n", CURRENT_TIME, sig, getpid(), getuid(), edit_file);
      fclose(stream);
   }

#ifndef WIN32
/* exit(sig);RES 4/11/95 add code to resignal */
kill(getpid(), sig); 
#else
/* no kill in WIN32 */
exit(sig);
#endif

}  /* end of catch_quit */


char  *ce_xerror_strerror(int xerrno)
{

static char   other_message[40];
static char   *error_str[] = {
        /*   0 */ "Success",
        /*   1 */ "BadRequest - bad request code",
        /*   2 */ "BadValue - int parameter out of range",
        /*   3 */ "BadWindow - parameter not a Window",
        /*   4 */ "BadPixmap - parameter not a Pixmap",
        /*   5 */ "BadAtom - parameter not an Atom",
        /*   6 */ "BadCursor - parameter not a Cursor",
        /*   7 */ "BadFont - parameter not a Font",
        /*   8 */ "BadMatch - parameter mismatch",
        /*   9 */ "BadDrawable - parameter not a Pixmap or Window",
        /*  10 */ "BadAccess - key/button already grabbed. |  attempt to free an illegal cmap entry. | attempt to store into a read-only color map entry. | attempt to modify the access control. | list from other than the local host.",
        /*  11 */ "BadAlloc - insufficient resources",
        /*  12 */ "BadColor - no such colormap",
        /*  13 */ "BadGC - parameter not a GC",
        /*  14 */ "BadIDChoice - choice not in range or already used",
        /*  15 */ "BadName - font or color name doesn't exist",
        /*  16 */ "BadLength - Request length incorrect",
        /*  17 */ "BadImplementation - server is defective"
        };

if (xerrno < 0 || xerrno > 17)
   {
      snprintf(other_message, sizeof(other_message), "Unknown X error code %d", xerrno);
      return(other_message);
   }
else
   return(error_str[xerrno]);

} /* end of ce_xerror_strerror */


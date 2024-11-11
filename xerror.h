#ifndef _XERROR_H_INCLUDED
#define _XERROR_H_INCLUDED

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
*  Routines in xerror.c
*
*         ce_xerror           - Handle X protocol errors
*         ce_xioerror         - Handle fatal system errors detected by X
*         create_crash_file   - Create a .CRA file during failure.
*         catch_quit          - quit signal handler
*
***************************************************************/

#include <setjmp.h>         /* "/bsd4.3/usr/include/setjmp.h" */

#ifdef WIN32
#ifdef WIN32_XNT
#include "xnt.h"
#else
#include "X11/Xlib.h"
#endif
#else
#include <X11/Xlib.h>
#endif

#ifdef _MAIN_
/***************************************************************
*
*  GLOBAL ERROR RECOVERY DATA
*  
*  ce_error_bad_window_flag   -
*         Special flag for flagging badwindow errors.  The routines
*         in cc.c use this flag to detect a badwindow condition when working
*         with other windows.  They clear the flag prior to making the
*         call which would generate the badwindow error.
*
*  ce_error_generate_message  -
*         This flag, which is normally true, specifies whether the
*         ce_xerror error handler should print a message to stderr.
*         Routines which expect errors, can temporarily set this
*         flag to false to avoid messages.
*  
*  ce_error_errno  -
*         The X error code from the error record is placed in this
*         variable.  Error codes are listed in X11/X.h.  The name
*         of the error code can be had from ce_xerror_strerror.
*
*  setjmp_env  -
*         This variable is used in the XIO error handler to return to
*         the top of the main event loop.  This is done in "cc"
*         mode when loosing the connection to the display is not fatal.
*
***************************************************************/

int             ce_error_bad_window_flag = 0;
Window          ce_error_bad_window_window = None;
int             ce_error_generate_message = True;
int             ce_error_errno = 0;
Window          ce_error_unixcmd_window = None;
jmp_buf         setjmp_env;

#else
extern int      ce_error_bad_window_flag;
extern Window   ce_error_bad_window_window;
extern int      ce_error_generate_message;
extern int      ce_error_errno;
extern Window   ce_error_unixcmd_window;
extern jmp_buf  setjmp_env;
#endif

int  ce_xerror(Display      *display,
               XErrorEvent  *err_event);

int create_crash_file(void);

int  ce_xioerror(Display      *display);


void catch_quit();

char  *ce_xerror_strerror(int xerrno);

/***************************************************************
*  
*  Definition for CURRENT_TIME
*  This macro may be substituted where you need a pointer to
*  char.  such as:
*  printf("%s\n", CURRENT_TIME);
*  The current time is what the pointer points to.  You can
*  change _t1_ and _t3_ to something else if needed.
*  the _t3_ manipulations take off the \n from the string
*  returned by asctime.
*  
***************************************************************/

#ifdef _MAIN_
time_t       _t1_;
char         *_t3_;
#else
extern time_t       _t1_;
extern char         *_t3_;
#endif
#define CURRENT_TIME    ((_t3_ = asctime(localtime((_t1_ = time(NULL), &_t1_)))),(_t3_[strlen(_t3_)-1]='\0'), _t3_ )

#endif


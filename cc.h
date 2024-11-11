#ifndef _CC_H_INCLUDED
#define _CC_H_INCLUDED

/* static char *cc_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

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
*  Routines in cc.c
*     cc_check_other_windows -   Check other windows to see if they are open on this file.
*     cc_set_pad_mode        -   Set the cc configuration to pad mode.
*     cc_register_file       -   Register a file with the X server.
*     cc_shutdown            -   Remove the cclist property from the WM window
*     dm_cc                  -   Perform the cc command
*     cc_get_update          -   Process an update sent via property change from another window
*     cc_plbn                -   Send a line as a result of a PutLine_By_Number
*     cc_ca                  -   Tranmit color impacts to other displays
*     cc_dlbn                -   Tell the other windows to delete a line  (Delete Line By Number)
*     cc_joinln              -   Flush cc windows prior to a join
*     cc_titlebar            -   Tell the other windows about a titlebar change
*     cc_line_chk            -   Check if a line to be processed is already in use
*     cc_typing_chk          -   Update typing on a CC screens
*     cc_padname             -   Tell the other windows about a pad_name command
*     cc_undo                -   Tell the other windows about an undo event.
*     cc_redo                -   Tell the other windows about a redo event.
*     cc_ttyinfo_send        -   Send the slave tty name in pad mode to the main process
*     cc_ttyinfo_receive     -   Receive the slave tty name from cc_ttyinfo_send and save in the CC_LIST
*     ce_gethostid           -   Get the Internet host id
*     cc_xdmc                -   Send a dm command to a window.
*     cc_debug_cclist_data   -   Print this windows cc_list info
*     ccdump                 -   Print the cc_list (called from xdmc.c)
*     cc_to_next             -   Move to the next window.
*     cc_icon                -   Flag the window iconned or not iconned
*     cc_is_iconned          -   Check if the window is iconned
*     cc_obscured            -   Mark a window obscurred or non-obscurred
*     cc_receive_cc_request  -   process a request to cc sent via property change from another window
*     send_api_stats         -   Send current stats to the ceapi
*
***************************************************************/

/***************************************************************
*  
*  Includes needed for types used in prototypes.
*  
***************************************************************/

#include "dmc.h"
#include "buffer.h"
#include "memdata.h"

#include <sys/stat.h>   /* /bsd4.3/usr/include/sys/stat.h */

#ifdef WIN32
#ifdef WIN32_XNT
#include "xnt.h"
#else
#include "X11/X.h"         /* /usr/include/X11/X.h */
#include "X11/Xatom.h"     /* /usr/include/X11/Xatom.h */
#endif
#else
#include <X11/X.h>         /* /usr/include/X11/X.h */
#include <X11/Xatom.h>     /* /usr/include/X11/Xatom.h */
#endif


/**************************************************************
*  
*  The following macro detects at run time whether this is a
*  big endian or little endian machine.
*  
***************************************************************/
#define IS_BIG_ENDIAN (*((short int *)"\0\01") == 1)


#ifdef _MAIN_
/**************************************************************
*  
*  Global variable to tell if there are other cc copies of the
*  file being edited.
*  
***************************************************************/

int        cc_ce = 0;

#else

extern int cc_ce;

#endif

/***************************************************************
*  
*  Prototypes for routines in cc.c
*  
***************************************************************/

int  cc_check_other_windows(struct stat   *file_stats,
                            DISPLAY_DESCR *dspl_descr,
                            int            writable);

void  cc_set_pad_mode(Display       *display);

int  cc_register_file(DISPLAY_DESCR *dspl_descr,
                      int            shell_pid,
                      char          *edit_file);

void cc_shutdown(DISPLAY_DESCR *dspl_descr);

int  cc_get_update(XEvent         *event,
                   DISPLAY_DESCR  *dspl_descr,           /* input  */
                   int            *warp_needed);         /* output */

void     cc_plbn(DATA_TOKEN *token,       /* opaque */
                 int         line_no,     /* input */
                 char       *line,        /* input */ 
                 int         flag);       /* input;   0=overwrite 1=insert after */

void     cc_ca(DISPLAY_DESCR   *dspl_descr,   /* input/output */
               int              top_line,     /* input        */
               int              bottom_line); /* input        */

void     cc_dlbn(DATA_TOKEN *token,       /* opaque */
                 int         line_no,     /* input */
                 int         count);      /* input;   (always 1) */

void     cc_joinln(DATA_TOKEN *token,       /* opaque */
                   int         line_no);    /* input */

void     cc_titlebar(void);

int      cc_line_chk(DATA_TOKEN    *token,       /* opaque */
                     DISPLAY_DESCR *dspl_descr,
                     int            line_no);

void     cc_typing_chk(DATA_TOKEN     *token,       /* opaque */
                       DISPLAY_DESCR  *dspl_descr,   /* input  */
                       int             line_no,      /* input */
                       char           *line);        /* input */ 

void     cc_padname(DISPLAY_DESCR *dspl_descr,    /* input */
                    char          *edit_file);    /* input */

#define cc_undo(a,b,c) {}
#define cc_redo(a,b,c) {}

#define  CC_TTYINFO_CHAR 254

void     cc_ttyinfo_receive( char       *slave_tty_name);   /* input */

void     cc_ttyinfo_send(int        slave_tty_fd);   /* input */

unsigned int  ce_gethostid(void);


/* special flag value for window parameter in cc_xdmc */

#define ANY_WINDOW (Window)-1

int      cc_xdmc(Display    *display,    /* input */
                 int         pid,        /* input */
                 char       *cmd_line,   /* input */
                 Window     *window,     /* input/output */
                 int         quiet);     /* input */

void cc_debug_cclist_data(Display       *display,
                          Window         wm_window);

void ccdump(Display *display,
            int      print_cmds);


void     cc_to_next(int         input_flag);

void     cc_icon(int         icon_flag);

int      cc_is_iconned(void);

void     cc_obscured(int         obscured);


int  cc_receive_cc_request(XEvent          *event,                  /* input */
                           DISPLAY_DESCR   *dspl_descr,             /* input / output  in child process */
                           char            *edit_file,              /* input  */
                           char            *resource_class,         /* input  */
                           char            *arg0);                  /* input  */


int   dm_cc(DMC             *dmc,                    /* input  */
            DISPLAY_DESCR   *dspl_descr,             /* input / output  in child process */
            char            *edit_file,              /* input  */
            char            *resource_class,         /* input  */
            char            *arg0);                  /* input  */

void send_api_stats(PAD_DESCR   *main_pad,
                    XEvent      *event);


#endif


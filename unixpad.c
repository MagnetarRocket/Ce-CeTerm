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
*  module unixpad.c
*
*  These routines do not fall into pad.c which handles shell
*  commumication, nor unixwin.c which is mostly drawing routines.
*
*  Routines:
*         send_to_shell         - send lines to the shell
*         dm_eef                - send end of file to the shell.
*         wait_for_input        - Wait on the Xserver and shell sockets
*         scroll_some           - scroll shell output onto the window
*         dm_dq                 - Send a quit signal to a shell
*         transpad_input        - WIN32 only, process -TRANSPAD or -STDIN input
* 
*  Internal:
*         scroll_a_line         - scroll the main pad one line.
*         setup_fdset           - Set up the fd_sets for the main select
*         process_dm_cmdline    - Process a command from the cmd file descriptor
*         transpad_fgets        - Extract 1 line from a buffer filled by read(2)
*         read_dm_cmd           - Read a command line from the altername command file.
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h         */
#include <string.h>         /* /usr/include/string.h        */
#include <errno.h>          /* /usr/include/errno.h         */
#include <fcntl.h>          /* /usr/include/fcntl.h         */
#include <sys/types.h>      /* usr/include/sys/types.h      */
#include <signal.h>         /* /usr/include/sys/signal.h    */
#if defined(linux)
#include <stdlib.h>
#endif
#ifdef WIN32
#include <io.h>
#include <winsock.h>
#include <time.h>
#else
#include <sys/time.h>       /* /usr/include/sys/time.h      */
#ifndef FD_ZERO
#include <sys/select.h>     /* /usr/include/sys/select.h    */
#endif
#endif /* WIN32 */

#include "debug.h"
#include "dmsyms.h"
#include "dmc.h"
#include "dmwin.h"
#include "getevent.h"
#include "init.h"
#include "pad.h"
#include "parms.h"
#include "pw.h"
#include "lineno.h"
#include "memdata.h"
#include "mvcursor.h"
#include "redraw.h"
#include "sendevnt.h"
#include "tab.h"
#include "timeout.h"
#include "txcursor.h"
#include "typing.h"
#include "undo.h"
#include "unixpad.h"
#include "unixwin.h"
#include "vt100.h"
#include "wc.h"
#include "window.h"
#include "windowdefs.h"
#include "xerror.h"
#include "xerrorpos.h"
#include "xutil.h"
#include "xsmp.h"


/***************************************************************
*  
*  Local prototypes
*  
***************************************************************/

static int read_dm_cmd(DISPLAY_DESCR   *dspl_descr,
                       int             *cmd_fd);

static void setup_fdset(DISPLAY_DESCR   *dspl_descr,
                        int               Shell_socket,
                        int               cmd_fd,
                        fd_set           *readfds,
                        fd_set           *writefds);

/***************************************************************
*  
*  Flag to show socket is full.
*  
***************************************************************/
static int        toshell_socket_full = False;


#ifdef PAD
static void  scroll_a_line(PAD_DESCR         *main_pad,
                           DRAWABLE_DESCR    *lineno_subarea);

/***************************************************************
*  
*  Special line inserted into pad for eof (eef command)
*  
***************************************************************/

static char       eof_marker[] = {EOF, 0};
#define MAX_SHELL_LINE 1023



/************************************************************************

NAME:      send_to_shell  - send lines to the shell

PURPOSE:    This routine takes care of taking lines which need to be
            sent to the shell.

PARAMETERS:

   1.   count     -  int (INPUT)
                     This is the number of lines to be sent.  The 
                     send always starts with line zero.

   2.  unixcmd_cur_descr -  pointer to DATA_TOKEN  (INPUT)
                     This is the pad description for the pad to have lines
                     converted and removed.  It is the one for the unix command
                     window.


FUNCTIONS:

   1.  Read each line to be sent from the memdata structure.

   2.  Try to send the line.

   3.  Return the number of lines sent.

RETURNED VALUE:

   lines_sent  -  int
                  This is a count of the number of lines successfully sent.
                  On an error writing to the shell, -1 is returned.

*************************************************************************/

int   send_to_shell(int           count,
                    PAD_DESCR    *unixcmd_cur_descr)
{
char             *next_cmd;
int               lines_sent = 0;
int               rc;
int               newline_flag;

DEBUG19(fprintf(stderr, "send_to_shell:  Want to send %d line(s)\n", count); )
toshell_socket_full = False;

/***************************************************************
*  If count is zero, flush anything in the pad buffer.
***************************************************************/
if (count == 0)
   {
      rc = pad2shell(NULL, True);
      if (rc == PAD_FAIL)
         return(-1);
      if (rc == PAD_PARTIAL)
         toshell_socket_full = True;
      return(0);
   }

flush(unixcmd_cur_descr);

while((lines_sent < count) && (total_lines(unixcmd_cur_descr->token) > 0))
{
   /***************************************************************
   *  Read line 0, if it does not exist, return what we have so far.
   ***************************************************************/
   next_cmd = get_line_by_num(unixcmd_cur_descr->token, 0);
   if (next_cmd == NULL)
      return(lines_sent);

   /***************************************************************
   *  The shell cannot handle lines longer than 1023, flag lines
   *  that are too long.  If allowed to pass, they mess up the shell.
   ***************************************************************/
   if ((int)strlen(next_cmd) > MAX_SHELL_LINE)
      {
         dm_error("Line too long to send to shell, max len is 1023", DM_ERROR_BEEP);
         return(lines_sent);
      }

   DEBUG4(fprintf(stderr, "send_to_shell:  line %2d \"%s\"\n", lines_sent, next_cmd); )

   /***************************************************************
   * See if this is a dq, where we do not add a newline.
   ***************************************************************/
   if ((next_cmd[0] == ce_tty_char[TTY_INT]) && (next_cmd[1] == '\0'))
      newline_flag = False;
   else
      newline_flag = True;

   /***************************************************************
   * Try to send the line.
   ***************************************************************/
   rc = pad2shell(next_cmd, newline_flag);
   if ((rc == PAD_SUCCESS) || (rc == PAD_PARTIAL))
      {
         /* RES 6/27/95 2 event do's added */
         /* RES 8/8/95 took event do's back out, they make undo put a newline inthe UNIX window */
         /*event_do(unixcmd_cur_descr->token, KS_EVENT, 0, 0, 0, 0);*/
         delete_line_by_num(unixcmd_cur_descr->token, 0, 1);  /* delete line zero */
         /*event_do(unixcmd_cur_descr->token, KS_EVENT, 0, 0, 0, 0);*/
         /* a new command being sent resets autohold to its base value */
         if (*unixcmd_cur_descr->autohold_mode)
            *unixcmd_cur_descr->autohold_mode = 1;
         lines_sent++; /* RES 3/2/94 Moved from lastline in while statement */
      }

   if (rc != PAD_SUCCESS)
      {
         if (rc == PAD_FAIL)
            return(-1);

         toshell_socket_full = True;
         break;   
      }

} /* while i < count */

return(lines_sent);

} /* end of send_to_shell  */


/************************************************************************

NAME:      dm_eef  - send eof to the unix command window.

PURPOSE:    This routine puts the eof marker into the unix command window.

PARAMETERS:

   1.  dmc            - pointer to DMC (INPUT)
                        This is the command structure for the eef command.
                        It contains the data from any dash options.
                        In particular the -a option which says what char to
                        send as an eof char.

   2.   cursor_buff -  pointer to BUFF_DESCR (INPUT)
                       This is window the cursor was in when eof was pressed.
                       If the cursor is not in the unixcmd window, nothing is done.


FUNCTIONS:

   1.  Put the eof marker at the end of the unix command window memdata structure.

   2.  Call send_to_shell to send the remaining commands to the shell unless
       we are in hold mode.


RETURNED VALUE:
   redraw_needed -  This routine causes a full redraw of the unix cmd window.


*************************************************************************/

int  dm_eef(DMC        *dmc,
            BUFF_DESCR *cursor_buff) 
{
int     redraw_needed = (MAIN_PAD_MASK & FULL_REDRAW);

DEBUG19(fprintf(stderr, "@dm_eef\n");)

/***************************************************************
*  Only accept eof in the Unix command window.
***************************************************************/

if (cursor_buff->which_window != UNIXCMD_WINDOW)
   return(0);

/***************************************************************
*  If the value to send with eof was supplied, use it.  This is
*  done by adjusting the tty_char file.  Otherwise, call the
*  routine to get the current values of the eof char.
***************************************************************/

if (dmc->eef.dash_a)
   ce_tty_char[TTY_EOF] = dmc->eef.dash_a;
else
   ce_tty_char[TTY_EOF] = 4;
   /*get_stty();  in pad.c RES 11/18/97, get_stty is undependable */

/***************************************************************
*  If the unix window pad is empty, insert the eof as the first line.
*  If the current unix window line is a null line, replace it.
*  Otherwise insert the eof marker.
***************************************************************/

if (total_lines(cursor_buff->current_win_buff->token) == 0)
   put_line_by_num(cursor_buff->current_win_buff->token,
                   -1,
                   eof_marker,
                   INSERT);
else
   if ((cursor_buff->current_win_buff->file_line_no >= total_lines(cursor_buff->current_win_buff->token)-1) && !(*cursor_buff->current_win_buff->buff_ptr) && !*cursor_buff->current_win_buff->hold_mode)
      put_line_by_num(cursor_buff->current_win_buff->token,
                      cursor_buff->current_win_buff->file_line_no,
                      eof_marker,
                      OVERWRITE);
   else
      {
         put_line_by_num(cursor_buff->current_win_buff->token,
                         cursor_buff->current_win_buff->file_line_no,
                         eof_marker,
                         INSERT);
         cursor_buff->current_win_buff->file_line_no++;
      }

/***************************************************************
*  If we are not in hold mode, send all the data to the shell.
*  Otherwise add a blank line after the eof and put the cursor
*  on it.
***************************************************************/

if (!*cursor_buff->current_win_buff->hold_mode)
   {
      cursor_buff->current_win_buff->file_line_no++;
      DEBUG22(fprintf(stderr, "dm_eef: Want to send %d lines to the shell\n", cursor_buff->current_win_buff->file_line_no);)
      cursor_buff->current_win_buff->file_line_no -= send_to_shell(cursor_buff->current_win_buff->file_line_no, cursor_buff->current_win_buff);
      DEBUG22(fprintf(stderr, "dm_eef: %d line(s) left to send\n", cursor_buff->current_win_buff->file_line_no);)

      if (total_lines(cursor_buff->current_win_buff->token) > 0)
         redraw_needed |= (UNIXCMD_MASK & FULL_REDRAW);
         
   }
else
   {
      /* add a blank line after the eof and put the cursor on it */
      put_line_by_num(cursor_buff->current_win_buff->token,
                      cursor_buff->current_win_buff->file_line_no,
                      "",
                      INSERT);
      cursor_buff->current_win_buff->file_line_no++;
      redraw_needed |= (UNIXCMD_MASK & FULL_REDRAW);
   }

cursor_buff->current_win_buff->buff_ptr = get_line_by_num(cursor_buff->current_win_buff->token, cursor_buff->current_win_buff->file_line_no);

return(redraw_needed);

} /* end of dm_eef */


/***************************************************************
  
NAME:      scroll_a_line         - shift the main pad up one line.


PURPOSE:    This routine scrolls the main pad up one line.

PARAMETERS:

   1.  main_pad -  pointer to DATA_TOKEN  (INPUT)
                     This is the pad description for the main scrollable window.
                     This is the window which is shifted up one.

   2.  lineno_subarea   - pointer to DRAWABLE_DESCR (INPUT)
                     This is the window subarea description for the lineno
                     text area of the main window.  If line numbers are
                     off, this parm is NULL.


FUNCTIONS :

   1.   Copy the pixmap up one line onto itself.

   2.   Shift the winlines data up one element in the array.

   3.   Draw the new line which was shifted onto the screen.

   4.   If line numbers are on, draw the new line number.

   5.   Copy the pixmap to the main window.

   6.   Flush to make sure the event is on it's way.


*************************************************************************/

static void  scroll_a_line(PAD_DESCR         *main_pad,
                           DRAWABLE_DESCR    *lineno_subarea)
{
char       *new_line;
int         new_win_line_no;
int         i;
int         expose_x;

/***************************************************************
*  Do nothing if the pixmap is down
***************************************************************/
if (main_pad->display_data->x_pixmap == None)
   return;

DEBUG9(XERRORPOS)
XCopyArea(main_pad->display_data->display, main_pad->display_data->x_pixmap, main_pad->display_data->x_pixmap,
          main_pad->window->gc,
          0, main_pad->window->sub_y + main_pad->window->line_height,
          main_pad->window->width, main_pad->window->sub_height - main_pad->window->line_height,
          0, main_pad->window->sub_y);

new_win_line_no = main_pad->window->lines_on_screen - 1;
new_line = get_line_by_num(main_pad->token, main_pad->first_line + new_win_line_no + 1);
if (main_pad->first_line == main_pad->file_line_no)
   {
      main_pad->first_line++;
      main_pad->file_line_no = main_pad->first_line;
      main_pad->buff_ptr = get_line_by_num(main_pad->token, main_pad->file_line_no);
   }
else
   main_pad->first_line++;

for (i = 0; i < new_win_line_no; i++)
   main_pad->win_lines[i] = main_pad->win_lines[i+1];

draw_partial_line(main_pad,
                  new_win_line_no,
                  0,
                  main_pad->first_line + new_win_line_no,
                  new_line,
                  False,      /* do_dots */
                  &expose_x); /* not used for anything in this case */


if (lineno_subarea)
   write_lineno(main_pad->display_data->display,
                main_pad->display_data->x_pixmap,
                lineno_subarea,
                main_pad->first_line,
                0,    
                new_win_line_no,  
                main_pad->token,
                main_pad->formfeed_in_window,
                main_pad->win_lines);

DEBUG9(XERRORPOS)
XCopyArea(main_pad->display_data->display, main_pad->display_data->x_pixmap, main_pad->x_window,
          main_pad->window->gc,
          0, main_pad->window->sub_y,
          main_pad->window->width, main_pad->window->sub_height,
          0, main_pad->window->sub_y);

XFlush(main_pad->display_data->display);


} /* end of scroll_a_line  */
#endif /* PAD */


/************************************************************************

NAME:      wait_for_input  - Wait on the Xserver and shell sockets

PURPOSE:    This routine is used in pad mode to watch for input from the
            Xserver and the attached shell at the same time.  It also is
            used when the socket to the shell is full.  The routine waits
            for X events and handles I/O events to the shell.  This isolates
            the rest of the program from the scrolling and stuff involved 
            in pad support.

PARAMETERS:

   1.   dspl_descr   - pointer to DISPLAY_DESCR (INPUT)
                       This is the first display description in the list.
                       of displays.  Except when a cc is in progress, 
                       there is only 1.  Otherwise, if input shows up on
                       multiple X server sockets, this one is chosen first.

   2.   Shell_socket - int (INPUT)
                       This is the file descriptor for the connection to the
                       shell.  If pad mode is 

   3.   cmd_fd       - pointer to int (INPUT)
                       This is a pointer to the to the file descriptor for the alternate
                       input file of DM commands.  If not used, the value is -1.  If
                       eof is detected on this file, the value is set to -1.

GLOBAL DATA:

   1.  toshell_socket_full -  int (INPUT)
                     This flag is maintained global within this module.  It is set
                     by send_to_shell to show that part of a line is waiting for
                     transmission.

FUNCTIONS:


RETURNED VALUE:
   returned_display  -  pointer to DISPLAY_DESCR
                        This routine returns when X server data is ready.
                        The display description for the connection the
                        data is ready on is returned.

*************************************************************************/

DISPLAY_DESCR *wait_for_input(DISPLAY_DESCR   *dspl_descr,
                              int               Shell_socket,
                              int              *cmd_fd)
{
fd_set                readfds;
fd_set                writefds;
struct timeval        time_out;
struct timeval       *time_ptr;
int                   nfound;
int                   nfds;
#ifndef WIN32
int                   lines_displayed;
int                   lcl_warp_needed;
int                   got_it_all;
int                   lines_sent;
#endif
int                   lines_copied = 0;
int                   lines_read_from_shell = 1;
int                   warp_needed = False;
int                   redraw_needed;
int                   done = False;
char                  msg[256];
DISPLAY_DESCR        *returned_display = dspl_descr;
DISPLAY_DESCR        *padmode_dspl = dspl_descr;
DISPLAY_DESCR        *walk_dspl;

msg[0] = '\0';

/***************************************************************
*  
*  Set up for a select on the X server socket, the socket to the
*  shell, and maybe to the socket for the cmd input file.
*  
***************************************************************/

nfds = MAX(MAX(ConnectionNumber(dspl_descr->display), Shell_socket), *cmd_fd) + 1;
for (walk_dspl = dspl_descr->next; walk_dspl != dspl_descr; walk_dspl = walk_dspl->next)
{
   nfds = MAX(ConnectionNumber(walk_dspl->display)+1, nfds);
   if (walk_dspl->pad_mode)
      {
         padmode_dspl = walk_dspl;
         DEBUG22(fprintf(stderr, "Pad mode display set to is display %d\n", padmode_dspl->display_no);)
      }
}
DEBUG22(
   if (dspl_descr->next != dspl_descr)
      fprintf(stderr, "Pad mode display is %sdisplay %d\n", (padmode_dspl->pad_mode ? "" : "NON_PAD MODE DISPLAY"), padmode_dspl->display_no);
)

while(!done)
{
   /***************************************************************
   *  
   *  First do a check to see in there is X input.  If so, ignore
   *  everything else.
   *  
   ***************************************************************/

   /***************************************************************
   *  5/13/96 RES License server code moved to timeout.c
   *  add timeout_set routine to deal with timeouts.
   *  We don't need the timeout for the first select, but we set
   *  it up now in case it queue's any events via ce_XSendEvent.
   ***************************************************************/
   time_ptr = timeout_set(dspl_descr);
   /* Added 3/1/94  RES */
   if (queued_event_ready())
      {
         DEBUG22(fprintf(stderr, "X Data is ready on queued list\n");)
         break;
      }

   setup_fdset(dspl_descr, -1, -1, &readfds, NULL);
   time_out.tv_sec = 0;
   time_out.tv_usec = 25000;
   DEBUG22(fprintf(stderr, "     Waiting on X server(s) for %d micro seconds\n", time_out.tv_usec);)
#ifdef  HAVE_X11_SM_SMLIB_H
   if (dspl_descr->xsmp_active)
      nfds = MAX(nfds,(xsmp_fdset(dspl_descr->xsmp_private_data, &readfds)+1));  /* in xsmp.c */
#endif

   nfound = select(nfds, HT &readfds, NULL, NULL, &time_out);

#ifdef  HAVE_X11_SM_SMLIB_H
   /**************************************************************
   *  If XSMP (X Session Manager) has something for us, do it first.
   *  nfound is checked to make sure we did not get out on a timeout.
   *  xsmp_fdisset takes care of the ICE callbacks.
   **************************************************************/
   if ((dspl_descr->xsmp_active) && (nfound > 0))
      xsmp_fdisset(dspl_descr->xsmp_private_data, &readfds); /* in xsmp.c */
#endif

   if (nfound != 0)
      {
         DEBUG22(if (nfound < 0) fprintf(stderr, "ERROR on Xserver socket (%s)(#0)\n", strerror(errno));)
         if (!FD_ISSET(ConnectionNumber(dspl_descr->display), &readfds))
            for (walk_dspl = dspl_descr->next; walk_dspl != dspl_descr; walk_dspl = walk_dspl->next)
               if (FD_ISSET(ConnectionNumber(walk_dspl->display), &readfds))
                  {
                     returned_display = walk_dspl;
                     DEBUG22(fprintf(stderr, "(A) Data ready on Xserver socket (#%d)\n", returned_display->display_no);)
                     break; /* break out of this little for loop */
                  }
         break; /* out of while not done loop */
      }
   else
      {
         DEBUG22(fprintf(stderr, "Select on Xserver only socket timed out\n");)
      }

   /**************************************************************
   *  Wait for I/O to be ready on any socket.
   **************************************************************/
   XFlush(dspl_descr->display);
   /***************************************************************
   *  Set up the parms for the select call.  5/17/94, broken out
   *  We are always interested in reading on either file descriptor.
   *  Sometimes we are interested in writing to the shell.
   *  The test on lines_read_from_shell deals with the following
   *  case:  When eof is sent on the shell, the socket is closed
   *  and the pad_mode flag gets cleared asynchronously.  Future selects
   *  get satisfied by this socket with zero bytes to read.  Thus
   *  we do not want to look at this socket any more.
   ***************************************************************/
   setup_fdset(dspl_descr,
               (lines_read_from_shell ? Shell_socket : -1),
               *cmd_fd,
               &readfds, &writefds);
#ifdef  HAVE_X11_SM_SMLIB_H
   if (dspl_descr->xsmp_active)
      nfds = MAX(nfds,(xsmp_fdset(dspl_descr->xsmp_private_data, &readfds)+1));  /* in xsmp.c */
#endif


   DEBUG22(
      if (time_ptr)
         fprintf(stderr, "Select timeout %d seconds, %d microseconds\n",  time_ptr->tv_sec, time_ptr->tv_usec);
   )
   while((nfound = select(nfds, HT &readfds, HT &writefds, NULL, time_ptr)) <= 0)
   {
      DEBUG22(
         if (nfound == 0)
            fprintf(stderr, "wait_for_input: select timed out, retrying\n");
         else
            fprintf(stderr, "wait_for_input: select interupted(%d), retrying (%s)\n", nfound, strerror(errno));
      )
      setup_fdset(dspl_descr, Shell_socket, *cmd_fd, &readfds, &writefds);
#ifdef  HAVE_X11_SM_SMLIB_H
      if (dspl_descr->xsmp_active)
         nfds = MAX(nfds,(xsmp_fdset(dspl_descr->xsmp_private_data, &readfds)+1));  /* in xsmp.c */
#endif
      time_ptr = timeout_set(dspl_descr);
      if (queued_event_ready()) /* added for use with kill_unixcmd_window RES 9/7/95 */
         {
            DEBUG22(fprintf(stderr, "X Data is ready on queued list(a)\n");)
            break;
         }
      DEBUG22(
         if (time_ptr)
            fprintf(stderr, "Select timeout %d seconds, %d microseconds\n",  time_ptr->tv_sec, time_ptr->tv_usec);
      )
   } /* while select is not satisfied */

#ifdef  HAVE_X11_SM_SMLIB_H
   /**************************************************************
   *  If XSMP (X Session Manager) has something for us, do it first.
   *  nfound is checked to make sure we did not get out on a timeout.
   *  xsmp_fdisset takes care of the ICE callbacks.
   **************************************************************/
   if ((dspl_descr->xsmp_active) && (nfound > 0))
      xsmp_fdisset(dspl_descr->xsmp_private_data, &readfds); /* in xsmp.c */
#endif

   /**************************************************************
   *  If queued data showed up. process it.
   **************************************************************/
   if (queued_event_ready()) /* added for use with kill_unixcmd_window RES 9/7/95 */
      {
         DEBUG22(fprintf(stderr, "X Data is ready on queued list(b)\n");)
         break;
      }

   /**************************************************************
   *  cmd fd data, read it and return.  Routine creates a fake keypress event.
   **************************************************************/
   if ((*cmd_fd != -1) && FD_ISSET(*cmd_fd, &readfds))
      {
         done = read_dm_cmd(dspl_descr, cmd_fd);
         DEBUG19(fprintf(stderr, "Data ready on Cmd file descriptor (%s)\n", (CMDS_FROM_ARG ? CMDS_FROM_ARG : ""));)
      }

   /**************************************************************
   *  If there is X data, we are done, return   res 9/24/93
   **************************************************************/
   if (FD_ISSET(ConnectionNumber(dspl_descr->display), &readfds))
      {
         done = True;
         DEBUG22(fprintf(stderr, "(B) Data ready on Xserver socket (#%d)\n", dspl_descr->display_no);)
      }
   else
      for (walk_dspl = dspl_descr->next; walk_dspl != dspl_descr; walk_dspl = walk_dspl->next)
         if (FD_ISSET(ConnectionNumber(walk_dspl->display), &readfds))
            {
               DEBUG22(fprintf(stderr, "(C) Data ready on Xserver socket (#%d)\n", walk_dspl->display_no);)
               returned_display = walk_dspl;
               done = True;
               break; /* break out of this little for loop */
            }

#ifdef PAD
#ifndef WIN32
   /**************************************************************
   *  If there is shell data, read it in and start displaying it. 
   **************************************************************/
   if (!done && (Shell_socket != -1) && FD_ISSET(Shell_socket, &readfds))
      {
         DEBUG19(fprintf(stderr, "wait_for_input: data ready from the shell\n");)
         /***************************************************************
         *  Get data on what the screen looks like, we will need it
         *  if output is written to the screen.
         ***************************************************************/
         lines_displayed = total_lines(padmode_dspl->main_pad->token) - padmode_dspl->main_pad->first_line;
         if (lines_displayed > padmode_dspl->main_pad->window->lines_on_screen)
            lines_displayed = padmode_dspl->main_pad->window->lines_on_screen;

         set_global_dspl(padmode_dspl); /* switch to make the pad mode display the current global one. */
         /* dspl_descr = padmode_dspl;*/
         lines_read_from_shell = shell2pad(padmode_dspl, &lcl_warp_needed, False);
         if (padmode_dspl->vt100_mode || padmode_dspl->hold_mode)
            {
               /* added redraw to get first page output in hold mode.  RES 9/27/95 */
               if ((!padmode_dspl->vt100_mode) &&
                   (padmode_dspl->main_pad->first_line+padmode_dspl->main_pad->window->lines_on_screen >= total_lines(padmode_dspl->main_pad->token)))
                  process_redraw(padmode_dspl, (padmode_dspl->main_pad->redraw_mask & FULL_REDRAW), False);
               break;
            }

         if (lines_read_from_shell < 0) /* error on read from shell */
            {
               lines_read_from_shell = 0;
               done = True; /* go back to process dead shell   RES 2/11/94 */
            }
         lines_copied = lines_read_from_shell + get_background_work(BACKGROUND_SCROLL);
         DEBUG19(fprintf(stderr, "shell2pad put %d lines in the pad, prompt was %schanged, cursor in %s window\n", lines_read_from_shell, (lcl_warp_needed ? "" : "not "), which_window_names[padmode_dspl->cursor_buff->which_window]);)
          /* return of just a prompt counts for doing more reads from shell */
         DEBUG22(fprintf(stderr, "(1)unix pad first line = %d\n", padmode_dspl->unix_pad->first_line);)
         if (lcl_warp_needed && (padmode_dspl->unix_pad->first_line == 0))   /* RES 3/3/94  - addedlast test to cut down on cursor jumping */
            {
               if (lines_read_from_shell > 0) /* RES 10/22/97 Test added to avoid mouse dropping in main window when only cursor changed */
                  clear_text_cursor(padmode_dspl->cursor_buff->current_win_buff->x_window, padmode_dspl);  /* if the cursor is in the main window, we blasted it */
               lines_read_from_shell = 1;
               redraw_needed = (padmode_dspl->unix_pad->redraw_mask & FULL_REDRAW);
               set_window_col_from_file_col(padmode_dspl->cursor_buff);
               lcl_warp_needed = (padmode_dspl->cursor_buff->which_window == UNIXCMD_WINDOW);  /* RES 3/15/94 only warp if we are in the unix window */
               process_redraw(padmode_dspl, redraw_needed, lcl_warp_needed);
               warp_needed = False;
            }
         else
            warp_needed |= lcl_warp_needed;

         got_it_all = scroll_some(&lines_copied,
                                  padmode_dspl,
                                  lines_displayed);

      }

   /**************************************************************
   *  If we need to send stuff to the shell, do it now.
   *  3/2/94 Moved block from before the check for "cmd fd data", RES
   *  Purpose of move, give shell socket more time in the overflow case.
   **************************************************************/
   if (!done && (Shell_socket != -1) && FD_ISSET(Shell_socket, &writefds)) /* 3/3/94 RES added test for not done */
      {
         DEBUG19(fprintf(stderr, "wait_for_input: want to send %d lines to the shell\n", padmode_dspl->unix_pad->file_line_no);)
         lines_sent = send_to_shell(padmode_dspl->unix_pad->file_line_no, padmode_dspl->unix_pad);
         if (lines_sent < 0)
            break; /* leave routine, maybe to process close of unixcmd window */
         padmode_dspl->unix_pad->file_line_no -= lines_sent;
         padmode_dspl->unix_pad->first_line -= lines_sent;  /* RES 3/3/94  - added */
         if (padmode_dspl->unix_pad->first_line < 0)
            padmode_dspl->unix_pad->first_line = 0;

         DEBUG22(fprintf(stderr, "(2)unix pad first line = %d\n", padmode_dspl->unix_pad->first_line);)
         if (padmode_dspl->unix_pad->first_line == 0)  /* RES 3/3/94  - added to cut down on cursor jumping */
            {
               redraw_needed = ((padmode_dspl->main_pad->redraw_mask | padmode_dspl->unix_pad->redraw_mask) & FULL_REDRAW);
               process_redraw(padmode_dspl, redraw_needed, True);
            }
         DEBUG19(fprintf(stderr, "send_to_shell sent %d lines to the shell\n", lines_sent);)
         /* 3/3/94 RES sending lines to the shell is same as getting lines from shell, trigger select on read from shell */
         lines_read_from_shell = 1;
      }

   /**************************************************************
   *  Check for data on the X server socket
   **************************************************************/
   while (!(FD_ISSET(ConnectionNumber(dspl_descr->display), &readfds)) && !got_it_all && !done)
   {
      FD_ZERO(&readfds);
      FD_SET(ConnectionNumber(dspl_descr->display), &readfds);
      for (walk_dspl = dspl_descr->next; walk_dspl != dspl_descr; walk_dspl = walk_dspl->next)
         FD_SET(ConnectionNumber(walk_dspl->display), &readfds);
      time_out.tv_sec = 0;
      time_out.tv_usec = 35000;
      DEBUG22(fprintf(stderr, "wait_for_input:  Waiting on X server fd %d for %d micro seconds (4)\n", ConnectionNumber(dspl_descr->display), time_out.tv_usec);)

      nfound = select(nfds, HT &readfds, NULL, NULL, &time_out);
      if (nfound != 0)
         {
            DEBUG22(if (nfound < 0) fprintf(stderr, "ERROR on Xserver socket (%s)\n", strerror(errno));)
            if (!FD_ISSET(ConnectionNumber(dspl_descr->display), &readfds))
               for (walk_dspl = dspl_descr->next; walk_dspl != dspl_descr; walk_dspl = walk_dspl->next)
                  if (FD_ISSET(ConnectionNumber(walk_dspl->display), &readfds))
                     {
                        returned_display = walk_dspl;
                        DEBUG22(fprintf(stderr, "(D) Data ready on Xserver socket (#%d)\n", returned_display->display_no);)
                        break; /* break out of this little for loop */
                     }
            done = True;
         }

      if (!done && padmode_dspl->pad_mode) /* RES 9/15/2005 Added pad_mode check */
         got_it_all = scroll_some(&lines_copied,
                                  padmode_dspl,
                                  dspl_descr->main_pad->window->lines_on_screen);

   } /* end of while fds is not set on x server socket and there is more to scroll and there is no errors  */
#else  /* is WIN32 */
   if ((Shell_socket != -1) && FD_ISSET(Shell_socket, &readfds))
      {
         int count;
         /***************************************************************
         *  This always just has a short bit of junk (the word "Tickle")
         *  The shell2pad thread writes to this pipe to wake up the main
         *  thread.  This satisfies the select, however no useful data is
         *  passed.
         ***************************************************************/
         count = recv(Shell_socket, msg, sizeof(msg), 0);
      }
#endif /* WIN32 */
#endif /* PAD */

}  /* while not done */

/***************************************************************
*  
*  If we changed the prompt, we need to redraw the window
*   and maybe adjust the cursor pos.
*  
***************************************************************/

DEBUG22(fprintf(stderr, "(3)unix pad first line = %d\n", dspl_descr->unix_pad->first_line);)
if (warp_needed && (padmode_dspl->unix_pad->first_line == 0))  /* RES 3/3/94  - added second test to cut down on cursor jumping */
   {
      DEBUG19(fprintf(stderr, "unix redraw\n");)
      warp_needed = (padmode_dspl->cursor_buff->which_window == UNIXCMD_WINDOW);  /* only warp if we are in the unix window */
      redraw_needed = (padmode_dspl->unix_pad->redraw_mask & FULL_REDRAW);
      set_window_col_from_file_col(padmode_dspl->cursor_buff);
      clear_text_cursor(padmode_dspl->cursor_buff->current_win_buff->x_window, padmode_dspl);  /* if the cursor is in the main window, we blasted it */
      process_redraw(padmode_dspl, redraw_needed, warp_needed);
   }

return(returned_display);

} /* end of wait_for_input */


/************************************************************************

NAME:      setup_fdset  - Set up the fd_sets for the main select

PURPOSE:    This routine sets the file descriptor set structures used
            by select for the main select.  This is the one which waits
            for any input.

PARAMETERS:
   1.   dspl_descr   - pointer to DISPLAY_DESCR (INPUT)
                       This is the first display description in the list.
                       of displays.  Except when a cc is in progress, 
                       there is only 1.  Otherwise, if input shows up on
                       multiple X server sockets, this one is chosen first.

   2.   Shell_socket - int (INPUT)
                       This is the file descriptor for the connection to the
                       shell.  If pad mode is 

   3.   cmd_fd       - int (INPUT)
                       This is the file descriptor for the alternate
                       input file of DM commands.  If not used, the value is -1.

   5.   readfds      - pointer to fd_set (OUTPUT)
                       This is the file descriptor set for read.  It is the one
                       which watches for input from the X server, the shell, or
                       the alternate command file.

   6.   writefds     - pointer to fd_set (OUTPUT)
                       This is the file descriptor set for write.  It is only
                       used if output to the shell was blocked and we have stuff
                       waiting to go.  This parameter can be NULL.

GLOBAL DATA:
   1.  toshell_socket_full -  int (INPUT)
                     This flag is maintained global within this module.  It is set
                     by send_to_shell to show that part of a line is waiting for
                     transmission.

FUNCTIONS:
   1.  set the file descriptor sets.


*************************************************************************/

static void setup_fdset(DISPLAY_DESCR   *dspl_descr,
                        int               Shell_socket,
                        int               cmd_fd,
                        fd_set           *readfds,
                        fd_set           *writefds)
{
DISPLAY_DESCR        *walk_dspl;
int                   lcl_pad_mode;


/***************************************************************
*  Set up the parms for the select call.
*  We are always interested in reading on the X file descriptor
*  Usually also on the Shell file descriptor.
*  Sometimes we are interested in writing to the shell.
***************************************************************/
FD_ZERO(readfds);
if (writefds)
   FD_ZERO(writefds);

FD_SET(ConnectionNumber(dspl_descr->display), readfds);
lcl_pad_mode = dspl_descr->pad_mode;
DEBUG22(
if (Shell_socket != -1)
   fprintf(stderr, "wait_for_input:  Waiting on server fd %d and shell fd %d", ConnectionNumber(dspl_descr->display), Shell_socket);
else
   fprintf(stderr, "wait_for_input:  Waiting on server fd %d", ConnectionNumber(dspl_descr->display));
)
for (walk_dspl = dspl_descr->next; walk_dspl != dspl_descr; walk_dspl = walk_dspl->next)
{
   FD_SET(ConnectionNumber(walk_dspl->display), readfds);
   lcl_pad_mode |= walk_dspl->pad_mode;
   DEBUG22(fprintf(stderr, " and CC Xsserver fd %d\n", ConnectionNumber(walk_dspl->display));)
}

if (Shell_socket != -1)
   FD_SET(Shell_socket, readfds);
else
   if (lcl_pad_mode)
      dirty_bit(dspl_descr->main_pad->token) = False;

if (cmd_fd != -1)
   FD_SET(cmd_fd, readfds);

DEBUG22(
if (cmd_fd != -1)
   fprintf(stderr, " and cmd fd %d\n", cmd_fd);
else
   fprintf(stderr, "\n");
)

if (toshell_socket_full && writefds && (Shell_socket != -1))
   {
      DEBUG22(fprintf(stderr, "Shell socket full flag set, watching shell socket for write\n");)
      FD_SET(Shell_socket, writefds);
   }
} /* end of setup_fdset */


/************************************************************************

NAME:      transpad_fgets  - Extract 1 line from a buffer filled by read(2)

PURPOSE:    This routine is called from read_dm_cmd.
            It performs fgets type processing on a single buffer.  We
            cannot use the real fgets because it will block.  The read
            which gets the data is driven by a select statement.

PARAMETERS:
   1.  target_buff   - pointer to char (OUTPUT)
                       This is where the new line goes.

   2.  max_target    - int (INPUT)
                       This is sizeof target_buff.  The maximum number of
                       characters copied is max_target-1.  The NULL is 
                       always put on the end.

   3.  readbuff      - pointer to char (INPUT/OUTPUT)
                       This is the current buffer filled by read.  The
                       first line is pulled off and put in target_buff.
                       If there is no newline character, the buffer is
                       not modified.

   4.  readbuff_len  - int (INPUT)
                       This is the current length of the read buffer.

FUNCTIONS:
   1.  If there is a newline in the string, or more bytes than will
       fit in the target, move the line from the readbuff to the
       target buff and remove it from the front of the readbuff.

   2.  Otherwise do nothing.


RETURNED VALUE:
   target_buff  -  pointer to char
                   If a line is copied, the target_buff pointer is returned (like fgets)
                   If no line is copied, NULL is returned.

*************************************************************************/


static char  *transpad_fgets(char   *target_buff,
                             int     max_target,
                             char   *readbuff,
                             int     readbuff_len)
{
char         *end_input = readbuff + readbuff_len;
int           line_len;
char         *p;
char         *from;
char         *to;
char         *ret;

p = strchr(readbuff,  '\n');
if (p || (readbuff_len >= (max_target - 1)))
   {
      line_len = (p - readbuff) + 1; /* include the newline */
      if (line_len > (max_target - 1))
         line_len = max_target - 1;
   }
else
   {
      if (readbuff_len > (max_target - 1 ))
         line_len = max_target - 1;
      else
         line_len = 0;
   }

if (line_len > 0)
   {
      memcpy(target_buff, readbuff, line_len);
      target_buff[line_len] = '\0';
      from = &readbuff[line_len];
      to = readbuff;
      while (from <= end_input) *to++ = *from++;
      ret = target_buff;
   }
else
   ret = NULL;

return(ret);

} /* end of transpad_fgets */


/************************************************************************

NAME:      process_dm_cmdline  - Process a command from the cmd file descriptor

PURPOSE:    This routine is called from read_dm_cmd.
            The line either contains Ce commands (-input) or 
            lines showing up from a program (-transpad).
            The lines are either put in the command holding area or
            put in the main pad memdata.

PARAMETERS:
   1.  dspl_descr   - pointer to DISPLAY_DESCR (INPUT/OUTPUT)
               This is a display description needed to access the main pad
               memdata structure or the stored command area.

   2.  buff  - pointer to int (INPUT)
               This is the line to process.  The newline has been stripped off.

FUNCTIONS:
   1.  In TRANSPAD mode, put the line passed at the end of the main
       pad memdata.  Then determine if we need to scroll the window.
       We will issue the redraw from here.

   2.  In non-TRANSPAD mode, store the commands in the saved commmand
       area and send a fake keystroke to trigger their parsing.
       The fake keystroke has an invalid keycode, so it will never
       accidentally collide with something else.


RETURNED VALUE:
   got_nonblank_cmd  -  int
                        True -  Process redraw was called or commands were stored
                        False - No command was generated.

*************************************************************************/

static int process_dm_cmdline(DISPLAY_DESCR   *dspl_descr,
                              char            *buff)
{
int           redraw_needed = 0;
int           got_nonblank_cmd = False;
int           winline_added;
char          color_line[MAX_LINE+1];

if (TRANSPAD)
   {
      /* RES 7/23/2003 Added call to process vt100 color lines returned from Linux 'ls' command */
      if (vt100_color_line(dspl_descr, buff, color_line, sizeof(color_line)) != False)  /* in vt100.c */
         {
            put_line_by_num(dspl_descr->main_pad->token,
                            total_lines(dspl_descr->main_pad->token) - 1,
                            buff,
                            INSERT);
            put_color_by_num(dspl_descr->main_pad->token, total_lines(dspl_descr->main_pad->token) - 1, color_line);
        }
      else
         put_line_by_num(dspl_descr->main_pad->token,
                         total_lines(dspl_descr->main_pad->token) - 1,
                         buff,
                         INSERT);

      winline_added = (total_lines(dspl_descr->main_pad->token) - 1) - dspl_descr->main_pad->first_line;
      /* scroll down a line if we just added the line past the last line on the screen */
      if (winline_added == dspl_descr->main_pad->window->lines_on_screen)
         {
            dspl_descr->main_pad->first_line++;
            redraw_needed = (dspl_descr->main_pad->redraw_mask & FULL_REDRAW);
            process_redraw(dspl_descr, redraw_needed, False);
            got_nonblank_cmd = True;
         }
      else
         if (winline_added < dspl_descr->main_pad->window->lines_on_screen)
            {
               if ((dspl_descr->main_pad->redraw_start_line == -1) ||
                   (dspl_descr->main_pad->redraw_start_line > winline_added))
                  dspl_descr->main_pad->redraw_start_line = winline_added;
               redraw_needed = (dspl_descr->main_pad->redraw_mask & PARTIAL_REDRAW);
               process_redraw(dspl_descr, redraw_needed, False);
               got_nonblank_cmd = True;
            }
     if (dspl_descr->next != dspl_descr) /* cc mode */
         {
            process_redraw(dspl_descr, 0, False); /* cover redrawing any cc windows */
            got_nonblank_cmd = True;
         }
   }
else
   {
      store_cmds(dspl_descr, buff, False);
      fake_keystroke(dspl_descr);
      got_nonblank_cmd = True;
   }

return(got_nonblank_cmd);

} /* end of process_dm_cmdline */


/************************************************************************

NAME:      read_dm_cmd  - Read a command from the cmd file descriptor

PURPOSE:    This routine is used when -input or -transpad was specfied.
            Ce commands or main pad data 
            can be read on an alternate file descriptor.
            This routine reads a block of data because select said there
            is one ready.  It extracts as many lines as possible and
            processes them.

PARAMETERS:
   1.  dspl_descr   - pointer to DISPLAY_DESCR (INPUT/OUTPUT)
               This is a display description needed to access the main pad
               memdata structure or the stored command area.

   2.  cmd_fd   - pointer to int (INPUT/OUTPUT)
               This is a pointer to the to the file descriptor for the alternate
               input file of DM commands.  If not used, the value is -1.  If
               eof is detected on this file, the vaule is set to -1.


FUNCTIONS:
   1.  Read a block of data.  Becareful in case there is residual data
       in the buffer from a previous partial read.

   2.  Chop out lines of data from the block and process them.

RETURNED VALUE:
   cmd_read  -  int
                True -  A command was read and saved
                False - No command was generated.

*************************************************************************/

static int read_dm_cmd(DISPLAY_DESCR   *dspl_descr,
                       int             *cmd_fd)
{
char          buff[MAX_LINE];
static char   readbuff[MAX_LINE+1] = "";
int           init_len = strlen(readbuff);
int           remaining_len;
char         *cur_pos;

int           count;
int           i;
int           got_nonblank_cmd = False;

cur_pos = &readbuff[init_len]; /* the null at the end of the line */
remaining_len = MAX_LINE - init_len;

count = read(*cmd_fd, cur_pos, remaining_len);
cur_pos[count] = '\0'; /* readbuff is 1 longer than MAX_LINE */
DEBUG19(fprintf(stderr, "read_dm_cmd: init len %d, just read %d\n", init_len, count);)

if (count > 0)
   {
      while (transpad_fgets(buff, sizeof(buff), readbuff, count+init_len) != NULL)
      {
         i = strlen(buff);
         DEBUG19(fprintf(stderr, "read_dm_cmd: one line len %d (%s)\n", i, buff);)
         if (i > 0)
            {
               if (buff[i-1] == '\n')
                  buff[i-1] = '\0';
               got_nonblank_cmd |= process_dm_cmdline(dspl_descr, buff);
             }
      }
   }
else
   if (count < 0)
      {
         /***************************************************************
         *  count < 0 means I/O error.
         ***************************************************************/
         snprintf(buff, sizeof(buff), "Error reading command/transpad file, (%s)", strerror(errno));
         dm_error(buff, DM_ERROR_LOG);
         if (init_len > 0)
            got_nonblank_cmd |= process_dm_cmdline(dspl_descr, readbuff);
         *cmd_fd = -1;
         return(got_nonblank_cmd);
      }
   else
      {
         /***************************************************************
         *  Must be eof, count == 0
         ***************************************************************/
         if (init_len > 0)
            got_nonblank_cmd |= process_dm_cmdline(dspl_descr, readbuff);

         *cmd_fd = -1; /* on end of file, turn off the command fd */
         DEBUG19(fprintf(stderr, "read_dm_cmd: eof found\n");)
         if (TRANSPAD)
            {
               /* turn off transpad on eof */
               free(OPTION_VALUES[TRANSPAD_IDX]);
               OPTION_VALUES[TRANSPAD_IDX] = strdup("no");
               if (dspl_descr->edit_file_changed)
                  dm_pw(dspl_descr, edit_file, False);
            }
      }

return(got_nonblank_cmd);

} /* end of read_dm_cmd */


#ifdef PAD
/************************************************************************

NAME:      scroll_some  - scroll shell output onto the window

PURPOSE:    This routine is used in pad mode to take care of scrolling
            data returned by the shell onto the screen.  It is called from
            two places.  wait_for_input calls it and also do_background_task
            from getevent.c.  The routine scrolls at most MAX_FLASHES_PER_CALL
            times and then returns.  It also handles no-scroll and autohold modes.

PARAMETERS:

   1.   lines_copied -  pointer to int (INPUT/OUTPUT)
                     This is the number of lines which need to be moved onto the
                     screen.  It is either the number of lines read or the number
                     of lines remaining from a previous call.
                     The value is decremented by the number of lines actually drawn.

   2.   main_pa d -  pointer to PAD_DESCR (INPUT)
                     This is the main pad description used to access current position and
                     window information.

   3.   lines_displayed - int (INPUT)
                     This is the number of lines currently displayed on the screen.  This value
                     must be calculated before the call and passed in.  The sequence is:
                     1.  wait for input is notified that there is data on the shell socket
                     2.  wait for input calculates lines_displayed (total_lines - first_line_displayed).
                     3.  wait for input reads the data from the shell socket into the pad (modifies total lines)
                     4.  wait for input calls scroll_some to display the data.

FUNCTIONS:

   1.  Determine if the window needs to be scrolled to the bottom and do it.

   2.  If the window is partway empty, flash draw all the lines

   3.  Scroll one line at a time until all the lines are on the screen, or
       until MAX_FLASHES_PER_CALL redraws have been done.

   4.  Update lines_copied to show the number of lines drawn.

   5.  Set the background task flag to show more data needs to be copied.


RETURNED VALUE:

   got_it_all  -  int
                  This flag show that all the passed in input has been displayed.
                  True  - All passed data has been drawn
                  False - Some data remains to be drawn

*************************************************************************/

int scroll_some(int              *lines_copied,
                DISPLAY_DESCR    *dspl_descr,
                int               lines_displayed)
{
#define MAX_FLASHES_PER_CALL 2
int                   flashes = 0;
int                   lines_drawn;
int                   j;
int                   got_it_all = False;
int                   lines_left;
static int            saved_scrolling_first_line;

/***************************************************************
*  
*  Special case for us being fully obscured or iconned.  
*  Do no drawing, just position to the bottom.
*  
***************************************************************/

if (dspl_descr->x_pixmap == None)
   {
      DEBUG19(fprintf(stderr, "scroll_some: Pixmap not available, going to bottom, first is now %d\n", dspl_descr->main_pad->first_line);)
      pad_to_bottom(dspl_descr->main_pad);
      saved_scrolling_first_line = dspl_descr->main_pad->first_line;
      change_background_work(dspl_descr, BACKGROUND_SCROLL, 0);
      return(True);
   }

/***************************************************************
*  
*  Special case for repositioning saved scrolling first line.
*  The lines copied pointer is null.
*  
***************************************************************/

if (!lines_copied)
   {
      DEBUG19(fprintf(stderr, "scroll_some: RESIZE ONLY, adjusting saved first line from %d to %d\n", saved_scrolling_first_line, dspl_descr->main_pad->first_line);)
      saved_scrolling_first_line = dspl_descr->main_pad->first_line;
      change_background_work(dspl_descr, BACKGROUND_SCROLL, 0);
      return(False);
   }

/***************************************************************
*  
*  If we were in hold mode and the user scrolled down before
*  pressing hold to release the hold, pick up at the new line.
*  Also account for this movement in the lines to scroll.
*  
***************************************************************/

if (dspl_descr->main_pad->first_line > saved_scrolling_first_line)
   {
      *lines_copied -= (dspl_descr->main_pad->first_line - saved_scrolling_first_line);
      if (*lines_copied <= 0)
         *lines_copied = 1;
      DEBUG19(fprintf(stderr, "scroll_some: User has scrolled down, adjusting saved first line from %d to %d\n", saved_scrolling_first_line, dspl_descr->main_pad->first_line);)
      saved_scrolling_first_line = dspl_descr->main_pad->first_line;
   }

DEBUG19(fprintf(stderr, "scroll_some: lines copied %d, scrolling first line %d, lines displayed %d\n",  *lines_copied, saved_scrolling_first_line, lines_displayed);)

if (!dspl_descr->hold_mode && (*lines_copied > 0))
   {
      if (saved_scrolling_first_line != dspl_descr->main_pad->first_line)
         {
            DEBUG19(fprintf(stderr, "scrolling_first_line %d current first line %d\n", saved_scrolling_first_line, dspl_descr->main_pad->first_line);)
            dspl_descr->main_pad->first_line = saved_scrolling_first_line;
            process_redraw(dspl_descr, MAIN_PAD_MASK & FULL_REDRAW, False);
         }

      if (dspl_descr->scroll_mode)
         {
            clear_text_cursor(dspl_descr->main_pad->x_window, dspl_descr);  /* if the cursor is in the main window, we blasted it */

            for (lines_drawn = 0; (lines_drawn < *lines_copied) && (flashes< MAX_FLASHES_PER_CALL); flashes++)
            {
               /*  note: autohold mode contains the number of lines scrolled so far +1, upon emptying, it is set to 1 */
               if (dspl_descr->autohold_mode && (dspl_descr->autohold_mode >= (dspl_descr->main_pad->window->lines_on_screen+1)))
                  {
                     dspl_descr->hold_mode = True;
                     break;
                  }
               if (lines_displayed < dspl_descr->main_pad->window->lines_on_screen)
                  {
                     lines_left  = dspl_descr->main_pad->window->lines_on_screen - lines_displayed;
                     DEBUG19(
                        fprintf(stderr, "lines_displayed: %d,  lines_empty: %d, lines on screen %d\n",
                                lines_displayed, lines_left, dspl_descr->main_pad->window->lines_on_screen);
                     )
                     process_redraw(dspl_descr, MAIN_PAD_MASK & FULL_REDRAW, False);
                     j = ((lines_left < *lines_copied) ? lines_left : *lines_copied);
                     lines_drawn += j;
                     lines_displayed += j;
                     if (dspl_descr->autohold_mode)
                        (dspl_descr->autohold_mode) += j;
                     DEBUG19(fprintf(stderr, "scroll_on, Flash drew %d lines\n", j);)
                  }
               else
                  {
                     scroll_a_line(dspl_descr->main_pad, (dspl_descr->show_lineno ? dspl_descr->lineno_subarea : NULL));
                     if (dspl_descr->autohold_mode)
                        (dspl_descr->autohold_mode)++;
                     lines_displayed++;
                     lines_drawn++;
                     DEBUG19(fprintf(stderr, "Scrolled a line\n");)
                  }

               if (dspl_descr->main_pad->formfeed_in_window)
                  {
                     if (dspl_descr->autohold_mode)
                        {
                           DEBUG19(fprintf(stderr, "Formfeed causing hold\n");)
                           dspl_descr->hold_mode = True;
                           break;
                        }
                     DEBUG19(fprintf(stderr, "Formfeed causing extra scroll of  %d lines\n", dspl_descr->main_pad->formfeed_in_window);)
                     dspl_descr->main_pad->first_line = pb_ff_scan(dspl_descr->main_pad, dspl_descr->main_pad->first_line);
                     process_redraw(dspl_descr, MAIN_PAD_MASK & FULL_REDRAW, False /* no warp*/);
                     lines_displayed = 1;
                  }
            } /* end of for loop on i */

            if (dspl_descr->main_pad->first_line > dspl_descr->main_pad->file_line_no)
               dspl_descr->main_pad->file_line_no = dspl_descr->main_pad->first_line;

            if (lines_drawn < *lines_copied)
               {
                  *lines_copied -= lines_drawn;
                  DEBUG19(fprintf(stderr, "(1) Changing background scroll mode to %d\n", *lines_copied);)
                  change_background_work(dspl_descr, BACKGROUND_SCROLL, *lines_copied);
               }
            else
               {
                  *lines_copied = 0;
                  got_it_all = True;
                  DEBUG19(fprintf(stderr, "(2) Changing background scroll mode to 0\n");)
                  change_background_work(dspl_descr, BACKGROUND_SCROLL, 0);
               }
            if (dspl_descr->sb_data->sb_mode != SCROLL_BAR_OFF)
               process_redraw(dspl_descr,
                              MAIN_PAD_MASK & SCROLL_REDRAW, /* zero length scroll gets the sb windows redrawn */
                              False /* no warp*/);

         } /* end of scroll mode */
      else
         {
            /***************************************************************
            *  Non-scrolling mode.
            *  If there is room on the screen, display the data.
            ***************************************************************/
            lines_left  = dspl_descr->main_pad->window->lines_on_screen - lines_displayed;
            if (lines_left >= *lines_copied)
               {
                  DEBUG19(fprintf(stderr, "lines_displayed: %d,  lines_empty: %d, lines to copy %d\n", lines_displayed, lines_left, *lines_copied);)
                  process_redraw(dspl_descr, MAIN_PAD_MASK & FULL_REDRAW, False);
                  if (dspl_descr->main_pad->formfeed_in_window)
                     {
                        if (dspl_descr->autohold_mode)
                           {
                              DEBUG19(fprintf(stderr, "Formfeed causing hold(#2)\n");)
                              dspl_descr->hold_mode = True;
                           }
                        DEBUG19(fprintf(stderr, "Formfeed causing extra scroll of  %d lines\n", dspl_descr->main_pad->formfeed_in_window);)
                        dspl_descr->main_pad->first_line = pb_ff_scan(dspl_descr->main_pad, dspl_descr->main_pad->first_line);
                        process_redraw(dspl_descr, MAIN_PAD_MASK & FULL_REDRAW, False /* no warp*/);
                        /*lines_displayed = 1; 10/11/95 compiler says not used */
                     }
                  else
                     if (dspl_descr->autohold_mode)
                        {
                           (dspl_descr->autohold_mode) += *lines_copied;
                           if (dspl_descr->autohold_mode >= (dspl_descr->main_pad->window->lines_on_screen+1))
                              dspl_descr->hold_mode = True;
                        }
                  DEBUG19(fprintf(stderr, "Non-scroll, Flash drew %d lines in partially full screen\n", *lines_copied);)
                  *lines_copied = 0;
                  got_it_all = True;
                  DEBUG19(fprintf(stderr, "(3) Changing background scroll mode to 0\n");)
                  change_background_work(dspl_descr, BACKGROUND_SCROLL, 0);
               }
            else
               if (dspl_descr->autohold_mode)
                  {
                     /***************************************************************
                     *  Non-scrolling mode with autohold.
                     ***************************************************************/
                     if (lines_left > 0)
                        {
                           /***************************************************************
                           *  There is some, but not enough empty space on the screen
                           *  Fill the rest of the screen and turn on hold.
                           ***************************************************************/
                           DEBUG19(fprintf(stderr, "Partial screen fill, lines_displayed: %d,  lines_empty: %d, lines to copy %d\n", lines_displayed, lines_left, *lines_copied);)
                           process_redraw(dspl_descr, MAIN_PAD_MASK & FULL_REDRAW, False);
                           dspl_descr->autohold_mode += lines_left;
                           dspl_descr->hold_mode = True;
                           DEBUG19(fprintf(stderr, "Flash drew %d lines, to fill screen\n", lines_left);)
                           *lines_copied -= lines_left;
                           DEBUG19(fprintf(stderr, "(4) Changing background scroll mode to %d\n", *lines_copied);)
                           change_background_work(dspl_descr, BACKGROUND_SCROLL, *lines_copied);
                        }
                     else
                        {
                           lines_left = dspl_descr->main_pad->window->lines_on_screen - (dspl_descr->autohold_mode - 1);
                           if (lines_left <= 0)
                              lines_left =  dspl_descr->main_pad->window->lines_on_screen;
                           if (*lines_copied >= lines_left)
                              {
                                 /***************************************************************
                                 *  There screen is full and we have a full page of data.
                                 *  Advance one screen.
                                 ***************************************************************/
                                 if (dspl_descr->main_pad->formfeed_in_window)
                                    dspl_descr->main_pad->first_line += dspl_descr->main_pad->formfeed_in_window;
                                 else
                                    dspl_descr->main_pad->first_line += lines_left;
                                 process_redraw(dspl_descr, MAIN_PAD_MASK & FULL_REDRAW, False);
                                 DEBUG19(fprintf(stderr, "Non-scroll mode, autohold mode, new full screen of data, first line %d, lines left = %d\n", dspl_descr->main_pad->first_line, lines_left);)
                                 if (dspl_descr->main_pad->formfeed_in_window) /* the newly drawn window */
                                    *lines_copied -= dspl_descr->main_pad->formfeed_in_window;
                                 else
                                    *lines_copied -= dspl_descr->main_pad->window->lines_on_screen;
                                 DEBUG19(fprintf(stderr, "(5) Changing background scroll mode to %d\n", *lines_copied);)
                                 change_background_work(dspl_descr, BACKGROUND_SCROLL, *lines_copied);
                                 dspl_descr->autohold_mode = dspl_descr->main_pad->window->lines_on_screen + 1;
                                 dspl_descr->hold_mode = True;
                              }
                           else
                              {
                                 /***************************************************************
                                 *  The screen is full, and we have a partial screen of data.
                                 *  Put out the partial screen of data, and the autohold value
                                 *  by the number of lines copied.
                                 ***************************************************************/
                                 pad_to_bottom(dspl_descr->main_pad);
                                 process_redraw(dspl_descr, MAIN_PAD_MASK & FULL_REDRAW, False);
                                 DEBUG19(fprintf(stderr, "Non-scroll mode, autohold mode, partial screen of data, first line %d, lines left = %d\n", dspl_descr->main_pad->first_line, lines_left);)
                                 (dspl_descr->autohold_mode) += *lines_copied;
                                 got_it_all = True;
                                 *lines_copied = 0;
                                 DEBUG19(fprintf(stderr, "(6) Changing background scroll mode to 0\n");)
                                 change_background_work(dspl_descr, BACKGROUND_SCROLL, 0);
                              }

                       }
                  } /* end of autohold mode */
               else
                  {
                     /***************************************************************
                     *  Non-scroll mode, non-autohold mode.
                     *  Go to the bottom of the new data, look for form feeds, and redraw.
                     ***************************************************************/
                     pad_to_bottom(dspl_descr->main_pad);
                     process_redraw(dspl_descr, MAIN_PAD_MASK & FULL_REDRAW, False);
                     DEBUG19(fprintf(stderr, "Non-scroll mode, non-autohold mode, new first line %d\n", dspl_descr->main_pad->first_line);)
                     got_it_all = True;
                     *lines_copied = 0;
                     DEBUG19(fprintf(stderr, "(7) Changing background scroll mode to 0\n");)
                     change_background_work(dspl_descr, BACKGROUND_SCROLL, 0);
                  } /* end of non-scroll, non-autohold mode */

         } /* end of non-scroll mode */

      /***************************************************************
      *  Save the first line where we left off.
      ***************************************************************/
      saved_scrolling_first_line = dspl_descr->main_pad->first_line;

      if (dspl_descr->hold_mode) /* condition added to line 10/1/93 RES */
         process_redraw(dspl_descr, TITLEBAR_MASK & FULL_REDRAW, False);

   }   /* end of not in hold mode */
else
   {
      DEBUG19(
         if (dspl_descr->hold_mode)
            fprintf(stderr, "In hold mode, no scrolling done\n");
         else
            fprintf(stderr, "No lines to scroll\n");
      )
      got_it_all = True;
      if (!(dspl_descr->hold_mode))
         {
            DEBUG19(fprintf(stderr, "(8) Changing background scroll mode to 0\n");)
            *lines_copied = 0;
            change_background_work(dspl_descr, BACKGROUND_SCROLL, 0);
         }
   }

DEBUG19(
   if (dspl_descr->autohold_mode)
      fprintf(stderr, "Autohold = %d\n", dspl_descr->autohold_mode);
)

if (dspl_descr->cursor_buff->which_window == MAIN_PAD)
   {
      if ((dspl_descr->main_pad->first_line <= dspl_descr->main_pad->file_line_no) && /* RES 10/17/97 - reposition cursor after text scrolls up a line */
         (dspl_descr->main_pad->first_line + dspl_descr->main_pad->window->lines_on_screen > dspl_descr->main_pad->file_line_no))
      dm_position(dspl_descr->cursor_buff, dspl_descr->main_pad, dspl_descr->main_pad->file_line_no, dspl_descr->main_pad->file_col_no);
      if (in_window(dspl_descr)) /* RES 11/7/97 - fix mouse dropping */
         {
            fake_cursor_motion(dspl_descr); /* RES 10/17/97 - reposition cursor after text scrolls up a line */
            dspl_descr->cursor_buff->up_to_snuff  = False;
         }
   }

return(got_it_all);

} /* end of scroll_some */

#endif

/************************************************************************

NAME:      dm_dq -  send a quit signal to a process

PURPOSE:    

PARAMETERS:

   1.  dmc            - pointer to DMC (INPUT)
                        This is the command structure for the dq command.
                        It contains the data from any dash options.

   2.   main_pad    -  pointer to PAD_DESCR (INPUT)
                       This is the main pad description.  It is needed to
                       scroll to the bottom of the window if we are not 
                       already at the bottom.
                       This could be NULL.  It should never be null in pad
                       mode, but we will check for it.

   3.   unix_pad    -  pointer to PAD_DESCR (INPUT)
                       This is the unix pad description.  dq causes any
                       lines in this window to be emptied out.

   4.   pad_mod     -  int (INPUT)
                       This is flag specifies whether we are in pad mode.


FUNCTIONS :
   1.   If a process name was passed, verify that it is a number (Unix does
        not have process names so we treat name as a pid) and issue a
        kill to that process using the passed signal number (-c).

   2.   If a process name was not passed, verify that we are in pad_mode.

   3.   If -b, -s, -i, or -c was passed, send that signal to the shell.

   4.   If -a was specified, send the requested character to the line discipline.

   5.   If no parm was specified, send the interupt character to the line discipline.

RETURNED VALUE:
   redraw_needed    -  int
                       This flag specifies whether the screen needs to be redrawn.


*************************************************************************/

int  dm_dq(DMC             *dmc,                    /* input  */
           DISPLAY_DESCR   *dspl_descr)             /* input  */
{
#ifndef WIN32
unsigned int     c_sig;
int              c_pid;
char             work[80];
#endif
int              rc;
char             kill_line[2];
int              redraw_needed;
int              signalled;
int              temp_redraw;
int              old_first_line;

DEBUG19(fprintf(stderr, "@dm_dq\n");)

#ifndef WIN32
if (dmc->dq.name)
   {
      if (dmc->dq.dash_c)
         c_sig = dmc->dq.dash_c;
      else
         if (dmc->dq.dash_s)
            c_sig = SIGQUIT;
         else
            if (dmc->dq.dash_b)
               c_sig = SIGKILL;
            else
               if (dmc->dq.dash_i)
                  c_sig = SIGINT;
               else
                  c_sig = SIGTERM;
      if (sscanf(dmc->dq.name,"%d%9s", &c_pid, work) != 1)
         {
            snprintf(work, sizeof(work), "(dq) Invalid decimal process id (%s)", dmc->dq.name);
            dm_error(work, DM_ERROR_BEEP);
         }
      else
         {
            DEBUG19(fprintf(stderr, "dm_dq: sending signal %d to %d\n", c_sig, c_pid);)
            rc = kill(c_pid, c_sig);
            if (rc != 0)
               {
                  snprintf(work, sizeof(work), "(dq) Kill failed for process %d (%s)", c_pid, strerror(errno));
                  dm_error(work, DM_ERROR_BEEP);
               }
         }
      return(0);
   }
else
   if (!dspl_descr->pad_mode)  /* dq without name is a no-op in non-pad mode */
      return(0);
#endif

#ifdef PAD
/***************************************************************
*  Initializations needed only in PAD mode.
***************************************************************/
redraw_needed = FULL_REDRAW & MAIN_PAD_MASK;
signalled = False;

/***************************************************************
*  If there is anything in the unix cmd window, get rid of it.
*  RES 9/15/95
*  Also, if we are in vt100 mode scroll to the bottom of the
*  main pad.
***************************************************************/
if (dspl_descr->unix_pad)
   {
      while(total_lines(dspl_descr->unix_pad->token) > 0)
      {
         delete_line_by_num(dspl_descr->unix_pad->token, 0, 1);
         redraw_needed |= FULL_REDRAW & UNIXCMD_MASK;
      }
      dspl_descr->unix_pad->file_line_no = 0;
      put_line_by_num(dspl_descr->unix_pad->token, -1, "", INSERT); /* one empty line in file RES 8/19/94 */
      dspl_descr->unix_pad->buff_ptr = get_line_by_num(dspl_descr->unix_pad->token, 0);
      dspl_descr->unix_pad->buff_modified = False;
      if ((!dspl_descr->vt100_mode) && dspl_descr->pad_mode)
         {
            pad_to_bottom(dspl_descr->main_pad);
            scroll_some(NULL, dspl_descr, 0);
         }
   }
else
   DEBUG(fprintf(stderr, "dm_dq:  Unix pad pointer unexpectedly NULL\n");)

/***************************************************************
*  If the -c num was specified, send that signal to the shell
***************************************************************/
if (dmc->dq.dash_c)
   {
      DEBUG19(fprintf(stderr, "dm_dq: sending signal %d\n", dmc->dq.dash_c);)
      kill_shell(dmc->dq.dash_c);
      signalled = True;
   }

/***************************************************************
*  If the -s option was specified, send a soft kill followed by
*  a hard kill.
***************************************************************/
if (dmc->dq.dash_s && !signalled)
   {
#ifndef WIN32
      DEBUG19(fprintf(stderr, "dm_dq: -s sending SIGTERM wait .01 sec and send SIGKILL\n");)
      kill_shell(SIGQUIT);
      usleep(10000);
      kill_shell(SIGKILL);
#else
      kill_shell(0);
#endif
      signalled = True;
   }

/***************************************************************
*  For a -b, close the pipe to the shell, that should stop it.
***************************************************************/
if (dmc->dq.dash_b && !signalled)
   {
      DEBUG19(fprintf(stderr, "dm_dq: -b sending SIGKILL and closing shell socket\n");)
#ifndef WIN32
      kill_shell(SIGKILL);
#else
      kill_shell(0);
#endif
      close_shell(); /* in pad.c */
      signalled = True;
   }

/***************************************************************
*  For a -i, send a sigint
***************************************************************/
if (dmc->dq.dash_i && !signalled)
   {
      DEBUG19(fprintf(stderr, "dm_dq:-i  sending SIGINT\n");)
      kill_shell(SIGINT);
      signalled = True;
   }

/***************************************************************
*  The default dq, send an int char across the pipe to the 
*  shell, this is normally a hex 03.
*  We only check for PAD_BLOCKED because PAD_PARTIAL is never
*  returned, and on i/o error, the message has already been generated.
*  If the eof could not be sent, put in in the input queue.
***************************************************************/
if (!signalled)
   {
#ifdef WIN32
      if (dmc->dq.dash_a)
         {
            ce_tty_char[TTY_INT] = dmc->dq.dash_a;
            kill_line[0] = ce_tty_char[TTY_INT];
            kill_line[1] = '\0';
            DEBUG19(fprintf(stderr, "dm_dq: dash a = 0x%02X, ce_tty_char[TTY_INT] = 0x%02X, kill sending char = 0x%02X\n", dmc->dq.dash_a, ce_tty_char[TTY_INT], kill_line[0]);)
            DEBUG19(fprintf(stderr, "dm_dq: sending kill character 0x%02X\n", kill_line[0]);)

            rc = pad2shell(kill_line, False);
           if (rc == PAD_BLOCKED && dspl_descr->unix_pad)
               {
                  put_line_by_num(dspl_descr->unix_pad->token, -1, kill_line, INSERT);
                  toshell_socket_full = True;
                  redraw_needed |= FULL_REDRAW & UNIXCMD_MASK;
               }
         }
      else
         send_control_c(); /* default interupt in NT, in pad.c */

#else
      if (dmc->dq.dash_a)
         ce_tty_char[TTY_INT] = dmc->dq.dash_a;
      else
         ce_tty_char[TTY_INT] = 3;
       /*get_stty();  in pad.c RES 11/18/97, get_stty is undependable */
      kill_line[0] = ce_tty_char[TTY_INT];
      kill_line[1] = '\0';
      DEBUG19(fprintf(stderr, "dm_dq: dash a = 0x%02X, ce_tty_char[TTY_INT] = 0x%02X, kill sending char = 0x%02X\n", dmc->dq.dash_a, ce_tty_char[TTY_INT], kill_line[0]);)
      DEBUG19(fprintf(stderr, "dm_dq: sending kill character 0x%02X\n", kill_line[0]);)

      rc = pad2shell(kill_line, False);
      if (rc == PAD_BLOCKED && dspl_descr->unix_pad)
         {
            put_line_by_num(dspl_descr->unix_pad->token, -1, kill_line, INSERT);
            toshell_socket_full = True;
            redraw_needed |= FULL_REDRAW & UNIXCMD_MASK;
         }
#endif
   }

/***************************************************************
*  
*  Scroll the main pad to the bottom unless we are already at
*  or past the bottom.
*  
***************************************************************/

if (dspl_descr->main_pad)
   {
      old_first_line = dspl_descr->main_pad->first_line;
      temp_redraw = pad_to_bottom(dspl_descr->main_pad);
      if (old_first_line < dspl_descr->main_pad->first_line)
         {
            redraw_needed |= temp_redraw;
            change_background_work(dspl_descr, BACKGROUND_SCROLL, 0);
         }
      else
         dspl_descr->main_pad->first_line = old_first_line;
   }
else
   DEBUG(fprintf(stderr, "dm_dq:  Main pad pointer unexpectedly NULL\n");)

return(redraw_needed);
#endif

}  /* end of dm_dq */

        
#ifdef WIN32

DWORD WINAPI transpad_thread(LPVOID pParam) 
{
DISPLAY_DESCR        *dspl_descr = (DISPLAY_DESCR *)pParam;
int                   cmd_fd = 0;
int                   done = False;

while (!done)
{
   lock_display_descr(dspl_descr, "transpad");

   read_dm_cmd(dspl_descr, &cmd_fd);
   if (cmd_fd == -1)
      done = True;

   unlock_display_descr(dspl_descr, "transpad");
}

return(0);
} /* end of transpad_thread */

/************************************************************************

NAME:      transpad_input - WIN32 only, process -TRANSPAD or -STDIN input

PURPOSE:    

PARAMETERS:

   1.  dmc            - pointer to DMC (INPUT)
                        This is the command structure for the dq command.
                        It contains the data from any dash options.

   2.   main_pad    -  pointer to PAD_DESCR (INPUT)
                       This is the main pad description.  It is needed to
                       scroll to the bottom of the window if we are not 
                       already at the bottom.
                       This could be NULL.  It should never be null in pad
                       mode, but we will check for it.

   3.   unix_pad    -  pointer to PAD_DESCR (INPUT)
                       This is the unix pad description.  dq causes any
                       lines in this window to be emptied out.

   4.   pad_mod     -  int (INPUT)
                       This is flag specifies whether we are in pad mode.


FUNCTIONS :
   1.   If a process name was passed, verify that it is a number (Unix does
        not have process names so we treat name as a pid) and issue a
        kill to that process using the passed signal number (-c).

   2.   If a process name was not passed, verify that we are in pad_mode.

   3.   If -b, -s, -i, or -c was passed, send that signal to the shell.

   4.   If -a was specified, send the requested character to the line discipline.

   5.   If no parm was specified, send the interupt character to the line discipline.

RETURNED VALUE:
   redraw_needed    -  int
                       This flag specifies whether the screen needs to be redrawn.


*************************************************************************/

void transpad_input(DISPLAY_DESCR   *dspl_descr)
{
HANDLE       transpad_thread_handle; /* handle for thread reading shell output */
DWORD        transpad_thread_id;    /* thread id */

transpad_thread_handle =  CreateThread(NULL, /* pointer to thread security attributes  */
                                       0,    /* initial thread stack size, in bytes  */
                                       &transpad_thread, /* pointer to thread function  */
                                       dspl_descr, /* argument for new thread  */
                                       0, /* creation flags  */
                                       &transpad_thread_id); /* pointer to returned thread identifier  */

} /* end of transpad_input */

#endif


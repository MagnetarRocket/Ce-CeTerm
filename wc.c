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
*  module wc.c
*
*  Routines:
*     dm_wc              - Window close
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h      */
#include <signal.h>         /* /usr/include/signal      */

#ifndef WIN32
#include <X11/Xlib.h>       /* /usr/include/X11/Xlib.h   */
#endif

#include "cc.h"
#include "dmwin.h"
#include "display.h"
#include "getevent.h"
#include "init.h"
#ifdef PAD
#include "pad.h"
#endif
#include "pastebuf.h"
#include "prompt.h"
#include "pw.h"
#include "wc.h"
#include "wdf.h"
#include "xerrorpos.h"

#include <signal.h>

void   exit(int code);

/************************************************************************

NAME:      dm_wc

PURPOSE:    This routine performs the window close operation.
            It is the shut down command for the editor.
            Unless a prompt is generated to confirm quiting a changed file,
            or the autoclose mode is being changed, this routine does not return.

PARAMETERS:

   1.  dmc            - pointer to DMC (INPUT)
                        This is the command structure for the wc command.
                        It contains the data from any dash options.
                        This parm may be null if the confirm_dash_q parm is
                        not null.

   2.  dspl_descr     - pointer to DISPLAY_DESCR (INPUT / OUTPUT)
                        This is the description for the display.
                        It is used to close down the display or set up the prompt.
                        If there are still displays open it is pointed to a valid
                        display using set_global_dspl.

   3.  edit_file      - pointer to char (INPUT)
                        If a save needs to be done, the file name is needed to pass
                        to dm_pw.

   4.  display_deleted - pointer to pointer to DISPLAY_DESCR (OUTPUT)
                         This flag tells the caller the display was actually closed down.
                         It is used in when there are multiple CC windows.
                         If non-NULL, it is the display description of the next display
                         after the one which has just been destroyed.  When this value
                         is non-NULL, do not use parameter dspl_descr any more.
                         non-NULL -  A cc window was closed, value is the next
                                     cc window in the list.
                         NULL      - No windows deleted, maybe a prompt was issued

   5.  no_prompt       - int (INPUT)
                         This flag is used to avoid prompting in autoclose mode.

   6.  make_utmp      - int (INPUT)
                        True if we created a untmp entry in pad mode and need to clear it out.


FUNCTIONS :

   1.   Switch on the type of the wc.

   2.   If no parms were specified, or -f was specified on the wc, do a save if
        needed and exit.

   3.   If -q was specified, and the file is dirty, generate a new wc command which is
        contained in a prompt command.  The result of the prompt will be a dash option.
        A yes reply will cause a wc -y to be generated, which we interpret here as a
        confirmed quit.

   4.   If -a or -s was specified, set the autoclose variable and return NULL.

OUTPUTS:
   redraw -  The mask anded with the type of redraw needed is returned.

*************************************************************************/

int  dm_wc(DMC             *dmc,                    /* input  */
           DISPLAY_DESCR   *dspl_descr,             /* input  / output */
           char            *edit_file,              /* input  */
           DISPLAY_DESCR  **display_deleted,        /* output  */
           int              no_prompt,              /* input  */
           int              make_utmp)              /* input  */
{
int              shut_down = False;
DMC             *prompt;
char             work[50];
int              rc;
int              redraw_needed = 0;
DISPLAY_DESCR   *next_dspl_descr;
DISPLAY_DESCR   *walk_dspl;
int              dash_w = False;
int              dash_f = False;


if (display_deleted)
   *display_deleted = NULL;

/***************************************************************
*  
*  Deal with the case where this is the second call after a prompt
*  is executed.  We expect either a -y or -n answer.
*  
***************************************************************/

switch (dmc->wc.type)
{
case 'y':
   shut_down = True;
   break;

case 'a':
   dspl_descr->autoclose_mode = True;
   dm_error("(wc) Autoclose is on", DM_ERROR_MSG);
   break;

case 's':
   dspl_descr->autoclose_mode = False;
   dm_error("(wc) Autoclose is off", DM_ERROR_MSG);
   break;

case '\0':
#ifdef PAD
   if (dspl_descr->pad_mode)
      {
         dm_error("(wc) Pad still active", DM_ERROR_BEEP);
         break;
      }
#endif

/* FALLTHRU */
case 'f':
case 'w':
   if (dmc->wc.type == 'w')
      dash_w = True;
   if (dmc->wc.type == 'f')
      dash_f = True;
#ifdef PAD
   DEBUG1(fprintf(stderr, "dirty bit = %d, pad mode =  %d, edit_file_changed = %d\n", dirty_bit(dspl_descr->main_pad->token), dspl_descr->pad_mode, dspl_descr->edit_file_changed);)
   if ((dirty_bit(dspl_descr->main_pad->token) && !dspl_descr->pad_mode) || (dspl_descr->edit_file_changed))
#else
   if (dirty_bit(dspl_descr->main_pad->token))
#endif
      rc = dm_pw(dspl_descr, edit_file, False);
   else
      rc = 0;
   if (rc == 0 || (strcmp(edit_file, STDIN_FILE_STRING) == 0))
      shut_down = True;
   break;

case 'q':
#ifdef PAD
   if (dspl_descr->pad_mode)
      {
         dm_error("(WC) Pad still active", DM_ERROR_BEEP);
         break;
      }
#endif
   if (!dirty_bit(dspl_descr->main_pad->token) || no_prompt)
      shut_down = True;
   else
      {
         strcpy(work, "wc -&'File modified, OK to quit? ' ");
         prompt = prompt_prescan(work, True, dspl_descr->escape_char);  /* in kd.c */
         if (prompt == NULL)
            dm_error("(wc) Unable to wc -q", DM_ERROR_LOG);
         else
            redraw_needed = dm_prompt(prompt, dspl_descr);
      }
   break;


case 'n':
default:
   break;

} /* end of switch on type */


/***************************************************************
*  
*  If all has gone well, we shut down the program.
*  
***************************************************************/

if (shut_down || dash_f)
   {
      next_dspl_descr = dspl_descr->next;
      closeup_paste_buffers(dspl_descr->display);
      if (!dash_w)
         wdf_save_last(dspl_descr);
      clear_prompt_stack(dspl_descr);
      cc_shutdown(dspl_descr);
#ifdef PAD
      if (dspl_descr->pad_mode)
         {
            kill_shell(SIGTERM);
#ifndef WIN32
            kill_shell(SIGHUP);
            kill_shell(SIGKILL);
#endif
            if (dspl_descr->next != dspl_descr) /* more than one cc running */
               {
                  if (!dspl_descr->edit_file_changed)
                     strcpy(edit_file, STDIN_FILE_STRING);
                  cc_padname(next_dspl_descr, edit_file);
                  change_icon(next_dspl_descr->display,
                              next_dspl_descr->main_pad->x_window,
                              edit_file,
                              False);
                  close_shell();
               }
#ifndef WIN32
            if (make_utmp)
               kill_utmp();  /* in pad.c */
            tty_reset(0); /* in pad.c */
#endif

         }
#endif
      DEBUG9(XERRORPOS)
      XDestroyWindow(dspl_descr->display, dspl_descr->main_pad->x_window);
      DEBUG9(XERRORPOS)
      XFlush(dspl_descr->display);
      DEBUG9(XERRORPOS)
      XCloseDisplay(dspl_descr->display);
      if (del_display(dspl_descr) || dash_f)
         exit(0);
      else
         {
            set_global_dspl(next_dspl_descr);
            if (next_dspl_descr->next == next_dspl_descr)
               cc_ce = False; /* turn off cc processing when only one window left. */
            else
               cc_titlebar();
            redraw_needed |= TITLEBAR_MASK & FULL_REDRAW;
            if (display_deleted)
               *display_deleted = next_dspl_descr;
            /***************************************************************
            *  Give the message to each of the remaining windows.
            ***************************************************************/
            walk_dspl = next_dspl_descr;
            do
            {
               walk_dspl->main_pad->impacted_redraw_mask |= TITLEBAR_MASK & FULL_REDRAW;
               dm_error_dspl("", DM_ERROR_MSG, walk_dspl);
               dm_error_dspl("CC window closed", DM_ERROR_BEEP, walk_dspl);
               walk_dspl = walk_dspl->next;
            } while(walk_dspl != next_dspl_descr);
         }
   }

return(redraw_needed);

}  /* end of dm_wc */



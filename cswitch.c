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
*  module cswitch.c
*
*  Routine:
*     cswitch                 - Main command switch
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <errno.h>          /* /usr/include/errno.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <limits.h>         /* /usr/include/limits.h     */
#include <signal.h>         /* /usr/include/signal.h    /usr/include/sys/signal.h    */
#include <sys/types.h>      /* /usr/include/sys/types.h  */
#include <time.h>           /* /usr/include/time.h       */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "alias.h"
#include "bl.h"
#include "ca.h"
#include "cc.h"
#include "color.h"
#include "cswitch.h"
#include "debug.h"
#include "dmc.h"
#include "dmfind.h"
#include "dmwin.h"
#include "emalloc.h"   /* needed for malloc copy, causes a lint error */
#include "execute.h"
#include "getevent.h"
#include "init.h"
#include "ind.h"
#include "kd.h"
#include "label.h"
#include "lineno.h"
#include "mark.h"
#include "mouse.h"
#include "memdata.h"
#include "mvcursor.h"
#include "netlist.h"
#include "pad.h"
#include "parms.h"
#include "pd.h"
#include "prompt.h"
#include "pw.h"
#include "record.h"
#include "redraw.h"
#include "reload.h"
#include "serverdef.h"
#include "tab.h"
#include "textflow.h"
#include "txcursor.h"
#include "typing.h"
#include "undo.h"
#include "usleep.h"
#ifdef PAD
#include "unixpad.h"
#include "unixwin.h"
#include "vt100.h"
#endif
#include "wc.h"
#include "wdf.h"
#include "ww.h"
#include "window.h"
#include "windowdefs.h"
#include "xc.h"
#include "xerror.h"
#include "xerrorpos.h"


#ifdef DebuG
static void dm_debug(DMC               *dmc,
                     DISPLAY_DESCR     *dspl_descr);

#ifdef WIN32
#ifdef WIN32_XNT
#include "xnt.h"
#else
#include "X11/X.h"
#include "X11/Xutil.h"
#endif
#include <stdlib.h>
#else
#include <X11/X.h>
#include <X11/Xutil.h>
#endif

static DMC   *dm_cntlc(DMC               *dmc,
                       DISPLAY_DESCR     *dspl_descr);

static void dump_window_attrs(Display     *display,
                              Window       window);
#endif

#if !defined(FREEBSD) && !defined(WIN32) && !defined(_OSF_SOURCE) && !defined(OMVS)
int   putenv(char *name);
#endif

/************************************************************************

NAME:      cswitch  -  Main command switch


PURPOSE:    This routine does the switch on the passed DM command
            structure and calls the appropriate routines.

PARAMETERS:

   1.  dspl_descr      - pointer to DISPLAY_DESCR  (INPUT/OUTPUT)
                         This is the current display description.
                            
   2.  dm_list         - pointer to DMC  (INPUT)
                         This is the DM command to execute
                            
   3.  new_dmc         - pointer to pointer to DMC  (OUTPUT)
                         If any DM command structures are generated, the head
                         of the list of these is put in this output variable.
                            
   4.  event_union     - pointer to XEvent (INPUT)
                         This is the event which caused the DM commands to get
                         executed.  It the event structure for a:
                         KeyPress, KeyRelease, ButtonPress, ButtonRelease

   5.  read_locked     - int  (INPUT)
                         Read locked flag.  Prevents "ro" commands if True.
                            
   6.  warp_needed     - pointer to int (OUTPUT)
                         True if the cursor needs to be warped as a result
                         of the DM command.

   7.  wc_display_deleted     - pointer to pointer to DISPLAY_DESCR (OUTPUT)
                         This flag tells the caller the display was actually closed down.
                         It is used in when there are multiple CC windows.
                         If non-NULL, it is the display description of the next display
                         after the one which has just been destroyed.  When this value
                         is non-NULL, do not use parameter dspl_descr any more.
                         non-NULL -  A cc window was closed, value is the next
                                     cc window in the list.
                         NULL      - No windows deleted, maybe a prompt was issued

   8.  stop_dm_list     - pointer to int (OUTPUT)
                         True causes the list of DM commands to stop being processed.
                         Normally caused by prompts or finds which failed. Differs
                         from wc_display_deleted, in that normal post DM command calls
                         to update the display should be made.

   9.  arg0            - pointer to char (INPUT)
                         The command name, passed to lower level routines.

GLOBAL INPUT DATA:

   1.  something_to_do_in_background -  int (INPUT)
               This flag shows that there is something to do in background.

FUNCTIONS :

   1.   Switch on the command passed and call the appropritate 
        routine.

RETURNS:
   redraw -  If the screen needs to be redrawn, the mask of which screen
             and what type of redrawing is returned.

*************************************************************************/

int  cswitch(DISPLAY_DESCR  *dspl_descr,         /* in/out */
             DMC            *dm_list,            /* input  */
             DMC           **new_dmc,            /* output */
             XEvent         *event_union,        /* input  KeyPress, KeyRelease, ButtonPress, ButtonRelease */
             int             read_locked,        /* input  */
             int            *warp_needed,        /* output */
             DISPLAY_DESCR **wc_display_deleted, /* output */
             int            *stop_dm_list,       /* output */
             char           *arg0)               /* input  */
{
int                   redraw_needed = 0;  /* bitmask flag to tell when window redraw is needed and also what kind  */                

int                   from_line;        /* local use variable for displaying the current cursor position */
int                   from_col;         /*     "                         "                       "       */
PAD_DESCR            *from_buffer;      /*     "                         "                       "       */

char                 *p;                /* localized use scrap variables                                           */
int                   i;
int                   changed;
unsigned int          ui;
char                  msg[MAX_LINE];
int                   window_echo;      /* Boolean work variable used when processing cursor motion events - shows the current window has text highlighting */
DISPLAY_DESCR        *tmp_dspl_descr;

*new_dmc            = NULL;  /* no commands initially generated */
*wc_display_deleted = NULL;  /* initialize output flag */
*stop_dm_list       = False; /* initialize output flag */

/***************************************************************
*  Switch on the command id to decide what to do.
***************************************************************/
switch(dm_list->any.cmd)
{

/***************************************************************
*
*  abrt - (Abort) Turn off echo mode
*
***************************************************************/
case DM_abrt:
case DM_sq:
   clear_prompt_stack(dspl_descr); /* RES 8/3/95 Clear any outstanding prompts (enhancement) */
   set_dm_prompt(dspl_descr, NULL);
   redraw_needed = DMINPUT_MASK & FULL_REDRAW;

   if (dspl_descr->echo_mode)
      {
         stop_text_highlight(dspl_descr);
         dm_error_dspl("Echo mode aborted", DM_ERROR_MSG, dspl_descr);
         dspl_descr->echo_mode = NO_HIGHLIGHT;
         dspl_descr->mark1.mark_set = False;
         *warp_needed = True;  /* force cursor change */
      }
   if (get_background_work(BACKGROUND_FIND))
      {
         change_background_work(dspl_descr, BACKGROUND_FIND, False);
         dm_error_dspl("Find aborted", DM_ERROR_BEEP, dspl_descr);
         *warp_needed = True;  /* force cursor change */
      }
   if (get_background_work(BACKGROUND_SUB))
      {
         change_background_work(dspl_descr, BACKGROUND_SUB, False);
         dm_error_dspl("Substitute aborted", DM_ERROR_BEEP, dspl_descr);
         *warp_needed = True;  /* force cursor change */
      }
   if (dspl_descr->pd_data->pulldown_active)
      pd_remove(dspl_descr, 0);
   break;

/***************************************************************
*
*  ad - (Arrow Down) Go down one character
*  In mvcursor.c
*
***************************************************************/
case DM_ad:
   dm_ad(dspl_descr->cursor_buff,
         dspl_descr->dmoutput_pad,
         dspl_descr->dminput_pad,
         (dspl_descr->pad_mode ? dspl_descr->unix_pad : NULL));
   *warp_needed = True;
   break;

/***************************************************************
*
*  al - (Arrow Left) Go left one character
*  In mvcursor.c
*
***************************************************************/
case DM_al:
   redraw_needed |= dm_al(dspl_descr->cursor_buff, dspl_descr->dminput_pad, dm_list->al.dash_w);
   *warp_needed = True;
   break;

/***************************************************************
*
*  alias - Assign an alias to a string of commands
*  In alias.c
*
***************************************************************/
case DM_alias:
   dm_alias(dm_list, True/* update server*/, dspl_descr);
   break;

/***************************************************************
*
*  ar - (Arrow Right) Go right one character
*  In mvcursor.c
*
***************************************************************/
case DM_ar:
   redraw_needed |= dm_ar(dspl_descr->cursor_buff, dspl_descr->dmoutput_pad, dm_list->ar.dash_w);
   *warp_needed = True;
   break;

/***************************************************************
*
*  au - (Arrow Up) Go up one character
*  In mvcursor.c
*
***************************************************************/
case DM_au:
   dm_au(dspl_descr->cursor_buff,
         dspl_descr->main_pad,
         dspl_descr->dmoutput_pad,
         (dspl_descr->pad_mode ? dspl_descr->unix_pad : NULL));
   *warp_needed = True;
   break;

/***************************************************************
*
*  ! - Bang (! cut region into stdin of cmd paste resulting stdout)
*  In exeute.c
*
***************************************************************/
case DM_bang:
   *new_dmc = dm_bang(dm_list, dspl_descr);
   redraw_needed |= (dspl_descr->cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW);
   *warp_needed = True;  /* force cursor change */
   break;

/***************************************************************
*
*  bell - Make terminal beep
*
***************************************************************/
case DM_bell:
      i = dm_list->bell.chars;
      if (i > 100) i = 100;
      if (i < -100) i = -100;
      ce_XBell(dspl_descr, i);
   break;

/***************************************************************
*
*  bl - Balance 
*  In bl.c
*
***************************************************************/
case DM_bl:
   redraw_needed |= dm_bl(dspl_descr->cursor_buff, dm_list, dspl_descr->case_sensitive);
   *warp_needed = True;  /* cursor always moves */
   break;

/***************************************************************
*
*  bgc - Background color
*  fgc - Foreground color
*  In color.c
*
***************************************************************/
case DM_bgc:
case DM_fgc:
   dm_bgc(dm_list, dspl_descr);
   redraw_needed = ((MAIN_PAD_MASK | DMINPUT_MASK | DMOUTPUT_MASK | TITLEBAR_MASK | UNIXCMD_MASK) & FULL_REDRAW);
   break;

/***************************************************************
*
*  ca - Color Area
*  In ca.c
*
***************************************************************/
case DM_ca:
   redraw_needed |= dm_ca(dm_list, dspl_descr, &(dspl_descr->mark1), dspl_descr->cursor_buff, dspl_descr->echo_mode);
   dspl_descr->echo_mode = NO_HIGHLIGHT;
   *warp_needed = True;  /* force cursor change */
   break;

/***************************************************************
*
*  caps - toggle in and out of caps mode
*  toggle_switch in mark.c
*
***************************************************************/
case DM_caps:
   dspl_descr->caps_mode = toggle_switch(dm_list->caps.mode, 
                                         dspl_descr->caps_mode,
                                         &i); /* junk parm */
   break;

/***************************************************************
*
*  case - Change case of a range of text
*  In xc.c
*
***************************************************************/
case DM_case:
   redraw_needed |= dm_case(dm_list, &(dspl_descr->mark1), dspl_descr->cursor_buff, dspl_descr->echo_mode);
   dspl_descr->echo_mode = NO_HIGHLIGHT;
   *warp_needed = True;  /* force cursor change */
   break;

/***************************************************************
*
*  cc - (carbon copy)  cc the current window.
*  In cc.c
*
***************************************************************/
case DM_cc:
   i = dm_cc(dm_list, dspl_descr, edit_file, resource_class, arg0);
   break;

/***************************************************************
*
*  cd - Change working dir of edit process
*  In pw.c
*
***************************************************************/
case DM_cd:
   dm_cd(dm_list, edit_file);
   break;

/***************************************************************
*
*  ce - (create edit)  edit a new file.
*  Note: This routine may return twice.  Once in the parent process
*        and once in a child process it forks and sets up with the
*        new file.  In the child process we must go back and start
*        over.
*  In execute.c
*
***************************************************************/
case DM_ce:
case DM_cv:
case DM_ceterm:
case DM_cp:
   i = dm_ce(dm_list, dspl_descr);
   if (i < 0)  /* no child created if i < 0 */
      *warp_needed = True;
   else
      *warp_needed = False; /* in the parent, override the warp to give the child a chance */
   break;

/***************************************************************
*
*  comma - define region for a xc, xd, s, so, etc
*  In mark.c
*
***************************************************************/
case DM_comma:
   dm_comma(&(dspl_descr->mark1), dspl_descr->cursor_buff, &(dspl_descr->comma_mark));
   *warp_needed = True;
   break;

/***************************************************************
*
*  cmdf - (Cmd File) load and parse a file of DM commands.
*         If they were good, the list of parsed cmds is in new_dmc.
*  In kd.c
*
***************************************************************/
case DM_cmdf:
   *new_dmc = dm_cmdf(dm_list,
                     dspl_descr->display,
                     dspl_descr->main_pad->x_window,
                     dspl_descr->escape_char,
                     dspl_descr->hsearch_data,
                     True /* prompts allowed */);
   /* event_do added 2/25/97 RES, to break up multiple playbacks after record */
   event_do(dspl_descr->main_pad->token, KS_EVENT, 0, 0, 0, 0);
   *warp_needed = True;
   break;

/***************************************************************
*
*  cms - (Clear Mark Stack) Clear any marks, turn off echoing if
*        on. 
*
***************************************************************/
case DM_cms:
   stop_text_highlight(dspl_descr);
   dspl_descr->mark1.mark_set = False;
   dspl_descr->echo_mode = NO_HIGHLIGHT;
   *warp_needed = True;  /* force cursor change */
   break;

/***************************************************************
*
*  cntlc - (Control C special) If ceterm and nothing marked and
*         cursor in UNIX window, do a dq, otherwise xc. 
*
***************************************************************/
case DM_cntlc:
   *new_dmc = dm_cntlc(dm_list, dspl_descr); /* in cswitch.c (this file) */
   break;

/***************************************************************
*
*  corner - {row,col} Make row and col the upper left corner
*  In mvcursor.c
*
***************************************************************/
case DM_corner:
   redraw_needed |= dm_corner(dm_list, dspl_descr->cursor_buff);
   dspl_descr->tdm_mark.mark_set = False;
   *warp_needed = True;
   break;

/***************************************************************
*
*  cpo - (Create Process Only) start a child process executing
*        a command (not a shell script)
*  cps - (Create Process Server) start a child process executing
*        a command (not a shell script), child does a no-hup
*  In execute.c
*
***************************************************************/
case DM_cpo:
case DM_cps:
   dm_cpo(dm_list, dspl_descr);
   redraw_needed |= DMOUTPUT_MASK & FULL_REDRAW;
   break;

#ifdef Encrypt
/***************************************************************
*
*  crypt - (set encrypt key) 
*  In crpad.c
*
***************************************************************/
case DM_crypt:
   dm_crypt(dm_list);
      *warp_needed = True;
   break;
#endif
/***************************************************************
*
*  delsd -  delete X server copy of key definitions
*  In serverdef.c
*
***************************************************************/
case DM_delsd:
   dm_delsd(dspl_descr->display, KEYDEF_PROP, dspl_descr->properties);
   break;

/***************************************************************
*
*  debug -  Debugging aid
*  In crpad.c
*
***************************************************************/
case DM_debug:
#ifdef DebuG
   dm_debug(dm_list, dspl_descr);
   DEBUG9(XERRORPOS)
   DEBUG9( (void)XSynchronize(dspl_descr->display, True);)
   DEBUG9(XERRORPOS)
   DEBUG0( if (!debug)  (void)XSynchronize(dspl_descr->display, False);)
#else
   dm_error_dspl("Ce not compiled with Debug enabled", DM_ERROR_BEEP, dspl_descr);
#endif
   break;

/***************************************************************
*
*  dlb - delete labels
*  In label.c
*
***************************************************************/
case DM_dlb:
   redraw_needed |= dm_dlb(&(dspl_descr->mark1),
                           dspl_descr->cursor_buff,
                           dspl_descr->echo_mode,
                           dspl_descr->show_lineno);
   break;

/***************************************************************
*
*  dollar  - ($) Go to the bottom of file.
*
***************************************************************/
case DM_dollar:
   /***************************************************************
   *  Go to the bottom of the file. First make sure the file is
   *  read in, then position to the last page.
   ***************************************************************/
   load_enough_data(INT_MAX);
   redraw_needed |= dm_pb(dspl_descr->cursor_buff);
   (void) dm_tb(dspl_descr->cursor_buff);
   redraw_needed |= dm_tr(dspl_descr->cursor_buff);
   dspl_descr->cursor_buff->current_win_buff->file_col_no++; /* include the last column */
   dspl_descr->cursor_buff->win_col_no++; 
   *warp_needed = True;
   break;

/***************************************************************
*
*  dq - (quit) send a signal to the shell
*       in unixpad.c
*
***************************************************************/
case DM_dq:
#ifdef PAD
   if (dspl_descr->pad_mode)
      {
         flush(dspl_descr->main_pad);
         redraw_needed |= dm_dq(dm_list, dspl_descr);
         *warp_needed = True;
      }
   else
      redraw_needed |= dm_dq(dm_list, dspl_descr);
#else
   redraw_needed |= dm_dq(dm_list, dspl_descr);
#endif
   break;

/***************************************************************
*
*  dr - define region
*  In mark.c
*
***************************************************************/
case DM_dr:
#ifdef PAD
/***************************************************************
*  In vt100 mode we have not been following the cursor.
*  We will take position from the current key position.
*  Note, this code make use of the fact that the motion event,
*  keypress event, and buttonpress event all map the same with
*  respect to the x and y coordinates.
***************************************************************/

if (dspl_descr->vt100_mode && (dspl_descr->cursor_buff->which_window == MAIN_PAD))
   {
      if (dspl_descr->echo_mode && dspl_descr->mark1.buffer == dspl_descr->cursor_buff->current_win_buff)
         window_echo = True;
      else
         window_echo = False;
      event_union->type = MotionNotify;
      text_cursor_motion(dspl_descr->main_pad->window,              /* input  */
                         window_echo,                               /* input  */
                         event_union,                               /* input  */
                         &(dspl_descr->cursor_buff->win_line_no),   /* output */
                         &(dspl_descr->cursor_buff->win_col_no),    /* output */
                         dspl_descr->main_pad->win_lines,           /* input  */
                         dspl_descr);                               /* input  */
      dspl_descr->cursor_buff->current_win_buff->file_line_no = dspl_descr->cursor_buff->win_line_no + dspl_descr->cursor_buff->current_win_buff->first_line;
      dspl_descr->cursor_buff->current_win_buff->file_col_no  = dspl_descr->cursor_buff->win_col_no  + dspl_descr->cursor_buff->current_win_buff->first_char;
   }
#endif
   dm_dr(&(dspl_descr->mark1), dspl_descr->cursor_buff, dspl_descr->echo_mode);
   break;

/***************************************************************
*
*  echo - swap the forground and background colors.
*
***************************************************************/
case DM_echo:
   if (dspl_descr->mark1.mark_set)
      {
         if (dm_list->echo.dash_r)
            {
               dspl_descr->echo_mode = RECTANGULAR_HIGHLIGHT;
               *warp_needed = True; /* needed to trigger initial highlight of the line */
            }
         else
            dspl_descr->echo_mode = REGULAR_HIGHLIGHT;
         highlight_setup(&(dspl_descr->mark1), dspl_descr->echo_mode);
      }
   break;

/***************************************************************
*
*  ed - Delete the character under the cursor
*  In typing.c
*
***************************************************************/
case DM_ed:
   redraw_needed |= dm_ed(dspl_descr->cursor_buff);
   *warp_needed = True;
   break;

/***************************************************************
*
*  ee - Delete the character to the left of the cursor and move
*       left one character (backspace).
*  In typing.c
*
***************************************************************/
case DM_ee:
   redraw_needed |= dm_ee(dspl_descr->cursor_buff, dm_list->ee.dash_w);
   if (cc_ce && (dspl_descr->cursor_buff->which_window == MAIN_PAD))
      cc_typing_chk(dspl_descr->cursor_buff->current_win_buff->token, dspl_descr, dspl_descr->cursor_buff->current_win_buff->file_line_no, dspl_descr->cursor_buff->current_win_buff->buff_ptr);
   *warp_needed = True;
   break;

#ifdef PAD
/***************************************************************
*
*  eef - Send eof to the shell in pad mode.
*  In unixpad.c
*
***************************************************************/
case DM_eef:
   if (dspl_descr->pad_mode)
      {
         redraw_needed |= dm_eef(dm_list, dspl_descr->cursor_buff);
         *warp_needed = True;
      }
   break;
#endif

/***************************************************************
*
*  ei -  Change insert mode on a window
*  In toggle_switch in mark.c
*
***************************************************************/
case DM_ei:
   dspl_descr->insert_mode = toggle_switch(dm_list->ei.mode, 
                                           dspl_descr->insert_mode,
                                           &i); /* junk parm */
   redraw_needed |= (FULL_REDRAW & TITLEBAR_MASK);
   break;

/***************************************************************
*
*  en (Enter) An enter was pressed. If this is the command window
*  execute the command.
*  In typing.c
*
***************************************************************/
case DM_en:
   redraw_needed |=  dm_en(dm_list, dspl_descr, new_dmc);
   *warp_needed = True;
   break;

/***************************************************************
*
*  = - (Equal) Display current x, y, row, col
*
***************************************************************/
case DM_equal:
   mark_point_setup(NULL,                    /* input  */
                    dspl_descr->cursor_buff, /* input  */
                    &from_line,              /* output  */
                    &from_col,               /* output  */
                    &from_buffer);           /* output  */
   snprintf(msg, sizeof(msg), "Line: %d, Column: %d, X: %d, Y: %d,  Root X: %d,  Root Y: %d corner [%d,%d]",
                 from_line + 1, from_col + 1,
                 dspl_descr->cursor_buff->x, dspl_descr->cursor_buff->y,
                 dspl_descr->cursor_buff->root_x, dspl_descr->cursor_buff->root_y,
                 from_buffer->first_line+1, from_buffer->first_char+1);
   dm_error_dspl(msg, DM_ERROR_MSG, dspl_descr);
   break;

/***************************************************************
*
*  env - Environment variable set/show
*  In execute.c
*
***************************************************************/
case DM_env:
   dm_env(dm_list->env.parm);
   break;

/***************************************************************
*
*  envar - toggle environment var mode for clicking on files
*  toggle_switch in mark.c
*
***************************************************************/
case DM_envar:
   toggle_switch(dm_list->envar.mode, ENV_VARS, &changed);
   if (changed) /* if changed */
      {
         if (ENV_VARS)
            {
               if (OPTION_VALUES[ENVAR_IDX])
                  free(OPTION_VALUES[ENVAR_IDX]);
               OPTION_VALUES[ENVAR_IDX] = malloc_copy("no");
            }
         else
            {
               if (OPTION_VALUES[ENVAR_IDX])
                  free(OPTION_VALUES[ENVAR_IDX]);
               OPTION_VALUES[ENVAR_IDX] = malloc_copy("yes");
            }
      }
   break;

/***************************************************************
*
*  er - (Enter Raw) Enter one character
*  es - (Enter String) Enter typed characters
*       Right now, the only input window is the dm input window,
*  In typing.c
*
***************************************************************/
case DM_er:
   if (dm_list->er.chars[0] == 0x0A)
      {
         /* treat er of newline as an enter (copied from DM_en) */
         redraw_needed |=  dm_en(dm_list, dspl_descr, new_dmc);
         *warp_needed = True;
         break;
      }
   /* FALLTHRU */

case DM_es:
   redraw_needed |= dm_es(dspl_descr->cursor_buff, dm_list);
   if (cc_ce && (dspl_descr->cursor_buff->which_window == MAIN_PAD))
      cc_typing_chk(dspl_descr->cursor_buff->current_win_buff->token, dspl_descr, dspl_descr->cursor_buff->current_win_buff->file_line_no, dspl_descr->cursor_buff->current_win_buff->buff_ptr);
#ifdef PAD
   if (!(dspl_descr->vt100_mode) || (dspl_descr->cursor_buff->which_window != MAIN_PAD))
      *warp_needed = True;
#else
   *warp_needed = True;
#endif
   break;

/***************************************************************
*
*  eval - Perform symbolic substitution and parse a line
*  in kd.c
*
***************************************************************/
case DM_eval:
   *new_dmc = dm_eval(dm_list, dspl_descr);
   break;

/***************************************************************
*
*  fbdr adjust the find border
*
***************************************************************/
case DM_fbdr:
   if (dm_list->fbdr.chars >= 0)
      dspl_descr->find_border = dm_list->fbdr.chars;
   else
      {
         snprintf(msg, sizeof(msg), "%d", dspl_descr->find_border);
         dm_error_dspl(msg, DM_ERROR_MSG, dspl_descr);
      }
   break;

/***************************************************************
*
*  find and rfind - do a find forward or reverse
*  In dmfind.c
*
***************************************************************/
case DM_find:
case DM_rfind:
   if (get_background_work(BACKGROUND_FIND_OR_SUB))
      {
         dm_error_dspl("Operation illegal during search or substitute", DM_ERROR_BEEP, dspl_descr);
         break;
      }
   redraw_needed |=  dm_find(dm_list,
                             dspl_descr->find_data,
                             dspl_descr->cursor_buff,
                             dspl_descr->case_sensitive,
                             stop_dm_list,
                             dspl_descr->escape_char);
   if(!get_background_work(BACKGROUND_FIND))
      *warp_needed = True;
   /* continue if next command is a colon (null command) RES 10/13/95 */
   if (*stop_dm_list && dm_list->next && (dm_list->next->any.cmd == DM_colon))
      *stop_dm_list = False;
   break;

/***************************************************************
*
*  fl - load a new font
*  in color.c
*
***************************************************************/
case DM_fl:
   dm_fl(dm_list, dspl_descr);
   *warp_needed = True;
   redraw_needed = DMINPUT_MASK & FULL_REDRAW; /* kill main window pending redraws, lineno reconfigures the screen */
   break;

/***************************************************************
*
*  geo - change window geomentry using Xlib geometry string (NEW COMMAND)
*  in window.c
*
***************************************************************/
case DM_geo:
   if (dm_geo(dm_list, dspl_descr))
      dspl_descr->redo_cursor_in_configure = REDRAW_FORCE_REAL_WARP;
   set_window_col_from_file_col(dspl_descr->cursor_buff);
   *warp_needed = True;
   break;

/***************************************************************
*
*  glb - go to label
*  In label.c
*
***************************************************************/
case DM_glb:
   *new_dmc = dm_glb(dm_list, dspl_descr->cursor_buff, dspl_descr->main_pad);
   break;

/***************************************************************
*
*  gm - go to mark
*  In mark.c
*
***************************************************************/
case DM_gm:
   redraw_needed |=  dm_gm(&(dspl_descr->mark1), dspl_descr->cursor_buff);
   dspl_descr->cursor_buff->up_to_snuff = 0;
   *warp_needed = True;
   break;

/***************************************************************
*
*  hex - (Toggle display mode to show non-ascii chars) 
*  In toggle_switch in mark.c
*
***************************************************************/
case DM_hex:
   dspl_descr->hex_mode = toggle_switch(dm_list->hex.mode, dspl_descr->hex_mode, &i); /* i is a junk parm */
   redraw_needed = FULL_REDRAW;
   *warp_needed = True;
   break;

/***************************************************************
*
*  icon - Iconify the window
*  in window.c
*
***************************************************************/
case DM_icon:
   dm_icon(dm_list, dspl_descr->display, dspl_descr->main_pad->x_window);
   break;

/***************************************************************
*
*  ind - language sensitive indent functionality
*  in ind.c
*
***************************************************************/
case DM_ind:
   if (dspl_descr->ind_data)
      *new_dmc = dm_ind(dspl_descr);
   break;

/***************************************************************
*
*  inv - swap the forground and background colors.
*        Then force a full redraw.
*  in gc.c
*
***************************************************************/
case DM_inv:
   dm_inv(dspl_descr);
   redraw_needed = ((MAIN_PAD_MASK | DMINPUT_MASK | DMOUTPUT_MASK | TITLEBAR_MASK | UNIXCMD_MASK) & FULL_REDRAW);
   *warp_needed = True;
   break;

/***************************************************************
*
*  kd - (Key Definition) Execute a Key defintion command.
*  ld - (local definition)
*       If we need new events as a result, get them.
*  In kd.c
*
***************************************************************/
case DM_kd:
case DM_lkd:
   dm_kd(dm_list, True /* update server copy */, dspl_descr);
   set_event_masks(dspl_descr, False); /* False means change masks only if needed */
   break;

/***************************************************************
*
*  keys - (display key definitions in a separate window)
*  In serverdef.c
*
***************************************************************/
case DM_keys:
   *new_dmc = dm_keys(dspl_descr->hsearch_data, dspl_descr->escape_char);
   break;

/***************************************************************
*
*  kk - Special added command,  set flag so next keypress will
*       print key name in dm window
*  In typing.c
*
***************************************************************/
case DM_kk:
   (dspl_descr->dm_kk_flag)++;
   break;

/***************************************************************
*
*  lbl - Define or delete a label
*  In label.c
*
***************************************************************/
case DM_lbl:
   redraw_needed = dm_lbl(dm_list, dspl_descr->cursor_buff, dspl_descr->show_lineno, False);
   break;

/***************************************************************
*
*  lineno -  New command, turn on and off line number display.
*  In lineno.c
*
***************************************************************/
case DM_lineno:
   if (dm_lineno(dm_list,
                 dspl_descr->main_pad,
                 dspl_descr->display,
                 &(dspl_descr->show_lineno)))
      dspl_descr->redo_cursor_in_configure = REDRAW_FORCE_REAL_WARP;
   *warp_needed = True;
   redraw_needed = DMINPUT_MASK & FULL_REDRAW; /* kill main window pending redraws, lineno reconfigures the screen */
   break;

/***************************************************************
*
*  lsf - (Language Sensitive Filter) Define an LSF
*  In kd.c
*
***************************************************************/
case DM_lsf:
   dm_lsf(dm_list, True /* update server copy */, dspl_descr);
   break;

/***************************************************************
*
*  markc - [row,col] Go to that line and col in the main window.
*  In mvcursor.c
*
***************************************************************/
case DM_markc:
   redraw_needed |= dm_num(dm_list, dspl_descr->cursor_buff, dspl_descr->find_border);
   dspl_descr->tdm_mark.mark_set = False;
   *warp_needed = True;
   break;

/***************************************************************
*
*  markp - (x, y) Go to a spot in the root window for a window grow.
*
***************************************************************/
case DM_markp:
   dspl_descr->root_mark.mark_set = True;
   dspl_descr->root_mark.file_line_no = dm_list->markp.y;
   dspl_descr->root_mark.col_no = dm_list->markp.x;
   dspl_descr->root_mark.buffer = NULL;
   if (dm_list->next == NULL)
      {
         if (dm_list->markp.x >= dspl_descr->main_pad->window->x &&
             dm_list->markp.x <  dspl_descr->main_pad->window->x + (int)dspl_descr->main_pad->window->width &&
             dm_list->markp.y >= dspl_descr->main_pad->window->y &&
             dm_list->markp.y <  dspl_descr->main_pad->window->y + (int)dspl_descr->main_pad->window->height)
            {
               dspl_descr->cursor_buff->x = dm_list->markp.x - dspl_descr->main_pad->window->x;
               dspl_descr->cursor_buff->y = dm_list->markp.y - dspl_descr->main_pad->window->y;
               dspl_descr->cursor_buff->win_line_no = (dspl_descr->cursor_buff->y - dspl_descr->main_pad->window->sub_y) / dspl_descr->main_pad->window->line_height;
               *warp_needed = True;
            }
         else
            {
               *warp_needed = False;
               DEBUG9(XERRORPOS)
               XWarpPointer(dspl_descr->display, None, RootWindow(dspl_descr->display, DefaultScreen(dspl_descr->display)),
                            0, 0, 0, 0,
                            dm_list->markp.x, dm_list->markp.y);
            }
      }

   break;

/***************************************************************
*
*  mi   - (menu item) Install a menu item in a pulldown
*  In pd.c
*
***************************************************************/
case DM_mi:
case DM_lmi:
   dm_mi(dm_list, (dm_list->any.cmd == DM_mi) /* update server copy */, dspl_descr);
   break;

/***************************************************************
*
*  mouse - toggle mouse mode
*  toggle_switch in mark.c
*
***************************************************************/
case DM_mouse:
   toggle_switch(dm_list->mouse.mode, MOUSEON, &changed);
   if (changed) /* if changed */
      {
         redraw_needed |= (FULL_REDRAW & TITLEBAR_MASK);
         if (MOUSEON)
            {
               if (OPTION_VALUES[MOUSE_IDX])
                  free(OPTION_VALUES[MOUSE_IDX]);
               OPTION_VALUES[MOUSE_IDX] = malloc_copy("off");
            }
         else
            {
               if (OPTION_VALUES[MOUSE_IDX])
                  free(OPTION_VALUES[MOUSE_IDX]);
               OPTION_VALUES[MOUSE_IDX] = malloc_copy("on");
            }
         set_event_masks(dspl_descr, True); /* True means force resetting of masks */
         normal_mcursor(dspl_descr);
      }
   break;

/***************************************************************
*
* msg - output a dm message
*
***************************************************************/
case DM_msg:
   if (dm_list->msg.msg != NULL)
      dm_error_dspl(dm_list->msg.msg, DM_ERROR_MSG, dspl_descr);
   break;

/***************************************************************
*
*  nc - (Next Color)      Position to the next colored area
*  pc - (Previous Color)  Position to the previous colored area
*  In ca.c
*
***************************************************************/
case DM_nc:
case DM_pc:
   *new_dmc = dm_nc(dm_list, dspl_descr->cursor_buff);
   break;

/***************************************************************
*
* null - no-op an "un"-defined key
*
***************************************************************/
case DM_NULL:
case DM_colon:
   break;

/***************************************************************
*
*  num   - (number entered) Go to that line number in the main window.
*  In mvcursor.c
*
***************************************************************/
case DM_num:
   redraw_needed |= dm_num(dm_list, dspl_descr->cursor_buff, dspl_descr->find_border);
   dspl_descr->tdm_mark.mark_set = False;
   *warp_needed = True;
   break;

/***************************************************************
*
*  pb - (Pad Bottom) Make last line in pad the bottom line of the window.
*       Only valid in the main window.
*  In mvcursor.c
*
***************************************************************/
case DM_pb:
   /***************************************************************
   *  Go to the bottom of the file. First make sure the file is
   *  read in, then position to the last page.
   ***************************************************************/
   if (dspl_descr->cursor_buff->which_window == MAIN_PAD)
      load_enough_data(INT_MAX);
   redraw_needed |= dm_pb(dspl_descr->cursor_buff);
#ifdef PAD
   if (dspl_descr->pad_mode && (dspl_descr->cursor_buff->which_window == MAIN_PAD) && dspl_descr->scroll_mode)
      scroll_some(NULL, dspl_descr, 0);
#endif
   *warp_needed  = True;
   break;

/***************************************************************
*
*  pd   - (pull down) Activate a pulldown 
*  In pd.c
*
***************************************************************/
case DM_pd:
   dm_pd(dm_list, event_union, dspl_descr);
   break;

/***************************************************************
*
*  pdm -  Turn the menu bar on and off
*  In toggle_switch in mark.c
*
***************************************************************/
case DM_pdm:
   dm_pdm(dm_list, dspl_descr);
   redraw_needed = 0; /* will redraw whole window during configure */
   break;

/***************************************************************
*
*  ph - (Pad Horizontal) move pad left and right by characters.
*  In mvcursor.c
*
***************************************************************/
case DM_ph:
   redraw_needed |= dm_ph(dspl_descr->cursor_buff, dm_list);
   *warp_needed |= redraw_needed;
   dspl_descr->cursor_buff->up_to_snuff  = 0;
   break;

/***************************************************************
*
*  pn -  Pad Name (name or rename the file)
*  In pw.c
*
***************************************************************/
case DM_pn:
   dm_pn(dm_list, edit_file, dspl_descr);
   if (dm_list->pn.path &&  dm_list->pn.path[0])
      {
         /* RES  4/3/2002, pn to list a name to the dm output window does not change the file name */
         redraw_needed |= (TITLEBAR_MASK & FULL_REDRAW);
      }
   /***************************************************************
   *  Set the icon name from the file name.  Use the last qualifier.
   ***************************************************************/
   p = strrchr(edit_file, '/');
   if (p != NULL)
      p = p+1;
   else
      p = edit_file;
   DEBUG9(XERRORPOS)
   change_icon(dspl_descr->display,
               dspl_descr->main_pad->x_window,
               p,
               dspl_descr->pad_mode);
   break;

/***************************************************************
*
*  pp - (pad page) Scroll vertically by pages or fractions thereof
*       Only valid in the main window.
*
*  pv - (pad vertical) Scroll vertically by pages or fractions thereof
*       Only valid in the main window.
*  In mvcursor.c
*
***************************************************************/
case DM_pp:
case DM_pv:
   redraw_needed |= dm_pv(dspl_descr->cursor_buff, dm_list, warp_needed);
   dspl_descr->cursor_buff->up_to_snuff  = 0;
   break;

#ifdef PAD
/***************************************************************
*
*  prefix - ceterm only command to set prefix to watch for.
*           prefixed lines contain DM commands.
*  In unixwin.c
*
***************************************************************/
case DM_prefix:
   if (dspl_descr->pad_mode)
      dm_prefix(dm_list->prefix.msg);
   break;
#endif

/***************************************************************
*
*  prompt - (internal command), commands with & prompts end up here.
*  In prompt.c
*
***************************************************************/
case DM_prompt:
   redraw_needed |= dm_prompt(dm_list, dspl_descr);
   *stop_dm_list = True;
   if (redraw_needed)
      *warp_needed = True;
   break;

/***************************************************************
*
*  pt - (Pad Top) Make line zero the first line in the window.
*       Only valid in the main window.
*  In mvcursor.c
*
***************************************************************/
case DM_pt:
   redraw_needed |= dm_pt(dspl_descr->cursor_buff);
   *warp_needed |= redraw_needed;
   break;

/***************************************************************
*
*  pw -  Pad Write  Save the file.
*  In pw.c
*
***************************************************************/
case DM_pw:
#ifdef PAD
   tmp_dspl_descr = NULL;
   if (WRITABLE(dspl_descr->main_pad->token) || (tmp_dspl_descr = find_padmode_dspl(dspl_descr)))
      {
         if (tmp_dspl_descr && !tmp_dspl_descr->edit_file_changed)
            {
               dm_error_dspl("Pad has not been named, use 'pn' first", DM_ERROR_BEEP, dspl_descr);
               break;
            }
         (void) dm_pw(dspl_descr, edit_file, False);
         redraw_needed |= (TITLEBAR_MASK & FULL_REDRAW);
      }
#else
   if (WRITABLE(dspl_descr->main_pad->token))
      {
         (void) dm_pw(dspl_descr, edit_file, False);
         redraw_needed |= (TITLEBAR_MASK & FULL_REDRAW);
      }
#endif
   break;

/***************************************************************
*
*  pwd -  put current working directory in the DM output window.
*  In pw.c
*
***************************************************************/
case DM_pwd:
   dm_pwd();
   break;

/***************************************************************
*
*  reload - reload the file from disk
*  In reload.c
*
***************************************************************/
case DM_reload:
   redraw_needed |=  dm_reload(dm_list, dspl_descr, edit_file);
   dspl_descr->cursor_buff->up_to_snuff = 0;
   break;

/***************************************************************
*
*  rl - returm to location set by sl (save location)
*  In mark.c
*
***************************************************************/
case DM_rl:
   redraw_needed |=  dm_gm(&(dspl_descr->mark2), dspl_descr->cursor_buff);
   dspl_descr->cursor_buff->up_to_snuff = 0;
   *warp_needed = True;
   break;

/***************************************************************
*
*  rd - Immediate redraw
*  In file redraw.c
*
***************************************************************/
case DM_rd:
   process_redraw(dspl_descr, FULL_REDRAW, False);
   break;

/***************************************************************
*
*  re - translate regular expression
*  In file search.c
*
***************************************************************/
case DM_re:
   dm_re(dm_list, dspl_descr->escape_char);
   break;

/***************************************************************
*
*  rce - Turn record on and off
*  In file record.c
*
***************************************************************/
case DM_rec:
   dm_rec(dm_list, dspl_descr);
   redraw_needed |= (TITLEBAR_MASK & FULL_REDRAW);
   break;

/***************************************************************
*
*  rm - replace mark (turn the mark back on)
*
***************************************************************/
case DM_rm:
   dspl_descr->mark1.mark_set = True;
   redraw_needed = ((MAIN_PAD_MASK | DMINPUT_MASK | DMOUTPUT_MASK | TITLEBAR_MASK | UNIXCMD_MASK) & FULL_REDRAW);
   break;

/***************************************************************
*
*  ro -  Change read / write mode on main window.
*  In pw.c
*
***************************************************************/
case DM_ro:
   dm_ro(dspl_descr, dm_list->ro.mode, read_locked, edit_file);
   redraw_needed |= (TITLEBAR_MASK & FULL_REDRAW);
   break;

/***************************************************************
*
*  rs - refresh screen
*
***************************************************************/
case DM_rs:
   dm_rs(dspl_descr->display);
   break;

/***************************************************************
*
*  rw - refresh window
*
***************************************************************/
case DM_rw:
   redraw_needed = ((MAIN_PAD_MASK | DMINPUT_MASK | DMOUTPUT_MASK | TITLEBAR_MASK | UNIXCMD_MASK) & FULL_REDRAW);
   *warp_needed = True;
   break;

/***************************************************************
*
*  substitute and substitute_once - substitute command
*  In dmfind.c
*
***************************************************************/
case DM_s:
case DM_so:
   if (get_background_work(BACKGROUND_FIND_OR_SUB))
      {
         dm_error_dspl("Operation illegal during search or substitute", DM_ERROR_BEEP, dspl_descr);
         break;
      }
   redraw_needed |=  dm_s(dm_list,
                          dspl_descr->find_data,
                          &(dspl_descr->mark1),
                          dspl_descr->cursor_buff,
                          stop_dm_list,
                          (dspl_descr->echo_mode == RECTANGULAR_HIGHLIGHT),
                          dspl_descr->escape_char);
   stop_text_highlight(dspl_descr);
   dspl_descr->echo_mode = NO_HIGHLIGHT;
   dspl_descr->mark1.mark_set = False;
   if (!get_background_work(BACKGROUND_SUB))
      *warp_needed = True;
   /* continue if next command is a colon (null command) RES 11/10/95 */
   if (*stop_dm_list && dm_list->next && (dm_list->next->any.cmd == DM_colon))
      *stop_dm_list = False;
   break;

/***************************************************************
*
*  sb -  Turn scroll bars on and off.
*  In toggle_switch in mark.c
*
***************************************************************/
case DM_sb:
   if (dspl_descr->sb_data->sb_mode == DM_AUTO && (dm_list->sb.mode == DM_TOGGLE))
      dm_error_dspl("Toggle of scroll bars not allowed when mode is AUTO", DM_ERROR_BEEP, dspl_descr);
   else
      if (dm_list->sb.mode == DM_AUTO)
         {
            if (dspl_descr->sb_data->sb_mode != SCROLL_BAR_AUTO)
               OPTION_FROM_PARM[SB_IDX] = True;
            dspl_descr->sb_data->sb_mode = SCROLL_BAR_AUTO;
         }
      else
         {
            if (toggle_switch(dm_list->sb.mode, 
                              (dspl_descr->sb_data->sb_mode ==  SCROLL_BAR_ON),
                              &i)) /* If i is true, change occurred */
               dspl_descr->sb_data->sb_mode = SCROLL_BAR_ON;
            else
               dspl_descr->sb_data->sb_mode = SCROLL_BAR_OFF;

            redraw_needed |= (MAIN_PAD_MASK & FULL_REDRAW);
            OPTION_FROM_PARM[SB_IDX] = True;
         }
   break;

/***************************************************************
*
*  sc -  Change case sensitivity for searches (find only)
*  In toggle_switch in mark.c
*
***************************************************************/
case DM_sc:
   dspl_descr->case_sensitive = toggle_switch(dm_list->sc.mode, 
                                              dspl_descr->case_sensitive,
                                              &i); /* junk parm */
   break;

/***************************************************************
*
*  sic -  Set insert cursor, for mouse off mode
*  In mvcursor.c
*
***************************************************************/
case DM_sic:
   if (MOUSEON)
      break;
   *warp_needed |= dm_sic(event_union, dspl_descr->cursor_buff, dspl_descr->main_pad, dm_list->sic.dash_w);
   break;

/***************************************************************
*
*  sl - save location (used dm_dr code)
*  In mark.c
*
***************************************************************/
case DM_sl:
   dspl_descr->mark2.file_line_no = dspl_descr->cursor_buff->current_win_buff->file_line_no;
   dspl_descr->mark2.col_no       = dspl_descr->cursor_buff->current_win_buff->file_col_no;
   dspl_descr->mark2.buffer       = dspl_descr->cursor_buff->current_win_buff;
   dspl_descr->mark2.corner_row   = dspl_descr->cursor_buff->current_win_buff->first_line;
   dspl_descr->mark2.corner_col   = dspl_descr->cursor_buff->current_win_buff->first_char;
   break;

/***************************************************************
*
*  sleep - sleep for a float number of seconds
*  In crpad.c
*
***************************************************************/
case DM_sleep:
   ui = (unsigned int)(ABSVALUE(dm_list->sleep.amount) * 1e6);
   usleep(ui);
   break;

#ifdef PAD
/***************************************************************
*
*  sp -  Change case sensitivity for searches (find only)
*  In toggle_switch in mark.c
*
***************************************************************/
case DM_sp:
   if (dspl_descr->pad_mode)
      {
         dm_sp(dspl_descr, dm_list->sp.msg);
         redraw_needed = FULL_REDRAW;
      }
   break;
#endif

/***************************************************************
*
*  tb - (To Bottom) Move cursor to bottom line in window
*       Only valid in the main window.
*  In mvcursor.c
*
***************************************************************/
case DM_tb:
   if (dspl_descr->cursor_buff->which_window != DMINPUT_WINDOW)
      *warp_needed |= dm_tb(dspl_descr->cursor_buff);
   dspl_descr->cursor_buff->up_to_snuff = 0;
   break;

/***************************************************************
*
*  tdm - (To DM) GO To DM window.
*  In mark.c
*
***************************************************************/
case DM_tdm:
   if (dm_tdm(&(dspl_descr->tdm_mark), dspl_descr->cursor_buff, dspl_descr->dminput_pad, dspl_descr->main_pad))
      {
         dspl_descr->redo_cursor_in_configure = REDRAW_FORCE_REAL_WARP;
         *warp_needed = False;
      }
   else
      *warp_needed = True;
   break;

/***************************************************************
*
*  tdmo - (To DM Output) GO To DM output window.
*  In mark.c
*
***************************************************************/
case DM_tdmo:
   if (dm_tdmo(dspl_descr->cursor_buff, dspl_descr->dmoutput_pad, dspl_descr->main_pad))
      {
         dspl_descr->redo_cursor_in_configure = REDRAW_FORCE_REAL_WARP;
         *warp_needed = False;
      }
   else
      {
         *warp_needed = True;
         dspl_descr->cursor_buff->up_to_snuff = 0;
      }
   break;

/***************************************************************
*
*  tf  - (Text Flow)
*  In textflow.c
*
***************************************************************/
case DM_tf:
   redraw_needed |= dm_tf(dm_list, &(dspl_descr->mark1), dspl_descr->cursor_buff, dspl_descr->echo_mode);
   stop_text_highlight(dspl_descr);
   dspl_descr->echo_mode = NO_HIGHLIGHT;
   *warp_needed = True;
   break;

/***************************************************************
*
*  th  - (Tab Horizontal) Go right to the next tab stop (just move the cursor)
*  thl - (To Horizontal Left) Go left to the next tab stop (just move the cursor)
*  In mvcursor.c
*
***************************************************************/
case DM_th:
case DM_thl:
   dm_th(dm_list->th.cmd, dspl_descr->cursor_buff);
   *warp_needed = True;
   break;

/***************************************************************
*
*  ti - (To Input) Go to the next input window
*  tn - (To next) Go to the next window
*       NOTE: in this case we want to override warp_needed, do not use |= for assignment.
*  In mvcursor.c
*
***************************************************************/
case DM_ti:
case DM_tn:
   *warp_needed = dm_tn(dm_list,
                       dspl_descr->cursor_buff,
                       dspl_descr->dminput_pad,
                       ((dspl_descr->pad_mode && !(dspl_descr->vt100_mode)) ? dspl_descr->unix_pad : NULL));
   break;

/***************************************************************
*
*  title - Change window manager title and icon title
*  In color.c
*
***************************************************************/
case DM_title:
   /* test added to effciently deal with title commands in unix prompt ala prefix option RES 9/20/95 */
   if (!dspl_descr->wm_title || !dm_list->title.parm || (strcmp(dm_list->title.parm, dspl_descr->wm_title) != 0))
      {
         change_title(dspl_descr, dm_list->title.parm);/* in color.c */
         if (dspl_descr->wm_title)
            free(dspl_descr->wm_title);
         dspl_descr->wm_title = malloc_copy(dm_list->title.parm);
      }
   break;

/***************************************************************
*
*  tl - (To Left) Go to the left end of the string.
*  In mvcursor.c
*
***************************************************************/
case DM_tl:
   redraw_needed |= dm_tl(dspl_descr->cursor_buff);
   *warp_needed = True;
   break;

/***************************************************************
*
*  tmb -  To Menu Bar.  Freeze main window to allow
*  pulldowns to operate on main window.
*
***************************************************************/
case DM_tmb:
   if (dm_tmb(dspl_descr->display, dspl_descr->pd_data))
      *warp_needed = False;
   break;

/***************************************************************
*
*  tmw - (To Main Window) GO To the main window.
*  In mark.c
*
***************************************************************/
case DM_tmw:
   redraw_needed |= dm_tmw(dspl_descr->cursor_buff, dspl_descr->main_pad);
   dspl_descr->cursor_buff->up_to_snuff = 0;
   *warp_needed = True;
   break;

/***************************************************************
*
*  tr - (To Right) Go to the right end of the string.
*  In mvcursor.c
*
***************************************************************/
case DM_tr:
   redraw_needed |= dm_tr(dspl_descr->cursor_buff);
   *warp_needed = True;
   break;

/***************************************************************
*
*  ts - (Tab Stops) Set the tab stops
*  In tab.c
*
***************************************************************/
case DM_ts:
   redraw_needed |= dm_ts(dm_list);
   break;

/***************************************************************
*
*  tt - (To Top) Move cursor to top line in window
*       Only valid in the main window.
*  In mvcursor.c
*
***************************************************************/
case DM_tt:
   /*if (dspl_descr->cursor_buff->which_window == MAIN_PAD) removed 11/7/94 */
   *warp_needed |= dm_tt(dspl_descr->cursor_buff);
   dspl_descr->cursor_buff->up_to_snuff = 0;
   break;

/***************************************************************
*
*  twb - Go to a requested window border
*  In mvcursor.c
*
***************************************************************/
case DM_twb:
   dm_twb(dspl_descr->cursor_buff, dm_list);
   *warp_needed = True;
   break;

/***************************************************************
*
*  undo -  undo a change
*  redo -  redo an undone change  (new command in this editor)
*  In xc.c
*
***************************************************************/
case DM_undo:
case DM_redo:
   redraw_needed |= dm_undo(dm_list, dspl_descr->cursor_buff);
   *warp_needed = True;
   break;

/***************************************************************
*
*  untab -  convert tabs to blanks using current tabstops
*  In tab.c
*
***************************************************************/
case DM_untab:
   redraw_needed |= dm_untab(&(dspl_descr->mark1), dspl_descr->cursor_buff, dspl_descr->echo_mode);
   dspl_descr->echo_mode = NO_HIGHLIGHT;
   *warp_needed = True;
   break;

/***************************************************************
*
*  vt -  toggle vt100 mode
*  In vt100.c
*
***************************************************************/
case DM_vt:
#ifdef PAD
   if (dspl_descr->pad_mode)
      {
         if (AUTOVT && (dm_list->vt100.mode == DM_TOGGLE))
            dm_error_dspl("Toggle of vt mode not allowed when autovt option is on", DM_ERROR_BEEP, dspl_descr);
         else
            {
               if (dm_vt100(dm_list, dspl_descr))
                  dspl_descr->redo_cursor_in_configure = REDRAW_FORCE_REAL_WARP;
               dspl_descr->mark1.mark_set = False;
            }
      }
#endif
   break;

/***************************************************************
*
*  wa -  Change window autohold mode mode on a window
*        This is a no-op in non-pad mode.
*  In toggle_switch in mark.c
*
***************************************************************/
case DM_wa:
#ifdef PAD
   if (dspl_descr->pad_mode)
      {
         dspl_descr->autohold_mode = toggle_switch(dm_list->wh.mode, 
                                                   dspl_descr->autohold_mode,
                                                   &i); /* junk parm */
         redraw_needed |= (FULL_REDRAW & TITLEBAR_MASK);
      }
#endif
   break;

/***************************************************************
*
*  wc - (Window close) bail out.
*       wc will return if it issues a prompt about file not saved
*       or has closed one of serveral cc windows.  When the last
*       window is closed, dm_wc does not return.
*       in wc.c
*
***************************************************************/
case DM_wc:
   flush(dspl_descr->main_pad);
   redraw_needed |= dm_wc(dm_list, dspl_descr, edit_file, wc_display_deleted, TRANSPAD, MAKE_UTMP);
   if (*wc_display_deleted)
      {
         redraw_needed = 0; /* once the window is gone, do not redraw it */
         *warp_needed = False;
      }
   else
      {
         if (redraw_needed)
            *warp_needed = True;
      }
   break;

/***************************************************************
*
*  wdc - (Window DeFault Color) set up defaults for colors
*       in wdf.c
*
***************************************************************/
case DM_wdc:
   dm_wdf(dm_list, dspl_descr->display, dspl_descr->properties);
   break;

/***************************************************************
*
*  wdf - (Window DeFault Geometry) set up defaults for geometries
*       in wdf.c
*
***************************************************************/
case DM_wdf:
   dm_wdf(dm_list, dspl_descr->display, dspl_descr->properties);
   break;

/***************************************************************
*
*  wh -  Change window hold mode mode on a window
*        This is a no-op in non-pad mode.
*  In dm_wh is in mvcursor.c
*
***************************************************************/
case DM_wh:
#ifdef PAD
   if (dspl_descr->pad_mode)
      {
         redraw_needed |= dm_wh(dm_list, dspl_descr);
         if (redraw_needed)
            *warp_needed = True;
      }
#endif
   break;

/***************************************************************
*
*  ws -  Change window hold mode mode on a window
*        This is a no-op in non-pad mode.
*  In toggle_switch in mark.c
*
***************************************************************/
case DM_ws:
#ifdef PAD
   if (dspl_descr->pad_mode)
      {
         dspl_descr->scroll_mode = toggle_switch(dm_list->wh.mode, 
                                                 dspl_descr->scroll_mode,
                                                 &i); /* junk parm */
         redraw_needed |= (FULL_REDRAW & TITLEBAR_MASK);
      }
#endif
   break;

/***************************************************************
*
*  wp - Window Pop
*  in window.c
*
***************************************************************/
case DM_wp:
   dm_wp(dm_list, dspl_descr->main_pad);
   dspl_descr->redo_cursor_in_configure = True;
   *warp_needed = True;
   break;

/***************************************************************
*
*  ww - (Window Wrap) Turn on and off window wrap.
*       in ww.c
*
***************************************************************/
case DM_ww:
#ifdef PAD
   if (find_padmode_dspl(dspl_descr))
      break;
#endif
   flush(dspl_descr->main_pad);
   redraw_needed |= dm_ww(dm_list, dspl_descr->cursor_buff, &(dspl_descr->mark1));
   *warp_needed = True;
   break;

/***************************************************************
*
*  xa - Append one paste buffer to another
*  In xc.c
*
***************************************************************/
case DM_xa:
   dm_xa(dm_list,
         dspl_descr->display,
         dspl_descr->main_pad->x_window,
         (((event_union->type == ButtonPress) || (event_union->type == ButtonRelease)) ? event_union->xbutton.time : event_union->xkey.time),
         dspl_descr->escape_char);
   break;

/***************************************************************
*
*  xc - (Copy)
*  xd - (Cut)
*  In xc.c
*
***************************************************************/
case DM_xc:
case DM_xd:
#ifdef PAD
   if (dspl_descr->vt100_mode && (dspl_descr->cursor_buff->which_window == MAIN_PAD))
      {
         set_event_masks(dspl_descr, True);  /* turn the mouse off in the main window, dr turned it on */
         dspl_descr->cursor_buff->current_win_buff->file_line_no = dspl_descr->cursor_buff->win_line_no + dspl_descr->cursor_buff->current_win_buff->first_line;
         dspl_descr->cursor_buff->current_win_buff->file_col_no  = dspl_descr->cursor_buff->win_col_no  + dspl_descr->cursor_buff->current_win_buff->first_char;
      }
#endif
   redraw_needed |= dm_xc(dm_list,
                          &(dspl_descr->mark1),
                          dspl_descr->cursor_buff,
                          dspl_descr->display,
                          dspl_descr->main_pad->x_window,
                          dspl_descr->echo_mode,
                          (((event_union->type == ButtonPress) || (event_union->type == ButtonRelease)) ? event_union->xbutton.time : event_union->xkey.time),
                          dspl_descr->escape_char);
   dspl_descr->echo_mode = NO_HIGHLIGHT;
   *warp_needed = True;  /* force cursor change */
#ifdef PAD
   if (dspl_descr->vt100_mode && (dspl_descr->cursor_buff->which_window == MAIN_PAD))
      dm_position(dspl_descr->cursor_buff, dspl_descr->cursor_buff->current_win_buff, dspl_descr->cursor_buff->current_win_buff->file_line_no, dspl_descr->cursor_buff->current_win_buff->file_col_no);
#endif
   break;

/***************************************************************
*
*  xl - (Copy Literal)
*  In xc.c
*
***************************************************************/
case DM_xl:
   dm_xl(dm_list,
         dspl_descr->display,
         dspl_descr->main_pad->x_window,
         (((event_union->type == ButtonPress) || (event_union->type == ButtonRelease)) ? event_union->xbutton.time : event_union->xkey.time),
         dspl_descr->escape_char);
   break;

/***************************************************************
*
*  xp - (P'aste)
*  In mark.c
*
***************************************************************/
case DM_xp:
   redraw_needed |= dm_xp(dm_list, dspl_descr->cursor_buff, dspl_descr->display, dspl_descr->main_pad->x_window, dspl_descr->escape_char);
   *warp_needed = True;
   break;

default:
   if (dm_list->any.cmd < DM_MAXDEF)
      snprintf(msg, sizeof(msg), "Unsupported CE command, (%s)", dmsyms[dm_list->any.cmd].name);
   else
      snprintf(msg, sizeof(msg), "Unknown CE command, (id = %d)", dm_list->any.cmd);
   dm_error_dspl(msg, DM_ERROR_BEEP, dspl_descr);
   break;

} /* end of switch on cmd_id */

return(redraw_needed);

} /* end of cswitch */


/************************************************************************

NAME:      dm_cntlc  - become either an xc or a dq


PURPOSE:    This routine processes the cntlc command.  It looks at the
            situation and generates either an xc or a dq command.  If
            this is a ceterm, and nothing is marked, and the cursor is
            in the UNIX window, we generate a dq.  Otherwise an xc.


PARAMETERS:
   1. dmc          -  pointer to DMC  (INPUT)
                      This the first DM command structure for the cntlc
                      command being executed.  It contains all the parts
                      of a dq -a and an xc.

   2. dspl_descr    - pointer to DISPLAY_DESCR (INPUT)
                      The current display description is passed.

FUNCTIONS :
   1.  See if we are in pad mode, if not, generate an xc.

   2.  See if the cursor is in the UNIX window.  If not generate an xc.

   3.  See if an area is being marked.  If so, generate an xc.

RETURNED VALUE:
   new_dmc   -  pointer to DMC
                The returned value is the either an xc or dq DMC.
                NULL - Failure, message already output.

*************************************************************************/

static DMC   *dm_cntlc(DMC               *dmc,
                       DISPLAY_DESCR     *dspl_descr)
{
DMC         *new_dmc;

/***************************************************************
*
*  Get the DMC (DM Command) structure to fill out.
*
***************************************************************/

new_dmc = (DMC *)CE_MALLOC(sizeof(DMC));
if (!new_dmc)
   return(NULL);
memset((char *)new_dmc, 0, sizeof(DMC));
new_dmc->any.next         = NULL;
new_dmc->any.temp         = True;

DEBUG4(
fprintf(stderr, "pad_mode = %d,   mark_set = %d,  which_window = %s\n", dspl_descr->pad_mode, dspl_descr->mark1.mark_set, which_window_names[dspl_descr->cursor_buff->which_window]);
)

if (dspl_descr->pad_mode &&
   /* (dspl_descr->cursor_buff->which_window == UNIXCMD_WINDOW) && */
    !dspl_descr->mark1.mark_set)
   {
      new_dmc->any.cmd          = DM_dq;
      new_dmc->dq.dash_a        = dmc->cntlc.dash_h;
   }
else
   {
      new_dmc->any.cmd          = DM_xc;
      new_dmc->xc.dash_r        = new_dmc->cntlc.dash_r;
      new_dmc->xc.dash_f        = new_dmc->cntlc.dash_f;
      new_dmc->xc.dash_a        = new_dmc->cntlc.dash_a;
      new_dmc->xc.dash_l        = new_dmc->cntlc.dash_l;
      new_dmc->xc.path          = malloc_copy(new_dmc->cntlc.path);
   }


return(new_dmc);

}  /* end of dm_cntlc */




#ifdef DebuG

#include <fcntl.h>                   /* /usr/include/fcntl.h     */


/************************************************************************

NAME:      dm_debug   -  debugging commands

PURPOSE:    This routine will either call one of a series of predefined
            debugging routines or set the debug variable.

PARAMETERS:
   1.   dmc        -  pointer to DMC (INPUT)
                      This is the debug dm structure.

   2.   dspl_descr -  pointer to DISPLAY_DESCR (INPUT)
                      The current display description.

FUNCTIONS :

   1.   Determine whether this call sets the debug environment variable
        or is a specil debug.

   2.   Dump the appropriate information to stderr.


*************************************************************************/

static void dump_bitlist(void)
{
fprintf(stderr, "Bits correspond to numbers after the @, ei. debug @1,4,16 turns on bits 1, 4, and 16\n");
fprintf(stderr, "Bit 1   -  Window setup and sizing, pgm args, intialization data\n");
fprintf(stderr, "Bit 2   -  Mark/copy/paste/find\n");
fprintf(stderr, "Bit 3   -  Memdata (Line / text insertion / replace)\n");
fprintf(stderr, "Bit 4   -  DM cmd parsing, assignment, and execution\n");
fprintf(stderr, "Bit 5   -  Cursor movement, window repositioning\n");
fprintf(stderr, "Bit 6   -  Undo/redo \n");
fprintf(stderr, "Bit 7   -  Typing es, ed, ee, en\n");
fprintf(stderr, "Bit 8   -  Text Cursor Motion (txcursor and highlight cursor motion) - verbose\n");
fprintf(stderr, "Bit 9   -  Set _Xdebug on (sync events in X)\n");
fprintf(stderr, "Bit 10  -  Warping cursor data,  as opposed to incoming handled with BIT 8\n");
fprintf(stderr, "Bit 11  -  Keysym translation, initial keydef loading in\n");
fprintf(stderr, "Bit 12  -  Screen drawing and copying from pixmap (often used with bit 10)\n");
fprintf(stderr, "Bit 13  -  Search / substitute\n");
fprintf(stderr, "Bit 14  -  Event dump for cursor motion events plus cursor pixel dumping (Very verbose)\n");
fprintf(stderr, "Bit 15  -  Event reception for non-motion events\n");
fprintf(stderr, "Bit 16  -  Pad support (shell window) - stuff to and from the shell\n");
fprintf(stderr, "Bit 17  -  Set copy / paste incr size to 128, forces X INCR processing also dm_rec tracing\n");
fprintf(stderr, "Bit 18  -  cc (carbon copy and multi window edit)\n");
fprintf(stderr, "Bit 19  -  unix pad routines (often used with bit 16)\n");
fprintf(stderr, "Bit 20  -  wdf  and wdc and color(ca) processing\n");
fprintf(stderr, "Bit 21  -  Local Malloc routine debugging (macro CE_MALLOC)\n");
fprintf(stderr, "Bit 22  -  I/O signals coming from Shell socket and X socket\n");
fprintf(stderr, "Bit 23  -  vt100 mode emulation in and scroll bar\n");
fprintf(stderr, "Bit 24  -  dump atoms on property notify events\n");
fprintf(stderr, "Bit 25  -  Keep track of signals also low level text highlighting\n");
fprintf(stderr, "Bit 26  -  Hash table lookup stats\n");
fprintf(stderr, "Bit 27  -  File locking\n");
fprintf(stderr, "Bit 28  -  Indent processing\n");
fprintf(stderr, "Bit 29  -  Pulldown creation and processing\n");
fprintf(stderr, "Bit 30  -  testing bit for ceterm, turns off the usleeps to avoid SIGALARM signals\n");
fprintf(stderr, "Bit 31  -  Not Used \n");
fprintf(stderr, "Bit 32  -  Not Used\n");
} /* end of dump_bitlist */


/************************************************************************

NAME:      dm_debug   -  debugging commands

PURPOSE:    This routine will either call one of a series of predefined
            debugging routines or set the debug variable.

PARAMETERS:
   1.   dmc        -  pointer to DMC (INPUT)
                      This is the debug dm structure.

   2.   dspl_descr -  pointer to DISPLAY_DESCR (INPUT)
                      The current display description.

FUNCTIONS :

   1.   Determine whether this call sets the debug environment variable
        or is a specil debug.

   2.   Dump the appropriate information to stderr.


*************************************************************************/

static void dm_debug(DMC               *dmc,
                     DISPLAY_DESCR     *dspl_descr)
{
char            scrap[512];
char           *p;
int             debug_option;
int             i;

static char    *list[] = {"select",    /*  0  Recalculate and dump X event masks */
                          "dump",      /*  1  Dump memdata map for main pad      */
                          "time",      /*  2  Dump the current time              */
                          "options",   /*  3  Dump current run time options      */
                          "windows",   /*  4  Dump window information            */
                          "tty",       /*  5  Dump tty info                      */
                          "dscpln",    /*  6  Bump line discipline               */
                          "drawables", /*  7  Bump line discipline               */
                          "color",     /*  8  Dump color data                    */
                          "colorbits", /*  9  Dump color data                    */
                          "unlock",    /* 10  Unlock the display                 */
                          "cclist",    /* 11  Unlock the display                 */
                          "bitlist",   /* 12  Unlock the display                 */
                          NULL};       /* 13  Default is debug info              */

if ((!dmc->debug.parm) || (*dmc->debug.parm == '\0'))
   {
      dm_error("debug {0 | @n[-n2][-n3]...[> path] | select | dump | time | options | windows | tty | dscpln | drawables | color | colorbits | cclist | bitlist} ", DM_ERROR_MSG);
      return;
   }

for (debug_option = 0;
     list[debug_option] && (strcmp(dmc->debug.parm, list[debug_option]) != 0);
     debug_option++)
 ; /* do nothing */
    

switch(debug_option)
{
case 0:
   /***************************************************************
   *  Recalculate and dump X event masks
   ***************************************************************/
   i = debug;
   debug = 1;
   set_event_masks(dspl_descr, True);
   debug = i;
   break;

case 1:
   /***************************************************************
   *  Dump memdata map for main pad
   ***************************************************************/
   dump_ds(dspl_descr->main_pad->token);
   break;

case 2:
   /***************************************************************
   *  Dump the current time
   ***************************************************************/
   dm_error((char *)CURRENT_TIME, DM_ERROR_LOG);
   break;

case 3:
   /***************************************************************
   *  Dump current run time options
   ***************************************************************/
   for (i = 0; i < OPTION_COUNT; i++)
      fprintf(stderr, "option %2d: %s = \"%s\"\n", i, cmd_options[i].specifier, (OPTION_VALUES[i] ? OPTION_VALUES[i] : "(NULL)"));
   break;

case 4:
   /***************************************************************
   *  Dump window information
   ***************************************************************/
   i = debug;
   debug = 1;
   socketfd2hostinet(ConnectionNumber(dspl_descr->display), DISPLAY_PORT_LOCAL);
   socketfd2hostinet(ConnectionNumber(dspl_descr->display), DISPLAY_PORT_REMOTE);
   debug = i;
   fprintf(stderr, "display # %d\nmain window     = 0x%02X\ndminput window  = 0x%02X\ndmoutput window = 0x%02X\npixmap id       = 0x%02X\nWM window       = 0x%02X\n",
                   dspl_descr->display_no,
                   dspl_descr->main_pad->x_window, dspl_descr->dminput_pad->x_window, dspl_descr->dmoutput_pad->x_window,
                   dspl_descr->x_pixmap, dspl_descr->wm_window);

   fprintf(stderr, "main:      window   %dx%d%+d%+d(%dx%d%+d%+d)    pixmap %dx%d%+d%+d(%dx%d%+d%+d)\n",
                   dspl_descr->main_pad->window->width,         dspl_descr->main_pad->window->height,
                   dspl_descr->main_pad->window->x,             dspl_descr->main_pad->window->y,
                   dspl_descr->main_pad->window->sub_width,     dspl_descr->main_pad->window->sub_height,
                   dspl_descr->main_pad->window->sub_x,         dspl_descr->main_pad->window->sub_y,
                   dspl_descr->main_pad->pix_window->width,     dspl_descr->main_pad->pix_window->height,
                   dspl_descr->main_pad->pix_window->x,         dspl_descr->main_pad->pix_window->y,
                   dspl_descr->main_pad->pix_window->sub_width, dspl_descr->main_pad->pix_window->sub_height,
                   dspl_descr->main_pad->pix_window->sub_x,     dspl_descr->main_pad->pix_window->sub_y);

   if (dspl_descr->pad_mode)
   fprintf(stderr, "unix:      window   %dx%d%+d%+d(%dx%d%+d%+d)    pixmap %dx%d%+d%+d(%dx%d%+d%+d)\n",
                   dspl_descr->unix_pad->window->width,         dspl_descr->unix_pad->window->height,
                   dspl_descr->unix_pad->window->x,             dspl_descr->unix_pad->window->y,
                   dspl_descr->unix_pad->window->sub_width,     dspl_descr->unix_pad->window->sub_height,
                   dspl_descr->unix_pad->window->sub_x,         dspl_descr->unix_pad->window->sub_y,
                   dspl_descr->unix_pad->pix_window->width,     dspl_descr->unix_pad->pix_window->height,
                   dspl_descr->unix_pad->pix_window->x,         dspl_descr->unix_pad->pix_window->y,
                   dspl_descr->unix_pad->pix_window->sub_width, dspl_descr->unix_pad->pix_window->sub_height,
                   dspl_descr->unix_pad->pix_window->sub_x,     dspl_descr->unix_pad->pix_window->sub_y);

   fprintf(stderr, "dm_input:  window   %dx%d%+d%+d(%dx%d%+d%+d)    pixmap %dx%d%+d%+d(%dx%d%+d%+d)\n",
                   dspl_descr->dminput_pad->window->width,         dspl_descr->dminput_pad->window->height,
                   dspl_descr->dminput_pad->window->x,             dspl_descr->dminput_pad->window->y,
                   dspl_descr->dminput_pad->window->sub_width,     dspl_descr->dminput_pad->window->sub_height,
                   dspl_descr->dminput_pad->window->sub_x,         dspl_descr->dminput_pad->window->sub_y,
                   dspl_descr->dminput_pad->pix_window->width,     dspl_descr->dminput_pad->pix_window->height,
                   dspl_descr->dminput_pad->pix_window->x,         dspl_descr->dminput_pad->pix_window->y,
                   dspl_descr->dminput_pad->pix_window->sub_width, dspl_descr->dminput_pad->pix_window->sub_height,
                   dspl_descr->dminput_pad->pix_window->sub_x,     dspl_descr->dminput_pad->pix_window->sub_y);

   fprintf(stderr, "dm_output: window   %dx%d%+d%+d(%dx%d%+d%+d)    pixmap %dx%d%+d%+d(%dx%d%+d%+d)\n",
                   dspl_descr->dmoutput_pad->window->width,         dspl_descr->dmoutput_pad->window->height,
                   dspl_descr->dmoutput_pad->window->x,             dspl_descr->dmoutput_pad->window->y,
                   dspl_descr->dmoutput_pad->window->sub_width,     dspl_descr->dmoutput_pad->window->sub_height,
                   dspl_descr->dmoutput_pad->window->sub_x,         dspl_descr->dmoutput_pad->window->sub_y,
                   dspl_descr->dmoutput_pad->pix_window->width,     dspl_descr->dmoutput_pad->pix_window->height,
                   dspl_descr->dmoutput_pad->pix_window->x,         dspl_descr->dmoutput_pad->pix_window->y,
                   dspl_descr->dmoutput_pad->pix_window->sub_width, dspl_descr->dmoutput_pad->pix_window->sub_height,
                   dspl_descr->dmoutput_pad->pix_window->sub_x,     dspl_descr->dmoutput_pad->pix_window->sub_y);

   dump_window_attrs(dspl_descr->display, dspl_descr->main_pad->x_window);
   break;

case 5:
   /***************************************************************
   *  Dump tty info
   ***************************************************************/
#if defined(PAD) && !defined(WIN32)
   if (dspl_descr->pad_mode)
      {
         dump_tty(-1);    /* dump tty attributes */
#if defined(_INCLUDE_HPUX_SOURCE) || defined(solaris)
      for (i = 0; i < 10; i++)
         dump_ttya(i, "TTY structure:");
#endif
      }
   else
      fprintf(stderr, "debug tty not valid, not a ceterm\n");
#else
   fprintf(stderr, "debug tty not compiled in\n");
#endif
   break;

case 6:
   /***************************************************************
   *  Bump line discipline
   ***************************************************************/
#if defined(PAD) && !defined(WIN32)
   if (dspl_descr->pad_mode)
      dscpln();  /* bump line discipline {0-4} */
   else
      fprintf(stderr, "debug dscpln not valid, not a ceterm\n");
#else
   fprintf(stderr, "debug dscpln not compiled in\n");
#endif
   break;

case 7:
   /***************************************************************
   *  Dump drawables
   ***************************************************************/
   fprintf(stderr, "\nmain     window:       %dx%d%+d%+d  sub: %dx%d%+d%+d  line height %d, fixed font %d, lines on screen %d, gc's (%x,%x,%x) font %x\n",
           dspl_descr->main_pad->window->width, dspl_descr->main_pad->window->height, dspl_descr->main_pad->window->x, dspl_descr->main_pad->window->y,
           dspl_descr->main_pad->window->sub_width, dspl_descr->main_pad->window->sub_height, dspl_descr->main_pad->window->sub_x, dspl_descr->main_pad->window->sub_y,
           dspl_descr->main_pad->window->line_height, dspl_descr->main_pad->window->fixed_font, dspl_descr->main_pad->window->lines_on_screen,
           dspl_descr->main_pad->window->gc, dspl_descr->main_pad->window->reverse_gc, dspl_descr->main_pad->window->xor_gc, dspl_descr->main_pad->window->font);
   fprintf(stderr, "main     pix sub:      %dx%d%+d%+d  sub: %dx%d%+d%+d  line height %d, fixed font %d, lines on screen %d, gc's (%x,%x,%x) font %x\n\n",
           dspl_descr->main_pad->pix_window->width, dspl_descr->main_pad->pix_window->height, dspl_descr->main_pad->pix_window->x, dspl_descr->main_pad->pix_window->y,
           dspl_descr->main_pad->pix_window->sub_width, dspl_descr->main_pad->pix_window->sub_height, dspl_descr->main_pad->pix_window->sub_x, dspl_descr->main_pad->pix_window->sub_y,
           dspl_descr->main_pad->pix_window->line_height, dspl_descr->main_pad->pix_window->fixed_font, dspl_descr->main_pad->pix_window->lines_on_screen,
           dspl_descr->main_pad->pix_window->gc, dspl_descr->main_pad->pix_window->reverse_gc, dspl_descr->main_pad->pix_window->xor_gc, dspl_descr->main_pad->pix_window->font);

   fprintf(stderr, "dminput  window:       %dx%d%+d%+d  sub: %dx%d%+d%+d  line height %d, fixed font %d, lines on screen %d, gc's (%x,%x,%x) font %x\n",
           dspl_descr->dminput_pad->window->width, dspl_descr->dminput_pad->window->height, dspl_descr->dminput_pad->window->x, dspl_descr->dminput_pad->window->y,
           dspl_descr->dminput_pad->window->sub_width, dspl_descr->dminput_pad->window->sub_height, dspl_descr->dminput_pad->window->sub_x, dspl_descr->dminput_pad->window->sub_y,
           dspl_descr->dminput_pad->window->line_height, dspl_descr->dminput_pad->window->fixed_font, dspl_descr->dminput_pad->window->lines_on_screen,
           dspl_descr->dminput_pad->window->gc, dspl_descr->dminput_pad->window->reverse_gc, dspl_descr->dminput_pad->window->xor_gc, dspl_descr->dminput_pad->window->font);
   fprintf(stderr, "dminput  pix sub:      %dx%d%+d%+d  sub: %dx%d%+d%+d  line height %d, fixed font %d, lines on screen %d, gc's (%x,%x,%x) font %x\n\n",
           dspl_descr->dminput_pad->pix_window->width, dspl_descr->dminput_pad->pix_window->height, dspl_descr->dminput_pad->pix_window->x, dspl_descr->dminput_pad->pix_window->y,
           dspl_descr->dminput_pad->pix_window->sub_width, dspl_descr->dminput_pad->pix_window->sub_height, dspl_descr->dminput_pad->pix_window->sub_x, dspl_descr->dminput_pad->pix_window->sub_y,
           dspl_descr->dminput_pad->pix_window->line_height, dspl_descr->dminput_pad->pix_window->fixed_font, dspl_descr->dminput_pad->pix_window->lines_on_screen,
           dspl_descr->dminput_pad->pix_window->gc, dspl_descr->dminput_pad->pix_window->reverse_gc, dspl_descr->dminput_pad->pix_window->xor_gc, dspl_descr->dminput_pad->pix_window->font);

   fprintf(stderr, "dmoutput window:       %dx%d%+d%+d  sub: %dx%d%+d%+d  line height %d, fixed font %d, lines on screen %d, gc's (%x,%x,%x) font %x\n",
           dspl_descr->dmoutput_pad->window->width, dspl_descr->dmoutput_pad->window->height, dspl_descr->dmoutput_pad->window->x, dspl_descr->dmoutput_pad->window->y,
           dspl_descr->dmoutput_pad->window->sub_width, dspl_descr->dmoutput_pad->window->sub_height, dspl_descr->dmoutput_pad->window->sub_x, dspl_descr->dmoutput_pad->window->sub_y,
           dspl_descr->dmoutput_pad->window->line_height, dspl_descr->dmoutput_pad->window->fixed_font, dspl_descr->dmoutput_pad->window->lines_on_screen,
           dspl_descr->dmoutput_pad->window->gc, dspl_descr->dmoutput_pad->window->reverse_gc, dspl_descr->dmoutput_pad->window->xor_gc, dspl_descr->dmoutput_pad->window->font);
   fprintf(stderr, "dmoutput pix sub:      %dx%d%+d%+d  sub: %dx%d%+d%+d  line height %d, fixed font %d, lines on screen %d, gc's (%x,%x,%x) font %x\n\n",
           dspl_descr->dmoutput_pad->pix_window->width, dspl_descr->dmoutput_pad->pix_window->height, dspl_descr->dmoutput_pad->pix_window->x, dspl_descr->dmoutput_pad->pix_window->y,
           dspl_descr->dmoutput_pad->pix_window->sub_width, dspl_descr->dmoutput_pad->pix_window->sub_height, dspl_descr->dmoutput_pad->pix_window->sub_x, dspl_descr->dmoutput_pad->pix_window->sub_y,
           dspl_descr->dmoutput_pad->pix_window->line_height, dspl_descr->dmoutput_pad->pix_window->fixed_font, dspl_descr->dmoutput_pad->pix_window->lines_on_screen,
           dspl_descr->dmoutput_pad->pix_window->gc, dspl_descr->dmoutput_pad->pix_window->reverse_gc, dspl_descr->dmoutput_pad->pix_window->xor_gc, dspl_descr->dmoutput_pad->pix_window->font);

   if (dspl_descr->pad_mode)
      {
         fprintf(stderr, "unix     window:       %dx%d%+d%+d  sub: %dx%d%+d%+d  line height %d, fixed font %d, lines on screen %d, gc's (%x,%x,%x) font %x\n",
                 dspl_descr->unix_pad->window->width, dspl_descr->unix_pad->window->height, dspl_descr->unix_pad->window->x, dspl_descr->unix_pad->window->y,
                 dspl_descr->unix_pad->window->sub_width, dspl_descr->unix_pad->window->sub_height, dspl_descr->unix_pad->window->sub_x, dspl_descr->unix_pad->window->sub_y,
                 dspl_descr->unix_pad->window->line_height, dspl_descr->unix_pad->window->fixed_font, dspl_descr->unix_pad->window->lines_on_screen,
                 dspl_descr->unix_pad->window->gc, dspl_descr->unix_pad->window->reverse_gc, dspl_descr->unix_pad->window->xor_gc, dspl_descr->unix_pad->window->font);
         fprintf(stderr, "unix     pix sub:      %dx%d%+d%+d  sub: %dx%d%+d%+d  line height %d, fixed font %d, lines on screen %d, gc's (%x,%x,%x) font %x\n\n",
                 dspl_descr->unix_pad->pix_window->width, dspl_descr->unix_pad->pix_window->height, dspl_descr->unix_pad->pix_window->x, dspl_descr->unix_pad->pix_window->y,
                 dspl_descr->unix_pad->pix_window->sub_width, dspl_descr->unix_pad->pix_window->sub_height, dspl_descr->unix_pad->pix_window->sub_x, dspl_descr->unix_pad->pix_window->sub_y,
                 dspl_descr->unix_pad->pix_window->line_height, dspl_descr->unix_pad->pix_window->fixed_font, dspl_descr->unix_pad->pix_window->lines_on_screen,
                 dspl_descr->unix_pad->pix_window->gc, dspl_descr->unix_pad->pix_window->reverse_gc, dspl_descr->unix_pad->pix_window->xor_gc, dspl_descr->unix_pad->pix_window->font);
      }

   fprintf(stderr, "title            area: %dx%d%+d%+d  sub: %dx%d%+d%+d  line height %d, fixed font %d, lines on screen %d, gc's (%x,%x,%x) font %x\n\n",
           dspl_descr->title_subarea->width, dspl_descr->title_subarea->height, dspl_descr->title_subarea->x, dspl_descr->title_subarea->y,
           dspl_descr->title_subarea->sub_width, dspl_descr->title_subarea->sub_height, dspl_descr->title_subarea->sub_x, dspl_descr->title_subarea->sub_y,
           dspl_descr->title_subarea->line_height, dspl_descr->title_subarea->fixed_font, dspl_descr->title_subarea->lines_on_screen,
           dspl_descr->title_subarea->gc, dspl_descr->title_subarea->reverse_gc, dspl_descr->title_subarea->xor_gc, dspl_descr->title_subarea->font);

   if (dspl_descr->show_lineno)
      fprintf(stderr, "lineno           area: %dx%d%+d%+d  sub: %dx%d%+d%+d  line height %d, fixed font %d, lines on screen %d, gc's (%x,%x,%x) font %x\n\n",
              dspl_descr->lineno_subarea->width, dspl_descr->lineno_subarea->height, dspl_descr->lineno_subarea->x, dspl_descr->lineno_subarea->y,
              dspl_descr->lineno_subarea->sub_width, dspl_descr->lineno_subarea->sub_height, dspl_descr->lineno_subarea->sub_x, dspl_descr->lineno_subarea->sub_y,
              dspl_descr->lineno_subarea->line_height, dspl_descr->lineno_subarea->fixed_font, dspl_descr->lineno_subarea->lines_on_screen,
              dspl_descr->lineno_subarea->gc, dspl_descr->lineno_subarea->reverse_gc, dspl_descr->lineno_subarea->xor_gc, dspl_descr->lineno_subarea->font);
   break;

case 8:
   /***************************************************************
   *  Dump memdata color data for main pad
   ***************************************************************/
   print_color(dspl_descr->main_pad->token);
   break;

case 9:
   /***************************************************************
   *  Dump memdata color bit patterns for main pad
   ***************************************************************/
   print_color_bits(dspl_descr->main_pad->token);
   break;

case 10:
   /***************************************************************
   *  Unlock the current display.
   ***************************************************************/
   XDisableAccessControl(dspl_descr->display);
   break;

case 11:
   /***************************************************************
   *  Dump the current cclist data.
   ***************************************************************/
   cc_debug_cclist_data(dspl_descr->display, dspl_descr->wm_window);
   break;

case 12:
   /***************************************************************
   *  Dump the current cclist data.
   ***************************************************************/
   dump_bitlist();
   break;

default:
   /***************************************************************
   *  Change the CEDEBUG envrionment variable
   ***************************************************************/
   snprintf(scrap, sizeof(scrap), "%s=%s", DEBUG_ENV_NAME, dmc->debug.parm);
   p = malloc_copy(scrap);
   if (p)
      putenv(p);
   Debug(DEBUG_ENV_NAME);
   break;
} /* end of switch on debug_option */

      
}  /* end dm_debug */


static void dump_window_attrs(Display     *display,
                              Window       window)
{
XWindowAttributes      attrs;
XWMHints              *hints;

XGetWindowAttributes(display, window, &attrs);
fprintf(stderr, "%dx%d%+d%+d border width %d, depth %d, visual 0x%X, root 0x%X, window 0x%X, class %s\n",
       attrs.width, attrs.height, attrs.x, attrs.y, attrs.border_width,
       attrs.depth, attrs.visual, attrs.root, window, ((attrs.class == InputOutput) ? "InputOutput" : "InputOnly"));

fprintf(stderr, "    bit gravity, %d, win_gravity %d, backing_store %s(%d), save_under %d, backing_planes 0x%X, backing_pixel %d\n",
                attrs.bit_gravity, attrs.win_gravity,
                ((attrs.backing_store == NotUseful) ? "NotUseful" : (((attrs.backing_store == WhenMapped) ? "WhenMapped" : (((attrs.backing_store == Always) ? "Always" : "Unknown"))))),
                attrs.backing_store, attrs.save_under, attrs.backing_planes, attrs.backing_pixel);

fprintf(stderr, "    colormap 0x%X, map_installed %d, map_state %d,\n    all_event_masks 0x%X, your_event_mask 0x%X, do_not_propagate_mask 0x%X, override_redirect %d\n",
                attrs.colormap, attrs.map_installed, attrs.map_state, attrs.all_event_masks, attrs.your_event_mask,
                attrs.do_not_propagate_mask, attrs.override_redirect);

hints = XGetWMHints(display, window);
if (hints)
   {
      fprintf(stderr, "Flags         = 0x%08X\n", hints->flags);
      fprintf(stderr, "Input         = %d\n",     hints->input);
      fprintf(stderr, "initial_state = %d\n", hints->initial_state);
      fprintf(stderr, "icon pixmap   = 0x%08X\n", hints->icon_pixmap);
      fprintf(stderr, "icon window   = 0x%08X\n", hints->icon_window);
      fprintf(stderr, "icon x        = %d\n", hints->icon_x);
      fprintf(stderr, "icon y        = %d\n", hints->icon_y);
      fprintf(stderr, "icon mask     = 0x%08X\n", hints->icon_mask);
      fprintf(stderr, "window group  = 0x%08X\n", hints->window_group);
      XFree((char *)hints);
   }

} /* end of dump_window_attrs  */


#endif



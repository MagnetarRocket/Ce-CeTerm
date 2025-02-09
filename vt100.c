/*static char *sccsid = "%Z% %M% %I% - %G% %U% ";*/
#ifdef PAD
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
*  module vt100.c
*
*  This module provides the routines to support vt100 mode emulation
*  used by programs such as vi and elm.
*
*  Routines:
*     dm_vt100         -   Swap in and out of vt100 mode
*     vt100_parse      -   Parse data returned by the shell for display on the screen
*     vt100_pad2shell  -   Send data to the shell when in vt100 mode.
*     vt100_key_trans  -   Translate keys in vt100 keypad application mode
*     vt100_resize     -   Respond to resize events.
*     vt100_color_line -   Parse a line with VT100 colorization data
*
* Internal:
*     vt_move_cursor           - Reposition the cursor in the main window
*     get_numbers              - Get the parms from an esc[n;nZ type command
*     insert_char_in_window    - Put regular text characters in the window
*     delete_chars_in_window   - Delete characters from the window
*     erase_chars_in_window    - Erase (blank out) characters in the window.
*     erase_area               - Erase areas of the screen
*     insert_lines             - Insert blank lines
*     change_tek_mode          - Change terminal emulation mode
*     set_scroll_margins       - Set up the scrolling region of the screen
*     vt_sgr                   - Process Set Graphic Rendition requests
*     vt_ht                    - Process a left or right horizontal tab.
*     vt_add_tab_stop          - Add a tab stop
*     vt_erase_tab_stop        - Erase a tab stop
*     insert_fancy_line_list   - Add a FANCY_LINE descriptor to the winlines data
*     delete_fancy_line_element- Delete one element out of the linked list of fancy line data
*     clean_fancy_list         - Do housekeeping to get rid of defunct FANCY_LINE data
*     delete_fancy_line_list   - Delete all the fancy line data for a line.
*     build_color              - Build memdata color data from VT100 color info.
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h   /usr/include/sys/errno.h      */
#include <ctype.h>          /* /usr/include/ctype.h      */
#include <sys/types.h>      /* /usr/include/sys/types.h */
#include <fcntl.h>          /* /usr/include/fcntl.h */
#include <signal.h>         /* /usr/include/signal.h */
#include <limits.h>         /* /usr/include/limits.h     */
#ifndef MAXPATHLEN
#define MAXPATHLEN	1024
#endif

#include <X11/Xlib.h>        /* /usr/include/X11/Xlib.h  needed for define of KeySym */
#include <X11/keysym.h>      /* /usr/include/X11/keysym.h /usr/include/X11/keysymdef.h */

#include "borders.h"
#include "cdgc.h"                                          
#include "dmwin.h"
#include "dmsyms.h"
#include "emalloc.h"
#include "getevent.h"
#include "hexdump.h"
#include "mark.h"
#include "memdata.h"
#include "mouse.h"
#include "mvcursor.h"
#include "pad.h"
#include "parms.h"
#include "redraw.h"
#include "sendevnt.h"
#include "tab.h"
#include "typing.h"
#include "txcursor.h"
#include "unixpad.h"
#include "unixwin.h"
#include "window.h"
#include "vt100.h"
#include "xerrorpos.h"

/***************************************************************
*  
*  Local prototypes
*  
***************************************************************/

static int vt_move_cursor(PAD_DESCR    *main_window_cur_descr,
                          int           row,
                          int           col,
                          int           rel_row,
                          int           rel_col,
                          int           stop_within_scroll,
                          int           add_scroll_lines);

static int  get_numbers(char   *line,
                        int     len,
                        int    *numbers);

static int insert_char_in_window(PAD_DESCR    *main_window_cur_descr,
                                 char          insert_char);

static int erase_chars_in_window(PAD_DESCR    *main_window_cur_descr,
                                 int           count);

static int delete_chars_in_window(PAD_DESCR    *main_window_cur_descr,
                                  int           count);


#define CURRENT_LINE_ERASE 0
#define WHOLE_BUFFER_ERASE 1
static int  erase_area(PAD_DESCR    *main_window_cur_descr,
                       int           type,
                       int           erase_whole_buffer);

static int   insert_lines(PAD_DESCR    *main_window_cur_descr,
                         int           lines);

static int change_tek_mode(PAD_DESCR    *main_window_cur_descr,
                           char         *line,
                           int           len,
                           int           do_set);

static void set_scroll_margins(PAD_DESCR    *main_window_cur_descr,
                               int           top,
                               int           bottom);

static void vt_sgr(PAD_DESCR    *main_window_cur_descr,
                   char         *line,
                   int           len,
                   int          *numbers);

static int   vt_ht(PAD_DESCR    *main_window_cur_descr,
                   int           count,
                   int           move_right);

static void  vt_add_tab_stop(PAD_DESCR    *main_window_cur_descr);

static void  vt_erase_tab_stop(PAD_DESCR    *main_window_cur_descr,
                               int           extent);

static void insert_fancy_line_list(FANCY_LINE    *new,
                                   FANCY_LINE   **head);

static void delete_fancy_line_element(FANCY_LINE    *kill,
                                      FANCY_LINE   **head);

static void clean_fancy_list(FANCY_LINE   **head);

static void delete_fancy_line_list(FANCY_LINE   **first);

static FANCY_LINE *build_color(DISPLAY_DESCR   *dspl_descr,
                               char            *graphic_rendition_key,
                               int              len,
                               int              first_col);


/***************************************************************
*  
*  static flags to show terminal state.
*  
***************************************************************/

/* 9/6/95 default changed to False except for SunOS  RES */
#if (defined(sun) && !defined(solaris))
static int   tek_autowrap_mode     = True;
#else
static int   tek_autowrap_mode     = False;  
#endif
static int   vt52_mode  = False;
static int   tek_overstrike_mode   = False;
static int   tek_cursor_key_mode   = False;
static int   tek_132_column_mode   = False;
static int   tek_local_echo_mode   = False;
static int   tek_application_keypad_mode   = False;
static int   tek_origin_mode_relative = False;
#ifdef WIN32
static int   linefeed_newline_mode = True;
#else
static int   linefeed_newline_mode = False;
#endif
static int   tek_top_margin        = 0;
static int   tek_bottom_margin     = 0;
static int   tek_margins_set       = False;
static char  device_attributes_vt100[]  = {VT_ESC, '[', '?', '1', ';', '2', 'c', 0};
static char  device_attributes_vt102[]  = {VT_ESC, '[', '?', '6', 'c', 0};

static int   power_up_tabstops[16] = {8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120, 129};
static int   power_up_tabcount      = 16;

#define      MAX_TABSTOPS 32
static int   tabstops[MAX_TABSTOPS];
static int   tabcount;

static int   save_cursor_row;
static int   save_cursor_col;
static int   save_cursor_origin;

static GC    fancy_gc = None;

/***************************************************************
*  
*  The data token for the normal main window pad when we are
*  it vt100 mode.  The data token for the normal main window
*  is salted away while we are in vt100 mode along with other
*  pieces of the pad description.  The data token is made global
*  to the module, so that lines which are scrolled off the
*  screen on the vt100 window are saved in the main pad.
*  
***************************************************************/

static DATA_TOKEN    *saved_main_window_token = NULL;


/************************************************************************

NAME:      dm_vt100               -   Swap in and out of vt100 mode


PURPOSE:    This routine handles the vt100 dm command.  It takes care of setting
            up and taking down vt100 mode.

PARAMETERS:

   1.  dmc          -  pointer to DMC (INPUT)
                       This is the file being edited.

   2.   dspl_descr   - pointer to DISPLAY_DESCR (INPUT / OUTPUT)
                       This is the current display description.

STATIC DATA:

saved_main_window_token - pointer to DATA_TOKEN (OUTPUT/INPUT)
                    When entering the vt100 mode, the main windows data token is
                    placed in a static storage area in this routine.  It is extracted
                    from this location when vt100 mode is turned off.

saved_lineno_mode       - int
                    When entering vt100 mode, the lineno option is forced off.
                    Its value is saved staticly so it can be restored when
                    vt100 mode is turned off.

saved_first_line       - int
                    When entering vt100 mode, first line is set to zero.
                    Its value is saved staticly so it can be restored when
                    vt100 mode is turned off.

saved_insert_mode       - int
                    When entering vt100 mode, insert mode is set to False.
                    Its value is saved staticly so it can be restored when
                    vt100 mode is turned off.

FUNCTIONS :
   1.   Update the vt100 mode flag in the main window current description
        and check to see if it changed (-on twice causes no change the
        second time).  If it did not change, return.

   2.   If the vt100 flag was turned on, save the main window data token and
        create a new one for use by the vt100 mode processing.  

   3.   If the vt100 flag was turned on, save the value of the lineno flag and
        turn the flag off.

   4.   If the vt100 flag was turned on, unmap the unxicmd window.

   5.   If the vt100 flag was turned off, delete the data token for the main window
        current description and restore the original one.

   6.   If the vt100 flag was turned off, restore the original lineno value.

   7.   If the value of the vt100 flag changed, generate a configure event to
        cause the windows to be recalculated and updated.

RETURNED VALUE:

   changed -  int flag
              True  -  The vt100 mode switched
              False -  The vt100 mode did not switch


*************************************************************************/

int  dm_vt100(DMC             *dmc,
              DISPLAY_DESCR   *dspl_descr)
{
int                   i;
int                   changed;
char                 *prompt;
char                 *p;
int                   new_vt100_mode;

static int            saved_lineno_mode;
static int            saved_first_line;
static int            saved_insert_mode;
static int            saved_sb_mode;


/***************************************************************
*  
*  Process vt -auto or the reverse.
*  
***************************************************************/

if (dmc->vt100.mode == DM_AUTO)
   {
      if (OPTION_VALUES[VT_IDX])
         free(OPTION_VALUES[VT_IDX]);
      OPTION_VALUES[VT_IDX] = malloc_copy("auto");
      return(0);
   }
else
   if (dmc->vt100.mode != DM_TOGGLE)
      if (OPTION_VALUES[VT_IDX])
         {
            free(OPTION_VALUES[VT_IDX]);
            OPTION_VALUES[VT_IDX] = malloc_copy((dmc->vt100.mode == DM_ON) ? "on" : "off");
         }

/***************************************************************
*  
*  Process the flag from the command.  If the value does not
*  change, return.
*  
***************************************************************/

new_vt100_mode = toggle_switch(dmc->vt100.mode, 
                               dspl_descr->vt100_mode,
                               &changed);

if (!changed)
   return(False);

DEBUG23(fprintf(stderr, "dm_vt: vt100 being turned %s\n", (new_vt100_mode ? "on" : "off"));)

/***************************************************************
*  
*  If we are going into vt100 mode, move the cursor into the
*  main window before we change the real flag.
*  
***************************************************************/

if (new_vt100_mode && (dspl_descr->cursor_buff->which_window != MAIN_PAD))
   {
      dm_position(dspl_descr->cursor_buff, dspl_descr->main_pad, dspl_descr->main_pad->first_line, strlen(get_unix_prompt()));
#ifndef WIN32
      process_redraw(dspl_descr, 0, True);
#else
      tickle_main_thread(dspl_descr);
#endif
      dm_sp(dspl_descr, NULL); /* force off setprompt mode */
   }


dspl_descr->vt100_mode = new_vt100_mode;

/***************************************************************
*  
*  We know the flag changed value.  Handle the turning on of
*  vt100 mode.
*  
***************************************************************/

if (dspl_descr->vt100_mode)
   {
      saved_main_window_token = dspl_descr->main_pad->token;
      saved_lineno_mode       = dspl_descr->show_lineno;
      saved_first_line        = dspl_descr->main_pad->first_line;
      saved_insert_mode       = dspl_descr->insert_mode;
      saved_sb_mode           = dspl_descr->sb_data->sb_mode;
      dspl_descr->insert_mode = False;
      dspl_descr->show_lineno = False;
      dspl_descr->sb_data->sb_mode = SCROLL_BAR_OFF;
      dspl_descr->main_pad->first_line = 0;
      dspl_descr->main_pad->first_char = 0;
      dspl_descr->main_pad->token = mem_init(100, False);
      prompt = get_unix_prompt();
      DEBUG23(fprintf(stderr, "dm_vt: saving prompt %s\n", prompt);)
      dspl_descr->main_pad->file_line_no = 0;
      dspl_descr->main_pad->file_col_no  = strlen(prompt);
      put_line_by_num(dspl_descr->main_pad->token, -1, prompt, INSERT);

      set_unix_prompt(dspl_descr, NULL); /* res 2/8/94 */
      normal_mcursor(dspl_descr); /* res 2/9/94 */

      dspl_descr->cursor_buff->win_line_no  = 0;
      dspl_descr->cursor_buff->win_col_no   = dspl_descr->main_pad->file_col_no;
      dspl_descr->cursor_buff->which_window = MAIN_PAD;
      dspl_descr->cursor_buff->current_win_buff = dspl_descr->main_pad;
      set_window_col_from_file_col(dspl_descr->cursor_buff);

      for (i = 0; i < power_up_tabcount; i++)
         tabstops[i] = power_up_tabstops[i];
      tabcount = power_up_tabcount;


      WRITABLE(dspl_descr->main_pad->token) = True;
      XUnmapWindow(dspl_descr->display, dspl_descr->unix_pad->x_window);

      tek_margins_set       = False;;
      vt100_resize(dspl_descr->main_pad);
   }
else
   {
      /***************************************************************
      *  
      *  The flag was turned off, restore things.
      *  
      ***************************************************************/
      if (!saved_main_window_token)
         {
            DEBUG(fprintf(stderr, "Internal error, vt100 mode is on but main window token was not saved\n");)
            return(False);
         }

      /***************************************************************
      *  Copy the last screen of data to the main pad.
      *  put the prompt in the prompt window.
      ***************************************************************/
      for (i = 0; i < dspl_descr->main_pad->file_line_no; i++)
      {
         p = get_line_by_num(dspl_descr->main_pad->token, i);
         if (p)
            put_line_by_num(saved_main_window_token, total_lines(saved_main_window_token)-1, p, INSERT);
      }
      p = get_line_by_num(dspl_descr->main_pad->token, dspl_descr->main_pad->file_line_no);
      if (p)
         {
            DEBUG23(fprintf(stderr, "dm_vt: New unix prompt %s\n", p);)
            set_unix_prompt(dspl_descr, p);
         }


      /***************************************************************
      *  Get rid of the vt main window memdata, and put back the regular one.
      ***************************************************************/
      mem_kill(dspl_descr->main_pad->token);
      dspl_descr->main_pad->token          = saved_main_window_token;
      saved_main_window_token              = NULL;
      dspl_descr->show_lineno              = saved_lineno_mode;
      dspl_descr->main_pad->window->lines_on_screen--; /* RES 11/21/95 Unixcmd window eats a line */
      dspl_descr->main_pad->file_line_no   = total_lines(dspl_descr->main_pad->token)
                                           - dspl_descr->main_pad->window->lines_on_screen;
      if (dspl_descr->main_pad->file_line_no < 0)
         dspl_descr->main_pad->file_line_no = 0;
      dspl_descr->main_pad->first_line     = 
      dspl_descr->main_pad->file_line_no   = pb_ff_scan(dspl_descr->main_pad, dspl_descr->main_pad->file_line_no);

      dspl_descr->insert_mode              = saved_insert_mode;
      dspl_descr->sb_data->sb_mode         = saved_sb_mode;
      dspl_descr->main_pad->buff_modified  = False;
      dspl_descr->main_pad->buff_ptr       = get_line_by_num(dspl_descr->main_pad->token, saved_first_line);

      dspl_descr->cursor_buff->win_line_no  = 0;
      dspl_descr->cursor_buff->win_col_no   = dspl_descr->unix_pad->file_col_no;
      dspl_descr->cursor_buff->which_window = UNIXCMD_WINDOW;
      dspl_descr->cursor_buff->current_win_buff = dspl_descr->unix_pad;
      set_window_col_from_file_col(dspl_descr->cursor_buff);

      WRITABLE(dspl_descr->main_pad->token) = False;

      /***************************************************************
      *  Delete any fancy line elements we have created.
      ***************************************************************/
      for (i = 0; i < dspl_descr->main_pad->win_lines_size; i++)
         if (dspl_descr->main_pad->win_lines[i].fancy_line != NULL)
            delete_fancy_line_list(&dspl_descr->main_pad->win_lines[i].fancy_line);

      scroll_some(0, dspl_descr, 0);
      normal_mcursor(dspl_descr); /* res 2/9/94 */

   }

/**************************************************************
*  
*  Get rid of the cursor.  The configure event will figure
*  out where to put it.
*  
***************************************************************/

erase_text_cursor(dspl_descr);

/***************************************************************
*  
*  Reset the event masks.  When going into vt100 mode, we turn
*  the mouse events off in the main window.  On exit, we turn
*  them back on.  The True forces the reset.
*  
***************************************************************/

set_event_masks(dspl_descr, True);

/* RES 5/9/97, use configure routine to replace inline code */
reconfigure(dspl_descr->main_pad, dspl_descr->display); /* in windows.c */

tty_echo_mode = -1; /* invalidate flag in pad.h res 5/3/94, handles return from telnet */


return(True); /* Send configure, window changed */


} /* end of dm_vt100 */


/************************************************************************

NAME:      vt100_parse  -   Parse incoming stream(line) and call CE(vt100) commands


PURPOSE:    This routine handles the data returned from the shell when in vt100 mode.
            It knows how to parse the vt100 escape command sequences and convert them
            to write on the ceterm window.

            The technique used is to put the data into the main window memdata
            structure use the normal drawing routines.  The pad description
            holds the current cursor position that the vt100 window thinks we
            are in. If the ce cursor is in the main pad, we keep the cursor_buff
            up to date.

PARAMETERS:

   1.  len        -  int (INPUT)
                     This is the length of the following parameter.  The length is passed
                     because it may contain imbedded nulls.

   2.  line       -  pointer to char (INPUT)
                     This is the data from the shell to be processed.  It contains
                     data and possible control sequences.

   3.  dspl_descr -  pointer to DISPLAY_DESCR (INPUT / OUTPUT)
                     This is the current display description.

GLOBAL DATA:

   The group of static flags used to show the terminal state are used and set globally by this routine.


FUNCTIONS :
   1.   Look at each character an process it as a simple character or as a command sequence.


*************************************************************************/


void vt100_parse(int            len,
                 char          *line,
                 DISPLAY_DESCR *dspl_descr)
{

int            n;
int            m;
char          *end;
int            redraw_needed = 0;
int            warp_needed;
char          *cmd_trailer;
int            count;
int            parms[20];
char           msg[512];
static char    hold_line[512] = "";  /* for commands broken across block boundaries */
static int     hold_line_len = 0;
char           work_line[MAX_LINE+sizeof(hold_line)];
#ifdef DebuG
static int     regular_chars = False;
static unsigned char    trigger_chars[] = {VT_ESC, VT_BL, VT_BS, VT_CN, VT_CR, VT_FF, VT_HT, VT_LF, VT_SI, VT_SO, VT_VT, VT__, '\0'};
#endif

DEBUG23(fprintf(stderr, "vt100: len = %d, %s held over data\n", len, (hold_line[0] ? "There is" : "No"));)

/***************************************************************
*  If we have left over data from a previous call, paste
*  it on the front of the line.
***************************************************************/
if (hold_line_len)
   {
      memcpy(work_line, hold_line, hold_line_len);
      memcpy(work_line+hold_line_len, line, len);
      line = work_line;
      hold_line[0] = '\0';
      len += hold_line_len;
      DEBUG23(fprintf(stderr, "vt100: %d, bytes of held over data, new len %d\n", hold_line_len, len);)
      hold_line_len = 0;
   }

end = line + len;

while(line < end)
{
   DEBUG23(
   if ((strchr((char *)trigger_chars, *line) != NULL) && !((*line == '_') && !tek_overstrike_mode))
      {
         if (regular_chars)
            {
               fputc('\n', stderr);
               regular_chars = False;
            }
         fprintf(stderr, "[%d,%d] ", dspl_descr->main_pad->file_line_no, dspl_descr->main_pad->file_col_no);
      }
   )

   switch(*line)
   {
    case '\0':     /* a null, ignore it */
               DEBUG23(fprintf(stderr, "vt100: Null char (0x00) found\n");)
               line++;
               break;

    case VT_BL :   /* Bell Character */
               DEBUG23(fprintf(stderr, "vt100: Bell char\n");)
               ce_XBell(dspl_descr, 0);
               line++;
               break;

    case VT_BS :   /* Backspace Character */
               DEBUG23(fprintf(stderr, "vt100: Backspace char\n");)
               redraw_needed |= vt_move_cursor(dspl_descr->main_pad, 0, -1, True, True, False, False); /* backspace one character */
               line++;
               break;

#ifdef blah
    /* remove to allow Rene Leblanc's ACDC emulator to use this character */
    case VT_CN :   /* Cancel Character */
               dm_error("vt100: Cancel Character not supported", DM_ERROR_LOG);
               line++;
               break;
#endif

    case VT_CR :   /* Carriage Return Character */
               DEBUG23(fprintf(stderr, "vt100:  Carriage Return Character \n");)
               if (linefeed_newline_mode)
                  redraw_needed |= vt_move_cursor(dspl_descr->main_pad, 1, 1, True, False, True, True); /* move down one line and to col one (first col) */
               else
                  redraw_needed |= vt_move_cursor(dspl_descr->main_pad, 0, 1, True, False, True, True); /* move to col one (first col) */
               line++;
               tty_echo_mode = -1; /* invalidate flag in pad.h res 2/9/94, handles return from telnet */
               break;

    case VT_FF :   /* Formfeed Character */
               dm_error("vt100: Formfeed Character not supported", DM_ERROR_LOG);
               line++;
               break;

    case VT_HT :   /* Horizontal Tab Character */
               DEBUG23(fprintf(stderr, "vt100:  Horizontal Tab Character\n");)
               redraw_needed |= vt_ht(dspl_descr->main_pad, 1, True /* go right */);
               line++;
               break;

    case VT_LF:   /* Linefeed Character */
               DEBUG23(fprintf(stderr, "vt100:  Linefeed Character \n");)
               redraw_needed |= vt_move_cursor(dspl_descr->main_pad, 1, 0, True, !linefeed_newline_mode, True, True); /* move down one line, move to col zero if in newline mode */
               line++;
               break;

    case VT_SI :   /* Shift In Character, Invoke the current G0 character set. */
               DEBUG23(fprintf(stderr, "vt100: Shift In Character, Invoke the current G0 character set. not supported\n");)
               line++;
               break;

    case VT_SO :   /* Shift Out Character, Invoke the current G1 character set.  */
               DEBUG23(fprintf(stderr, "vt100: Shift Out Character, Invoke the current G1 character set. not supported\n");)
               line++;
               break;

    case VT_SP :   /* Space Character */
               if (tek_overstrike_mode)
                  {
                     DEBUG23(fprintf(stderr, "\nTEK Overstrike Space Char\n");)
                     redraw_needed |= vt_move_cursor(dspl_descr->main_pad, 0, 1, True, True, False, False); /* space one character */
                  }
               else   
                  {
                     DEBUG23(fputc(' ', stderr); regular_chars = True;)
                     redraw_needed |= insert_char_in_window(dspl_descr->main_pad, ' ');
                  }
               line++;
               break;

    case VT_VT :   /* Vertical Tab Character */
               DEBUG23(fprintf(stderr, "vt100: Vertical Tab char\n");)
               redraw_needed |= vt_move_cursor(dspl_descr->main_pad, 1, 0, True, True, False, False); /* move down one line, stay in same col */
               line++;
               break;

    case VT__ :    /* Underscore Character */
               if (tek_overstrike_mode)
                  {
                     DEBUG23(fprintf(stderr, "\nTEK Overstrike Underscore Char\n");)
                     break; /* We do not do underscoring */
                  }
               else   
                  {
                     DEBUG23(fputc('_', stderr); regular_chars = True;)
                     redraw_needed |= insert_char_in_window(dspl_descr->main_pad, '_');
                  }
               line++;
               break;

    case VT_ESC :
               line++;
               if (line == end)
                  {
                     /* Incomplete vt100 command sequence, save for next call */
                     DEBUG23(fprintf(stderr, "vt100: Incomplete vt100 command sequence 'EC'\n");)
                     hold_line_len = 1;
                     hold_line[0] = VT_ESC;
                     line = end;  /* kill rest of command */
                     break;
                  }
               DEBUG23(if (*line != '[') fprintf(stderr, "vt100 EC%c ", *line);)
               switch(*line)
               {

               case 'A' :   /* VT52 Cursor Up   */
                        DEBUG23(fprintf(stderr, "  -  VT52 Cursor Up\n");)
                        if (dspl_descr->main_pad->file_line_no > tek_top_margin)
                           redraw_needed |= vt_move_cursor(dspl_descr->main_pad, -1, 0, True, True, False, False); /* move up one line, stay in same col */
                        line++;
                        break;

               case 'B' :   /* VT52 Cursor Down */
                        DEBUG23(fprintf(stderr, "  -  VT52 Cursor Down\n");)
                        if (dspl_descr->main_pad->file_line_no < tek_bottom_margin)
                           redraw_needed |= vt_move_cursor(dspl_descr->main_pad, 1, 0, True, True, False, False); /* move down one line, stay in same col */
                        line++;
                        break;

               case 'b' :   /* Enable Manual Input */
                        DEBUG23(fprintf(stderr, "  -  Enable Manual Input\n");)
                        WRITABLE(dspl_descr->main_pad->token) = True;
                        line++;
                        break;
                      
               case 'C' :   /* VT52 Cursor Right */
                        DEBUG23(fprintf(stderr, "  -  VT52 Cursor Right\n");)
                        redraw_needed |= vt_move_cursor(dspl_descr->main_pad, 0, 1, True, True, False, False); /* move right 1  col */
                        line++;
                        break;
                      
               case 'c' : /* Reset to Initial State */
                        DEBUG23(fprintf(stderr, "  -  Reset to Initial State\n");)
                        redraw_needed |= erase_area(dspl_descr->main_pad, 2, WHOLE_BUFFER_ERASE);
                        dspl_descr->insert_mode = False;
                        redraw_needed |= (TITLEBAR_MASK & FULL_REDRAW);
                        tek_margins_set = False;
                        tek_top_margin = 0;
                        tek_bottom_margin = dspl_descr->main_pad->window->lines_on_screen - 1;
                        redraw_needed |= vt_move_cursor(dspl_descr->main_pad, 1, 1, False, False, False, False); /* got to position 1,1 */
                        line++;
                        break;

               case 'D' :   /* Index  or VT52 Cursor Left */
                        DEBUG23(if (vt52_mode) fprintf(stderr, "  -  VT52 Cursor Right\n");else fprintf(stderr, "  -  Index\n");)
                        if (vt52_mode)
                           redraw_needed |= vt_move_cursor(dspl_descr->main_pad, 0, -1, True, True, False, False); /* move leftt 1  col */
                        else
                           if (dspl_descr->main_pad->file_line_no < tek_bottom_margin)
                              redraw_needed |= vt_move_cursor(dspl_descr->main_pad, 1, 0, True, True, True, True); /* move down one line */
                        line++;
                        break;
                      
               case 'E' :   /* Next Line */
                        DEBUG23(fprintf(stderr, "  -  Next Line\n");)
                        redraw_needed |= vt_move_cursor(dspl_descr->main_pad, 1, 1, True, False, True, True); /* move down one line, go to col 1 */
                        line++;
                        break;
                      
               case 'F' :   /* VT52 Enter Graphics Mode */
                        DEBUG23(fprintf(stderr, "vt100: Enter Graphics Mode not supported\n");)
                        line++;
                        break;
                      
               case 'G' :   /* VT52 Exit Graphics Mode */
                        DEBUG23(fprintf(stderr, "vt100: Exit Graphics Mode not supported\n");)
                        line++;
                        break;
                      
               case 'H' :   /* Horizontal Tab Set or  VT52 Cursor to Home*/
                        DEBUG23(if (vt52_mode) fprintf(stderr, "  -  VT52 Cursor to Home\n");else fprintf(stderr, "  -  Horizontal Tab Set \n");)
                        if (vt52_mode)
                           redraw_needed |= vt_move_cursor(dspl_descr->main_pad, 1, 1, False, False, False, False);
                        else
                           vt_add_tab_stop(dspl_descr->main_pad);
                        line++;
                        break;
                      
               case 'I' :   /* VT52 Reverse Linefeed */
                        DEBUG23(fprintf(stderr, "  - VT52 Reverse Linefeed\n");)
                        redraw_needed |= vt_move_cursor(dspl_descr->main_pad, -1, 0, True, True, False, False); /* move up one line, stay in same col */
                        line++;
                        break;
                      
               case 'J' :   /* VT52 Erase to End of Screen */
                        DEBUG23(fprintf(stderr, "  - VT52 Erase to End of Screen \n");)
                        redraw_needed |= erase_area(dspl_descr->main_pad, 0, WHOLE_BUFFER_ERASE);
                        line++;
                        break;

               case 'K' :   /* VT52  Erase to End of Line */
                        DEBUG23(fprintf(stderr, "  - VT52 Erase to End of Line \n");)
                        redraw_needed |= erase_area(dspl_descr->main_pad, 0, CURRENT_LINE_ERASE);
                        line++;
                        break;
                      
               case 'M' :   /* Reverse Index */
                        DEBUG23(fprintf(stderr, "  - Reverse Index \n");)
                        redraw_needed |= vt_move_cursor(dspl_descr->main_pad, -1, 0, True, True, True, True); /* move up one line */
                        line++;
                        break;
                      
               case 'N' :
               case 'n' :   /* Invoke G2 into GL (vt220 mode only) */
                        DEBUG23(fprintf(stderr, "  - Invoke G2 into GL (vt220 mode only), not supported\n", *line);)
                        line++;
                        break;

               case 'O' :
               case 'o' :   /* Invoke G3 into GL (vt220 mode only) */
                        DEBUG23(fprintf(stderr, "  - Invoke G3 into GL (vt220 mode only), not supported\n", *line);)
                        line++;
                        break;

               case 'Y' : /* VT52 Direct Cursor Address */
                        line++;
                        n = *line++ - 31;   
                        m = *line++ - 31;
                        if ((n > 0) && (n < tek_bottom_margin) && (m > 0) && (m < 80))
                           redraw_needed |= vt_move_cursor(dspl_descr->main_pad, n, m, False, False, False, False); 
                        DEBUG23(fprintf(stderr, "  - VT52  Direct Cursor Address to [%d,%d]\n", n, m);)
                        break;
                         
               case 'Z' :   /* Identify Terminal - Same as ansi device attributes or VT52 Identify */
                        DEBUG23(fprintf(stderr, "  - Identify Terminal \n");)
                        if (*line++ == '?')
                           break; /* echoed response of device_attributes, ignore */
                        pad2shell(device_attributes_vt100, False);
                        break;
                      
               case '7' :   /* Save Cursor   */
                        DEBUG23(fprintf(stderr, "  - Save Cursor \n");)
                        save_cursor_row = dspl_descr->main_pad->file_line_no; /* store as zero based */
                        save_cursor_col = dspl_descr->main_pad->file_col_no;
                        save_cursor_origin = tek_origin_mode_relative;
                        line++;
                        break;
                      
               case '8' :   /* Restore Cursor */
                        DEBUG23(fprintf(stderr, "  - Restore Cursor \n");)
                        tek_origin_mode_relative = save_cursor_origin;
                        redraw_needed |= vt_move_cursor(dspl_descr->main_pad, save_cursor_row+1, save_cursor_col+1, False, False, False, False); /* parms are 1 based */
                        line++;
                        break;
                      
               case '=' :   /* Keypad Application Mode or VT52 Alternate Keypad Mode */
                        DEBUG23(if (vt52_mode) fprintf(stderr, "  -  VT52 Alternate Keypad Mode \n");else fprintf(stderr, "  -  Keypad Application Mode \n");)
                        tek_application_keypad_mode = True;
                        line++;
                        break;
                      
               case '>' :   /* Keypad Numeric Mode or VT52 Exit Alternate Keypad Mode */
                        DEBUG23(if (vt52_mode) fprintf(stderr, "  -  VT52 Exit Alternate Keypad Mode \n");else fprintf(stderr, "  -  Keypad Numeric Mode \n");)
                        tek_application_keypad_mode = False;
                        line++;
                        break;
                      
               case '<' :   /* VT52 Enter Ansi Mode */
                        DEBUG23(fprintf(stderr, "  - VT52 Enter Ansi Mode \n");)
                        vt52_mode = False;
                        line++;
                        break;

               case '%' : /* SELECT CODE */
                        DEBUG23(fprintf(stderr, "  - SELECT CODE %c\n", *line);)
                        line++;  /* skip the ! */
                        switch (*line)
                        {
                        case '0':
                           dm_error("vt100: TEK mode commands not supported", DM_ERROR_LOG);
                           vt52_mode = False;
                           break;

                        case '1':
                        case '2':
                           vt52_mode = False;
                           break;

                        case '3':
                           vt52_mode = True;
                           break;

                        default:
                           snprintf(msg, sizeof(msg), "vt100: Unknown terminal command select code %c", *line);
                           dm_error(msg, DM_ERROR_LOG);
                        }
                        line++;
                        break;

               case '\'' :   /* Disable Manual Input */
                        DEBUG23(fprintf(stderr, "  - Disable Manual Input \n");)
                        WRITABLE(dspl_descr->main_pad->token) = False;
                        line++;
                        break;

               case '(' :   /* Select G0 character set */
                        line++;
                        DEBUG23(
                           fprintf(stderr, "  - Select G0 character set %c\n", *line);
                           if (*line != 'B')  /* U.S.A. ascii set */
                              {
                                 snprintf(msg, sizeof(msg), "vt100: Select G0 character set %c not supported.", *line);
                                 dm_error(msg, DM_ERROR_LOG);
                              }
                        )
                        line++;
                        break;

               case ')' :   /* Select G1 character set */
                        line++;
                        DEBUG23(
                           fprintf(stderr, "  - Select G1 character set %c\n", *line);
                           if (*line != 'B')  /* U.S.A. ascii set */
                              {
                                 snprintf(msg, sizeof(msg), "vt100: Select G1 character set %c not supported.", *line);
                                 dm_error(msg, DM_ERROR_LOG);
                              }
                        )
                        line++;
                        break;

               case '*' :   /* Select G2 character set */
                        line++;
                        DEBUG23(
                           fprintf(stderr, "  - Select G2 character set %c\n", *line);
                           if (*line != 'B')  /* U.S.A. ascii set */
                              {
                                 snprintf(msg, sizeof(msg), "vt100: Select G2 character set %c not supported.", *line);
                                 dm_error(msg, DM_ERROR_LOG);
                              }
                        )
                        line++;
                        break;

               case '+' :   /* Select G3 character set */
                        line++;
                        DEBUG23(
                           fprintf(stderr, "  - Select G3 character set %c\n", *line);
                           if (*line != 'B')  /* U.S.A. ascii set */
                              {
                                 snprintf(msg, sizeof(msg), "vt100: Select G3 character set %c not supported.", *line);
                                 dm_error(msg, DM_ERROR_LOG);
                              }
                           )
                        line++;
                        break;

               case '~' :   /* Invoke G1 into GR (vt220 mode only) */
                        DEBUG23(fprintf(stderr, "  - Invoke G1 into GR (vt220 mode only), not supported\n", *line);)
                        line++;
                        break;

               case '}' :   /* Invoke G2 into GR (vt220 mode only) */
                        DEBUG23(fprintf(stderr, "  - Invoke G2 into GR (vt220 mode only), not supported\n", *line);)
                        line++;
                        break;

               case '|' :   /* Invoke G3 into GR (vt220 mode only) */
                        DEBUG23(fprintf(stderr, "  - Invoke G3 into GR (vt220 mode only), not supported\n", *line);)
                        line++;
                        break;

               case ' ' :   /* C1 Control Transmission */
                        line++;
                        DEBUG23(fprintf(stderr, "  - C1 Control Transmission %c not supported, F=7bit, G=8bit\n", *line);)
                        line++;
                        break;

               case '#' :   /* Report Syntax Mode */
                        line++;
                        switch(*line){
                        case '3' :   /* Double Height Line, Top half    */
                                  dm_error("vt100: TEK Double Height Line not supported", DM_ERROR_LOG);
                                  break;
                        case '4' :   /* Double Height Line, Bottom half */
                                  dm_error("vt100: TEK Double Height Line not supported", DM_ERROR_LOG);
                                  break;

                        case '5' :   /* Single Width Line */
                                  dm_error("vt100: TEK Single Width Line not supported", DM_ERROR_LOG);
                                  break;

                        case '6' :   /* Double Width Line */
                                  dm_error("vt100: TEK Double Width Line not supported", DM_ERROR_LOG);
                                  break;

                        case '!' :
                                  line++;
                                  if (*line != '0') 
                                      dm_error("VT100 'ESC#!0' mode error", DM_ERROR_LOG);
                                  break;
                        }
                        line++;
                        break;

               case '[' :
                        line++;
                        cmd_trailer = strpbrk(line, " ABCcDfgHhIJKLlMmnPqRrSTXZ@");
                        if (cmd_trailer == NULL)
                           {
                              /* Incomplete vt100 command sequence, save for next call */
                              DEBUG23(fprintf(stderr, "vt100: Incomplete vt100 command sequence 'EC[%.*s'\n", end-line, line);)
                              line -= 2; /* put back the EC[ */
                              hold_line_len = end - line;
                              memcpy(hold_line, line, hold_line_len);
                              line = end;  /* kill rest of command */
                              break;
                           }

                        DEBUG23(fprintf(stderr, "vt100 EC[%.*s%c ", cmd_trailer-line, line, *cmd_trailer);)
                        switch(*cmd_trailer)
                        {
                        case ' ' :
                                  get_numbers(line, cmd_trailer-line, parms);
                                  if (parms[0] == 0)
                                     parms[0] = 1;
                                  cmd_trailer++;
                                  if (*cmd_trailer == '@')  /* Scroll Left */
                                     {
                                        DEBUG23(fprintf(stderr, "  - Scroll Left \n");)
                                        dspl_descr->main_pad->first_char += parms[0];
                                     }
                                  else
                                     if (*cmd_trailer == 'A') /* Scroll Right */
                                        {
                                           DEBUG23(fprintf(stderr, "  - Scroll Right \n");)
                                           dspl_descr->main_pad->first_char -= parms[0];
                                           if (dspl_descr->main_pad->first_char < 0)
                                              dspl_descr->main_pad->first_char = 0;
                                        }
                                  break;

                        case 'A' :   /* Cursor Up */
                                  DEBUG23(fprintf(stderr, "  - Cursor Up \n");)
                                  count = get_numbers(line, cmd_trailer-line, parms);
                                  if (count)
                                     redraw_needed |= vt_move_cursor(dspl_descr->main_pad, -(parms[0]), 0,  True, True, tek_origin_mode_relative, False); /* move up serveral lines */
                                  else
                                     redraw_needed |= vt_move_cursor(dspl_descr->main_pad, -1, 0, True, True, tek_origin_mode_relative, False);          /* move up one line */
                                  break;
    
                        case 'B' :  /* Cursor Down */
                                  DEBUG23(fprintf(stderr, "  - Cursor Down \n");)
                                  count = get_numbers(line, cmd_trailer-line, parms);
                                  if (count)
                                     redraw_needed |= vt_move_cursor(dspl_descr->main_pad, parms[0], 0,  True, True, tek_origin_mode_relative, False); /* move down serveral lines */
                                  else
                                     redraw_needed |= vt_move_cursor(dspl_descr->main_pad, 1, 0, True, True, tek_origin_mode_relative, False);          /* move down one line */
                                  break;
    
                        case 'C' :  /* Cursor Forward */
                                  DEBUG23(fprintf(stderr, "  - Cursor Forward \n");)
                                  count = get_numbers(line, cmd_trailer-line, parms);
                                  if (count)
                                     redraw_needed |= vt_move_cursor(dspl_descr->main_pad, 0, parms[0], True, True, False, False);    /* forward space several characters */
                                  else
                                     redraw_needed |= vt_move_cursor(dspl_descr->main_pad, 0, 1, True, True, False, False);           /* forward space one character */
                                  break;

                        case 'c' :  /* Device Attributes */
                        case '>' :  /* Device Attributes */
                                  if (*line == '?')
                                     break; /* echoed response of device_attributes, ignore */
                                  DEBUG23(fprintf(stderr, "  - Request for device attributes, esc[c\n");)
                                  pad2shell(device_attributes_vt102, False);
                                  break;
    
                        case 'D' :   /* Cursor Backward */
                                  DEBUG23(fprintf(stderr, "  - Cursor Backward \n");)
                                  count = get_numbers(line, cmd_trailer-line, parms);
                                  if (count)
                                     redraw_needed |= vt_move_cursor(dspl_descr->main_pad, 0, -(parms[0]), True, True, False, False); /* backspace several characters */
                                  else
                                     redraw_needed |= vt_move_cursor(dspl_descr->main_pad, 0, -1, True, True, False, False);          /* backspace one character */
                                  break;
    
                        case 'f' :   /* Horizontal and Vertical Position */
                                  DEBUG23(fprintf(stderr, "  - Horizontal and Vertical Position \n");)
                                  get_numbers(line, cmd_trailer-line, parms);
                                  if (tek_origin_mode_relative)
                                     {
                                        parms[0] += tek_top_margin;
                                        parms[1] += tek_top_margin;
                                        if (parms[1] > tek_bottom_margin)
                                           parms[1] = tek_bottom_margin;
                                     }
                                  redraw_needed |= vt_move_cursor(dspl_descr->main_pad, parms[0], parms[1], False, False, tek_origin_mode_relative, False); /* move location */
                                  break;

                        case 'g' :   /* Tab Clear */
                                  DEBUG23(fprintf(stderr, "  - Tab Clear \n");)
                                  get_numbers(line, cmd_trailer-line, parms);
                                  vt_erase_tab_stop(dspl_descr->main_pad, parms[0]);
                                  break;

                        case 'H' :    /* Cursor Position */
                                  get_numbers(line, cmd_trailer-line, parms);
                                  DEBUG23(fprintf(stderr, "  - Cursor Position\n");)
                                  redraw_needed |= vt_move_cursor(dspl_descr->main_pad, parms[0], parms[1], False, False, tek_origin_mode_relative, False); /* move to a location */
                                  break;

                        case 'h' :  /* Set TEK Mode */
                                  DEBUG23(fprintf(stderr, "  - Set TEK Mode \n");)
                                  redraw_needed |= change_tek_mode(dspl_descr->main_pad, line, cmd_trailer-line, True /* not set mode */);
                                  break;

                        case 'I' :  /* Cursor Horizontal Tab */
                                  DEBUG23(fprintf(stderr, "  - Cursor Horizontal Tab \n");)
                                  get_numbers(line, cmd_trailer-line, parms);
                                  if (parms[0] == 0)
                                     parms[0] = 1;
                                  redraw_needed |= vt_ht(dspl_descr->main_pad, parms[0], True /* go right */);
                                  break;

                        case 'J' :   /* Erase in Display */
                                  get_numbers(line, cmd_trailer-line, parms);
                                  DEBUG23(fprintf(stderr, "  - Erase in Display %d \n", parms[0]);)
                                  redraw_needed |= erase_area(dspl_descr->main_pad, parms[0], WHOLE_BUFFER_ERASE);
                                  break;
   
                        case 'K' :   /* Erase In Line */
                                  get_numbers(line, cmd_trailer-line, parms);
                                  DEBUG23(fprintf(stderr, "  - Erase In Line %d\n", parms[0]);)
                                  redraw_needed |= erase_area(dspl_descr->main_pad, parms[0], CURRENT_LINE_ERASE);
                                  break;

                        case 'L' :   /* Insert Lines */
                                  DEBUG23(fprintf(stderr, "  - Insert Lines \n");)
                                  get_numbers(line, cmd_trailer-line, parms);
                                  redraw_needed |= insert_lines(dspl_descr->main_pad, parms[0]);
                                  break;

                        case 'l' :   /* Reset TEK Mode */
                                  DEBUG23(fprintf(stderr, "  - Reset TEK Mode \n");)
                                  redraw_needed |= change_tek_mode(dspl_descr->main_pad, line, cmd_trailer-line, False /* not set mode */);
                                  break;

                        case 'M' :   /* Delete Line */
                                  DEBUG23(fprintf(stderr, "  - Delete Line \n");)
                                  count = get_numbers(line, cmd_trailer-line, parms);
                                  if (!count || !parms[0])
                                     parms[0] = 1;
                                  for (n = 0; n < parms[0]; n++)
                                      delete_line_by_num(dspl_descr->main_pad->token, dspl_descr->main_pad->file_line_no, 1);
                                  break;

                        case 'm' :   /* Select Graphic Rendition */
                                  DEBUG23(fprintf(stderr, "  - Select Graphic Rendition \n");)
                                  vt_sgr(dspl_descr->main_pad, line, cmd_trailer-line, parms);
                                  break;

                        case 'n' :   /* Device Status Report */
                                  DEBUG23(fprintf(stderr, "  - Device Status Report \n");)
                                  count = get_numbers(line, cmd_trailer-line, parms);
                                  if (count)
                                     if (parms[0] == 5)  /* request for status report */
                                        {
                                           snprintf(msg, sizeof(msg), "%c[0n", VT_ESC);
                                           pad2shell(msg, False);
                                        }
                                     else
                                        {
                                           snprintf(msg, sizeof(msg), "%c[%d;%dR", VT_ESC, dspl_descr->cursor_buff->win_line_no, dspl_descr->cursor_buff->win_col_no);
                                           pad2shell(msg, False);
                                        }
                                  break;

                        case 'P' :  /* Delete Character(s) */
                                  DEBUG23(fprintf(stderr, "  - Delete Character(s) \n");)
                                  get_numbers(line, cmd_trailer-line, parms);
                                  delete_chars_in_window(dspl_descr->main_pad, parms[0]);
                                  break;

                        case 'q' :  /* Select Character Attributes (DECSCA) */
                                  DEBUG23(
                                     fprintf(stderr, "  - Select Character Attributes (DECSCA) \n");
                                     get_numbers(line, cmd_trailer-line, parms);
                                     if (parms[0] != 0)
                                        fprintf(stderr, "Character attribute %d not supported\n", parms[0]);
                                  )
                                  break;

                        case 'R' :  /* Cursor Position Report */
                                  DEBUG23(fprintf(stderr, "  - Cursor Position Report echo, ignored \n");)
                                  /* echoed CPR request sent via case 'c' in this select, ignore the echo */
                                  break;

                        case 'r' :   /* Set Top and Bottom Margins */
                                  DEBUG23(fprintf(stderr, "  - Set Top and Bottom Margins \n");)
                                  get_numbers(line, cmd_trailer-line, parms);
                                  set_scroll_margins(dspl_descr->main_pad, parms[0], parms[1]);
                                  break;

                        case 'S' :   /* Scroll Up */
                                  dm_error("vt100: Scroll Up not supported", DM_ERROR_LOG);
                                  break;

                        case 'T' :   /* Scroll Down */
                                  dm_error("vt100: Scroll Down not supported", DM_ERROR_LOG);
                                  break;

                        case 'X' :   /* Erase Character */
                                  DEBUG23(fprintf(stderr, "  - Erase Character \n");)
                                  get_numbers(line, cmd_trailer-line, parms);
                                  erase_chars_in_window(dspl_descr->main_pad, parms[0]);
                                  break;

                        case 'Z' :  /* Reverse horizontal Tab */
                                  DEBUG23(fprintf(stderr, "  - Reverse horizontal Tab \n");)
                                  get_numbers(line, cmd_trailer-line, parms);
                                  if (parms[0] == 0)
                                     parms[0] = 1;
                                  redraw_needed |= vt_ht(dspl_descr->main_pad, parms[0], False /* go left */);
                                  break;

                        case '@' :   /* Insert blank Character */
                                  DEBUG23(fprintf(stderr, "  - Insert blank Character \n");)
                                  get_numbers(line, cmd_trailer-line, parms);
                                  if (parms[0] == 0)
                                     parms[0] = 1;
                                  for (count = 0; count < parms[0]; count++)
                                     redraw_needed |= insert_char_in_window(dspl_descr->main_pad, ' ');
                                  break;

                        default:
                           snprintf(msg, sizeof(msg), "VT100 'ESC[n' mode error, char = %c (0x%02X)", *cmd_trailer, *cmd_trailer);
                           dm_error(msg, DM_ERROR_LOG);
                        } /* end of switch on command trailer */

                        line = cmd_trailer + 1;
                        break;  /* end of case [ */
              default:
                  snprintf(msg, sizeof(msg), "VT100 'ESCn' mode error, char = %c (0x%02X)", *line, *line);
                  dm_error(msg, DM_ERROR_LOG);
              } /* switch in case VT_ESC */
              break;
     default:
              DEBUG23(fputc(*line, stderr); regular_chars = True;)
              redraw_needed |= insert_char_in_window(dspl_descr->main_pad, *line++);
              break;
    }  /* switch(input)  *line  */
} /* while input */

if (dspl_descr->cursor_buff->which_window == MAIN_PAD)
   {
      dspl_descr->cursor_buff->win_line_no  = dspl_descr->main_pad->file_line_no - dspl_descr->main_pad->first_line;
      dspl_descr->cursor_buff->y            = dspl_descr->main_pad->window->sub_y + (dspl_descr->main_pad->window->line_height * dspl_descr->cursor_buff->win_line_no);
      set_window_col_from_file_col(dspl_descr->cursor_buff);  /* sets win_col and x */
      dspl_descr->cursor_buff->up_to_snuff  = True;
      warp_needed = True;
   }
else
   warp_needed = False;

#ifndef WIN32
process_redraw(dspl_descr, redraw_needed, warp_needed);
#else
tickle_main_thread(dspl_descr);
#endif

}  /*  vt100_parse() */


/************************************************************************

NAME:      vt100_pad2shell   -   send data to the shell when in vt100 mode


PURPOSE:    This routine handles getting a list of numeric parameters from a string
            and puts them in an array of integers.  The count is returned.

PARAMETERS:

   1.  main_window_cur_descr - pointer to PAD_DESCR (INPUT / OUTPUT)
                     This is the buffer description for the main window.

   2.  line        - pointer to char (INPUT)
                     This is the string to be sent to the shell.

   3.  newline     - int (INPUT)
                     This flag is set to true if a newline is to be added
                     to the data sent to the shell.  This flag is passed
                     throught to pad2shell in pad.c.

FUNCTIONS :

   1.   If we are in tek_local_echo_mode, copy the data to the screen.

   2.   Send the data to the shell.

   3.   If we are sending a newline character clear the tty echo mode
        flag in pad.h.  This is used in conjunction with AUTOVT mode.

   4.   If we outrun the shell's ability to take data, wait for until
        we send it all.

RETURNED VALUE:
   rc   -   int
            The returned value specifies whether the data was successfully
            sent to the shell.  

*************************************************************************/

int  vt100_pad2shell(PAD_DESCR    *main_window_cur_descr,
                     char         *line,
                     int           newline)
{
int     rc;
int     wait_rc;
char   *p;

if (tek_local_echo_mode)
   for (p = line; *p; p++)
   {
      insert_char_in_window(main_window_cur_descr, *p);
       if ((*p == '\n') || (*line == '\r'))
          tty_echo_mode = -1; /* invalidate flag in pad.h res 2/8/94 */
   }
else
   for (p = line; *p; p++)
      if ((*p == '\n') || (*line == '\r'))
         tty_echo_mode = -1; /* invalidate flag in pad.h res 2/8/94 */

rc = pad2shell(line, newline);

while (rc == PAD_BLOCKED)
{
   wait_rc = pad2shell_wait();
   if (wait_rc <= 0)
      rc = PAD_FAIL;
   else
      rc = pad2shell(line, newline);
}

if (rc == PAD_PARTIAL)
   rc = PAD_SUCCESS;

return(rc);

} /* end of vt100_pad2shell */

#define ESC_2C(s1,c1,c2) {s1[0] = VT_ESC; s1[1] = c1; s1[2] = c2; s1[3] = '\0';}
#define ESC_1C(s1,c)     {s1[0] = VT_ESC; s1[1] = c;              s1[2] = '\0';}

void  vt100_key_trans(KeySym       keysym,
                      char        *str)
{

if (vt52_mode)
   if (tek_application_keypad_mode)
      switch(keysym)
      {
      case XK_KP_0:
         ESC_2C(str, '?', 'p')
         break;
      case XK_KP_1:
         ESC_2C(str, '?', 'q')
         break;
      case XK_KP_2:
         ESC_2C(str, '?', 'r')
         break;
      case XK_KP_3:
         ESC_2C(str, '?', 's')
         break;
      case XK_KP_4:
         ESC_2C(str, '?', 't')
         break;
      case XK_KP_5:
         ESC_2C(str, '?', 'u')
         break;
      case XK_KP_6:
         ESC_2C(str, '?', 'v')
         break;
      case XK_KP_7:
         ESC_2C(str, '?', 'w')
         break;
      case XK_KP_8:
         ESC_2C(str, '?', 'x')
         break;
      case XK_KP_9:
         ESC_2C(str, '?', 'y')
         break;
      case XK_KP_Subtract:
         ESC_2C(str, '?', 'm')
         break;
      case XK_KP_Separator:
         ESC_2C(str, '?', 'l')
         break;
      case XK_KP_Decimal:
         ESC_2C(str, '?', 'n')
         break;
      case XK_KP_Enter:
         ESC_2C(str, '?', 'M')
         break;
      case XK_F5:
         ESC_1C(str, 'P')
         break;
      case XK_F6:
         ESC_1C(str, 'Q')
         break;
      case XK_F7:
         ESC_1C(str, 'R')
         break;
      case XK_F8:
         ESC_1C(str, 'S')
         break;
      default:
         break;
      }
   else
      switch(keysym)
      {
      case XK_F5:
         ESC_1C(str, 'P')
         break;
      case XK_F6:
         ESC_1C(str, 'Q')
         break;
      case XK_F7:
         ESC_1C(str, 'R')
         break;
      case XK_F8:
         ESC_1C(str, 'S')
         break;
      default:
         break;
      }
else
   if (tek_application_keypad_mode)
      switch(keysym)
      {
      case XK_KP_0:
         ESC_2C(str, 'O', 'p')
         break;
      case XK_KP_1:
         ESC_2C(str, 'O', 'q')
         break;
      case XK_KP_2:
         ESC_2C(str, 'O', 'r')
         break;
      case XK_KP_3:
         ESC_2C(str, 'O', 's')
         break;
      case XK_KP_4:
         ESC_2C(str, 'O', 't')
         break;
      case XK_KP_5:
         ESC_2C(str, 'O', 'u')
         break;
      case XK_KP_6:
         ESC_2C(str, 'O', 'v')
         break;
      case XK_KP_7:
         ESC_2C(str, 'O', 'w')
         break;
      case XK_KP_8:
         ESC_2C(str, 'O', 'x')
         break;
      case XK_KP_9:
         ESC_2C(str, 'O', 'y')
         break;
      case XK_KP_Subtract:
         ESC_2C(str, 'O', 'm')
         break;
      case XK_KP_Separator:
         ESC_2C(str, 'O', 'l')
         break;
      case XK_KP_Decimal:
         ESC_2C(str, 'O', 'n')
         break;
      case XK_KP_Enter:
         ESC_2C(str, 'O', 'M')
         break;
      case XK_F5:
         ESC_2C(str, 'O', 'P')
         break;
      case XK_F6:
         ESC_2C(str, 'O', 'Q')
         break;
      case XK_F7:
         ESC_2C(str, 'O', 'R')
         break;
      case XK_F8:
         ESC_2C(str, 'O', 'S')
         break;
      default:
         break;
      }
   else
      switch(keysym)
      {
      case XK_F5:
         ESC_2C(str, 'O', 'P')
         break;
      case XK_F6:
         ESC_2C(str, 'O', 'Q')
         break;
      case XK_F7:
         ESC_2C(str, 'O', 'R')
         break;
      case XK_F8:
         ESC_2C(str, 'O', 'S')
         break;
      default:
         break;
      }

if (tek_cursor_key_mode)
   switch(keysym)
   {
   case XK_F1:
   case XK_Up:
      ESC_2C(str, 'O', 'A')
      break;
   case XK_F2:
   case XK_Down:
      ESC_2C(str, 'O', 'B')
      break;
   case XK_F3:
   case XK_Left:
      ESC_2C(str, 'O', 'D')
      break;
   case XK_F4:
   case XK_Right:
      ESC_2C(str, 'O', 'C')
      break;
   case XK_Prior:
      str[0] = 0x02;  /* ^b */
      str[1] = '\0';
      break;
   case XK_Next:
      str[0] = 0x06;  /* ^f */
      str[1] = '\0';
      break;
   default:
      break;
   }
else
   switch(keysym)
   {
   case XK_F1:
   case XK_Up:
      ESC_2C(str, '[', 'A')
      break;
   case XK_F2:
   case XK_Down:
      ESC_2C(str, '[', 'B')
      break;
   case XK_F3:
   case XK_Left:
      ESC_2C(str, '[', 'D')
      break;
   case XK_F4:
   case XK_Right:
      ESC_2C(str, '[', 'C')
      break;
   default:
      break;
   }



} /* end of vt100_key_trans */


void  vt100_resize(PAD_DESCR    *main_window_cur_descr)
{

while(total_lines(main_window_cur_descr->token) > main_window_cur_descr->window->lines_on_screen)
{
   put_line_by_num(saved_main_window_token,
                   total_lines(saved_main_window_token)-1,
                   get_line_by_num(main_window_cur_descr->token, tek_top_margin),
                   INSERT);
   delete_line_by_num(main_window_cur_descr->token, tek_top_margin, 1);
   main_window_cur_descr->file_line_no--;
   if (main_window_cur_descr->file_line_no < 0)
      main_window_cur_descr->file_line_no = 0;
}

if (!tek_margins_set)
   {
      tek_top_margin = 0;
      tek_bottom_margin = main_window_cur_descr->window->lines_on_screen - 1;
   }

} /* end of vt100_resize */


/************************************************************************

NAME:      vt_move_cursor               -   position the vt100 cursor


PURPOSE:    This routine handles the vt100 dm command.  It takes care of setting
            up and taking down vt100 mode.

PARAMETERS:

   1.  main_window_cur_descr - pointer to BUFF_DESCR (INPUT)
                     This is main window description.

   2.  row        -  int (INPUT)
                     This is row from the command.  It is either an absolute
                     screen row number or a relative increment to the current
                     row number.  The rel_row flag determines which.
                     If it is an absolute row number, the parm is one based,
                     the values in the main_window_cur_descr are zero based.

   3.  col        -  int (INPUT)
                     This is col from the command.  It is either an absolute
                     screen col number or a relative increment to the current
                     col number.  The rel_col flag determines which.
                     If it is an absolute col number, the parm is one based,
                     the values in the main_window_cur_descr are zero based.

   4.  rel_row    -  int (INPUT)
                     This is flag modifies how the row parameter is interpreted.
                     Values:
                          False -  Interpret row as an absolute 1 based row number on the screen.
                          True  -  Interpret row as a relative value with respect to the current row.

   5.  rel_row    -  int (INPUT)
                     This is flag modifies how the col parameter is interpreted.
                     Values:
                          False -  Interpret col as an absolute 1 based column number on the screen.
                          True  -  Interpret col as a relative value with respect to the current column.

   6.  stop_within_scroll -  int (INPUT)
                     This is flag specifies whether the movement is allowed outside the "scroll region."
                     Values:
                          True  -  Stay within the scroll region
                          False -  Use the full dialog buffer.

   6.  add_scroll_lines -  int (INPUT)
                     This flag specifies whether scroll lines are to be added when the user scrolls outside the
                     bounds of the dialog area or scroll area as defined by the stop_within_scroll flag.
                     Values:
                          True  -  Add lines
                          False -  Do not add lines

FUNCTIONS :

   1.   Move the file and window cursor positions to the correct place.


*************************************************************************/

/* ARGSUSED */
static int vt_move_cursor(PAD_DESCR    *main_window_cur_descr,
                          int           row,
                          int           col,
                          int           rel_row,
                          int           rel_col,
                          int           stop_within_scroll,
                          int           add_scroll_lines)
{
int   cols_on_screen = (main_window_cur_descr->window->sub_width / main_window_cur_descr->window->font->max_bounds.width);
int   row_changed;
int   redraw_needed;
int   i;

while (total_lines(main_window_cur_descr->token) < main_window_cur_descr->window->lines_on_screen)
   put_line_by_num(main_window_cur_descr->token, total_lines(main_window_cur_descr->token)-1, "", INSERT);
   

/***************************************************************
*  If we are changing rows, save the current buffer.
***************************************************************/
if ((row && rel_row) || (!rel_row && ((row - 1) != main_window_cur_descr->file_line_no)))
   {
      flush(main_window_cur_descr);

      if ((main_window_cur_descr->redraw_start_line > main_window_cur_descr->file_line_no) || (main_window_cur_descr->redraw_start_line == -1))  /* not set yet */
         main_window_cur_descr->redraw_start_line = main_window_cur_descr->file_line_no;
      redraw_needed = (MAIN_PAD_MASK & PARTIAL_REDRAW);
      row_changed = True;
   }
else
   {
      main_window_cur_descr->redraw_start_col = 0;  /* redraw the whole line in vt100 mode */
      if ((main_window_cur_descr->redraw_start_line > main_window_cur_descr->file_line_no) || (main_window_cur_descr->redraw_start_line == -1))  /* not set yet */
         main_window_cur_descr->redraw_start_line = main_window_cur_descr->file_line_no;
      redraw_needed = (MAIN_PAD_MASK & PARTIAL_LINE);
      row_changed = False;
   }

/***************************************************************
*  Adjust the row value.
***************************************************************/
if (rel_row)
   main_window_cur_descr->file_line_no  += row;
else
   main_window_cur_descr->file_line_no   = row - 1; /* switch from 1 based to zero based */

/***************************************************************
*  If we need to, do some scrolling.
***************************************************************/
if (rel_row && add_scroll_lines)
   {
      /***************************************************************
      *  Scroll up, add blank lines to the bottom take lines off the top.
      ***************************************************************/
      if (row > 0)
         while (main_window_cur_descr->file_line_no > tek_bottom_margin)
            {
               put_line_by_num(main_window_cur_descr->token, tek_bottom_margin, "", INSERT);
               /* copy line being scrolled out to the saved pad */
               put_line_by_num(saved_main_window_token,
                               total_lines(saved_main_window_token)-1,
                               get_line_by_num(main_window_cur_descr->token, tek_top_margin),
                               INSERT);
               delete_line_by_num(main_window_cur_descr->token, tek_top_margin, 1);
               main_window_cur_descr->file_line_no--;
               redraw_needed |= (FULL_REDRAW & MAIN_PAD_MASK);
               if (main_window_cur_descr->win_lines[tek_top_margin].fancy_line)
                  delete_fancy_line_list(&main_window_cur_descr->win_lines[tek_top_margin].fancy_line);
               for (i = tek_top_margin; i < tek_bottom_margin; i++)
                  main_window_cur_descr->win_lines[i] = main_window_cur_descr->win_lines[i+1];
               main_window_cur_descr->win_lines[tek_bottom_margin].fancy_line = NULL;
            }
      /***************************************************************
      *  Scroll down, take lines off the bottom and add blank lines at the top.
      ***************************************************************/
      if (row < 0)
         while (main_window_cur_descr->file_line_no < tek_top_margin)
            {
               delete_line_by_num(main_window_cur_descr->token, tek_bottom_margin, 1);
               put_line_by_num(main_window_cur_descr->token, tek_top_margin - 1, "", INSERT);
               main_window_cur_descr->file_line_no++;
               redraw_needed |= (FULL_REDRAW & MAIN_PAD_MASK);
               if (main_window_cur_descr->win_lines[tek_bottom_margin].fancy_line)
                  delete_fancy_line_list(&main_window_cur_descr->win_lines[tek_bottom_margin].fancy_line);
               for (i = tek_bottom_margin; i < tek_top_margin; i--)
                  main_window_cur_descr->win_lines[i+1] = main_window_cur_descr->win_lines[i];
               main_window_cur_descr->win_lines[tek_top_margin].fancy_line = NULL;
            }
   }

/***************************************************************
*  Make sure we are still on screen.
***************************************************************/
#ifdef blah
/* removed 10/12/93 for hp/ux dbx, RES */
if (stop_within_scroll)
   {
      if (main_window_cur_descr->file_line_no > tek_bottom_margin)
         main_window_cur_descr->file_line_no = tek_bottom_margin;
      else
         if (main_window_cur_descr->file_line_no < tek_top_margin)
            main_window_cur_descr->file_line_no = tek_top_margin;
   }
else
   {
#endif
      if (main_window_cur_descr->file_line_no >= main_window_cur_descr->window->lines_on_screen)
         main_window_cur_descr->file_line_no = main_window_cur_descr->window->lines_on_screen - 1;
      else
         if (main_window_cur_descr->file_line_no < 0)
           main_window_cur_descr->file_line_no = 0;
#ifdef blah
   }     
#endif


/***************************************************************
*  Adjust the col value.
***************************************************************/
if (rel_col)
   main_window_cur_descr->file_col_no  += col;
else
   main_window_cur_descr->file_col_no   = col - 1; /* switch from 1 based to zero based */

/***************************************************************
*  Make sure we are still on screen.
***************************************************************/
if (main_window_cur_descr->file_col_no > cols_on_screen)
   main_window_cur_descr->file_col_no = cols_on_screen - 1;
else
   if (main_window_cur_descr->file_col_no < 0)
      main_window_cur_descr->file_col_no = 0;

if (redraw_needed & (MAIN_PAD_MASK & PARTIAL_REDRAW))
   if (main_window_cur_descr->redraw_start_line > main_window_cur_descr->file_line_no)
      main_window_cur_descr->redraw_start_line = main_window_cur_descr->file_line_no;

if (row_changed)
   main_window_cur_descr->buff_ptr = get_line_by_num(main_window_cur_descr->token, main_window_cur_descr->file_line_no);


return(redraw_needed);
   
} /* end of vt_move_cursor */


/************************************************************************

NAME:      get_numbers   -   get a list of numbers separated by semicolons


PURPOSE:    This routine handles getting a list of numeric parameters from a string
            and puts them in an array of integers.  The count is returned.

PARAMETERS:

   1.  line        - pointer to char (INPUT)
                     This is the line of parameters.  This string may not be
                     null terminated.

   2.  len         - pointer to char (INPUT)
                     This is the length of the parm line.

   3.  numbers     - pointer to int (an array of ints) (OUTPUT)
                     This is the output parameter where the values are placed.


FUNCTIONS :

   1.   Walk the input string getting each element of the list of numbers.

   2.   Flag bad characters.

   3.   Return the count of numbers extracted.


*************************************************************************/

static int  get_numbers(char   *line,
                        int     len,
                        int    *numbers)
{
int       n;
int       count = 0;
char     *p = line;
char     *end = line + len;

char      msg[512];

/***************************************************************
*  The calling code counts on this intialization.
***************************************************************/
numbers[0] = 0;
numbers[1] = 0;

while(p < end)
{
   n = 0;
   while ((*p <= '9') && (*p >= '0'))
      n = (n * 10) + (*p++ - '0');     

   numbers[count++] = n;
   if (p < end)
      if (*p == ';')
         p++;
      else
         {
            snprintf(msg, sizeof(msg), "VT100: Bad character in number string (%c) in vt100 cmd sequence line (%s)", *p, line);
            dm_error(msg, DM_ERROR_LOG);
            break;
         }
   
}

return(count);

} /* end of get_numbers */

static int insert_char_in_window(PAD_DESCR    *main_window_cur_descr,
                                 char          insert_char)
{
char                  insert_string[2];
char                 *line;
int                   redraw_needed = 0;
int                   cols_on_screen = (main_window_cur_descr->window->sub_width / main_window_cur_descr->window->font->max_bounds.width);

FANCY_LINE           *this_line_fancy;
FANCY_LINE           *temp_fancy_line;

insert_string[0] = insert_char;
insert_string[1] = '\0';


/***************************************************************
*  
*  If we are in autowrap mode and we are past the end of
*  the screen, wrap.
*  
***************************************************************/

if (tek_autowrap_mode && (main_window_cur_descr->file_col_no >= cols_on_screen))
   {
      DEBUG23(fprintf(stderr, "\n");)
      redraw_needed |= vt_move_cursor(main_window_cur_descr, 1, 0, True, False, True, True); /* move down one line and move to col zero */
      DEBUG23(fprintf(stderr, "vt100:  AutoWrapping to [%d,%d] cols on screen %d\n", main_window_cur_descr->file_line_no, main_window_cur_descr->file_col_no, cols_on_screen);)
   }

/***************************************************************
*  
*  Put the character in the window
*  
***************************************************************/

if (main_window_cur_descr->buff_ptr != main_window_cur_descr->work_buff_ptr)
   {
      line = get_line_by_num(main_window_cur_descr->token, main_window_cur_descr->file_line_no);
      strcpy(main_window_cur_descr->work_buff_ptr, line);
      main_window_cur_descr->buff_modified = True;
      main_window_cur_descr->buff_ptr = main_window_cur_descr->work_buff_ptr;
   }

insert_in_buff(main_window_cur_descr->buff_ptr, insert_string, main_window_cur_descr->file_col_no, *main_window_cur_descr->insert_mode, MAX_LINE);
main_window_cur_descr->file_col_no++;

/***************************************************************
*  
*  Figure out what type of redrawing is needed.
*  
***************************************************************/

if (insert_char == '\f')  /* putting in a form feed forces a partial redraw */
   {
      if ((main_window_cur_descr->redraw_start_line == -1) || (main_window_cur_descr->redraw_start_line > main_window_cur_descr->file_line_no))
         main_window_cur_descr->redraw_start_line = main_window_cur_descr->file_line_no;
      redraw_needed |= (MAIN_PAD_MASK & PARTIAL_REDRAW);
   }
else
   if (main_window_cur_descr->redraw_start_line == -1)  /* not set yet */
      {
         main_window_cur_descr->redraw_start_line = main_window_cur_descr->file_line_no;
         redraw_needed |= (MAIN_PAD_MASK & PARTIAL_LINE);
      }
   else
      if (main_window_cur_descr->redraw_start_line == main_window_cur_descr->file_line_no)
         redraw_needed |= (MAIN_PAD_MASK & PARTIAL_LINE);
      else
         {
            if (main_window_cur_descr->redraw_start_line > main_window_cur_descr->file_line_no)
               main_window_cur_descr->redraw_start_line = main_window_cur_descr->file_line_no;
            redraw_needed |= (MAIN_PAD_MASK & PARTIAL_REDRAW);
         }

/***************************************************************
*  
*  Look at the Fancy Line data to see if there are any special
*  graphics renditions for this character.  Or maybe we have
*  to turn off the special graphics.  As fancy line elements
*  tend to get extended as data is drawn, we stick with the
*  current one till we are more than 1 character past the end.
*  However, if that one character puts us inside a new fancy
*  line element one, we will use this new element.
*  This is the if check after the while loop.
*  
***************************************************************/

this_line_fancy = main_window_cur_descr->win_lines[main_window_cur_descr->file_line_no].fancy_line;
while(this_line_fancy && this_line_fancy->end_col < main_window_cur_descr->file_col_no-1)
   this_line_fancy = this_line_fancy->next;

if ((this_line_fancy != NULL) || (fancy_gc != None))
   {
      if (this_line_fancy && (this_line_fancy->gc == fancy_gc) && (this_line_fancy->end_col+1 == main_window_cur_descr->file_col_no))
         {
            this_line_fancy->end_col = main_window_cur_descr->file_col_no;
            if (this_line_fancy->next && (this_line_fancy->next->first_col < main_window_cur_descr->file_col_no))
               this_line_fancy->next->first_col = main_window_cur_descr->file_col_no;
         }
      else
         {
            /***************************************************************
            *  We have to change the fancy gc.  Either we are getting a new
            *  one or turning off the fancy gc data.
            ***************************************************************/
            if (fancy_gc != None)
               {
                  DEBUG23(fprintf(stderr, "Adding a Fancy GC element at col %d\n", main_window_cur_descr->file_col_no - 1);)
                  this_line_fancy = (FANCY_LINE *)CE_MALLOC(sizeof(FANCY_LINE));
                  if (!this_line_fancy)
                     return(redraw_needed); /* on failure, keep going, message already produced */
                  memset((char *)this_line_fancy, 0, sizeof(FANCY_LINE));
                  this_line_fancy->gc = fancy_gc;
                  this_line_fancy->first_col = main_window_cur_descr->file_col_no - 1;
                  this_line_fancy->end_col   = main_window_cur_descr->file_col_no;
                  insert_fancy_line_list(this_line_fancy,
                                         &(main_window_cur_descr->win_lines[main_window_cur_descr->file_line_no].fancy_line));
               }
            else
               {
                  /***************************************************************
                  *  We are either turning off the fancy gc, or writing in plain
                  *  video on a line containing a fancy piece.  If the writing
                  *  is taking place within the fancy area, move the start of the
                  *  fancy pointer.  This may not work if you switch to non-fancy
                  *  mode after positioning to the middle of the fancy area.  We
                  *  may have to split this fancy_line structure.
                  *  The first if would then be:
                  *  if (this_line_fancy->first_col+1 == main_window_cur_descr->file_col_no)
                  ***************************************************************/
                  /* up to the ifdef blah added 9/16/93and 10/5/93  */
                  /* for turning stuff off, see if we need to jump to the next fancy line element */
                  if (this_line_fancy->next && (this_line_fancy->next->first_col+1 == main_window_cur_descr->file_col_no))
                     this_line_fancy = this_line_fancy->next;

                  if (this_line_fancy->first_col+1 == main_window_cur_descr->file_col_no)
                     {
                        DEBUG23(fprintf(stderr, "vt100:  shrinking a fancy line area by 1, start now at %d\n", main_window_cur_descr->file_col_no);)
                        this_line_fancy->first_col = main_window_cur_descr->file_col_no;
                     }
                  else
                     {
                        if ((this_line_fancy->first_col < main_window_cur_descr->file_col_no) && (this_line_fancy->end_col > main_window_cur_descr->file_col_no))
                           {
                              DEBUG23(fprintf(stderr, "vt100:  splitting a fancy line area at col %d\n", main_window_cur_descr->file_col_no);)
                              temp_fancy_line = (FANCY_LINE *)CE_MALLOC(sizeof(FANCY_LINE));
                              if (!temp_fancy_line)
                                 return(redraw_needed); /* on failure, keep going, message already produced */
                              memset((char *)temp_fancy_line, 0, sizeof(FANCY_LINE));
                              temp_fancy_line->gc = this_line_fancy->gc;
                              temp_fancy_line->first_col = main_window_cur_descr->file_col_no;
                              temp_fancy_line->end_col   = this_line_fancy->end_col;

                              this_line_fancy->end_col = main_window_cur_descr->file_col_no - 1;  /* truncate old*fancy area */
                              insert_fancy_line_list(temp_fancy_line,
                                         &(main_window_cur_descr->win_lines[main_window_cur_descr->file_line_no].fancy_line));
                        }
                     else
                        {
                           DEBUG23(fprintf(stderr, "vt100:  Writing on normal text outside fancy line element range at col %d\n", main_window_cur_descr->file_col_no);)
                        }
                     }

#ifdef blah
                  if ((this_line_fancy->first_col < main_window_cur_descr->file_col_no) && (this_line_fancy->end_col < main_window_cur_descr->file_col_no)) /* 9/15/93 added second condition */
                     this_line_fancy->first_col = main_window_cur_descr->file_col_no;
#endif
                  if (this_line_fancy->first_col >= this_line_fancy->end_col)
                     delete_fancy_line_element(this_line_fancy, &(main_window_cur_descr->win_lines[main_window_cur_descr->file_line_no].fancy_line));
               }
         }
   }


return(redraw_needed);

} /* end of insert_char_in_window */

static int delete_chars_in_window(PAD_DESCR    *main_window_cur_descr,
                                  int           count)
{
char    *line;
int      redraw_needed;

if (count == 0)
   count = 1;

DEBUG23(fprintf(stderr, "@delete_chars_in_window: count = %d\n", count);)

if (main_window_cur_descr->buff_ptr != main_window_cur_descr->work_buff_ptr)
   {
      line = get_line_by_num(main_window_cur_descr->token, main_window_cur_descr->file_line_no);
      strcpy(main_window_cur_descr->work_buff_ptr, line);
      main_window_cur_descr->buff_modified = True;
      main_window_cur_descr->buff_ptr = main_window_cur_descr->work_buff_ptr;
   }

delete_from_buff(main_window_cur_descr->buff_ptr, count, main_window_cur_descr->file_col_no);

/***************************************************************
*  
*  Figure out what type of redrawing is needed.
*  
***************************************************************/

if (main_window_cur_descr->redraw_start_line == -1)  /* not set yet */
   {
      main_window_cur_descr->redraw_start_line = main_window_cur_descr->file_line_no;
      redraw_needed = (MAIN_PAD_MASK & PARTIAL_LINE);
   }
else
   {
      if (main_window_cur_descr->redraw_start_line > main_window_cur_descr->file_line_no)
         main_window_cur_descr->redraw_start_line = main_window_cur_descr->file_line_no;
      redraw_needed = (MAIN_PAD_MASK & PARTIAL_REDRAW);
   }

return(redraw_needed);

} /* end of delete_chars_in_window */

static int erase_chars_in_window(PAD_DESCR    *main_window_cur_descr,
                                 int           count)
{
char    *line;
int      redraw_needed;
int      pos;
int      lineno;
int      cols_on_screen = (main_window_cur_descr->window->sub_width / main_window_cur_descr->window->font->max_bounds.width);

if (count == 0)
   count = 1;

DEBUG23(fprintf(stderr, "@erase_chars_in_window: count = %d\n", count);)

if (main_window_cur_descr->buff_ptr != main_window_cur_descr->work_buff_ptr)
   {
      line = get_line_by_num(main_window_cur_descr->token, main_window_cur_descr->file_line_no);
      strcpy(main_window_cur_descr->work_buff_ptr, line);
      main_window_cur_descr->buff_modified = True;
      main_window_cur_descr->buff_ptr = main_window_cur_descr->work_buff_ptr;
   }

pos    = main_window_cur_descr->file_col_no;
lineno = main_window_cur_descr->file_line_no;

while(count)
{
   while((pos < cols_on_screen) && count)
   {
      if (!main_window_cur_descr->buff_ptr[pos])
         main_window_cur_descr->buff_ptr[pos+1] = '\0';
      main_window_cur_descr->buff_ptr[pos++] = ' ';
      count--;
   }

   put_line_by_num(main_window_cur_descr->token, lineno, main_window_cur_descr->buff_ptr, OVERWRITE);
   
   if (count)
      {
         pos = 0;
         lineno++;
         line = get_line_by_num(main_window_cur_descr->token, main_window_cur_descr->file_line_no);
         strcpy(main_window_cur_descr->work_buff_ptr, line);
         main_window_cur_descr->buff_modified = True;
         main_window_cur_descr->buff_ptr = main_window_cur_descr->work_buff_ptr;
      }
}

if (lineno != main_window_cur_descr->file_line_no)
   {
      line = get_line_by_num(main_window_cur_descr->token, main_window_cur_descr->file_line_no);
      strcpy(main_window_cur_descr->work_buff_ptr, line);
      main_window_cur_descr->buff_modified = False;
      main_window_cur_descr->buff_ptr = main_window_cur_descr->work_buff_ptr;
   }
  
      

/***************************************************************
*  
*  Figure out what type of redrawing is needed.
*  
***************************************************************/

if ((main_window_cur_descr->redraw_start_line == -1) && (lineno == main_window_cur_descr->file_line_no))
   {
      main_window_cur_descr->redraw_start_line = main_window_cur_descr->file_line_no;
      redraw_needed = (MAIN_PAD_MASK & PARTIAL_LINE);
   }
else
   {
      if (main_window_cur_descr->redraw_start_line > main_window_cur_descr->file_line_no)
         main_window_cur_descr->redraw_start_line = main_window_cur_descr->file_line_no;
      redraw_needed = (MAIN_PAD_MASK & PARTIAL_REDRAW);
   }

return(redraw_needed);

} /* end of erase_chars_in_window */

static int erase_area(PAD_DESCR    *main_window_cur_descr,
                       int           type,
                       int           erase_whole_buffer)
{
int       i;
char      msg[512];
char     *line;

if (main_window_cur_descr->buff_ptr != main_window_cur_descr->work_buff_ptr)
   {
      line = get_line_by_num(main_window_cur_descr->token, main_window_cur_descr->file_line_no);
      strcpy(main_window_cur_descr->work_buff_ptr, line);
      main_window_cur_descr->buff_modified = True;
      main_window_cur_descr->buff_ptr = main_window_cur_descr->work_buff_ptr;
   }

while (total_lines(main_window_cur_descr->token) < main_window_cur_descr->window->lines_on_screen)
   put_line_by_num(main_window_cur_descr->token, total_lines(main_window_cur_descr->token)-1, "", INSERT);


switch(type)
{
/***************************************************************
*  Type zero, delete from current position to end of buffer.
***************************************************************/
case 0:
   main_window_cur_descr->buff_ptr[main_window_cur_descr->file_col_no] = '\0';
   flush(main_window_cur_descr);
   if (!main_window_cur_descr->file_col_no)
      delete_fancy_line_list(&main_window_cur_descr->win_lines[main_window_cur_descr->file_line_no].fancy_line);
   if (erase_whole_buffer == WHOLE_BUFFER_ERASE)
      for (i = main_window_cur_descr->file_line_no+1; i < total_lines(main_window_cur_descr->token); i++)
         {
            put_line_by_num(main_window_cur_descr->token, i, "", OVERWRITE);
            if (main_window_cur_descr->win_lines[i].fancy_line != NULL)
               delete_fancy_line_list(&main_window_cur_descr->win_lines[i].fancy_line);
         }
   break;

/***************************************************************
*  Type one, delete from start up to current cursor position
***************************************************************/
case 1:
   for (i = 0; i < main_window_cur_descr->file_col_no; i++)
      main_window_cur_descr->buff_ptr[i] = ' ';
   if (erase_whole_buffer == WHOLE_BUFFER_ERASE)
      for (i = 0; i < main_window_cur_descr->file_line_no; i++)
      {
         put_line_by_num(main_window_cur_descr->token, i, "", OVERWRITE);
         if (main_window_cur_descr->win_lines[i].fancy_line != NULL)
            delete_fancy_line_list(&main_window_cur_descr->win_lines[i].fancy_line);
      }
   break;

/***************************************************************
*  Type two, delete the whole buffer.
***************************************************************/
case 2:
   if (erase_whole_buffer == WHOLE_BUFFER_ERASE)
      for (i = 0; i < total_lines(main_window_cur_descr->token); i++)
      {
         put_line_by_num(main_window_cur_descr->token, i, "", OVERWRITE);
         if (main_window_cur_descr->win_lines[i].fancy_line != NULL)
            delete_fancy_line_list(&main_window_cur_descr->win_lines[i].fancy_line);
         main_window_cur_descr->buff_modified = False;
         main_window_cur_descr->buff_ptr = get_line_by_num(main_window_cur_descr->token, 0);
      }
   else
      {
         put_line_by_num(main_window_cur_descr->token, main_window_cur_descr->file_line_no, "", OVERWRITE);
         if (main_window_cur_descr->win_lines[main_window_cur_descr->file_line_no].fancy_line != NULL)
            delete_fancy_line_list(&main_window_cur_descr->win_lines[main_window_cur_descr->file_line_no].fancy_line);
      }
   break;

default:
   snprintf(msg, sizeof(msg), "Unknown erase area type invt100 mode %d", type);
   dm_error(msg, DM_ERROR_LOG);

} /* end of switch on type */

return(MAIN_PAD_MASK & FULL_REDRAW);

} /* end of erase_area */


static int insert_lines(PAD_DESCR    *main_window_cur_descr,
                         int           lines)
{
int       i;

if (lines == 0)
   lines = 1;

for (i = 0; i < lines; i++)
   put_line_by_num(main_window_cur_descr->token, main_window_cur_descr->file_line_no-1, "", INSERT);

while (total_lines(main_window_cur_descr->token) > main_window_cur_descr->window->lines_on_screen)
   delete_line_by_num(main_window_cur_descr->token, tek_bottom_margin, 1);

return(MAIN_PAD_MASK & FULL_REDRAW);

} /* end of insert_lines */


static int change_tek_mode(PAD_DESCR    *main_window_cur_descr,
                           char         *line,
                           int           len,
                           int           do_set)
{

char      msg[512];
char     *p = line;
char     *end = line + len;
int       n;
int       redraw_needed  = 0;


while(p < end)
   switch(*p)
   {
   case ';':
      p++;
      break;

   case '<':
      tek_overstrike_mode = do_set;
      DEBUG23(fprintf(stderr, "vt100:  tek_overstrike_mode set to %d\n", tek_overstrike_mode);)
      p++;
      break;

   case '?':
      p++;
      while((p < end) && (*p != ';'))
         switch(*p++)
         {
         case '1':
            tek_cursor_key_mode = do_set;
            DEBUG23(fprintf(stderr, "vt100:  tek_cursor_key_mode set to %d\n", tek_cursor_key_mode);)
            break;
   
         case '2':
            vt52_mode = do_set;
            DEBUG23(fprintf(stderr, "vt100:  vt52_mode set to %d\n", vt52_mode);)
            break;
   
         case '3':
            tek_132_column_mode = do_set;
            DEBUG23(fprintf(stderr, "vt100:  tek_132_column_mode set to %d\n", tek_132_column_mode);)
            break;
   
         case '5':
            if (do_set)
               DEBUG23(fprintf(stderr, "vt100:  Mode switch to reverse video not supported\n");)
            break;
   
         case '6':
            tek_origin_mode_relative = do_set;
            if (tek_origin_mode_relative)
               redraw_needed = vt_move_cursor(main_window_cur_descr, tek_top_margin, 1, False, False, False, False);
            else
               redraw_needed = vt_move_cursor(main_window_cur_descr, 1, 1, False, False, False, False);
            DEBUG23(fprintf(stderr, "vt100:  tek_origin_mode_relative set to %d\n", tek_origin_mode_relative);)
            break;
   
         case '7':
            tek_autowrap_mode = do_set;
            DEBUG23(fprintf(stderr, "vt100:  tek_autowrap_mode set to %d\n", tek_autowrap_mode);)
            break;
   
         case '8':
            DEBUG23(fprintf(stderr, "vt100:  Autorepeat mode not supported\n");)
            break;
   
         default:
            snprintf(msg, sizeof(msg), "vt100: invalid mode switch value ?%c", *(p-1));
            dm_error(msg, DM_ERROR_LOG);
            break;
        }
     break;
      
   default:  /* numeric parms */
      n = 0;
      while ((*p <= '9') && (*p >= '0') && (p < end))
         n = (n * 10) + (*p++ - '0');     

      switch(n)
      {
      case 2:
         WRITABLE(main_window_cur_descr->token) = !do_set;
         DEBUG23(fprintf(stderr, "vt100:  keyboard enable set to %d\n", WRITABLE(main_window_cur_descr->token));)
         break;

      case 4:
         *main_window_cur_descr->insert_mode = do_set;
         DEBUG23(fprintf(stderr, "vt100:  insert mode set to %d\n", do_set);)
         break;

      case 12:
         tek_local_echo_mode = !do_set;
         DEBUG23(fprintf(stderr, "vt100:  tek_local_echo_mode set to %d\n", tek_local_echo_mode);)
         break;

      case 20:
         linefeed_newline_mode = do_set;
         DEBUG23(fprintf(stderr, "vt100:  linefeed_newline_mode set to %d\n", linefeed_newline_mode);)
         break;

      default:
         snprintf(msg, sizeof(msg), "vt100: Invalid mode switch value %d", n);
         dm_error(msg, DM_ERROR_LOG);
         break;
      } 

     if (p < end)
        p++;
     break;

   } /* end of switch on *p */

return(redraw_needed);

} /* end of change_tek_mode */

static void set_scroll_margins(PAD_DESCR    *main_window_cur_descr,
                               int           top,
                               int           bottom)
{

DEBUG23(fprintf(stderr, "@set_scroll_margins(%d,%d) lines on screen = %d\n", top, bottom, main_window_cur_descr->window->lines_on_screen);)

/***************************************************************
*  A reset to the whole screen counts as the margins are not set.
***************************************************************/
if ((top <= 1) && (bottom == main_window_cur_descr->window->lines_on_screen))
   tek_margins_set = False;
else
   tek_margins_set = True;

/***************************************************************
*  Set the margins within the bounds allowed.
***************************************************************/
if (top == 0)
   tek_top_margin = 0;
else
   tek_top_margin = top - 1;  /* top is one based, we are zero based */

if ((bottom == 0) || (bottom >= main_window_cur_descr->window->lines_on_screen))
   tek_bottom_margin = main_window_cur_descr->window->lines_on_screen - 1;
else
   tek_bottom_margin = bottom - 1;  /* bottom is one based, we are zero based */

} /* end of set_scroll_margins */


/* select graphic rendition */
static void vt_sgr(PAD_DESCR    *main_window_cur_descr,
                   char         *line,
                   int           len,
                   int          *numbers)
{
int       count;
char     *p = line;
char     *end = line + len;
char     *q;
int       i;

/***************************************************************
*  
*  Default of nothing, resets graphic renditions to the default.
*  
*  Note we cannot support changing the font because of the way
*  the drawing routines work.
*
***************************************************************/

if (len == 0)
   {
      DEBUG23(fprintf(stderr, "vt_sgr: Null case, normal video\n");)
      fancy_gc = None;
      return;
   }


while(p < end)
   switch(*p)
   {
   case '<':
      q = strchr(p, ';');
      if ((q == NULL) || (q >= end))
         p = end;
      DEBUG23(fprintf(stderr, "vt_sgr: found <, %.*s\n", end-p, p);)
      break;

   case '=':
      q = strchr(p, ';');
      if ((q == NULL) || (q >= end))
         p = end;
      DEBUG23(fprintf(stderr, "vt_sgr: found =, %.*s\n", end-p, p);)
      break;

   case '>':
      q = strchr(p, ';');
      if ((q == NULL) || (q >= end))
         p = end;
      DEBUG23(fprintf(stderr, "vt_sgr: found >, %.*s\n", end-p, p);)
      break;

   default:
      count = get_numbers(p, len, numbers);
      DEBUG23(fprintf(stderr, "vt_sgr: count = %d  (%.*s)\n", count, end-p, p);)
      for (i = 0; i < count; i++)
         switch(numbers[i])
         {
         case 0:
         case 27:
            fancy_gc = None;
            DEBUG23(fprintf(stderr, "vt_sgr: case 27 or 0, normal video\n");)
            break;

         case 7:
            fancy_gc = main_window_cur_descr->window->reverse_gc;
            DEBUG23(fprintf(stderr, "vt_sgr: case 7, turn on reverse video\n");)
            break;

         default:
            DEBUG23(fprintf(stderr, "vt_sgr: unsupported parm %d found\n", numbers[i]);)
         }
      p = end;
   }

} /* end of vt_sgr */


static int   vt_ht(PAD_DESCR    *main_window_cur_descr,
                   int           count,
                   int           move_right)
{
int       i;
int       new_col;
int       line_len;
int       redraw_needed;
int       cols_on_screen = (main_window_cur_descr->window->sub_width / main_window_cur_descr->window->font->max_bounds.width);

line_len = strlen(main_window_cur_descr->buff_ptr) - 1;
if (line_len < 0)
   line_len = 0;

if (move_right)
   {
      if (tabcount)
         {
            new_col = line_len;
            for (i = 0; i < tabcount && count; i++)
               if (tabstops[i] > main_window_cur_descr->file_col_no)
                  {
                     new_col = tabstops[i];
                     count--;
                  }
         }
      else
         new_col = main_window_cur_descr->file_col_no + 1;
   }
else
   {
      if (tabcount)
         {
            new_col = 0;
            for (i = tabcount - 1; i >= 0 && count; i--)
               if (tabstops[i] < main_window_cur_descr->file_col_no)
                  {
                     new_col = tabstops[i];
                     count--;
                  }
         }
      else
         new_col = main_window_cur_descr->file_col_no - 1;
   }

DEBUG23(fprintf(stderr, "vt100: Tab %s to col %d from col %d\n", (move_right ? "right" : "left"), new_col, main_window_cur_descr->file_col_no);)

#ifdef blah
if (line_len < new_col)
   {
      new_col = line_len;
      DEBUG23(fprintf(stderr, "vt100: Tab adjust to max line col %d\n", new_col);)
   }
#endif

if (cols_on_screen < new_col)
   {
      new_col = cols_on_screen;
      DEBUG23(fprintf(stderr, "vt100: Tab adjust to screen edge col %d\n", new_col);)
   }

redraw_needed = vt_move_cursor(main_window_cur_descr, 0, new_col+1, True, False, False, False); /* +1 converts to 1 based for this call */

return(redraw_needed);

} /* end of vt_th */


static void  vt_add_tab_stop(PAD_DESCR    *main_window_cur_descr)
{
int       new_col = main_window_cur_descr->file_col_no;
int       i;
int       j;
int       got_it = False;

for (i = 0; i < tabcount; i++)
   if (tabstops[i] > new_col)
      {
         for (j = tabcount-1; j >= i; j--)
             tabstops[j+1] = tabstops[j];
         tabcount++;
         tabstops[i] = new_col;
         got_it = True;
      }

if (!got_it)
   tabstops[tabcount++] = new_col;

} /* end of vt_add_tab_stop */


static void  vt_erase_tab_stop(PAD_DESCR    *main_window_cur_descr,
                               int           extent)
{
int       new_col = main_window_cur_descr->file_col_no;
int       i;
int       j;
int       got_it = False;

if (extent)
   tabcount = 0;
else
   {
      for (i = 0; i < tabcount; i++)
         if (tabstops[i] == new_col)
            {
               for (j = i; j < tabcount; j++)
                   tabstops[j] = tabstops[j+1];
               tabcount--;
               got_it = True;
            }
   }

if (!got_it)
   DEBUG23(fprintf(stderr, "vt100:  Could not delete tabstop at position %d, no tab at that location\n", new_col);)

} /* end of vt_add_tab_stop */


static void insert_fancy_line_list(FANCY_LINE    *new,
                                   FANCY_LINE   **head)
{
FANCY_LINE    *current;
FANCY_LINE    *last = NULL;


DEBUG23(fprintf(stderr, "@insert_fancy_line_list, head = 0x%X, *head = 0x%X \n", head, *head);)

/***************************************************************
*  Scan the linked list looking for the first fancy line element
*  with a start greater or equal to the current start col.  We
*  will put the new one just in front of that one.
***************************************************************/
for (current = *head; current && current->first_col < new->first_col; current = current->next)
   last = current;

if (*head == NULL)
   *head = new;  /* first on an empty list */
else
   {
      if (last == NULL)         
         {
            /* put at the front */
            new->next = *head;
            *head = new;
         }
      else
         {
            new->next = last->next;
            last->next = new;
         }
   }

clean_fancy_list(head);

} /* end of insert_fancy_line_list */

static void delete_fancy_line_element(FANCY_LINE    *kill,
                                      FANCY_LINE   **head)
{
FANCY_LINE    *current;
FANCY_LINE    *last = NULL;


DEBUG23(fprintf(stderr, "@delete_fancy_line_element, head = 0x%X, *head = 0x%X, kill = 0x%X \n", head, *head, kill);)

/***************************************************************
*  Scan the linked list looking for the first fancy line element
*  with a start greater or equal to the current start col.  We
*  will put the new one just in front of that one.
***************************************************************/
for (current = *head; current && current != kill; current = current->next)
   last = current;

if (current == NULL)
   {
      DEBUG(fprintf(stderr, "@delete_fancy_line_element:  Cannot find passed element in linked list\n");)
      return;
   }

if (last == NULL)
   {
      /* delete the first one on the list */
      *head = current->next;
   }
else
   {
      /* delete one from the middle of the list */
      last->next = current->next;
   }

free((char *)kill);


} /* end of delete_fancy_line_element */

static void clean_fancy_list(FANCY_LINE   **head)
{
FANCY_LINE    *current;
FANCY_LINE    *last = *head;
FANCY_LINE    *temp;

DEBUG23(fprintf(stderr, "@clean_fancy_list, head = 0x%X, *head = 0x%X \n", head, *head);)

if(*head)
   for (current = *head; current && current->next != NULL; current = current->next)
   {
      if (current->end_col > current->next->first_col)
         {
            current->next->first_col = current->end_col;
            if (current->next->first_col >= current->next->end_col)  /* area has become zero size */
               {
                  /***************************************************************
                  *  Remove an element out of the middle.
                  ***************************************************************/

                  temp = current->next;
                  current->next = current->next->next;
                  free(temp);
                  current = last;  /* repeat using the same current as this loop */
               }
         }
      last = current;
   }

} /* end of clean_fancy_list */


static void delete_fancy_line_list(FANCY_LINE   **first)
{
FANCY_LINE   *current;
FANCY_LINE   *next;


DEBUG23(fprintf(stderr, "@delete_fancy_line_list, first = 0x%X, *first = 0x%X \n", first, *first);)

for (current = *first; current != NULL; current = next)
{
   next = current->next;
   free(current);
}

*first = NULL;

} /* end of delete_fancy_line_list */


/************************************************************************

NAME:      vt100_color_line -   Parse a line with VT100 colorization data


PURPOSE:    This routine handles the data returned from the shell when in not in
            vt100 mode.  It checks for the color escape control sequences
            and processes them.  Processing consists of removing them and building
            the CDGC data which would go into the memdata database for this line.

PARAMETERS:
   1.   dspl_descr   - pointer to DISPLAY_DESCR (INPUT)
                       This is the current display description.
                       Passed through to build_color.

   1.  line          -  pointer to char (INPUT/OUTPUT)
                        This is the data from the shell to be processed.  It contains
                        data and possible control sequences.  On output, the control
                        sequences have been removed.

   2.  color_line     - pointer to char (OUTPUT)
                        This is the where the color data is placed.  If there is no
                        color data, this line is set to the empty string.

   3.  max_color_line - int (INPUT)
                        This is size of the string passed as parameter color_line.


FUNCTIONS :
   1.   Quickly scan for a x'1b' (escape).  If not found, zero out the target color
        line and return.

RETURNED VALUE:
   colored  -         int
                      FALSE  -  Line contained no color data
                      TRUE   -  Line contained color data, placed int color_line

*************************************************************************/

int  vt100_color_line(DISPLAY_DESCR   *dspl_descr,
                      char            *line,
                      char            *color_line,
                      int              max_color_line)
{
int                   n;
int                   m;
char                 *pos;
char                 *out_pos;
char                 *end;
char                 *first_escape;
int                   warp_needed;
char                 *cmd_trailer;
int                   count;
int                   parms[20];
char                  msg[512];
static char           hold_line[512] = "";  /* for commands broken across block boundaries */
static int            hold_line_len = 0;
char                  work_line[MAX_LINE+sizeof(hold_line)];
#ifdef DebuG
static int            regular_chars = False;
static unsigned char    trigger_chars[] = {VT_ESC, VT_BL, VT_BS, VT_CN, VT_CR, VT_FF, VT_HT, VT_LF, VT_SI, VT_SO, VT_VT, VT__, '\0'};
#endif
FANCY_LINE           *curr;
FANCY_LINE           *head = NULL;
FANCY_LINE           *tail = NULL;

*color_line = '\0';

if ((first_escape = strchr(line, VT_ESC)) == NULL)
   return(False);

DEBUG23(fprintf(stderr, "@vt100_color_line: line= \"%s\"\n", line);
        hexdump(stderr, "@vt100_color_line data", (char *)line, strlen(line),False);)

end      = line + strlen(line);
pos      = first_escape;
out_pos  = first_escape;

while(pos < end)
{
   if ((*pos == VT_ESC) && (*(pos+1) == '['))
      {
         pos += 2; /* skip past the escape and open square bracket */

         if (pos < end)
            cmd_trailer = strpbrk(pos, " ABCcDfgHhIJKLlMmnPqRrSTXZ@");
         else
            cmd_trailer = NULL;

         if (cmd_trailer != NULL)
            {
               DEBUG23(fprintf(stderr, "vt100_color_line EC[%.*s%c (at pos %d)", cmd_trailer-pos, pos, *cmd_trailer, pos-line);)
               if (*cmd_trailer == 'm')   /*   case 'm' :   Select Graphic Rendition */
                  {
                     DEBUG23(fprintf(stderr, "  - Select Graphic Rendition \n");)
                     curr = build_color(dspl_descr, pos, cmd_trailer-pos, out_pos-line);
                     if (curr)
                        {
                           /* build it backwards the front does not cover up the later elements */
                           curr->next = head;
                           head = curr;
                        }
#ifdef blah
                        {
                           if (head == NULL)
                              head = curr;
                           else
                              tail->next = curr;
                           tail = curr;
                        }
#endif
                  }
               else
                  {
                     DEBUG23(fprintf(stderr, "  - Ignored \n");)
                  }
               pos = cmd_trailer + 1;
            }
         else
            {
               if (pos != out_pos)
                  {
                     *out_pos = *pos;
                     *(out_pos+1) = *(pos+1);
                  }
               out_pos += 2;
               pos     += 2;
            }
      }
   else
      {
         if (pos != out_pos)
            *out_pos = *pos;
         out_pos++;
         pos++;
      }
} /* end of do while not at end of line */

if (head)
   {
      flatten_cdgc(head, color_line, max_color_line, False /* do them all */);
      free_cdgc(head);
   }

*out_pos = '\0';
DEBUG23(fprintf(stderr, "vt100_color_line: line= \"%s\"\ncolor = \"%s\"\n", line, color_line);)

return(color_line[0]);

}  /*  vt100_color_line() */


/************************************************************************

NAME:      build_color   -   Build memdata color data from VT100 color info.


PURPOSE:    This routine handles getting a list of numeric parameters from a string
            and puts them in an array of integers.  The count is returned.

PARAMETERS:
   1.   dspl_descr            - pointer to DISPLAY_DESCR (INPUT)
                                This is the current display description.
                                Needed for VT color information from parm

   2.  graphic_rendition_key  - pointer to char (INPUT)
                                This is the part of the line starting with the
                                number.  That is, in the sequence
                                Escape(x1B)[43m this parm points to the 4.

   3.  len                    - int (INPUT)
                                this is the length of the string up to the end
                                of the number.  In the above example: 
                                graphic_rendition_key+len ponts to the 'm'

   4.  first_col              - int (INPUT)
                                This is the current column in the output line.
                                It is used to build the color information.


FUNCTIONS :
   1.   Check for a few special characters.

   2.   Grab the number out of the string

   3.   Convert the number to a color.

   4.   Build a FANCY_LINE structure to return.

RETURNED VALUE:
   curr     -   pointer to FANCY_LINE
                NULL  -  out of memory.
                !NULL -  A structure containing colorization data.
                         Start point on the line, and foreground and backgroud colors.

*************************************************************************/

static FANCY_LINE *build_color(DISPLAY_DESCR   *dspl_descr,
                               char            *graphic_rendition_key,
                               int              len,
                               int              first_col)
{
int                   count;
char                 *p = graphic_rendition_key;
char                 *end = graphic_rendition_key + len;
char                 *q;
int                   i;
int                   numbers[2];
FANCY_LINE           *curr;
char                  work_colors[256];
char                  current_bg[40];
char                  current_fg[40];
char                  current_bgfg[40];
int                   reverse_video = False;


curr = (FANCY_LINE *)malloc(sizeof(FANCY_LINE));
if (curr == NULL)/* handle out of memory */
   return(curr);

memset(curr, 0, sizeof(FANCY_LINE));
/* curr->next = NULL;   * next data in list                                    **/
curr->first_col = first_col;
curr->end_col  = MAX_LINE;
/*curr->gc = None;                     * GC with the fancy line information                   */

strcpy(current_bg, DEFAULT_BG);
strcpy(current_fg, DEFAULT_FG);
current_bgfg[0] = '\0';

/***************************************************************
*  
*  Default of nothing, resets graphic renditions to the default.
*  
*  Note we cannot support changing the font because of the way
*  the drawing routines work.
*
***************************************************************/

if (len == 0)
   {
      curr->bgfg =    malloc_copy(DEFAULT_BGFG);    /* background / foreground color names                  */
      if (!curr->bgfg)
         {
            free(curr);
            curr = NULL;
         }
      DEBUG23(fprintf(stderr, "build_color: Null case, normal video\n");)
      return(curr);
   }

while(p < end)
   switch(*p)
   {
   case '<':
      q = strchr(p, ';');
      if ((q == NULL) || (q >= end))
         p = end;
      DEBUG23(fprintf(stderr, "vt_sgr: found <, %.*s\n", end-p, p);)
      break;

   case '=':
      q = strchr(p, ';');
      if ((q == NULL) || (q >= end))
         p = end;
      DEBUG23(fprintf(stderr, "vt_sgr: found =, %.*s\n", end-p, p);)
      break;

   case '>':
      q = strchr(p, ';');
      if ((q == NULL) || (q >= end))
         p = end;
      DEBUG23(fprintf(stderr, "vt_sgr: found >, %.*s\n", end-p, p);)
      break;

   default:
      count = get_numbers(p, len, numbers);
      DEBUG23(fprintf(stderr, "vt_sgr: count = %d  (%.*s)\n", count, end-p, p);)
      for (i = 0; i < count; i++)
         switch(numbers[i])
         {
         case 0:
         case 27:
            curr->bgfg =    malloc_copy(DEFAULT_BGFG);    /* background / foreground color names                  */
            DEBUG23(fprintf(stderr, "build_color: case 27 or 0, normal video\n");)
            break;

         case 01:
            DEBUG23(fprintf(stderr, "build_color: case 1, bold, un-supported\n");)
            break;

         case 7:
            reverse_video = True;
            DEBUG23(fprintf(stderr, "build_color: case 7, turn on reverse video\n");)
            break;

         case 30:
         case 90:
            if (dspl_descr->VT_colors[0])
               if (strchr(dspl_descr->VT_colors[0], '/') != NULL)
                  strcpy(current_bgfg, dspl_descr->VT_colors[0]);
               else
                  strcpy(current_fg, dspl_descr->VT_colors[0]);
            else
               strcpy(current_fg, "brown");
            DEBUG23(fprintf(stderr, "build_color: case 30, Foreground %s\n", (current_bgfg[0] ? current_bgfg : current_fg));)
            break;

         case 31:
         case 91:
            if (dspl_descr->VT_colors[1])
               if (strchr(dspl_descr->VT_colors[1], '/') != NULL)
                  strcpy(current_bgfg, dspl_descr->VT_colors[1]);
               else
                  strcpy(current_fg, dspl_descr->VT_colors[1]);
            else
               strcpy(current_fg, "red");
            DEBUG23(fprintf(stderr, "build_color: case 31, Foreground %s\n", (current_bgfg[0] ? current_bgfg : current_fg));)
            break;

         case 32:
         case 92:
            if (dspl_descr->VT_colors[2])
               if (strchr(dspl_descr->VT_colors[2], '/') != NULL)
                  strcpy(current_bgfg, dspl_descr->VT_colors[2]);
               else
                  strcpy(current_fg, dspl_descr->VT_colors[2]);
            else
               strcpy(current_fg, "#00aa00");
            DEBUG23(fprintf(stderr, "build_color: case 32 Foreground %s\n", (current_bgfg[0] ? current_bgfg : current_fg));)
            break;

         case 33:
         case 93:
            if (dspl_descr->VT_colors[3])
               if (strchr(dspl_descr->VT_colors[3], '/') != NULL)
                  strcpy(current_bgfg, dspl_descr->VT_colors[3]);
               else
                  strcpy(current_fg, dspl_descr->VT_colors[3]);
            else
               strcpy(current_fg, "yellow");
            DEBUG23(fprintf(stderr, "build_color: case 33 Foreground %s\n", (current_bgfg[0] ? current_bgfg : current_fg));)
            break;

         case 34:
         case 94:
            if (dspl_descr->VT_colors[4])
               if (strchr(dspl_descr->VT_colors[4], '/') != NULL)
                  strcpy(current_bgfg, dspl_descr->VT_colors[4]);
               else
                  strcpy(current_fg, dspl_descr->VT_colors[4]);
            else
               strcpy(current_fg, "blue");
            DEBUG23(fprintf(stderr, "build_color: case 34 Foreground %s\n", (current_bgfg[0] ? current_bgfg : current_fg));)
            break;

         case 35:
         case 95:
            if (dspl_descr->VT_colors[5])
               if (strchr(dspl_descr->VT_colors[5], '/') != NULL)
                  strcpy(current_bgfg, dspl_descr->VT_colors[5]);
               else
                  strcpy(current_fg, dspl_descr->VT_colors[5]);
            else
               strcpy(current_fg, "magenta");
            DEBUG23(fprintf(stderr, "build_color: case 35 Foreground %s\n", (current_bgfg[0] ? current_bgfg : current_fg));)
            break;

         case 36:
         case 96:
            if (dspl_descr->VT_colors[6])
               if (strchr(dspl_descr->VT_colors[6], '/') != NULL)
                  strcpy(current_bgfg, dspl_descr->VT_colors[6]);
               else
                  strcpy(current_fg, dspl_descr->VT_colors[6]);
            else
               strcpy(current_fg, "#00aaaa");
            DEBUG23(fprintf(stderr, "build_color: case 36 Foreground %s\n", (current_bgfg[0] ? current_bgfg : current_fg));)
            break;

         case 37:
         case 97:
            if (dspl_descr->VT_colors[7])
               if (strchr(dspl_descr->VT_colors[7], '/') != NULL)
                  strcpy(current_bgfg, dspl_descr->VT_colors[7]);
               else
                  strcpy(current_fg, dspl_descr->VT_colors[7]);
            else
               strcpy(current_fg, "gray");
            DEBUG23(fprintf(stderr, "build_color: case 37 Foreground %s\n", (current_bgfg[0] ? current_bgfg : current_fg));)
            break;


         case 40:
            if (dspl_descr->VT_colors[0])
               if (strchr(dspl_descr->VT_colors[0], '/') != NULL)
                  strcpy(current_bgfg, dspl_descr->VT_colors[0]);
               else
                  strcpy(current_bg, dspl_descr->VT_colors[0]);
            else
               strcpy(current_bg, "brown");
            DEBUG23(fprintf(stderr, "build_color: case 40 Background %s\n", (current_bgfg[0] ? current_bgfg : current_fg));)
            break;

         case 41:
            if (dspl_descr->VT_colors[1])
               if (strchr(dspl_descr->VT_colors[1], '/') != NULL)
                  strcpy(current_bgfg, dspl_descr->VT_colors[1]);
               else
                  strcpy(current_bg, dspl_descr->VT_colors[1]);
            else
               strcpy(current_bg, "red");
            DEBUG23(fprintf(stderr, "build_color: case 41 Background %s\n", (current_bgfg[0] ? current_bgfg : current_fg));)
            break;

         case 42:
            if (dspl_descr->VT_colors[2])
               if (strchr(dspl_descr->VT_colors[2], '/') != NULL)
                  strcpy(current_bgfg, dspl_descr->VT_colors[2]);
               else
                  strcpy(current_bg, dspl_descr->VT_colors[2]);
            else
               strcpy(current_bg, "#00aa00");
            DEBUG23(fprintf(stderr, "build_color: case 42 Background %s\n", (current_bgfg[0] ? current_bgfg : current_fg));)
            break;

         case 43:
            if (dspl_descr->VT_colors[3])
               if (strchr(dspl_descr->VT_colors[3], '/') != NULL)
                  strcpy(current_bgfg, dspl_descr->VT_colors[3]);
               else
                  strcpy(current_bg, dspl_descr->VT_colors[3]);
            else
               strcpy(current_bg, "yellow");
            DEBUG23(fprintf(stderr, "build_color: case 43 Background %s\n", (current_bgfg[0] ? current_bgfg : current_fg));)
            break;

         case 44:
            if (dspl_descr->VT_colors[4])
               if (strchr(dspl_descr->VT_colors[4], '/') != NULL)
                  strcpy(current_bgfg, dspl_descr->VT_colors[4]);
               else
                  strcpy(current_bg, dspl_descr->VT_colors[4]);
            else
               strcpy(current_bg, "blue");
            DEBUG23(fprintf(stderr, "build_color: case 44 Background %s\n", (current_bgfg[0] ? current_bgfg : current_fg));)
            break;

         case 45:
            if (dspl_descr->VT_colors[5])
               if (strchr(dspl_descr->VT_colors[5], '/') != NULL)
                  strcpy(current_bgfg, dspl_descr->VT_colors[5]);
               else
                  strcpy(current_bg, dspl_descr->VT_colors[5]);
            else
               strcpy(current_bg, "magenta");
            DEBUG23(fprintf(stderr, "build_color: case 45 Background %s\n", (current_bgfg[0] ? current_bgfg : current_fg));)
            break;

         case 46:
            if (dspl_descr->VT_colors[6])
               if (strchr(dspl_descr->VT_colors[6], '/') != NULL)
                  strcpy(current_bgfg, dspl_descr->VT_colors[6]);
               else
                  strcpy(current_bg, dspl_descr->VT_colors[6]);
            else
               strcpy(current_bg, "#00aaaa");
            DEBUG23(fprintf(stderr, "build_color: case 46 Background %s\n", (current_bgfg[0] ? current_bgfg : current_fg));)
            break;

         case 47:
            if (dspl_descr->VT_colors[7])
               if (strchr(dspl_descr->VT_colors[7], '/') != NULL)
                  strcpy(current_bgfg, dspl_descr->VT_colors[7]);
               else
                  strcpy(current_bg, dspl_descr->VT_colors[7]);
            else
               strcpy(current_bg, "gray");
            DEBUG23(fprintf(stderr, "build_color: case 47 Background %s\n", (current_bgfg[0] ? current_bgfg : current_fg));)
            break;


         default:
            DEBUG23(fprintf(stderr, "vt_sgr: unsupported parm %d found\n", numbers[i]);)
         }
      p = end;
   }

if (current_bgfg[0])
   snprintf(work_colors, sizeof(work_colors), "%s", current_bgfg);
else
   if (reverse_video)
      snprintf(work_colors, sizeof(work_colors), "%s/%s", current_fg, current_bg);
   else
      snprintf(work_colors, sizeof(work_colors), "%s/%s", current_bg, current_fg);

curr->bgfg =    malloc_copy(work_colors);

return(curr);

} /* end of build_color */

#endif /* end of ifdef pad */

/* ARGSUSED */
int free_drawable(void   *drawable_data)
{
return(0);
} /* end of free_drawable */




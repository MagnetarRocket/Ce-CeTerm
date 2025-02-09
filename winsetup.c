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
*  module winsetup.c
*
*  Routines:
*     win_setup               - Set up a new set of windows
*     bring_up_to_snuff       - Synchronize the cursor and pad buffers
*
*  Internal Routines:
*     vt_color_setup          - Fill in vt100 colors (normal ceterm) from parm or resource
*     set_workspace_parm      - Use the -ws option to set the workspace
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <errno.h>          /* /usr/include/errno.h   /usr/include/sys/errno.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <limits.h>         /* /usr/include/limits.h     */
#include <ctype.h>          /* /usr/include/ctype.h      */

#ifdef WIN32
#include <stdlib.h>
#define putenv _putenv

#include <winsock.h>
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "borders.h"
#include "cc.h"
#include "color.h"
#include "debug.h"
#include "dmwin.h"
#include "emalloc.h"
#include "gc.h"
#include "getevent.h"
#include "getxopts.h"
#ifndef HAVE_CONFIG_H
#include "help.h"
#endif
#include "init.h"
#include "lineno.h"
#include "mouse.h"
#include "pad.h"
#include "parms.h"
#include "parsedm.h"
#include "pastebuf.h"
#include "pd.h"
#include "pw.h"
#include "redraw.h"
#include "titlebar.h"
#include "tab.h"
#include "typing.h"
#include "unixwin.h"
#include "vt100.h"   /* needed for free_drawable */
#include "wdf.h"
#include "window.h"
#include "winsetup.h"
#include "xerror.h"
#include "xerrorpos.h"
#include "xsmp.h"
#include "xutil.h"

#ifndef HAVE_STRLCPY
#include "strl.h"
#endif

char   *getenv(const char *name);

static void vt_color_setup(DISPLAY_DESCR   *dspl_descr,
                           const char      *vtcolor_parm);

static void set_workspace_parm(Display              *display,
                               Window                main_window,
                               const char           *ws_id);

/************************************************************************

NAME:      win_setup  -  Build a set of windows

PURPOSE:    This routine will build the set of windows for a ce or ceterm
            session.

PARAMETERS:
   1.   dspl_descr     - pointer to DISPLAY_DESCR (INPUT / OUTPUT)
                         This is the current display description being built.

   2.  edit_file       - pointer to char (INPUT / OUTPUT)
                         This is the current edit file name.  This may be changed
                         by this routine.

   3.  resource_class  - pointer to char (INPUT)
                         The X resource class for this program.
                         It should contain the string "Ce".

   4.  open_for_write  - int (INPUT)
                         This flag is True if we intend to open the
                         filefor input.

   5.  cmdpath         - pointer to char (INPUT)
                         This string contains the command needed to restart
                         this ce/ceterm session.

FUNCTIONS :
   1.   Perform a bunch of setup for the display description and also
        create the new windows and map them.


RETURNED VALUE:
   resized   -  int flag
                True  - Window was resized
                False - Window was not resized

*************************************************************************/

int  win_setup(DISPLAY_DESCR   *dspl_descr,
               char            *edit_file,
               char            *resource_class,
               int              open_for_write,
               char            *cmdpath)
{
char                  msg[MAX_LINE];
int                   i;
char                 *p;
char                 *q;
char                 *shell;
char                 *vendor_info;
DMC                  *new_dmc;
char                  window_name[256];
char                 *icon_name;
XGCValues             gcvalues;         /* work var used in setting up colors and font for the windows   */
XSetWindowAttributes  window_attrs;     /* setup structure for building the main window                  */
unsigned long         window_mask;

DEBUG1(fprintf(stderr, "@win_setup window : dspl %d\n", dspl_descr->display_no);)

/***************************************************************
*  If there is a tabstop resource, process it.
***************************************************************/
if (TABSTOPS)
   {
      new_dmc = parse_dm_line(TABSTOPS, True, 0, dspl_descr->escape_char, dspl_descr->hsearch_data);  /* parse the tabsource resource */
      if (new_dmc != NULL)
         if (new_dmc->any.cmd == DM_ts)
            dm_ts(new_dmc);
         else
            {
               snprintf(msg, sizeof(msg), "DM cmd %s, invalid in tabstop resource or parm",  dmsyms[new_dmc->any.cmd].name);
               dm_error_dspl(msg, DM_ERROR_BEEP, dspl_descr);
            }
   }


/***************************************************************
*  
*  Set up the error handlers for problems with X.
*  The first one is for protocol problems, run out of server memory
*  and so forth.
*
*  The second is for things like a server crash.
*  
***************************************************************/

DEBUG9(XERRORPOS)
XSetErrorHandler(ce_xerror);
DEBUG9(XERRORPOS)
XSetIOErrorHandler(ce_xioerror);


/***************************************************************
*  
*  Option values gets looked at alot (in get_next_X_event).  To speed
*  things up, if the yes value was not specified, make the parm
*  null.  The test in get_next_X_event just tests for the value being
*  present.
*
*  READAHEAD is a macro in parms.h
*  BACKGROUND_READ_AHEAD is a constant in getevent.h
*
***************************************************************/

change_background_work(dspl_descr, BACKGROUND_READ_AHEAD, READAHEAD);

/***************************************************************
*  
*  If the line spacing was specified in a parm, fix it up 
*  Note, this has to be done before the get_font_data call.
*  
***************************************************************/

if (LINE_PADDING != NULL)
   {
      if (sscanf(LINE_PADDING, "%d%9s", &i, msg) == 1)  /* msg is a scrap variable */
         {
            if (i < 0 || i > 200)
               {
                  snprintf(msg, sizeof(msg), "Out of range line padding value %d, default of %d%% used\n", i, dspl_descr->padding_between_line);
                  dm_error_dspl(msg, DM_ERROR_BEEP, dspl_descr);
               }
            else
               dspl_descr->padding_between_line = i;
         }
      else
         {
            snprintf(msg, sizeof(msg), "Non-numric line padding value %s, default of %d%% used\n", LINE_PADDING, dspl_descr->padding_between_line);
            dm_error_dspl(msg, DM_ERROR_BEEP, dspl_descr);
         }
   }

/***************************************************************
*  
*  If the linemax parameter was specified, put it in the display description.
*  Was initialized to zero in display.c
*  
***************************************************************/

if (LINEMAX != NULL)
   {
      if (sscanf(LINEMAX, "%d%9s", &i, msg) == 1)  /* msg is a scrap variable */
         {
            if (i < 0)
               {
                  snprintf(msg, sizeof(msg), "Out of range linemax value %d, parameter ignored\n", i);
                  dm_error_dspl(msg, DM_ERROR_BEEP, dspl_descr);
               }
            else
               dspl_descr->linemax = i;
         }
      else
         {
            snprintf(msg, sizeof(msg), "Non-numric linemax value %s, parameter ignored\n", LINEMAX);
            dm_error_dspl(msg, DM_ERROR_BEEP, dspl_descr);
         }
   }

/***************************************************************
*  
*  If the find border was specified, via the .Xdefault file,
*  save it in the display description.
*  
***************************************************************/

if (FINDBRDR != NULL)
   {
      if (sscanf(FINDBRDR, "%d%9s", &i, msg) == 1)  /* msg is a scrap variable */
         dspl_descr->find_border = i;
      else                                     
         {
            snprintf(msg, sizeof(msg), "Non-numeric find border (.findbrdr) value %s, ignored\n", FINDBRDR);
            dm_error_dspl(msg, DM_ERROR_BEEP, dspl_descr);
         }
   }

/***************************************************************
*  
*  Set up the font data from the parm, or use a default.
*  This fills in the font, fixed_font, and line_height fields
*  in the window description.  It will exit if no font can
*  be loaded.
*  
***************************************************************/

dspl_descr->current_font_name = get_font_data(FONT_NAME, dspl_descr); /* in init.c */

/****************************************************************
*
*   Parse the geometry string to get the width, height, x and y
*   Set reasonable defaults for anything we don't get.
*   The font must already be set up because we need the character
*   size to handle the 'C' refix on the geometry string.
*   The 'C' prefix means the width and height are in characters.
*
****************************************************************/

if ((GEOMETRY == NULL) || (*GEOMETRY == '\0'))
   GEOMETRY = wdf_get_geometry(dspl_descr->display, dspl_descr->properties);
process_geometry(dspl_descr, GEOMETRY, 1 /* set defaults if needed */); /* in window.c */

/***************************************************************
*
*  Set the globals from the lineno and expression mode parms.
*  
***************************************************************/

if (AUTOSAVE)
   {
      if (sscanf(AUTOSAVE, "%d%9s", &i, msg) == 1)  /* msg is a scrap variable here */
         {
            if (i < 0)
               {
                  snprintf(msg, sizeof(msg), "Out of range autosave value %d, autosave not enabled\n", i);
                  dm_error_dspl(msg, DM_ERROR_BEEP, dspl_descr);
               }
            else
               dspl_descr->auto_save_limit = i;
         }
      else
         {
            snprintf(msg, sizeof(msg), "Non-numeric autosave value value %s, autosave not enabled", AUTOSAVE);
            dm_error_dspl(msg, DM_ERROR_BEEP, dspl_descr);
         }
   }

if ((LINENO_PARM[0] | 0x20) == 'y')
   dspl_descr->show_lineno = 1;
else
   dspl_descr->show_lineno = 0;

if (UNIXEXPR_ERR)
   {
      snprintf(msg, sizeof(msg), "Invalid value for Ce.expr (%s), Aegis or Unix must be capitalized. The cpp pass over the .Xdefaults file may convert \"unix\" or \"aegis\" to \"1\"\n", OPTION_VALUES[EXPR_IDX]);
      dm_error_dspl(msg, DM_ERROR_LOG, dspl_descr);
   }
if (UNIXEXPR)
   dspl_descr->escape_char = '\\';
else
   dspl_descr->escape_char = '@';

#ifdef PAD
if (AUTOCLOSE)
   dspl_descr->autoclose_mode = True;
else
   dspl_descr->autoclose_mode = False;
#endif

/***************************************************************
*  
*  Set initial case sensitivity from the passed Parameter
*  or Xdefault value.
*  
***************************************************************/

dspl_descr->case_sensitive = CASE_SENSITIVE;

/***************************************************************
*
*  Initialize the buffer descriptions and other such things.
*
***************************************************************/

dspl_descr->first_expose = True;
dspl_descr->mark1.buffer = dspl_descr->main_pad;

dspl_descr->dminput_pad->token     = mem_init(100, False);
WRITABLE(dspl_descr->dminput_pad->token)  = True;
dspl_descr->dmoutput_pad->token     = mem_init(100, True);
WRITABLE(dspl_descr->dmoutput_pad->token) = False;

#ifdef PAD
if (dspl_descr->pad_mode)
   {
      dspl_descr->unix_pad->token        = mem_init(100, False);
      WRITABLE(dspl_descr->unix_pad->token)  = True;
      cc_set_pad_mode(dspl_descr->display);  /* show the cc list we are in pad mode */
   }
#endif



/***************************************************************
*  
*  Put one blank line in the dminput memdata area and one in the
*  output area.
*  
***************************************************************/

put_line_by_num(dspl_descr->dminput_pad->token, -1, "", INSERT); /* in memdata.c */
put_line_by_num(dspl_descr->dmoutput_pad->token, -1, "", INSERT);

/***************************************************************
*  
*  If this was piped in from stdin, use 100% full, otherwise use 75%.
*  The main token is always null at this point except in the case of a cc
*
***************************************************************/

if (dspl_descr->main_pad->token == NULL)
   {
      if (strcmp(edit_file, STDIN_FILE_STRING) != 0)
         dspl_descr->main_pad->token     = mem_init(75, False);  /* r/w fill blocks 75 %full  in memdata.c */
      else
         dspl_descr->main_pad->token     = mem_init(100, dspl_descr->pad_mode);  /* ro fill blocks 100 %full */

      WRITABLE(dspl_descr->main_pad->token) = open_for_write;
   }

dspl_descr->main_pad->work_buff_ptr[0] = '\0';

/***************************************************************
*  
*  Set the icon name from the file name.  Use the last qualifier.
*  If this is a command name with parms,  strip off the parms.
*  
***************************************************************/

#ifdef PAD
if (dspl_descr->pad_mode && (strcmp(edit_file, STDIN_FILE_STRING) == 0))
   {
      shell = getenv("SHELL");
      if (!shell) shell = SHELL;
         strlcpy(edit_file, shell, MAX_LINE+1);
   }
q = strpbrk(edit_file, " \t");
if (q)
   {
      i = *q;
      *q = '\0';
   }
#endif

if (WM_TITLE)
  icon_name = WM_TITLE;
else
   {
      p = strrchr(edit_file, '/');
      if (p != NULL)
         icon_name = p+1;
      else
         icon_name = edit_file;
   }

/***************************************************************
*  
*  If we are talking to an XFree86 X Server, record this fact.
*  It gives focus events under different conditions and needs
*  to be flagged in one case.
*  
***************************************************************/

vendor_info = ServerVendor(dspl_descr->display);

DEBUG1(fprintf(stderr, "vendor_info = %s\n", (vendor_info ? vendor_info : "<NULL>"));)

if (vendor_info && (strstr(vendor_info, "XFree86") != NULL))
   dspl_descr->XFree86_flag = True;

if (vendor_info && (strstr(vendor_info, "Hummingbird Communications") != NULL))
   dspl_descr->Hummingbird_flag = True;

/***************************************************************
*  
*  If we are displaying on a remote host, put the current
*  host name in the titlebar, otherwise, do not.
*  We do this by first checking for a logically null display name.
*  That is :0 or :0.0 or some such.  If not, then check to
*  see if the host names match, if so, we do not want the 
*  host name displayed.
*  
***************************************************************/

strlcpy(dspl_descr->display_host_name, DisplayString(dspl_descr->display), sizeof(dspl_descr->display_host_name));
if ((dspl_descr->display_host_name[0] != ':') && (strncmp(dspl_descr->display_host_name, "unix:", 5) != 0) && (strncmp(dspl_descr->display_host_name, "local:", 6) != 0))
   {
      gethostname(dspl_descr->titlebar_host_name, sizeof(dspl_descr->titlebar_host_name));
      if (strncmp(DisplayString(dspl_descr->display), dspl_descr->titlebar_host_name, strlen(dspl_descr->titlebar_host_name)) == 0)
         {
            dspl_descr->titlebar_host_name[0] = '\0';
            dspl_descr->display_host_name[0] = '\0';
         }
      p = strchr(dspl_descr->display_host_name, ':');
      if (p)
         *p = '\0';
   }
else
   dspl_descr->display_host_name[0] = '\0';

#ifdef PAD
/***************************************************************
*  
*  If this is pad mode and we are displaying on a node other
*  than we are running on, change the icon name.
*  
***************************************************************/

if (dspl_descr->pad_mode && (*dspl_descr->titlebar_host_name != '\0') && !WM_TITLE)
   icon_name = dspl_descr->titlebar_host_name;

if (q)
   *q = i;
#endif

/***************************************************************
*  
*  Set the initial color values
*  
***************************************************************/
set_initial_colors(dspl_descr,               /* in color.c */
                   BACKGROUND_CLR,           /* macro in parms.h */
                   &gcvalues.background,
                   FOREGROUND_CLR,
                   &gcvalues.foreground);

/***************************************************************
*
*  Build the main window.  Then get the attributes to verify
*  the position and size.
*
***************************************************************/

window_attrs.background_pixel = gcvalues.background;
window_attrs.border_pixel     = gcvalues.foreground;
window_mask = CWBackPixel | CWBorderPixel;

/* temp debug start */
/*window_attrs.backing_store = NotUseful;*/
/*window_attrs.save_under = False;*/
/*window_attrs.backing_store = Always;*/
/*window_attrs.save_under = True;*/
/*window_mask |= (CWBackingStore | CWSaveUnder);*/
/* temp debug end   */

DEBUG1(fprintf(stderr, "win_setup:  creating main window:  %dx%d%+d%+d\n",
              dspl_descr->main_pad->window->width, dspl_descr->main_pad->window->height,
              dspl_descr->main_pad->window->x,     dspl_descr->main_pad->window->y);
)

DEBUG9(XERRORPOS)
dspl_descr->main_pad->x_window = XCreateWindow(dspl_descr->display,
                                RootWindow(dspl_descr->display, DefaultScreen(dspl_descr->display)),
                                dspl_descr->main_pad->window->x,     dspl_descr->main_pad->window->y,
                                dspl_descr->main_pad->window->width, dspl_descr->main_pad->window->height,
                                BORDER_WIDTH,                   /*  border_width */
                                DefaultDepth(dspl_descr->display, DefaultScreen(dspl_descr->display)),  /*  depth        */
                                InputOutput,                    /*  class        */
                                CopyFromParent,                 /*  visual       */
                                window_mask,
                                &window_attrs
    );

/* still need the gc at this point */

/***************************************************************
*  
*  Put the window id in the environment like xterm does.
*  
***************************************************************/

snprintf(msg, sizeof(msg), "WINDOWID=%X", dspl_descr->main_pad->x_window);
q = malloc_copy(msg);
if (q)
   putenv(q);


/****************************************************************
*
*   Build the gc for the main pad, pixmap, dminput and output windows.
*
****************************************************************/
gcvalues.line_width = 2;
gcvalues.font = dspl_descr->main_pad->window->font->fid;

gcvalues.graphics_exposures = False;

create_gcs(dspl_descr->display,              /* input  in gc.c */
           dspl_descr->main_pad->x_window,   /* input  */
           dspl_descr->main_pad->window,     /* output */
           &gcvalues);                       /* input  */

/***************************************************************
*
*  Build the pixmap.
*  This is what all the main window drawing is done into.
*  The main window is copied to with natural and sent expose events.
*
***************************************************************/

/*memcpy((char *)dspl_descr->main_pad->drawing_area, (char *)dspl_descr->main_pad->window, sizeof(DRAWABLE_DESCR));*/


DEBUG9(XERRORPOS)
dspl_descr->x_pixmap = XCreatePixmap(dspl_descr->display, dspl_descr->main_pad->x_window,
                                  dspl_descr->main_pad->window->width, dspl_descr->main_pad->window->height,
                                  DefaultDepth(dspl_descr->display, DefaultScreen(dspl_descr->display)));


/***************************************************************
*
*  Build the DM input window and output windows.
*  Then set up their sub areas.
*
***************************************************************/

get_dm_windows(dspl_descr, 0);   /* input/ output  in dmwin.c   */

dm_subarea(dspl_descr,
           dspl_descr->dminput_pad->window,              /* input  in dmwin.c */
           dspl_descr->dminput_pad->pix_window);         /* output */
dm_subarea(dspl_descr,
           dspl_descr->dmoutput_pad->window,             /* input  */
           dspl_descr->dmoutput_pad->pix_window);        /* output */
set_dm_prompt(dspl_descr, "");  /* set up the default prompt */

#ifdef PAD
if (dspl_descr->pad_mode)
   {
      get_unix_window(dspl_descr);
      unix_subarea(dspl_descr->unix_pad->window,
                   dspl_descr->unix_pad->pix_window);
      dspl_descr->scroll_mode   = SCROLL;
      dspl_descr->autohold_mode = AUTOHOLD;
      if (TERMTYPE)
         {
            if (TERMTYPE[0]) /* Null termtype means leave it alone */
               {
                  /* user specified term type, RES 8/10/95, Debugging */
                  snprintf(msg, sizeof(msg), "TERM=%s", TERMTYPE);
                  p = malloc_copy(msg);
                  if (p)
                     putenv(p);
                  snprintf(msg, sizeof(msg), "term=%s", TERMTYPE);
                  p = malloc_copy(msg);
                  if (p)
                     putenv(p);
               }
         }
      else
         {
            p = malloc_copy("TERM=vt100");  /* we look like a vt100 */
            if (p)
               putenv(p);
            p = malloc_copy("term=vt100");  /* we look like a vt100 */
            if (p)
               putenv(p);
         }
   }
#endif

/***************************************************************
*  
*  Build the menubar area and turn it on if needed.
*  
***************************************************************/

get_menubar_window(dspl_descr->display,
                   dspl_descr->pd_data,
                   MOUSECURSOR,
                   dspl_descr->main_pad->window,
                   dspl_descr->main_pad->x_window);

if (PDM)
   dspl_descr->pd_data->menubar_on = True;

/***************************************************************
*  
*  Build the subarea of the main pad for the titlebar.
*  
***************************************************************/

get_titlebar_window(dspl_descr->display,                      /* input   in titlebar.c */
                    dspl_descr->main_pad->x_window,           /* input   */
                    dspl_descr->main_pad->window,             /* input   */
                    (dspl_descr->pd_data->menubar_on ? &dspl_descr->pd_data->menu_bar : NULL),
                    TITLEBARFONT,                             /* input   */
                    dspl_descr->title_subarea);               /* output  */

setup_lineno_window(dspl_descr->main_pad->window,                         /* input  in lineno.c */
                    dspl_descr->title_subarea,                            /* input  */
                    dspl_descr->dminput_pad->window,                      /* input  */
                    (dspl_descr->pad_mode ? dspl_descr->unix_pad->window : NULL),  /* input  */
                    dspl_descr->lineno_subarea);                          /* output */



/***************************************************************
*
*  Show the location of the text subarea in the main window.
*  It is what is left after the titlebar, dminput, and dmoutput
*  windows eat up their share.
*
***************************************************************/

main_subarea(dspl_descr);

/***************************************************************
*  
*  Allocate the array which will hold the pixel lengths of
*  each drawn line.  It is filled in by the textfill_drawable
*  routine and in doing reverse video.  The size of the array
*  allocated will handle a full screen area at the font
*  used.
*  
***************************************************************/

setup_winlines(dspl_descr->main_pad);

/***************************************************************
*  
*  Set up the default paste buffer name from the parm.
*  Added 5/23/95 by RES.
*  
***************************************************************/

init_paste_buff(DFLT_PASTE, dspl_descr->display);


/***************************************************************
*  
*  Copy the vt color data from the parm to the display descr.
*  Added 7/11/2003 by RES.
*  
***************************************************************/
vt_color_setup(dspl_descr, VTCOLORS_PARM);

/***************************************************************
*
*  Set the Motif window title.
*
***************************************************************/

if (WM_TITLE)
   strlcpy(window_name, WM_TITLE, sizeof(window_name));
else
#ifdef PAD
   if (dspl_descr->pad_mode)
      snprintf(window_name, sizeof(window_name), PACKAGE_STRING, VERSION);
   else
#endif
      strlcpy(window_name, icon_name, sizeof(window_name)); /* RES 12/16/2002, put file name in title for Gnome window manager */

/***************************************************************
*
*  Set up the hints data for the window manager.
*
***************************************************************/

setup_window_manager_properties(dspl_descr,                         /* in init.c */
                                window_name,
                                icon_name,
                                cmdname,
                                START_ICONIC);

/**************************************************************
*  
*  Set up the two mouse cursors.
*  
***************************************************************/

build_cursors(dspl_descr);
normal_mcursor(dspl_descr);

/**************************************************************
*  
*  Save the restart commands for the X session manager, if there is one.
*  
***************************************************************/

save_restart_state(dspl_descr,
                   edit_file,
                   cmdpath);

/***************************************************************
*  
*  Set the name as known to the window manager for special window
*  manager setup.
*  
***************************************************************/

set_wm_class_hints(dspl_descr->display, dspl_descr->main_pad->x_window, cmdname, RESOURCE_NAME, resource_class);

/***************************************************************
*
*  Determine what sort of events we want to process
*  in the windows.
*
***************************************************************/

set_event_masks(dspl_descr, True); /* True forces the getevent,  in getevent.c */

/***************************************************************
*
*  We are ready for data, make sure we have enough for the first
*  screen. First clear out the pixmap.  The build the first
*  screen of data in the pixmap for the expose event.
*
***************************************************************/

DEBUG1(
      fprintf(stderr, "main window     = 0x%02X\ndminput window  = 0x%02X\ndmoutput window = 0x%02X\n",
                      dspl_descr->main_pad->x_window, dspl_descr->dminput_pad->x_window, dspl_descr->dmoutput_pad->x_window);
 )  

DEBUG12(
      dump_gc(stderr, dspl_descr->display, dspl_descr->main_pad->window->gc, "main GC");
      dump_gc(stderr, dspl_descr->display, dspl_descr->main_pad->window->reverse_gc, "reverse GC");
      dump_gc(stderr, dspl_descr->display, dspl_descr->main_pad->window->xor_gc, "Xor GC");
)

/***************************************************************
*  
*  Set the initial scroll bar settings.
*  Added 6/7/96 RES
*  
***************************************************************/

if (SCROLLBARS != NULL)
   {
      switch(SCROLLBARS[0])
      {
      case 'a':
         dspl_descr->sb_data->sb_mode = SCROLL_BAR_AUTO;
         break;
      case 'n':
         dspl_descr->sb_data->sb_mode = SCROLL_BAR_OFF;
         break;
      case 'y':
         dspl_descr->sb_data->sb_mode = SCROLL_BAR_ON;
         break;
      case '\0':
         break;
      default:
         snprintf(msg, sizeof(msg), "Invalid scroll bar mode \"%s\" (-sb or Ce.scrollBar) no assumed (valid: yes, no, auto)\n", SCROLLBARS);
         dm_error_dspl(msg, DM_ERROR_BEEP, dspl_descr);
         dspl_descr->sb_data->sb_mode = SCROLL_BAR_OFF;
         break;
      }
   }
else
   dspl_descr->sb_data->sb_mode = SCROLL_BAR_OFF;


/***************************************************************
*  
*  This code loads the original screen full of data.
*  This needs to be done before the first up to snuff call.
*  
***************************************************************/

process_redraw(dspl_descr, FULL_REDRAW, False);

/***************************************************************
*  
*  Initialize the cursor buff to initially refer to the main
*  pad top left character.  This resolves problems caused by 
*  unusual events before any pointer motion occurs.
*  
***************************************************************/

#ifdef PAD
if (dspl_descr->pad_mode)
   {
      dspl_descr->cursor_buff->which_window = UNIXCMD_WINDOW;
      dspl_descr->cursor_buff->current_win_buff = dspl_descr->unix_pad;
      dspl_descr->cursor_buff->x = dspl_descr->unix_pad->window->sub_x + 2;
      dspl_descr->cursor_buff->y = dspl_descr->unix_pad->window->sub_y + 2;
   }
else
   {
      dspl_descr->cursor_buff->current_win_buff = dspl_descr->main_pad;
      dspl_descr->cursor_buff->which_window     = MAIN_PAD;
      dspl_descr->cursor_buff->x = dspl_descr->main_pad->window->sub_x + 1;
      dspl_descr->cursor_buff->y = dspl_descr->main_pad->window->sub_y + 1;
   }
#else
dspl_descr->cursor_buff->current_win_buff = dspl_descr->main_pad;
dspl_descr->cursor_buff->which_window     = MAIN_PAD;
dspl_descr->cursor_buff->x = dspl_descr->main_pad->window->sub_x + 1;
dspl_descr->cursor_buff->y = dspl_descr->main_pad->window->sub_y + 1;
#endif
dspl_descr->cursor_buff->win_line_no = 0;
dspl_descr->cursor_buff->win_col_no  = 0;
bring_up_to_snuff(dspl_descr);

/***************************************************************
*  
*  If we are starting iconic, trigger the commands, if any.
*  Added 8/10/95 RES
*  
***************************************************************/
if ((CMDS_FROM_ARG) && (START_ICONIC))
   fake_keystroke(dspl_descr);  /* in getevent.c */


#ifdef  HAVE_X11_SM_SMLIB_H
/***************************************************************
*
*  If the X Session Manager (XSMP) is available initialize the
*  interface to it.
*  If we are using XSMP, register the window with the window
*  manager.
*
***************************************************************/
if (dspl_descr->xsmp_private_data == NULL)
   dspl_descr->xsmp_active =  xsmp_init(dspl_descr, edit_file, cmdpath); /* in xsmp.c */
if (dspl_descr->xsmp_active)
   xsmp_register(dspl_descr);
#endif

if (WS_NUM && WS_NUM[0])
   {
     set_workspace_parm(dspl_descr->display, dspl_descr->main_pad->x_window, WS_NUM);
   }

/***************************************************************
*
*  Map the window and we are under way.
*  After the map, the window is visable on the screen.
*
***************************************************************/

DEBUG9(XERRORPOS)
XMapWindow(dspl_descr->display, dspl_descr->main_pad->x_window);
XFlush(dspl_descr->display);


return(0);
} /* end of win_setup */


/************************************************************************

NAME:      vt_color_setup - Fill in vt100 colors (normal ceterm) from parm or resource

PURPOSE:    In normal ceterm mode (not vt100 mode), ceterm will try to process the
            EC[00m (select graphic rendition) control sequences.  00 is replaced
            by 30 - 37, or 40 - 78.  This is seen in the Linux ls command.  This
            routine takes the parm and fills in the colors in the display description.

PARAMETERS:
   1.   dspl_descr   - pointer to DISPLAY_DESCR (INPUT / OUTPUT)
                       This is the current display description.

   2.  vtcolor_parm -  pointer to const char (INPUT)
                       This is a list of up to 8 characters separated by
                       commas.  Normally this is just a single color in
                       each comma delimited element.  The contruct bgc/fgc
                       is allowed.  Also to skip a color and use the
                       default, code two commas in a row.  To use the
                       current background and forground colors for a default,
                       the color name "DEFAULT" is used (no quotes).

FUNCTIONS :
   1.   Walk the color string separating out the colors.

   2.   Copy the color to each display description element 

   3.   Fill any missing elements with the defaults.

*************************************************************************/

static void vt_color_setup(DISPLAY_DESCR   *dspl_descr,
                           const char      *vtcolor_parm)
{
#define          MAX_COLOR_NAME 80
static char     *default_colors[8] = {"brown",   /* 30 */
                                      "red",     /* 31 */
                                      "#00aa00", /* 32 */
                                      "yellow",  /* 33 */
                                      "blue",    /* 34 */
                                      "magenta", /* 35 */
                                      "#00aaaa", /* 36 */
                                      "gray"};   /* 37 */
int              color_idx = 0;
const char      *p = vtcolor_parm;
char             color[MAX_COLOR_NAME];
char            *q = color;

color[0] = '\0';

while(p && *p && ((*p == ' ') || (*p == '\t')))
   p++;

if (p)
   while(*p && (color_idx < 8))
   {
      if (*p != ',')
         if ((q-color) < MAX_COLOR_NAME-1)
            *q++ = *p++;
         else
            *p++;
      else
         {
            *q = '\0';
            if (color[0])
               dspl_descr->VT_colors[color_idx] = malloc_copy(color);
            else
               dspl_descr->VT_colors[color_idx] = malloc_copy(default_colors[color_idx]);
            color_idx++;
            q = color;
            p++; /* go pat the comma */
         }
   }

if ((color_idx < 8) && (color[0]))
   dspl_descr->VT_colors[color_idx++] = malloc_copy(color);

while(color_idx < 8)
{
   dspl_descr->VT_colors[color_idx] = malloc_copy(default_colors[color_idx]);
   color_idx++;
}
 

} /* end of vt_color_setup */


/************************************************************************

NAME:      bring_up_to_snuff - Synchronize the cursor and pad buffers

PURPOSE:    This routine completes setup of the cursor_buff and sets up
            positioning information in the various pad buffers after
            movement activity.

PARAMETERS:

   1.  cursor_buff -  pointer to BUFF_DESCR (INPUT / OUTPUT)
                      This is the buffer description for current cursor position.

   2.  comma_mark -  pointer to MARK_REGION (OUTPUT)
                     This mark is used to identify the last position int he
                     main window where activity took place.  This data is used
                     by the comma command (dm_comma for reseting position).
               

GLOBAL INPUT DATA:

   1.  main_window -  BUFF_DESCR (INPUT)
               This is the main window description.

   2.  dminput_window -  BUFF_DESCR (INPUT)
               This is the description of the dm input window.

   3.  dmoutput_window -  BUFF_DESCR (INPUT)
               This is the description of the dm output window.

   4.  unixcmd_window -  BUFF_DESCR (INPUT)
               This is the description of the dm output window.


FUNCTIONS :

   1.   Set the calculatable variables for the current cursor position.


*************************************************************************/

void bring_up_to_snuff(DISPLAY_DESCR   *dspl_descr)
{
BUFF_DESCR     *cursor_buff = dspl_descr->cursor_buff;
WINDOW_LINE    *win_line;

DEBUG7( fprintf(stderr,"Bring up to Snuff %s  (%d,%d) window:[%d,%d] dspl (%d)\n",
        which_window_names[cursor_buff->which_window], 
        cursor_buff->x, cursor_buff->y,
        cursor_buff->win_line_no,  cursor_buff->win_col_no, dspl_descr->display_no);
 )

if (cursor_buff->win_line_no < 0)
   cursor_buff->win_line_no = 0;

switch(cursor_buff->which_window)
{
case MAIN_PAD:
#ifdef PAD
   if (dspl_descr->vt100_mode)
      {
         cursor_buff->up_to_snuff  = True;
         return;
      }
#endif
   cursor_buff->current_win_buff  = dspl_descr->main_pad;
   if (dspl_descr->pad_mode)
      cursor_buff->current_win_buff->file_line_no = -1; /* force reread in pad mode */
   break;

case DMINPUT_WINDOW:
   cursor_buff->current_win_buff  = dspl_descr->dminput_pad;
   break;

case DMOUTPUT_WINDOW:
   cursor_buff->current_win_buff  = dspl_descr->dmoutput_pad;
   break;

#ifdef PAD
case UNIXCMD_WINDOW:
   cursor_buff->current_win_buff  = dspl_descr->unix_pad;
   break;
#endif

default:
   DEBUG(fprintf(stderr, "bring_up_to_snuff:  Don't know which window I am in\n");)
   break;

} /* end of switch on which window */

/***************************************************************
*  Get the correct line in the buffer.  Flush any typing changes
*  if necessary.
*  RES 8/29/95 Force reread in cc_ce mode. 
***************************************************************/

if (cc_ce || ((cursor_buff->current_win_buff->first_line + cursor_buff->win_line_no) != cursor_buff->current_win_buff->file_line_no))
   {
      if (cursor_buff->current_win_buff->buff_modified)
         flush(cursor_buff->current_win_buff);

      cursor_buff->current_win_buff->file_line_no = cursor_buff->current_win_buff->first_line + cursor_buff->win_line_no;
      DEBUG7( fprintf(stderr,"Reading file line %d\n", cursor_buff->current_win_buff->file_line_no);)
      cursor_buff->current_win_buff->buff_ptr = get_line_by_num(cursor_buff->current_win_buff->token, cursor_buff->current_win_buff->file_line_no);
   }



if (cursor_buff->win_line_no >= 0)
   {
      win_line = &cursor_buff->current_win_buff->win_lines[cursor_buff->win_line_no];
      win_line->line = cursor_buff->current_win_buff->buff_ptr;
   }
else
   win_line = NULL;

if (cursor_buff->win_col_no < 0)
   cursor_buff->win_col_no = 0;

if ((cursor_buff->win_line_no == 0) && (cursor_buff->y < cursor_buff->current_win_buff->window->sub_y))
   {
       DEBUG7( fprintf(stderr,"bring_up_to_snuff:  adjusting y coordinate");)
       cursor_buff->y = cursor_buff->current_win_buff->window->sub_y + 1;
   }


if (win_line)
   if (win_line->tabs)
      cursor_buff->current_win_buff->file_col_no = tab_cursor_adjust(win_line->line,
                                                                     cursor_buff->win_col_no,
                                                                     cursor_buff->current_win_buff->first_char,
                                                                     TAB_LINE_OFFSET,
                                                                     cursor_buff->current_win_buff->display_data->hex_mode);
   else
      cursor_buff->current_win_buff->file_col_no = cursor_buff->current_win_buff->first_char + cursor_buff->win_col_no;
else
   cursor_buff->current_win_buff->file_col_no = cursor_buff->current_win_buff->first_char;

/***************************************************************
*  Added 8/27/96, RES, if adjusting tabs causes file col# to
*  go below start of window, fix it.
***************************************************************/
if (cursor_buff->current_win_buff->file_col_no < cursor_buff->current_win_buff->first_char)
   cursor_buff->current_win_buff->file_col_no = cursor_buff->current_win_buff->first_char;

cursor_buff->up_to_snuff  = True;

if (cursor_buff->which_window != DMINPUT_WINDOW)
   {
      dspl_descr->comma_mark.mark_set     = True;
      dspl_descr->comma_mark.file_line_no =  cursor_buff->current_win_buff->file_line_no;
      dspl_descr->comma_mark.col_no       =  cursor_buff->current_win_buff->file_col_no;
      dspl_descr->comma_mark.buffer       =  cursor_buff->current_win_buff;
   }


DEBUG7( fprintf(stderr,"Snuffed %s  file: [%d,%d]\n",
        which_window_names[cursor_buff->which_window], 
        cursor_buff->current_win_buff->file_line_no,  cursor_buff->current_win_buff->file_col_no);
)


} /* end of bring_up_to_snuff */


/************************************************************************

NAME:      set_workspace_parm - Use the -ws option to set the workspace

PURPOSE:    This routine uses the value passed in the -ws parm to set
            the X Resource telling the window manager which workspace to
            open the ceterm in.
            document is http://www.freedesktop.org
            positioning information in the various pad buffers after
            movement activity.

PARAMETERS:
   1.  display     -  pointer to Display (X Type) (INPUT)
                      This is the display pointer for the ceterm used in X calls.

   2.  main_window -  Window (X Type) (INPUT)
                     This mark is used to identify the last position int he
                     main window where activity took place.  This data is used
                     by the comma command (dm_comma for reseting position).

   3.  ws_id      -  pointer to const char (INPUT)
                     This is the value from the -ws parm it is a number.

FUNCTIONS :
   1.   Get the atom id for _NET_WM_DESKTOP.  If it does not exist,
        this window manager does not support this technique.

   2.   Convert the workspace number to a fullword integer.

   3.   Set the atom on the passed window.

*************************************************************************/

static void set_workspace_parm(Display              *display,
                               Window                main_window,
                               const char           *ws_id)
{
int                      workspace_num;
XTextProperty            WorkspaceID;
Atom                     net_wm_desktop_Atom;

sscanf(ws_id, "%x", &workspace_num);

DEBUG9(XERRORPOS)
net_wm_desktop_Atom = XInternAtom(display, "_NET_WM_DESKTOP", True);
if (net_wm_desktop_Atom != None)
   {
      WorkspaceID.value    = (unsigned char *)&workspace_num;
      WorkspaceID.encoding = XA_CARDINAL;
      WorkspaceID.format   = 32;
      WorkspaceID.nitems   = 1;

      DEBUG9(XERRORPOS)
      XSetTextProperty(display, main_window, &WorkspaceID, net_wm_desktop_Atom);

      DEBUG1(
           fprintf(stderr, "set_workspace_parm: _NET_WM_DESKTOP property set to 0x%X for main window 0x%X\n",
                                workspace_num, main_window);
            )
   }
else
   {
            DEBUG1(fprintf(stderr, "set_workspace_parm: Cannot get atom id for _NET_WM_DESKTOP, session manager may not put this window in correct workspace\n");)
   }

} /* end of set_workspace_parm  */

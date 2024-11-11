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
*  module dmwin.c
*
*  Routines:
*         get_dm_windows        - get the dm input and output windows.
*         draw_dm_prompt        - Draw the DM prompt in the dminput command window.
*         dm_error              - Write message to dm_output window and beep
*         dm_error_dspl         - Write message to dm_output specifying the display.
*         dm_clean              - Clean up dm error messages but not informatory data
*         resize_dm_windows     - handle a resize of the main window to the client.
*         set_dm_prompt         - Set the DM input prompt.
*         dm_subarea            - Build the subarea drawing description for a DM window.
*         scroll_dminput_prompt - scroll the dm input window prompt.
*         get_dm_prompt         - Return the dm prompt address, set by set dm prompt.
*
*  Internal:
*         check_private_data    - Set up the dmwin private data in the dspl descr
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h         */
#include <string.h>         /* /usr/include/string.h        */
#include <errno.h>          /* /usr/include/errno.h         */
#include <time.h>           /* /usr/include/time.h          */

#ifdef WIN32

#include <windows.h>

#ifdef WIN32_XNT
#include "xnt.h"
#else
#include "X11/X.h"          /* /usr/include/X11/X.h      */
#include "X11/Xlib.h"       /* /usr/include/X11/Xlib.h   */
#include "X11/Xutil.h"      /* /usr/include/X11/Xutil.h  */
#include "X11/keysym.h"     /* /usr/include/X11/keysym.h */
#endif
#include <process.h>
#else
#include <X11/X.h>          /* /usr/include/X11/X.h      */
#include <X11/Xlib.h>       /* /usr/include/X11/Xlib.h   */
#include <X11/Xutil.h>      /* /usr/include/X11/Xutil.h  */
#include <X11/keysym.h>     /* /usr/include/X11/keysym.h */
#endif

#ifdef WIN32
#define getpid _getpid
#define getuid()  -1
#define chmod(file, mode) {}
#endif

#define DMWIN_C 1
#include "borders.h"
#include "cc.h"
#include "dmwin.h"
#include "debug.h"
#include "emalloc.h"
#include "vt100.h"   /* needed for free_drawable */
#include "memdata.h"
#include "parms.h"
#include "redraw.h"
#include "windowdefs.h"
#include "xerror.h"
#include "xerrorpos.h"

#if !defined(FREEBSD) && !defined(CYGNUS) && !defined(OMVS)
char  *getenv(const char *name);
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN	1024
#endif

/***************************************************************
*
*  This is the padding around the dm window line in percent of
*  the font height.
*
***************************************************************/

#define   DM_WINDOW_HEIGHT_PADDING   30


/***************************************************************
*  
*  Static data hung off the display description.
*
*  The following structure is hung off the display_data field
*  in the display description.
*  
***************************************************************/

typedef struct {
    char               dm_prompt[80];
    int                prompt_first_char;
    int                last_dm_error_had_beep;
    int                last_dm_error_was_blank_line;
    int                beep_issued;
    int                recursive_flag;
} DMWIN_PRIVATE;

static DMWIN_PRIVATE *check_private_data(DISPLAY_DESCR    *passed_dspl_descr);


/************************************************************************

NAME:      get_dm_windows

PURPOSE:    This routine creates and initially maps the "Command:" input
            window and the output message window at the bottom of the
            main widnow.
            NOTE:  This routine should be called after the main window
            is created but before the XSelectInput is done for the main window.

PARAMETERS:

   1.  dspl_descr   - pointer to DISPLAY_DESCR (INPUT)
               This is a pointer to all the relevant data about
               the main window.  It is used to get data to build
               the DM windows.

   4.  format -  int (INPUT)
               This parm is not currently used.  It is intended to support putting the DM
               windows at the top of the screen or stacking the DM windows around the screen


FUNCTIONS :

   1.   Determine the size of the DM windows.  They are half
        the width of the main window and the height of the font % 20% tall.
        The boarder width is 1.

   2.   At a future time, we will check to see if a global DM window exists and
        use it if it does.

   3.   Create the 2 DM windows, map the windows and select the input
        events on those windows.

   4.   Save the window data and set up the command prompt

   5.   Return 1

OUTPUTS:

   returned_value - int
        The returned value is a flag which shows whether the dm windows
        are children of the main window or whether they are separate
        windows.  Currently they always return true.


*************************************************************************/

/* ARGSUSED1 */
int   get_dm_windows(DISPLAY_DESCR    *dspl_descr,           /* input / output */
                     int               format)               /* input   */

{
int                   window_height;
int                   window_width;
int                   border_width;
XWindowAttributes     window_attributes;
XSetWindowAttributes  window_attrs;
unsigned long         window_mask;
XGCValues             gcvalues;
unsigned long         valuemask;

/***************************************************************
*
*  Do the fixed stuff first:
*
***************************************************************/

memcpy((char *)dspl_descr->dminput_pad->window,  (char *)dspl_descr->main_pad->window, sizeof(DRAWABLE_DESCR));
memcpy((char *)dspl_descr->dmoutput_pad->window, (char *)dspl_descr->main_pad->window, sizeof(DRAWABLE_DESCR));

dspl_descr->dminput_pad->window->lines_on_screen  = 1;
dspl_descr->dmoutput_pad->window->lines_on_screen = 1;


/***************************************************************
*
*  Get the boarder width from the main window.
*
***************************************************************/

DEBUG9(XERRORPOS)
if (!XGetWindowAttributes(dspl_descr->display,
                         dspl_descr->main_pad->x_window,
                         &window_attributes))
   {
      fprintf(stderr, "get_dm_windows:  Cannot get window attributes for main window 0x%02X\n", dspl_descr->main_pad->x_window);
      border_width = 1;
   }
else
   border_width = window_attributes.border_width;

if (border_width == 0)
   border_width = 1;

/***************************************************************
*
*  Get the pixel values from the main window gc.
*
***************************************************************/

valuemask = GCForeground | GCBackground;

DEBUG9(XERRORPOS)
if (!XGetGCValues(dspl_descr->display,
                  dspl_descr->main_pad->window->gc,
                  valuemask,
                  &gcvalues))
   {
      DEBUG(   fprintf(stderr, "get_dm_windows:  XGetGCValues failed for main window gc = 0x%X\n", dspl_descr->main_pad->window->gc);)
      gcvalues.foreground = BlackPixel(dspl_descr->display, DefaultScreen(dspl_descr->display));
      gcvalues.background = WhitePixel(dspl_descr->display, DefaultScreen(dspl_descr->display));
   }

/***************************************************************
*
*  Determine the height, width, X and Y of the windows.
*
***************************************************************/

window_height = dspl_descr->main_pad->window->font->ascent + dspl_descr->main_pad->window->font->descent;

dspl_descr->dminput_pad->window->line_height  = window_height;
dspl_descr->dmoutput_pad->window->line_height = window_height;

window_height += (window_height * DM_WINDOW_HEIGHT_PADDING) / 100;

dspl_descr->dminput_pad->window->height  = window_height;
dspl_descr->dmoutput_pad->window->height = window_height;

window_width = (dspl_descr->main_pad->window->width + 1) / 2;       /* divide by 2 rounding up */
dspl_descr->dmoutput_pad->window->width = window_width;

window_width = window_width - border_width;        /* take out the center line boarder */
dspl_descr->dminput_pad->window->width  = window_width;


dspl_descr->dminput_pad->window->x  = 0 - border_width; /* overlap the main window boarder on the right side */
dspl_descr->dmoutput_pad->window->x = window_width;     /* the two borders overlap */

dspl_descr->dminput_pad->window->y  = dspl_descr->main_pad->window->height - (window_height + border_width); /* 1 border width because bottom borders overlap */
dspl_descr->dmoutput_pad->window->y = dspl_descr->dminput_pad->window->y;


/***************************************************************
*
*  Create the DM input window.
*
***************************************************************/

window_attrs.background_pixel = gcvalues.background;
window_attrs.border_pixel     = gcvalues.foreground;
window_attrs.win_gravity      = UnmapGravity;


window_mask = CWBackPixel
            | CWBorderPixel
            | CWWinGravity;

DEBUG9(XERRORPOS)
dspl_descr->dminput_pad->x_window = XCreateWindow(
                                dspl_descr->display,
                                dspl_descr->main_pad->x_window,
                                dspl_descr->dminput_pad->window->x,     dspl_descr->dminput_pad->window->y,
                                dspl_descr->dminput_pad->window->width, dspl_descr->dminput_pad->window->height,
                                border_width,
                                window_attributes.depth,
                                InputOutput,                    /*  class        */
                                CopyFromParent,                 /*  visual       */
                                window_mask,
                                &window_attrs
    );


/***************************************************************
*
*  Create the DM output window.
*
***************************************************************/


DEBUG9(XERRORPOS)
dspl_descr->dmoutput_pad->x_window = XCreateWindow(
                                dspl_descr->display,
                                dspl_descr->main_pad->x_window,
                                dspl_descr->dmoutput_pad->window->x,     dspl_descr->dmoutput_pad->window->y,
                                dspl_descr->dmoutput_pad->window->width, dspl_descr->dmoutput_pad->window->height,
                                border_width,
                                window_attributes.depth,
                                InputOutput,                    /*  class        */
                                CopyFromParent,                 /*  visual       */
                                window_mask,
                                &window_attrs
    );



/***************************************************************
*
*  Map the windows and we are done.
*
***************************************************************/

DEBUG9(XERRORPOS)
XMapWindow(dspl_descr->display, dspl_descr->dminput_pad->x_window);
DEBUG9(XERRORPOS)
XMapWindow(dspl_descr->display, dspl_descr->dmoutput_pad->x_window);

return(1);

} /* end of get_dm_windows  */


/************************************************************************

NAME:      draw_dm_prompt

PURPOSE:    This routine will draw the dminput window prompt if needed.
            The prompt is needed if it is not scrolled off the left side of
            the window.

PARAMETERS:

   1.  dm_pix_subarea  - DRAWABLE_DESCR
               The dminput window pixmap subarea is passed.  This differs
               from the dminput_subarea in that it describes the position
               of the subarea in the pixmap instead of in the dm window.

GLOBAL DATA:
    dm_prompt   -  char string
               This is the current dm prompt.  It is global within this module.

    prompt_first_char  -  int
               The pad description for the dminput window is needed to
               find out it's scroll position.  It is global within this module.

FUNCTIONS :

   1.   If the prompt is scrolled off the left side of the
        window, do nothing.

   2.   Calculate the y coordinate of the baseline for the text.

   3.   Calculate the first of the prompt character to draw.

   4.   Write the text and return the pixel length of the text.


*************************************************************************/

void draw_dm_prompt(DISPLAY_DESCR    *dspl_descr,
                    DRAWABLE_DESCR   *dminput_pix_window)
{
char                 *drawn_prompt;
int                   len;
int                   baseline_y;
DMWIN_PRIVATE        *private;

if (((private = check_private_data(dspl_descr)) == NULL) || (dspl_descr->x_pixmap == None))
   return;

if (private->prompt_first_char < (int)strlen(private->dm_prompt))
   {

      baseline_y = dminput_pix_window->sub_y + dminput_pix_window->font->ascent;

      drawn_prompt = &private->dm_prompt[private->prompt_first_char];
      len = strlen(drawn_prompt);
      DEBUG9(XERRORPOS)
      XDrawString(dspl_descr->display,
                  dspl_descr->x_pixmap,
                  dminput_pix_window->gc,
                  MARGIN, baseline_y,
                  drawn_prompt, len);
   }

} /* end of draw_dm_prompt */


/************************************************************************

NAME:      dm_error

PURPOSE:    This routine will write a message to the DM error window
            and then beep.

PARAMETERS:

   1.  text         - pointer to char (INPUT)
                      This is the error message to write.

   2.  level       -  int (INPUT)
                      One of DM_ERROR_MSG, DM_ERROR_BEEP, DM_ERROR_LOG
                      Defined in dmwin.h

FUNCTIONS :

   1.   Call dm_error_dspl with the global display description.


*************************************************************************/

void  dm_error(char             *text,         /* input  */
               int               level)        /* input  */
{

dm_error_dspl(text, level, dspl_descr); /* use the global dspl_descr */

} /* end of dm_error  */


/************************************************************************

NAME:      dm_error_dspl

PURPOSE:    This routine will write a message to the DM error window
            for a particular display description and then beep.

PARAMETERS:

   1.  text        -  pointer to char (INPUT)
                      This is the error message to write.

   2.  level       -  int (INPUT)
                      One of DM_ERROR_MSG, DM_ERROR_BEEP, DM_ERROR_LOG
                      Defined in dmwin.h

   3.  dspl_descr  -  pointer to DISPLAY_DESCR (INPUT)
                      display to put the message out on.

FUNCTIONS :

   1.   Check for recursion.  If we recurse once, clear the DM output pad
        and continue.  If we recurse twice, stop with a crash file.

   2.   If there is a window to draw in, draw in it and save the message
        in the dmoutput pad.

   3.   If there is no window yet, see if we want to send this message to
        another window.  If so, do it.  If not write it to stdout.

   4.   If this is a message to be logged, write it to the log file.


*************************************************************************/

void  dm_error_dspl(char             *text,         /* input  */
                    int               level,        /* input  */
                    DISPLAY_DESCR    *dspl_descr)   /* input  */
{
int                   rc;
FILE                 *stream;
DMWIN_PRIVATE        *private;
char                 *to_window_char;
Window                to_window;
static char          *work_buff;
#ifdef WIN32
char                  e_log[256];
char                 *p;
#endif
#if  !defined(OpenNT) && !defined(WIN32)
char                  link[MAXPATHLEN];
#endif

if ((private = check_private_data(dspl_descr)) == NULL)
   return;

if (private->recursive_flag)
   {
      if ((private->recursive_flag == 1) && (dspl_descr->dmoutput_pad->token != NULL))
         {
            DEBUG(fprintf(stderr, "DM error recursed once\n");)
            mem_kill(dspl_descr->dmoutput_pad->token);
            dspl_descr->dmoutput_pad->token = NULL;
            dspl_descr->dmoutput_pad->token = mem_init(100, True); /* 100 % fill */
            WRITABLE(dspl_descr->dmoutput_pad->token) = False;
         }
      else
         {
            DEBUG(fprintf(stderr, "DM error recursed twice, exiting via interupt handler\n");)
            catch_quit(6);
         }
   }
else
   private->recursive_flag++;

if ((dspl_descr->dmoutput_pad->x_window != None) && (!dspl_descr->first_expose))
   {
      /***************************************************************
      *  Avoid putting out a lot of blank lines in the DM output window.
      ***************************************************************/
      if (text[0] == '\0' && private->last_dm_error_was_blank_line)
         {
            private->recursive_flag = 0;
            return;
         }
         
      /***************************************************************
      *  Put the new line in the memdata structure at the end.
      ***************************************************************/
      put_line_by_num(dspl_descr->dmoutput_pad->token,
                      total_lines(dspl_descr->dmoutput_pad->token)-1,
                      text,
                      INSERT);
#ifdef verbal
{
      char  msg[4096];
      if (text && text[0] && (text[0] != ' '))
         {
            snprintf(msg, sizeof(msg), "/home/plylerk/bin/say '%s' >/dev/null 2>&1", text);
            system(msg);
         }
}
#endif
      dspl_descr->dmoutput_pad->file_line_no =  total_lines(dspl_descr->dmoutput_pad->token)-1;
      dspl_descr->dmoutput_pad->first_line   = dspl_descr->dmoutput_pad->file_line_no;
      dspl_descr->dmoutput_pad->buff_modified = False;
      dspl_descr->dmoutput_pad->first_char = 0;

      if (text[0] == '\0')
         private->last_dm_error_was_blank_line = True;
      else
         private->last_dm_error_was_blank_line = False;

#ifdef WIN32
      if (dspl_descr->main_thread_id == GetCurrentThreadId())
         process_redraw(dspl_descr, DMOUTPUT_MASK & FULL_REDRAW, False);
#else
      process_redraw(dspl_descr, DMOUTPUT_MASK & FULL_REDRAW, False);
#endif
      if (level)
         {
            if (!private->beep_issued)
               {
                  DEBUG9(XERRORPOS)
#ifdef WIN32
                  if (dspl_descr->main_thread_id == GetCurrentThreadId())
                     ce_XBell(dspl_descr, 0);
#else
                  ce_XBell(dspl_descr, 0);
#endif
                  private->beep_issued = True; /* never more than one beep per keystroke */
               }
            private->last_dm_error_had_beep = True;
         }
      else
         private->last_dm_error_had_beep = False;

      dspl_descr->dmoutput_pad->buff_ptr = dspl_descr->dmoutput_pad->work_buff_ptr;
      DEBUG(fprintf(stderr, "%s\n", text);)
   }
else
   {
      /***************************************************************
      *  If the dmerror msg window id environment variable is set
      *  to a window id, send the message to the window;
      ***************************************************************/
      to_window_char = getenv(DMERROR_MSG_WINDOW);
      if (to_window_char &&
          (private->recursive_flag == 1) && /* only the first time into dm_error */
          dspl_descr->display &&
          ((work_buff = CE_MALLOC(strlen(text)+16)) != NULL) &&
          (sscanf(to_window_char, "%lX%9s", &to_window, work_buff) == 1))
         {
            sprintf(work_buff, "bell;msg '%s'", text);
            rc = cc_xdmc(dspl_descr->display, 0, work_buff, &to_window, True);
            free(work_buff);
            if (rc != 0)
               {
                  fprintf(stderr, "%s\n", text); /* Could not send, try stderr */
                  level = DM_ERROR_LOG;  /* force to log */
               }
         }
      else
         {
            fprintf(stderr, "%s\n", text);
            level = DM_ERROR_LOG; /* RES 7/8/97, send it to the log too */
         }
   }

if (level == DM_ERROR_LOG)
   {
#ifdef WIN32
      p = getenv("TEMP");
      if (p)
         _snprintf(e_log, sizeof(e_log), "%s\\%s", p, ERROR_LOG);
      else
         _snprintf(e_log, sizeof(e_log), "C:\\TEMP\\%s", ERROR_LOG);
      stream = fopen(e_log, "a");
#else
      if (getuid() && !geteuid())
         {
            DEBUG1(fprintf(stderr, "Setting uid to %d dm_error\n", getuid());)
            setuid(getuid());
         }
      stream = fopen(ERROR_LOG, "a");
#endif
      if (stream != NULL)
         {
            fprintf(stream, "%s PID(%d) UID(%d) '%s', in file: %s\n", CURRENT_TIME, getpid(), getuid(), text, edit_file);
            fclose(stream);
#if  !defined(OpenNT) && !defined(WIN32)
            if (readlink(ERROR_LOG, link, sizeof(link)) == -1)
               chmod(ERROR_LOG, 0666);
#endif
         }
      else
         fprintf(stderr, "%s\n", text);
   }

private->recursive_flag = 0;


} /* end of dm_error_dspl  */





/************************************************************************

NAME:      dm_clean

PURPOSE:    This routine clears error messages but leaves informatory
            data in the dm output window alone.  It also clears the record
            of whether beeps should be output.

PARAMETERS:

   1.  clear_dmoutput   - int (INPUT)
                          This flag specifies whether the dm_output window should bew
                          cleared if the last message had a beep.



GLOBAL VARIABLES:

   1.  last_dm_error_had_beep   - int (INPUT / OUTPUT)
               This static variable global to this module is true if
               the last call to dm_error specified DM_ERROR_BEEP or DM_ERROR_LOG.

   2.  beep_issued              - int (OUTPUT)
               To avoid getting more than one beep per keystroke, global

FUNCTIONS :

   1.   If the last dm error had a beep, clear the dm output window.

   2.   Reset the beep_issued flag to show that dm_error should output a
        beep the next time it needs to.


*************************************************************************/

void  dm_clean(int clear_dmoutput)
{
DMWIN_PRIVATE        *private;

DEBUG12(fprintf(stderr, "@dm_clean\n");)

if ((private = check_private_data(NULL)) == NULL)
   return;

private = (DMWIN_PRIVATE *)dspl_descr->dmwin_data;

if (clear_dmoutput && private->last_dm_error_had_beep)
   dm_error("", DM_ERROR_MSG);

private->beep_issued = False;

} /* end of dm_clean  */


/************************************************************************

NAME:      resize_dm_windows

PURPOSE:    This routine will resize a DM window to correctly fit back in the
            main window.
            NOTE:  This routine should be called after the main window
            data has been updated to reflect the resize.

PARAMETERS:

   1.  main_window   - pointer to DRAWABLE_DESCR (INPUT)
               This is a pointer to the main window drawable description.
               The DM windows, will be children of this window.

   2.  dminput_window -  pointer to DRAWABLE_DESCR (OUTPUT)
               Upon return, this drawable description describes the
               DM input window.

   3.  dmoutput_window -  pointer to DRAWABLE_DESCR (OUTPUT)
               Upon return, this drawable description describes the
               DM output window.


FUNCTIONS :

   1.   Determine the new width of the windows. They are half
        the width of the main window.  Window gravity has positioned
        them to the right places (I hope).


   2.   Resize each window.


*************************************************************************/

void  resize_dm_windows(DRAWABLE_DESCR   *main_window,          /* input   */
                        DRAWABLE_DESCR   *dminput_window,       /* input / output  */
                        DRAWABLE_DESCR   *dmoutput_window,      /* input / output  */
                        Display          *display,              /* input   */
                        Window            main_x_window,        /* input   */
                        Window            dminput_x_window,     /* input   */
                        Window            dmoutput_x_window)    /* input   */
{
int                   window_height;
int                   window_width;
int                   border_width;
XWindowAttributes     window_attributes;
XWindowChanges        window_attrs;
unsigned long         window_mask;

/***************************************************************
*
*  Get the border width from the main window.
*
***************************************************************/

DEBUG9(XERRORPOS)
if (!XGetWindowAttributes(display,
                         main_x_window,
                         &window_attributes))
   {
      fprintf(stderr, "resize_dm_windows:  Cannot get window attributes for main window 0x%02X\n", main_x_window);
      border_width = 1;
   }
else
   border_width = window_attributes.border_width;


/***************************************************************
*
*  Determine the height, width, X and Y of the windows.
*
***************************************************************/

window_height = main_window->font->ascent + main_window->font->descent;
window_height += (window_height * DM_WINDOW_HEIGHT_PADDING) / 100;

dminput_window->height  = window_height;
dmoutput_window->height = window_height;

window_width = (main_window->width + 1) / 2;       /* divide by 2 rounding up */
dmoutput_window->width = window_width;

window_width = window_width - border_width;        /* take out the center line boarder */
dminput_window->width  = window_width;


dminput_window->x  = 0 - border_width; /* overlap the main window border on the right side */
dmoutput_window->x = window_width;     /* the two borders overlap */

dminput_window->y  = main_window->height - (window_height + border_width); /* 1 border width because bottom borders overlap */
dmoutput_window->y = dminput_window->y;


dminput_window->lines_on_screen  = 1;
dmoutput_window->lines_on_screen = 1;

DEBUG1(
   fprintf(stderr, "resize_dm_windows:  main:  %dx%d%+d%+d, dminput:  %dx%d%+d%+d, dmoutput:  %dx%d%+d%+d\n",
                   main_window->width, main_window->height, main_window->x, main_window->y,
                   dminput_window->width, dminput_window->height, dminput_window->x, dminput_window->y,
                   dmoutput_window->width, dmoutput_window->height, dmoutput_window->x, dmoutput_window->y);
)

/***************************************************************
*
*  Resize the currently unmapped windows
*
***************************************************************/

window_attrs.height           = window_height;
window_attrs.width            = window_width;
window_attrs.x                = dminput_window->x;
window_attrs.y                = dminput_window->y;

window_mask = CWX
            | CWY
            | CWWidth
            | CWHeight;

DEBUG9(XERRORPOS)
XConfigureWindow(display, dminput_x_window, window_mask, &window_attrs);


window_attrs.x                = dmoutput_window->x;
window_attrs.y                = dmoutput_window->y;

DEBUG9(XERRORPOS)
XConfigureWindow(display, dmoutput_x_window, window_mask, &window_attrs);

/***************************************************************
*
*  Remap the windows and we are done.
*
***************************************************************/

DEBUG9(XERRORPOS)
XMapWindow(display, dminput_x_window);
XMapWindow(display, dmoutput_x_window);

} /* end of resize_dm_windows  */


/************************************************************************

NAME:      set_dm_prompt

PURPOSE:    This routine will load a new prompt for the DM input window.

PARAMETERS:

   1.  dminput_window -  pointer to DRAWABLE_DESCR (INPUT)
               This is the dm input window;

   2.  text   - pointer to char (INPUT)
               This is the command input text.  A null string
               restores the default value.

GLOBAL DATA:
   dminput_window - DRAWABLE_DESCR
               The dm input window description is accessed globally
               from windowdefs.h

   dm_prompt - string
               The DM prompt is stored global to this module only.

FUNCTIONS :

   1.   Set the static prompt to the default or passed
        string.

   2.   Calculate the length of the prompt in pixels.


*************************************************************************/

void  set_dm_prompt(DISPLAY_DESCR    *dspl_descr,
                    char             *text)
{
DMWIN_PRIVATE        *private;

/***************************************************************
*
*  Set up the prompt data for the DM input window:
*
***************************************************************/

if ((private = check_private_data(dspl_descr)) == NULL)
   return;

if (!text || (text[0] == '\0'))
   strcpy(private->dm_prompt, "Command: ");
else
   strcpy(private->dm_prompt, text);

dm_subarea(dspl_descr, dspl_descr->dminput_pad->window, dspl_descr->dminput_pad->pix_window);

} /* end of set_dm_prompt  */


/************************************************************************

NAME:      dm_subarea

PURPOSE:    This routine sets up one of the dm subwindows for both the
            window and pixmap.

PARAMETERS:

   1.  dm_window -  pointer to DRAWABLE_DESCR (INPUT)
               This is the dm window

   2.  pixmap -  Drawable (Xlib type) (INPUT)
               This is the drawable for the pixmap.

   3.  pix_subarea -  pointer to DRAWABLE_DESCR (OUTPUT)
               This is the pixmap subarea being set up.


FUNCTIONS :

   1.   Copy the standard stuff accross.

   2.   calculate the special y and height of the dm windows/

   3.   Set the x and width based on whether this is the input or output
        dm window.


*************************************************************************/

void  dm_subarea(DISPLAY_DESCR    *dspl_descr,   /* input / output */
                 DRAWABLE_DESCR   *dm_window,    /* input / output */
                 DRAWABLE_DESCR   *pix_window)  /* output */
{
int                   baseline_y;
int                   font_offcenter;
char                 *drawn_prompt;
DMWIN_PRIVATE        *private;

/***************************************************************
*
*  Do the fixed stuff first:
*
***************************************************************/

if ((private = check_private_data(dspl_descr)) == NULL)
   return;

/***************************************************************
*
*  Calculate the area which actually has data in it.
*  For the DM input window, the prompt is part of the window
*  but not the writable subarea.
*
***************************************************************/

font_offcenter          = dm_window->font->ascent - ((dm_window->font->ascent + dm_window->font->descent) / 2);
baseline_y              = (dm_window->height / 2) + font_offcenter;
dm_window->sub_y        = baseline_y - dm_window->font->ascent;
dm_window->sub_height   = dm_window->font->ascent + dm_window->font->descent;


if (dm_window == dspl_descr->dminput_pad->window)
   {
      if (private->prompt_first_char < (int)strlen(private->dm_prompt))
         {
            drawn_prompt = &private->dm_prompt[private->prompt_first_char];
            dm_window->sub_x = XTextWidth(dm_window->font, drawn_prompt, strlen(drawn_prompt)) + MARGIN;
         }
      else
         dm_window->sub_x     = MARGIN;
   }
else
   dm_window->sub_x     = MARGIN;


if ((int)dm_window->width >= dm_window->sub_x)
   dm_window->sub_width   = dm_window->width - dm_window->sub_x;
else
   dm_window->sub_width   = 0;

dm_window->lines_on_screen  = 1;

/***************************************************************
*  
*  Set up the pixmap subarea,  same size, but offset in the
*  pixmap instead of the window.
*  
***************************************************************/

memcpy((char *)pix_window, (char *)dm_window,  sizeof(DRAWABLE_DESCR));
pix_window->sub_x     += dm_window->x;
pix_window->sub_y     += dm_window->y;
pix_window->x          = 0;
pix_window->y          = 0;

DEBUG1(fprintf(stderr, "dm_subarea(%s): win %dx%d%+d%+d, subarea %dx%d%+d%+d, pix %dx%d%+d%+d\n",
                        ((dm_window == dspl_descr->dminput_pad->window) ? "input" : "output"),
                        dm_window->width, dm_window->height, dm_window->x, dm_window->y,
                        dm_window->sub_width, dm_window->sub_height, dm_window->sub_x, dm_window->sub_y,
                        pix_window->sub_width, pix_window->sub_height, pix_window->sub_x, pix_window->sub_y);
)

} /* end of dm_subarea  */


/************************************************************************

NAME:      scroll_dminput_prompt

PURPOSE:    This routine handles the scrolling of the DM_INPUT prompt.
            It is called by dm_ph when the cursor is in the DM input window.

PARAMETERS:

   1.  scroll_chars -  int (INPUT)
                       This is the number of characters being scrolled.  Positive
                       numbers indicate scrolling the window to the right (stuff
                       disappears off the left side of the window).  Negative
                       numbers indicate scrolling left.

FUNCTIONS :

   1.   Determine if this is a scroll right or a scroll left.

   2.   For a scroll right, if the prompt is still exposed, scroll the
        prompt by the lesser of the number of chars passed or the number
        of chars in the prompt left exposed.  Return the number of chars
        the prompt was scrolled as this must be subtracted from the amount
        to scroll for the base amount.

   3.   For a scroll left, this routine is not called until there is no
        more regular data to scroll.  That is, the dspl_descr->dminput_pad->first_char
        is zero.  If the prompt first char is non zero, scroll it by the
        scroll amount  and return the amount scrolled.  The amount scrolled
        can be less than the amount passed if there is less than scroll_chars
        of the prompt obscured.

RETURNED VALUE:
   chars_eaten - int
        The returned value specifies how much of the initial scroll amount
        was used up scrolling the prompt. This tells how much of the text
        which has to be scrolled off the left side of the screen.

*************************************************************************/

int scroll_dminput_prompt(int     scroll_chars)
{

int    prompt_len;
int    chars_eaten;
DMWIN_PRIVATE        *private;

if ((private = check_private_data(NULL)) == NULL)
   return(0);

prompt_len = strlen(private->dm_prompt) - private->prompt_first_char;
if (prompt_len < 0)
   prompt_len = 0;

if (scroll_chars > 0)  /* scroll the window to the to the right, stuff disappears off the left side of the window */
   {
      if (prompt_len == 0)
         return(0);
      if (prompt_len <= scroll_chars)
         {
            /* whole prompt is now gone */
            chars_eaten = prompt_len;
            private->prompt_first_char = strlen(private->dm_prompt);
         }
      else
         {
            /* some prompt is still left */
            chars_eaten = scroll_chars;
            private->prompt_first_char += scroll_chars;
         }
   }
else
   {
      if (private->prompt_first_char == 0)
         return(0);
      scroll_chars = ABSVALUE(scroll_chars);
      if (private->prompt_first_char <= scroll_chars)
         {
            /* whole prompt is now restored */
            chars_eaten = private->prompt_first_char;
            private->prompt_first_char = 0;
         }
      else
         {
            /* some prompt is still obscured */
            chars_eaten = private->prompt_first_char - scroll_chars;
            private->prompt_first_char -= scroll_chars;
         }
   }

dm_subarea(dspl_descr, dspl_descr->dminput_pad->window, dspl_descr->dminput_pad->pix_window);

return(chars_eaten);

} /* end of scroll_dminput_prompt */

/************************************************************************

NAME:      get_dm_prompt

PURPOSE:    This routine returns the address of the current dm input prompt.

GLOBAL DATA:

   dm_prompt  -  char string
               This is the dm prompt which is global to this module.


FUNCTIONS :

   1.   return the address off the dm prompt.

RETURNED VALUE:
   dm_prompt  -  pointer to char
        The static unix prompt address is returned.  This is always
        the save value.

*************************************************************************/

char  *get_dm_prompt()
{
DMWIN_PRIVATE        *private;

if ((private = check_private_data(NULL)) == NULL)
   return("Command: ");

return(private->dm_prompt);

} /* end of get_dm_prompt  */


/************************************************************************

NAME:      check_private_data - get the dmwin private data from the display description.

PURPOSE:    This routine returns the existing dmwin private area
            from the display description or initializes a new one.

PARAMETERS:

   none

GLOBAL DATA:

   dspl_descr  -  pointer to DISPLAY_DESCR (in windowdefs.h)
                  This is where the private data for this window set
                  is hung.


FUNCTIONS :

   1.   If the private area has already been allocated, return it.

   2.   Malloc a new area.  If the malloc fails, return NULL

   3.   Initialize the new private area and return it.

RETURNED VALUE
   private  -  pointer to DMWIN_PRIVATE
               This is the private static area used by the
               routines in this module.
               NULL is returned on malloc failure only.

NOTE:
   How do I know whether to pass NULL or a display description?
   If one is available pass it.  Winsetup.c and cc.c are two
   of the few routines where the current global display description
   is not the one being processed.  In call from these routines
   we will have to be passing in the new display description.
   This means it has to be an parameter to the dmwin routine
   which calls this.

*************************************************************************/

static DMWIN_PRIVATE *check_private_data(DISPLAY_DESCR    *passed_dspl_descr)
{
DMWIN_PRIVATE    *private;

if (passed_dspl_descr == NULL)
   passed_dspl_descr = dspl_descr;

if (passed_dspl_descr->dmwin_data)
   return((DMWIN_PRIVATE *)passed_dspl_descr->dmwin_data);

private = (DMWIN_PRIVATE *)CE_MALLOC(sizeof(DMWIN_PRIVATE));
if (!private)
   return(NULL);   
else
   memset((char *)private, 0, sizeof(DMWIN_PRIVATE));

passed_dspl_descr->dmwin_data = (void *)private;

return(private);


} /* end of check_private_data */


/************************************************************************

NAME:      ce_XBell - get the dmwin private data from the display description.

PURPOSE:    This routine returns the existing dmwin private area
            from the display description or initializes a new one.

PARAMETERS:

   dspl_descr  -  pointer to DISPLAY_DESCR (in windowdefs.h)
                  This is the display description for the window which
                  is to receive the bell.

   percent     -  int
                  This value is passed through to XBell if XBell
                  gets called.  See documentation for XBell for
                  a description of this parameter.

FUNCTIONS :
   1.   If the ce run parms suppress the bell, do nothing

   2.   If the ce run time parms request a visual bell, reverse
        video the dm output window.

   3.   The default action is to call XBell.


*************************************************************************/

void ce_XBell(DISPLAY_DESCR    *dspl_descr,
              int               percent)
{
PAD_DESCR            *pad;

if (!(SUPPRESS_BELL)) 
   {
      if (VISUAL_BELL)
         {
            if (VISUAL_BELL_UC)
               pad = dspl_descr->main_pad;
            else
               pad = dspl_descr->dmoutput_pad;
            XFillRectangle(dspl_descr->display,
                           pad->x_window,
                           pad->window->xor_gc,
                           0, /* left start pixel */
                           0, /* top start pixel */
                           pad->window->width,
                           pad->window->height);
            XFlush(dspl_descr->display);
            usleep(125000);
            XFillRectangle(dspl_descr->display,
                           pad->x_window,
                           pad->window->xor_gc,
                           0, /* left start pixel */
                           0, /* top start pixel */
                           pad->window->width,
                           pad->window->height);
            XFlush(dspl_descr->display);
         }
      else
         XBell(dspl_descr->display, 0);
   }

} /* end of ce_XBell */


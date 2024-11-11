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
*  module unixwin.c
*
*  Routines:
*         get_unix_window       - get the unix command window.
*         resize_unix_window    - handle a resize of the main window to the client.
*         unix_subarea          - Build the subarea drawing description for the unix command window.
*         need_unixwin_resize   - Check if we have to resize the Unix command window.
*         set_unix_prompt       - Prompt for input at the DM input window.
*         scroll_unix_prompt    - scroll the dm input window prompt.
*         draw_unix_prompt      - Draw the unix prompt in the unix command window.
*         get_unix_prompt       - Return the unix prompt address, set by set unix prompt.
*         get_drawn_unix_prompt - Return the unix to draw, set by set unix prompt or dm_sp
*         dm_sp                 - set prompt command
*         setprompt_mode        - Are we in setprompt mode
*         dm_prefix             - ceterm prefix command for passed DM commands
*         ceterm_prefix_cmds    - ceterm check for dm commands passed from shell.
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h         */
#include <string.h>         /* /usr/include/string.h */
#include <errno.h>          /* /usr/include/errno.h         */
#include <time.h>           /* /usr/include/time.h          */

#ifndef WIN32
#include <X11/X.h>          /* /usr/include/X11/X.h      */
#endif

#include "borders.h"
#include "debug.h"
#include "dmwin.h"
#include "emalloc.h"
#include "getevent.h"
#include "gc.h"
#include "pad.h"
#include "unixwin.h"
#include "windowdefs.h"  /* used in scroll_unix_prompt for global dspl_descr and ceterm_prefix_cmds for edit_file */
#include "xerrorpos.h"

/***************************************************************
*  
*  Stack structure used to stack prefix commands:
*  
***************************************************************/
struct prefix_stack ;
struct prefix_stack
{
   struct prefix_stack    *next;   
   int                      len;
   char                    *ceterm_prefix_string;
};

typedef struct prefix_stack PREFIX_STACK; 


/***************************************************************
*  
*  Global prompt data used by set_unix_prompt and scroll_unix_prompt.
*  
***************************************************************/

static char               unix_prompt[MAX_LINE+1] = "";
static int                prompt_first_char = 0;
static char              *drawn_unix_prompt = unix_prompt;

#define CETERM_PREFIX_INIT  {0x02, 0x02, '\0'}

static PREFIX_STACK      *ceterm_prefix_stack = NULL;
static char INITIAL_CETERM_PREFIX[]  = CETERM_PREFIX_INIT;


/***************************************************************
  
NAME:      get_unix_window       - get the unix command window.

PURPOSE:    This routine sets up and creates the initial unix command window.
            It needs the main window and dm_input window descriptions to be
            able to size and position itself.
            NOTE:  This routine should be called after the main and dm_input 
            windows are created but before the XSelectInput is done for the main window.

PARAMETERS:

   1.  display       - Display (Xlib type) (INPUT)
                       This is the display token needed for Xlib calls.

   2.  main_x_window - Window (Xlib type) (INPUT)
                       This is the X window id of the window being highlighted.

   3.  main_window   - pointer to DRAWABLE_DESCR (INPUT)
               This is a pointer to the main window drawable description.
               The DM windows, will be children of this window.

   4.  dminput_window -  pointer to DRAWABLE_DESCR (INPUT)
               This is the description of the DM input window.  It is
               needed  for positioning the unix window.
               It could be NULL.

   5.  unix_window -  pointer to DRAWABLE_DESCR (OUTPUT)
               Upon return, this drawable description describes the
               Unix command window.


FUNCTIONS :

   1.   Determine the size of the unix window.  It is the width of the main window
        and the height of the font + UNIX_WINDOW_HEIGHT_PADDING% tall.
        The border width is 1.

   2.   Create the Unix window, map the window and select the input
        events on the window.


*************************************************************************/

void  get_unix_window(DISPLAY_DESCR    *dspl_descr)          /* input / output  */
{
int                   window_height;
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

memcpy((char *)dspl_descr->unix_pad->window, (char *)dspl_descr->main_pad->window, sizeof(DRAWABLE_DESCR));

dspl_descr->unix_pad->window->lines_on_screen  = 1;


/***************************************************************
*
*  Get the border width from the main window, and other stuff.
*
***************************************************************/

DEBUG9(XERRORPOS)
if (!XGetWindowAttributes(dspl_descr->display,
                          dspl_descr->main_pad->x_window,
                          &window_attributes))
   {
      DEBUG(fprintf(stderr, "get_unix_window:  Cannot get window attributes for main window 0x%02X\n", dspl_descr->main_pad->x_window);)
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
      DEBUG(fprintf(stderr, "get_unix_window: XGetGCValues failed for main window gc = 0x%X\n", dspl_descr->main_pad->window->gc);)
      gcvalues.foreground = BlackPixel(dspl_descr->display, DefaultScreen(dspl_descr->display));
      gcvalues.background = WhitePixel(dspl_descr->display, DefaultScreen(dspl_descr->display));
   }

/***************************************************************
*
*  Determine the height, width, X and Y of the windows.
*
***************************************************************/

window_height = dspl_descr->main_pad->window->font->ascent + dspl_descr->main_pad->window->font->descent;

dspl_descr->unix_pad->window->line_height  = window_height;

window_height += 2 * MARGIN;

dspl_descr->unix_pad->window->height = window_height;

dspl_descr->unix_pad->window->x  = 0 - border_width; /* overlap the main window border on the left side */
dspl_descr->unix_pad->window->y  = dspl_descr->dminput_pad->window->y   - (window_height + border_width); /* 1 border width because bottom borders overlap */

/***************************************************************
*
*  Create the unix window.
*
***************************************************************/

window_attrs.background_pixel = gcvalues.background;
window_attrs.border_pixel     = gcvalues.foreground;
window_attrs.win_gravity      = UnmapGravity;


window_mask = CWBackPixel
            | CWBorderPixel
            | CWWinGravity;

DEBUG9(XERRORPOS)
dspl_descr->unix_pad->x_window = XCreateWindow(
                                dspl_descr->display,
                                dspl_descr->main_pad->x_window,
                                dspl_descr->unix_pad->window->x,     dspl_descr->unix_pad->window->y,
                                dspl_descr->unix_pad->window->width, dspl_descr->unix_pad->window->height,
                                border_width,
                                window_attributes.depth,
                                InputOutput,                    /*  class        */
                                CopyFromParent,                 /*  visual       */
                                window_mask,
                                &window_attrs
    );


/***************************************************************
*
*  Map the window and we are done.
*
***************************************************************/

DEBUG9(XERRORPOS)
XMapWindow(dspl_descr->display, dspl_descr->unix_pad->x_window);

/***************************************************************
*  
*  Create the initial prefix string.
*  
***************************************************************/

if (ceterm_prefix_stack == NULL && *INITIAL_CETERM_PREFIX)
   {
      ceterm_prefix_stack = (PREFIX_STACK *)CE_MALLOC(sizeof(PREFIX_STACK));
      if (ceterm_prefix_stack) /* if malloc worked */
         {
            ceterm_prefix_stack->next = NULL;
            ceterm_prefix_stack->len = strlen(INITIAL_CETERM_PREFIX);
            ceterm_prefix_stack->ceterm_prefix_string = malloc_copy(INITIAL_CETERM_PREFIX);
            if (!ceterm_prefix_stack->ceterm_prefix_string) /* if malloc failed, get rid of the stack element */
               {
                  free(ceterm_prefix_stack);
                  ceterm_prefix_stack = NULL;
               }
         }
   }


} /* end of get_unix_window  */


/************************************************************************

NAME:      resize_unix_window    - handle a resize of the main window

PURPOSE:    This routine will resize the unix window to correctly fit back in the
            main window.
            NOTE:  This routine should be called after the main window
            data has been updated to reflect the resize.

PARAMETERS:

   1.  main_window   - pointer to DRAWABLE_DESCR (INPUT)
               This is a pointer to the main window drawable description.
               The DM windows, will be children of this window.

   2.  dminput_window -  pointer to DRAWABLE_DESCR (INPUT)
               Upon return, this drawable description describes the
               DM input window.  It can be NULL.

   3.  unix_window -  pointer to DRAWABLE_DESCR (INPUT / OUTPUT)
               Upon return, this drawable description describes the
               unix command window.

   4.  unix_window -  int (INPUT)
               This is the number of lines to be displayed in the 
               unix command window.


FUNCTIONS :

   1.   Determine the new width of unix window.  It is the same as
        the width of the main window.  Window gravity has positioned
        it to the right places (I hope).


   2.   Resize each window.


*************************************************************************/

void  resize_unix_window(DISPLAY_DESCR    *dspl_descr,
                         int               window_lines)         /* input   */
{

int                   line_height;
int                   window_height;
int                   border_width;
XWindowChanges        window_attrs;
unsigned long         window_mask;

border_width = BORDER_WIDTH;

if (window_lines > MAX_UNIX_LINES)
   window_lines = MAX_UNIX_LINES;
if (window_lines > dspl_descr->main_pad->window->lines_on_screen || window_lines < 1)
   window_lines = 1;


/***************************************************************
*
*  Determine the height, width, X and Y of the window.
*
***************************************************************/

line_height = dspl_descr->main_pad->window->font->ascent + dspl_descr->main_pad->window->font->descent;
line_height +=  (line_height * dspl_descr->padding_between_line) / 100;

window_height = line_height * window_lines;
window_height += 2 * MARGIN;

dspl_descr->unix_pad->window->height  = window_height;
dspl_descr->unix_pad->window->width   = dspl_descr->main_pad->window->width;

dspl_descr->unix_pad->window->x  = 0 - border_width; /* overlap the main window border on the right side */

dspl_descr->unix_pad->window->lines_on_screen = window_lines;

dspl_descr->unix_pad->window->y  = dspl_descr->dminput_pad->window->y   - (window_height + border_width); /* 1 border width because bottom borders overlap */

DEBUG1(
   fprintf(stderr, "resize_unix_window:  main:  %dx%d%+d%+d, dminput:  %dx%d%+d%+d, unix:  %dx%d%+d%+d\n",
                   dspl_descr->main_pad->window->width, dspl_descr->main_pad->window->height, dspl_descr->main_pad->window->x, dspl_descr->main_pad->window->y,
                   dspl_descr->dminput_pad->window->width, dspl_descr->dminput_pad->window->height, dspl_descr->dminput_pad->window->x, dspl_descr->dminput_pad->window->y,
                   dspl_descr->unix_pad->window->width, dspl_descr->unix_pad->window->height, dspl_descr->unix_pad->window->x, dspl_descr->unix_pad->window->y);
)

/***************************************************************
*
*  Resize the currently unmapped window
*
***************************************************************/

window_attrs.height           = dspl_descr->unix_pad->window->height;
window_attrs.width            = dspl_descr->unix_pad->window->width;
window_attrs.x                = dspl_descr->unix_pad->window->x;
window_attrs.y                = dspl_descr->unix_pad->window->y;

window_mask = CWX
            | CWY
            | CWWidth
            | CWHeight;

DEBUG9(XERRORPOS)
XConfigureWindow(dspl_descr->display, dspl_descr->unix_pad->x_window , window_mask, &window_attrs);


/***************************************************************
*
*  Remap the windows and we are done.
*
***************************************************************/

DEBUG9(XERRORPOS)
XMapWindow(dspl_descr->display, dspl_descr->unix_pad->x_window);

} /* end of resize_unix_window  */


/************************************************************************

NAME:      unix_subarea - Build the subarea drawing description for the unix command window.

PURPOSE:    This routine will build the drawing subarea for the Unix
            command window.  This is the area which can be filled with text.

PARAMETERS:
   1.  unix_window          -  pointer to DRAWABLE_DESCR (INPUT)
                               This is the unix window to be drawn into.

   2.  first_line_in_window -  int (INPUT)
                               This is the first line in the window. 

   3.  unixcmd_pix_subarea  -  pointer to DRAWABLE_DESCR (OUTPUT)
                               This is the text drawable subarea to be
                               created with respect to the 
                               pixmap.  It is the same size as the window
                               text area, but the y offset is different.
                

FUNCTIONS :
   1.   Copy the standard stuff across.

   2.   Calculate the x, y, width, and height of the area that text
        can be drawn into.

   3.   Create the pixmap copy of the sub area, which is offset from
        the window version.

*************************************************************************/

void  unix_subarea(DRAWABLE_DESCR   *unix_window,           /* input  */
                   DRAWABLE_DESCR   *unixcmd_pix_window)   /* output  */
{
char                 *drawn_prompt;


/***************************************************************
*
*  Calculate the area which actually has data in it.
*
***************************************************************/

if (prompt_first_char < (int)strlen(drawn_unix_prompt))
   {
      drawn_prompt = &drawn_unix_prompt[prompt_first_char];
      unix_window->sub_x = XTextWidth(unix_window->font, drawn_prompt, strlen(drawn_prompt)) + MARGIN;
   }
else
   unix_window->sub_x = MARGIN;

if ((int)unix_window->width >= unix_window->sub_x)
   unix_window->sub_width   = unix_window->width - unix_window->sub_x;
else
   unix_window->sub_width   = 0;

unix_window->sub_y       = MARGIN;
unix_window->sub_height  = unix_window->height - (MARGIN * 2);

/***************************************************************
*  
*  Set up the pixmap subarea,  same size, but offset in the
*  pixmap.
*  
***************************************************************/

memcpy((char *)unixcmd_pix_window,  (char *)unix_window, sizeof(DRAWABLE_DESCR));
unixcmd_pix_window->sub_y     +=  unix_window->y;
unixcmd_pix_window->x          =  0;
unixcmd_pix_window->y          =  0;

} /* end of unix_subarea  */



/************************************************************************

NAME:      need_unixwin_resize - Check if we have to resize the Unix command window.

PURPOSE:    This routine checks the number of lines in the Unix window to see
            if the window needs to be resized.

PARAMETERS:

   1.  unixcmd_cur_descr -  pointer to PAD_DESCR (INPUT)
                      This is the pad description for the unix command input window.


FUNCTIONS :

   1.   See how many lines should be displayed;

   2.   Compare this to the number of lines currently displayed.

RETURNED VALUE:
   resize_needed  -  Boolean flag
                     True  -  A resize of the unix command window and dependent
                              windows is needed.
                     False -  The window is fine the way it is.

*************************************************************************/

int need_unixwin_resize(PAD_DESCR   *unixcmd_cur_descr)
{
register int   lines_to_display;

lines_to_display = total_lines(unixcmd_cur_descr->token);
if (lines_to_display > MAX_UNIX_LINES)
   lines_to_display = MAX_UNIX_LINES;
if (lines_to_display == 0)
   lines_to_display = 1;


if (lines_to_display  != unixcmd_cur_descr->window->lines_on_screen)
   {
      DEBUG1(fprintf(stderr, "need_unixwin_resize:  lines to display: %d, current lines on screen: %d, need resize\n", lines_to_display, unixcmd_cur_descr->window->lines_on_screen);)
      return(True);
   }
else
   return(False);

} /* end of need_unixwin_resize */


/************************************************************************

NAME:      set_unix_prompt

PURPOSE:    This routine will load a new prompt for the Unix command window.
            Sometimes this is the actual prompt and sometimes it is just
            a piece of a line being returned from the shell.  There is no
            difference except that the partial line will be replaced soon
            after it is set.

PARAMETERS:
   1.  disp_descr - DISPLAY_DESCR (INPUT)
                    This is the display description for the current window
                    set.  This should be the pad_mode dspl_descr.

   2.  text       - pointer to char (INPUT)
                    This is the command input text.  A null string
                    is a null prompt.

GLOBAL DATA:

   unix_prompt  -  char string
               This is the unix prompt which is global to this module.


FUNCTIONS :

   1.   Set the static prompt to the passed string.

   2.   Calculate the length of the prompt in pixels.


*************************************************************************/

void  set_unix_prompt(DISPLAY_DESCR    *dspl_descr,  /* input  */
                      char             *text)        /* input  */
{
int    len;
int    max_pixels;

/***************************************************************
*
*  Set up the prompt data for the Unix command input window:
*
***************************************************************/

if (!text)
   unix_prompt[0] = '\0';
else
   {
      len = strlen(text);
      if (len > sizeof(unix_prompt)-1)
         {
            dm_error_dspl("Unix prompt too long - truncated", DM_ERROR_BEEP, dspl_descr);
            len = sizeof(unix_prompt)-1;
         }
      strncpy(unix_prompt, text, len);
      unix_prompt[len] = '\0';
   }

DEBUG19(fprintf(stderr, "set_unix_prompt: prompt set to \"%s\" current scroll = %d\n", unix_prompt, prompt_first_char);)

#ifdef blah
/***************************************************************
*  RES 4/4/2003
*  Do not let the end of the UNIX prompt go off screen.  Shift
*  if over till it is OK.
***************************************************************/
max_pixels = dspl_descr->unix_pad->window->width - (2 * MARGIN);
if ((XTextWidth(dspl_descr->unix_pad->window->font, drawn_unix_prompt, strlen(drawn_unix_prompt)) + MARGIN) > max_pixels)
   scroll_unix_prompt(strlen(drawn_unix_prompt));
#endif


unix_subarea(dspl_descr->unix_pad->window,
             dspl_descr->unix_pad->pix_window);

} /* end of set_unix_prompt  */


/************************************************************************

NAME:      dm_sp - set prompt command

PURPOSE:    This routine save a unix prompt with is the forced prompt
            and turn on setprompt mode.

PARAMETERS:
   1.  dspl_descr   - pointer to DISPLAY_DESCR (INPUT)
               This is the display description of for the display which
               is in pad mode.

   2.  text   - pointer to char (INPUT)
               This is the command input text.  A null string
               turns off setprompt mode.

GLOBAL DATA:
   drawn_unix_prompt  -  char string
               This is the unix prompt which is global to this module.

FUNCTIONS :

   1.   Set the static prompt to the passed string.

*************************************************************************/

void  dm_sp(DISPLAY_DESCR    *dspl_descr,  /* input  */
            char             *text)        /* input  */
{
int    len;

/***************************************************************
*
*  Set up the prompt data for the Unix command input window:
*
***************************************************************/

if (!text || !(*text))
   {
      if (drawn_unix_prompt != unix_prompt)
         free(drawn_unix_prompt);
      drawn_unix_prompt = unix_prompt;
   }
else
   {
      len = strlen(text);
      drawn_unix_prompt = malloc_copy(text);
      if (!drawn_unix_prompt)
         drawn_unix_prompt = unix_prompt;
      else
         if (len > MAX_LINE)
            {
               dm_error_dspl("Unix prompt too long - truncated", DM_ERROR_BEEP, dspl_descr);
               drawn_unix_prompt[len] = '\0';
            }
   }

DEBUG19(fprintf(stderr, "dm_sp: prompt set to \"%s\" current scroll = %d\n", drawn_unix_prompt, prompt_first_char);)

unix_subarea(dspl_descr->unix_pad->window,
             dspl_descr->unix_pad->pix_window);

} /* end of dm_sp  */


/************************************************************************

NAME:      scroll_unix_prompt

PURPOSE:    This routine handles the scrolling of the Unix command window prompt.
            It is called by dm_ph when the cursor is in the Unix command window.

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
        more regular data to scroll.  That is, the dminput_cur_descr.first_char
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

int scroll_unix_prompt(int     scroll_chars)
{

int    prompt_len;
int    chars_eaten;

prompt_len = strlen(drawn_unix_prompt) - prompt_first_char;
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
            prompt_first_char = strlen(drawn_unix_prompt);
         }
      else
         {
            /* some prompt is still left */
            chars_eaten = scroll_chars;
            prompt_first_char += scroll_chars;
         }
   }
else
   {
      if (prompt_first_char == 0)
         return(0);
      scroll_chars = ABSVALUE(scroll_chars);
      if (prompt_first_char <= scroll_chars)
         {
            /* whole prompt is now restored */
            chars_eaten = prompt_first_char;
            prompt_first_char = 0;
         }
      else
         {
            /* some prompt is still obscured */
            chars_eaten = prompt_first_char - scroll_chars;
            prompt_first_char -= scroll_chars;
         }
   }

unix_subarea(dspl_descr->unix_pad->window,
             dspl_descr->unix_pad->pix_window);

return(chars_eaten);

} /* end of scroll_unix_prompt */


/************************************************************************

NAME:      draw_unix_prompt

PURPOSE:    This routine will draw the dminput window prompt if needed.
            The prompt is needed if it is not scrolled off the left side of
            the window.

PARAMETERS:
   1.  display             - Display (Xlib type) (INPUT)
                             This is the display token needed for Xlib calls.

   2.  x_pixmap            - Pixmap (Xlib type) (INPUT)
                             This is the X window id of the window being highlighted.


   3.  unixcmd_pix_subarea  - pointer to DRAWABLE_DESCR
                              The unixcmd window pixmap subarea is passed.  This differs
                              from the unixcmd_subarea in that it describes the position
                              of the subarea in the pixmap instead of in the unix window.

GLOBAL DATA:
    drawn_unix_prompt   -  char string
               This is the current unix prompt.  It is global within this module.

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

void draw_unix_prompt(Display        *display,
                      Pixmap          x_pixmap,
                      DRAWABLE_DESCR *unixcmd_pix_window)
{
char                 *drawn_prompt;
int                   len;
int                   baseline_y;
char                  work[MAX_LINE+1];


vt100_eat(work, drawn_unix_prompt);

if (prompt_first_char < (int)strlen(work))
   {

      baseline_y = unixcmd_pix_window->sub_y + unixcmd_pix_window->font->ascent;

      drawn_prompt = &work[prompt_first_char];
      len = strlen(drawn_prompt);
      DEBUG9(XERRORPOS)
      XDrawString(display,
                  x_pixmap,
                  unixcmd_pix_window->gc,
                  MARGIN, baseline_y,
                  drawn_prompt, len);
   }

} /* end of draw_unix_prompt */


/************************************************************************

NAME:      get_unix_prompt

PURPOSE:    This routine returns the address of the current unix prompt.
            It is used to copy the data to the output pad.

GLOBAL DATA:

   unix_prompt  -  char string
               This is the unix prompt which is global to this module.


FUNCTIONS :

   1.   return the address off the unix prompt.

RETURNED VALUE:
   unix_prompt  -  pointer to char
        The static unix prompt address is returned.  This is always
        the same value.

*************************************************************************/

char  *get_unix_prompt(void)
{
   return(unix_prompt);

} /* end of get_unix_prompt  */


/************************************************************************

NAME:   get_drawn_unix_prompt - Return the unix to draw, set by set unix prompt or dm_sp

PURPOSE:    This routine returns the address of the current unix prompt
            for drawing purposes.  This may be the prompt from the
            shell or one from the set prompt command.  

GLOBAL DATA:

   drawn_unix_prompt  -  char string
               This is the unix prompt which is global to this module.


FUNCTIONS :

   1.   return the address off the unix prompt.

RETURNED VALUE:
   drawn_unix_prompt  -  pointer to char
        This is the current drawn prompt.

*************************************************************************/

char  *get_drawn_unix_prompt(void)
{
   return(drawn_unix_prompt);

} /* end of get_drawn_unix_prompt  */


/************************************************************************

NAME:   setprompt_mode - Are we in setprompt mode

PURPOSE:    This routine true if we are in setprompt mode.  This
            means a setprompt has been successfully executed.
GLOBAL DATA:

   drawn_unix_prompt  -  char string
               This is the current drawable unix prompt which is global to this module.

   unix_prompt  -  char string
               This is the unix prompt which is global to this module.


FUNCTIONS :

   1.   Return true if the drawn prompt differs from the set normal prompt.
        Otherwise return false.

RETURNED VALUE:
   setprompt mode  -  int
                      True  -  We are insetprompt mode
                      False -  We are not in setprompt mode

*************************************************************************/

int setprompt_mode(void)
{
   return(drawn_unix_prompt != unix_prompt);

} /* end of setprompt_mode  */


/************************************************************************

NAME:      dm_prefix - ceterm prefix command for passed DM commands

PURPOSE:    This routine saves a ceterm prefix string and turns on
            prefix mode.  In prefix mode, a line from the shell which
            is prefixed by the magic string (which may contain non-printables)
            is processed as a string of dm commands instead of being
            put in the output pad.  This is used by ceterm aware programs.

PARAMETERS:
   1.  text   - pointer to char (INPUT)
               This is the prefix text.  A null string
               turns off prefix mode.

GLOBAL DATA:
   ceterm_prefix_string  -  pointer to char
               This is the prefix which is global to this module.
               When prefix processing is off, it is NULL.

FUNCTIONS :
   1.   Set the static prefix to the passed string.



*************************************************************************/

void  dm_prefix(char             *text)        /* input  */
{
PREFIX_STACK   *current;

/***************************************************************
*
*  Set up the prompt data for the Unix command input window:
*
***************************************************************/

if (!text || !(*text))
   {
      /***************************************************************
      *  Null text, pop the stack.
      ***************************************************************/
      if (ceterm_prefix_stack)
         {
            current = ceterm_prefix_stack;
            ceterm_prefix_stack = ceterm_prefix_stack->next;
            if (current->ceterm_prefix_string)
               free(current->ceterm_prefix_string);
            free(current);
         }
   }
else
   {
      current = (PREFIX_STACK *)CE_MALLOC(sizeof(PREFIX_STACK));
      if (current)
         {
            current->next = ceterm_prefix_stack;
            current->len = strlen(text);
            current->ceterm_prefix_string = malloc_copy(text);
            if (current->ceterm_prefix_string)
               {
                  if (current->len > MAX_LINE)
                     {
                        dm_error("(prefix) Ceterm DM command prefix too long - prefix command mode turned off", DM_ERROR_BEEP);
                        free(current->ceterm_prefix_string);
                        free(current);
                     }
                  else
                     {
                        /***************************************************************
                        *  Push new entry on the stack
                        ***************************************************************/
                        ceterm_prefix_stack = current;
                     }
               }
            else
               free(current);
         }
   }

DEBUG19(fprintf(stderr, "dm_prefix: prefix set to \"%s\"\n", ceterm_prefix_stack->ceterm_prefix_string);)

} /* end of dm_prefix  */


/************************************************************************

NAME:      ceterm_prefix_cmds - ceterm check for dm commands passed from shell.

PURPOSE:    This routine checks the line passed to see if it is a list of
            dm commands passed from the shell.  If so, it salts the commands
            away to be executed.

PARAMETERS:
   1.  dspl_descr   - pointer to DISPLAY_DESCR (INPUT)
               This is the display description of for the display which
               is in pad mode.

   2.  line   - pointer to char (INPUT)
                This is the line of output from the shell to be checked.

GLOBAL DATA:
   ceterm_prefix_stack  -  pointer to PREFIX_STACK
               This is the stack of prefixes to look at.

FUNCTIONS :
   1.   Check each prefix if there are any against the leading
        characters of the passed line.  If no match is found return False.

   2.   Check for the special trigger to cause the escape character to
        be returned.  This technique is used so what is sent by the
        ce_isceterm command is <esc>%<VERIFY_CMD_TRIGGER>.  An xterm
        or other terminal driver will ignore the single character.
        Ceterm knows how to respond.

   3.   Put the commands in a place to be found by the command processor
        and return true.

RETURNED VALUE:
   cmds  -  int flag
            True  -  The line contained commands and was processed.
                     Ignore the line.
            False -  Normal line

*************************************************************************/

#include "keypress.h"
#include "parms.h"

int  ceterm_prefix_cmds(DISPLAY_DESCR    *dspl_descr,  /* input  */
                        char             *line)        /* input  */
{
PREFIX_STACK         *current;
char                 *p;
char                 *q;
XEvent                event_union;      /* The event structure filled in by X as events occur            */

/***************************************************************
*  Check each known prefix (could be zero) to see if this is
*  a prefix line.  
***************************************************************/

for (current = ceterm_prefix_stack; current; current=current->next)
   if ((p = strstr(line, current->ceterm_prefix_string)) != NULL)
      {
         /***************************************************************
         *  We hit a match on the prefix.  If there are commands, use
         *  them.  Watch for the special trigger to send back an escape.
         ***************************************************************/
         q = p + current->len; /* skip the prefix */
         if (*q)
            {
               /***************************************************************
               *  RES 2/3/1999 Process the passed commands immediately
               *  by calling the keypress routine instead of setting up a
               *  fake keystroke to do them later.
               ***************************************************************/
               store_cmds(dspl_descr, q, False); /* put commands where keypress will find them */

               event_union.xkey.type        = KeyPress;
               event_union.xkey.serial      = 0;
               event_union.xkey.send_event  = True;
               event_union.xkey.display     = dspl_descr->display;
               event_union.xkey.window      = dspl_descr->main_pad->x_window;
               event_union.xkey.root        = RootWindow(dspl_descr->display, DefaultScreen(dspl_descr->display));
               event_union.xkey.subwindow   = None;
               event_union.xkey.time        = 0;
               event_union.xkey.x           = 0;
               event_union.xkey.y           = 0;
               event_union.xkey.x_root      = dspl_descr->main_pad->window->x;
               event_union.xkey.y_root      = dspl_descr->main_pad->window->y;
               event_union.xkey.state       = None;
               event_union.xkey.keycode     = 0; /* an invalid keycode, can never translate to anything */
               event_union.xkey.same_screen = True;

               keypress(dspl_descr, 
                        &event_union,  /* a fake keystroke         */
                        READLOCK,      /* from parms.h             */
                        edit_file,     /* global from windowdefs.h */
                        "ceterm");     /* always a ceterm          */
               /*fake_keystroke(dspl_descr);*/
            }
         *p = '\0';
         DEBUG19(fprintf(stderr, "ceterm_prefix_cmds: commands detected: \"%s\"\n", q);)
         return(True);
      }
return(False);

} /* end of ceterm_prefix_cmds  */


#endif

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
*  module intialization routines:
*
*  These routines exist solely to move code out of crpad.c
*
*  Routines:
*         get_font_data                   - load the font and font data
*         setup_lineno_window             - set up font data for a window from the font parm
*         setup_window_manager_properties - set up the window manager hints data
*         setup_winlines                  - allocate a new winlines structure
*         change_icon                     - Change the icon name and or pixmap.
*         build_cmd_args                  - turn the option values into a argv/argc
*         save_restart_state              - Save the state so the session mangaer can restart us
*         get_execution_path              - Get the execution path of this program
*
*  Internal:
*         build_default_icon              - Build the pixmap for a default Ce icon
*         workspace_parm                  - Get the current workspace number as a string.
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h      */
#include <stdlib.h>

#ifdef WIN32
#include <io.h>
#include <direct.h>
#include <winsock2.h>

#define access _access
#define getcwd _getcwd

extern catch_quit(int);

#else
#include <pwd.h>            /* /usr/include/pwd.h        */
#include <sys/param.h>      /* /usr/include/sys/param.h  */
#endif


#ifndef MAXPATHLEN
#ifdef WIN32
#define MAXPATHLEN	256
#else
#define MAXPATHLEN	1024
#endif
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "borders.h"
#include "buffer.h"
#include "cdgc.h"
#include "debug.h"
#include "dmwin.h"
#include "editiconNT.h"
#include "editicon.h"
#include "emalloc.h"
#include "init.h"
#include "mouse.h"
#include "normalize.h"
#include "pad.h"    /* needed for ce_getcwd */
#include "parms.h"
#include "pw.h"
#ifdef PAD
#include "shelliconNT.h"
#include "shellicon.h"
#endif
#include "xerrorpos.h"

#ifndef HAVE_STRLCPY
#include "strl.h"
#endif

#ifdef WIN32
#ifdef WIN32_XNT
#include "xnt.h"
#else
#include "X11/Xutil.h"      /* /usr/include/X11/Xutil.h  */
#include "X11/Xatom.h"      /* /usr/include/X11/Xatom.h  */
#endif
#else
#include <X11/Xutil.h>      /* /usr/include/X11/Xutil.h  */
#include <X11/Xatom.h>      /* /usr/include/X11/Xatom.h  */
#endif

void   exit(int code);
#if !defined(FREEBSD) && !defined(WIN32) && !defined(_OSF_SOURCE) && !defined(OMVS) && !defined(linux) && !defined(_INCLUDE_HPUX_SOURCE)
void   gethostname(char  *target, int size);
int    getuid(void);
#endif
static void set_home(void);

/***************************************************************
*  
*  Prototypes for local routines
*  
***************************************************************/

static char  *workspace_parm(Display              *display,
                             Window                main_window);

/************************************************************************

NAME:      get_font_data

PURPOSE:    This routine will take the font parm and set up the font
            related data in the passed in window description.  This
            is usually the main window.

PARAMETERS:

   1.  font_name   - pointer to char (INPUT)
               This is the name of the font to use.  It can be in any
               form acceptable to XLoadQueryFont.  This allows 
               there to be wild cards in the names.

   2.  x_window  -  Window (Xlib type) (INPUT)
                    This is the window id for the Main Ce window.

   3.  window   - pointer to DRAWABLE_DESCR (OUTPUT)
               This is a pointer to the drawable description 
               for the window to have the font data
               installed.  This is normally the main window.
               The font, fixed_font, and line_height fields are
               set in the structure.

FUNCTIONS :

   1.   Attempt to load the named font.  If that does not work use 
        font fixed which is supposed to be on all X servers.  If that
        fails, exit since we cannot do any good without text.

   2.   Determine if the font has fixed or variable width characters and
        set the fixed font flag in the window structure accordingly.

   3.   Calculate the height of a line in the window and put in the
        window structure.

RETURNED VALUE:
   current_font_name - pointer to malloc'ed storage area
                    This variable holds the name of the current
                    font name with no wild cards.

*************************************************************************/

char *get_font_data(char              *font_name,      /* input  */
                    DISPLAY_DESCR     *dspl_descr)     /* input  */

{
#define               MAX_NAMES_TO_RETURN 20
XFontStruct          *font_data = NULL;
int                   line_spacer;
char                 *all_exists_str;
char                 *p,*q;
char                  hostname[128];
int                   actual_font_count;
int                   i;
char                **font_list;
char                 *current_font_name;
char                  msg[512];

/***************************************************************
*
*   Get the font to use.  It may have been specified in the parm
*   string or a defaults file.  If not we pick one.
*
***************************************************************/

DEBUG1(fprintf(stderr, "Font requested: \"%s\"\n", font_name);)


/***************************************************************
*
*   Get the data about the font we will use and load the font too.
*   If the passed font name is null 
*
***************************************************************/
if (font_name != NULL)
   {
      DEBUG9(XERRORPOS)
      font_list = XListFonts(dspl_descr->display, font_name, (int)MAX_NAMES_TO_RETURN, &actual_font_count);
      DEBUG1(fprintf(stderr, "%d matching fonts found\n", actual_font_count);)
      for (i = 0; i < actual_font_count && font_data == NULL; i++)
         font_data = XLoadQueryFont(dspl_descr->display, font_list[i]);

      i--; /* back off to the entry found */
   }

if (font_data == NULL)
   {
      if (font_name != NULL)
         {
            snprintf(msg, sizeof(msg), "Cannot find a font to match \"%s\", using font \"fixed\"", font_name);
            dm_error(msg, DM_ERROR_LOG);
         }

      DEBUG9(XERRORPOS)
      font_data = XLoadQueryFont(dspl_descr->display, "fixed");

      if (font_data == NULL)
         {
            dm_error("Cannot access font \"fixed\", trying any font", DM_ERROR_LOG);
            DEBUG9(XERRORPOS)
            font_list = XListFonts(dspl_descr->display, "*", (int)MAX_NAMES_TO_RETURN, &actual_font_count);

            for (i = 0; i < actual_font_count && font_data == NULL; i++)
               font_data = XLoadQueryFont(dspl_descr->display, font_list[i]);

            i--; /* back off to the entry found */

            if (font_data == NULL)
               {
                  dm_error("Cannot access any font, aborting!", DM_ERROR_LOG);
                  exit(1);
               }
            else
               {
                  current_font_name = malloc_copy(font_list[i]);
                  XFreeFontNames(font_list);
               }
         }
      else
         current_font_name = malloc_copy("fixed");
   }
else
   {
      current_font_name = malloc_copy(font_list[i]);
      XFreeFontNames(font_list);
   }

DEBUG1(fprintf(stderr, "Font selected:  \"%s\"\n", current_font_name);)


dspl_descr->main_pad->window->font = font_data;

if ((font_data->min_bounds.width == font_data->max_bounds.width) && font_data->all_chars_exist)
   dspl_descr->main_pad->window->fixed_font = font_data->min_bounds.width;
else
   dspl_descr->main_pad->window->fixed_font = 0;

/***************************************************************
*  Get the inter line gap in pixels.  Variable
*  padding_between_line is modifiable by the
*  user.  It is the percent of the total character height to be
*  placed between lines.  Thus if padding_between_line is 10 (10%),
*  and the font is 80 pixels tall, the padding is 8 pixels.
***************************************************************/

line_spacer =  ((font_data->ascent + font_data->descent) * dspl_descr->padding_between_line) / 100;
dspl_descr->main_pad->window->line_height = font_data->ascent + font_data->descent + line_spacer;

DEBUG1(
   if (font_data->all_chars_exist)
      all_exists_str = ", all chars exist";
   else
      all_exists_str = "";

   if (font_data->min_bounds.width == font_data->max_bounds.width)
      fprintf(stderr, "Fixed width, %d pixels, height = %d pixels%s\n", font_data->min_bounds.width, dspl_descr->main_pad->window->line_height, all_exists_str);
   else
      fprintf(stderr, "Variable width %d - %d pixels, height = %d pixels%s\n", font_data->min_bounds.width, font_data->max_bounds.width, dspl_descr->main_pad->window->line_height, all_exists_str);
   fprintf(stderr, "MIN: ascent %d, descent %d, lbearing %d, rbearing %d\n", font_data->min_bounds.ascent, font_data->min_bounds.descent, font_data->min_bounds.lbearing, font_data->min_bounds.rbearing);
   fprintf(stderr, "MAX: ascent %d, descent %d, lbearing %d, rbearing %d\n", font_data->max_bounds.ascent, font_data->max_bounds.descent, font_data->max_bounds.lbearing, font_data->max_bounds.rbearing);

   fprintf(stderr, "min char = 0x%02x (%c), max char = 0x%02x (%c)\n", 
                   font_data->min_char_or_byte2, (font_data->min_char_or_byte2 ? font_data->min_char_or_byte2 : '.'),
                   font_data->max_char_or_byte2, font_data->max_char_or_byte2);
   fprintf(stderr, "min row  = 0x%02x,      max row  = 0x%02x\n", 
                   font_data->min_byte1,
                   font_data->max_byte1);

   fprintf(stderr, "all_chars_exist = %d, default char = 0x%02x (%c)\n",
                   font_data->all_chars_exist, font_data->default_char, (font_data->default_char ? font_data->default_char : '.'));

)

DEBUG31(
int   i;
if (font_data->per_char)
   for (i = 0; (unsigned)i < font_data->max_char_or_byte2 - font_data->min_char_or_byte2; i++)
      fprintf(stderr, "char  0x%02x (%c), width = %d, ascent = %d, descent = %d, lbearing = %d, rbearing = %d, attrs = 0x%x\n",
                       i + font_data->min_char_or_byte2, i + font_data->min_char_or_byte2,
                       font_data->per_char[i].width, font_data->per_char[i].ascent, font_data->per_char[i].descent,
                       font_data->per_char[i].lbearing, font_data->per_char[i].rbearing,
                       font_data->per_char[i].attributes);
)


/***************************************************************
*  
*  Make sure that variable CEHOST exists.  If not, set it to
*  the current host name.
*  
***************************************************************/

p = getenv("CEHOST");
if ((p == NULL) || (*p == '\0') || (*p == ':') || (strncmp(p, "unix:", 5) == 0) || (strncmp(p, "local:", 6) == 0))
   {
      q = DisplayString(dspl_descr->display);
      if (!q || (*q == '\0') || (*q == ':') || (strncmp(q, "unix:", 5) == 0) || (strncmp(q, "local:", 6) == 0))
         {
            p = strchr(q, ':');  /* find the part after the : RES 3/16/94, add display data to end of CEHOST */
            gethostname(hostname, sizeof(hostname));
            if (p)  /*RES 3/16/94, add display data to end of CEHOST */
               strlcat(hostname, p, sizeof(hostname));
            else
               strlcat(hostname, ":0.0", sizeof(hostname));
            q = hostname;
            DEBUG1(fprintf(stderr, "init: building CEHOST from the host name \"%s\"\n", hostname);)
         }
      else
         {
            DEBUG1(fprintf(stderr, "init: taking CEHOST from the X DisplayString name \"%s\"\n", q);)
         }

      snprintf(msg, sizeof(msg), "CEHOST=%s", q);
      p = malloc_copy(msg);
      if (p)
         putenv(p);

      p = getenv("DISPLAY");
      if (!p || *p == '\0')
         {
            snprintf(msg, sizeof(msg), "DISPLAY=%s", q);
            p = malloc_copy(msg);
            if (p)
               putenv(p);
         }
   }
else
   {
      DEBUG1(fprintf(stderr, "init: Keeping existing CEHOST from the environment \"%s\"\n", p);)
#ifndef NO_LICENSE
      /* RES 8/29/95 Don't leave display set to :0 for license server reasons */
      p = getenv("DISPLAY");
      if ((p == NULL) || (*p == '\0') || (*p == ':') || (strncmp(p, "unix:", 5) == 0) || (strncmp(p, "local:", 6) == 0))
         {
            /***************************************************************
            *  Save part past colon to paste back onto the host name if needed.
            ***************************************************************/
            if (p)
               q = strchr(p, ':');
            else
               q = NULL;
            gethostname(hostname, sizeof(hostname));
            if (q)
               strlcat(hostname, q, sizeof(hostname));
            else
               strlcat(hostname, ":0.0", sizeof(hostname));

            DEBUG1(fprintf(stderr, "init: Changing display from %s to %s\n", p, hostname);)
            snprintf(msg, sizeof(msg), "DISPLAY=%s", hostname);
            p = malloc_copy(msg);
            if (p)
               putenv(p);
         }
#endif
   }

return(current_font_name);

} /* end of get_font_data  */


/************************************************************************

NAME:      build_default_icon - Build the pixmap for a default Ce icon

PURPOSE:    This routine builds the pixmap for the various icon.h files.

PARAMETERS:
   1.  display       - Display (Xlib type) (INPUT)
                       This is the display token needed for Xlib calls.

   2.  main_x_window - Window (Xlib type) (INPUT)
                       This is the X window id of the main window.
                       The new icon name is associated with this window.

   3. icon_name      - pointer to char (INPUT)
                       This is the name of the icon.  The window
                       manager usually puts string below the icon
                       when the window is icon'ed.

   4. pad_mode       - int (INPUT)
                       This flag specifies whether we are in pad mode.  It
                       it true if we want the pad mode icon.

FUNCTIONS :

OUTPUTS:
   The window manager properties for the passed window are set by this routine.

*************************************************************************/

static Pixmap build_default_icon(Display  *display, Window main_x_window, int pad_mode)
{
unsigned int          icon_width;
unsigned int          icon_height;
unsigned char        *icon_bits;
char                 *vendor_info;
Pixmap                pixmap_for_icon = None;

vendor_info = ServerVendor(display);

if (vendor_info && (strstr(vendor_info, "Hummingbird Communications") != NULL))
   {
#ifdef PAD
      if (pad_mode)
         {
            icon_width   = shellicon_width_NT;
            icon_height  = shellicon_height_NT;
            icon_bits    = shellicon_bits_NT;
         }
      else
         {
            icon_width   = editicon_width_NT;
            icon_height  = editicon_height_NT;
            icon_bits    = editicon_bits_NT;
         }
#else
      icon_width   = editicon_width_NT;
      icon_height  = editicon_height_NT;
      icon_bits    = editicon_bits_NT;
#endif
   }
else
   {
#ifdef PAD
      if (pad_mode)
         {
            icon_width   = shellicon_width;
            icon_height  = shellicon_height;
            icon_bits    = shellicon_bits;
         }
      else
         {
            icon_width   = editicon_width;
            icon_height  = editicon_height;
            icon_bits    = editicon_bits;
         }
#else
      icon_width   = editicon_width;
      icon_height  = editicon_height;
      icon_bits    = editicon_bits;
#endif
   }

pixmap_for_icon = XCreateBitmapFromData (
      display,
      main_x_window,
      (char *)icon_bits,
      icon_width,
      icon_height
    );

return(pixmap_for_icon);

} /* end of build_default_icon */



/************************************************************************

NAME:      setup_window_manager_properties

PURPOSE:    This routine does the calls to set up the standard properties
            which need to be passed to the window manager.

PARAMETERS:

   1.  dspl_descr    - pointer to DISPLAY_DESCR (INPUT)
                       This is the current display description. It is the root pointer
                       for information about this set of windows.

   2.  window_name   - pointer to char (INPUT)
                       This is the name of the window.  The window
                       manager usually puts this in the titlebar of
                       the window.

   3. icon_name      - pointer to char (INPUT)
                       This is the name of the icon.  The window
                       manager usually puts string below the icon
                       when the window is icon'ed.

   4. cmdname        - pointer to char (INPUT)
                       This is the command name taken from argv[0].  It
                       is the last part of the path.

   5. start_iconic   - int (INPUT)
                       If true, we tell the window manager we want to start life as
                       an icon.



FUNCTIONS :

   1.   Get the screen size data into local variables and print them if we are in
        debug mode.

   2.   Build the icon pixmap from the include file editicon.h or shellicon.h.
        The include file was generated with the X utility /usr/X11/bin/bitmap.

   3.   Convert the window name and icon name into the X format required and set
        up the window manager hints structures.

   4.   Call XSetWMProperties to set the properties for this window into the
        X server so the window manager can get to them.

OUTPUTS:
   The window manager properties for the passed window are set by this routine.

*************************************************************************/

void setup_window_manager_properties(DISPLAY_DESCR     *dspl_descr,
                                     char              *window_name,
                                     char              *icon_name,
                                     char              *cmdname,
                                     int                start_iconic)
{

XTextProperty         windowName;
XTextProperty         iconName;
XSizeHints            size_hints;
Pixmap                pixmap_for_icon = None;
XWMHints              wm_hints;
XClassHint            class_hint;
char                 *p;
unsigned int          w, h;   /* junk variables */
int                   hx, hy; /* junk variables */
int                   rc;
char                  msg[1024];
char                  bitmap_path[MAXPATHLEN+1];

DEBUG1(
      fprintf(stderr, "Display name = \"%s\"\n", DisplayString(dspl_descr->display));
      fprintf(stderr, "Screen       = %d,   count = %d\n", DefaultScreen(dspl_descr->display), ScreenCount(dspl_descr->display));
)

/***************************************************************
*  
*  Make sure the home directory is set for this process.
*  
***************************************************************/

set_home();

/***************************************************************
*
*  Get the pixmap for the icon.
*  If the parm was set, use it.  If not or failure, use
*  the defaults.
*
***************************************************************/

if (ICON_BITMAP && *ICON_BITMAP)
   {
      strlcpy(bitmap_path, ICON_BITMAP, sizeof(bitmap_path));
      translate_tilde(bitmap_path);
      rc = XReadBitmapFile(
                dspl_descr->display,
                dspl_descr->main_pad->x_window,
                bitmap_path,
                &w, &h,
                &pixmap_for_icon,
                &hx, &hy);
      if (rc != BitmapSuccess)
         {
            switch(rc)
            {
            case BitmapOpenFailed:
               snprintf(msg, sizeof(msg), "Cannot open bitmap file %s (%s)\n", bitmap_path, strerror(errno));
               break;

            case BitmapFileInvalid:
               snprintf(msg, sizeof(msg), "Invalid bitmap file format in %s\n", bitmap_path);
               break;

            case BitmapNoMemory:
               snprintf(msg, sizeof(msg), "Insufficient memory to load bitmap file %s\n", bitmap_path);
               break;

            default:
               snprintf(msg, sizeof(msg), "Unknown return code from XReadBitmapFile, %d\n", rc);
               break;
            }
            dm_error(msg, DM_ERROR_LOG);
            pixmap_for_icon = None;
         }
   }

if (pixmap_for_icon == None)
   {
      pixmap_for_icon = build_default_icon(dspl_descr->display, dspl_descr->main_pad->x_window, dspl_descr->pad_mode);
   }


/***************************************************************
*
*  Put the window name and icon name into the correct format for X.
*
***************************************************************/

DEBUG9(XERRORPOS)
XStringListToTextProperty(
      &window_name,
      1,
      &windowName);

DEBUG9(XERRORPOS)
XStringListToTextProperty(
      &icon_name,
      1,
      &iconName);

/***************************************************************
*
*  Set up the hints data and give it to the window manager
*
***************************************************************/

  size_hints.flags = 0;
    size_hints.flags |= PPosition;
    size_hints.flags |= PSize;
  /*  size_hints.flags |= PMinSize;  main_subarea  in window.c takes care of this, RES 3/25/1998 */
    size_hints.width  = dspl_descr->main_pad->window->width;
    size_hints.height = dspl_descr->main_pad->window->height;
    size_hints.x      = dspl_descr->main_pad->window->x;
    size_hints.y      = dspl_descr->main_pad->window->y;

  wm_hints.flags = InputHint
                 | StateHint
                 | IconPixmapHint;

  wm_hints.input = True;
  if (start_iconic)
     wm_hints.initial_state = IconicState;
  else
     wm_hints.initial_state = NormalState;
  wm_hints.icon_pixmap = pixmap_for_icon;

  p = strrchr(cmdname, '/');
  if (p)
     class_hint.res_name  = p+1;
  else
     class_hint.res_name  = cmdname;
  class_hint.res_class = "Ce";


  DEBUG9(XERRORPOS)
  XSetWMProperties(
      dspl_descr->display,
      dspl_descr->main_pad->x_window,
      &windowName,
      &iconName,
      NULL,       /*lcl_argv,*/
      0,          /*lcl_argc,*/
      &size_hints,
      &wm_hints,
      &class_hint);

} /* end of setup_window_manager_properties */



/************************************************************************

NAME:      setup_winlines

PURPOSE:    This routine allocates or reallocates the win_lines structure
            used by the main routine when a font is loaded.

PARAMETERS:


   1.  main_window   - pointer to PAD_DESCR (INPUT)
                       This is a pointer to the pad description for the
                       main window.  This is where the address of the
                       winlines structure is stored.  It is also provides
                       access to the drawable descriptions for the main window.


FUNCTIONS :

   1.   Calculate the number of lines needed for the winlines structure.  If what
        we have is already big enough we need do nothing.

   2.   If there is an existing win_lines structure, free it.

   3.   Malloc the new winlines structure.  If we cannot get it, abend
        after causing a crash file to be created.


*************************************************************************/

void setup_winlines(PAD_DESCR   *main_pad)
{

int    new_winlines_size;
int    new_winlines_bytes;
int    i;

/***************************************************************
*  
*  Given the new font, see how many lines of this would fit on
*  the whole physical screen.  If this is less than the window
*  size, use the window size.  If we have enough room, use
*  what we have.
*  
***************************************************************/

new_winlines_size = (DisplayHeight(main_pad->display_data->display, DefaultScreen(main_pad->display_data->display)) / main_pad->window->line_height);
if (main_pad->window->lines_on_screen > new_winlines_size)
   new_winlines_size = main_pad->window->lines_on_screen;

DEBUG1(fprintf(stderr, "setup_winlines: old size: %d, new display: %d, new window: %d\n", main_pad->win_lines_size, new_winlines_size, main_pad->window->lines_on_screen);)

if (new_winlines_size <= main_pad->win_lines_size)
   return;

/***************************************************************
*  
*  We need to make the winlines array bigger.  If one already
*  exists, free it.
*  
***************************************************************/

if (main_pad->win_lines)
   {
      for (i = 0; i < main_pad->win_lines_size; i++)
         if (main_pad->win_lines[i].fancy_line)
            free_cdgc(main_pad->win_lines[i].fancy_line);
      free((char *)main_pad->win_lines);
   }

/***************************************************************
*  
*  Get the new winlines structure.  If the malloc fails, try
*  again.  The second malloc will fail also, but the error
*  recovery routines will save a crash file.  We do this, because
*  without win_lines, we cannot display the screen so we are
*  in dire straights and will do our best to save the data.
*  
***************************************************************/

main_pad->win_lines_size = new_winlines_size;

new_winlines_bytes = new_winlines_size * sizeof(WINDOW_LINE);
main_pad->win_lines = (WINDOW_LINE *)CE_MALLOC(new_winlines_bytes);

if (!main_pad->win_lines)
   catch_quit(99); /* use the interupt handler to shut down */

memset((char *)main_pad->win_lines, 0, new_winlines_bytes);


} /* end of setup_winlines */


/************************************************************************

NAME:      change_icon - change the icon name and/or pixmap

PURPOSE:    This routine changes the window manager properties needed
            to change the icon.

PARAMETERS:
   1.  display       - Display (Xlib type) (INPUT)
                       This is the display token needed for Xlib calls.

   2.  main_x_window - Window (Xlib type) (INPUT)
                       This is the X window id of the main window.
                       The new icon name is associated with this window.

   3. icon_name      - pointer to char (INPUT)
                       This is the name of the icon.  The window
                       manager usually puts string below the icon
                       when the window is icon'ed.

   4. pad_mode       - int (INPUT)
                       This flag specifies whether we are in pad mode.  It
                       it true if we want the pad mode icon.

FUNCTIONS :

OUTPUTS:
   The window manager properties for the passed window are set by this routine.

*************************************************************************/

void change_icon(Display           *display,
                 Window             main_x_window,
                 char              *icon_name,
                 int                pad_mode)
{
Pixmap                pixmap_for_icon;

/***************************************************************
*
*  get the pixmap for the icon.
*
***************************************************************/

pixmap_for_icon = build_default_icon(display, main_x_window, pad_mode);

DEBUG9(XERRORPOS)
XSetStandardProperties(display,
                       main_x_window,
                       NULL,
                       icon_name,
                       pixmap_for_icon,
                       NULL, /* argv */
                       0,    /* argc */
                       NULL);

} /* end of change_icon */


/************************************************************************

NAME:      build_cmd_args - Turn the option values into a argv/argc

PURPOSE:    This routine  builds the argc/argv which can be used to restart
            this session.  It is used in the  XA_WM_COMMAND property and
            also by the XSMP (X Session Manager Protocol).

PARAMETERS:

   1.  dspl_descr    - pointer to DISPLAY_DESCR (INPUT)
                       This is the current display description. It is the root pointer
                       for information about this set of windows.

   2. edit_file      - pointer to char (INPUT)
                       This is the name of the file being edited.

   3. arg0           - pointer to char (INPUT)
                       This is the name from argv[0]. It may contain a full path.

   4. argc           - pointer to int (OUTPUT)
                       The generated argc is returned in this value.

   5. argv           - pointer to pointer to pointer to char (OUTPUT)
                       This parm points to an argv which is loaded by this routine.

GLOBAL DATA:

   cmd_options       - array of XrmOptionDescRec (INPUT)
                       This is from parms.h, it is the option parsing array.
                       We get the option values from it

   fullpath          - array of char (OUTPUT)
                       The the full execution path is saved in this variable
                       which is global within the scope of this .c file.
                       This variable is used by routine get_execution_path.

FUNCTIONS :

   1.  Figure out what command name to use and put it in argv[0];

   2.  Copy the options from the parms data to the argv.

   3.  Save the edit file in the argv.

   4.  Put a NULL at the end of the list per convention.

OUTPUTS:
   The window manager properties for the passed window are set by this routine.

*************************************************************************/

static char           fullpath[MAXPATHLEN];

void build_cmd_args(DISPLAY_DESCR     *dspl_descr,
                    char              *edit_file,
                    char              *arg0,
                    int               *argc,
                    char             **argv)
{
int                   i;
int                   j;
int                   found;
char                 *cmdname;
char                 *p;
char                 *save_cmd;
static char           cmdpath[MAXPATHLEN];
static char           shell_path[MAXPATHLEN];
DISPLAY_DESCR        *walk_dspl;

#ifndef CYGNUS
#define CETERM_CMDNAME  "ceterm"
#define CE_CMDNAME      "ce"
#define CV_CMDNAME      "cv"
#else
#define CETERM_CMDNAME  "ceterm.exe"
#define CE_CMDNAME      "ce.exe"
#define CV_CMDNAME      "cv.exe"
#endif
#ifdef WIN32
char                  separator = ';';
#else
char                  separator = ':';
#endif

/***************************************************************
*  
*  Determine what the command name will be.  Normally use the
*  display description memdata token.  If it does not exist
*  use the passed command.  The does not exist occurs in set_file_from_parm.
*  
***************************************************************/

if (dspl_descr->pad_mode)
   save_cmd = CETERM_CMDNAME;
else
   if (dspl_descr->main_pad->token)
      if (WRITABLE(dspl_descr->main_pad->token))
         save_cmd = CE_CMDNAME;
      else
         save_cmd = CV_CMDNAME;
   else
      if (((arg0[0] == 'c') && (arg0[1] == 'v')) || (strcmp(arg0, "crpad") == 0))
         save_cmd = CV_CMDNAME;
      else
         save_cmd = CE_CMDNAME;

p = strrchr(arg0, '/');
if (p == NULL)
   cmdname = arg0;
else
   cmdname = p+1;

if (cmdname == arg0)
   strlcpy(cmdpath, save_cmd, sizeof(cmdpath));
else
   snprintf(cmdpath, sizeof(cmdpath), "%.*s%s", cmdname-arg0, arg0, save_cmd);

argv[0] = cmdpath;

/***************************************************************
*  
*  If we have a full path, make sure it is executable.  Otherwise
*  try a few things and then search the path if all else fails.
*  
***************************************************************/

if ((strchr(cmdpath, '/')) && (access(cmdpath, 01) != 0))
   {
      if ((strchr(arg0, '/')) && (access(arg0, 01) == 0)) /* arg0 is accessable, see if it is compatible */
         {
            i = (strncmp(cmdname, "ceterm", 6) == 0) ^ (strncmp(save_cmd, "ceterm", 6) == 0); /* both ceterm or both not ceterm */
            if (i)  /* if true, one is ceterm and the other not, search the path */
               strlcpy(cmdpath, save_cmd, sizeof(cmdpath));
            else
               strlcpy(cmdpath, arg0, sizeof(cmdpath)); /* existing arg0 is compatible */
         }
      else
         strlcpy(cmdpath, save_cmd, sizeof(cmdpath));
   }
 


/***************************************************************
*  
*  If we now have a partial name or just a command name, make it a full name.
*  
***************************************************************/

if (cmdpath[0] != '/')
   {
      if (strchr(cmdpath, '/') != NULL) /* this is a relative path */
         {
            if (cmdpath[0] == '~')
               translate_tilde(cmdpath);
            else
#ifdef PAD
               if (ce_getcwd(fullpath, sizeof(fullpath)) != NULL)  /* in pad.c */
#else
               if (getcwd(fullpath, sizeof(fullpath)) != NULL)
#endif
                  {
                     if (fullpath[strlen(fullpath)-1] != '/')  /* make sure we are not in the root directory */
                        strcat(fullpath, "/");
                     strlcat(fullpath, cmdpath, sizeof(fullpath));
                  }
            normalize_file_name(fullpath, (short)strlen(fullpath), cmdpath);
         }
      else
         {
            search_path_n(cmdpath, sizeof(fullpath), fullpath, "PATH", separator); /* in normalize.c */
            if (fullpath[0] != '\0')
               strlcpy(cmdpath, fullpath, sizeof(cmdpath));
         }
   }
else
   strlcpy(fullpath, cmdpath, sizeof(fullpath));

*argc = 1;

/***************************************************************
*  
*  Add in the command arguments
*  We filter out values which match the default except
*  in a few cases.  We also filter out parms which appear twice 
*  such as -background and -bgc which are really the same parm.
*  
***************************************************************/

for (i = 0; i < OPTION_COUNT; i++)
{
   DEBUG1(
      if ((i == DISPLAY_IDX) && (OPTION_VALUES[i] == NULL))
         fprintf(stderr, "build_cmd_args:  titlebar_host_name \"%s\" DisplayString \"%s\" display_host_name \"%s\"\n", dspl_descr->titlebar_host_name, XDisplayString(dspl_descr->display), dspl_descr->display_host_name);
   )

   found = False;
   for (j = 0; j < i && !found; j++)
      if (strcmp(cmd_options[i].specifier, cmd_options[j].specifier) == 0)
         found = True;  /* found a duplicate */

   if (i == SM_CLIENT_IDX) /* never look at the special XSMP (X Session Manager Protocol) restart client ID parm */
      continue;            /* If needed it is added in in xsmp.c  RES  09/14/2005, release 2.6.2 */

   if (i == WS_IDX)
      {
         p = workspace_parm(dspl_descr->display, dspl_descr->main_pad->x_window);
         if (p)
            {
               argv[(*argc)++] = cmd_options[i].option;
               argv[(*argc)++] = p;
            }
      }
   else
      if (i == DELAY_IDX)
         {
            for (walk_dspl = dspl_descr->next; walk_dspl != dspl_descr; walk_dspl = walk_dspl->next)
               if (walk_dspl->display_no < dspl_descr->display_no)
                  {
                     argv[(*argc)++] = cmd_options[i].option;
                     argv[(*argc)++] = "10";  /* delay 10 seconds */
                     break; /* leave this little loop */
                  }
         }
      else
         if ((OPTION_VALUES[i] != NULL) && !found && OPTION_FROM_PARM[i])
            {
               if (cmd_options[i].argKind == XrmoptionSepArg)
                  {
                     if (i != TABSTOP_IDX)
                        {
                           argv[(*argc)++] = cmd_options[i].option;
                           argv[(*argc)++] = OPTION_VALUES[i];
                        }
                  }
               else
                  if (cmd_options[i].argKind == XrmoptionNoArg)
                     {
                        if (OPTION_VALUES[i] && (strcmp(cmd_options[i].value, OPTION_VALUES[i]) == 0))
                           argv[(*argc)++] = cmd_options[i].option;
                     }
                  else
                     DEBUG(fprintf(stderr, "Don't know how to process arg #%d in parm list\n", i);)
            }
} /* for loop on i walking through the options */

#ifdef PAD
strlcpy(shell_path, edit_file, sizeof(shell_path));
p = strchr(shell_path, '(');
if (p != NULL)
   *p = '\0';
argv[(*argc)++] = shell_path;
#else
argv[(*argc)++] = edit_file;
#endif

argv[*argc] = NULL; /* standard convention */

DEBUG1(
   fputs("Saved command:  ", stderr);

   for (i = 0; i < *argc; i++)
      fprintf(stderr, "%s ", argv[i]);

   fputc('\n', stderr);
)

} /* end of build_cmd_args */


/************************************************************************

NAME:      get_execution_path - Get the execution path of this program

PURPOSE:    This routine returns the path to this program

PARAMETERS:

   1.  dspl_descr    - pointer to DISPLAY_DESCR (INPUT)
                       This is the current display description. It is the root pointer
                       for information about this set of windows.  It is passed

   2. arg0           - pointer to char (INPUT)
                       This is the name of the command, it is either ce, ceterm, or cv.

GLOBAL DATA:

   fullpath          - array of char (INPUT)
                       The the full execution path is saved in this variable
                       which is global within the scope of this .c file.
                       This variable is set by routine build_cmd_args

FUNCTIONS :

   1.  If fullpath was not already set, get it set.
       This should never happen.

   2.  Put the appropriate name (ce, cv ceterm) on the end of the path.

   3.  Make sure the file is executable.

   4.  If the first pass fails, try a second time to build the name.

RETURNED VALUE:
   lcl_fullpath  -  pointer to char
                    The path to execute this program is returned.
                    The returned path points to a static string.

*************************************************************************/

char *get_execution_path(DISPLAY_DESCR     *dspl_descr,
                         char              *arg0)
{
int                   argc;
char                 *argv[256];
static char           lcl_fullpath[MAXPATHLEN];
char                 *p;

/***************************************************************
*  If the passed name is cp (The ce create process command),
*  we really want ceterm.
***************************************************************/
if (strcmp(arg0, "cp") == 0)
   arg0 = CETERM_CMDNAME; /* cp's execute ceterm */

/***************************************************************
*  This should never occur. The static variable fullpath
*  should already be set, but if it wasn't, this is what we
*  would want to do.
***************************************************************/
if (fullpath[0] == '\0')
   {
      build_cmd_args(dspl_descr, "", arg0, &argc, argv);
      /*return(fullpath);  Removed 4/23/1998 */
   }

/***************************************************************
*  Get a copy of the path we were called with to hack up.
*  If there are no slashes in the name, use it as it is. 
*  Otherwise, strip off the clase qualifier and replace it with
*  argv[0].  If we are in Windows95, make sure the .exe is on 
*  the path.
***************************************************************/
strlcpy(lcl_fullpath, fullpath, sizeof(lcl_fullpath));
p = strrchr(lcl_fullpath, '/');
if (!p)
   return(lcl_fullpath);
else
   *(p+1) = '\0';
strlcat(lcl_fullpath, arg0, sizeof(lcl_fullpath));

#ifdef CYGNUS
p = strrchr(lcl_fullpath, '.');
if (!p || ((strcmp(p, ".exe") != 0) && (strcmp(p, ".EXE") != 0)))
   strlcat(lcl_fullpath, ".exe", sizeof(lcl_fullpath));
#endif

/***************************************************************
*  If the path we built is good, use it.
*  If not, have build_cmd_args build us one.
***************************************************************/
if (access(lcl_fullpath, 01) == 0)
   return(lcl_fullpath);

build_cmd_args(dspl_descr,"", arg0, &argc, argv);
return(fullpath);   

} /* end of get_execution_path */


/************************************************************************

NAME:      save_restart_state - Save the state so the window mangaer can restart us

PURPOSE:    This routine sets the XA_WM_COMMAND AND WM_CLIENT_MACHINE properties.

PARAMETERS:

   1.  dspl_descr    - pointer to DISPLAY_DESCR (INPUT)
                       This is the current display description. It is the root pointer
                       for information about this set of windows.  It is passed through
                       to build_cmd_args.

   2. edit_file      - pointer to char (INPUT)
                       This is the name of the file being edited.

   3. arg0           - pointer to char (INPUT)
                       This is the name from argv[0]. It may contain a full path.

GLOBAL DATA:

   save_yourself_atom  -  Atom (OUTPUT)
                          The value of the WM_SAVE_YOURSELF atom is stored globally so it
                          can be used in the main event loop to detect when a clientmessage
                          with this atom as data comes in.

FUNCTIONS :

   1.   Call build_cmd_string to set up the argc / argv from the existing options.

   2.   Set the XA_WM_COMMAND property.

   3.   Set the WM_CLIENT_MACHINE property.

   4.   Set the WM_PROTOCOLS property so we will be notifed when we need to save ourselves.

OUTPUTS:
   The window manager properties for the passed window are set by this routine.

*************************************************************************/

void save_restart_state(DISPLAY_DESCR     *dspl_descr,
                        char              *edit_file,
                        char              *arg0)
{
int                   argc;
char                 *argv[256];
char                  host_name[256];
char                  work_geometry[128];
char                 *host_name_p = host_name;
char                 *temp;
XTextProperty         HostName;
Atom                  atom_list[2];

/***************************************************************
*  
*  Get the argv and argc from the command options and save them
*  in the X server.
*  
***************************************************************/

temp = OPTION_VALUES[GEOMETRY_IDX];
snprintf(work_geometry, sizeof(work_geometry), "%dx%d%+d%+d",
                       dspl_descr->main_pad->window->width,
                       dspl_descr->main_pad->window->height,
                       dspl_descr->main_pad->window->x,
                       dspl_descr->main_pad->window->y);
OPTION_VALUES[GEOMETRY_IDX] = work_geometry;
build_cmd_args(dspl_descr, edit_file, arg0, &argc, argv);
OPTION_VALUES[GEOMETRY_IDX] = temp;

DEBUG9(XERRORPOS)
XSetCommand(dspl_descr->display, dspl_descr->main_pad->x_window, argv, argc);

gethostname(host_name, sizeof(host_name));

XStringListToTextProperty(&host_name_p, 1, &HostName);

DEBUG9(XERRORPOS)
XSetWMClientMachine(dspl_descr->display, dspl_descr->main_pad->x_window, &HostName);

/***************************************************************
*  
*  Set the window manager property so we get told we need to
*  save ourselves.
*  
***************************************************************/

DEBUG9(XERRORPOS)
dspl_descr->properties->save_yourself_atom = XInternAtom(dspl_descr->display, "WM_SAVE_YOURSELF", False);

DEBUG9(XERRORPOS)
dspl_descr->properties->delete_window_atom = XInternAtom(dspl_descr->display, "WM_DELETE_WINDOW", False);

DEBUG9(XERRORPOS)
dspl_descr->properties->wm_state_atom = XInternAtom(dspl_descr->display, "WM_STATE", False);

DEBUG9(XERRORPOS)
dspl_descr->properties->wm_protocols_atom  = XInternAtom(dspl_descr->display, "WM_PROTOCOLS", False);
DEBUG1(fprintf(stderr, "WM_SAVE_YOURSELF = %d, WM_PROTOCOLS = %d\n", dspl_descr->properties->save_yourself_atom, dspl_descr->properties->wm_protocols_atom);)

atom_list[0] = dspl_descr->properties->save_yourself_atom;
atom_list[1] = dspl_descr->properties->delete_window_atom;

DEBUG9(XERRORPOS)
XSetWMProtocols(dspl_descr->display, dspl_descr->main_pad->x_window, atom_list, 2);


} /* end of save_restart_state */


/************************************************************************

NAME:      set_home - Set the home variable in the current environment

PARAMETERS:

   none

FUNCTIONS :

   1.   If the HOME env variable is already set, do nothing.

   2.   Get the pw entry for the user.

   3.   put the directory from the pw entry into the environment.


*************************************************************************/

static void set_home(void)
{
#ifndef WIN32
struct passwd   *pw_rec;
#endif
char            *home;
char            *env_home;
int              home_len;

env_home = getenv("HOME");
if (env_home && (env_home[0] != '\0'))
   return;

#ifdef WIN32
home = malloc_copy("HOME=U:\\");
if (home)
  _putenv(home);
#else
pw_rec = getpwuid(getuid());
if (!pw_rec)
   return;

home_len = strlen(pw_rec->pw_dir)+6;
home = CE_MALLOC(home_len);
if (!home)
   return;  /* message already output */

snprintf(home, home_len, "HOME=%s", pw_rec->pw_dir);
DEBUG1(fprintf(stderr, "Need to set $HOME  -> %s\n", home);)

putenv(home);

endpwent();
#endif

} /* end of set_home */


/************************************************************************

NAME:      workspace_parm - Get the window manager workspace number and return it

PARAMETERS:
   1.  display       - pointer to Display (X Type) (INPUT)
                       This is the current open display.  It is used for X calls.

   2. main_window    - Window (X Type) (INPUT)
                       This is the main window for the ce/ceterm session.

FUNCTIONS :
   1.   Get the value of the _NET_WM_DESKTOP atom.

   2.   Return it in malloc'ed storate.

RETURNED VALUE:
   ws_string   -  malloc'ed string.
                  NULL - no workspace parm.

*************************************************************************/

static char  *workspace_parm(Display              *display,
                             Window                main_window)
{
char                    *p;
char                    *q;
int                      workspace_num;
XTextProperty            WorkspaceID;
char                     work[512];
Atom                     net_wm_desktop_Atom;

DEBUG9(XERRORPOS)
net_wm_desktop_Atom = XInternAtom(display, "_NET_WM_DESKTOP", True);
if (net_wm_desktop_Atom != None)
   {
      /*   WorkspaceID.value    = (unsigned char *)&workspace_num;
           WorkspaceID.encoding = XA_CARDINAL;
           WorkspaceID.format   = 32;
           WorkspaceID.nitems   = 1; */

      DEBUG9(XERRORPOS)
      if (main_window)
         {
            XGetTextProperty(display, main_window, &WorkspaceID, net_wm_desktop_Atom);

            if (WorkspaceID.nitems > 0)
               {
                  workspace_num = *((int *)WorkspaceID.value);
                  DEBUG1(
                        fprintf(stderr, "init.c:workspace_parm: _NET_WM_DESKTOP property is 0x%X for main window 0x%X\n",
                                        workspace_num, main_window);
                  )
                  snprintf(work, sizeof(work), "%d", workspace_num);
                  return(malloc_copy(work));
               }
         }
   }
else
   {
      DEBUG1(fprintf(stderr, "init.c:workspace_parm: Cannot get atom id for _NET_WM_DESKTOP, session manager may not put this window in correct workspace\n");)
   }
return(NULL);

} /* end of workspace_parm */




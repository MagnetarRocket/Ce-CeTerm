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
*  module cdgc.c  - color data graphics content
*
*  Routines:
*     cdgc_bgc           - Set up default background and foreground colors
*     get_gc             - Get the GC for a background/forground combination, create if necessary.
*     new_font_gc        - Process a font change in all known gc's
*     expand_cdgc        - Take GC data from the string data and create the linked list
*     dup_cdgc           - Create a copy of a linked list cdgc element.
*     flatten_cdgc       - Turn GC data in the linked list to the string format
*     GC_to_colors       - Return the colors for a GC.
*     free_cdgc          - Free a GC data linked list
*     extract_gc         - Find the correct GC for a given column
*     line_is_xed        - See if a line is x'ed out.
*     chk_color_overflow - See if a string cdgc flows to the next line
*     cdgc_split         - Split color data info into 2 pieces or insert chars
*     find_cdgc          - Scan the memdata database for a color/label data
*
* Internal:
*     cdgc_clean         - adjust boundaries of covered up color.
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <errno.h>          /* /usr/include/errno.h      */
#include <string.h>         /* /usr/include/string.h     */

#ifdef WIN32
#ifdef WIN32_XNT
#include "xnt.h"
#else
#include "X11/X.h"          /* /usr/include/X11/X.h      */
#include "X11/Xlib.h"       /* /usr/include/X11/Xlib.h   */
#include "X11/Xutil.h"      /* /usr/include/X11/Xutil.h  */
#endif
#else
#include <X11/X.h>          /* /usr/include/X11/X.h      */
#include <X11/Xlib.h>       /* /usr/include/X11/Xlib.h   */
#include <X11/Xutil.h>      /* /usr/include/X11/Xutil.h  */
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "debug.h"
#include "cdgc.h"
#include "dmwin.h"
#include "emalloc.h"
#include "xerror.h"
#include "xerrorpos.h"
#include "xutil.h"

#ifndef HAVE_STRLCPY
#include "strl.h"
#endif

/***************************************************************
*  
*  Static data hung off the display description.
*
*  The following structure is hung off the display description
*  it is needed to keep track of the graphic contents (GC's)
*  allocated.  It is a variable size structure.  This is to
*  allow it to be freed during cleanup.  It's size may change.
*  
***************************************************************/

#define DEFAULT_COLOR_ENTRIES 20
#define INCR_COLOR_ENTRIES     5
#define MAX_COLOR_NAME        24

typedef struct {
     char              background[MAX_COLOR_NAME];
     char              foreground[MAX_COLOR_NAME];
     GC                gc;
} COLOR_DATA;

typedef struct {
     unsigned long     default_bg_pixel;
     unsigned long     default_fg_pixel;
     int               max_color_entries;
     int               cur_color_entries;
     COLOR_DATA        color[DEFAULT_COLOR_ENTRIES];
} CDGC_PRIVATE;


/***************************************************************
*  
*  Local static data
*  
***************************************************************/

static char   *out_of_memory_bg_fg = DEFAULT_BGFG;

static int   cdgc_clean(FANCY_LINE      *cdgc_list);   /* input/output */

/************************************************************************

NAME:      cdgc_bgc   - Set up default background and foreground colors

PURPOSE:    This routine is called when the overall background or foreground
            colors are set up or changed (BGC, FGC, or initialization)
            It saves the pixel values in the private data area so they
            can be used for colorization.

PARAMETERS:
   1.  display         - pointer to Display (Xlib type) (INPUT)
                         This is the display token needed for Xlib calls.
                         It is needed if the GC has to be created.

   2.  bg_pix          - unsigned long (INPUT)
                         This is the current background color pixel.
                         (BAD_COLOR is passed if we don't change this value).

   3.  fg_pix          - unsigned long (INPUT)
                         This is the current foreground color pixel.
                         (BAD_COLOR is passed if we don't change this value).

   4.  gc_private_data - pointer to pointer to void (INPUT/OUTPUT)
                         This is the static data associated with this
                         display.  It is used exclusively by this module.
                         It is allocated by this module.  It is freed when
                         the display is freed.
FUNCTIONS :

   1.   Allocate the private data area if needed.

   2.   If needed, copy the background and/or foreground colors.

*************************************************************************/

void   cdgc_bgc(Display       *display,           /* input  */
                unsigned long  bg_pix,            /* input  */
                unsigned long  fg_pix,            /* input  */
                void           **gc_private_data) /* output */
{
CDGC_PRIVATE         *private = *gc_private_data;
int                   i;
XGCValues             gcvalues;

/***************************************************************
*  If there is no private area yet, build one.
***************************************************************/
if (!private)
   {
      *gc_private_data = CE_MALLOC(sizeof(CDGC_PRIVATE));
      if (!(*gc_private_data))
         return;
      memset((char *)*gc_private_data, 0, sizeof(CDGC_PRIVATE));
      private = (CDGC_PRIVATE *)*gc_private_data;
      private->max_color_entries = DEFAULT_COLOR_ENTRIES;
   }

if (bg_pix != BAD_COLOR)
   private->default_bg_pixel = bg_pix;

if (fg_pix != BAD_COLOR)
   private->default_fg_pixel = fg_pix;

/***************************************************************
*  See if any of the GC's need to be updated.
***************************************************************/
for (i = 0; i < private->cur_color_entries; i++)
{
   if ((strcmp(DEFAULT_BG, private->color[i].background) == 0) &&
       (bg_pix != BAD_COLOR))
      {
         gcvalues.background = private->default_bg_pixel;
         DEBUG9(XERRORPOS)
         XChangeGC(display,
                   private->color[i].gc,
                   GCBackground,
                   &gcvalues);

      }
   if ((strcmp(DEFAULT_FG, private->color[i].foreground) == 0) &&
       (fg_pix != BAD_COLOR))
      {
         gcvalues.foreground = private->default_fg_pixel;
         DEBUG9(XERRORPOS)
         XChangeGC(display,
                   private->color[i].gc,
                   GCForeground,
                   &gcvalues);

      }

   /* RES 7/9/2003, handle reverse video using default colors */
   if ((strcmp(DEFAULT_FG, private->color[i].background) == 0) &&
       (bg_pix != BAD_COLOR))
      {
         gcvalues.background = private->default_fg_pixel;
         DEBUG9(XERRORPOS)
         XChangeGC(display,
                   private->color[i].gc,
                   GCBackground,
                   &gcvalues);

      }
   if ((strcmp(DEFAULT_BG, private->color[i].foreground) == 0) &&
       (fg_pix != BAD_COLOR))
      {
         gcvalues.foreground = private->default_bg_pixel;
         DEBUG9(XERRORPOS)
         XChangeGC(display,
                   private->color[i].gc,
                   GCForeground,
                   &gcvalues);

      }
}

}  /* end of cdgc_bgc */


/************************************************************************

NAME:      get_gc   - Get the GC for a background/forground combination, create if necessary.

PURPOSE:    This routine will get a GC, given a background/foreground
            color pair and a font.  It will create the GC if necessary.
            GC's are kept track of in a private area associated with the
            display.

PARAMETERS:
   1.  display         - pointer to Display (Xlib type) (INPUT)
                         This is the display token needed for Xlib calls.
                         It is needed if the GC has to be created.
                         May be NULL, in which case no new colors can be
                         allocated.

   2.  main_x_window   - Window (Xlib type) (INPUT)
                         This is the X window id of the main window.
                         The new gc's are associated with this window.
                         It is needed if the GC has to be created.
                         May be None (Xlib define) in which case no new colors
                         can be allocated.

   3.  bg_fg           - pointer to char (INPUT)
                         This is the colors to find a GC for.  The string is in
                         the form backgroundcolor/foregroundcolor.  For example:
                         midnightblue/wheat

   4.  font           -  Font (Xlib type) (INPUT)
                         This is the font id to be associated with the GC.
                         It is always the current font.
                         It is needed if the GC has to be created.

   5.  gc_private_data - pointer to pointer to void (INPUT/OUTPUT)
                         This is the static data associated with this
                         display.  It is used exclusively by this module.
                         It is allocated by this module.  It is freed when
                         the display is freed.  If NULL is passed for the
                         display pointer, a pointer to a void pointer containing
                         NULL may be passed to this routine.


FUNCTIONS :

   1.   Break the colors into the background and foreground components.

   2.   Scan the existing list of GC data (if any) to see if there is a
        GC for this color combination.  If so, return it.

   3.   If the private area has to be allocated or expanded, do so.

   4.   Create a GC and and entry in the private area for it.

RETURNED VALUE:
   gc    -  GC (Xlib type)
            The graphics content with the desired attributes is returned.
            On error the value DEFAULT_GC is returned.

*************************************************************************/

GC     get_gc(Display         *display,           /* input  */
              Window           main_x_window,     /* input  */
              char            *bg_fg,             /* input  */
              Font             font,              /* input  */
              void           **gc_private_data)   /* input/output */
{
char                 *sep;
int                   len;
int                   i;
char                  background_color[MAX_COLOR_NAME+1];
char                  foreground_color[MAX_COLOR_NAME+1];
CDGC_PRIVATE         *private;
CDGC_PRIVATE         *temp_private;
unsigned long         gc_mask;
XGCValues             gcvalues;
char                  msg[256];


#ifdef EXCLUDEP
/***************************************************************
*  Check for the fixed values first
***************************************************************/
if (strcmp(bg_fg, XCLUDE_BGFB) == 0)
   return(XCLUDE_GC);
#endif

/***************************************************************
*  For labels, just use the default GC.  It is not really used.
***************************************************************/
if (IS_LABEL(bg_fg))
   return(DEFAULT_GC);

if ((strcmp(bg_fg, DEFAULT_BGFG) == 0) ||
            !gc_private_data ||
            ((*gc_private_data == NULL) && (display == NULL)))
   return(DEFAULT_GC);

private = (CDGC_PRIVATE *)*gc_private_data;

/***************************************************************
*  Find the background foreground separator position.
*  If we cannot find it, the value is invalid, so forget it.
***************************************************************/
sep = strchr(bg_fg, '/');
if (sep == NULL) 
   {
      snprintf(msg, sizeof(msg), "get_gc: Invalid color set string passed \"%s\"", bg_fg);
      dm_error(msg, DM_ERROR_LOG);
      return(DEFAULT_GC);
   }

/***************************************************************
*  Copy the background color to a local variable.
***************************************************************/
*sep = '\0';
strlcpy(background_color, bg_fg, sizeof(background_color));
*sep = '/';

/***************************************************************
*  Now do the foreground color the same way.
***************************************************************/
sep++;
strlcpy(foreground_color, sep, sizeof(foreground_color));

/***************************************************************
*  Trim both strings 
***************************************************************/
sep = &background_color[strlen(background_color)-1];
while ((sep > background_color) && ((*sep == ' ') || (*sep == '\t'))) *sep-- = '\0'; 
sep = &foreground_color[strlen(foreground_color)-1];
while ((sep > foreground_color) && ((*sep == ' ') || (*sep == '\t'))) *sep-- = '\0'; 

/***************************************************************
*  Make sure foreground and background are different.
***************************************************************/
if (strcmp(foreground_color, background_color) == 0)
   {
      snprintf(msg, sizeof(msg), "Background and Foreground colors are the same %s/%s", background_color, foreground_color);
      dm_error(msg, DM_ERROR_BEEP);
      return(DEFAULT_GC);
   }

/***************************************************************
*  Scan the list of existing GC's looking for a match.
***************************************************************/
if (private)
   {
      for (i = 0; i < private->cur_color_entries; i++)
         if ((strcmp(background_color, private->color[i].background) == 0) &&
             (strcmp(foreground_color, private->color[i].foreground) == 0))
            return(private->color[i].gc);
   }

/***************************************************************
*  We will need a new gc, return the default if a display
*  was not passed.
***************************************************************/
if ((display == NULL) || (main_x_window == None) || (font == None))
   return(DEFAULT_GC);

/***************************************************************
*  If there is no private area yet, build one.
***************************************************************/
if (!private)
   {
      *gc_private_data = CE_MALLOC(sizeof(CDGC_PRIVATE));
      if (!(*gc_private_data))
         return(DEFAULT_GC);
      memset((char *)*gc_private_data, 0, sizeof(CDGC_PRIVATE));
      private = (CDGC_PRIVATE *)*gc_private_data;
      private->max_color_entries = DEFAULT_COLOR_ENTRIES;
   }

/***************************************************************
*  If the private data overflows, make it bigger.
***************************************************************/
if (private->cur_color_entries == private->max_color_entries)
   {
      len = sizeof(CDGC_PRIVATE)+(((private->max_color_entries-DEFAULT_COLOR_ENTRIES))*sizeof(COLOR_DATA));
      DEBUG20(fprintf(stderr, "get_gc: Resizing CDGC_PRIVATE from %d entries to %d entries\n", private->max_color_entries, private->max_color_entries+INCR_COLOR_ENTRIES);)
      temp_private = (CDGC_PRIVATE *)CE_MALLOC(len+(INCR_COLOR_ENTRIES*sizeof(COLOR_DATA)));
      if (!temp_private)
         return(DEFAULT_GC);
      memcpy((char *)temp_private, (char *)private, len);
      temp_private->max_color_entries += INCR_COLOR_ENTRIES;
      free((char *)private);
      private = temp_private;
      *gc_private_data = (void *)private;
   }

/****************************************************************
*   Build the new gc
****************************************************************/
gc_mask = GCForeground
        | GCBackground
        | GCFont;


gcvalues.font       = font;
if (strcmp(background_color, DEFAULT_BG) == 0)
   gcvalues.background = private->default_bg_pixel;
else
   if (strcmp(background_color, DEFAULT_FG) == 0)
      gcvalues.background = private->default_fg_pixel;
   else
      gcvalues.background = colorname_to_pixel(display, background_color);

if (gcvalues.background == BAD_COLOR)
   {
      snprintf(msg, sizeof(msg), "Cannot allocate color %s", background_color);
      dm_error(msg, DM_ERROR_BEEP);
      return(DEFAULT_GC);
   }

if (strcmp(foreground_color, DEFAULT_FG) == 0)
   gcvalues.foreground = private->default_fg_pixel;
else
   if (strcmp(foreground_color, DEFAULT_BG) == 0)
      gcvalues.foreground = private->default_bg_pixel;
   else
      gcvalues.foreground = colorname_to_pixel(display, foreground_color);

if (gcvalues.foreground == BAD_COLOR)
   {
      snprintf(msg, sizeof(msg), "Cannot allocate color %s", foreground_color);
      dm_error(msg, DM_ERROR_BEEP);
      return(DEFAULT_GC);
   }

DEBUG9(XERRORPOS)
private->color[private->cur_color_entries].gc =
       XCreateGC(display,
                 main_x_window,
                 gc_mask,
                 &gcvalues
                );

if (private->color[private->cur_color_entries].gc == None)
   {
      snprintf(msg, sizeof(msg), "Cannot create GC for %s/%s", background_color, foreground_color);
      dm_error(msg, DM_ERROR_LOG);
      return(DEFAULT_GC);
   }

strcpy(private->color[private->cur_color_entries].background, background_color);
strcpy(private->color[private->cur_color_entries].foreground, foreground_color);

private->cur_color_entries++;

return(private->color[private->cur_color_entries-1].gc);

}  /* end of get_gc */


/************************************************************************

NAME:      new_font_gc        - Process a font change in all known gc's

PURPOSE:    This routine does a change GC on each gc when a new font is loaded.

PARAMETERS:
   1.  display         - Display (Xlib type) (INPUT)
                         This is the display token needed for Xlib calls.
                         It is needed if the GC has to be created.

   2.  font           -  Font (Xlib type) (INPUT)
                         This is the font id to be associated with the GC.
                         It is always the current font.
                         It is needed if the GC has to be created.

   3.  gc_private_data - pointer to pointer to void (INPUT/OUTPUT)
                         This is the static data associated with this
                         display.  It is used exclusively by this module.
                         It is allocated by this module.  It is freed when
                         the display is freed.

FUNCTIONS :
   1.   Build a gc_values list and a mask to change the font.

   2.   Walk the array of GC's and change the font in each one.

*************************************************************************/

void   new_font_gc(Display         *display,           /* input  */
                   Font             font,              /* input  */
                   void           **gc_private_data)   /* input/output */
{

XGCValues            gcvalues;
CDGC_PRIVATE         *private = (CDGC_PRIVATE *)*gc_private_data;
int                   i;


/***************************************************************
*  Walk the array of existing GC's changing each one.
***************************************************************/
if (private)
   {
      gcvalues.font = font;

      for (i = 0; i < private->cur_color_entries; i++)
      {
         DEBUG9(XERRORPOS)
         XChangeGC(display,
                   private->color[i].gc,
                   GCFont,
                   &gcvalues);

      }
   }

} /* end of new_font_gc */


/************************************************************************

NAME:      expand_cdgc        - Take GC data from the string data and create the linked list

PURPOSE:    This routine takes the string form a Color Data Graphics Content as stored
            in the memdata database for a line and expands it into a linked list
            of drawing data.  It takes the parameters to get_gc because in the case of
            a carbon copy window, it may have to create GC's on the fly.

PARAMETERS:
   1.  display         - Display (Xlib type) (INPUT)
                         This is the display token needed for Xlib calls.
                         It is needed if the GC has to be created.

   2.  main_x_window   - Window (Xlib type) (INPUT)
                         This is the X window id of the main window.
                         The new gc's are associated with this window.
                         It is needed if the GC has to be created.

   3.  cdgc            - pointer to char (INPUT)
                         This is the cdgc string to parse.  It is of the form
                         start_col,end_col,bgc/fgc;..
                         For example:
                         0,5,midnightblue/wheat;5,80,yellow/grey5

   4.  font           -  Font (Xlib type) (INPUT)
                         This is the font id to be associated with the GC.
                         It is always the current font.
                         It is needed if the GC has to be created.

   5.  gc_private_data - pointer to pointer to void (INPUT/OUTPUT)
                         This is the static data associated with this
                         display.  It is used exclusively by this module.
                         It is allocated by this module.  It is freed when
                         the display is freed.  If accurate GC values
                         are not required, (adding and deleting characters)
                         NULL or pointer to NULL can be passed.

FUNCTIONS :
   1.   Walk the cdgc string and create a Fancy Line structure for each one.

RETURNED VALUE:
    head   -   FANCY_LINE (RETURNED VALUE)
               The list is returned.  On error NULL is returned.
    
*************************************************************************/

FANCY_LINE *expand_cdgc(Display         *display,           /* input  -> if we have to make any GC's */
                        Window           main_x_window,     /* input  -> if we have to make any GC's */
                        char            *cdgc,              /* input  */
                        Font             font,              /* input  -> if we have to make any GC's */
                        void           **gc_private_data)   /* input/output */
{
FANCY_LINE           *head = NULL; 
FANCY_LINE           *tail = NULL;
FANCY_LINE           *curr;
char                 *work_buff;
char                 *p;
int                   start;
int                   end;
char                  bg_fg[MAX_COLOR_NAME*2];
char                  msg[256];
GC                    new_gc;

work_buff = malloc_copy(cdgc);
if (!work_buff)
   return(NULL);

/***************************************************************
*  Grab each color segment for parsing, they are separated by semicolons.
***************************************************************/
for (p = strtok(work_buff, ";");
     p;
     p = strtok(NULL, ";"))
{
   /***************************************************************
   *  Parse each segment of the string.
   ***************************************************************/
   if (sscanf(p, "%d,%d,%s", &start, &end, bg_fg) != 3)   
      {
         snprintf(msg, sizeof(msg), "Invalid color data item ignored, \"%s\"", p);
         dm_error(msg, DM_ERROR_LOG);
         free_cdgc(head);
         head = NULL;
         break;
      }

   /***************************************************************
   *  Special case check:
   *  If the first and only element on a line is 0,COLOR_OVERFLOW_COL,DEFAULT
   *  Return NULL to return to normal drawing.
   ***************************************************************/
   if ((p == work_buff) && (start == 0) && (end == COLOR_OVERFLOW_COL) && (strcmp(bg_fg, DEFAULT_BGFG) == 0))
      return(NULL);

   /***************************************************************
   *  Ignore zero sized regions.
   ***************************************************************/
   if ((start == end) && !IS_LABEL(bg_fg))
      continue;

   /***************************************************************
   *  Convert the color pair or special value to a GC
   ***************************************************************/
   new_gc = get_gc(display, main_x_window, bg_fg, font, gc_private_data);

   /***************************************************************
   *  Create a new fancy line element and add it to the list.
   ***************************************************************/
   curr = (FANCY_LINE *)CE_MALLOC(sizeof(FANCY_LINE));
   if (!curr)
      {
         free_cdgc(head);
         head = NULL;
         break;
      }

   curr->next      = NULL;
   curr->first_col = start;
   curr->end_col   = end;
   curr->gc        = new_gc;
   curr->bgfg      = malloc_copy(bg_fg);
   if (!curr->bgfg)
      curr->bgfg = out_of_memory_bg_fg;
  
   if (head == NULL)
      head = curr;
   else
      tail->next = curr;

   tail = curr;
} /* end of for loop */

cdgc_clean(head);

free(work_buff);
return(head);

} /* end of expand_cdgc */


/************************************************************************

NAME:      dup_cdgc           - Create a copy of a linked list cdgc element.

PURPOSE:    This routine duplicates a single cdgc linked list element (FANCY_LINE).

PARAMETERS:
   1.  orig            - pointer to FANCY_LINE (INPUT)
                         This is the element to be duplicated.
                         Only 1 element is duplicated and its next pointer
                         is set to NULL;

FUNCTIONS :
   1.   Allocate a new element.

   2.   Duplicate the color field.

   3.   Copy the data from the orig to the new, except for the
        next pointer which is set to NULL.

RETURNED VALUE:
    new    -   FANCY_LINE (RETURNED VALUE)
               The list is returned.  On error NULL is returned.
    
*************************************************************************/

FANCY_LINE *dup_cdgc(FANCY_LINE         *orig)
{
FANCY_LINE           *new;

new = CE_MALLOC(sizeof(FANCY_LINE));
if (!new)
   return(NULL);
memset((char *)new, 0, sizeof(FANCY_LINE));
new->first_col = orig->first_col;
new->end_col   = orig->end_col;
new->gc        = orig->gc;
new->bgfg      = malloc_copy(orig->bgfg);
if (!new->bgfg)
   new->bgfg = out_of_memory_bg_fg;

return(new);

} /* end of dup_cdgc */


/************************************************************************

NAME:      flatten_cdgc       - Turn GC data in the linked list to the string format

PURPOSE:    This routine takes the linked list form a Color Data Graphics Content
            and stores it as a string suitable for storage in the memdata database.

PARAMETERS:
   1.  cdgc            - pointer to FANCY_LINE (INPUT)
                         This is the cdgc linked list to parse.

   2.  target_area    -  pointer to char (OUTPUT)
                         The address of the target buffer to be written
                         to is passed.  No checking is done.  The
                         target area is expected to be 

   3.  max_target     -  int (INPUT)
                         This is the maximum length of the target area.
                         This is to avoid overflow.  If overflow would occur,
                         the data is truncated before the item which would
                         overflow.  That is, only complete entries are put
                         in the target area.

   4.  just_1         -  int (INPUT)
                         This flag is set to true if we want to just expand the
                         cdgc element passed and not follow the linked list.

FUNCTIONS :
   1.   Walk the cdgc linked list and put the data into the target string.

    
*************************************************************************/

void  flatten_cdgc(FANCY_LINE      *cdgc,              /* input */
                   char            *target_area,       /* output*/
                   int              max_target,        /* input */
                   int              just_1)            /* input */
{

FANCY_LINE           *curr;
char                 *target_pos = target_area;
char                 *target_end = target_area + (max_target - 1); /* -1 for null terminator */

*target_area = '\0';
if (!just_1)
   cdgc_clean(cdgc);


for (curr = cdgc;
     curr;
     curr = curr->next)
{
   /***************************************************************
   *  Make sure the new data will fit.
   *  The length of the colors + 2 commas + 1 semicolon + 4 for
   *  the start column + 4 for the end column.
   ***************************************************************/
   if (curr->bgfg == NULL) /* Should never happen, but we will flag it */
      {
         dm_error("Null bgfg found in flatten_cdgc! default used", DM_ERROR_LOG);
         curr->bgfg = out_of_memory_bg_fg;
      }

   /***************************************************************
   *  Ignore zero sized regions.
   ***************************************************************/
   if ((curr->first_col >= curr->end_col) && (!just_1) && !IS_LABEL(curr->bgfg)) 
      continue;

   if (target_pos + (strlen(curr->bgfg) + 11) < target_end)
      {
         snprintf(target_pos, (max_target-(target_pos-target_area)), "%d,%d,%s;", curr->first_col, curr->end_col, curr->bgfg);
         target_pos += strlen(target_pos);
      }
   else
      {
         DEBUG20(fprintf(stderr, "flatten_cdgc: string cdgc truncated at length %d, \"%s\"\n", strlen(target_area), target_area);)
         break;
      }

   if (just_1)
      break;
} /* end of for loop walking the FANCY_LINE list **/
 
} /* end of flatten_cdgc */


/************************************************************************

NAME:      GC_to_colors       - Return the colors for a GC.

PURPOSE:    This routine takes a GC and returns the colors for it.
            It first searches the table and then askes the X server
            if it is not in the server.

PARAMETERS:
   1.  gc              - GC (Xlib type) (INPUT)
                         This is the GC to be looked up.

   2.  private         - pointer to void (INPUT/OUTPUT)
                         This is the static data associated with this
                         display.  It is used exclusively by this module.
                         It is allocated by this module.  It is freed when
                         the display is freed.

FUNCTIONS :
   1.   Scan the private GC area and find the colors.

RETURNED VALUE:
   colors     -  pointer to char
                 The string with the background_color/foreground_color
                 is returned.  If the default GC is passed in or the
                 colors cannot be found, the default GC string is returned.
                 The returned data is in a static string and should be
                 copied somewhere if it is to be kept.
    
*************************************************************************/

char *GC_to_colors(GC               gc,                /* input */
                   void            *gc_private)        /* input */
{
CDGC_PRIVATE         *private = (CDGC_PRIVATE *)gc_private;
int                   i;
static char           bg_fg[MAX_COLOR_NAME*2]; /* returned string */
static GC             last_gc;
int                   found = False;

if (gc == DEFAULT_GC)
   return(DEFAULT_BGFG);

#ifdef EXCLUDEP 
if (gc == XCLUDE_GC)
   return(XCLUDE_BGFB);
#endif

if (gc == last_gc)
   return(bg_fg);

/***************************************************************
*  First search the list if it is available.
*  The gc's we are processing should have been created by get_gc;
***************************************************************/
if (private)
   {
      for (i = 0; (i < private->cur_color_entries) && !found; i++)
         if (private->color[i].gc == gc)
            {
               found = True;
               last_gc = gc;
               snprintf(bg_fg, sizeof(bg_fg), "%s/%s", private->color[i].background, private->color[i].foreground);
            }
      DEBUG20(
         if (!found)
            fprintf(stderr, "GC passed to GC_to_colors (0x%X) not in private list.\n", gc);
      )
   }
else
   {
      DEBUG20(fprintf(stderr, "GC_to_colors Can't get colors for GC (0x%X), no private area, returning default.\n", gc);)
      strlcpy(bg_fg, DEFAULT_BGFG, sizeof(bg_fg));
   }

return(bg_fg);
 
} /* end of GC_to_colors */


/************************************************************************

NAME:      free_cdgc          - Free a GC data linked list

PURPOSE:    This routine takes the storage associated with a linked list of
            FANCY_LINES.

PARAMETERS:
   1.  cdgc            - pointer to FANCY_LINE (INPUT)
                         This is the cdgc linked list to free.

FUNCTIONS :
   1.   Walk the cdgc linked list and free the elements.

    
*************************************************************************/

void  free_cdgc(FANCY_LINE       *cdgc)
{

FANCY_LINE           *current;
FANCY_LINE           *next;


for (current = cdgc; current != NULL; current = next)
{
   next = current->next;
   if ((current->bgfg) && (current->bgfg != out_of_memory_bg_fg))
      free(current->bgfg);
   free(current);
}

} /* end of free_cdgc */


/************************************************************************

NAME:      extract_gc         - Find the correct GC for a given column

PURPOSE:    This routine finds the GC to use for the specified column.
            It also returns the next column which will need processing.

PARAMETERS:
   1.  cdgc            - pointer to FANCY_LINE (INPUT)
                         This is the cdgc linked list to evaluated.

   2.  cdgc_line       - int (INPUT)
                         This is the line number for the line which goes
                         with the passed cdgc.  It is the line returned
                         int the color_line_no parm of get_color_by_num(memdata.c).
                         This number must be less than or equal to line_no.

   3.  line_no         - int (INPUT)
                         This is the line number for the line we are extracting
                         a gc for.  This may the the cdgc_line value or some
                         line cdgc_line overflows into.

   4.  col             - int (INPUT)
                         This is the zero based column number to look for.

   5.  end_col         - pointer to pointer to int (OUTPUT)
                         This is the next column which will need to be looked
                         at.

FUNCTIONS :
   1.   Walk the cdgc linked list and look for the first entry which covers
        the passed column.  Return this GC.

   2.   Walk the cdgc linked list and look for the smallest start area greater
        than the passed column.  If none is found, use the end column from
        the GC returned.  Do not look past the place where the GC comes from.

RETURNED VALUE:
   found_gc    -  GC
                  The GC to use in drawing this portion of the string is returned.

    
*************************************************************************/

GC    extract_gc(FANCY_LINE      *cdgc,              /* input */
                 int              cdgc_line,         /* input */
                 int              line_no,           /* input */
                 int              col,               /* input */
                 int             *end_col)           /* output */
{
FANCY_LINE           *curr;
GC                    found_gc = DEFAULT_GC;
char                  msg[256];

*end_col = COLOR_OVERFLOW_COL;

if (cdgc_line == line_no)
   {
     /***************************************************************
     *  The color data is for this line.
     *  Find where this column fits in.
     ***************************************************************/
      for (curr = cdgc; curr; curr = curr->next)
      {
         if (curr->first_col == curr->end_col)
            continue; /* ignore zero sized regions */
      
         if ((curr->first_col > col) && (curr->first_col < *end_col))
            *end_col = curr->first_col;
      
         if ((curr->first_col <= col) &&  (curr->end_col > col))
            {
               found_gc = curr->gc;
               if (curr->end_col < *end_col)
                  *end_col = curr->end_col;
               break;
            }
      }
   }
else
   if (cdgc_line < line_no)
      {
        /***************************************************************
        *  The color data is for a previous line.  Find the first element
        *  which shows overflow to next line and use it.
        ***************************************************************/
         for (curr = cdgc; curr; curr = curr->next)
         {
            if (curr->first_col == curr->end_col)
               continue; /* ignore zero sized regions */
         
            if (curr->end_col == COLOR_OVERFLOW_COL)
               {
                  found_gc = curr->gc;
                  break;
               }
         }
      }
   else
      {
          snprintf(msg, sizeof(msg), "(extract_gc) internal error, color line %d greater than text line %d",
                       cdgc_line, line_no);
          dm_error(msg, line_no);
      }

return(found_gc);
 
} /* end of extract_gc */


#ifdef EXCLUDEP
/************************************************************************

NAME:      line_is_xed         - See if a line is totally x'ed out

PURPOSE:    This routine checks a color data string to see if the
            line is totally x'ed out.

PARAMETERS:
   1.  cdgc            - pointer to char (INPUT)
                         This is the cdgc line as stored in memdata which
                         is to be evaluated.

FUNCTIONS :
   1.   The exclude GC is always the first on the list.  If has a
        range of 0 to COLOR_OVERFLOW_COL.  If it is not there, the line is not x'ed out.

RETURNED VALUE:
   xed          - int
                  True  - The line is totally x'ed out
                  False - The line is not totally x'ed out

    
*************************************************************************/

int   line_is_xed(char            *cdgc,              /* input */
                  int              cdgc_line,         /* input */
                  int              line_no)           /* input */
{
int                   start;
int                   end;
GC                    gc;
char                  buff[MAX_LINE+1];

if (!cdgc)
   return(False);

if (sscanf(cdgc, "%d,%d,%s", &start, &end, buff) != 3)   
   return(False);
else
   if (strncmp(buff, XCLUDE_BGFB, strlen(XCLUDE_BGFB)) != 0)
      return(False);
   else
      if ((cdgc_line == line_no) && (end != 0)) /* cdgc is for this line and not zero size marker */
         return(True);
      else
         if (end == COLOR_OVERFLOW_COL) /* cdgc is for a different line and marker shows overflow */
            return(True);
         else
            return(False);
   

} /* end of line_is_xed */
#endif


/************************************************************************

NAME:       chk_color_overflow - See if a string cdgc flows to the next line

PURPOSE:    This routine checks a color data line to see if there is any
            color elements which overflow to later lines.

PARAMETERS:
   1.  cdgc            - pointer to char (INPUT)
                         This is the cdgc line as stored in memdata which
                         is to be evaluated.

FUNCTIONS :
   1.   If there is no cdgc or it cannot be converted to a linked list,
        return False.

   2.   Convert the color data to linked list form.

   3.   Scan the linked list searching for color which has an end column
        of COLOR_OVERFLOW_COL.  If it is found, we will return True.

RETURNED VALUE:
   overflow     - int
                  True  - The color data flows to the next line.
                  False - The color data does not flow to the next line

    
*************************************************************************/

int   chk_color_overflow(char      *cdgc)              /* input */
{
FANCY_LINE           *cdgc_list;
FANCY_LINE           *curr;
int                   overflow = False;
void                 *junk = NULL;


if (!cdgc || !*cdgc)
   return(overflow);

cdgc_list = expand_cdgc(NULL, /* no Display  */
                        None, /* no x_window */
                        cdgc,
                        None,   /* no font     */
                        &junk); /* no color data private area */

if (!cdgc_list)
   return(overflow);

for (curr = cdgc_list;
     curr;
     curr = curr->next)
{
   if (curr->end_col == COLOR_OVERFLOW_COL)
      {
         overflow = True;
         break;
      }
} /* end of for loop walking the FANCY_LINE list **/
   
free_cdgc(cdgc_list);
return(overflow);

} /* end of chk_color_overflow */


/************************************************************************

NAME:       cdgc_split - Split color data info into 2 pieces or insert chars

PURPOSE:    This routine optionally splits color data at the specified point
            and inserts the specified number of characters at the beginning
            of the second half or just inserts characters at the required point.

PARAMETERS:
   1.  color_line      - pointer to char (INPUT)
                         This is the color data as stored in memdata which
                         is to be evaluated.

   2.  split_col       - int (INPUT)
                         This is the point the the line is to be split and
                         the characters to be inserted.

   3.  insert_chars    - int (INPUT)
                         This is the number of characters to insert.

   4.  text_len        - int (INPUT)
                         The length of the text line being split

   5.  insert_type     - int (INPUT)
                         This flag specifies whether to split the line or
                         just insert characters.
                         Values:  CDGC_SPLIT_NOBREAK
                                  CDGC_SPLIT_BREAK

   6.  front_target    - pointer to char (OUTPUT)
                         If non NULL, the first half of the color data
                         is placed here.  When insert type is CDGC_SPLIT_NOBREAK,
                         all the data is placed here.

   7.  back_target     - pointer to char (OUTPUT)
                         If non NULL, the second half of the color data
                         is placed here.  When insert type is CDGC_SPLIT_NOBREAK,
                         This string is set to zero length.

FUNCTIONS :
   1.   Expand the passed color data to linked list form

   2.   Determine whether each cdgc element is in the first half, second half
        or split across the break point.

   3.   Flatten the list(s) into the returne parameters.

    
*************************************************************************/

void cdgc_split(char   *color_line,
                int     split_col,
                int     insert_chars,
                int     text_len,
                int     insert_type,
                char   *front_target,  /* should be MAX_LINE+1 long */
                char   *back_target)   /* should be MAX_LINE+1 long */
{
FANCY_LINE           *curr;
FANCY_LINE           *new;
FANCY_LINE           *first_line = NULL;
FANCY_LINE           *first_line_end = NULL;
FANCY_LINE           *sec_line = NULL;
FANCY_LINE           *sec_line_end = NULL;
FANCY_LINE           *next;
FANCY_LINE           *cdgc_list;


if (front_target)
   *front_target = '\0';
if (back_target)
   *back_target = '\0';

if ((color_line == NULL) || (*color_line == '\0'))
   return;

cdgc_list = expand_cdgc(NULL, None, color_line, None, NULL);
if (!cdgc_list)
   return;

for (curr = cdgc_list;
     curr;
     curr = next)
{
   next = curr->next;
   /* RES 2/24/1999 ignore zero size regions */
   /* RES 3/01/2005, Don't ignore labels */
   if ((curr->first_col >= curr->end_col) && !(IS_LABEL(curr->bgfg)))
      continue; 
               
   curr->next = NULL;

   /***************************************************************
   *  If the end column is before the split column, this entry
   *  goes on the first list.
   ***************************************************************/
   if (curr->end_col < split_col)
      {
         if (first_line == NULL)
            first_line = curr;
         else
            first_line_end->next = curr;
         first_line_end = curr;
      }
   /***************************************************************
   *  If the start column is after the split, this entry
   *  goes on the second list.  Subtract off the split col.
   *  Add in the insert characters
   ***************************************************************/
   else
      if (curr->first_col >= split_col)
         {
            if (insert_type == CDGC_SPLIT_BREAK)
               {
                  if (sec_line == NULL)
                     sec_line = curr;
                  else
                     sec_line_end->next = curr;
                  sec_line_end = curr;
                  curr->first_col -= split_col;
                  if ((curr->end_col != COLOR_OVERFLOW_COL) && (curr->end_col != MAX_LINE))
                     curr->end_col   -= split_col;
               }
            else
               {
                  if (first_line == NULL)
                     first_line = curr;
                  else
                     first_line_end->next = curr;
                  first_line_end = curr;
               }

            curr->first_col += insert_chars;
            if ((curr->end_col != COLOR_OVERFLOW_COL) && (curr->end_col != MAX_LINE))
               curr->end_col   += insert_chars;
         }
      else
         if (insert_type == CDGC_SPLIT_BREAK)
            {
               /***************************************************************
               *  Else the color area straddles the split.  On the first
               *  line create a overflow area, on the second, start at zero
               *  up to the end.
               ***************************************************************/
               new = dup_cdgc(curr);
               if (!new)
                  return;
               new->end_col   = COLOR_OVERFLOW_COL;
               if (first_line == NULL)
                  first_line = new;
               else
                  first_line_end->next = new;
               first_line_end = new;

               curr->first_col = 0;
               if ((curr->end_col != COLOR_OVERFLOW_COL) && (curr->end_col != MAX_LINE))
                  curr->end_col -= split_col;

               /*******************************************
               *  On zero size color on next line, do not
               *  overflow color onto the line.
               *******************************************/
               if ((curr->end_col  == curr->first_col) || (split_col >= text_len))
                  new->end_col   = MAX_LINE;

               if (sec_line == NULL)
                  sec_line = curr;
               else
                  sec_line_end->next = curr;
               sec_line_end = curr;
               if ((curr->end_col != COLOR_OVERFLOW_COL) && (curr->end_col != MAX_LINE))
                  curr->end_col   += insert_chars;
            }
         else
            {
               if (first_line == NULL)
                  first_line = curr;
               else
                  first_line_end->next = curr;
               first_line_end = curr;

               if ((curr->end_col != COLOR_OVERFLOW_COL) && (curr->end_col != MAX_LINE))
                  curr->end_col   += insert_chars;
            }
}

if (first_line_end)
   {
      if (front_target)
         flatten_cdgc(first_line, front_target, MAX_LINE+1, False);
      free_cdgc(first_line);
   }

if (sec_line_end)
   {
      if (back_target)
         flatten_cdgc(sec_line, back_target, MAX_LINE+1, False);
      free_cdgc(sec_line);
   }

} /* end of cdgc_split */


/************************************************************************

NAME:       find_cdgc - Scan the memdata database for a color/label data

PURPOSE:    This routine scans the memdata file looking for color data
            or mark data.

PARAMETERS:
   1.  token           - pointer to DATA_TOKEN (INPUT)
                         This is the memdata file to search.
                         Usually from the main pad.

   2.  bgfg            - pointer to char (INPUT)
                         This is background/foreground color to look for.
                         Usually from the main pad.

   3.  top_line        - int (INPUT)
                         This is the first line to scan if searching forward
                         and the last line to scan if searching backward.

   4.  bottom_line     - int (INPUT)
                         This is the first line not to scan if searching forward
                         and the first line to scan if searching backward.

   5.  search_down     - int (INPUT)
                         This flag specifies whether to search forward in the
                         file (down) or backward (up).
                         True  -  Search forward
                         False -  Search backward

   6.  full_match      - int (INPUT)
                         This flag specifies whether we need an exact match on
                         the passed bg/fg or just a leading substr for the length
                         of the passed bgfg.  That is if bgfg="red/" and the
                         cdgc element being inspected is "red/blue", Full_match=True
                         would fail to match while Full_match=False would succeed.
                         True  -  Full match
                         False -  partial match

   7.  type              int (INPUT) 
                         One of FIND_CDGC_COLOR or FIND_CDGC_LABEL or FIND_CDGC_ANY.
                         This state flag indicates whether the search is for labels,
                         colors, or either one.  It is only interesting if bgfg
                         is a null string.

   8.  found_line      - pointer to int (OUTPUT)
                         If the find is successful, the file line number of the
                         first byte of color or the mark is returned.  The beginning
                         pos of the color is always returned.  -1 is returned on not
                         found.

   9.  found_col       - pointer to int (OUTPUT)
                         If the find is successful, the file col number of the
                         first byte of color or the mark is returned.  The beginning
                         pos of the color is always returned.  Due to overflow, it
                         is too hard to find the end of the color.  Also this is 
                         consistent with 'find' processing.  -1 is returned on not
                         found.

   10. found_cdgc      - pointer to pointer to FANCY_LINE (OUTPUT)
                         If the find is successful, actual cdgc refering to the
                         found bgfg is returned in this argument.  This is one of
                         the FANCY_LINE elements in the FANCY_LINE returned by
                         this function.  If the function returns NULL, this parm
                         is set to null too.

FUNCTIONS :
   1.   Read color data lines sequentially in the memdata database looking
        for the required data.

   2.   When the data is found, expand the cdgc and find the column.


RETURNED VALUE:
   cdgc       - pointer to FANCY_LINE (linked list)
                NULL  - The data was not found
                ^NULL - The cdgc list containing the requested data.

    
*************************************************************************/

FANCY_LINE *find_cdgc(DATA_TOKEN   *token,               /* input   */
                      char         *bgfg,                /* input   */
                      int           top_line,            /* input   */
                      int           bottom_line,         /* input   */
                      int           search_down,         /* input   */
                      int           full_match,          /* input   */
                      int           type,                /* input   */
                      int          *found_line,          /* output  */
                      int          *found_col,           /* output  */
                      FANCY_LINE  **found_cdgc)          /* output  */
{
char                    *color_line;
char                    *p;
int                      current_lineno;
int                      color_lineno;
int                      done = False;
int                      bgfg_len;
int                      found;
FANCY_LINE              *cdgc = NULL;
FANCY_LINE              *curr;

*found_line = -1;
*found_col = -1;
*found_cdgc = NULL;

if (!bgfg)
   bgfg = "";

/***************************************************************
*  Search in the appropriate direction for color.  If a match
*  is found (or no match needed) stop the search.
***************************************************************/
if (search_down)
   current_lineno = top_line-1;
else
   current_lineno = bottom_line;

while(!done)
{
   if (search_down) /* search downward */
      {
         if (current_lineno < 0)
            {
               current_lineno = 0;
               color_line = get_color_by_num(token,
                                      current_lineno,
                                      COLOR_CURRENT,
                                      &color_lineno);
               if ((color_line == NULL) || (*color_line == '\0'))
                  color_line = get_color_by_num(token,
                                         current_lineno,
                                         COLOR_DOWN,
                                         &color_lineno);
            }
         else
            color_line = get_color_by_num(token,
                                   current_lineno,
                                   COLOR_DOWN,
                                   &color_lineno);

         if (color_lineno > bottom_line)
            color_line = NULL;
 
      }  /* end of search downward */
   else
      {
         if (current_lineno > 0)
            color_line = get_color_by_num(token,
                                   current_lineno-1,
                                   COLOR_UP,
                                   &color_lineno);
         else
            color_line = "";

         if (color_lineno < top_line)
            color_line = NULL;
 
      } /* end of search upward */

   if ((color_line == NULL) || (*color_line == '\0'))
      done = True; /* not found */
   else
      if ((bgfg[0] == '\0') || (strstr(color_line, bgfg) != NULL))
         {
            /***************************************************************
            *  We found a color line with possibly the right stuff on it.
            *
            *  Expand the cdgc to linked list format.  We do not care about
            *  the GC's since we do not need them to flatten back to char format.
            *  This list will not be used for drawing.
            ***************************************************************/
            bgfg_len = strlen(bgfg);
            cdgc = expand_cdgc(NULL, None, color_line, None, NULL);
            found = False;

            for (curr = cdgc;
                 curr;
                 curr = curr->next)
            {
               /* RES 4/9/98, ignore zero sized color */
               /* RES 5/13/98, Don't ignore labels */
               if ((curr->first_col >= curr->end_col) && !(IS_LABEL(curr->bgfg)))
                  continue;

               if (bgfg[0] == '\0')
                  if (type == FIND_CDGC_ANY)
                     found = True;
                  else
                     if ((type == FIND_CDGC_COLOR) && !(IS_LABEL(curr->bgfg)))
                        found = True;
                     else
                        if ((type == FIND_CDGC_LABEL) && (IS_LABEL(curr->bgfg)))
                           found = True;
                        else
                           found = False;
               else
                  {
                     if (bgfg[0] == '/')
                        p = strchr(curr->bgfg, '/'); /* search for foreground color only */
                     else
                        p = curr->bgfg;
                     found = (p && (full_match ? strcmp(p, bgfg) : strncmp(p, bgfg, bgfg_len)) == 0);
                     if ((type == FIND_CDGC_LABEL) && !(IS_LABEL(curr->bgfg)))
                        found = False;
                     if ((type == FIND_CDGC_COLOR) && (IS_LABEL(curr->bgfg)))
                        found = False;
                  }
               if (found)
                  {
                     *found_line = color_lineno;
                     *found_col =  curr->first_col;
                     *found_cdgc = curr;
                     if (search_down)
                        break;  /* search down takes the first one we find */
                  }
            }
            /***************************************************************
            *  If we found a match, keep it.  Otherwise clear and continue.
            ***************************************************************/
            if (found)
               done = True;
            else
               {
                  free_cdgc(cdgc);
                  cdgc = NULL;
                  current_lineno = color_lineno; /* keep searching */
               }
         }
      else
         current_lineno = color_lineno; /* keep searching */

} /* while not done (still searching) */

return(cdgc);

} /* end of find_cdgc */


/************************************************************************

NAME:      cdgc_clean  - adjust boundaries of covered up color.

PURPOSE:    This routine will check the linked list of color data and
            adjust the boundaries of color which is partially or fully
            covered up.

PARAMETERS:
   1.  cdgc_list       - pointer to FANCY_LINE (INPUT)
                         This is the color data list to be manipulated.
                         This color data is associated with the line
                         rather than overflowing onto the line.

FUNCTIONS :
   1.   Walk the linked list in a set of nested loops.  Since
        items near the front of the list cover up items near the
        end, the one nearer the front is the master.

RETURNED VALUE:
   changed  -  int
               True  -  The passed cdgc_list was changed.
               False -  The passed cdgc_list was not changed.

*************************************************************************/

static int   cdgc_clean(FANCY_LINE      *cdgc_list)   /* input/output */
{
FANCY_LINE           *master;
FANCY_LINE           *curr;
FANCY_LINE           *new;
int                   changed = False;

for (master = cdgc_list;
     master;
     master = master->next)
{
   if ((master->first_col >= master->end_col) || (IS_LABEL(master->bgfg)))
      continue; /* not interested in empty color areas or lablels */

   for (curr = master->next;
        curr;
        curr = curr->next)
   {
   /***************************************************************
   *  RES 2/25/1999, fix bug found in regression testing.
   *  Don't clean up labels.
   ***************************************************************/
   if (IS_LABEL(curr->bgfg))
      continue; /* not labels */

   /***************************************************************
   *  If the end of the master extends into the beginning of the
   *  current entry, back off the start of the current entry unless
   *  the current entry started before the master.
   ***************************************************************/
   if ((master->end_col > curr->first_col) && (master->first_col <= curr->first_col))
      {
         changed = True;
         curr->first_col  = master->end_col;
      }

   /***************************************************************
   *  If the beginning of the master extends into the end of the
   *  current entry, back off the end of the current entry.
   ***************************************************************/
   if ((master->first_col < curr->end_col) && (master->end_col > curr->end_col))
      {
         changed = True;
         curr->end_col = master->first_col;
         changed = True;
      }

   /***************************************************************
   *  If the master is totally contained in the current,  split the
   *  current entry into 2 pieces.
   ***************************************************************/
   if ((master->first_col > curr->first_col) && (master->end_col < curr->end_col))
      {
         new = dup_cdgc(curr);
         new->next = curr->next;  /* put new after curr in the list */
         curr->next = new;
         curr->end_col = master->first_col; /* curr gets the front part */
         new->first_col = master->end_col;
         changed = True;
      }

   /* see if the coloration went away */
   if (curr->first_col >= curr->end_col)
      curr->end_col = curr->first_col;
   }
}

return(changed);

}  /* end of cdgc_clean */




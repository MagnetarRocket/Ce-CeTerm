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
*  module wdf.c
*
*  This module provides the wdf (window default position list) feature of ce
*  and ceterm.  The window positions are used to determine the start
*  positions of windows when the program first comes up.  dm_wdf loads
*  the property hung off the root window with the geometries which are
*  usually kept in the .Cekeys file.  wdf_get_geometry extracts the
*  next geometry off the queue.
*
*  Routines:
*     dm_wdf                 -   Add or replace an entry in the WDF or WDC list.
*     wdf_get_geometry       -   Get the next window geometry.
*     wdc_get_colors         -   Get the next window color set
*     wdf_save_last          -   Save the current window geometry and color set on exit
*
* Internal:
*     check_atoms            -   Make sure we have all the atoms we need to get.
*     dump_wd_list           -   debugging routine to dump the wd list structure.
*     set_color_parms        -  Put color/color in two malloc'ed pieces of storage.
*
*
* OVERVIEW of WDF operations:
*     If Ce comes up and there is no geometry string specified via parms or the .Xdefaults
*     file, it gets a geometry string from the wdf string.  The same is true for color.
*
*     The CE_WD_LIST property has the following format:
*
*     +----+-----+-------+----+-----+-------+----+-----+-------+     +----+-----+-------+
*     | p0 |   data 0    | p1 |   data 1    | p2 |   data 2    | ... | p11| data 11     |...
*     +----+-------------+----+-------------+----+-------------+     +----+-------------+
*     0    1            51                                         
*
*     Data strings are either geometry strings or color strings.  There is a separate
*     property for each one.
*     Each geometry string has the normal format [c] width x height + x + y
*     Each color string has the format backgroundcolor,foregroundcolor
*     In the property the p0 through p... are the wdf or wdc position number for the
*     data.  The wdf command just appends the structures together in any order as it gets them
*     When the property is read in, it is sorted into a fixed structure. Duplicates are removed.
*     
*     The strings are null terminated.
*
*     When wdf_get_geometry goes to get a geometry it needs to serialize with other
*     windows going after geometries.  Using the limited serialization function of
*     the server, the following property is used to serialize.
*
*     The CE_WDF_Q property has the following format:
*     
*     +-----+-----+-----+-----+
*     | pid | pid | pid | pid | ....
*     +-----+-----+-----+-----+
*        4     4     4     4 
*
*    The current PID of the process is added to the queue in append mode.  This establishes
*    The position on the queue.  The property is then read in and scanned for the current
*    pid.  The position modulo the number of geometries in the CE_WDF_LIST is the one to
*    choose.  Cleanup on the running que is performed when the modulo is zero and the
*    queue size is greater than 1.  Cleanup is performed by doing a read with delete against
*    part of the queue property.  This is also done for colors.
*
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /bsd4.3/usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h   /usr/include/sys/errno.h      */
#include <limits.h>         /* /usr/include/limits.h     */
#ifdef WIN32
#include <process.h>
#define getpid _getpid
#endif

#include "dmwin.h"
#include "dmsyms.h"
#include "emalloc.h"
#include "parms.h"
#include "wdf.h"
#include "xerror.h"
#include "xerrorpos.h"


/***************************************************************
*  
*  Structure used to describe the CE_WDF_LIST property saved in the
*  server.  These are all strings.
*  
*  WD_WINDOW_COUNT  is the array size of CE_WD_LIST elements.
*  WD_MAXLEN        is the max length of a geomentry string.  The
*                   total structure length must be a multiple
*                   of 4 for some machines.
*
***************************************************************/

#define WD_WINDOW_COUNT 12
#define WD_MAXLEN       51

typedef struct {
   char               wd_number;                      /* one byte int, 1 to WD_WINDOW_COUNT   */
   char               data[WD_MAXLEN];                /* geometry string or color string      */
} CE_WD_LIST;


/***************************************************************
*  
*  Atoms for communicating with other windows
*  CE_WDF_LIST is the property used to contain the geometry strings
*              it is set by dm_wdf and read by wdf_get_geometry.
*  CE_WDF_Q    is the property used by wdf_get_geometry to 
*              determine who gets wich window geometry.
*  CE_WDF_LASTWIN is the property used by to save the last geometry
*
*  CE_WDC_LIST is the property used to contain the colorset strings
*              it is set by dm_wdc and read by wdc_get_colors
*  CE_WDC_Q    is the property used by wdc_get_colors to 
*              determine who gets which window color set.
*  CE_WDC_LASTWIN is the property used by to save the last color set.
*
*  The atom declarations are held in the wdf.h file since they are
*  needed in the main event loop.
*  
***************************************************************/

#define  CE_WDF_LIST_NAME        "CE_WDF_LIST"
#define  CE_WDF_Q                "CE_WDF_Q"
#define  CE_WDF_LASTWIN          "CE_WDF_LASTWIN"

#define  CE_WDC_LIST_NAME        "CE_WDC_LIST"
#define  CE_WDC_Q                "CE_WDC_Q"
#define  CE_WDC_LASTWIN          "CE_WDC_LASTWIN"

/***************************************************************
*  
*  Prototypes for local routines.
*  
***************************************************************/

static char  *wd_get_data(Display     *display,
                          PROPERTIES  *properties,
                          int          color);

static void check_atoms(Display     *display,
                        PROPERTIES  *properties);

static void set_color_parms(char   *buff,
                            char  **bgc,
                            char  **fgc);

static void dump_wd_list(char   *buff,
                         int     buff_len,
                         int     color_atom);


/************************************************************************

NAME:      dm_wdf -  Add or replace an entry in the WDF or WDC list.


PURPOSE:    This routine adds or replaces a entry in the wdf geometry list.
            It uses selection mechanisms to do serialization.

PARAMETERS:

   1.  dmc        -  pointer to DMC (INPUT)
                     This is the command structure for the wdf command which
                     caused this routine to be called.

   2.  display     - pointer to Display (INPUT)
                     This is a display pointer used in X calls.

   4.  properties  - pointer to PROPERTIES (INPUT)
                     This is the static list of X properties
                     associated with this display.

FUNCTIONS :

   1.   Make sure the atoms are set up.

   2.   Get ownership of the CE_WDF_LIST selection.

   3.   Read the property, If it does not exist, initialize an empty one.

   4.   Fill in the new data and see if we still have
        ownership. of the property.  If so, write the property back out to the root window.


*************************************************************************/

void  dm_wdf(DMC         *dmc,
             Display     *display,
             PROPERTIES  *properties)
{
int               wdf_index;
CE_WD_LIST        wd_entry;
Atom              actual_type;
int               actual_format;
int               keydef_property_size;
unsigned long     nitems;
unsigned long     bytes_after;
char             *buff = NULL;
char              msg[256];
Atom              list_atom;

check_atoms(display, properties);

/***************************************************************
*  
*  Determine which set of atoms we are using.
*  
***************************************************************/

if (dmc->wdf.cmd == DM_wdf)
   {
      list_atom =  properties->ce_wdf_list_atom;
      wdf_index = dmc->wdf.number;
   }
else
   {
      list_atom =  properties->ce_wdc_list_atom;
      wdf_index = dmc->wdc.number;
   }

/***************************************************************
*  
* Handle the special case of -1 (delete wdf or wdc list).
* Also the special case of 0, dump the existing list.
*  
***************************************************************/

if (wdf_index < 0)
   {
      DEBUG20(fprintf(stderr, "Deleting %s property\n", ((dmc->wdf.cmd == DM_wdf) ? "WDF" : "WDC"));)
      DEBUG9(XERRORPOS)
      XGetWindowProperty(display,
                         RootWindow(display, DefaultScreen(display)),
                         list_atom,
                         0, /* offset zero from start of property */
                         INT_MAX / 4,
                         True, /* do delete the property */
                         AnyPropertyType,
                         &actual_type,
                         &actual_format,
                         &nitems,
                         &bytes_after,
                         (unsigned char **)&buff);
      keydef_property_size = nitems * (actual_format / 8);
      if (keydef_property_size > 0)
         XFree(buff);
      snprintf(msg, sizeof(msg), "Deleted %s list", ((dmc->wdf.cmd == DM_wdf) ? "geometry" : "color"));
      dm_error(msg, DM_ERROR_MSG);
      return;
   }

if (wdf_index == 0)
   {
      DEBUG9(XERRORPOS)
      XGetWindowProperty(display,
                         RootWindow(display, DefaultScreen(display)),
                         list_atom,
                         0, /* offset zero from start of property */
                         INT_MAX / 4,
                         False, /* do not delete the property */
                         AnyPropertyType,
                         &actual_type,
                         &actual_format,
                         &nitems,
                         &bytes_after,
                         (unsigned char **)&buff);
      keydef_property_size = nitems * (actual_format / 8);
      if (keydef_property_size > 0)
         {
            dump_wd_list(buff, keydef_property_size, (dmc->wdf.cmd == DM_wdc));
            XFree(buff);
         }
      else
         {
            snprintf(msg, sizeof(msg), "No %s list found", ((dmc->wdf.cmd == DM_wdf) ? "geometry" : "color"));
            dm_error(msg, DM_ERROR_MSG);
         }
      return;
   }

/***************************************************************
*  
*  Make sure the WDF number value is in range.
*  If so, convert it to an index into the wdf list.
*  
***************************************************************/


if (wdf_index > WD_WINDOW_COUNT)
   {
      snprintf(msg, sizeof(msg), "(%s) number %d is out of range, range is 1 to %d", dmsyms[dmc->wdf.cmd].name, dmc->wdf.number, WD_WINDOW_COUNT);
      dm_error(msg, DM_ERROR_BEEP);
      return;
   }
else
   wdf_index--;


DEBUG20(
if (dmc->wdf.cmd == DM_wdf)
   fprintf(stderr, "@dm_wdf:  Put in window pos %d, \"%s\"\n", dmc->wdf.number, dmc->wdf.geometry);
else
   fprintf(stderr, "@dm_wdc:  Put in window pos %d, \"%s/%s\"\n", dmc->wdc.number,
                    dmc->wdc.bgc ? dmc->wdc.bgc : "", dmc->wdc.fgc ? dmc->wdc.fgc : "");
)

/***************************************************************
*  
*  for WDC, Make sure the colors are good.
*  
***************************************************************/

if (dmc->wdf.cmd == DM_wdc)
   {
      if (dmc->wdc.bgc && *dmc->wdc.bgc && (colorname_to_pixel(display, dmc->wdc.bgc) == BAD_COLOR))
         {
            snprintf(msg, sizeof(msg), "Cannot allocate color %s", dmc->wdc.bgc);
            dm_error(msg, DM_ERROR_BEEP);
            return;
         }

      if (dmc->wdc.fgc && *dmc->wdc.fgc && (colorname_to_pixel(display, dmc->wdc.fgc) == BAD_COLOR))
         {
            snprintf(msg, sizeof(msg), "Cannot allocate color %s", dmc->wdc.fgc);
            dm_error(msg, DM_ERROR_BEEP);
            return;
         }
   }

/***************************************************************
*  
*  Fill in the data 
*  
***************************************************************/

wd_entry.wd_number = wdf_index;
if (dmc->wdf.cmd == DM_wdf)
   {
      /***************************************************************
      *  Copy in the new geometry data
      ***************************************************************/
      strncpy(wd_entry.data, dmc->wdf.geometry, WD_MAXLEN);
      if ((int)strlen(dmc->wdf.geometry) >= WD_MAXLEN)
         {
            snprintf(msg, sizeof(msg), "wdf geometry string too long, truncated (%s)", dmc->wdf.geometry);
            dm_error(msg, DM_ERROR_BEEP);
            wd_entry.data[WD_MAXLEN-1] = '\0';
         }
   }
else
   {
      /***************************************************************
      *  Make sure the new color set data will fit
      ***************************************************************/
      snprintf(msg, sizeof(msg), "%s/%s", (dmc->wdc.bgc ? dmc->wdc.bgc : ""), (dmc->wdc.fgc ? dmc->wdc.fgc : ""));
      strncpy(wd_entry.data, msg, WD_MAXLEN);
      if (((int)strlen(msg)) > WD_MAXLEN)
         {
            snprintf(msg, sizeof(msg), "(wdc) Implementation limit exceeded, lengths of color names exceed %d chars, %s/%s", WD_MAXLEN-2, dmc->wdc.bgc, dmc->wdc.fgc);
            dm_error(msg, DM_ERROR_BEEP);
            return;
         }
   }

/***************************************************************
*  
*  Append the entry to the property hung off the root window
*  
***************************************************************/

DEBUG9(XERRORPOS)
XChangeProperty(display,
                RootWindow(display, DefaultScreen(display)),
                list_atom,
                XA_STRING, 
                8, /* only 1 byte entries */
                PropModeAppend,
                (unsigned char *)&wd_entry,
                (int)sizeof(CE_WD_LIST));
DEBUG20(fprintf(stderr, "dm_wdf:  Property %s updated\n", ((dmc->wdf.cmd == DM_wdf) ? CE_WDF_LIST_NAME : CE_WDC_LIST_NAME));)
XFlush(display);

} /* end of dm_wdf */


/************************************************************************

NAME:      wdf_get_geometry  -  Get the next window geometry.


PURPOSE:    This routine gets the next geometry off the queue and returns it.

PARAMETERS:

   1.  display     - pointer to Display (INPUT)
                     This is the display handle needed for X calls.

   2.  properties  - pointer to PROPERTIES (INPUT)
                     This is the static list of X properties
                     associated with this display.

FUNCTIONS :
   1.   Make sure all the atoms are set up.

   2.   Read the CE_WDF_LIST property and sort the active elements.
        into the
        If there is only one return it.  If there are zero, return NULL.

   3.   Append this processes pid to the end of the CE_WDF_Q property
        on the root window.

   4.   Read the CE_WDF_Q property back in and scan for this window id.

   5.   Select the geometry to choose based on our position in the queue.

   6.   If we are choosing the first geometry in the list and we are not
        the first pid on the queue, do a read / delete agaist the que
        for a length covering up to but not including our pid.

RETURNED VALUE:        

   geo  -  The selected geometry string is returned.
           NULL is returned if there are no window defaults set up.


*************************************************************************/

char  *wdf_get_geometry(Display     *display,
                        PROPERTIES  *properties)
{

return(wd_get_data(display, properties, False));

} /* end of wdf_get_geometry */


/************************************************************************

NAME:      wd_get_data  -  Get the next window geometry or color set string


PURPOSE:    This routine gets the next geometry off the queue and returns it.

PARAMETERS:

   1.  display     - pointer to Display (INPUT)
                     This is the display handle needed for X calls.

   2.  properties  - pointer to PROPERTIES (INPUT)
                     This is the static list of X properties
                     associated with this display.

   3.  color       - int (INPUT)
                     This flag is true if the color set is desired.
                     False for the geometry.

FUNCTIONS :
   1.   Make sure all the atoms are set up.

   2.   Read the CE_WDF_LIST property and sort the active elements.
        into the
        If there is only one return it.  If there are zero, return NULL.

   3.   Append this processes pid to the end of the CE_WDF_Q property
        on the root window.

   4.   Read the CE_WDF_Q property back in and scan for this window id.

   5.   Select the geometry to choose based on our position in the queue.

   6.   If we are choosing the first geometry in the list and we are not
        the first pid on the queue, do a read / delete agaist the que
        for a length covering up to but not including our pid.

RETURNED VALUE:        

   data  -  The selected data string is returned.
            This is either a color set or a geometry.
            It is in malloc'ed storage.
            NULL is returned if there are no window defaults set up.

*************************************************************************/

static char  *wd_get_data(Display     *display,
                          PROPERTIES  *properties,
                          int          color)
{
int               wd_index;
Atom              actual_type;
int               actual_format;
int               keydef_property_size;
int               queue_property_size;
unsigned long     nitems;
unsigned long     bytes_after;
char             *qbuff = NULL;
char             *buff = NULL;
char             *buff_pos;
char             *buff_end;
char             *que_end;
int               wd_count = 0;
char             *cur_geo;
int               this_pid;
int               pid_index = 0;
int              *q_pos;
int               del_size;
int               i;
char             *p;
char              returned_data[WD_MAXLEN];
char              wd_list[WD_WINDOW_COUNT][WD_MAXLEN];
CE_WD_LIST       *wd_ent;
Atom              list_atom;
Atom              lastwin_atom;
Atom              queue_atom;


check_atoms(display, properties);

if (color)
   {
      list_atom    = properties->ce_wdc_list_atom;
      lastwin_atom = properties->ce_wdc_lastwin_atom;
      queue_atom   = properties->ce_wdc_wq_atom;
   }
else
   {
      list_atom    = properties->ce_wdf_list_atom;
      lastwin_atom = properties->ce_wdf_lastwin_atom;
      queue_atom   = properties->ce_wdf_wq_atom;
   }

returned_data[0] = '\0';

/***************************************************************
*  
*  Check the last window position first.  If we get one, use it.
*  
***************************************************************/

DEBUG9(XERRORPOS)
XGetWindowProperty(display,
                   RootWindow(display, DefaultScreen(display)),
                   lastwin_atom,
                   0, /* offset zero from start of property */
                   INT_MAX / 4,
                   True, /* delete the property */
                   AnyPropertyType,
                   &actual_type,
                   &actual_format,
                   &nitems,
                   &bytes_after,
                   (unsigned char **)&buff);

keydef_property_size = nitems * (actual_format / 8);

DEBUG9(XERRORPOS)
DEBUG20(
if (actual_type != None)
   p = XGetAtomName(display, actual_type);
else
   p = "None";
fprintf(stderr, "wd_get_data,lastwin:  Size = %d, actual_type = %d (%s), actual_format = %d, nitems = %d, &bytes_after = %d\n",
                keydef_property_size, actual_type, p,
                actual_format, nitems, bytes_after);
if (actual_type != None)
   XFree(p);
)

if ((keydef_property_size > 0) && (keydef_property_size < WD_MAXLEN))
   {
      strcpy(returned_data, buff);
      DEBUG20(fprintf(stderr, ":  Using last saved %s: %s\n", (color ? "Colors" : "Geometry" ), returned_data);)
      XFree(buff);
      return(malloc_copy(returned_data));
   }

/***************************************************************
*  
*  Get the LIST
*  
***************************************************************/

DEBUG9(XERRORPOS)
XGetWindowProperty(display,
                   RootWindow(display, DefaultScreen(display)),
                   list_atom,
                   0, /* offset zero from start of property */
                   INT_MAX / 4,
                   False, /* do not delete the property */
                   AnyPropertyType,
                   &actual_type,
                   &actual_format,
                   &nitems,
                   &bytes_after,
                   (unsigned char **)&buff);

keydef_property_size = nitems * (actual_format / 8);
buff_pos = buff;
buff_end = buff + keydef_property_size;

DEBUG9(XERRORPOS)
DEBUG20(
if (actual_type != None)
   p = XGetAtomName(display, actual_type);
else
   p = "None";
fprintf(stderr, "wd_get_data:  Size = %d, actual_type = %d (%s), actual_format = %d, nitems = %d, &bytes_after = %d\n",
                keydef_property_size, actual_type, p,
                actual_format, nitems, bytes_after);
if (actual_type != None)
   XFree(p);
dump_wd_list(buff, keydef_property_size, color);
)

/***************************************************************
*  
*  If it does not exist, return NULL.
*  
***************************************************************/

if (keydef_property_size == 0)
   return(NULL);

/***************************************************************
*  
*  Sort the wd property into the list and remove duplicates.
*  Use the last verion of a value found.
*  
***************************************************************/

memset((char *)wd_list, 0, sizeof(wd_list));

while(buff_pos+sizeof(CE_WD_LIST) <= buff_end)
{
   wd_ent = (CE_WD_LIST *)buff_pos;
   if ((wd_ent->wd_number >= WD_WINDOW_COUNT) || (wd_ent->wd_number < 0))
      {
         DEBUG20(fprintf(stderr, "Bad WD number %d found in property, ignored\n", wd_ent->wd_number);)
      }
   else
      strcpy(wd_list[wd_ent->wd_number], wd_ent->data);

   buff_pos += sizeof(CE_WD_LIST);
} /* end of while walking through the wd property */

XFree(buff);

/***************************************************************
*  
*  Count and compress the wd list.
*  
***************************************************************/

buff_pos = (char *)wd_list;
cur_geo = buff_pos;

for (i = 0; i < WD_WINDOW_COUNT; i++)
   if (*buff_pos != '\0')
      {
         if (cur_geo != buff_pos)
            {
               strcpy(cur_geo, buff_pos);
               *buff_pos = '\0';
            }
         if (returned_data[0] == '\0')
            strcpy(returned_data, cur_geo);
         wd_count++;
         cur_geo  += WD_MAXLEN;
         buff_pos += WD_MAXLEN;
      }
   else
      buff_pos += WD_MAXLEN;

/***************************************************************
*  
*  Deal with the trivial cases of zero or 1 entries.
*  
***************************************************************/

if (wd_count == 0)
   return(NULL);

if (wd_count == 1)
   return(malloc_copy(returned_data));
      
/***************************************************************
*  
*  Write our pid to the queue and then read in the queue.
*  
***************************************************************/

this_pid = getpid();

DEBUG9(XERRORPOS)
XChangeProperty(display,
                RootWindow(display, DefaultScreen(display)),
                queue_atom,
                XA_STRING, 
                8, /* we handle little endian ourselves */
                PropModeAppend,
                (unsigned char *)&this_pid, 
                (int)sizeof(int));
 
XFlush(display);

DEBUG9(XERRORPOS)
XGetWindowProperty(display,
                   RootWindow(display, DefaultScreen(display)),
                   queue_atom,
                   0, /* offset zero from start of property */
                   INT_MAX / 4,
                   False, /* do not delete the property */
                   AnyPropertyType,
                   &actual_type,
                   &actual_format,
                   &nitems,
                   &bytes_after,
                   (unsigned char **)&qbuff);

queue_property_size = nitems * (actual_format / 8);
que_end = qbuff + queue_property_size;

for (q_pos = (int *)qbuff; (char *)q_pos < que_end; q_pos++)
   if (*q_pos == this_pid)
      break;
   else
      pid_index++;

/***************************************************************
*  
*  See if we found it.  If not, (very unlikely), use the first one.
*  
***************************************************************/

if ((char *)q_pos >= que_end)
   pid_index = 0;

/***************************************************************
*  
*  Convert the pid index to the wd_index by dividing the 
*  number of geometries by the position of this pid in the pid
*  que and taking the remainder.
*  
***************************************************************/

wd_index = pid_index % wd_count;
strcpy(returned_data, wd_list[wd_index]);
DEBUG20(fprintf(stderr, ":  Choose index %d : %s, pid index is %d\n", wd_index, returned_data, pid_index);)

/***************************************************************
*  
*  If we have wd_index zero and are not in pos zero, clean
*  up the que.
*  
***************************************************************/

if (pid_index && !wd_index)
   {
      del_size = (pid_index + 1) * sizeof(int);
      if (del_size == queue_property_size)
         {
            DEBUG20(fprintf(stderr, ":  Delete whole property\n");)
            DEBUG9(XERRORPOS)
            XGetWindowProperty(display,
                               RootWindow(display, DefaultScreen(display)),
                               queue_atom,
                               0, /* offset zero from start of property */
                               del_size,
                               True, /* do delete the property */
                               AnyPropertyType,
                               &actual_type,
                               &actual_format,
                               &nitems,
                               &bytes_after,
                               (unsigned char **)&qbuff);
         }
      else
         {
            DEBUG20(fprintf(stderr, ":  Replace property with shorter pid list (len = %d)\n", (int)(queue_property_size-del_size));)
            DEBUG9(XERRORPOS)
            XChangeProperty(display,
                            RootWindow(display, DefaultScreen(display)),
                            queue_atom,
                            XA_STRING, 
                            8,
                            PropModeReplace,
                            (unsigned char *)&qbuff[del_size],
                            (int)(queue_property_size-del_size));
         }
    }

return(malloc_copy(returned_data));

} /* end of wd_get_data */


/************************************************************************

NAME:      wdc_get_colors         -   Get the next window color set


PURPOSE:    This routine gets the next color set off the queue and returns it.

PARAMETERS:

   1.  display     - pointer to Display (INPUT)
                     This is the display handle needed for X calls.

   2.  bgc         - pointer to pointer to char (OUTPUT)
                     The place to put the address of a malloc'ed
                     string containing the background color is passed in.

   3.  bgc         - pointer to pointer to char (OUTPUT)
                     The place to put the address of a mallo'ced
                     string containing the foreground color is passed in.

   4.  properties  - pointer to PROPERTIES (INPUT)
                     This is the static list of X properties
                     associated with this display.

FUNCTIONS :
   1.   Make sure all the atoms are set up.

   2.   Initially null out the returned parameters.

   3.   Read the CE_WDC_LIST property and count the active elements.
        If there is only one return it.  If there are zero, return NULL.

   4.   Append this processes pid to the end of the CE_WDC_Q property
        on the root window.

   5.   Read the CE_WDC_Q property back in and scan for this window id.

   6.   Select the color set to use based on our position in the queue.

   7.   If we are choosing the first color set in the list and we are not
        the first pid on the queue, do a read / delete agaist the que
        for a length covering up to but not including our pid.


*************************************************************************/

void   wdc_get_colors(Display     *display,       /* input  */
                      char       **bgc,           /* output */
                      char       **fgc,           /* output */
                      PROPERTIES  *properties)    /* input  */
{

char             *saved_colors;

*bgc = NULL;
*fgc = NULL;

saved_colors = wd_get_data(display, properties, True);
if (saved_colors)
   {
      set_color_parms(saved_colors, bgc, fgc);
      free(saved_colors);
   }

} /* end of wdc_get_colors */


/************************************************************************

NAME:      wdf_save_last  -  Save the current window geometry on exit


PURPOSE:    This routine is called during window close processing to
            save the last window geometry.

PARAMETERS:

   1.  main_window - pointer to DRAWABLE_DESCR (INPUT)
                     This is the main window drawable description

   2.  properties  - pointer to PROPERTIES (INPUT)
                     This is the static list of X properties
                     associated with this display.


FUNCTIONS :
   1.   Build the geometry string from the window geometry.

   2.   Save the window string in the root window property.


*************************************************************************/

void   wdf_save_last(DISPLAY_DESCR   *dspl_descr)
{
char              geometry_string[WD_MAXLEN];
char              colorset_string[WD_MAXLEN*2];
char             *bgc;
char             *fgc;

snprintf(geometry_string, sizeof(geometry_string), "%dx%d%+d%+d",
        dspl_descr->main_pad->window->width, dspl_descr->main_pad->window->height,
        dspl_descr->main_pad->window->x, dspl_descr->main_pad->window->y);
fgc = FOREGROUND_CLR;
if (!fgc)
   fgc = "white"; /* should neve use this, safety code */
bgc = BACKGROUND_CLR;
if (!bgc)
   bgc = "black";
snprintf(colorset_string, sizeof(colorset_string), "%s/%s", bgc, fgc); 

check_atoms(dspl_descr->display, dspl_descr->properties);

DEBUG9(XERRORPOS)
XChangeProperty(dspl_descr->display,
                RootWindow(dspl_descr->display, DefaultScreen(dspl_descr->display)),
                dspl_descr->properties->ce_wdf_lastwin_atom,
                XA_STRING, 
                8, /* we handle little endian ourselves */
                PropModeReplace,
                (unsigned char *)geometry_string, 
                (int)strlen(geometry_string)+1); /* include the null */

DEBUG9(XERRORPOS)
XChangeProperty(dspl_descr->display,
                RootWindow(dspl_descr->display, DefaultScreen(dspl_descr->display)),
                dspl_descr->properties->ce_wdc_lastwin_atom,
                XA_STRING, 
                8, /* we handle little endian ourselves */
                PropModeReplace,
                (unsigned char *)colorset_string, 
                (int)strlen(colorset_string)+1); /* include the null */
XFlush(dspl_descr->display);

} /* end of wdf_save_last */


/************************************************************************

NAME:      check_atoms  -   Make sure the needed atom ids have been gotten.

PURPOSE:    This routine checks to see that the atom ids needed by wdf have
            been gotten from the server.

PARAMETERS:

   1.  display     - pointer to Display (INPUT)
                     This is the display handle needed for X calls.

FUNCTIONS :

   1.   If the atoms are already gotten, do nothing.

   2.   Get each atom from the server.


*************************************************************************/

static void check_atoms(Display     *display,
                        PROPERTIES  *properties)
{
/***************************************************************
*  If the wdf_list buffer atom does not already exist, create it.
*  This is the property used to keep track of what files are being
*  edited.
***************************************************************/
if (properties->ce_wdf_list_atom == 0)
   {
      DEBUG9(XERRORPOS)
      properties->ce_wdf_list_atom    = XInternAtom(display, CE_WDF_LIST_NAME, False);
      DEBUG9(XERRORPOS)
      properties->ce_wdf_wq_atom      = XInternAtom(display, CE_WDF_Q, False);
      DEBUG9(XERRORPOS)
      properties->ce_wdf_lastwin_atom = XInternAtom(display, CE_WDF_LASTWIN, False);

      DEBUG20(fprintf(stderr, "ce_wdf_list_atom = %d, ce_wdf_wq_atom = %d, ce_wdf_lastwin_atom = %d\n",
                              properties->ce_wdf_list_atom, properties->ce_wdf_wq_atom, properties->ce_wdf_lastwin_atom);
      )

      DEBUG9(XERRORPOS)
      properties->ce_wdc_list_atom    = XInternAtom(display, CE_WDC_LIST_NAME, False);
      DEBUG9(XERRORPOS)
      properties->ce_wdc_wq_atom      = XInternAtom(display, CE_WDC_Q, False);
      DEBUG9(XERRORPOS)
      properties->ce_wdc_lastwin_atom = XInternAtom(display, CE_WDC_LASTWIN, False);

      DEBUG20(fprintf(stderr, "ce_wdc_list_atom = %d, ce_wdc_wq_atom = %d, ce_wdc_lastwin_atom = %d\n",
                              properties->ce_wdc_list_atom, properties->ce_wdc_wq_atom, properties->ce_wdc_lastwin_atom);

      )
   }
} /* end of check_atoms */


/************************************************************************

NAME:      set_color_parms  -  Put color/color in two malloc'ed pieces of storage.


PURPOSE:    This routine is called to copy the color data to the background and
            forground parms which expect malloc'ed storage.

PARAMETERS:

   1.  buff        - pointer to char (INPUT)
                     This is the string containing color/color

   2.  bgc         - pointer to pointer to char (OUTPUT)
                     This where the background color (first
                     part of string before the slash gets put.

   3.  fgc         - pointer to pointer to char (OUTPUT)
                     This where the foreground color (second
                     part of string after the slash gets put.


FUNCTIONS :
   1.   Find the separator slash.  If not found, forget the whole thing.
        This is not a valid color set.

   2.   Malloc and save the bgc and fgc colors.


*************************************************************************/

static void set_color_parms(char   *buff,
                            char  **bgc,
                            char  **fgc)
{
char             *sep;
int               len;

/***************************************************************
*  
*  Find the background foreground separator position.
*  If we cannot find it, the parms are invalid, so forget it.
*  
***************************************************************/

sep = strchr(buff, '/');
if (sep == NULL) 
   {
      DEBUG20(fprintf(stderr, "set_color_parms: Invalid color set string passed \"%s\"\n", buff);)
      return;
   }

/***************************************************************
*  
*  Malloc space for the background color and copy it in.
*  
***************************************************************/

len = (sep - buff) + 1;
*bgc = CE_MALLOC(len);
if (!(*bgc))
   return;
*sep = '\0';
strcpy(*bgc, buff);
*sep = '/';

/***************************************************************
*  
*  Now do the forground color the same way.
*  
***************************************************************/

sep++;
len = strlen(sep) + 1;
*fgc = CE_MALLOC(len);
if (*fgc)
   strcpy(*fgc, sep);


} /* end of set_color_parms */


/************************************************************************

NAME:      dump_wd_list  -   debugging routine to dump the wdf list structure.


PURPOSE:    This routine dumps the passed wdf list structure

PARAMETERS:

   1.  buff        - pointer to char (INPUT)
                     This is the buffer containing the WDF list.

   2.  buff_len    - int (INPUT)
                     This is the length of buffer buff.

   3.  color_atom  - int (INPUT)
                     This flag is True if we are dumping the color list
                     False for the geometry list.


FUNCTIONS :

   1.   For each WDF_LIST entry dump the geometry


*************************************************************************/

static void dump_wd_list(char   *buff,
                         int     buff_len,
                         int     color_atom)
{
char             *buff_end = buff + buff_len;
int               i;
CE_WD_LIST       *wd_ent;
char              msg[256];
char              c;

snprintf(msg, sizeof(msg), "%s list property, len = %d", (color_atom ? "Color" : "Geometry"), buff_len);
dm_error(msg, DM_ERROR_MSG);

while ((char *)buff < buff_end)
{
   wd_ent = (CE_WD_LIST *)buff;
   c = wd_ent->wd_number; /* Solaris 2.3, character promotion problem */
   i = c + 1;
   if (color_atom)
      {
         if (wd_ent->data[0] == '\0')
            snprintf(msg, sizeof(msg), "Color set %2d : NULL", i);
         else
            snprintf(msg, sizeof(msg), "Color set %2d : %s", i, wd_ent->data);

      }
   else
      {
         if (wd_ent->data[0] == '\0')
            snprintf(msg, sizeof(msg), "Geometry %2d : NULL", i);
         else
            snprintf(msg, sizeof(msg), "Geometry %2d : %s", i, wd_ent->data);
      }

   dm_error(msg, DM_ERROR_MSG);
   buff = buff + sizeof(CE_WD_LIST);
}

} /* end of dump_wd_list */


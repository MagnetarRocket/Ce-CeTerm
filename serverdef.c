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
*  module serverdef.c
*
*  Routines:
*         save_server_keydefs  - save the key definitions in the X server property
*         load_server_keydefs  - load the keydefs from the server
*         change_keydef        - Update the keydef property to show a keydef change.
*         get_changed_keydef   - Handle an update to the key defintions detected by a keydef property change.
*         dm_delsd             - Delete server copy of the key definitions
*         dm_keys              - Display all the definitions in a separate window
*
*  Local:
*         update_keymask       - Update the keymask field in the properties area if needed
*         flatten_keydef       - convert a keydef into flattened format
*         size_keydef          - get the length required for a key def if it was flattened.
*         inflate_keydef       - convert a flatted keydef into its normal format.
*         flatten_dmc          - convert one DMC structure into flattened format
*         size_dmc             - get the length required for one DMC structure if it was flattened
*         inflate_dmc          - convert a flatted DMC into its normal
*
*
*  OVERVIEW:
*         This module deals with saving the keydefs in the X server and extracting
*         the same.  
*
*         When the keydefs are read in from the file, they are put in the hsearch hash
*         table.  The hash table puts the entry in te table and also on a linked list.
*         The hash keyis the parsed and formated keysym, whether this is a key press or
*         release, or a button press or release, and what ctrl/shift/etc. keys are
*         needed.  The data portion is the list of DMC structures.
*
*         When keydefs are replaced, the same ENTRY in the hash table is used.  Thus,
*         the order of the linked list is not effected.  However, two things are required to be
*         done.
*         1.  Mark that there have been changes so we can accurately report this fact
*             in routine keydefs_changed.
*         2.  Append the new definition to the property stored in the server.
*             To do this, get the selection for the keydefs, and copy the data in
*             append mode.
*
*         The flattened keydefs are length/data constructs.  That is, there are length
*         fields followed by data.  The X server is told to treat this as character format
*         data, so we perform the bigendin and littleendin convertions using the TCP hton and ntoh
*         macros.
*
***************************************************************/


#include <stdio.h>           /* /usr/include/stdio.h  */
#include <string.h>          /* /usr/include/string.h  */
#include <limits.h>          /* /usr/include/limits.h */
#include <errno.h>           /* /usr/include/errno.h /usr/include/sys/errno.h */
#include <sys/types.h>       /* /usr/include/sys/types.h    */
#ifdef WIN32
#include <winsock.h>
#else
#include <netinet/in.h>      /* /usr/include/netinet/in.h */
#include <arpa/inet.h>       /* /usr/include/arpa/inet.h    */

#include <X11/X.h>           /* /usr/include/X11/X.h   */
#include <X11/Xlib.h>        /* /usr/include/X11/Xlib.h */
#include <X11/keysym.h>      /* /usr/include/X11/keysym.h /usr/include/X11/keysymdef.h */
#endif


/***************************************************************
*  
*  Property names and atoms used.  The atom for CEKEYCHANGE_ATOM_NAME
*  is in the serverdef.h file because the main event loop needs it.
*  
***************************************************************/

#define   CESELECTION_ATOM_NAME  "CeSelection"

#define   MASK_1          (ULONG_MAX << 1)
#define   ROUND_SHORT(n, base)  (((n - base) % 2) ? (n + 1) : n)
#define   SHORT_BUMP(a)   (a) += sizeof(short)

/***************************************************************
*  
*  This macro puts a null string in the buffer and bumps the
*  pointer.  It is used in flatten_dmc to copy in zero length strings.
*  
***************************************************************/

#define NULL_STRING(p)  *((p)++) = '\0'


#define _SERVERDEF_C_

#include "debug.h"
#include "dmc.h"
#include "dmsyms.h"
#include "dmwin.h"
#include "hsearch.h"
#include "kd.h"
#include "emalloc.h"
#include "serverdef.h"
#include "xerrorpos.h"
#include "parsedm.h"
#include "prompt.h"


/***************************************************************
*  
*  System routines which have no built in defines
*  
***************************************************************/

char  *getenv(const char *name);
int    access(char *path, int mode);
int    getpid(void);

static void update_keymask(char        *key,
                           PROPERTIES  *properties);

static void flatten_keydef(char   *keykey,    /* input  */
                           DMC    *dm_list,   /* input  */
                           char  **buff);     /* output / output (writes to the target and updates the pointer) */

static int size_keydef(char   *keykey,        /* input  */
                       DMC    *dm_list);      /* input  */

static void inflate_keydef(char  **keykey,    /* output */
                           DMC   **dm_list,   /* output */
                           char  **buff);     /* input  */

static void flatten_dmc(DMC     *mc,       /* input  */
                        char  **buff);     /* output / output (writes to the target and updates the pointer) */

static int size_dmc(DMC    *dmc);          /* input  */

static DMC *inflate_dmc(char   **buff);     /* input  */


/************************************************************************

NAME:      save_server_keydefs  - save the key definitions in the X server property


PURPOSE:    This routine saves the current key defintions in the X property maintained
            by Ce.

PARAMETERS:

   1.  display         - pointer to Display (INPUT)
                         This is the display pointer returned by XOpenDisplay.
                         It is needed for all Xlib calls.

   2.  properties      - pointer to PROPERTIES (INPUT/OUTPUT) (in buffer.h)
                         This is the list of properties used by this display

   3.  cekeys_atom_name -  pointer to char (INPUT)
                     This is the property name to hang the key definitios off.
                     the default name is: "CeKeys"

   4.   hash_table     - pointer to void (INPUT)
                         This is the hash table pointer for the current display.

FUNCTIONS :

   1.  If the flag shows that there have been no changes, return. ???

   2.  Walk the linked list and sum the space required for each flattened key definition.

   3.  Malloc space for all the flattened key defs.

   4.  Walk the linked list and the malloc'ed buffer and flatten each buffer into place.

   5.  Save the server keydefs in the Ce keydef property hung off the root window.


*************************************************************************/

void      save_server_keydefs(Display     *display,
                              PROPERTIES  *properties,
                              char        *cekeys_atom_name,
                              void        *hash_table)
{
ENTRY           *cur_key;
int              size_estimate = 0;
char            *buff;
char            *buff_pos;

/***************************************************************
*  
*  Get the estimate for the space required to save the keydefs
*  and malloc that much space.
*  
***************************************************************/

for (cur_key = hlinkhead(hash_table); cur_key != NULL; cur_key = (ENTRY *)cur_key->link)
   size_estimate +=  size_keydef(cur_key->key, (DMC *)cur_key->data);

DEBUG1(fprintf(stderr, "Server Keydef size estimate = %d\n", size_estimate);)

buff = CE_MALLOC(size_estimate);
if (buff == NULL)
   return;

/***************************************************************
*  
*  Walk the keydef list and fill in the buffer.
*  
***************************************************************/

buff_pos = buff;

for (cur_key = hlinkhead(hash_table); cur_key != NULL; cur_key = (ENTRY *)cur_key->link)
{
   flatten_keydef(cur_key->key, (DMC *)cur_key->data, &buff_pos);
   update_keymask(cur_key->key, properties);
}

properties->keydef_property_size = buff_pos - buff;

DEBUG1(fprintf(stderr, "Server Keydef size = %d\n", properties->keydef_property_size);)

if (properties->keydef_property_size == 0)
   {
      DEBUG1(fprintf(stderr, "No keydefs to save\n");)
      return;
   }

/***************************************************************
*  
*  Save the key definitions
*  
***************************************************************/

DEBUG9(XERRORPOS)
if (properties->cekeys_atom == None)
   properties->cekeys_atom = XInternAtom(display, cekeys_atom_name, False);

DEBUG9(XERRORPOS)
XChangeProperty(display,
                RootWindow(display, DefaultScreen(display)),
                properties->cekeys_atom,
                XA_STRING,
                8, /* 8 bit quantities, we handle littleendin ourselves */
                PropModeReplace,
                (unsigned char *)buff, 
                properties->keydef_property_size);

XFlush(display);

free(buff);

} /* end of save_server_keydefs */


/************************************************************************

NAME:      load_server_keydefs  - load the keydefs from the X server


PURPOSE:    This routine checks to see if there are key definitions stored
            in the X server.  If so, it reads them into the hash table and
            the linked list.

PARAMETERS:

   1.  display    -  pointer to Display (INPUT)
                     This flag indicates whether the keydefs are intialized
                     it is statically zero and set to one when the keydefs are
                     completed.

   2.  properties -  pointer to PROPERTIES (INPUT/OUTPUT) (in buffer.h)
                     This is the list of properties used by this display

   3.  cekeys_atom_name -  pointer to char (INPUT)
                     This is the property name to hang the key definitios off.
                     the default name is: "CeKeys"

   4.  hash_table -  pointer to void (INPUT)
                     This is the hash table pointer for the current display.

   
FUNCTIONS :

   1.  Make sure the atom for the CEKEYCHANGE_ATOM_NAME has been
       identified.  If not, lget it it.

   2.  Convert the property name for the key defintions into the X server atom id.
       This will create the atom if needed.

   3.  Request the property from the X server.  Allow it to be big enough 
       so we read it all at once.  The property is stored agaist the root window.

   4.  If the property is not set, return failure.

   5.  Walk the property and process the inflate (unflatten) the key defintions.

   6.  Put the inflated key defintions in the hash table and the linked list.


RETURNED VALUE:

   rc   -  return code to show keydefs were successfully gotten.
           0  -  got the defintions.
          -1  -  failed to get the definitions.


*************************************************************************/

int  load_server_keydefs(Display     *display,
                         PROPERTIES  *properties,
                         char        *cekeys_atom_name,
                         void        *hash_table)
{
Atom             actual_type;
int              actual_format;
unsigned long    nitems;
unsigned long    bytes_after;
char            *buff = NULL;
char            *buff_pos;
char            *buff_end;
char            *p;
DMC             *dm_list;
ENTRY            inserted_def;
ENTRY           *current_def;
int              rc = 0;
char            *key_key;
#ifdef DebuG
int              keydef_count = 0;
#endif

/***************************************************************
*  
*  Make sure at least keypress and button press events are read.
*  RES  04/13/2020 Made sure key release events are read off the
*  queue so they did not build up.
***************************************************************/

properties->kb_event_mask |= KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask;

/***************************************************************
*  
*  See if the property exists,  If not,  bail out.  Note the
*  True parm to XInternAtom means the property must already exist.
*  
***************************************************************/

DEBUG9(XERRORPOS)
if (properties->cekeys_atom == None)
   properties->cekeys_atom = XInternAtom(display, cekeys_atom_name, True);

if (properties->cekeys_atom == None)
   return(-1);

/***************************************************************
*  
*  Get the string of attributes.
*  
***************************************************************/

DEBUG9(XERRORPOS)
XGetWindowProperty(display,
                   RootWindow(display, DefaultScreen(display)),
                   properties->cekeys_atom,
                   0, /* offset zero from start of property */
                   INT_MAX / 4,
                   False, /* do not delete the property */
                   XA_STRING,
                   &actual_type,
                   &actual_format,
                   &nitems,
                   &bytes_after,
                   (unsigned char **)&buff);

properties->keydef_property_size = nitems * (actual_format / 8);
buff_pos = buff;
buff_end = buff + properties->keydef_property_size;


DEBUG1(
DEBUG9(XERRORPOS)
if (actual_type != None)
   p = XGetAtomName(display, actual_type);
else
   p = "None";
fprintf(stderr, "keydefs:  Actual_type = %d (%s), actual_format = %d, nitems = %d, &bytes_after = %d, mylen = %d\n",
                actual_type, p,
                actual_format, nitems, bytes_after, properties->keydef_property_size);
if (actual_type != None)
   XFree(p);
)

/***************************************************************
*  
*  If there is no data, we have not loaded anything.
*  
***************************************************************/

if (properties->keydef_property_size == 0)
   {
       if (buff)
          XFree(buff);
       return(-1);
   }


/***************************************************************
*  
*  Walk the list and install the key defintions
*  
***************************************************************/

while (buff_pos < buff_end && buff_pos != NULL)
{
   inflate_keydef(&key_key,
                  &dm_list,
                  &buff_pos);

   if (dm_list != NULL)
      {
         DEBUG11(dump_kd(stderr, dm_list, 0 /* print just the current key def */, False, NULL, '@'/* hardcode for debugging */);)

         inserted_def.key  = key_key;

         if ((current_def = (ENTRY *)hsearch(hash_table, inserted_def, FIND)) != NULL)
            {
               free_dmc((DMC *)current_def->data);
               current_def->data = (char *)dm_list;
               free(key_key);
            }
         else
            {
               inserted_def.data = (char *)dm_list;
               if ((current_def = (ENTRY *)hsearch(hash_table, inserted_def, ENTER)) == NULL)
                  rc = -1;
               else
                  {
                     update_keymask(current_def->key, properties);
                     DEBUG1(keydef_count++;)
                  }
            }

      }

}

DEBUG1(fprintf(stderr, "%d Keydefs loaded from server\n", keydef_count);)

if (buff_pos == NULL)
   {
      dm_error("Invalid server keydef file, loading keydefs from file", DM_ERROR_BEEP);
      rc = -1;
   }

XFree(buff);

return(rc);

} /* end of load_server_keydefs */


/************************************************************************

NAME:      change_keydef - Update the keydef property to show a keydef change.
                           or addition.


PURPOSE:    This routine is called when a key defintion is replaced or added.  It 
            puts the keydef change in the property to notify other copies of
            Ce that a keydef has changed.  The updated definition is appended
            to the current definition.  This routine is not used during initial
            keydef loading.

PARAMETERS:

   1.  ent         -  pointer to ENTRY  (INPUT)
                      The ENTRY (key and data) are passed to this routine.

   2.  display         - pointer to Display (INPUT)
                         This is the display pointer returned by XOpenDisplay.
                         It is needed for all Xlib calls.

   3.  cekeys_atom_name -  pointer to char (INPUT)
                     This is the property name to hang the key definitios off.
                     the default name is: "CeKeys"

   4.  properties -  pointer to PROPERTIES (INPUT/OUTPUT) (in buffer.h)
                     This is the list of properties used by this display

FUNCTIONS :

   1.  Flatten the new entry.

   1.  Append the new keydef to the CEKEYS property.



*************************************************************************/

void change_keydef(ENTRY             *ent,
                   Display           *display,
                   char              *cekeys_atom_name,
                   PROPERTIES        *properties)
{

int              size_estimate;
int              keydef_size;
char            *buff;
char            *buff_pos;

/***************************************************************
*  
*  Get the estimate for the space required to save the keydefs
*  and malloc that much space.
*  
***************************************************************/

size_estimate = size_keydef(ent->key, (DMC *)ent->data);

buff = CE_MALLOC(size_estimate);
if (buff == NULL)
   return;

/***************************************************************
*  
*  Walk the keydef list and fill in the buffer.
*  
***************************************************************/

buff_pos = buff;

flatten_keydef(ent->key, (DMC *)ent->data, &buff_pos);
update_keymask(ent->key, properties);

keydef_size = buff_pos - buff;

if (keydef_size <= 0)
   {
      DEBUG4(fprintf(stderr, "Zero size keydef\n");)
      return;
   }


/***************************************************************
*  
*  Save the key definition
*  
***************************************************************/

DEBUG9(XERRORPOS)
if (properties->cekeys_atom == None)
   properties->cekeys_atom = XInternAtom(display, cekeys_atom_name, False);

DEBUG9(XERRORPOS)
XChangeProperty(display,
                RootWindow(display, DefaultScreen(display)),
                properties->cekeys_atom,
                XA_STRING,
                8, /* 8 bit quantities, we handle littleendin ourselves */
                PropModeAppend,
                (unsigned char *)buff, 
                keydef_size);

properties->keydef_property_size += keydef_size;
properties->expecting_change++;

DEBUG4(fprintf(stderr, "change_keydef:  added %d bytes, new len %d\n", keydef_size, properties->keydef_property_size);)

free(buff);

} /* end of change_keydef */



/************************************************************************

NAME:      get_changed_keydef   - Handle an update to the key defintions
                                  detected by a keydef property change.


PURPOSE:    This routine is the receive end of change_keydef.  It gets called when
            the X property notify event occurs which signals someone changed a keydef.

PARAMETERS:

   1.   event      - pointer to XEvent (INPUT) 
                     This the property change event which triggered this call.

   2.  properties -  pointer to PROPERTIES (INPUT/OUTPUT) (in buffer.h)
                     This is the list of properties used by this display

   3.   hash_table - pointer to void (INPUT)
                     This is the hash table pointer for the current display.

FUNCTIONS :

   1.  Read the property off the root window.

   2.  Using the current size static variable, skip past all the definitions we already
       know about.

   2.  Unflattenen the new key defintion(s).

   3.  Install the key defintion in the hash table and update the
       linked list if required.

*************************************************************************/

void  get_changed_keydef(XEvent     *event,
                         PROPERTIES *properties,
                         void       *hash_table)
{
Atom             actual_type;
int              actual_format;
unsigned long    nitems;
unsigned long    bytes_after;
char            *buff = NULL;
char            *buff_pos;
char            *buff_end;
char            *keykey;
char            *p;
DMC             *dm_list;
ENTRY            inserted_def;
ENTRY           *current_def;
int              keydef_size;
int              property_offset;
#ifdef DebuG
int              keydef_count = 0;
#endif

/***************************************************************
*  
*  Check to make sure we have been called with the correct atom.
*  
***************************************************************/

if ((properties->cekeys_atom == None) || (event->xproperty.atom != properties->cekeys_atom))
   return;

/***************************************************************
*  
*  Check if this is an expected change initiated by us and 
*  forget it if it is.
*  
***************************************************************/

if (properties->expecting_change)
   {
      properties->expecting_change--;
      return;
   }

/***************************************************************
*  
*  Get the size of the property first.  This may seem to be
*  a waste, but in the case of cmdf commands with kd's in them
*  it saves time.
*  
***************************************************************/

DEBUG9(XERRORPOS)
XGetWindowProperty(event->xproperty.display,
                   RootWindow(event->xproperty.display, DefaultScreen(event->xproperty.display)),
                   properties->cekeys_atom,
                   0, /* read the whole property, it could get smaller */
                   0,
                   False, /* do not delete the property */
                   XA_STRING,
                   &actual_type,
                   &actual_format,
                   &nitems,
                   &bytes_after,
                   (unsigned char **)&buff);

if (bytes_after == (unsigned)properties->keydef_property_size)
   {
      if (buff)
         XFree(buff);
      return;
   }


/***************************************************************
*  
*  Get the string of the definition.
*  We want to just get the stuff we don't have, so we use the
*  offset parm to skip the stuff we do have.  However, if the
*  update is length zero, the server gets mad at us so we will
*  decrement the offset by a halfword and always get something.
*  We will skip that ourselves.
*  
***************************************************************/

DEBUG9(XERRORPOS)
XGetWindowProperty(event->xproperty.display,
                   RootWindow(event->xproperty.display, DefaultScreen(event->xproperty.display)),
                   properties->cekeys_atom,
                   0, /* read the whole property, it could get smaller */
                   INT_MAX / 4,
                   False, /* do not delete the property */
                   XA_STRING,
                   &actual_type,
                   &actual_format,
                   &nitems,
                   &bytes_after,
                   (unsigned char **)&buff);

keydef_size = nitems * (actual_format / 8);
buff_pos = buff;
buff_end = buff + keydef_size;


DEBUG11(
DEBUG9(XERRORPOS)
if (actual_type != None)
   p = XGetAtomName(event->xproperty.display, actual_type);
else
   p = "None";
fprintf(stderr, "get_changed_keydef:  Actual_type = %d (%s), actual_format = %d, nitems = %d, &bytes_after = %d, mylen = %d\n",
                actual_type, p,
                actual_format, nitems, bytes_after, properties->keydef_property_size);
if (actual_type != None)
   XFree(p);
)

/***************************************************************
*  
*  Compare the size we got to the amount of space we know about.
*  There are four possiblities.
*  1.  The size is zero because someone deleted the property
*      -  zero our current size
*  2.  The property got bigger because someone did a key definition.
*      -  Process the new data
*  3.  The size is the one we know about because we did the kd
*      -  Do nothing
*  4.  The size is smaller because it was deleted and new stuff added
*      -  process all the definitions and reset the size.
*
***************************************************************/

property_offset = keydef_size - properties->keydef_property_size;
DEBUG11(fprintf(stderr, "get_changed_keydef: new size %d, old size %d, offset %d\n", keydef_size, properties->keydef_property_size, property_offset);)

if (keydef_size == 0)
   {
      if (buff)
         XFree(buff);
      properties->keydef_property_size = 0;
      return;
   }

if (property_offset >= 0)
   {
      /* if property_offset is zero, we just move to the end of the buffer and fall through */
      buff_pos += properties->keydef_property_size;
   }
else
   {
      /* property got smaller, do nothing */
      buff_pos = buff_end;
   }

properties->keydef_property_size = keydef_size;

/***************************************************************
*  
*  Walk the list and install the key defintions
*  
***************************************************************/

while (buff_pos < buff_end && buff_pos != NULL)
{
   inflate_keydef(&keykey,
                  &dm_list,
                  &buff_pos);

   if (dm_list != NULL)
      {
         DEBUG11(dump_kd(stderr, dm_list, 0 /* print just the current key def */, False, NULL, '@'/* hardcode for debuggin */);)

         inserted_def.key  = keykey;

         if ((current_def = (ENTRY *)hsearch(hash_table, inserted_def, FIND)) != NULL)
            {
               free_dmc((DMC *)current_def->data);
               current_def->data = (char *)dm_list;
               DEBUG11(keydef_count++;)
               free(keykey);
            }
         else
            {
               inserted_def.data = (char *)dm_list;
               if ((current_def = (ENTRY *)hsearch(hash_table, inserted_def, ENTER)) != NULL)
                  {
                     DEBUG11(keydef_count++;)
                     update_keymask(current_def->key, properties);
                  }
            }

      }

}

DEBUG11(fprintf(stderr, "%d Keydefs loaded from server\n", keydef_count);)

if (buff == NULL)
   dm_error("Invalid server keydef gotten from server", DM_ERROR_BEEP);
    
if (buff)
   XFree(buff);

return;

}  /* end of get_changed_keydef */


/************************************************************************

NAME:      update_keymask  - Update the keymask field in the properties area if needed


PURPOSE:    This routine updates the keymask field in the properties global area
            attached to the display description.  This is used to set the
            event masks.  This field is set here and looked at in getevent.c

PARAMETERS:

   1.   key      -   pointer to char (INPUT)
                     This is the hash table keykey to be examined

   2.  properties      - pointer to PROPERTIES (INPUT/OUTPUT) (in buffer.h)
                         This is the list of properties used by this display


GLOBAL DATA:
   This data is global outside serverdef.c, it is contained in the display description

   1.  kb_event_mask  -  int (declared in kd.h)
                   This mask is maintained as key defintions are processed.
                   There are 4 keyboard type events which CE is interested in.
                   KeyPress, KeyRelease, ButtonPress, or ButtonRelease.
                   We always start out assuming we will need to process KeyPress
                   and ButtonPress events.  However, if there are no button up
                   (eg. m2u) definitions or no keypress up (eg. F1u) definitions,
                   there is no need to request these type events from the X server.
                   This mask keeps track of what is required for the event processing
                   routines.
   
FUNCTIONS :

   1.  Or in the appropriate mask based on the event for the passed keykey.

*************************************************************************/

static void update_keymask(char        *key,
                           PROPERTIES  *properties)
{
KeySym           keysym;
int              state;
int              event;

parse_hashtable_key(key, "keys", &keysym, &state, &event);
if (keysym != NoSymbol)  /* message already output on failure */
   {
      if (event == KeyPress)
         properties->kb_event_mask |= KeyPressMask;
      else
         if (event == ButtonPress)
            properties->kb_event_mask |= ButtonPressMask;
         else
            if (event == KeyRelease)
               properties->kb_event_mask |= KeyReleaseMask;
            else
               if (event == ButtonRelease)
                  properties->kb_event_mask |= ButtonReleaseMask;
   }

} /* end of update_keymask */


/************************************************************************

NAME:      flatten_keydef       - convert a keydef into flattened format


PURPOSE:    This routine flattens a list of one or more DMC structures along
            with a hash table keykey into a flattened area.

PARAMETERS:

   1. keykey       -  pointer to char  (INPUT)
                      This the key has table key for the key definition.

   2. dm_list      -  pointer to DMC  (INPUT)
                      This is a list of DMC structure to process.

   3. buff         -  pointer to pointer to char  (INPUT / OUTPUT)
                      The value passed in contains a pointer to the
                      current position in the output buffer to start
                      writing.  The position to resume writing is replaced
                      in this parameter.

FUNCTIONS :

   1.  Save space in the front of the buffer for the total key definition length.

   2.  put the key in the buffer and advance the pointer.

   3.  Flatten each dmc and put it in the buffer.
 
   4.  Update the length field and return the pointer to the location to continue writing.


*************************************************************************/

static void flatten_keydef(char   *keykey,    /* input  */
                           DMC    *dm_list,   /* input  */
                           char  **buff)      /* output / output (writes to the target and updates the pointer) */
{

short int    *def_len;
char         *var_part_start;
short int     sh;
DMC          *dmc;
#ifdef DebuG
static int    count = 0;
#endif

def_len = (short int *)*buff;
SHORT_BUMP(*buff);
var_part_start = *buff;

DEBUG11(fprintf(stderr, "Flatten Def %3d:  ", count++);)

strcpy(*buff, keykey);
*buff += strlen(keykey) + 1;
*buff = (char *)ROUND_SHORT(*buff, var_part_start);

for (dmc = dm_list; dmc != NULL; dmc = dmc->next)
    flatten_dmc(dmc, buff);

sh = *buff - var_part_start;
*def_len = htons(sh);

DEBUG11(fprintf(stderr, "  Keydef size %d\n", (*buff) - ((char *)def_len));)


} /* end of flatten_keydef */


/*****************************************************************************

NAME:       size_keydef    - get the length required for a key def if it was flattened.


PURPOSE:    This routine determines the flattened length of one or more DM command 
            structures along with a hash table keykey into a flattened area.

PARAMETERS:

   1. keykey       -  pointer to char  (INPUT)
                      This the key has table key for the key definition.

   2. dm_list      -  pointer to DMC  (INPUT)
                      This is a list of DMC structure to process.

FUNCTIONS :

   1.  Save space in the front of the buffer for the total key definition length.

   2.  put the key in the buffer and advance the pointer.

   3.  Flatten each dmc and put it in the buffer.
 
   4.  Update the length field and return the pointer to the location to continue writing.

RETURNED VALUE:

   len  -  length in bytes.


*************************************************************************/

static int size_keydef(char   *keykey,        /* input  */
                       DMC    *dm_list)       /* input  */
{
DMC          *dmc;
int           flat_len;
#ifdef DebuG
static int    count = 0;
#endif

DEBUG11(fprintf(stderr, "Size    Def %3d:  ", count++);)

/* key length + 2 byte length + null at end of keykey */
flat_len = ROUND_SHORT((int)strlen(keykey)+1, 0) + sizeof(short);

for (dmc = dm_list; dmc != NULL; dmc = dmc->next)
   flat_len += size_dmc(dmc);

DEBUG11(fprintf(stderr, "  Keydef est size %d\n", flat_len);)

return(flat_len);

}  /* end of size_keydef */


/*****************************************************************************

NAME:       inflate_keydef       - convert a flatted keydef into its normal format.


PURPOSE:    This routine takes a key definition and hash table keykey generated
            by flatten_keydef and returns it to its original size and shape.

PARAMETERS:

   1. keykey       -  pointer to pointer to char  (OUTPUT)
                      Into this parameter is placed the hash table key extracted
                      from the flattened defintion.  This space is malloced.

   2. dm_list      -  pointer to pointer to DMC  (OUTPUT)
                      Into this parameter is placed the list of DMC structures generated.
                      This space is malloced.

   3. buff         -  pointer to pointer to char  (INPUT / OUTPUT)
                      The value passed in contains a pointer to the
                      current position in the output buffer to start
                      reading.  The position to resume reading is replaced
                      in this parameter.

FUNCTIONS :

   1.  Save space in the front of the buffer for the total key definition length.

   2.  put the key in the buffer and advance the pointer.

   3.  Flatten each dmc and put it in the buffer.
 
   4.  Update the length field and return the pointer to the location to continue writing.

RETURNED VALUE:

   len  -  length in bytes.


*************************************************************************/

static void inflate_keydef(char  **keykey,    /* output */
                           DMC   **dm_list,   /* output */
                           char  **buff)      /* input  */
{

short int    *def_len;
char         *buff_pos;
char         *buff_end;
char         *var_part_start;
DMC          *dmc;
DMC          *dmc_head = NULL;
DMC          *dmc_tail = NULL;
#ifdef DebuG
static int    count = 0;
#endif


DEBUG11(fprintf(stderr, "Inflate Def %3d:  ", count++);)

*dm_list = NULL;

def_len = (short int *)*buff;
SHORT_BUMP(*buff);
buff_pos = *buff;
if ((short int)ntohs(*def_len) <= 0)
   {
      dm_error("Zero length server key definition found, aborting reading of server definitions", DM_ERROR_BEEP);
      *buff = NULL;
      return;
   }
buff_end = buff_pos + (short int)ntohs(*def_len);
var_part_start = buff_pos;


*keykey = CE_MALLOC(KEY_KEY_SIZE);
if (*keykey == NULL)
      return;

strcpy(*keykey, buff_pos);
buff_pos += strlen(buff_pos) + 1;
buff_pos = (char *)ROUND_SHORT(buff_pos, var_part_start);

while(buff_pos < buff_end)
{
   dmc = inflate_dmc(&buff_pos);
   if (dmc != NULL)
      {
         if (dmc_head == NULL)
            dmc_head = dmc;
         else
            dmc_tail->next = dmc;
         dmc_tail = dmc;
      }
   else
      {
         DEBUG11(fprintf(stderr, "Bad flattened key definition passed to inflate_keydef\n");)
         *buff = NULL; /* kill any future calls */
         break;
      }
}

/* 8/8/95 RES, handle multiple prompts in X server def */
if ((dmc_head->any.cmd == DM_prompt) && (dmc_head->next != NULL))
   prompt_target_merge(dmc_head); /* in prompt.c */

DEBUG11(fprintf(stderr, "  Keydef size %d\n", buff_end-(*buff)) ;)

*buff = buff_end;
*dm_list = dmc_head;



}  /* end of inflate_keydef */


/************************************************************************

NAME:      flatten_dmc - convert one DMC structure into flattened format


PURPOSE:    This routine flattens one or more DMC structures

PARAMETERS:

   1. dmc          -  pointer to DMC  (INPUT)
                      This is the DM command structure to process.

   3. buff         -  pointer to pointer to char  (INPUT / OUTPUT)
                      The value passed in contains a pointer to the
                      current position in the output buffer to start
                      writing.  The position to resume writing is replaced
                      in this parameter.

FUNCTIONS :

   1.  Save space in the front of the buffer for the flattened DMC length.

   2.  Flatten the DMC using a switch to do the formatting.
 
   3.  Update the length field and return the pointer to the location to continue writing.


*************************************************************************/


static void flatten_dmc(DMC    *dmc,       /* input  */
                        char  **buff)      /* output / output (writes to the target and updates the pointer) */

{
short int    *def_len;
char         *var_part_start;
short int    *s_i;
int           i;
short int     sh;


/***************************************************************
*  
*  First put in the fields for a dmc_any, these are common
*  to all DMC structures.
*  next  is reduced to a short and and set to the length of the 
*        the definition not counting the length field rounded
*        up to a 2 byte length.
*  cmdid is reduced to an unsigned char.  This will do as long
*        as we do not exceed 255 different commands.  We are
*        less than half way there with all the DM commands
*        accounted for.
*  temp  is not relvant and is ommitted.
*  
*  def_len is the length of the flattened DMC structure not
*          counting itself.
***************************************************************/

def_len = (short int *)*buff;
SHORT_BUMP(*buff);
var_part_start = *buff;

**buff = (unsigned char)dmc->any.cmd;
(*buff)++;

/***************************************************************
*  
*  Switch on the cmd id and process the fields of the different types.
*  
***************************************************************/

switch(dmc->any.cmd)
{

/***************************************************************
*  
*  Format of flattened find and rfind:
*
*  +-----+-----+--------------------+--+
*  | len | cmd |  find str          |\0|
*  +-----+-----+--------------------+--+
*     2     1    var
*  
***************************************************************/

case DM_find:
case DM_rfind:
   if (dmc->find.buff)
      {
         strcpy(*buff, dmc->find.buff);
         *buff += strlen(*buff) + 1;
      }
   else
      NULL_STRING(*buff);
   break;

/***************************************************************
*  
*  Format of flattened markc and corner({)
*
*  +-----+-----+-----+-----+-----+-----+
*  | len | cmd | relr| row | col | relc|
*  +-----+-----+-----+-----+-----+-----+
*     2     1     1     2     2     1   
*  
***************************************************************/

case DM_markc:
case DM_corner:
   *((*buff)++) = dmc->markc.row_relative;
   s_i = (short int *)*buff;
   *s_i  = htons((short int)dmc->markc.row);
   SHORT_BUMP(*buff);
   s_i = (short int *)*buff;
   *s_i  = htons((short int)dmc->markc.col);
   SHORT_BUMP(*buff);
   *((*buff)++) = dmc->markc.col_relative;
   break;

/***************************************************************
*  
*  Format of flattened markp
*
*  +-----+-----+-----+-----+-----+
*  | len | cmd | pad |  x  |  y  |
*  +-----+-----+-----+-----+-----+
*     2     1     1     2     2   
*  
***************************************************************/

case DM_markp:
   (*buff)++;  /* padding */
   s_i = (short int *)*buff;
   *s_i  = htons((short int)dmc->markp.x);
   SHORT_BUMP(*buff);
   s_i = (short int *)*buff;
   *s_i  = htons((short int)dmc->markp.y);
   SHORT_BUMP(*buff);
   break;

/***************************************************************
*  
*  Format of flattened num
*
*  +-----+-----+-----+-----+
*  | len | cmd | rel | row |
*  +-----+-----+-----+-----+
*     2     1     1     2
*  
***************************************************************/

case DM_num:
   *((*buff)++) = dmc->num.relative;
   s_i = (short int *)*buff;
   *s_i  = htons((short int)dmc->num.row);
   SHORT_BUMP(*buff);
   break;

/***************************************************************
*  
*  Format of flattened bang
*
*  +-----+-----+-----+-----+-----+--------------+--+----------+--+
*  | len | cmd | -c  | -m  | -e  |  parm        |\0| -s value |\0|
*  +-----+-----+-----+-----+-----+--------------+--+----------+--+
*     2     1     1     1     1    var                var
*  
***************************************************************/

case DM_bang:
   *((*buff)++) = dmc->bang.dash_c;
   *((*buff)++) = dmc->bang.dash_m;
   *((*buff)++) = dmc->bang.dash_e;
   if (dmc->bang.cmdline)
      {
         strcpy(*buff, dmc->bang.cmdline);
         *buff += strlen(*buff) + 1;
      }
   else
      NULL_STRING(*buff);
   if (dmc->bang.dash_s)
      {
         strcpy(*buff, dmc->bang.dash_s);
         *buff += strlen(*buff) + 1;
      }
   else
      NULL_STRING(*buff);
   break;


/***************************************************************
*  
*  Format of flattened bl
*
*  +-----+-----+-----+--------+--+---------+--+
*  | len | cmd | -d  | string |\0| string2 |\0|
*  +-----+-----+-----+--------+--+---------+--+
*     2     1     1     var          var
*  
***************************************************************/

case DM_bl:
   *((*buff)++) = dmc->bl.dash_d;
   if (dmc->bl.l_ident)
      {
         strcpy(*buff, dmc->bl.l_ident);
         *buff += strlen(*buff) + 1;
      }
   else
      NULL_STRING(*buff);
   if (dmc->bl.r_ident)
      {
         strcpy(*buff, dmc->bl.r_ident);
         *buff += strlen(*buff) + 1;
      }
   else
      NULL_STRING(*buff);
   break;

/***************************************************************
*  
*  Format of flattened case
*
*  +-----+-----+-----+
*  | len | cmd |func |
*  +-----+-----+-----+
*     2     1     1   
*  
***************************************************************/

case DM_case:
   *((*buff)++) = dmc->d_case.func;
   break;

/***************************************************************
*  
*  Format of flattened ce cv, cc, ceterm, and cp
*
*  +-----+-----+------+------+--+------+--+------
*  | len | cmd | argc | arg0 |\0| arg1 |\0| arg2...
*  +-----+-----+------+------+--+------+--+-------
*     2     1     1     var
*  
***************************************************************/

case DM_ce:
case DM_cv:
case DM_cc:
case DM_ceterm:
case DM_cp:
   *((*buff)++) = dmc->cv.argc;
   for (i = 0; i < dmc->cv.argc; i++)
   {
      strcpy(*buff, dmc->cv.argv[i]);
      *buff += strlen(*buff) + 1;
   }
   break;


/***************************************************************
*  
*  Format of flattened cmdf and rec
*
*  +-----+-----+-----+------+--+
*  | len | cmd | -p  | path |\0|
*  +-----+-----+-----+------+--+
*     2     1     1     var
*  
***************************************************************/

case DM_cmdf:
case DM_rec:
   *((*buff)++) = dmc->cmdf.dash_p;
   if (dmc->cmdf.path)
      {
         strcpy(*buff, dmc->cmdf.path);
         *buff += strlen(*buff) + 1;
      }
   else
      NULL_STRING(*buff);
   break;


/***************************************************************
*  
*  Format of flattened cpo and cps
*
*  +-----+-----+-----+-----+-----+------+------+--+------+--+------
*  | len | cmd | -w  | -d  | -s  | argc | arg0 |\0| arg1 |\0| arg2...
*  +-----+-----+-----+-----+-----+------+------+--+------+--+-------
*     2     1     1     1     1     1      var
*  
***************************************************************/

case DM_cpo:
case DM_cps:
   *((*buff)++) = dmc->cpo.dash_w;
   *((*buff)++) = dmc->cpo.dash_d;
   *((*buff)++) = dmc->cpo.dash_s;
   *((*buff)++) = dmc->cpo.argc;
   for (i = 0; i < dmc->cpo.argc; i++)
   {
      strcpy(*buff, dmc->cpo.argv[i]);
      *buff += strlen(*buff) + 1;
   }
   break;


/***************************************************************
*  
*  Format of flattened dq
*
*  +-----+-----+-----+-----+-----+-----+------+--+
*  | len | cmd | -s  | -b  | -a  | -c  | pid  |\0|
*  +-----+-----+-----+-----+-----+-----+------+--+
*     2     1     1     1     1     2    var
*  
***************************************************************/

case DM_dq:
   *((*buff)++) = dmc->dq.dash_s;
   *((*buff)++) = dmc->dq.dash_b;
   *((*buff)++) = dmc->dq.dash_a;
   s_i = (short int *)*buff;
   *s_i  = htons((short int)dmc->dq.dash_c);
   SHORT_BUMP(*buff);
   if (dmc->dq.name)
      {
         strcpy(*buff, dmc->dq.name);
         *buff += strlen(*buff) + 1;
      }
   else
      NULL_STRING(*buff);
   break;


 /***************************************************************
*  
*  Format of flattened echo
*
*  +-----+-----+-----+
*  | len | cmd | -r  |
*  +-----+-----+-----+
*     2     1     1   
*  
***************************************************************/

case DM_echo:
   *((*buff)++) = dmc->echo.dash_r;
   break;


 /***************************************************************
*  
*  Format of flattened eef
*
*  +-----+-----+-----+
*  | len | cmd | -a  |
*  +-----+-----+-----+
*     2     1     1   
*  
***************************************************************/

case DM_eef:
   *((*buff)++) = dmc->eef.dash_a;
   break;


/***************************************************************
*  
*  Format of flattened ei and company
*
*  +-----+-----+-----+
*  | len | cmd |func |
*  +-----+-----+-----+
*     2     1     1   
*  
***************************************************************/

case DM_ei:
case DM_inv:
case DM_ro:
case DM_sb:
case DM_sc:
case DM_wa:
case DM_wh:
case DM_wi:
case DM_ws:
case DM_lineno:
case DM_hex:
case DM_vt:
case DM_wp:
case DM_mouse:
case DM_envar:
case DM_pdm:
case DM_caps:
   *((*buff)++) = dmc->ei.mode;
   break;


/***************************************************************
*  
*  Format of flattened er
*
*  +-----+-----+------+
*  | len | cmd | char |
*  +-----+-----+------+
*     2     1     1
*  
***************************************************************/

case DM_er:
   *((*buff)++) = dmc->es.chars[0];
   break;

/***************************************************************
*  
*  Format of flattened es
*
*  +-----+-----+--------------+--+
*  | len | cmd |  es          |\0|
*  +-----+-----+--------------+--+
*     2     1    var
*  
***************************************************************/

case DM_es:
   if (dmc->es.string)
      {
         strcpy(*buff, dmc->es.string);
         *buff += strlen(*buff) + 1;
      }
   else
      NULL_STRING(*buff);
   break;



/***************************************************************
*  
*  Format of flattened geo, debug, etc.
*
*  +-----+-----+--------------+--+
*  | len | cmd |  parm        |\0|
*  +-----+-----+--------------+--+
*     2     1    var
*  
***************************************************************/

case DM_fl:
case DM_geo:
case DM_debug:
case DM_re:
case DM_cd:
case DM_pd:
case DM_env:
case DM_bgc:
case DM_fgc:
case DM_title:
case DM_eval:
case DM_lbl:
case DM_glb:
   if (dmc->geo.parm)
      {
         strcpy(*buff, dmc->geo.parm);
         *buff += strlen(*buff) + 1;
      }
   else
      NULL_STRING(*buff);
   break;


/***************************************************************
*  
*  Format of flattened icon, al, ar, sic, ee
*
*  +-----+-----+-----+-----+
*  | len | cmd | -i  | -w  |
*  +-----+-----+-----+-----+
*     2     1     1     1   
*  
***************************************************************/

case DM_icon:
case DM_al:
case DM_ar:
case DM_sic:
case DM_ee:
   *((*buff)++) = dmc->icon.dash_i;
   *((*buff)++) = dmc->icon.dash_w;
   break;


/***************************************************************
*  
*  Format of flattened kd, ld, alias, mi, and lsf
*
*  +-----+-----+-----+--+-----+--+
*  | len | cmd | key |\0| def |\0|
*  +-----+-----+-----+--+-----+--+
*     2     1    var      var
*  
***************************************************************/

case DM_kd:
case DM_lkd:
case DM_alias:
case DM_mi:
case DM_lmi:
case DM_lsf:
   strcpy(*buff, dmc->kd.key);
   *buff += strlen(*buff) + 1;
   /***************************************************************
   *  Differentiate between a NULL kd pointer (request for display)
   *  and a null key definition (take default), defininition is a
   *  null string.
   ***************************************************************/
   if (dmc->kd.def)
      {
         if (dmc->kd.def[0] != '\0') /* check for null string definition */
            {
               strcpy(*buff, dmc->kd.def);
               *buff += strlen(*buff) + 1;
            }
         else
            {
               strcpy(*buff, " ");  /* a single blank */
               *buff += strlen(*buff) + 1;
            }
      }
   else
      NULL_STRING(*buff);
   break;


/***************************************************************
*  
*  Format of flattened msg, prefix, and sp
*
*  +-----+-----+-----+--+
*  | len | cmd | msg |\0|
*  +-----+-----+-----+--+
*     2     1    var     
*  
***************************************************************/

case DM_msg:
case DM_prefix:
case DM_sp:
   if (dmc->msg.msg)
      {
         strcpy(*buff, dmc->msg.msg);
         *buff += strlen(*buff) + 1;
      }
   else
      NULL_STRING(*buff);
   break;


/***************************************************************
*  
*  Format of flattened ph, pv, bell, and fbdr
*
*  +-----+-----+-----+-------+
*  | len | cmd | pad | chars |
*  +-----+-----+-----+-------+
*     2     1     1     2
*  
***************************************************************/

case DM_ph:
case DM_pv:
case DM_bell:
case DM_fbdr:
   (*buff)++;
   s_i = (short int *)*buff;
   *s_i  = htons((short int)dmc->ph.chars);
   SHORT_BUMP(*buff);
   break;

/***************************************************************
*  
*  Format of flattened pn
*
*  +-----+-----+-----+-----+------+--+
*  | len | cmd | -c  | -r  | path |\0|
*  +-----+-----+-----+-----+------+--+
*     2     1     1     1     var
*  
***************************************************************/

case DM_pn:
   *((*buff)++) = dmc->pn.dash_c;
   *((*buff)++) = dmc->pn.dash_r;
   if (dmc->pn.path)
      {
         strcpy(*buff, dmc->pn.path);
         *buff += strlen(*buff) + 1;
      }
   else
      NULL_STRING(*buff);
   break;


/***************************************************************
*  
*  Format of flattened pp
*
*  +-----+-----+-------+--+
*  | len | cmd | pages |\0|
*  +-----+-----+-------+--+
*     2     1     var
*  pages is in string format.
*  
***************************************************************/

case DM_pp:
case DM_sleep:
   sprintf(*buff, "%f", dmc->pp.amount);
   *buff += strlen(*buff) + 1;
   break;


/***************************************************************
*  
*  Format of flattened prompt.  
*  This creates separate target strings for each prompt.
*
*  +-----+-----+------------+-------+------------+--+------------+--+
*  | len | cmd | tar_offset | quote | prompt_str |\0| target_str |\0|
*  +-----+-----+------------+-------+------------+--+------------+--+
*     2     1        1          1        var          var        
*  
***************************************************************/

case DM_prompt:
   *((*buff)++) = (dmc->prompt.target - dmc->prompt.target_strt);
   *((*buff)++) = dmc->prompt.quote_char;
   if (dmc->prompt.prompt_str)
      {
         strcpy(*buff, dmc->prompt.prompt_str);
         *buff += strlen(*buff) + 1;
      }
   else
      NULL_STRING(*buff);
   if (dmc->prompt.target_strt)
      {
         strcpy(*buff, dmc->prompt.target_strt);
         *buff += strlen(*buff) + 1;
      }
   else
      NULL_STRING(*buff);
   break;


/***************************************************************
*  
*  Format of flattened rw
*
*  +-----+-----+-----+
*  | len | cmd | -r  |
*  +-----+-----+-----+
*     2     1     1   
*  
***************************************************************/

case DM_rw:
   *((*buff)++) = dmc->rw.dash_r;
   break;


/***************************************************************
*  
*  Format of flattened s and so
*
*  +-----+-----+------+--+-------+--+
*  | len | cmd | s1   |\0| s2    |\0|
*  +-----+-----+------+--+-------+--+
*     2     1    var       var
*  
***************************************************************/

case DM_s:
case DM_so:
   if (dmc->s.s1)
      {
         strcpy(*buff, dmc->s.s1);
         *buff += strlen(*buff) + 1;
      }
   else
      NULL_STRING(*buff);
   if (dmc->s.s2)
      {
         strcpy(*buff, dmc->s.s2);
         *buff += strlen(*buff) + 1;
      }
   else
      NULL_STRING(*buff);
   break;

/***************************************************************
*  
*  Format of flattened tf
*
*  +-----+-----+----+----+-----+----+----+
*  | len | cmd | -b | -l | -r  | -c | -p |
*  +-----+-----+----+----+-----+----+----+
*     2     1     1    2    2     1    1  
*
***************************************************************/

case DM_tf:
   *((*buff)++) = dmc->tf.dash_b;
   s_i = (short int *)*buff;
   *s_i  = htons(dmc->tf.dash_l);
   SHORT_BUMP(*buff);
   s_i = (short int *)*buff;
   *s_i  = htons(dmc->tf.dash_r);
   SHORT_BUMP(*buff);
   *((*buff)++) = dmc->tf.dash_c;
   *((*buff)++) = dmc->tf.dash_p;
   break;


/***************************************************************
*  
*  Format of flattened ts
*
*  +-----+-----+-----+----+-----+----+----+-----
*  | len | cmd | pad | -r | cnt | t1 | t2 | t3 ...
*  +-----+-----+-----+----+-----+----+----+------
*     2     1     1     2    2    2    2     2
*
*  cnt == 1 means that just t1 exists,
*  
***************************************************************/

case DM_ts:
   (*buff)++;
   s_i = (short int *)*buff;
   *s_i  = htons((short int)dmc->ts.dash_r);
   SHORT_BUMP(*buff);
   s_i = (short int *)*buff;
   *s_i  = htons((short int)dmc->ts.count);
   SHORT_BUMP(*buff);
   for (i = 0; i < dmc->ts.count; i++)
   {
      s_i = (short int *)*buff;
      *s_i  = htons(dmc->ts.stops[i]);
      SHORT_BUMP(*buff);
   }
   break;


/***************************************************************
*  
*  Format of flattened twb
*
*  +-----+-----+-----+
*  | len | cmd | brd |
*  +-----+-----+-----+
*     2     1     1   
*  
***************************************************************/

case DM_twb:
   *((*buff)++) = dmc->twb.border;
   break;


/***************************************************************
*  
*  Format of flattened wc
*
*  +-----+-----+------+
*  | len | cmd | type |
*  +-----+-----+------+
*     2     1     1   
*  
***************************************************************/

case DM_wc:
case DM_reload:
   *((*buff)++) = dmc->wc.type;
   break;


/***************************************************************
*  
*  Format of flattened wdc, ca, nc, and pc
*
*  +-----+-----+-----+--------+--+--------+--+
*  | len | cmd | num | bgc    |\0| fgc    |\0|
*  +-----+-----+-----+--------+--+--------+--+
*     2     1     1     var          var
*  
***************************************************************/

case DM_wdc:
case DM_ca:
case DM_nc:
case DM_pc:
   *((*buff)++) = dmc->wdc.number;
   if (dmc->wdc.bgc)
      {
         strcpy(*buff, dmc->wdc.bgc);
         *buff += strlen(*buff) + 1;
      }
   else
      NULL_STRING(*buff);
   if (dmc->wdc.fgc)
      {
         strcpy(*buff, dmc->wdc.fgc);
         *buff += strlen(*buff) + 1;
      }
   else
      NULL_STRING(*buff);
   break;


/***************************************************************
*  
*  Format of flattened wdf
*
*  +-----+-----+-----+-------------+--+
*  | len | cmd | num | geometry    |\0|
*  +-----+-----+-----+-------------+--+
*     2     1     1     var
*  
***************************************************************/

case DM_wdf:
   *((*buff)++) = dmc->wdf.number;
   if (dmc->wdf.geometry)
      {
         strcpy(*buff, dmc->wdf.geometry);
         *buff += strlen(*buff) + 1;
      }
   else
      NULL_STRING(*buff);
   break;


/***************************************************************
*  
*  Format of flattened ww
*
*  +-----+-----+-----+------+-----+-----+
*  | len | cmd | -i  | -c   | mode| -a  |
*  +-----+-----+-----+------+-----+-----+
*     2     1     1     2      1     1   
*  
***************************************************************/

case DM_ww:
   *((*buff)++) = dmc->ww.dash_i;

   s_i = (short int *)*buff;
   *s_i  = htons((short int)dmc->ww.dash_c);
   SHORT_BUMP(*buff);

   *((*buff)++) = (char)dmc->ww.mode;
   *((*buff)++) = dmc->ww.dash_a;
   break;


/***************************************************************
*  
*  Format of flattened xa
*
*  +-----+-----+-----+-----+-----+-------+-------+--+
*  | len | cmd | -f1 | -f2 | -l  | path1 | path2 |\0|
*  +-----+-----+-----+-----+-----+-------+-------+--+
*     2     1     1     1     1    var     var
*  
***************************************************************/

case DM_xa:
   *((*buff)++) = dmc->xa.dash_f1;
   *((*buff)++) = dmc->xa.dash_f2;
   *((*buff)++) = dmc->xa.dash_l;
   if (dmc->xa.path1)
      {
         strcpy(*buff, dmc->xa.path1);
         *buff += strlen(*buff) + 1;
      }
   else
      NULL_STRING(*buff);
   if (dmc->xa.path2)
      {
         strcpy(*buff, dmc->xa.path2);
         *buff += strlen(*buff) + 1;
      }
   else
      NULL_STRING(*buff);
   break;


/***************************************************************
*  
*  Format of flattened xc, xd, and xp
*
*  +-----+-----+-----+-----+-----+-----+------+--+
*  | len | cmd | -r  | -f  | -a  | -l  | path |\0|
*  +-----+-----+-----+-----+-----+-----+------+--+
*     2     1     1     1     1     1    var
*  
***************************************************************/

case DM_xc:
case DM_xd:
case DM_xp:
   *((*buff)++) = dmc->xc.dash_r;
   *((*buff)++) = dmc->xc.dash_f;
   *((*buff)++) = dmc->xc.dash_a;
   *((*buff)++) = dmc->xc.dash_l;
   if (dmc->xc.path)
      {
         strcpy(*buff, dmc->xc.path);
         *buff += strlen(*buff) + 1;
      }
   else
      NULL_STRING(*buff);
   break;


/***************************************************************
*  
*  Format of flattened cntlc
*
*  +-----+-----+-----+-----+-----+-----+-----+------+--+
*  | len | cmd | -r  | -f  | -a  | -l  | -h  | path |\0|
*  +-----+-----+-----+-----+-----+-----+-----+------+--+
*     2     1     1     1     1     1     1    var
*  
***************************************************************/

case DM_cntlc:
   *((*buff)++) = dmc->cntlc.dash_r;
   *((*buff)++) = dmc->cntlc.dash_f;
   *((*buff)++) = dmc->cntlc.dash_a;
   *((*buff)++) = dmc->cntlc.dash_l;
   *((*buff)++) = dmc->cntlc.dash_h;
   if (dmc->cntlc.path)
      {
         strcpy(*buff, dmc->cntlc.path);
         *buff += strlen(*buff) + 1;
      }
   else
      NULL_STRING(*buff);
   break;


/***************************************************************
*  
*  Format of flattened xl
*
*  +-----+-----+-----+-----+-----+-----+------+--+------+--+
*  | len | cmd | -r  | -f  | -a  | -l  | path |\0| data |\0|
*  +-----+-----+-----+-----+-----+-----+------+--+------+--+
*     2     1     1     1     1     1    var        var      
*  
***************************************************************/

case DM_xl:
   *((*buff)++) = dmc->xl.base.dash_r;
   *((*buff)++) = dmc->xl.base.dash_f;
   *((*buff)++) = dmc->xl.base.dash_a;
   *((*buff)++) = dmc->xl.base.dash_l;
   if (dmc->xl.base.path)
      {
         strcpy(*buff, dmc->xl.base.path);
         *buff += strlen(*buff) + 1;
      }
   else
      NULL_STRING(*buff);
   if (dmc->xl.data)
      {
         strcpy(*buff, dmc->xl.data);
         *buff += strlen(*buff) + 1;
      }
   else
      NULL_STRING(*buff);
   break;


default:   /* default is for comamands with no parms */
   break;
}  /* end of switch on cmd_id  */


/***************************************************************
*  
*  Make sure that the end position for buff is on a halfword
*  boundary.  This takes care of machines which are sensitive
*  to such things.  
*  
***************************************************************/

*buff = (char *)ROUND_SHORT(*buff, var_part_start);
sh = *buff - var_part_start;
*def_len = htons(sh);

DEBUG11(fprintf(stderr, "Size %s = %d, ", dmsyms[dmc->any.cmd].name,  (*buff) - ((char *)def_len));)

} /* end of flatten_dmc */


/*****************************************************************************

NAME:       size_dmc  - get the length required for one DMC structure if it was flattened


PURPOSE:    This routine determines the flattened length of one DM command 
            structure.

PARAMETERS:

   1. dmc          -  pointer to DMC  (INPUT)
                      This is the DM command structure to process.

FUNCTIONS :

   1.  Use switch to calculate the length occupied by the DMC>

RETURNED VALUE:

   len  -  length in bytes.


*************************************************************************/

static int size_dmc(DMC    *dmc)           /* input  */
{

int           def_len;
char          work[20];
int           i;


/***************************************************************
*  
*  First put in the fields for a dmc_any, these are common
*  to all DMC structures.
*  next  is reduced to a short and and set to the length of the 
*        the definition not counting the length field rounded
*        up to a 2 byte length.
*  cmdid is reduced to an unsigned char.  This will do as long
*        as we do not exceed 255 different commands.  We are
*        less than half way there with all the DM commands
*        accounted for.
*  temp  is not relvant and is ommitted.
*  
*  def_len is the length of the flattened DMC structure not
*          counting itself.
***************************************************************/

def_len = sizeof(short) + 1;  /* 1 is the flattened cmd id */

/***************************************************************
*  
*  See flatten_dmc for the formats of the flattened commands.
*  
***************************************************************/

switch(dmc->any.cmd)
{

/***************************************************************
*  
*  Format of flattened find and rfind:
*
*  +-----+-----+--------------------+--+
*  | len | cmd |  find str          |\0|
*  +-----+-----+--------------------+--+
*     2     1    var
*  
***************************************************************/

case DM_find:
case DM_rfind:
   def_len += (dmc->find.buff ? strlen(dmc->find.buff) : 0) + 1;
   break;

/***************************************************************
*  
*  Format of flattened markc and corner({)
*
*  +-----+-----+-----+-----+-----+-----+
*  | len | cmd | relr| row | col | relc|
*  +-----+-----+-----+-----+-----+-----+
*     2     1     1     2     2     1   
*  
***************************************************************/

case DM_markc:
case DM_corner:
   def_len += (sizeof(short) * 2) + 2;  
   break;

/***************************************************************
*  
*  Format of flattened markp
*
*  +-----+-----+-----+-----+-----+
*  | len | cmd | pad |  x  |  y  |
*  +-----+-----+-----+-----+-----+
*     2     1     1     2     2
*  
***************************************************************/

case DM_markp:
   def_len += (sizeof(short) * 2) + 1;  
   break;

/***************************************************************
*  
*  Format of flattened num
*
*  +-----+-----+-----+-----+
*  | len | cmd | rel | row |
*  +-----+-----+-----+-----+
*     2     1     1     2
*  
***************************************************************/

case DM_num:
   def_len += sizeof(short) + 1;  
   break;

/***************************************************************
*  
*  Format of flattened bang
*
*  +-----+-----+-----+-----+-----+--------------+--+----------+--+
*  | len | cmd | -c  | -m  | -e  |  parm        |\0| -s value |\0|
*  +-----+-----+-----+-----+-----+--------------+--+----------+--+
*     2     1     1     1     1    var                var
*  
***************************************************************/

case DM_bang:
   /*  4 flags, 1 string and 1 '\0' */
   def_len += (dmc->bang.cmdline ? strlen(dmc->bang.cmdline) : 0) +
              (dmc->bang.dash_s ? strlen(dmc->bang.dash_s) : 0) +
              5; /* 3 flags plus 2 null terminators */
   break;

/***************************************************************
*  
*  Format of flattened bl
*
*  +-----+-----+-----+--------+--+---------+--+
*  | len | cmd | -d  | string |\0| string2 |\0|
*  +-----+-----+-----+--------+--+---------+--+
*     2     1     1     var          var
*  
***************************************************************/

case DM_bl:
   /*  1 flag, two strings and two \0 */
   def_len += (dmc->bl.l_ident ? strlen(dmc->bl.l_ident) : 0) + (dmc->bl.r_ident ? strlen(dmc->bl.r_ident) : 0) + 3;
   break;

/***************************************************************
*  
*  Format of flattened case
*
*  +-----+-----+-----+
*  | len | cmd |func |
*  +-----+-----+-----+
*     2     1     1   
*  
***************************************************************/

case DM_case:
   def_len++;
   break;

/***************************************************************
*  
*  Format of flattened ce cv, cc, ceterm, and cp
*
*  +-----+-----+------+------+--+------+--+------
*  | len | cmd | argc | arg0 |\0| arg1 |\0| arg2...
*  +-----+-----+------+------+--+------+--+-------
*     2     1     1     var
*  
***************************************************************/

case DM_ce:
case DM_cv:
case DM_cc:
case DM_ceterm:
case DM_cp:
   /* the argc */
   def_len++;
   for (i = 0; i < dmc->cv.argc; i++)
      def_len += strlen(dmc->cv.argv[i]) + 1;
   break;


/***************************************************************
*  
*  Format of flattened cmdf and rec
*
*  +-----+-----+-----+------+--+
*  | len | cmd | -p  | path |\0|
*  +-----+-----+-----+------+--+
*     2     1     1     var
*  
***************************************************************/

case DM_cmdf:
case DM_rec:
   /*  1 flags, 1 strings and 1 \0 */
   def_len += (dmc->cmdf.path ? strlen(dmc->cmdf.path) : 0) + 2;
   break;



/***************************************************************
*  
*  Format of flattened cpo and cps
*
*  +-----+-----+-----+-----+-----+------+------+--+------+--+------
*  | len | cmd | -w  | -d  | -s  | argc | arg0 |\0| arg1 |\0| arg2...
*  +-----+-----+-----+-----+-----+------+------+--+------+--+-------
*     2     1     1     1     1     1      var
*  
***************************************************************/

case DM_cpo:
case DM_cps:
   /* 3 flag, the argc */
   def_len += 4;
   for (i = 0; i < dmc->cp.argc; i++)
      def_len += strlen(dmc->cp.argv[i]) + 1;
   break;



/***************************************************************
*  
*  Format of flattened dq
*
*  +-----+-----+-----+-----+-----+-----+------+--+
*  | len | cmd | -s  | -b  | -a  | -c  | pid  |\0|
*  +-----+-----+-----+-----+-----+-----+------+--+
*     2     1     1     1     1     2    var
*  
***************************************************************/

case DM_dq:
   def_len += (dmc->dq.name ? strlen(dmc->dq.name) : 0) + 6;
   break;


/***************************************************************
*  
*  Format of flattened echo
*
*  +-----+-----+-----+
*  | len | cmd | -r  |
*  +-----+-----+-----+
*     2     1     1   
*  
***************************************************************/

case DM_echo:
   def_len++;
   break;


/***************************************************************
*  
*  Format of flattened eef
*
*  +-----+-----+-----+
*  | len | cmd | -a  |
*  +-----+-----+-----+
*     2     1     1   
*  
***************************************************************/

case DM_eef:
   def_len++;
   break;


/***************************************************************
*  
*  Format of flattened ei and company
*
*  +-----+-----+-----+
*  | len | cmd |func |
*  +-----+-----+-----+
*     2     1     1   
*  
***************************************************************/

case DM_ei:
case DM_inv:
case DM_ro:
case DM_sb:
case DM_sc:
case DM_wa:
case DM_wh:
case DM_wi:
case DM_ws:
case DM_lineno:
case DM_hex:
case DM_vt:
case DM_wp:
case DM_mouse:
case DM_envar:
case DM_pdm:
case DM_caps:
   def_len++;
   break;


/***************************************************************
*  
*  Format of flattened er
*
*  +-----+-----+------+
*  | len | cmd | char |
*  +-----+-----+------+
*     2     1     1
*  
***************************************************************/

case DM_er:
   def_len++;
   break;

/***************************************************************
*  
*  Format of flattened es
*
*  +-----+-----+--------------+--+
*  | len | cmd |  es          |\0|
*  +-----+-----+--------------+--+
*     2     1    var
*  
***************************************************************/

case DM_es:
   def_len += (dmc->es.string ? strlen(dmc->es.string) : 0) + 1;
   break;



/***************************************************************
*  
*  Format of flattened geo, debug, etc.
*
*  +-----+-----+--------------+--+
*  | len | cmd |  parm        |\0|
*  +-----+-----+--------------+--+
*     2     1    var
*  
***************************************************************/

case DM_fl:
case DM_geo:
case DM_debug:
case DM_re:
case DM_cd:
case DM_pd:
case DM_env:
case DM_bgc:
case DM_fgc:
case DM_title:
case DM_eval:
case DM_lbl:
case DM_glb:
   def_len += (dmc->geo.parm ? strlen(dmc->geo.parm) : 0) + 1;
   break;


/***************************************************************
*  
*  Format of flattened icon, al ar, sic, ee
*
*  +-----+-----+-----+-----+
*  | len | cmd | -i  | -w  |
*  +-----+-----+-----+-----+
*     2     1     1     1   
*  
***************************************************************/

case DM_icon:
case DM_al:
case DM_ar:
case DM_sic:
case DM_ee:
   def_len += 2;
   break;


/***************************************************************
*  
*  Format of flattened kd, ld, alias, mi, and lsf
*
*  +-----+-----+-----+--+-----+--+
*  | len | cmd | key |\0| def |\0|
*  +-----+-----+-----+--+-----+--+
*     2     1    var      var
*  
***************************************************************/

case DM_kd:
case DM_lkd:
case DM_alias:
case DM_mi:
case DM_lmi:
case DM_lsf:
   /* two strings and two \0's */
   def_len += (dmc->kd.key ? strlen(dmc->kd.key) : 0);
   if (dmc->kd.def == NULL)
      def_len += 2;
   else
      if (dmc->kd.def[0] == '\0')
         def_len += 3;
      else
         def_len += strlen(dmc->kd.def) + 2;
   break;


/***************************************************************
*  
*  Format of flattened msg, prefix, and sp
*
*  +-----+-----+-----+--+
*  | len | cmd | msg |\0|
*  +-----+-----+-----+--+
*     2     1    var     
*  
***************************************************************/

case DM_msg:
case DM_prefix:
case DM_sp:
   def_len += (dmc->msg.msg ? strlen(dmc->msg.msg) : 0) + 1;
   break;


/***************************************************************
*  
*  Format of flattened ph, pv, bell, and fbdr
*
*  +-----+-----+-----+-------+
*  | len | cmd | pad | chars |
*  +-----+-----+-----+-------+
*     2     1     1     2
*  
***************************************************************/

case DM_ph:
case DM_pv:
case DM_bell:
case DM_fbdr:
   def_len += sizeof(short) + 1;
   break;

/***************************************************************
*  
*  Format of flattened pn
*
*  +-----+-----+-----+-----+------+--+
*  | len | cmd | -c  | -r  | path |\0|
*  +-----+-----+-----+-----+------+--+
*     2     1     1     1     var
*  
***************************************************************/

case DM_pn:
   def_len += (dmc->pn.path ? strlen(dmc->pn.path) : 0) + 3;
   break;


/***************************************************************
*  
*  Format of flattened pp
*
*  +-----+-----+-------+--+
*  | len | cmd | pages |\0|
*  +-----+-----+-------+--+
*     2     1     var
*  pages is in string format.
*  
***************************************************************/

case DM_pp:
case DM_sleep:
   sprintf(work, "%f", dmc->pp.amount);
   def_len += (work ? strlen(work) : 0) + 1;
   break;


/***************************************************************
*  
*  Format of flattened prompt.  
*  This creates separate target strings for each prompt.
*
*  +-----+-----+------------+-------+------------+--+------------+--+
*  | len | cmd | tar_offset | quote | prompt_str |\0| target_str |\0|
*  +-----+-----+------------+-------+------------+--+------------+--+
*     2     1        1          1        var          var        
*  
***************************************************************/

case DM_prompt:
   def_len += (dmc->prompt.prompt_str ? strlen(dmc->prompt.prompt_str) : 0) +
              (dmc->prompt.target_strt ? strlen(dmc->prompt.target_strt) : 0) + 4;
   /* +4 => One for each of 2 nulls, 1 for target_offset, 1 for quote */
   break;


/***************************************************************
*  
*  Format of flattened rw
*
*  +-----+-----+-----+
*  | len | cmd | -r  |
*  +-----+-----+-----+
*     2     1     1   
*  
***************************************************************/

case DM_rw:
   def_len++;
   break;


/***************************************************************
*  
*  Format of flattened s and so
*
*  +-----+-----+------+--+-------+--+
*  | len | cmd | s1   |\0| s2    |\0|
*  +-----+-----+------+--+-------+--+
*     2     1    var       var
*  
***************************************************************/

case DM_s:
case DM_so:
   def_len += (dmc->s.s1 ? strlen(dmc->s.s1) : 0) + (dmc->s.s2 ? strlen(dmc->s.s2) : 0) + 2;
   break;


/***************************************************************
*  
*  Format of flattened tf
*
*  +-----+-----+----+----+-----+----+----+
*  | len | cmd | -b | -l | -r  | -c | -p |
*  +-----+-----+----+----+-----+----+----+
*     2     1     1    2    2     1    1  
*
***************************************************************/

case DM_tf:
   /* 1 for -b two each for -l and -r  and 1 for -c and 1 for -p */
   def_len += (2 * sizeof(short)) + 3;
   break;


/***************************************************************
*  
*  Format of flattened ts
*
*  +-----+-----+-----+----+-----+----+----+-----
*  | len | cmd | pad | -r | cnt | t1 | t2 | t3 ...
*  +-----+-----+-----+----+-----+----+----+------
*     2     1     1     2    2    2    2     2
*
*  cnt == 1 means that just t1 exists,
*  
***************************************************************/

case DM_ts:
   /* 1 pad, 2 -r, 2 cnt, plus 2*cnt */
   def_len += ((dmc->ts.count + 2) * sizeof(short)) + 1;
   break;


/***************************************************************
*  
*  Format of flattened twb
*
*  +-----+-----+-----+
*  | len | cmd | brd |
*  +-----+-----+-----+
*     2     1     1   
*  
***************************************************************/

case DM_twb:
   def_len++;
   break;


/***************************************************************
*  
*  Format of flattened wc
*
*  +-----+-----+------+
*  | len | cmd | type |
*  +-----+-----+------+
*     2     1     1   
*  
***************************************************************/

case DM_wc:
case DM_reload:
   def_len++;
   break;


/***************************************************************
*  
*  Format of flattened wdc, ca, nc, and pc
*
*  +-----+-----+-----+--------+--+--------+--+
*  | len | cmd | num | bgc    |\0| fgc    |\0|
*  +-----+-----+-----+--------+--+--------+--+
*     2     1     1     var          var
*  
***************************************************************/

case DM_wdc:
case DM_ca:
case DM_nc:
case DM_pc:
   def_len += (dmc->wdc.bgc ? (int)strlen(dmc->wdc.bgc) : 0) + (dmc->wdc.fgc ? (int)strlen(dmc->wdc.fgc) : 0) + 3;
   break;


/***************************************************************
*  
*  Format of flattened wdf
*
*  +-----+-----+-----+-------------+--+
*  | len | cmd | num | geometry    |\0|
*  +-----+-----+-----+-------------+--+
*     2     1     1     var
*  
***************************************************************/

case DM_wdf:
   def_len += (dmc->wdf.geometry ? (int)strlen(dmc->wdf.geometry) : 0) + 2;
   break;


/***************************************************************
*  
*  Format of flattened ww
*
*  +-----+-----+-----+------+-----+-----+
*  | len | cmd | -i  | -c   | mode| -a  |
*  +-----+-----+-----+------+-----+-----+
*     2     1     1     2      1     1   
*  
***************************************************************/

case DM_ww:
   /* -c is 2 bytes, plus three flags */
   def_len += sizeof(short) + 3;
   break;


/***************************************************************
*  
*  Format of flattened xa
*
*  +-----+-----+-----+-----+-----+-------+-------+--+
*  | len | cmd | -f1 | -f2 | -l  | path1 | path2 |\0|
*  +-----+-----+-----+-----+-----+-------+-------+--+
*     2     1     1     1     1    var     var
*  
***************************************************************/

case DM_xa:
   def_len += (dmc->xa.path1 ? strlen(dmc->xa.path1) : 0) +
              (dmc->xa.path2 ? strlen(dmc->xa.path2) : 0) + 4;
   break;


/***************************************************************
*  
*  Format of flattened xc, xd, and xp
*
*  +-----+-----+-----+-----+-----+-----+------+--+
*  | len | cmd | -r  | -f  | -a  | -l  | path |\0|
*  +-----+-----+-----+-----+-----+-----+------+--+
*     2     1     1     1     1     1    var
*  
***************************************************************/

case DM_xc:
case DM_xd:
case DM_xp:
   def_len += (dmc->xc.path ? strlen(dmc->xc.path) : 0) + 5;
   break;


/***************************************************************
*  
*  Format of flattened cntlc
*
*  +-----+-----+-----+-----+-----+-----+-----+------+--+
*  | len | cmd | -r  | -f  | -a  | -l  | -h  | path |\0|
*  +-----+-----+-----+-----+-----+-----+-----+------+--+
*     2     1     1     1     1     1     1    var
*  
***************************************************************/

case DM_cntlc:
   def_len += (dmc->cntlc.path ? strlen(dmc->cntlc.path) : 0) + 6;
   break;


/***************************************************************
*  
*  Format of flattened xl
*
*  +-----+-----+-----+-----+-----+-----+------+--+------+--+
*  | len | cmd | -r  | -f  | -a  | -l  | path |\0| data |\0|
*  +-----+-----+-----+-----+-----+-----+------+--+------+--+
*     2     1     1     1     1     1    var        var      
*  
***************************************************************/

case DM_xl:
   def_len += (dmc->xl.base.path ? strlen(dmc->xl.base.path) : 0) + 6 +
              (dmc->xl.data ? strlen(dmc->xl.data) : 0);
              /* 6 , one each for -r, -f, -a, -l, and one for each null */
   break;


default:   /* default is for comamands with no parms */
   break;
}  /* end of switch on cmd_id  */


/***************************************************************
*  
*  Make sure that the end position for buff is on a halfword
*  boundary.  This takes care of machines which are sensitive
*  to such things.  
*  
***************************************************************/

def_len = ROUND_SHORT(def_len, 0);

DEBUG11(fprintf(stderr, "Est %s = %d, ", dmsyms[dmc->any.cmd].name,  def_len);)

return(def_len);

}  /* end of size_dmc */


/*****************************************************************************

NAME:       inflate_dmc          - convert a flatted DMC into its normal


PURPOSE:    This routine takes a key definition and hash table keykey generated
            by flatten_keydef and returns it to its original size and shape.

PARAMETERS:

   2. dm_list      -  pointer to pointer to DMC  (OUTPUT)
                      Into this parameter is placed the list of DM command structure generated.
                      This space is malloced.

   3. buff         -  pointer to pointer to char  (INPUT / OUTPUT)
                      The value passed in contains a pointer to the
                      current position in the output buffer to start
                      reading.  The position to resume reading is replaced
                      in this parameter.

FUNCTIONS :

   1.  malloc the basic command structure. and set the fixed information.

   2.  switch on the command id and fill in the rest of the information.

   3.  Update the buffer pointer and return the malloced structure.

RETURNED VALUE:

   dmc  -  the dm command structure is returned.


*************************************************************************/

static DMC *inflate_dmc(char   **buff)      /* input  */
{
short int    *def_len;
char         *var_part_start;
short int    *s_i;
DMC          *dmc;
int           i;
int           bad = False;


/***************************************************************
*  
*  First put in the fields for a dmc_any, these are common
*  to all DMC structures.
*  next  is reduced to a short and and set to the length of the 
*        the definition not counting the length field rounded
*        up to a 2 byte length.
*  cmdid is reduced to an unsigned char.  This will do as long
*        as we do not exceed 255 different commands.  We are
*        less than half way there with all the DM commands
*        accounted for.
*  temp  is not relvant and is ommitted.
*  
*  def_len is the length of the flattened DMC structure not
*          counting itself.
***************************************************************/

def_len = (short int *)*buff;
SHORT_BUMP(*buff);
var_part_start = *buff;

dmc = (DMC *)CE_MALLOC(sizeof(DMC));
if (!dmc)
   return(NULL);
memset((char *)dmc, 0, sizeof(DMC));
dmc->any.next    = NULL;
dmc->any.cmd     = (unsigned char)**buff;
dmc->any.temp    = 0;

(*buff)++;

/***************************************************************
*  
*  Switch on the cmd id and process the fields of the different types.
*  
***************************************************************/

switch(dmc->any.cmd)
{

/***************************************************************
*  
*  Format of flattened find and rfind:
*
*  +-----+-----+--------------------+--+
*  | len | cmd |  find str          |\0|
*  +-----+-----+--------------------+--+
*     2     1    var
*  
***************************************************************/

case DM_find:
case DM_rfind:
   if (**buff)
      {
         dmc->find.buff = malloc_copy(*buff);
         if (!dmc->find.buff)
            bad = True;
      }
   else
      dmc->find.buff = NULL;
   break;

/***************************************************************
*  
*  Format of flattened markc and corner({)
*
*  +-----+-----+-----+-----+-----+-----+
*  | len | cmd | relr| row | col | relc|
*  +-----+-----+-----+-----+-----+-----+
*     2     1     1     2     2     1   
*  
***************************************************************/

case DM_markc:
case DM_corner:
   dmc->markc.row_relative = (char)*((*buff)++);
   s_i = (short int *)*buff;
   dmc->markc.row = (short int)ntohs(*s_i);
   SHORT_BUMP(*buff);
   s_i = (short int *)*buff;
   dmc->markc.col = (short int)ntohs(*s_i);
   SHORT_BUMP(*buff);
   dmc->markc.col_relative = (char)*((*buff)++);
   break;

/***************************************************************
*  
*  Format of flattened markp
*
*  +-----+-----+-----+-----+-----+
*  | len | cmd | pad |  x  |  y  |
*  +-----+-----+-----+-----+-----+
*     2     1     1     2     2
*  
***************************************************************/

case DM_markp:
   (*buff)++; /* skip padding */
   s_i = (short int *)*buff;
   dmc->markp.x = (short int)ntohs(*s_i);
   SHORT_BUMP(*buff);
   s_i = (short int *)*buff;
   dmc->markp.y = (short int)ntohs(*s_i);
   break;

/***************************************************************
*  
*  Format of flattened num
*
*  +-----+-----+-----+-----+
*  | len | cmd | rel | row |
*  +-----+-----+-----+-----+
*     2     1     1     2
*  
***************************************************************/

case DM_num:
   dmc->num.relative = (char)*((*buff)++);
   s_i = (short int *)*buff;
   dmc->num.row = (short int)ntohs(*s_i);
   break;

/***************************************************************
*  
*  Format of flattened bang
*
*  +-----+-----+-----+-----+-----+--------------+--+----------+--+
*  | len | cmd | -c  | -m  | -e  |  parm        |\0| -s value |\0|
*  +-----+-----+-----+-----+-----+--------------+--+----------+--+
*     2     1     1     1     1    var                var
*  
***************************************************************/

case DM_bang:
   dmc->bang.dash_c = *((*buff)++);
   dmc->bang.dash_m = *((*buff)++);
   dmc->bang.dash_e = *((*buff)++);
   if (**buff)
      {
         dmc->bang.cmdline = malloc_copy(*buff);
         if (!dmc->bang.cmdline)
            bad = True;
         *buff += strlen(*buff) + 1;
      }
   else
      dmc->bang.cmdline = NULL;
   if (((*buff) < (var_part_start+((short int)ntohs(*def_len)))) && **buff)
      {
         dmc->bang.dash_s = malloc_copy(*buff);
         if (!dmc->bang.dash_s)
            bad = True;
         *buff += strlen(*buff) + 1;
      }
   else
      dmc->bang.dash_s = NULL;
   break;

/***************************************************************
*  
*  Format of flattened bl
*
*  +-----+-----+-----+--------+--+---------+--+
*  | len | cmd | -d  | string |\0| string2 |\0|
*  +-----+-----+-----+--------+--+---------+--+
*     2     1     1     var          var
*  
***************************************************************/

case DM_bl:
   dmc->bl.dash_d = *((*buff)++);
   if (**buff)
      {
         dmc->bl.l_ident = malloc_copy(*buff);
         if (!dmc->bl.l_ident)
            bad = True;
         *buff += strlen(*buff) + 1;
      }
   else
      {
         dmc->bl.l_ident =  NULL;
         (*buff)++;
      }

   if (**buff)
      {
         dmc->bl.r_ident = malloc_copy(*buff);
         if (!dmc->bl.r_ident)
            bad = True;
      }
   else
      dmc->bl.r_ident =  NULL;
   break;

/***************************************************************
*  
*  Format of flattened case
*
*  +-----+-----+-----+
*  | len | cmd |func |
*  +-----+-----+-----+
*     2     1     1   
*  
***************************************************************/

case DM_case:
   dmc->d_case.func = *((*buff)++);
   break;


/***************************************************************
*  
*  Format of flattened ce cv, cc, ceterm, and cp
*
*  +-----+-----+------+------+--+------+--+------
*  | len | cmd | argc | arg0 |\0| arg1 |\0| arg2...
*  +-----+-----+------+------+--+------+--+-------
*     2     1     1     var
*  
***************************************************************/

case DM_ce:
case DM_cv:
case DM_cc:
case DM_ceterm:
case DM_cp:
   dmc->cv.argc   = *((*buff)++);

   dmc->cv.argv = (char **)CE_MALLOC((dmc->cv.argc + 1) * sizeof(var_part_start));
   if (!dmc->cv.argv)
      bad = True;

   for (i = 0; i < dmc->cv.argc && !bad; i++)
   {
      if (!bad)
         dmc->cv.argv[i] = CE_MALLOC(strlen(*buff)+1);
      if (dmc->cv.argv[i] && !bad)
         strcpy(dmc->cv.argv[i], *buff);
      else
         bad = True;
      *buff += strlen(*buff) + 1;
   }
   dmc->cv.argv[dmc->cv.argc] = NULL;
   break;


/***************************************************************
*  
*  Format of flattened cmdf and rec
*
*  +-----+-----+-----+------+--+
*  | len | cmd | -p  | path |\0|
*  +-----+-----+-----+------+--+
*     2     1     1     var
*  
***************************************************************/

case DM_cmdf:
case DM_rec:
   dmc->cmdf.dash_p = *((*buff)++);

   if (**buff)
      {
         dmc->cmdf.path = malloc_copy(*buff);
         if (!dmc->cmdf.path)
            bad = True;
      }
   else
      dmc->cmdf.path = NULL;
   break;


/***************************************************************
*  
*  Format of flattened cpo and cps
*
*  +-----+-----+-----+-----+-----+------+------+--+------+--+------
*  | len | cmd | -w  | -d  | -s  | argc | arg0 |\0| arg1 |\0| arg2...
*  +-----+-----+-----+-----+-----+------+------+--+------+--+-------
*     2     1     1     1     1     1      var
*  
***************************************************************/

case DM_cpo:
case DM_cps:
   dmc->cpo.dash_w = *((*buff)++);
   dmc->cpo.dash_d = *((*buff)++);
   dmc->cpo.dash_s = *((*buff)++);
   dmc->cpo.argc   = *((*buff)++);

   dmc->cpo.argv = (char **)CE_MALLOC((dmc->cpo.argc + 1) * sizeof(var_part_start));
   if (!dmc->cpo.argv)
      bad = True;

   for (i = 0; i < dmc->cpo.argc && !bad; i++)
   {
      if (!bad)
         dmc->cpo.argv[i] = CE_MALLOC(strlen(*buff)+1);
      if (dmc->cpo.argv[i] && !bad)
         strcpy(dmc->cpo.argv[i], *buff);
      else
         bad = True;
      *buff += strlen(*buff) + 1;
   }
   dmc->cpo.argv[dmc->cpo.argc] = NULL;
   break;


/***************************************************************
*  
*  Format of flattened dq
*
*  +-----+-----+-----+-----+-----+-----+------+--+
*  | len | cmd | -s  | -b  | -a  | -c  | pid  |\0|
*  +-----+-----+-----+-----+-----+-----+------+--+
*     2     1     1     1     1     2    var
*  
***************************************************************/

case DM_dq:
   dmc->dq.dash_s = *((*buff)++);
   dmc->dq.dash_b = *((*buff)++);
   dmc->dq.dash_a = *((*buff)++);
   s_i = (short int *)*buff;
   dmc->dq.dash_c = (short int)ntohs(*s_i);
   SHORT_BUMP(*buff);
   if (**buff)
      {
         dmc->dq.name = malloc_copy(*buff);
         if (!dmc->dq.name)
            bad = True;
      }
   else
      dmc->dq.name = NULL;
   break;


/***************************************************************
*  
*  Format of flattened echo
*
*  +-----+-----+-----+
*  | len | cmd | -r  |
*  +-----+-----+-----+
*     2     1     1   
*  
***************************************************************/

case DM_echo:
   dmc->echo.dash_r = *((*buff)++);
   break;


/***************************************************************
*  
*  Format of flattened eef
*
*  +-----+-----+-----+
*  | len | cmd | -a  |
*  +-----+-----+-----+
*     2     1     1   
*  
***************************************************************/

case DM_eef:
   dmc->eef.dash_a = *((*buff)++);
   break;


/***************************************************************
*  
*  Format of flattened ei and company
*
*  +-----+-----+-----+
*  | len | cmd |func |
*  +-----+-----+-----+
*     2     1     1   
*  
***************************************************************/

case DM_ei:
case DM_inv:
case DM_ro:
case DM_sb:
case DM_sc:
case DM_wa:
case DM_wh:
case DM_wi:
case DM_ws:
case DM_lineno:
case DM_hex:
case DM_vt:
case DM_wp:
case DM_mouse:
case DM_envar:
case DM_pdm:
case DM_caps:
   dmc->ei.mode = *((*buff)++);
   break;


/***************************************************************
*  
*  Format of flattened er
*
*  +-----+-----+------+
*  | len | cmd | char |
*  +-----+-----+------+
*     2     1     1
*  
***************************************************************/

case DM_er:
   dmc->es.chars[0] = *((*buff)++);
   dmc->es.chars[1] = '\0';
   dmc->es.string = dmc->es.chars;
   break;

/***************************************************************
*  
*  Format of flattened es
*
*  +-----+-----+--------------+--+
*  | len | cmd |  es          |\0|
*  +-----+-----+--------------+--+
*     2     1    var
*  
***************************************************************/

case DM_es:
   i = strlen(*buff) + 1;
   if (i < ES_SHORT)
      dmc->es.string = dmc->es.chars;
   else
      dmc->es.string = CE_MALLOC(i);
   if (dmc->es.string)
      strcpy(dmc->es.string, *buff);
   else
      bad = True;
   break;



/***************************************************************
*  
*  Format of flattened geo, debug, etc
*
*  +-----+-----+--------------+--+
*  | len | cmd |  parm        |\0|
*  +-----+-----+--------------+--+
*     2     1    var
*  
***************************************************************/

case DM_fl:
case DM_geo:
case DM_debug:
case DM_re:
case DM_cd:
case DM_pd:
case DM_env:
case DM_bgc:
case DM_fgc:
case DM_title:
case DM_eval:
case DM_lbl:
case DM_glb:
   if (**buff)
      {
         dmc->geo.parm = malloc_copy(*buff);
         if (!dmc->geo.parm)
            bad = True;
      }
   else
      dmc->geo.parm = NULL;
   break;


/***************************************************************
*  
*  Format of flattened icon, al ,ar, sic, ee
*
*  +-----+-----+-----+-----+
*  | len | cmd | -i  | -w  |
*  +-----+-----+-----+-----+
*     2     1     1     1   
*  
***************************************************************/

case DM_icon:
case DM_al:
case DM_ar:
case DM_sic:
case DM_ee:
   if (((short int)ntohs(*def_len)) > 3) /* RES 2/10/98 temp check for al, ar, sic, and ee 2.5 -> 2.5.1 */
      {
         dmc->icon.dash_i = *((*buff)++);
         dmc->icon.dash_w = *((*buff)++);
      }
   break;


/***************************************************************
*  
*  Format of flattened kd, ld, alias, mi, and lsf
*
*  +-----+-----+-----+--+-----+--+
*  | len | cmd | key |\0| def |\0|
*  +-----+-----+-----+--+-----+--+
*     2     1    var      var
*  
***************************************************************/

case DM_kd:
case DM_lkd:
case DM_alias:
case DM_mi:
case DM_lmi:
case DM_lsf:
   dmc->kd.key = malloc_copy(*buff);
   if (!dmc->kd.key)
      bad = True;
   *buff += strlen(*buff) + 1;
   if (**buff && !bad)
      {
         /* take care of single blank key defintion (null key defintion) */
         if ((**buff == ' ') && (*((*buff)+1) == '\0'))
            (*buff)++;
         dmc->kd.def = malloc_copy(*buff);
         if (!dmc->kd.def)
            bad = True;
      }
   else
      dmc->kd.def = NULL;
   break;


/***************************************************************
*  
*  Format of flattened msg, prefix, and sp
*
*  +-----+-----+-----+--+
*  | len | cmd | msg |\0|
*  +-----+-----+-----+--+
*     2     1    var     
*  
***************************************************************/

case DM_msg:
case DM_prefix:
case DM_sp:
   if (**buff)
      {
         dmc->msg.msg = malloc_copy(*buff);
         if (!dmc->msg.msg)
            bad = True;
      }
   else
      dmc->msg.msg = NULL;
   break;


/***************************************************************
*  
*  Format of flattened ph, pv, bell, and fbdr
*
*  +-----+-----+-----+-------+
*  | len | cmd | pad | chars |
*  +-----+-----+-----+-------+
*     2     1     1     2
*  
***************************************************************/

case DM_ph:
case DM_pv:
case DM_bell:
case DM_fbdr:
   (*buff)++;
   s_i = (short int *)*buff;
   dmc->ph.chars = (short int)ntohs(*s_i);
   break;

/***************************************************************
*  
*  Format of flattened pn
*
*  +-----+-----+-----+-----+------+--+
*  | len | cmd | -c  | -r  | path |\0|
*  +-----+-----+-----+-----+------+--+
*     2     1     1     1     var
*  
***************************************************************/

case DM_pn:
   dmc->pn.dash_c = *((*buff)++);
   dmc->pn.dash_r = *((*buff)++);
   if (**buff)
      {
         dmc->pn.path = malloc_copy(*buff);
         if (!dmc->pn.path)
            bad = True;
      }
   else
      dmc->pn.path = NULL;
   break;


/***************************************************************
*  
*  Format of flattened pp
*
*  +-----+-----+-------+--+
*  | len | cmd | pages |\0|
*  +-----+-----+-------+--+
*     2     1     var
*  pages is in string format.
*  
***************************************************************/

case DM_pp:
case DM_sleep:
   sscanf(*buff, "%f", &(dmc->pp.amount));
   break;


/***************************************************************
*  
*  Format of flattened prompt.  
*  This creates separate target strings for each prompt.
*
*  +-----+-----+------------+-------+------------+--+------------+--+
*  | len | cmd | tar_offset | quote | prompt_str |\0| target_str |\0|
*  +-----+-----+------------+-------+------------+--+------------+--+
*     2     1        1          1        var          var        
*  
***************************************************************/


case DM_prompt:
   i = *((*buff)++);  /* save target offset */
   dmc->prompt.quote_char = *((*buff)++); /* save quote character */
   if (**buff)
      {
         dmc->prompt.prompt_str = malloc_copy(*buff);
         if (!dmc->prompt.prompt_str)
            bad = True;

         *buff += strlen(*buff) + 1;
      }
   else
      {
         dmc->prompt.prompt_str = NULL;
         (*buff)++;
      }

   dmc->prompt.target_strt = malloc_copy(*buff);
   if (!dmc->prompt.target_strt)
      bad = True;

   dmc->prompt.target = dmc->prompt.target_strt + i;
   break;


/***************************************************************
*  
*  Format of flattened rw
*
*  +-----+-----+-----+
*  | len | cmd | -r  |
*  +-----+-----+-----+
*     2     1     1   
*  
***************************************************************/

case DM_rw:
   dmc->rw.dash_r = *((*buff)++);
   break;


/***************************************************************
*  
*  Format of flattened s and so
*
*  +-----+-----+------+--+-------+--+
*  | len | cmd | s1   |\0| s2    |\0|
*  +-----+-----+------+--+-------+--+
*     2     1    var       var
*  
***************************************************************/

case DM_s:
case DM_so:
   if (**buff)
      {
         dmc->s.s1 = malloc_copy(*buff);
         if (!dmc->s.s1)
            bad = True;
         *buff += strlen(*buff) + 1;
      }
   else
      {
         dmc->s.s1 =  NULL;
         (*buff)++;
      }
   if (**buff && !bad)
      {
         dmc->s.s2 = malloc_copy(*buff);
         if (!dmc->s.s2)
            bad = True;
      }
   else
      dmc->s.s2 =  NULL;
   break;


/***************************************************************
*  
*  Format of flattened tf
*
*  +-----+-----+----+----+-----+----+----+
*  | len | cmd | -b | -l | -r  | -c | -p |
*  +-----+-----+----+----+-----+----+----+
*     2     1     1    2    2     1    1  
*
***************************************************************/

case DM_tf:
   dmc->tf.dash_b = *((*buff)++);
   s_i = (short int *)*buff;
   dmc->tf.dash_l = (short int)ntohs(*s_i);
   SHORT_BUMP(*buff);
   s_i = (short int *)*buff;
   dmc->tf.dash_r = (short int)ntohs(*s_i);
   SHORT_BUMP(*buff);
   dmc->tf.dash_c = *((*buff)++);
   dmc->tf.dash_p = *((*buff)++);
   break;


/***************************************************************
*  
*  Format of flattened ts
*
*  +-----+-----+-----+----+-----+----+----+-----
*  | len | cmd | pad | -r | cnt | t1 | t2 | t3 ...
*  +-----+-----+-----+----+-----+----+----+------
*     2     1     1     2    2    2    2     2
*
*  cnt == 1 means that just t1 exists,
*  
***************************************************************/

case DM_ts:
   (*buff)++;
   s_i = (short int *)*buff;
   dmc->ts.dash_r = (short int)ntohs(*s_i);
   SHORT_BUMP(*buff);
   s_i = (short int *)*buff;
   dmc->ts.count = (short int)ntohs(*s_i);
   SHORT_BUMP(*buff);
   if (dmc->ts.count)
      {
         dmc->ts.stops  = (short int *)CE_MALLOC(dmc->ts.count * sizeof(short int));
         if (dmc->ts.stops)
            memcpy((char *)dmc->ts.stops, *buff, (dmc->ts.count * sizeof(short int)));
         else
            bad = True;
      }
   break;


/***************************************************************
*  
*  Format of flattened twb
*
*  +-----+-----+-----+
*  | len | cmd | brd |
*  +-----+-----+-----+
*     2     1     1   
*  
***************************************************************/

case DM_twb:
   dmc->twb.border = *((*buff)++);
   break;


/***************************************************************
*  
*  Format of flattened wc
*
*  +-----+-----+------+
*  | len | cmd | type |
*  +-----+-----+------+
*     2     1     1   
*  
***************************************************************/

case DM_wc:
case DM_reload:
   dmc->wc.type = *((*buff)++);
   break;


/***************************************************************
*  
*  Format of flattened wdc, ca, nc, and pc
*
*  +-----+-----+-----+--------+--+--------+--+
*  | len | cmd | num | bgc    |\0| fgc    |\0|
*  +-----+-----+-----+--------+--+--------+--+
*     2     1     1     var          var
*  
***************************************************************/

case DM_wdc:
case DM_ca:
case DM_nc:
case DM_pc:
   dmc->wdc.number = *((*buff)++);
   if (**buff)
      {
         dmc->wdc.bgc = malloc_copy(*buff);
         if (!dmc->wdc.bgc)
            bad = True;
         *buff += strlen(*buff) + 1;
      }
   else
      {
         dmc->wdc.bgc =  NULL;
         (*buff)++;
      }
   if (**buff && !bad)
      {
         dmc->wdc.fgc = malloc_copy(*buff);
         if (!dmc->wdc.fgc)
            bad = True;
      }
   else
      dmc->wdc.fgc =  NULL;
   break;


/***************************************************************
*  
*  Format of flattened wdf
*
*  +-----+-----+-----+-------------+--+
*  | len | cmd | num | geometry    |\0|
*  +-----+-----+-----+-------------+--+
*     2     1     1     var
*  
***************************************************************/

case DM_wdf:
   dmc->wdf.number = *((*buff)++);
   if (**buff)
      {
         dmc->wdf.geometry = malloc_copy(*buff);
         if (!dmc->wdf.geometry)
            bad = True;
      }
   else
      dmc->wdf.geometry = NULL;
   break;


/***************************************************************
*  
*  Format of flattened ww
*
*  +-----+-----+-----+------+-----+-----+
*  | len | cmd | -i  | -c   | mode| -a  |
*  +-----+-----+-----+------+-----+-----+
*     2     1     1     2      1     1   
*  
***************************************************************/

case DM_ww:
   dmc->ww.dash_i = *((*buff)++);

   s_i = (short int *)*buff;
   dmc->ww.dash_c = (short int)ntohs(*s_i);
   SHORT_BUMP(*buff);

   dmc->ww.mode   = *((*buff)++);
   dmc->ww.dash_a = *((*buff)++);
   break;


/***************************************************************
*  
*  Format of flattened xa
*
*  +-----+-----+-----+-----+-----+-------+-------+--+
*  | len | cmd | -f1 | -f2 | -l  | path1 | path2 |\0|
*  +-----+-----+-----+-----+-----+-------+-------+--+
*     2     1     1     1     1    var     var
*  
***************************************************************/

case DM_xa:
   dmc->xa.dash_f1 = *((*buff)++);
   dmc->xa.dash_f2 = *((*buff)++);
   dmc->xa.dash_l  = *((*buff)++);
   dmc->xa.path1 = malloc_copy(*buff);
   if (!dmc->xa.path1)
      bad = True;
   *buff += strlen(*buff) + 1;
   if (!bad)
      dmc->xa.path2 = malloc_copy(*buff);
   if (!dmc->xa.path2)
      bad = True;
   break;


/***************************************************************
*  
*  Format of flattened xc, xd, and xp
*
*  +-----+-----+-----+-----+-----+-----+------+--+
*  | len | cmd | -r  | -f  | -a  | -l  | path |\0|
*  +-----+-----+-----+-----+-----+-----+------+--+
*     2     1     1     1     1     1    var
*  
***************************************************************/

case DM_xc:
case DM_xd:
case DM_xp:
   dmc->xc.dash_r = *((*buff)++);
   dmc->xc.dash_f = *((*buff)++);
   dmc->xc.dash_a = *((*buff)++);
   dmc->xc.dash_l = *((*buff)++);
   if (**buff)
      {
         dmc->xc.path = malloc_copy(*buff);
         if (!dmc->xc.path)
            bad = True;
      }
   else
      dmc->xc.path = NULL;
   break;


/***************************************************************
*  
*  Format of flattened xl
*
*  +-----+-----+-----+-----+-----+-----+------+--+------+--+
*  | len | cmd | -r  | -f  | -a  | -l  | path |\0| data |\0|
*  +-----+-----+-----+-----+-----+-----+------+--+------+--+
*     2     1     1     1     1     1    var        var      
*  
***************************************************************/

case DM_xl:
   dmc->xl.base.dash_r = *((*buff)++);
   dmc->xl.base.dash_f = *((*buff)++);
   dmc->xl.base.dash_a = *((*buff)++);
   dmc->xl.base.dash_l = *((*buff)++);
   if (**buff)
      {
         dmc->xl.base.path = malloc_copy(*buff);
         if (!dmc->xl.base.path)
            bad = True;
         *buff += strlen(*buff) + 1;
      }
   else
      {
         dmc->xl.data = NULL;
         (*buff)++;
      }
   if (**buff)
      {
         dmc->xl.data = malloc_copy(*buff);
         if (!dmc->xl.data)
            bad = True;
      }
   else
      {
         dmc->xl.data = NULL;
         (*buff)++;
      }
   break;


/***************************************************************
*  
*  Format of flattened cntlc
*
*  +-----+-----+-----+-----+-----+-----+-----+------+--+
*  | len | cmd | -r  | -f  | -a  | -l  | -h  | path |\0|
*  +-----+-----+-----+-----+-----+-----+-----+------+--+
*     2     1     1     1     1     1     1    var
*  
***************************************************************/

case DM_cntlc:
   dmc->cntlc.dash_r = *((*buff)++);
   dmc->cntlc.dash_f = *((*buff)++);
   dmc->cntlc.dash_a = *((*buff)++);
   dmc->cntlc.dash_l = *((*buff)++);
   dmc->cntlc.dash_h = *((*buff)++);
   if (**buff)
      {
         dmc->cntlc.path = malloc_copy(*buff);
         if (!dmc->cntlc.path)
            bad = True;
      }
   else
      dmc->cntlc.path = NULL;
   break;



default:   /* default is for comamands with no parms */
   break;
}  /* end of switch on cmd_id  */


/***************************************************************
*  
*  Make sure that the end position for buff is on a halfword
*  boundary.  This takes care of machines which are sensitive
*  to such things.  
*  
***************************************************************/

*buff = var_part_start + (short int)ntohs(*def_len);

if ((short int)ntohs(*def_len) <= (short int)0)
   {
      dm_error("Zero length server key definition element found, aborting reading of server definitions", DM_ERROR_BEEP);
      *buff = NULL; /* kill processing of the definitions */
      bad = 1;
   }

DEBUG11(fprintf(stderr, "Def %s = %d, ", dmsyms[dmc->any.cmd].name,  ((short int)ntohs(*def_len)+sizeof(short)));)


if (!bad)
   return(dmc);
else
   {
      free((char *)dmc);
      DEBUG(fprintf(stderr, "Inflate server keydef failure\n");)
      return(NULL);
   }

}  /* end of inflate_dmc */



/************************************************************************

NAME:      dm_delsd  - delete the key definitions in the X server property


PURPOSE:    This routine deletes the current key defintions in the X property maintained
            by Ce.

PARAMETERS:

   1.  display         - pointer to Display (INPUT)
                         This is the display pointer returned by XOpenDisplay.
                         It is needed for all Xlib calls.

   2.  cekeys_atom_name -  pointer to char (INPUT)
                     This is the property name to hang the key definitios off.
                     the default name is: "CeKeys"

   3.  properties      - pointer to PROPERTIES (INPUT/OUTPUT) (in buffer.h)
                         This is the list of properties used by this display

FUNCTIONS :

   1.  Make sure the atom name has been gotten.  This should always have occurred,
       but we will check anyway.

   2.  Read the server key definitions with the delete after read option.

   3.  Issue a message showing how many bytes were released.


*************************************************************************/

void dm_delsd(Display      *display,
              char         *cekeys_atom_name,
              PROPERTIES   *properties)
{

Atom             actual_type;
int              actual_format;
unsigned long    nitems;
unsigned long    bytes_after;
char            *buff;

DEBUG9(XERRORPOS)
if (properties->cekeys_atom == None)
   properties->cekeys_atom = XInternAtom(display, cekeys_atom_name, False);

if (properties->cekeys_atom == None)
   {
      dm_error("Could not get key definition atom", DM_ERROR_BEEP);
      return;
   }

DEBUG9(XERRORPOS)
XGetWindowProperty(display,
                   RootWindow(display, DefaultScreen(display)),
                   properties->cekeys_atom,
                   0, /* offset zero from start of property */
                   INT_MAX / 4,
                   True, /* do delete the property */
                   XA_STRING,
                   &actual_type,
                   &actual_format,
                   &nitems,
                   &bytes_after,
                   (unsigned char **)&buff);

DEBUG11(fprintf(stderr, "%d chars of definitions deleted\n", nitems * (actual_format / 8));)

properties->keydef_property_size = 0;

XFlush(display);
XFree(buff);

} /* end of dm_delsd */


/************************************************************************

NAME:      dm_keys - display the key defintions in a separate window


PURPOSE:    This routine creates a file in /tmp, fills the file with
            key names and definitions and generates a cv command on this
            file.

PARAMETERS:

   1.  display     - pointer to Display (INPUT)
                     This is the display pointer returned by XOpenDisplay.
                     It is needed for all Xlib calls.

   2.   hash_table - pointer to void (INPUT)
                     This is the hash table pointer for the current display.

FUNCTIONS :

   1.  Create a file in /tmp called ce.<pid> and open if for output.

   2.  Walk the list of key defintions and process each one.

   3.  Translate the key hash key back into a key name and modifiers

   4.  Pad the key name to cause data alignment.

   5.  Dump the key defintion to the end of the key name and write the line to the file.

   6.  Generate a cv command to be processed and return it.

RETURNED VALUE:
   dmc  - pointer to DMC
          The returned value is NULL on failure.  On success it points to
          a DM command structure for the cv command.


*************************************************************************/

DMC *dm_keys(void             *hash_table,
             char              escape_char)
{
DMC             *dmc;
char             msg[256];
char             file[100];
FILE            *stream;
ENTRY           *cur_key;
KeySym           keysym;
int              state;
int              event;
char             key_work[MAX_KEY_NAME];
char            *key_string;
char             line[1050];
char            *end;
char             escape_string[2];

escape_string[0] = escape_char;
escape_string[1] = '\0';

/***************************************************************
*  
*  Create a work file to put the current key map in.
*  
***************************************************************/

snprintf(file, sizeof(file), "/tmp/ce.%d", getpid());

unlink(file);
if ((stream = fopen(file, "w")) == NULL)
   {
      snprintf(msg, sizeof(msg), "%s: %s", file, strerror(errno));
      dm_error(msg, DM_ERROR_BEEP);
      return(NULL);
   }

for (cur_key = hlinkhead(hash_table); cur_key != NULL; cur_key = (ENTRY *)cur_key->link)
{
   parse_hashtable_key(cur_key->key, "keys", &keysym, &state, &event);
   if (keysym == NoSymbol)  /* message already output */
      continue;

   /***************************************************************
   *  
   *  Translate the keysym to characters
   *  
   ***************************************************************/
   if ((event == KeyPress) || (event == KeyRelease))
      {
         DEBUG9(XERRORPOS)
         key_string = XKeysymToString(keysym);
         if (key_string == NULL)
            {
               snprintf(key_work, sizeof(key_work), "#%X", keysym);
               key_string = key_work;
            }

         if (state)
            add_modifier_chars(key_string, state, line, sizeof(line));
         else
            strncpy(line, key_string, sizeof(line));
         if (event == KeyRelease)
            strcat(line, "U");
      }
   else
      if ((event == ButtonPress) || (event == ButtonRelease))
         {
            snprintf(key_work, sizeof(key_work), "m%d", keysym);
            if (state)
               add_modifier_chars(key_work, state, line, sizeof(line));
            else
               strncpy(line, key_work, sizeof(line));
            if (event == ButtonRelease)
               strcat(line, "U");
         }
      else
         snprintf(line, sizeof(line), "Unknown X event type %d", event);

   /***************************************************************
   *  
   *  Pad the key name to the correct length
   *  
   ***************************************************************/

   strcat(line, " "); /* force at least one blank after key name */

   for (end = &line[strlen(line)]; (end - line) < 12; end++)
      *end = ' ';
   *end = '\0';

   /***************************************************************
   *  
   *  Add in the key defintion and write it out.
   *  
   ***************************************************************/

   dump_kd(NULL, (DMC *)cur_key->data, 0 /* list them all */, True, end, escape_char);
   add_escape_chars(end, end, escape_string, escape_char, False, True);
   fputs("kd ", stream);
   fputs(line, stream);
   fputs(" ke \n", stream);

} /* end of for loop looking at entries */

fclose(stream);

/***************************************************************
*  
*  Generate a cv command and return it.
*  
***************************************************************/

dmc = (DMC *)CE_MALLOC(sizeof(DMC));
if (!dmc)
   return(NULL);

dmc->next = NULL;
dmc->any.cmd  = DM_cv;
dmc->any.temp = True;
dmc->cv.argc  = 2;
dmc->cv.argv  = (char **)CE_MALLOC(sizeof(end) * 3);  /* get three pointers, argv[0], argv[1], and a null */
if (!dmc->cv.argv)
   {
      free((char *)dmc);
      return(NULL);
   }
dmc->cv.argv[0] = CE_MALLOC(4);
if (!dmc->cv.argv[0])
   {
      free((char *)dmc->cv.argv);
      free((char *)dmc);
      return(NULL);
   }
else
   strcpy(dmc->cv.argv[0], "cv");

dmc->cv.argv[1] = CE_MALLOC(strlen(file)+1);
if (!dmc->cv.argv[1])
   {
      free((char *)dmc->cv.argv[0]);
      free((char *)dmc->cv.argv);
      free((char *)dmc);
      return(NULL);
   }
else
   strcpy(dmc->cv.argv[1], file);

dmc->cv.argv[2] = NULL;

return(dmc);

} /* end of dm_keys */





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
*  module kd.c
*
*  Routines:
*         key_to_dm            - Convert key or mouse press/release event to dm command
*         init_dm_keys         - Init the DM lookup table and start the load.
*         process_some_keydefs - Load some key definitions in a background mode
*         install_keydef       - Put a keydef in the table
*         dm_kd                - Process a parsed kd command.
*         dm_cmdf              - Process a cmdf command.
*         dm_kk                - kk (key key command)
*         add_modifier_chars   - Use an X event modifier mask to add modifier chars to a string
*         parse_hashtable_key  - Parse a keys hash table key into keysym, state, and event.
*         ce_merge             - Merge old and new key definitions.
*         dm_eval              - do symbolic substitution and parse a line of data
*
*  Local:
*         parse_keyname             -  Convert a keyname to a keysym, state, and event
*         translate_keyname         -  Attempt standard and Apollo keyname translations.
*         translate_apollo_keyname  -  special checking for translatable
*                                      Apollo keynames.
*         load_default_keydefs      -  load in hard coded defintions if there
*                                      is nothing else.
*         check_required_keydefs    -  Make sure certain important keys are defined, like return.
*         hex_char                  -  Convert a character to it's printable hex value
*         lookup_key                - Convert a key name to command list
*
***************************************************************/


#include <stdio.h>          /* /usr/include/stdio.h  */
#include <string.h>         /* /usr/include/string.h  */
#include <errno.h>          /* /usr/include/errno.h /usr/include/sys/errno.h */
#include <ctype.h>          /* /usr/include/ctype.h */
#include <time.h>           /* /usr/include/time.h  */
#if !defined(OMVS) && !defined(WIN32)
#include <sys/param.h>      /* /usr/include/sys/param.h */
#endif
#ifndef MAXPATHLEN
#define MAXPATHLEN	1024
#endif

#ifdef WIN32
/* XK_SPECIAL gives us the define for XK_lf */
#ifdef WIN32_XNT
#define XK_SPECIAL
#include "xnt.h"
#else
#include "X11/X.h"           /* /usr/include/X11/X.h   */
#include "X11/Xlib.h"        /* /usr/include/X11/Xlib.h */
/* XK_SPECIAL gives us the define for XK_lf */
#define XK_SPECIAL
#include <X11/keysym.h>      /* /usr/include/X11/keysym.h /usr/include/X11/keysymdef.h */
#endif
#else
#include <X11/X.h>           /* /usr/include/X11/X.h   */
#include <X11/Xlib.h>        /* /usr/include/X11/Xlib.h */
/* XK_SPECIAL gives us the define for XK_lf */
#define XK_SPECIAL
#include <X11/keysym.h>      /* /usr/include/X11/keysym.h /usr/include/X11/keysymdef.h */
#endif

#define KEYDEF_ENV_NAME "CEKEYS"
#define KEYDEF_HOME_FILE ".Cekeys"
#define LOAD_NO_KEYDEFS   "/dev/null"

/***************************************************************
*  
*  Size format of the hash table key entry.
*  
***************************************************************/

#define KEY_KEY_FORMAT   "%08X%08X%04X"


#define CNTL_PREFIX_CHAR  '^'
#define MOD1_PREFIX_CHAR  '*'
#define MOD2_PREFIX_CHAR  '~'
#define MOD3_PREFIX_CHAR  '='
#define MOD4_PREFIX_CHAR  '`'
#define MOD5_PREFIX_CHAR  '%'


/* get the real copy of the global variable in kd.h */
#define KD_C 1

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "alias.h"
#include "debug.h"
#include "defkds.h"
#include "dmc.h"
#include "dmsyms.h"
#include "dmwin.h"
#include "drawable.h"
#include "execute.h"
#ifndef HAVE_CONFIG_H
#include "help.h"        /* used in ce_merge */
#endif
#include "hsearch.h"
#include "ind.h"
#include "pw.h"
#include "getevent.h"
#include "emalloc.h"
#include "kd.h"
#include "normalize.h"
#include "parms.h"        /* used in ce_merge */
#include "parsedm.h"
#include "pastebuf.h"
#include "pd.h"
#include "prompt.h"
#include "serverdef.h"
#include "wdf.h"
#include "xc.h"
#include "tab.h"
#ifdef PAD
#include "vt100.h"
#endif
#include "xerrorpos.h"

#ifndef HAVE_STRLCPY
#include "strl.h"
#endif

/***************************************************************
*  
*  System routines which have no built in defines
*  
***************************************************************/

char  *getenv(const char *name);
void   exit(int code);
#ifndef linux
int    access(char *path, int mode);
#endif

static int  parse_keyname(Display *display,     /* input   */
                          char    *key,         /* input   */
                          KeySym  *keysym,      /* output  */
                          int     *state,       /* output  */
                          int     *event,       /* output  */
                          int     line_no);     /* input   */

static KeySym translate_keyname(Display   *display,
                                char      *key,
                                int       *event_type,
                                int       *state);

static KeySym translate_apollo_keyname(char      *key,
                                       int       *event_type,
                                       int       *state);

static void load_default_keydefs(DISPLAY_DESCR     *dspl_descr);

static void check_required_keydefs(DISPLAY_DESCR     *dspl_descr);

static char  *hex_char(char *str, int len);

static DMC   *lookup_key(char              *key,
                         DISPLAY_DESCR     *dspl_descr,
                         int                line_no);

static int add_line(char  *line, char *key_path);

static void truncate_dm_list(DMC    *kill_dmc,
                             DMC    *curr_dmc);


/************************************************************************

NAME:      key_to_dm  -   Translate a keyboard keycode into a DM command structure
                          to be executed.


PURPOSE:    This routine handles the translation of keyboard input into
            DM command structures.  I handles both key and mouse events.

PARAMETERS:

   1.   event  -    pointer to XEvent  (INPUT)

   2.   raw_mode -  int (INPUT)
                    This flag turns on raw mode.  In raw mode, all dm translations
                    are turned off and the only things which is returned are
                    es commands of the value of the keysym.  This is used in
                    vt100 mode emulation.

   3.   isolatin -  int (INPUT)
                    This flag turns on isolatin mode.  This allows default
                    es type action for characters having modifier bits other
                    than Shift.

   3.   caps_mode - int (INPUT)
                    This flag causes keys which default to an 'es' of some
                    string to convert the entered characters to upper case.
                    This is used for editing mainframe files.

   4.   hash_table - pointer to void (INPUT)
                     This is the hash table pointer for the current display.

FUNCTIONS :

   1.  If this is a key press or key release event get the X keysym for this

   2.  Filter out the modifier keys which are not supported.

   3.  Build the DM hash table lookup key from the keypress information.
       For a key press or key release, this is the X keysym, the modifier
       mask and the event type (keypress or keyrelease)  For a mouse button press
       or release, the mouse button number replaces the X keysym.  There can
       be no overlap because the event types are different.

   4.  Lookup the event in the hash table.  If it is found, return the 
       associated DM command list.

   5.  If the event is not found, see if it is one of the default events.
       The return and keypad enter keys default to the "en" comand.
       The alphanumeric keys default to their respective symbols (Keysym A is an A).

RETURNED VALUE:
   dmc  -  pointer to DMC
           The returned value is the command structure for the command(s) this
           keysym translates to.  NULL is returned if there is no translation.

*************************************************************************/

#define  ALLOWED_MODIFIERS (ShiftMask | ControlMask | Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask)


DMC      *key_to_dm(XEvent *event,
                    int     raw_mode,
                    int     isolatin,
                    int     caps_mode,  /* added 10/21/1998 RES */
                    void   *hash_table)
{

int                   name_len;
KeySym                keysym;

ENTRY                 sym_to_find;
ENTRY                *def_found;
char                  the_key[KEY_KEY_SIZE];
DMC                  *dmc;

static char           key_name[MAX_KEY_NAME];
static DMC_es         default_cmd = {NULL, DM_es, 0, ""};


if (event->type == KeyPress || event->type == KeyRelease)
   {
      DEBUG9(XERRORPOS)
      if (event->xkey.keycode != 0) /* special invalid keycode passed by fake_keystroke in getevent.c */
         name_len = XLookupString((XKeyEvent *)event,
                                  key_name,
                                  sizeof(key_name),
                                  &keysym,
                                 NULL);
      else
         return(NULL);
      key_name[name_len] = '\0';
      event->xkey.state &= ALLOWED_MODIFIERS;
      snprintf(the_key, sizeof(the_key), KEY_KEY_FORMAT, keysym, event->xkey.state, event->type);
#ifdef PAD
      /***************************************************************
      *  Raw mode is really the same as vt100 mode.  Only do the
      *  key translation if there are no modifier keys pressed.
      ***************************************************************/
      if (raw_mode && !event->xkey.state)
         {
            vt100_key_trans(keysym, key_name);
            name_len = strlen(key_name);
         }
#endif
      DEBUG11(fprintf(stderr, "key_to_dm:  keycode = 0x%X, keysym = 0x%X, state = 0x%X, event type = %s(%d), value = 0x%s, key = 0x%s, ",
                             event->xkey.keycode, keysym, event->xkey.state,
                             ((event->type == KeyPress) ? "KeyPress" : "KeyRelease"), event->type,
                             hex_char(key_name, name_len),
                             the_key);
      )

   }
else
   if (event->type == ButtonPress || event->type == ButtonRelease)
      {
         name_len = 0;
         event->xbutton.state &= ALLOWED_MODIFIERS;
         snprintf(the_key, sizeof(the_key), KEY_KEY_FORMAT, event->xbutton.button, event->xbutton.state, event->type);
         DEBUG11(fprintf(stderr, "key_to_dm:  button = 0x%X, state = 0x%X, event type = %s(%d), key = 0x%s, ",
                                event->xbutton.button, event->xbutton.state,
                                ((event->type == KeyPress) ? "ButtonPress" : "ButtonRelease"), event->type,
                                the_key);
         )
      }
   else
      {
         DEBUG( fprintf(stderr, "?   Unexpected event type %d passed to key_to_dm\n", event->type); )
         return(NULL);
      }


sym_to_find.key = the_key;

if ((def_found = (ENTRY *)hsearch(hash_table, sym_to_find, FIND)) != NULL && (((DMC *)def_found->data)->any.cmd != DM_NULL) && !(raw_mode && name_len))
   {
      /***************************************************************
      *
      *  Found a definition
      *
      ***************************************************************/
      DEBUG11(fprintf(stderr, " - definition found\n");)
      dmc = (DMC *)def_found->data;
      return(dmc);
   }
else
   {
      /***************************************************************
      *
      *  Keysym has no definition, if it has a value, and no modifier
      *  other than shift is pressed, we will assume
      *  es '<value>'.  If the ISOLATIN parm is turned on, we will
      *  assume es '<value>' even if alt, compose, or some such is turned on.
      *
      ***************************************************************/
      if ((name_len != 0) && (event->type == KeyPress) && (((event->xbutton.state & ~ShiftMask) == 0) || isolatin || raw_mode))
         {
            default_cmd.string = key_name;
            if (caps_mode)   /* 10/21/1998, RES, added */
               case_line('u', key_name, 0, MAX_LINE);

            DEBUG11(fprintf(stderr, " - default es of %s (0x%02X)\n", key_name, key_name[0]);)
            return((DMC *)&default_cmd);
         }
      else
         {
            DEBUG11(fprintf(stderr, " - No translation\n");)
            return(NULL);
         }
   }

} /* end of key_to_dm */

/***************************************************************
*  
*  SOME_KDS  is the number of key definitions to process on each
*            iteration.
*
*  The other variables are set up in init_dm_keys and used in
*  process some keydefs.
*  
***************************************************************/

#define SOME_KDS 10

/*static int    dm_keys_intialized = 0; RES 6/22/95 Lint change */
static char  *key_path;
static char   key_path_work[MAXPATHLEN];
static int    line_no;

/************************************************************************

NAME:      init_dm_keys  -   Start the DM keydef processing.


PURPOSE:    This routine gets the business started of loading the key
            definitions.  The bulk of the work is done in background mode.
            background processing is initiated through routines in getevent.c

PARAMETERS:

   1.  backup_keydefs - pointer to char (INPUT)
                        This is a pointer to the backup keydef path name.
                        It should be a resouce which the main routine passes.

   2.  tabstop_resource - pointer to char (INPUT)
                        This is a pointer to a ts (tabstop) command read from
                        the .Xdefaults file.

   3.  display        - pointer to Display (Xlib type) (INPUT)
                        This the current display pointer used to access the
                        key definitions if they are stored in the server.

   4.  hash_table     - pointer to pointer to void (OUTPUT)
                        The address of the hsearch table is placed in this
                        address.

GLOBAL DATA:
   This data is global within kd.c but not visible outside.

   1.  dm_keys_intialized  -  int
                   This flag indicates whether the keydefs are intialized
                   it is statically zero and set to one when the keydefs are
                   completed.
   
   2.  key_path  -  pointer to char 
                   This is the name of text key def file.  It is used with
                   key_path_work if a default name is built or it points to
                   a name gotten from an environment variable.
   

FUNCTIONS :

   1.  Create the hash table.

   2.  See if the environment variable giving the name of the keydef file
       exists.  If so, see if it exists.  If it exists, use it.  Otherwise
       build the default name of $HOME/.Key_definitions.

   3.  Verify that the keydef file is accessable for read.


*************************************************************************/

/* ARGSUSED */
void      init_dm_keys(char              *site_keydefs,
                       char              *tabstop_resource,
                       DISPLAY_DESCR     *dspl_descr)
{

char                 *home;
char                  msg[512];
char                  homedir[MAXPATHLEN];
int                   rc;

/**************************************************************
*  
*  Create the hash table.  hcreate is a system routine
*  in search.h (it uses malloc to create the table)
*  
***************************************************************/

if (dspl_descr->hsearch_data)
   hdestroy(dspl_descr->hsearch_data);

if (!(dspl_descr->hsearch_data = hcreate(HASH_SIZE)))
   {
      snprintf(msg, sizeof(msg), "?    Could not build hash table for DM commands, Out of memory\n");
      dm_error(msg, DM_ERROR_LOG);
      exit(2);
   }


/***************************************************************
*  
*  At this point insert the code to determine if we want the
*  X server copy of the resources.
*  If we get them we are done and need go no further.
*  
***************************************************************/

rc = load_server_keydefs(dspl_descr->display, dspl_descr->properties, KEYDEF_PROP, dspl_descr->hsearch_data);
if (rc == 0)
   {
      key_path = LOAD_NO_KEYDEFS; /* null out key def processing */
      check_required_keydefs(dspl_descr);
      check_menu_items(dspl_descr);
   }
else
   {

      /**************************************************************
      *  
      *  Get the name of the keydef file.  If it is readable,
      *  read it in and execute the keydefs.
      *
      *  Search for keydefs:
      *  1.  If environment var CEKEYS is set and readable, use it.
      *  2.  If $HOME/.Cekeys exists and is readable, use it.
      *  3.  If the name in the resource exists and is readble, use it.
      *  4.  Out of luck, flag an error.
      *  
      ***************************************************************/
      key_path = getenv(KEYDEF_ENV_NAME);
      if (key_path && (key_path[0] == '~'))
         key_path = translate_tilde(key_path);


      if (key_path == NULL || key_path[0] == '\0' || access(key_path, 04) != 0) 
         {   
            if ((key_path != NULL) && (key_path[0] != '\0'))
               {
                  snprintf(msg, sizeof(msg), "%s: %s", key_path, strerror(errno));
                  dm_error(msg, DM_ERROR_LOG);
               }
            home = get_home_dir(homedir, NULL);
            if (home != NULL)
               {
                  snprintf(key_path_work, sizeof(key_path_work), "%s/%s", home, KEYDEF_HOME_FILE);
                  if (access(key_path_work, 00) == 0) 
                     key_path = key_path_work;
               }
         }

      if ((key_path != NULL) && (access(key_path, 04) == 0)) 
         {
            DEBUG1(fprintf(stderr, "Using key definition file %s\n", key_path); )
            change_background_work(NULL, BACKGROUND_KEYDEFS, 1);
         }
      else
         {
            if (key_path != NULL)
               {
                  snprintf(msg, sizeof(msg), "%s: %s", key_path, strerror(errno));
                  dm_error(msg, DM_ERROR_LOG);
               }
            if ((site_keydefs != NULL) && (access(site_keydefs, 04) == 0)) 
               {
                  key_path = site_keydefs;
                  DEBUG1(fprintf(stderr, "Using key definition file %s\n", key_path); )
                  change_background_work(NULL, BACKGROUND_KEYDEFS, 1);
                  line_no = 0;
               }
            else         
               {
                  snprintf(msg, sizeof(msg), "Unable to find key definition file %s - run ce_init and try again\n", (key_path ? key_path : "<NULL>"));
                  dm_error(msg, DM_ERROR_LOG);
                  load_default_keydefs(dspl_descr);
                  key_path = LOAD_NO_KEYDEFS; /* null out key def processing */
               }
         }
   }

} /* end of init_dm_keys */


/************************************************************************

NAME:      process_some_keydefs  - Read and process the keydef file.


PURPOSE:    This routine gets called as a background task.  It reads in
            or processes a number of key defintions and returns a flag
            saying if it is done.

PARAMETERS:

   1.  display    - pointer to Display (INPUT)
                    This is the open display which is used in X calls.

   2.  window     - pointer to Window (INPUT)
                    This is the main window id which is used in X calls.
                    In some cases, the X value None is passed.


GLOBAL DATA:
   This data is global within kd.c but not visible outside.

   1.  dm_keys_intialized  -  int
                   This flag indicates whether the keydefs are intialized
                   it is statically zero and set to one when the keydefs are
                   completed.
   
   2.  key_path  -  pointer to char 
                   This is the name of text key def file.  It is used with
                   key_path_work if a default name is built or it points to
                   a name gotten from an environment variable.
   
STATIC DATA:
   This data is statically held in process_some_keydefs

   1.  first_dmc    -  pointer to DMC
                   This is the head of the linked list of kd commands built up
                   when reading the keydef file.
   
   2.  last_dmc    -  pointer to DMC
                   This is the tail of the linked list of kd commands built up
                   when reading the keydef file.
   
   3.  kd_file_read  -  int
                   This flag is set when all the key defintions have been
                   read in and parsed.
   
   4.  cmd_file  -  pointer to FILE
                   This is the stream used to read in the key definitions.
                   When it is still null, it needs to be opened.
   
   3.  total_keydef_lines  -  int
                   This is a count of the number of lines in the keydef file.
                   it is used for debugging purposes.
   


FUNCTIONS :

   1.  If the key definitions are not all read in read some.  If the file
       needs to be opened, open it.

   2.  Read and parse SOME_KDS or to end of file and return.  Once end of file has
       been reached, set the flag so we know we are done with the
       reading part.

   3.  On eof of the key definition file, close it.

   4.  Execute the parsed key definitions SOME_KDS at a time until they are
       all gone.

   5.  When we finish, set the dm_keys_intialized flag.

RETURNED VALUE:

   more_to_do -  The returned flag specifies whether there are more
                 keydefs to look at.  If there are, another call to
                 process_some_keydefs is required when time permits.
                 0  -  done
                 1  -  more to do

NOTES:
   1.  The static variable first_dmc is used a flag.  After it has been 
       loaded when reading the key definitions, elements are taken off
       the linked list till it is exhausted.  At that point it is
       NULL and shows there is nothing left to do.


*************************************************************************/

int       process_some_keydefs(DISPLAY_DESCR     *dspl_descr)
{

static DMC           *first_dmc = NULL;
static DMC           *last_dmc;
static int            kd_file_read = 0;
static FILE          *cmd_file = NULL;
static int            total_keydef_lines = 0;
static int            total_keydefs = 0;
static int            total_aliases = 0;
static int            total_menu_items = 0;
static int            total_lsf_items = 0;
static int            did_site_keydefs = False;

DMC                  *hold_dmc;
DMC                  *prev_dmc;
DMC                  *work_dmc;
DMC                  *cmdf_dmlist;
char                  msg[512];
char                  site_name[512];
int                   kd_count = 0;
char                 *eof_detector;
char                  cmdline[MAX_LINE+2];
int                   len;
DMC                  *new_dmc;
char                 *p;


if (!kd_file_read)
   {   
      if (cmd_file == NULL)
         {
            if (strcmp(key_path, LOAD_NO_KEYDEFS) == 0)
               return(0); /* no keydefs to load */

            if ((cmd_file = fopen(key_path, "r")) == NULL)
               {
                  snprintf(cmdline, sizeof(cmdline), "Cannot open kd file %s : %s", key_path, strerror(errno));
                  dm_error(cmdline, DM_ERROR_LOG);
                  /*dm_keys_intialized = 1;  they are as initialized as they are going to get */ /* RES 6/22/95 Lint change */
                  return(0); /* we are all done */
               }
         }

      /***************************************************************
      *  Read one line of commands from the .Cekeys file.
      ***************************************************************/

      DEBUG4(fprintf(stderr, "Reading some key defs\n"); )
      while((kd_count < SOME_KDS) && ((eof_detector = fgets(cmdline, sizeof(cmdline), cmd_file)) != NULL))
      {
         DEBUG4(total_keydef_lines++;)
         len = strlen(cmdline);

         if (cmdline[len-1] == '\n')
            cmdline[--len] = '\0';

         new_dmc = parse_dm_line(cmdline,
                                 True /* The kd's are temp */,
                                 ++line_no,
                                 dspl_descr->escape_char,
                                 dspl_descr->hsearch_data);  /* parse the command line, show temp */
         kd_count++;

         /***************************************************************
         *  If there are site keydefs to process, splice the cmdf on
         *  the front of the first line.
         *  (changed meaning of APP_KEYDEF  6/9/97 RES)
         ***************************************************************/
         if (!did_site_keydefs)
            {
               if ((APP_KEYDEF != NULL) && (*(APP_KEYDEF) != '\0') && (strcmp(key_path, APP_KEYDEF) != 0))
                  {
                     if (dspl_descr->escape_char == '\\')
                        snprintf(site_name, sizeof(site_name), "%s.U", APP_KEYDEF);
                     else
                        snprintf(site_name, sizeof(site_name), "%s.A", APP_KEYDEF);
                     if (access(site_name, 04) != 0)
                        snprintf(site_name, sizeof(site_name), "%s", APP_KEYDEF);

                     snprintf(msg, sizeof(msg), "cmdf %s", site_name);
                     work_dmc = parse_dm_line(msg, True, 0, dspl_descr->escape_char, dspl_descr->hsearch_data);  /* parse the command line, show temp */
                     if (work_dmc)
                        {
                           work_dmc->next = new_dmc;
                           new_dmc = work_dmc;
                        }
                  }
               did_site_keydefs = True;
            }

         /***************************************************************
         *  Make sure there are only KD commands and WDF commands and WDC commands.
         *  CMDF commands are also processed, as includes.
         ***************************************************************/
         work_dmc = new_dmc;
         prev_dmc = NULL;
         while (work_dmc != NULL && work_dmc != DMC_COMMENT)
         {
            if ((work_dmc->any.cmd != DM_kd)  &&
                (work_dmc->any.cmd != DM_wdf) &&
                (work_dmc->any.cmd != DM_wdc) &&
                (work_dmc->any.cmd != DM_mi)  &&
                (work_dmc->any.cmd != DM_lsf) &&
                (work_dmc->any.cmd != DM_alias))
               {
                  /***************************************************************
                  *  Process a cmdf right away and collect all the commands.  
                  *  The following test reads:
                  *  If the current command we just got was a cmdf and
                  *  executing the cmdf did not return a DMC of NULL and
                  *  the returned DMC was not the comment value,
                  *  Then splice in the new dmc's in place of the cmdf.
                  ***************************************************************/
                  if (work_dmc->any.cmd == DM_cmdf)
                     cmdf_dmlist = dm_cmdf(work_dmc, NULL, None, dspl_descr->escape_char, dspl_descr->hsearch_data, False);
                  else
                     if (work_dmc->any.cmd == DM_bang)
                        cmdf_dmlist = dm_bang_lite(work_dmc, dspl_descr);
                     else
                        cmdf_dmlist = NULL;
                  if ((cmdf_dmlist != NULL) && (cmdf_dmlist != DMC_COMMENT))
                     {
                        /***************************************************************
                        *  Splice the results of the cmdf into the list (like an include)
                        ***************************************************************/
                        for (hold_dmc = cmdf_dmlist; hold_dmc->next != NULL; hold_dmc = hold_dmc->next)
                            ; /* do nothing, just follow the chain, on exit hold_dmc is the last dmc */
                        hold_dmc->next = work_dmc->next;
                        work_dmc->next = NULL;
                        if (prev_dmc != NULL)
                           prev_dmc->next = cmdf_dmlist;
                        else
                           new_dmc = cmdf_dmlist;
                        free_dmc(work_dmc);
                        work_dmc = cmdf_dmlist;
                        kd_count = SOME_KDS; /* enough work for this pass */
                     }
                  else
                     {
                        /***************************************************************
                        *  cmdf did not work, throw the DMC out or the DMC was an
                        *  invalid type anyway.  Don't flag null commands, as errors.
                        ***************************************************************/
                        if ((work_dmc->any.cmd != DM_NULL) && (work_dmc->any.cmd != DM_cmdf) && (work_dmc->any.cmd != DM_bang))
                           {
                              snprintf(msg, sizeof(msg), "DM cmd \"%s\", invalid in keydef file (%s\"%s\")",
                                           dmsyms[work_dmc->any.cmd].name, iln(line_no), cmdline);
                              dm_error(msg, DM_ERROR_LOG);
                              DEBUG(fprintf(stderr, "Invalid cmd id = %d\n", work_dmc->any.cmd);)
                           }
                        hold_dmc = work_dmc;
                        work_dmc = work_dmc->next;
                        if (prev_dmc != NULL)
                           prev_dmc->next = work_dmc;
                        else
                           new_dmc = work_dmc;
                        hold_dmc->next = NULL;
                        free_dmc(hold_dmc);
                    }
               }
            else
               {
                  /***************************************************************
                  *  Normal case: a kd, alias, mi, wdf, or wdc command, just pass over it.
                  ***************************************************************/
                  prev_dmc = work_dmc;
                  work_dmc = work_dmc->next;
               }
         }


         /***************************************************************
         *  Put the new key definition commands in the list.
         ***************************************************************/
         if ((new_dmc != DMC_COMMENT) && (new_dmc != NULL))
            {
               if (first_dmc == NULL)
                  first_dmc = new_dmc;
               else
                  last_dmc->next = new_dmc;
               last_dmc = new_dmc;
          
               /***************************************************************
               *  If the parse_dm_line made a list of dm cmds, got to the end.
               ***************************************************************/
               while((last_dmc->next != NULL) && (last_dmc->next != DMC_COMMENT))
                  last_dmc = last_dmc->next;
            }
      } /* end of while more data */

      if (eof_detector == NULL)  /* found eof */
         {
            fclose(cmd_file);
            cmd_file = NULL;
            kd_file_read = 1;
            DEBUG4(fprintf(stderr, "Total keydef lines = %d\n", total_keydef_lines); )
         }
      else
         return(1); /* still in progress */
   }



DEBUG4(fprintf(stderr, "Processing some key defs\n"); )


while (first_dmc != NULL && kd_count < SOME_KDS)
{
   if (first_dmc->any.cmd == DM_kd)
      {
         DEBUG4(dump_kd(stderr, first_dmc, 1 /* only print one */, False, NULL, dspl_descr->escape_char);)
         if (dm_kd(first_dmc, False/* don't update server def now */, dspl_descr) == 0)
            total_keydefs++;
      }
   else
      if ((first_dmc->any.cmd == DM_wdf) || (first_dmc->any.cmd == DM_wdc))
         {
            DEBUG4(dump_kd(stderr, first_dmc, 1 /* only print one */, False, NULL, dspl_descr->escape_char);)
            dm_wdf(first_dmc, dspl_descr->display, dspl_descr->properties);
         }
      else
         if (first_dmc->any.cmd == DM_alias)
            {
               DEBUG4(dump_kd(stderr, first_dmc, 1 /* only print one */, False, NULL, dspl_descr->escape_char);)
               if (dm_alias(first_dmc, False/* don't update server def now */, dspl_descr) == 0)
                  total_aliases++;
            }
         else
            if (first_dmc->any.cmd == DM_mi)
               {
                  DEBUG4(dump_kd(stderr, first_dmc, 1 /* only print one */, False, NULL, dspl_descr->escape_char);)
                  if (dm_mi(first_dmc, False/* don't update server def now */, dspl_descr) == 0)
                     total_menu_items++;
               }
            else
               if (first_dmc->any.cmd == DM_lsf)
                  {
                     DEBUG4(dump_kd(stderr, first_dmc, 1 /* only print one */, False, NULL, dspl_descr->escape_char);)
                     if (dm_lsf(first_dmc, False/* don't update server def now */, dspl_descr) == 0)
                        total_lsf_items++;
                  }
               else
                  {
                     snprintf(msg, sizeof(msg), "DM cmd %s, invalid in keydef file",  dmsyms[first_dmc->any.cmd].name);
                     dm_error(msg, DM_ERROR_LOG);
                     DEBUG(fprintf(stderr, "(2)Invalid cmd id = %d\n", work_dmc->any.cmd);)
                  }

   kd_count++;
   hold_dmc = first_dmc->next;
   first_dmc->next = NULL;
   free_dmc(first_dmc);
   first_dmc = hold_dmc;
}

if (first_dmc != NULL)
   return(1); /* show we are not done */
else
   {
      /*dm_keys_intialized = 1;  RES 6/22/95 Lint change */
      if ((total_keydefs < 15) && (strcmp(key_path, LOAD_NO_KEYDEFS) != 0))
         load_default_keydefs(dspl_descr);
      else
         {
            check_required_keydefs(dspl_descr);
            save_server_keydefs(dspl_descr->display, dspl_descr->properties, KEYDEF_PROP, dspl_descr->hsearch_data);
            check_menu_items(dspl_descr);
         }

      if (strcmp(key_path, LOAD_NO_KEYDEFS) != 0)
         {
            snprintf(msg, sizeof(msg), "%4d Key definitions loaded.\n", total_keydefs);
            p = &msg[strlen(msg)];
            if (total_aliases)
               {
                  snprintf(p, (sizeof(msg)-(p-msg)), "%4d Aliases loaded.\n", total_aliases);
                  p = &p[strlen(p)];
               }
            if (total_lsf_items)
               {
                  snprintf(p, (sizeof(msg)-(p-msg)), "%4d Lang Sensitive Filters loaded.\n", total_lsf_items);
                  p = &p[strlen(p)];
               }
            if (total_menu_items)
               {
                  snprintf(p, (sizeof(msg)-(p-msg)), "%4d Menu items loaded.\n", total_menu_items);
                  p = &p[strlen(p)];
               }
            dm_error(msg, DM_ERROR_MSG);
         }
      DEBUG(fprintf(stderr, "Keydefs Time completed: (%d)\n", time(0));)
#ifdef WIN32
      /* If the key definitions were just loaded, force a window redraw. */
      store_cmds(dspl_descr,"rw", False); /* put commands where keypress will find them */
      fake_keystroke(dspl_descr);
#endif
      return(0); /* show we are done */
   }

} /* end of process_some_keydefs */



/************************************************************************

NAME:      install_keydef  - Put a key defintion (list of commands)
                             into the hash table.


PURPOSE:    This routine gets called by dm_kd.  After the kd has been
            processed, the result needs to be put in the hash table
            keyed by the key event which will extract it.

PARAMETERS:

   1.  the_key     -  pointer to char (INPUT)
                      This is the hash table key to store the keydef under.
                      It is a combination of the key pressed, the modifier
                      keys pressed (ctrl, shift, alt, etc.), and the event
                      type (KeyPress, KeyRelease).

   2. dmc          -  pointer to DMC  (INPUT)
                      This the first DM command structure in the linked list of
                      commands to be put in the hash table.
                      The hash table has a key and data,  The key is made from
                      the key, state, and event parms.  This parm is the data.

   3. update_server - int (INPUT)
                      This flag specifies whether the key defintion should be
                      updated in the server.  It is set to False during
                      initial keydef loading and True for interactive key
                      definition commands.

   4. dspl_descr    - pointer to DISPLAY_DESCR (INPUT)
                      The current display description is passed.  It is 
                      needed for property information, the hash table,
                      and the Display for X calls.

   5. curr_dm_list -  pointer to DMC  (INPUT)
                      This the list of DMC's currently being processed.
                      It is possible that a keydef could be executed which
                      redefines itself.  In this case, the pending DMC's
                      from the original list are freed.  We need to truncate
                      the list at the first one to be free to avoid 
                      accessing freed space.


FUNCTIONS :

   1.  Lookup the key name event.  If it already exists, replace its current
       data (list of command) with the new list of commands.

   2.  If the key name event does not already exist, build a new entry
       structure and put it in the hash table.

   3.  If requested via the update server flag, update the server copy
       of the key defintions.

RETURNED VALUE:

   rc        -  The returned flag indicates whether the key definition
                was successfully installed.
                 0  -  Everything worked
                 1  -  Error, dm error already called.

NOTES:
   1.  ENTRY is a type defined in search.h.  It is used by the
       system hsearch routine.  It contains two pointers.  One to
       the key and one to the data.


*************************************************************************/

int  install_keydef(char           *the_key,
                    DMC            *dmc,
                    int             update_server,
                    DISPLAY_DESCR  *dspl_descr,
                    DMC            *curr_dm_list)
{
ENTRY                 inserted_def;
ENTRY                *current_def;
char                 *inserted_key;
int                   rc;

inserted_def.key  = the_key;
DEBUG11(fprintf(stderr, "install_keydef:  key = 0x%s", the_key);)

if (dmc)
   dmc->any.temp = False;  /* is not temporary */

/***************************************************************
*  If there is a current definition,  reuse the data portion.
***************************************************************/
if ((current_def = (ENTRY *)hsearch(dspl_descr->hsearch_data, inserted_def, FIND)) != NULL)
   {
      /***************************************************************
      *  It is possible we are redefining a key which is currently
      *  being processed.  Truncate the list before we free the
      *  DMC elements.
      ***************************************************************/
      truncate_dm_list((DMC *)current_def->data, curr_dm_list);
      free_dmc((DMC *)current_def->data);
      current_def->data = (char *)dmc;
      rc = 0;
   }
else
   {
      inserted_key      = malloc_copy(the_key);
      if (!inserted_key)
         return(-1);
      inserted_def.key  = inserted_key;
      inserted_def.data = (char *)dmc;

      if ((current_def = (ENTRY *)hsearch(dspl_descr->hsearch_data, inserted_def, ENTER)) == NULL)
         rc = -1;
      else
         rc = 0;
   }

DEBUG11(if (rc == 0) fprintf(stderr, "  -  installed\n"); else fprintf(stderr, "  -  install FAILED\n"); )

/***************************************************************
*  
*  If requested update the server copy of the key defintions.
*  
***************************************************************/

if (update_server && rc == 0)
   change_keydef(current_def, dspl_descr->display, KEYDEF_PROP, dspl_descr->properties);


return(rc);

} /* end of install_keydef */



/************************************************************************

NAME:      dm_kd  - Execute a kd command structure.


PURPOSE:    This routine "does" a kd.  After the defintion has been
            parsed, it needs to be executed.

PARAMETERS:

   1. dmc          -  pointer to DMC  (INPUT)
                      This the first DM command structure for the key
                      definition.  If there are more commands on the
                      list, they are ignored by this routine.  That is this
                      routine does not look at dmc->next.

   2. update_server - int (INPUT)
                      This flag specifies whether the key defintion should be
                      updated in the server.  It is set to False during
                      initial keydef loading and True for interactive key
                      definition commands.

   3. dspl_descr    - pointer to DISPLAY_DESCR (INPUT)
                      The current display description is passed.  It is 
                      needed for property information, the hash table,
                      and the Display for X calls.


FUNCTIONS :

   1.  Parse the keyname to the the keysym, event, and the modifier mask.

   2.  If the data in the key definition is NULL, this is a request for
       data about that key definition.  Look up the definition and output
       it to the DM command window via dump_kd.

   3.  Prescan the defintion to see if it contains any prompts for input.
       such as   kd *h  cv /ce/help &'Help on: '  ke

   4.  If the prescan produced no results, parse the key definition.

   5.  If the parse worked, install the keydef.

RETURNED VALUE:
   rc   -  int
           The returned value shows whether a key definition was successfully
           loaded.
           0   -  keydef loaded
           -1  -  keydef not loaded, message already generated

*************************************************************************/

int   dm_kd(DMC               *dmc,
            int                update_server,
            DISPLAY_DESCR     *dspl_descr)
{
KeySym       keysym; /* which key or button            */
int          state;  /* input State, normal, cntl, etc */
int          event;  /* input  KeyPress or KeyRelease  */
int          rc;
DMC         *new_dmc;
ENTRY       *def_found;
ENTRY        sym_to_find;
char         the_key[KEY_KEY_SIZE];
char         work[80];
char         work_def[MAX_LINE+1];

/***************************************************************
*  parse the key name into the keysym, state, and event
*  event is KeyPress or KeyRelease.  If it fails, bail out.
***************************************************************/

rc = parse_keyname(dspl_descr->display,   /* input   */
                   dmc->kd.key,           /* input   */
                   &keysym,               /* output  */
                   &state,                /* output  */
                   &event,                /* output  */
                   dmc->kd.line_no);      /* input   */

DEBUG11(fprintf(stderr, "dm_kd: key name %s - 0x%X\n", dmc->kd.key, keysym);)

/* on failure, parse_keyname has called dm_error */
if (rc != 0)
   return(-1);

/***************************************************************
*  Generate the hash table key for this key definition.
***************************************************************/
snprintf(the_key, sizeof(the_key), KEY_KEY_FORMAT, keysym, state, event);

/***************************************************************
*  Check if this is a request to dump an existing keydef.
*  If so, dump it and we are done.
***************************************************************/
if (dmc->kd.def == NULL)
   {
      sym_to_find.key = the_key;
      def_found = (ENTRY *)hsearch(dspl_descr->hsearch_data, sym_to_find, FIND);
      if (def_found != NULL)
         dump_kd(NULL, (DMC *)def_found->data, 0 /* list them all */, False, NULL, dspl_descr->escape_char);
      else
         dm_error("", DM_ERROR_MSG);
      return(-1);  /* not really failure, but no keydef was loaded */
   }

/***************************************************************
*  See if any prompts need to be processed.  If so, build a
*  prompt type DMC.  Otherwise parse the line into a DMC structure.
***************************************************************/
strlcpy(work_def, dmc->kd.def, sizeof(work_def));

new_dmc = prompt_prescan(work_def, False, dspl_descr->escape_char); /* in prompt.c */
if (new_dmc == NULL)
   new_dmc = parse_dm_line(work_def, 0, dmc->kd.line_no, dspl_descr->escape_char, dspl_descr->hsearch_data);

if (new_dmc != NULL)
   {
      if (new_dmc == DMC_COMMENT)
         {
            /***************************************************************
            *  Special case, install a null command.
            *  Used when with  kd ^f1 ke  (undefining keys)
            ***************************************************************/
            new_dmc = (DMC *)CE_MALLOC(sizeof(DMC));
            if (!new_dmc)
               return(-1);
            new_dmc->any.next         = NULL;
            new_dmc->any.cmd          = DM_NULL;
            new_dmc->any.temp         = 0;
         }
      DEBUG4(
         fprintf(stderr, "Installing:  ");
         dump_kd(stderr, new_dmc, 0, False, NULL, dspl_descr->escape_char);
      )
      /***************************************************************
      *  If this is an ld (local definition command), do not update
      *  the server.
      ***************************************************************/
      if (dmc->any.cmd == DM_lkd)
         update_server = False;
      rc = install_keydef(the_key, new_dmc, update_server, dspl_descr, dmc);
      if (rc != 0)
         {
            snprintf(work, sizeof(work), "(%s) Unable to install key definition, internal error", dmsyms[dmc->kd.cmd].name);
            dm_error(work, DM_ERROR_LOG);
            free_dmc(new_dmc);
            rc = -1;  /* make sure it has the expected value */
         }
   }
else
   rc = -1;

return(rc);

}  /* end of dm_kd */


/*****************************************************************************
*
*   parse_hashtable_key - translate a hash table "keykey" back into keysym,
*                         state, and event data.
*
*   PARAMETERS:
*
*      keykey -   pointer to char (INPUT)
*                 This is the keykey string to translate.
*                 It is a value as set up by install keydef.
*
*      cmd    -   pointer to char (INPUT)
*                 This is the command name, it is used in error message generation
*                 if the parse fails.
*
*      keysym -   pointer to KeySym (OUTPUT)
*                 This is the returned keysym.  On failure it has the value
*                 NoSymbol defined in the keysym.h file.  Otherwise it is the
*                 keysym.
*
*      state  -   pointer to int (OUTPUT)
*                 This is the returned state mask.  It shows whether Shift,
*                 ctrl, Alt, or a combination of these needs to be pressed
*                 for the dm key definition being defined to be invoked.
*
*      event  -   pointer to int (OUTPUT)
*                 This is the event which will trigger this translation.
*                 VALUES (defined in /usr/include/X11/X.h)
*                 KeyPress
*                 KeyRelease
*                 ButtonPress
*                 ButtonRelease
*
*   FUNCTIONS:
*
*   1.  Use sscanf to parse the string into it's component pieces.
*
*   2.  If the scan fails, generate an error message and set the 
*       error values.
*
*
*********************************************************************************/

void  parse_hashtable_key(char       *keykey, /* input,  hash table entry */
                          char       *cmd,    /* input,  cmd name for error messages */
                          KeySym     *keysym, /* output, X keysym value */ 
                          int        *state,  /* output, State, normal, cntl, etc */
                          int        *event)  /* output, KeyPress or KeyRelease */
{
int          count;
char         msg[100];

if (keykey)
   count = sscanf(keykey, KEY_KEY_FORMAT, keysym, state, event);
else
   count = -1;

if (count != 3)
   {
      *keysym = NoSymbol;
      *state  = 0;
      *event  = 0;

      DEBUG(
      if (!is_alias_key(keykey) && !is_lsf_key(keykey) && !is_menu_item_key(keykey)) /* in alias.c  ind.c  pd.c*/
         {
            snprintf(msg, sizeof(msg), "(%s) Unable to parse internal keysym \"%s\"", cmd, (keykey ? keykey : "<NULL>"));
            dm_error(msg, DM_ERROR_LOG);
         }
      )
   }

} /* end of parse_hashtable_key */



/*****************************************************************************
*
*   parse_keyname     Translate a text key name to a KeySym, state mask, and 
*                     event.
*
*   PARAMETERS:
*
*      display -  pointer to Display (INPUT)
*                 This is the current display pointer.  It is used in X calls.
*
*      key    -   pointer to char (INPUT)
*                 This is the key name to translate.  It can be either an
*                 X key name such as accepted by XStringToKeysym; one of
*                 the XK_ symbols from /usr/include/X11/keysym.h; or an
*                 apollo DM key name.  Note that the mouse button key
*                 names are only supported as Apollo keynames m1, m2, etc.
*                 Key names may be prefixed with ^ for Ctrl or * for Alt (mod1).
*                 They may be suffixed with S for shift or U for key release.
*
*      keysym -   pointer to KeySym (OUTPUT)
*                 This is the returned keysym.  On failure it has the value
*                 NoSymbol defined in the keysym.h file.  Otherwise it is the
*                 keysym.
*
*      state  -   pointer to int (OUTPUT)
*                 This is the returned state mask.  It shows whether Shift,
*                 ctrl, Alt, or a combination of these needs to be pressed
*                 for the dm key definition being defined to be invoked.
*
*      event  -   pointer to int (OUTPUT)
*                 This is the event which will trigger this translation.
*                 VALUES (defined in /usr/include/X11/X.h)
*                 KeyPress
*                 KeyRelease
*                 ButtonPress
*                 ButtonRelease
*
*      line_no -  int (INPUT)
*                 This is the line number used in error message generation.
*                 For lines typed at the window, this is zero, for lines
*                 being processed from a command file, this is the line
*                 number in the file with the command.  Special routine iln
*                 which is defined in parsedm.c returns a null string for
*                 value zero and a formated line number for non-zero.
*
*   FUNCTIONS:
*
*   1.  Check for leading ^ and * characters and strip them off.  Save this
*       state information.
*
*   2.  Try to translate this keysym as an X key name (XStringToKeysym)
*
*   3.  Try to translate this keysym as an Apollo Key name.
*
*   4.  Strip of an S or U if they exist, and retry the translations.
*
*   RETURNED VALUE:
*
*   rc      -   int return code
*               This tells whether the translation worked or not.
*               Values:
*               0  -  Worked
*              -1  -  Failed, call to dm_error has already been made.
*
*
*********************************************************************************/

static int  parse_keyname(Display *display,     /* input   */
                          char    *key,         /* input   */
                          KeySym  *keysym,      /* output  */
                          int     *state,       /* output  */
                          int     *event,       /* output  */
                          int     line_no)      /* input   */
{
int     got_front = False;
int     possible_shift = 0;
char    msg[80];
char    work_key[40];
int     key_len;
int     num_value;
int     i;
static char   *unknown_key_format = "(%s%s) Unknown function key name";

/***************************************************************
*  
*  initialize the state and look for any of the up front state
*  modifiers.  Namely, control (^) and alc (*).
*  
***************************************************************/

*state = 0;
*event = KeyPress;

while(!got_front)
{
   if (*key == CNTL_PREFIX_CHAR)
      {
         *state |= ControlMask;
         key++;
      }
   else
      if (*key == MOD1_PREFIX_CHAR)
         {
            *state |= Mod1Mask;
            key++;
         }
      else
         if (*key == MOD2_PREFIX_CHAR)
            {
               *state |= Mod2Mask;
               key++;
            }
         else
            if (*key == MOD3_PREFIX_CHAR)
               {
                  *state |= Mod3Mask;
                  key++;
               }
            else
               if (*key == MOD4_PREFIX_CHAR)
                  {
                     *state |= Mod4Mask;
                     key++;
                  }
               else
                  if (*key == MOD5_PREFIX_CHAR)
                     {
                        *state |= Mod5Mask;
                        key++;
                     }
                  else
                     got_front = True;

} /* while processing prefix chars */

/***************************************************************
*  
*  Try to translate the name we have assuming no suffix characters.
*  
***************************************************************/

*keysym = translate_keyname(display, key, event, state);
if (*keysym != NoSymbol)
   return(0);


/***************************************************************
*  
*  No luck.  Look a suffixed
*  's' for shift or 'u' for keyrelease, then try again.
*  
***************************************************************/

strlcpy(work_key, key, sizeof(work_key));
key_len = strlen(work_key) - 1;

if (work_key[key_len] == 's' || work_key[key_len] == 'S')
   {
      possible_shift = ShiftMask;
      work_key[key_len--] = '\0';
   }
else
   if (work_key[key_len] == 'u' || work_key[key_len] == 'U')
      {
         *event = KeyRelease;
         work_key[key_len--] = '\0';
      }
   else
      if (work_key[0] != '#')
         {
            snprintf(msg, sizeof(msg), unknown_key_format, iln(line_no), key); /* iln in parsedm.c */
            dm_error(msg, DM_ERROR_BEEP);
            return(-1);
         }

*keysym = translate_keyname(display, work_key, event, state);
if (*keysym != NoSymbol)
   {
      *state |= possible_shift;
      return(0);
   }

/***************************************************************
*  
*  No luck.  maybe it had both an s and a u suffix.  Strip them off
*  's' for shift or 'u' for keyrelease, then try again.
*  
***************************************************************/

if (work_key[key_len] == 's' || work_key[key_len] == 'S')
   {
      possible_shift = ShiftMask;
      work_key[key_len] = '\0';
   }
else
   if (work_key[key_len] == 'u' || work_key[key_len] == 'U')
      {
         *event = KeyRelease;
         work_key[key_len] = '\0';
      }
   else
      if (work_key[0] != '#')
         {
            snprintf(msg, sizeof(msg), unknown_key_format, iln(line_no), key);  /* iln in parsedm.c */
            dm_error(msg, DM_ERROR_BEEP);
            return(-1);
         }

/***************************************************************
*  
*  Check the syntax #<keysym_value>.
*  
***************************************************************/

if (work_key[0] == '#')
   {
      if ((sscanf(&work_key[1], "%x%n", &num_value, &i) == 1) && (i == ((int)strlen(work_key) - 1)))
         {
            *keysym = num_value;
            *state |= possible_shift;
            return(0);
         }
   }


*keysym = translate_keyname(display, work_key, event, state);
if (*keysym != NoSymbol)
   {
      *state |= possible_shift;
      return(0);
   }
else
   {
      snprintf(msg, sizeof(msg), unknown_key_format, iln(line_no), key); /* iln in parsedm.c */
      dm_error(msg, DM_ERROR_BEEP);
      return(-1);
   }


}  /* end of parse_keyname */


/************************************************************************

NAME:      translate_keyname  - Attempt translation of a keyname using the
                                standard X translator and our own Apollo
                                keyname translator.


PURPOSE:    If a key definition contains prompts, which could theoretically
            be the command name, the command cannot yet be parsed.  Thus an
            internal prompt command (or list of them) is generated to 
            collect the required information which will be parsed later.

PARAMETERS:

   1. display      -  pointer to Display (INPUT)
                      This is the current display pointer.  It is used in X calls.

   2. key          -  pointer to char  (INPUT)
                      This the keyname to be looked for.

   3. event        -  pointer to int  (INPUT / OUTPUT)
                      This the X event type, KeyPress, KeyRelease, etc.
                      If a mouse key name is pressed, KeyPress is
                      translated to ButtonPress and KeyRelease is
                      translated to ButtonRelease.  This is done by
                      translate_apollo_keyname.

   4. state        -  pointer to int  (OUTPUT)
                      This variable describes the state of the 
                      key/button event to be detected.  That is
                      must any modifier keys be pressed.
                      This parameter is modified by translate_apollo_keyname.


FUNCTIONS :

   1.  Try the X translation routine.

   2.  Try out internal routine which handles the mouse keys and the
       old apollo key names.

RETURNED VALUE:
   keysym  - pointer to KeySym
             This is the keysym the name was tranlated.  If there is no
             translation, the X defined value NoSymbol is returned.


*************************************************************************/

static KeySym translate_keyname(Display   *display,
                                char      *key,
                                int       *event,
                                int       *state)
{
KeySym      keysym;

/***************************************************************
*  First see if this is a regular keyname.  If not, see if
*  it is one of the special Apollo keynames we know about.
*
*  Special case for the lf key which is a conflict between apollo
*  and non-apollo keyboards.  If we get a lf, make sure it is in
*  the keymap.  If not, assume the Apollo one.
***************************************************************/

DEBUG9(XERRORPOS)
keysym = XStringToKeysym(key);

DEBUG9(XERRORPOS)
if (keysym == XK_lf)
   if (!XKeysymToKeycode(display, keysym))
      keysym = NoSymbol;

if (keysym == NoSymbol)
   keysym = translate_apollo_keyname(key, event, state);


return(keysym);

} /* end of translate_keyname */



typedef struct {
   char        *apollo_key;
   KeySym       xkey;
   int          is_button;
   int          implied_state;
} TRANS_TBL;

static TRANS_TBL   key_table[] = {
                                  "m1",    Button1,        1,   0,   
                                  "m2",    Button2,        1,   0,   
                                  "m3",    Button3,        1,   0,   
                                  "m4",    Button4,        1,   0,   
                                  "m5",    Button5,        1,   0,   
                                  "l1",    XK_Select,      0,   0,   
                                  "l1s",   XK_Insert,      0,   ShiftMask,   
                                  "l2",    0x1000FF00,     0,   0,   
                                  "l3",    0x1000FF01,     0,   0,   
                                  "l1a",   0x1000FF02,     0,   0,   
                                  "l1as",  0x1000FF03,     0,   ShiftMask,   
                                  "l2a",   0x1000FF04,     0,   0,   
                                  "l2as",  XK_Undo,        0,   ShiftMask,   
                                  "l3a",   0x1000FF06,     0,   0,   
                                  "l3as",  0x1000FF05,     0,   ShiftMask,   
                                  "l4",    0x1000FF09,     0,   0,   
                                  "l5",    0x1000FF07,     0,   0,   
                                  "l5s",   0x1000FF08,     0,   ShiftMask,   
                                  "l6",    0x1000FF0A,     0,   0,   
                                  "l7",    0x1000FF0B,     0,   0,   
                                  "l8",    XK_Up,          0,   0,   
                                  "l9",    0x1000FF0C,     0,   0,   
                                  "la",    XK_Left,        0,   0,   
                                  "lb",    XK_Next,        0,   0,   
                                  "lc",    XK_Right,       0,   0,   
                                  "ld",    0x1000FF0D,     0,   0,   
                                  "le",    XK_Down,        0,   0,   
                                  "lf",    0x1000FF0E,     0,   0,   
                                  "r1",    0x1000FF0F,     0,   0,   
                                  "r2",    XK_Redo,        0,   0,   
                                  "r3",    0x1000FF10,     0,   0,   
                                  "r4",    0x1000FF10,     0,   0,   
                                  "r4s",   0x1000FF12,     0,   ShiftMask,   
                                  "r5",    0x1000FF13,     0,   0,   
                                  "r5s",   XK_Cancel,      0,   ShiftMask,   
                                  "r6",    XK_Pause,       0,   0,   
                                  "r6s",   XK_Help,        0,   ShiftMask,   
                                  "f0",    XK_F10,         0,   0,   
                                  "f1",    XK_F1,          0,   0,   
                                  "f2",    XK_F2,          0,   0,   
                                  "f3",    XK_F3,          0,   0,   
                                  "f4",    XK_F4,          0,   0,   
                                  "f5",    XK_F5,          0,   0,   
                                  "f6",    XK_F6,          0,   0,   
                                  "f7",    XK_F7,          0,   0,   
                                  "f8",    XK_F8,          0,   0,   
                                  "f9",    XK_F9,          0,   0,   
                                  "esc",   XK_Escape,      0,   0,   
                                  "tab",   XK_Tab,         0,   0,   
                                  "cr",    XK_Return,      0,   0,   
                                  "del",   XK_Delete,      0,   0,   
                                  "np0",   XK_KP_0,        0,   0,   
                                  "np1",   XK_KP_1,        0,   0,   
                                  "np2",   XK_KP_2,        0,   0,   
                                  "np3",   XK_KP_3,        0,   0,   
                                  "np4",   XK_KP_4,        0,   0,   
                                  "np5",   XK_KP_5,        0,   0,   
                                  "np6",   XK_KP_6,        0,   0,   
                                  "np7",   XK_KP_7,        0,   0,   
                                  "np8",   XK_KP_8,        0,   0,   
                                  "np9",   XK_KP_9,        0,   0,   
                                  "np+",   XK_KP_Add,      0,   0,   
                                  "np-",   XK_KP_Subtract, 0,   0,   
                                  "npe",   XK_KP_Enter,    0,   0,   
                                  "np.",   XK_KP_Decimal,  0,   0,
                                  NULL,    0,              0,   0 };



/************************************************************************

NAME:      translate_apollo_keyname  - convert Apollo type key names
                                       to keysyms.


PURPOSE:    To support things which look like Apollo key name, this routine
            handles look up of these type names.  For example, the X keyname
            for the keyname is "Escape" (the capital is required).  The Apollo
            name for the key is "esc".  Wherever possible, these names are
            converted.  All the mouse button names (m1, m2, etc are parsed here.

PARAMETERS:

   1. key          -  pointer to char  (INPUT)
                      This the keyname to be looked for.

   2. event_type   -  pointer to int  (INPUT / OUTPUT)
                      This the X event type, KeyPress, KeyRelease, etc.
                      If a mouse key name is pressed, KeyPress is
                      translated to ButtonPress and KeyRelease is
                      translated to ButtonRelease.

   2. state        -  pointer to int  (OUTPUT)
                      This variable describes the state of the 
                      key/button event to be detected.  That is
                      must any modifier keys be pressed.
                      This data is taken from the key table.

FUNCTIONS :

   1.  Translate the key name to lower case as Apollo keynames are case
       insensitive.

   2.  Scan the list of names to see if this is an Apollo keyname.
       If is is and is a mouse name, convert the event type parm.

RETURNED VALUE:
   keysym -   KeySym (an int)
              This is the keysym this name translated to.
              X defined number NoSymbol is used if match is found.


*************************************************************************/

static KeySym translate_apollo_keyname(char      *key,
                                       int       *event_type,
                                       int       *state)
{

char    work_key[80];
int     i;

strlcpy(work_key, key, sizeof(work_key));
(void) case_line('l', work_key, 0, sizeof(work_key));

for (i = 0; key_table[i].apollo_key != NULL; i++)
   if (strcmp(work_key, key_table[i].apollo_key) == 0)
      {
         if (key_table[i].is_button)
            if (*event_type == KeyPress)
               *event_type = ButtonPress;
            else
               *event_type = ButtonRelease;
         *state |= key_table[i].implied_state;
         return(key_table[i].xkey);
      }

return(NoSymbol);
                          
} /*  end of translate_apollo_keyname */


/************************************************************************

NAME:      dm_cmdf  - Do a cmdf command.


PURPOSE:    This routine reads a file containing DM commands and returns
            the linked list of the parsed command structures.

PARAMETERS:

   1. dmc             - pointer to DMC  (INPUT)
                        This the cmdf command structure.

   2.  display        - pointer to Display (Xlib type) (INPUT)
                        This is the token whichr represents the display in Xlib
                        calls.  NULL can be passed during .Cekeys processing since
                        a real file name will always be passed.  This is as opposed
                        to 'cmdf -p' which reads a paste buffer and thus needs access
                        to X in open_paste_buffer.

   3.  main_x_window  - Window (Xlib type) (INPUT)
                        This is X window id of the main Ce window.  The value None
                        can be passed during .Cekeys processing or any time the 
                        '-p' option to cmdf is not used.

   4.  escape_char    - char (INPUT)
                        This is escape character needed for parsing.  It is a '\'
                        in UNIX expression mode and '@' in Aegis expression mode.

   5.  hash_table     - pointer to void (INPUT)
                        This is the hash table used in parsing Aliases.  It comes off
                        the display description.

   6.  prompt_allowed - int (INPUT)
                        This flag is true except when called during .Cekeys processing
                        as an include.  Prompts are not allowed, so we parse the line
                        without prescanning for a prompt.

FUNCTIONS :
   1.  Try to open the command file.  If it fails, flag the fact.

   2.  Read and parse each command line.

   3.  Collect the parsed command structures into a linked list.


RETURNED VALUE:
   dm_list -  pointer to DMC
              This is the collected list of commands.


*************************************************************************/

DMC  *dm_cmdf(DMC             *dmc,
              Display         *display,
              Window           main_x_window,
              char             escape_char,
              void            *hash_table,
              int              prompt_allowed)
{
int          len;
DMC         *new_dmc;
DMC         *first_dmc = NULL;
DMC         *last_dmc = NULL;
char         cmdline[MAX_LINE+2];
FILE        *cmd_file;
int          prompt_flag= False;
int          line_no = 0;
char        *hold_cmdf_path = NULL;
char         cmdf_path[MAXPATHLEN];
#ifdef WIN32
char                  separator = ';';
#else
char                  separator = ':';
#endif

/***************************************************************
*  Search the path if needed.
***************************************************************/
if (!dmc->cmdf.dash_p && dmc->cmdf.path && *dmc->cmdf.path)
   {
      /***************************************************************
      *  If we find the cmdf in the search path, swap it temporarily in
      *  the command.
      ***************************************************************/
      search_path_n(dmc->cmdf.path, sizeof(cmdf_path), cmdf_path, "CE_CMDF_PATH", separator); /* in normalize.h */
      if ((cmdf_path[0] != '\0') && (strcmp(cmdf_path, dmc->cmdf.path) != 0))
         {
            hold_cmdf_path = dmc->cmdf.path;
            dmc->cmdf.path = cmdf_path;
         }
   }

/***************************************************************
*  Open the command file for input.  Quit on failure.
***************************************************************/
cmd_file = open_paste_buffer(dmc, display, main_x_window, 0, escape_char);
if (hold_cmdf_path)
   dmc->cmdf.path = hold_cmdf_path; /* put the command back together */
if (cmd_file == NULL)
   return(NULL);

/***************************************************************
*  Gather one command, read lines until we get a whole command
***************************************************************/
while(ce_fgets(cmdline, sizeof(cmdline), cmd_file) != NULL)
{
   len = strlen(cmdline);
   if (cmdline[len-1] == '\n')
      cmdline[--len] = '\0';
   if (cmdline[len-1] == '\r')
      cmdline[--len] = '\0';

   if (prompt_allowed)
      new_dmc = prompt_prescan(cmdline, True, escape_char); /* in prompt.c */
   else
      new_dmc = NULL;
   if (new_dmc == NULL)
      new_dmc = parse_dm_line(cmdline, True, ++line_no, escape_char, hash_table);  /* parse the command line, show temp */

   if (new_dmc != DMC_COMMENT && new_dmc != NULL)
      {
         if (first_dmc == NULL)
            first_dmc = new_dmc;
         else
            if (!prompt_flag)
               last_dmc->next = new_dmc;
            else
               {
                  last_dmc->prompt.delayed_cmds = new_dmc;
                  prompt_flag = False;
               }
         last_dmc = new_dmc;
 
         /***************************************************************
         *  If the parse_dm_line made a list of dm cmds, got to the end.
         ***************************************************************/
         while(last_dmc->next != NULL)
            last_dmc = last_dmc->next;

         /***************************************************************
         *  If a prompt was added set the flag so the next cmd will be
         *  chained off the prompt.
         ***************************************************************/
         if (last_dmc->any.cmd == DM_prompt)
            {
               prompt_flag = True;
               DEBUG4(fprintf(stderr, "Found a prompt, delaying commands\n");)
            }
      }

} /* end of while more data */


ce_fclose(cmd_file);  /* deals with paste buffers */

return(first_dmc);

}  /* end of dm_cmdf */





/************************************************************************

NAME:      dm_kk - DM Key Key command

PURPOSE:    This routine is not in the original DM.  It causes the
            key event to be displayed on the DM output window.

PARAMETERS:

   1.  event       - pointer to XEvent (INPUT)
                     This is the event to be processed.  It is either
                     a keypress event or a mouse buttonpress event.


FUNCTIONS :

   1.   For a keypress, look up the keysym, adds the modifier data and
        output the line.

   2.   For a buttonpress, format the button and modifier data and output
        the line.

OUTPUTS:
        Output is written to the DM output window.


*************************************************************************/

void  dm_kk(XEvent     *event)
{
char                  buff[256];
int                   len;
KeySym                keysym = 0;
static char           key_name[MAX_KEY_NAME];
char                 *key_string;
char                  key_work[MAX_KEY_NAME];
char                 *target;
char                 *p;

/***************************************************************
*  
*  Make sure the first char output is a blank.  This makes finds
*  in the DM output window work.
*  
***************************************************************/

buff[0] = ' ';
target = &buff[1];


if (event->type == KeyPress)
   {
      DEBUG9(XERRORPOS)
      len = XLookupString((XKeyEvent *)event,
                           key_name,
                           sizeof(key_name),
                           &keysym,
                           NULL);

      key_name[len] = '\0';

      DEBUG9(XERRORPOS)
      key_string = XKeysymToString(keysym);
      if (key_string == NULL)
         {
            snprintf(key_work, sizeof(key_work), "#%X - keycode 0x%02X", keysym, event->xkey.keycode);
            key_string = key_work;
         }

      if (event->xkey.state)
         add_modifier_chars(key_string, event->xkey.state, target, (sizeof(buff)-(target-buff)));
      else
         strlcpy(target, key_string, (sizeof(buff)-(target-buff)));

      if (((p = getenv("CE_FULL_KK")) != NULL) && *p)
         {
            target = &target[strlen(target)]; /* point to the null at end of string */
            if (len)
               if (isprint(key_name[0]))
                  snprintf(key_work, sizeof(key_work), "default %ses '%s'", (((event->xkey.state & ~ShiftMask) != 0) ? "ISOLATIN " : ""), key_name);
               else
                  snprintf(key_work, sizeof(key_work), "default %ser %s", (((event->xkey.state & ~ShiftMask) != 0) ? "ISOLATIN " : ""), hex_char(key_name, len));
            else
               strlcpy(key_work, "No es string", sizeof(key_work));
            snprintf(target, (sizeof(buff)-(target-buff)), " - keycode: 0x%X, %s ", event->xkey.keycode, key_work);
         }

      dm_error(buff, DM_ERROR_MSG);
   }
else
   if (event->type == ButtonPress)
      {
         snprintf(key_work, sizeof(key_work), "m%d", event->xbutton.button);
         if (event->xbutton.state)
            add_modifier_chars(key_work, event->xbutton.state, target, (sizeof(buff)-(target-buff)));
         else
            strlcpy(target, key_work, (sizeof(buff)-(target-buff)));

         dm_error(buff, DM_ERROR_MSG);
      }
   else
      {
         snprintf(buff, sizeof(buff), "Unexpected event passed to dm_kk, %d", event->type);
         dm_error(buff, DM_ERROR_LOG);
      }
} /* end of dm_kk */


static void load_default_keydefs(DISPLAY_DESCR     *dspl_descr)
{
int          i;
DMC         *new_dmc;


/***************************************************************
*  
*  Walk the list of default key defintions and process each one.
*  We know there are only kd commands since we coded the 
*  defkds.h file.
*  
***************************************************************/

for (i = 0; default_key_definitions[i] != NULL; i++)
{
   DEBUG4(fprintf(stderr, "default definition: \"%s\"\n", default_key_definitions[i]);)
   new_dmc = parse_dm_line(default_key_definitions[i],
                           True,  /* show temp definition */
                           0,     /* no line number */
                           dspl_descr->escape_char,
                           dspl_descr->hsearch_data);
   if (new_dmc != NULL)
      {
         dm_kd(new_dmc, False, dspl_descr); /* always a kd */
         free_dmc(new_dmc);
      }

} /* end of for loop walking default_key_definitions */

} /* end of load_default_keydefs */


/************************************************************************

NAME:      check_required_keydefs - Make sure certain key defintions exist

PURPOSE:    This routine make sure key definitions such as return and enter
            exist.  This prevents a bad key definition file from making
            it so there is no way to get out.  You can always type wc since
            typing letters are defaulted, but without an en, there is no way
            to execute the command.


PARAMETERS:

   None


FUNCTIONS :

   1.   Loop through the required keysyms to make sure they are defined.
        If they have some sort of definition, all is cool.

   2.   If a keysym is undefined, force the definition.

OUTPUTS:
        The table of key defintions is modified both in ths program and in the server.

NOTES:
   1.   This routine is called after all the keysyms have been read in.

*************************************************************************/

typedef struct {
   KeySym       xkey;
   char        *defn;
} FORCED_DEF;

static FORCED_DEF   forced_def[] = {XK_KP_Enter,       "kd KP_Enter   en ke",
                                    XK_Return,         "kd Return     en ke",
                                    0,                 NULL};

static void check_required_keydefs(DISPLAY_DESCR     *dspl_descr)
{

int                   i;
char                  the_key[KEY_KEY_SIZE];
ENTRY                 sym_to_find;
DMC                  *new_dmc;
ENTRY                *cur_ent;

for (i = 0; forced_def[i].xkey; i++)
{
   snprintf(the_key, sizeof(the_key), KEY_KEY_FORMAT, forced_def[i].xkey, 0, KeyPress);
   sym_to_find.key = the_key;

   /***************************************************************
   *  
   *  If the important keysym from the table is not defined, define it.
   *  
   ***************************************************************/
   if ((ENTRY *)hsearch(dspl_descr->hsearch_data, sym_to_find, FIND) == NULL)
      {
         DEBUG4(fprintf(stderr, "forced defintion: \"%s\"\n", forced_def[i].defn);)
         new_dmc = parse_dm_line(forced_def[i].defn,
                                 True,  /* temp DMC structures */
                                 0,     /* no line number      */
                                 dspl_descr->escape_char,
                                 dspl_descr->hsearch_data);
         if (new_dmc != NULL)
            {
               dm_kd(new_dmc, False, dspl_descr); /* always a kd */
               free_dmc(new_dmc);
            }

      }
}

/***************************************************************
*  
*  Make sure a reasonable set if definitions is loaded.
*  RES 9/8/95
***************************************************************/

i = 0;
for (cur_ent = hlinkhead(dspl_descr->hsearch_data); cur_ent != NULL; cur_ent = (ENTRY *)cur_ent->link)
   i++;

if (i < 15)
   load_default_keydefs(dspl_descr);

} /* end of check_required_keydefs */


/************************************************************************

NAME:      add_modifier_chars - DM Key Key command support routine.

PURPOSE:    This routine adds the modifier characters to the a key name.

PARAMETERS:

   1.  key_string  - pointer to char (INPUT)
                     This is the key name.  It is copied to the target
                     string.

   2.  state       -  unsigned int (INPUT)
                     This is the state mask of the key being evaluatated.

   3.  target     -  pointer to char (OUTPUT)
                     The key_string with appropriate modfier prefixes and
                     suffix, is placed in this string.


FUNCTIONS :

   1.   Check the state for each used modifier and put that prefix
        into the target string.

   2.   Copy the keysym to the target string after the prefixes.

   3.   Put the S for shift on the end if it is needed.


*************************************************************************/

void add_modifier_chars(char            *key_string, /* input  */
                        unsigned int     state,      /* input  */
                        char            *target,     /* output */
                        int              max_target) /* input  */
{
int      pos = 0;


if (state & ControlMask)
   target[pos++] = CNTL_PREFIX_CHAR;

if (state & Mod1Mask)
   target[pos++] = MOD1_PREFIX_CHAR;

if (state & Mod2Mask)
   target[pos++] = MOD2_PREFIX_CHAR;

if (state & Mod3Mask)
   target[pos++] = MOD3_PREFIX_CHAR;

if (state & Mod4Mask)
   target[pos++] = MOD4_PREFIX_CHAR;

if (state & Mod5Mask)
   target[pos++] = MOD5_PREFIX_CHAR;

if ((strlen(key_string) + strlen(target)) < max_target)
   strlcpy(&target[pos], key_string, max_target);

if (state & ShiftMask)
   strlcat(target, "S", max_target);


} /* end of add_modifier_chars */


static char  *hex_char(char *str, int len)
{
static char   target[128];
int           i;

target[0] = '\0';

for (i = 0; (i < len) && (i < (sizeof(target)/2)-2); i++)
   snprintf(&target[i*2], (sizeof(target)-(i*2)), "%02X", str[i]);

return(target);

} /* end of hex_char */


/************************************************************************

NAME:      lookup_key - Convert a key name to command list

PURPOSE:    This routine converts a key name like ^c to the
            dmc list of commands.  It is used in ce_merge.

PARAMETERS:

   1.  key         - pointer to char (INPUT)
                     This is the key name.  For example ^c, for F19

   2.  dspl_descr  - pointer to DISPLAY_DESCR (INPUT)
                     This is the current display description.

   3.  line_no     -  int (OUTPUT)
                      This is the line number for error messages.


FUNCTIONS :

   1.   Parse the key name.

   2.   Convert it to keykey format (hash table lookup)

   3.   Look up the name in the hash table and return the result.

REURNED VALUE:
   dmc    -  pointer to DMC
             NULL  - key has no definition
             !NULL - list of DMC's for this key

*************************************************************************/

static DMC   *lookup_key(char              *key,
                         DISPLAY_DESCR     *dspl_descr,
                         int                line_no)
{
KeySym                keysym; /* which key or button            */
int                   state;  /* input State, normal, cntl, etc */
int                   event;  /* input  KeyPress or KeyRelease  */
int                   rc;
ENTRY                *def_found;
ENTRY                 sym_to_find;
char                  the_key[KEY_KEY_SIZE];

/***************************************************************
*  parse the key name into the keysym, state, and event
*  event is KeyPress or KeyRelease.  If it fails, bail out.
***************************************************************/

rc = parse_keyname(dspl_descr->display,   /* input   */
                   key,                   /* input   */
                   &keysym,               /* output  */
                   &state,                /* output  */
                   &event,                /* output  */
                   line_no);              /* input   */

DEBUG11(fprintf(stderr, "lookup_key: key name %s - 0x%X\n", key, keysym);)

/* on failure, parse_keyname has called dm_error */
if (rc != 0)
   return(NULL);

/***************************************************************
*  Generate the hash table key for this key definition.
***************************************************************/
snprintf(the_key, sizeof(the_key), KEY_KEY_FORMAT, keysym, state, event);

/***************************************************************
*  Look up the key and return it.
***************************************************************/
sym_to_find.key = the_key;
def_found = (ENTRY *)hsearch(dspl_descr->hsearch_data, sym_to_find, FIND);
if (def_found != NULL)
   return((DMC *)def_found->data);
else
   return(NULL);

} /* end of lookup_key */


/************************************************************************

NAME:      ce_merge - Merge old and new key definitions.

PURPOSE:    This routine is the main processing line when
            Ce is called with the name ce_merge.

PARAMETERS:

   1.  dspl_descr  - pointer to DISPLAY_DESCR (INPUT)
                     This is a partially initialized display
                     description.  No windows have been created
                     yet.

FUNCTIONS :

   1.   Make sure the current key definitions are loaded.

   2.   Get the name of the target .Cekeys file

   3.   Parse each key definition or alias  passed in via stdin and see if
        the name is already in use.  If not, append it to the .Cekeys file.

REURNED VALUE:
   rc    -  int
            0  -  everything worked
            !0 -  merge failed, error already output

*************************************************************************/

int  ce_merge(DISPLAY_DESCR *dspl_descr)
{
char                 *home;
char                  homedir[MAXPATHLEN];
char                 *key_path;
char                  key_path_work[MAXPATHLEN];
int                   found;
DMC                  *new_dmc;
int                   line_no = 0;
int                   kd_count = 0;
char                  msg[512];
char                  cmdline[MAX_LINE+1];
int                   len;
int                   rc;

/***************************************************************
*  
*  Load up the key current key defintions.
*  
***************************************************************/

if (RELOAD_PARM)
   dm_delsd(dspl_descr->display, KEYDEF_PROP, dspl_descr->properties);
init_dm_keys(APP_KEYDEF, TABSTOPS, dspl_descr); /* in kd.c */
while(process_some_keydefs(dspl_descr));
XFlush(dspl_descr->display);


/**************************************************************
*  
*  Get the name of the keydef file.  If it is readable,
*  read it in and execute the keydefs.
*
*  Search for keydefs:
*  1.  If environment var CEKEYS is set and readable, use it.
*  2.  If $HOME/.Cekeys exists and is readable, use it.
*  3.  Out of luck, flag an error.
*  
***************************************************************/
key_path = getenv(KEYDEF_ENV_NAME);
if (key_path && (key_path[0] == '~'))
   key_path = translate_tilde(key_path);


if (key_path == NULL || key_path[0] == '\0' || access(key_path, 04) != 0) 
   {   
      if ((key_path != NULL) && (key_path[0] != '\0'))
         {
            snprintf(msg, sizeof(msg), "%s: %s", key_path, strerror(errno));
            dm_error(msg, DM_ERROR_MSG);
         }
      home = get_home_dir(homedir, NULL);
      if (home != NULL)
         {
            snprintf(key_path_work, sizeof(key_path_work), "%s/%s", home, KEYDEF_HOME_FILE);
            if (access(key_path_work, 00) == 0) 
               key_path = key_path_work;
         }
   }

if ((key_path != NULL) && (access(key_path, 04) == 0)) 
   {
      DEBUG1(fprintf(stderr, "Using key definition file %s\n", key_path); )
   }
else
   {
      if (key_path != NULL)
         {
            snprintf(msg, sizeof(msg), "%s: %s", key_path, strerror(errno));
            dm_error(msg, DM_ERROR_MSG);
         }
      else
         {
            snprintf(msg, sizeof(msg), "Unable to find key definition file %s - run ce_init\n", (key_path ? key_path : "<NULL>"));
            dm_error(msg, DM_ERROR_MSG);
         }
      return(2);
   }

/***************************************************************
*  
*  Read each key definition supplied via stdin and process it.
*  Determine if it needs to be added.
*  
***************************************************************/

while(fgets(cmdline, sizeof(cmdline), stdin) != NULL)
{
   len = strlen(cmdline);

   if (cmdline[len-1] == '\n')
      cmdline[--len] = '\0';

   new_dmc = parse_dm_line(cmdline, 1, ++line_no, dspl_descr->escape_char, dspl_descr->hsearch_data);  /* parse the command line, show temp */

   /***************************************************************
   *  Make sure there are only KD commands and WDF commands and WDC commands.
   *  CMDF commands are also processed, as includes.
   *  This code only checks the first command on the line.  We do not
   *  support multiple key definitions on a line for purposes of the merge.
   ***************************************************************/
   if (!new_dmc || (new_dmc == DMC_COMMENT))
      continue; /* ignore comments */

   if ((new_dmc->any.cmd != DM_kd)  &&
       (new_dmc->any.cmd != DM_wdf) &&
       (new_dmc->any.cmd != DM_wdc) &&
       (new_dmc->any.cmd != DM_mi)  &&
       (new_dmc->any.cmd != DM_lsf) &&
       (new_dmc->any.cmd != DM_alias))
      {
         if (new_dmc->any.cmd != DM_NULL)
            {
               snprintf(msg, sizeof(msg), "Ce cmd \"%s\", invalid in keydef file (%s\"%s\")",
                            dmsyms[new_dmc->any.cmd].name, iln(line_no), cmdline);
               dm_error(msg, DM_ERROR_MSG);
               DEBUG(fprintf(stderr, "Invalid cmd id = %d\n", new_dmc->any.cmd);)
            }
      }
   else
      {
         /***************************************************************
         *  Normal case: a kd, alias, wdf, or wdc command, ignore the wdc and wdf,s
         ***************************************************************/
         if ((new_dmc->any.cmd == DM_kd) || (new_dmc->any.cmd == DM_alias))
            {
               if (new_dmc->any.cmd == DM_kd)
                  found = (lookup_key(new_dmc->kd.key, dspl_descr, line_no) != NULL);
               else
                  found = (lookup_alias(new_dmc->alias.key, dspl_descr->hsearch_data) != NULL);
               if (!found)
                  {
                     rc = add_line(cmdline, key_path);
                     if (rc != 0)
                        return(rc);
                     kd_count++;
                  }
            }
         else
            {
               snprintf(msg, sizeof(msg), "%s command line ignored: (%s\"%s\")", dmsyms[new_dmc->any.cmd].name, iln(line_no), cmdline);
               dm_error(msg, DM_ERROR_MSG);
            }
      }
} /* end of while more data */

   
fprintf(stderr, "%d key definitions or aliases added\n", kd_count);

return(0);
} /* end of ce_merge */


/************************************************************************

NAME:      add_line - Append a line to the .Cekeys file

PURPOSE:    This routine add's new key definitions to the .Cekeys
            file.  It is used by ce_merge.

PARAMETERS:

   1.  line        - pointer to char (INPUT)
                     This is the line to be added.

   2.  key_path    - pointer to char (INPUT)
                     This is the name of the key file.

FUNCTIONS :

   1.   Open the key file if it is not already open.

   2.   Append the header comment if necessary.

   3.   Append the new key definition.

REURNED VALUE:
   rc    -  int
            0  -  everything worked
            !0 -  Open failed (can only occur on first call)

*************************************************************************/

static int add_line(char  *line, char *key_path)
{
static FILE          *stream = NULL;
char                  msg[512];

if (stream == NULL)
   {
      if ((stream = fopen(key_path, "a")) == NULL)
         {
            snprintf(msg, sizeof(msg), "Cannot open %s for output (%s)", key_path, strerror(errno));
            dm_error(msg, DM_ERROR_MSG); 
            return(errno);
         }
      else
         fprintf(stream, "\n\n############### Lines added by ce_merge %s #####################\n\n", VERSION);
   }

fputs(line, stream);
putc('\n', stream);
return(0);

} /* end of add_line */


/************************************************************************

NAME:      truncate_dm_list - Check for pending dmcs being freed by a kd or alias

PURPOSE:    This routine scans the list of pending dmc's (dm_list from keypress.c)
            and compares it against the list of dmc's being freed as part
            of an alias redefinition or key redefinition.  If a dmc being
            freed is on the list awaiting execution, the execution list is
            truncated to prevent a memory fault.

PARAMETERS:
   1.  kill_dmc    - pointer to DMC (INPUT)
                     This is first dmc in the list being freed

   2.  curr_dmc    - pointer to DMC (INPUT/OUTPUT)
                     This is the list of dmc's being executed.
                     The first one on the list is the alias or kd
                     currently being processed.

FUNCTIONS :

   1.   Look at each pending dmc after the first (outer loop)

   2.   Look at each dmc to be freed.

   3.   If a dmc to be freed is on the pending list, truncate
        the pending list just before the one to be freed.


*************************************************************************/

static void truncate_dm_list(DMC    *kill_dmc,
                             DMC    *curr_dmc) /* the kd or alias dmc being executed */
{
DMC                  *last_dmc;
DMC                  *dmc1;
DMC                  *dmc2;

last_dmc = curr_dmc;
for (dmc2 = curr_dmc->next; dmc2; dmc2 = dmc2->next)   
{
   for (dmc1 = kill_dmc; dmc1; dmc1 = dmc1->next)
      if (dmc1 == dmc2)
         {
            DEBUG4(
               fprintf(stderr, "truncate_dm_list: pending cmds being freed, modified kd is %s:  ", (last_dmc->any.temp ? "TEMP" : "NOT TEMP"));
               dump_kd(stderr, dmc2, False, False, NULL, '@');
            )
            last_dmc->next = NULL;
            return;
         }

   last_dmc = dmc2;
}

} /* end of truncate_dm_list */


/************************************************************************

NAME:      dm_eval  - do symbolic substitution and parse a line of data


PURPOSE:    This routine does symbolic substitution of environment variables
            and parses the command.  The new dmc list is returned.


PARAMETERS:
   1. dmc          -  pointer to DMC  (INPUT)
                      This the first DM command structure for the eval
                      command being executed.

   2. dspl_descr    - pointer to DISPLAY_DESCR (INPUT)
                      The current display description is passed.  It is 
                      needed for the escape character and the hash table.

FUNCTIONS :
   1.  Get a copy of the passed line and substitute the environment variables.

   2.  Parse the resultant command and return the linked list of DMC's

RETURNED VALUE:
   new_dmc   -  pointer to DMC
                The returned value is the DMC list corresponding to the passed
                command.
                NULL - Failure, message already output.

*************************************************************************/

DMC   *dm_eval(DMC               *dmc,
               DISPLAY_DESCR     *dspl_descr)
{
DMC         *new_dmc;
char         msg[512];
char         work[MAX_LINE+1];

/***************************************************************
*  parse the key name into the keysym, state, and event
*  event is KeyPress or KeyRelease.  If it fails, bail out.
***************************************************************/

strlcpy(work, dmc->eval.parm, sizeof(work));
if (substitute_env_vars(work, sizeof(work), dspl_descr->escape_char) != 0)
   {
      snprintf(msg, sizeof(msg), "(%s) Symbolic substitution expands to more than %d characters, aborted", dmsyms[dmc->eval.cmd].name, sizeof(work));
      dm_error(work, DM_ERROR_BEEP);
      return(NULL);
   }

new_dmc = prompt_prescan(work, True, dspl_descr->escape_char); /* in prompt.c */
if (new_dmc == NULL)
   new_dmc = parse_dm_line(work, 0, 0, dspl_descr->escape_char, dspl_descr->hsearch_data);

return(new_dmc);

}  /* end of dm_eval */




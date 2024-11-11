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
*  module getxopts.c
*
*  Routines:
*         getxopts    -   parse the options in the cmd string and
*                         merge them with the default options
*
*         set_wm_class_hints - Set up the class name resource hints for the window manager
*
*
*  Internal:
*         GetUsersDatabase - get the users x databases and merge
*                            them in the order prescribed by X.
*         search_xfilesearchpath - Parse and search XFILESEARCHPATH
*         GetHomeDir             - Get the users home directory
*
***************************************************************/

#include <stdio.h>             /*  /usr/include/stdio.h           */
#include <string.h>            /*  /usr/include/string.h          */
#include <errno.h>             /*  /usr/include/errno.h           */

#ifdef WIN32
#include <sys/types.h>
#include <winsock.h>

#ifdef WIN32_XNT
#include "xnt.h"
#else
#include "X11/Xatom.h"         /*  /usr/include/X11/Xatom.h       */
#include "X11/Xlib.h"          /*  /usr/include/X11/Xlib.h        */
#endif
#else
#include <X11/Xatom.h>         /*  /usr/include/X11/Xatom.h       */
#include <X11/Xlib.h>          /*  /usr/include/X11/Xlib.h        */
#endif

#ifndef WIN32
#include <pwd.h>               /*  /usr/include/pwd.h             */
#endif

#if !defined(OMVS) && !defined(WIN32)
#include <sys/param.h>         /*  /usr/include/sys/param.h       */
#endif

#if defined(FREEBSD) || defined(OMVS) || defined(WIN32)
#include <stdlib.h>            /*  /usr/include/stdlib.h   RES  4/18/98 */
#else
#include <malloc.h>            /*  /usr/include/malloc.h           */
#endif

#ifdef WIN32
#include <io.h>

#define access _access
#define R_OK 04

#ifndef MAXPATHLEN
#define MAXPATHLEN	256
#endif
#else
#include <unistd.h>

#ifndef MAXPATHLEN
#define MAXPATHLEN	1024
#endif
#endif /* WIN32 */

#ifndef MIN
#define MIN(a,b)  ((a) < (b) ? (a) : (b))
#endif

#include "getxopts.h"
#include "debug.h"

#if !defined(FREEBSD) && !defined(OMVS) && !defined(WIN32)
char   *getenv(const char *name);
int    *putenv(const char *name);
#endif

#define  DEFAULT_OPTS       "/usr/lib/X11/app-defaults/"
#define  DEFAULT_LANG_OPTS  "/usr/lib/%s/app-defaults/%s"


char MyDisplayName[256];


static void  GetUsersDatabase(char              *classname,
                              Display           *display,
                              XrmDatabase       *rDB,
                              int                reload_app_defaults);

static char  *search_xfilesearchpath(char    *class_name);

#ifdef ARPUS_CE
static XrmDatabase ce_XrmGetFileDatabase(Display   *display,
                                         char      *filename,
                                         Atom       target_atom);
#endif

#define MAX_X_NAME 256



/************************************************************************

NAME:      getxopts

PURPOSE:    This routine will extract the arguments from the parm string
            and merge them with the data in the Xdefaults type files.

PARAMETERS:

   1.  opTable   - array of XrmOptionDescRec (INPUT)
                   This is the description of the parameters.  It describes
                   parms and their defaults.  Each entry contains the
                   argument header, the resource specification name (eg .background)
                   the type of argument, and a default value.  Note that the
                   resource specification name must start with a '.'.  The '*'
                   prefix does not always work.  For an example, see the test
                   main routine at the end of this file.

   2.  op_count -  int (INPUT)
                   This is the count of the number of array elements in opTable.

   3.  defaults -  pointer to array of pointer to char (INPUT)
                   These values are loaded into output parm parm_values as defaults.
                   The referenced array matches element for element with optTable and
                   with parm_values.

   4.  argc     -  pointer to int (INPUT / OUTPUT)
                   This is the argc from the command line.  It is modified by
                   the X routines to get rid of parms they process.

   5.  argv     -  pointer to array of char (INPUT / OUTPUT)
                   This is the argv from the command line.  It is modified by
                   the X routines to get rid of parms they process.

   6.  parm_values  -  pointer to array of pointer to char (OUTPUT)
                   The values are loaded into this array.  If a resource name
                   appears twice in opTable, the first is assigned a value.
                   the second is set to null.  The value is in malloc'ed storage. 
                   An example of this is -bg and -background.  Both are parms for
                   the background color and set resource "background".
                   This array matches element for element with array opTable

   7.  from_parm  -  pointer to array of char (OUTPUT)
                   The bytes in this array are used as flags.  The bytes
                   correspond 1 for 1 with the elements in parm_values.
                   A non-zero byte in this array indicates that the
                   corresponding parm value came from the parm list.

   8.  display  -  pointer to pointer to Display (OUTPUT)
                   This routine has to open the display.  The display pointer
                   returned by XOpenDisplay is returned in this parm.

   9.  display_idx -  int (INPUT)
                   This is the index into the opTable array of the parm for the
                   display name.  This is needed to extract the display name from
                   the parm string if it was provided on the parm string.  If
                   -1 is passed, it is assumed that the display name cannot be
                   specified on the command line.

  10.  name_idx -  int (INPUT)
                   This is the index into the opTable array of the parm for the
                   X -name parameter.  This is needed to extract the name to look up
                   resources under if it was provided on the parm string.  If
                   -1 is passed, it is assumed that the base name of argv[0] is the
                   name to use.

  11.  class    -  pointer to char (INPUT)
                   If non-null this is the "X Class" class for resource retrieval.
                   If this parm is NULL or points to a null string, the basename
                   of argv[0] is used with the first letter capitalized.


FUNCTIONS :

   1.   Calculate the command name and class name from argv[0]

   2.   Use the X routine to parse the opTable.

   3.   If a display name was specified in the parm string, use it.  Then
        open the display.

   4.   Get the other xrm databases for this tool

   5.   Walk the opTable and extract the value for each name and put
        in the parm values array. 


*************************************************************************/

void  getxopts(XrmOptionDescRec    opTable[],     /*  input   */
               int                 op_count,      /*  input   */
               char               *defaults[],    /*  input   */
               int                *argc,          /*  input / output   */
               char               *argv[],        /*  input / output   */
               char               *parm_values[], /*  output  */
               char               *from_parm,     /*  output  */
               Display           **display,       /*  output  */
               int                 display_idx,   /*  input   */
               int                 name_idx,      /*  input   */
               char               *class)         /*  input   */
{

int         i;
int         j;
int         found;
char        classname[MAX_X_NAME];
char        cmdname[MAX_X_NAME];
char        new_cmdname[MAX_X_NAME];
char       *p;
char        resource_name_1[MAX_X_NAME+64];
char        resource_name_2[MAX_X_NAME+64];
char       *str_type;
XrmValue    value;
XrmDatabase commandlineDB = NULL;
XrmDatabase rDB =  NULL;
#ifdef ARPUS_CE
int         reload_app_defaults = False;
#endif


/***************************************************************
*  
*  So we can use the resource manager data merging functions
*  
***************************************************************/

XrmInitialize();

/***************************************************************
*  
*  Set up the cmdname (argv[0]) and classname variables.
*  If the first char of the command name is lower case alphabetic, convert
*  it to upper case to get the classname per X standards.
*  Otherwise try the second char.  Otherwise, forget it.
*  
***************************************************************/

if ((p = strrchr(argv[0], '/')) == NULL)
   (void) strncpy(cmdname, argv[0], MAX_X_NAME);
else
   (void) strncpy(cmdname, p+1, MAX_X_NAME);
cmdname[MAX_X_NAME-1] = '\0';

/***************************************************************
*  
*  RES 4/18/1998
*  For Cygnus on Windows95, get rid of the exe suffix if there
*  is one.  Otherwise X cannot pick up arguments.
*  
***************************************************************/

if ((p = strrchr(cmdname, '.')) != NULL)
   *p = '\0';

DEBUG1(
for (i = 0; i < *argc; i++)
   fprintf(stderr, "argv[%2d] = \"%s\"\n", i, argv[i]);
)


if (class == NULL || *class == '\0')
   {
      (void) strncpy(classname, cmdname, MAX_X_NAME);
      if (classname[0] >= 'a' && classname[0] <= 'z')
         classname[0] &= ~0x20;
      else
         if (classname[1] >= 'a' && classname[1] <= 'z')
            classname[1] &= ~0x20;
   }
else
   strncpy(classname, class, MAX_X_NAME);
classname[MAX_X_NAME-1] = '\0';


/***************************************************************
*  
*  Parse command line first so we can open display, store any
* options in a database 
*  
***************************************************************/

XrmParseCommand(&commandlineDB, opTable, op_count, cmdname, argc, argv);

/***************************************************************
*  
*  Get display now, because we need it to get other databases
*  Dig though the opt table looking for it.  If found, 
*  
***************************************************************/

MyDisplayName[0] = '\0';

if ((display_idx >= 0) && (*display == NULL))
   {
      snprintf(resource_name_1, sizeof(resource_name_1), "%s%s", cmdname, opTable[display_idx].specifier);
      snprintf(resource_name_2, sizeof(resource_name_2), "%s%s", classname, opTable[display_idx].specifier);

      if (XrmGetResource(commandlineDB, resource_name_1, resource_name_2, &str_type, &value) == True)
         {
            (void) strncpy(MyDisplayName, value.addr, MIN(((int) sizeof(MyDisplayName)),((int) value.size)));
            if (value.size < sizeof(MyDisplayName))
               MyDisplayName[value.size] = '\0';
            if (strchr(MyDisplayName, ':') == NULL)
               strncat(MyDisplayName, ":0.0", (sizeof(MyDisplayName)-strlen(MyDisplayName)));
         }
   }

/***************************************************************
*  
*  If -name processing was specified, and a -name was specified,
*  we have to copy the command line arguments over to the new
*  names so they get picked up properly in later queries.
*  
***************************************************************/

if (name_idx >= 0)
   {
      snprintf(resource_name_1, sizeof(resource_name_1), "%s%s", cmdname, opTable[name_idx].specifier);
      snprintf(resource_name_2, sizeof(resource_name_2), "%s%s", classname, opTable[name_idx].specifier);

      if (XrmGetResource(commandlineDB, resource_name_1, resource_name_2, &str_type, &value) == True)
         if (value.addr && value.addr[0])
            {
               (void) strncpy(new_cmdname, value.addr, MIN(((int) sizeof(new_cmdname)),((int) value.size)));
               new_cmdname[sizeof(new_cmdname)-1] = '\0';
               for (i = 0; i < op_count; i++)
               {
                  snprintf(resource_name_1, sizeof(resource_name_1), "%s%s", cmdname,   opTable[i].specifier);
                  snprintf(resource_name_2, sizeof(resource_name_2), "%s%s", classname, opTable[i].specifier);

                  if (XrmGetResource(commandlineDB, resource_name_1, resource_name_2, &str_type, &value) == True)
                     {
                        snprintf(resource_name_2, sizeof(resource_name_2), "%s%s", new_cmdname, opTable[i].specifier);
                        DEBUG1(fprintf(stderr, "Copying option %s to %s, value %s\n", resource_name_1, resource_name_2, value.addr);)
                        XrmPutResource(&commandlineDB, resource_name_2, str_type, &value);
                     }
               }  /* end of walking through the parm descriptor list */
               (void) strncpy(cmdname, new_cmdname, sizeof(cmdname));
            }
   }

#ifdef WIN32
/***************************************************************
*  
*  Check the display variable.  If it is null and the display
*  name is null, set it to :0.0 which is the display on the
*  host we are running on.
*  
***************************************************************/

if ((MyDisplayName[0] == '\0') && (*display == NULL))
   {
      p = getenv("DISPLAY");
      if (p == NULL)
         {
            p = strdup("DISPLAY=127.0.0.1:0.0");
            if (p)
               putenv(p);
         }
   }
#endif

/***************************************************************
*  
*  See what was specified on the command line.
*  
***************************************************************/

for (i = 0; i < op_count; i++)
{
   snprintf(resource_name_1, sizeof(resource_name_1), "%s%s", cmdname,   opTable[i].specifier);
   snprintf(resource_name_2, sizeof(resource_name_2), "%s%s", classname, opTable[i].specifier);

   if (XrmGetResource(commandlineDB, resource_name_1, resource_name_2, &str_type, &value) == True)
      {
         from_parm[i] = True;
         DEBUG(fprintf(stderr, "Option from arg list %s = %s\n", resource_name_1, value.addr);)
#ifdef ARPUS_CE
         if (strcmp(opTable[i].specifier, ".internalreload") == 0)
            reload_app_defaults = True;
#endif
      }
   else
      from_parm[i] = False;

}  /* end of walking through the parm descriptor list */

/***************************************************************
*  
*  Open display
*  
***************************************************************/

if (*display == NULL)
   *display = XOpenDisplay(MyDisplayName);

/***************************************************************
*  
*  Get server defaults, program defaults, and .Xdefaults and merge them
*  
***************************************************************/

if (*display)
   GetUsersDatabase(classname, *display, &rDB, reload_app_defaults);

XrmMergeDatabases(commandlineDB, &rDB);


/***************************************************************
*  
*  Put the collected data into the returned parms.
*  Only fill in the first occurance of each resource.
*  
***************************************************************/

for (i = 0; i < op_count; i++)
{
   found = 0;
   for (j = 0; j < i && !found; j++)
      if (strcmp(opTable[i].specifier, opTable[j].specifier) == 0)
         {
            parm_values[i] = NULL;
            found = 1;
         }
   
   if (!found)
      {
         snprintf(resource_name_1, sizeof(resource_name_1), "%s%s", cmdname,   opTable[i].specifier);
         snprintf(resource_name_2, sizeof(resource_name_2), "%s%s", classname, opTable[i].specifier);

         /*if ((display_idx != i) && (XrmGetResource(rDB, resource_name_1, resource_name_2, &str_type, &value) == True))*/
         if (XrmGetResource(rDB, resource_name_1, resource_name_2, &str_type, &value) == True)
            {
               /*  parm_values[i] = (char *)malloc(strlen(value.addr)+1);  RES 04/22/2005 */
               parm_values[i] = strdup(value.addr);
               if (!parm_values[i])
                  {
                     fprintf(stderr, "Malloc error parsing parms, cannot malloc %d bytes\n", strlen(value.addr)+1);
                     break;
                  }

               p = &parm_values[i][strlen(parm_values[i])-1];
               while((p > parm_values[i]) && (*p == ' ')) *p-- = '\0';
               DEBUG1(fprintf(stderr, "%s 0x%x = %s (size is %d)\n", resource_name_1, parm_values[i], parm_values[i], value.size);)
            }
         else
            if (defaults[i])
               {
                  parm_values[i] = strdup(defaults[i]);
                  if (!parm_values[i])
                     {
                        fprintf(stderr, "Malloc error parsing parms, cannot malloc %d bytes\n", strlen(value.addr)+1);
                        break;
                     }
               }
            else
               parm_values[i] = NULL;
      }

}  /* end of walking through the parm descriptor list */

DEBUG1(
/***************************************************************
*  
*  DEBUG: List the final options
*  
***************************************************************/

for (i = 0; i < op_count; i++)
   fprintf(stderr, "(%d) %s%s = \"%s\"\n", i, cmdname, opTable[i].specifier, (parm_values[i] ? parm_values[i] : "<NULL>"));

)

/***************************************************************
*  
*  Destroy the created database.  The parms are in malloc'ed storage.
*  
***************************************************************/
XrmDestroyDatabase(rDB);


} /* end of getxopts */


/************************************************************************

NAME:      set_wm_class_hints - Set up the class name resource hints for the window manager

PURPOSE:    This routine sets up the call to XSetClassHint and makes the call.

PARAMETERS:

   1.  display       - pointer to Display (INPUT)
                       This is the display pointer for the current display.

   2.  window        - Window (INPUT)
                       This is the window to set the property on.

   3.  arg0          - pointer to char (INPUT)
                       This is the command name from argv[0].

   4. dash_name      - pointer to char (INPUT)
                       This is the value from the -name parm.  It may be null or
                       point to a null string.

   5. class          - pointer to char (INPUT)
                       This is the class name to be used.  It may be null or point to a
                       null string.

FUNCTIONS :

   1.  If the -name parm was specified, use its value for the resource name.

   2.  If dash_name was not specified, use the last qualifier of the cmdname.

   3.  If the class name was specified, use it.

   4.  If the class name was not specified, use the resource name with the
       first character capitalized.

OUTPUTS:
   The window XA_WM_CLASS property for the passed window are set by this routine.

*************************************************************************/

void set_wm_class_hints(Display           *display,
                        Window             window,
                        char              *arg0,
                        char              *dash_name,
                        char              *class)
{
char        classname[MAX_X_NAME];
char        cmdname[MAX_X_NAME];
char       *p;
XClassHint  class_hint;


/***************************************************************
*  
*  Set up the cmdname (argv[0]) and classname variables.
*  If the first char of the command name is lower case alphabetic, convert
*  it to upper case to get the classname per X standards.
*  Otherwise try the second char.  Otherwise, forget it.
*  
***************************************************************/

if (dash_name && *dash_name)
   strncpy(cmdname, dash_name, MAX_X_NAME);
else
   if ((p = strrchr(arg0, '/')) == NULL)
   (void) strncpy(cmdname, arg0, MAX_X_NAME);
else
   (void) strncpy(cmdname, p+1, MAX_X_NAME);
cmdname[MAX_X_NAME-1] = '\0';


if (class && *class)
   strncpy(classname, class, MAX_X_NAME);
else
   {
      (void) strncpy(classname, cmdname, MAX_X_NAME);
      if (classname[0] >= 'a' && classname[0] <= 'z')
         classname[0] &= ~0x20;
      else
         if (classname[1] >= 'a' && classname[1] <= 'z')
            classname[1] &= ~0x20;
   }
classname[MAX_X_NAME-1] = '\0';

DEBUG1(fprintf(stderr, "set_wm_class_hints: Using resource name %s and class name %s for XA_WM_CLASS property.\n", cmdname, classname);)

class_hint.res_name  = cmdname;
class_hint.res_class = classname;

XSetClassHint(display, window, &class_hint);


} /* end of set_wm_class_hints */


/***************************************************************
*  
*  Routine to extract the home directory on different systems.
*  
***************************************************************/

static char *GetHomeDir(char *dest, int max_len)
{
/*   extern struct passwd *getpwuid(); lint message */
#ifndef WIN32
   int uid;
   struct passwd *pw;
#endif
   register char *ptr;

   if((ptr = getenv("HOME")) != NULL) {
      (void) strncpy(dest, ptr, max_len);
      dest[max_len-1] = '\0';

   } else {
#ifdef WIN32
         (void) strncpy(dest, "U:\\", max_len);
#else
      if((ptr = getenv("USER")) != NULL) {
         pw = getpwnam(ptr);
      } else {
         uid = getuid();
         pw = getpwuid(uid);
      }
      if (pw) {
         (void) strncpy(dest, pw->pw_dir, max_len);
      } else {
         *dest = '\0';
      }
#endif
   }
   return(dest);
} /* end of GetHomeDir  */


/***************************************************************
*  
*  Get program's and user's defaults
*  
***************************************************************/

static void  GetUsersDatabase(char              *classname,
                              Display           *display,
                              XrmDatabase       *rDB,
                              int                reload_app_defaults)
{
XrmDatabase homeDB, serverDB, applicationDB;

char            *path;
char             filename[MAXPATHLEN];
char            *environment;
char            *lang;
char            *xa_resource;

#ifdef ARPUS_CE
Atom             ce_apps_default_atom;
char             apps_default_atom_name[80];
Atom             actual_type;
int              actual_format;
int              property_size;
unsigned long    nitems;
unsigned long    bytes_after;
char            *buff = NULL;
#endif

#ifdef ARPUS_CE
/***************************************************************
*  
*  In Ce we will do something a little strange.  We will read
*  the app-defaults the first time and save them in a property.
*  After this, we will read them from the property, thus avoiding
*  accessing the app-defaults file every time.
*  
***************************************************************/

snprintf(apps_default_atom_name, sizeof(apps_default_atom_name), "%s_APPS_DEFAULT", classname);
ce_apps_default_atom = XInternAtom(display, apps_default_atom_name, False);
XGetWindowProperty(display,
                   RootWindow(display, DefaultScreen(display)),
                   ce_apps_default_atom,
                   0, /* offset zero from start of property */
                   1000000, /* a million ints is plenty */
                   False, /* do not delete the property */
                   XA_STRING,
                   &actual_type,
                   &actual_format,
                   &nitems,
                   &bytes_after,
                   (unsigned char **)&buff);

property_size = nitems * (actual_format / 8);
if (buff && (property_size > 0) && !reload_app_defaults)
   {
      applicationDB = XrmGetStringDatabase(buff);
      DEBUG1(if (applicationDB) fprintf(stderr, "Got data from Ce_APPS_DEFAULT\n");)
   }
else
   applicationDB = NULL;

if (buff)
   XFree(buff);

if (applicationDB == NULL)
   {
      /***************************************************************
      *  Get the language variable if it exists and get the language
      *  dependent application defaults.  If it is not found,
      *  Get the defaults app file if it exists.
      ***************************************************************/
      lang = getenv("LANG");
      if (lang)
         {
            snprintf(filename, sizeof(filename), DEFAULT_LANG_OPTS, lang, classname);
            applicationDB = ce_XrmGetFileDatabase(display, filename, ce_apps_default_atom);
            DEBUG1(if (applicationDB) fprintf(stderr, "Got application DB from %s\n", filename);)
         }
 
      if (applicationDB == NULL)
         {
            path = search_xfilesearchpath(classname);
            if (path)
               {
                  applicationDB = ce_XrmGetFileDatabase(display, path, ce_apps_default_atom);
                  DEBUG1(if (applicationDB) fprintf(stderr, "Got application DB from %s\n", filename);)
               }
         }

   }
#else
/***************************************************************
*  Get the language variable if it exists and get the language
*  dependent application defaults.  If it is not found,
*  Get the defaults app file if it exists.
***************************************************************/
applicationDB = NULL;
lang = getenv("LANG");
if (lang)
   {
      snprintf(filename, sizeof(filename), DEFAULT_LANG_OPTS, lang, classname);
      applicationDB = XrmGetFileDatabase(filename);
      DEBUG1(if (applicationDB) fprintf(stderr, "Got application DB from %s\n", filename);)
   }

if (applicationDB == NULL)
   {
      path = search_xfilesearchpath(classname);
      if (path)
         {
            applicationDB = XrmGetFileDatabase(path);
            DEBUG1(if (applicationDB) fprintf(stderr, "Got application DB from %s\n", filename);)
         }
   }
#endif

if (applicationDB)
   (void) XrmMergeDatabases(applicationDB, rDB);


/***************************************************************
*  MERGE The data from XAPPLRESLANGPATH/class or XAPPLRESDIR/class
***************************************************************/
applicationDB = NULL;
if ((environment = getenv("XAPPLRESLANGPATH")) != NULL)
   {
      snprintf(filename, sizeof(filename), "%s/%s", environment, classname);
      applicationDB = XrmGetFileDatabase(filename);
      DEBUG1(if (applicationDB) fprintf(stderr, "Got application DB from %s\n", filename);)
   }
if ((applicationDB == NULL) && ((environment = getenv("XAPPLRESDIR")) != NULL))
   {
      snprintf(filename, sizeof(filename), "%s/%s", environment, classname);
      applicationDB = XrmGetFileDatabase(filename);
      DEBUG1(if (applicationDB) fprintf(stderr, "Got application DB from %s\n", filename);)
   }

if (applicationDB)
   XrmMergeDatabases(applicationDB, rDB);

/***************************************************************
*  MERGE server defaults, these are created by xrdb, loaded as a
*  property of the root window when the server initializes, and
*  loaded into the display structure on XOpenDisplay.  If not defined,
*  use .Xdefaults .
***************************************************************/

xa_resource = XResourceManagerString(display);
if (xa_resource != NULL)
   {
      serverDB = XrmGetStringDatabase(xa_resource);
      DEBUG1(if (serverDB) fprintf(stderr, "Got data from XResourceManagerString\n");)
   }
else
   {
      /* Open .Xdefaults file and merge into existing data base */
      (void) GetHomeDir(filename, sizeof(filename));
      (void) strncat(filename, "/.Xdefaults", (sizeof(filename)-strlen(filename)));

      serverDB = XrmGetFileDatabase(filename);
      DEBUG1(if (serverDB) fprintf(stderr, "Got data from .Xdefaults %s\n", filename);)
   }

XrmMergeDatabases(serverDB, rDB);

/***************************************************************
*  Open XENVIRONMENT file, or if not defined, the ~/.Xdefaults,
*  and merge into existing data base 
***************************************************************/

homeDB = NULL;
if ((environment = getenv("XENVIRONMENT")) != NULL)
   {
      homeDB = XrmGetFileDatabase(environment);
      DEBUG1(if (homeDB) fprintf(stderr, "Got data from %s\n", environment);)
   }

if (homeDB == NULL)
   {
      int len;
      GetHomeDir(filename, sizeof(filename));
      (void) strncat(filename, "/.Xdefaults-", (sizeof(filename)-strlen(filename)));
      len = strlen(filename);
      gethostname(filename+len, MAXPATHLEN-len);
      homeDB = XrmGetFileDatabase(filename);
      DEBUG1(if (homeDB) fprintf(stderr, "Got data from %s\n", filename);)
   }

if (homeDB)
   XrmMergeDatabases(homeDB, rDB);

} /* end of GetUsersDatabase */

/************************************************************************

NAME:      search_xfilesearchpath - Parse and search XFILESEARCHPATH

PURPOSE:    This routine lookes for the app-defaults file.  It uses the
            class name and ignores the 

PARAMETERS:
   1.  class_name          - pointer to char (INPUT)
                       This is the command name from argv[0].

FUNCTIONS :
   1.  Get the environment variable and dup it so we can chop it up.

   2.  Chop up the filename and get rid of any %N type junk in the
       last qualiffier.

   3.  Concatinate the class name and see if the file exists.

RETURNED VALUE:
   filename   -  pointer to char
                 NULL -  No class found
                 static pointer to the file name.

SAMPLE XFILESEARCHPATH:
   XFILESEARCHPATH=:/phx/app-defaults/%N:/pkg/app-defaults/%N:/phx/usr/app-defaults/%N


*************************************************************************/

static char  *search_xfilesearchpath(char    *class_name)
{
char        *search_path;
int          i;
int          done = False;
char        *work;
int          work_size;
char        *last;
static char  filename[MAXPATHLEN+1];
#ifdef WIN32
char                  separator = ';';
char                 *path_sep = "\\";
#else
char                  separator = ':';
char                 *path_sep = "/";
#endif

search_path = getenv("XFILESEARCHPATH");
if (!search_path)
   search_path = "";

work_size = strlen(search_path) + strlen(DEFAULT_OPTS) + 3;
work = (char *)malloc(work_size);
if (!work)
   return(NULL);

snprintf(work, work_size, "%s%c%s", search_path, separator, DEFAULT_OPTS);
search_path = work;

filename[0] = '\0';
i = 0;

while(!done)
{
   if (!*search_path || *search_path == separator)
      {
         filename[i] = '\0';
#ifdef WIN32
         last = strrchr(filename, '\\');
         if (!last) last = strrchr(filename, '/');
#else
         last = strrchr(filename, '/');
#endif
         if (last)
            {
               while(*last && (*last != '%'))
                  last++;      
               *last = '\0';
            }
         if (filename[0] && !last)
            strncat(filename, "/", (sizeof(filename)-strlen(filename)));
         else
            if (last && (*(last-1) != '\\') && (*(last-1) != '/'))
               strncat(filename, path_sep, (sizeof(filename)-strlen(filename)));
         strncat(filename, class_name, (sizeof(filename)-strlen(filename)));
         if (access(filename, R_OK) == 0)
            done = True;
         else
            {
               filename[0] = '\0';
               if (*search_path)
                  {
                     i = 0;
                     search_path++;
                  }
               else
                  done = True;
            }
     }
  else
     filename[i++] = *search_path++;
}

free(work);

if (filename[0])
   return(filename);
else
   return(NULL);

} /* end of search_xfilesearchpath */


#ifdef ARPUS_CE
static XrmDatabase ce_XrmGetFileDatabase(Display   *display,
                                         char      *filename,
                                         Atom       target_atom)
{
FILE        *app_defaults;
char         line[1024];
char        *p;
char        *buff = NULL;
XrmDatabase  db;

if ((app_defaults = fopen(filename, "r")) == NULL)
   {
      DEBUG(fprintf(stderr, "Cannot read app-defaults file %s (%s)\n", filename, strerror(errno));)
      return(NULL);
   }

while(fgets(line, sizeof(line), app_defaults) != NULL)
{
   p = line;   
   while((*p == ' ') || (*p == '\t')) p++;
   if (*p == '!')
      continue;

   DEBUG1(fprintf(stderr, "Saving Option from appdefaults %s : %s\n", filename, line);)
   if (buff)
      {
         buff = (char *)realloc(buff, strlen(buff) + strlen(p) + 1);
         strcat(buff, p);
      }
   else
      buff = strdup(p);
}

if (!buff)
   buff = strdup("  ");

XChangeProperty(display,
                RootWindow(display, DefaultScreen(display)),
                target_atom,
                XA_STRING,
                8, /* 8 bit quantities */
                PropModeReplace,
                (unsigned char *)buff, 
                strlen(buff));

db = XrmGetStringDatabase(buff);

XFlush(display);
free(buff);

return(db);

} /* end of ce_XrmGetFileDatabase */
#endif


#ifdef GETXOPTS_STAND_ALONE
/***************************************************************
*  
*  Stand alone test of getxopts.
*  
***************************************************************/

main(int  argc, char  *argv[])
{

Display     *display;
int          i;
char        *values[25];


static int opTableEntries = 11;
static XrmOptionDescRec opTable[] = {
/* option             resource name                   type                  used only if type is XrmoptionNoArg */

{"=",               ".geometry",                    XrmoptionIsArg,         (caddr_t) NULL},
{"-background",     ".background",                  XrmoptionSepArg,        (caddr_t) NULL},
{"-bg",             ".background",                  XrmoptionSepArg,        (caddr_t) NULL},
{"-borderwidth",    ".TopLevelShell.borderWidth",   XrmoptionSepArg,        (caddr_t) NULL},
{"-bw",             ".TopLevelShell.borderWidth",   XrmoptionSepArg,        (caddr_t) NULL},
{"-display",        ".display",                     XrmoptionSepArg,        (caddr_t) NULL},
{"-fg",             ".foreground",                  XrmoptionSepArg,        (caddr_t) NULL},
{"-font",           ".font",                        XrmoptionSepArg,        (caddr_t) NULL},
{"-foreground",     ".foreground",                  XrmoptionSepArg,        (caddr_t) NULL},
{"-geometry",       ".TopLevelShell.geometry",      XrmoptionSepArg,        (caddr_t) NULL},
{"-title",          ".TopLevelShell.title",         XrmoptionSepArg,        (caddr_t) NULL},
};

static char  *defaults[] = {
             "100x100+0+0",   /* default for geometry  */
             "white",         /* default for background */
             NULL,            /* irrelevant, second background value */
             "2",             /* default for .TopLevelShell.borderWidth */
             NULL,            /* irrelevant, second .TopLevelShell.borderWidth value */
             NULL,            /* default for dispaly */
             "black",         /* default for foreground */
             "8x13.bold",     /* default for font */
             NULL,            /* irrelevant, second foreground value */
             "150x150+5-5",   /* default for TopLevelShell.geometry */
             NULL};           /* default for title */


#ifdef DebuG
Debug();
#endif

getxopts(opTable, opTableEntries, defaults, &argc, argv, values, &display);

if (display == NULL)
   {
      fprintf(stderr, "Could not open display\n");
   }

fprintf(stderr, "Remaining argc = %d\n", argc);

for (i = 0; i < opTableEntries; i++)
   if (values[i] != NULL)
      fprintf(stderr, "values[%d] = 0x%x %\"%s\"  (%s)\n", i, values[i], values[i], opTable[i].specifier);
   else
      fprintf(stderr, "values[%d] = NULL (%s)\n", i, opTable[i].specifier);


} /* end of main */
#endif



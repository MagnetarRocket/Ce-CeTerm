static char *sccsid = "%Z% %M% %I% - %G% %U% ";
/*
lab7 -font '*-r-*-240-*-p-*'  -s 'this is a test'

*/
#include <stdio.h>          /* /usr/include/stdio.h      */


#include <X11/Xlib.h>       /* /usr/include/X11/Xlib.h   */
#include <X11/Xutil.h>      /* /usr/include/X11/Xutil.h  */
#include <X11/keysym.h>     /* /usr/include/X11/keysym.h */
#include <X11/cursorfont.h> /* /usr/include/X11/cursorfont.h */

#include "keysym.h"

char     *modifier_names[8] = {"Shift    ",
                               "ShiftLock",
                               "Control  ",
                               "Mod1     ",
                               "Mod2     ",
                               "Mod3     ",
                               "Mod4     ",
                               "Mod5     "};

void main (argc, argv)
int   argc;
char **argv;
{

Display              *display;
char                 *name_of_display = NULL;
int                   screen;
int                   min_key;
int                   max_key;
int                   syms_per_code;
int                   keycode_count;
KeySym               *key_array;
int                   sym_index;
KeySym                keysym;
KeySym                keysym2;
int                   k;
int                   n;
char                 *x_key_name;
FILE                 *fd;
char                **flist;
int                   count;
int                   i;
XModifierKeymap      *keymods;
KeyCode              *modkey;
KeyCode               mkey;


char                  key_name[16];

/****************************************************************
*
*   Open the display so we can start getting information about it.
*
****************************************************************/

 if ((display = XOpenDisplay(name_of_display)) == NULL)
    {
       perror("XOpenDisplay");
       fprintf(stderr,
               "%s: cannot connect to X server %s\n",
               argv[0],
               XDisplayName(name_of_display)
               );
       exit (1);
    }
screen = DefaultScreen(display);

if ((fd = fopen("/tmp/zkeys", "w")) == NULL)
   {
      perror("fopen");
      fprintf(stderr, "cannot open /tmp/zkeys for output\n");
      exit(1);
   }

/*flist = XGetFontPath(display, &count);
for (i = 0; i < count; i++)
   fprintf(stdout, "path = \"%s\"\n", flist[i]);*/

/***************************************************************
*  
*  Get the modifier mapping.
*  
***************************************************************/

keymods = XGetModifierMapping(display);
if (keymods == NULL)
   {
      fprintf(stderr, "Cannot get modifier mapping\n");
      exit(1);
   }

modkey = keymods->modifiermap;
max_key = keymods->max_keypermod;
fprintf(stdout, "Max keys per modifier = %d\n", max_key);


for (k = 0; k < 8; k++)
{
   fprintf(stdout, "Modifier %s  :  ", modifier_names[k]);
   for (n = 0; n < max_key; n++)
   {
      mkey = modkey[k * max_key + n];
      if (mkey != 0)
         fprintf(stdout, "%12s  ", XKeysymToString(XKeycodeToKeysym(display, mkey, 0)) );
   }
     
   fprintf(stdout, "\n");
}


fprintf(stdout, "\n\n");


/***************************************************************
*  
*  List all the KeyCodes
*  
***************************************************************/

XDisplayKeycodes(display, &min_key, &max_key);

keycode_count = max_key - min_key;
keycode_count++;

key_array = XGetKeyboardMapping(display, min_key, keycode_count,
                                &syms_per_code);

fprintf(stdout, "min keycode = 0x%X, max keycode = 0x%X, count = %d,  syms_per_code = %d\n\n",
               min_key, max_key, keycode_count, syms_per_code);

for (k = min_key;
     k < max_key;
     k++)
{
   for (n = 0; n < syms_per_code; n++)
   {
     sym_index = (k - min_key) * syms_per_code + n;
     keysym = key_array[sym_index];
     x_key_name = XKeysymToString(keysym);
     if (x_key_name != NULL)
        {
           fprintf(fd, "%s\n", x_key_name);    
           keysym2 = XStringToKeysym(x_key_name);
           if (keysym2 != keysym)
              fprintf(stdout, "?  KeySym does not reverse %d -> %s -> %d\n", keysym, x_key_name, keysym2);
        }
     else
        x_key_name = "";
     fprintf(stdout, "keysym #%d for keycode 0x%-04X = 0x%-8X string = %-20s  C define = %s\n",
                     n, k, keysym, x_key_name, translate_keysym(keysym) );
   } /* end of loop on n */
fprintf(stdout, "\n");
} /* end of loop on k */

fclose(fd);

}  /* end main   */




#ifndef __lint
static char *sccsid = "%Z% %M% %I% - %G% %U% ";
#endif

/**************************************************************
*
* This piggy demonstrates a problem with the Exceed X server 6.1
* which results in the X server getting a memory fault after a call
* to XMoveResizeWindow.  This program creates a simple window and
* two sub windows.  The two subwindows are children of the main
* window and created by the call to get_dm_windows.
*
* To reprocduce the problem, compile and link the program.
* On a Sun:
* cc -g -I/usr/openwin/include  -DNeedFunctionPrototypes  -o piggy1 piggy1.c   /usr/openwin/lib/libX11.so -lsocket -lnsl
*
* Start the program from a unix prompt displaying to an Exceed X server.
* There are no required parameters.
*    You can specify -d display_name:0 if the environment variable DISPLAY is
*    not set.
*    You can specify -D to cause the program to dump all the X events which
*    are received from the Xserver to stdout.
*
* The program will draw a picture showing all the active pixels.  This
* is not relevant but is the behavior of the program I used to build the 
* sample program.
*
* Press the a, b, c, and d keys in succession letting the window resize
* after each one.  The program will abort after a time.
*
* The control C sequence shuts down the program normally.
* Characters a, b, c, and d resize the window to various sizes.
* If you comment out the calls to get_dm_windows, the program will work.
* If you run the program talking to a UNIX X server, the program will work.
*
*
*  MODULE pixels - Show all pixels (256)
*
*  OVERVIEW
*      This program builds a window with a square for each
*      pixel value.  Free pixel values have an X in the square
*      and a small square in the lower right corner of the square 
*      with whatever is in the color map for that color.
*
*  ARGUMENTS:
*      -s <num>      -   Size of each square in the window
*      -p            -   Print the color information about each pixel
*                        the specified hex window id (use -list to find it)
*      -display <node:0.0> -  open the specified node.
*
***************************************************************/

#include <stdio.h>
#include <string.h>         /* /usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h   /usr/include/sys/errno.h      */
#include <ctype.h>          /* /usr/include/ctype.h      */
#include <stdlib.h>         /* /usr/include/stdlib.h     */
#include <limits.h>         /* /usr/include/limits.h     */
#ifndef MAXPATHLEN
#define MAXPATHLEN	1024
#endif


#include <X11/X.h>          /* /usr/openwin/include/X11/X.h      /usr/include/X11/X.h      */
#include <X11/Xlib.h>       /* /usr/openwin/include/X11/Xlib.h   /usr/include/X11/Xlib.h   */
#include <X11/Xatom.h>      /* /usr/openwin/include/X11/Xatom.h  /usr/include/X11/Xatom.h  */
#include <X11/Xutil.h>      /* /usr/openwin/include/X11/Xutil.h  /usr/include/X11/Xutil.h  */
#include <X11/keysym.h>      /* /usr/include/X11/keysym.h /usr/openwin/include/X11/keysymdef.h */

void draw_squares(int        square_size,
                  Display   *display,
                  Window     window,
                  GC        *gc,
                  XColor    *colors,
                  GC         string_gc,
                  GC         inv_gc,
                  int       *free_colors);

void   create_gcs(Display   *display,
                  Window     window,
                  GC        *gc,
                  XColor    *colors,
                  GC        *string_gc,
                  GC        *inv_gc);

int  find_free_colors(Display   *display,
                      int       *free_colors);

char  *name_the_color(int r, int g, int b);

char   *shared_color(Display        *display,
                     XColor         *color_parm,
                     int             free_color,
                     unsigned long   index);

static void build_picture(Display    *display,
                          int        *free_colors,
                          XColor     *colors,
                          int         print_rgb);

static int which_square(int        square_size,
                        int        x,
                        int        y);

int   get_dm_windows(Display          *display,
                     Window            x_window,
                     int               square_size,
                     unsigned int      main_width,
                     unsigned int      main_height);

#define DebuG
void dump_Xevent(FILE     *stream,
                 XEvent   *event,
                 int       level);

void translate_buttonmask(unsigned int     state,
                          char            *work);



int getopt();
extern char *optarg;
extern int optind, opterr; 

char             window_name[256] = "Pixel Map";

int main(int argc, char *argv[])
{
int              ch;
int              i;
int              square_size = 0;
GC               gc[256];
GC               string_gc;
GC               inv_gc;
XColor           colors[256];
int              free_colors[256];
int              free_colors_count;
int              print_rgb = 0;
int              square_num;
Display         *display;
char             display_name[128];
Window           prop_window = None;
XEvent           event_union;      /* The event structure filled in by X as events occur            */
int              unmapped = False;
XTextProperty    windowName;
char            *window_name_p = window_name;
int              getting_cursor_num = False;
int              cursor_num = 0;
int              max_colors;
Cursor           pointer_cursor = None;
Atom             atom_list[2];
Atom             wm_protocols_atom;
Atom             save_yourself_atom;
Atom             delete_window_atom;
int              orig_width_height;
int              new_x;
int              new_y;
unsigned int     new_width;
unsigned int     new_height;
char             work[256];
int              Debug = False;
 
display_name[0] = '\0';

 while ((ch = getopt(argc, argv, "Dd:s:p")) != EOF)
    switch (ch)
    {
    case 'D':
       Debug = True;
       break;

    case 'd':
       strncpy(display_name, optarg, sizeof(display_name)); 
       if (strchr(display_name, ':') == NULL)
          strcat(display_name, ":0.0");
       break;

    case 's':
       square_size = atoi(optarg);
       break;

    case 'p':
       print_rgb++;
       break;

    default  :                                                     
       fprintf(stderr, "Usage:  pixels [-d<display>] [-a] [-n] [-p] [-s<size>]\n");
       fprintf(stderr, "        -D  -  Dump X events to stdout\n");
       fprintf(stderr, "        -d  -  Specify a different display\n");
       fprintf(stderr, "        -p  -  Print the color data to stdout (rgb values)\n");
       fprintf(stderr, "        -s  -  Specify initial pixel size for each square\n");
       exit(1);

    }   /* end of switch on ch */

if (square_size == 0)
    square_size = 28;

if (!(display = XOpenDisplay(display_name)))
   {
      (void) fprintf(stderr, "%s: Can't open display '%s'\n",
                     argv[0], XDisplayName(display_name));
      exit(1);
   }

if (print_rgb)
   {
      max_colors = DisplayCells(display, DefaultScreen(display));
      fprintf(stderr, "Colors in default color map = %d\n", max_colors);
   }

orig_width_height = (square_size+1)*16;

XSynchronize(display, True);

prop_window = XCreateSimpleWindow(display, 
                                  RootWindow(display, DefaultScreen(display)),
                                  100, 0, (square_size+1)*16, (square_size+1)*16,
                                  1, BlackPixel(display, DefaultScreen(display)), 0);
if (prop_window == None)
   {
      fprintf(stderr, "Could not create a window\n");
      XCloseDisplay(display);
      exit(1);
   }

get_dm_windows(display,
               prop_window,
               square_size,
               orig_width_height,
               orig_width_height);


/***************************************************************
*  Set up for the save yourself client message and the delete
*  window message.  Delete window shows up when the user uses
*  the window manager pulldown to close the window.
***************************************************************/
save_yourself_atom = XInternAtom(display, "WM_SAVE_YOURSELF", False);
delete_window_atom = XInternAtom(display, "WM_DELETE_WINDOW", False);
wm_protocols_atom  = XInternAtom(display, "WM_PROTOCOLS", False);

atom_list[0] = save_yourself_atom;
atom_list[1] = delete_window_atom;

XSetWMProtocols(display, prop_window, atom_list, 2);



XSelectInput(display,
             prop_window,
             ExposureMask
             | VisibilityChangeMask
             | StructureNotifyMask
             | KeyPressMask
             | EnterWindowMask
             | LeaveWindowMask
             | ButtonPressMask);


free_colors_count = find_free_colors(display, free_colors);

create_gcs(display, prop_window, gc, colors, &string_gc, &inv_gc);
if (print_rgb)
   fprintf(stdout, "%d free color map entries available (pixels)\n\n", free_colors_count);

for (i = 0; i < 256; i++)
{
   if (print_rgb)
      fprintf(stdout, "Pixel  %3d, r = %5d, g = %5d, b = %5d, flags = 0x%X\n", i, colors[i].red>>8, colors[i].green>>8, colors[i].blue>>8, colors[i].flags);
   if (free_colors[i])
      {
         colors[i].red   = 0;  /* unallocated colors are not drawn */
         colors[i].green = 0;
         colors[i].blue  = 0;
      }
}
fflush(stdout);

XStringListToTextProperty(
      &window_name_p,
      1,
      &windowName);


XSetWMName(display, prop_window, &windowName);


XMapWindow(display, prop_window);

/* CONSTCOND */
while(1)
{
   XNextEvent(display, &event_union);
   if (Debug)
      dump_Xevent(stdout, &event_union, 2);



   /***************************************************************
   *  
   *  Switch on the type of event.  This separates screen things
   *  such as expose (pop), configure (resize and move), from
   *  user input such as keypress and mouse press.
   *  
   ***************************************************************/

   switch (event_union.type)
   {
   /***************************************************************
   *  In the case of an expose event, we need to copy the pixmap
   *  to the screen.  Copy the portion needed.
   ***************************************************************/
   case Expose:
      draw_squares(square_size, display, prop_window, gc, colors, string_gc, inv_gc, free_colors);
      break;


   case MapNotify:
      if (unmapped)
         build_picture(display, free_colors, colors, print_rgb);

      unmapped = False;
      break;

   case UnmapNotify:
      unmapped = True;
      break;



   /***************************************************************
   *
   *  The window changed size (or location).  Recalculate the
   *  size of the squares.
   *
   ***************************************************************/
   case ConfigureNotify:
      if (event_union.xconfigure.width < event_union.xconfigure.height)
         i = event_union.xconfigure.width;
      else
         i = event_union.xconfigure.height;
      square_size = (i / 16) - 1;
      get_dm_windows(display,
                     prop_window,
                     square_size,
                     event_union.xconfigure.width,
                     event_union.xconfigure.height);
      break;

   /***************************************************************
   *
   *  If we get a MappingNotify event, the keyboard mapping has
   *  changed.  We call XRefreshKeyboardMapping to fix some fields
   *  in the Display structure.  This is one of the "Nobrainers"
   *  that the instructor talked about.
   *
   ***************************************************************/
   case MappingNotify:
      XRefreshKeyboardMapping((XMappingEvent *)&event_union);
      break;

   /***************************************************************
   *
   *  We only look at a few keypresses.  ^c, ^d, and ^q cause
   *  normal termination of the program.
   *  The letter c in a square causes the RGB values for that
   *  square to be written to stdout.
   *  An exclamation mark, followed by digits, followed by a
   *  Return, cause the cursor to be changed to that cursor number.
   *  This is just for playing with cursors.  Use a bad number
   *  and you will abort the program.  The list of cursors is 
   *  in /usr/include/X11/cursorfont.h (/usr/openwin/include/X11/cursorfont.h)
   *
   ***************************************************************/
   case KeyPress:
   {
      KeySym           keysym;
      char             key_val[2];
      int              len;
      char             ch;
      XSetWindowAttributes    win_attr;

      len = XLookupString((XKeyEvent *)&event_union,
                          key_val,
                          sizeof(key_val),
                          &keysym,
                          NULL);
      key_val[len] = '\0';
      ch = key_val[0];
      
      switch (ch)
      {
      case '\r': case '\n':
        if (getting_cursor_num)
           {
              if (pointer_cursor != None)
                 XFreeCursor(display, pointer_cursor);
              pointer_cursor = XCreateFontCursor(display, cursor_num);
              win_attr.cursor = pointer_cursor;
              XChangeWindowAttributes(display,
                                      prop_window,
                                      CWCursor,
                                      &win_attr);
           }
        getting_cursor_num = False;
        break;

      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
         cursor_num = (cursor_num * 10) + (ch & 0x0f);
         break;

      case '!':
        getting_cursor_num = True;
        cursor_num = 0;
        break;

      case 'a':
         XParseGeometry("300x300+100+100",    /* geometry string */
                        &new_x, &new_y,             /* location */
                        &new_width, &new_height);   /* size */
         XMoveResizeWindow(display, prop_window,
                           new_x, new_y, new_width, new_height);
        break;

      case 'b':
         XParseGeometry("500x500+50+50",    /* geometry string */
                        &new_x, &new_y,             /* location */
                        &new_width, &new_height);   /* size */
         XMoveResizeWindow(display, prop_window,
                           new_x, new_y, new_width, new_height);
        break;

      case 'c':
         XParseGeometry("1045x367+90-21",    /* geometry string */
                        &new_x, &new_y,             /* location */
                        &new_width, &new_height);   /* size */
         XMoveResizeWindow(display, prop_window,
                           new_x, new_y, new_width, new_height);
        break;

      case 'd':
         sprintf(work, "%dx%d+100+0", orig_width_height, orig_width_height);
         XParseGeometry(work                   ,    /* geometry string */
                        &new_x, &new_y,             /* location */
                        &new_width, &new_height);   /* size */
         XMoveResizeWindow(display, prop_window,
                           new_x, new_y, new_width, new_height);
        break;

      case 'z':
        square_num = which_square(square_size, event_union.xkey.x, event_union.xkey.y);
        fprintf(stdout, "(%d) RGB = %d, %d, %d\n", square_num, colors[square_num].red>>8,
                        colors[square_num].green>>8,
                        colors[square_num].blue>>8);
        fflush(stdout);
        break;

      case 0x03:
      case 0x04:
      case 0x11:
         /* exit on ^c */
         XCloseDisplay(display);
         exit(0);
         break;

      default:
         break;
      } /* end of switch on character */
      if (keysym == XK_F1)
         {
            build_picture(display, free_colors, colors, print_rgb);
            draw_squares(square_size, display, prop_window, gc, colors, string_gc, inv_gc, free_colors);
         }
   } /* end of keypress group */
      break;


   /***************************************************************
   *
   *  A buttonpress in a square causes the rgb values and the
   *  color name (if we can find it) to be placed in the
   *  window manager title.
   *
   ***************************************************************/
   case ButtonPress:
      square_num = which_square(square_size, event_union.xkey.x, event_union.xkey.y);
      sprintf(window_name, "(%d%s) RGB = %d %d %d%s\n",
                        square_num,
                        shared_color(display, &colors[square_num], free_colors[square_num], square_num), 
                        colors[square_num].red>>8,
                        colors[square_num].green>>8,
                        colors[square_num].blue>>8,
                        name_the_color(colors[square_num].red>>8, colors[square_num].green>>8, colors[square_num].blue>>8));
      XSetStandardProperties(display,
                             prop_window,
                             window_name,
                             window_name,
                             None, /* pixmap for icon */
                             NULL, /* argv */
                             0,    /* argc */
                             NULL);

      break;

   /***************************************************************
   *
   *  If we get a ClientMessage, It may be the SAVE_YOURSELF message
   *  from the window manager.  If it is, do a pad write if we
   *  are in edit mode and the file is dirty.  Then confirm that
   *  we are ready with a zero length append to the XA_WM_COMMMAND property.
   *
   ***************************************************************/
   case ClientMessage:
      if (event_union.xclient.message_type == wm_protocols_atom)
         {
            if (*event_union.xclient.data.l == save_yourself_atom)
               {
                  XSetCommand(display, prop_window, argv, argc);
               }
            if (*event_union.xclient.data.l == delete_window_atom)
               {
                  XCloseDisplay(display);
                  exit(0);
               }
         }
      break;

 

   }     /* end switch on event_union.type */
}     /* end while 1  */


/*XCloseDisplay(display);*/

} /* end of main */


void draw_squares(int        square_size,
                  Display   *display,
                  Window     window,
                  GC        *gc,
                  XColor    *colors,
                  GC         string_gc,
                  GC         inv_gc,
                  int       *free_colors)
{
int       row;
int       col; 
int       idx;
char      num[5];
unsigned long  white_on;
XPoint    dx[5];
int       sq = square_size - 1;
int       white_pixel;
XColor    white_color;

white_pixel = WhitePixel(display, DefaultScreen(display));
white_color.pixel = white_pixel;
XQueryColor(display, DefaultColormap(display, DefaultScreen(display)), &white_color);

white_on = (white_color.red +
            white_color.green +
            white_color.blue);

white_on = white_on / 2;

dx[1].x = sq;
dx[1].y = sq;
dx[2].x = 0;
dx[2].y = -sq;
dx[3].x = -sq;
dx[3].y = sq;
dx[4].x = 0;
dx[4].y = -sq;

for (row = 0; row < 16; row++)
   for (col = 0; col < 16; col++)
   {                                      
      idx = (row * 16) + col;
      sprintf(num, "%d", idx);

      if (free_colors && free_colors[idx]) 
         {
            /* entry is free, put a white x in a black box */
            XFillRectangle(display, window, inv_gc,
                           col * (square_size+1),
                           row * (square_size+1),
                           square_size, square_size);
            XFillRectangle(display, window, gc[idx],
                          (col * (square_size+1)) + (square_size/2),
                          (row * (square_size+1)) + (square_size/2),
                          square_size/2, square_size/2);
            dx[0].x = col * (square_size+1);
            dx[0].y = row * (square_size+1);
            XDrawLines(display, window, string_gc,
                       dx, 5, CoordModePrevious);
         }
      else
         XFillRectangle(display, window, gc[idx],
                        col * (square_size+1),
                        row * (square_size+1),
                        square_size, square_size);

      if ((colors[idx].red + colors[idx].green + colors[idx].blue) < white_on)
         XDrawString(display, window, string_gc,
                     (col * (square_size+1)) + 4,
                     (row * (square_size+1)) + (square_size/2),
                     num, strlen(num));
      else
         XDrawString(display, window, inv_gc,
                     (col * (square_size+1)) + 4,
                     (row * (square_size+1)) + (square_size/2),
                     num, strlen(num));

   }

} /* end of draw_squares */

void   create_gcs(Display   *display,
                  Window     window,
                  GC        *gc,
                  XColor    *colors,
                  GC        *string_gc,
                  GC        *inv_gc)
{
unsigned long         gc_mask;
XGCValues             gcvalues;
int                   i;
XFontStruct          *font;

/***************************************************************
*  
*  Some sanity checking.  Make sure foreground and background are different.
*  
***************************************************************/

memset((char *)&gcvalues, 0, sizeof(XGCValues));

/****************************************************************
*
*   Build the gc for the screen and pixmap.
*
****************************************************************/

gc_mask = GCForeground
        | GCBackground;


for (i = 0; i < 256; i++)
{
   gcvalues.foreground = i;
   gcvalues.background = i;
   colors[i].pixel = i;


   gc[i] = XCreateGC(display,
                     window,
                     gc_mask,
                     &gcvalues
                    );

}


XQueryColors(display, 
             DefaultColormap(display, DefaultScreen(display)),
             colors,
             256);


font = XLoadQueryFont(display, "fixed");

gc_mask = GCForeground
        | GCBackground
        | GCFont;
gcvalues.foreground = WhitePixel(display, DefaultScreen(display));
gcvalues.background = BlackPixel(display, DefaultScreen(display));
gcvalues.font       = font->fid;

*string_gc = XCreateGC(display,
                     window,
                     gc_mask,
                     &gcvalues
                    );

gcvalues.foreground = BlackPixel(display, DefaultScreen(display));
gcvalues.background = WhitePixel(display, DefaultScreen(display));
*inv_gc = XCreateGC(display,
                     window,
                     gc_mask,
                     &gcvalues
                    );

}  /* end of create_gcs */

int  find_free_colors(Display   *display,
                      int       *free_colors)
{
int           rc;
int           i;
int           count = 125;
int           last_worked = 0;
int           last_failed = 256;
int           done = False;
unsigned long free_pixels[256];

memset((char *)free_colors, 0, sizeof(int)*256);

while(!done)
{
   rc = XAllocColorCells(display,   
                         DefaultColormap(display, DefaultScreen(display)),
                         False,
                         NULL, 0,  /* no plane masks */
                         free_pixels,
                         count);
   if (rc == 0) /* failure */
      {
         last_failed = count;
         if (last_worked + 1 == count) 
            done = True;
         else
            {
               i = count -  (count - last_worked) / 2;
               if (i == count)
                  count--;
               else
                  count = i;
            }
      }
   else
      {
         XFreeColors(display,
                     DefaultColormap(display, DefaultScreen(display)),
                     free_pixels,
                     count,
                     0); /* no planes */
         last_worked = count;
         if (last_failed - 1 == count) 
            done = True;
         else
            {
               i = count +  (last_failed - count) / 2;
               if (i == count)
                  count++;
               else
                  count = i;
            }
      }
}            

for (i = 0; i < last_worked; i++)
   free_colors[free_pixels[i]] = True;

return(last_worked);

} /* end of find_free_colors */


static int which_square(int        square_size,
                        int        x,
                        int        y)
{
int     row;
int     col;

row  = y / (square_size+1);
col  = x / (square_size+1);

return((row * 16) + col);
                        
} /* end of which_square */




struct COLOR_ENT  ;

struct COLOR_ENT {
        struct COLOR_ENT       *next;              /* next data in list                          */
        char                    name[80];          /* paste buffer name                          */
        int                     r;                 /* red                                        */
        int                     g;                 /* green                                      */
        int                     b;                 /* blue                                       */
  };


static struct COLOR_ENT  *load_colors(void)
{
struct COLOR_ENT        *new;
struct COLOR_ENT        *head = NULL;
struct COLOR_ENT        *last = NULL;
FILE                    *stream;
char                     line[256];
int                      r,g,b;
char                     name[80];
char                     junk[80];
int                      count;
static struct COLOR_ENT  static_head;

if ((stream = fopen("/usr/lib/X11/rgb.txt", "r")) == NULL)
   if ((stream = fopen("/usr/openwin/lib/rgb.txt", "r")) == NULL)
      {
         fprintf(stderr, "Cannot open /usr/lib/X11/rgb.txt or /usr/openwin/lib/rgb.txt for input (%s)\n", strerror(errno));
         head = &static_head;
         memset((char *)head, 0, sizeof(struct COLOR_ENT));
         return(head);
      }

while(fgets(line, sizeof(line), stream) != NULL)
{
   
   count = sscanf(line, "%d %d %d %s %s", &r, &g, &b, name, junk);
   if (count != 4)
      continue;
   new = malloc(sizeof(struct COLOR_ENT));
   if (!new)
      {
         fprintf(stderr, "Out of memory");
         fclose(stream);
         break;
      }
   memset((char *)new, 0, sizeof(struct COLOR_ENT));
   new->r = r;
   new->g = g;
   new->b = b;
   strcpy(new->name, name); 
   if (head)
      last->next = new;
   else
      head = new;
   last = new;

}

return(head);

} /* end of  load_colors */


char  *name_the_color(int r, int g, int b)
{
static struct COLOR_ENT  *head = NULL;
struct COLOR_ENT         *cur;
static char               buff[256];

if (!head)
   head = load_colors();

for (cur = head; cur; cur = cur->next)
   if ((r == cur->r) && (g == cur->g) && (b == cur->b))
      {
         sprintf(buff, " (%s)", cur->name);
         return(buff);
      }

sprintf(buff, " (#%02X%02X%02X)", r, g, b);
return(buff);

} /* end of name_the_color */

#ifdef blah
static void save_restart_state(Display   *display,
                               int        argc,
                               char      *argv[])
{
} /* end of save_restart_state */
#endif


char   *shared_color(Display        *display,
                     XColor         *color_parm,
                     int             free_color,
                     unsigned long   index)
{
char                 *returned_val;
XColor                color;
int                   success;

if (free_color)
   returned_val = "";
else
   {
      memcpy((char *)&color, (char *)color_parm, sizeof(color));

      success = XAllocColor(display,  DefaultColormap(display, DefaultScreen(display)),  &color);
      if (!success) /* zero means failure */
         returned_val = ":pvt";  /* private color */
      else
         {
            if (color.pixel == index)
               returned_val = ":shr";  /* shared color */
            else
               returned_val = ":pvt";  /* private color */

            XFreeColors(display,
                  DefaultColormap(display, DefaultScreen(display)),
                  &color.pixel,
                  1,                      /* one pixel value to free */
                  0);
         }
   }

return(returned_val);

} /* end of shared_color */

static void build_picture(Display    *display,
                          int        *free_colors,
                          XColor     *colors,
                          int         print_rgb)

{
int              free_colors_count;
int              i;

free_colors_count = find_free_colors(display, free_colors);
if (print_rgb)
   fprintf(stdout, "%d free color map entries available (pixels)\n\n", free_colors_count);
fflush(stdout);
XQueryColors(display, 
             DefaultColormap(display, DefaultScreen(display)),
             colors,
             256);

for (i = 0; i < 256; i++)
   if (free_colors[i])
      {
         colors[i].red   = 0;  /* unallocated colors are not drawn */
         colors[i].green = 0;
         colors[i].blue  = 0;
      }
} /* end of build_picture */


int   get_dm_windows(Display          *display,
                     Window            x_window,
                     int               square_size,
                     unsigned int      main_width,
                     unsigned int      main_height)

{
int                   window_height;
int                   window_width;
int                   border_width;
XWindowAttributes     window_attributes;
XSetWindowAttributes  window_attrs;
XWindowChanges        window_chngs;
unsigned long         window_mask;
XGCValues             gcvalues;
unsigned long         valuemask;
static Window         new_window_1 = None;
static Window         new_window_2 = None;
static Window         new_window_3 = None;
static Window         new_window_4 = None;
unsigned int          new_window1_width;
unsigned int          new_window1_height;
int                   new_window1_x;
int                   new_window1_y;
unsigned int          new_window2_width;
unsigned int          new_window2_height;
int                   new_window2_x;
int                   new_window2_y;
unsigned int          new_window3_width;
unsigned int          new_window3_height;
int                   new_window3_x;
int                   new_window3_y;
unsigned int          new_window4_width;
unsigned int          new_window4_height;
int                   new_window4_x;
int                   new_window4_y;

/***************************************************************
*
*  Get the boarder width from the main window.
*
***************************************************************/

if (!XGetWindowAttributes(display,
                          x_window,
                         &window_attributes))
   {
      fprintf(stderr, "get_dm_windows:  Cannot get window attributes for main window 0x%02X\n", x_window);
      border_width = 1;
   }
else
   border_width = window_attributes.border_width;

if (border_width == 0)
   border_width = 1;

/***************************************************************
*
*  Determine the height, width, X and Y of the windows.
*
***************************************************************/

new_window1_height = new_window2_height = new_window3_height = new_window4_height = square_size;
new_window1_width = new_window2_width = new_window3_width = new_window4_width = (main_width + 1) / 2;       /* divide by 2 rounding up */

new_window1_x = new_window3_x = -1;
new_window2_x = new_window4_x = new_window1_width;  /* the two borders overlap */

new_window1_y  = new_window2_y  = main_height - (new_window1_height + border_width); /* 1 border width because bottom borders overlap */
new_window3_y  = new_window4_y  = main_height - ((new_window1_height*2) + (border_width*2)); /* 1 border width because bottom borders overlap */

if (new_window_1 == None)
{
      window_attrs.background_pixel = BlackPixel(display, DefaultScreen(display));
      window_attrs.border_pixel     = WhitePixel(display, DefaultScreen(display));
      window_attrs.win_gravity      = UnmapGravity;


      window_mask = CWBackPixel
                  | CWBorderPixel
                  | CWWinGravity;

      new_window_1 = XCreateWindow(
                                      display,
                                      x_window,
                                      new_window1_x,     new_window1_y,
                                      new_window1_width, new_window1_height,
                                      border_width,
                                      window_attributes.depth,
                                      InputOutput,                    /*  class        */
                                      CopyFromParent,                 /*  visual       */
                                      window_mask,
                                      &window_attrs
          );


      new_window_2 = XCreateWindow(
                                      display,
                                      x_window,
                                      new_window2_x,     new_window2_y,
                                      new_window2_width, new_window2_height,
                                      border_width,
                                      window_attributes.depth,
                                      InputOutput,                    /*  class        */
                                      CopyFromParent,                 /*  visual       */
                                      window_mask,
                                      &window_attrs
          );


      new_window_3 = XCreateWindow(
                                      display,
                                      x_window,
                                      new_window3_x,     new_window3_y,
                                      new_window3_width, new_window3_height,
                                      border_width,
                                      window_attributes.depth,
                                      InputOutput,                    /*  class        */
                                      CopyFromParent,                 /*  visual       */
                                      window_mask,
                                      &window_attrs
          );

      new_window_4 = XCreateWindow(
                                      display,
                                      x_window,
                                      new_window4_x,     new_window4_y,
                                      new_window4_width, new_window4_height,
                                      border_width,
                                      window_attributes.depth,
                                      InputOutput,                    /*  class        */
                                      CopyFromParent,                 /*  visual       */
                                      window_mask,
                                      &window_attrs
          );

}
else
{
      /***************************************************************
      *
      *  Resize the currently unmapped windows
      *
      ***************************************************************/

      window_chngs.height           = new_window1_height;
      window_chngs.width            = new_window1_width;
      window_chngs.x                = new_window1_x;
      window_chngs.y                = new_window1_y;

      window_mask = CWX
                  | CWY
                  | CWWidth
                  | CWHeight;

      XConfigureWindow(display, new_window_1, window_mask, &window_chngs);


      window_chngs.height           = new_window2_height;
      window_chngs.width            = new_window2_width;
      window_chngs.x                = new_window2_x;
      window_chngs.y                = new_window2_y;

      XConfigureWindow(display, new_window_2, window_mask, &window_chngs);

      window_chngs.height           = new_window3_height;
      window_chngs.width            = new_window3_width;
      window_chngs.x                = new_window3_x;
      window_chngs.y                = new_window3_y;

      XConfigureWindow(display, new_window_3, window_mask, &window_chngs);

      window_chngs.height           = new_window4_height;
      window_chngs.width            = new_window4_width;
      window_chngs.x                = new_window4_x;
      window_chngs.y                = new_window4_y;

      XConfigureWindow(display, new_window_4, window_mask, &window_chngs);

}


/***************************************************************
*
*  Map the windows and we are done.
*
***************************************************************/

XMapWindow(display, new_window_1);
XMapWindow(display, new_window_2);
XMapWindow(display, new_window_3);
XMapWindow(display, new_window_4);

return(1);

} /* end of get_dm_windows  */


/*static char *sccsid = "%Z% %M% %I% - %G% %U% ";*/

/***************************************************************
*
*  module dumpxevent.c
*
*  Routines:
*         dump_Xevent                 - Format and dump an X event, debugging only
*         translate_buttonmask        - Translate a button mask into a string
*
*  Local:
*         translate_detail            - Translate notify X event data into a string
*         translate_configure_detail  - Translate detail field from a configure request into a string
*         translate_mode              - Translate xcrossing data into a string 
*         translate_visibility        - Translate visibility state data into a string
* 
*
***************************************************************/

#include <stdio.h>       /* /usr/include/stdio.h      */
#include <string.h>      /* /bsd4.3/usr/include/string.h     */

/*#include <X11/X.h>       /* /usr/include/X11/X.h      */
/*#include <X11/Xlib.h>    /* /usr/include/X11/Xlib.h   */
#include <X11/Xutil.h>   /* /usr/include/X11/Xutil.h  */
#include <X11/Xproto.h>  /* /usr/include/X11/Xproto.h */

/*#include "dumpxevent.h"*/

/***************************************************************
*  
*  dump_Xevent and the local routines are only needed when
*  compiling for debug.
*  
***************************************************************/

#ifdef DebuG

/**************************************************************
*  
*  Routine dump_Xevent
*
*   This routine will format and dump an X window system
*   event structure to the file descriptor passed.
*
* PARAMETERS:
*
*   stream      -   Pointer to File (INPUT)
*                   This is the stream file to write to.
*                   It must already be open.  Values such
*                   as stderr or stdout work fine.  Or you
*                   may fopen a file and pass the returned
*                   pointer.
*
*   event       -   Pointer to XEvent (INPUT)
*                   The event to dump.  This is the event
*                   structure returned bu a call to XNextEvent,
*                   An event handler in motif, or the event
*                   structure from a calldata parm in a motif
*                   widget callback routine.
*
*   level       -   int (INPUT)
*                   This parm indicates how much stuff to dump
*                   Values:
*                   0  -  Do nothing, just return
*                   1  -  Dump just the name of the event
*                   2  -  Dump the major interesting fields
*                   3  -  Dump all the fields
*
*  FUNCTIONS:
*
*  1.   If the level is zero or less, just return.
*
*  2.   Switch on the event type and print the name of the event
*
*  3.   Based on the level parm format and print some or all
*       of the fields.  In general, level 3 prints the
*       serial number, display token, and whether this event
*       was generated by send event.
*       
*
*  OUTPUTS:
*   Data is written to the file referenced by input parm stream.
*
*
*  SAMPLE USAGE:
*
*     Display  *display;
*     XEvent    event_union;
*     int       debug;
*
*     while (1)
*     {
*         XNextEvent (display, &event_union);
*         if (debug)
*            dump_Xevent(stderr, &event_union, debug);
*         switch (event_union.type)
*         {
*            ...
*  
***************************************************************/

static void translate_detail(int              detail,
                             char            *work);

static void translate_configure_detail(int              detail,
                                       char            *work);

static void translate_mode(int              mode,
                           char            *work);

static void translate_visibility(int              state,
                                 char            *work);

#define itoa(v, s) ((sprintf(s, "UNKNOWN VALUE (%d)", (v))), (s))



void dump_Xevent(FILE     *stream,
                 XEvent   *event,
                 int       level)
{
char     work[256];
char     work2[256];
char     work3[256];
int      len;
char    *x_key_name;
KeySym   keysym;

if (level <= 0)
   return;

switch(event->type)
{
case  KeyPress:
case  KeyRelease:
   if (event->type == KeyPress)
      fprintf(stream, "event  KeyPress\n");
   else
      fprintf(stream, "event  KeyRelease\n");
   if (level > 1)
      {
         translate_buttonmask(event->xkey.state, work);
         len = XLookupString((XKeyEvent *)event, work2, sizeof(work2), &keysym,  NULL);
         work2[len] = '\0';
         x_key_name = XKeysymToString(keysym);
         if (x_key_name == NULL)
            x_key_name = "";
         fprintf(stream, "     keysym = \"%s\" (0x%02X), state = %s (0x%02X)\n     X key name = \"%s\",  hardware keycode = 0x%02X\n     x = %d, y = %d\n     x_root = %d, y_root = %d\n     window = 0x%02X, same_screen = %s\n",
                        work2, keysym,
                        work, event->xkey.state,
                        x_key_name, event->xkey.keycode,
                        event->xkey.x, event->xkey.y,
                        event->xkey.x_root, event->xkey.y_root,
                        event->xkey.window,
                        (event->xkey.same_screen ? "True" : "False"));
      }
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, root = 0x%X, subwindow = 0x%02X, time = %d, serial = %u\n",
                     event->xkey.display,
                     (event->xkey.send_event ? "True" : "False"),
                     event->xkey.root,
                     event->xkey.subwindow,
                     event->xkey.time,
                     event->xkey.serial);
   break;

case  ButtonPress:
case  ButtonRelease:
   if (event->type == ButtonPress)
      fprintf(stream, "event  ButtonPress\n");
   else
      fprintf(stream, "event  ButtonRelease\n");
   if (level > 1)
      {
         translate_buttonmask(event->xkey.state, work);
         fprintf(stream, "     button = %u, state = %s (0x%02X)\n     x = %d, y = %d\n     x_root = %d, y_root = %d\n     window = 0x%02X, same_screen = %s\n",
                        event->xbutton.button,
                        work, event->xbutton.state,
                        event->xbutton.x, event->xbutton.y,
                        event->xbutton.x_root, event->xbutton.y_root,
                        event->xbutton.window,
                        (event->xbutton.same_screen ? "True" : "False"));
      }
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, root = 0x%X, subwindow = 0x%02X, time = %d, serial = %u\n",
                     event->xbutton.display,
                     (event->xbutton.send_event ? "True" : "False"),
                     event->xbutton.root,
                     event->xbutton.subwindow,
                     event->xbutton.time,
                     event->xbutton.serial);
   break;

case  MotionNotify:
   fprintf(stream, "event  MotionNotify\n");
   if (level > 1)
      {
         translate_buttonmask(event->xkey.state, work);
         fprintf(stream, "     is_hint = 0x%02x, state = %s (0x%02X)\n     x = %d, y = %d\n     x_root = %d, y_root = %d\n     window = 0x%02X, same_screen = %s\n",
                        event->xmotion.is_hint,
                        work, event->xmotion.state,
                        event->xmotion.x, event->xmotion.y,
                        event->xmotion.x_root, event->xmotion.y_root,
                        event->xmotion.window,
                        (event->xmotion.same_screen ? "True" : "False"));
      }
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, root = 0x%X, subwindow = 0x%02X, time = %u, serial = %u\n",
                     event->xmotion.display,
                     (event->xmotion.send_event ? "True" : "False"),
                     event->xmotion.root,
                     event->xmotion.subwindow,
                     event->xmotion.time,
                     event->xmotion.serial);
   break;

case  EnterNotify:
case  LeaveNotify:
   if (event->type == EnterNotify)
      fprintf(stream, "event  EnterNotify\n");
   else
      fprintf(stream, "event  LeaveNotify\n");
   if (level > 1)
      {
         translate_buttonmask(event->xcrossing.state, work);
         translate_detail(event->xcrossing.detail, work2);
         translate_mode(event->xcrossing.mode, work3);
         fprintf(stream, "     mode = %s (0x%02X),  state = %s (0x%02X), detail = %s (0x%02X)\n     x = %d, y = %d\n     x_root = %d, y_root = %d\n     focus = %s, window = 0x%02X, same_screen = %s\n",
                        work3, event->xcrossing.mode,
                        work, event->xcrossing.state,
                        work2, event->xcrossing.detail,
                        event->xcrossing.x, event->xcrossing.y,
                        event->xcrossing.x_root, event->xcrossing.y_root,
                        (event->xcrossing.focus ? "True" : "False"),
                        event->xcrossing.window,
                        (event->xcrossing.same_screen ? "True" : "False"));
      }
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, root = 0x%X, subwindow = 0x%X, time = %d, serial = %u\n",
                     event->xcrossing.display,
                     (event->xcrossing.send_event ? "True" : "False"),
                     event->xcrossing.root,
                     event->xcrossing.subwindow,
                     event->xcrossing.time,
                     event->xcrossing.serial);
   break;

case  FocusIn:
case  FocusOut:
   if (event->type == FocusIn)
      fprintf(stream, "event  FocusIn\n");
   else
      fprintf(stream, "event  FocusOut\n");
   if (level > 1)
      {
         translate_detail(event->xfocus.detail, work2);
         translate_mode(event->xfocus.mode, work3);
         fprintf(stream, "     mode = %s (0x%02X), detail = %s (0x%02X), window = 0x%02X\n",
                        work3, event->xfocus.mode,
                        work2, event->xfocus.detail,
                        event->xfocus.window);
      }
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xfocus.display,
                     (event->xfocus.send_event ? "True" : "False"),
                     event->xfocus.serial);
   break;

case  KeymapNotify:
   fprintf(stream, "event  KeymapNotify\n");
   if (level > 1)
      {
         fprintf(stream, "     key_vector = %08X %08X %08X %08X %08X %08X %08X %08X, window = 0x%02X\n",
                        *(int *)&event->xkeymap.key_vector[0],
                        *(int *)&event->xkeymap.key_vector[4],
                        *(int *)&event->xkeymap.key_vector[8],
                        *(int *)&event->xkeymap.key_vector[12],
                        *(int *)&event->xkeymap.key_vector[16],
                        *(int *)&event->xkeymap.key_vector[20],
                        *(int *)&event->xkeymap.key_vector[24],
                        *(int *)&event->xkeymap.key_vector[28],
                        event->xkeymap.window);
      }
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xkeymap.display,
                     (event->xkeymap.send_event ? "True" : "False"),
                     event->xkeymap.serial);
   break;

case  Expose:
   fprintf(stream, "event  Expose\n");
   if (level > 1)
      fprintf(stream, "     x = %d, y = %d, width = %d, height = %d\n     window = 0x%02X, count = %d\n",
                     event->xexpose.x, event->xexpose.y,
                     event->xexpose.width, event->xexpose.height,
                     event->xexpose.window,
                     event->xexpose.count);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xexpose.display,
                     (event->xexpose.send_event ? "True" : "False"),
                     event->xexpose.serial);
   break;

case  GraphicsExpose:
   fprintf(stream, "event  GraphicsExpose\n");
   if (level > 1)
      fprintf(stream, "     x = %d, y = %d, width = %d, height = %d\n     drawable = 0x%02X, count = %d\n     major_code = %s, minor_code = %d\n",
                     event->xgraphicsexpose.x, event->xgraphicsexpose.y,
                     event->xgraphicsexpose.width, event->xgraphicsexpose.height,
                     event->xgraphicsexpose.drawable,
                     event->xgraphicsexpose.count,
                     ((event->xgraphicsexpose.major_code == X_CopyArea) ? "CopyArea" : ((event->xgraphicsexpose.major_code == X_CopyPlane) ? "CopyPlane" : itoa(event->xgraphicsexpose.major_code, work))),
                     event->xgraphicsexpose.minor_code);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xgraphicsexpose.display,
                     (event->xgraphicsexpose.send_event ? "True" : "False"),
                     event->xgraphicsexpose.serial);
   break;

case  NoExpose:
   fprintf(stream, "event  NoExpose\n");
   if (level > 1)
      fprintf(stream, "     drawable = 0x%X\n     major_code = %s, minor_code = %d\n",
                     event->xnoexpose.drawable,
                     ((event->xnoexpose.major_code == X_CopyArea) ? "CopyArea" : ((event->xnoexpose.major_code == X_CopyArea) ? "CopyPlane" : itoa(event->xnoexpose.major_code, work))),
                     event->xnoexpose.minor_code);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xnoexpose.display,
                     (event->xnoexpose.send_event ? "True" : "False"),
                     event->xnoexpose.serial);
   break;

case  VisibilityNotify:
   fprintf(stream, "event  VisibilityNotify\n");
   if (level > 1)
      {
         translate_visibility(event->xvisibility.state, work);
         fprintf(stream, "     state = %s (0x%02X), window = 0x%02X\n",
                        work, event->xvisibility.state,
                        event->xvisibility.window);
      }
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xvisibility.display,
                     (event->xvisibility.send_event ? "True" : "False"),
                     event->xvisibility.serial);
   break;

case  CreateNotify:
   fprintf(stream, "event  CreateNotify\n");
   if (level > 1)
      fprintf(stream, "     x = %d, y = %d, width = %d, height = %d\n     Border width = %d, override_redirect = %s\n     window = 0x%X, Parent = 0x%X\n",
                     event->xcreatewindow.x, event->xcreatewindow.y,
                     event->xcreatewindow.width, event->xcreatewindow.height,
                     event->xcreatewindow.border_width,
                     (event->xcreatewindow.override_redirect ? "True" : "False"),
                     event->xcreatewindow.window,
                     event->xcreatewindow.parent);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xcreatewindow.display,
                     (event->xcreatewindow.send_event ? "True" : "False"),
                     event->xcreatewindow.serial);
   break;

case  DestroyNotify:
   fprintf(stream, "event  DestroyNotify\n");
   if (level > 1)
      fprintf(stream, "     event = 0x%X, window = 0x%X\n",
                     event->xdestroywindow.event,
                     event->xdestroywindow.window);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xdestroywindow.display,
                     (event->xdestroywindow.send_event ? "True" : "False"),
                     event->xdestroywindow.serial);
   break;

case  UnmapNotify:
   fprintf(stream, "event  UnmapNotify\n");
   if (level > 1)
      fprintf(stream, "     event = 0x%X, window = 0x%X, from_configure = %s\n",
                     event->xunmap.event,
                     event->xunmap.window,
                     (event->xunmap.from_configure ? "True" : "False"));
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xunmap.display,
                     (event->xunmap.send_event ? "True" : "False"),
                     event->xunmap.serial);
   break;

case  MapNotify:
   fprintf(stream, "event  MapNotify\n");
   if (level > 1)
      fprintf(stream, "     event = 0x%X, window = 0x%X, override_redirect = %s\n",
                     event->xmap.event,
                     event->xmap.window,
                     (event->xmap.override_redirect ? "True" : "False"));
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xmap.display,
                     (event->xmap.send_event ? "True" : "False"),
                     event->xmap.serial);
   break;

case  MapRequest:
   fprintf(stream, "event  MapRequest\n");
   if (level > 1)
      fprintf(stream, "     parent = 0x%X, window = 0x%02X\n",
                     event->xmaprequest.parent,
                     event->xmaprequest.window);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xmaprequest.display,
                     (event->xmaprequest.send_event ? "True" : "False"),
                     event->xmaprequest.serial);
   break;

case  ReparentNotify:
   fprintf(stream, "event  ReparentNotify\n");
   if (level > 1)
      fprintf(stream, "     event = 0x%X, window = 0x%02X, parent = 0x%X\n     x = %d, y = %d, override_redirect = %s\n",
                     event->xreparent.event,
                     event->xreparent.window,
                     event->xreparent.parent,
                     event->xreparent.x,
                     event->xreparent.y,
                     (event->xreparent.override_redirect ? "True" : "False"));
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xreparent.display,
                     (event->xreparent.send_event ? "True" : "False"),
                     event->xreparent.serial);
   break;

case  ConfigureNotify:
   fprintf(stream, "event  ConfigureNotify\n");
   if (level > 1)
      fprintf(stream, "     x = %d, y = %d, width = %d, height = %d\n     Border width = %d, override_redirect = %s\n     Event = 0x%X, window = 0x%X, above = 0x%X\n",
                     event->xconfigure.x, event->xconfigure.y,
                     event->xconfigure.width, event->xconfigure.height,
                     event->xconfigure.border_width,
                     (event->xconfigure.override_redirect ? "True" : "False"),
                     event->xconfigure.event,
                     event->xconfigure.window,
                     event->xconfigure.above);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xconfigure.display,
                     (event->xconfigure.send_event ? "True" : "False"),
                     event->xconfigure.serial);
   break;

case  ConfigureRequest:
   fprintf(stream, "event  ConfigureRequest\n");
   if (level > 1)
      {
      translate_configure_detail(event->xconfigurerequest.detail, work);
      fprintf(stream, "     x = %d, y = %d, width = %d, height = %d\n     Border width = %d, detail = %s, value_mask = 0x%08X\n     parent = 0x%X, window = 0x%X, above = 0x%X\n",
                     event->xconfigurerequest.x, event->xconfigurerequest.y,
                     event->xconfigurerequest.width, event->xconfigurerequest.height,
                     event->xconfigurerequest.border_width,
                     work,
                     event->xconfigurerequest.value_mask,
                     event->xconfigurerequest.parent,
                     event->xconfigurerequest.window,
                     event->xconfigurerequest.above);
      }
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xconfigurerequest.display,
                     (event->xconfigurerequest.send_event ? "True" : "False"),
                     event->xconfigurerequest.serial);
   break;

case  GravityNotify:
   fprintf(stream, "event  GravityNotify\n");
   if (level > 1)
      fprintf(stream, "     x = %d, y = %d     Event = %d, window = 0x%02X\n",
                     event->xgravity.x, event->xgravity.y,
                     event->xgravity.event,
                     event->xgravity.window);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xgravity.display,
                     (event->xgravity.send_event ? "True" : "False"),
                     event->xgravity.serial);
   break;

case  ResizeRequest:
   fprintf(stream, "event  ResizeRequest\n");
   if (level > 1)
      fprintf(stream, "     width = %d, height = %d\n     window = 0x%X\n",
                     event->xresizerequest.width, event->xresizerequest.height,
                     event->xconfigurerequest.window);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xresizerequest.display,
                     (event->xresizerequest.send_event ? "True" : "False"),
                     event->xresizerequest.serial);
   break;

case  CirculateNotify:
   fprintf(stream, "event  CirculateNotify\n");
   if (level > 1)
      fprintf(stream, "     window = 0x%X, event = 0x%X, place = %s\n",
                     event->xcirculate.window,
                     event->xcirculate.event,
                     ((event->xcirculate.place == PlaceOnTop) ? "PlaceOnTop" : ((event->xcirculate.place == PlaceOnBottom) ? "PlaceOnBottom" : itoa(event->xcirculate.place, work))));
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xcirculate.display,
                     (event->xcirculate.send_event ? "True" : "False"),
                     event->xcirculate.serial);
   break;

case  CirculateRequest:
   fprintf(stream, "event  CirculateRequest\n");
   if (level > 1)
      fprintf(stream, "     window = 0x%X, parent = 0x%X, place = %s\n",
                     event->xcirculaterequest.window,
                     event->xcirculaterequest.parent,
                     ((event->xcirculaterequest.place == PlaceOnTop) ? "PlaceOnTop" : ((event->xcirculaterequest.place == PlaceOnBottom) ? "PlaceOnBottom" : itoa(event->xcirculaterequest.place, work))));
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xcirculaterequest.display,
                     (event->xcirculaterequest.send_event ? "True" : "False"),
                     event->xcirculaterequest.serial);
   break;

case  PropertyNotify:
   fprintf(stream, "event  PropertyNotify\n");
   if (level > 1)
      fprintf(stream, "     window = 0x%X, atom = %d, state = %s, time = %d\n",
                     event->xproperty.window,
                     event->xproperty.atom,
                     ((event->xproperty.state == PropertyNewValue) ? "PropertyNewValue" : ((event->xproperty.state == PropertyDelete) ? "PropertyDelete" : itoa(event->xproperty.state, work))),
                     event->xproperty.time);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xproperty.display,
                     (event->xproperty.send_event ? "True" : "False"),
                     event->xproperty.serial);
   break;

case  SelectionClear:
   fprintf(stream, "event  SelectionClear\n");
   if (level > 1)
      fprintf(stream, "     window = 0x%02X, selection = %d, time = %d\n",
                     event->xselectionclear.window,
                     event->xselectionclear.selection,
                     event->xselectionclear.time);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xselectionclear.display,
                     (event->xselectionclear.send_event ? "True" : "False"),
                     event->xselectionclear.serial);
   break;

case  SelectionRequest:
   fprintf(stream, "event  SelectionRequest\n");
   if (level > 1)
      fprintf(stream, "     owner = 0x%X, requestor = 0x%X, selection = %d\n     target = 0x%02, property = 0X%02X, time = %d\n",
                     event->xselectionrequest.owner,
                     event->xselectionrequest.requestor,
                     event->xselectionrequest.selection,
                     event->xselectionrequest.target,
                     event->xselectionrequest.property,
                     event->xselectionrequest.time);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xselectionrequest.display,
                     (event->xselectionrequest.send_event ? "True" : "False"),
                     event->xselectionrequest.serial);
   break;

case  SelectionNotify:
   fprintf(stream, "event  SelectionNotify\n");
   if (level > 1)
      fprintf(stream, "     requestor = 0x%X, selection = %d,  target = %d\n     property = 0X%02X, time = %d\n",
                     event->xselection.requestor,
                     event->xselection.selection,
                     event->xselection.target,
                     event->xselection.property,
                     event->xselection.time);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xselection.display,
                     (event->xselection.send_event ? "True" : "False"),
                     event->xselection.serial);
   break;

case  ColormapNotify:
   fprintf(stream, "event  ColormapNotify\n");
   if (level > 1)
      fprintf(stream, "     colormap = %s, new = %s, state = %s window = 0x%02X\n",
                     ((event->xcolormap.colormap == None) ? "None" : itoa(event->xcolormap.colormap, work)),
                     (event->xcolormap.new ? "True" : "False"),
                     ((event->xcolormap.state == ColormapInstalled) ? "ColormapInstalled" : ((event->xcolormap.state == ColormapUninstalled) ? "ColormapUninstalled" : itoa(event->xcolormap.state, work))),
                     event->xcolormap.window);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xcolormap.display,
                     (event->xcolormap.send_event ? "True" : "False"),
                     event->xcolormap.serial);
   break;

case  ClientMessage:
   fprintf(stream, "event  ClientMessage\n");
   if (level > 1)
      fprintf(stream, "     type = 0x%02X, format = 0x%02X,\n     data = %08X %08X %08X %08X %08X\n     window = 0x%02X\n",
                     event->xclient.message_type,
                     event->xclient.format,
                     event->xclient.data.l[0],
                     event->xclient.data.l[1],
                     event->xclient.data.l[2],
                     event->xclient.data.l[3],
                     event->xclient.data.l[4],
                     event->xclient.window);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xclient.display,
                     (event->xclient.send_event ? "True" : "False"),
                     event->xclient.serial);
   break;

case  MappingNotify:
   fprintf(stream, "event  MappingNotify\n");
   if (level > 1)
      fprintf(stream, "     request = %s, first_keycode = %d (0x%x), count = %d\n",
                     (event->xmapping.request == MappingModifier ? "MappingModifer" :
                      (event->xmapping.request == MappingKeyboard ? "MappingKeyboard" :
                       (event->xmapping.request == MappingPointer ? "MappingPointer" :
                       itoa(event->xmapping.request, work)))),
                       event->xmapping.first_keycode, event->xmapping.first_keycode,
                     event->xmapping.count);
   if (level > 2)
      fprintf(stream, "     display = 0x%02X, send_event = %s, serial = %u\n",
                     event->xmapping.display,
                     (event->xmapping.send_event ? "True" : "False"),
                     event->xmapping.serial);
   break;

default:
   fprintf(stream, "Unknown event type %02X (%d)\n", event->type, event->type);
   break;

}  /* end of switch statement */

if (level > 1)
   fprintf(stream, "\n");

} /* end of dump_Xevent */

static void translate_detail(int              detail,
                             char            *work)
{

if (detail == NotifyAncestor)
   strcpy(work, "NotifyAncestor");
else
   if (detail == NotifyInferior)
      strcpy(work, "NotifyInferior");
   else
      if (detail == NotifyNonlinear)
         strcpy(work, "NotifyNonlinear");
      else
         if (detail == NotifyNonlinearVirtual)
            strcpy(work, "NotifyNonlinearVirtual");
         else
            if (detail == NotifyVirtual)
               strcpy(work, "NotifyVirtual");
            else
               if (detail == NotifyPointer)
                  strcpy(work, "NotifyPointer");
               else
                  if (detail == NotifyPointerRoot)
                     strcpy(work, "NotifyPointerRoot");
                  else
                     if (detail == NotifyDetailNone)
                        strcpy(work, "NotifyDetailNone");
                     else
                        sprintf(work, "UNKNOWN VALUE 0x%02X", detail);
} /* end of translate_detail */


static void translate_configure_detail(int              detail,
                                       char            *work)
{

if (detail == None)
   strcpy(work, "None");
else
   if (detail == Above)
      strcpy(work, "Above");
   else
      if (detail == Below)
         strcpy(work, "Below");
      else
         if (detail == TopIf)
            strcpy(work, "TopIf");
         else
            if (detail == BottomIf)
               strcpy(work, "BottomIf");
            else
               if (detail == Opposite)
                  strcpy(work, "Opposite");
               else
                  sprintf(work, "UNKNOWN VALUE 0x%02X", detail);
} /* end of translate_configure_detail */


static void translate_mode(int              mode,
                           char            *work)
{

if (mode == NotifyNormal)
   strcpy(work, "NotifyNormal");
else
   if (mode == NotifyGrab)
      strcpy(work, "NotifyGrab");
   else
      if (mode == NotifyUngrab)
         strcpy(work, "NotifyUngrab");
      else
         sprintf(work, "UNKNOWN VALUE 0x%02X", mode);
} /* end of translate_mode */


static void translate_visibility(int              state,
                                 char            *work)
{

if (state == VisibilityFullyObscured)
   strcpy(work, "VisibilityFullyObscured");
else
   if (state == VisibilityPartiallyObscured)
      strcpy(work, "VisibilityPartiallyObscured");
   else
      if (state == VisibilityUnobscured)
         strcpy(work, "VisibilityUnobscured");
      else
         sprintf(work, "UNKNOWN VISIBILITY VALUE 0x%02X", state);
} /* end of translate_visibility */

#endif

void translate_buttonmask(unsigned int     state,
                          char            *work)
{
work[0] = '\0';
if (state & ShiftMask)                strcat(work, " Shift");
if (state & ControlMask)              strcat(work, " Cntl");
if (state & LockMask)                 strcat(work, " Shift-Lock");
if (state & Mod1Mask)                 strcat(work, " Mod1");
if (state & Mod2Mask)                 strcat(work, " Mod2");
if (state & Mod3Mask)                 strcat(work, " Mod3");
if (state & Mod4Mask)                 strcat(work, " Mod4");
if (state & Mod5Mask)                 strcat(work, " Mod5");
if (state & Button1Mask)              strcat(work, " Button1");
if (state & Button2Mask)              strcat(work, " Button2");
if (state & Button3Mask)              strcat(work, " Button3");
if (state & Button4Mask)              strcat(work, " Button4");
if (state & Button5Mask)              strcat(work, " Button5");
if (work[0] == '\0') strcpy(work, "(NO MODIFIERS)");
} /* end of translate_buttonmask */






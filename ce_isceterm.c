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

/**************************************************************
*
*  MODULE ce_isceterm - Arpus/Ce determine if terminal emulator is ceterm
*
*  OVERVIEW
*      This program can be called from a shell to determine if the enclosing
*      shell is a ceterm.  It can do so without having to open the display,
*      thus it is useful for use in a .kshrc file which may be run during
*      a telnet.  It does it's work by sending a special escape sequence
*      down the line which ceterm knows to look for.  Other terminal emulators
*      will ignore this sequence.
*
*  ARGUMENTS:
*      none
*
***************************************************************/


#include <stdio.h>
#include <string.h>         /* /usr/include/string.h       */
#include <errno.h>          /* /usr/include/errno.h        */
#include <sys/types.h>      /* /usr/include/sys/types.h    */
#include <fcntl.h>          /* /usr/include/fcntl.h        */
#include <sys/time.h>       /* /usr/include/sys/time.h     */
#ifndef FD_ZERO
#include <sys/select.h>     /* /usr/include/sys/select.h   */
#endif
#ifdef blah
#include <termio.h>
/* termio.h not in freebsd */
#endif

#include "pad.h"         /* needed for macro HT */
#include "unixwin.h"

#ifdef blah
static void dump_termio(FILE             *stream,
                        struct termio    *term,
                        char             *header);


#include "hexdump.h"
#endif

#define VT_ESC   0x1B
char    special_string[] = {0x02, 0x02, 0x08, 0x08, 0 };
/* special_string2 also in pad.c */
char    special_string2[] = {0x02, 0x02, 0 };

void exit(int code);
int  read();
int  write();
char *getenv(const char*);

int main(int argc, char *argv[])
{
int                   nfound;
char                  buff[10];
fd_set                readfds;
struct timeval        time_out;
int                   debug = 0;
#ifdef blah
struct termio         save;
struct termio         new;
#endif
int                   rc = 1;
int                   ioctl_rc;
int                   s2_len;
int                   chars1;
char                 *term;

if ((argc > 1) && (strcmp(argv[1], "-d") == 0))
   debug = 1;

if (!isatty(0) || !isatty(1))
   {
      if (debug)
         fprintf(stderr, "stdin is %sa tty, stdout is %sa tty\n", (isatty(0) ? "" : "not "), (isatty(1) ? "" : "not "));
      return(1);
   }

term = getenv("TERM");
if (!term)
   {
      if (debug)
         fprintf(stderr, "No TERM environment variable, not a ceterm\n");
      return(1);
   }
else
   if (strncmp(term, "vt100", 5) != 0)
   {
      if (debug)
         fprintf(stderr, "TERM is not vt100, it is %s\n", term);
      return(1);
   }
else
   return(0);

#ifdef blah

ioctl_rc = ioctl(0, TCGETA, &save);
if (ioctl_rc < 0)
   perror("TCGETA");
memcpy((char *)&new, (char *)&save, sizeof(save));
/*dump_termio(stderr, &save, "Original terminal state");*/

new.c_lflag &= ~ECHO & ~ICANON;
new.c_cc[VMIN] = (char)1;
new.c_cc[VTIME] = (char)0;
/*dump_termio(stderr, &new, "New terminal state");*/

ioctl_rc = ioctl(0, TCSETA, &new);
if (ioctl_rc < 0)
   perror("first TCSETA");

/***************************************************************
*  Send the magic string to the ceterm.
***************************************************************/
/*sprintf(buff, "%cZ", VT_ESC);*/
/*sprintf(buff, "%c[c\n", VT_ESC);*/
/*sprintf(buff, "    ");*/
write(1, special_string, strlen(special_string));

FD_ZERO(&readfds);
FD_SET(0, &readfds);
time_out.tv_sec = 4;
time_out.tv_usec = 0;

/***************************************************************
*  See if ceterm responded.
***************************************************************/
nfound = select(2, HT &readfds, NULL, NULL, &time_out);

if (nfound > 0)
   {
      s2_len = strlen(special_string2);
      chars1 = read(0, buff, s2_len);
      if (debug)
         {
            buff[chars1] = '\0';
            fprintf(stderr, "chars = %d, expected chars = %d\n", chars1, s2_len);
            hexdump(stderr, "buff", (char *)buff, chars1, 0);
         }
      /***************************************************************
      *  Deal with got partial response.
      *  special_string2 is only 2 characters long.
      ***************************************************************/
      if ((chars1 < s2_len) && (strncmp(special_string2, buff, chars1) == 0))
         {
            FD_ZERO(&readfds);
            FD_SET(0, &readfds);
            time_out.tv_sec = 2;
            time_out.tv_usec = 0;
            nfound = select(2, HT &readfds, NULL, NULL, &time_out);

            if (nfound > 0)
               {
                  chars1 += read(0, &buff[chars1], s2_len-chars1);
                  if (debug)
                     {
                        buff[chars1] = '\0';
                        fprintf(stderr, "read 2 total chars = %d, expected chars = %d\n", chars1, s2_len);
                        hexdump(stderr, "buff", (char *)buff, chars1, 0);
                     }
               }
         }

      if ((chars1 == s2_len) && (strncmp(special_string2, buff, chars1) == 0))
         rc = 0;
   }
else
   {
      if (debug)
         fprintf(stderr, "not ceterm, %d\n", nfound);
   }

ioctl_rc = ioctl(0, TCSETA, &save);
if (ioctl_rc < 0)
   perror("second TCSETA");

return(rc);

#endif


}   /* end of main */

#ifdef blah
static void dump_termio(FILE             *stream,
                        struct termio    *term,
                        char             *header)
{
int i;


fprintf(stream, "termio.h structure: %s\n", header);
fprintf(stream, "c_iflag = 0x%04X\n", term->c_iflag);
fprintf(stream, "c_oflag = 0x%04X\n", term->c_oflag);
fprintf(stream, "c_cflag = 0x%04X\n", term->c_cflag);
fprintf(stream, "c_lflag = 0x%04X\n", term->c_lflag);
fprintf(stream, "control characters = \"");
for (i = 1; i < NCC; i++)
   fprintf(stream, "%d ");
fprintf(stream, "\"\n");

} /* end of dump_termio */
#endif

/*static char *sccsid = "%Z% %M% %I% - %G% %U%";*/
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


#include <stdio.h>   /* /usr/include/stdio.h  */
#include <errno.h>   /* /usr/include/errno.h  */
#include <string.h>  /* /usr/include/string.h */
/*#include <sys/types.h>       /* /bsd4.3/usr/include/sys/types.h    */
/*#include <sys/stat.h>       /* /bsd4.3/usr/include/sys/stat.h    */
/*#include <netinet/in.h>      /* /bsd4.3/usr/include/netinet/in.h */

/*#include <X11/Xlib.h>*/
#define False 0

#include "ceapi.h"
#include "hexdump.h"

main (int argc, char *argv[])
{
char       buffer[1024];
int        rc;
CeStats   *ce_stats;
char      *back;
char      *pos_line;
int        row;
int        col;

rc = ceApiInit(0);
if (rc != 0)
   {
      fprintf(stderr, "ApiInit failed \n");
      exit(1);
   }

/* get the current position and remember it */
/* sample pos_line returned "Line: 1, Column: 1, X: 5, Y: 38,  Root X: 558,  Root Y: 39" */
pos_line = ceGetMsg("=");
if (pos_line)
  sscanf(pos_line, "%s %d, %s %d", buffer, &row, buffer, &col);
else
   fprintf(stderr, "Could not get pos\n");
free(pos_line);

ce_stats = ceGetStats();
if (ce_stats == NULL)
   {
      fprintf(stderr, "Could not get stats\n");
      exit(1);
   }

sprintf(buffer, "total_lines     = %d\ncurrent_line    = %d\ncurrent_col     = %d\nwritable        = %d\n",
        ce_stats->total_lines, ce_stats->current_line, ce_stats->current_col, ce_stats->writable);

if (!ce_stats->writable)
   {
      fprintf(stderr, "Ce pad is not writable\n");
      exit(1);
   }

rc = ceXdmc("msg 'Inserting after line 4'", False);
if (rc != 0)
   {
      fprintf(stderr, "Could not xdmc insert message\n");
      exit(rc);
   }

rc = cePutLines(4, 4, 1, buffer); /* insert before line 4 */
if (rc != 0)
   {
      fprintf(stderr, "cePutLines failed (%s)\n", strerror(errno));
      exit(rc);
   }

rc = ceXdmc("msg 'Reading the lines back'", False);
if (rc != 0)
   {
      fprintf(stderr, "Could not xdmc read message\n");
      exit(rc);
   }

back = ceGetLines(4, 4);
if (back == NULL)
   {
      fprintf(stderr, "ceGetLines failed (%s)\n", strerror(errno));
      exit(rc);
   }
else
   {
      if (strcmp(back, buffer) == 0)
         {
            rc = ceXdmc("msg 'strings matched'", False);
            fprintf(stderr, "strings matched\n");
         }
      else
         {
            hexdump(stderr, "sent", buffer, strlen(buffer), 0);
            hexdump(stderr, "returned", back, strlen(back), 0);
         }
      
   }

sleep(4);  /* let the user look at the change */
rc = ceXdmc("msg 'Deleteing first two lines inserted'", False);
if (rc != 0)
   {
      fprintf(stderr, "Could not xdmc insert message\n");
      exit(rc);
   }

rc = ceDelLines(4, 2); /* insert before line 4 */
if (rc != 0)
   {
      fprintf(stderr, "ceDelLines failed (%s)\n", strerror(errno));
      exit(rc);
   }

sleep(4);  /* let the user look at the change */
rc = ceXdmc("msg 'Replacing first line in file '", False);
if (rc != 0)
   {
      fprintf(stderr, "Could not xdmc replace message\n");
      exit(rc);
   }

strcpy(buffer, "this is a test line replacing the first line of the file\n");
rc = cePutLines(1, 1, 0, buffer); /* insert before line 4 */
if (rc != 0)
   {
      fprintf(stderr, "cePutLines failed (%s)\n", strerror(errno));
      exit(rc);
   }


sleep(4);  /* let the user look at the change */


pos_line = ceGetMsg("pn");
if (pos_line)
   fprintf(stderr, "Pad name is %s\n", pos_line);
else
   fprintf(stderr, "Could not get pad name\n");
free(pos_line);


sprintf(buffer, "tmw;[%d,%d];msg 'zapi done'", row, col);

rc = ceXdmc(buffer, False);
exit(rc);

} /* end of main */



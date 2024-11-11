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

/*************************************************************
*
*  ce_gpb  - copy a paste buffer to stdout
*
*  This routine copies an X paste buffer to stdout.
*  It accepts one argument which is the name of the
*  paste buffer.  The default is CLIPBOARD
*  CLIPBOARD is the default paste buffer used by Sun systems
*  and the Ce editor.  For xterm's the name XA_PRIMARY which
*  may be abbreviated in this program as X is the name to use.
*
*  To compile:
*  On Apollo:
*  /bsd4.3/bin/cc -o ce_gpb -I/phx/include ce_gpb.c /phx/lib/libceapi.a
*  On solaris
*  /opt/SUNWspro/bin/cc -o ce_gpb -g -I/phx/include ce_gpb.c /phx/lib/libceapi.a -R /usr/openwin/lib:/usr/ucblib /usr/openwin/lib/libX11.so /lib/libsocket.so /lib/libw.so /lib/libintl.so /lib/libdl.so /lib/libnsl.so
*
*************************************************************/


#include <stdio.h>   /* /usr/include/stdio.h  */

#include "ceapi.h"

main (int argc, char *argv[])
{
char      *buff;
int        rc;

rc = ceApiInit(0);
buff = ceReadPasteBuff(argv[1]);
if (buff)
   write(1, buff, strlen(buff));

} /* end of main */



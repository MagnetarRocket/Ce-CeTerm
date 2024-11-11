#ifndef _HEXDUMP_INCLUDED
#define _HEXDUMP_INCLUDED

/* static char *hexdump_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

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
*  Routine in hexdump.c
*     hexdump              -  dump data in side by side dump format
*  
* DESCRIPTION:
*      This routine is useful in debugging programs which use complex
*      structures.  It accepts a title, a pointer, and an length.
*      It then prints the title, and dumps the data at the pointer in
*      hex and character format.
*
* USAGE:   
*      char  header[]  = "this is the title"
*      char  a[]      = "1234567890abcdefghijklmnopqrstuvwxyz@#$";
*      int   print_addr = 1;
*
*      hexdump(stderr, header, a, sizeof(a), print_addr);
*
*      produces:
this is the title
--ADDR-----OFFSET----DATA---------------------------------------------------------------------------------------------------------
000800AC   000000   31323334 35363738 39306162 63646566 6768696A 6B6C6D6E 6F707172 73747576   * 1234567890abcdefghijklmnopqrstuv *
000800CC   000020   7778797A 40232400                                                         * wxyz.... *
*
*      print_addr = 0;
*      hexdump(stderr, header, a, sizeof(a), print_addr);
*
*      produces:
this is the title
OFFSET----DATA---------------------------------------------------------------------------------------------------------
000000   31323334 35363738 39306162 63646566 6768696A 6B6C6D6E 6F707172 73747576   * 1234567890abcdefghijklmnopqrstuv *
000020   7778797A 40232400                                                         * wxyz.... *
*
*
* For backward compatiblity, The macro hex_dump
* is defined which calls hexdump with the addr parmameter hard
* coded.
***************************************************************/

#define hex_dump(a,b,c,d)  hexdump(a,b,c,d,1)

#ifdef __STDC__
void hexdump(FILE    *ofd,          /* target stream to output to */
             char    *header,       /* title to print             */
             char    *line,         /* data to dump               */
             int      length,       /* length of data to dump     */
             int      print_addr);  /* flag                      */
#else
void hexdump();
#endif

#endif

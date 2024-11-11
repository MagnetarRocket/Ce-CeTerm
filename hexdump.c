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

/*******************************************************************
*
* MODULE: hexdump   -   dump memory in hex/char format
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
*      hexdump(stderr, header, a, sizeof(a));
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
* INPUT PARAMETERS: 
*    ofd    - pointer to FILE
*             The first parm is a pointer to file (FILE  *efd).  This is where
*             the output is written.  stderr is a popular value for this parm.
*    header - pointer to character
*             The second parm points to a null terminated string which is
*             printed as a title over the dumped data.  If a null string
*             is passed, the title is suppressed.
*    line   - pointer to anything
*             The address of the area to be dumped is passed as the third parm
*             to this routine.  It is treated as pointer to character during
*             processing, but that is irrelevant.
*    length - int
*             The fourth parm is the length of the data to dump in bytes.  Any
*             int will do, but sizeof  the data to dump is a very convenient
*             parm to use.
*    print_addr - int
*             The fifth parm is a flag which specfies whether to print the
*             memory addresses of the data being dumped in addition to the offsets.
*             parm to use.
*
* FUNCTIONS:
*      1.  -  Output the header if it is non-null.
*      2.  -  break the data into BYTES_PER_LINE byte pieces and output them in hex and
*             character format.  In the char format area, replace non-printable
*             characters by a dot.
*
* OUTPUTS:
*      The dumped data is written to the file passed.
*
* 
*******************************************************************/


#include <stdio.h>
#include <ctype.h>

/***************************************************************
*  
*  definition for number of bytes of dump data to put out
*  on one line.  
*  
***************************************************************/
#ifndef BYTES_PER_LINE
#define BYTES_PER_LINE  32
#endif

#ifdef __STDC__
void hexdump(FILE    *ofd,         /* target stream to output to */
              char    *header,     /* title to print             */
              char    *line,       /* data to dump               */
              int      length,     /* length of data to dump     */
              int      print_addr) /* flag                       */
#else
void hexdump(ofd, header,line, length, print_addr)
FILE     *ofd;
char     *header;
char     *line;
int       length;
int       print_addr;
#endif
{

char         *ptr;
int           offset = 0;
int           i = 0;
int           j;
int           data_dashes;
unsigned int  tchar;
char         *b_pos;
/* note, there is some padding in the following buffers */
char          text[BYTES_PER_LINE+10];
char          buffer[(BYTES_PER_LINE*3) + 80];

/***************************************************************
*  ptr is the current place in the input string being dumped
***************************************************************/
ptr = line;

/***************************************************************
*  
*  Output the header line if it was supplied
*  
***************************************************************/

if (header[0] != '\0')
   fprintf(ofd,"%s     Decimal len = %d\n",header, length);
else
   fprintf(ofd,"Decimal len = %d\n", length);

/***************************************************************
*  
*  Output the title line above the dump
*  
***************************************************************/

data_dashes = (BYTES_PER_LINE * 3) + (BYTES_PER_LINE / 4);
b_pos = buffer;
if (print_addr)
   {
      sprintf(b_pos, "--ADDR-----OFFSET----DATA-");
      b_pos += 26;
   }
else
   {
      sprintf(b_pos, "--OFFSET----DATA-");
      b_pos += 17;
   }
for (j = 0; j < data_dashes; j++)
   *b_pos++ = '-';
*b_pos = '\0';
fprintf(ofd, "%s\n", buffer);

b_pos = buffer;

/***************************************************************
*  
*  Dump the complete lines of data
*  
***************************************************************/

while (i < length )
{
   /* every 4 bytes put in a space */
   if ( (i % 4) == 0 && i != 0)
      *b_pos++ = ' ';
   /* break lines at BYTES_PER_LINE  bytes */
   if ( (i % BYTES_PER_LINE) == 0 && i != 0)
      {
         *b_pos = '\0';
         fprintf(ofd,"%s  * %s *\n", buffer, text);
         b_pos = buffer;
      }
      

   /**************************************************
   * get one byte of the data to output.
   * if is it printable, put it in the char text.
   * if it is not printable put a dot in the text.
   ***************************************************/
   tchar = *ptr;
   tchar &= 0X000000FF;
   if (isdigit(tchar) || isalpha(tchar))
      text[i % BYTES_PER_LINE] = (char)tchar;
   else
      text[i % BYTES_PER_LINE] = '.';
   text[(i % BYTES_PER_LINE) + 1] = '\0';
    
   /* print out the address at the beginning of the line */
   if ((i % BYTES_PER_LINE) == 0)
      if (print_addr)
         {
                sprintf(b_pos, "%08X   %06X   ", ptr, offset);
                b_pos += 20; /* 8 for addr, 6 for offset, 6 for blanks */
         }
      else
         {
                sprintf(b_pos, "  %06X   ", offset);
                b_pos += 11; /* 6 for offset, 5 for blanks */
         }
    /* print out 1 byte of data in hex */
    sprintf(b_pos, "%02X", tchar);
    b_pos += 2;
    /* bump to the next byte of data, also bump the byte count */
    ptr++;
    offset++;
    i++;
} /* end of while more data */

/***************************************************************
*  
*  Dump the last line of data.  First pad it with blanks.
*  
***************************************************************/

if ((i  % BYTES_PER_LINE) != 0)
   {
#ifndef OMVS
      j = (BYTES_PER_LINE - (i % BYTES_PER_LINE)) * 2;
#else
      j =  BYTES_PER_LINE; /* avoid gcc compiler error in OMVS */
      j -= i % BYTES_PER_LINE;
      j *= 2;
#endif
      j += (j / 8) + 1;
      for (i = 0; i < j; i++)
         *b_pos++ = ' ';
   }
else
   *b_pos++ = ' ';

*b_pos = '\0';
fprintf(ofd,"%s  * %s *\n", buffer, text);

   
}  /* end of hexdump */


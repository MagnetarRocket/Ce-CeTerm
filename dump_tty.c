
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

#include <stdio.h>          /* /bsd4.3/usr/include/stdio.h      */ 
#include <errno.h>          /* /bsd4.3/usr/include/errno.h      */ 
#include <sys/ioctl.h>
#include <fcntl.h>

static struct sgttyb  Dtty_sgttyb; 
static struct tchars  Dtty_tchars; 
static struct ltchars Dtty_ltchars;
static struct winsize Dtty_winsize;
static int Dtty_localmode;
static int Dtty_ldisc;

void dump_tty(void);
void hex_dump(FILE *ofd, char *header, char *line, int length);

main()
{
dump_tty();
}

void dump_tty(void)
{
int tty = 0;
#ifdef _INCLUDE_HPUX_SOURCE
return;
#else
if ((ioctl(tty, TIOCGETP,   (char *) &Dtty_sgttyb) < 0) ||
    (ioctl(tty, TIOCGETC,   (char *) &Dtty_tchars) < 0) ||
    (ioctl(tty, TIOCGLTC,   (char *) &Dtty_ltchars) < 0) ||
    (ioctl(tty, TIOCLGET,   (char *) &Dtty_localmode) < 0) ||
    (ioctl(tty, TIOCGETD,   (char *) &Dtty_ldisc) < 0) ||
    (ioctl(tty, TIOCGWINSZ, (char *) &Dtty_winsize) < 0)){
       fprintf(stderr, "Could not dump tty info(GETMODES(%s))", strerror(errno));   
       return;
}

hex_dump(stderr, "tty_sgttyb", (char *)&Dtty_sgttyb, sizeof(Dtty_sgttyb));
hex_dump(stderr, "tty_tchars", (char *)&Dtty_tchars, sizeof(Dtty_tchars));
hex_dump(stderr, "tty_ltchars", (char *)&Dtty_ltchars, sizeof(Dtty_ltchars));
hex_dump(stderr, "tty_localmode", (char *)&Dtty_localmode, sizeof(Dtty_localmode));
hex_dump(stderr, "tty_ldisc", (char *)&Dtty_ldisc, sizeof(Dtty_ldisc));
hex_dump(stderr, "tty_winsize", (char *)&Dtty_winsize, sizeof(Dtty_winsize));

#endif

}  /*  dump_tty() */


/*******************************************************************
*
* MODULE: hex_dump   -   dump memory in hex/char format
*
* DESCRIPTION:
*      This routine is useful in debugging programs which use complex
*      structures.  It accepts a title, a pointer, and an length.
*      It then prints the title, and dumps the data at the pointer in
*      hex and character format.
*
* USAGE:   
*      hex_dump();
*      
*      char  header[]  = "this is the title"
*      char  a[]      = "1234567890abcdefghijklmnopqrstuvwxyz@#$";
*
*      hex_dump(stderr, header, a, sizeof(a));
*
*      produces:
*this is the title
--ADDR-----OFFSET----DATA---------------------------------------------------------------------------------------------------------
000800AC   000000   31323334 35363738 39306162 63646566 6768696A 6B6C6D6E 6F707172 73747576   * 1234567890abcdefghijklmnopqrstuv *
000800CC   000020   7778797A 40232400                                                         * wxyz.... *
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
*             The forth parm is the length of the data to dump in bytes.  Any
*             int will do, but sizeof  the data to dump is a very convenient
*             parm to use.
*
* FUNCTIONS:
*      1.  -  Output the header if it is non-null.
*      2.  -  break the data into 16 byte pieces and output them in hex and
*             character format.  In the char format area, replace non-printable
*             characters by a dot.
*
* OUTPUTS:
*      The dumped data is written to the file passed.
*
* 
*******************************************************************/

/*  definition for number of bytes of dump data to put out */
#define BYTES_PER_LINE  32

void hex_dump(FILE *ofd, char *header, char *line, int length)
{

char *ptr;
int offset = 0;
int i = 0;
int j;
int data_dashes;
unsigned int tchar;
char text[36];

ptr = line;

/*
 *  output the header line if it was supplied
 */

if (header[0] != '\0')
   fprintf(ofd,"%s\n",header);

/*
 *  output the title line above the dump
 */

data_dashes = (BYTES_PER_LINE * 3) + (BYTES_PER_LINE / 4);
fprintf(ofd,"--ADDR-----OFFSET----DATA-");
for (j = 0; j < data_dashes; j++)
   putc('-', ofd);
putc('\n', ofd);

while (i < length )
{
    /* every 4 bytes put in a space */
    if ( (i % 4) == 0 && i != 0)
       fprintf(ofd, " ");
    /* break lines at BYTES_PER_LINE  bytes */
    if ( (i % BYTES_PER_LINE) == 0 && i != 0)
          fprintf(ofd,"  * %s *\n",text);

    /*
     * get one byte of the data to output.
     * if is it printable, put it in the char text.
     * if it is not printable put a dot in the text.
     */

    tchar = *ptr;
    tchar &= 0X000000FF;
    if ((tchar >= '!') && (tchar <= '~')) /* printable characters */
       text[i % BYTES_PER_LINE] = tchar;
    else
       text[i % BYTES_PER_LINE] = '.';
    text[(i % BYTES_PER_LINE) + 1] = '\0';
    
    /* print out the address at the beginning of the line */
    if ( (i % BYTES_PER_LINE) == 0)
       fprintf(ofd, "%08X   %06X   ", ptr, offset);
    /* print out 1 byte of data in hex */
    fprintf(ofd, "%02X",tchar);
    /* bump to the next byte of data, also the byte count */
    ptr++;
    offset++;
    i++;
}

/*tchar = ' ';*/
j = (BYTES_PER_LINE - (i % BYTES_PER_LINE)) * 2;
j += (j / 8) + 1;
for (i = 0; i < j; i++)
   fprintf(ofd, " ");

fprintf(ofd, "  * %s *\n\n",text);
   
}  /* end of hex_dump() */


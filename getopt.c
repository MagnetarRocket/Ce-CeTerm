static char *sccsid = "%Z% %M% %I% - %G% %U% ";
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

/************************************************************************

NAME:      getopt - routine used by acfpasswd to do UNIX like getopt processing

PURPOSE:    This routine is used with the acfread and acfpasswd routines
            in place of the UNIX getopt function on Windows applications.

PARAMETERS:
   1.  ac    -  int (INPUT)
                This is the argc from the main parm list.

   2.  av    -  array of pointer to char (INPUT)
                This is the argv from the main parm list.

   3.  opts  -  pointer to char (INPUT)
                This is the list of arguments to process.  See
                the UNIX getopt man page for their description.


FUNCTIONS :
   1.   Process each argument and return the data in optarg

   2.   Statically remember the position in the arglist with optind.

RETURNED VALUE:
   optopt     -  int
                 EOF is returned on error or when the last argument is
                 parsed.

*************************************************************************/
/* (C) Copyright 1993 by John G. Myers
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of John G.
 * Myers not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  John G. Myers makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * JOHN G. MYERS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL JOHN G. MYERS BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <stdio.h>
#include <string.h>

#define ERR(s, c)					\
    if (opterr) {					\
	char buff[3];					\
	buff[0] = c; buff[1] = '\n'; buff[2] = '\0';			\
	fputs(av[0], stderr);		\
	fputs(s, stderr);			\
	fputs(buff, stderr);			\
    }


int	opterr = 1;
int	optind = 1;
int	optopt;
char	*optarg;


/*
**  Return options and their values from the command line.
**  This comes from the AT&T public-domain getopt published in mod.sources
**  (i.e., comp.sources.unix before the great Usenet renaming).
*/
#ifdef __STDC__
int getopt(int ac, char * const av[], const char *opts)
#else
int
getopt(ac, av, opts)
    int		ac;
    char	*av[];
    char	*opts;
#endif
{
    extern char	*strchr();
    static int	i = 1;
    char	*p;

    /* Move to next value from argv? */
    if (i == 1) {
	if (optind >= ac ||
#ifdef __MSDOS__
	    (av[optind][0] != '-' && av[optind][0] != '/')
#else
	    av[optind][0] != '-'
#endif
	    || av[optind][1] == '\0')
	    return EOF;
	if (strcmp(av[optind], "--") == 0) {
	    optind++;
	    return EOF;
	}
    }

    /* Get next option character. */
    if ((optopt = av[optind][i]) == ':'
     || (p = strchr(opts,  optopt)) == NULL) {
	ERR(": illegal option -- ", optopt);
	if (av[optind][++i] == '\0') {
	    optind++;
	    i = 1;
	}
	return '?';
    }

    /* Snarf argument? */
    if (*++p == ':') {
	if (av[optind][i + 1] != '\0')
	    optarg = &av[optind++][i + 1];
	else {
	    if (++optind >= ac) {
		ERR(": option requires an argument -- ", optopt);
		i = 1;
		return '?';
	    }
	    optarg = av[optind++];
	}
	i = 1;
    }
    else {
	if (av[optind][++i] == '\0') {
	    i = 1;
	    optind++;
	}
	optarg = NULL;
    }

    return optopt;
}

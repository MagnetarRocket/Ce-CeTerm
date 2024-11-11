#ifndef _CE_MALLOC__INCLUDED
#define _CE_MALLOC__INCLUDED

/* static char *malloc_h_sccsid = "%Z% %M% %I% - %G% %U% "; */
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
*  Routines in emalloc.c
*     malloc_error       - Handle malloc failure. Called from macro CE_MALLOC
*     malloc_init        - CE_MALLOC / malloc_error setup.
*     malloc_copy        - Copy a string into malloc'ed area.
*
*     The system malloc (3)is declared here because of inconsistencies
*     accross platforms in the location of the malloc declare.
*
***************************************************************/

static char *malloc_ptr;  /* used in macro CE_MALLOC */

#if defined(FREEBSD) || defined(OMVS) || defined(__alpha)
#include <stdlib.h>    /* /usr/include/stdlib.h   RES  1/26/98 */
#else
#include <malloc.h>    /* /usr/include/malloc.h   RES  2/20/95 */
#endif

#define MEMORY_THRESHOLD 8192

#if defined(DebuG) && !defined(OpenNT)
#ifdef _MAIN_
char  *emalloc_format = "Mallocing %d bytes in %s at line %d, address: ";
#else
extern char *emalloc_format;
#endif
#define CE_MALLOC(size) (((debug & D_BIT21) ? fprintf(stderr, emalloc_format, size, __FILE__, __LINE__) : 0)\
           ,((malloc_ptr = (char *)malloc(size)) ? (((debug & D_BIT21) ? fprintf(stderr, "0x%X\n", malloc_ptr) : 0), malloc_ptr) : \
           malloc_error(size, __FILE__, __LINE__)))
#else
#define CE_MALLOC(size) ((malloc_ptr = malloc(size)) ?  malloc_ptr : malloc_error(size, __FILE__, __LINE__))
#endif

/*char *malloc(int size);  RES  2/20/95  */

/*void *free(char  *addr); RES  2/20/95 */

void *malloc_error(int size, char *file, int line);

void malloc_init(int size);

char *malloc_copy(char   *str);

#endif

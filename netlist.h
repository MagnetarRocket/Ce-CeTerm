#ifndef _NETLIST_INCLUDED
#define _NETLIST_INCLUDED    

/* static char *etlist_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

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
*  Routines in netlist.c
*         netlist             -  Get the list of internet addresses for this node
*         socketfd2hostinet   -  Get the internet ip address of a socket file descriptor
*
***************************************************************/

int netlist(unsigned long     *host_list,    /* output */
            int                max_host,     /* input  */
            unsigned long     *netmask) ;    /* output */

/**************************************************************
*  values for the second parm to socketfd2hostinet
***************************************************************/
#define DISPLAY_PORT_REMOTE   0
#define DISPLAY_PORT_LOCAL    1

unsigned int socketfd2hostinet(int socketfd, int local);

   
#endif

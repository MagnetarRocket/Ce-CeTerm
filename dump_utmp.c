
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

#include <stdio.h> 
#include <sys/types.h>
#include <sys/time.h> 
#include <utmp.h>
extern struct utmp *getutent();

static char  *trans_type(int type);

main(){
struct utmp *ut;

setutent();

while(ut = getutent()){
fprintf(stderr, "USER=%8s id=%12s line=%12s pid=%8d type=%12s, (%d), exit = %4d, termination = %4d\n", ut->ut_user, ut->ut_id, ut->ut_line, 
        ut->ut_pid, trans_type(ut->ut_type), ut->ut_type, ut->ut_exit.e_exit, ut->ut_exit.e_termination); 

}

endutent();

}

static char  *trans_type(int type)
{
static char    number[32];

switch(type)
{
case  EMPTY:
   return("EMPTY");
case  RUN_LVL:
   return("RUN_LVL");
case  BOOT_TIME:
   return("BOOT_TIME");
case  OLD_TIME:
   return("OLD_TIME");
case  NEW_TIME:
   return("NEW_TIME");
case  INIT_PROCESS:
   return("INIT_PROCESS");
case  LOGIN_PROCESS:
   return("LOGIN_PROCESS");
case  USER_PROCESS:
   return("USER_PROCESS");
case  DEAD_PROCESS:
   return("DEAD_PROCESS");
case  ACCOUNTING:
   return("ACCOUNTING");
#ifdef SHUTDOWN_TIME
case  SHUTDOWN_TIME:
   return("SHUTDOWN_TIME");
#endif
default:
   sprintf(number, "%d", type);
   return(number);
}

} /* end of trans_type */

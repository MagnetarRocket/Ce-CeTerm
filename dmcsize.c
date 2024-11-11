#include <stdio.h>
#include "dmc.h"

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

void main(int argc,
          char *argv[])
{

fprintf(stdout, "%2d    DMC_any   \n", sizeof(DMC_any   ));
fprintf(stdout, "%2d    DMC_find  \n", sizeof(DMC_find  ));
fprintf(stdout, "%2d    DMC_markp \n", sizeof(DMC_markp ));
fprintf(stdout, "%2d    DMC_markc \n", sizeof(DMC_markc ));
fprintf(stdout, "%2d    DMC_num   \n", sizeof(DMC_num   ));
fprintf(stdout, "%2d    DMC_bl    \n", sizeof(DMC_bl    ));
fprintf(stdout, "%2d    DMC_case  \n", sizeof(DMC_case  ));
fprintf(stdout, "%2d    DMC_ce    \n", sizeof(DMC_ce    ));
fprintf(stdout, "%2d    DMC_cmdf  \n", sizeof(DMC_cmdf  ));
fprintf(stdout, "%2d    DMC_cp    \n", sizeof(DMC_cp    ));
fprintf(stdout, "%2d    DMC_dq    \n", sizeof(DMC_dq    ));
fprintf(stdout, "%2d    DMC_echo  \n", sizeof(DMC_echo  ));
fprintf(stdout, "%2d    DMC_ei    \n", sizeof(DMC_ei    ));
fprintf(stdout, "%2d    DMC_env   \n", sizeof(DMC_env   ));
fprintf(stdout, "%2d    DMC_es    \n", sizeof(DMC_es    ));
fprintf(stdout, "%2d    DMC_icon  \n", sizeof(DMC_icon  ));
fprintf(stdout, "%2d    DMC_kd    \n", sizeof(DMC_kd    ));
fprintf(stdout, "%2d    DMC_msg   \n", sizeof(DMC_msg   ));
fprintf(stdout, "%2d    DMC_prompt\n", sizeof(DMC_prompt));
fprintf(stdout, "%2d    DMC_ph    \n", sizeof(DMC_ph    ));
fprintf(stdout, "%2d    DMC_pn    \n", sizeof(DMC_pn    ));
fprintf(stdout, "%2d    DMC_geo   \n", sizeof(DMC_geo   ));
fprintf(stdout, "%2d    DMC_pp    \n", sizeof(DMC_pp    ));
fprintf(stdout, "%2d    DMC_rw    \n", sizeof(DMC_rw    ));
fprintf(stdout, "%2d    DMC_s     \n", sizeof(DMC_s     ));
fprintf(stdout, "%2d    DMC_ts    \n", sizeof(DMC_ts    ));
fprintf(stdout, "%2d    DMC_twb   \n", sizeof(DMC_twb   ));
fprintf(stdout, "%2d    DMC_wc    \n", sizeof(DMC_wc    ));
fprintf(stdout, "%2d    DMC_xc    \n", sizeof(DMC_xc    ));
fprintf(stdout, "%2d    DMC       \n", sizeof(DMC       ));


} /* end of main */

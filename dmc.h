#ifndef _DMC_INCLUDED
#define _DMC_INCLUDED

/* static char *dmc_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

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
*  Routines in DM command structures
*
***************************************************************/


#define  MAXPTH  1024
#define  MAXSTR  256
#define  MAXPR   16
#define  ES_SHORT 16

#define  DM_OFF    -1
#define  DM_TOGGLE  0
#define  DM_ON      1
/* special extra value for vt command */
#define  DM_AUTO    2


struct DMC  ;
union _DMC ;


typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id DM_NULL  */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
  } DMC_any;                     /* used for cmds with no parms */

typedef DMC_any DMC_abrt;
typedef DMC_any DMC_ad;
typedef DMC_any DMC_au;
typedef DMC_any DMC_cms;
typedef DMC_any DMC_comma;
typedef DMC_any DMC_dollar;
typedef DMC_any DMC_dr;
typedef DMC_any DMC_ed;
typedef DMC_any DMC_en;
typedef DMC_any DMC_gm;
typedef DMC_any DMC_pb;
typedef DMC_any DMC_pt;
typedef DMC_any DMC_pw;
typedef DMC_any DMC_redo;
typedef DMC_any DMC_rm;
typedef DMC_any DMC_rs;
typedef DMC_any DMC_sq;
typedef DMC_any DMC_tb;
typedef DMC_any DMC_tdm;
typedef DMC_any DMC_th;
typedef DMC_any DMC_ti;
typedef DMC_any DMC_thl;
typedef DMC_any DMC_tl;
typedef DMC_any DMC_tmb;
typedef DMC_any DMC_tmw;
typedef DMC_any DMC_tn;
typedef DMC_any DMC_tr;
typedef DMC_any DMC_tt;
typedef DMC_any DMC_undo;
typedef DMC_any DMC_untab;
typedef DMC_any DMC_wg;
typedef DMC_any DMC_wge;
typedef DMC_any DMC_tdmo;
typedef DMC_any DMC_ind;
typedef DMC_any DMC_dlb;
typedef DMC_any DMC_sl;
typedef DMC_any DMC_rl;


typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id DM_NULL  */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        char       *buff;        /* regular expression  */
  } DMC_find;

typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id DM_NULL  */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        int         x;           /* X coordiate         */
        int         y;           /* Y coordiate         */
  } DMC_markp;

typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id DM_NULL  */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        int         row;       
        int         col;
        char        row_relative;
        char        col_relative;
        char        padding[2];
  } DMC_markc;
typedef DMC_markc DMC_corner;

typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id DM_NULL  */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        int         row;       
        int         relative;    /* negative: relative up, positive: relative down, zero: absolute */
  } DMC_num;

/*   NOT IMPLEMENTED
   DMC_aa;
   DMC_ap;
   DMC_as;
   DMC_cc;
   DMC_cdm;
   DMC_cpb;
   DMC_curs;
   DMC_dc;
   DMC_ds;
   DMC_ex;
   DMC_idf;
   DMC_kbd;
   DMC_lo;
   DMC_mono;
   DMC_shut;
   DMC_tlw;
   DMC_tni;
   DMC_wgra;
   DMC_wgrr;
   DMC_wme;
   DMC_xi;
*/

typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        char        dash_c;      /* nonzero if specified */
        char        dash_m;      /* nonzero if specified */
        char        dash_e;      /* nonzero if specified */
        char        spare;
        char       *cmdline;     /* path to command line to pass to system(2) */
        char       *dash_s;      /* options to pass to the shell */
  } DMC_bang;

typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        char       *l_ident;    /* left balance ident     */
        char       *r_ident;    /* lright balance ident   */
        char        dash_d;      /* nonzero if specified- defaults */
        char        padding[3];
  } DMC_bl;

typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        char        func;        /* s, u, or l */
        char        padding[3];
  } DMC_case;

typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        int         argc;        /* arg count */
        char      **argv;        /* command args */
  } DMC_ce;
typedef DMC_ce DMC_cv;
typedef DMC_ce DMC_cc;
typedef DMC_ce DMC_cp;

typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        int         dash_p;      /* nonzero if specified */
        char       *path;        /* path of command file */
  } DMC_cmdf;
typedef DMC_cmdf DMC_rec;

typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        int         argc;        /* arg count */
        char      **argv;        /* command args */
        char        dash_w;      /* nonzero if specified  ( cpo & cps only ) */
        char        dash_d;      /* nonzero if specified  */
        char        dash_s;      /* nonzero if specified  */
        char        padding;
  } DMC_cpo;
typedef DMC_cpo DMC_cps;


typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        char       *name;        /* name of process to signal */
        int         dash_c;      /* nonzero if specified, kill number is value */
        char        dash_s;      /* nonzero if specified */
        char        dash_b;      /* nonzero if specified */
        char        dash_i;      /* nonzero if specified */
        char        dash_a;      /* nonzero if specified, char to send is value */
  } DMC_dq;


typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        int         dash_r;      /* nonzero if specified */
  } DMC_echo;

typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        char        dash_a;      /* nonzero if specified, eof value to transmit is value */
        char        padding[3];
  } DMC_eef;


typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        int         mode;        /* DM_OFF, DM_TOGGLE, DM_ON   */
  } DMC_ei;
typedef DMC_ei DMC_envar;
typedef DMC_ei DMC_inv;
typedef DMC_ei DMC_mouse;
typedef DMC_ei DMC_ro;
typedef DMC_ei DMC_sb;
typedef DMC_ei DMC_sc;
typedef DMC_ei DMC_wa;
typedef DMC_ei DMC_wh;
typedef DMC_ei DMC_wi;
typedef DMC_ei DMC_ws;
typedef DMC_ei DMC_lineno;
typedef DMC_ei DMC_hex;
typedef DMC_ei DMC_vt100;
typedef DMC_ei DMC_wp;
typedef DMC_ei DMC_pdm;
typedef DMC_ei DMC_caps;

typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        char       *string;      /* string to enter  */
        char        chars[ES_SHORT]; /* raw char or short string */
  } DMC_es;
typedef DMC_es   DMC_er;

typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        char       *parm;        /* path to copy to */
  } DMC_geo;
typedef DMC_geo DMC_debug;
typedef DMC_geo DMC_crypt;
typedef DMC_geo DMC_fl;
typedef DMC_geo DMC_re;
typedef DMC_geo DMC_cd;
typedef DMC_geo DMC_env;
typedef DMC_geo DMC_bgc;
typedef DMC_geo DMC_fgc;
typedef DMC_geo DMC_title;
typedef DMC_geo DMC_pd;
typedef DMC_geo DMC_eval;
typedef DMC_geo DMC_lbl;
typedef DMC_geo DMC_glb;

typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        int         dash_i;      /* nonzero if specified  */
        int         dash_w;      /* nonzero if specified  */
  } DMC_icon;
typedef DMC_icon DMC_al;
typedef DMC_icon DMC_ar;
typedef DMC_icon DMC_sic;
typedef DMC_icon DMC_ee;

typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        char       *key;         /* key to define       */
        char       *def;         /* dm command string if non-null */
        int         line_no;     /* line number from command file */
  } DMC_kd;
typedef DMC_kd DMC_ld;
typedef DMC_kd DMC_alias;
typedef DMC_kd DMC_mi;
typedef DMC_kd DMC_lsf;


typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        char       *msg;         /* value to set if non-null */
  } DMC_msg;
typedef DMC_msg DMC_sp;
typedef DMC_msg DMC_prefix;

typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        int         chars;       /* # chars to scroll (+ scrolls pad left, data disappears off left side of screen  */
  } DMC_ph;
typedef DMC_ph DMC_pv;
typedef DMC_ph DMC_bell;
typedef DMC_ph DMC_fbdr;

typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        int         dash_c;      /* nonzero if specified  */
        int         dash_r;      /* nonzero if specified  */
        char       *path;        /* path to copy to */
  } DMC_pn;

typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        float       amount;      /* amout of scroll   */
  } DMC_pp;
typedef DMC_pp DMC_sleep;

typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        char       *prompt_str;  /* DM prompt string, extracted from orginial prompt (part in quotes)  */
        char       *response;    /* response is put here when prompt is fulfilled. */
        char       *target;      /* target string insert pos, points into containing command (target_strt) */
        char       *target_strt; /* target string start, points to containing command */
        union _DMC *delayed_cmds;/* cmds waiting for prompt to finish, used only in cmdf */
        char        mult_prmt;   /* flag set to True in second and beyond prompts when more than one prompt on a line */
        char        quote_char;  /* Set to prompt quote char.  Blanks retained in read in string if grave quote */
        char        padding[2];
  } DMC_prompt;

typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        int         dash_r;      /* nonzero if specified  */
  } DMC_rw;

typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        char       *s1;          /* from string */
        char       *s2;          /* to string */
  } DMC_s;
typedef DMC_s DMC_so;

typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        short int   dash_r;      /* non-zero if specified, - right margin  */
        short int   dash_l;      /* non-zero if specified, - left margin, default is zero */
        char        dash_b;      /* non-zero if specified, - boolean flag, right and left justify  */
        char        dash_c;      /* non-zero if specified, - boolean flag, center each line  */
        char        dash_p;      /* non-zero if specified, - boolean flag, squeeze out blanks before processing  */
        char        padding;
  } DMC_tf;


typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        int         dash_r;      /* non-zero if specified, - is repeat inverval  */
        int         count;       /* Count of tab stops  */
        short      *stops;       /* tab stops */
  } DMC_ts;


typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        char        border;      /* char l, r, t, or b */
        char        padding[3];
  } DMC_twb;


typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        char        type;        /* char q, f, a, or s (y or n also) */
        char        padding[2];
  } DMC_wc;
typedef DMC_wc DMC_reload;       /* type for reload is f, y, or n */


typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        int         number;      /* wdf number (1 - 12) */
        char       *geometry;    /* associated geometry string */
  } DMC_wdf;


typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        int         number;      /* wdc number (1 - 12), used in wdc, not it ca */
        char       *bgc;         /* associated background color */
        char       *fgc;         /* associated background color */
  } DMC_wdc;
typedef DMC_wdc DMC_ca;
typedef DMC_wdc DMC_nc;


typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        int         dash_c;      /* width from -c if specified  (leave as int, sccanf in parsedm cannot handle shorts */
        short int   mode;        /* DM_OFF, DM_TOGGLE, DM_ON    */
        char        dash_i;      /* non-zero if specified       */
        char        dash_a;      /* non-zero if specified       */
  } DMC_ww;


typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        char        dash_f1;     /* non-zero if specified, -f before first pastebuf name  */
        char        dash_f2;     /* non-zero if specified, -f before second pastebuf name */
        char        dash_l;      /* non-zero if specified, -l             */
        char        padding;
        char       *path1;       /* path to paste buffer, or buffer name  */
        char       *path2;       /* path to paste buffer, or buffer name  */
  } DMC_xa;


typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        char        dash_r;      /* non-zero if specified  */
        char        dash_f;      /* non-zero if specified  */
        char        dash_a;      /* non-zero if specified  */
        char        dash_l;      /* non-zero if specified  */
        char       *path;        /* path to paste buffer, or buffer name  */
  } DMC_xc;
typedef DMC_xc DMC_xd;
typedef DMC_xc DMC_xp;

typedef struct {
        DMC_xc      base;        /* base is an xc command       */
        char       *data;        /* data to put in paste buffer */
  } DMC_xl;

typedef struct {
        struct DMC *next;        /* next cmd if chained */
        short int   cmd;         /* command id          */
        short int   temp;        /* true if delete after use (cmd line cmds)  */
        char        dash_r;      /* non-zero if specified  */
        char        dash_f;      /* non-zero if specified  */
        char        dash_a;      /* non-zero if specified  */
        char        dash_l;      /* non-zero if specified  */
        char        dash_h;      /* nonzero if specified, char to send as dq is value */
        char        spare[3];
        char       *path;        /* path to paste buffer, or buffer name  */
  } DMC_cntlc;

/*
 * this union is defined so DM can always use the same sized
 * command structure internally, to avoid memory fragmentation.
 */


typedef union _DMC {
   union
   _DMC      *next;
   DMC_alias  alias;
   DMC_any    any;
   DMC_abrt   abrt;
   DMC_ad     ad;
   DMC_al     al;
   DMC_ar     ar;
   DMC_au     au;
   DMC_bang   bang;
   DMC_bell   bell;
   DMC_bl     bl;
   DMC_bgc    bgc;
   DMC_ca     ca;
   DMC_caps   caps;
   DMC_case   d_case; /* c keyword */
   DMC_ce     ce;
   DMC_cc     cc;
   DMC_cd     cd;
   DMC_cmdf   cmdf;
   DMC_cms    cms;
   DMC_cntlc  cntlc;
   DMC_comma  comma;
   DMC_corner corner;
   DMC_cp     cp;
   DMC_cpo    cpo;
   DMC_cps    cps;
   DMC_crypt  crypt;
   DMC_cv     cv;
   DMC_debug  debug;
   DMC_dlb    dlb;
   DMC_dollar dollar;
   DMC_dq     dq;
   DMC_dr     dr;
   DMC_echo   echo;
   DMC_ed     ed;
   DMC_ee     ee;
   DMC_eef    eef;
   DMC_ei     ei;
   DMC_en     en;
   DMC_env    env;
   DMC_envar  envar;
   DMC_er     er;
   DMC_es     es;
   DMC_eval   eval;
   DMC_fbdr   fbdr;
   DMC_find   find;
   DMC_fgc    fgc;
   DMC_fl     fl;
   DMC_geo    geo;
   DMC_glb    glb;
   DMC_gm     gm;
   DMC_hex    hex;
   DMC_icon   icon;
   DMC_ind    ind;
   DMC_inv    inv;
   DMC_kd     kd;
   DMC_ld     ld;
   DMC_lbl    lbl;
   DMC_lineno lineno;
   DMC_lsf    lsf;
   DMC_markc  markc;
   DMC_markp  markp;
   DMC_mi     mi;
   DMC_mouse  mouse;
   DMC_msg    msg;
   DMC_nc     nc;
   DMC_num    num;
   DMC_pb     pb;
   DMC_pd     pd;
   DMC_pdm    pdm;
   DMC_ph     ph;
   DMC_pn     pn;
   DMC_pp     pp;
   DMC_prefix prefix;
   DMC_prompt prompt;
   DMC_pt     pt;
   DMC_pv     pv;
   DMC_pw     pw;
   DMC_re     re;
   DMC_rec    rec;
   DMC_reload reload;
   DMC_rm     rm;
   DMC_ro     ro;
   DMC_rs     rs;
   DMC_s      s;
   DMC_sb     sb;
   DMC_sc     sc;
   DMC_sic    sic;
   DMC_sleep  sleep;
   DMC_so     so;
   DMC_sp     sp;
   DMC_sq     sq;
   DMC_rw     rw;
   DMC_tb     tb;
   DMC_tdm    tdm;
   DMC_tdmo   tdmo;
   DMC_tf     tf;
   DMC_th     th;
   DMC_ti     ti;
   DMC_thl    thl;
   DMC_tl     tl;
   DMC_title  title;
   DMC_tmb    tmb;
   DMC_tmw    tmw;
   DMC_tn     tn;
   DMC_tr     tr;
   DMC_ts     ts;
   DMC_tt     tt;
   DMC_twb    twb;
   DMC_undo   undo;
   DMC_untab  untab;
   DMC_vt100  vt100;
   DMC_wa     wa;
   DMC_wc     wc;
   DMC_wdc    wdc;
   DMC_wdf    wdf;
   DMC_wh     wh;
   DMC_wi     wi;
   DMC_wg     wg;
   DMC_ws     ws;
   DMC_wge    wge;
   DMC_wp     wp;
   DMC_ww     ww;
   DMC_xa     xa;
   DMC_xc     xc;
   DMC_xd     xd;
   DMC_xl     xl;
   DMC_xp     xp;
} DMC;

#endif



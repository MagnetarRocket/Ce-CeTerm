/*
 * main program decleration
 */

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <errno.h>          /* /usr/include/errno.h      */
#include <string.h>         /* /bsd4.3/usr/include/string.h     */
#include <limits.h>         /* /bsd4.3/usr/include/limits.h     */
#include <signal.h>         /* "/usr/include/signal.h"        */

#include <X11/Xlib.h>       /* /usr/include/X11/Xlib.h   */
#include <X11/keysym.h>     /* /usr/include/X11/keysym.h  /usr/include/X11/keysymdef.h */
#include <X11/cursorfont.h> /* /usr/include/X11/cursorfont.h */
#include <X11/Xatom.h>      /* /usr/include/X11/Xatom.h  */

#define MAX_KEY_NAME_LEN    16
#define MAX_WINDOW_NAME_LEN 80

#define _MAIN_ 1

#include "bl.h"
#include "borders.h"
#include "cc.h"
#include "color.h"
#include "debug.h"
#include "dmc.h"
#include "dmfind.h"
#include "dmwin.h"
#include "dumpxevent.h"
#include "execute.h"
#include "expidate.h"
#include "pw.h"
#include "gc.h"
#include "getevent.h"
#include "getxopts.h"
#include "help.h"
#include "init.h"
#include "kd.h"
#include "lineno.h"
#include "emalloc.h"
#include "mark.h"
#include "memdata.h"
#include "mvcursor.h"
#include "parms.h"
#include "parsedm.h"
#include "pastebuf.h"
#include "prompt.h"
#include "tab.h"
#include "serverdef.h"
#include "search.h"
#include "textflow.h"
#include "titlebar.h"
#include "txcursor.h"
#include "typing.h"
#include "undo.h"
#include "wc.h"
#include "ww.h"
#include "window.h"
#include "windowdefs.h"
#include "xc.h"
#include "xerror.h"
#include "xutil.h"
#ifdef PAD
#include "unixpad.h"
#include "unixwin.h"
#endif

int flag = 0;    /*  0=overwrite, 1=insertafter */

int line = 0;
int coulumn = 0;
int newlines = 0;

extern char *optarg;
extern int optind, opterr, errno;

int eof = 0;
int column;

int markfl =  0,       markfc =  -1;
int marktl =  INT_MAX-1, marktc =  INT_MAX-1;
int case_s = 1, direction = 0, rectangular = 0, found_line, found_column;
int rec_start_col;

char *sub;
char substit[256];
char pattern[256];
char command[256];
char trash[256];
char new_file[256];
char file[256];

int intitialized=0;

FILE *fp = NULL;
DATA_TOKEN *token;

main(argc, argv)
int   argc;
char *argv[];
{

Debug();
cmdname =  "MD: ";
escape_char = '\\';


if (argc > 1)
   memdata_test(argv[1]);
else
   memdata_test(NULL);

argv[1][0] = NULL;

}  /* main */

memdata_test(char *init_cmd){

int event;
int c, rc;

char *ofile, *ifile, *bfile, *cmd;

char text[256];

char text1[256];
char text2[256];
char text3[256];
char text4[256];
char text5[256];
char text6[256];
char text7[256];
char text8[256];
char text9[256];

if (!intitialized) 
       token= mem_init(68);

intitialized = 1;

while(1){
    
    if (!fp) fp = stdin;
    
    if (fp == stdin) fprintf(stderr,"\n*****Command: ");

    if (init_cmd){
         sprintf(command, "run %s", init_cmd);
         *init_cmd = NULL;
         goto q;
    }

    if (!fgets(command, 256, fp)){ 
       fprintf(stderr, " EOF\n");
       return 0;
    }

q:  if (fp != stdin) fprintf(stderr, "\n#####COMMAND: %s\n\n", command);
    cmd = command;fflush(stderr);

    while (*cmd == ' ') cmd++;

    if (!strncmp(cmd, "load", 4)){
             sscanf(cmd,"%s %s", trash, file); 
             input_file(file);
             continue;
    }

    if (!strncmp(cmd, "run", 3)){
             sscanf(cmd,"%s %s", trash, file); 
             fp = fopen(file, "r");
             if (!fp){ 
                  sprintf(new_file,"./mtest/%s", file);
                  fp = fopen(new_file, "r");
                  if (!fp){ 
                      fprintf(stderr, "File not found: %s\n",file);
                      continue;
                  } 
             } 
             memdata_test(NULL);   /* yes, recursive main */
             init_cmd = NULL;
             fp = stdin;
             continue;
    }
      
    if (!strncmp(cmd, "save_file", 4)){
             rc = sscanf(cmd,"%s %s", trash, file); 
             if (rc == 2) output_file(file);
             else output_file("stderr");
             continue;
    }

    if (!strncmp(cmd, "get_line", 3)){
             sscanf(cmd,"%s %d", trash, &line); 
             fprintf(stderr, "GLBN: '%s'\n", get_line_by_num(token, line));
             continue;
    }
    
    if (!strncmp(cmd, "put_block", 9)){
             sscanf(cmd,"%s %d %d %s", trash, &line, &column, file); 
             put_block(file);
             continue;
    }

    if (!strncmp(cmd, "put_line", 3)){
             rc = sscanf(cmd,"%s %d %s %d", trash, &line, text, &flag); 
             if (rc < 4){
                flag = atoi(text);
                text[0] = NULL;       /* NULL test */
             }
             put_line(text);
             continue;
    }
    
    if (!strncmp(cmd, "delete_line", 3)){
             column=0;
             sscanf(cmd,"%s %d", trash, &line, &column); 
    	  	  delete_line_by_num(token, line, column);
             continue;
    }
    
    if (!strncmp(cmd, "position", 3)){
             sscanf(cmd,"%s %d", trash, &line); 
    	  	  position_file_pointer(token, line);
             continue;
    }
    
    if (!strncmp(cmd, "split", 5)){
             sscanf(cmd,"%s %d %d %d", trash, &line, &column, &flag); 
    	  	  split_line(token, line, column, flag);
             continue;
    }
    
    if (!strncmp(cmd, "sum", 3)){
             sscanf(cmd,"%s %d %d ", trash, &line, &column); 
    	  	  rc = sum_size(token, line, column);
             fprintf(stderr, "The sum is: %d\n", rc);
             continue;
    }
    
    if (!strncmp(cmd, "next_line", 4)){
    	  	  next_l();
             continue;
    }
    
    if (!strncmp(cmd, "prev", 4)){
    	  	  prev_l();
             continue;
    }

    if (!strncmp(cmd, "undo", 4)){
    	  	  un_do(token, &line);
             continue;
    }

    if (!strncmp(cmd, "redo", 4)){
    	  	  re_do(token, &line);
             continue;
    }

    if (!strncmp(cmd, "list_events", 4)){
    	  	  list_events(token);
             continue;
    }

    if (!strncmp(cmd, "event", 4)){
             column = 0;flag = 0; *file=NULL;line=0;
             sscanf(cmd,"%s %d", trash, &event, &line, file, &column, &flag); 
             event_do(token, event, line, column, flag, file);
             continue;
    }

    if (!strncmp(cmd, "quit", 1)){
    	  	  exit(0);
    }

    if (!strncmp(cmd, "debug", 5)){
             sscanf(cmd,"%s %s", trash, file); 
             debug=0;
             sprintf(text,"DEBUG=@%s", file);
             putenv(text);
             Debug();
             continue;
    }

    if (!strncmp(cmd, "find", 3)){
             sub = substit;
             rc = sscanf(cmd,"%s %s %s", trash, pattern, substit); 
             if (rc == 2) sub = NULL;
             found_line = 1;
             rec_start_col = markfc;
             while (found_line != DM_FIND_NOT_FOUND){
    	  	     search(token, markfl, markfc, marktl, marktc, direction, pattern, sub, case_s, &found_line, &found_column, &newlines, rectangular, rec_start_col);
                if (!direction){
                     markfl = found_line; markfc = found_column;
                }else{
                     marktl = found_line; marktc = found_column;
                }
                if (found_line != DM_FIND_NOT_FOUND)
                    fprintf(stderr, "Pattern found in line %d, in column %d\n", found_line, found_column);
             }                      
             if (newlines) process_inserted_newlines(token, 0, total_lines(token), &found_line, &found_column);
             markfl =  0;       markfc =  -1;
             marktl =  INT_MAX-1; marktc =  INT_MAX-1;
             newlines = 0;
             continue;
    }

    if (!strncmp(cmd, "marks", 4)){
             sscanf(cmd,"%s %d %d %d %d", trash, &markfl, &markfc, &marktl, &marktc); 
             fprintf(stderr, "Marks set at (%d,%d),(%d,%d)\n",markfl,markfc,marktl,marktc);
             continue;
    }

    if (!strncmp(cmd, "direction", 3)){
             TOGLE(direction);
             fprintf(stderr, "Direction is: %s\n",direction ? "Reverse" : "Forward");
             continue;
    }

    if (!strncmp(cmd, "rectangular", 3)){
             TOGLE(rectangular);
             fprintf(stderr, "Rectangular Cut/Paste is: %s\n",rectangular ? "ON" : "OFF");
             continue;
    }


    if (!strncmp(cmd, "case", 4)){
             TOGLE(case_s);
             fprintf(stderr, "Case is: %s\n", case_s ? "Insensitive" : "Sensitive");
             continue;
    }

    if (!strncmp(cmd, "kill", 4)){
             mem_kill(token);
             fprintf(stderr, "Toke(%d) is dead!\n", token);
             token = NULL;
             continue;
    }

    if (!strncmp(cmd, "pad", 3)){
             pad_init("/bin/ksh");
             continue;
    }

    if (!strncmp(cmd, "2shell", 3)){
             trash[0] =  text1[0] = text2[0] = text3[0] = text4[0] = text5[0] = NULL;
             text6[0] = text7[0] = text8[0] = text9[0] = NULL; 
             sscanf(cmd,"%s %s %s %s %s %s %s %s %s %s", trash, text1, text2, text3, text4, text5, text6, text7, text8, text9); 
             sprintf(text, "%s %s %s %s %s %s %s %s %s", text1, text2, text3, text4, text5, text6, text7, text8, text9); 
             fprintf(stderr,"Shelling: '%s'\n", text);
             rc = pad2shell(text);
             continue;
    }

    if (!strncmp(cmd, "init", 4)){
             if (token){
                fprintf(stderr, "Token is still alive!\n");
                continue;
             }
             token = mem_init(68);
             eof = 0;
             fprintf(stderr, "Token(%d) is alive!\n", token);
             continue;
    }


    if (!strncmp(cmd, "a_to_re", 4)){
             if (escape_char == '@')
                 escape_char = '\\';
             else
                 escape_char = '@';
             fprintf(stderr, "Escape character is: '%c'\n", escape_char);
             continue;
    }
     
    fprintf(stderr, "\nValid (memdata/undo) commands are:\n\n");
    fprintf(stderr, "\t\tsave <file>\n");
    fprintf(stderr, "\t\tget_line <line>\n");
    fprintf(stderr, "\t\tput_line <line> <text> <flag>\n");
    fprintf(stderr, "\t\tput_block <line> <column> <file>\n");
    fprintf(stderr, "\t\tsplit <line> <column> <flag>\n");
    fprintf(stderr, "\t\tdelete_line <line>\n");
    fprintf(stderr, "\t\tposition <line>\n");
    fprintf(stderr, "\t\tnext\n");
    fprintf(stderr, "\t\tprev\n");
    fprintf(stderr, "\t\tsum\n");

    fprintf(stderr, "\n");
    fprintf(stderr, "\t\tundo\n");
    fprintf(stderr, "\t\tredo\n");
    fprintf(stderr, "\t\tlist_events\n");
    fprintf(stderr, "\t\tevent <PL, DL, RB, KS, SL> <line> <data> <column> <flag>\n");

    fprintf(stderr, "\n");
    fprintf(stderr, "\t\tpad\n");
    fprintf(stderr, "\t\t2shell\n");

    fprintf(stderr, "\n");
    fprintf(stderr, "\t\tdirection\n");
    fprintf(stderr, "\t\trectangular\n");
    fprintf(stderr, "\t\ta_to_re\n");
    fprintf(stderr, "\t\tfind <pattern> <substitution>\n");
    fprintf(stderr, "\t\tmarks <from_line> <from_col> <to_line> <to_col>\n");

    fprintf(stderr, "\n");
    fprintf(stderr, "\t\trun <file>\n");
    fprintf(stderr, "\t\tload <file>\n");
    fprintf(stderr, "\t\tdebug\n");
    fprintf(stderr, "\t\tkill\n");
    fprintf(stderr, "\t\tinit\n");
    fprintf(stderr, "\t\tquit\n");
 
} /* while (1) */

}  /* memdata_test() */

/*********************************************************************/

input_file(char *file)
{
FILE *fp;

fp = fopen(file, "r");

if (!fp){ 
     sprintf(new_file,"./mtest/%s", file);
     fp = fopen(new_file, "r");
     if (!fp){ 
         fprintf(stderr, "File not found: %s\n",file);
         return(1);
     } 
} 

if (!fp){ 
   fprintf(stderr, "File not found: %s\n",file);
} 

while (!eof)
   load_a_block(token, fp, &eof);

fprintf(stderr, "File Loaded!\n");

}

/*********************************************************************/

output_file(char *file)
{
FILE *fp;

if (!strcmp(file , "stderr")){
   fprintf(stderr, "Placing output on STDERR.\n");
   save_file(token, stderr);
}else{
   fp = fopen(file, "w");
   if (!fp){
      fprintf(stderr," Could not open %s (%d)", file, errno);
      exit(1);
   }
   save_file(token, fp);
   fprintf(fp,"\n");
   fclose(fp);
}
}

/*********************************************************************/

put_line(char *text)
{

put_line_by_num(token, line, text, flag);

}

/*********************************************************************/

put_block(char *file)
{
FILE *fp;
char buf[256];
int count;

fp = fopen(file, "r");
fgets(buf, sizeof(buf), fp);
sprintf(buf, "%d", &count); 

put_block_by_num(token, line, count, column, fp);


}

/*********************************************************************/

/* dm_error(char *string, int trash)
{

fprintf(stderr,"%s %s\n", cmdname, string);

}   */

/*********************************************************************/

next_l()
{

fprintf(stderr,"Next_Line is:%s\n\n", next_line(token));

}

/*********************************************************************/

prev_l()
{

fprintf(stderr,"Prev_Line is:%s\n\n", prev_line(token));

} 

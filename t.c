
#include <stdio.h>   /* /usr/include/stdio.h  */
#include <errno.h>   /* /usr/include/errno.h  */
#include <string.h>  /* /usr/include/string.h */
/*#include <sys/types.h>       /* /bsd4.3/usr/include/sys/types.h    */
/*#include <sys/stat.h>       /* /bsd4.3/usr/include/sys/stat.h    */
/*#include <netinet/in.h>      /* /bsd4.3/usr/include/netinet/in.h */

/*#include <X11/Xlib.h>*/
#define blanks "             "

#include "ceapi.h"

static void start_ce(char *file, char *file_kcc);

main (int argc, char *argv[])
{
int        rc;
int        make = 0;
int        errs = 0;
int        session_started = 0;
int        i;
int        ln;
int        gotten = 0;

char      *ptr;
char      *file;

char       cmd[4096];
char       line[256];
char       line2[256];
char       file_kcc[256];
char       cecmd[512];

CeStats   *ce_stats;
FILE      *fp;

*cmd = NULL;

#ifdef SIGCLD           /* apollo has CLD and CHLD backwrds */
signal(SIGCLD, sigdc);                               
#endif
#ifdef SIGCHLD
signal(SIGCHLD, sigdc);                              
#endif

for (i=1; i < argc; i++){
    strcat(cmd, argv[i]);
    strcat(cmd, " ");
}

file = argv[argc];
strcpy(file_kcc, file);
strcat(file_kcc, ".kcc 2>&1");

again:  fp = popen(cmd);
while (gotten ? 1 : (int)fgets(line, sizeof(line), fp)){
    gotten = 0;
    fprintf(stderr, "%s", line);
    ptr = NULL;
    if ((ptr = strstr(", line ", line)) && (*(ptr + 7) <= '9') &&  (*(ptr + 7) >= '0'))
         errs++;
    if ((ptr = strstr("******** Line ", line)) && (*(ptr + 14) <= '9') &&  (*(ptr + 14) >= '0')){
         errs++
         fgets(line2, sizeof(line2), fp);
         fprintf(stderr, "%s", line2);
         if (strncmp(line, "         ", 9)) /* continuation */
             gotten = 1;
         else
             strcat(line, line2 + 8);
    }
    if (errs && !session_started++){
         start_ce(file, file_kcc);
         if (!ceApiInit()){ fprintf(stderr, "%s: ceApiInit failed \n", argv[0]); unlink(file_kcc); exit(1);}
    }

    if (ptr){
        sscanf(i + 5, "%d", &ln);
        rc = ceXdmc(cecmd);
    
        if (rc != ){
          fprintf(stderr, "Could not xdmc insert message\n");
          exit(rc);
        }
    } /* if err found (ptr) */
    if (gotten)
        strcpy(line, line2);

} /* while more errors */

pause();

goto again;

if (session_started)
      unlink(file_kcc);

} /* end of main */

/**************************************************************************/

static void start_ce(char *file, char *file_kcc)
{

if (!fork()) /* child */
    execlp("ce", file_kcc);
}

/**************************************************************************/
sigdc(){}    /* yes, this is a function */





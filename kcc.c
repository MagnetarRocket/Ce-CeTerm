/*
 *   This program is a compiler front end that does a merge of error messages
 *   and allows the user to correct his program and resubmit for compilation.
 *
 *    This program is meant to:
 *       A) Demonstrate some of the Ce API interfaces
 *       B) Demonstrate how to communicate witha  ce session in terms of signals
 *       C) Be useful!
 */

#include <stdio.h> 
#include <errno.h> 
#include <string.h> 
#include <signal.h> 

#include "ceapi.h"

/*
 *  Macros
 */

#define itoa(v, s) ((sprintf(s, "%d", (v))), (s))

/*
 *  Prototypes
 */

static int start_ce(char *file, char *file_kcc);
static void await(int sig, int pid);
static void sigdc();

/*
 *  Global data
 */

char cecmd[512];
static int debug = 0;

main (int argc, char *argv[]){

int        rc;
int        make = 0;
int        errs = 0;
int        session_started = 0;
int        i;
int        ln;
int        status
int        cepid;
int        gotten = 0;
int        offset = 0;

char      *ptr;
char      *file;

char       cmd[4096];
char       line[256];
char       line2[256];
char       file_kcc[256];

CeStats   *ce_stats;
FILE      *fp;

*cmd = NULL;

for (i=1; i<33; i++)
  signal(i, sigdc);

signal(SIGCLD, sigdc);                               
signal(SIGCHLD, sigdc);                               

file = argv[argc -1];
strcpy(file_kcc, file);
strcat(file_kcc, ".kcc");

sprintf(cmd, "/bin/cp %s %s; ", file, file_kcc);

for (i=1; i < argc; i++){
    strcat(cmd, argv[i]);
    strcat(cmd, " ");
}
strcat(cmd, "2>&1");

if (debug) fprintf(stderr, "%s", cmd); 

again: fp = popen(cmd, "r");
while (gotten ? 1 : (int)fgets(line, sizeof(line), fp)){
    gotten = 0;
    fprintf(stderr, "%s", line);
    if ((ptr = strstr(line, ", line ")) && (*(ptr + 7) <= '9') &&  (*(ptr + 7) >= '0')){
         sscanf(ptr + 14, "%d", &ln);
         errs++;
    }
    if ((ptr = strstr(line, "******** Line ")) && (*(ptr + 14) <= '9') &&  (*(ptr + 14) >= '0')){
         errs++;
         sscanf(ptr + 14, "%d", &ln);
         fgets(line2, sizeof(line2), fp);
         if (strncmp(line, "         ", 9)) /* continuation */
             gotten = 1;
         else
             strcat(line, line2 + 8);
    }
    if (errs && !session_started++){
         cepid = start_ce(file, file_kcc);
         await(SIGCHLD, cepid);
         if (ceApiInit(cepid)){ fprintf(stderr, "%s: ceApiInit failed \n", argv[0]); unlink(file_kcc); exit(1);}
    }

    if (ptr){
        offset++;
        line[strlen(line) - 1] = NULL; /* kill newline */
        sprintf(cecmd,"%d;es '%s   <Kcc>!';en", ln + offset, line);
        if (debug)  fprintf(stderr, "Executing cmd:%s\n", cecmd);
        rc = ceXdmc(cecmd);
    
        if (rc != 0){
          fprintf(stderr, "Could not xdmc insert message\n");
          exit(rc);
        }
    } /* if err found (ptr) */
    if (gotten)
        strcpy(line, line2);

} /* while more errors */

await(SIGCLD, cepid);

offset = gotten = session_started = 0;
sprintf(cecmd,"/bin/sed '/<Kcc>!/d' %s > %s", file_kcc, file);
if (debug) fprintf(stderr, cecmd);
system(cecmd);
goto again;

if (session_started)
      unlink(file_kcc);

} /* end of main */

/**************************************************************************/

static int start_ce(char *file, char *file_kcc){

int pid;

pid = fork();

if (!pid){ 
    sprintf(cecmd, "dq -c %d %d", SIGCHLD, pid);
    execlp("ce", "kcc.ce", "-w", "-cmd", cecmd, file_kcc, 0);
}

return(pid);

} /* start_ce() */

/**************************************************************************/

static void sigdc(int sig){
fprintf(stderr, "Signal(%d) Recieved!\n", sig);
}   

/**************************************************************************/
static void await(int sig, int pid){

int wpid = 0, status = 0;

while(){ 
   fprintf(stderr, "Signal(%d) came from process: \n", wpid);
   wpid = wait(&status);
}   
} /* await() */  







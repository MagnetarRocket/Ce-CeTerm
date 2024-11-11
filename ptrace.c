/*
 * main program decleration
 */

#include <stdio.h>
#include <limits.h>
#include <signal.h>
#include <ptrace.h>    /* /bsd4.3/usr/include/ptrace.h */
#include <sys/wait.h>
#include <sys/user.h>  /* /bsd4.3/usr/include/sys/user.h */

main(argc, argv)
	int             argc;
	char           *argv[];
{

int rc, pid, sig;
int status[2];

struct user *u_proc;

struct ptrace_set pt_options;


pt_options.events = (1<<PTRACE_SIGNAL)|(1<<PTRACE_FORK)|(1<<PTRACE_EXEC)|(1<<PTRACE_EXIT)|(1<<PTRACE_LOAD)|(1<<PTRACE_STEP_OUTSIDE)|(1<<PTRACE_STEP_INSIDE);
pt_options.signals = 0;
pt_options.inherit = 0;
pt_options.range_start = 0;
pt_options.range_end = 0;

pid =  fork();

if (pid == -1){
   perror("For Failed");
   exit(1);
}

if (pid == 0){  /*   CHILD */

    fprintf(stderr, "In Child\n");

    fprintf(stderr, "0\n");
    rc = ptrace(PTRACE_TRACEME, pid, 0, 0, 0, 0); 
    perror("PT_TRACEME");
    fprintf(stderr, "1\n");

    sleep(10);
    fprintf(stderr, "2\n");
    sleep(10);
    fprintf(stderr, "3\n");
    sleep(10);
    fprintf(stderr, "4\n");
    sleep(10);
    fprintf(stderr, "5\n");
    sleep(10);
    fprintf(stderr, "6\n");
    sleep(10);
    fprintf(stderr, "7\n");

    execl("/tmp/child", "child", (char *)0);
    execl("/bin/sh", "pad0", (char *)0);
    fprintf(stderr, "Execl failed!\n");
    exit(1);

}else{          /* PARENT */

    fprintf(stderr, "In Parent\n");
    rc = ptrace(PTRACE_SET_OPTIONS, pid, 0, 0, &pt_options, 0); 

    while (1){
        rc = wait(status);
        fprintf(stderr, "Child is suspended(x0%X, x0%X)[%d]\n", status, rc);

        fprintf(stderr, "Resuming Child");
        rc = ptrace(PTRACE_CONT, pid, 1, sig, 0, 0);
        fprintf(stderr, "[%d]\n", rc);
        perror("PT_CONT");
       
        sleep(10);
        kill(pid, SIGSTOP);                               
    }
} /* if (pid) */


}

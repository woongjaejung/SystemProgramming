/* header */
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<errno.h>
#include<wait.h>
#include<signal.h>
/* header END */

/* MACROS */
#define MAXARGS		32      //  max number of arguments
#define MAXLINE     512     //  max cmd input
#define READ        0       //  file descripter
#define WRITE       1       //  file descripter
#define MAXBUF      2048    //  max buffer
#define MAXJOBS     32      //  max jobs
#define ST          0       //  job status: suspended
#define BG          1       //  job status: running on background
#define FG          2       //  job status: running on foreground
#define TERM        3       //  job status: terminated
#define REAPED      -1      //  job pid of reaped
#define FALSE       0
#define TRUE        1
/* MACROS END */

/* function prototypes */
// re-used functions
void eval(char* cmdline);
void execute(char* cmdline);
int builtin_command(char **argv);
void parseline(char *buf, char **argv);
int parsecmdbypipe(char *buf, char *cmd[MAXLINE], int* cmdCnt);
/* function prototypes END */

// command to implement
void cmd_fg(char** argv);
void cmd_bg(char** argv);
void cmd_kill(char** argv);

// functions for maintenance job list */
void listjobs();
void addjob(pid_t _pid, char _name[]);
void deletejob(pid_t pid);
int getJobidx(pid_t pid);
int getStat(int _pid);
void cleanlist();


/* signal handlers */
void sigint_handler(int sig);
void sigchld_handler(int sig);
void sigtstp_handler(int sig);
/* signal handlers END */


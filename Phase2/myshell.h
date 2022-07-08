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
#define MAXARGLEN	256     //  maximum length of one argument
#define MAXARGS		32      //  max number of arguments
#define MAXLINE     256   //  max cmd input
#define READ        0       //  file descripter
#define WRITE       1       //  file descripter
#define MAXJOBS     32    //  max jobs
#define FALSE       0
#define TRUE        1
/* MACROS END */

/* function prototypes */
void eval(char* cmdline);
void execute(char* cmdline);
int builtin_command(char **argv);
void parseline(char *buf, char **argv);
void parsecmdbypipe(char *buf, char *cmd[MAXLINE], int* cmdCnt);
void sigint_handler(int sig);
/* function prototypes END */



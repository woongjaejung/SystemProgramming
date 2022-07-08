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
#define MAXLINE     4096   //  max cmd input
#define READ        0       //  file descripter
#define WRITE       1       //  file descripter
#define MAXRET      20000   //  max buffer
#define MAXJOBS     32    //  max jobs
#define ST          0
#define BG          1
#define FG          2
#define TERM        3
#define FALSE       0
#define TRUE        1
/* MACROS END */

/* Function prototypes */
void eval(char *cmdline);
void parseline(char *buf, char **argv);
int builtin_command(char **argv); 
void sigint_handler(int sig);
/* Function prototypes END */
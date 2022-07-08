#include "myshell.h"

/* main */
int main() {
    char cmdline[MAXLINE]; 
    signal(SIGINT, sigint_handler);

    while (1) {
        printf("CSE4100-SP-P1> ");
        fgets(cmdline, MAXLINE, stdin); 
        if (feof(stdin)) exit(0);
        if (strlen(cmdline)==1) continue;

        execute(cmdline);
    } 
}
/* main end */

/* execute */
/* This function execute command line entered by STDIN.
 * first, get the number of pipe '|' to determine number of loop
 * during loop, previous child computes each command(cmd[i]) and write to kernel using file descripter
 *              and the next child reads the written data from kernel as the input of current command(cmd[i+1])
 * if current child is the last of, write the result in STDOUT rather than kernel.
 */
void execute(char* cmdline){
    int cmdCnt=0;                                           //  num of commands separated by pipe '|'
    char* cmd[MAXLINE];                                     //  each command separated by pipe '|'
    int fd[2];                                              //  file descripter for pipe
    int pid[MAXARGS];
    int wpid;
    int jobpid;
    int read_fd = STDIN_FILENO;         //  for access to kernel
    /*  The reason I used read_fd is following; 
     *  I will not store the result of child process in parent, just leaving the result in kernel.
     *  For the next child reading the data in kernel requires me to save previous READ fp.
     *  The last command, cmd[last], will write the result in STDOUT, not in kernel.
     */     

    for(int i=0; i<MAXLINE; i++) cmd[i] = (char*)malloc(sizeof(char) * MAXLINE);  
    
    parsecmdbypipe(cmdline, cmd, &cmdCnt);  

    if(!builtin_command(cmd)){                              //  No need to fork built in command(cd, quit, exit, ...)
        if((jobpid=fork())==0){                             //  in CMDLINE
            for(int i=0; i<cmdCnt; i++){                    //  each process in a job
                pipe(fd);
                if((pid[i]=fork()) == 0){   
                    dup2(read_fd, STDIN_FILENO);            //  read from kernel
                    close(fd[READ]);
                    if(i+1 != cmdCnt)
                        dup2(fd[WRITE], STDOUT_FILENO);     //  write to kernel
                    eval(cmd[i]);
                }

                /* in JOB */
                wpid = waitpid(pid[i], NULL, 0);            //  reap all processes in a job
                close(fd[WRITE]);
                read_fd = fd[READ];
            }
            exit(0);                                        //  exit job
        }
        else{
            /* in SHELL */
            wpid = waitpid(jobpid, NULL, 0);                //  reap job process           
        }
    }
}
/* execute end */

/* builtin_command */
/* This function gets a parameter argv: user input command
 * if the command is built in command, the shell doesn't fork child.
 * rather, shell execute the command itself.
 */
int builtin_command(char **argv) {
    /* if built in command, return 1
     * else return 0
     */ 
    if (!strcmp(argv[0], "quit")) 	                        /* quit command */
		exit(0);
	if (!strcmp(argv[0], "exit")) 	                        /* exit command */
        exit(0);
    if (!strcmp(argv[0], "&"))    	                        /* Ignore singleton & */
		return 1;
	if (!strncmp(argv[0], "cd", 2)){	                    /* cd command */
        parseline(argv[0], argv);
		if(chdir(argv[1]) < 0)
			fprintf(stderr, "directory is not available \n");
		return 1;
	}    
    /* Not a builtin command */
    return 0;                     	
}
/* builtin_command end */

/* parsecmdbypipe */
/* This function parse the user input cmdline, using delemeter pipe '|'.
 * the fragments are stored in cmd[i].
 * total number of parsed command will be stored in cmdCnt.
 */
void parsecmdbypipe(char *cmdline, char *cmd[MAXLINE], int* cmdCnt){
    char *p;

    cmdline[strlen(cmdline)-1] = '|';                       //  last character of cmdline is '\n'
    p=strtok(cmdline, "|");                                 //  parse cmdline by '|'
    while(p){
        strcpy(cmd[(*cmdCnt)++], p);
        p=strtok(NULL, "|");
    }
}
/* parsecmdbypipe end */

/* eval */
/* This function evaluates each cmd[i] */
void eval(char *cmdline) { 
    char *argv[MAXARGS];                                    /* Argument list execve() */
    char buf[MAXLINE];                                      /* Holds modified command line */
    pid_t pid;                                              /* Process id */
    
	for(int i=0; i<MAXARGS; i++)
		argv[i] = (char*)malloc(sizeof(char) * MAXLINE);

    strcpy(buf, cmdline);
    parseline(buf, argv); 
    if (argv[0] == NULL)  
    /* Ignore empty lines */
		return;   
    if (execvp(argv[0], argv) < 0) {	
        printf("%s: Command not found.\n", argv[0]);
        exit(0);
    }       
 
    return;
}
/* eval end */

/* parseline */
/* This function gets two parameters, buf, argv 
 * Note, this function are mainly used in eval()
 * buf is cmd[i], and it may contains multiple ' '(space).
 * we need to ignore space and specify given cmd[i].
 * the result is stored in argv.
 */
void parseline(char *buf, char **argv) {
    /* This function parse given command_line(buf) into argv */
    char *delim;                                            /* Points to first space delimiter */
    int argc;                                               /* Number of args */
    int tmplen;
    buf[strlen(buf)+1] = '\0';                              /* Replace trailing '\n' with space */
    buf[strlen(buf)] = ' ';
    while (*buf && (*buf == ' '))                           /* Ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    while (TRUE) {
        if(*buf=='\'' || *buf=='\"'){                       // ' or " surrounding string
            delim=strchr(++buf, '\"');                      // must be ignored
            if(!delim) delim=strchr(buf, '\'');
        }
        else delim = strchr(buf, ' ');

        if(delim==NULL) break;                              // end of string, break;
		*delim = '\0';
        strcpy(argv[argc++], buf);
		buf = delim + 1;

		while (*buf && (*buf == ' '))                       /* Ignore spaces */
            buf++;
    }
    argv[argc] = NULL;  
}
/* parseline end */

/* sigint_handler */
/* shell will not be end even though SIGINT is received */
void sigint_handler(int sig){
    char s[]="\nCSE4100-SP-P1> ";
    write(STDOUT_FILENO, s, strlen(s));
    return;
}
/* sigint_handler end */
#include "myshell.h"

/* main */
int main() {
    char cmdline[MAXLINE]; 
    signal(SIGINT, sigint_handler);

    while (1) {
        printf("CSE4100-SP-P1> ");
        fgets(cmdline, MAXLINE, stdin); 
        if (feof(stdin)) exit(0);

        eval(cmdline);
    } 
}
/* main end */
  
/* eval */
/* This function evaluate command line entered by STDIN.
 * if input cmdline is not a builtin command,
 * fork a child and let it start given command.
 * we should wait the child to reap it.
 */
void eval(char *cmdline) 
{
    char *argv[MAXARGS];                                        /* Argument list execve() */
    char buf[MAXLINE];                                          /* Holds modified command line */
    pid_t pid;                                                  /* Process id */
    
	for(int i=0; i<MAXARGS; i++) 
        argv[i] = (char*)malloc(sizeof(char) * 1000);         

    strcpy(buf, cmdline);                                       // copy to buffer
    parseline(buf, argv);                                       // parse buf and store into argv
    if (argv[0] == NULL) return;                                /* Ignore empty lines */
    if (!builtin_command(argv)) {
        if((pid = fork()) == 0){
            // in COMMAND                                
            if (execvp(argv[0], argv) < 0) {	                // exec command
                printf("%s: Command not found.\n", argv[0]);    
                exit(0);
            }        
        }
        else{
            // in SHELL                                                   
            int status;
            pid_t wpid = waitpid(pid, &status, 0);
        }            	
    }
    return;
}
/* eval end */

/* builtin_command */
/* This function gets a parameter argv: user input command
 * if the command is built in command, the shell doesn't fork child.
 * rather, shell execute the command itself.
 */
int builtin_command(char **argv) {
    if (!strcmp(argv[0], "quit")) 	                            /* quit command */
		exit(0);
	if (!strcmp(argv[0], "exit")) 	                            /* exit command */
        exit(0);
    if (!strcmp(argv[0], "&"))    	                            /* Ignore singleton & */
		return 1;
    if (!strcmp(argv[0], "|"))                                  /* Ignore singleton | */
        return 1;
	if (!strcmp(argv[0], "cd")){	                            /* cd command */
		if(chdir(argv[1]) < 0)
			fprintf(stderr, "directory is not available \n");
		return 1;
	}
    return 0;                     	                            /* Not a builtin command */
}

/* parseline */
/* This function gets two parameters, buf, argv 
 * Note, this function are mainly used in eval()
 * buf is cmd[i], and it may contains multiple ' '(space).
 * we need to ignore space and specify given cmd[i].
 * the result is stored in argv.
 */
void parseline(char *buf, char **argv) {
    /* This function parse given command_line(buf) into argv */
    char *delim;                                                /* Points to first space delimiter */
    int argc;                                                   /* Number of args */
    int tmplen;
    buf[strlen(buf)-1] = ' ';                                   /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' '))                               /* Ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    while (TRUE) {
        if(*buf=='\'' || *buf=='\"'){                           // ' or " surrounding string
            delim=strchr(++buf, '\"');                          // must be ignored
            if(!delim) delim=strchr(buf, '\'');
        }
        else delim = strchr(buf, ' ');

        if(delim==NULL) break;                                  // end of string, break;
		*delim = '\0';
        strcpy(argv[argc++], buf);
		buf = delim + 1;

		while (*buf && (*buf == ' '))                           /* Ignore spaces */
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
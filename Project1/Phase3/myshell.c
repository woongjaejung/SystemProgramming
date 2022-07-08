#include "myshell.h"

/* global variables */
struct job{
    pid_t pid;
    int status;
    char name[MAXLINE];
} joblist[MAXJOBS];
int jobCnt;

int fg_pid; char fg_name[MAXLINE];          //  for foreground process
sigset_t mask, prev;                        //  signal masks
/* global variables end */

/* main */
int main(int argc, char** argv){
    char cmdline[MAXLINE];

    /* set signal handler to process */
    signal(SIGINT, sigint_handler);
    signal(SIGTSTP, sigtstp_handler);
    signal(SIGCHLD, sigchld_handler);

    /* initialize global variables */
    sigemptyset(&mask); sigemptyset(&prev);
	sigemptyset(&mask);	sigaddset(&mask, SIGINT); sigaddset(&mask, SIGTSTP);    // mask blocks SIGINT(CTRL+C), SIGTSTP(CTRL+Z)
	sigprocmask(SIG_BLOCK, &mask, &prev);   //  prev
    sigprocmask(SIG_SETMASK, &prev, NULL);  //  initial condition: shell can recieve signals
    memset(joblist, 0, sizeof(joblist));
    jobCnt=0;    
    /* initialization end */
    
    while(1){
		fg_pid=0;        
        printf("CSE4100-SP-P1> ");
        fgets(cmdline, MAXLINE, stdin); 
        if(feof(stdin)) exit(0);
        if(strlen(cmdline)==1) continue;
        
        execute(cmdline);        
    }

}
/* main end */

/* execute */
/* re-used
 * MODIFIED point: we can execute the background process.
 * note that, background process does not interrupted by SIGINT, SIGTSTP signals.
 */
void execute(char* cmdline){
    int cmdCnt=0;                       //  num of commands separated by pipe '|'
    char* cmd[MAXARGS];                 //  each command separated by pipe '|'
    int fd[2];                          //  file descripter for pipe
    int pid[MAXARGS];
    int jobpid;
    int stat;
    int wpid;
    int read_fd = STDIN_FILENO;         //  for access to kernel
    char buf[MAXLINE];                  //  remember cmdline
    /*  The reason I used read_fd is following; 
     *  I will not store the result of child process in parent, just leaving the result in kernel.
     *  For the next child reading the data in kernel requires me to save previous READ fp.
     *  The last command, cmd[last], will write the result in STDOUT, not in kernel.
     */    
    
    strcpy(buf, cmdline); buf[strlen(buf)-1]='\0';
    for(int i=0; i<MAXARGS; i++) cmd[i] = (char*)malloc(sizeof(char) * MAXLINE);  
    stat=parsecmdbypipe(cmdline, cmd, &cmdCnt);

    if(!builtin_command(cmd)){
        if((jobpid=fork())==0){    
            //  in CMDLINE
            setpgid(getpid(), 0);                               //  separate job from shell
            if(stat==BG){ 
                sigprocmask(SIG_BLOCK, &mask, &prev);           //  background process should not be interrupted by SIGINT, SIGTSTP                
            }
            else            
                sigprocmask(SIG_SETMASK, &prev, NULL);          //  foreground process should receive SIGINT, SIGTSTP

            for(int i=0; i<cmdCnt; i++){                        //  each process in a CMDLINE             
                pipe(fd);
                if((pid[i]=fork()) == 0){
                //  in PROCESS                                       
                    dup2(read_fd, STDIN_FILENO);                //  read from kernel
                    close(fd[READ]);
                    if(i+1 != cmdCnt)
                        dup2(fd[WRITE], STDOUT_FILENO);         //  write to kernel
                    eval(cmd[i]);
                }

                //  in CMDLINE
                wpid = waitpid(pid[i], NULL, 0);                //  reap each process in a job
                close(fd[WRITE]);
                read_fd = fd[READ];
            }
            // exit CMDLINE
            exit(0);                                        
        }
        else{  
        //  in MYSHELL
            setpgid(getpid(), 0);                               //  separate job from shell
            if(stat==BG){  
				sigprocmask(SIG_BLOCK, &mask, &prev);           //  background process should not be interrupted by SIGINT, SIGTSTP
                addjob(jobpid, buf);                            //  add job as original job name (without &)
            }

            sigprocmask(SIG_SETMASK, &prev, NULL);              //  foreground process should receive SIGINT, SIGTSTP

            if(stat == FG) {  
				int _status;
				fg_pid=jobpid;
				memset(fg_name, 0, sizeof(fg_name));
				strcpy(fg_name, buf);                           //  remember name as original job name
                
				if(wpid = waitpid(jobpid, &_status, WUNTRACED)<0)
                //  reap job process explicitly                      
                    fprintf(stderr, "wait job error in MYSHELL \n");    
            }
        }
    }
    for(int i=0; i<cmdCnt; i++) free(cmd[i]);                   //  memory free
    return;
}
/* execute and */

/* builtin_command */
/* re-used
 * MODIFIED point:  jobs, fg, bg, kill are added.
 *                  they are computed in listjobs(), cmd_fg, cmd_bg, cmd_kill, respectively
 */
int builtin_command(char **argv) {
    if (!strcmp(argv[0], "quit")) 	    /* quit command */{
        exit(0);
    }
	if (!strcmp(argv[0], "exit")) 	    /* exit command */
        exit(0);
    if (!strcmp(argv[0], "&"))    	    /* Ignore singleton & */
		return 1;
	if (!strncmp(argv[0], "cd", 2)){	/* cd command */
        parseline(argv[0], argv);
		if(chdir(argv[1]) < 0)
			fprintf(stderr, "directory is not available \n");
		return 1;
	}
    if (!strncmp(argv[0], "jobs", 4)){
        listjobs();
        return 1;
    }
    if (!strncmp(argv[0], "fg", 2)){    /* fg <job>: background job to foreground */
        parseline(argv[0], argv);
        cmd_fg(argv);
        return 1;
    }
    if (!strncmp(argv[0], "bg", 2)){    /* bg <job>: foreground job to background */
        parseline(argv[0], argv);
        cmd_bg(argv);
        return 1;

    }
    if (!strncmp(argv[0], "kill", 4)){  /* kill <job>: terminate given job */
        parseline(argv[0], argv);
        cmd_kill(argv);
        return 1;
    }
    return 0;                     	/* Not a builtin command */
}
/* builtin_command end */

/************************* JOB-LIST MANAGEMENT FUNCTIONS *************************/

/* listjobs */
/* This function is called when the command fg is entered.
 * list all suspended and running background jobs.
 */
void listjobs(){
    char *s[] ={"suspended", "running", "running", "terminated"};
    for(int i=1; i<=jobCnt; i++){
        if(joblist[i].pid != REAPED)
            printf("[%d] %s %s \n", i, s[joblist[i].status], joblist[i].name);
    }
}
/* listjobs end */

/* addjob */

void addjob(pid_t _pid, char* _name){
    int _status = getStat(_pid);
    int idx=getJobidx(_pid);

    if(idx<0)  
    //  current pid is not in the joblist -> add job at the end
        idx = (++jobCnt);

    for(int i=strlen(_name)-1; i>=0; i--)
        if(_name[i]==' ' || _name[i]=='|') _name[i]='\0';
        else break;

    joblist[idx].pid=_pid;
    joblist[idx].status=_status;
    strcpy(joblist[idx].name, _name);
}
/* addjob end */

/* delete job */
void deletejob(pid_t _pid){
    int idx=getJobidx(_pid);
    if(idx<0) return;

    joblist[idx].pid=REAPED;            // mask pid
	memset(joblist[idx].name, 0, sizeof(joblist[idx].name));
    if(idx==jobCnt) jobCnt--;           // if deleted job is the last
}
/* delete job end */

/* getJobidx */
/* returns the index of joblist where its pid=_pid */
int getJobidx(pid_t _pid){
    for(int i=1; i<=jobCnt; i++)
        if(joblist[i].pid == _pid) return i;

    //  if _pid is not in the joblist return -1
    return -1;  
}

/* getJobidx end */

/* getStat */
/* joblist must retain the status of job in joblist
 * the data of process(pid) are in the directory "/proc/[pid]/stat"
 * access the directory using sprintf, sscanf functions
 */
int getStat(int _pid){
    char path[30], *buf, foostr[100];
    char stat;
    FILE* fp;
    int fooint;
	buf=(char*)malloc(sizeof(char) * MAXBUF);
    sprintf(path, "/proc/%d/stat", _pid);    /* /proc/[pid]/stat */

    fp=fopen(path, "r");

	if(fp==NULL) return -1;

    fgets(buf, MAXBUF, fp);
    sscanf(buf, "%d %s %c", &fooint, foostr, &stat);

    fclose(fp);
	free(buf);

    switch(stat){
    case 'R': return BG;    //  running jobs always on background when getStat called.
    case 'S': return ST;    //  sleeping    
    case 'T': return ST;    //  stopped by signal
    case 'Z': return TERM;  //  zombie
    }
    return -1;

}

/* cleanlist */
/* This function is called when a background job is finished.
 * if there is any terminated jobs in joblist, remove them.
 * This is called in the SIGCHLD handler
 */
void cleanlist(){
	int _status;
	if(jobCnt==0) return;
	for(int i=jobCnt; i>0; i--){
		if(joblist[i].pid==REAPED) continue;	// already checked

		_status=getStat(joblist[i].pid);
		if(_status<0) {
			/* status<0 AND pid!=REAPED means process already terminated !!! */
			joblist[i].pid=REAPED;
			memset(joblist[i].name, 0, sizeof(joblist[i].name));
			
			if(i==jobCnt) --jobCnt;
		}
		
		if(joblist[i].status != ST && joblist[i].status != BG && joblist[i].status != FG){		// terminated but yet alive in list
			joblist[i].pid=REAPED;
			memset(joblist[i].name, 0, sizeof(joblist[i].name));
			
			if(i==jobCnt) --jobCnt;
		}
		
	}
	if(jobCnt==1 && joblist[1].pid==REAPED) --jobCnt;
}
/* cleanlist end */

/************************* JOB-LIST MANAGEMENT FUNCTIONS END *************************/

/* cmd_fg */
/* fg <job>: if job is suspended or running on background, bring it on foreground
 * job number always is given with '%' as a prefix
 * print the result of this command.
 * "[num] running name"
 */
void cmd_fg(char** argv){
    
    int jnum=atoi(&argv[1][1]);
    if(jnum > jobCnt || joblist[jnum].pid == REAPED)
        fprintf(stderr, "No Such Job \n");   

	/* check current job state */
    joblist[jnum].status=getStat(joblist[jnum].pid);
    if(joblist[jnum].status==BG || joblist[jnum].status==ST){
        joblist[jnum].status=FG;

        /* send signal to given job */
        kill(-(joblist[jnum].pid), SIGCONT);
        sigprocmask(SIG_SETMASK, &prev, NULL);

        /* update foreground job */
        fg_pid=joblist[jnum].pid; strcpy(fg_name, joblist[jnum].name);
		deletejob(joblist[jnum].pid);
        printf("[%d] running %s \n", jnum, fg_name);

        /* wait for foreground job */
		int _status;
        if(waitpid(joblist[jnum].pid, &_status, WUNTRACED) < 0){
            fprintf(stderr, "wait error in CMD_FG \n");
            exit(0);
        }		
    }
}
/* cmd_fg end */

/* cmd_bg */
/* bg <job>: if job is suspended, bring it at background
 * job number always is given with '%' as a prefix
 * print the result of this command.
 * "[num] running name"
 */
void cmd_bg(char** argv){
    // bg <job>
    // job number always entered with '%'
    int jnum=atoi(&argv[1][1]);  
    if(jnum > jobCnt || joblist[jnum].pid == REAPED)
        fprintf(stderr, "No Such Job \n");   

   	joblist[jnum].status=getStat(joblist[jnum].pid);
	/* check current job state */

    if(joblist[jnum].status==ST){
        /* running job does not receive signal */
        joblist[jnum].status=BG;
        kill(-(joblist[jnum].pid), SIGCONT);
        printf("[%d] running %s \n", jnum, joblist[jnum].name);

    }
}
/* cmd_bg end */

/* cmd_kill */
/* kill <job/pid>: terminate given job or process of pid in job list
 * job number always is given with '%' as a prefix
 * do not print the result of this command, except wrong input entered.
 */
void cmd_kill(char** argv){
    int pid;
	int jnum;
    if(argv[1][0] == '%') {
        pid = atoi(&argv[1][1]);        
        pid=joblist[pid].pid;
    }
    else{
        pid=atoi(argv[1]);    
    }
	/* check current job state */
	jnum=getJobidx(pid);
	if(jnum < 0 || joblist[jnum].pid==REAPED) {
		fprintf(stderr, "No Such Job \n");   
		return;
	}

    joblist[jnum].status=TERM;

    if(kill(-(pid), SIGKILL) < 0){   //  fix ATOI
        if(errno == ESRCH){
            fprintf(stderr, "No Such Job \n");
        }
        if(errno == EPERM){
            fprintf(stderr, "calling process has no permission to send the signal! \n");
        }
        return;
    }
    deletejob(pid);
}
/* cmd_kill end */

/* sigchld_handler */
/* using when background job terminated.
 * we should not hang on for child, so that using WNOHANG
 */
void sigchld_handler(int sig){
    int oerno = errno; 
    sigset_t mask, prev;
    pid_t pid;
    int status;

	sigfillset(&mask);
    sigprocmask(SIG_BLOCK, &mask, &prev);

    while((pid = waitpid(-1, &status, WNOHANG)) > 0){ 
        /* children(more than one) can be waiting to be reaped in WAIT set */
        cleanlist();
    }
    
    sigprocmask(SIG_SETMASK, &prev, NULL);
    errno = oerno;
    return;
}
/* sigchld_handler end */

/* sigint_handler */
/* send SIGKILL signal to foreground job (fg_pid)
 * fg_pid and fg_name automatically initialized in shell
 */
void sigint_handler(int sig){
    int oerno = errno; 
    sigset_t mask_all, prev_one;
    
	if(fg_pid==0) {		
    //	no foreground process... return
		fprintf(stderr, "\nCSE4100-SP-P1> ");
		return;	
	}
    
    sigfillset(&mask_all);
    sigprocmask(SIG_BLOCK, &mask_all, &prev_one);

    fprintf(stderr, "\n");
    kill(-fg_pid, SIGKILL);
    deletejob(fg_pid);

    sigprocmask(SIG_SETMASK, &prev_one, NULL);
    errno=oerno;
    return;
}
/* sigint_handler end */

/* sigtstp_handler */
/* send SIGSTOP signal to foreground job (fg_pid)
 * fg_pid and fg_name automatically initialized in shell
 */
void sigtstp_handler(int sig){
	int oerno = errno; 
    sigset_t mask_all, prev_one;

	if(fg_pid==0) {		
    //	no foreground process... return
		fprintf(stderr, "\nCSE4100-SP-P1> ");
		return;	
	}
    sigfillset(&mask_all);
    sigprocmask(SIG_BLOCK, &mask_all, &prev_one);    

    fprintf(stderr, "\n");
    kill(-fg_pid, SIGSTOP);
    addjob(fg_pid, fg_name);
    joblist[getJobidx(fg_pid)].status=ST; // can be SEG

    sigprocmask(SIG_SETMASK, &prev_one, NULL);
    errno=oerno;
}
/* sigtstp_handler end */


/************************* RE-USED *************************/
/* parsecmdbypipe */
/* re-used.
 * MODIFIED point: check the input command line contains '&'.
 * if so, return BG. else, return FG.
 */
int parsecmdbypipe(char *cmdline, char *cmd[MAXLINE], int* cmdCnt){
    char *p;
    int stat=FG;

    cmdline[strlen(cmdline)-1] = '|';   //  last character of cmdline is '\n'
    p=strtok(cmdline, "|");           //  parse cmdline by '|'
    while(p){
        strcpy(cmd[(*cmdCnt)++], p);
        p=strtok(NULL, "|");
    }

    // MODIFIED point, check BG or FG
    int len=strlen(cmd[(*cmdCnt)-1]);
    for(int i=len-1; i>=0; i--){
        if(cmd[(*cmdCnt)-1][i] == '&'){
            stat=BG;
            cmd[(*cmdCnt)-1][i] = '\0';
            break;
        }
    }
    return stat;
}
/* parsecmdbypipe end */

/* eval */
/* re-used */
void eval(char *cmdline) { 
    char *argv[MAXARGS];                /* Argument list execve() */
    char buf[MAXLINE];                  /* Holds modified command line */
    pid_t pid;                          /* Process id */
    
	for(int i=0; i<MAXARGS; i++)
		argv[i] = (char*)malloc(sizeof(char) * MAXLINE);

    strcpy(buf, cmdline);
    parseline(buf, argv); 
    if (argv[0] == NULL)  
		return;   /* Ignore empty lines */
    if (execvp(argv[0], argv) < 0) {	//ex) /bin/ls ls -al &
        printf("%s: Command not found.\n", argv[0]);
        exit(0);
    }       
 
    return;
}
/* eval end */

/* parseline */
/* re-used */
void parseline(char *buf, char **argv) {
    char *delim;                                                /* Points to first space delimiter */
    int argc;                                                   /* Number of args */
    int tmplen;
    buf[strlen(buf)+1] = '\0';                                  /* Replace trailing '\n' with space */
    buf[strlen(buf)] = ' ';
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

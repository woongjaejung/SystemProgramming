System Programming Project 1 Phase3
Author: 20181685 Jaewoong Jung
Prof: Youngjae Kim

How to compile:
    3 Files must be included in same directory; myshell.c, myshell.h, Makefile
Just type 'make' to make a executable file, name of 'myshell'

Phase3:
    This phase is basically based on previous 2 phases.
Almost all functions in previous 2 phases are re-used.
The main issue in thie phase is 'How to reap background job?'.
We can solve the problem using SIGCHLD handler, not explicitly waiting for background jobs.

Outline of this phase is following;
1. make a data structure for jobs command; struct job and list of job, joblist[].
2. we must maintain the foreground job pid(fg_pid) and foreground job name(fg_name).
3. new commands to implement; kill, fg, bg, jobs
    3-1. kill <job/pid>: terminate given job or process of pid in job list
    3-2. fg <job>: if job is suspended or running on background, bring it on foreground
    3-3. bg <job>: if job is suspended, bring it at background
    3-4. jobs: list jobs using listjobs(). print as "[num] status name"
4. CTRL+C(SIGINT), CTRL+Z(SIGTSTP) must affect on only foreground job.

Fail to implement
1. myshell on myshell; 
if we execute myshell on myshell, the quit/exit command will terminate all the foreground(incluing origin) myshell
resulting escaping to bash shell.
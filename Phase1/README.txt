System Programming Project 1 Phase1
Author: 20181685 Jaewoong Jung
Prof: Youngjae Kim

How to compile:
    3 Files must be included in same directory; myshell.c, myshell.h, Makefile
Just type 'make' to make a executable file, name of 'myshell'

Phase1:
1. get command_line and tokenize it by using parseline().
2. if no given command(argv[0] == NULL) or argv[0] is an built in command,
    we don't need to call fork(). (also singleton '&')
    * built-in-command: cd, quit, exit
3. after check, call fork() and execute it in child process by calling execvp()

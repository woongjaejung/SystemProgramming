System Programming Project 1 Phase2
Author: 20181685 Jaewoong Jung
Prof: Youngjae Kim

How to compile:
    3 Files must be included in same directory; myshell.c, myshell.h, Makefile
Just type 'make' to make a executable file, name of 'myshell'

Phase2:
1. get command line and tokenize it with delimiter "|".
2. store the fractions into cmd[], element num of cmd[] into cmdCnt.
3. if no given command(argv[0] == NULL) or argv[0] is an built in command,
    we don't need to call fork(). (also singleton '&')
    * built-in-command: cd, quit, exit
4. loop cmdCnt times; 
    4-1. evaluate cmd[i] in the forked child.
    4-2. store the result in kernel by using fd[WRITE].
    4-3. save fd[READ] for next child to read result from kernel.




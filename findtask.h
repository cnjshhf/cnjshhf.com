#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <regex.h>
#include <memory.h>

int regxmatch_task(char *bematch,char *pattern,char **matchback);
int readfile_task(char **p);
int findtask(const char * ep,char **task,char **port);

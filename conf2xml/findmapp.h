#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <regex.h>
#include <memory.h>

int regxmatch_mapp(char *bematch,char *pattern,char **matchback);
int readfile_mapp (char **p,char *propath);
int findmapp(char * ep,char *filepath,char * type);
int findproc(char * ep,char *filepath,char *type);

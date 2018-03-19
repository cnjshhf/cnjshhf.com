#include <strings.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include "qstring.h"
#include <time.h>
#include <sys/types.h>
#include <unistd.h>


 
char * strcat2(int argc, const char *str1, const char * str2, ...) 
{
    int tmp = 0;
    char *dest = NULL;
    char *cur = NULL;
    va_list va_ptr;
    size_t len = strlen(str1) + strlen(str2);

    argc -= 2;
    tmp = argc;

    va_start(va_ptr, str2);
    while(argc-- && (cur = va_arg(va_ptr,char *))) {
        len += strlen(cur);
    }
    va_end(va_ptr);

    dest = (char *)malloc(len+1);
    dest[0] = '\0';
    strcat(dest, str1);
    strcat(dest, str2);

    argc = tmp;
    va_start(va_ptr, str2);
    while(argc-- && (cur = va_arg(va_ptr,char *))) {
        strcat(dest, cur);
    }
    va_end(va_ptr);

    return dest;
}

char * strim(char *str)
{
    char *end, *sp, *ep;
    size_t len;

    sp = str;
    end = ep = str+strlen(str)-1;
    while(sp <= end && isspace(*sp)) sp++;
    while(ep >= sp && isspace(*ep)) ep--;
    len = (ep < sp) ? 0 : (ep-sp)+1;
    sp[len] = '\0';
    return sp;
}

char ** strsplit(char *line, char delimeter, int *count, int limit)
{
    char *ptr = NULL, *str = line;
    char **vector = NULL;

    *count = 0;
    while((ptr = strchr(str, delimeter))) {
        *ptr = '\0';
        vector = (char **)realloc(vector,((*count)+1)*sizeof(char *));
        vector[*count] = strim(str);
        str = ptr+1;
        (*count)++;	
        if (--limit == 0) break;
    }
    if (*str != '\0') {
        vector = (char **)realloc(vector,((*count)+1)*sizeof(char *));
        vector[*count] = strim(str);
        (*count)++;
    }
    return vector;
}

int yesnotoi(char *str)
{
    if (strcasecmp(str,"yes") == 0) return 1;
    if (strcasecmp(str,"no") == 0) return 0;
    return -1;
}

int setLogName(char *str)
{       
        memset(LogName,0,sizeof(LogName));
        strcpy(LogName,str);
        return 0;
}

void LOG(char *fl,int li,const char* ms, ...)
{       
        char wzLog[1024] = { 0 };
        char buffer[1024] = { 0 };
        va_list args;
        va_start(args, ms); 
        vsprintf(wzLog, ms, args);
        va_end(args);
        
        time_t now;
        time(&now);
        struct tm *local;
        local = localtime(&now);
        sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d |%d|%s->%d%s", local->tm_year
 + 1900, local->tm_mon,
                local->tm_mday, local->tm_hour, local->tm_min, local->tm_sec,getpgid
(0),fl,li,      
                wzLog);
        FILE* file = fopen(LogName, "a+");
        fwrite(buffer, 1, strlen(buffer), file);
        fclose(file);


}

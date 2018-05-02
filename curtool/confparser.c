#include "qstring.h"
#include "confparser.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#include <malloc.h>

 
Config * initconfig()
{
    Config *conf = (Config *)malloc(sizeof(Config));

    conf->url = NULL;

    return conf;
}
void loadconfig(Config *conf)
{
    FILE *fp = NULL;
    char buf[MAX_CONF_LEN+1];
    int argc = 0;
    char **argv = NULL;
    int linenum = 0;
    char *line = NULL;
    const char *err = NULL;

    if ((fp = fopen(CONF_FILE, "r")) == NULL) {
        printf("Can't load conf_file %s", CONF_FILE);	
    } 

    while (fgets(buf, MAX_CONF_LEN+1, fp) != NULL) {
        linenum++;
        line = strim(buf);

        if (line[0] == '#' || line[0] == '\0') continue;

        argv = strsplit(line, '=', &argc, 1);
        if (argc == 2) {
            if (strcasecmp(argv[0], "vurl") == 0) {
                conf->vurl.push_back(strdup(argv[1]));
            } else {
                err = "未知的配置文件名称"; goto conferr;
            }
        } else {
            err = "配置文件的形式必须为 'key=value'"; goto conferr;
        }

    }
    return;
    

conferr:
    printf("Bad directive in %s[line:%d] %s", CONF_FILE, linenum, err);	
}


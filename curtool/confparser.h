#ifndef CONFPARSER_H
#define CONFPARSER_H

#include <vector>
using namespace std;
 
#define MAX_CONF_LEN  1024
#define CONF_FILE     "curtool.conf"
typedef struct Config {
    char            *url;
    vector<char *>   vurl;
};

extern Config * initconfig();

extern void loadconfig(Config *conf);

#endif


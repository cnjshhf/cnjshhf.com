#ifndef CONFPARSER_H
#define CONFPARSER_H

#include <vector>
using namespace std;
 
#define MAX_CONF_LEN  1024
#define CONF_FILE     "tool.conf"
typedef struct Config {
    char            *url;
    char            *module_path;
    vector<char *>   vurl;
    vector<char *>   load_modules;
    vector<char *>   xml_modules;
}Config;

extern Config * initconfig();

extern void loadconfig(Config *conf);

#endif


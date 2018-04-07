#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <signal.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include <vector>
#include "qstring.h"
#include "confparser.h"
#include "dso.h"
#include "xml.h"
#include <dlfcn.h>

#include <iostream>

using namespace std;


int main(void )
{

    /*载入配置文件*/
	Config *g_conf = NULL;
    g_conf = initconfig();
    loadconfig(g_conf);
    Module * module;
    map<string,Module*> mymap;


    /*载入测试动态库文件*/
    vector<char *>::iterator it = g_conf->load_modules.begin();
    for(; it != g_conf->load_modules.end(); it++) {
        module=dso_load(g_conf->module_path, *it); 
        mymap[*it] = module; 
    } 
    cout << "mymap.size() is " << (int) mymap.size() << endl;
    mymap.find("mytest")->second->handle(NULL);
    mymap.find("test")->second->handle(NULL);
	return 0;
}

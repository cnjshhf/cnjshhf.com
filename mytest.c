#include "dso.h"
#include <fcntl.h>
#include <iostream>
#include <vector>
using namespace std;
 
 
static int handler(void * data) {
    printf("mytest调用成功");
    return MODULE_OK;
}

static void init(Module *mod)
{

}

Module mytest = {
        init,
        handler

};
 


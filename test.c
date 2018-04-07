#include "dso.h"
#include <fcntl.h>
 
static int handler(void * data) {
    printf("test调用成功");
    return MODULE_OK;
}

static void init(Module *mod)
{

}

Module test = {
    init,
    handler
};


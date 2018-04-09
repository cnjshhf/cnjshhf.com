/*************************************************************************
	> File Name: mytest.c
	> Author: hhf
	> Mail: hhf 
	> Created Time: 2018-03-19 21:34:00
 ************************************************************************/
#include <stdio.h>
#include "dso.h"
#include "xml.h"

int handle(void *data)
{
    printf("模块载入测试成功\n");
}

void init(Module *mod)
{
    return;
}

Module mytest = {
    init,
    handle
};

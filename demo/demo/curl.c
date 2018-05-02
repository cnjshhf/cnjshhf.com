#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
        FILE *fp;
        char tmp[1024*10] = {0};
        char cmd[1024] = {0};

        snprintf(cmd, sizeof(cmd), "curl http://192.168.59.148:80/test.do");
        if ((fp = popen(cmd, "r")) != NULL){
                while(fgets(tmp, sizeof(tmp), fp)!=NULL){
                        printf("[%s]",tmp);
                }
	}
	pclose(fp);
	return 0;
}



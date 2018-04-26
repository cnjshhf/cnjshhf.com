#include <openssl/md5.h>
#include <string.h>
#include <stdio.h>

/*int MD5_Init(MD5_CTX *c);
 * int MD5_Update(MD5_CTX *c, const void *data, size_t len);
 * int MD5_Final(unsigned char *md, MD5_CTX *c);*/

int main()
{
	int i;
	MD5_CTX ctx;
	unsigned char outmd[16]={0};
	const char *data = "123";	
	size_t len = strlen(data);	
	MD5_Init(&ctx);
	MD5_Update(&ctx,data,len);
	MD5_Final(outmd,&ctx);
	for(i=0;i<16;i++)
	{
		printf("%02X",outmd[i]);
	}
	return 0;
	
}

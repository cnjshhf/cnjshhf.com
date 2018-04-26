#include <openssl/sha.h>
#include <string.h>
#include <stdio.h>
/*
#define SHA_DIGEST_LENGTH 20
#define SHA224_DIGEST_LENGTH    28
#define SHA256_DIGEST_LENGTH    32
#define SHA384_DIGEST_LENGTH    48
#define SHA512_DIGEST_LENGTH    64


typedef struct SHAstate_st
        {
        SHA_LONG h0,h1,h2,h3,h4;
        SHA_LONG Nl,Nh;
        SHA_LONG data[SHA_LBLOCK];
        unsigned int num;
        } SHA_CTX;

int SHA1_Init(SHA_CTX *c);
int SHA1_Update(SHA_CTX *c, const void *data, size_t len);
int SHA1_Final(unsigned char *md, SHA_CTX *c);
*/

int main1()
{
	int i;

	SHA_CTX ctx;
	unsigned char outmd[20]={0};
	const char *data = "1231";	
	size_t len = strlen(data);	
	SHA1_Init(&ctx);
	SHA1_Update(&ctx,data,len);
	SHA1_Final(outmd,&ctx);
	for(i=0;i<20;i++)
	{
		printf("%02x",outmd[i]);
	}
	return 0;
	
}
int main224()
{
        int i;
        SHA256_CTX ctx;
        unsigned char outmd[28]={0};
        const char *data = "1231";
        size_t len = strlen(data);
        SHA224_Init(&ctx);
        SHA224_Update(&ctx,data,len);
        SHA224_Final(outmd,&ctx);
        for(i=0;i<28;i++)
        {
                printf("%02x",outmd[i]);
        }
        return 0;

}
int main384()
{
        int i;
        SHA512_CTX ctx;
        unsigned char outmd[48]={0};
        const char *data = "1231";
        size_t len = strlen(data);
        SHA384_Init(&ctx);
        SHA384_Update(&ctx,data,len);
        SHA384_Final(outmd,&ctx);
        for(i=0;i<48;i++)
        {
                printf("%02x",outmd[i]);
        }
        return 0;

}
int main()
{
        int i;
        SHA512_CTX ctx;
        unsigned char outmd[64]={0};
        const char *data = "1231";
        size_t len = strlen(data);
        SHA512_Init(&ctx);
        SHA512_Update(&ctx,data,len);
        SHA512_Final(outmd,&ctx);
        for(i=0;i<64;i++)
        {
                printf("%02x",outmd[i]);
        }
        return 0;

}

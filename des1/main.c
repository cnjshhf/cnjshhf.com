
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "des.h"

int main()
{
	int		rv = 0;

	unsigned char *pInData = "12345678";
	int            nInDataLen = strlen(pInData);
	unsigned char *aKey = "12345678";

	
	unsigned char OutData[2048];
	int           OutDataLen = 0;

	unsigned char buf2[2048];//明文2
	int           buf2Len = 0;
	char buf[1024] = {0};


	memset(buf2, 0, sizeof(buf2));
	memset(OutData, 0, sizeof(OutData));

	//用户使用的函数
	//void Ek(unsigned char *aPlainText, unsigned char *aKey, unsigned char *aEncipher);
	//void Dk(unsigned char *aEncipher, unsigned char *aKey, unsigned char *aPlainText);
	rv = Ek(pInData, aKey, &buf);
	if (rv != 0)
	{
		printf("func DesEnc() err.err[%d]", rv);
		return rv;
	}
	/*

	//用户使用函数des解密
	rv = DesDec(OutData,OutDataLen, buf2, &buf2Len);
	if (rv != 0)
	{
		printf("func DesDec() err.err[%d]", rv);
		return rv;
	}

	if (nInDataLen != buf2Len)
	{
		rv = -1;
		printf("解密后的明文和原始明文长度不一致[%d][%d]\n",nInDataLen,buf2Len);
		return rv;
	}
	else
	{
		printf("解密后的明文和原始明文长度一致\n");
	}

	if (memcmp(pInData, buf2, buf2Len)!=0)
	{
		rv = -1;
		printf("解密后的明文和原始明文内容不一致\n");
		return rv;
	}
	else
	{
		printf("解密后的明文和原始明文内容一致\n");
	}

	printf("加解密绿灯测试通过\n");

	getchar();
	*/
	return 0;
}

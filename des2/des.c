#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <rpc/des_crypt.h>

void des_encrypt(const char *key, char *data, int len)
{
    char pkey[8];
    strncpy(pkey, key, 8);
    des_setparity(pkey);
    do {
        data[len++] = '\x8';
    } while (len % 8 != 0);
    ecb_crypt(pkey, data, len, DES_ENCRYPT);
}

void des_decrypt(const char *key, char *data, int len)
{
    char pkey[8];
    strncpy(pkey, key, 8);
    des_setparity(pkey);
    ecb_crypt(pkey, data, len, DES_DECRYPT);
}

int main(int argc, char *argv[])
//C=Ek3(Dk2(Ek1(M)))
//M=Dk1(EK2(Dk3(C)))
//3des加密公式
{
    int i;
    char data[4096] = "1234567887654321";
    int len = strlen(data);
    des_encrypt("abcd1234", data, len);
    des_decrypt("abcd1233", data, len);
    des_encrypt("abcd1232", data, len);
    des_decrypt("abcd1232", data, len);
    des_encrypt("abcd1233", data, len);
    des_decrypt("abcd1234", data, len);
    for(i=0;i<len;i++)
    {
    	printf("%c",data[i]);
    }
    return 0;
}

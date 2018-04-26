// sign.cpp : Defines the entry point for the console application.  
//  
  
//#include "stdafx.h"  
  
#include <stdio.h>  
//#include <openssl/rsa.h>  
#include <openssl/evp.h>  
//#include <openssl/objects.h>  
#include <openssl/x509.h>  
#include <openssl/err.h>  
#include <openssl/pem.h>  
#include <openssl/ssl.h>  
  
  
  
void InitOpenSSL()  
{  
    ERR_load_crypto_strings();  
}  
  
unsigned char * GetSign(char* keyFile,char* plainText,unsigned char* cipherText,unsigned int *cipherTextLen)  
{     
    FILE* fp = fopen (keyFile, "r");  
    if (fp == NULL)   
        return NULL;  
  
    /* Read private key */  
    EVP_PKEY* pkey = PEM_read_PrivateKey(fp, NULL, NULL, NULL);  
    fclose (fp);  
  
    if (pkey == NULL) {   
        ERR_print_errors_fp (stderr);  
        return NULL;  
    }  
  
    /* Do the signature */  
    EVP_MD_CTX     md_ctx;  
    //EVP_SignInit   (&md_ctx, EVP_sha1());  
    EVP_SignInit   (&md_ctx, EVP_md5());
    EVP_SignUpdate (&md_ctx, plainText, strlen(plainText));  
    int err = EVP_SignFinal (&md_ctx, cipherText, cipherTextLen, pkey);  
  
    if (err != 1) {  
        ERR_print_errors_fp(stderr);  
        return NULL;  
    }  
  
    EVP_PKEY_free(pkey);  
  
    return cipherText;  
}  
  
int VerifySign(char* certFile,unsigned char* cipherText,unsigned int cipherTextLen,char* plainText)  
{  
    /* Get X509 */  
    FILE* fp = fopen (certFile, "r");  
    if (fp == NULL)   
        return 1;  
    X509* x509 = PEM_read_X509(fp, NULL, NULL, NULL);
    fclose (fp);  
  
    if (x509 == NULL) {  
        ERR_print_errors_fp (stderr);  
        return 1;  
    }  
  
    /* Get public key - eay */  
    EVP_PKEY *pkey=X509_get_pubkey(x509);  
    if (pkey == NULL) {  
        ERR_print_errors_fp (stderr);  
        return 1;  
    } 
  
    /* Verify the signature */  
    EVP_MD_CTX md_ctx;  
    //EVP_VerifyInit   (&md_ctx, EVP_sha1());  
    EVP_VerifyInit   (&md_ctx, EVP_md5()); 
    EVP_VerifyUpdate (&md_ctx, plainText, strlen((char*)plainText));  
    int err = EVP_VerifyFinal (&md_ctx, cipherText, cipherTextLen, pkey);  
    EVP_PKEY_free (pkey);  
  
    if (err != 1) {  
        ERR_print_errors_fp (stderr);  
        return 1;  
    }  
    return 0;  
}  
  
int main(int argc, char* argv[])  
{  
    char certFile[] = "cert.pem";//含共匙  
    char keyFile[]  = "key.pem";//含私匙  
  
    char plainText[]     = "I owe you...";//待签名的明文  
    unsigned char cipherText[1024];  
    unsigned int cipherTextLen;  
  
    InitOpenSSL();  
  
    memset(cipherText,0,sizeof(cipherText));  
    if(NULL==GetSign(keyFile,plainText,cipherText,&cipherTextLen))  
    {  
        printf("签名失败！\n");  
        return -1;  
    }  
  
    if(1==VerifySign(certFile,cipherText,cipherTextLen,plainText))  
    {  
        printf("验证签名失败！\n");  
        return -2;  
    }  
  
  
    printf ("Signature Verified Ok.\n");  
    return 0;  
}  


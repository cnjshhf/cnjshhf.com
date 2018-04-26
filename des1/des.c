/*************************************************************************
 * Module     : Data Encryption Algorithm.
 * Description: DES/3DES APIs.
 * -----------------------------------------------------------------------
 * APIs       : int Ek() --Encryption using Data Encryption Algorithm.
 *            : int Dk() --Decryption using Data Encryption Algorithm.
 *************************************************************************/
#include <string.h>
#include "des.h"

/* Initial Permutation Table */
static short des_tbl_IP[64] = {
  58, 50, 42, 34, 26, 18, 10, 2,
  60, 52, 44, 36, 28, 20, 12, 4,
  62, 54, 46, 38, 30, 22, 14, 6,
  64, 56, 48, 40, 32, 24, 16, 8,
  57, 49, 41, 33, 25, 17, 9,  1,
  59, 51, 43, 35, 27, 19, 11, 3,
  61, 53, 45, 37, 29, 21, 13, 5,
  63, 55, 47, 39, 31, 23, 15, 7
};

/* Final Permutation Table */
static short des_tbl_FP[64] = {
  40, 8, 48, 16, 56, 24, 64, 32,
  39, 7, 47, 15, 55, 23, 63, 31,
  38, 6, 46, 14, 54, 22, 62, 30,
  37, 5, 45, 13, 53, 21, 61, 29,
  36, 4, 44, 12, 52, 20, 60, 28,
  35, 3, 43, 11, 51, 19, 59, 27,
  34, 2, 42, 10, 50, 18, 58, 26,
  33, 1, 41,  9, 49, 17, 57, 25
};

/* Permuted Choice Table 1 */
static short des_tbl_PC1[56] = {
  57, 49, 41, 33, 25, 17,  9,
   1, 58, 50, 42, 34, 26, 18,
  10,  2, 59, 51, 43, 35, 27,
  19, 11,  3, 60, 52, 44, 36,
  63, 55, 47, 39, 31, 23, 15,
   7, 62, 54, 46, 38, 30, 22,
  14,  6, 61, 53, 45, 37, 29,
  21, 13,  5, 28, 20, 12,  4
};

/* Permuted Choice Table 2 */
static short des_tbl_PC2[48] = {
  14, 17, 11, 24,  1,  5,
   3, 28, 15,  6, 21, 10,
  23, 19, 12,  4, 26,  8,
  16,  7, 27, 20, 13,  2,
  41, 52, 31, 37, 47, 55,
  30, 40, 51, 45, 33, 48,
  44, 49, 39, 56, 34, 53,
  46, 42, 50, 36, 29, 32
};

/* Shift Number Table */
static short des_tbl_SN[16] = {1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1};

/* Expansion Function EBOX */
static short des_tbl_EBOX[48] = {
  32,  1,  2,  3,  4,  5,  4,  5,  6,  7,  8,  9,
   8,  9, 10, 11, 12, 13, 12, 13, 14, 15, 16, 17,
  16, 17, 18, 19, 20, 21, 20, 21, 22, 23, 24, 25,
  24, 25, 26, 27, 28, 29, 28, 29, 30, 31, 32,  1
};

/* Permutation Function PBOX */
static short des_tbl_PBOX[32] = {
  16,  7, 20, 21, 29, 12, 28, 17,
   1, 15, 23, 26,  5, 18, 31, 10,
   2,  8, 24, 14, 32, 27,  3,  9,
  19, 13, 30,  6, 22, 11,  4, 25
};

/* Selection Function SBOX1-8 */
static unsigned char des_tbl_SBOX1[64] = {
  14,  4, 13,  1,  2, 15, 11,  8,  3, 10,  6, 12,  5,  9,  0,  7,
   0, 15,  7,  4, 14,  2, 13,  1, 10,  6, 12, 11,  9,  5,  3,  8,
   4,  1, 14,  8, 13,  6,  2, 11, 15, 12,  9,  7,  3, 10,  5,  0,
  15, 12,  8,  2,  4,  9,  1,  7,  5, 11,  3, 14, 10,  0,  6, 13
};

static unsigned char des_tbl_SBOX2[64] = {
  15,  1,  8, 14,  6, 11,  3,  4,  9,  7,  2, 13, 12,  0,  5, 10,
   3, 13,  4,  7, 15,  2,  8, 14, 12,  0,  1, 10,  6,  9, 11,  5,
   0, 14,  7, 11, 10,  4, 13,  1,  5,  8, 12,  6,  9,  3,  2, 15,
  13,  8, 10,  1,  3, 15,  4,  2, 11,  6,  7, 12,  0,  5, 14,  9
};

static unsigned char des_tbl_SBOX3[64] = {
  10,  0,  9, 14,  6,  3, 15,  5,  1, 13, 12,  7, 11,  4,  2,  8,
  13,  7,  0,  9,  3,  4,  6, 10,  2,  8,  5, 14, 12, 11, 15,  1,
  13,  6,  4,  9,  8, 15,  3,  0, 11,  1,  2, 12,  5, 10, 14,  7,
   1, 10, 13,  0,  6,  9,  8,  7,  4, 15, 14,  3, 11,  5,  2, 12
};

static unsigned char des_tbl_SBOX4[64] = {
   7, 13, 14,  3,  0,  6,  9, 10,  1,  2,  8,  5, 11, 12,  4, 15,
  13,  8, 11,  5,  6, 15,  0,  3,  4,  7,  2, 12,  1, 10, 14,  9,
  10,  6,  9,  0, 12, 11,  7, 13, 15,  1,  3, 14,  5,  2,  8,  4,
   3, 15,  0,  6, 10,  1, 13,  8,  9,  4,  5, 11, 12,  7,  2, 14
};

static unsigned char des_tbl_SBOX5[64] = {
   2, 12,  4,  1,  7, 10, 11,  6,  8,  5,  3, 15, 13,  0, 14,  9,
  14, 11,  2, 12,  4,  7, 13,  1,  5,  0, 15, 10,  3,  9,  8,  6,
   4,  2,  1, 11, 10, 13,  7,  8, 15,  9, 12,  5,  6,  3,  0, 14,
  11,  8, 12,  7,  1, 14,  2, 13,  6,  15,  0,  9, 10,  4,  5, 3
};

static unsigned char des_tbl_SBOX6[64] = {
  12,  1, 10, 15,  9,  2,  6,  8,  0, 13,  3,  4, 14,  7,  5, 11,
  10, 15,  4,  2,  7, 12,  9,  5,  6,  1, 13, 14,  0, 11,  3,  8,
   9, 14, 15,  5,  2,  8, 12,  3,  7,  0,  4, 10,  1, 13, 11,  6,
   4,  3,  2, 12,  9,  5, 15, 10, 11, 14,  1,  7,  6,  0,  8, 13
};

static unsigned char des_tbl_SBOX7[64] = {
   4, 11,  2, 14, 15,  0,  8, 13,  3, 12,  9,  7,  5, 10,  6,  1,
  13,  0, 11,  7,  4,  9,  1, 10, 14,  3,  5, 12,  2, 15,  8,  6,
   1,  4, 11, 13, 12,  3,  7, 14, 10, 15,  6,  8,  0,  5,  9,  2,
   6, 11, 13,  8,  1,  4, 10,  7,  9,  5,  0, 15, 14,  2,  3, 12
};

static unsigned char des_tbl_SBOX8[64] = {
  13,  2,  8,  4,  6, 15, 11,  1, 10,  9,  3, 14,  5,  0, 12,  7,
   1, 15, 13,  8, 10,  3,  7,  4, 12,  5,  6, 11,  0, 14,  9,  2,
   7, 11,  4,  1,  9, 12, 14,  2,  0,  6, 10, 13, 15,  3,  5,  8,
   2,  1, 14,  7,  4, 10,  8, 13, 15, 12,  9,  0,  3,  5,  6, 11
};

/* Local functions */
static void des_fun_KS(unsigned char *aKey, unsigned char aKn[][6]);
static void des_fun_F(unsigned char *aInputRi, unsigned char *aSubKi, unsigned char *aOutputTmpLi);

/* APIs for users */
void Ek(unsigned char *aPlainText, unsigned char *aKey, unsigned char *aEncipher);
void Dk(unsigned char *aEncipher, unsigned char *aKey, unsigned char *aPlainText);

/*****************************************************************************
 * Function    : Ek - Encryption using Data Encryption Algorithm.
 * Description : Input a 64-bit block(R) of plaintext, and a 64-bit key,
 *               yield a 64-bit cipher text.
 *               NOTE NOT CHECK INPUT.
 * Input       : unsigned char *aPlainText --64 bits of plaintext
 *               unsigned char *aKey --64 bits of key
 * Output      : unsigned char *aEncipher --a 64-bit encipher text
 *****************************************************************************/
void Ek(unsigned char *aPlainText, unsigned char *aKey, unsigned char *aEncipher)
{
  int           i, j;
  unsigned char alTmpText[8];
  unsigned char alLi[4], alRi[4], alTmpLi[4];
  unsigned char alKn[16][6];

  for (i=0; i<64; i++) {
    j = i >> 3;
    alTmpText[j] = (alTmpText[j]<<1) | ((aPlainText[(des_tbl_IP[i]-1)>>3]>>((8-des_tbl_IP[i]%8)%8)) & 0x01);
  }
  /* Create L0, R0 */
  memcpy(alLi, alTmpText, 4);
  memcpy(alRi, alTmpText+4, 4);

  des_fun_KS(aKey, alKn);

  for (i=0; i<16; i++) {

    des_fun_F(alRi, alKn[i], alTmpLi);

    for (j=0; j<4; j++) alTmpLi[j] = alTmpLi[j] ^ alLi[j];

    memcpy(alLi, alRi, 4);
    memcpy(alRi, alTmpLi, 4);
  }
  memcpy(alTmpText, alRi, 4);
  memcpy(alTmpText+4, alLi, 4);

  for (i=0; i<64; i++) {
    j = i >> 3;
    aEncipher[j] = (aEncipher[j]<<1) | ((alTmpText[(des_tbl_FP[i]-1)>>3]>>((8-des_tbl_FP[i]%8)%8)) & 0x01);
  }

  return;
}

/* ****************************************************************************
 * Function    : Dk - Decryption using Data Encryption Algorithm.
 * Description : Input a 64-bit block(R) of cipher text, and a 64-bit key,
 *               yield a 64-bit plaintext.
 *               NOTE NOT CHECK INPUT.
 * Input       : unsigned char *aEncipher --64 bits of cipher text
 *               unsigned char *aKey --64 bits of key
 * Output      : unsigned char *aPlainText --a 64-bit plaintext
 *****************************************************************************/
void Dk(unsigned char *aEncipher, unsigned char *aKey, unsigned char *aPlainText)
{
  int           i, j;
  unsigned char alTmpText[8];
  unsigned char alLi[4], alRi[4], alTmpLi[4];
  unsigned char alKn[16][6];

  for (i=0; i<64; i++) {
    j = i >> 3;
    alTmpText[j] = (alTmpText[j]<<1) | ((aEncipher[(des_tbl_IP[i]-1)>>3]>>((8-des_tbl_IP[i]%8)%8)) & 0x01);
  }
  memcpy(alLi, alTmpText, 4);
  memcpy(alRi, alTmpText+4, 4);

  des_fun_KS(aKey, alKn);

  for (i=0; i<16; i++) {

    des_fun_F(alRi, alKn[15-i], alTmpLi);

    for (j=0; j<4; j++) alTmpLi[j] = alTmpLi[j] ^ alLi[j];

    memcpy(alLi, alRi, 4);
    memcpy(alRi, alTmpLi, 4);
  }
  memcpy(alTmpText, alRi, 4);
  memcpy(alTmpText+4, alLi, 4);

  for (i=0; i<64; i++) {
    j = i >> 3;
    aPlainText[j] = (aPlainText[j]<<1) | ((alTmpText[(des_tbl_FP[i]-1)>>3]>>((8-des_tbl_FP[i]%8)%8)) & 0x01);
  }

  return;
}

/* ****************************************************************************
 * Function    : des_fun_KS - Function KS.
 * Description : Input a 64-bit block of key, applying permuted choice 1 then
 *               yield a 56-bit C0D0. key(K), C0D0 left shift and applying permuted
 *               choice 2, yield 48-bit key Kn,  for n = 1, 2, ..., 16.
 * Input       : unsigned char *aKey --64 bits of key
 * Output      : unsigned char aKn[16][6] --a pointer that indicates a 48-element
 *               array of key
 *****************************************************************************/
static void des_fun_KS(unsigned char *aKey, unsigned char aKn[][6])
{
  int           i, j, k;
  short         nlShfit;
  unsigned char alCD[7], alCi[4], alDi[4];
  unsigned char alKi[7];
  unsigned char clTmpShfit, clTmp;

  for (i=0; i<56; i++) {
    j = i >> 3;
    alCD[j] = (alCD[j]<<1) | ((aKey[(des_tbl_PC1[i]-1)>>3]>>((8-des_tbl_PC1[i]%8)%8)) & 0x01);
  }
  memcpy(alCi, alCD, 3);
  alCi[3] = alCD[3] & 0xF0;
  alDi[0] = (alCD[3]<<4) | (alCD[4]>>4);
  alDi[1] = (alCD[4]<<4) | (alCD[5]>>4);
  alDi[2] = (alCD[5]<<4) | (alCD[6]>>4);
  alDi[3] = alCD[6] << 4;

  for (i=0; i<16; i++) {

    nlShfit = des_tbl_SN[i];

    for (clTmpShfit=0, j=0; j<nlShfit; j++)
      clTmpShfit = (clTmpShfit<<1) | 0x01; /* Eg: nlShfit=3 -> 00000111 */

    clTmp = alCi[0];
    alCi[0] = (alCi[0]<<nlShfit) | ((alCi[1]>>(8-nlShfit))&clTmpShfit);
    alCi[1] = (alCi[1]<<nlShfit) | ((alCi[2]>>(8-nlShfit))&clTmpShfit);
    alCi[2] = (alCi[2]<<nlShfit) | ((alCi[3]>>(8-nlShfit))&clTmpShfit);
    alCi[3] = (alCi[3]<<nlShfit) | ((clTmp>>(4-nlShfit))&(clTmpShfit<<4));

    clTmp = alDi[0];
    alDi[0] = (alDi[0]<<nlShfit) | ((alDi[1]>>(8-nlShfit))&clTmpShfit);
    alDi[1] = (alDi[1]<<nlShfit) | ((alDi[2]>>(8-nlShfit))&clTmpShfit);
    alDi[2] = (alDi[2]<<nlShfit) | ((alDi[3]>>(8-nlShfit))&clTmpShfit);
    alDi[3] = (alDi[3]<<nlShfit) | ((clTmp>>(4-nlShfit))&(clTmpShfit<<4));

    /* Ci + Di -> 56bit Ki */
    alKi[0] = alCi[0];
    alKi[1] = alCi[1];
    alKi[2] = alCi[2];
    alKi[3] = (alCi[3]&0xF0) | (alDi[0]>>4);
    alKi[4] = (alDi[0]<<4) | (alDi[1]>>4);
    alKi[5] = (alDi[1]<<4) | (alDi[2]>>4);
    alKi[6] = (alDi[2]<<4) | (alDi[3]>>4);

    /* PC2 */
    for (j=0; j<48; j++) {
      k = j >> 3;
      aKn[i][k] = (aKn[i][k]<<1) | ((alKi[(des_tbl_PC2[j]-1)>>3]>>((8-des_tbl_PC2[j]%8)%8)) & 0x01);
    }
  }

  return;
}

/* ****************************************************************************
 * Function    : des_fun_F - Function Cipher.
 * Description : Input a 32-bit block(R) of rignt plaintext, and a 48-bit key(K)
 *               let B1B2B3B4B5B6B7B8 = K Xor E(R)
 *               out = P(S1(B1)S2(B2)S3(B3)S4(B4)S5(B5)S6(B6)S7(B7)S8(B8))
 * Input       : unsigned char *aInputRi --the right 32 bits of plaintext
 *               unsigned char *aSubKi --a pointer that indicates a key of 48 bits
 * Output      : unsigned char *aOutputTmpLi --a 32 bits output
 *****************************************************************************/
static void des_fun_F(unsigned char *aInputRi, unsigned char *aSubKi, unsigned char *aOutputTmpLi)
{
  int      i, j;
  unsigned char alTmp[6];
  unsigned char alBlockSel[8];

  /* E(R): 32-bit -> 48-bit */
  for (i=0; i<48; i++) {
    j = i >> 3;
    alTmp[j] = (alTmp[j]<<1) | ((aInputRi[(des_tbl_EBOX[i]-1)>>3]>>((8-des_tbl_EBOX[i]%8)%8)) & 0x01);
  }

  for (i=0; i<6; i++) alTmp[i] = alTmp[i] ^ aSubKi[i];

  memset(alBlockSel, 0x00, sizeof(alBlockSel));
  alBlockSel[0] = alTmp[0] >> 2;
  alBlockSel[1] = ((alTmp[0]&0x03)<<4) | (alTmp[1]>>4);
  alBlockSel[2] = ((alTmp[1]&0x0F)<<2) | (alTmp[2]>>6);
  alBlockSel[3] = alTmp[2] & 0x3F;
  alBlockSel[4] = alTmp[3] >> 2;
  alBlockSel[5] = ((alTmp[3]&0x03)<<4) | (alTmp[4]>>4);
  alBlockSel[6] = ((alTmp[4]&0x0F)<<2) | (alTmp[5]>>6);
  alBlockSel[7] = alTmp[5] & 0x3F;
  for (i=0; i<8; i++)
    alBlockSel[i] = (alBlockSel[i]&0x20) | ((alBlockSel[i]&0x01)<<4) | ((alBlockSel[i]>>1)&0x0F);

  /* Get 32-bit output from selection function */
  alTmp[0] = (des_tbl_SBOX1[alBlockSel[0]]<<4) | des_tbl_SBOX2[alBlockSel[1]];
  alTmp[1] = (des_tbl_SBOX3[alBlockSel[2]]<<4) | des_tbl_SBOX4[alBlockSel[3]];
  alTmp[2] = (des_tbl_SBOX5[alBlockSel[4]]<<4) | des_tbl_SBOX6[alBlockSel[5]];
  alTmp[3] = (des_tbl_SBOX7[alBlockSel[6]]<<4) | des_tbl_SBOX8[alBlockSel[7]];

  /* From a 32-bit input, and permutation function P,  yields a 32-bit output */
  for (i=0; i<32; i++) {
    j = i >> 3;
    aOutputTmpLi[j] = (aOutputTmpLi[j]<<1) | ((alTmp[(des_tbl_PBOX[i]-1)>>3]>>((8-des_tbl_PBOX[i]%8)%8)) & 0x01);
  }

  return;
}


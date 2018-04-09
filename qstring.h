#ifndef QSTRING_H
#define QSTRING_H

#define LOG1(...) LOG(__FILE__,__LINE__,__VA_ARGS__)

extern char LogName[100];
 
/* 拼接多个字符串
 *  * argc: the number of strings
 *   */
extern char * strcat2(int argc, const char *str1, const char * str2, ...);

/* 剪切字符串，不重新分配内存 */
extern char * strim(char *str);

/* 分割字符串
 *  * count: 分割后的字符串长度
 *   * limit: 分割多少次
 *    */
extern char ** strsplit(char *line, char delimeter, int *count, int limit);

/* 把字符串yes或no转换成整形, 错误返回-1 
 *  */
 int yesnotoi(char *str);

extern void LOG(char *fl,int li,const char* ms, ...);

extern int setLogName(char *str);

extern char *strRepl(char *s, const char *s1, const char *s2);

extern int regxmatch(char *bematch,char *pattern);

extern void FreeMem(char **pp,int num);

extern char **getMem(int num);

#endif


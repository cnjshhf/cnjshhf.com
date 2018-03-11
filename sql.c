/***************************************************************
 * 数据库操作函数(基于SQLite)
 **************************************************************/
#include "log.h"
#include "sqlite3.h"

#define RETRY_NMAX  1000
#define RETRY_INTV  1000  /*1ms*/

/***************************************************************
 * 打开数据库
 **************************************************************/
int sqliteOpen(char *fileName, sqlite3 **ppdb)
{
    int rc;

    if (fileName == NULL) {
        rc = sqlite3_open_v2(":memory:", ppdb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    } else {
        rc = sqlite3_open_v2(fileName, ppdb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    }
    if (rc != SQLITE_OK) {
        LOG_ERR("SQLite: %s", sqlite3_errmsg(*ppdb));
        sqlite3_close_v2(*ppdb);
        *ppdb = NULL;
        return -1;
    }
    return 0;
}

/***************************************************************
 * 关闭数据库
 **************************************************************/
int sqliteClose(sqlite3 *pdb)
{
    if (sqlite3_close_v2(pdb) != SQLITE_OK) {
        LOG_ERR("SQLite: %s", sqlite3_errmsg(pdb));
        return -1;
    }
    return 0;
}

/***************************************************************
 * 检查数据库对象是否存在
 **************************************************************/
int sqliteObjectExist(sqlite3 *pdb, char *objectType, char *objectName)
{
    char sql[256];
    sqlite3_stmt *pStmt;

    snprintf(sql, sizeof(sql), "select count(*) from sqlite_master where type='%s' and name='%s'", objectType, objectName);
    if (sqlite3_prepare_v2(pdb, sql, strlen(sql), &pStmt, NULL) != SQLITE_OK) {
        LOG_ERR("SQLite: %s", sqlite3_errmsg(pdb));
        return 0;
    }
    if (sqlite3_step(pStmt) != SQLITE_ROW) {
        sqlite3_finalize(pStmt);
        return 0;
    }
    if (sqlite3_column_int(pStmt, 0) <= 0) {
        sqlite3_finalize(pStmt);
        return 0;
    }
    sqlite3_finalize(pStmt);
    return 1;
}

/***************************************************************
 * 执行SQL语句
 **************************************************************/
int sqliteExecute(sqlite3 *pdb, char *sql)
{
    int i,rc;
    sqlite3_stmt *pStmt;

    for (i=0; i<RETRY_NMAX && (rc = sqlite3_prepare_v2(pdb, sql, strlen(sql), &pStmt, NULL)) == SQLITE_BUSY; i++) usleep(RETRY_INTV);
    if (rc != SQLITE_OK) {
        LOG_ERR("SQLite: %s", sqlite3_errmsg(pdb));
        return -1;
    }

    for (i=0; i<RETRY_NMAX && (rc = sqlite3_step(pStmt)) == SQLITE_BUSY; i++) usleep(RETRY_INTV);
    switch (rc) {
        case SQLITE_DONE:
            sqlite3_finalize(pStmt);
              return 0;
        case SQLITE_ROW:
            LOG_ERR("SQLite: The query statment not be expected");
            break;
        default:
            LOG_ERR("SQLite: %s", sqlite3_errmsg(pdb));
            break;
    }
    sqlite3_finalize(pStmt);
    return -1;
}

/***************************************************************
 * 准备SQL语句
 **************************************************************/
int sqlitePrepare(sqlite3 *pdb, char *sql, sqlite3_stmt **ppStmt)
{
    int i,rc;

    for (i=0; i<RETRY_NMAX && (rc = sqlite3_prepare_v2(pdb, sql, strlen(sql), ppStmt, NULL)) == SQLITE_BUSY; i++) usleep(RETRY_INTV);
    if (rc != SQLITE_OK) {
        LOG_ERR("SQLite: %s", sqlite3_errmsg(pdb));
        return -1;
    }
    return 0;
}

/***************************************************************
 * 重置SQL语句
 **************************************************************/
int sqliteReset(sqlite3_stmt *pStmt)
{
    if (sqlite3_reset(pStmt) != SQLITE_OK) {
        sqlite3 *pdb = sqlite3_db_handle(pStmt);
        LOG_ERR("SQLite: %s", sqlite3_errmsg(pdb));
        return -1;
    }
    return 0;
}

/***************************************************************
 * 释放SQL语句
 **************************************************************/
int sqliteRelease(sqlite3_stmt *pStmt)
{
    if (sqlite3_finalize(pStmt) != SQLITE_OK) {
        sqlite3 *pdb = sqlite3_db_handle(pStmt);
        LOG_ERR("SQLite: %s", sqlite3_errmsg(pdb));
        return -1;
    }
    return 0;
}

/***************************************************************
 * 绑定SQL语句变量(整型)
 **************************************************************/
int sqliteBindInt(sqlite3_stmt *pStmt, char *name, int value)
{
    int index;
    
    if ((index = sqlite3_bind_parameter_index(pStmt, name)) <= 0) {
        LOG_ERR("SQLite: no matching parameter[%s] is found", name);
        return -1;
    }
    if (sqlite3_bind_int(pStmt, index, value) != SQLITE_OK) {
        sqlite3 *pdb = sqlite3_db_handle(pStmt);
        LOG_ERR("SQLite: %s", sqlite3_errmsg(pdb));
        return -1;
    }
    return 0;
}

/***************************************************************
 * 绑定SQL语句变量(数值型)
 **************************************************************/
int sqliteBindDouble(sqlite3_stmt *pStmt, char *name, double value)
{
    int index;
    
    if ((index = sqlite3_bind_parameter_index(pStmt, name)) <= 0) {
        LOG_ERR("SQLite: no matching parameter[%s] is found", name);
        return -1;
    }
    if (sqlite3_bind_double(pStmt, index, value) != SQLITE_OK) {
        sqlite3 *pdb = sqlite3_db_handle(pStmt);
        LOG_ERR("SQLite: %s", sqlite3_errmsg(pdb));
        return -1;
    }
    return 0;
}

/***************************************************************
 * 绑定SQL语句变量(字符串型)
 **************************************************************/
int sqliteBindString(sqlite3_stmt *pStmt, char *name, char *value)
{
    int index;
    
    if ((index = sqlite3_bind_parameter_index(pStmt, name)) <= 0) {
        LOG_ERR("SQLite: no matching parameter[%s] is found", name);
        return -1;
    }
    if (sqlite3_bind_text(pStmt, index, value, strlen(value), SQLITE_STATIC) != SQLITE_OK) {
        sqlite3 *pdb = sqlite3_db_handle(pStmt);
        LOG_ERR("SQLite: %s", sqlite3_errmsg(pdb));
        return -1;
    }
    return 0;
}

/***************************************************************
 * 绑定SQL语句变量(BLOB)
 **************************************************************/
int sqliteBindBlob(sqlite3_stmt *pStmt, char *name, char *value, int length)
{
    int index;
    
    if ((index = sqlite3_bind_parameter_index(pStmt, name)) <= 0) {
        LOG_ERR("SQLite: no matching parameter[%s] is found", name);
        return -1;
    }
    if (sqlite3_bind_blob(pStmt, index, value, length, SQLITE_STATIC) != SQLITE_OK) {
        sqlite3 *pdb = sqlite3_db_handle(pStmt);
        LOG_ERR("SQLite: %s", sqlite3_errmsg(pdb));
        return -1;
    }
    return 0;
}

/***************************************************************
 * 提取SQL结果集记录
 **************************************************************/
int sqliteFetch(sqlite3_stmt *pStmt)
{
    int i,rc;
    struct sqlite3 *pdb;

    for (i=0; i<RETRY_NMAX && (rc = sqlite3_step(pStmt)) == SQLITE_BUSY; i++) usleep(RETRY_INTV);
    switch (rc) {
        case SQLITE_ROW:
        case SQLITE_DONE:
            return rc;
        default:
            pdb = sqlite3_db_handle(pStmt);
            LOG_ERR("SQLite: %s", sqlite3_errmsg(pdb));
            return -1;
    }
}

/***************************************************************
 * 提取当前记录的字符串型列值
 **************************************************************/
char *sqliteGetString(sqlite3_stmt *pStmt, int index, char *buf, size_t size)
{
    char *str;

    if ((str = (char *)sqlite3_column_text(pStmt, index)) == NULL) return NULL;
    if (str == NULL || buf == NULL || size <= 0) return str;
    strncpy(buf, str, size-1);
    return buf;
}

/***************************************************************
 * 提取当前记录的整型列值
 **************************************************************/
int sqliteGetInt(sqlite3_stmt *pStmt, int index)
{
    return sqlite3_column_int(pStmt, index);
}

/***************************************************************
 * 提取当前记录的长整型列值
 **************************************************************/
long sqliteGetLong(sqlite3_stmt *pStmt, int index)
{
    char *str;

    str = sqliteGetString(pStmt, index, NULL, 0);
    return ((!str)? 0:atol(str));
}

/***************************************************************
 * 提取当前记录的数值型列值
 **************************************************************/
double sqliteGetDouble(sqlite3_stmt *pStmt, int index)
{
    return sqlite3_column_double(pStmt, index);
}


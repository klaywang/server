/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/***************************************************************************
 This is a test sample to test the new features in MySQL client-server
 protocol

 Main author: venu ( venu@mysql.com )

 NOTES:
  - To be able to test which fields are used, we are not clearing
    the MYSQL_BIND with bzero() but instead just clearing the fields that
    are used by the API.

***************************************************************************/

#include <my_global.h>
#include <my_sys.h>
#include <mysql.h>
#include <my_getopt.h>
#include <m_string.h>
#include <assert.h>


#define MAX_TEST_QUERY_LENGTH 300 /* MAX QUERY BUFFER LENGTH */

/* set default options */
static char *opt_db=0;
static char *opt_user=0;
static char *opt_password=0;
static char *opt_host=0;
static char *opt_unix_socket=0;
static unsigned int  opt_port;
static my_bool tty_password=0;

static MYSQL *mysql=0;
static char query[MAX_TEST_QUERY_LENGTH]; 
static char current_db[]= "client_test_db";
static unsigned int test_count= 0;
static unsigned int opt_count= 0;
static unsigned int iter_count= 0;

static time_t start_time, end_time;
static double total_time;

const char *default_dbug_option="d:t:o,/tmp/client_test.trace";

#define myheader(str) \
{ \
  fprintf(stdout,"\n\n#####################################\n"); \
  fprintf(stdout,"%d of (%d/%d): %s",test_count++, iter_count,\
                                     opt_count, str); \
  fprintf(stdout,"  \n#####################################\n"); \
}
#define myheader_r(str) \
{ \
  fprintf(stdout,"\n\n#####################################\n"); \
  fprintf(stdout,"%s", str); \
  fprintf(stdout,"  \n#####################################\n"); \
}

static void print_error(const char *msg);
static void print_st_error(MYSQL_STMT *stmt, const char *msg);
static void client_disconnect();

#define myerror(msg) print_error(msg)
#define mysterror(stmt, msg) print_st_error(stmt, msg)

#define myquery(r) \
{ \
if (r) \
  myerror(NULL); \
 assert(r == 0); \
}

#define myquery_r(r) \
{ \
if (r) \
  myerror(NULL); \
assert(r != 0); \
}

#define check_execute(stmt,r) \
{ \
if (r) \
  mysterror(stmt,NULL); \
assert(r == 0);\
}

#define check_execute_r(stmt,r) \
{ \
if (r) \
  mysterror(stmt,NULL); \
assert(r != 0);\
}

#define check_stmt(stmt) \
{ \
if ( stmt == 0) \
  myerror(NULL); \
assert(stmt != 0); \
}

#define check_stmt_r(stmt) \
{ \
if (stmt == 0) \
  myerror(NULL);\
assert(stmt == 0);\
} 

#define mytest(x) if (!x) {myerror(NULL);assert(TRUE);}
#define mytest_r(x) if (x) {myerror(NULL);assert(TRUE);}

/********************************************************
* print the error message                               *
*********************************************************/
static void print_error(const char *msg)
{  
  if (mysql && mysql_errno(mysql))
  {
    if (mysql->server_version)
      fprintf(stdout,"\n [MySQL-%s]",mysql->server_version);
    else
      fprintf(stdout,"\n [MySQL]");
    fprintf(stdout,"[%d] %s\n",mysql_errno(mysql),mysql_error(mysql));
  }
  else if (msg) fprintf(stderr, " [MySQL] %s\n", msg);
}

static void print_st_error(MYSQL_STMT *stmt, const char *msg)
{  
  if (stmt && mysql_stmt_errno(stmt))
  {
    if (stmt->mysql && stmt->mysql->server_version)
      fprintf(stdout,"\n [MySQL-%s]",stmt->mysql->server_version);
    else
      fprintf(stdout,"\n [MySQL]");

    fprintf(stdout,"[%d] %s\n",mysql_stmt_errno(stmt),
            mysql_stmt_error(stmt));
  }
  else if (msg) fprintf(stderr, " [MySQL] %s\n", msg);
}

/*
  This is to be what mysql_query() is for mysql_real_query(), for
  mysql_prepare(): a variant without the 'length' parameter.
*/
MYSQL_STMT *STDCALL
mysql_simple_prepare(MYSQL  *mysql, const char *query)
{
  MYSQL_STMT *stmt= mysql_stmt_init(mysql);
  if (mysql_stmt_prepare(stmt, query, strlen(query)))
    return 0;
  return stmt;
}


/********************************************************
* connect to the server                                 *
*********************************************************/
static void client_connect()
{
  int  rc;
  myheader_r("client_connect");  

  fprintf(stdout, "\n Establishing a connection to '%s' ...", opt_host);
  
  if (!(mysql = mysql_init(NULL)))
  { 
    myerror("mysql_init() failed");
    exit(0);
  }
  
  if (!(mysql_real_connect(mysql,opt_host,opt_user,
                           opt_password, opt_db ? opt_db:"test", opt_port,
                           opt_unix_socket, 0)))
  {
    myerror("connection failed");    
    mysql_close(mysql);
    fprintf(stdout,"\n Check the connection options using --help or -?\n");
    exit(0);
  }    
  
  fprintf(stdout," OK");

  /* set AUTOCOMMIT to ON*/
  mysql_autocommit(mysql, TRUE);
  
  fprintf(stdout, "\n Creating a test database '%s' ...", current_db);
  strxmov(query,"CREATE DATABASE IF NOT EXISTS ", current_db, NullS);
  
  rc = mysql_query(mysql, query);
  myquery(rc);
  
  strxmov(query,"USE ", current_db, NullS);
  rc = mysql_query(mysql, query);
  myquery(rc);
  
  fprintf(stdout," OK");
}

/********************************************************
* close the connection                                  *
*********************************************************/
static void client_disconnect()
{  
  myheader_r("client_disconnect");  

  if (mysql)
  {
    fprintf(stdout, "\n droping the test database '%s' ...", current_db);
    strxmov(query,"DROP DATABASE IF EXISTS ", current_db, NullS);
    
    mysql_query(mysql, query);
    fprintf(stdout, " OK");
    
    fprintf(stdout, "\n closing the connection ...");
    mysql_close(mysql);
    fprintf(stdout, " OK\n");
  }
}

/********************************************************
* query processing                                      *
*********************************************************/
static void client_query()
{
  int rc;

  myheader("client_query");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS myclient_test");
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE myclient_test(id int primary key auto_increment,\
                                              name varchar(20))");
  myquery(rc);
  
  rc = mysql_query(mysql,"CREATE TABLE myclient_test(id int, name varchar(20))");
  myquery_r(rc);
  
  rc = mysql_query(mysql,"INSERT INTO myclient_test(name) VALUES('mysql')");
  myquery(rc);
  
  rc = mysql_query(mysql,"INSERT INTO myclient_test(name) VALUES('monty')");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO myclient_test(name) VALUES('venu')");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO myclient_test(name) VALUES('deleted')");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO myclient_test(name) VALUES('deleted')");
  myquery(rc);

  rc = mysql_query(mysql,"UPDATE myclient_test SET name='updated' WHERE name='deleted'");
  myquery(rc);

  rc = mysql_query(mysql,"UPDATE myclient_test SET id=3 WHERE name='updated'");
  myquery_r(rc);
}

/********************************************************
* print dashes                                          *
*********************************************************/
static void my_print_dashes(MYSQL_RES *result)
{
  MYSQL_FIELD  *field;
  unsigned int i,j;

  mysql_field_seek(result,0);
  fputc('\t',stdout);
  fputc('+', stdout);

  for(i=0; i< mysql_num_fields(result); i++)
  {
    field = mysql_fetch_field(result);
    for(j=0; j < field->max_length+2; j++)
      fputc('-',stdout);
    fputc('+',stdout);
  }
  fputc('\n',stdout);
}

/********************************************************
* print resultset metadata information                  *
*********************************************************/
static void my_print_result_metadata(MYSQL_RES *result)
{
  MYSQL_FIELD  *field;
  unsigned int i,j;
  unsigned int field_count;

  mysql_field_seek(result,0);
  fputc('\n', stdout);
  fputc('\n', stdout);

  field_count = mysql_num_fields(result);
  for(i=0; i< field_count; i++)
  {
    field = mysql_fetch_field(result);
    j = strlen(field->name);
    if (j < field->max_length)
      j = field->max_length;
    if (j < 4 && !IS_NOT_NULL(field->flags))
      j = 4;
    field->max_length = j;
  }
  my_print_dashes(result);
  fputc('\t',stdout);
  fputc('|', stdout);

  mysql_field_seek(result,0);
  for(i=0; i< field_count; i++)
  {
    field = mysql_fetch_field(result);
    fprintf(stdout, " %-*s |",(int) field->max_length, field->name);
  }
  fputc('\n', stdout);
  my_print_dashes(result);
}

/********************************************************
* process the result set                                *
*********************************************************/
int my_process_result_set(MYSQL_RES *result)
{
  MYSQL_ROW    row;
  MYSQL_FIELD  *field;
  unsigned int i;
  unsigned int row_count=0;
  
  if (!result)
    return 0;

  my_print_result_metadata(result);

  while ((row = mysql_fetch_row(result)) != NULL)
  {
    mysql_field_seek(result,0);
    fputc('\t',stdout);
    fputc('|',stdout);

    for(i=0; i< mysql_num_fields(result); i++)
    {
      field = mysql_fetch_field(result);
      if (row[i] == NULL)
        fprintf(stdout, " %-*s |", (int) field->max_length, "NULL");
      else if (IS_NUM(field->type))
        fprintf(stdout, " %*s |", (int) field->max_length, row[i]);
      else
        fprintf(stdout, " %-*s |", (int) field->max_length, row[i]);
    }
    fputc('\t',stdout);
    fputc('\n',stdout);
    row_count++;
  }
  if (row_count) 
    my_print_dashes(result);

  if (mysql_errno(mysql) != 0)
    fprintf(stderr, "\n\tmysql_fetch_row() failed\n");
  else
    fprintf(stdout,"\n\t%d %s returned\n", row_count, 
                   row_count == 1 ? "row" : "rows");
  return row_count;
}

int my_process_result(MYSQL *mysql)
{
  MYSQL_RES *result;
  int       row_count;

  if (!(result = mysql_store_result(mysql)))
    return 0;
  
  row_count= my_process_result_set(result);
  
  mysql_free_result(result);
  return row_count;
}

/********************************************************
* process the stmt result set                           *
*********************************************************/
#define MAX_RES_FIELDS 50
#define MAX_FIELD_DATA_SIZE 255

uint my_process_stmt_result(MYSQL_STMT *stmt)
{
  int         field_count;
  uint        row_count= 0;
  MYSQL_BIND  buffer[MAX_RES_FIELDS];
  MYSQL_FIELD *field;
  MYSQL_RES   *result;
  char        data[MAX_RES_FIELDS][MAX_FIELD_DATA_SIZE];
  ulong       length[MAX_RES_FIELDS];
  my_bool     is_null[MAX_RES_FIELDS];
  int         rc, i;

  if (!(result= mysql_get_metadata(stmt))) /* No meta info */
  {
    while (!mysql_fetch(stmt)) 
      row_count++;
    return row_count;
  }
  
  field_count= min(mysql_num_fields(result), MAX_RES_FIELDS);
  for(i=0; i < field_count; i++)
  {
    buffer[i].buffer_type= MYSQL_TYPE_STRING;
    buffer[i].buffer_length= MAX_FIELD_DATA_SIZE;
    buffer[i].length= &length[i];
    buffer[i].buffer= (char*) data[i];
    buffer[i].is_null= &is_null[i];
  }
  my_print_result_metadata(result);

  rc= mysql_bind_result(stmt,buffer);
  check_execute(stmt,rc);

  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt,rc);

  mysql_field_seek(result, 0);  
  while (mysql_fetch(stmt) == 0)
  {    
    fputc('\t',stdout);
    fputc('|',stdout);
    
    mysql_field_seek(result,0);
    for (i=0; i < field_count; i++)
    {
      field = mysql_fetch_field(result);
      if (is_null[i])
        fprintf(stdout, " %-*s |", (int) field->max_length, "NULL");
      else if (length[i] == 0)
      {
        data[i][0]='\0';  /* unmodified buffer */
        fprintf(stdout, " %*s |", (int) field->max_length, data[i]);
      }
      else if (IS_NUM(field->type))
        fprintf(stdout, " %*s |", (int) field->max_length, data[i]);
      else
        fprintf(stdout, " %-*s |", (int) field->max_length, data[i]);
    }
    fputc('\t',stdout);
    fputc('\n',stdout);
    row_count++;
  }
  if (row_count)
    my_print_dashes(result);

  fprintf(stdout,"\n\t%d %s returned\n", row_count, 
                 row_count == 1 ? "row" : "rows");
  mysql_free_result(result);
  return row_count;
}

/********************************************************
* process the stmt result set                           *
*********************************************************/
uint my_stmt_result(const char *buff)
{
  MYSQL_STMT *stmt;
  uint       row_count;
  int        rc;

  fprintf(stdout,"\n\n %s", buff);
  stmt= mysql_simple_prepare(mysql,buff);
  check_stmt(stmt);

  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  row_count= my_process_stmt_result(stmt);
  mysql_stmt_close(stmt);
    
  return row_count;
}

/*
  Utility function to verify a particular column data
*/
static void verify_col_data(const char *table, const char *col, 
                            const char *exp_data)
{
  MYSQL_RES *result;
  MYSQL_ROW row;
  int       rc, field= 1;
 
  if (table && col)
  {
    strxmov(query,"SELECT ",col," FROM ",table," LIMIT 1", NullS);
    fprintf(stdout,"\n %s", query);
    rc = mysql_query(mysql, query);
    myquery(rc);

    field= 0;
  }

  result = mysql_use_result(mysql);
  mytest(result);

  if (!(row= mysql_fetch_row(result)) || !row[field])
  {
    fprintf(stdout,"\n *** ERROR: FAILED TO GET THE RESULT ***");
    exit(1);
  }
  if (strcmp(row[field],exp_data))
  {
    fprintf(stdout,"\n obtained: `%s` (expected: `%s`)", 
	    row[field], exp_data);
    assert(0);
  }
  mysql_free_result(result);
}

/*
  Utility function to verify the field members
*/

static void verify_prepare_field(MYSQL_RES *result, 
      unsigned int no,const char *name, const char *org_name, 
      enum enum_field_types type, const char *table, 
      const char *org_table, const char *db, 
      unsigned long length, const char *def)
{
  MYSQL_FIELD *field;

  if (!(field= mysql_fetch_field_direct(result,no)))
  {
    fprintf(stdout,"\n *** ERROR: FAILED TO GET THE RESULT ***");
    exit(1);
  }
  fprintf(stdout,"\n field[%d]:", no);
  fprintf(stdout,"\n    name     :`%s`\t(expected: `%s`)", field->name, name);
  fprintf(stdout,"\n    org_name :`%s`\t(expected: `%s`)", field->org_name, org_name);
  fprintf(stdout,"\n    type     :`%d`\t(expected: `%d`)", field->type, type);
  fprintf(stdout,"\n    table    :`%s`\t(expected: `%s`)", field->table, table);
  fprintf(stdout,"\n    org_table:`%s`\t(expected: `%s`)", field->org_table, org_table);
  fprintf(stdout,"\n    database :`%s`\t(expected: `%s`)", field->db, db);
  fprintf(stdout,"\n    length   :`%ld`\t(expected: `%ld`)", field->length, length);
  fprintf(stdout,"\n    maxlength:`%ld`", field->max_length);
  fprintf(stdout,"\n    charsetnr:`%d`", field->charsetnr);
  fprintf(stdout,"\n    default  :`%s`\t(expected: `%s`)", field->def ? field->def : "(null)", def ? def: "(null)");
  fprintf(stdout,"\n");
  assert(strcmp(field->name,name) == 0);
  assert(strcmp(field->org_name,org_name) == 0);
  assert(field->type == type);
  assert(strcmp(field->table,table) == 0);
  assert(strcmp(field->org_table,org_table) == 0);
  assert(strcmp(field->db,db) == 0);
  assert(field->length == length);
  if (def)
    assert(strcmp(field->def,def) == 0);
}

/*
  Utility function to verify the parameter count
*/
static void verify_param_count(MYSQL_STMT *stmt, long exp_count)
{
  long param_count= mysql_param_count(stmt);
  fprintf(stdout,"\n total parameters in stmt: `%ld` (expected: `%ld`)",
                  param_count, exp_count);
  assert(param_count == exp_count);
}

/*
  Utility function to verify the total affected rows
*/
static void verify_st_affected_rows(MYSQL_STMT *stmt, ulonglong exp_count)
{
  ulonglong affected_rows= mysql_stmt_affected_rows(stmt);
  fprintf(stdout,"\n total affected rows: `%lld` (expected: `%lld`)",
          affected_rows, exp_count);
  assert(affected_rows == exp_count);
}

/*
  Utility function to verify the total affected rows
*/
static void verify_affected_rows(ulonglong exp_count)
{
  ulonglong affected_rows= mysql_affected_rows(mysql);
  fprintf(stdout,"\n total affected rows: `%lld` (expected: `%lld`)",
          affected_rows, exp_count);
  assert(affected_rows == exp_count);
}

/*
  Utility function to verify the total fields count
*/
static void verify_field_count(MYSQL_RES *result, uint exp_count)
{
  uint field_count= mysql_num_fields(result);
  fprintf(stdout,"\n total fields in the result set: `%d` (expected: `%d`)",
          field_count, exp_count);
  assert(field_count == exp_count);
}

/*
  Utility function to execute a query using prepare-execute
*/
static void execute_prepare_query(const char *query, ulonglong exp_count)
{
  MYSQL_STMT *stmt;
  ulonglong  affected_rows;
  int        rc;

  stmt= mysql_simple_prepare(mysql,query);
  check_stmt(stmt);

  rc = mysql_execute(stmt);
  myquery(rc);  

  affected_rows= mysql_stmt_affected_rows(stmt);
  fprintf(stdout,"\n total affected rows: `%lld` (expected: `%lld`)",
          affected_rows, exp_count);

  assert(affected_rows == exp_count);
  mysql_stmt_close(stmt);
}


/********************************************************
* store result processing                               *
*********************************************************/
static void client_store_result()
{
  MYSQL_RES *result;
  int       rc;

  myheader("client_store_result");

  rc = mysql_query(mysql, "SELECT * FROM myclient_test");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  my_process_result_set(result);
  mysql_free_result(result);
}

/********************************************************
* fetch the results
*********************************************************/
static void client_use_result()
{
  MYSQL_RES *result;
  int       rc;
  myheader("client_use_result");

  rc = mysql_query(mysql, "SELECT * FROM myclient_test");
  myquery(rc);

  /* get the result */
  result = mysql_use_result(mysql);
  mytest(result);

  my_process_result_set(result);
  mysql_free_result(result);
}

/*
  Separate thread query to test some cases
*/
static my_bool thread_query(char *query)
{
  MYSQL *l_mysql;
  my_bool error;

  error= 0;
  fprintf(stdout,"\n in thread_query(%s)", query);
  if (!(l_mysql = mysql_init(NULL)))
  { 
    myerror("mysql_init() failed");
    return 1;
  }
  if (!(mysql_real_connect(l_mysql,opt_host,opt_user,
			   opt_password, current_db, opt_port,
			   opt_unix_socket, 0)))
  {
    myerror("connection failed");    
    error= 1;
    goto end;
  }    
  if (mysql_query(l_mysql,(char *)query))
  {
     fprintf(stderr,"Query failed (%s)\n",mysql_error(l_mysql));
     error= 1;
     goto end;
  }
  mysql_commit(l_mysql);
end:
  mysql_close(l_mysql);
  return error;
}


/********************************************************
* query processing                                      *
*********************************************************/
static void test_debug_example()
{
  int rc;
  MYSQL_RES *result;

  myheader("test_debug_example");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_debug_example");
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_debug_example(id int primary key auto_increment,\
                                              name varchar(20),xxx int)");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_debug_example(name) VALUES('mysql')");
  myquery(rc);

  rc = mysql_query(mysql,"UPDATE test_debug_example SET name='updated' WHERE name='deleted'");
  myquery(rc);

  rc = mysql_query(mysql,"SELECT * FROM test_debug_example where name='mysql'");
  myquery(rc);

  result = mysql_use_result(mysql);
  mytest(result);

  my_process_result_set(result);
  mysql_free_result(result);

  rc = mysql_query(mysql,"DROP TABLE test_debug_example");
  myquery(rc);
}

/********************************************************
* to test autocommit feature                            *
*********************************************************/
static void test_tran_bdb()
{
  MYSQL_RES *result;
  MYSQL_ROW row;
  int       rc;

  myheader("test_tran_bdb");

  /* set AUTOCOMMIT to OFF */
  rc = mysql_autocommit(mysql, FALSE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS my_demo_transaction");
  myquery(rc);


  rc = mysql_commit(mysql);
  myquery(rc);

  /* create the table 'mytran_demo' of type BDB' or 'InnoDB' */
  rc = mysql_query(mysql,"CREATE TABLE my_demo_transaction(col1 int ,col2 varchar(30)) TYPE = BDB");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* insert a row and commit the transaction */
  rc = mysql_query(mysql,"INSERT INTO my_demo_transaction VALUES(10,'venu')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* now insert the second row, and rollback the transaction */
  rc = mysql_query(mysql,"INSERT INTO my_demo_transaction VALUES(20,'mysql')");
  myquery(rc);

  rc = mysql_rollback(mysql);
  myquery(rc);

  /* delete first row, and rollback it */
  rc = mysql_query(mysql,"DELETE FROM my_demo_transaction WHERE col1 = 10");
  myquery(rc);

  rc = mysql_rollback(mysql);
  myquery(rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM my_demo_transaction");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  my_process_result_set(result);
  mysql_free_result(result);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM my_demo_transaction");
  myquery(rc);

  /* get the result */
  result = mysql_use_result(mysql);
  mytest(result);

  row = mysql_fetch_row(result);
  mytest(row);

  row = mysql_fetch_row(result);
  mytest_r(row);

  mysql_free_result(result);
  mysql_autocommit(mysql,TRUE);
}

/********************************************************
* to test autocommit feature                            *
*********************************************************/
static void test_tran_innodb()
{
  MYSQL_RES *result;
  MYSQL_ROW row;
  int       rc;

  myheader("test_tran_innodb");

  /* set AUTOCOMMIT to OFF */
  rc = mysql_autocommit(mysql, FALSE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS my_demo_transaction");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* create the table 'mytran_demo' of type BDB' or 'InnoDB' */
  rc = mysql_query(mysql,"CREATE TABLE my_demo_transaction(col1 int ,col2 varchar(30)) TYPE = InnoDB");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* insert a row and commit the transaction */
  rc = mysql_query(mysql,"INSERT INTO my_demo_transaction VALUES(10,'venu')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* now insert the second row, and rollback the transaction */
  rc = mysql_query(mysql,"INSERT INTO my_demo_transaction VALUES(20,'mysql')");
  myquery(rc);

  rc = mysql_rollback(mysql);
  myquery(rc);

  /* delete first row, and rollback it */
  rc = mysql_query(mysql,"DELETE FROM my_demo_transaction WHERE col1 = 10");
  myquery(rc);

  rc = mysql_rollback(mysql);
  myquery(rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM my_demo_transaction");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  my_process_result_set(result);
  mysql_free_result(result);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM my_demo_transaction");
  myquery(rc);

  /* get the result */
  result = mysql_use_result(mysql);
  mytest(result);

  row = mysql_fetch_row(result);
  mytest(row);

  row = mysql_fetch_row(result);
  mytest_r(row);

  mysql_free_result(result);
  mysql_autocommit(mysql,TRUE);
}


/********************************************************
 To test simple prepares of all DML statements
*********************************************************/

static void test_prepare_simple()
{
  MYSQL_STMT *stmt;
  int        rc;

  myheader("test_prepare_simple");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prepare_simple");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prepare_simple(id int, name varchar(50))");
  myquery(rc);

  /* insert */
  strmov(query,"INSERT INTO test_prepare_simple VALUES(?,?)");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt,2);
  mysql_stmt_close(stmt);

  /* update */
  strmov(query,"UPDATE test_prepare_simple SET id=? WHERE id=? AND name= ?");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt,3);
  mysql_stmt_close(stmt);

  /* delete */
  strmov(query,"DELETE FROM test_prepare_simple WHERE id=10");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt,0);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);
  mysql_stmt_close(stmt);

  /* delete */
  strmov(query,"DELETE FROM test_prepare_simple WHERE id=?");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt,1);

  mysql_stmt_close(stmt);

  /* select */
  strmov(query,"SELECT * FROM test_prepare_simple WHERE id=? AND name= ?");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt,2);

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);
}


/********************************************************
* to test simple prepare field results                  *
*********************************************************/
static void test_prepare_field_result()
{
  MYSQL_STMT *stmt;
  MYSQL_RES  *result;
  int        rc;

  myheader("test_prepare_field_result");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prepare_field_result");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prepare_field_result(int_c int, \
                          var_c varchar(50), ts_c timestamp(14),\
                          char_c char(3), date_c date,extra tinyint)");
  myquery(rc); 

  /* insert */
  strmov(query,"SELECT int_c,var_c,date_c as date,ts_c,char_c FROM \
                  test_prepare_field_result as t1 WHERE int_c=?");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt,1);

  result = mysql_get_metadata(stmt);
  mytest(result);
  
  my_print_result_metadata(result);

  fprintf(stdout,"\n\n field attributes:\n");
  verify_prepare_field(result,0,"int_c","int_c",MYSQL_TYPE_LONG,
                       "t1","test_prepare_field_result",current_db,11,0);
  verify_prepare_field(result,1,"var_c","var_c",MYSQL_TYPE_VAR_STRING,
                       "t1","test_prepare_field_result",current_db,50,0);
  verify_prepare_field(result,2,"date","date_c",MYSQL_TYPE_DATE,
                       "t1","test_prepare_field_result",current_db,10,0);
  verify_prepare_field(result,3,"ts_c","ts_c",MYSQL_TYPE_TIMESTAMP,
                       "t1","test_prepare_field_result",current_db,19,0);
  verify_prepare_field(result,4,"char_c","char_c",MYSQL_TYPE_STRING,
                       "t1","test_prepare_field_result",current_db,3,0);

  verify_field_count(result, 5);
  mysql_free_result(result);
  mysql_stmt_close(stmt);
}


/********************************************************
* to test simple prepare field results                  *
*********************************************************/
static void test_prepare_syntax()
{
  MYSQL_STMT *stmt;
  int        rc;

  myheader("test_prepare_syntax");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prepare_syntax");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prepare_syntax(id int, name varchar(50), extra int)");
  myquery(rc);

  strmov(query,"INSERT INTO test_prepare_syntax VALUES(?");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt_r(stmt);

  strmov(query,"SELECT id,name FROM test_prepare_syntax WHERE id=? AND WHERE");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt_r(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);
}


/********************************************************
* to test simple prepare                                *
*********************************************************/
static void test_prepare()
{
  MYSQL_STMT *stmt;
  int        rc, i;
  int        int_data, o_int_data;
  char       str_data[50], data[50];
  char       tiny_data, o_tiny_data;
  short      small_data, o_small_data;
  longlong   big_data, o_big_data;
  float      real_data, o_real_data;
  double     double_data, o_double_data;
  ulong      length[7], len;
  my_bool    is_null[7];
  MYSQL_BIND bind[7];

  myheader("test_prepare");

  rc = mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS my_prepare");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE my_prepare(col1 tinyint,\
                                col2 varchar(15), col3 int,\
                                col4 smallint, col5 bigint, \
                                col6 float, col7 double )");
  myquery(rc);

  /* insert by prepare */
  strxmov(query,"INSERT INTO my_prepare VALUES(?,?,?,?,?,?,?)",NullS);
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt,7);

  /* tinyint */
  bind[0].buffer_type=FIELD_TYPE_TINY;
  bind[0].buffer= (char *)&tiny_data;
  /* string */
  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].buffer= (char *)str_data;
  bind[1].buffer_length= 1000;			/* Max string length */
  /* integer */
  bind[2].buffer_type=FIELD_TYPE_LONG;
  bind[2].buffer= (char *)&int_data;
  /* short */
  bind[3].buffer_type=FIELD_TYPE_SHORT;
  bind[3].buffer= (char *)&small_data;
  /* bigint */
  bind[4].buffer_type=FIELD_TYPE_LONGLONG;
  bind[4].buffer= (char *)&big_data;
  /* float */
  bind[5].buffer_type=FIELD_TYPE_FLOAT;
  bind[5].buffer= (char *)&real_data;
  /* double */
  bind[6].buffer_type=FIELD_TYPE_DOUBLE;
  bind[6].buffer= (char *)&double_data;

  for (i= 0; i < (int) array_elements(bind); i++)
  {
    bind[i].length= &length[i];
    bind[i].is_null= &is_null[i];
    is_null[i]= 0;
  }

  rc = mysql_bind_param(stmt,bind);
  check_execute(stmt, rc);

  int_data = 320;
  small_data = 1867;
  big_data   = 1000;
  real_data = 2;
  double_data = 6578.001;

  /* now, execute the prepared statement to insert 10 records.. */
  for (tiny_data=0; tiny_data < 100; tiny_data++)
  {
    length[1]= my_sprintf(str_data,(str_data, "MySQL%d",int_data));
    rc = mysql_execute(stmt);
    check_execute(stmt, rc);
    int_data += 25;
    small_data += 10;
    big_data += 100;
    real_data += 1;
    double_data += 10.09;
  }

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exists */
  assert(tiny_data == (char) my_stmt_result("SELECT * FROM my_prepare"));
      
  stmt = mysql_simple_prepare(mysql,"SELECT * FROM my_prepare");
  check_stmt(stmt);

  rc = mysql_bind_result(stmt, bind);
  check_execute(stmt, rc);

  /* get the result */
  rc = mysql_execute(stmt);
  check_execute(stmt, rc);
  
  o_int_data = 320;
  o_small_data = 1867;
  o_big_data   = 1000;
  o_real_data = 2;
  o_double_data = 6578.001;

  /* now, execute the prepared statement to insert 10 records.. */
  for (o_tiny_data=0; o_tiny_data < 100; o_tiny_data++)
  {
    len = my_sprintf(data, (data, "MySQL%d",o_int_data));
    
    rc = mysql_fetch(stmt);
    check_execute(stmt, rc);

    fprintf(stdout, "\n");
    
    fprintf(stdout, "\n\t tiny   : %d (%lu)", tiny_data,length[0]);
    fprintf(stdout, "\n\t short  : %d (%lu)", small_data,length[3]);
    fprintf(stdout, "\n\t int    : %d (%lu)", int_data,length[2]);
    fprintf(stdout, "\n\t big    : %lld (%lu)", big_data,length[4]);

    fprintf(stdout, "\n\t float  : %f (%lu)", real_data,length[5]);
    fprintf(stdout, "\n\t double : %f (%lu)", double_data,length[6]);

    fprintf(stdout, "\n\t str    : %s (%lu)", str_data, length[1]);

    assert(tiny_data == o_tiny_data);
    assert(is_null[0] == 0);
    assert(length[0] == 1);

    assert(int_data == o_int_data);
    assert(length[2] == 4);
    
    assert(small_data == o_small_data);
    assert(length[3] == 2);
    
    assert(big_data == o_big_data);
    assert(length[4] == 8);
    
    assert(real_data == o_real_data);
    assert(length[5] == 4);
    
    assert(double_data == o_double_data);
    assert(length[6] == 8);
    
    assert(strcmp(data,str_data) == 0);
    assert(length[1] == len);
    
    o_int_data += 25;
    o_small_data += 10;
    o_big_data += 100;
    o_real_data += 1;
    o_double_data += 10.09;
  }

  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);

}


/********************************************************
* to test double comparision                            *
*********************************************************/
static void test_double_compare()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       real_data[10], tiny_data;
  double     double_data;
  MYSQL_RES  *result;
  MYSQL_BIND bind[3];
  ulong	     length[3];

  myheader("test_double_compare");

  rc = mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_double_compare");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_double_compare(col1 tinyint,\
                                col2 float, col3 double )");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_double_compare VALUES(1,10.2,34.5)");
  myquery(rc);

  strmov(query, "UPDATE test_double_compare SET col1=100 WHERE col1 = ? AND col2 = ? AND COL3 = ?");
  stmt = mysql_simple_prepare(mysql,query);
  check_stmt(stmt);

  verify_param_count(stmt,3);

  /* tinyint */
  bind[0].buffer_type=FIELD_TYPE_TINY;
  bind[0].buffer=(char *)&tiny_data;
  bind[0].buffer_length= 0;
  bind[0].length= 0;
  bind[0].is_null= 0;				/* Can never be null */

  /* string->float */
  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].buffer= (char *)&real_data;
  bind[1].buffer_length=sizeof(real_data);
  bind[1].is_null= 0;
  bind[1].length= &length[1];
  length[1]= 10; 

  /* double */
  bind[2].buffer_type=FIELD_TYPE_DOUBLE;
  bind[2].buffer= (char *)&double_data;
  bind[2].buffer_length= 0;
  bind[2].length= 0;
  bind[2].is_null= 0;

  tiny_data = 1;
  strmov(real_data,"10.2");
  double_data = 34.5;
  rc = mysql_bind_param(stmt,bind);
  check_execute(stmt, rc);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  verify_affected_rows(0);

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM test_double_compare");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  assert((int)tiny_data == my_process_result_set(result));
  mysql_free_result(result);
}


/********************************************************
* to test simple null                                   *
*********************************************************/
static void test_null()
{
  MYSQL_STMT *stmt;
  int        rc;
  uint       nData;
  MYSQL_BIND bind[2];
  my_bool    is_null[2];

  myheader("test_null");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_null");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_null(col1 int,col2 varchar(50))");
  myquery(rc);

  /* insert by prepare, wrong column name */
  strmov(query,"INSERT INTO test_null(col3,col2) VALUES(?,?)");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt_r(stmt);

  strmov(query,"INSERT INTO test_null(col1,col2) VALUES(?,?)");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt,2);

  bind[0].buffer_type=MYSQL_TYPE_LONG;
  bind[0].is_null= &is_null[0];
  bind[0].length= 0;
  is_null[0]= 1;
  bind[1]=bind[0];		

  rc = mysql_bind_param(stmt,bind);
  check_execute(stmt, rc);

  /* now, execute the prepared statement to insert 10 records.. */
  for (nData=0; nData<10; nData++)
  {
    rc = mysql_execute(stmt);
    check_execute(stmt, rc);
  }

  /* Re-bind with MYSQL_TYPE_NULL */
  bind[0].buffer_type= MYSQL_TYPE_NULL;
  is_null[0]= 0; /* reset */
  bind[1]= bind[0];

  rc = mysql_bind_param(stmt,bind);
  check_execute(stmt,rc);
  
  for (nData=0; nData<10; nData++)
  {
    rc = mysql_execute(stmt);
    check_execute(stmt, rc);
  }
  
  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);

  nData*= 2;
  assert(nData == my_stmt_result("SELECT * FROM test_null"));

  /* Fetch results */
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (char *)&nData; /* this buffer won't be altered */
  bind[0].length= 0;
  bind[1]= bind[0];
  bind[0].is_null= &is_null[0];
  bind[1].is_null= &is_null[1];

  stmt = mysql_simple_prepare(mysql,"SELECT * FROM test_null");
  check_stmt(stmt);

  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  rc = mysql_bind_result(stmt,bind);
  check_execute(stmt,rc);

  rc= 0;
  is_null[0]= is_null[1]= 0;
  while (mysql_fetch(stmt) != MYSQL_NO_DATA)
  {
    assert(is_null[0]);
    assert(is_null[1]);
    rc++;
    is_null[0]= is_null[1]= 0;
  }
  assert(rc == (int)nData);
  mysql_stmt_close(stmt);
}

/*********************************************************
* Test for NULL as PS parameter (BUG#3367, BUG#3371)     *
**********************************************************/
static void test_ps_null_param()
{
  MYSQL_STMT *stmt;
  int        rc;
  
  MYSQL_BIND in_bind;
  my_bool    in_is_null;
  long int   in_long;
  
  MYSQL_BIND out_bind;
  ulong	     out_length;
  my_bool    out_is_null;
  char       out_str_data[20];

  const char *queries[]= {"select ?", "select ?+1", 
                    "select col1 from test_ps_nulls where col1 <=> ?",
                    NULL
                    };
  const char **cur_query= queries;

  myheader("test_null_ps_param_in_result");

  rc= mysql_query(mysql,"DROP TABLE IF EXISTS test_ps_nulls");
  myquery(rc);

  rc= mysql_query(mysql,"CREATE TABLE test_ps_nulls(col1 int)");
  myquery(rc);

  rc= mysql_query(mysql,"INSERT INTO test_ps_nulls values (1),(null)");
  myquery(rc);

  in_bind.buffer_type= MYSQL_TYPE_LONG;
  in_bind.is_null= &in_is_null;
  in_bind.length= 0;
  in_bind.buffer= (char*)&in_long;
  in_is_null= 1;
  in_long= 1;

  out_bind.buffer_type=FIELD_TYPE_STRING;
  out_bind.is_null= &out_is_null;
  out_bind.length= &out_length;
  out_bind.buffer= out_str_data;
  out_bind.buffer_length= array_elements(out_str_data); 
  
  /* Execute several queries, all returning NULL in result. */
  for(cur_query= queries; *cur_query; cur_query++)
  {
    strmov(query, *cur_query);
    stmt = mysql_simple_prepare(mysql, query);
    check_stmt(stmt);
    verify_param_count(stmt,1);

    rc = mysql_bind_param(stmt,&in_bind);
    check_execute(stmt, rc);
    rc= mysql_bind_result(stmt,&out_bind);
    check_execute(stmt, rc);
    rc = mysql_execute(stmt);
    check_execute(stmt, rc);
    rc= mysql_fetch(stmt);
    assert(rc != MYSQL_NO_DATA);
    assert(out_is_null);
    rc= mysql_fetch(stmt);
    assert(rc == MYSQL_NO_DATA);
    mysql_stmt_close(stmt);
  }
}

/********************************************************
* to test fetch null                                    *
*********************************************************/
static void test_fetch_null()
{
  MYSQL_STMT *stmt;
  int        rc;
  int        i, nData;
  MYSQL_BIND bind[11];
  ulong	     length[11];
  my_bool    is_null[11];

  myheader("test_fetch_null");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_fetch_null");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_fetch_null(col1 tinyint, col2 smallint, \
                                                       col3 int, col4 bigint, \
                                                       col5 float, col6 double, \
                                                       col7 date, col8 time, \
                                                       col9 varbinary(10), \
                                                       col10 varchar(50),\
                                                       col11 char(20))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_fetch_null(col11) VALUES(1000),(88),(389789)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* fetch */
  for (i= 0; i < (int) array_elements(bind); i++)
  {
    bind[i].buffer_type=FIELD_TYPE_LONG;
    bind[i].is_null= &is_null[i];
    bind[i].length= &length[i];
  }
  bind[i-1].buffer=(char *)&nData;		/* Last column is not null */

  strmov((char *)query , "SELECT * FROM test_fetch_null");

  assert(3 == my_stmt_result(query));

  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  rc = mysql_bind_result(stmt,bind);
  check_execute(stmt, rc);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  rc= 0;
  while (mysql_fetch(stmt) != MYSQL_NO_DATA)
  {
    rc++;
    for (i=0; i < 10; i++)
    {
      fprintf(stdout, "\n data[%d] : %s", i, 
              is_null[i] ? "NULL" : "NOT NULL");
      assert(is_null[i]);
    }
    fprintf(stdout, "\n data[%d]: %d", i, nData);
    assert(nData == 1000 || nData == 88 || nData == 389789);
    assert(is_null[i] == 0);
    assert(length[i] == 4);
  }
  assert(rc == 3);
  mysql_stmt_close(stmt);
}


/********************************************************
* to test simple select                                 *
*********************************************************/
static void test_select_version()
{
  MYSQL_STMT *stmt;
  int        rc;

  myheader("test_select_version");

  stmt = mysql_simple_prepare(mysql, "SELECT @@version");
  check_stmt(stmt);

  verify_param_count(stmt,0);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  my_process_stmt_result(stmt);
  mysql_stmt_close(stmt);
}

/********************************************************
* to test simple show                                   *
*********************************************************/
static void test_select_show_table()
{
  MYSQL_STMT *stmt;
  int        rc, i;

  myheader("test_select_show_table");

  stmt = mysql_simple_prepare(mysql, "SHOW TABLES FROM mysql");
  check_stmt(stmt);

  verify_param_count(stmt,0);

  for (i= 1; i < 3; i++)
  {
    rc = mysql_execute(stmt);
    check_execute(stmt, rc);
  }

  my_process_stmt_result(stmt);
  mysql_stmt_close(stmt);
}


/********************************************************
* to test simple select to debug                        *
*********************************************************/
static void test_select_direct()
{
  int        rc;
  MYSQL_RES  *result;

  myheader("test_select_direct");
  
  rc = mysql_autocommit(mysql,TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_select");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_select(id int, id1 tinyint, \
                                                   id2 float, \
                                                   id3 double, \
                                                   name varchar(50))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* insert a row and commit the transaction */
  rc = mysql_query(mysql,"INSERT INTO test_select VALUES(10,5,2.3,4.5,'venu')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"SELECT * FROM test_select");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  my_process_result_set(result);
  mysql_free_result(result);
}


/********************************************************
* to test simple select with prepare                    *
*********************************************************/
static void test_select_prepare()
{
  int        rc;
  MYSQL_STMT *stmt;

  myheader("test_select_prepare");
  
  rc = mysql_autocommit(mysql,TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_select");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_select(id int, name varchar(50))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* insert a row and commit the transaction */
  rc = mysql_query(mysql,"INSERT INTO test_select VALUES(10,'venu')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  stmt = mysql_simple_prepare(mysql,"SELECT * FROM test_select");
  check_stmt(stmt);
  
  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  assert(1 == my_process_stmt_result(stmt));
  mysql_stmt_close(stmt);

  rc = mysql_query(mysql,"DROP TABLE test_select");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_select(id tinyint, id1 int, \
                                                   id2 float, id3 float, \
                                                   name varchar(50))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* insert a row and commit the transaction */
  rc = mysql_query(mysql,"INSERT INTO test_select(id,id1,id2,name) VALUES(10,5,2.3,'venu')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  stmt = mysql_simple_prepare(mysql,"SELECT * FROM test_select");
  check_stmt(stmt);

  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  assert(1 == my_process_stmt_result(stmt));
  mysql_stmt_close(stmt);
}


/********************************************************
* to test simple select                                 *
*********************************************************/
static void test_select()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       szData[25];
  int        nData=1;
  MYSQL_BIND bind[2];
  ulong length[2];

  myheader("test_select");

  rc = mysql_autocommit(mysql,TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_select");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_select(id int,name varchar(50))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* insert a row and commit the transaction */
  rc = mysql_query(mysql,"INSERT INTO test_select VALUES(10,'venu')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* now insert the second row, and rollback the transaction */
  rc = mysql_query(mysql,"INSERT INTO test_select VALUES(20,'mysql')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  strmov(query,"SELECT * FROM test_select WHERE id=? AND name=?");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt,2);

  /* string data */
  nData=10;
  strmov(szData,(char *)"venu");
  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].buffer=(char *)szData;
  bind[1].buffer_length= 4;
  bind[1].length= &length[1];
  length[1]= 4;
  bind[1].is_null=0;

  bind[0].buffer=(char *)&nData;
  bind[0].buffer_type=FIELD_TYPE_LONG;
  bind[0].buffer_length= 0;
  bind[0].length= 0;
  bind[0].is_null= 0;

  rc = mysql_bind_param(stmt,bind);
  check_execute(stmt, rc);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  assert(my_process_stmt_result(stmt) == 1);

  mysql_stmt_close(stmt);
}

/*
  test BUG#1115 (incorrect string parameter value allocation)
*/
static void test_bug1115()
{
  MYSQL_STMT *stmt;
  int rc;
  MYSQL_BIND bind[1];
  ulong length[1];
  char szData[11];

  myheader("test_bug1115");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_select");
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_select(\
session_id  char(9) NOT NULL, \
    a       int(8) unsigned NOT NULL, \
    b        int(5) NOT NULL, \
    c      int(5) NOT NULL, \
    d  datetime NOT NULL)");
  myquery(rc);
  rc = mysql_query(mysql,"INSERT INTO test_select VALUES (\"abc\",1,2,3,2003-08-30), (\"abd\",1,2,3,2003-08-30), (\"abf\",1,2,3,2003-08-30), (\"abg\",1,2,3,2003-08-30), (\"abh\",1,2,3,2003-08-30), (\"abj\",1,2,3,2003-08-30), (\"abk\",1,2,3,2003-08-30), (\"abl\",1,2,3,2003-08-30), (\"abq\",1,2,3,2003-08-30), (\"abw\",1,2,3,2003-08-30), (\"abe\",1,2,3,2003-08-30), (\"abr\",1,2,3,2003-08-30), (\"abt\",1,2,3,2003-08-30), (\"aby\",1,2,3,2003-08-30), (\"abu\",1,2,3,2003-08-30), (\"abi\",1,2,3,2003-08-30), (\"abo\",1,2,3,2003-08-30), (\"abp\",1,2,3,2003-08-30), (\"abz\",1,2,3,2003-08-30), (\"abx\",1,2,3,2003-08-30)");
  myquery(rc);

  strmov(query,"SELECT * FROM test_select WHERE session_id = ?");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt,1);

  strmov(szData,(char *)"abc");
  bind[0].buffer_type=FIELD_TYPE_STRING;
  bind[0].buffer=(char *)szData;
  bind[0].buffer_length= 10;
  bind[0].length= &length[0];
  length[0]= 3;
  bind[0].is_null=0;

  rc = mysql_bind_param(stmt,bind);
  check_execute(stmt, rc);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  assert(my_process_stmt_result(stmt) == 1);

  strmov(szData,(char *)"venu");
  bind[0].buffer_type=FIELD_TYPE_STRING;
  bind[0].buffer=(char *)szData;
  bind[0].buffer_length= 10;
  bind[0].length= &length[0];
  length[0]= 4;
  bind[0].is_null=0;

  rc = mysql_bind_param(stmt,bind);
  check_execute(stmt, rc);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  assert(my_process_stmt_result(stmt) == 0);

  strmov(szData,(char *)"abc");
  bind[0].buffer_type=FIELD_TYPE_STRING;
  bind[0].buffer=(char *)szData;
  bind[0].buffer_length= 10;
  bind[0].length= &length[0];
  length[0]= 3;
  bind[0].is_null=0;

  rc = mysql_bind_param(stmt,bind);
  check_execute(stmt, rc);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  assert(my_process_stmt_result(stmt) == 1);

  mysql_stmt_close(stmt);
}

/*
  test BUG#1180 (optimized away part of WHERE clause)
*/
static void test_bug1180()
{
  MYSQL_STMT *stmt;
  int rc;
  MYSQL_BIND bind[1];
  ulong length[1];
  char szData[11];

  myheader("test_select_bug");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_select");
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_select(session_id  char(9) NOT NULL)");
  myquery(rc);
  rc = mysql_query(mysql,"INSERT INTO test_select VALUES (\"abc\")");
  myquery(rc);

  strmov(query,"SELECT * FROM test_select WHERE ?=\"1111\" and session_id = \"abc\"");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt,1);

  strmov(szData,(char *)"abc");
  bind[0].buffer_type=FIELD_TYPE_STRING;
  bind[0].buffer=(char *)szData;
  bind[0].buffer_length= 10;
  bind[0].length= &length[0];
  length[0]= 3;
  bind[0].is_null=0;

  rc = mysql_bind_param(stmt,bind);
  check_execute(stmt, rc);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  assert(my_process_stmt_result(stmt) == 0);

  strmov(szData,(char *)"1111");
  bind[0].buffer_type=FIELD_TYPE_STRING;
  bind[0].buffer=(char *)szData;
  bind[0].buffer_length= 10;
  bind[0].length= &length[0];
  length[0]= 4;
  bind[0].is_null=0;

  rc = mysql_bind_param(stmt,bind);
  check_execute(stmt, rc);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  assert(my_process_stmt_result(stmt) == 1);

  strmov(szData,(char *)"abc");
  bind[0].buffer_type=FIELD_TYPE_STRING;
  bind[0].buffer=(char *)szData;
  bind[0].buffer_length= 10;
  bind[0].length= &length[0];
  length[0]= 3;
  bind[0].is_null=0;

  rc = mysql_bind_param(stmt,bind);
  check_execute(stmt, rc);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  assert(my_process_stmt_result(stmt) == 0);

  mysql_stmt_close(stmt);
}

/*
  test BUG#1644 (Insertion of more than 3 NULL columns with
                 parameter binding fails)
*/
static void test_bug1644()
{
  MYSQL_STMT *stmt;
  MYSQL_RES *result;
  MYSQL_ROW row;
  MYSQL_BIND bind[4];
  int num;
  my_bool isnull;
  int rc, i;

  myheader("test_bug1644");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS foo_dfr");
  myquery(rc);

  rc= mysql_query(mysql,
	   "CREATE TABLE foo_dfr(col1 int, col2 int, col3 int, col4 int);");
  myquery(rc);

  strmov(query, "INSERT INTO foo_dfr VALUES (?,?,?,? )");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 4);

  num= 22;
  isnull= 0;
  for (i = 0 ; i < 4 ; i++)
  {
    bind[i].buffer_type= FIELD_TYPE_LONG;
    bind[i].buffer= (char *)&num;
    bind[i].buffer_length= 0;
    bind[i].length= 0;
    bind[i].is_null= &isnull;
  }

  rc= mysql_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_execute(stmt);
  check_execute(stmt, rc);

  isnull= 1;
  for (i = 0 ; i < 4 ; i++)
    bind[i].is_null= &isnull;

  rc= mysql_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_execute(stmt);
  check_execute(stmt, rc);

  isnull= 0;
  num= 88;
  for (i = 0 ; i < 4 ; i++)
    bind[i].is_null= &isnull;

  rc= mysql_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_execute(stmt);
  check_execute(stmt, rc);

  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "SELECT * FROM foo_dfr");
  myquery(rc);

  result= mysql_store_result(mysql);
  mytest(result);

  assert(3 == my_process_result_set(result));

  mysql_data_seek(result, 0);

  row= mysql_fetch_row(result);
  mytest(row);
  for (i = 0 ; i < 4 ; i++)
  {
    assert(strcmp(row[i], "22") == 0);
  }
  row= mysql_fetch_row(result);
  mytest(row);
  for (i = 0 ; i < 4 ; i++)
  {
    assert(row[i] == 0);
  }
  row= mysql_fetch_row(result);
  mytest(row);
  for (i = 0 ; i < 4 ; i++)
  {
    assert(strcmp(row[i], "88") == 0);
  }
  row= mysql_fetch_row(result);
  mytest_r(row);

  mysql_free_result(result);
}


/********************************************************
* to test simple select show                            *
*********************************************************/
static void test_select_show()
{
  MYSQL_STMT *stmt;
  int        rc;

  myheader("test_select_show");

  mysql_autocommit(mysql,TRUE);

  rc = mysql_query(mysql, "DROP TABLE IF EXISTS test_show");
  myquery(rc);
  
  rc = mysql_query(mysql, "CREATE TABLE test_show(id int(4) NOT NULL primary key, name char(2))");
  myquery(rc);

  stmt = mysql_simple_prepare(mysql, "show columns from test_show");
  check_stmt(stmt);

  verify_param_count(stmt,0);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  my_process_stmt_result(stmt);
  mysql_stmt_close(stmt);
  
  stmt = mysql_simple_prepare(mysql, "show tables from mysql like ?");
  check_stmt_r(stmt);
  
  strxmov(query,"show tables from ", current_db, " like \'test_show\'", NullS);
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  my_process_stmt_result(stmt);
  mysql_stmt_close(stmt);
  
  stmt = mysql_simple_prepare(mysql, "describe test_show");
  check_stmt(stmt);
  
  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  my_process_stmt_result(stmt);
  mysql_stmt_close(stmt);
  
  stmt = mysql_simple_prepare(mysql, "show keys from test_show");
  check_stmt(stmt);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  assert(1 == my_process_stmt_result(stmt));
  mysql_stmt_close(stmt);
}


/********************************************************
* to test simple update                                *
*********************************************************/
static void test_simple_update()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       szData[25];
  int        nData=1;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];   
  ulong	     length[2];

  myheader("test_simple_update");

  rc = mysql_autocommit(mysql,TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_update");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_update(col1 int,\
                                col2 varchar(50), col3 int )");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_update VALUES(1,'MySQL',100)");
  myquery(rc);

  verify_affected_rows(1);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* insert by prepare */
  strmov(query,"UPDATE test_update SET col2=? WHERE col1=?");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt,2);

  nData=1;
  bind[0].buffer_type=FIELD_TYPE_STRING;
  bind[0].buffer=szData;		/* string data */
  bind[0].buffer_length=sizeof(szData);
  bind[0].length= &length[0];
  bind[0].is_null= 0;
  length[0]= my_sprintf(szData, (szData,"updated-data"));

  bind[1].buffer=(char *) &nData;
  bind[1].buffer_type=FIELD_TYPE_LONG;
  bind[1].buffer_length= 0;
  bind[1].length= 0;
  bind[1].is_null= 0;

  rc = mysql_bind_param(stmt,bind);
  check_execute(stmt, rc);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);
  verify_affected_rows(1);

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM test_update");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  assert(1 == my_process_result_set(result));
  mysql_free_result(result);
}


/********************************************************
* to test simple long data handling                     *
*********************************************************/
static void test_long_data()
{
  MYSQL_STMT *stmt;
  int        rc, int_data;
  char       *data=NullS;
  MYSQL_RES  *result;
  MYSQL_BIND bind[3];

  myheader("test_long_data");

  rc = mysql_autocommit(mysql,TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_long_data");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_long_data(col1 int,\
                                col2 long varchar, col3 long varbinary)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  strmov(query,"INSERT INTO test_long_data(col1,col2) VALUES(?)");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt_r(stmt);

  strmov(query,"INSERT INTO test_long_data(col1,col2,col3) VALUES(?,?,?)");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt,3);

  bind[0].buffer=(char *)&int_data;
  bind[0].buffer_type=FIELD_TYPE_LONG;
  bind[0].is_null=0;
  bind[0].buffer_length= 0;
  bind[0].length= 0;

  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].is_null=0;
  bind[1].buffer_length=0;			/* Will not be used */
  bind[1].length=0;				/* Will not be used */

  bind[2]=bind[1];
  rc = mysql_bind_param(stmt,bind);
  check_execute(stmt, rc);

  int_data= 999;
  data = (char *)"Michael";

  /* supply data in pieces */
  rc = mysql_send_long_data(stmt,1,data,strlen(data));
  data = (char *)" 'Monty' Widenius";
  rc = mysql_send_long_data(stmt,1,data,strlen(data));
  check_execute(stmt, rc);
  rc = mysql_send_long_data(stmt,2,"Venu (venu@mysql.com)",4);
  check_execute(stmt, rc);

  /* execute */
  rc = mysql_execute(stmt);
  fprintf(stdout," mysql_execute() returned %d\n",rc);
  check_execute(stmt,rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* now fetch the results ..*/
  rc = mysql_query(mysql,"SELECT * FROM test_long_data");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  assert(1 == my_process_result_set(result));
  mysql_free_result(result);

  verify_col_data("test_long_data","col1","999");
  verify_col_data("test_long_data","col2","Michael 'Monty' Widenius");
  verify_col_data("test_long_data","col3","Venu");
  mysql_stmt_close(stmt);
}


/********************************************************
* to test long data (string) handling                   *
*********************************************************/
static void test_long_data_str()
{
  MYSQL_STMT *stmt;
  int        rc, i;
  char       data[255];
  long       length, length1;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];
  my_bool    is_null[2];

  myheader("test_long_data_str");

  rc = mysql_autocommit(mysql,TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_long_data_str");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_long_data_str(id int, longstr long varchar)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  strmov(query,"INSERT INTO test_long_data_str VALUES(?,?)");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt,2);

  bind[0].buffer = (char *)&length;
  bind[0].buffer_type = FIELD_TYPE_LONG;
  bind[0].is_null= &is_null[0];
  bind[0].buffer_length= 0;
  bind[0].length= 0;
  is_null[0]=0;
  length= 0;

  bind[1].buffer=data;				/* string data */
  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].length= &length1;
  bind[1].buffer_length=0;			/* Will not be used */
  bind[1].is_null= &is_null[1];
  is_null[1]=0;
  rc = mysql_bind_param(stmt,bind);
  check_execute(stmt, rc);

  length = 40;
  strmov(data,"MySQL AB");

  /* supply data in pieces */
  for(i=0; i < 4; i++)
  {
    rc = mysql_send_long_data(stmt,1,(char *)data,5);
    check_execute(stmt, rc);
  }
  /* execute */
  rc = mysql_execute(stmt);
  fprintf(stdout," mysql_execute() returned %d\n",rc);
  check_execute(stmt,rc);

  mysql_stmt_close(stmt);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* now fetch the results ..*/
  rc = mysql_query(mysql,"SELECT LENGTH(longstr), longstr FROM test_long_data_str");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  assert(1 == my_process_result_set(result));
  mysql_free_result(result);

  my_sprintf(data,(data,"%d", i*5));
  verify_col_data("test_long_data_str","LENGTH(longstr)", data);
  data[0]='\0';
  while (i--)
   strxmov(data,data,"MySQL",NullS);
  verify_col_data("test_long_data_str","longstr", data);

  rc = mysql_query(mysql,"DROP TABLE test_long_data_str");
  myquery(rc);
}


/********************************************************
* to test long data (string) handling                   *
*********************************************************/
static void test_long_data_str1()
{
  MYSQL_STMT *stmt;
  int        rc, i;
  char       data[255];
  long       length, length1;
  ulong	     max_blob_length, blob_length;
  my_bool    true_value;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];
  MYSQL_FIELD *field;

  myheader("test_long_data_str1");

  rc = mysql_autocommit(mysql,TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_long_data_str");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_long_data_str(longstr long varchar,blb long varbinary)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  strmov(query,"INSERT INTO test_long_data_str VALUES(?,?)");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt,2);

  bind[0].buffer=data;		  /* string data */
  bind[0].buffer_length= sizeof(data);
  bind[0].length= &length1;
  bind[0].buffer_type=FIELD_TYPE_STRING;
  bind[0].is_null= 0;
  length1= 0;

  bind[1] = bind[0];
  bind[1].buffer_type=FIELD_TYPE_BLOB;

  rc = mysql_bind_param(stmt,bind);
  check_execute(stmt, rc);
  length = my_sprintf(data, (data, "MySQL AB"));

  /* supply data in pieces */
  for (i=0; i < 3; i++)
  {
    rc = mysql_send_long_data(stmt,0,data,length);
    check_execute(stmt, rc);

    rc = mysql_send_long_data(stmt,1,data,2);
    check_execute(stmt, rc);
  }

  /* execute */
  rc = mysql_execute(stmt);
  fprintf(stdout," mysql_execute() returned %d\n",rc);
  check_execute(stmt,rc);

  mysql_stmt_close(stmt);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* now fetch the results ..*/
  rc = mysql_query(mysql,"SELECT LENGTH(longstr),longstr,LENGTH(blb),blb FROM test_long_data_str");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);

  mysql_field_seek(result, 1);
  field= mysql_fetch_field(result);
  max_blob_length= field->max_length;

  mytest(result);

  assert(1 == my_process_result_set(result));
  mysql_free_result(result);

  my_sprintf(data,(data,"%ld",(long)i*length));
  verify_col_data("test_long_data_str","length(longstr)",data);

  my_sprintf(data,(data,"%d",i*2));
  verify_col_data("test_long_data_str","length(blb)",data);

  /* Test length of field->max_length */
  stmt= mysql_simple_prepare(mysql, "SELECT * from test_long_data_str");
  check_stmt(stmt);
  verify_param_count(stmt,0);
 
  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt,rc);

  result= mysql_get_metadata(stmt);
  field= mysql_fetch_fields(result);

  /* First test what happens if STMT_ATTR_UPDATE_MAX_LENGTH is not used */
  DBUG_ASSERT(field->max_length == 0);
  mysql_free_result(result);

  /* Enable updating of field->max_length */
  true_value= 1;
  mysql_stmt_attr_set(stmt, STMT_ATTR_UPDATE_MAX_LENGTH, (void*) &true_value);
  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt,rc);

  result= mysql_get_metadata(stmt);
  field= mysql_fetch_fields(result);

  DBUG_ASSERT(field->max_length == max_blob_length);

  /* Fetch results into a data buffer that is smaller than data */
  bzero((char*) bind, sizeof(*bind));
  bind[0].buffer_type= MYSQL_TYPE_BLOB;
  bind[0].buffer= (char *) &data; /* this buffer won't be altered */
  bind[0].buffer_length= 16;
  bind[0].length= &blob_length;
  rc= mysql_bind_result(stmt,bind);
  data[16]= 0;

  DBUG_ASSERT((mysql_fetch(stmt) == 0));
  DBUG_ASSERT(strlen(data) == 16);
  DBUG_ASSERT(blob_length == max_blob_length);

  /* Fetch all data */
  bzero((char*) (bind+1), sizeof(*bind));
  bind[1].buffer_type= MYSQL_TYPE_BLOB;
  bind[1].buffer= (char *) &data; /* this buffer won't be altered */
  bind[1].buffer_length= sizeof(data);
  bind[1].length= &blob_length;
  bzero(data, sizeof(data));
  mysql_stmt_fetch_column(stmt, bind+1, 0, 0);
  DBUG_ASSERT(strlen(data) == max_blob_length);

  mysql_free_result(result);
  mysql_stmt_close(stmt);

  /* Drop created table */
  rc = mysql_query(mysql,"DROP TABLE test_long_data_str");
  myquery(rc);
}


/********************************************************
* to test long data (binary) handling                   *
*********************************************************/
static void test_long_data_bin()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       data[255];
  long       length;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];


  myheader("test_long_data_bin");

  rc = mysql_autocommit(mysql,TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_long_data_bin");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_long_data_bin(id int, longbin long varbinary)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  strmov(query,"INSERT INTO test_long_data_bin VALUES(?,?)");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt,2);

  bind[0].buffer = (char *)&length;
  bind[0].buffer_type = FIELD_TYPE_LONG;
  bind[0].buffer_length= 0;
  bind[0].length= 0;
  bind[0].is_null= 0;
  length= 0;

  bind[1].buffer=data;		 /* string data */
  bind[1].buffer_type=FIELD_TYPE_LONG_BLOB;
  bind[1].length= 0;		/* Will not be used */
  bind[1].is_null= 0;
  rc = mysql_bind_param(stmt,bind);
  check_execute(stmt, rc);

  length = 10;
  strmov(data,"MySQL AB");

  /* supply data in pieces */
  {
    int i;
    for (i=0; i < 100; i++)
    {
      rc = mysql_send_long_data(stmt,1,(char *)data,4);
      check_execute(stmt, rc);
    }
  }
  /* execute */
  rc = mysql_execute(stmt);
  fprintf(stdout," mysql_execute() returned %d\n",rc);
  check_execute(stmt,rc);

  mysql_stmt_close(stmt);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* now fetch the results ..*/
  rc = mysql_query(mysql,"SELECT LENGTH(longbin), longbin FROM test_long_data_bin");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  assert(1 == my_process_result_set(result));
  mysql_free_result(result);
}


/********************************************************
* to test simple delete                                 *
*********************************************************/
static void test_simple_delete()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       szData[30]={0};
  int        nData=1;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];
  ulong length[2];

  myheader("test_simple_delete");

  rc = mysql_autocommit(mysql,TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_simple_delete");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_simple_delete(col1 int,\
                                col2 varchar(50), col3 int )");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_simple_delete VALUES(1,'MySQL',100)");
  myquery(rc);

  verify_affected_rows(1);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* insert by prepare */
  strmov(query,"DELETE FROM test_simple_delete WHERE col1=? AND col2=? AND col3=100");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt,2);

  nData=1;
  strmov(szData,"MySQL");
  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].buffer= szData;		/* string data */
  bind[1].buffer_length=sizeof(szData);
  bind[1].length= &length[1];
  bind[1].is_null= 0;
  length[1]= 5;

  bind[0].buffer=(char *)&nData;
  bind[0].buffer_type=FIELD_TYPE_LONG;
  bind[0].buffer_length= 0;
  bind[0].length= 0;
  bind[0].is_null= 0;

  rc = mysql_bind_param(stmt,bind);
  check_execute(stmt, rc);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  verify_affected_rows(1);

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM test_simple_delete");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  assert(0 == my_process_result_set(result));
  mysql_free_result(result);
}



/********************************************************
* to test simple update                                 *
*********************************************************/
static void test_update()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       szData[25];
  int        nData=1;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];
  ulong length[2];

  myheader("test_update");

  rc = mysql_autocommit(mysql,TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_update");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_update(col1 int primary key auto_increment,\
                                col2 varchar(50), col3 int )");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  strmov(query,"INSERT INTO test_update(col2,col3) VALUES(?,?)");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt,2);

  /* string data */
  bind[0].buffer_type=FIELD_TYPE_STRING;
  bind[0].buffer=szData;
  bind[0].buffer_length= sizeof(szData);
  bind[0].length= &length[0];
  length[0]= my_sprintf(szData, (szData, "inserted-data"));
  bind[0].is_null= 0;

  bind[1].buffer=(char *)&nData;
  bind[1].buffer_type=FIELD_TYPE_LONG;
  bind[1].buffer_length= 0;
  bind[1].length= 0;
  bind[1].is_null= 0;

  rc = mysql_bind_param(stmt,bind);
  check_execute(stmt, rc);

  nData=100;
  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  verify_affected_rows(1);
  mysql_stmt_close(stmt);

  strmov(query,"UPDATE test_update SET col2=? WHERE col3=?");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt,2);
  nData=100;

  bind[0].buffer_type=FIELD_TYPE_STRING;
  bind[0].buffer=szData;
  bind[0].buffer_length= sizeof(szData);
  bind[0].length= &length[0];
  length[0]= my_sprintf(szData, (szData, "updated-data"));
  bind[1].buffer=(char *)&nData;
  bind[1].buffer_type=FIELD_TYPE_LONG;
  bind[1].buffer_length= 0;
  bind[1].length= 0;
  bind[1].is_null= 0;

  rc = mysql_bind_param(stmt,bind);
  check_execute(stmt, rc);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);
  verify_affected_rows(1);

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM test_update");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  assert(1 == my_process_result_set(result));
  mysql_free_result(result);
}


/********************************************************
* to test simple prepare                                *
*********************************************************/
static void test_prepare_noparam()
{
  MYSQL_STMT *stmt;
  int        rc;
  MYSQL_RES  *result;

  myheader("test_prepare_noparam");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS my_prepare");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE my_prepare(col1 int ,col2 varchar(50))");
  myquery(rc);


  /* insert by prepare */
  strmov(query,"INSERT INTO my_prepare VALUES(10,'venu')");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt,0);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM my_prepare");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  assert(1 == my_process_result_set(result));
  mysql_free_result(result);
}


/********************************************************
* to test simple bind result                            *
*********************************************************/
static void test_bind_result()
{
  MYSQL_STMT *stmt;
  int        rc;
  int        nData;
  ulong	     length, length1;
  char       szData[100];
  MYSQL_BIND bind[2];
  my_bool    is_null[2];

  myheader("test_bind_result");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_result");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_result(col1 int ,col2 varchar(50))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_bind_result VALUES(10,'venu')");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_bind_result VALUES(20,'MySQL')");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_bind_result(col2) VALUES('monty')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* fetch */

  bind[0].buffer_type=FIELD_TYPE_LONG;
  bind[0].buffer= (char *) &nData;	/* integer data */
  bind[0].is_null= &is_null[0];
  bind[0].length= 0;

  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].buffer=szData;		/* string data */
  bind[1].buffer_length=sizeof(szData);
  bind[1].length= &length1;
  bind[1].is_null= &is_null[1];

  stmt = mysql_simple_prepare(mysql, "SELECT * FROM test_bind_result");
  check_stmt(stmt);

  rc = mysql_bind_result(stmt,bind);
  check_execute(stmt, rc);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  fprintf(stdout,"\n row 1: %d,%s(%lu)",nData, szData, length1);
  assert(nData == 10);
  assert(strcmp(szData,"venu")==0);
  assert(length1 == 4);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  fprintf(stdout,"\n row 2: %d,%s(%lu)",nData, szData, length1);
  assert(nData == 20);
  assert(strcmp(szData,"MySQL")==0);
  assert(length1 == 5);

  length=99;
  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);
 
  if (is_null[0])
    fprintf(stdout,"\n row 3: NULL,%s(%lu)", szData, length1);
  assert(is_null[0]);
  assert(strcmp(szData,"monty")==0);
  assert(length1 == 5);

  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}


/********************************************************
* to test ext bind result                               *
*********************************************************/
static void test_bind_result_ext()
{
  MYSQL_STMT *stmt;
  int        rc, i;
  uchar      t_data;
  short      s_data;
  int        i_data;
  longlong   b_data;
  float      f_data;
  double     d_data;
  char       szData[20], bData[20];
  ulong       szLength, bLength;
  MYSQL_BIND bind[8];
  long	     length[8];
  my_bool    is_null[8];

  myheader("test_bind_result_ext");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_result");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_result(c1 tinyint, c2 smallint, \
                                                        c3 int, c4 bigint, \
                                                        c5 float, c6 double, \
                                                        c7 varbinary(10), \
                                                        c8 varchar(50))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_bind_result VALUES(19,2999,3999,4999999,\
                                                              2345.6,5678.89563,\
                                                              'venu','mysql')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  for (i= 0; i < (int) array_elements(bind); i++)
  {
    bind[i].length=  &length[i];
    bind[i].is_null= &is_null[i];
  }

  bind[0].buffer_type=MYSQL_TYPE_TINY;
  bind[0].buffer=(char *)&t_data;

  bind[1].buffer_type=MYSQL_TYPE_SHORT;
  bind[2].buffer_type=MYSQL_TYPE_LONG;
  
  bind[3].buffer_type=MYSQL_TYPE_LONGLONG;
  bind[1].buffer=(char *)&s_data;
  
  bind[2].buffer=(char *)&i_data;
  bind[3].buffer=(char *)&b_data;

  bind[4].buffer_type=MYSQL_TYPE_FLOAT;
  bind[4].buffer=(char *)&f_data;

  bind[5].buffer_type=MYSQL_TYPE_DOUBLE;
  bind[5].buffer=(char *)&d_data;

  bind[6].buffer_type=MYSQL_TYPE_STRING;
  bind[6].buffer= (char *)szData;
  bind[6].buffer_length= sizeof(szData);
  bind[6].length= &szLength;

  bind[7].buffer_type=MYSQL_TYPE_TINY_BLOB;
  bind[7].buffer=(char *)&bData;
  bind[7].length= &bLength;
  bind[7].buffer_length= sizeof(bData);

  stmt = mysql_simple_prepare(mysql, "select * from test_bind_result");
  check_stmt(stmt);

  rc = mysql_bind_result(stmt,bind);
  check_execute(stmt, rc);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  fprintf(stdout, "\n data (tiny)   : %d", t_data);
  fprintf(stdout, "\n data (short)  : %d", s_data);
  fprintf(stdout, "\n data (int)    : %d", i_data);
  fprintf(stdout, "\n data (big)    : %lld", b_data);

  fprintf(stdout, "\n data (float)  : %f", f_data);
  fprintf(stdout, "\n data (double) : %f", d_data);

  fprintf(stdout, "\n data (str)    : %s(%lu)", szData, szLength);

  bData[bLength]= '\0';                         /* bData is binary */
  fprintf(stdout, "\n data (bin)    : %s(%lu)", bData, bLength);


  assert(t_data == 19);
  assert(s_data == 2999);
  assert(i_data == 3999);
  assert(b_data == 4999999);
  /*assert(f_data == 2345.60);*/
  /*assert(d_data == 5678.89563);*/
  assert(strcmp(szData,"venu")==0);
  assert(strncmp(bData,"mysql",5)==0);
  assert(szLength == 4);
  assert(bLength == 5);

  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}


/********************************************************
* to test ext bind result                               *
*********************************************************/
static void test_bind_result_ext1()
{
  MYSQL_STMT *stmt;
  uint	     i;
  int        rc;
  char       t_data[20];
  float      s_data;
  short      i_data;
  uchar      b_data;
  int        f_data;
  long       bData;
  char       d_data[20];
  double     szData;
  MYSQL_BIND bind[8];
  ulong      length[8];
  my_bool    is_null[8];
  myheader("test_bind_result_ext1");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_result");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_result(c1 tinyint, c2 smallint, \
                                                        c3 int, c4 bigint, \
                                                        c5 float, c6 double, \
                                                        c7 varbinary(10), \
                                                        c8 varchar(10))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_bind_result VALUES(120,2999,3999,54,\
                                                              2.6,58.89,\
                                                              '206','6.7')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  bind[0].buffer_type=MYSQL_TYPE_STRING;
  bind[0].buffer=(char *) t_data;
  bind[0].buffer_length= sizeof(t_data);

  bind[1].buffer_type=MYSQL_TYPE_FLOAT;
  bind[1].buffer=(char *)&s_data;
  bind[1].buffer_length= 0;

  bind[2].buffer_type=MYSQL_TYPE_SHORT;
  bind[2].buffer=(char *)&i_data;
  bind[2].buffer_length= 0;

  bind[3].buffer_type=MYSQL_TYPE_TINY;
  bind[3].buffer=(char *)&b_data;
  bind[3].buffer_length= 0;

  bind[4].buffer_type=MYSQL_TYPE_LONG;
  bind[4].buffer=(char *)&f_data;
  bind[4].buffer_length= 0;

  bind[5].buffer_type=MYSQL_TYPE_STRING;
  bind[5].buffer=(char *)d_data;
  bind[5].buffer_length= sizeof(d_data);

  bind[6].buffer_type=MYSQL_TYPE_LONG;
  bind[6].buffer=(char *)&bData;
  bind[6].buffer_length= 0;

  bind[7].buffer_type=MYSQL_TYPE_DOUBLE;
  bind[7].buffer=(char *)&szData;
  bind[7].buffer_length= 0;

  for (i= 0; i < array_elements(bind); i++)
  {
    bind[i].is_null= &is_null[i];
    bind[i].length= &length[i];
  }

  stmt = mysql_simple_prepare(mysql, "select * from test_bind_result");
  check_stmt(stmt);

  rc = mysql_bind_result(stmt,bind);
  check_execute(stmt, rc);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  fprintf(stdout, "\n data (tiny)   : %s(%lu)", t_data, length[0]);
  fprintf(stdout, "\n data (short)  : %f(%lu)", s_data, length[1]);
  fprintf(stdout, "\n data (int)    : %d(%lu)", i_data, length[2]);
  fprintf(stdout, "\n data (big)    : %d(%lu)", b_data, length[3]);

  fprintf(stdout, "\n data (float)  : %d(%lu)", f_data, length[4]);
  fprintf(stdout, "\n data (double) : %s(%lu)", d_data, length[5]);

  fprintf(stdout, "\n data (bin)    : %ld(%lu)", bData, length[6]);
  fprintf(stdout, "\n data (str)    : %g(%lu)", szData, length[7]);

  assert(strcmp(t_data,"120")==0);
  assert(i_data == 3999);
  assert(f_data == 2);
  assert(strcmp(d_data,"58.89")==0);
  assert(b_data == 54);

  assert(length[0] == 3);
  assert(length[1] == 4);
  assert(length[2] == 2);
  assert(length[3] == 1);
  assert(length[4] == 4);
  assert(length[5] == 5);
  assert(length[6] == 4);
  assert(length[7] == 8);

  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}

/*
 Generalized fetch conversion routine for all basic types           
*/
static void bind_fetch(int row_count)
{ 
  MYSQL_STMT   *stmt;
  int          rc, i, count= row_count;
  ulong	       bit;
  long         data[10];
  float        f_data;
  double       d_data;
  char         s_data[10];
  ulong	       length[10];
  MYSQL_BIND   bind[7];
  my_bool      is_null[7]; 

  stmt = mysql_simple_prepare(mysql,"INSERT INTO test_bind_fetch VALUES(?,?,?,?,?,?,?)");
  check_stmt(stmt);

  verify_param_count(stmt, 7);
 
  for (i= 0; i < (int) array_elements(bind); i++)
  {  
    bind[i].buffer_type= MYSQL_TYPE_LONG;
    bind[i].buffer= (char *) &data[i];
    bind[i].length= 0;
    bind[i].is_null= 0;
  }   
  rc = mysql_bind_param(stmt, bind);
  check_execute(stmt,rc);
 
  while (count--)
  {
    rc= 10+count;
    for (i= 0; i < (int) array_elements(bind); i++)
    {  
      data[i]= rc+i;
      rc+= 12;
    }
    rc = mysql_execute(stmt);
    check_execute(stmt, rc);
  }

  rc = mysql_commit(mysql);
  myquery(rc);

  mysql_stmt_close(stmt);

  assert(row_count == (int)
           my_stmt_result("SELECT * FROM test_bind_fetch"));

  stmt = mysql_simple_prepare(mysql,"SELECT * FROM test_bind_fetch");
  myquery(rc);

  for (i= 0; i < (int) array_elements(bind); i++)
  {
    bind[i].buffer= (char *) &data[i];
    bind[i].length= &length[i];
    bind[i].is_null= &is_null[i];
  }

  bind[0].buffer_type= MYSQL_TYPE_TINY;
  bind[1].buffer_type= MYSQL_TYPE_SHORT;
  bind[2].buffer_type= MYSQL_TYPE_LONG;
  bind[3].buffer_type= MYSQL_TYPE_LONGLONG;

  bind[4].buffer_type= MYSQL_TYPE_FLOAT;
  bind[4].buffer= (char *)&f_data;

  bind[5].buffer_type= MYSQL_TYPE_DOUBLE;
  bind[5].buffer= (char *)&d_data;

  bind[6].buffer_type= MYSQL_TYPE_STRING;
  bind[6].buffer= (char *)&s_data;
  bind[6].buffer_length= sizeof(s_data);

  rc = mysql_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  rc = mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  while (row_count--)
  {
    rc = mysql_fetch(stmt);
    check_execute(stmt,rc);

    fprintf(stdout, "\n");
    fprintf(stdout, "\n tiny     : %ld(%lu)", data[0], length[0]);
    fprintf(stdout, "\n short    : %ld(%lu)", data[1], length[1]);
    fprintf(stdout, "\n int      : %ld(%lu)", data[2], length[2]);
    fprintf(stdout, "\n longlong : %ld(%lu)", data[3], length[3]);
    fprintf(stdout, "\n float    : %f(%lu)",  f_data,  length[4]);
    fprintf(stdout, "\n double   : %g(%lu)",  d_data,  length[5]);
    fprintf(stdout, "\n char     : %s(%lu)",  s_data,  length[6]);

    bit= 1;
    rc= 10+row_count;
    for (i=0; i < 4; i++)
    {
      assert(data[i] == rc+i);
      assert(length[i] == bit);
      bit<<= 1;
      rc+= 12;
    }

    /* FLOAT */
    rc+= i;
    assert((int)f_data == rc);
    assert(length[4] == 4);

    /* DOUBLE */
    rc+= 13;
    assert((int)d_data == rc);
    assert(length[5] == 8);

    /* CHAR */
    rc+= 13;
    {
      char buff[20];
      long len= my_sprintf(buff, (buff, "%d", rc));
      assert(strcmp(s_data,buff)==0);
      assert(length[6] == (ulong) len);
    }
  }
  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}

/********************************************************
* to test fetching of date, time and ts                 *
*********************************************************/
static void test_fetch_date()
{
  MYSQL_STMT *stmt;
  uint	     i;
  int        rc, year;
  char       date[25], time[25], ts[25], ts_4[15], ts_6[20], dt[20];
  ulong      d_length, t_length, ts_length, ts4_length, ts6_length, 
             dt_length, y_length;
  MYSQL_BIND bind[8];
  my_bool    is_null[8];
  ulong	     length[8];

  myheader("test_fetch_date");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_result");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_result(c1 date, c2 time, \
                                                        c3 timestamp(14), \
                                                        c4 year, \
                                                        c5 datetime, \
                                                        c6 timestamp(4), \
                                                        c7 timestamp(6))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_bind_result VALUES('2002-01-02',\
                                                              '12:49:00',\
                                                              '2002-01-02 17:46:59', \
                                                              2010,\
                                                              '2010-07-10', \
                                                              '2020','1999-12-29')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  for (i= 0; i < array_elements(bind); i++)
  {
    bind[i].is_null= &is_null[i];
    bind[i].length= &length[i];
  }

  bind[0].buffer_type=MYSQL_TYPE_STRING;
  bind[1]=bind[2]=bind[0];

  bind[0].buffer=(char *)&date;
  bind[0].buffer_length= sizeof(date);
  bind[0].length= &d_length;

  bind[1].buffer=(char *)&time;
  bind[1].buffer_length= sizeof(time);
  bind[1].length= &t_length;

  bind[2].buffer=(char *)&ts;
  bind[2].buffer_length= sizeof(ts);
  bind[2].length= &ts_length;

  bind[3].buffer_type=MYSQL_TYPE_LONG;
  bind[3].buffer=(char *)&year;
  bind[3].length= &y_length;

  bind[4].buffer_type=MYSQL_TYPE_STRING;
  bind[4].buffer=(char *)&dt;
  bind[4].buffer_length= sizeof(dt);
  bind[4].length= &dt_length;

  bind[5].buffer_type=MYSQL_TYPE_STRING;
  bind[5].buffer=(char *)&ts_4;
  bind[5].buffer_length= sizeof(ts_4);
  bind[5].length= &ts4_length;

  bind[6].buffer_type=MYSQL_TYPE_STRING;
  bind[6].buffer=(char *)&ts_6;
  bind[6].buffer_length= sizeof(ts_6);
  bind[6].length= &ts6_length;

  assert(1 == my_stmt_result("SELECT * FROM test_bind_result"));

  stmt = mysql_simple_prepare(mysql, "SELECT * FROM test_bind_result");
  check_stmt(stmt);

  rc = mysql_bind_result(stmt,bind);
  check_execute(stmt, rc);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);
  
  ts_4[0]='\0';
  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  fprintf(stdout, "\n date   : %s(%lu)", date, d_length);
  fprintf(stdout, "\n time   : %s(%lu)", time, t_length);
  fprintf(stdout, "\n ts     : %s(%lu)", ts, ts_length);
  fprintf(stdout, "\n year   : %d(%lu)", year, y_length);
  fprintf(stdout, "\n dt     : %s(%lu)", dt,  dt_length);
  fprintf(stdout, "\n ts(4)  : %s(%lu)", ts_4, ts4_length);
  fprintf(stdout, "\n ts(6)  : %s(%lu)", ts_6, ts6_length);

  assert(strcmp(date,"2002-01-02")==0);
  assert(d_length == 10);

  assert(strcmp(time,"12:49:00")==0);
  assert(t_length == 8);

  assert(strcmp(ts,"2002-01-02 17:46:59")==0);
  assert(ts_length == 19);

  assert(year == 2010);
  assert(y_length == 4);
  
  assert(strcmp(dt,"2010-07-10 00:00:00")==0);
  assert(dt_length == 19);

  assert(ts_4[0] == '\0');
  assert(ts4_length == 0);

  assert(strcmp(ts_6,"1999-12-29 00:00:00")==0);
  assert(ts6_length == 19);

  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}

/********************************************************
* to test fetching of str to all types                  *
*********************************************************/
static void test_fetch_str()
{
  int rc;

  myheader("test_fetch_str");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_fetch");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_fetch(c1 char(10),\
                                                     c2 char(10),\
                                                     c3 char(20),\
                                                     c4 char(20),\
                                                     c5 char(30),\
                                                     c6 char(40),\
                                                     c7 char(20))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  bind_fetch(3);
}

/********************************************************
* to test fetching of long to all types                 *
*********************************************************/
static void test_fetch_long()
{
  int rc;

  myheader("test_fetch_long");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_fetch");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_fetch(c1 int unsigned,\
                                                     c2 int unsigned,\
                                                     c3 int,\
                                                     c4 int,\
                                                     c5 int,\
                                                     c6 int unsigned,\
                                                     c7 int)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  bind_fetch(4);
}


/********************************************************
* to test fetching of short to all types                 *
*********************************************************/
static void test_fetch_short()
{
  int rc;

  myheader("test_fetch_short");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_fetch");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_fetch(c1 smallint unsigned,\
                                                     c2 smallint,\
                                                     c3 smallint unsigned,\
                                                     c4 smallint,\
                                                     c5 smallint,\
                                                     c6 smallint,\
                                                     c7 smallint unsigned)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);
  
  bind_fetch(5);
}


/********************************************************
* to test fetching of tiny to all types                 *
*********************************************************/
static void test_fetch_tiny()
{
  int rc;

  myheader("test_fetch_tiny");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_fetch");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_fetch(c1 tinyint unsigned,\
                                                     c2 tinyint,\
                                                     c3 tinyint unsigned,\
                                                     c4 tinyint,\
                                                     c5 tinyint,\
                                                     c6 tinyint,\
                                                     c7 tinyint unsigned)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);
  
  bind_fetch(3);

}


/********************************************************
* to test fetching of longlong to all types             *
*********************************************************/
static void test_fetch_bigint()
{
  int rc;

  myheader("test_fetch_bigint");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_fetch");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_fetch(c1 bigint,\
                                                     c2 bigint,\
                                                     c3 bigint unsigned,\
                                                     c4 bigint unsigned,\
                                                     c5 bigint unsigned,\
                                                     c6 bigint unsigned,\
                                                     c7 bigint unsigned)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);
  
  bind_fetch(2);

}


/********************************************************
* to test fetching of float to all types                 *
*********************************************************/
static void test_fetch_float()
{
  int rc;

  myheader("test_fetch_float");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_fetch");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_fetch(c1 float(3),\
                                                     c2 float,\
                                                     c3 float unsigned,\
                                                     c4 float,\
                                                     c5 float,\
                                                     c6 float,\
                                                     c7 float(10) unsigned)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);
  
  bind_fetch(2);

}

/********************************************************
* to test fetching of double to all types               *
*********************************************************/
static void test_fetch_double()
{
  int rc;

  myheader("test_fetch_double");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_fetch");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_fetch(c1 double(5,2),\
                                                     c2 double unsigned,\
                                                     c3 double unsigned,\
                                                     c4 double unsigned,\
                                                     c5 double unsigned,\
                                                     c6 double unsigned,\
                                                     c7 double unsigned)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);
  
  bind_fetch(3);

}

/********************************************************
* to test simple prepare with all possible types        *
*********************************************************/
static void test_prepare_ext()
{
  MYSQL_STMT *stmt;
  uint	     i;
  int        rc;
  char       *sql;
  int        nData=1;
  char       tData=1;
  short      sData=10;
  longlong   bData=20;
  MYSQL_BIND bind[6];
  myheader("test_prepare_ext");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prepare_ext");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  sql = (char *)"CREATE TABLE test_prepare_ext\
			(\
			c1  tinyint,\
			c2  smallint,\
			c3  mediumint,\
			c4  int,\
			c5  integer,\
			c6  bigint,\
			c7  float,\
			c8  double,\
			c9  double precision,\
			c10 real,\
			c11 decimal(7,4),\
      c12 numeric(8,4),\
			c13 date,\
			c14 datetime,\
			c15 timestamp(14),\
			c16 time,\
			c17 year,\
			c18 bit,\
      c19 bool,\
			c20 char,\
			c21 char(10),\
			c22 varchar(30),\
			c23 tinyblob,\
			c24 tinytext,\
			c25 blob,\
			c26 text,\
			c27 mediumblob,\
			c28 mediumtext,\
			c29 longblob,\
			c30 longtext,\
			c31 enum('one','two','three'),\
			c32 set('monday','tuesday','wednesday'))";

  rc = mysql_query(mysql,sql);
  myquery(rc);

  /* insert by prepare - all integers */
  strmov(query,(char *)"INSERT INTO test_prepare_ext(c1,c2,c3,c4,c5,c6) VALUES(?,?,?,?,?,?)");
  stmt = mysql_simple_prepare(mysql,query);
  check_stmt(stmt);

  verify_param_count(stmt,6);

  /*tinyint*/
  bind[0].buffer_type=FIELD_TYPE_TINY;
  bind[0].buffer= (char *)&tData;

  /*smallint*/
  bind[1].buffer_type=FIELD_TYPE_SHORT;
  bind[1].buffer= (char *)&sData;

  /*mediumint*/
  bind[2].buffer_type=FIELD_TYPE_LONG;
  bind[2].buffer= (char *)&nData;

  /*int*/
  bind[3].buffer_type=FIELD_TYPE_LONG;
  bind[3].buffer= (char *)&nData;

  /*integer*/
  bind[4].buffer_type=FIELD_TYPE_LONG;
  bind[4].buffer= (char *)&nData;

  /*bigint*/
  bind[5].buffer_type=FIELD_TYPE_LONGLONG;
  bind[5].buffer= (char *)&bData;

  for (i= 0; i < array_elements(bind); i++)
  {
    bind[i].is_null=0;
    bind[i].buffer_length= 0;
    bind[i].length= 0;
  }

  rc = mysql_bind_param(stmt,bind);
  check_execute(stmt, rc);

  /*
  *  integer to integer
  */
  for (nData=0; nData<10; nData++, tData++, sData++,bData++)
  {
    rc = mysql_execute(stmt);
    check_execute(stmt, rc);
  }
  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);

  stmt = mysql_simple_prepare(mysql,"SELECT c1,c2,c3,c4,c5,c6 FROM test_prepare_ext");
  check_stmt(stmt);

  /* get the result */
  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  assert(nData == (int)my_process_stmt_result(stmt));

  mysql_stmt_close(stmt);
}



/********************************************************
* to test real and alias names                          *
*********************************************************/
static void test_field_names()
{
  int        rc;
  MYSQL_RES  *result;

  myheader("test_field_names");

  fprintf(stdout,"\n %d,%d,%d",MYSQL_TYPE_DECIMAL,MYSQL_TYPE_NEWDATE,MYSQL_TYPE_ENUM);
  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_field_names1");
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_field_names2");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_field_names1(id int,name varchar(50))");
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_field_names2(id int,name varchar(50))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* with table name included with TRUE column name */
  rc = mysql_query(mysql,"SELECT id as 'id-alias' FROM test_field_names1");
  myquery(rc);

  result = mysql_use_result(mysql);
  mytest(result);

  assert(0 == my_process_result_set(result));
  mysql_free_result(result);

  /* with table name included with TRUE column name */
  rc = mysql_query(mysql,"SELECT t1.id as 'id-alias',test_field_names2.name FROM test_field_names1 t1,test_field_names2");
  myquery(rc);

  result = mysql_use_result(mysql);
  mytest(result);

  assert(0 == my_process_result_set(result));
  mysql_free_result(result);
}

/********************************************************
* to test warnings                                      *
*********************************************************/
static void test_warnings()
{
  int        rc;
  MYSQL_RES  *result;

  myheader("test_warnings");

  mysql_query(mysql, "DROP TABLE if exists test_non_exists");

  rc = mysql_query(mysql, "DROP TABLE if exists test_non_exists");
  myquery(rc);

  fprintf(stdout, "\n total warnings: %d", mysql_warning_count(mysql));
  rc = mysql_query(mysql,"SHOW WARNINGS");
  myquery(rc);

  result = mysql_store_result(mysql);
  mytest(result);

  assert(1 == my_process_result_set(result));
  mysql_free_result(result);
}

/********************************************************
* to test errors                                        *
*********************************************************/
static void test_errors()
{
  int        rc;
  MYSQL_RES  *result;

  myheader("test_errors");

  mysql_query(mysql, "DROP TABLE if exists test_non_exists");

  rc = mysql_query(mysql, "DROP TABLE test_non_exists");
  myquery_r(rc);

  rc = mysql_query(mysql,"SHOW ERRORS");
  myquery(rc);

  result = mysql_store_result(mysql);
  mytest(result);

  my_process_result_set(result);
  mysql_free_result(result);
}



/********************************************************
* to test simple prepare-insert                         *
*********************************************************/
static void test_insert()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       str_data[50];
  char       tiny_data;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];
  ulong	     length;

  myheader("test_insert");

  rc = mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prep_insert");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prep_insert(col1 tinyint,\
                                col2 varchar(50))");
  myquery(rc);

  /* insert by prepare */
  stmt = mysql_simple_prepare(mysql, "INSERT INTO test_prep_insert VALUES(?,?)");
  check_stmt(stmt);

  verify_param_count(stmt,2);

  /* tinyint */
  bind[0].buffer_type=FIELD_TYPE_TINY;
  bind[0].buffer=(char *)&tiny_data;
  bind[0].is_null= 0;
  bind[0].length= 0;

  /* string */
  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].buffer=str_data;
  bind[1].buffer_length=sizeof(str_data);;
  bind[1].is_null= 0;
  bind[1].length= &length;

  rc = mysql_bind_param(stmt,bind);
  check_execute(stmt, rc);

  /* now, execute the prepared statement to insert 10 records.. */
  for (tiny_data=0; tiny_data < 3; tiny_data++)
  {
    length = my_sprintf(str_data, (str_data, "MySQL%d",tiny_data));
    rc = mysql_execute(stmt);
    check_execute(stmt, rc);
  }

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM test_prep_insert");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  assert((int)tiny_data == my_process_result_set(result));
  mysql_free_result(result);

}

/********************************************************
* to test simple prepare-resultset info                 *
*********************************************************/
static void test_prepare_resultset()
{
  MYSQL_STMT *stmt;
  int        rc;
  MYSQL_RES  *result;

  myheader("test_prepare_resultset");

  rc = mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prepare_resultset");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prepare_resultset(id int,\
                                name varchar(50),extra double)");
  myquery(rc);

  stmt = mysql_simple_prepare(mysql, "SELECT * FROM test_prepare_resultset");
  check_stmt(stmt);

  verify_param_count(stmt,0);

  result = mysql_get_metadata(stmt);
  mytest(result);
  my_print_result_metadata(result);
  mysql_free_result(result);
  mysql_stmt_close(stmt);
}

/********************************************************
* to test field flags (verify .NET provider)            *
*********************************************************/

static void test_field_flags()
{
  int          rc;
  MYSQL_RES    *result;
  MYSQL_FIELD  *field;
  unsigned int i;


  myheader("test_field_flags");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_field_flags");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_field_flags(id int NOT NULL AUTO_INCREMENT PRIMARY KEY,\
                                                        id1 int NOT NULL,\
                                                        id2 int UNIQUE,\
                                                        id3 int,\
                                                        id4 int NOT NULL,\
                                                        id5 int,\
                                                        KEY(id3,id4))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* with table name included with TRUE column name */
  rc = mysql_query(mysql,"SELECT * FROM test_field_flags");
  myquery(rc);

  result = mysql_use_result(mysql);
  mytest(result);

  mysql_field_seek(result,0);
  fputc('\n', stdout);  

  for(i=0; i< mysql_num_fields(result); i++)
  {
    field = mysql_fetch_field(result);
    fprintf(stdout,"\n field:%d",i);
    if (field->flags & NOT_NULL_FLAG)
      fprintf(stdout,"\n  NOT_NULL_FLAG");
    if (field->flags & PRI_KEY_FLAG)
      fprintf(stdout,"\n  PRI_KEY_FLAG");
    if (field->flags & UNIQUE_KEY_FLAG)
      fprintf(stdout,"\n  UNIQUE_KEY_FLAG");
    if (field->flags & MULTIPLE_KEY_FLAG)
      fprintf(stdout,"\n  MULTIPLE_KEY_FLAG");
    if (field->flags & AUTO_INCREMENT_FLAG)
      fprintf(stdout,"\n  AUTO_INCREMENT_FLAG");

  }
  mysql_free_result(result);
}

/**************************************************************
 * Test mysql_stmt_close for open stmts                       *
**************************************************************/
static void test_stmt_close()
{
  MYSQL *lmysql;
  MYSQL_STMT *stmt1, *stmt2, *stmt3, *stmt_x;
  MYSQL_BIND  bind[1];
  MYSQL_RES   *result;
  unsigned int  count;
  int   rc;
  
  myheader("test_stmt_close");  

  fprintf(stdout, "\n Establishing a test connection ...");
  if (!(lmysql = mysql_init(NULL)))
  { 
    myerror("mysql_init() failed");
    exit(0);
  }
  if (!(mysql_real_connect(lmysql,opt_host,opt_user,
			   opt_password, current_db, opt_port,
			   opt_unix_socket, 0)))
  {
    myerror("connection failed");
    exit(0);
  }   
  fprintf(stdout," OK");
  

  /* set AUTOCOMMIT to ON*/
  mysql_autocommit(lmysql, TRUE);
  
  rc = mysql_query(lmysql,"DROP TABLE IF EXISTS test_stmt_close");
  myquery(rc);
  
  rc = mysql_query(lmysql,"CREATE TABLE test_stmt_close(id int)");
  myquery(rc);

  strmov(query,"DO \"nothing\"");
  stmt1= mysql_simple_prepare(lmysql, query);
  check_stmt(stmt1);
  
  verify_param_count(stmt1, 0);
  
  strmov(query,"INSERT INTO test_stmt_close(id) VALUES(?)");
  stmt_x= mysql_simple_prepare(mysql, query);
  check_stmt(stmt_x);

  verify_param_count(stmt_x, 1);
  
  strmov(query,"UPDATE test_stmt_close SET id=? WHERE id=?");
  stmt3= mysql_simple_prepare(lmysql, query);
  check_stmt(stmt3);
  
  verify_param_count(stmt3, 2);
  
  strmov(query,"SELECT * FROM test_stmt_close WHERE id=?");
  stmt2= mysql_simple_prepare(lmysql, query);
  check_stmt(stmt2);

  verify_param_count(stmt2, 1);

  rc= mysql_stmt_close(stmt1);
  fprintf(stdout,"\n mysql_close_stmt(1) returned: %d", rc);
  assert(rc == 0);
  
  /*
    Originally we were going to close all statements automatically in
    mysql_close(). This proved to not work well - users weren't able to
    close statements by hand once mysql_close() had been called.
    Now mysql_close() doesn't free any statements, so this test doesn't
    serve its original destination any more. 
    Here we free stmt2 and stmt3 by hande to avoid memory leaks.
  */
  mysql_stmt_close(stmt2);
  mysql_stmt_close(stmt3);
  mysql_close(lmysql);
 
  count= 100;
  bind[0].buffer=(char *)&count;
  bind[0].buffer_type=MYSQL_TYPE_LONG;
  bind[0].buffer_length= 0;
  bind[0].length= 0;
  bind[0].is_null=0;

  rc = mysql_bind_param(stmt_x, bind);
  check_execute(stmt_x, rc);
  
  rc = mysql_execute(stmt_x);
  check_execute(stmt_x, rc);

  verify_st_affected_rows(stmt_x, 1);

  rc= mysql_stmt_close(stmt_x);
  fprintf(stdout,"\n mysql_close_stmt(x) returned: %d", rc);
  assert( rc == 0);

  rc = mysql_query(mysql,"SELECT id FROM test_stmt_close");
  myquery(rc);

  result = mysql_store_result(mysql);
  mytest(result);

  assert(1 == my_process_result_set(result));
  mysql_free_result(result);
}


/********************************************************
 * To test simple set-variable prepare                   *
*********************************************************/
static void test_set_variable()
{
  MYSQL_STMT *stmt, *stmt1;
  int        rc;
  int        set_count, def_count, get_count;
  ulong      length;
  char       var[NAME_LEN+1];
  MYSQL_BIND set_bind[1], get_bind[2];

  myheader("test_set_variable");

  mysql_autocommit(mysql, TRUE);
  
  stmt1 = mysql_simple_prepare(mysql, "show variables like 'max_error_count'");
  check_stmt(stmt1);

  get_bind[0].buffer_type= MYSQL_TYPE_STRING;
  get_bind[0].buffer= (char *)var;
  get_bind[0].is_null= 0;
  get_bind[0].length= &length;
  get_bind[0].buffer_length= (int)NAME_LEN;
  length= NAME_LEN;

  get_bind[1].buffer_type= MYSQL_TYPE_LONG;
  get_bind[1].buffer= (char *)&get_count;
  get_bind[1].is_null= 0;
  get_bind[1].length= 0;

  rc = mysql_execute(stmt1);
  check_execute(stmt1, rc);
  
  rc = mysql_bind_result(stmt1, get_bind);
  check_execute(stmt1, rc);

  rc = mysql_fetch(stmt1);
  check_execute(stmt1, rc);

  fprintf(stdout, "\n max_error_count(default): %d", get_count);
  def_count= get_count;

  assert(strcmp(var,"max_error_count") == 0);
  rc = mysql_fetch(stmt1);
  assert(rc == MYSQL_NO_DATA);

  stmt = mysql_simple_prepare(mysql, "set max_error_count=?");
  check_stmt(stmt);

  set_bind[0].buffer_type= MYSQL_TYPE_LONG;
  set_bind[0].buffer= (char *)&set_count;
  set_bind[0].is_null= 0;
  set_bind[0].length= 0;
  
  rc = mysql_bind_param(stmt, set_bind);
  check_execute(stmt,rc);
  
  set_count= 31;
  rc= mysql_execute(stmt);
  check_execute(stmt,rc);

  mysql_commit(mysql);

  rc = mysql_execute(stmt1);
  check_execute(stmt1, rc);
  
  rc = mysql_fetch(stmt1);
  check_execute(stmt1, rc);

  fprintf(stdout, "\n max_error_count         : %d", get_count);
  assert(get_count == set_count);

  rc = mysql_fetch(stmt1);
  assert(rc == MYSQL_NO_DATA);
  
  /* restore back to default */
  set_count= def_count;
  rc= mysql_execute(stmt);
  check_execute(stmt, rc);
  
  rc = mysql_execute(stmt1);
  check_execute(stmt1, rc);
  
  rc = mysql_fetch(stmt1);
  check_execute(stmt1, rc);

  fprintf(stdout, "\n max_error_count(default): %d", get_count);
  assert(get_count == set_count);

  rc = mysql_fetch(stmt1);
  assert(rc == MYSQL_NO_DATA);
  
  mysql_stmt_close(stmt);
  mysql_stmt_close(stmt1);
}

#if NOT_USED
/* Insert meta info .. */
static void test_insert_meta()
{
  MYSQL_STMT *stmt;
  int        rc;
  MYSQL_RES  *result;
  MYSQL_FIELD *field;

  myheader("test_insert_meta");

  rc = mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prep_insert");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prep_insert(col1 tinyint,\
                                col2 varchar(50), col3 varchar(30))");
  myquery(rc);

  strmov(query,"INSERT INTO test_prep_insert VALUES(10,'venu1','test')");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt,0);

  result= mysql_param_result(stmt);
  mytest_r(result);

  mysql_stmt_close(stmt);

  strmov(query,"INSERT INTO test_prep_insert VALUES(?,'venu',?)");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt,2);

  result= mysql_param_result(stmt);
  mytest(result);

  my_print_result_metadata(result);

  mysql_field_seek(result, 0);
  field= mysql_fetch_field(result);
  mytest(field);
  fprintf(stdout, "\n obtained: `%s` (expected: `%s`)", field->name, "col1");
  assert(strcmp(field->name,"col1")==0);

  field= mysql_fetch_field(result);
  mytest(field);
  fprintf(stdout, "\n obtained: `%s` (expected: `%s`)", field->name, "col3");
  assert(strcmp(field->name,"col3")==0);

  field= mysql_fetch_field(result);
  mytest_r(field);

  mysql_free_result(result);
  mysql_stmt_close(stmt);
}

/* Update meta info .. */
static void test_update_meta()
{
  MYSQL_STMT *stmt;
  int        rc;
  MYSQL_RES  *result;
  MYSQL_FIELD *field;

  myheader("test_update_meta");

  rc = mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prep_update");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prep_update(col1 tinyint,\
                                col2 varchar(50), col3 varchar(30))");
  myquery(rc);

  strmov(query,"UPDATE test_prep_update SET col1=10, col2='venu1' WHERE col3='test'");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt,0);

  result= mysql_param_result(stmt);
  mytest_r(result);

  mysql_stmt_close(stmt);

  strmov(query,"UPDATE test_prep_update SET col1=?, col2='venu' WHERE col3=?");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt,2);

  result= mysql_param_result(stmt);
  mytest(result);

  my_print_result_metadata(result);

  mysql_field_seek(result, 0);
  field= mysql_fetch_field(result);
  mytest(field);
  fprintf(stdout, "\n col obtained: `%s` (expected: `%s`)", field->name, "col1");
  fprintf(stdout, "\n tab obtained: `%s` (expected: `%s`)", field->table, "test_prep_update");
  assert(strcmp(field->name,"col1")==0);
  assert(strcmp(field->table,"test_prep_update")==0);

  field= mysql_fetch_field(result);
  mytest(field);
  fprintf(stdout, "\n col obtained: `%s` (expected: `%s`)", field->name, "col3");
  fprintf(stdout, "\n tab obtained: `%s` (expected: `%s`)", field->table, "test_prep_update");
  assert(strcmp(field->name,"col3")==0);
  assert(strcmp(field->table,"test_prep_update")==0);

  field= mysql_fetch_field(result);
  mytest_r(field);

  mysql_free_result(result);
  mysql_stmt_close(stmt);
}

/* Select meta info .. */
static void test_select_meta()
{
  MYSQL_STMT *stmt;
  int        rc;
  MYSQL_RES  *result;
  MYSQL_FIELD *field;

  myheader("test_select_meta");

  rc = mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prep_select");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prep_select(col1 tinyint,\
                                col2 varchar(50), col3 varchar(30))");
  myquery(rc);

  strmov(query,"SELECT * FROM test_prep_select WHERE col1=10");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt,0);

  result= mysql_param_result(stmt);
  mytest_r(result);

  strmov(query,"SELECT col1, col3 from test_prep_select WHERE col1=? AND col3='test' AND col2= ?");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt,2);

  result= mysql_param_result(stmt);
  mytest(result);

  my_print_result_metadata(result);

  mysql_field_seek(result, 0);
  field= mysql_fetch_field(result);
  mytest(field);
  fprintf(stdout, "\n col obtained: `%s` (expected: `%s`)", field->name, "col1");
  fprintf(stdout, "\n tab obtained: `%s` (expected: `%s`)", field->table, "test_prep_select");
  assert(strcmp(field->name,"col1")==0);
  assert(strcmp(field->table,"test_prep_select")==0);

  field= mysql_fetch_field(result);
  mytest(field);
  fprintf(stdout, "\n col obtained: `%s` (expected: `%s`)", field->name, "col2");
  fprintf(stdout, "\n tab obtained: `%s` (expected: `%s`)", field->table, "test_prep_select");
  assert(strcmp(field->name,"col2")==0);
  assert(strcmp(field->table,"test_prep_select")==0);

  field= mysql_fetch_field(result);
  mytest_r(field);

  mysql_free_result(result);
  mysql_stmt_close(stmt);
}
#endif


/* Test FUNCTION field info / DATE_FORMAT() table_name . */
static void test_func_fields()
{
  int        rc;
  MYSQL_RES  *result;
  MYSQL_FIELD *field;

  myheader("test_func_fields");

  rc = mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_dateformat");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_dateformat(id int, \
                                                       ts timestamp)");
  myquery(rc);

  rc = mysql_query(mysql, "INSERT INTO test_dateformat(id) values(10)");
  myquery(rc);

  rc = mysql_query(mysql, "SELECT ts FROM test_dateformat");
  myquery(rc);

  result = mysql_store_result(mysql);
  mytest(result);

  field = mysql_fetch_field(result);
  mytest(field);
  fprintf(stdout,"\n table name: `%s` (expected: `%s`)", field->table,
                  "test_dateformat");
  assert(strcmp(field->table, "test_dateformat")==0);

  field = mysql_fetch_field(result);
  mytest_r(field); /* no more fields */

  mysql_free_result(result);

  /* DATE_FORMAT */
  rc = mysql_query(mysql, "SELECT DATE_FORMAT(ts,'%Y') AS 'venu' FROM test_dateformat");
  myquery(rc);

  result = mysql_store_result(mysql);
  mytest(result);

  field = mysql_fetch_field(result);
  mytest(field);
  fprintf(stdout,"\n table name: `%s` (expected: `%s`)", field->table, "");
  assert(field->table[0] == '\0');

  field = mysql_fetch_field(result);
  mytest_r(field); /* no more fields */

  mysql_free_result(result);

  /* FIELD ALIAS TEST */
  rc = mysql_query(mysql, "SELECT DATE_FORMAT(ts,'%Y')  AS 'YEAR' FROM test_dateformat");
  myquery(rc);

  result = mysql_store_result(mysql);
  mytest(result);

  field = mysql_fetch_field(result);
  mytest(field);
  fprintf(stdout,"\n field name: `%s` (expected: `%s`)", field->name, "YEAR");
  fprintf(stdout,"\n field org name: `%s` (expected: `%s`)",field->org_name,"");
  assert(strcmp(field->name, "YEAR")==0);
  assert(field->org_name[0] == '\0');

  field = mysql_fetch_field(result);
  mytest_r(field); /* no more fields */

  mysql_free_result(result);
}


/* Multiple stmts .. */
static void test_multi_stmt()
{

  MYSQL_STMT  *stmt, *stmt1, *stmt2;
  int         rc, id;
  char        name[50];
  MYSQL_BIND  bind[2];
  ulong       length[2];
  my_bool     is_null[2];
  myheader("test_multi_stmt");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_multi_table");
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_multi_table(id int, name char(20))");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_multi_table values(10,'mysql')");
  myquery(rc);

  stmt = mysql_simple_prepare(mysql, "SELECT * FROM test_multi_table WHERE id = ?");
  check_stmt(stmt);

  stmt2 = mysql_simple_prepare(mysql, "UPDATE test_multi_table SET name='updated' WHERE id=10");
  check_stmt(stmt2);

  verify_param_count(stmt,1);

  bind[0].buffer_type= MYSQL_TYPE_SHORT;
  bind[0].buffer= (char *)&id;
  bind[0].is_null= &is_null[0];
  bind[0].buffer_length= 0;
  bind[0].length= &length[0];
  is_null[0]= 0;
  length[0]= 0;

  bind[1].buffer_type = MYSQL_TYPE_STRING;
  bind[1].buffer = (char *)name;
  bind[1].buffer_length= sizeof(name);
  bind[1].length = &length[1];
  bind[1].is_null= &is_null[1];
    
  rc = mysql_bind_param(stmt, bind);
  check_execute(stmt, rc);
  
  rc = mysql_bind_result(stmt, bind);
  check_execute(stmt, rc);

  id = 10;
  rc = mysql_execute(stmt);
  check_execute(stmt, rc);
  
  id = 999;  
  rc = mysql_fetch(stmt);
  check_execute(stmt, rc);

  fprintf(stdout, "\n int_data: %d(%lu)", id, length[0]);
  fprintf(stdout, "\n str_data: %s(%lu)", name, length[1]);
  assert(id == 10);
  assert(strcmp(name,"mysql")==0);

  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);

  /* alter the table schema now */
  stmt1 = mysql_simple_prepare(mysql,"DELETE FROM test_multi_table WHERE id = ? AND name=?");
  check_stmt(stmt1);

  verify_param_count(stmt1,2);

  rc = mysql_bind_param(stmt1, bind);
  check_execute(stmt1, rc);
  
  rc = mysql_execute(stmt2);
  check_execute(stmt2, rc);

  verify_st_affected_rows(stmt2, 1);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);
  
  rc = mysql_fetch(stmt);
  check_execute(stmt, rc);

  fprintf(stdout, "\n int_data: %d(%lu)", id, length[0]);
  fprintf(stdout, "\n str_data: %s(%lu)", name, length[1]);
  assert(id == 10);
  assert(strcmp(name,"updated")==0);

  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);

  rc = mysql_execute(stmt1);
  check_execute(stmt1, rc);

  verify_st_affected_rows(stmt1, 1);

  mysql_stmt_close(stmt1);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);

  assert(0 == my_stmt_result("SELECT * FROM test_multi_table"));

  mysql_stmt_close(stmt);
  mysql_stmt_close(stmt2);

}


/********************************************************
* to test simple sample - manual                        *
*********************************************************/
static void test_manual_sample()
{
  unsigned int param_count;
  MYSQL_STMT   *stmt;
  short        small_data;
  int          int_data;
  char         str_data[50];
  ulonglong    affected_rows;
  MYSQL_BIND   bind[3];
  my_bool      is_null;

  myheader("test_manual_sample");

  /*
    Sample which is incorporated directly in the manual under Prepared 
    statements section (Example from mysql_execute()
  */

  mysql_autocommit(mysql, 1);
  if (mysql_query(mysql,"DROP TABLE IF EXISTS test_table"))
  {
    fprintf(stderr, "\n drop table failed");
    fprintf(stderr, "\n %s", mysql_error(mysql));
    exit(0);
  }
  if (mysql_query(mysql,"CREATE TABLE test_table(col1 int, col2 varchar(50), \
                                                 col3 smallint,\
                                                 col4 timestamp(14))"))
  {
    fprintf(stderr, "\n create table failed");
    fprintf(stderr, "\n %s", mysql_error(mysql));
    exit(0);
  }
  
  /* Prepare a insert query with 3 parameters */
  strmov(query, "INSERT INTO test_table(col1,col2,col3) values(?,?,?)");
  if (!(stmt = mysql_simple_prepare(mysql,query)))
  {
    fprintf(stderr, "\n prepare, insert failed");
    fprintf(stderr, "\n %s", mysql_error(mysql));
    exit(0);
  }
  fprintf(stdout, "\n prepare, insert successful");

  /* Get the parameter count from the statement */
  param_count= mysql_param_count(stmt);

  fprintf(stdout, "\n total parameters in insert: %d", param_count);
  if (param_count != 3) /* validate parameter count */
  {
    fprintf(stderr, "\n invalid parameter count returned by MySQL");
    exit(0);
  }

  /* Bind the data for the parameters */

  /* INTEGER PART */
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (char *)&int_data;
  bind[0].is_null= 0;
  bind[0].length= 0;

  /* STRING PART */
  bind[1].buffer_type= MYSQL_TYPE_VAR_STRING;
  bind[1].buffer= (char *)str_data;
  bind[1].buffer_length= sizeof(str_data);
  bind[1].is_null= 0;
  bind[1].length= 0;
 
  /* SMALLINT PART */
  bind[2].buffer_type= MYSQL_TYPE_SHORT;
  bind[2].buffer= (char *)&small_data;       
  bind[2].is_null= &is_null;
  bind[2].length= 0;
  is_null= 0;

  /* Bind the buffers */
  if (mysql_bind_param(stmt, bind))
  {
    fprintf(stderr, "\n param bind failed");
    fprintf(stderr, "\n %s", mysql_stmt_error(stmt));
    exit(0);
  }

  /* Specify the data */
  int_data= 10;             /* integer */
  strmov(str_data,"MySQL"); /* string  */
  
  /* INSERT SMALLINT data as NULL */
  is_null= 1;

  /* Execute the insert statement - 1*/
  if (mysql_execute(stmt))
  {
    fprintf(stderr, "\n execute 1 failed");
    fprintf(stderr, "\n %s", mysql_stmt_error(stmt));
    exit(0);
  }
    
  /* Get the total rows affected */   
  affected_rows= mysql_stmt_affected_rows(stmt);

  fprintf(stdout, "\n total affected rows: %lld", affected_rows);
  if (affected_rows != 1) /* validate affected rows */
  {
    fprintf(stderr, "\n invalid affected rows by MySQL");
    exit(0);
  }

  /* Re-execute the insert, by changing the values */
  int_data= 1000;             
  strmov(str_data,"The most popular open source database"); 
  small_data= 1000;         /* smallint */
  is_null= 0;               /* reset */

  /* Execute the insert statement - 2*/
  if (mysql_execute(stmt))
  {
    fprintf(stderr, "\n execute 2 failed");
    fprintf(stderr, "\n %s", mysql_stmt_error(stmt));
    exit(0);
  }
    
  /* Get the total rows affected */   
  affected_rows= mysql_stmt_affected_rows(stmt);

  fprintf(stdout, "\n total affected rows: %lld", affected_rows);
  if (affected_rows != 1) /* validate affected rows */
  {
    fprintf(stderr, "\n invalid affected rows by MySQL");
    exit(0);
  }

  /* Close the statement */
  if (mysql_stmt_close(stmt))
  {
    fprintf(stderr, "\n failed while closing the statement");
    fprintf(stderr, "\n %s", mysql_stmt_error(stmt));
    exit(0);
  }
  assert(2 == my_stmt_result("SELECT * FROM test_table"));

  /* DROP THE TABLE */
  if (mysql_query(mysql,"DROP TABLE test_table"))
  {
    fprintf(stderr, "\n drop table failed");
    fprintf(stderr, "\n %s", mysql_error(mysql));
    exit(0);
  }
  fprintf(stdout, "Success !!!");
}


/********************************************************
* to test alter table scenario in the middle of prepare *
*********************************************************/
static void test_prepare_alter()
{
  MYSQL_STMT  *stmt;
  int         rc, id;
  long        length;
  MYSQL_BIND  bind[1];
  my_bool     is_null;

  myheader("test_prepare_alter");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prep_alter");
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prep_alter(id int, name char(20))");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_prep_alter values(10,'venu'),(20,'mysql')");
  myquery(rc);

  stmt = mysql_simple_prepare(mysql, "INSERT INTO test_prep_alter VALUES(?,'monty')");
  check_stmt(stmt);

  verify_param_count(stmt,1);

  is_null= 0;
  bind[0].buffer_type= MYSQL_TYPE_SHORT;
  bind[0].buffer= (char *)&id;
  bind[0].buffer_length= 0;
  bind[0].length= 0;
  bind[0].is_null= &is_null;

  rc = mysql_bind_param(stmt, bind);
  check_execute(stmt, rc);
  
  id = 30; length= 0;
  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  if (thread_query((char *)"ALTER TABLE test_prep_alter change id id_new varchar(20)"))
    exit(0);

  is_null=1;
  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  assert(4 == my_stmt_result("SELECT * FROM test_prep_alter"));

  mysql_stmt_close(stmt);
}

/********************************************************
* to test the support of multi-statement executions         *
*********************************************************/

static void test_multi_statements()
{
  MYSQL *mysql_local;
  MYSQL_RES *result;
  int    rc;

  const char *query="\
DROP TABLE IF EXISTS test_multi_tab;\
CREATE TABLE test_multi_tab(id int,name char(20));\
INSERT INTO test_multi_tab(id) VALUES(10),(20);\
INSERT INTO test_multi_tab VALUES(20,'insert;comma');\
SELECT * FROM test_multi_tab;\
UPDATE test_multi_tab SET name='new;name' WHERE id=20;\
DELETE FROM test_multi_tab WHERE name='new;name';\
SELECT * FROM test_multi_tab;\
DELETE FROM test_multi_tab WHERE id=10;\
SELECT * FROM test_multi_tab;\
DROP TABLE test_multi_tab;\
select 1;\
DROP TABLE IF EXISTS test_multi_tab";
  uint count, exp_value;
  uint rows[]= {0, 0, 2, 1, 3, 2, 2, 1, 1, 0, 0, 1, 0};

  myheader("test_multi_statements");

  /*
    First test that we get an error for multi statements
    (Becasue default connection is not opened with CLIENT_MULTI_STATEMENTS)
  */
  rc = mysql_query(mysql, query); /* syntax error */
  myquery_r(rc);

  assert(-1 == mysql_next_result(mysql));
  assert(0 == mysql_more_results(mysql));

  if (!(mysql_local = mysql_init(NULL)))
  { 
    fprintf(stdout,"\n mysql_init() failed");
    exit(1);
  }

  /* Create connection that supprot multi statements */
  if (!(mysql_real_connect(mysql_local,opt_host,opt_user,
			   opt_password, current_db, opt_port,
			   opt_unix_socket, CLIENT_MULTI_STATEMENTS)))
  {
    fprintf(stdout,"\n connection failed(%s)", mysql_error(mysql_local));
    exit(1);
  }

  rc = mysql_query(mysql_local, query);
  myquery(rc);

  for (count=0 ; count < array_elements(rows) ; count++)
  {
    fprintf(stdout,"\n Query %d: ", count);
    if ((result= mysql_store_result(mysql_local)))
    {
      my_process_result_set(result);
      mysql_free_result(result);
    }
    else
      fprintf(stdout,"OK, %lld row(s) affected, %d warning(s)\n",
	      mysql_affected_rows(mysql_local),
	      mysql_warning_count(mysql_local));

    exp_value= (uint) mysql_affected_rows(mysql_local);
    if (rows[count] !=  exp_value)
    {
      fprintf(stdout, "row %d  had affected rows: %d, should be %d\n",
	      count, exp_value, rows[count]);
      exit(1);
    }
    if (count != array_elements(rows) -1)
    {
      if (!(rc= mysql_more_results(mysql_local)))
      {
	fprintf(stdout,
		"mysql_more_result returned wrong value: %d for row %d\n",
		rc, count);
	exit(1);
      }
      if ((rc= mysql_next_result(mysql_local)))
      {
	exp_value= mysql_errno(mysql_local);

	exit(1);
      }
    }
    else
    {
      assert(mysql_more_results(mysql_local) == 0);
      assert(mysql_next_result(mysql_local) == -1);
    }
  }

  /* check that errors abort multi statements */

  rc= mysql_query(mysql_local, "select 1+1+a;select 1+1");
  myquery_r(rc);
  assert(mysql_more_results(mysql_local) == 0);
  assert(mysql_next_result(mysql_local) == -1);

  rc= mysql_query(mysql_local, "select 1+1;select 1+1+a;select 1");
  myquery(rc);
  result= mysql_store_result(mysql_local);
  mytest(result);
  mysql_free_result(result);
  assert(mysql_more_results(mysql_local) == 1);
  assert(mysql_next_result(mysql_local) > 0);

  /*
    Ensure that we can now do a simple query (this checks that the server is
    not trying to send us the results for the last 'select 1'
  */
  rc= mysql_query(mysql_local, "select 1+1+1");
  myquery(rc);
  result= mysql_store_result(mysql_local);
  mytest(result);
  my_process_result_set(result);
  mysql_free_result(result);

  mysql_close(mysql_local);
}

/********************************************************
* Check that Prepared statement cannot contain several  *
*  SQL statements                                       *
*********************************************************/
static void test_prepare_multi_statements()
{
  MYSQL *mysql_local;
  MYSQL_STMT *stmt;
  myheader("test_prepare_multi_statements");

  if (!(mysql_local = mysql_init(NULL)))
  { 
    fprintf(stdout,"\n mysql_init() failed");
    exit(1);
  }

  if (!(mysql_real_connect(mysql_local,opt_host,opt_user,
                           opt_password, current_db, opt_port,
                           opt_unix_socket, CLIENT_MULTI_STATEMENTS)))
  {
    fprintf(stdout,"\n connection failed(%s)", mysql_error(mysql_local));
    exit(1);
  }
  strmov(query, "select 1; select 'another value'");
  stmt = mysql_simple_prepare(mysql_local,query);
  check_stmt_r(stmt);
  mysql_close(mysql_local);
}

/********************************************************
* to test simple bind store result                      *
*********************************************************/
static void test_store_result()
{
  MYSQL_STMT *stmt;
  int        rc;
  long        nData;
  char       szData[100];
  MYSQL_BIND bind[2];
  ulong	     length, length1;
  my_bool    is_null[2];

  myheader("test_store_result");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_store_result");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_store_result(col1 int ,col2 varchar(50))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_store_result VALUES(10,'venu'),(20,'mysql')");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_store_result(col2) VALUES('monty')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* fetch */
  bind[0].buffer_type=FIELD_TYPE_LONG;
  bind[0].buffer= (char*) &nData;	/* integer data */
  bind[0].length= &length;
  bind[0].is_null= &is_null[0];

  length= 0; 
  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].buffer=szData;		/* string data */
  bind[1].buffer_length=sizeof(szData);
  bind[1].length= &length1;
  bind[1].is_null= &is_null[1];
  length1= 0;

  stmt = mysql_simple_prepare(mysql, "SELECT * FROM test_store_result");
  check_stmt(stmt);

  rc = mysql_bind_result(stmt,bind);
  check_execute(stmt, rc);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  rc = mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  fprintf(stdout,"\n row 1: %ld,%s(%lu)", nData, szData, length1);
  assert(nData == 10);
  assert(strcmp(szData,"venu")==0);
  assert(length1 == 4);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  fprintf(stdout,"\n row 2: %ld,%s(%lu)",nData, szData, length1);
  assert(nData == 20);
  assert(strcmp(szData,"mysql")==0);
  assert(length1 == 5);

  length=99;
  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);
 
  if (is_null[0])
    fprintf(stdout,"\n row 3: NULL,%s(%lu)", szData, length1);
  assert(is_null[0]);
  assert(strcmp(szData,"monty")==0);
  assert(length1 == 5);

  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);
  
  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  rc = mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  fprintf(stdout,"\n row 1: %ld,%s(%lu)",nData, szData, length1);
  assert(nData == 10);
  assert(strcmp(szData,"venu")==0);
  assert(length1 == 4);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  fprintf(stdout,"\n row 2: %ld,%s(%lu)",nData, szData, length1);
  assert(nData == 20);
  assert(strcmp(szData,"mysql")==0);
  assert(length1 == 5);

  length=99;
  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);
 
  if (is_null[0])
    fprintf(stdout,"\n row 3: NULL,%s(%lu)", szData, length1);
  assert(is_null[0]);
  assert(strcmp(szData,"monty")==0);
  assert(length1 == 5);

  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}


/********************************************************
* to test simple bind store result                      *
*********************************************************/
static void test_store_result1()
{
  MYSQL_STMT *stmt;
  int        rc;

  myheader("test_store_result1");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_store_result");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_store_result(col1 int ,col2 varchar(50))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_store_result VALUES(10,'venu'),(20,'mysql')");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_store_result(col2) VALUES('monty')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  stmt = mysql_simple_prepare(mysql,"SELECT * FROM test_store_result");
  check_stmt(stmt);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  rc = mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  rc = 0;
  while (mysql_fetch(stmt) != MYSQL_NO_DATA)
    rc++;
  fprintf(stdout, "\n total rows: %d", rc);
  assert(rc == 3);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  rc = mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  rc = 0;
  while (mysql_fetch(stmt) != MYSQL_NO_DATA)
    rc++;
  fprintf(stdout, "\n total rows: %d", rc);
  assert(rc == 3);

  mysql_stmt_close(stmt);
}


/********************************************************
* to test simple bind store result                      *
*********************************************************/
static void test_store_result2()
{
  MYSQL_STMT *stmt;
  int        rc;
  int        nData;
  long       length;
  MYSQL_BIND bind[1];

  myheader("test_store_result2");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_store_result");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_store_result(col1 int ,col2 varchar(50))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_store_result VALUES(10,'venu'),(20,'mysql')");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_store_result(col2) VALUES('monty')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  bind[0].buffer_type=FIELD_TYPE_LONG;
  bind[0].buffer= (char *) &nData;	/* integer data */
  bind[0].length= &length;
  bind[0].is_null= 0;

  strmov((char *)query , "SELECT col1 FROM test_store_result where col1= ?");
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  rc = mysql_bind_param(stmt,bind);
  check_execute(stmt, rc);

  rc = mysql_bind_result(stmt,bind);
  check_execute(stmt, rc);
  
  nData = 10; length= 0;
  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  nData = 0;
  rc = mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  fprintf(stdout,"\n row 1: %d",nData);
  assert(nData == 10);

  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);

  nData = 20;
  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  nData = 0;
  rc = mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  fprintf(stdout,"\n row 1: %d",nData);
  assert(nData == 20);

  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);
  mysql_stmt_close(stmt);
}


/********************************************************
* to test simple subselect prepare                      *
*********************************************************/
static void test_subselect()
{
#if TO_BE_FIXED_IN_SERVER

  MYSQL_STMT *stmt;
  int        rc, id;
  MYSQL_BIND bind[1];

  myheader("test_subselect");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_sub1");
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_sub2");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_sub1(id int)");
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_sub2(id int, id1 int)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_sub1 values(2)");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_sub2 VALUES(1,7),(2,7)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* fetch */
  bind[0].buffer_type= FIELD_TYPE_LONG;
  bind[0].buffer= (char *) &id;	
  bind[0].length= 0;
  bind[0].is_null= 0;

  stmt = mysql_simple_prepare(mysql, "INSERT INTO test_sub2(id) SELECT * FROM test_sub1 WHERE id=?", 100);
  check_stmt(stmt);

  rc = mysql_bind_param(stmt,bind);
  check_execute(stmt, rc);

  rc = mysql_bind_result(stmt,bind);
  check_execute(stmt, rc);

  id = 2; 
  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  verify_st_affected_rows(stmt, 1);

  id = 9; 
  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  verify_st_affected_rows(stmt, 0);

  mysql_stmt_close(stmt);

  assert(3 == my_stmt_result("SELECT * FROM test_sub2"));

  strmov((char *)query , "SELECT ROW(1,7) IN (select id, id1 from test_sub2 WHERE id1=?)");
  assert(1 == my_stmt_result("SELECT ROW(1,7) IN (select id, id1 from test_sub2 WHERE id1=8)"));
  assert(1 == my_stmt_result("SELECT ROW(1,7) IN (select id, id1 from test_sub2 WHERE id1=7)"));

  stmt = mysql_simple_prepare(mysql, query, 150);
  check_stmt(stmt);

  rc = mysql_bind_param(stmt,bind);
  check_execute(stmt, rc);

  rc = mysql_bind_result(stmt,bind);
  check_execute(stmt, rc);

  id = 7; 
  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  fprintf(stdout,"\n row 1: %d",id);
  assert(id == 1);

  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);
  
  id= 8;
  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  fprintf(stdout,"\n row 1: %d",id);
  assert(id == 0);

  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
#endif
}

/*
  Generalized conversion routine to handle DATE, TIME and DATETIME
  conversion using MYSQL_TIME structure
*/
static void test_bind_date_conv(uint row_count)
{ 
  MYSQL_STMT   *stmt= 0;
  uint         rc, i, count= row_count;
  ulong	       length[4];
  MYSQL_BIND   bind[4];
  my_bool      is_null[4]={0};
  MYSQL_TIME   tm[4]; 
  ulong        second_part;
  uint         year, month, day, hour, minute, sec;

  stmt = mysql_simple_prepare(mysql,"INSERT INTO test_date VALUES(?,?,?,?)");
  check_stmt(stmt);

  verify_param_count(stmt, 4);  

  bind[0].buffer_type= MYSQL_TYPE_TIMESTAMP;
  bind[1].buffer_type= MYSQL_TYPE_TIME;
  bind[2].buffer_type= MYSQL_TYPE_DATETIME;
  bind[3].buffer_type= MYSQL_TYPE_DATE;
 
  second_part= 0;

  year=2000;
  month=01;
  day=10;

  hour=11;
  minute=16;
  sec= 20;

  for (i= 0; i < (int) array_elements(bind); i++)
  {  
    bind[i].buffer= (char *) &tm[i];
    bind[i].is_null= &is_null[i];
    bind[i].length= &length[i];
    bind[i].buffer_length= 30;
    length[i]=20;
  }   

  rc = mysql_bind_param(stmt, bind);
  check_execute(stmt,rc);
 
  for (count= 0; count < row_count; count++)
  { 
    for (i= 0; i < (int) array_elements(bind); i++)
    {
      tm[i].neg= 0;   
      tm[i].second_part= second_part+count;
      tm[i].year= year+count;
      tm[i].month= month+count;
      tm[i].day= day+count;
      tm[i].hour= hour+count;
      tm[i].minute= minute+count;
      tm[i].second= sec+count;
    }   
    rc = mysql_execute(stmt);
    check_execute(stmt, rc);    
  }

  rc = mysql_commit(mysql);
  myquery(rc);

  mysql_stmt_close(stmt);

  assert(row_count == my_stmt_result("SELECT * FROM test_date"));

  stmt = mysql_simple_prepare(mysql,"SELECT * FROM test_date");
  myquery(rc);

  rc = mysql_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  rc = mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  for (count=0; count < row_count; count++)
  {
    rc = mysql_fetch(stmt);
    check_execute(stmt,rc);

    fprintf(stdout, "\n");
    for (i= 0; i < array_elements(bind); i++)
    {  
      fprintf(stdout, "\n");
      fprintf(stdout," time[%d]: %02d-%02d-%02d %02d:%02d:%02d.%02lu",
                      i, tm[i].year, tm[i].month, tm[i].day, 
                      tm[i].hour, tm[i].minute, tm[i].second,
                      tm[i].second_part);                      

      assert(tm[i].year == 0 || tm[i].year == year+count);
      assert(tm[i].month == 0 || tm[i].month == month+count);
      assert(tm[i].day == 0 || tm[i].day == day+count);

      assert(tm[i].hour == 0 || tm[i].hour == hour+count);
      /* 
         minute causes problems from date<->time, don't assert, instead
         validate separatly in another routine
       */
      /*assert(tm[i].minute == 0 || tm[i].minute == minute+count);
      assert(tm[i].second == 0 || tm[i].second == sec+count);*/

      assert(tm[i].second_part == 0 || tm[i].second_part == second_part+count);
    }
  }
  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}

/*
  Test DATE, TIME, DATETIME and TS with MYSQL_TIME conversion
*/

static void test_date()
{
  int        rc;

  myheader("test_date");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_date");
  myquery(rc);
  
  rc= mysql_query(mysql,"CREATE TABLE test_date(c1 TIMESTAMP(14), \
                                                 c2 TIME,\
                                                 c3 DATETIME,\
                                                 c4 DATE)");

  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  test_bind_date_conv(5);
}

/*
  Test all time types to DATE and DATE to all types
*/

static void test_date_date()
{
  int        rc;

  myheader("test_date_date");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_date");
  myquery(rc);
  
  rc= mysql_query(mysql,"CREATE TABLE test_date(c1 DATE, \
                                                 c2 DATE,\
                                                 c3 DATE,\
                                                 c4 DATE)");

  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  test_bind_date_conv(3);
}

/*
  Test all time types to TIME and TIME to all types
*/

static void test_date_time()
{
  int        rc;

  myheader("test_date_time");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_date");
  myquery(rc);
  
  rc= mysql_query(mysql,"CREATE TABLE test_date(c1 TIME, \
                                                 c2 TIME,\
                                                 c3 TIME,\
                                                 c4 TIME)");

  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  test_bind_date_conv(3);
}

/*
  Test all time types to TIMESTAMP and TIMESTAMP to all types
*/

static void test_date_ts()
{
  int        rc;

  myheader("test_date_ts");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_date");
  myquery(rc);
  
  rc= mysql_query(mysql,"CREATE TABLE test_date(c1 TIMESTAMP(10), \
                                                 c2 TIMESTAMP(14),\
                                                 c3 TIMESTAMP,\
                                                 c4 TIMESTAMP(6))");

  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  test_bind_date_conv(2);
}

/*
  Test all time types to DATETIME and DATETIME to all types
*/

static void test_date_dt()
{
  int        rc;

  myheader("test_date_dt");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_date");
  myquery(rc);
  
  rc= mysql_query(mysql,"CREATE TABLE test_date(c1 datetime, \
                                                 c2 datetime,\
                                                 c3 datetime,\
                                                 c4 date)");

  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  test_bind_date_conv(2);
}

/*
  Misc tests to keep pure coverage happy
*/
static void test_pure_coverage()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[1];
  int        rc;
  ulong      length;
  
  myheader("test_pure_coverage");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_pure");
  myquery(rc);
  
  rc = mysql_query(mysql,"CREATE TABLE test_pure(c1 int, c2 varchar(20))");
  myquery(rc);

  stmt = mysql_simple_prepare(mysql,"insert into test_pure(c67788) values(10)");
  check_stmt_r(stmt);
  
  /* Query without params and result should allow to bind 0 arrays */
  stmt = mysql_simple_prepare(mysql,"insert into test_pure(c2) values(10)");
  check_stmt(stmt);
  
  rc = mysql_bind_param(stmt, (MYSQL_BIND*)0);
  check_execute(stmt, rc);
  
  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  rc = mysql_bind_result(stmt, (MYSQL_BIND*)0);
  check_execute(stmt, rc);
  
  mysql_stmt_close(stmt);

  stmt = mysql_simple_prepare(mysql,"insert into test_pure(c2) values(?)");
  check_stmt(stmt);

  bind[0].length= &length;
  bind[0].is_null= 0;
  bind[0].buffer_length= 0;

  bind[0].buffer_type= MYSQL_TYPE_GEOMETRY;
  rc = mysql_bind_param(stmt, bind);
  check_execute_r(stmt, rc); /* unsupported buffer type */
  
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  rc = mysql_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc = mysql_stmt_store_result(stmt);
  check_execute(stmt, rc); 

  mysql_stmt_close(stmt);

  stmt = mysql_simple_prepare(mysql,"select * from test_pure");
  check_execute(stmt, rc);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  bind[0].buffer_type= MYSQL_TYPE_GEOMETRY;
  rc = mysql_bind_result(stmt, bind);
  check_execute_r(stmt, rc); /* unsupported buffer type */

  rc = mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);
  
  rc = mysql_stmt_store_result(stmt);
  check_execute_r(stmt, rc); /* commands out of sync */

  mysql_stmt_close(stmt);

  mysql_query(mysql,"DROP TABLE test_pure");
  mysql_commit(mysql);
}

/*
  test for string buffer fetch
*/

static void test_buffers()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[1];
  int        rc;
  ulong      length;
  my_bool    is_null;
  char       buffer[20];
  
  myheader("test_buffers");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_buffer");
  myquery(rc);
  
  rc = mysql_query(mysql,"CREATE TABLE test_buffer(str varchar(20))");
  myquery(rc);

  rc = mysql_query(mysql,"insert into test_buffer values('MySQL')\
                          ,('Database'),('Open-Source'),('Popular')");
  myquery(rc);
  
  stmt = mysql_simple_prepare(mysql,"select str from test_buffer");
  check_stmt(stmt);
  
  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  bzero(buffer, 20);		/* Avoid overruns in printf() */

  bind[0].length= &length;
  bind[0].is_null= &is_null;
  bind[0].buffer_length= 1;
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (char *)buffer;
  
  rc = mysql_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc = mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  buffer[1]='X';
  rc = mysql_fetch(stmt);
  check_execute(stmt, rc);
  fprintf(stdout, "\n data: %s (%lu)", buffer, length);
  assert(buffer[0] == 'M');
  assert(buffer[1] == 'X');
  assert(length == 5);

  bind[0].buffer_length=8;
  rc = mysql_bind_result(stmt, bind);/* re-bind */
  check_execute(stmt, rc);
  
  rc = mysql_fetch(stmt);
  check_execute(stmt, rc);
  fprintf(stdout, "\n data: %s (%lu)", buffer, length);
  assert(strncmp(buffer,"Database",8) == 0);
  assert(length == 8);
  
  bind[0].buffer_length=12;
  rc = mysql_bind_result(stmt, bind);/* re-bind */
  check_execute(stmt, rc);
  
  rc = mysql_fetch(stmt);
  check_execute(stmt, rc);
  fprintf(stdout, "\n data: %s (%lu)", buffer, length);
  assert(strcmp(buffer,"Open-Source") == 0);
  assert(length == 11);
  
  bind[0].buffer_length=6;
  rc = mysql_bind_result(stmt, bind);/* re-bind */
  check_execute(stmt, rc);
  
  rc = mysql_fetch(stmt);
  check_execute(stmt, rc);
  fprintf(stdout, "\n data: %s (%lu)", buffer, length);
  assert(strncmp(buffer,"Popula",6) == 0);
  assert(length == 7);
  
  mysql_stmt_close(stmt);
}

/* 
 Test the direct query execution in the middle of open stmts 
*/
static void test_open_direct()
{
  MYSQL_STMT  *stmt;
  MYSQL_RES   *result;
  int         rc;
  
  myheader("test_open_direct");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_open_direct");
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_open_direct(id int, name char(6))");
  myquery(rc);

  stmt = mysql_simple_prepare(mysql,"INSERT INTO test_open_direct values(10,'mysql')");
  check_stmt(stmt);

  rc = mysql_query(mysql, "SELECT * FROM test_open_direct");
  myquery(rc);

  result = mysql_store_result(mysql);
  mytest(result);

  assert(0 == my_process_result_set(result));
  mysql_free_result(result);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  verify_st_affected_rows(stmt, 1);
  
  rc = mysql_query(mysql, "SELECT * FROM test_open_direct");
  myquery(rc);

  result = mysql_store_result(mysql);
  mytest(result);

  assert(1 == my_process_result_set(result));
  mysql_free_result(result);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  verify_st_affected_rows(stmt, 1);
  
  rc = mysql_query(mysql, "SELECT * FROM test_open_direct");
  myquery(rc);

  result = mysql_store_result(mysql);
  mytest(result);

  assert(2 == my_process_result_set(result));
  mysql_free_result(result);

  mysql_stmt_close(stmt);

  /* run a direct query in the middle of a fetch */
  stmt= mysql_simple_prepare(mysql,"SELECT * FROM test_open_direct");
  check_stmt(stmt);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt, rc);

  rc = mysql_query(mysql,"INSERT INTO test_open_direct(id) VALUES(20)");
  myquery_r(rc);

  rc = mysql_stmt_close(stmt);
  check_execute(stmt, rc);

  rc = mysql_query(mysql,"INSERT INTO test_open_direct(id) VALUES(20)");
  myquery(rc);

  /* run a direct query with store result */
  stmt= mysql_simple_prepare(mysql,"SELECT * FROM test_open_direct");
  check_stmt(stmt);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  rc = mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt, rc);

  rc = mysql_query(mysql,"drop table test_open_direct");
  myquery(rc);

  rc = mysql_stmt_close(stmt);
  check_execute(stmt, rc);
}

/*
  To test fetch without prior bound buffers
*/
static void test_fetch_nobuffs()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[4];
  char       str[4][50];
  int        rc;

  myheader("test_fetch_nobuffs");

  stmt = mysql_simple_prepare(mysql,"SELECT DATABASE(), CURRENT_USER(), \
                              CURRENT_DATE(), CURRENT_TIME()");
  check_stmt(stmt);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  rc = 0;
  while (mysql_fetch(stmt) != MYSQL_NO_DATA)
    rc++;

  fprintf(stdout, "\n total rows        : %d", rc);
  assert(rc == 1);

  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (char *)str[0];
  bind[0].is_null= 0;
  bind[0].length= 0;
  bind[0].buffer_length= sizeof(str[0]);
  bind[1]= bind[2]= bind[3]= bind[0];
  bind[1].buffer= (char *)str[1];
  bind[2].buffer= (char *)str[2];
  bind[3].buffer= (char *)str[3];

  rc = mysql_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  rc = 0;
  while (mysql_fetch(stmt) != MYSQL_NO_DATA)
  {
    rc++;
    fprintf(stdout, "\n CURRENT_DATABASE(): %s", str[0]);
    fprintf(stdout, "\n CURRENT_USER()    : %s", str[1]);
    fprintf(stdout, "\n CURRENT_DATE()    : %s", str[2]);
    fprintf(stdout, "\n CURRENT_TIME()    : %s", str[3]);
  }
  fprintf(stdout, "\n total rows        : %d", rc);
  assert(rc == 1);

  mysql_stmt_close(stmt);
}

/*
  To test a misc bug 
*/
static void test_ushort_bug()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[4];
  ushort     short_value;
  ulong      long_value;
  ulong      s_length, l_length, ll_length, t_length;
  ulonglong  longlong_value;
  int        rc;
  uchar      tiny_value;

  myheader("test_ushort_bug");

  rc= mysql_query(mysql,"DROP TABLE IF EXISTS test_ushort");
  myquery(rc);
  
  rc= mysql_query(mysql,"CREATE TABLE test_ushort(a smallint unsigned, \
                                                  b smallint unsigned, \
                                                  c smallint unsigned, \
                                                  d smallint unsigned)");
  myquery(rc);
  
  rc= mysql_query(mysql,"INSERT INTO test_ushort VALUES(35999, 35999, 35999, 200)");
  myquery(rc);
  
  
  stmt = mysql_simple_prepare(mysql,"SELECT * FROM test_ushort");
  check_stmt(stmt);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  bind[0].buffer_type= MYSQL_TYPE_SHORT;
  bind[0].buffer= (char *)&short_value;
  bind[0].is_null= 0;
  bind[0].length= &s_length;
  
  bind[1].buffer_type= MYSQL_TYPE_LONG;
  bind[1].buffer= (char *)&long_value;
  bind[1].is_null= 0;
  bind[1].length= &l_length;

  bind[2].buffer_type= MYSQL_TYPE_LONGLONG;
  bind[2].buffer= (char *)&longlong_value;
  bind[2].is_null= 0;
  bind[2].length= &ll_length;
  
  bind[3].buffer_type= MYSQL_TYPE_TINY;
  bind[3].buffer= (char *)&tiny_value;
  bind[3].is_null= 0;
  bind[3].length= &t_length;
  
  rc = mysql_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt, rc);
  
  fprintf(stdout,"\n ushort   : %d (%ld)", short_value, s_length);
  fprintf(stdout,"\n ulong    : %ld (%ld)", long_value, l_length);
  fprintf(stdout,"\n longlong : %lld (%ld)", longlong_value, ll_length);
  fprintf(stdout,"\n tinyint  : %d   (%ld)", tiny_value, t_length);
  
  assert(short_value == 35999);
  assert(s_length == 2);
  
  assert(long_value == 35999);
  assert(l_length == 4);

  assert(longlong_value == 35999);
  assert(ll_length == 8);

  assert(tiny_value == 200);
  assert(t_length == 1);
  
  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}

/*
  To test a misc smallint-signed conversion bug 
*/
static void test_sshort_bug()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[4];
  short      short_value;
  long       long_value;
  ulong      s_length, l_length, ll_length, t_length;
  ulonglong  longlong_value;
  int        rc;
  uchar      tiny_value;

  myheader("test_sshort_bug");

  rc= mysql_query(mysql,"DROP TABLE IF EXISTS test_sshort");
  myquery(rc);
  
  rc= mysql_query(mysql,"CREATE TABLE test_sshort(a smallint signed, \
                                                  b smallint signed, \
                                                  c smallint unsigned, \
                                                  d smallint unsigned)");
  myquery(rc);
  
  rc= mysql_query(mysql,"INSERT INTO test_sshort VALUES(-5999, -5999, 35999, 200)");
  myquery(rc);
  
  
  stmt = mysql_simple_prepare(mysql,"SELECT * FROM test_sshort");
  check_stmt(stmt);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  bind[0].buffer_type= MYSQL_TYPE_SHORT;
  bind[0].buffer= (char *)&short_value;
  bind[0].is_null= 0;
  bind[0].length= &s_length;
  
  bind[1].buffer_type= MYSQL_TYPE_LONG;
  bind[1].buffer= (char *)&long_value;
  bind[1].is_null= 0;
  bind[1].length= &l_length;

  bind[2].buffer_type= MYSQL_TYPE_LONGLONG;
  bind[2].buffer= (char *)&longlong_value;
  bind[2].is_null= 0;
  bind[2].length= &ll_length;
  
  bind[3].buffer_type= MYSQL_TYPE_TINY;
  bind[3].buffer= (char *)&tiny_value;
  bind[3].is_null= 0;
  bind[3].length= &t_length;
  
  rc = mysql_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt, rc);
  
  fprintf(stdout,"\n sshort   : %d (%ld)", short_value, s_length);
  fprintf(stdout,"\n slong    : %ld (%ld)", long_value, l_length);
  fprintf(stdout,"\n longlong : %lld (%ld)", longlong_value, ll_length);
  fprintf(stdout,"\n tinyint  : %d   (%ld)", tiny_value, t_length);
  
  assert(short_value == -5999);
  assert(s_length == 2);
  
  assert(long_value == -5999);
  assert(l_length == 4);

  assert(longlong_value == 35999);
  assert(ll_length == 8);

  assert(tiny_value == 200);
  assert(t_length == 1);
  
  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}

/*
  To test a misc tinyint-signed conversion bug 
*/
static void test_stiny_bug()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[4];
  short      short_value;
  long       long_value;
  ulong      s_length, l_length, ll_length, t_length;
  ulonglong  longlong_value;
  int        rc;
  uchar      tiny_value;

  myheader("test_stiny_bug");

  rc= mysql_query(mysql,"DROP TABLE IF EXISTS test_stiny");
  myquery(rc);
  
  rc= mysql_query(mysql,"CREATE TABLE test_stiny(a tinyint signed, \
                                                  b tinyint signed, \
                                                  c tinyint unsigned, \
                                                  d tinyint unsigned)");
  myquery(rc);
  
  rc= mysql_query(mysql,"INSERT INTO test_stiny VALUES(-128, -127, 255, 0)");
  myquery(rc);
  
  
  stmt = mysql_simple_prepare(mysql,"SELECT * FROM test_stiny");
  check_stmt(stmt);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  bind[0].buffer_type= MYSQL_TYPE_SHORT;
  bind[0].buffer= (char *)&short_value;
  bind[0].is_null= 0;
  bind[0].length= &s_length;
  
  bind[1].buffer_type= MYSQL_TYPE_LONG;
  bind[1].buffer= (char *)&long_value;
  bind[1].is_null= 0;
  bind[1].length= &l_length;

  bind[2].buffer_type= MYSQL_TYPE_LONGLONG;
  bind[2].buffer= (char *)&longlong_value;
  bind[2].is_null= 0;
  bind[2].length= &ll_length;
  
  bind[3].buffer_type= MYSQL_TYPE_TINY;
  bind[3].buffer= (char *)&tiny_value;
  bind[3].is_null= 0;
  bind[3].length= &t_length;
  
  rc = mysql_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt, rc);
  
  fprintf(stdout,"\n sshort   : %d (%ld)", short_value, s_length);
  fprintf(stdout,"\n slong    : %ld (%ld)", long_value, l_length);
  fprintf(stdout,"\n longlong : %lld  (%ld)", longlong_value, ll_length);
  fprintf(stdout,"\n tinyint  : %d    (%ld)", tiny_value, t_length);
  
  assert(short_value == -128);
  assert(s_length == 2);
  
  assert(long_value == -127);
  assert(l_length == 4);

  assert(longlong_value == 255);
  assert(ll_length == 8);

  assert(tiny_value == 0);
  assert(t_length == 1);
  
  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}

/********************************************************
* to test misc field information, bug: #74              *
*********************************************************/
static void test_field_misc()
{
  MYSQL_STMT  *stmt;
  MYSQL_RES   *result;
  MYSQL_BIND  bind[1];
  char        table_type[NAME_LEN];
  ulong       type_length;
  int         rc;

  myheader("test_field_misc");

  rc = mysql_query(mysql,"SELECT @@autocommit");
  myquery(rc);

  result = mysql_store_result(mysql);
  mytest(result);

  assert(1 == my_process_result_set(result));
  
  verify_prepare_field(result,0,
                       "@@autocommit","",   /* field and its org name */
                       MYSQL_TYPE_LONGLONG, /* field type */
                       "", "",              /* table and its org name */
                       "",1,0);             /* db name, length(its bool flag)*/

  mysql_free_result(result);
  
  stmt = mysql_simple_prepare(mysql,"SELECT @@autocommit");
  check_stmt(stmt);
  
  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  result = mysql_get_metadata(stmt);
  mytest(result);
  
  assert(1 == my_process_stmt_result(stmt));
  
  verify_prepare_field(result,0,
                       "@@autocommit","",   /* field and its org name */
                       MYSQL_TYPE_LONGLONG, /* field type */
                       "", "",              /* table and its org name */
                       "",1,0);             /* db name, length(its bool flag)*/

  mysql_free_result(result);
  mysql_stmt_close(stmt);

  stmt = mysql_simple_prepare(mysql, "SELECT @@table_type");
  check_stmt(stmt);

  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= table_type;
  bind[0].length= &type_length;
  bind[0].is_null= 0;
  bind[0].buffer_length= NAME_LEN;

  rc = mysql_bind_result(stmt, bind);
  check_execute(stmt,rc);    

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);
  fprintf(stdout,"\n default table type: %s(%ld)", table_type, type_length);

  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);

  stmt = mysql_simple_prepare(mysql, "SELECT @@table_type");
  check_stmt(stmt);

  result = mysql_get_metadata(stmt);
  mytest(result);

  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  assert(1 == my_process_stmt_result(stmt));
  
  verify_prepare_field(result,0,
                       "@@table_type","",   /* field and its org name */
                       MYSQL_TYPE_STRING,   /* field type */
                       "", "",              /* table and its org name */
                       "",type_length*3,0);   /* db name, length */

  mysql_free_result(result);
  mysql_stmt_close(stmt);

  stmt = mysql_simple_prepare(mysql, "SELECT @@max_error_count");
  check_stmt(stmt);

  result = mysql_get_metadata(stmt);
  mytest(result);

  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  assert(1 == my_process_stmt_result(stmt));
  
  verify_prepare_field(result,0,
                       "@@max_error_count","",   /* field and its org name */
                       MYSQL_TYPE_LONGLONG, /* field type */
                       "", "",              /* table and its org name */
                       "",10,0);            /* db name, length */

  mysql_free_result(result);
  mysql_stmt_close(stmt);
  
  stmt = mysql_simple_prepare(mysql, "SELECT @@max_allowed_packet");
  check_stmt(stmt);

  result = mysql_get_metadata(stmt);
  mytest(result);

  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  assert(1 == my_process_stmt_result(stmt));
  
  verify_prepare_field(result,0,
                       "@@max_allowed_packet","",   /* field and its org name */
                       MYSQL_TYPE_LONGLONG, /* field type */
                       "", "",              /* table and its org name */
                       "",10,0);            /* db name, length */

  mysql_free_result(result);
  mysql_stmt_close(stmt);
  
  stmt = mysql_simple_prepare(mysql, "SELECT @@sql_warnings");
  check_stmt(stmt);
  
  result = mysql_get_metadata(stmt);
  mytest(result);

  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  assert(1 == my_process_stmt_result(stmt));
  
  verify_prepare_field(result,0,
                       "@@sql_warnings","",   /* field and its org name */
                       MYSQL_TYPE_LONGLONG,   /* field type */
                       "", "",                /* table and its org name */
                       "",1,0);               /* db name, length */

  mysql_free_result(result);
  mysql_stmt_close(stmt);
}


/*
  To test SET OPTION feature with prepare stmts
  bug #85 (reported by mark@mysql.com)
*/
static void test_set_option()
{
  MYSQL_STMT *stmt;
  MYSQL_RES  *result;
  int        rc;

  myheader("test_set_option");

  mysql_autocommit(mysql, TRUE);

  /* LIMIT the rows count to 2 */
  rc= mysql_query(mysql,"SET OPTION SQL_SELECT_LIMIT=2");
  myquery(rc);

  rc= mysql_query(mysql,"DROP TABLE IF EXISTS test_limit");
  myquery(rc);
  
  rc= mysql_query(mysql,"CREATE TABLE test_limit(a tinyint)");
  myquery(rc);
  
  rc= mysql_query(mysql,"INSERT INTO test_limit VALUES(10),(20),(30),(40)");
  myquery(rc);  
  
  fprintf(stdout,"\n with SQL_SELECT_LIMIT=2 (direct)");
  rc = mysql_query(mysql,"SELECT * FROM test_limit");
  myquery(rc);

  result = mysql_store_result(mysql);
  mytest(result);

  assert(2 == my_process_result_set(result));

  mysql_free_result(result);
  
  fprintf(stdout,"\n with SQL_SELECT_LIMIT=2 (prepare)");  
  stmt = mysql_simple_prepare(mysql, "SELECT * FROM test_limit");
  check_stmt(stmt);

  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  assert(2 == my_process_stmt_result(stmt));

  mysql_stmt_close(stmt);

  /* RESET the LIMIT the rows count to 0 */  
  fprintf(stdout,"\n with SQL_SELECT_LIMIT=DEFAULT (prepare)");
  rc= mysql_query(mysql,"SET OPTION SQL_SELECT_LIMIT=DEFAULT");
  myquery(rc);
  
  stmt = mysql_simple_prepare(mysql, "SELECT * FROM test_limit");
  check_stmt(stmt);

  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  assert(4 == my_process_stmt_result(stmt));

  mysql_stmt_close(stmt);
}

/*
  To test a misc GRANT option
  bug #89 (reported by mark@mysql.com)
*/
static void test_prepare_grant()
{
  int rc;

  myheader("test_prepare_grant");

  mysql_autocommit(mysql, TRUE);

  rc= mysql_query(mysql,"DROP TABLE IF EXISTS test_grant");
  myquery(rc);
  
  rc= mysql_query(mysql,"CREATE TABLE test_grant(a tinyint primary key auto_increment)");
  myquery(rc);

  strxmov(query,"GRANT INSERT,UPDATE,SELECT ON ", current_db,
                ".test_grant TO 'test_grant'@",
                opt_host ? opt_host : "'localhost'", NullS);

  if (mysql_query(mysql,query))
  {
    myerror("GRANT failed");
    
    /* 
       If server started with --skip-grant-tables, skip this test, else
       exit to indicate an error

       ER_UNKNOWN_COM_ERROR = 1047
     */ 
    if (mysql_errno(mysql) != 1047)  
      exit(0);  
  }
  else
  {
    MYSQL *org_mysql= mysql, *lmysql;
    MYSQL_STMT *stmt;
    
    fprintf(stdout, "\n Establishing a test connection ...");
    if (!(lmysql = mysql_init(NULL)))
    { 
      myerror("mysql_init() failed");
      exit(0);
    }
    if (!(mysql_real_connect(lmysql,opt_host,"test_grant",
			     "", current_db, opt_port,
			     opt_unix_socket, 0)))
    {
      myerror("connection failed");   
      mysql_close(lmysql);
      exit(0);
    }   
    fprintf(stdout," OK");

    mysql= lmysql;
    rc = mysql_query(mysql,"INSERT INTO test_grant VALUES(NULL)");
    myquery(rc);

    rc = mysql_query(mysql,"INSERT INTO test_grant(a) VALUES(NULL)");
    myquery(rc);
    
    execute_prepare_query("INSERT INTO test_grant(a) VALUES(NULL)",1);    
    execute_prepare_query("INSERT INTO test_grant VALUES(NULL)",1);
    execute_prepare_query("UPDATE test_grant SET a=9 WHERE a=1",1);
    assert(4 == my_stmt_result("SELECT a FROM test_grant"));

    /* Both DELETE expected to fail as user does not have DELETE privs */

    rc = mysql_query(mysql,"DELETE FROM test_grant");
    myquery_r(rc);

    stmt= mysql_simple_prepare(mysql,"DELETE FROM test_grant");
    check_stmt_r(stmt);
    
    assert(4 == my_stmt_result("SELECT * FROM test_grant"));
    
    mysql_close(lmysql);        
    mysql= org_mysql;

    rc = mysql_query(mysql,"delete from mysql.user where User='test_grant'");
    myquery(rc);
    assert(1 == mysql_affected_rows(mysql));

    rc = mysql_query(mysql,"delete from mysql.tables_priv where User='test_grant'");
    myquery(rc);
    assert(1 == mysql_affected_rows(mysql));

  }
}


/*
  To test a crash when invalid/corrupted .frm is used in the 
  SHOW TABLE STATUS
  bug #93 (reported by serg@mysql.com).
*/
static void test_frm_bug()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[2];
  MYSQL_RES  *result;
  MYSQL_ROW  row;
  FILE       *test_file;
  char       data_dir[NAME_LEN];
  char       test_frm[255];
  int        rc;

  myheader("test_frm_bug");

  mysql_autocommit(mysql, TRUE);

  rc= mysql_query(mysql,"drop table if exists test_frm_bug");
  myquery(rc);

  rc= mysql_query(mysql,"flush tables");
  myquery(rc);
  
  stmt = mysql_simple_prepare(mysql, "show variables like 'datadir'");
  check_stmt(stmt);

  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= data_dir;
  bind[0].buffer_length= NAME_LEN;
  bind[0].is_null= 0;
  bind[0].length= 0;
  bind[1]=bind[0];

  rc = mysql_bind_result(stmt,bind);
  check_execute(stmt,rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  fprintf(stdout,"\n data directory: %s", data_dir);

  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);

  strxmov(test_frm,data_dir,"/",current_db,"/","test_frm_bug.frm",NullS);

  fprintf(stdout,"\n test_frm: %s", test_frm);

  if (!(test_file= my_fopen(test_frm, (int) (O_RDWR | O_CREAT), MYF(MY_WME))))
  {
    fprintf(stdout,"\n ERROR: my_fopen failed for '%s'", test_frm);
    fprintf(stdout,"\n test cancelled");
    return;  
  }
  fprintf(test_file,"this is a junk file for test");

  rc = mysql_query(mysql,"SHOW TABLE STATUS like 'test_frm_bug'");
  myquery(rc);

  result = mysql_store_result(mysql);
  mytest(result);/* It can't be NULL */

  assert(1 == my_process_result_set(result));

  mysql_data_seek(result,0);

  row= mysql_fetch_row(result);
  mytest(row);

  fprintf(stdout,"\n Comment: %s", row[16]);
  assert(row[16] != 0);

  mysql_free_result(result);
  mysql_stmt_close(stmt);

  my_fclose(test_file,MYF(0));
  mysql_query(mysql,"drop table if exists test_frm_bug");
}

/*
  To test DECIMAL conversion 
*/
static void test_decimal_bug()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[1];
  char       data[30];
  int        rc;
  my_bool    is_null;

  myheader("test_decimal_bug");

  mysql_autocommit(mysql, TRUE);

  rc= mysql_query(mysql,"drop table if exists test_decimal_bug");
  myquery(rc);
  
  rc = mysql_query(mysql, "create table test_decimal_bug(c1 decimal(10,2))");
  myquery(rc);
  
  rc = mysql_query(mysql, "insert into test_decimal_bug value(8),(10.22),(5.61)");
  myquery(rc);

  stmt = mysql_simple_prepare(mysql,"select c1 from test_decimal_bug where c1= ?");
  check_stmt(stmt);

  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (char *)data;
  bind[0].buffer_length= 25;
  bind[0].is_null= &is_null;
  bind[0].length= 0;

  is_null= 0; 
  rc = mysql_bind_param(stmt, bind);
  check_execute(stmt,rc);

  strcpy(data, "8.0");
  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  data[0]=0;
  rc = mysql_bind_result(stmt, bind);
  check_execute(stmt,rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  fprintf(stdout, "\n data: %s", data);
  assert(strcmp(data, "8.00")==0);

  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);

  strcpy(data, "5.61");
  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  data[0]=0;
  rc = mysql_bind_result(stmt, bind);
  check_execute(stmt,rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  fprintf(stdout, "\n data: %s", data);
  assert(strcmp(data, "5.61")==0);

  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);

  is_null= 1;
  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);

  strcpy(data, "10.22"); is_null= 0;
  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  data[0]=0; 
  rc = mysql_bind_result(stmt, bind);
  check_execute(stmt,rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  fprintf(stdout, "\n data: %s", data);
  assert(strcmp(data, "10.22")==0);

  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}


/*
  To test EXPLAIN bug
  bug #115 (reported by mark@mysql.com & georg@php.net).
*/

static void test_explain_bug()
{
  MYSQL_STMT *stmt;
  MYSQL_RES  *result;
  int        rc;

  myheader("test_explain_bug");

  mysql_autocommit(mysql,TRUE);

  rc = mysql_query(mysql, "DROP TABLE IF EXISTS test_explain");
  myquery(rc);
  
  rc = mysql_query(mysql, "CREATE TABLE test_explain(id int, name char(2))");
  myquery(rc);

  stmt = mysql_simple_prepare(mysql, "explain test_explain");
  check_stmt(stmt);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  assert( 2 == my_process_stmt_result(stmt));  

  result = mysql_get_metadata(stmt);
  mytest(result);

  fprintf(stdout, "\n total fields in the result: %d", 
          mysql_num_fields(result));
  assert(6 == mysql_num_fields(result));

  verify_prepare_field(result,0,"Field","",MYSQL_TYPE_VAR_STRING,
                       "","","",NAME_LEN,0);

  verify_prepare_field(result,1,"Type","",MYSQL_TYPE_VAR_STRING,
                       "","","",40,0);

  verify_prepare_field(result,2,"Null","",MYSQL_TYPE_VAR_STRING,
                       "","","",1,0);

  verify_prepare_field(result,3,"Key","",MYSQL_TYPE_VAR_STRING,
                       "","","",3,0);

  verify_prepare_field(result,4,"Default","",MYSQL_TYPE_VAR_STRING,
                       "","","",NAME_LEN,0);

  verify_prepare_field(result,5,"Extra","",MYSQL_TYPE_VAR_STRING,
                       "","","",20,0);

  mysql_free_result(result);
  mysql_stmt_close(stmt);
  
  stmt = mysql_simple_prepare(mysql, "explain select id, name FROM test_explain");
  check_stmt(stmt);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  assert( 1 == my_process_stmt_result(stmt)); 

  result = mysql_get_metadata(stmt);
  mytest(result);

  fprintf(stdout, "\n total fields in the result: %d", 
          mysql_num_fields(result));
  assert(10 == mysql_num_fields(result));

  verify_prepare_field(result,0,"id","",MYSQL_TYPE_LONGLONG,
                       "","","",3,0);

  verify_prepare_field(result,1,"select_type","",MYSQL_TYPE_VAR_STRING,
                       "","","",19,0);

  verify_prepare_field(result,2,"table","",MYSQL_TYPE_VAR_STRING,
                       "","","",NAME_LEN,0);

  verify_prepare_field(result,3,"type","",MYSQL_TYPE_VAR_STRING,
                       "","","",10,0);

  verify_prepare_field(result,4,"possible_keys","",MYSQL_TYPE_VAR_STRING,
                       "","","",NAME_LEN*64,0);

  verify_prepare_field(result,5,"key","",MYSQL_TYPE_VAR_STRING,
                       "","","",NAME_LEN,0);

  verify_prepare_field(result,6,"key_len","",MYSQL_TYPE_LONGLONG,
                       "","","",3,0);

  verify_prepare_field(result,7,"ref","",MYSQL_TYPE_VAR_STRING,
                       "","","",NAME_LEN*16,0);

  verify_prepare_field(result,8,"rows","",MYSQL_TYPE_LONGLONG,
                       "","","",10,0);

  verify_prepare_field(result,9,"Extra","",MYSQL_TYPE_VAR_STRING,
                       "","","",255,0);

  mysql_free_result(result);
  mysql_stmt_close(stmt);
}

#ifdef NOT_YET_WORKING

/*
  To test math functions
  bug #148 (reported by salle@mysql.com).
*/

#define myerrno(n) check_errcode(n)

static void check_errcode(const unsigned int err)
{  
  if (mysql->server_version)
    fprintf(stdout,"\n [MySQL-%s]",mysql->server_version);
  else
    fprintf(stdout,"\n [MySQL]");
  fprintf(stdout,"[%d] %s\n",mysql_errno(mysql),mysql_error(mysql));
  assert(mysql_errno(mysql) == err);
}

static void test_drop_temp()
{
  int rc;

  myheader("test_drop_temp");

  rc= mysql_query(mysql,"DROP DATABASE IF EXISTS test_drop_temp_db");
  myquery(rc);

  rc= mysql_query(mysql,"CREATE DATABASE test_drop_temp_db");
  myquery(rc);
  
  rc= mysql_query(mysql,"CREATE TABLE test_drop_temp_db.t1(c1 int, c2 char(1))");
  myquery(rc);

  rc = mysql_query(mysql,"delete from mysql.db where Db='test_drop_temp_db'");
  myquery(rc);

  rc = mysql_query(mysql,"delete from mysql.db where Db='test_drop_temp_db'");
  myquery(rc);

  strxmov(query,"GRANT SELECT,USAGE,DROP ON test_drop_temp_db.* TO test_temp@",
                opt_host ? opt_host : "localhost", NullS);

  if (mysql_query(mysql,query))
  {
    myerror("GRANT failed");
    
    /* 
       If server started with --skip-grant-tables, skip this test, else
       exit to indicate an error

       ER_UNKNOWN_COM_ERROR = 1047
     */ 
    if (mysql_errno(mysql) != 1047)  
      exit(0);  
  }
  else
  {
    MYSQL *org_mysql= mysql, *lmysql;
    
    fprintf(stdout, "\n Establishing a test connection ...");
    if (!(lmysql = mysql_init(NULL)))
    { 
      myerror("mysql_init() failed");
      exit(0);
    }

    rc = mysql_query(mysql,"flush privileges");
    myquery(rc);

    if (!(mysql_real_connect(lmysql,opt_host ? opt_host : "localhost","test_temp",
			     "", "test_drop_temp_db", opt_port,
			     opt_unix_socket, 0)))
    {
      mysql= lmysql;
      myerror("connection failed");   
      mysql_close(lmysql);
      exit(0);
    }   
    fprintf(stdout," OK");

    mysql= lmysql;
    rc = mysql_query(mysql,"INSERT INTO t1 VALUES(10,'C')");
    myerrno((uint)1142);

    rc = mysql_query(mysql,"DROP TABLE t1");
    myerrno((uint)1142);
  
    mysql= org_mysql;
    rc= mysql_query(mysql,"CREATE TEMPORARY TABLE test_drop_temp_db.t1(c1 int)");
    myquery(rc);
  
    rc= mysql_query(mysql,"CREATE TEMPORARY TABLE test_drop_temp_db.t2 LIKE test_drop_temp_db.t1");
    myquery(rc);

    mysql= lmysql;

    rc = mysql_query(mysql,"DROP TABLE t1,t2");
    myquery_r(rc);

    rc = mysql_query(mysql,"DROP TEMPORARY TABLE t1");
    myquery_r(rc);

    rc = mysql_query(mysql,"DROP TEMPORARY TABLE t2");
    myquery_r(rc);
    
    mysql_close(lmysql);        
    mysql= org_mysql;

    rc = mysql_query(mysql,"drop database test_drop_temp_db");
    myquery(rc);
    assert(1 == mysql_affected_rows(mysql));

    rc = mysql_query(mysql,"delete from mysql.user where User='test_temp'");
    myquery(rc);
    assert(1 == mysql_affected_rows(mysql));


    rc = mysql_query(mysql,"delete from mysql.tables_priv where User='test_temp'");
    myquery(rc);
    assert(1 == mysql_affected_rows(mysql));
  }
}
#endif

/*
  To test warnings for cuted rows
*/
static void test_cuted_rows()
{
  int        rc, count;
  MYSQL_RES  *result;

  myheader("test_cuted_rows");

  mysql_query(mysql, "DROP TABLE if exists t1");  
  mysql_query(mysql, "DROP TABLE if exists t2");

  rc = mysql_query(mysql, "CREATE TABLE t1(c1 tinyint)");
  myquery(rc);

  rc = mysql_query(mysql, "CREATE TABLE t2(c1 int not null)");
  myquery(rc);

  rc = mysql_query(mysql, "INSERT INTO t1 values(10),(NULL),(NULL)");
  myquery(rc);

  count= mysql_warning_count(mysql);
  fprintf(stdout, "\n total warnings: %d", count);
  assert(count == 0);

  rc = mysql_query(mysql, "INSERT INTO t2 SELECT * FROM t1");
  myquery(rc);

  count= mysql_warning_count(mysql);
  fprintf(stdout, "\n total warnings: %d", count);
  assert(count == 2);

  rc = mysql_query(mysql, "SHOW WARNINGS");
  myquery(rc);

  result = mysql_store_result(mysql);
  mytest(result);

  assert(2 == my_process_result_set(result));
  mysql_free_result(result);

  rc = mysql_query(mysql, "INSERT INTO t1 VALUES('junk'),(876789)");
  myquery(rc);

  count= mysql_warning_count(mysql);
  fprintf(stdout, "\n total warnings: %d", count);
  assert(count == 2);

  rc = mysql_query(mysql, "SHOW WARNINGS");
  myquery(rc);

  result = mysql_store_result(mysql);
  mytest(result);

  assert(2 == my_process_result_set(result));
  mysql_free_result(result);
}

/*
  To test update/binary logs
*/
static void test_logs()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[2];
  char       data[255];
  ulong      length;
  int        rc;
  short      id;

  myheader("test_logs");


  rc = mysql_query(mysql, "DROP TABLE IF EXISTS test_logs");
  myquery(rc);

  rc = mysql_query(mysql, "CREATE TABLE test_logs(id smallint, name varchar(20))");
  myquery(rc);

  length= (ulong)(strmov((char *)data,"INSERT INTO test_logs VALUES(?,?)") - data);
  stmt = mysql_prepare(mysql, data, length);
  check_stmt(stmt); 
  
  bind[0].buffer_type= MYSQL_TYPE_SHORT;
  bind[0].buffer= (char *)&id;
  bind[0].is_null= 0;
  bind[0].length= 0;
  
  bind[1].buffer_type= MYSQL_TYPE_STRING;
  bind[1].buffer= (char *)&data;
  bind[1].is_null= 0;
  bind[1].buffer_length= 255;
  bind[1].length= &length;

  id= 9876;
  length= (ulong)(strmov((char *)data,"MySQL - Open Source Database")- data);    

  rc = mysql_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  strmov((char *)data, "'");
  length= 1;

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);
  
  strmov((char *)data, "\"");
  length= 1;

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);
  
  length= (ulong)(strmov((char *)data, "my\'sql\'")-data);
  rc = mysql_execute(stmt);
  check_execute(stmt, rc);
  
  length= (ulong)(strmov((char *)data, "my\"sql\"")-data);
  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  mysql_stmt_close(stmt);

  length= (ulong)(strmov((char *)data,"INSERT INTO test_logs VALUES(20,'mysql')") - data);
  stmt = mysql_prepare(mysql, data, length);
  check_stmt(stmt); 

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  mysql_stmt_close(stmt);

  length= (ulong)(strmov((char *)data, "SELECT * FROM test_logs WHERE id=?") - data);
  stmt = mysql_prepare(mysql, data, length);
  check_stmt(stmt); 

  rc = mysql_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  bind[1].buffer_length= 255;
  rc = mysql_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt, rc);

  fprintf(stdout, "\n id    : %d", id);
  fprintf(stdout, "\n name  : %s(%ld)", data, length);

  assert(id == 9876);  
  assert(length == 19); /* Due to VARCHAR(20) */
  assert(strcmp(data,"MySQL - Open Source")==0); 

  rc = mysql_fetch(stmt);
  check_execute(stmt, rc);
  
  fprintf(stdout, "\n name  : %s(%ld)", data, length);

  assert(length == 1);
  assert(strcmp(data,"'")==0); 

  rc = mysql_fetch(stmt);
  check_execute(stmt, rc);
  
  fprintf(stdout, "\n name  : %s(%ld)", data, length);

  assert(length == 1);
  assert(strcmp(data,"\"")==0); 

  rc = mysql_fetch(stmt);
  check_execute(stmt, rc);
  
  fprintf(stdout, "\n name  : %s(%ld)", data, length);

  assert(length == 7);
  assert(strcmp(data,"my\'sql\'")==0); 

  rc = mysql_fetch(stmt);
  check_execute(stmt, rc);
  
  fprintf(stdout, "\n name  : %s(%ld)", data, length);

  assert(length == 7);
  /*assert(strcmp(data,"my\"sql\"")==0); */

  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);

  rc = mysql_query(mysql,"DROP TABLE test_logs");
  myquery(rc);
}

/*
  To test 'n' statements create and close  
*/

static void test_nstmts()
{
  MYSQL_STMT  *stmt;
  char        query[255];
  int         rc;
  static uint i, total_stmts= 2000;
  long        length;
  MYSQL_BIND  bind[1];

  myheader("test_nstmts");

  mysql_autocommit(mysql,TRUE);

  rc = mysql_query(mysql, "DROP TABLE IF EXISTS test_nstmts");
  myquery(rc);
  
  rc = mysql_query(mysql, "CREATE TABLE test_nstmts(id int)");
  myquery(rc);

  bind[0].buffer= (char *)&i;
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].length= 0;
  bind[0].is_null= 0;
  bind[0].buffer_length= 0;
  
  for (i=0; i < total_stmts; i++)
  {
    fprintf(stdout, "\r stmt: %d", i);
    
    length = (long)(strmov(query, "insert into test_nstmts values(?)")-query);
    stmt = mysql_prepare(mysql, query, length);
    check_stmt(stmt);

    rc = mysql_bind_param(stmt, bind);
    check_execute(stmt, rc);

    rc = mysql_execute(stmt);
    check_execute(stmt, rc);

    mysql_stmt_close(stmt);
  }

  stmt = mysql_simple_prepare(mysql," select count(*) from test_nstmts");
  check_stmt(stmt);

  rc = mysql_execute(stmt);
  check_execute(stmt, rc);

  i = 0;
  rc = mysql_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt, rc);
  fprintf(stdout, "\n total rows: %d", i);
  assert( i == total_stmts);

  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
  
  rc = mysql_query(mysql,"DROP TABLE test_nstmts");
  myquery(rc);
}

/*
  To test stmt seek() functions
*/
static void test_fetch_seek()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[3];
  MYSQL_ROW_OFFSET row;
  int        rc;
  long       c1;
  char       c2[11], c3[20];

  myheader("test_fetch_seek");

  rc= mysql_query(mysql,"drop table if exists test_seek");
  myquery(rc);
  
  rc = mysql_query(mysql, "create table test_seek(c1 int primary key auto_increment, c2 char(10), c3 timestamp(14))");
  myquery(rc);
  
  rc = mysql_query(mysql, "insert into test_seek(c2) values('venu'),('mysql'),('open'),('source')");
  myquery(rc);

  stmt = mysql_simple_prepare(mysql,"select * from test_seek");
  check_stmt(stmt);

  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (char *)&c1;
  bind[0].buffer_length= 0;
  bind[0].is_null= 0;
  bind[0].length= 0;

  bind[1].buffer_type= MYSQL_TYPE_STRING;
  bind[1].buffer= (char *)c2;
  bind[1].buffer_length= sizeof(c2);
  bind[1].is_null= 0;
  bind[1].length= 0;

  bind[2]= bind[1];
  bind[2].buffer= (char *)c3;
  bind[2].buffer_length= sizeof(c3);

  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  rc = mysql_bind_result(stmt, bind);
  check_execute(stmt,rc);

  rc = mysql_stmt_store_result(stmt);
  check_execute(stmt,rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  fprintf(stdout, "\n row 0: %ld,%s,%s", c1,c2,c3);

  row = mysql_stmt_row_tell(stmt);

  row = mysql_stmt_row_seek(stmt, row);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  fprintf(stdout, "\n row 2: %ld,%s,%s", c1,c2,c3);

  row = mysql_stmt_row_seek(stmt, row);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  fprintf(stdout, "\n row 2: %ld,%s,%s", c1,c2,c3);

  mysql_stmt_data_seek(stmt, 0);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  fprintf(stdout, "\n row 0: %ld,%s,%s", c1,c2,c3);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}

/*
  To test mysql_fetch_column() with offset 
*/
static void test_fetch_offset()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[1];
  char       data[11];
  ulong      length;
  int        rc;
  my_bool    is_null;


  myheader("test_fetch_offset");

  rc= mysql_query(mysql,"drop table if exists test_column");
  myquery(rc);
  
  rc = mysql_query(mysql, "create table test_column(a char(10))");
  myquery(rc);
  
  rc = mysql_query(mysql, "insert into test_column values('abcdefghij'),(null)");
  myquery(rc);

  stmt = mysql_simple_prepare(mysql,"select * from test_column");
  check_stmt(stmt);

  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (char *)data;
  bind[0].buffer_length= 11;
  bind[0].is_null= &is_null;
  bind[0].length= &length;

  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  rc = mysql_fetch_column(stmt,bind,0,0);
  check_execute_r(stmt,rc);

  rc = mysql_bind_result(stmt, bind);
  check_execute(stmt,rc);

  rc = mysql_stmt_store_result(stmt);
  check_execute(stmt,rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);
  
  data[0]= '\0';
  rc = mysql_fetch_column(stmt,bind,0,0);
  check_execute(stmt,rc);
  fprintf(stdout, "\n col 1: %s (%ld)", data, length);
  assert(strncmp(data,"abcd",4) == 0 && length == 10);
  
  rc = mysql_fetch_column(stmt,bind,0,5);
  check_execute(stmt,rc);
  fprintf(stdout, "\n col 1: %s (%ld)", data, length);
  assert(strncmp(data,"fg",2) == 0 && length == 10);  

  rc = mysql_fetch_column(stmt,bind,0,9);
  check_execute(stmt,rc);
  fprintf(stdout, "\n col 0: %s (%ld)", data, length);
  assert(strncmp(data,"j",1) == 0 && length == 10);  

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  is_null= 0;

  rc = mysql_fetch_column(stmt,bind,0,0);
  check_execute(stmt,rc);

  assert(is_null == 1);

  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);

  rc = mysql_fetch_column(stmt,bind,1,0);
  check_execute_r(stmt,rc);

  mysql_stmt_close(stmt);
}
/*
  To test mysql_fetch_column()
*/
static void test_fetch_column()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[2];
  char       c2[20], bc2[20];
  ulong      l1, l2, bl1, bl2;
  int        rc, c1, bc1;

  myheader("test_fetch_column");

  rc= mysql_query(mysql,"drop table if exists test_column");
  myquery(rc);
  
  rc = mysql_query(mysql, "create table test_column(c1 int primary key auto_increment, c2 char(10))");
  myquery(rc);
  
  rc = mysql_query(mysql, "insert into test_column(c2) values('venu'),('mysql')");
  myquery(rc);

  stmt = mysql_simple_prepare(mysql,"select * from test_column order by c2 desc");
  check_stmt(stmt);

  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (char *)&bc1;
  bind[0].buffer_length= 0;
  bind[0].is_null= 0;
  bind[0].length= &bl1;
  bind[1].buffer_type= MYSQL_TYPE_STRING;
  bind[1].buffer= (char *)bc2;
  bind[1].buffer_length= 7;
  bind[1].is_null= 0;
  bind[1].length= &bl2;

  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  rc = mysql_bind_result(stmt, bind);
  check_execute(stmt,rc);

  rc = mysql_stmt_store_result(stmt);
  check_execute(stmt,rc);

  rc = mysql_fetch_column(stmt,bind,1,0); /* No-op at this point */
  check_execute_r(stmt,rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  fprintf(stdout, "\n row 0: %d,%s", bc1,bc2);

  c2[0]= '\0'; l2= 0;
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (char *)c2;
  bind[0].buffer_length= 7;
  bind[0].is_null= 0;
  bind[0].length= &l2;

  rc = mysql_fetch_column(stmt,bind,1,0);
  check_execute(stmt,rc);
  fprintf(stdout, "\n col 1: %s(%ld)", c2, l2);
  assert(strncmp(c2,"venu",4)==0 && l2 == 4);
  
  c2[0]= '\0'; l2= 0;
  rc = mysql_fetch_column(stmt,bind,1,0);
  check_execute(stmt,rc);
  fprintf(stdout, "\n col 1: %s(%ld)", c2, l2);
  assert(strcmp(c2,"venu")==0 && l2 == 4);  

  c1= 0;     
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (char *)&c1;
  bind[0].buffer_length= 0;
  bind[0].is_null= 0;
  bind[0].length= &l1;

  rc = mysql_fetch_column(stmt,bind,0,0);
  check_execute(stmt,rc);
  fprintf(stdout, "\n col 0: %d(%ld)", c1, l1);
  assert(c1 == 1 && l1 == 4);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);  

  fprintf(stdout, "\n row 1: %d,%s", bc1,bc2);

  c2[0]= '\0'; l2= 0;
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (char *)c2;
  bind[0].buffer_length= 7;
  bind[0].is_null= 0;
  bind[0].length= &l2;

  rc = mysql_fetch_column(stmt,bind,1,0);
  check_execute(stmt,rc);
  fprintf(stdout, "\n col 1: %s(%ld)", c2, l2);
  assert(strncmp(c2,"mysq",4)==0 && l2 == 5);
  
  c2[0]= '\0'; l2= 0;
  rc = mysql_fetch_column(stmt,bind,1,0);
  check_execute(stmt,rc);
  fprintf(stdout, "\n col 1: %si(%ld)", c2, l2);
  assert(strcmp(c2,"mysql")==0 && l2 == 5);  

  c1= 0;     
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (char *)&c1;
  bind[0].buffer_length= 0;
  bind[0].is_null= 0;
  bind[0].length= &l1;

  rc = mysql_fetch_column(stmt,bind,0,0);
  check_execute(stmt,rc);
  fprintf(stdout, "\n col 0: %d(%ld)", c1, l1);
  assert(c1 == 2 && l1 == 4);

  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);

  rc = mysql_fetch_column(stmt,bind,1,0);
  check_execute_r(stmt,rc);

  mysql_stmt_close(stmt);
}

/*
  To test mysql_list_fields()
*/
static void test_list_fields()
{
  MYSQL_RES *result;
  int rc;
  myheader("test_list_fields");

  rc= mysql_query(mysql,"drop table if exists test_list_fields");
  myquery(rc);
  
  rc = mysql_query(mysql, "create table test_list_fields(c1 int primary key auto_increment, c2 char(10) default 'mysql')");
  myquery(rc);

  result = mysql_list_fields(mysql, "test_list_fields",NULL);
  mytest(result);

  assert( 0 == my_process_result_set(result));
  
  verify_prepare_field(result,0,"c1","c1",MYSQL_TYPE_LONG,
                       "test_list_fields","test_list_fields",current_db,11,"0");
  
  verify_prepare_field(result,1,"c2","c2",MYSQL_TYPE_STRING,
                       "test_list_fields","test_list_fields",current_db,10,"mysql");

  mysql_free_result(result);
}

/*
  To test a memory ovverun bug
*/
static void test_mem_overun()
{
  char       buffer[10000], field[10];
  MYSQL_STMT *stmt;
  MYSQL_RES  *field_res;
  int        rc,i, length;


  myheader("test_mem_overun");

  /*
    Test a memory ovverun bug when a table had 1000 fields with 
    a row of data
  */
  rc= mysql_query(mysql,"drop table if exists t_mem_overun");
  myquery(rc);

  strxmov(buffer,"create table t_mem_overun(",NullS);
  for (i=0; i < 1000; i++)
  {
    sprintf(field,"c%d int", i);
    strxmov(buffer,buffer,field,",",NullS);
  }
  length= (int)(strmov(buffer,buffer) - buffer);
  buffer[length-1]='\0';
  strxmov(buffer,buffer,")",NullS);
  
  rc = mysql_real_query(mysql, buffer, length);
  myquery(rc);

  strxmov(buffer,"insert into t_mem_overun values(",NullS);
  for (i=0; i < 1000; i++)
  {
    strxmov(buffer,buffer,"1,",NullS);
  }
  length= (int)(strmov(buffer,buffer) - buffer);
  buffer[length-1]='\0';
  strxmov(buffer,buffer,")",NullS);
  
  rc = mysql_real_query(mysql, buffer, length);
  myquery(rc);

  rc = mysql_query(mysql,"select * from t_mem_overun");
  myquery(rc);

  assert(1 == my_process_result(mysql));
  
  stmt = mysql_simple_prepare(mysql, "select * from t_mem_overun");
  check_stmt(stmt);

  rc = mysql_execute(stmt);
  check_execute(stmt,rc); 
  
  field_res = mysql_get_metadata(stmt);
  mytest(field_res);

  fprintf(stdout,"\n total fields : %d", mysql_num_fields(field_res));
  assert( 1000 == mysql_num_fields(field_res));

  rc = mysql_stmt_store_result(stmt);
  check_execute(stmt,rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);

  mysql_free_result(field_res);

  mysql_stmt_close(stmt);
}

/*
  To test mysql_stmt_free_result()
*/
static void test_free_result()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[1];
  char       c2[5];
  ulong      bl1, l2;
  int        rc, c1, bc1;

  myheader("test_free_result");

  rc= mysql_query(mysql,"drop table if exists test_free_result");
  myquery(rc);
  
  rc = mysql_query(mysql, "create table test_free_result(c1 int primary key auto_increment)");
  myquery(rc);
  
  rc = mysql_query(mysql, "insert into test_free_result values(),(),()");
  myquery(rc);

  stmt = mysql_simple_prepare(mysql,"select * from test_free_result");
  check_stmt(stmt);

  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (char *)&bc1;
  bind[0].buffer_length= 0;
  bind[0].is_null= 0;
  bind[0].length= &bl1;

  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  rc = mysql_bind_result(stmt, bind);
  check_execute(stmt,rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  c2[0]= '\0'; l2= 0;
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (char *)c2;
  bind[0].buffer_length= 7;
  bind[0].is_null= 0;
  bind[0].length= &l2;

  rc = mysql_fetch_column(stmt,bind,0,0);
  check_execute(stmt,rc);
  fprintf(stdout, "\n col 0: %s(%ld)", c2, l2);
  assert(strncmp(c2,"1",1)==0 && l2 == 1);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);  

  c1= 0, l2= 0;   
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (char *)&c1;
  bind[0].buffer_length= 0;
  bind[0].is_null= 0;
  bind[0].length= &l2;

  rc = mysql_fetch_column(stmt,bind,0,0);
  check_execute(stmt,rc);
  fprintf(stdout, "\n col 0: %d(%ld)", c1, l2);
  assert(c1 == 2 && l2 == 4);  

  rc = mysql_query(mysql,"drop table test_free_result");
  myquery_r(rc); /* error should be, COMMANDS OUT OF SYNC */

  rc = mysql_stmt_free_result(stmt);
  check_execute(stmt,rc);

  rc = mysql_query(mysql,"drop table test_free_result");
  myquery(rc);  /* should be successful */

  mysql_stmt_close(stmt);
}

/*
  To test mysql_stmt_free_result()
*/
static void test_free_store_result()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[1];
  char       c2[5];
  ulong      bl1, l2;
  int        rc, c1, bc1;

  myheader("test_free_store_result");

  rc= mysql_query(mysql,"drop table if exists test_free_result");
  myquery(rc);
  
  rc = mysql_query(mysql, "create table test_free_result(c1 int primary key auto_increment)");
  myquery(rc);
  
  rc = mysql_query(mysql, "insert into test_free_result values(),(),()");
  myquery(rc);

  stmt = mysql_simple_prepare(mysql,"select * from test_free_result");
  check_stmt(stmt);

  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (char *)&bc1;
  bind[0].buffer_length= 0;
  bind[0].is_null= 0;
  bind[0].length= &bl1;

  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  rc = mysql_bind_result(stmt, bind);
  check_execute(stmt,rc);

  rc = mysql_stmt_store_result(stmt);
  check_execute(stmt,rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  c2[0]= '\0'; l2= 0;
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (char *)c2;
  bind[0].buffer_length= 7;
  bind[0].is_null= 0;
  bind[0].length= &l2;

  rc = mysql_fetch_column(stmt,bind,0,0);
  check_execute(stmt,rc);
  fprintf(stdout, "\n col 1: %s(%ld)", c2, l2);
  assert(strncmp(c2,"1",1)==0 && l2 == 1);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);  

  c1= 0, l2= 0;   
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (char *)&c1;
  bind[0].buffer_length= 0;
  bind[0].is_null= 0;
  bind[0].length= &l2;

  rc = mysql_fetch_column(stmt,bind,0,0);
  check_execute(stmt,rc);
  fprintf(stdout, "\n col 0: %d(%ld)", c1, l2);
  assert(c1 == 2 && l2 == 4);

  rc = mysql_stmt_free_result(stmt);
  check_execute(stmt,rc);

  rc = mysql_query(mysql,"drop table test_free_result");
  myquery(rc); 

  mysql_stmt_close(stmt);
}

/********************************************************
 To test SQLmode
*********************************************************/

static void test_sqlmode()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[2];
  char       c1[5], c2[5];
  int        rc;

  myheader("test_sqlmode");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_piping");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_piping(name varchar(10))");
  myquery(rc);
  
  /* PIPES_AS_CONCAT */
  strcpy(query,"SET SQL_MODE=\"PIPES_AS_CONCAT\"");
  fprintf(stdout,"\n With %s", query);
  rc = mysql_query(mysql,query);
  myquery(rc);

  strcpy(query, "INSERT INTO test_piping VALUES(?||?)");
  fprintf(stdout,"\n  query: %s", query);
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  fprintf(stdout,"\n  total parameters: %ld", mysql_param_count(stmt));      

  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (char *)c1;
  bind[0].buffer_length= 2;
  bind[0].is_null= 0;
  bind[0].length= 0;

  bind[1].buffer_type= MYSQL_TYPE_STRING;
  bind[1].buffer= (char *)c2;
  bind[1].buffer_length= 3;
  bind[1].is_null= 0;
  bind[1].length= 0;

  rc = mysql_bind_param(stmt, bind);
  check_execute(stmt,rc);

  strcpy(c1,"My"); strcpy(c2, "SQL");
  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  mysql_stmt_close(stmt);
  verify_col_data("test_piping","name","MySQL");  

  rc = mysql_query(mysql,"DELETE FROM test_piping");
  myquery(rc);
  
  strcpy(query, "SELECT connection_id    ()");
  fprintf(stdout,"\n  query: %s", query);
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt_r(stmt);

  /* ANSI */
  strcpy(query,"SET SQL_MODE=\"ANSI\"");
  fprintf(stdout,"\n With %s", query);
  rc = mysql_query(mysql,query);
  myquery(rc);

  strcpy(query, "INSERT INTO test_piping VALUES(?||?)");
  fprintf(stdout,"\n  query: %s", query);
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);
  fprintf(stdout,"\n  total parameters: %ld", mysql_param_count(stmt));      

  rc = mysql_bind_param(stmt, bind);
  check_execute(stmt,rc);

  strcpy(c1,"My"); strcpy(c2, "SQL");
  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  mysql_stmt_close(stmt);
  verify_col_data("test_piping","name","MySQL");  

  /* ANSI mode spaces ... */
  strcpy(query, "SELECT connection_id    ()");
  fprintf(stdout,"\n  query: %s", query);
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);
  fprintf(stdout,"\n  returned 1 row\n");

  mysql_stmt_close(stmt);
  
  /* IGNORE SPACE MODE */
  strcpy(query,"SET SQL_MODE=\"IGNORE_SPACE\"");
  fprintf(stdout,"\n With %s", query);
  rc = mysql_query(mysql,query);
  myquery(rc);

  strcpy(query, "SELECT connection_id    ()");
  fprintf(stdout,"\n  query: %s", query);
  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt,rc);

  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);
  fprintf(stdout,"\n  returned 1 row");

  mysql_stmt_close(stmt);
}

/*
  test for timestamp handling
*/
static void test_ts()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[6];
  MYSQL_TIME ts;
  MYSQL_RES  *prep_res;
  char       strts[30];
  long       length;
  int        rc, field_count;
  char       name;

  myheader("test_ts");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_ts");
  myquery(rc);
  
  rc= mysql_query(mysql,"CREATE TABLE test_ts(a DATE, b TIME, c TIMESTAMP)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  stmt = mysql_simple_prepare(mysql,"INSERT INTO test_ts VALUES(?,?,?),(?,?,?)");
  check_stmt(stmt);

  ts.year= 2003;
  ts.month= 07;
  ts.day= 12;
  ts.hour= 21;
  ts.minute= 07;
  ts.second= 46;
  ts.second_part= 0;
  length= (long)(strmov(strts,"2003-07-12 21:07:46") - strts);

  bind[0].buffer_type= MYSQL_TYPE_TIMESTAMP;
  bind[0].buffer= (char *)&ts;
  bind[0].buffer_length= sizeof(ts);
  bind[0].is_null= 0;
  bind[0].length= 0;

  bind[2]= bind[1]= bind[0];

  bind[3].buffer_type= MYSQL_TYPE_STRING;
  bind[3].buffer= (char *)strts;
  bind[3].buffer_length= sizeof(strts);
  bind[3].is_null= 0;
  bind[3].length= &length;

  bind[5]= bind[4]= bind[3];

  rc = mysql_bind_param(stmt, bind);
  check_execute(stmt,rc);

  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  mysql_stmt_close(stmt);

  verify_col_data("test_ts","a","2003-07-12");
  verify_col_data("test_ts","b","21:07:46");
  verify_col_data("test_ts","c","2003-07-12 21:07:46");

  stmt = mysql_simple_prepare(mysql,"SELECT * FROM test_ts");
  check_stmt(stmt);

  prep_res = mysql_get_metadata(stmt);
  mytest(prep_res);

  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  assert( 2== my_process_stmt_result(stmt));
  field_count= mysql_num_fields(prep_res);

  mysql_free_result(prep_res);
  mysql_stmt_close(stmt);

  for (name= 'a'; field_count--; name++)
  {
    int row_count= 0;

    sprintf(query,"SELECT a,b,c FROM test_ts WHERE %c=?",name);
    length= (long)(strmov(query,query)- query);

    fprintf(stdout,"\n  %s", query);
    stmt = mysql_prepare(mysql, query, length);
    check_stmt(stmt);

    rc = mysql_bind_param(stmt, bind);
    check_execute(stmt,rc);

    rc = mysql_execute(stmt);
    check_execute(stmt,rc);

    while (mysql_fetch(stmt) == 0)
      row_count++;

    fprintf(stdout, "\n   returned '%d' rows", row_count);
    assert(row_count == 2);
    mysql_stmt_close(stmt);
  }
}

/*
  Test for bug #1500.
  XXX: despite that this bug is fixed, it spots mysqld code which is not
  working correctly yet: to fix all things  properly we need to implement
  Item::cleanup() method for all items (as described in bugs #1663 and
  #1749). So don't be surprised in case valgrind barks on it.
*/

static void test_bug1500()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[3];
  int        rc;
  long       int_data[3]= {2,3,4}; 
  const char *data;

  myheader("test_bug1500");

  rc= mysql_query(mysql,"DROP TABLE IF EXISTS test_bg1500");
  myquery(rc);
  
  rc= mysql_query(mysql,"CREATE TABLE test_bg1500 (i INT)");
  myquery(rc);
  
  rc= mysql_query(mysql,"INSERT INTO test_bg1500 VALUES (1),(2)");
  myquery(rc);

  rc= mysql_commit(mysql);
  myquery(rc);

  stmt= mysql_simple_prepare(mysql,"SELECT i FROM test_bg1500 WHERE i IN (?,?,?)");
  check_stmt(stmt);
  verify_param_count(stmt,3);

  bind[0].buffer= (char *)int_data;
  bind[0].buffer_type= FIELD_TYPE_LONG;
  bind[0].is_null= 0;
  bind[0].length= NULL;
  bind[0].buffer_length= 0;
  bind[2]= bind[1]= bind[0];
  bind[1].buffer= (char *)(int_data + 1);
  bind[2].buffer= (char *)(int_data + 2);
  
  rc= mysql_bind_param(stmt, bind);
  check_execute(stmt,rc);
  
  rc= mysql_execute(stmt);
  check_execute(stmt,rc);
  
  assert(1 == my_process_stmt_result(stmt));

  /* FIXME If we comment out next string server will crash :( */
  mysql_stmt_close(stmt);

  rc= mysql_query(mysql,"DROP TABLE test_bg1500");
  myquery(rc);
  
  rc= mysql_query(mysql,"CREATE TABLE test_bg1500 (s VARCHAR(25), FULLTEXT(s))");
  myquery(rc);
  
  rc= mysql_query(mysql,
        "INSERT INTO test_bg1500 VALUES ('Gravedigger'), ('Greed'),('Hollow Dogs')");
  myquery(rc);

  rc= mysql_commit(mysql);
  myquery(rc);

  stmt= mysql_simple_prepare(mysql,
          "SELECT s FROM test_bg1500 WHERE MATCH (s) AGAINST (?)");
  check_stmt(stmt);
  
  verify_param_count(stmt,1);
  
  data= "Dogs";
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (char *) data;
  bind[0].buffer_length= strlen(data);
  bind[0].is_null= 0;
  bind[0].length= 0;
  
  rc= mysql_bind_param(stmt, bind);
  check_execute(stmt,rc);
  
  rc= mysql_execute(stmt);
  check_execute(stmt,rc);
  
  assert(1 == my_process_stmt_result(stmt));

  /* 
    FIXME If we comment out next string server will crash too :( 
    This is another manifestation of bug #1663
  */
  mysql_stmt_close(stmt);
  
  /* This should work too */
  stmt= mysql_simple_prepare(mysql,
          "SELECT s FROM test_bg1500 WHERE MATCH (s) AGAINST (CONCAT(?,'digger'))");
  check_stmt(stmt);
  
  verify_param_count(stmt,1);
  
  data= "Grave";
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (char *) data;
  bind[0].buffer_length= strlen(data);
  bind[0].is_null= 0;
  bind[0].length= 0;
  
  rc= mysql_bind_param(stmt, bind);
  check_execute(stmt,rc);
  
  rc= mysql_execute(stmt);
  check_execute(stmt,rc);
  
  assert(1 == my_process_stmt_result(stmt));

  mysql_stmt_close(stmt);
}

static void test_bug1946()
{
  MYSQL_STMT *stmt;
  int rc;
  const char *query= "INSERT INTO prepare_command VALUES (?)";

  myheader("test_bug1946");
  
  rc = mysql_query(mysql, "DROP TABLE IF EXISTS prepare_command");
  myquery(rc);
  
  rc= mysql_query(mysql,"CREATE TABLE prepare_command(ID INT)");
  myquery(rc);

  stmt = mysql_simple_prepare(mysql, query);
  check_stmt(stmt);
  rc= mysql_real_query(mysql, query, strlen(query));
  assert(rc != 0);
  fprintf(stdout, "Got error (as expected):\n");
  myerror(NULL);

  mysql_stmt_close(stmt);
  rc= mysql_query(mysql,"DROP TABLE prepare_command");
}

static void test_parse_error_and_bad_length()
{
  MYSQL_STMT *stmt;
  int rc;

  /* check that we get 4 syntax errors over the 4 calls */
  myheader("test_parse_error_and_bad_length");

  rc= mysql_query(mysql,"SHOW DATABAAAA");
  assert(rc);
  fprintf(stdout, "Got error (as expected): '%s'\n", mysql_error(mysql));
  rc= mysql_real_query(mysql,"SHOW DATABASES",100);
  assert(rc);
  fprintf(stdout, "Got error (as expected): '%s'\n", mysql_error(mysql));

  stmt= mysql_simple_prepare(mysql,"SHOW DATABAAAA");
  assert(!stmt);
  fprintf(stdout, "Got error (as expected): '%s'\n", mysql_error(mysql));
  stmt= mysql_prepare(mysql,"SHOW DATABASES",100);
  assert(!stmt);
  fprintf(stdout, "Got error (as expected): '%s'\n", mysql_error(mysql));
}


static void test_bug2247()
{
  MYSQL_STMT *stmt;
  MYSQL_RES *res;
  int rc;
  int i;
  const char *create= "CREATE TABLE bug2247(id INT UNIQUE AUTO_INCREMENT)";
  const char *insert= "INSERT INTO bug2247 VALUES (NULL)";
  const char *select= "SELECT id FROM bug2247";
  const char *update= "UPDATE bug2247 SET id=id+10";
  const char *drop= "DROP TABLE IF EXISTS bug2247";
  ulonglong exp_count;
  enum { NUM_ROWS= 5 };

  myheader("test_bug2247");
  
  fprintf(stdout, "\nChecking if stmt_affected_rows is not affected by\n"
                  "mysql_query ... ");
  /* create table and insert few rows */
  rc = mysql_query(mysql, drop);
  myquery(rc);
  
  rc= mysql_query(mysql, create);
  myquery(rc);

  stmt= mysql_prepare(mysql, insert, strlen(insert));
  check_stmt(stmt);
  for (i= 0; i < NUM_ROWS; ++i)
  {
    rc= mysql_execute(stmt);
    check_execute(stmt, rc);
  }
  exp_count= mysql_stmt_affected_rows(stmt);
  assert(exp_count == 1);

  rc= mysql_query(mysql, select);
  myquery(rc);
  /* 
    mysql_store_result overwrites mysql->affected_rows. Check that
    mysql_stmt_affected_rows() returns the same value, whereas
    mysql_affected_rows() value is correct.
  */
  res= mysql_store_result(mysql);
  mytest(res);

  assert(mysql_affected_rows(mysql) == NUM_ROWS);
  assert(exp_count == mysql_stmt_affected_rows(stmt));
  
  rc= mysql_query(mysql, update);
  myquery(rc);
  assert(mysql_affected_rows(mysql) == NUM_ROWS);
  assert(exp_count == mysql_stmt_affected_rows(stmt));

  mysql_free_result(res);
  mysql_stmt_close(stmt);

  /* check that mysql_stmt_store_result modifies mysql_stmt_affected_rows */
  stmt= mysql_prepare(mysql, select, strlen(select));
  check_stmt(stmt);

  rc= mysql_execute(stmt);
  check_execute(stmt, rc);
  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);
  exp_count= mysql_stmt_affected_rows(stmt);
  assert(exp_count == NUM_ROWS);

  rc= mysql_query(mysql, insert);
  myquery(rc);
  assert(mysql_affected_rows(mysql) == 1);
  assert(mysql_stmt_affected_rows(stmt) == exp_count);
  
  mysql_stmt_close(stmt);
  fprintf(stdout, "OK");
}


static void test_subqueries()
{
  MYSQL_STMT *stmt;
  int rc, i;
  const char *query= "SELECT (SELECT SUM(a+b) FROM t2 where t1.b=t2.b GROUP BY t1.a LIMIT 1) as scalar_s, exists (select 1 from t2 where t2.a/2=t1.a) as exists_s, a in (select a+3 from t2) as in_s, (a-1,b-1) in (select a,b from t2) as in_row_s FROM t1, (select a x, b y from t2) tt WHERE x=a";

  myheader("test_subquery");
  
  rc = mysql_query(mysql, "DROP TABLE IF EXISTS t1,t2");
  myquery(rc);
  
  rc= mysql_query(mysql,"CREATE TABLE t1 (a int , b int);");
  myquery(rc);

  rc= mysql_query(mysql,
		  "insert into t1 values (1,1), (2, 2), (3,3), (4,4), (5,5);");
  myquery(rc);

  rc= mysql_query(mysql,"create table t2 select * from t1;");
  myquery(rc);

  stmt= mysql_prepare(mysql, query, strlen(query));
  check_stmt(stmt);
  for (i= 0; i < 3; i++)
  {
    rc= mysql_execute(stmt);
    check_execute(stmt, rc);
    assert(5 == my_process_stmt_result(stmt));
  }
  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "DROP TABLE t1,t2");
  myquery(rc);
}


static void test_bad_union()
{
  MYSQL_STMT *stmt;
  const char *query= "SELECT 1, 2 union SELECT 1";

  myheader("test_bad_union");
  
  stmt= mysql_prepare(mysql, query, strlen(query));
  assert(stmt == 0);
  myerror(NULL); 
}

static void test_distinct()
{
  MYSQL_STMT *stmt;
  int rc, i;
  const char *query= 
    "SELECT 2+count(distinct b), group_concat(a) FROM t1 group by a";

  myheader("test_subquery");
  
  rc = mysql_query(mysql, "DROP TABLE IF EXISTS t1");
  myquery(rc);
  
  rc= mysql_query(mysql,"CREATE TABLE t1 (a int , b int);");
  myquery(rc);

  rc= mysql_query(mysql,
		  "insert into t1 values (1,1), (2, 2), (3,3), (4,4), (5,5),\
(1,10), (2, 20), (3,30), (4,40), (5,50);");
  myquery(rc);

  for (i= 0; i < 3; i++)
  {
    stmt= mysql_prepare(mysql, query, strlen(query));
    check_stmt(stmt);
    rc= mysql_execute(stmt);
    check_execute(stmt, rc);
    assert(5 == my_process_stmt_result(stmt));
    mysql_stmt_close(stmt);
  }

  rc= mysql_query(mysql, "DROP TABLE t1");
  myquery(rc);
}

/*
  Test for bug#2248 "mysql_fetch without prior mysql_execute hangs"
*/

static void test_bug2248()
{
  MYSQL_STMT *stmt;
  int rc;
  const char *query1= "SELECT DATABASE()";
  const char *query2= "INSERT INTO test_bug2248 VALUES (10)";
  
  myheader("test_bug2248");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_bug2248");
  myquery(rc);
  
  rc= mysql_query(mysql, "CREATE TABLE test_bug2248 (id int)");
  myquery(rc);
  
  stmt= mysql_prepare(mysql, query1, strlen(query1));
  check_stmt(stmt);

  /* This should not hang */
  rc= mysql_fetch(stmt);
  check_execute_r(stmt,rc);
  
  /* And this too */
  rc= mysql_stmt_store_result(stmt);
  check_execute_r(stmt,rc);
  
  mysql_stmt_close(stmt);
  
  stmt= mysql_prepare(mysql, query2, strlen(query2));
  check_stmt(stmt);
  
  rc= mysql_execute(stmt);
  check_execute(stmt,rc);

  /* This too should not hang but should return proper error */
  rc= mysql_fetch(stmt);
  assert(rc==MYSQL_NO_DATA);
  
  /* This too should not hang but should not bark */
  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt,rc);
 
  /* This should return proper error */
  rc= mysql_fetch(stmt);
  check_execute_r(stmt,rc);
  assert(rc==MYSQL_NO_DATA);
  
  mysql_stmt_close(stmt);
  
  rc= mysql_query(mysql,"DROP TABLE test_bug2248");
  myquery(rc);
}

static void test_subqueries_ref()
{
  MYSQL_STMT *stmt;
  int rc, i;
  const char *query= "SELECT a as ccc from t1 where a+1=(SELECT 1+ccc from t1 where ccc+1=a+1 and a=1)";

  myheader("test_subquery_ref");
  
  rc = mysql_query(mysql, "DROP TABLE IF EXISTS t1");
  myquery(rc);
  
  rc= mysql_query(mysql,"CREATE TABLE t1 (a int);");
  myquery(rc);

  rc= mysql_query(mysql,
		  "insert into t1 values (1), (2), (3), (4), (5);");
  myquery(rc);

  stmt= mysql_prepare(mysql, query, strlen(query));
  check_stmt(stmt);
  for (i= 0; i < 3; i++)
  {
    rc= mysql_execute(stmt);
    check_execute(stmt, rc);
    assert(1 == my_process_stmt_result(stmt));
  }
  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "DROP TABLE t1");
  myquery(rc);
}


static void test_union()
{
  MYSQL_STMT *stmt;
  int rc;

  myheader("test_union");
 
  rc= mysql_query(mysql, "DROP TABLE IF EXISTS t1, t2");
  myquery(rc);

  rc= mysql_query(mysql,
                  "CREATE TABLE t1 "
                  "(id INTEGER NOT NULL PRIMARY KEY, "
                  " name VARCHAR(20) NOT NULL)");
  myquery(rc);
  rc= mysql_query(mysql,
                  "INSERT INTO t1 (id, name) VALUES "
                  "(2, 'Ja'), (3, 'Ede'), "
                  "(4, 'Haag'), (5, 'Kabul'), "
                  "(6, 'Almere'), (7, 'Utrecht'), "
                  "(8, 'Qandahar'), (9, 'Amsterdam'), "
                  "(10, 'Amersfoort'), (11, 'Constantine')");
  myquery(rc);
  rc= mysql_query(mysql,
                  "CREATE TABLE t2 "
                  "(id INTEGER NOT NULL PRIMARY KEY, "
                  " name VARCHAR(20) NOT NULL)");
  myquery(rc);
  rc= mysql_query(mysql,
                  "INSERT INTO t2 (id, name) VALUES "
                  "(4, 'Guam'), (5, 'Aruba'), "
                  "(6, 'Angola'), (7, 'Albania'), "
                  "(8, 'Anguilla'), (9, 'Argentina'), "
                  "(10, 'Azerbaijan'), (11, 'Afghanistan'), "
                  "(12, 'Burkina Faso'), (13, 'Faroe Islands')");
  myquery(rc);
 
  stmt= mysql_simple_prepare(mysql,
                             "SELECT t1.name FROM t1 UNION "
                             "SELECT t2.name FROM t2");
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt,rc);
  assert(20 == my_process_stmt_result(stmt));
  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "DROP TABLE t1, t2");
  myquery(rc);
}

static void test_bug3117()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND buffer;
  longlong lii;
  ulong length;
  my_bool is_null;
  int rc;

  myheader("test_bug3117");
  
  rc = mysql_query(mysql, "DROP TABLE IF EXISTS t1");
  myquery(rc);
  
  rc= mysql_query(mysql,"CREATE TABLE t1 (id int auto_increment primary key)");
  myquery(rc);

  stmt = mysql_simple_prepare(mysql, "SELECT LAST_INSERT_ID()");
  check_stmt(stmt);

  rc= mysql_query(mysql, "INSERT INTO t1 VALUES (NULL)");
  myquery(rc);

  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  buffer.buffer_type= MYSQL_TYPE_LONGLONG;
  buffer.buffer_length= sizeof(lii);
  buffer.buffer= (char *)&lii;
  buffer.length= &length;
  buffer.is_null= &is_null;

  rc= mysql_bind_result(stmt, &buffer);
  check_execute(stmt,rc);

  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt,rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt, rc);

  assert(is_null == 0 && lii == 1);
  fprintf(stdout, "\n\tLAST_INSERT_ID() = 1 ok\n");

  rc= mysql_query(mysql, "INSERT INTO t1 VALUES (NULL)");
  myquery(rc);

  rc = mysql_execute(stmt);
  check_execute(stmt,rc);

  rc = mysql_fetch(stmt);
  check_execute(stmt, rc);

  assert(is_null == 0 && lii == 2);
  fprintf(stdout, "\tLAST_INSERT_ID() = 2 ok\n");

  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "DROP TABLE t1");
  myquery(rc);
}


static void test_join()
{
  MYSQL_STMT *stmt;
  int rc, i, j;
  const char *query[]={"SELECT * FROM t2 join t1 on (t1.a=t2.a)",
		       "SELECT * FROM t2 natural join t1",
		       "SELECT * FROM t2 join t1 using(a)",
		       "SELECT * FROM t2 left join t1 on(t1.a=t2.a)",
		       "SELECT * FROM t2 natural left join t1",
		       "SELECT * FROM t2 left join t1 using(a)",
		       "SELECT * FROM t2 right join t1 on(t1.a=t2.a)",
		       "SELECT * FROM t2 natural right join t1",
		       "SELECT * FROM t2 right join t1 using(a)"};

  myheader("test_join");
  
  rc = mysql_query(mysql, "DROP TABLE IF EXISTS t1,t2");
  myquery(rc);
  
  rc= mysql_query(mysql,"CREATE TABLE t1 (a int , b int);");
  myquery(rc);

  rc= mysql_query(mysql,
		  "insert into t1 values (1,1), (2, 2), (3,3), (4,4), (5,5);");
  myquery(rc);

  rc= mysql_query(mysql,"CREATE TABLE t2 (a int , c int);");
  myquery(rc);

  rc= mysql_query(mysql,
		  "insert into t2 values (1,1), (2, 2), (3,3), (4,4), (5,5);");
  myquery(rc);

  for (j= 0; j < 9; j++)
  {
    stmt= mysql_prepare(mysql, query[j], strlen(query[j]));
    check_stmt(stmt);
    for (i= 0; i < 3; i++)
    {
      rc= mysql_execute(stmt);
      check_execute(stmt, rc);
      assert(5 == my_process_stmt_result(stmt));
    }
    mysql_stmt_close(stmt);
  }

  rc= mysql_query(mysql, "DROP TABLE t1,t2");
  myquery(rc);
}


static void test_selecttmp()
{
  MYSQL_STMT *stmt;
  int rc, i;
  const char *query= "select a,(select count(distinct t1.b) as sum from t1,t2 where t1.a=t2.a and t2.b > 0 and t1.a <= t3.b group by t1.a order by sum limit 1) from t3";

  myheader("test_select_tmp");
  
  rc = mysql_query(mysql, "DROP TABLE IF EXISTS t1,t2,t3");
  myquery(rc);
  
  rc= mysql_query(mysql,"CREATE TABLE t1 (a int , b int);");
  myquery(rc);

  rc= mysql_query(mysql,"create table t2 (a int, b int);");
  myquery(rc);

  rc= mysql_query(mysql,"create table t3 (a int, b int);");
  myquery(rc);

  rc= mysql_query(mysql,
		  "insert into t1 values (0,100),(1,2), (1,3), (2,2), (2,7), \
(2,-1), (3,10);");
  myquery(rc);
  rc= mysql_query(mysql,
		  "insert into t2 values (0,0), (1,1), (2,1), (3,1), (4,1);");
  myquery(rc);
  rc= mysql_query(mysql,
		  "insert into t3 values (3,3), (2,2), (1,1);");
  myquery(rc);

  stmt= mysql_prepare(mysql, query, strlen(query));
  check_stmt(stmt);
  for (i= 0; i < 3; i++)
  {
    rc= mysql_execute(stmt);
    check_execute(stmt, rc);
    assert(3 == my_process_stmt_result(stmt));
  }
  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "DROP TABLE t1,t2,t3");
  myquery(rc);
}


static void test_create_drop()
{
  MYSQL_STMT *stmt_create, *stmt_drop, *stmt_select, *stmt_create_select;
  char *query;
  int rc, i;
  myheader("test_table_manipulation");
  
  rc = mysql_query(mysql, "DROP TABLE IF EXISTS t1,t2");
  myquery(rc);

  rc= mysql_query(mysql,"create table t2 (a int);");
  myquery(rc);

  rc= mysql_query(mysql,"create table t1 (a int);");
  myquery(rc);

  rc= mysql_query(mysql, "insert into t2 values (3), (2), (1);");
  myquery(rc);
  
  query= (char*)"create table t1 (a int)";
  stmt_create= mysql_prepare(mysql, query, strlen(query));
  check_stmt(stmt_create);

  query= (char*)"drop table t1";
  stmt_drop= mysql_prepare(mysql, query, strlen(query));
  check_stmt(stmt_drop);

  query= (char*)"select a in (select a from t2) from t1";
  stmt_select= mysql_prepare(mysql, query, strlen(query));
  check_stmt(stmt_select);
  
  rc= mysql_query(mysql, "DROP TABLE t1");
  myquery(rc);

  query= (char*)"create table t1 select a from t2";
  stmt_create_select= mysql_prepare(mysql, query, strlen(query));
  check_stmt(stmt_create_select);

  for (i= 0; i < 3; i++)
  {
    rc= mysql_execute(stmt_create);
    check_execute(stmt_create, rc);
    fprintf(stdout, "created %i\n", i);

    rc= mysql_execute(stmt_select);
    check_execute(stmt_select, rc);
    assert(0 == my_process_stmt_result(stmt_select));

    rc= mysql_execute(stmt_drop);
    check_execute(stmt_drop, rc);
    fprintf(stdout, "droped %i\n", i);

    rc= mysql_execute(stmt_create_select);
    check_execute(stmt_create, rc);
    fprintf(stdout, "created select %i\n", i);

    rc= mysql_execute(stmt_select);
    check_execute(stmt_select, rc);
    assert(3 == my_process_stmt_result(stmt_select));

    rc= mysql_execute(stmt_drop);
    check_execute(stmt_drop, rc);
    fprintf(stdout, "droped %i\n", i);
  }
  
  mysql_stmt_close(stmt_create);
  mysql_stmt_close(stmt_drop);
  mysql_stmt_close(stmt_select);
  mysql_stmt_close(stmt_create_select);

  rc= mysql_query(mysql, "DROP TABLE t2");
  myquery(rc);
}


static void test_rename()
{
  MYSQL_STMT *stmt;
  const char *query= "rename table t1 to t2, t3 to t4";
  int rc;
  myheader("test_table_manipulation");
  
  rc = mysql_query(mysql, "DROP TABLE IF EXISTS t1,t2,t3,t4");
  myquery(rc);
  
  stmt= mysql_prepare(mysql, query, strlen(query));
  check_stmt(stmt);

  rc= mysql_query(mysql,"create table t1 (a int)");
  myquery(rc);

  rc= mysql_execute(stmt);
  check_execute_r(stmt, rc);
  fprintf(stdout, "rename without t3\n");

  rc= mysql_query(mysql,"create table t3 (a int)");
  myquery(rc);

  rc= mysql_execute(stmt);
  check_execute(stmt, rc);
  fprintf(stdout, "rename with t3\n");

  rc= mysql_execute(stmt);
  check_execute_r(stmt, rc);
  fprintf(stdout, "rename renamed\n");

  rc= mysql_query(mysql,"rename table t2 to t1, t4 to t3");
  myquery(rc);

  rc= mysql_execute(stmt);
  check_execute(stmt, rc);
  fprintf(stdout, "rename reverted\n");

  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "DROP TABLE t2,t4");
  myquery(rc);
}


static void test_do_set()
{
  MYSQL_STMT *stmt_do, *stmt_set;
  char *query;
  int rc, i;
  myheader("test_do_set");
  
  rc = mysql_query(mysql, "DROP TABLE IF EXISTS t1");
  myquery(rc);

  rc= mysql_query(mysql,"create table t1 (a int)");
  myquery(rc);
  
  query= (char*)"do @var:=(1 in (select * from t1))";
  stmt_do= mysql_prepare(mysql, query, strlen(query));
  check_stmt(stmt_do);

  query= (char*)"set @var=(1 in (select * from t1))";
  stmt_set= mysql_prepare(mysql, query, strlen(query));
  check_stmt(stmt_set);

  for (i= 0; i < 3; i++)
  {
    rc= mysql_execute(stmt_do);
    check_execute(stmt_do, rc);
    fprintf(stdout, "do %i\n", i);
    rc= mysql_execute(stmt_set);
    check_execute(stmt_set, rc);
    fprintf(stdout, "set %i\n", i);  
  }
  
  mysql_stmt_close(stmt_do);
  mysql_stmt_close(stmt_set);
}

static void test_multi()
{
  MYSQL_STMT *stmt_delete, *stmt_update, *stmt_select1, *stmt_select2;
  char *query;
  MYSQL_BIND bind[1];
  int rc, i;
  long param= 1, length= 1;
  myheader("test_multi");

  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (char *)&param;
  bind[0].buffer_length= 0;
  bind[0].is_null= 0;
  bind[0].length= &length;

  rc = mysql_query(mysql, "DROP TABLE IF EXISTS t1, t2");
  myquery(rc);

  rc= mysql_query(mysql,"create table t1 (a int, b int)");
  myquery(rc);

  rc= mysql_query(mysql,"create table t2 (a int, b int)");
  myquery(rc);

  rc= mysql_query(mysql,"insert into t1 values (3,3), (2,2), (1,1)");
  myquery(rc);

  rc= mysql_query(mysql,"insert into t2 values (3,3), (2,2), (1,1)");
  myquery(rc);
  
  query= (char*)"delete t1,t2 from t1,t2 where t1.a=t2.a and t1.b=10";
  stmt_delete= mysql_prepare(mysql, query, strlen(query));
  check_stmt(stmt_delete);

  query= (char*)"update t1,t2 set t1.b=10,t2.b=10 where t1.a=t2.a and t1.b=?";
  stmt_update= mysql_prepare(mysql, query, strlen(query));
  check_stmt(stmt_update);

  query= (char*)"select * from t1";
  stmt_select1= mysql_prepare(mysql, query, strlen(query));
  check_stmt(stmt_select1);

  query= (char*)"select * from t2";
  stmt_select2= mysql_prepare(mysql, query, strlen(query));
  check_stmt(stmt_select2);

  for(i= 0; i < 3; i++)
  {
    rc= mysql_bind_param(stmt_update, bind);
    check_execute(stmt_update,rc);

    rc= mysql_execute(stmt_update);
    check_execute(stmt_update, rc);
    fprintf(stdout, "update %ld\n", param);
  
    rc= mysql_execute(stmt_delete);
    check_execute(stmt_delete, rc);
    fprintf(stdout, "delete %ld\n", param);

    rc= mysql_execute(stmt_select1);
    check_execute(stmt_select1, rc);
    assert((uint)(3-param) == my_process_stmt_result(stmt_select1));

    rc= mysql_execute(stmt_select2);
    check_execute(stmt_select2, rc);
    assert((uint)(3-param) == my_process_stmt_result(stmt_select2));

    param++;
  }

  mysql_stmt_close(stmt_delete);
  mysql_stmt_close(stmt_update);
  mysql_stmt_close(stmt_select1);
  mysql_stmt_close(stmt_select2);
  rc= mysql_query(mysql,"drop table t1,t2");
  myquery(rc);
}


static void test_insert_select()
{
  MYSQL_STMT *stmt_insert, *stmt_select;
  char *query;
  int rc;
  uint i;
  myheader("test_insert_select");

  rc = mysql_query(mysql, "DROP TABLE IF EXISTS t1, t2");
  myquery(rc);

  rc= mysql_query(mysql,"create table t1 (a int)");
  myquery(rc);

  rc= mysql_query(mysql,"create table t2 (a int)");
  myquery(rc);

  rc= mysql_query(mysql,"insert into t2 values (1)");
  myquery(rc);
  
  query= (char*)"insert into t1 select a from t2";
  stmt_insert= mysql_prepare(mysql, query, strlen(query));
  check_stmt(stmt_insert);

  query= (char*)"select * from t1";
  stmt_select= mysql_prepare(mysql, query, strlen(query));
  check_stmt(stmt_select);

  for(i= 0; i < 3; i++)
  {
    rc= mysql_execute(stmt_insert);
    check_execute(stmt_insert, rc);
    fprintf(stdout, "insert %u\n", i);
  
    rc= mysql_execute(stmt_select);
    check_execute(stmt_select, rc);
    assert((i+1) == my_process_stmt_result(stmt_select));
  }

  mysql_stmt_close(stmt_insert);
  mysql_stmt_close(stmt_select);
  rc= mysql_query(mysql,"drop table t1,t2");
  myquery(rc);
}


static void test_bind_nagative()
{
  MYSQL_STMT *stmt_insert;
  char *query;
  int rc;
  MYSQL_BIND      bind[1];
  long            my_val = 0L;
  long            my_length = 0L;
  long            my_null = 0L;
  myheader("test_insert_select");

  rc = mysql_query(mysql, "DROP TABLE IF EXISTS t1");
  myquery(rc);

  rc= mysql_query(mysql,"create temporary table t1 (c1 int unsigned)");
  myquery(rc);

  rc= mysql_query(mysql,"INSERT INTO t1 VALUES (1),(-1)");
  myquery(rc);

  query= (char*)"INSERT INTO t1 VALUES (?)";
  stmt_insert= mysql_prepare(mysql, query, strlen(query));
  check_stmt(stmt_insert);

  /* bind parameters */
  bind[0].buffer_type = FIELD_TYPE_LONG;
  bind[0].buffer = (char *)&my_val;
  bind[0].length = &my_length;
  bind[0].is_null = (char*)&my_null;

  rc= mysql_bind_param(stmt_insert, bind);
  check_execute(stmt_insert,rc);

  my_val = -1;
  rc= mysql_execute(stmt_insert);
  check_execute(stmt_insert, rc);

  mysql_stmt_close(stmt_insert);
  rc= mysql_query(mysql,"drop table t1");
  myquery(rc);
}

static void test_derived()
{
  MYSQL_STMT *stmt;
  int rc, i;
  MYSQL_BIND      bind[1];
  long            my_val = 0L;
  long            my_length = 0L;
  long            my_null = 0L;
  const char *query=
    "select count(1) from (select f.id from t1 f where f.id=?) as x";

  myheader("test_derived");
  
  rc = mysql_query(mysql, "DROP TABLE IF EXISTS t1");
  myquery(rc);
  
  rc= mysql_query(mysql,"create table t1 (id  int(8), primary key (id)) \
TYPE=InnoDB DEFAULT CHARSET=utf8");
  myquery(rc);

  rc= mysql_query(mysql, "insert into t1 values (1)");
  myquery(rc);

  stmt= mysql_prepare(mysql, query, strlen(query));
  check_stmt(stmt);

  bind[0].buffer_type = FIELD_TYPE_LONG;
  bind[0].buffer = (char *)&my_val;
  bind[0].length = &my_length;
  bind[0].is_null = (char*)&my_null;
  my_val= 1;
  rc= mysql_bind_param(stmt, bind);
  check_execute(stmt,rc);

  for (i= 0; i < 3; i++)
  {
    rc= mysql_execute(stmt);
    check_execute(stmt, rc);
    assert(1 == my_process_stmt_result(stmt));
  }
  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "DROP TABLE t1");
  myquery(rc);
}


static void test_xjoin()
{
  MYSQL_STMT *stmt;
  int rc, i;
  const char *query=
    "select t.id,p1.value, n1.value, p2.value, n2.value from t3 t LEFT JOIN t1 p1 ON (p1.id=t.param1_id) LEFT JOIN t2 p2 ON (p2.id=t.param2_id) LEFT JOIN t4 n1 ON (n1.id=p1.name_id) LEFT JOIN t4 n2 ON (n2.id=p2.name_id) where t.id=1";

  myheader("test_xjoin");
  
  rc = mysql_query(mysql, "DROP TABLE IF EXISTS t1,t2,t3,t4");
  myquery(rc);
  
  rc= mysql_query(mysql,"create table t3 (id int(8), param1_id int(8), param2_id int(8)) TYPE=InnoDB DEFAULT CHARSET=utf8");
  myquery(rc);
  
  rc= mysql_query(mysql,"create table t1 ( id int(8), name_id int(8), value varchar(10)) TYPE=InnoDB DEFAULT CHARSET=utf8");
  myquery(rc);

  rc= mysql_query(mysql,"create table t2 (id int(8), name_id int(8), value varchar(10)) TYPE=InnoDB DEFAULT CHARSET=utf8;");
  myquery(rc);

  rc= mysql_query(mysql,"create table t4(id int(8), value varchar(10)) TYPE=InnoDB DEFAULT CHARSET=utf8");
  myquery(rc);

  rc= mysql_query(mysql, "insert into t3 values (1,1,1),(2,2,null)");
  myquery(rc);
  
  rc= mysql_query(mysql, "insert into t1 values (1,1,'aaa'),(2,null,'bbb')");
  myquery(rc);

  rc= mysql_query(mysql,"insert into t2 values (1,2,'ccc')");
  myquery(rc);

  rc= mysql_query(mysql, "insert into t4 values (1,'Name1'),(2,null)");
  myquery(rc);

  stmt= mysql_prepare(mysql, query, strlen(query));
  check_stmt(stmt);

  for (i= 0; i < 3; i++)
  {
    rc= mysql_execute(stmt);
    check_execute(stmt, rc);
    assert(1 == my_process_stmt_result(stmt));
  }
  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "DROP TABLE t1,t2,t3,t4");
  myquery(rc);
}

static void test_bug3035()
{
  MYSQL_STMT *stmt;
  int rc;
  MYSQL_BIND bind_array[12];
  int8 int8_val;
  uint8 uint8_val;
  int16 int16_val;
  uint16 uint16_val;
  int32 int32_val;
  uint32 uint32_val;
  longlong int64_val;
  ulonglong uint64_val;
  double double_val, udouble_val;
  char longlong_as_string[22],ulonglong_as_string[22];

  /* mins and maxes */
  const int8 int8_min= -128;
  const int8 int8_max= 127;
  const uint8 uint8_min= 0;
  const uint8 uint8_max= 255;
  
  const int16 int16_min= -32768;
  const int16 int16_max= 32767;
  const uint16 uint16_min= 0;
  const uint16 uint16_max= 65535;

  const int32 int32_max= 2147483647L;
  const int32 int32_min= -int32_max - 1;
  const uint32 uint32_min= 0;
  const uint32 uint32_max= 4294967295U;

  /* it might not work okay everyplace */
  const longlong int64_max= LL(9223372036854775807);
  const longlong int64_min= -int64_max - 1;

  const ulonglong uint64_min= 0U;
  const ulonglong uint64_max= ULL(18446744073709551615);
  
  const char *stmt_text;
  
  myheader("test_bug3035");
  
  stmt_text= "DROP TABLE IF EXISTS t1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  stmt_text= "CREATE TABLE t1 (i8 TINYINT, ui8 TINYINT UNSIGNED, "
                              "i16 SMALLINT, ui16 SMALLINT UNSIGNED, "
                              "i32 INT, ui32 INT UNSIGNED, "
                              "i64 BIGINT, ui64 BIGINT UNSIGNED, "
                              "id INTEGER NOT NULL PRIMARY KEY AUTO_INCREMENT)";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  bzero(bind_array, sizeof(bind_array));

  bind_array[0].buffer_type= MYSQL_TYPE_TINY;
  bind_array[0].buffer= (char*) &int8_val;

  bind_array[1].buffer_type= MYSQL_TYPE_TINY;
  bind_array[1].buffer= (char*) &uint8_val;
  bind_array[1].is_unsigned= 1;

  bind_array[2].buffer_type= MYSQL_TYPE_SHORT;
  bind_array[2].buffer= (char*) &int16_val;
  
  bind_array[3].buffer_type= MYSQL_TYPE_SHORT;
  bind_array[3].buffer= (char*) &uint16_val;
  bind_array[3].is_unsigned= 1;

  bind_array[4].buffer_type= MYSQL_TYPE_LONG;
  bind_array[4].buffer= (char*) &int32_val;

  bind_array[5].buffer_type= MYSQL_TYPE_LONG;
  bind_array[5].buffer= (char*) &uint32_val;
  bind_array[5].is_unsigned= 1;

  bind_array[6].buffer_type= MYSQL_TYPE_LONGLONG;
  bind_array[6].buffer= (char*) &int64_val;
  
  bind_array[7].buffer_type= MYSQL_TYPE_LONGLONG;
  bind_array[7].buffer= (char*) &uint64_val;
  bind_array[7].is_unsigned= 1;

  stmt= mysql_stmt_init(mysql);
  check_stmt(stmt);

  stmt_text= "INSERT INTO t1 (i8, ui8, i16, ui16, i32, ui32, i64, ui64) "
                     "VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);

  mysql_stmt_bind_param(stmt, bind_array);

  int8_val= int8_min;
  uint8_val= uint8_min;
  int16_val= int16_min;
  uint16_val= uint16_min;
  int32_val= int32_min;
  uint32_val= uint32_min;
  int64_val= int64_min;
  uint64_val= uint64_min;

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);
  
  int8_val= int8_max;
  uint8_val= uint8_max;
  int16_val= int16_max;
  uint16_val= uint16_max;
  int32_val= int32_max;
  uint32_val= uint32_max;
  int64_val= int64_max;
  uint64_val= uint64_max;

  mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  stmt_text= "SELECT i8, ui8, i16, ui16, i32, ui32, i64, ui64, ui64, cast(ui64 as signed),ui64, cast(ui64 as signed)"
             "FROM t1 ORDER BY id ASC";

  mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);

  mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  bind_array[8].buffer_type= MYSQL_TYPE_DOUBLE;
  bind_array[8].buffer= (char*) &udouble_val;

  bind_array[9].buffer_type= MYSQL_TYPE_DOUBLE;
  bind_array[9].buffer= (char*) &double_val;

  bind_array[10].buffer_type= MYSQL_TYPE_STRING;
  bind_array[10].buffer= (char*) &ulonglong_as_string;
  bind_array[10].buffer_length= sizeof(ulonglong_as_string);

  bind_array[11].buffer_type= MYSQL_TYPE_STRING;
  bind_array[11].buffer= (char*) &longlong_as_string;
  bind_array[11].buffer_length= sizeof(longlong_as_string);
  
  mysql_stmt_bind_result(stmt, bind_array);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  assert(int8_val == int8_min);
  assert(uint8_val == uint8_min);
  assert(int16_val == int16_min);
  assert(uint16_val == uint16_min);
  assert(int32_val == int32_min);
  assert(uint32_val == uint32_min);
  assert(int64_val == int64_min);
  assert(uint64_val == uint64_min);
  assert(double_val == (longlong) uint64_min);
  assert(udouble_val == ulonglong2double(uint64_val));
  assert(!strcmp(longlong_as_string, "0"));
  assert(!strcmp(ulonglong_as_string, "0"));

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);
  
  assert(int8_val == int8_max);
  assert(uint8_val == uint8_max);
  assert(int16_val == int16_max);
  assert(uint16_val == uint16_max);
  assert(int32_val == int32_max);
  assert(uint32_val == uint32_max);
  assert(int64_val == int64_max);
  assert(uint64_val == uint64_max);
  assert(double_val == (longlong) uint64_val);
  assert(udouble_val == ulonglong2double(uint64_val));
  assert(!strcmp(longlong_as_string, "-1"));
  assert(!strcmp(ulonglong_as_string, "18446744073709551615"));

  rc= mysql_stmt_fetch(stmt);
  assert(rc == MYSQL_NO_DATA); 

  mysql_stmt_close(stmt);

  stmt_text= "DROP TABLE t1";
  mysql_real_query(mysql, stmt_text, strlen(stmt_text));
}

/*
  Read and parse arguments and MySQL options from my.cnf
*/

static const char *client_test_load_default_groups[]= { "client", 0 };
static char **defaults_argv;

static struct my_option client_test_long_options[] =
{
  {"help", '?', "Display this help and exit", 0, 0, 0, GET_NO_ARG, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {"database", 'D', "Database to use", (char **) &opt_db, (char **) &opt_db,
   0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"debug", '#', "Output debug log", (gptr*) &default_dbug_option,
   (gptr*) &default_dbug_option, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"host", 'h', "Connect to host", (char **) &opt_host, (char **) &opt_host, 0, GET_STR_ALLOC,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"password", 'p',
   "Password to use when connecting to server. If password is not given it's asked from the tty.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#ifndef DONT_ALLOW_USER_CHANGE
  {"user", 'u', "User for login if not current user", (char **) &opt_user,
   (char **) &opt_user, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"port", 'P', "Port number to use for connection", (char **) &opt_port,
   (char **) &opt_port, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"socket", 'S', "Socket file to use for connection", (char **) &opt_unix_socket,
   (char **) &opt_unix_socket, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"count", 't', "Number of times test to be executed", (char **) &opt_count,
   (char **) &opt_count, 0, GET_UINT, REQUIRED_ARG, 1, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static void client_test_print_version(void)
{
  fprintf(stdout, "%s Distrib %s, for %s (%s)\n\n",
    my_progname,MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
}


static void usage(void)
{
  /*
   *  show the usage string when the user asks for this
  */    
  putc('\n',stdout);
  puts("***********************************************************************\n");
  puts("                Test for client-server protocol 4.1");
  puts("                        By Monty & Venu \n");
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,");
  puts("and you are welcome to modify and redistribute it under the GPL license\n");
  puts("                 Copyright (C) 1995-2003 MySQL AB ");
  puts("-----------------------------------------------------------------------\n");
  client_test_print_version();
  fprintf(stdout,"Usage: %s [OPTIONS]\n\n", my_progname);  
  
  my_print_help(client_test_long_options);
  print_defaults("my", client_test_load_default_groups);
  my_print_variables(client_test_long_options);

  puts("***********************************************************************\n");
}

static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  switch (optid) {
  case '#':
    DBUG_PUSH(argument ? argument : default_dbug_option);
    break;
  case 'p':
    if (argument)
    {
      char *start=argument;
      my_free(opt_password, MYF(MY_ALLOW_ZERO_PTR));
      opt_password= my_strdup(argument, MYF(MY_FAE));
      while (*argument) *argument++= 'x';		/* Destroy argument */
      if (*start)
        start[1]=0;
    }
    else
      tty_password= 1;
    break;
  case '?':
  case 'I':					/* Info */
    usage();
    exit(0);
    break;
  }
  return 0;
}

static void get_options(int argc, char **argv)
{
  int ho_error;

  if ((ho_error= handle_options(&argc, &argv, client_test_long_options, 
                                get_one_option)))
    exit(ho_error);

  if (tty_password)
    opt_password=get_tty_password(NullS);
  return;
}

/*
  Print the test output on successful execution before exiting
*/

static void print_test_output()
{
  fprintf(stdout,"\n\n");
  fprintf(stdout,"All '%d' tests were successful (in '%d' iterations)", 
          test_count-1, opt_count);
  fprintf(stdout,"\n  Total execution time: %g SECS", total_time);
  if (opt_count > 1)
    fprintf(stdout," (Avg: %g SECS)", total_time/opt_count);
  
  fprintf(stdout,"\n\n!!! SUCCESS !!!\n");
}

/********************************************************
* main routine                                          *
*********************************************************/
int main(int argc, char **argv)
{  
  DEBUGGER_OFF;
  MY_INIT(argv[0]);

  load_defaults("my",client_test_load_default_groups,&argc,&argv);
  defaults_argv= argv;
  get_options(argc,argv);
    
  client_connect();       /* connect to server */
  
  total_time= 0;
  for (iter_count=1; iter_count <= opt_count; iter_count++) 
  {
    /* Start of tests */
    test_count= 1;
   
    start_time= time((time_t *)0);

    client_query();         /* simple client query test */
#if NOT_YET_WORKING
    /* Used for internal new development debugging */
    test_drop_temp();       /* to test DROP TEMPORARY TABLE Access checks */
#endif
    test_fetch_seek();      /* to test stmt seek() functions */
    test_fetch_nobuffs();   /* to fecth without prior bound buffers */
    test_open_direct();     /* direct execution in the middle of open stmts */
    test_fetch_null();      /* to fetch null data */
    test_ps_null_param();   /* Fetch value of null parameter */
    test_fetch_date();      /* to fetch date,time and timestamp */
    test_fetch_str();       /* to fetch string to all types */
    test_fetch_long();      /* to fetch long to all types */
    test_fetch_short();     /* to fetch short to all types */
    test_fetch_tiny();      /* to fetch tiny to all types */
    test_fetch_bigint();    /* to fetch bigint to all types */
    test_fetch_float();     /* to fetch float to all types */
    test_fetch_double();    /* to fetch double to all types */
    test_bind_result_ext(); /* result bind test - extension */
    test_bind_result_ext1(); /* result bind test - extension */
    test_select_direct();   /* direct select - protocol_simple debug */
    test_select_prepare();  /* prepare select - protocol_prep debug */
    test_select();          /* simple select test */
    test_select_version();  /* select with variables */
    test_select_show_table();/* simple show prepare */
#if NOT_USED
  /* 
     Enable this tests from 4.1.1 when mysql_param_result() is 
     supported 
  */
    test_select_meta();     /* select param meta information */
    test_update_meta();     /* update param meta information */  
    test_insert_meta();     /* insert param meta information */
#endif
    test_func_fields();     /* test for new 4.1 MYSQL_FIELD members */
    test_long_data();       /* test for sending text data in chunks */
    test_insert();          /* simple insert test - prepare */
    test_set_variable();    /* prepare with set variables */
    test_select_show();     /* prepare - show test */
    test_prepare_noparam(); /* prepare without parameters */
    test_bind_result();     /* result bind test */   
    test_prepare_simple();  /* simple prepare */ 
    test_prepare();         /* prepare test */
    test_null();            /* test null data handling */
    test_debug_example();   /* some debugging case */
    test_update();          /* prepare-update test */
    test_simple_update();   /* simple prepare with update */
    test_simple_delete();   /* prepare with delete */
    test_double_compare();  /* float comparision */ 
    client_store_result();  /* usage of mysql_store_result() */
    client_use_result();    /* usage of mysql_use_result() */  
    test_tran_bdb();        /* transaction test on BDB table type */
    test_tran_innodb();     /* transaction test on InnoDB table type */ 
    test_prepare_ext();     /* test prepare with all types
			       conversion -- TODO */
    test_prepare_syntax();  /* syntax check for prepares */
    test_field_names();     /* test for field names */
    test_field_flags();     /* test to help .NET provider team */
    test_long_data_str();   /* long data handling */
    test_long_data_str1();  /* yet another long data handling */
    test_long_data_bin();   /* long binary insertion */
    test_warnings();        /* show warnings test */
    test_errors();          /* show errors test */
    test_prepare_resultset();/* prepare meta info test */
    test_stmt_close();      /* mysql_stmt_close() test -- hangs */
    test_prepare_field_result(); /* prepare meta info */
    test_multi_stmt();      /* multi stmt test */
    test_multi_statements();/* test multi statement execution */
    test_prepare_multi_statements(); /* check that multi statements are 
                                       disabled in PS */
    test_store_result();    /* test the store_result */
    test_store_result1();   /* test store result without buffers */
    test_store_result2();   /* test store result for misc case */
    test_subselect();       /* test subselect prepare -TODO*/
    test_date();            /* test the MYSQL_TIME conversion */
    test_date_date();       /* test conversion from DATE to all */
    test_date_time();       /* test conversion from TIME to all */
    test_date_ts()  ;       /* test conversion from TIMESTAMP to all */
    test_date_dt()  ;       /* test conversion from DATETIME to all */
    test_prepare_alter();   /* change table schema in middle of prepare */
    test_manual_sample();   /* sample in the manual */
    test_pure_coverage();   /* keep pure coverage happy */
    test_buffers();         /* misc buffer handling */
    test_ushort_bug();      /* test a simple conv bug from php */
    test_sshort_bug();      /* test a simple conv bug from php */
    test_stiny_bug();       /* test a simple conv bug from php */
    test_field_misc();      /* check the field info for misc case, bug: #74 */   
    test_set_option();      /* test the SET OPTION feature, bug #85 */
    test_prepare_grant();   /* to test the GRANT command, bug #89 */
    test_frm_bug();         /* test the crash when .frm is invalid, bug #93 */
    test_explain_bug();     /* test for the EXPLAIN, bug #115 */
    test_decimal_bug();     /* test for the decimal bug */
    test_nstmts();          /* test n statements */
    test_logs(); ;          /* to test logs */
    test_cuted_rows();      /* to test for WARNINGS from cuted rows */
    test_fetch_offset();    /* to test mysql_fetch_column with offset */
    test_fetch_column();    /* to test mysql_fetch_column */
    test_mem_overun();      /* test DBD ovverun bug */
    test_list_fields();     /* test COM_LIST_FIELDS for DEFAULT */
    test_free_result();     /* test mysql_stmt_free_result() */
    test_free_store_result(); /* test to make sure stmt results are cleared 
                                 during stmt_free_result() */
    test_sqlmode();         /* test for SQL_MODE */
    test_ts();              /* test for timestamp BR#819 */
    test_bug1115();         /* BUG#1115 */
    test_bug1180();         /* BUG#1180 */
    test_bug1500();         /* BUG#1500 */
    test_bug1644();	    /* BUG#1644 */
    test_bug1946();         /* test that placeholders are allowed only in 
                               prepared queries */
    test_bug2248();         /* BUG#2248 */ 
    test_parse_error_and_bad_length(); /* test if bad length param in
                                         mysql_prepare() triggers error */
    test_bug2247();         /* test that mysql_stmt_affected_rows() returns
                               number of rows affected by last prepared 
                               statement execution */
    test_subqueries();	    /* repeatable subqueries */
    test_bad_union();       /* correct setup of UNION */
    test_distinct();	    /* distinct aggregate functions */
    test_subqueries_ref();  /* outer reference in subqueries converted
			       Item_field -> Item_ref */
    test_union();	    /* test union with prepared statements */
    test_bug3117();	    /* BUG#3117: LAST_INSERT_ID() */
    test_join();	    /* different kinds of join, BUG#2794 */
    test_selecttmp();	    /* temporary table used in select execution */
    test_create_drop();	    /* some table manipulation BUG#2811 */
    test_rename();	    /* rename test */
    test_do_set();	    /* DO & SET commands test BUG#3393 */
    test_multi();	    /* test of multi delete & update */
    test_insert_select();   /* test INSERT ... SELECT */
    test_bind_nagative();   /* bind negative to unsigned BUG#3223 */
    test_derived();	    /* derived table with parameter BUG#3020 */
    test_xjoin();	    /* complex join test */
    test_bug3035();         /* inserts of INT32_MAX/UINT32_MAX */

    end_time= time((time_t *)0);
    total_time+= difftime(end_time, start_time);
    
    /* End of tests */
  }
  
  client_disconnect();    /* disconnect from server */
  free_defaults(defaults_argv);
  print_test_output();
  my_end(0);

  return(0);
}

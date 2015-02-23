#include <mysql.h>

extern "C" { 
	my_bool fb_create_init(UDF_INIT *initid, UDF_ARGS *args, char* message);
	long long fb_create(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);
	void fb_create_deinit(UDF_INIT *initid);
	my_bool fb_load_init(UDF_INIT *initid, UDF_ARGS *args, char* message);
	long long fb_load(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);
	void fb_load_deinit(UDF_INIT *initid);
	my_bool fb_query_init(UDF_INIT *initid, UDF_ARGS *args, char* message);
	char *fb_query(UDF_INIT *initid, UDF_ARGS *args,char *result, long long *length, char *is_null, char *error);
	void fb_query_deinit(UDF_INIT *initid);
	my_bool fb_insert_init(UDF_INIT *initid, UDF_ARGS *args, char* message);
	long long fb_insert(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);
	void fb_insert_deinit(UDF_INIT *initid);
	my_bool fb_delete_init(UDF_INIT *initid, UDF_ARGS *args, char* message);
	long long fb_delete(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);
	void fb_delete_deinit(UDF_INIT *initid);
	my_bool fb_unlink_init(UDF_INIT *initid, UDF_ARGS *args, char* message);
	long long fb_unlink(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);
	void fb_unlink_deinit(UDF_INIT *initid);
}

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)   // some identifier longer than 256 characters
#endif
#include <ibis.h>
#include <mensa.h>      // ibis::mensa
#include <twister.h>
#include <stdio.h>
#include <sstream>      // std::ostringstream
#include <iostream>      // std::ostringstream
#include <algorithm>    // std::sort
#include <memory>       // std::unique_ptr
#include <iomanip>      // std::setprecision


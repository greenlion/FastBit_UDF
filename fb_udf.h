#include <mysql.h>
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
//#include <sql_class.h>
//#include <hash.h>
//extern pthread_key_t THR_THD;

/*
class user_var_entry;
class Name_string;
*/



extern "C" { 
	my_bool fb_create_init(UDF_INIT *initid, UDF_ARGS *args, char* message);
	long long fb_create(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);
	void fb_create_deinit(UDF_INIT *initid);
	my_bool fb_load_init(UDF_INIT *initid, UDF_ARGS *args, char* message);
	long long fb_load(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);
	void fb_load_deinit(UDF_INIT *initid);
 //       user_var_entry *get_variable(HASH *hash, const Name_string &name, bool create_if_not_exists);
}

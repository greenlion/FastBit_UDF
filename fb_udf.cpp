#include <stdlib.h>
#include <string.h>
#include <mysql.h>
#include <sys/types.h>

#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)   // some identifier longer than 256 characters
#endif
#include <ibis.h>
#include <mensa.h>      // ibis::mensa
#include <twister.h>

#include <sstream>      // std::ostringstream
#include <algorithm>    // std::sort
#include <memory>       // std::unique_ptr
#include <iomanip>      // std::setprecision

my_bool fb_load_init(UDF_INIT *initid, UDF_ARGS *args, char* message) {
	int i;
	long scale;
	if(args->arg_count < 2 || args->arg_count > 3 ) {
		strcpy(message, "This function takes string arguments: index_path, load_file_path, [delimiter]"); 
		return 1;
	}

	for(i=0;i<args->arg_count;i++) {
		if(args->arg_type[i] != STRING_RESULT) {
			strcpy(message, "This function takes only string arguments: index_path, load_file_path, [delimiter]"); 
			return 1;
		} 
	}

	return 0;
}

long long fb_load(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error) {
	long long retval;
	retval = 0;
	return retval;
}

void fb_load_deinit(UDF_INIT *initid) { 

}

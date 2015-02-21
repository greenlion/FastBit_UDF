#include "fb_udf.h"

int file_exists (const char * file_name)
{
	struct stat buf;
	return(stat ( file_name, &buf ) == 0);
}

my_bool fb_create_init(UDF_INIT *initid, UDF_ARGS *args, char* message) {
	if(args->arg_count != 2) {
		strcpy(message, "This function takes string arguments: index_path, column_specification"); 
		return 1;
	}
	return 0;
}

long long fb_create(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error) {
	int err;
	long long retval = 0;
        if(args->args[0] == NULL || args->args[0] == NULL) {
                *is_null=1;
                return 0;
        }

	// The specified directory already exists
	if(!file_exists(args->args[0])) {
		err = mkdir(args->args[0], S_IRWXU | S_IXOTH);
		if(err != 0) {
			return(99998);
		}
	} else {
		return(99997);
	}

	// Does the colspec already exist?	
	std::string filename(args->args[0]);
        filename = filename + "/udf_colspec.txt";
	if(file_exists(filename.c_str())) {
		return(99999);
	}

	// Create the colref file
	FILE* fp;
	fp = fopen(filename.c_str(),"w");
	if(fp == NULL) {
		return(100000);
	}

	// Write out the contents of the file
	err = fputs(args->args[1],fp);
	fclose(fp);
	if(err < 0) {
		return(100001);
	}
	
	return retval;
}

void fb_create_deinit(UDF_INIT *initid) { 

}

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

#include "fb_udf.h"
/*
THD *current_thd() { return (THD*)pthread_getspecific(THR_THD); }
bool set_mysql_var(const char* name, const char* m) {
	THD *thd = current_thd();
	user_var_entry *entry = get_variable(&thd->user_vars, name, true);
	if(!entry) return false;	
	entry->reset_value();
	entry->store((void*)m, strlen(m), STRING_RESULT);
}*/

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
	if(args->arg_count < 2 || args->arg_count > 4 ) {
		strcpy(message, "This function takes the following arguments: index_path, load_file_path, delimiter, [max_rows_in_partition]"); 
		return 1;
	}

	for(int i=0;i<args->arg_count && i<3 ;i++) {
		if(args->arg_type[i] != STRING_RESULT) {
			std::string arg;
			switch(i) {
				case 0:
					arg = "index_path";	
				break;
				case 1:
					arg = "load_file_path";
				break;
				case 2:
					arg = "delimiter";
				break;

					
			}
			arg += " must be a string";
			strcpy(message, arg.c_str());
			return 1;
		} 
	}

	if(args->arg_count > 2 && args->arg_type[3] != INT_RESULT) {
		strcpy(message, "max_rows_in_partition must be an integer");
		return 1;
	}

	return 0;
}

long long fb_load(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error) {
	/* Handle arguments */
        std::string outdir(args->args[0]);
	std::string csv_path(args->args[1]);

	/* Use custom delimiter?  If not the default is comma "," */
	std::string del = ",";
	if(args->arg_count > 1) {
		del.assign(args->args[2]);
	}

	/* How many maximum rows in a partition? */
	long long pmax = 100000000;
	if(args->arg_count > 2) {
		pmax =  *(long long*)args->args[3];
	}

	const char* sel;

	std::unique_ptr<ibis::tablex> ta(ibis::tablex::create());
	ta->setPartitionMax(pmax);
	std::string metadatafile(outdir + "/udf_colspec.txt");
	ta->readNamesAndTypes(metadatafile.c_str());
	int ierr = ta->readCSV(csv_path.c_str(), 0, outdir.c_str(), del.c_str());
	if (ierr < 0) {
		std::string errmsg = " failed to parse file '" + csv_path + "' error from readCSV:" + std::to_string(ierr);
		return ierr;
	}
	std::string part_name = std::to_string(std::time(nullptr));
	int retval = ierr;
	ierr = ta->write(outdir.c_str(),part_name.c_str(), "", "", "");
	if (ierr < 0) {
		/*std::clog << *argv << " failed to write data in CSV file "
			<< csvfiles[i] << " to \"" << outdir
			<< "\", error code = " << ierr << std::endl; */
		return ierr;
	}
	ta->clearData();
	std::unique_ptr<ibis::table> tbl(ibis::table::create(outdir.c_str()));
	if (tbl.get() != 0) {
		tbl->buildIndexes(0);
        }

	return retval;
}

void fb_load_deinit(UDF_INIT *initid) { 

}

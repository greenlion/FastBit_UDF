/* FastBit MySQL UDF library
 * -- This library is inserted into a running MySQL server to enable creation and manipulation of 
 * -- bitmap indexed data using the FastBit WAH library.
 * (c) 2015 Justin Swanhart, all rights reserved
   Portions copyright 2001-2014 the Regents of the University of California
 */
#include "fb_udf.h"
#include <plugin.h>

typedef struct fbudf_insert_state_t {
  std::string outdir="";
  ibis::tablex* ta = ibis::tablex::create();
  bool errstate = false;
} FBISTATE, *FBISTATEPTR;

/*push_warning_printf(THD *thd, Sql_condition::enum_warning_level level,
			 uint code, const char *format, ...);

extern pthread_key_t THR_THD;
THD *current_thd() { return (THD*)pthread_getspecific(THR_THD); }
#define current_thd current_thd()
*/

const char *outputname = 0;
const char *ridfile = 0;
int verbose_level=  0; 

bool has_null(const UDF_ARGS *args, int stop_at = 0) {
	for(int i=0;i<args->arg_count && i <= stop_at ;i++) {
        	if(args->args[i] == NULL) {
			return 1;
	        }
	}
	return 0;

}

my_bool fb_debug_init(UDF_INIT *initid, UDF_ARGS *args, char* message) {
	if(args->arg_type[0] != INT_RESULT || args->arg_count != 1) {
		strcpy(message,"Takes args: debuglevel");
		return 1;
	}
	return 0;
}
long long fb_debug(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error) {
	if(has_null(args) || *(long long*)args->args[0] < 0) {
		*is_null =1;
		return 0;
	}
	ibis::gVerbose = *(long long*)args->args[0];
	return ibis::gVerbose;
}

void fb_debug_deinit(UDF_INIT *initid) { 

}


my_bool fb_unlink_init(UDF_INIT *initid, UDF_ARGS *args, char* message) {
	if(args->arg_count != 1) {
		strcpy(message, "Takes args: file_path"); 
		return 1;
	}
	if(args->arg_type[0] != STRING_RESULT) {
		strcpy(message, "file_path must be a string");
		return 1;
	}
	return 0;
}

long long fb_unlink(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error) {
	if(has_null(args)) {
		*is_null = 1;
		return 0;
	}
	int err;
	long long retval = 0;
	std::string file(args->args[0], args->lengths[0]);

	// The specified directory already exists
	if(!file_exists(file.c_str())) {
		return(-1);
	}
	long long ierr = unlink(file.c_str());
	return ierr;
}

void fb_unlink_deinit(UDF_INIT *initid) { 

}

my_bool fb_resort_init(UDF_INIT *initid, UDF_ARGS *args, char* message) {
	if(args->arg_count < 1) {
		strcpy(message, "Takes args: index_path, [col1], ..., [colN]"); 
		return 1;
	}
	for(int i=0;i<args->arg_count;i++) {
		if(args->arg_type[i] != STRING_RESULT) {
			std::string arg;
			switch(i) {
				case 0:
					arg = "index_path";	
				break;

				default:
					arg = "column_name";
				break;
			}
			arg += " must be a string";
			strcpy(message, arg.c_str());
			return 1;
		} 
	}
	return 0;

}

long long fb_resort(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error) {
	long ierr = 0;
	std::string colstr="";
	ibis::table::stringList slist;
	ibis::partList plist;
	std::string dir(args->args[0],args->lengths[0]);
	char *str;
	for(int i=1;i<args->arg_count;++i) {
		if(colstr!="") colstr += ",";
		colstr += std::string(args->args[i], args->lengths[i]);
	}

	if(args->arg_count > 1) {
		str = ibis::util::strnewdup(colstr.c_str());
		ibis::table::parseNames(str, slist);
        } 

	ibis::util::gatherParts(plist, dir.c_str());
        uint32_t nr = 0;
	for (ibis::partList::iterator it = plist.begin(); it != plist.end(); ++ it) {
            const char* ddir = (*it)->currentDataDir();
            {
                ibis::part tbl(ddir, static_cast<const char*>(0));
                if (args->arg_count > 1) {
                    ierr = tbl.reorder(slist);
                } else {
                    ierr = tbl.reorder();
                }
                nr = tbl.nRows();
            }
        }
	if(args->arg_count > 1) {
		delete[] str;
	}
	return nr;
}

void fb_resort(UDF_INIT *initid) { 

}



my_bool fb_create_init(UDF_INIT *initid, UDF_ARGS *args, char* message) {
	if(args->arg_count != 2) {
		strcpy(message, "Takes args: index_path, column_specification"); 
		return 1;
	}
	for(int i=0;i<args->arg_count;i++) {
		if(args->arg_type[i] != STRING_RESULT) {
			std::string arg;
			switch(i) {
				case 0:
					arg = "index_path";	
				break;
				case 1:
					arg = "colspec";
				break;

					
			}
			arg += " must be a string";
			strcpy(message, arg.c_str());
			return 1;
		} 
	}
	return 0;
}

long long fb_create(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error) {
	if(has_null(args)) {
		*is_null = 1;
		return 0;
	}
	int err;
	long long retval = 0;

	// The specified directory already exists
	if(!file_exists(args->args[0])) {
		err = mkdir(args->args[0], S_IRWXU | S_IXOTH);
		if(err != 0) {
			return(-1);
		}
	} else {
		return(-2);
	}

	// Does the colspec already exist?	
	std::string filename(args->args[0]);
        filename = filename + "/udf_colspec.txt";
	if(file_exists(filename.c_str())) {
		return(-3);
	}

	// Create the colref file
	FILE* fp;
	fp = fopen(filename.c_str(),"w");
	if(fp == NULL) {
		return(-4);
	}

	// Write out the contents of the file
	err = fputs(args->args[1],fp);
	fclose(fp);
	if(err < 0) {
		return(-5);
	}
	
	return retval;
}

void fb_create_deinit(UDF_INIT *initid) { 

}

my_bool fb_insert_init(UDF_INIT *initid, UDF_ARGS *args, char *message) {
	if(args->arg_count < 2) {
		strcpy(message, "Takes args: index_path, insert_string, [delimiter=,]"); 
		return 1;
	}
	for(int i=0;i<args->arg_count;i++) {
		if(args->arg_type[i] != STRING_RESULT) {
			std::string arg;
			switch(i) {
				case 0:
					arg = "index_path";	
				break;
				case 1:
					arg = "insert_string";
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
	FBISTATEPTR s = new FBISTATE;
	initid->ptr = (char*)s;
	return 0;
}

long long fb_insert(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error) {
	if(has_null(args)) {
		*is_null = 1;
		return 0;
	}
	long long ierr;
	std::string outdir(args->args[0],args->lengths[0]);
	std::string insert(args->args[1],args->lengths[1]);
	std::string del = ",";
	if(args->arg_count > 2) { 
		del.assign(args->args[2],args->lengths[2]);
	}
	FBISTATEPTR s = (FBISTATEPTR)initid->ptr;
	if (s->outdir == "") {
		s->outdir.assign(args->args[0],args->lengths[0]);
		std::string metadatafile(s->outdir + "/udf_colspec.txt");
		s->outdir += "/inserts";

		long long ierr = s->ta->readNamesAndTypes(metadatafile.c_str());
		if(ierr < 0) {
			s->errstate = true;
			strcpy(error, "Could not read names and types");	
			return ierr;
		}
		s->ta->setPartitionMax(100000000);
		
	} else if(s->outdir != outdir) {
		s->ta->write(s->outdir.c_str(), "", "", ""); 
		s->ta->clearData();
		s->outdir = outdir;
	} 
		
	ierr = s->ta->appendRow(insert.c_str(), del.c_str());
	if(ierr < 0) {
		s->errstate = true;
		strcpy(error, "INSERT ERROR");
		return ierr;
	}
	return 1;
}

void fb_insert_deinit(UDF_INIT *initid) { 
	FBISTATEPTR s = (FBISTATEPTR)initid->ptr;
	if(!s->errstate) s->ta->write(s->outdir.c_str(), "", "", ""); 
	free(s->ta);
	delete s;
}


my_bool fb_insert2_init(UDF_INIT *initid, UDF_ARGS *args, char *message) {
	if(args->arg_count < 2) {
		strcpy(message, "Takes args: index_path, val1, ..., valN"); 
		return 1;
	}
	if(args->arg_type[0] != STRING_RESULT) {
		strcpy(message, "index_path must be a string");
		return 1;
	}
	FBISTATEPTR s = new FBISTATE;
	initid->ptr = (char*)s;
	
	return 0;
}


long long fb_insert2(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error) {
	if(has_null(args,0)) {
		*is_null = 1;
		return 0;
	}
	FBISTATEPTR s = (FBISTATEPTR)initid->ptr;
	long long ierr;
	std::string outdir;
	outdir.assign(args->args[0], args->lengths[0]);

	if (s->outdir == "") {
		s->outdir = outdir;
		std::string metadatafile(s->outdir + "/udf_colspec.txt");
		s->outdir += "/inserts";

		ierr = s->ta->readNamesAndTypes(metadatafile.c_str());
		if(ierr < 0) {
			s->errstate = true;
			strcpy(error, "Could not read names and types");	
			return ierr;
		}
		s->ta->setPartitionMax(100000000);
		
	} else if(s->outdir != outdir) {
		s->ta->write(s->outdir.c_str(), "", "", ""); 
		s->ta->clearData();
		s->outdir = outdir;
	} 
		
	std::string insert = "";
	for(int i = 1; i < args->arg_count; ++i) {
		if(insert != "") insert += ",";
		if(args->args[i] == NULL) {
			continue;
		}
		if(args->arg_type[i] == STRING_RESULT) {
			insert +=  "'" + std::string(args->args[i], args->lengths[i]) + "'";
			continue;
		} 	
		if(args->arg_type[i] == INT_RESULT) {
			insert +=  std::to_string(*(long long *)args->args[i]); 
			continue;
		}
		insert +=  std::to_string(*(double *)args->args[i]); 
	}	

	ierr = s->ta->appendRow(insert.c_str(), ",");
	if(ierr < 0) {
		strcpy(error,"INSERT ERROR");
		s->errstate = true;
		strcpy(error, "Could not insert");	
		return ierr;
	}

	return 1;
}

void fb_insert2_deinit(UDF_INIT *initid) { 
	FBISTATEPTR s = (FBISTATEPTR)initid->ptr;
	if(!s->errstate) s->ta->write(s->outdir.c_str(), "", "", ""); 
	free(s->ta);
	delete s;
}


my_bool fb_delete_init(UDF_INIT *initid, UDF_ARGS *args, char *message) {
	if(args->arg_count != 2) {
		strcpy(message, "Takes args: idx_path, where_clause"); 
		return 1;
	}
	return 0;
}

long long fb_delete(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error) {
	if(has_null(args)) {
		*is_null = 1;
		return 0;
	}
	long ierr = 0;
	long long retval = 0;
	ibis::partList parts;
	std::string path(args->args[0], args->lengths[0]);
	std::string yankstring(args->args[1], args->lengths[1]);

	std::string s = yankstring.substr(0,5); 
	std::transform(s.begin(), s.end(), s.begin(), ::tolower);
	if (s == "where") {
		yankstring = yankstring.substr(6,yankstring.length());
	}

	if (yankstring == "") return 0;
	int part_count = ibis::util::gatherParts(parts, path.c_str());
	if(part_count <= 0) return 0;

	for (ibis::partList::iterator it = parts.begin();
		it != parts.end(); ++ it) {
		ierr = (*it)->deactivate(yankstring.c_str());
		if(ierr < 0) return ierr;
		retval += ierr;
		ierr = (*it)->purgeInactive();
		if(ierr < 0) return ierr;
	}
	return retval;
}

void fb_delete_deinit(UDF_INIT *initid) {

}

my_bool fb_load_init(UDF_INIT *initid, UDF_ARGS *args, char* message) {
	if(args->arg_count < 2 || args->arg_count > 4 ) {
		strcpy(message, "Takes args: idx_path, load_path, [delim=,], [max_rows_in_part=100000000]"); 
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
		strcpy(message, "max_rows_in_part must be an integer");
		return 1;
	}

	return 0;
}

long long fb_load(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error) {
	if(has_null(args)) {
		*is_null = 1;
		return 0;
	}
	/* Handle arguments */
        std::string outdir(args->args[0],args->lengths[0]);
	std::string csv_path(args->args[1],args->lengths[1]);

	/* Use custom delimiter?  If not the default is comma "," */
	std::string del = ",";
	if(args->arg_count > 2) {
		del.assign(args->args[2],args->lengths[2]);
	}

	/* How many maximum rows in a partition? */
	long long pmax = 100000000;
	if(args->arg_count > 3) {
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
	std::string tmp = outdir + "/" + part_name;
	ierr = ta->write(tmp.c_str(), "", "", "");
	if (ierr < 0) {
		/*std::clog << *argv << " failed to write data in CSV file "
			<< csvfiles[i] << " to \"" << outdir
			<< "\", error code = " << ierr << std::endl; */
		return ierr;
	}
	ta->clearData();
	std::unique_ptr<ibis::table> tbl(ibis::table::create(tmp.c_str()));
	if (tbl.get() != 0) {
		tbl->buildIndexes(0);
        }

	return retval;
}

void fb_load_deinit(UDF_INIT *initid) { 

}

my_bool fb_query_init(UDF_INIT *initid, UDF_ARGS *args, char* message) {
	if(args->arg_count < 3) {
		strcpy(message, "Takes args: index_path, output_path, fastbit_select"); 
		return 1;
	}

        initid->ptr = NULL;

	std::string msg = "";
	for(int i=0;i<args->arg_count && i < 3 ;i++) {
		if(args->arg_type[i] != STRING_RESULT) {
			if(msg != "") {
				msg += ", ";
			}
			switch(i) {
				case 0:
					msg += "index_path";	
				break;
				case 1:
					msg += "output_path";
				break;
				case 2:
					msg += "select_string";
				break;
			}
			msg += " must be a string";
		} 
	}

	if(msg != "") {
		strcpy(message, msg.c_str());
		return 1;
	}

	return 0;
}

char* fb_query(UDF_INIT *initid, UDF_ARGS *args, char *result, long long *length, char *is_null, char *error) {
	if(has_null(args)) {
		*is_null = 1;
		return 0;
	}
	char *retval = 0;
	std::string path = "", out_file = "", select = "";
	unsigned part_count = 0;
	const char* uid = ibis::util::userName();
	long long limit = 0;
	long long offset = 0;
	ibis::partList parts;
	std::string message = "";
	std::ostringstream datatypes;

	path.assign(args->args[0], args->lengths[0]);
	out_file.assign(args->args[1], args->lengths[1]);
	select.assign(args->args[2], args->lengths[2]);

	// TODO: support output to named pipes
	if(file_exists(out_file.c_str())) {
		message = "QUERY_ERROR Output file exists";
	} else {
		// gather the partitions in the index directory
		part_count = ibis::util::gatherParts(parts, path.c_str());

		// if there are no data partitions return an error
		if( part_count == 0 || parts.empty()) {
			message = "QUERY_ERROR: No data partitions found";
		} else { /* actually run the query */
			int result = processSelect(uid, select.c_str(), parts, out_file,datatypes);
			if(result < 0) {
				message = "QUERY_ERROR processSelect reported error: " + std::to_string(result);
			} else {
				message = "QUERY_OK " + std::to_string(result) + " (" + datatypes.str() + ")";
			}
		}
	}

        if(message.length() > MAXSTR) {
		initid->ptr = (char*)malloc(*length);	
		*length = message.length();
		memcpy(initid->ptr, message.c_str(), *length);
		return initid->ptr;
        } else { 
		memset(result,0,MAXSTR);
		*length = message.length();
		strncpy(result, message.c_str(), *length);
	}
	return result;
	
}

void fb_query_deinit(UDF_INIT *initid) { 
	if(initid->ptr != NULL) {
		free(initid->ptr);
	}
}
    /*
    HELPER FUNCTIONS 
    These functions are modified slightly from the ibis.cpp example from FastBit
    Author: John Wu <John.Wu at ACM.org>
    Lawrence Berkeley National Laboratory
    Copyright 2001-2014 the Regents of the University of California
    -----------------------------------------------------------------------------
    */

static void printQueryResults(std::ostream &out, ibis::query &q) {
    ibis::query::result cursor(q);
    const unsigned w = cursor.width();
    out << "printing results of query " << q.id() << " (numHits="
	<< q.getNumHits() << ")\n";
    cursor.printColumnNames(out);
    out << "\n";
    if (w == 0) return;

    while (cursor.next()) {
	out << cursor.getString(static_cast<uint32_t>(0U));
	for (uint32_t i = 1; i < w; ++ i)
	    out << ", " << cursor.getString(i);
	out << "\n";
    }
} // printQueryResults

int file_exists (const char * file_name)
{
	struct stat buf;
	return(stat ( file_name, &buf ) == 0);
}

static long long tableSelect(const ibis::partList &pl, const char* uid,
			const char* wstr, const char* sstr,
			const char* ordkeys,
			uint32_t limit, uint32_t start, std::string out_file,std::ostringstream& datatypes){
    int64_t ierr;
    std::unique_ptr<ibis::table> tbl(ibis::table::create(pl));
    ibis::horometer timer;
    std::ofstream outputstream;

    std::unique_ptr<ibis::table> sel1(tbl->select(sstr, wstr));
    if (sel1.get() == 0) {
	return -1;
    }

    outputstream.open(out_file);

    if (sel1->nRows() > 1 && ((ordkeys && *ordkeys) || limit > 0)) {
	// top-K query
	sel1->orderby(ordkeys);
    }

    if (showheader)
        sel1->dumpNames(outputstream, ", ");
    if (limit == 0)
        limit = static_cast<uint32_t>(sel1->nRows());
    ibis::table::stringList SL = sel1->columnNames();
    ibis::table::typeList CL = sel1->columnTypes();
    int i=0;
    for (ibis::table::stringList::iterator it = SL.begin();
        it != SL.end(); ++ it) {
        if(datatypes.str() != "") datatypes << ", ";
        datatypes << "`" + std::string(*it) + "`"; 
        ibis::TYPE_T t = CL[i];
        ++i;

        switch(t) {
            case ibis::BYTE:
                datatypes << " TINYINT";
                break;
            case ibis::UBYTE:
                datatypes << " TINYINT UNSIGNED";
                break;
            case ibis::SHORT:
                datatypes << " SMALLINT";
                break;
            case ibis::USHORT:
                datatypes << " SMALLINT UNSIGNED";
                break;
            case ibis::INT:
                datatypes << " INT";
                break;
            case ibis::UINT:
                datatypes << " INT UNSIGNED";
                break;
            case ibis::LONG:
                datatypes << " BIGINT";
                break;
            case ibis::ULONG:
                datatypes << " BIGINT UNSIGNED";
            break;
            case ibis::FLOAT:
                datatypes << " FLOAT";
            break;
            case ibis::DOUBLE:
                datatypes << " DOUBLE";
            break;
            case ibis::TEXT:
                datatypes << " LONGTEXT";
            break;

            default:
                datatypes << " LONGBLOB";
        }
    }
        
    ierr = sel1->dump(outputstream, start, limit, ", ");
    outputstream.close();
    return sel1->nRows();
} // tableSelect

// parse the query string and evaluate the specified query
static long long processSelect(const char* uid, const char* qstr,
			ibis::partList& prts, std::string out_file, std::ostringstream &datatypes) {
    if (qstr == 0 || *qstr == 0) return -100;

    // got a valid string
    const char* str = qstr;
    const char* end;
    std::string fstr; // from clause
    std::string sstr; // select clause
    std::string wstr; // where clause
    std::string ordkeys; // order by clause (the order keys)
    uint32_t start = 0; // the 1st row to print
    uint32_t limit = 0; // the limit on the number of output rows
    //const bool usequaere = (outputbinary || strchr(qstr, '.') != 0);
    const bool usequaere = false;
    int independent_parts = false;
    long long row_count=0;

    // skip leading space
    while (std::isspace(*str)) ++str;
    // look for key word SELECT
    if (0 == strnicmp(str, "select ", 7)) {
	str += 7;
	while (std::isspace(*str)) ++str;
	// look for the next key word (either FROM or WHERE)
	end = strstr(str, " from ");
	if (end == 0) {
	    end = strstr(str, " FROM ");
	    if (end == 0)
		end = strstr(str, " From ");
	}
	if (end) { // found FROM clause
	    while (str < end) {
		sstr += *str;
		++ str;
	    }
	    str = end + 1;
	}
	else { // no FROM clause, try to locate WHERE
	    end = strstr(str, " where ");
	    if (end == 0) {
		end = strstr(str, " WHERE ");
		if (end == 0)
		    end = strstr(str, " Where ");
	    }
	    if (end == 0) {
		sstr = str;
		str = 0;
	    }
	    else {
		while (str < end) {
		    sstr += *str;
		    ++ str;
		}
		str = end + 1;
	    }
	}
    }

    // look for key word FROM
    if (str != 0 && 0 == strnicmp(str, "from ", 5)) {
	str += 5;
	while (std::isspace(*str)) ++str;
	end = strstr(str, " where "); // look for key word WHERE
	if (end == 0) {
	    end = strstr(str, " WHERE ");
	    if (end == 0)
		end = strstr(str, " Where ");
	}
	if (end == 0 && sstr.empty()) {
	    LOGGER(verbose_level >= 0)
		<< "processSelect(" << qstr << ") is unable to locate "
		<< "key word WHERE following FROM clause";
	    return -200;
	}
	else if (end != 0) {
	    fstr.append(str, end);
	    str = end + 1;
	}
	else {
	    fstr = str;
	    str = 0;
	}
    }

    // check for the WHERE clause
    if (str == 0 || *str == 0) {
	if (sstr.empty()) {
	    LOGGER(verbose_level >= 0)
		<< "Unable to fund a where clause or a select clause in the "
		"query string \"" << qstr << "\"";
	    return -300;
	}
	else {
	    str = "where 1=1";
	}
    }
    if (0 == strnicmp(str, "where ", 6)) {
	str += 6;
    }
    else if (verbose_level > 1) {
	ibis::util::logger lg;
	lg() << "processSelect(" << qstr
	     << ") is unable to locate key word WHERE.  "
	    "assume the string is the where clause.";
    }
    // the end of the where clause is marked by the key words "order by" or
    // "limit" or the end of the string
    if (str != 0) {
	end = strstr(str, "order by");
	if (end == 0) {
	    end = strstr(str, "ORDER BY");
	    if (end == 0)
		end = strstr(str, "Order By");
	    if (end == 0)
		end = strstr(str, "Order by");
	    if (end == 0)
		end = strstr(str, "limit");
	    if (end == 0)
		end = strstr(str, "Limit");
	    if (end == 0)
		end = strstr(str, "LIMIT");
	}
	if (end != 0) {
	    while (str < end) {
		wstr += *str;
		++ str;
	    }
	}
	else {
	    while (*str != 0) {
		wstr += *str;
		++ str;
	    }
	}
    }

    if (str != 0 && 0 == strnicmp(str, "order by ", 9)) { // order by clause
	// the order by clause may be the last clause or followed by the
	// key word "LIMIT"
	str += 9;
	end = strstr(str, "limit");
	if (end == 0) {
	    end = strstr(str, "Limit");
	    if (end == 0)
		end = strstr(str, "LIMIT");
	}
	if (end != 0) {
	    while (str < end) {
		ordkeys += *str;
		++ str;
	    }
	}
	else {
	    while (*str) {
		ordkeys += *str;
		++ str;
	    }
	}
    }
    while (str != 0 && *str && std::isspace(*str)) // skip blank spaces
	++ str;
    if (str != 0 && 0 == strnicmp(str, "limit ", 6)) {
	str += 6;
	uint64_t tmp;
	int ierr = ibis::util::readUInt(tmp, str, ", ");
	if (ierr < 0) {
	    LOGGER(verbose_level >= 0)
		<< "Warning -- processSelect(" << qstr
		<< ") expects a unsigned interger following "
		"the keyword LIMIT, but got '" << *str
		<< "', skip the limit clause";
	}
	else if (std::isspace(*str) || *str == ',') {
	    for (++ str; *str != 0 && (std::isspace(*str) || *str == ',');
                 ++ str);
	    limit = static_cast<uint32_t>(tmp);
	    ierr = ibis::util::readUInt(tmp, str, 0);
	    if (ierr >= 0) {
		start = limit;
		limit = static_cast<uint32_t>(tmp);
	    }
	}
	else if (*str == 0) {
	    limit = static_cast<uint32_t>(tmp);
	}
	else {
	    ibis::util::logger()()
		<< "Warning -- processSelect(" << qstr
		<< ") reached a unexpected end of string \"" << str << "\"";
	}
    }
    else if (str != 0 && *str != 0 && verbose_level >= 0) {
	ibis::util::logger()()
            << "Warning -- processSelect(" << qstr
            << ") expects the key word LIMIT, but got " << str;
    }

    ibis::nameList qtables(fstr.c_str());
    if (usequaere) {
	doQuaere(prts, sstr.c_str(), fstr.c_str(), wstr.c_str(),
		 ordkeys.c_str(), limit, start);
    }
    else if (independent_parts == 0 ||
             (! sstr.empty() && (sstr.find('(') < sstr.size() ||
                                 sstr.find(" as ") < sstr.size()))) {
	//  || recheckvalues || !ordkeys.empty() || limit > 0
	// more complex select clauses need tableSelect
	if (! qtables.empty()) {
	    ibis::partList tl2;
	    for (unsigned k = 0; k < prts.size(); ++ k) {
		for (unsigned j = 0; j < qtables.size(); ++ j) {
		    if (stricmp(prts[k]->name(), qtables[j]) == 0 ||
			ibis::util::strMatch(prts[k]->name(),
					     qtables[j])) {
			tl2.push_back(prts[k]);
			break;
		    }
		}
	    }
	    try {
		row_count = tableSelect(tl2, uid, wstr.c_str(), sstr.c_str(),
			    ordkeys.c_str(), limit, start, out_file,datatypes);
	    }
	    catch (...) {
		if (ibis::util::serialNumber() % 3 == 0) {
		    ibis::util::quietLock lock(&ibis::util::envLock);
#if defined(__unix__) || defined(__linux__) || defined(__CYGWIN__) || defined(__APPLE__) || defined(__FreeBSD)
		    sleep(1);
#endif
		}
		for (ibis::partList::iterator it = tl2.begin();
		     it != tl2.end(); ++ it) {
		    (*it)->emptyCache();
		}
		row_count = tableSelect(tl2, uid, wstr.c_str(), sstr.c_str(),
			    ordkeys.c_str(), limit, start, out_file,datatypes);
	    }
	}
	else {
	    try {
		row_count = tableSelect(prts, uid, wstr.c_str(), sstr.c_str(),
			    ordkeys.c_str(), limit, start, out_file,datatypes);
	    }
	    catch (...) {
		if (ibis::util::serialNumber() % 3 == 0) {
		    ibis::util::quietLock lock(&ibis::util::envLock);
#if defined(__unix__) || defined(__linux__) || defined(__CYGWIN__) || defined(__APPLE__) || defined(__FreeBSD)
		    sleep(1);
#endif
		}
		for (ibis::partList::iterator it = prts.begin();
		     it != prts.end(); ++ it) {
		    (*it)->emptyCache();
		}
		row_count = tableSelect(prts, uid, wstr.c_str(), sstr.c_str(),
			    ordkeys.c_str(), limit, start, out_file,datatypes);
	    }
	}
    }
    else if (! qtables.empty()) {
	// simple select clauses can be handled through doQuery
	for (unsigned k = 0; k < prts.size(); ++ k) {
	    // go through each partition the user has specified
	    for (unsigned j = 0; j < qtables.size(); ++ j) {
		if (stricmp(prts[k]->name(), qtables[j]) == 0 ||
		    ibis::util::strMatch(prts[k]->name(),
					 qtables[j])) {
		    if ( recheckvalues || sequential_scan || 
			prts[k]->getMeshShape().empty()) {
			try {
			    doQuery(prts[k], uid, wstr.c_str(),
                                    sstr.c_str(), ordkeys.c_str(),
				    limit, start);
			}
			catch (...) {
			    if (ibis::util::serialNumber() % 3 == 0) {
				ibis::util::quietLock
				    lock(&ibis::util::envLock);
#if defined(__unix__) || defined(__linux__) || defined(__CYGWIN__) || defined(__APPLE__) || defined(__FreeBSD)
				sleep(1);
#endif
			    }
			    prts[k]->emptyCache();
			    doQuery(prts[k], uid, wstr.c_str(),
				    sstr.c_str(), ordkeys.c_str(),
				    limit, start);
			}
		    }
		    else {
			try {
			    doMeshQuery(prts[k], uid, wstr.c_str(),
					sstr.c_str());
			}
			catch (...) {
			    if (ibis::util::serialNumber() % 3 == 0) {
				ibis::util::quietLock
				    lock(&ibis::util::envLock);
#if defined(__unix__) || defined(__linux__) || defined(__CYGWIN__) || defined(__APPLE__) || defined(__FreeBSD)
				sleep(1);
#endif
			    }
			    prts[k]->emptyCache();
			    doMeshQuery(prts[k], uid, wstr.c_str(),
					sstr.c_str());
			}
		    }

		    if (verbose_level > 7 || testing > 0) {
			try {
			    xdoQuery(prts[k], uid, wstr.c_str(), sstr.c_str());
			}
			catch (...) {
			    if (ibis::util::serialNumber() % 3 == 0) {
				ibis::util::quietLock
				    lock(&ibis::util::envLock);
#if defined(__unix__) || defined(__linux__) || defined(__CYGWIN__) || defined(__APPLE__) || defined(__FreeBSD)
				sleep(1);
#endif
			    }
			    prts[k]->emptyCache();
			}
		    }
		    break;
		}
	    }
	}
    }
    else {
	for (ibis::partList::iterator tit = prts.begin();
	     tit != prts.end(); ++ tit) {
	    // go through every partition and process the user query
	    if (recheckvalues || sequential_scan ||
		(*tit)->getMeshShape().empty()) {
		try {
		    doQuery((*tit), uid, wstr.c_str(), sstr.c_str(),
			    ordkeys.c_str(), limit, start);
		}
		catch (...) {
		    if (ibis::util::serialNumber() % 3 == 0) {
			ibis::util::quietLock lock(&ibis::util::envLock);
#if defined(__unix__) || defined(__linux__) || defined(__CYGWIN__) || defined(__APPLE__) || defined(__FreeBSD)
			sleep(1);
#endif
		    }
		    (*tit)->emptyCache();
		    doQuery((*tit), uid, wstr.c_str(), sstr.c_str(),
			    ordkeys.c_str(), limit, start);
		}
	    }
	    else {
		try {
		    doMeshQuery((*tit), uid, wstr.c_str(), sstr.c_str());
		}
		catch (...) {
		    if (ibis::util::serialNumber() % 3 == 0) {
			ibis::util::quietLock lock(&ibis::util::envLock);
#if defined(__unix__) || defined(__linux__) || defined(__CYGWIN__) || defined(__APPLE__) || defined(__FreeBSD)
			sleep(1);
#endif
		    }
		    (*tit)->emptyCache();
		    doMeshQuery((*tit), uid, wstr.c_str(), sstr.c_str());
		}
	    }

	    if (verbose_level > 7 || testing > 0) {
		try {
		    xdoQuery((*tit), uid, wstr.c_str(), sstr.c_str());
		}
		catch (...) {
		    if (ibis::util::serialNumber() % 3 == 0) {
			ibis::util::quietLock lock(&ibis::util::envLock);
#if defined(__unix__) || defined(__linux__) || defined(__CYGWIN__) || defined(__APPLE__) || defined(__FreeBSD)
			sleep(1);
#endif
		    }
		    (*tit)->emptyCache();
		}
	    }
	}
    }
    return row_count;
} // processSelect

// New style query.
static void doQuaere(const ibis::partList& pl,
		     const char *sstr, const char *fstr, const char *wstr,
		     const char *ordkeys, uint32_t limit, uint32_t start) {
    ibis::horometer timer;
    timer.start();
    std::string sqlstring; //
    {
	std::ostringstream ostr;
	if (sstr != 0 && *sstr != 0)
	    ostr << "SELECT " << sstr;
	if (fstr != 0 && *fstr != 0)
	    ostr << " FROM " << fstr;
	if (wstr != 0 && *wstr != 0)
	    ostr << " WHERE " << wstr;
	if (ordkeys && *ordkeys) {
	    ostr << " ORDER BY " << ordkeys;
	}
	if (limit > 0) {
	    ostr << " LIMIT ";
	    if (start > 0) ostr << start << ", ";
	    ostr << limit;
	}
	sqlstring = ostr.str();
    }
    LOGGER(verbose_level > 1)
	<< "doQuaere -- processing \"" << sqlstring << '\"';
    std::ofstream outputstream;
    if (outputname != 0 && *outputname != 0 &&
        0 != strcmp(outputname, "/dev/null")) {
        // open the file now to clear the existing content, in cases of
        // error, the output file would have been cleared
	bool appendToOutput = false;
        outputstream.open(outputname,
                          std::ios::out | 
                          (appendToOutput ? std::ios::app : std::ios::trunc));
        appendToOutput = true; // all query output go to the same file
    }

    std::unique_ptr<ibis::table> res;
    if (estimation_opt < 0) { // directly evaluate the select clause
	std::unique_ptr<ibis::quaere>
	    qq(ibis::quaere::create(0, fstr, wstr, pl));
	if (qq.get() == 0) {
	    LOGGER(verbose_level >= 0)
		<< "Warning -- doQuaere(" << sqlstring
		<< ") failed to create an ibis::quaere object";
	    return;
	}
	res.reset(qq->select(sstr));
    }
    else {
	std::unique_ptr<ibis::quaere>
	    qq(ibis::quaere::create(sstr, fstr, wstr, pl));
	if (qq.get() == 0) {
	    LOGGER(verbose_level >= 0)
		<< "Warning -- doQuaere(" << sqlstring
		<< ") failed to create an ibis::quaere object";
	    return;
	}

	uint64_t nhits=1, hmax=0;
	qq->roughCount(nhits, hmax);
	if (nhits < hmax) {
	    LOGGER(verbose_level > 0)
		<< "doQuaere -- " << wstr << " --> [" << nhits << ", "
		<< hmax << ']';
	}
	else {
	    LOGGER(verbose_level > 0)
		<< "doQuaere -- " << wstr << " --> " << nhits
		<< " hit" << (hmax>1?"s":"");
	}
	if (estimation_opt > 0) return;

	int64_t cnts = qq->count();
	if (cnts < 0) {
	    LOGGER(verbose_level >= 0)
		<< "Warning -- doQuaere(" << sqlstring
		<< ") failed to produce a count of the number of hits"
		<< ", ierr = " << cnts;
	    return;
	}
	else if (nhits < hmax) {
	    LOGGER(verbose_level >= 0 &&
		   ((uint64_t)cnts < nhits || (uint64_t)cnts > hmax))
		<< "Warning -- doQuaere(" << sqlstring
		<< ") expects the return of count to be between "
		<< nhits << " and " << hmax
		<< ", but the actual return value is " << cnts;
	    nhits = cnts;
	}
	else if ((uint64_t)cnts != nhits) {
	    LOGGER(verbose_level >= 0)
		<< "Warning -- doQuaere(" << sqlstring
		<< ") expects the return of count to be " << nhits
		<< ", but the actual return value is " << cnts;
	    nhits = cnts;
	}

	res.reset(qq->select());
    }
    if (res.get() == 0) {
	LOGGER(verbose_level >= 0)
	    << "Warning -- doQuaere(" << sqlstring
	    << ") failed to produce a result table";
	return;
    }

    if (res->nRows() > 1 && ((ordkeys && *ordkeys) || limit > 0)) {
	// top-K query
	res->orderby(ordkeys);
    }

    timer.stop();
    if (verbose_level >= 0) {
	ibis::util::logger lg;
	lg() << "doQuaere -- \"" << sqlstring << "\" produced a table with "
	     << res->nRows() << " row" << (res->nRows() > 1 ? "s" : "")
	     << " and " << res->nColumns() << " column"
	     << (res->nColumns() > 1 ? "s" : "");
	if (verbose_level > 0)
	    lg () << ", took " << timer.CPUTime() << " CPU seconds, "
		  << timer.realTime() << " elapsed seconds";
    }

    int64_t ierr;
    if (outputname != 0 && 0 == strcmp(outputname, "/dev/null")) {
    }
    else if (res->nRows() == 0 || res->nColumns() == 0) {
        return;
    }
    else if (outputbinary) {
        if (zapping)
            ibis::util::removeDir(outputname);
        ierr = res->backup(outputname);
        LOGGER(ierr < 0 && 0 != outputname && res->name() != 0)
            << "Warning -- doQuaere failed to write the content of "
            << res->name() << " in binary to " << outputname
            << ", ierr = " << ierr;
    }
    else if (outputstream.is_open() && outputstream.good()) {
        if (showheader)
            res->dumpNames(outputstream, ", ");
        if (limit == 0)
            limit = static_cast<uint32_t>(res->nRows());
        ierr = res->dump(outputstream, start, limit, ", ");
        LOGGER(ierr < 0 && 0 != outputname && res->name() != 0)
            << "Warning -- doQuaere failed to write the content of "
            << res->name() << " in CSV to " << outputname
            << ", ierr = " << ierr;
    }
    else if (verbose_level >= 0) {
	ibis::util::logger lg;
	if (limit == 0 && res->nColumns() > 0) {
	    limit = ((res->nRows() >> verbose_level) > 0 ?
		     1 << verbose_level : static_cast<uint32_t>(res->nRows()));
	    if (limit > (res->nRows() >> 1))
		limit = res->nRows();
	}
	if (limit > 0 && limit < res->nRows()) {
	    lg() << "doQuaere -- the first ";
	    if (limit > 1)
		lg() << limit << " rows ";
	    else
		lg() << "row ";
	    lg() << "(of " << res->nRows()
		 << ") from the result table for \""
		 << sqlstring << "\"\n";
	}
	else {
	    lg() << "doQuaere -- the result table (" << res->nRows()
		 << " x " << res->nColumns() << ") for \""
		 << sqlstring << "\"\n";
	}
	if (showheader)
	    res->dumpNames(lg(), ", ");
	res->dump(lg(), start, limit, ", ");
    }

    ibis::table::stringList cn = res->columnNames();
    ibis::table::typeList ct = res->columnTypes();
    if (cn.size() > 1 && ct.size() == cn.size() &&
	(ct[0] == ibis::TEXT || ct[0] == ibis::CATEGORY) &&
	(ct.back() != ibis::TEXT && ct.back() != ibis::CATEGORY)) {
	const char* s = cn[0];
	cn[0] = cn.back();
	cn.back() = s;
	ibis::TYPE_T t = ct[0];
	ct[0] = ct.back();
	ct.back() = t;
    }
    if (verbose_level > 3 && res->nRows() > 1 && !cn.empty() && !ct.empty() &&
	(ct.back() == ibis::BYTE || ct.back() == ibis::UBYTE ||
	 ct.back() == ibis::SHORT || ct.back() == ibis::USHORT ||
	 ct.back() == ibis::INT || ct.back() == ibis::UINT ||
	 ct.back() == ibis::LONG || ct.back() == ibis::ULONG ||
	 ct.back() == ibis::FLOAT || ct.back() == ibis::DOUBLE)) {
	// try a silly query on res
	std::string cnd3, sel1, sel3;
	sel1 = "max(";
	sel1 += cn.back();
	sel1 += ") as mx, min(";
	sel1 += cn.back();
	sel1 += ") as mn";
	if (cn.size() > 1) {
	    sel3 = cn[0];
	    sel3 += ", avg(";
	    sel3 += cn[1];
	    sel3 += ')';
	}
	else {
	    sel3 = "floor(";
	    sel3 += cn[0];
	    sel3 += "/10), avg(";
	    sel3 += cn[0];
	    sel3 += ')';
	}

	std::unique_ptr<ibis::table> res1(res->select(sel1.c_str(), "1=1"));
	if (res1.get() == 0) {
	    LOGGER(verbose_level >= 0)
		<< "Warning -- doQuaere(" << sqlstring
		<< ") failed to find the min and max of " << cn.back()
		<< " in the result table " << res->name();
	    return;
	}
	double minval, maxval;
	ierr = res1->getColumnAsDoubles("mx", &maxval);
	if (ierr != 1) {
	    LOGGER(verbose_level >= 0)
		<< "Warning -- doQuaere(" << sqlstring
		<< ") expects to retrieve exactly one value from res1, "
		"but getColumnAsDoubles returned " << ierr;
	    return;
	}
	ierr = res1->getColumnAsDoubles("mn", &minval);
	if (ierr != 1) {
	    LOGGER(verbose_level >= 0)
		<< "Warning -- doQuaere(" << sqlstring
		<< ") expects to retrieve exactly one value from res1, "
		"but getColumnAsDoubles returned " << ierr;
	    return;
	}

	std::ostringstream oss;
	oss << "log(" << (0.5*(minval+maxval)) << ") <= log("
	    << cn.back() << ')';
	std::unique_ptr<ibis::table>
	    res3(res->select(sel3.c_str(), oss.str().c_str()));
	if (res3.get() == 0) {
	    LOGGER(verbose_level >= 0)
		<< "Warning -- doQuaere(" << sqlstring
		<< ") failed to evaluate query " << oss.str()
		<< " on the table " << res->name();
	    return;
	}

	ibis::util::logger lg;
	res3->describe(lg());
	res3->dump(lg(), ", ");
    }
    else if (verbose_level > 3 && res->nRows() > 1 &&
	     cn.size() > 1 && ct.size() > 1 &&
	     (ct.back() == ibis::CATEGORY ||
	      ct.back() == ibis::TEXT)) {
	// try a silly query on res
	std::string cnd2, sel2;
	if (cn.size() > 1) {
	    sel2 = "floor(";
	    sel2 += cn[0];
	    sel2 += ")/3, min(";
	    sel2 += cn[0];
	    sel2 += "), avg(";
	    sel2 += cn[1];
	    sel2 += ')';
	}
	else {
	    sel2 = "floor(";
	    sel2 += cn[0];
	    sel2 += "/10, avg(";
	    sel2 += cn[0];
	    sel2 += ')';
	}
	{
	    std::unique_ptr<ibis::table::cursor> cur(res->createCursor());
	    if (cur.get() == 0) {
		LOGGER(verbose_level >= 0)
		    << "Warning -- doQuaere(" << sqlstring
		    << ") failed to create a cursor from the result table";
		return;
	    }
	    std::string tmp;
	    for (size_t j = 0; tmp.empty() && j < cur->nRows(); ++ j) {
		ierr = cur->fetch();
		if (ierr != 0) {
		    LOGGER(verbose_level >= 0)
			<< "Warning -- doQuaere(" << sqlstring
			<< ") failed to fetch row " << j
			<< " for the cursor from table " << res->name();
		    return;
		}
		ierr = cur->getColumnAsString(cn.back(), tmp);
		if (ierr != 0) {
		    LOGGER(verbose_level >= 0)
			<< "Warning -- doQuaere(" << sqlstring
			<< ") failed to retrieve row " << j << " of column "
			<< cn.back() << " from the cursor for table "
			<< res->name();
		    return;
		}
	    }
	    if (tmp.empty()) {
		LOGGER(verbose_level > 0)
		    << "doQuaere(" << sqlstring
		    << ") can not find a non-empty string for column "
		    << cn.back() << " from the table " << res->name();
		return;
	    }
	    cnd2 = cn.back();
	    cnd2 += " = \"";
	    cnd2 += tmp;
	    cnd2 += '"';
	}

	std::unique_ptr<ibis::table>
	    res2(res->select(sel2.c_str(), cnd2.c_str()));
	if (res2.get() == 0) {
	    LOGGER(verbose_level >= 0)
		<< "Warning -- doQuaere(" << sqlstring
		<< ") failed to evaluate query " << cnd2
		<< " on the table " << res->name();
	    return;
	}

	ibis::util::logger lg;
	res2->describe(lg());
	res2->dump(lg(), ", ");
    }
} // doQuaere

// evaluate a single query -- print selected columns through ibis::bundle
static void doQuery(ibis::part* tbl, const char* uid, const char* wstr,
		    const char* sstr, const char* ordkeys,
		    uint32_t limit, uint32_t start) {
    std::string sqlstring; //
    {
	std::ostringstream ostr;
	if (sstr != 0 && *sstr != 0)
	    ostr << "SELECT " << sstr;
	ostr << " FROM " << tbl->name();
	if (wstr != 0 && *wstr != 0)
	    ostr << " WHERE " << wstr;
	if (ordkeys && *ordkeys) {
	    ostr << " ORDER BY " << ordkeys;
	}
	if (limit > 0) {
	    ostr << " LIMIT ";
	    if (start > 0)
		ostr << start << ", ";
	    ostr << limit;
	}
	sqlstring = ostr.str();
    }
    LOGGER(verbose_level > 1)
	<< "doQuery -- processing \"" << sqlstring << '\"';

    long num1, num2;
    const char* asstr = 0;
    ibis::horometer timer;
    timer.start();
    std::ofstream outputstream;
    if (outputname != 0 && *outputname != 0 &&
        0 != strcmp(outputname, "/dev/null")) {
        // open the file now to clear the existing content, in cases of
        // error, the output file would have been cleared
	bool appendToOutput = false;
        outputstream.open(outputname,
                          std::ios::out | 
                          (appendToOutput ? std::ios::app : std::ios::trunc));
        appendToOutput = true; // all query output go to the same file
    }
    // the third argument is needed to make sure a private directory is
    // created for the query object to store the results produced by the
    // select clause.
    ibis::query aQuery(uid, tbl,
		       ((sstr != 0 && *sstr != 0 &&
			 ((ordkeys != 0 && *ordkeys != 0) || limit > 0 ||
			  recheckvalues || testing > 0)) ?
			"ibis" : static_cast<const char*>(0)));
    if (ridfile != 0) {
	ibis::ridHandler handle(0); // a sample ridHandler
	ibis::RIDSet rset;
	handle.read(rset, ridfile);
	aQuery.setRIDs(rset);
    }
    if (wstr != 0 && *wstr != 0) {
	num2 = aQuery.setWhereClause(wstr);
	if (num2 < 0) {
	    LOGGER(verbose_level > 3)
		<< "Warning -- doQuery failed to assigned the where clause \""
		<< wstr << "\" on partition " << tbl->name()
		<< ", setWhereClause returned " << num2;
	    return;
	}
    }
    if (sstr != 0 && *sstr != 0) {
	num2 = aQuery.setSelectClause(sstr);
	if (num2 < 0) {
	    LOGGER(verbose_level > 3)
		<< "Warning -- doQuery failed to assign the select clause \""
		<< sstr << "\" on partition " << tbl->name()
		<< ", setSelectClause returned " << num2;
	    return;
	}

	asstr = aQuery.getSelectClause();
    }
    if (aQuery.getWhereClause() == 0 && ridfile == 0
	&& asstr == 0)
	return;
    if (zapping && aQuery.getWhereClause()) {
	std::string old = aQuery.getWhereClause();
	std::string comp = aQuery.removeComplexConditions();
	if (verbose_level > 1) {
	    ibis::util::logger lg;
	    if (! comp.empty())
		lg() << "doQuery -- the WHERE clause \""
		     <<  old.c_str() << "\" is split into \""
		     << comp.c_str()  << "\" AND \""
		     << aQuery.getWhereClause() << "\"";
	    else
		lg() << "doQuery -- the WHERE clause \""
		     << aQuery.getWhereClause()
		     << "\" is considered simple";
	}
    }

    if (sequential_scan) {
	num2 = aQuery.countHits();
	if (num2 < 0) {
	    ibis::bitvector btmp;
	    num2 = aQuery.sequentialScan(btmp);
	    if (num2 < 0) {
		ibis::util::logger lg;
		lg() << "Warning -- doQuery:: sequentialScan("
		     << aQuery.getWhereClause() << ") failed";
		return;
	    }

	    num2 = btmp.cnt();
	}
	if (verbose_level >= 0) {
	    timer.stop();
	    ibis::util::logger lg;
	    lg() << "doQuery:: sequentialScan("
		 << aQuery.getWhereClause() << ") produced "
		 << num2 << " hit" << (num2>1 ? "s" : "");
	    if (verbose_level > 0)
		lg () << ", took " << timer.CPUTime() << " CPU seconds, "
		      << timer.realTime() << " elapsed seconds";
	}
	return;
    }

    if (estimation_opt >= 0) {
	num2 = aQuery.estimate();
	if (num2 < 0) {
	    LOGGER(verbose_level >= 0)
		<< "Warning -- doQuery failed to estimate \"" << wstr
		<< "\", error code = " << num2;
	    return;
	}
	num1 = aQuery.getMinNumHits();
	num2 = aQuery.getMaxNumHits();
	if (verbose_level > 1) {
	    ibis::util::logger lg;
	    lg() << "doQuery -- the number of hits is ";
	    if (num2 > num1)
		lg() << "between " << num1 << " and ";
	    lg() << num2;
	}
	if (estimation_opt > 0 || num2 == 0) {
	    if (verbose_level >= 0) {
		timer.stop();
		ibis::util::logger lg;
		lg() << "doQuery:: estimate("
		     << aQuery.getWhereClause() << ") took "
		     << timer.CPUTime() << " CPU seconds, "
		     << timer.realTime() << " elapsed seconds.";
		if (num1 == num2)
		    lg() << "  The number of hits is " << num1;
		else
		    lg() << "  The number of hits is between "
			 << num1 << " and " << num2;
	    }
	    return; // stop here is only want to estimate
	}
    }

    num2 = aQuery.evaluate();
    if (num2 < 0) {
	LOGGER(verbose_level >= 0)
	    << "Warning -- doQuery failed to evaluate \"" << wstr
	    << "\", error code = " << num2;
	return;
    }
    num1 = aQuery.getNumHits();

    if (asstr != 0 && *asstr != 0 && num1 > 0 && verbose_level >= 0) {
	std::unique_ptr<ibis::bundle> bdl(ibis::bundle::create(aQuery));
	if (bdl.get() == 0) {
	    LOGGER(verbose_level >= 0)
		<< "Warning -- doQuery(" << sqlstring
		<< ") failed to create the bundle object for output operations";
	    return;
	}

	if (ordkeys && *ordkeys) { // top-K query
	    bdl->reorder(ordkeys);
	}
	if (limit > 0 || start > 0) {
	    num2 = bdl->truncate(limit, start);
	    if (num2 < 0) {
		LOGGER(verbose_level >= 0)
		    << "Warning -- doQuery failed to truncate the bundle";
		return;
	    }
	}
	if (0 != outputname && 0 == strcmp(outputname, "/dev/null")) {
	    // no need to actually write anything to the output file
	}
	else if (outputstream.is_open() && outputstream.good()) {
            LOGGER(verbose_level >= 0)
                << "doQuery -- query (" << aQuery.getWhereClause()
                << ") results written to file \""
                <<  outputname << "\"";
            if (verbose_level > 8 || recheckvalues) {
                bdl->printAll(outputstream);
            }
            else {
                const int gvold = verbose_level;
                if (gvold < 4) verbose_level = 4;
                bdl->print(outputstream);
                verbose_level = gvold;
            }
        }
        else {
            ibis::util::logger lg;
            if (0 != outputname) {
                lg() << "Warning ** doQuery failed to open file \""
                     << outputname << "\" for writing query ("
                     << aQuery.getWhereClause() << ")\n";
            }
            if (verbose_level > 8 || recheckvalues) {
                bdl->printAll(lg());
            }
            else {
                bdl->print(lg());
            }
        }
    }
    if (verbose_level >= 0) {
	timer.stop();
	ibis::util::logger lg;
	lg() << "doQuery:: evaluate(" << sqlstring
	     << ") produced " << num1 << (num1 > 1 ? " hits" : " hit");
	if (verbose_level > 0)
	    lg() << ", took " << timer.CPUTime() << " CPU seconds, "
		 << timer.realTime() << " elapsed seconds";
    }

    if (verbose_level > 0 && (sstr == 0 || *sstr == 0) &&
	aQuery.getWhereClause()) {
	ibis::countQuery cq(tbl);
	num2 = cq.setWhereClause(aQuery.getWhereClause());
	if (num2 < 0) {
	    LOGGER(verbose_level > 0)
		<< "Warning -- doQuery failed to set \""
                << aQuery.getWhereClause()
		<< "\" on a countQuery";
	}
	else {
	    num2 = cq.evaluate();
	    if (num2 < 0) {
		LOGGER(verbose_level > 0)
		    << "Warning -- doQuery failed to count the where clause "
		    << aQuery.getWhereClause();
	    }
	    else if (cq.getNumHits() != num1) {
		LOGGER(verbose_level > 0)
		    << "Warning -- countQuery.getNumHits returned "
		    << cq.getNumHits() << ", while query.getNumHits returned "
		    << num1;
	    }
	}
    }
    if (verbose_level > 5 || (recheckvalues && verbose_level >= 0)) {
	ibis::bitvector btmp;
	num2 = aQuery.sequentialScan(btmp);
	if (num2 < 0) {
	    ibis::util::logger lg;
	    lg() << "Warning -- doQuery:: sequentialScan("
		 << aQuery.getWhereClause() << ") failed";
	}
	else {
	    num2 = btmp.cnt();
	    if (num1 != num2 && verbose_level >= 0) {
		ibis::util::logger lg;
		lg() << "Warning ** query \"" << aQuery.getWhereClause()
		     << "\" generated " << num1
		     << " hit" << (num1 >1  ? "s" : "")
		     << " with evaluate(), but generated "
		     << num2 << " hit" << (num2 >1  ? "s" : "")
		     << " with sequentialScan";
	    }
	}
    }

    if (verbose_level >= 0 && (recheckvalues || testing > 1)) {
	// retrieve RIDs as bundles
	uint32_t nbdl = 0;
	ibis::RIDSet* rid0 = new ibis::RIDSet;
	const ibis::RIDSet *tmp = aQuery.getRIDsInBundle(0);
	while (tmp != 0) {
	    rid0->insert(rid0->end(), tmp->begin(), tmp->end());
	    delete tmp;
	    ++ nbdl;
	    tmp = aQuery.getRIDsInBundle(nbdl);
	}
	if (rid0->size() == 0) {
	    delete rid0;
	    return;
	}
	ibis::util::sortRIDs(*rid0);

	// retrieve the RIDs in one shot
	ibis::RIDSet* rid1 = aQuery.getRIDs();
	if (rid1 == 0) {
	    delete rid0;
	    return;
	}

	ibis::util::sortRIDs(*rid1);
	if (rid1->size() == rid0->size()) {
	    uint32_t i, cnt=0;
	    ibis::util::logger lg;
	    for (i=0; i<rid1->size(); ++i) {
		if ((*rid1)[i].value != (*rid0)[i].value) {
		    ++cnt;
		    lg() << i << "th RID (" << (*rid1)[i]
			 << ") != (" << (*rid0)[i] << ")\n";
		}
	    }
	    if (cnt > 0)
		lg() << "Warning -- " << cnt
		     << " mismatches out of a total of "
		     << rid1->size();
	    else
		lg() << "Successfully verified " << rid0->size()
		     << " hit" << (rid0->size()>1?"s":"");
	}
	else if (sstr != 0) {
	    ibis::util::logger lg;
	    lg() << "sent " << rid1->size() << " RIDs, got back "
		 << rid0->size();
	    uint32_t i=0, cnt;
	    cnt = (rid1->size() < rid0->size()) ? rid1->size() :
		rid0->size();
	    while (i < cnt) {
		lg() << "\n(" << (*rid1)[i] << ") >>> (" << (*rid0)[i];
		++i;
	    }
	    if (rid1->size() < rid0->size()) {
		while (i < rid0->size()) {
		    lg() << "\n??? >>> (" << (*rid0)[i] << ")";
		    ++i;
		}
	    }
	    else {
		while (i < rid1->size()) {
		    lg() << "\n(" << (*rid1)[i] << ") >>> ???";
		    ++i;
		}
	    }
	}
	delete rid0;

	if (rid1->size() > 1024) {
	    // select no more than 1024 RIDs -- RID2Hits is slow
	    uint32_t len = 512 + (511 & rid1->size());
	    if (len == 0) len = 1024;
	    rid1->resize(len);
	}

	ibis::RIDSet* rid2 = new ibis::RIDSet;
	rid2->deepCopy(*rid1);
	delete rid1; // setRIDs removes the underlying file for rid1
	aQuery.setRIDs(*rid2);
	rid1 = rid2; // setRIDs has copied rid2
	aQuery.evaluate();
	rid2 = aQuery.getRIDs();
	if (rid2 == 0) { // make sure the pointer is valid
	    rid2 = new ibis::RIDSet;
	}
	ibis::util::sortRIDs(*rid2);
	if (rid1->size() == rid2->size()) {
	    uint32_t i, cnt=0;
	    ibis::util::logger lg;
	    for (i=0; i<rid1->size(); ++i) {
		if ((*rid1)[i].value != (*rid2)[i].value) {
		    ++cnt;
		    lg() << i << "th RID (" << (*rid1)[i]
			 << ") != (" << (*rid2)[i] << ")\n";
		}
	    }
	    if (cnt > 0)
		lg() << "Warning -- " << cnt
		     << " mismatches out of a total of "
		     << rid1->size();
	    else
		lg() << "Successfully verified " << rid1->size()
		     << " hit" << (rid1->size()>1?"s":"");
	}
	else {
	    ibis::util::logger lg;
	    lg() << "sent " << rid1->size() << " RIDs, got back "
		 << rid2->size();
	    uint32_t i=0, cnt;
	    cnt = (rid1->size() < rid2->size()) ? rid1->size() :
		rid2->size();
	    while (i < cnt) {
		lg() << "\n(" << (*rid1)[i] << ") >>> (" << (*rid2)[i]
		     << ")";
		++i;
	    }
	    if (rid1->size() < rid2->size()) {
		while (i < rid2->size()) {
		    lg() << "\n??? >>> (" << (*rid2)[i] << ")";
		    ++i;
		}
	    }
	    else {
		while (i < rid1->size()) {
		    lg() << "\n(" << (*rid1)[i] << ") >>> ???";
		    ++i;
		}
	    }
	}
	delete rid1;
	delete rid2;
    }
} // doQuery

// evaluate a single query -- only work on partitions that have defined
// column shapes, i.e., they contain data computed on meshes.
static void doMeshQuery(ibis::part* tbl, const char* uid, const char* wstr,
			const char* sstr) {
    const std::vector<uint32_t>& dim = tbl->getMeshShape();
    if (dim.empty()) {
	doQuery(tbl, uid, wstr, sstr, 0, 0, 0);
	return;
    }

    LOGGER(verbose_level > 0)
	<< "doMeshQuery -- processing query " << wstr
	<< " on partition " << tbl->name();
    std::ofstream outputstream;
    if (outputname != 0 && *outputname != 0 &&
        0 != strcmp(outputname, "/dev/null")) {
        // open the file now to clear the existing content, in cases of
        // error, the output file would have been cleared
	bool appendToOutput = false;
        outputstream.open(outputname,
                          std::ios::out | 
                          (appendToOutput ? std::ios::app : std::ios::trunc));
        appendToOutput = true; // all query output go to the same file
    }

    long num1, num2;
    ibis::horometer timer;
    timer.start();
    ibis::meshQuery aQuery(uid, tbl);
    aQuery.setWhereClause(wstr);
    if (aQuery.getWhereClause() == 0)
	return;
    if (zapping && aQuery.getWhereClause()) {
	std::string old = aQuery.getWhereClause();
	std::string comp = aQuery.removeComplexConditions();
	if (verbose_level > 1) {
	    ibis::util::logger lg;
	    if (! comp.empty())
		lg() << "doMeshQuery -- the WHERE clause \""
		     << old.c_str() << "\" is split into \""
		     << comp.c_str() << "\" AND \""
		     << aQuery.getWhereClause() << "\"";
	    else
		lg() << "doMeshQuery -- the WHERE clause \""
		     << aQuery.getWhereClause()
		     << "\" is considered simple";
	}
    }

    const char* asstr = 0;
    if (sstr != 0 && *sstr != 0) {
	aQuery.setSelectClause(sstr);
	asstr = aQuery.getSelectClause();
    }
    if (estimation_opt >= 0) {
	num2 = aQuery.estimate();
	if (num2 < 0) {
	    LOGGER(verbose_level >= 0)
		<< "Warning -- doMeshQuery failed to estimate \"" << wstr
		<< "\", error code = " << num2;
	    return;
	}
	num1 = aQuery.getMinNumHits();
	num2 = aQuery.getMaxNumHits();
	if (verbose_level > 0) {
	    ibis::util::logger lg;
	    lg() << "doMeshQuery -- the number of hits is ";
	    if (num1 < num2)
		lg() << "between " << num1 << " and ";
	    lg() << num2;
	}
	if (estimation_opt > 0 || num2 == 0) {
	    if (verbose_level > 0) {
		timer.stop();
		ibis::util::logger lg;
		lg() << "doMeshQuery:: estimate("
		     << aQuery.getWhereClause() << ") took "
		     << timer.CPUTime() << " CPU seconds, "
		     << timer.realTime() << " elapsed seconds";
	    }
	    return; // stop here is only want to estimate
	}
    }

    num2 = aQuery.evaluate();
    if (num2 < 0) {
	LOGGER(verbose_level >= 0)
	    << "Warning -- doMeshQuery -- failed to evaluate \"" << wstr
	    << "\", error code = " << num2;
	return;
    }
    num1 = aQuery.getNumHits();
    if (verbose_level >= 0) {
	timer.stop();
	ibis::util::logger lg;
	lg() << "doMeshQuery:: evaluate(" << aQuery.getWhereClause() 
	     << ") produced " << num1 << (num1 > 1 ? " hits" : " hit");
	if (verbose_level > 0)
	    lg() << ", took " << timer.CPUTime() << " CPU seconds, "
		 << timer.realTime() << " elapsed seconds";
    }

    std::vector<uint32_t> lines;
    num2 = ibis::meshQuery::bitvectorToCoordinates
	(*aQuery.getHitVector(), tbl->getMeshShape(), lines);
    LOGGER(verbose_level > 0 && num2 != num1)
	<< "Warning -- meshQuery::bitvectorToCoordinates returned " << num2
	<< ", expected " << num1;

    num2 = aQuery.getHitsAsLines(lines);
    if (num2 < 0) {
	LOGGER(verbose_level > 0)
	    << "Warning -- aQuery.getHitsAsLines returned " << num2;
	return;
    }
    else if (lines.empty()) {
	LOGGER(verbose_level > 1)
	    << "Warning -- aQuery.getHitsAsLines returned no lines";
	return;
    }
    if (verbose_level > 0) {
	ibis::util::logger lg;
	lg() << "doMeshQuery:: turned " << num1 << " hit" << (num1>1?"s":"")
	     << " into " << num2 << " query lines on a " << dim[0];
	for (unsigned j = 1; j < dim.size(); ++ j)
	    lg() << " x " << dim[j];
	lg() << " mesh";
    }
    std::vector<uint32_t> label1;
    num2 = aQuery.labelLines(dim.size(), lines, label1);
    if (num2 < 0) {
	LOGGER(verbose_level > 0)
	    << "Warning -- aQuery.labelLines failed with error code " << num2;
	return;
    }
    LOGGER(verbose_level > 0)
	<< "doMeshQuery: identified " << num2 << " connected component"
	<< (num2>1?"s":"") << " among the query lines";

    if (verbose_level >= 0 || testing > 0) {
	std::vector< std::vector<uint32_t> > blocks;
	std::vector<uint32_t> label2;
	num2 = aQuery.getHitsAsBlocks(blocks);
	if (num2 < 0) {
	    LOGGER(verbose_level > 0)
		<< "Warning -- aQuery.getHitsAsBlocks returned " << num2;
	    return;
	}
	else if (blocks.empty()) {
	    LOGGER(verbose_level > 1)
		<< "Warning -- aQuery.getHitsAsBlocks returned no blocks";
	    return;
	}

	num2 = aQuery.labelBlocks(blocks, label2);
	if (num2 < 0) {
	    LOGGER(num2 < 0)
		<< "Warning -- aQuery.labelBlocks failed with error code "
		<< num2;
	    return;
	}
	LOGGER(verbose_level > 0)
	    << "doMeshQuery:: converted " << num1 << " hit" << (num1>1?"s":"")
	    << " into " << blocks.size() << " block" << (blocks.size()>1?"s":"")
	    << " and identified " << num2 << " connected component"
	    << (num2>1?"s":"") << " among the blocks";

	/// compare the output from labeling lines against those from labeling
	/// the blocks
	const unsigned ndim = dim.size();
	const unsigned ndm1 = dim.size() - 1;
	const unsigned ndp1 = dim.size() + 1;
	size_t jb = 0;	// pointing to a block
	size_t jl = 0;	// pointing to a line
	num1 = 0;	// number of mismatches
	ibis::util::logger lg;
	lg() << "\ndoMeshQuery -- Compare the two sets of labels";
	while (jb < blocks.size() || jl < lines.size()) {
	    if (jb < blocks.size()) { // has a valid block
		if (jl < lines.size()) { // has a valid line
		    int cmp = (lines[jl] < blocks[jb][0] ? -1 :
			       lines[jl] >= blocks[jb][1] ? 1 : 0);
		    for (unsigned j3 = 1; cmp == 0 && j3 < ndm1; ++ j3)
			cmp = (lines[jl+j3] < blocks[jb][j3+j3] ? -1 :
			       lines[jl+j3] >= blocks[jb][j3+j3+1] ? 1 : 0);
		    if (cmp == 0)
			cmp = (lines[jl+ndim] <= blocks[jb][ndm1+ndm1] ? -1 :
			       lines[jl+ndm1] >= blocks[jb][ndm1+ndm1+1] ? 1 :
			       0);
		    if (cmp > 0) { // extra block?
			lg() << "\nblock[" << jb << "] (" << blocks[jb][0]
			     << ", " << blocks[jb][1];
			for (unsigned j3 = 2; j3 < ndim+ndim; ++ j3)
			    lg() << ", " << blocks[jb][j3];
			lg() << "\tline[??]( )";
			++ jb;
			++ num1;
		    }
		    else if (cmp < 0) { // extra line?
			lg() << "\nblock[??]( )\tline[" << jl/ndp1 << "] ("
			     << lines[jl];
			for (unsigned j4 = jl+1; j4 < jl+ndp1; ++ j4)
			    lg() << ", " << lines[j4];
			lg() << ")";
			jl += ndp1;
			++ num1;
		    }
		    else { // matching block and line
			size_t j3;
			unsigned linecount = 0;
			unsigned labelcount = 0;
			unsigned expectedcount = blocks[jb][1] - blocks[jb][0];
			for (unsigned jj = 2; jj+3 < blocks[jb].size(); jj += 2)
			    expectedcount *=
				(blocks[jb][jj+1] - blocks[jb][jj]);
			linecount = (blocks[jb][ndm1+ndm1] == lines[jl+ndm1] &&
				     blocks[jb][ndm1+ndim] == lines[jl+ndim]);
			labelcount = (label2[jb] == label1[jl/ndp1]);
			for (j3 = jl+ndp1; j3 < lines.size(); j3 += ndp1) {
			    bool match =
				(blocks[jb][ndm1+ndm1] == lines[j3+ndm1] &&
				 blocks[jb][ndm1+ndim] == lines[j3+ndim]);
			    for (unsigned jj = 0;
				 match && jj < ndm1;
				 ++ jj) {
				match = (blocks[jb][jj+jj] <= lines[j3+jj] &&
					 blocks[jb][jj+jj+1] > lines[j3+jj]);
			    }
			    if (match) {
				labelcount += (label2[jb] == label1[j3/ndp1]);
				++ linecount;
			    }
			    else {
				break;
			    }
			}
			if (linecount != expectedcount ||
			    labelcount != expectedcount || verbose_level > 6) {
			    lg() << "\nblock[" << jb << "] (" << blocks[jb][0]
				 << ", " << blocks[jb][1];
			    for (unsigned j3 = 2; j3 < ndim+ndim; ++ j3)
				lg() << ", " << blocks[jb][j3];
			    lg() << ")\tline[" << jl << "] (" << lines[jl];
			    for (unsigned j4 = jl+1; j4 < jl+ndp1; ++ j4)
				lg() << ", " << lines[j4];
			    lg() << "),\tlabelb = " << label2[jb]
				 << "\tlabell = " << label1[jl/ndp1];
			    if (expectedcount > 1)
				lg() << "\t... expected " << expectedcount
				     << " lines, found " << linecount
				     << " matching line" << (linecount>1?"s":"")
				     << " with " << labelcount
				     << " correct label"
				     << (labelcount>1?"s":"");
			    if (linecount != expectedcount ||
				labelcount != expectedcount) lg() << " ??";
			}
			num1 += (linecount != expectedcount ||
				 labelcount != expectedcount);
			jl = j3;
			++ jb;
		    }
		}
		else { // no more lines
		    lg() << "\nblock[" << jb << "] (" << blocks[jb][0]
			 << ", " << blocks[jb][1];
		    for (unsigned j3 = 2; j3 < ndim+ndim; ++ j3)
			lg() << ", " << blocks[jb][j3];
		    lg() << ")\tline[??]( )";
		    ++ jb;
		    ++ num1;
		}
	    }
	    else { // no more blocks
		lg() << "\nblock[??]( )\tline[" << jl << "] ("
		     << lines[jl];
		for (unsigned j4 = jl+1; j4 < jl+ndp1; ++ j4)
		    lg() << ", " << lines[j4];
		lg() << ")";
		jl += ndp1;
		++ num1;
	    }
	}
	lg() << "\n" << (num1>0?"Warning (!__!) --":"(^o^)") << " found "
	     << num1 << " mismatch" << (num1>1 ? "es" : "") << "\n";
    }

    if (asstr != 0 && *asstr != 0 && verbose_level > 0 &&
        (outputname == 0 || 0 != strcmp(outputname, "/dev/null"))) {
        if (outputstream.is_open() && outputstream.good()) {
            LOGGER(verbose_level > 0)
                << "doMeshQuery -- query (" << aQuery.getWhereClause()
                << ") results written to file \""
                << outputname << "\"";
            if (verbose_level > 8 || recheckvalues)
                aQuery.printSelectedWithRID(outputstream);
            else
                aQuery.printSelected(outputstream);
        }
        else {
            ibis::util::logger lg;
            if (outputname != 0) {
                lg() << "Warning -- doMeshQuery failed to "
                     << "open file \"" << outputname
                     << "\" for writing query ("
                     << aQuery.getWhereClause() << ") output\n";
            }
            if (verbose_level > 8 || recheckvalues)
                aQuery.printSelectedWithRID(lg());
            else
                aQuery.printSelected(lg());
        }
    } // if (asstr != 0 && num1>0 && verbose_level > 0)
} // doMeshQuery

// evaluate a single query -- directly retrieve values of selected columns
static void xdoQuery(ibis::part* tbl, const char* uid, const char* wstr,
		     const char* sstr) {
    LOGGER(verbose_level > 0)
	<< "xdoQuery -- processing query " << wstr
	<< " on partition " << tbl->name();
    std::ofstream outputstream;
    if (outputname != 0 && *outputname != 0 &&
        0 != strcmp(outputname, "/dev/null")) {
        // open the file now to clear the existing content, in cases of
        // error, the output file would have been cleared
	bool appendToOutput = false;
        outputstream.open(outputname,
                          std::ios::out | 
                          (appendToOutput ? std::ios::app : std::ios::trunc));
        appendToOutput = true; // all query output go to the same file
    }

    ibis::query aQuery(uid, tbl); // in case of exception, content of query
				  // will be automatically freed
    long num1, num2;
    aQuery.setWhereClause(wstr);
    if (aQuery.getWhereClause() == 0)
	return;
    if (zapping) {
	std::string old = aQuery.getWhereClause();
	std::string comp = aQuery.removeComplexConditions();
	if (verbose_level > 1) {
	    ibis::util::logger lg;
	    if (! comp.empty())
		lg() << "xdoQuery -- the WHERE clause \"" << old.c_str()
		     << "\" is split into \"" << comp.c_str()
		     << "\" AND \"" << aQuery.getWhereClause() << "\"";
	    else
		lg() << "xdoQuery -- the WHERE clause \""
		     << aQuery.getWhereClause()
		     << "\" is considered simple";
	}
    }
    const char* asstr = 0;
    if (sstr != 0) {
	aQuery.setSelectClause(sstr);
	asstr = aQuery.getSelectClause();
    }

    if (estimation_opt >= 0) {
	num2 = aQuery.estimate();
	if (num2 < 0) {
	    LOGGER(verbose_level >= 0)
		<< "Warning -- xdoQuery failed to estimate \"" << wstr
		<< "\", error code = " << num2;
	    return;
	}
	num1 = aQuery.getMinNumHits();
	num2 = aQuery.getMaxNumHits();
	if (verbose_level > 0) {
	    ibis::util::logger lg;
	    lg() << "xdoQuery -- the number of hits is ";
	    if (num2 > num1) 
		lg() << "between " << num1 << " and ";
	    lg() << num2;
	}
	if (estimation_opt > 0)
	    return;
    }

    num2 = aQuery.evaluate();
    if (num2 < 0) {
	LOGGER(verbose_level >= 0)
	    << "Warning -- xdoQuery failed to evaluate \"" << wstr
	    << "\", error code = " << num2;
	return;
    }
    num1 = aQuery.getNumHits();
    LOGGER(verbose_level > 0) << "xdoQuery -- the number of hits = " << num1;

    if (asstr != 0 && *asstr != 0 && num1 > 0 &&
        (outputname == 0 || 0 != strcmp(outputname, "/dev/null"))) {
        if (outputstream.is_open() && outputstream.good()) {
            LOGGER(verbose_level >= 0)
                << "xdoQuery -- query (" <<  aQuery.getWhereClause()
                << ") results written to file \"" <<  outputname << "\"";
            printQueryResults(outputstream, aQuery);
        }
        else {
            ibis::util::logger lg;
            if (outputname != 0)
            lg() << "Warning ** xdoQuery failed to open \""
                 << outputname << "\" for writing query ("
                 << aQuery.getWhereClause() << ")";
            printQueryResults(lg(), aQuery);
        }
    } // if (asstr != 0 && num1 > 0)
} // xdoQuery

/*
static void reverseDeletion() {
    if (keepstring == 0 || *keepstring == 0) return;

    if (ibis::util::getFileSize(keepstring) > 0) {
	// assume the file contain a list of numbers that are row numbers
	std::vector<uint32_t> rows;
	readInts(keepstring, rows);
	if (rows.empty()) {
	    LOGGER(ibis::gVerbose >= 0)
		<< "reverseDeletion -- file \"" << keepstring
		<< "\" does not start with integers, integer expected";
	    return;
	}
	LOGGER(ibis::gVerbose > 0)
	    << "reverseDeletion will invoke deactive on "
	    << ibis::datasets.size()
	    << " data partition" << (ibis::datasets.size() > 1 ? "s" : "")
	    << " with " << rows.size() << " row number"
	    << (rows.size() > 1 ? "s" : "");

	for (ibis::partList::iterator it = ibis::datasets.begin();
	     it != ibis::datasets.end(); ++ it) {
	    long ierr = (*it)->reactivate(rows);
	    LOGGER(ibis::gVerbose >= 0)
		<< "reverseDeletion -- reactivate(" << (*it)->name()
		<< ") returned " << ierr;
	}
    }
    else {
	LOGGER(ibis::gVerbose > 0)
	    << "reverseDeletion will invoke deactive on "
	    << ibis::datasets.size()
	    << " data partition" << (ibis::datasets.size() > 1 ? "s" : "")
	    << " with \"" << keepstring << "\"";

	for (ibis::partList::iterator it = ibis::datasets.begin();
	     it != ibis::datasets.end(); ++ it) {
	    long ierr = (*it)->reactivate(keepstring);
	    LOGGER(ibis::gVerbose >= 0)
		<< "reverseDeletion -- reactivate(" << (*it)->name()
		<< ", " << keepstring << ") returned " << ierr;
	}
    }
} // reverseDeletion
*/


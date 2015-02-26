#include <mysql.h>
#include <ibis.h>
#include <mensa.h>	// ibis::mensa
#include <twister.h>

#include <sstream>	// std::ostringstream
#include <algorithm>	// std::sort
#include <memory>	// std::unique_ptr
#include <iomanip>	// std::setprecision


#ifndef MAXSTR
#define MAXSTR 766
#endif

int file_exists (const char * file_name);

static long long processSelect(const char* uid, const char* qstr,
			ibis::partList& prts, std::string out_file,std::ostringstream &datatypes); 
static long long tableSelect(const ibis::partList &pl, const char* uid,
			const char* wstr, const char* sstr,
			const char* ordkeys,
			uint32_t limit, uint32_t start,std::string out_file,std::ostringstream &datatypes); 
static void doQuaere(const ibis::partList& pl,
		     const char *sstr, const char *fstr, const char *wstr,
		     const char *ordkeys, uint32_t limit, uint32_t start);

static void doQuery(ibis::part* tbl, const char* uid, const char* wstr,
		    const char* sstr, const char* ordkeys,
		    uint32_t limit, uint32_t start); 

static void doMeshQuery(ibis::part* tbl, const char* uid, const char* wstr,
			const char* sstr); 

static void xdoQuery(ibis::part* tbl, const char* uid, const char* wstr,
		     const char* sstr); 
static void printQueryResults(std::ostream &out, ibis::query &q); 

/* these were variables in ibis but they can be constants here */
#define testing 0
#define showheader 0
#define recheckvalues 0
#define sequential_scan 0 
#define estimation_opt 0
#define zapping 0
#define outputbinary 0

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
	my_bool fb_insert2_init(UDF_INIT *initid, UDF_ARGS *args, char* message);
	long long fb_insert2(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);
	void fb_insert2_deinit(UDF_INIT *initid);
	my_bool fb_delete_init(UDF_INIT *initid, UDF_ARGS *args, char* message);
	long long fb_delete(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);
	void fb_delete_deinit(UDF_INIT *initid);
	my_bool fb_unlink_init(UDF_INIT *initid, UDF_ARGS *args, char* message);
	long long fb_unlink(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);
	void fb_unlink_deinit(UDF_INIT *initid);
	my_bool fb_resort_init(UDF_INIT *initid, UDF_ARGS *args, char* message);
	long long fb_resort(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);
	void fb_resort_deinit(UDF_INIT *initid);
	my_bool fb_debug_init(UDF_INIT *initid, UDF_ARGS *args, char* message);
	long long fb_debug(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);
	void fb_debug_deinit(UDF_INIT *initid);
}

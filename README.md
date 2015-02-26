# FastBit_UDF
MySQL UDF for creating, manipulating and querying FastBit indexes

##About these UDF Functions
FastBit is a data store which implements WAH (word aligned hybrid) bitmap indexes.  These UDF create, modify and query FastBit tables.  The UDF treats a single directory on the filesystem as one FastBit table.  Inside of the FastBit table/directory are directories representing partitions.  The partitions are created automatically when data is loaded.
**All functions take as the first argument the table path/directory **.

**All columns of a fastbit table are automatically bitmapped indexed**
FastBit WAH bitmap indexes are optimal for multi-dimensional range scans, unlike b-tree indexes which are optimal only for one-dimensional queries.  This means that FastBit can very efficiently handle queries that MySQL can not, like ```select c1 from table where c2 between 1 and 20 or c3 between 1 and 90 or c4 in (1,2,3)```

**Note**: that these UDF use default binning and encoding for the bitmap index.  You can use the ibis tool from fastbit (look in the bin/ directory after building the UDF) to change the binning/encoding for an index.

**Note**: you should not use the ibis tool to modify data while using the table at the same time from inside of MySQL.

MySQL can not answer that query using a b-tree index and will resort to a full table scan.

###Fastbit data directory 
* OK: /path/to/writable/directory 
* NOT OK: ., .., "", any path not beginning with /

It is a good practice to put all FastBit tables under one directory, for example:
```
sudo mkdir /var/lib/fastbit
sudo chown -R mysql:mysql /var/lib/fastbit
mysql -uroot -e "select fb_create('/var/lib/fastbit/table','c1:int')"
+----------------------------------------------+
| fb_create('/var/lib/fastbit/table','c1:int') |
+----------------------------------------------+
|                                            0 |
+----------------------------------------------+
```

###For more information about FastBit, see the FastBit site which includes information about using the Fastbit library
* https://sdm.lbl.gov/fastbit/

#Stored Routines / Functions
There are two routines that make working with FastBit tables easier.  There is a stored procedure that stores results in a table and optionally returns the result set, and there is a function that creaes an IN list from a FastBit resultset, for constructing lookup queries from FastBit results.

## fb_helper
### Run a query, store the results in a table.  Optionally return the results and drop the table.

This function runs fastbit queries and stores the results in a table, optionally returning a resultset and optionally dropping the table (only useful if you return a resultset).

* usage: fb_helper(table_path varchar, into_schema varchar, into_table varchar, query text, return_result bool, drop_result bool)
* returns: when result_result=1, a resultset is produced, otherwise nothing is returned

**Note:** This function automatically creates and removes intermediate files in @@tmpdir (usually /tmp)

```
mysql> call fb_helper(
@index := '/tmp/fbtest', 
@schema:='test',
@table:='temp',
@query := 'select count(*)',
@return_result:=1,
@drop_temp_table:=1
);
+----------+
| count(*) |
+----------+
|        2 |
+----------+
1 row in set (0.01 sec)

mysql> select * from test.temp; /* Should be gone because drop_temp_table=1 in the previous call*/
ERROR 1146 (42S02): Table 'test.temp' doesn't exist
```

## fb_inlist
### Run a query and return the projected column as an IN list 
mysql> set @v_sql := CONCAT('select * from fastbit.fbdata where c1 IN ', 
->                          fastbit.fb_inlist('/tmp/fbtest','select c3 where c2>0')
->);
Query OK, 0 rows affected (0.00 sec)

mysql> select @v_sql;
+-------------------------------------------------+
| @v_sql                                          |
+-------------------------------------------------+
| select * from fastbit.fbdata where c1 IN (1,20) |
+-------------------------------------------------+
1 row in set (0.00 sec)

mysql> PREPARE stmt FROM @v_sql;
Query OK, 0 rows affected (0.00 sec)
Statement prepared

mysql> EXECUTE stmt;
+----+------+------+--------------+--------------------------------------------------------------------------------------------------+
| c1 | c2   | c3   | v1           | extra_data                                                                                       |
+----+------+------+--------------+--------------------------------------------------------------------------------------------------+
|  1 |    6 |   50 | customer#465 | baaedb329a3a8dfc79695cf41d67096be23c67febd84c9746af931327abbfd88a32654997615d338cb377bdae720ed36 |
| 20 |    7 |   24 | customer#845 | c9aeb0447c54facc96816315cdc7dbdc9c5f8d635e7ca94ac6bedb690184997449cef92fe3799f14d8ed3af92eaac566 |
+----+------+------+--------------+--------------------------------------------------------------------------------------------------+
2 rows in set (0.00 sec)

mysql> DEALLOCATE PREPARE stmt;
Query OK, 0 rows affected (0.00 sec)
```

#UDF Functions
These are the lower level functions that allow you to create fastbit tables, insert and delete rows and run queries.
## fb_create
### Create an table/index

This function creates a FastBit table on disk.  Each table should be stored in it's own directory.

* usage: fb_create(table_path, colspec)
* returns: 0 on success, negative on failure

The **colspec** argument is a comma separated string of column_name:datatype

#### Data types available
* byte: 8-bit signed integer.
* short: 16-bit signed integer.
* int: 32-bit signed integer.
* long: 64-bit signed integer.
* unsigned byte: unsigned 8-bit signed integer.
* unsigned short: unsigned 16-bit signed integer.
* unsigned int: unsigned 32-bit signed integer.
* unsigned long: unsigned 64-bit signed integer.
* float: 32-bit IEEE floating-point values.
* double: 64-bit IEEE floating-point values.
* key: String values with a small number of distinct choices.
* text: Arbitrary string values.

#### Create a table with two integer columns called c2 and c3
```
+------------------------------------------+
| fb_create("/tmp/fbtest","c2:int,c3:int") |
+------------------------------------------+
|                                        0 |
+------------------------------------------+
Query OK, 0 rows affected (0.02 sec)
```

##fb_load
### Loading a CSV file into the table/index

This function uses the FastBit library to load a CSV file.  You can specific a delimter other than comma.

* usage: fb_load(table_path varchar, file_name varchar, [delimiter=,] varchar, [max_per_partition=100000000] int)
* returns: number of rows loaded on success, negative on failure

note: an entire column of a partition is loaded into memory.  100M rows the the default maximum number of rows in a partition (remember partitions are automatically maintained) and the default is probably reasonable for most systems.  If FastBit consumes too much memory it will crash, taking MySQL with it, so don't make the per-partition value too large if you have many rows.
```
+------------------------------------------+
| fb_load("/tmp/fbtest","/tmp/fbdata.txt") |
+------------------------------------------+
|                                 10000000 |
+------------------------------------------+
Query OK, 0 rows affected (4.02 sec)
```

## fb_query
### Query the table 

* usage: fb_query(table_path varchar,output_file varchar,query_string varchar)
* returns: QUERY_OK row_count or ERROR error_message

Query the table and store the results in a CSV file

**Note**: that a list of column names and data types is returned so you can create 
a table to store the results in (or use fb_helper())
```
+-------------------------------------------------------------------------------------+
| fb_query("/tmp/fbtest","/tmp/out.txt","select c2, count(*) as cnt, avg(c3) as avg") |
+-------------------------------------------------------------------------------------+
| QUERY_OK 1 (`c2` INT, `cnt` INT UNSIGNED, `avg` DOUBLE)                             |
+-------------------------------------------------------------------------------------+
Query OK, 0 rows affected (0.00 sec)
```

### The results are stored in the output file
**cat /tmp/out.txt**
```
1, 1, 1 
```

**Note**: Use fb_helper to automate usign this function.

## fb_unlink
### Unlink files (be careful!)

* usage: fb_unlink(file_path)
* returns: 0 on success, negative on error

The FastBit UDF won't overwrite files, so you have to unlink files you create 
You also don't want to waste disk space on a bunch of intermediate files you don't need

**Note**: For safety purposes this tool will only remove files that end in **.fcsv** 
by default.  If you want to be able to unlink any files from the filesystem,
change 
#define SAFE_UNLINK 1
to
#define SAFE_UNLINK 0
in fb_udf.h

```
+---------------------------+
| fb_unlink("/tmp/out.txt") |
+---------------------------+
|                         0 |
+---------------------------+
Query OK, 0 rows affected (0.00 sec)
```

## fb_delete
### Delete rows

Deletes rows (and zaps them) from the table

* usage: fb_delete(table_path, where_conditions)
* returns: Number of rows deleted on success, negative on failure

```
+-------------------------------------+
| fb_delete("/tmp/fbtest", "c2 >= 0") |
+-------------------------------------+
|                            10000000 |
+-------------------------------------+
Query OK, 0 rows affected (0.4 sec)
```

## fb_insert
### Insert a row

Inserts a delimited row into the database.  Delimter may consist of more than one character, and each is used as a delimiter.  Multi-character delimiters are not supported.  The default delimter is comma (,). 

**Note**: that strings don't have to be quoted by it doesn't hurt to quote them with single quotes.

* usage: fb_insert(table_path, delimted_string)
* returns: 1 on success, negative on failure

```
+-------------------------------------+
| fb_insert('/tmp/fbtest', '1|1','|') |
+-------------------------------------+
|                                   1 |
+-------------------------------------+
Query OK, 0 rows affected (0.0 sec)
```

## fb_insert2
### Bulk insert (no intermediary file) essentially insert .. select

This function does not take a delimited input string, but instead, each argument (except the first, which is the table location) specifies a column value.  

* usage: fb_insert2(table_path, col1, ..., colN)
* returns: The function returns 1 on success for each row inserted. On error it returns NULL, or in cases of initialization failure) a negative number.

In testing, the  insert functions are 20% faster than exporting data to a flat file, and then importing it with fb_load.  The speedup is mostly due to reduced IO as no intermediary file must be written.  If you already have your data in a text file, use fb_load for best results.

```
mysql> select fb_insert2('/tmp/fbtest3', c2, c3), count(*) from fbdata group by 1;
+------------------------------------+----------+
| fb_insert2('/tmp/fbtest3', c2, c3) | count(*) |
+------------------------------------+----------+
|                                  1 | 10000000 |
+------------------------------------+----------+
Query OK, 0 rows affected (23.01 sec)
```

## fb_resort
### Re-sort a table for better compression (sort on LOW cardinality columns first!)

**WARNING**: You MUST NOT use this function on a table with **string** columns. Your data WILL BE CORRUPTED

* usage: fb_resort(table_path, [col1],...,[colN]) (omit all columns to sort on lowest cardinality column first)
* returns: negative on failure

```
+----------------------------+
| fb_resort('/tmp/t6', 'c2') |
+----------------------------+
|                          1 |
+----------------------------+
Query OK, 0 rows affected (0.41 sec)
```

## fb_debug
### Set the ibis::gVerbose level.  

Level 0 won't record anything to MySQL server error log while level 10 will fill it. 
Use this function if there is something wrong with the UDF and you want me
to debug it.

* usage: fb_debug(debug_level)
* returns: debug level

```
+-------------+
| fb_debug(5) |
+-------------+
|           5 |
+-------------+
Query OK, 0 rows affected (0.00 sec)
```

# Some notes on FastBit "SQL" conventions and limitations
##The query string (used with fb_query) is a FastBit ibis style query string withi the following limitations:  

1. There is no FROM clause required, and one MUST NOT be used
2. The C standard math functions are supported (sqrt, pow, etc)
3. There is no GROUP BY clause.  If aggregate functions are used all non-aggregate functions are grouped by.
4. Aggregate functions SUM/AVG/COUNT/GROUP_CONCAT/MIN/MAX/VAR/VARP/STDEV/STDEVP are available
5. If you SUM() an int column, it can overflow.  
6. Distinct count is avaialable through the COUNTDISTINCT() function, not COUNT(DISTINCT ...)
7. The HAVING clause is not supported
8. ORDER BY and LIMIT clauses are supported, but LIMIT does not support offset
9. Queries without aggregation and missing a WHERE clause are not supported and will return no results
10. Using the WHERE clause:

  1. Must be followed by a set of equality or range conditions joined by logical operators 'AND', 'OR', 'XOR', and '!'. 
  2. A range condition can be one-sided as "A = 5" or "B > 10", or two-sided as "10 <= B < 20." 
  3. The supported operators are = (alternatively ==), <, <=, >, and >=. 
  4. The range condition that involves only one attribute with constant bounds are known as simple conditions, which can be very efficiently processed. 
  5. A range condition can also involve multiple attributes, such as, "A < B <= 5", or even arithmetic expressions, such as, "sin(A) + fabs(B) < sqrt(cx*cx+cy*cy)". 
  6. Note all one-argument and two-argument arithmetic functions available in math.h are supported. 
  7. The key word WHERE and the conditions following it are essential to a query and can not be ommited.
  8. The BETWEEN operator is also supported in the WHERE clause

### FastBit NULL handling is not the same as MySQL!
  1. There is no IS keyword.  
  2. For IS NULL use     - "NOT column_name NOT NULL"
  3. For IS NOT NULL use - "column_name NOT NULL"
  4. To insert NULL values from a file or a string, leave the column blank.  
    - For example "1," would have NULL for the second attribute.
    - "," would leave both attributes NULL
  5. Queries ignore rows containing NULLs by default!

#### From the source code
```C
/// @note All select operations excludes null values!  In most SQL
/// implementations, the function 'count(*)' includes the null values.
/// However, in FastBit, null values are always excluded.  For example, the
/// return value for 'count(*)' in the following two select statements may
/// be different if there are any null values in column A,
/// @code
/// select count(*) from ...;
/// select avg(A), count(*) from ...;
/// @endcode
/// In the first case, the number reported is purely determined by the
/// where clause.  However, in the second case, because the select clause
/// also involves the column A, all of null values of A are excluded,
/// therefore 'count(*)' in the second example may be smaller than that
/// of the first example.
```

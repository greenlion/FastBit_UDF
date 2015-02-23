# FastBit_UDF
MySQL UDF for creating, manipulating and querying FastBit indexes

## Some notes on FastBit "SQL" conventions and limitations
The query string (used with fb_query) is a FastBit ibis style query string withi the following limitations:  

1. There is no FROM clause required, and one MUST NOT be used
2. The select clause should be lower case, except for string literals
3. The C standard math functions are supported (sqrt, pow, etc)
4. There is no GROUP BY clause.  If aggregate functions are used all non-aggregate functions are grouped by.
5. Aggregate functions SUM/AVG/COUNT/GROUP_CONCAT/MIN/MAX/VAR/VARP/STDEV/STDEVP are available
6. If you SUM() an int column, it can overflow.  
6. Distinct count is avaialable through the COUNTDISTINCT() function, not COUNT(DISTINCT ...)
6. The HAVING clause is not supported
7. ORDER BY and LIMIT clauses are supported, but LIMIT does not support offset
8. Queries without aggregation and missing a WHERE clause are not supported and will return no results
9. Using the WHERE clause:

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

## Create an table/index
These UDF treat a single directory as one table.  Inside of the directory are
directories called partitions, which are created automatically when data is loaded in bulk.   Partitions themselves may contain sub-partitions, also maintained automatically. You specify the column names and types when creating the index as a "column specification", or colspec.  It is a comma separarted list of columns names and data types.

### Data types available
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

### Create a table with two integer columns called c2 and c3
usage: fb_create(data_path, colspec)
returns: 0 on success, negative on failure
```
+------------------------------------------+
| fb_create("/tmp/fbtest","c2:int,c3:int") |
+------------------------------------------+
|                                        0 |
+------------------------------------------+
```

## Loading a CSV file into the table/index
usage: fb_load(data_path, file_name, [delimiter=,], [max_per_partition=100000])
returns: number of rows loaded on success, negative on failure
```
+------------------------------------------+
| fb_load("/tmp/fbtest","/tmp/fbdata.txt") |
+------------------------------------------+
|                                 10000000 |
+------------------------------------------+
```

## Query the table to get a row count
usage: fb_query(data_path,output_file,query_string)
returns: QUERY_OK row_count or ERROR error_message

Query output is sent to a file which you can load with LOAD DATA INFILE
```
+----------------------------------------------------------+
| fb_query("/tmp/fbtest","/tmp/out.txt","select count(*)") |
+----------------------------------------------------------+
| QUERY_OK 1                                               |
+----------------------------------------------------------+

cat /tmp/out.txt
10000000 
```

## Unlink files (be careful!)
usage: fb_unlink(file_path)
returns: 0 on success, negative on error
The FastBit UDF won't overwrite files, so you have to unlink files you create 
You also don't want to waste disk space on a bunch of intermediate files you don't need
```
+---------------------------+
| fb_unlink("/tmp/out.txt") |
+---------------------------+
|                         0 |
+---------------------------+
```

## Delete rows
Deletes rows (and zaps them) from the table
usage: fb_delete(index_path, where_conditions)
returns: Number of rows deleted on success, negative on failure
```
+-------------------------------------+
| fb_delete("/tmp/fbtest", "c2 >= 0") |
+-------------------------------------+
|                            10000000 |
+-------------------------------------+
```

## Insert a row
Inserts a delimited row into the database.  Delimter may consist of more than one character, and each is used as a delimiter.  Multi-character delimiters are not supported.  The default delimter is comma (,).  Note that strings don't have to be quoted by it doesn't hurt to quote them with single quotes.
usage: fb_insert(index_path, delimted_string)
returns: 1 on success, negative on failure
```
+-------------------------------------+
| fb_insert('/tmp/fbtest', '1|1','|') |
+-------------------------------------+
|                                   1 |
+-------------------------------------+
```

create function fb_query returns string soname 'fb_udf.so';
create function fb_create returns integer soname 'fb_udf.so';
create function fb_load returns integer soname 'fb_udf.so';
create function fb_delete returns integer soname 'fb_udf.so';
create function fb_insert returns integer soname 'fb_udf.so';
create function fb_unlink returns integer soname 'fb_udf.so';

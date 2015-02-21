all:
	sh get_and_build_fastbit.sh
	gcc -o ibis_udf.so ibis_udf.cpp `mysql_config --cflags` -shared -I include/

install:
	cp *.so `mysql_config --plugindir`

clean:
	rm *.o *.so

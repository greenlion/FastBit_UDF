all:
	sh get_and_build_fastbit.sh
	g++ -shared -Wl,-rpath '-Wl,$$ORIGIN' -L lib/ -I include/ `mysql_config --include` -l fastbit -o fb_udf.so `mysql_config --cxxflags` fb_udf.cpp

install:
	cp *.so `mysql_config --plugindir`
	cp lib/*.so* `mysql_config --plugindir`

clean:
	rm *.so

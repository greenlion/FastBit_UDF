all:
	sh get_and_build_fastbit.sh
	g++ --std=c++11 -shared -Wl,-rpath '-Wl,$$ORIGIN' -L lib/ -I include/ -I `mysql_config --include` -l fastbit -o fb_udf.so `mysql_config --cxxflags` fb_udf.cpp

install:
	cp *.so `mysql_config --plugindir`
	cp lib/*.so* `mysql_config --plugindir`

clean:
	rm *.so


all: fastbit udf

fastbit: 
	sh get_and_build_fastbit.sh "2.0.1"
	
udf: 
	g++ --std=c++11 -shared -Wl,-rpath '-Wl,$$ORIGIN' -L lib/ -I include/ -I include/fastbit -I `mysql_config --include` -l fastbit -o fb_udf.so `mysql_config --cxxflags` -fPIC fb_udf.cpp

install: 
	echo Installing in `mysql_config --plugindir` 
	cp *.so `mysql_config --plugindir`
	cp lib/*.so* `mysql_config --plugindir`

clean:
	rm -f *.so

deep_clean:
	rm -f *.so *.gz
	rm -rf lib/ include/ bin/ fastbit{'',-*/} share/
	

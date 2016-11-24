test: test.cpp ../DL_Raster.h Makefile
	clang++ test.cpp -g -o test -Ofast -I .. -std=c++11 $(shell ./dlrtest-config)
	#clang++ test.cpp -g -o test -D DEBUG -O0 -I .. -std=c++11 $(shell ./dlrtest-config)

run: test
	./test

clean:
	rm -f test

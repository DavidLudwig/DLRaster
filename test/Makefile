test: test.cpp test_scenes.hpp ../DL_Raster.h Makefile
	#emcc test.cpp -O2 -o test.html -I .. -std=c++11 $(shell ./dlrtest-config emscripten)
	clang++ test.cpp -g -o test -Ofast -I .. -std=c++11 $(shell ./dlrtest-config)
	#clang++ test.cpp -g -o test -D DEBUG -O0 -I .. -std=c++11 $(shell ./dlrtest-config)

run: test
	./test

clean:
	rm -f test

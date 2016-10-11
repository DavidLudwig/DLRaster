test: test.cpp DL_Raster.h Makefile
	clang++ test.cpp -o test -D DLRTEST_FPSTICKS=1000 -Ofast -F external -framework SDL2 -I external/SDL2.framework/Headers -std=c++11 -framework OpenGL -rpath "@executable_path/external"

run: test
	./test

clean:
	rm -f test

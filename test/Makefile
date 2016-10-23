test: test.cpp ../DL_Raster.h Makefile
	clang++ test.cpp -o test -Ofast -F external -framework SDL2 -I .. -I external/SDL2.framework/Headers -std=c++11 -framework OpenGL -rpath "@executable_path/external"
	#clang++ test.cpp -g -o test -D DEBUG -O0 -F external -framework SDL2 -I .. -I external/SDL2.framework/Headers -std=c++11 -framework OpenGL -rpath "@executable_path/external"

run: test
	./test

clean:
	rm -f test

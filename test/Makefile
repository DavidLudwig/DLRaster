test: test.cpp DL_Raster.h
	g++ test.cpp -o test -F external -framework SDL2 -I external/SDL2.framework/Headers -std=c++11 -framework OpenGL -rpath "@executable_path/external"

run: test
	./test

clean:
	rm test

LIBS = -lglut -lGLU -lGL


marching_square: marching_square.cpp
	g++ marching_square.cpp -o $@ $(LIBS)


CXX=g++-5
CXX_FLAGS=-g -Wall -Werror -std=c++11

prepare:
	@mkdir -p build
	@mkdir -p bin

clean:
	@rm -f ./bin/*
	@rm -f ./build/*
	@rm -f *.log



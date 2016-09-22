
CXX=g++-5
CXX_FLAGS=-g -Wall -Werror -std=c++11


benchmark: ./src/AtomicStack.cpp ./test/benchmark.cpp
	$(CXX) $(CXX_FLAGS) -O3 -DNDEBUG $^ -o ./bin/benchmark

prepare:
	@mkdir -p build
	@mkdir -p bin

clean:
	@rm -f ./bin/*
	@rm -f ./build/*
	@rm -f *.log



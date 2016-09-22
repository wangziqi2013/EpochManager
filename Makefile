
CXX=g++-5
CXX_FLAGS=-g -Wall -Werror -std=c++11


benchmark: ./src/AtomicStack.cpp ./test/benchmark.cpp
	$(CXX) $(CXX_FLAGS) -O3 -DNDEBUG $^ -o ./bin/benchmark

basic_test: ./src/AtomicStack.cpp ./test/basic_test.cpp
	$(CXX) $(CXX_FLAGS) $^ -o ./bin/basic_test

prepare:
	@mkdir -p build
	@mkdir -p bin

clean:
	@rm -f ./bin/*
	@rm -f ./build/*
	@rm -f *.log



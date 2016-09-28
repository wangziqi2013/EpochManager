
CXX=g++-5
CXX_FLAGS=-g -Wall -Werror -std=c++11 -pthread

benchmark: ./src/AtomicStack.cpp ./test/benchmark.cpp ./src/LocalWriteEM.cpp
	$(CXX) $(CXX_FLAGS) -O3 -DNDEBUG $^ -o ./bin/benchmark
	@ln -sf ./bin/benchmark ./benchmark-bin

basic_test: ./src/AtomicStack.cpp ./test/basic_test.cpp
	$(CXX) $(CXX_FLAGS) $^ -o ./bin/basic_test

prepare:
	@mkdir -p build
	@mkdir -p bin

clean:
	@rm -f ./bin/*
	@rm -f ./build/*
	@rm -f *.log



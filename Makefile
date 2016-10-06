
CXX=g++-5
CXX_FLAGS=-g -Wall -Werror -std=c++11 -pthread

benchmark: ./src/AtomicStack.cpp ./test/benchmark.cpp ./src/LocalWriteEM.cpp ./build/test_suite.o
	$(CXX) $(CXX_FLAGS) -O3 -DNDEBUG $^ -o ./bin/benchmark
	@ln -sf ./bin/benchmark ./benchmark-bin

basic_test: ./src/AtomicStack.cpp ./test/basic_test.cpp ./build/test_suite.o
	$(CXX) $(CXX_FLAGS) $^ -o ./bin/basic_test
	@ln -sf ./bin/basic_test ./basic_test-bin

em_test: ./src/AtomicStack.cpp ./test/em_test.cpp ./src/LocalWriteEM.cpp ./build/test_suite.o
	$(CXX) $(CXX_FLAGS) $^ -o ./bin/em_test
	@ln -sf ./bin/em_test ./em_test-bin

random_gen_test: ./test/random_gen_test.cpp ./build/test_suite.o
	$(CXX) $(CXX_FLAGS) $^ -o ./bin/random_gen_test
	@ln -sf ./bin/random_gen_test ./random_gen_test-bin

./build/test_suite.o: ./test/test_suite.cpp
	$(CXX) $(CXX_FLAGS) $^ -c -o ./build/test_suite.o

prepare:
	@mkdir -p build
	@mkdir -p bin

clean:
	@rm -f ./bin/*
	@rm -f ./build/*
	@rm -f *.log
	@rm -f ./*-bin



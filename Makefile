all: memory
clean:
	rm -f memory


memory: memory.cpp
	g++ -std=c++11 -O3 memory.cpp -lpthread -lrt -omemory

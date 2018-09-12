all: memory
clean:
	rm -f memory


memory:
	g++ -std=c++11 -O3 memory.cpp -lpthread -lrt -omemory

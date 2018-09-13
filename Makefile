all: memory

TOOL?=$(if $(shell which perf),perf stat,time)


clean:
	rm -f memory


memory: memory.cpp
	g++ -std=c++11 -O3 memory.cpp -lpthread -lrt -omemory

check: memory
	./memory 2 256 1000 malloc thread check
	./memory 2 256 1000 shm thread check
	./memory 2 256 1000 malloc fork check
	./memory 2 256 1000 shm fork check
	@echo
	@echo "Requires manual verification. Only 'malloc fork' is expected to give change_count=0"
	@echo

perf: memory
	$(TOOL) ./memory 2 256 1000 malloc thread
	$(TOOL) ./memory 2 256 1000 shm thread
	$(TOOL) ./memory 2 256 1000 shm fork

	$(TOOL) ./memory 3 256 1000 malloc thread
	$(TOOL) ./memory 3 256 1000 shm thread
	$(TOOL) ./memory 3 256 1000 shm fork

	taskset -c 0,1 $(TOOL) ./memory 2 256 1000 malloc thread
	taskset -c 0,1 $(TOOL) ./memory 2 256 1000 shm thread
	taskset -c 0,1 $(TOOL) ./memory 2 256 1000 shm fork

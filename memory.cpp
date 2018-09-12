#include <vector>
#include <stdlib.h>
#include <thread>
#include <unistd.h>
#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <assert.h>
#include <string.h>
#include <sys/time.h>

void* mymemcpy(void* d1, const void* s1, size_t size) throw ()
{
  uint64_t* d = (uint64_t*)d1;
  uint64_t* s = (uint64_t*)s1;
  while((int)size>0) {
    *d=*s;
    d++;
    s++;
    size-=sizeof(d);
  }
  return d1;
}

char* base_mem;
bool use_fork = false;

enum {use_memcpy, use_mymemcpy} memcpy_type = use_memcpy;
auto active_memcpy = memcpy;
enum {use_shm, use_mmap, use_malloc} alloc_type = use_malloc;

size_t thread_cnt;
size_t buf_size;
size_t buf_cnt;

void a_test(size_t thread_id)
{
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  srand(tv.tv_usec);
  
  char* memory = base_mem + buf_size * buf_cnt * thread_id;
  char* buffers[buf_cnt];  
  for (size_t i = 0; i < buf_cnt; i++)
      buffers[i] = memory + i * buf_size;

  for (size_t i = 0; i < 20000000; i++) {
    active_memcpy(buffers[rand() % buf_cnt], buffers[rand() % buf_cnt], buf_size);
  }
}

void fork_version()
{
  pid_t subs[thread_cnt];
  for (size_t i = 0; i < thread_cnt; i++) {
    pid_t p;
    p = fork();
    if (p == 0) {
      a_test(i);
      return;
    } else {
      subs[i] = p;
    }
  }

  for (size_t i = 0; i < thread_cnt; i++) {
    int wstatus;
    pid_t p = wait(&wstatus);
  }
}

void thread_version()
{
  std::thread worker[thread_cnt];
  for (size_t i = 0; i < thread_cnt; i++) {
    worker[i] = std::thread(a_test, i);
  }
  for (size_t i = 0; i < thread_cnt; i++) {
    worker[i].join();
  }
}


void the_test()
{
  

  switch (alloc_type) {
  case use_mmap:
    base_mem = (char*)mmap(nullptr, thread_cnt * buf_cnt * buf_size, PROT_READ|PROT_WRITE|PROT_EXEC,
			   MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    std::cout << "using mmap" << std::endl;
    break;
  case use_shm:
    {
      int mem_fd = shm_open("name", O_RDWR|O_CREAT, 0666);
      assert(mem_fd >= 0);
      int res = ftruncate(mem_fd, thread_cnt * buf_cnt * buf_size);
      assert(res == 0);
      base_mem = (char*)mmap(nullptr, thread_cnt * buf_cnt * buf_size, PROT_READ|PROT_WRITE|PROT_EXEC,
			     MAP_SHARED, mem_fd, 0);
      std::cout << "using shm" << std::endl;
    }
    break;
  case use_malloc:
    base_mem = (char*) malloc (thread_cnt * buf_cnt * buf_size);
    std::cout << "using malloc" << std::endl;    
    break;
  default:
    assert(0);
  }

  switch (memcpy_type) {
  case use_memcpy:
    active_memcpy = memcpy;
    std::cout << "using original memcpy" << std::endl;    
    break;
  case use_mymemcpy:
    active_memcpy = mymemcpy;
    std::cout << "using local memcpy" << std::endl;    
    break;
  default:
    assert(0);
  }
  
  if (use_fork) {
    std::cout << "using fork" << std::endl;
    fork_version();
  } else {
    std::cout << "using thread" << std::endl;
    thread_version();
  }
}



int main(int argc, char **argv)
{
  thread_cnt = atoi(argv[1]);
  buf_size = atoi(argv[2]);
  buf_cnt = atoi(argv[3]);

  argv+=4;
  argc-=4;
  while (argc > 0) {
    if (argv[0] == std::string("thread")) {
      use_fork = false;
    } else
    if (argv[0] == std::string("fork")) {
      use_fork = true;
    } else
    if (argv[0] == std::string("malloc")) {
      alloc_type = use_malloc;
    } else
    if (argv[0] == std::string("shm")) {
      alloc_type = use_shm;
    } else
    if (argv[0] == std::string("mmap")) {
      alloc_type = use_mmap;
    } else
    if (argv[0] == std::string("memcpy")) {
      memcpy_type = use_memcpy;
    } else
    if (argv[0] == std::string("mymemcpy")) {
      memcpy_type = use_mymemcpy;
    } else {
      std::cerr << "unrecognized '" << argv[0] << "'" << std::endl;
      return 1;
    }
    argc--;
    argv++;
  }
  std::cout << "thread_cnt=" << thread_cnt << " buf_size=" << buf_size << " buf_cnt=" << buf_cnt << std::endl;
  the_test();
  
  return 0;
}

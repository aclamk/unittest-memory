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
bool wide = false;
bool do_check = false;

size_t thread_cnt;
size_t buf_size;
size_t buf_cnt;

void a_test(size_t thread_id)
{
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  srand(tv.tv_usec);
  
  char* memory = base_mem + (buf_size * buf_cnt + (wide?10*1024*1024:0)) * thread_id;
  char* buffers[buf_cnt];  
  for (size_t i = 0; i < buf_cnt; i++) {
      buffers[i] = memory + i * buf_size;
      memset(buffers[i], rand(), buf_size);
  }

  size_t change_count=0;
  char* check_memory = base_mem + (buf_size * buf_cnt + (wide?10*1024*1024:0)) *
	((thread_id + 1) % thread_cnt);
  char last_val = *check_memory;
  
  for (size_t j = 0; j < 200000; j++) {
    for (size_t i = 0; i < 100; i++) {
      active_memcpy(buffers[rand() % buf_cnt], buffers[rand() % buf_cnt], buf_size);
      memset(buffers[rand() % buf_cnt], rand(), buf_size);
    }
    if (do_check) {
      if (last_val != *check_memory)
	change_count++;
      last_val = *check_memory;
    }
  }
  if (do_check) {
    std::cout << "thread_id=" << thread_id << " change_count=" << change_count << std::endl;
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
  std::cout << "threads=" << thread_cnt << " size=" << buf_size << " elements=" << buf_cnt << "; ";

  if (do_check)
    std::cout << "*check* ";
  
  size_t data_size = thread_cnt * (buf_cnt * buf_size + (wide?10*1024*1024:0));

  switch (alloc_type) {
  case use_mmap:
    base_mem = (char*)mmap(nullptr, data_size, PROT_READ|PROT_WRITE|PROT_EXEC,
			   MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    std::cout << "mmap ";
    break;
  case use_shm:
    {
      int mem_fd = shm_open("name", O_RDWR|O_CREAT, 0666);
      assert(mem_fd >= 0);
      int res = ftruncate(mem_fd, data_size);
      assert(res == 0);
      base_mem = (char*)mmap(nullptr, data_size, PROT_READ|PROT_WRITE|PROT_EXEC,
			     MAP_SHARED, mem_fd, 0);
      std::cout << "shm ";
    }
    break;
  case use_malloc:
    base_mem = (char*) malloc (data_size);
    std::cout << "malloc ";
    break;
  default:
    assert(0);
  }

  switch (memcpy_type) {
  case use_memcpy:
    active_memcpy = memcpy;
    std::cout << "memcpy ";    
    break;
  case use_mymemcpy:
    active_memcpy = mymemcpy;
    std::cout << "my-memcpy";    
    break;
  default:
    assert(0);
  }
  std::cout << "wide=" << wide << " ";
  if (use_fork) {
    std::cout << "fork " << std::endl;
    fork_version();
  } else {
    std::cout << "thread " << std::endl;
    thread_version();
  }

  if (alloc_type == use_shm) {
    shm_unlink("name");
  }
}

void print_help() {
  std::cout << "usage:" << std::endl;
  std::cout << "  memory thread-count element-size element-count [modifiers]" << std::endl;
  std::cout << std::endl;
  std::cout << "modifiers:" << std::endl;
  std::cout << "  thread - create worker threads [default]" << std::endl;
  std::cout << "  fork   - create worker subprocesses" << std::endl;
  std::cout << "  malloc [default] / shm / mmap" << std::endl;
  std::cout << "         - select allocator " << std::endl;
  std::cout << "  wide   - make 10MB space between test sets" << std::endl;
  std::cout << "  check  - verify if other regions do change (sanity checkup)" << std::endl;
  std::cout << "  memcpy - select glibc [default]" << std::endl;
  std::cout << "  mymemcpy - internal hand-crafted memcpy" << std::endl;
}

int main(int argc, char **argv)
{
  if (argc < 4) {
    print_help();
    return(1);
  }
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
    if (argv[0] == std::string("wide")) {
      wide = true;
    } else
    if (argv[0] == std::string("check")) {
      do_check = true;
    } else
    if (argv[0] == std::string("memcpy")) {
      memcpy_type = use_memcpy;
    } else
    if (argv[0] == std::string("mymemcpy")) {
      memcpy_type = use_mymemcpy;
    } else {
      std::cerr << "unrecognized '" << argv[0] << "'" << std::endl;
      print_help();
      return 1;
    }
    argc--;
    argv++;
  }
  the_test();
  
  return 0;
}

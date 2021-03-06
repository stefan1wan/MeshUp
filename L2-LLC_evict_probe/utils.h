// include files
#include <stdio.h>				// printf, etc
#include <stdint.h>				// standard integer types, e.g., uint32_t
#include <signal.h>				// for signal handler
#include <stdlib.h>				// exit() and EXIT_FAILURE
#include <string.h>				// strerror() function converts errno to a text string for printing
#include <fcntl.h>				// for open()
#include <errno.h>				// errno support
#include <assert.h>				// assert() function
#include <unistd.h>				// sysconf() function, sleep() function
#include <sys/mman.h>			// support for mmap() function
#include <linux/mman.h>			// required for 1GiB page support in mmap()
#include <math.h>				// for pow() function used in RAPL computations
#include <time.h>
#include <sys/time.h>			// for gettimeofday
#define __USE_GNU
#include <pthread.h>
#include <termio.h>
#include <sched.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <x86intrin.h>
#include <time.h>
#include <emmintrin.h>
#include <x86intrin.h>
#include <sys/stat.h>
#include <limits.h>

#define LOG_NAME "time_line.log"
#define NUMTRIES 5000
#define NUM_CHA_COUNTERS 4
#define NUM_CHA_USED 24
#define NUM_CHA_BOXES 28
#define NUMPAGES 1L
#define MYPAGESIZE 1073741824UL
//#define MYPAGESIZE 1024UL

#define NUM_SOCKETS 1
#define NUM_IMC_CHANNELS 6
#define NUM_IMC_COUNTERS 5
#define CORES_USED 24

#define Lines_per_PAGE 16777216
//#define Lines_per_PAGE 16
#define NFLUSHES 10

#define MIN(x,y) ((x)<(y)?(x):(y))
#define MAX(x,y) ((x)>(y)?(x):(y))

#define EXCHAGE_RATE 5000000
#define CACHELINES_READ 24 
#define L2_INDEX_NUM 10

#define VERBOSE 0
#define VERBOSE2 0

#define CPU_FREQ 2300000000.0

typedef struct
{
	int cpuid;
	uint64_t* cachelines;
  long cachelines_found;
  uint64_t* timeline_local;
  uint64_t* timeline_p;
} thread_args;

static inline uint64_t rdtscp(){
	uint64_t rax,rdx;
	asm volatile ( "rdtscp\n" : "=a" (rax), "=d" (rdx)::"%rcx","memory");
	return (rdx << 32) | rax;
}

void attach_cpu(int cpu){
  cpu_set_t mask; 
  cpu_set_t get;  
  int num=96;
  CPU_ZERO(&mask);  
  CPU_SET(cpu, &mask);
  if (sched_setaffinity(0, sizeof(mask), &mask) == -1)
  {
     printf("warning: could not set CPU affinity, continuing...\n");
  }

  CPU_ZERO(&get);
  if (sched_getaffinity(0, sizeof(get), &get) == -1)
  {
      printf("warning: cound not get thread affinity, continuing...\n");
  }
  for(int i=0; i< num; i++){
    if(CPU_ISSET(i, &get))
      printf("this thread is running on  processor : %d\n",i);
  }

}
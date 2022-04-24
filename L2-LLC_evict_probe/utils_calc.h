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

#define CHA0 0
#define CHA1 2
#define CHA2 7
#define CHA3 12
#define CHA4 17
#define CHA5 21

#define CPU0 0
#define CPU1 6
#define CPU2 19
#define CPU3 3
#define CPU4 16
#define CPU5 17

#define CACHELINES_READ 24

#define NUMTRIES 5000
#define NUM_CHA_COUNTERS 4
#define NUM_CHA_USED 24
#define NUM_CHA_BOXES 28
#define NUMPAGES 1L
#define MYPAGESIZE 1073741824UL

#define NUM_SOCKETS 2
#define NUM_IMC_CHANNELS 6
#define NUM_IMC_COUNTERS 5
#define CORES_USED 24

#define Lines_per_PAGE 16777216
#define NFLUSHES 10

#define MIN(x,y) ((x)<(y)?(x):(y))
#define MAX(x,y) ((x)>(y)?(x):(y))

#define EXCHAGE_RATE 100000000

#define VERBOSE 0
#define VERBOSE2 0

#define CPU_FREQ 2300000000.0

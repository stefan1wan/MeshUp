
#include <stdio.h>				// printf, etc
#include <stdint.h>				// standard integer types, e.g., uint32_t
#include <stdlib.h>				// exit() and EXIT_FAILURE
#include <string.h>				// strerror() function converts errno to a text string for printing
#include <fcntl.h>				// for open()
#include <errno.h>				// errno support
#include <unistd.h>				// sysconf() function, sleep() function
#define __USE_GNU
#include <pthread.h>

#define NUM_CHA_COUNTERS 4
#define NUM_TILE_ENABLED 24
#define EPOCHS 30
#define LOOP_NUM 10000000
#define MSRFLIENAME  "/dev/cpu/0/msr"

void show_counters();
void attach_core(int);
void make_core_busy(int processor);
int max_counter_cha();

uint32_t cha_perfevtsel[NUM_CHA_COUNTERS];
long cha_counts[NUM_TILE_ENABLED][NUM_CHA_COUNTERS][2]; // 28 tiles per socket, 4 counters per tile, 2 times (before and after)
uint64_t counters_changes[NUM_TILE_ENABLED];
uint64_t core2cha_map[NUM_TILE_ENABLED];

int main(){
    int fd, tile, counter;
    uint64_t msr_val, msr_num;

    /* Open msr fd of socket 0 */
	fd = open(MSRFLIENAME, O_RDWR);
    if ( fd== -1) {
		fprintf(stderr,"ERROR %s when trying to open %s\n", strerror(errno), MSRFLIENAME);
		exit(-1);
	}

    /* Clear all CHA counters */
    for (tile=0; tile<NUM_TILE_ENABLED; tile++) {
		for (counter=0; counter<4; counter++) {
			cha_counts[tile][counter][0] = 0;
			cha_counts[tile][counter][1] = 0;
		}
	}

    /* Set the PMON control resgister position*/
    cha_perfevtsel[0] = 0x00400217;     // CORE_PMA. C1 Transition
    cha_perfevtsel[1] = 0x00400417;     // CORE_PMA. C1 State
    cha_perfevtsel[2] = 0x00401017;     // *LCORE_PMA GV(We actually use this event)
    cha_perfevtsel[3] = 0x00400534;		// LLC_LOOKUP.DATA_WRITE (WB from L2) <-- requires CHA_FILTER0[26:17]

    uint64_t cha_filter0 =   0x0001c000;	// set bits 20,19,18 HES -- all SF lookups, no LLC lookups

    /*  Clear the Map */
    for(int i=0; i<NUM_TILE_ENABLED; i++)
        core2cha_map[i]=0xffff;
    
    /* Find the corresponding CHA for each core*/
    for(int core=0; core<NUM_TILE_ENABLED; core++){

        // clear the counters' array
        for(int i=0; i<NUM_TILE_ENABLED; i++) counters_changes[i]=0;

        for(int j=0; j<EPOCHS; j++){
            printf("Programming CHA counters\n");
            for (tile=0; tile<NUM_TILE_ENABLED; tile++) {

                    msr_num = 0xe00 + 0x10*tile;		// box control register -- set enable bit
                    msr_val = 0x00400000;
                    pwrite(fd,&msr_val,sizeof(msr_val),msr_num);

                    msr_num = 0xe00 + 0x10*tile + 1;	// ctl0
                    msr_val = cha_perfevtsel[0];
                    pwrite(fd,&msr_val,sizeof(msr_val),msr_num);

                    msr_num = 0xe00 + 0x10*tile + 2;	// ctl1
                    msr_val = cha_perfevtsel[1];
                    pwrite(fd,&msr_val,sizeof(msr_val),msr_num);

                    msr_num = 0xe00 + 0x10*tile + 3;	// ctl2
                    msr_val = cha_perfevtsel[2];
                    pwrite(fd,&msr_val,sizeof(msr_val),msr_num);

                    msr_num = 0xe00 + 0x10*tile + 4;	// ctl3
                    msr_val = cha_perfevtsel[3];
                    pwrite(fd,&msr_val,sizeof(msr_val),msr_num);

                    msr_num = 0xe00 + 0x10*tile + 5;	// filter0
                    msr_val = cha_filter0;				// core & thread
                    pwrite(fd,&msr_val,sizeof(msr_val),msr_num);
                }
            
            //  Counters before core busy
            for (tile=0; tile<NUM_TILE_ENABLED; tile++) {
                for (counter=0; counter<NUM_CHA_COUNTERS; counter++) {
                    msr_num = 0xe00 + 0x10*tile + 0x8 + counter;
                    pread(fd,&msr_val,sizeof(msr_val),msr_num);
                    cha_counts[tile][counter][0] = msr_val;
                }
            }
            
            make_core_busy(core);

           //  Counters after core busy
            for (tile=0; tile<NUM_TILE_ENABLED; tile++) {
                for (counter=0; counter<NUM_CHA_COUNTERS; counter++) {
                    msr_num = 0xe00 + 0x10*tile + 0x8 + counter;
                    pread(fd,&msr_val,sizeof(msr_val),msr_num);
                    cha_counts[tile][counter][1] = msr_val;
                }
            }
            // Record the changes of event "LCORE_PMA GV"
            for (tile=0; tile<NUM_TILE_ENABLED; tile++) 
                counters_changes[tile] += (cha_counts[tile][2][1]-cha_counts[tile][2][0]);
        }
        // Map the cha with largest counters to this core
        core2cha_map[core] = max_counter_cha();
        //show_counters();
    }

    /* Output the results */
    for(int i=0;i<NUM_TILE_ENABLED;i++)
        printf("cpu %d: cha %ld\n", i, core2cha_map[i]);
}

void show_counters(){
    puts("----Original----");
    puts("tile\t counter0 counter1 counter2 counter3");
    for (int tile=0; tile<NUM_TILE_ENABLED; tile++){
        printf("%d:\t", tile);
        for(int counter=0; counter<NUM_CHA_COUNTERS; counter++)
            printf("%ld\t", cha_counts[tile][counter][0]);
        putchar('\n');
    }
    puts("----------------------");

    puts("----Final----");
    puts("tile\t counter0 counter1 counter2 counter3");
    for (int tile=0; tile<NUM_TILE_ENABLED; tile++){
        printf("%d:\t", tile);
        for(int counter=0; counter<NUM_CHA_COUNTERS; counter++)
            printf("%ld\t", cha_counts[tile][counter][1]);
        putchar('\n');
    }

    puts("----------------------");
    puts("----Diff----");
    puts("tile\t counter0 counter1 counter2 counter3");
    for (int tile=0; tile<NUM_TILE_ENABLED; tile++){
        printf("%d:\t", tile);
        for(int counter=0; counter<NUM_CHA_COUNTERS; counter++)
            printf("%15ld\t", cha_counts[tile][counter][1]-cha_counts[tile][counter][0]);
        putchar('\n');
    }
}

void attach_core(int cpu){
    cpu_set_t mask;
    cpu_set_t get;
    int num=96;
    CPU_ZERO(&mask);  
    CPU_SET(cpu, &mask);
    if (sched_setaffinity(0, sizeof(mask), &mask) == -1){
        printf("warning: could not set CPU affinity, continuing...\n");
    }

    CPU_ZERO(&get);
    if (sched_getaffinity(0, sizeof(get), &get) == -1){
        printf("warning: cound not get thread affinity, continuing...\n");
    }
    for(int i=0; i< num; i++){
        if(CPU_ISSET(i, &get))
            printf("this thread is running on  processor : %d\n",i);
    }
}

void make_core_busy(int processor){
	attach_core(processor);
    int i=0;
    while(i<LOOP_NUM) 
        i++;
    return;
}

int max_counter_cha(){
    uint64_t m =0;
    uint64_t index = 0;
    for(int i=0;i<NUM_TILE_ENABLED; i++)
        if(m <= counters_changes[i]){
            m = counters_changes[i];
            index = i;
        }
    return index;
}

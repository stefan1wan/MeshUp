#include "utils.h"
#include "target_%d.h"

void* target_%d(void* param){
    thread_args* Param = (thread_args*) param;
	attach_cpu(Param->cpuid);
    uint64_t *cachelines = Param->cachelines;
    long cachelines_found = Param->cachelines_found;

    assert(cachelines_found >  CACHELINES_READ);
    uint64_t local_sum=0;

    for (volatile uint64_t i = 0; i < EXCHAGE_RATE; i++) {
        // 20, 10000, 共30us
        // 10, 5000, 约10us
        // 5, 2500, 约5us
        // 2, 1000, 约2.5us，已经不稳定
        if(i%10==0){
            for(volatile int j=0; j<5000; j++);
            //usleep(10);
        }
        else{
            FETCH_CACHELINE(i)
        }
    }
    printf("Done!");
}

/* module.h */
#ifndef LIB_H
#define LIB_H

void* target_%d(void* param);

#endif
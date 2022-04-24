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
        // 20, 10000, about 30us total
        // 10, 5000, about 10us
        // 5, 2500, about 5us
        // 2, 1000, about 2.5us, not distinguishable yet
        if(i%10==0){
            for(volatile int j=0; j<5000; j++);
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
#include "utils.h"
#include "hard_code_%d.h"

void* congest_%d(void* param){
    thread_args* Param = (thread_args*) param;
	attach_cpu(Param->cpuid);
    uint64_t *cachelines = Param->cachelines;
    long cachelines_found = Param->cachelines_found;
    uint64_t* timeline = Param->timeline_local;
    uint64_t* p = Param->timeline_p;

    assert(cachelines_found >  CACHELINES_READ);
    
    uint64_t t_start, t_end;
    uint64_t local_sum=0;
    uint64_t pos = 0;
    // t_start = rdtscp();

    for (volatile uint64_t i = 0; i < EXCHAGE_RATE; i++) {
        FETCH_CACHELINE(i)
        if(i%2==1)
            timeline[pos++]=rdtscp();
    }  

    *p=pos;
    printf("Done!");
    //t_end = rdtscp();
}

/* module.h */
#ifndef LIB_H
#define LIB_H

void* congest_%d(void* param);

#endif
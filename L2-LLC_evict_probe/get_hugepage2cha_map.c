#include "utils_calc.h"

extern unsigned long long get_pagemap_entry( void * va );
// 给定一个cacheline_确定它映射的CHA
int find_cha_by_cacheline(uint64_t page_number, uint64_t line_number, double*array, int8_t cha_by_page[NUM_CHA_USED][Lines_per_PAGE], uint64_t*paddr_by_page, int fd, double* globalsum);
// 准备cha的msr
int prapare_cha_ctr_reg();


void hugepage2cha(double*array, uint64_t len, double**page_pointers, int8_t cha_by_page[NUM_CHA_USED][Lines_per_PAGE], uint64_t*paddr_by_page){

    // 初始化所有cache_line的记录
    for(uint64_t page_number =0; page_number < NUMPAGES; page_number++)
        for(uint64_t line_number=0; line_number < Lines_per_PAGE; line_number++)
            cha_by_page[page_number][line_number] = -1;

    // initialize page_pointers to point to the beginning of each page in the array
	// then get and print physical addresses for each
    printf(" Page    ArrayIndex            VirtAddr        PagemapEntry         PFN           PhysAddr\n");

    uint64_t pageframenumber[NUMPAGES];     // one PFN entry for each page allocated
    for(uint64_t j=0;j<NUMPAGES;j++)
    {
        uint64_t k = j*MYPAGESIZE/sizeof(double);
        page_pointers[j] = &array[k];       //vitual address of hugepage
        unsigned long pagemapentry = get_pagemap_entry(&array[k]); //pagemap_entry of hugepage
        pageframenumber[j] = (pagemapentry & (unsigned long) 0x007FFFFFFFFFFFFF);
        printf(" %.5ld   %.10ld  %#18lx  %#18lx  %#18lx  %#18lx\n",j,k,(uint64_t)&array[k],pagemapentry,pageframenumber[j],(pageframenumber[j]<<12));
    }

    // init pysical address for every hugepage
    for(uint64_t j=0; j<NUMPAGES; j++)
		paddr_by_page[j] = pageframenumber[j] << 12;

    // prepare msr
    int fd=prapare_cha_ctr_reg();

    // find cha for each cacheline

    double globalsum =0;
    long totaltries = 0;

    for(uint64_t page_number = 0; page_number<NUMPAGES; page_number++){
        printf("Physical address of hugepage: 0x%.12lx\n", paddr_by_page[page_number]);

        for(uint64_t line_number=0; line_number < Lines_per_PAGE; line_number++){
            totaltries +=  find_cha_by_cacheline(page_number, line_number, array, cha_by_page, paddr_by_page, fd, &globalsum);
        }
    }

    printf("DUMMY: globalsum %f\n", globalsum);
    printf("VERBOSE: L3 Mapping Complete in %ld tries for %ld cache lines ratio %f\n", totaltries, Lines_per_PAGE*NUMPAGES, (double)totaltries/(double)(Lines_per_PAGE*NUMPAGES));

    return;
}


int prapare_cha_ctr_reg(){
    int fd;
    char filename[100];
    sprintf(filename,"/dev/cpu/0/msr");
    fd = open(filename,O_RDWR);
    if(fd == -1){
        printf("ERROR when trying to open %s\n",filename);
    }
    uint64_t msr_val, msr_num;
    uint32_t cha_perfevtsel[4];

    cha_perfevtsel[0] = 0x0040073d;		// SF_EVICTION S,E,M states
	cha_perfevtsel[1] = 0x00400334;		// LLC_LOOKUP.DATA_READ	<-- requires CHA_FILTER0[26:17]
	cha_perfevtsel[2] = 0x00400534;		// LLC_LOOKUP.DATA_WRITE (WB from L2) <-- requires CHA_FILTER0[26:17]
	cha_perfevtsel[3] = 0x0040af37;		// LLC_VICTIMS.TOTAL (MESF) (does not count clean victims)
	uint64_t cha_filter0 = 0x01e20000;		// set bits 24,23,22,21,17 FMESI -- all LLC lookups, no SF lookups

    for(int tile=0; tile < 28;tile++)
    {
        msr_num = 0xe00 + 0x10*tile;        //使能位置
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
        msr_val = cha_filter0;				// bits 28:21,17 FMESI -- all LLC lookups, not not SF lookups
        pwrite(fd,&msr_val,sizeof(msr_val),msr_num);
    }
    return fd;
}



int find_cha_by_cacheline(uint64_t page_number, uint64_t line_number, double*array, int8_t cha_by_page[NUM_CHA_USED][Lines_per_PAGE], uint64_t*paddr_by_page, int fd, double* globalsum)
{
    int good, good_old, good_new, found, numtries, function_total_try;

    double avg_count, goodness1, goodness2, goodness3;
    int min_count, max_count, sum_count, old_cha;
    uint64_t msr_val, msr_num;
    double sum=0;

    good = 0;
    good_old = 0;
    good_new = 0;
    numtries = 0;
    unsigned long cha_counts[NUM_CHA_BOXES][NUM_CHA_COUNTERS][2];

    uint64_t page_base_index = page_number * 262144;  //2*2^17 ;; 2M=2*2^20 ;; sizeof(double)=8

    if(VERBOSE){
        //if (line_number%64 == 0) {
            unsigned long pagemapentry = get_pagemap_entry(&array[page_base_index+line_number*8]);
            printf("DEBUG: page_base_index 0x%lx line_number 0x%lx index 0x%lx pagemapentry 0x%lx\n",page_base_index,line_number,page_base_index+line_number*8,pagemapentry);
        //}
    }
    
    do { // -------------- Inner Repeat Loop until results pass "goodness" tests --------------
        numtries++;
        if (numtries > NUMTRIES) {
            printf("ERROR: No good results for line %lx after %d tries\n",line_number,numtries);
            exit(101);
        }
        function_total_try++;

        // 1. read L3 counters before starting test
        for (int tile=0; tile<NUM_CHA_USED; tile++) {
            msr_num = 0xe00 + 0x10*tile + 0x8 + 1;				// counter 1 is the LLC_LOOKUPS.READ event
            pread(fd,&msr_val,sizeof(msr_val),msr_num);
            cha_counts[tile][1][0] = msr_val;					//  use the array I have already declared for cha counts
            // printf("DEBUG: page %ld line %ld msr_num 0x%x msr_val %ld cha_counter1 %lu\n",
            //		page_number,line_number,msr_num,msr_val,cha_counts[0][tile][1][0]);
        }

        //2. Access the line many times
        sum=0;
        for(uint64_t i = 0; i<NFLUSHES; i++)
        {
            sum += array[page_base_index+line_number*8];
            _mm_mfence();
            _mm_clflush(&array[page_base_index+line_number*8]);
            _mm_mfence();
        }
        *globalsum += sum;

        // 3. read L3 counters after loads are done
        for (int tile=0; tile<NUM_CHA_USED; tile++) {
            msr_num = 0xe00 + 0x10*tile + 0x8 + 1;				// counter 1 is the LLC_LOOKUPS.READ event
            pread(fd,&msr_val,sizeof(msr_val),msr_num);
            cha_counts[tile][1][1] = msr_val;					//  use the array I have already declared for cha counts
        }

        if(0){
            for (int tile=0; tile<NUM_CHA_USED; tile++) {
                printf("DEBUG: page %lx line %lx cha_counter1_after %lu cha_counter1 before %lu delta %lu\n",
                        page_number,line_number,cha_counts[tile][1][1],cha_counts[tile][1][0],cha_counts[tile][1][1]-cha_counts[tile][1][0]);
            }
        }

        //   CHA counter 1 set to LLC_LOOKUP.READ
        //  4. Determine which L3 slice owns the cache line
        //      first do a rough quantitative checks of the "goodness" of the data
        //		goodness1 = max/NFLUSHES (pass if >95%)
        // 		goodness2 = min/NFLUSHES (pass if <20%)
        //		goodness3 = avg/NFLUSHES (pass if <40%)
        max_count =0;
        min_count = 1<<30;
        sum_count = 0;

        // 记下哪个CHA访问LLC最多和最少  把前后差值加起来
        for (int tile=0; tile<NUM_CHA_USED; tile++) {
            max_count = MAX(max_count, cha_counts[tile][1][1]-cha_counts[tile][1][0]);
            min_count = MIN(min_count, cha_counts[tile][1][1]-cha_counts[tile][1][0]);
            sum_count += cha_counts[tile][1][1]-cha_counts[tile][1][0];
        }

        avg_count = (double)(sum_count - max_count) /(double)(NUM_CHA_USED);
        goodness1 = (double) max_count / (double) NFLUSHES;
        goodness2 = (double) min_count / (double) NFLUSHES;
        goodness3 =          avg_count / (double) NFLUSHES;

        if (goodness1 > 0.95 && goodness2 < 0.2 && goodness3 < 0.4) good_new=1;
        else good_new=0; // 整体的pattern

        if(VERBOSE){
            printf("GOODNESS: line_number 0x%lx max_count %d min_count %d sum_count %d avg_count %f goodness1 %f goodness2 %f goodness3 %f\n",
                            line_number, max_count, min_count, sum_count, avg_count, goodness1, goodness2, goodness3);
            if (good_new == 0) printf("DEBUG: goodness values %ld %f %f %f\n",
                line_number, goodness1,goodness2,goodness3);
        }

        //  5. Save the CHA number in the cha_by_page[page][line] array
        //  test to see if more than one CHA reports > 0.95*NFLUSHES events

        found = 0;
        old_cha = -1;
        int  min_counts = (NFLUSHES*19)/20;
        for (int tile= 0; tile<NUM_CHA_USED; tile++){
            if (cha_counts[tile][1][1] - cha_counts[tile][1][0] >= min_counts){
                old_cha = cha_by_page[page_number][line_number];
                cha_by_page[page_number][line_number] = tile;
                found++;
                if(VERBOSE){
                    if (found > 1) {
                        printf("WARNING: Multiple (%d) CHAs found using counter 1 for cache line %ld, index %ld: old_cha %d new_cha %d\n",found,line_number,page_base_index+line_number*8,old_cha,cha_by_page[page_number][line_number]);
                    }
                }
            }
        }

        if(found == 0){
            good_old = 0; // 检查最大值是否合理
            if(VERBOSE){
                printf("WARNING: no CHA entry has been found for line %ld!\n",line_number);
                printf("DEBUG dump for no CHA found\n");
                for (int tile=0; tile<NUM_CHA_USED; tile++) {
                    printf("CHA %d LLC_LOOKUP.READ          delta %ld\n",tile,(cha_counts[tile][1][1]-cha_counts[tile][1][0]));
                }
            }
        }
        else if(found ==1){
            good_old =1; 
        }
        else{
            good_old = 0;
            if(VERBOSE){
                printf("DEBUG dump for multiple CHAs found\n");
                for (int tile=0; tile<NUM_CHA_USED; tile++) {
                    printf("CHA %d LLC_LOOKUP.READ          delta %ld\n",tile,(cha_counts[tile][1][1]-cha_counts[tile][1][0]));
                }
            }
        }
        good = good_new * good_old;  // trigger a repeat if either the old or new tests failed
    }
    while(good == 0);
    if(VERBOSE){
        printf("DEBUG: CHA is %d\n", cha_by_page[page_number][line_number]);
    }

    return function_total_try;
}
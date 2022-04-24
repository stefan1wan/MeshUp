#include "utils_calc.h"


extern void hugepage2cha(double*array,uint64_t len, double**page_pointers, int8_t cha_by_page[NUM_CHA_USED][Lines_per_PAGE], uint64_t*paddr_by_page);
extern unsigned long long get_pagemap_entry( void * va );

static inline uint64_t rdtscp(){
	uint64_t rax,rdx;
	asm volatile ( "rdtscp\n" : "=a" (rax), "=d" (rdx)::"%rcx","memory");
	return (rdx << 32) | rax;
}

double * page_pointers[NUMPAGES]; 
uint64_t paddr_by_page[NUMPAGES];
int8_t cha_by_page[NUMPAGES][Lines_per_PAGE];  // L3 numbers for each of the 32,768 cache lines in each of the first PAGES_MAPPED 2MiB pages
// uint64_t cachelines_by_cha[NUM_CHA_USED][NUMPAGES* Lines_per_PAGE];

uint64_t** cachelines_by_cha_phy; 
uint64_t* cacheline_nums_of_cha;

int main(){

    uint64_t TIME_1 = rdtscp();
    uint64_t len = NUMPAGES * MYPAGESIZE;
    double* array = (double*)  mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB | MAP_HUGE_1GB, -1, 0 );
    if (array == (void *)(-1)) {
        perror("ERROR: mmap of array a failed! ");
        exit(1);
    }

    size_t arraylen = len/sizeof(double);
    for(uint64_t j=0;j<arraylen;j++){
        array[j]=1.0;
    }
    uint64_t TIME_2 = rdtscp();
    printf("Map Hugepage(s): %lf\n",(TIME_2-TIME_1)/CPU_FREQ);


    hugepage2cha(array,len, page_pointers, cha_by_page, paddr_by_page);
    uint64_t TIME_3 = rdtscp();
    printf("Calc HUGEPAGE<->CHA (s): %lf\n",(TIME_3-TIME_2)/CPU_FREQ);

    cachelines_by_cha_phy = (uint64_t**)malloc(NUM_CHA_USED * sizeof(uint64_t*));

    for(int i=0; i<NUM_CHA_USED; i++){
        cachelines_by_cha_phy[i] = (uint64_t*)malloc(NUMPAGES*Lines_per_PAGE*sizeof(uint64_t));
        if (cachelines_by_cha_phy[i] == (void *)(-1)) {
            perror("ERROR: mmap of array a failed! ");
            exit(1);
        }
        memset(cachelines_by_cha_phy[i], '\x00',NUMPAGES*Lines_per_PAGE*sizeof(uint64_t));
    }
    cacheline_nums_of_cha = (uint64_t*)malloc(NUM_CHA_USED*sizeof(uint64_t));
    memset(cacheline_nums_of_cha, '\x00', NUM_CHA_USED*sizeof(uint64_t));

    for(uint64_t page_number = 0; page_number < NUMPAGES; page_number++){
        for(uint64_t line_number=0; line_number < Lines_per_PAGE; line_number++){

            int cha = cha_by_page[page_number][line_number];
            int current = cacheline_nums_of_cha[cha];

            uint64_t physical_memory = paddr_by_page[page_number] + line_number*0x40 ;

            cachelines_by_cha_phy[cha][current] = physical_memory;
            
            cacheline_nums_of_cha[cha]++;
        }
    }

    if(VERBOSE2){
        for(int i=0; i<NUM_CHA_USED; i++){
            int length = cacheline_nums_of_cha[i];
            printf("\nvitual address of cha: %d, num: %d\n", i, length);
            for(uint64_t j=0; j<length; j++){
                printf("0x%lx\t", cachelines_by_cha_phy[i][j]);
                if((j+1)%10==0) putchar('\n');
            }
            putchar('\n');
        }
    }
    uint64_t TIME_4 = rdtscp();
    printf("Get CHA<->CACHELINE (s): %lf\n",(TIME_4-TIME_3)/CPU_FREQ);

    for(int i=0; i<NUM_CHA_USED; i++){
        char cha_name[24];
        sprintf(cha_name,"./phy_addr_by_cha/cha%d",i);
        FILE *fp = fopen(cha_name, "wb");
        for(int j=0; j<cacheline_nums_of_cha[i]; j++){
            printf("%lx\n", cachelines_by_cha_phy[i][j]);
            fwrite(&cachelines_by_cha_phy[i][j], sizeof(uint64_t),1,fp);
        }
        fclose(fp);
    }
    return 0;
}
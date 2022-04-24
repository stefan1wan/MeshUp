
#include "utils.h"

double * page_pointers[NUMPAGES];  
uint64_t paddr_by_page[NUMPAGES];   

uint64_t* cacheline_nums_of_cha; 
uint64_t** cachelines_by_cha_phy;
uint64_t** cachelines_by_cha; 

long cha_counts[NUM_CHA_BOXES][NUM_CHA_COUNTERS][2]; 

extern unsigned long long get_pagemap_entry( void * va );


void Init_V(){
    cachelines_by_cha = (uint64_t**)malloc(NUM_CHA_USED * sizeof(uint64_t));
    for(int i=0; i<NUM_CHA_USED; i++){
        cachelines_by_cha[i] = (uint64_t*)malloc(NUMPAGES*Lines_per_PAGE*sizeof(uint64_t));
        memset(cachelines_by_cha[i], '\x00', NUMPAGES*Lines_per_PAGE*sizeof(uint64_t));
    }

    cachelines_by_cha_phy = (uint64_t**)malloc(NUM_CHA_USED * sizeof(uint64_t));
    for(int i=0; i<NUM_CHA_USED; i++){
        cachelines_by_cha_phy[i] = (uint64_t*)malloc(NUMPAGES*Lines_per_PAGE*sizeof(uint64_t));
        memset(cachelines_by_cha_phy[i], '\x00', NUMPAGES*Lines_per_PAGE*sizeof(uint64_t));
    }

    cacheline_nums_of_cha = (uint64_t*)malloc(NUM_CHA_USED*sizeof(uint64_t));
    memset(cacheline_nums_of_cha, '\x00', NUM_CHA_USED*sizeof(uint64_t));

    uint64_t TIME_1 = rdtscp();
    uint64_t len = NUMPAGES * MYPAGESIZE;
    uint64_t* array = (uint64_t*)  mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB | MAP_HUGE_1GB, -1, 0);
    if (array == (void *)(-1)) {
        perror("ERROR: mmap of array a failed! ");
        exit(1);
    }
    size_t arraylen = len/sizeof(uint64_t);
    for(uint64_t j=0;j<arraylen;j++){
        array[j]=101;
    }
    uint64_t TIME_2 = rdtscp();
    printf("Map Hugepage(s): %lf\n",(TIME_2-TIME_1)/CPU_FREQ);

    // initialize page_pointers to point to the beginning of each page in the array
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

    uint64_t TIME_3 = rdtscp();
    printf("HUGEPAGE: VA->PA(s): %lf\n",(TIME_3-TIME_2)/CPU_FREQ);
}

uint64_t find_cachelines(uint64_t index, int cha, uint64_t* cachelines, int N){
    long cachelines_found=0;
    uint64_t*  one_cachelines = (uint64_t*) malloc(N*sizeof(uint64_t));
    uint64_t* zero_cachelines = (uint64_t*) malloc(N*sizeof(uint64_t));
    int one_num=0, zero_num=0;
    
    uint64_t total = cacheline_nums_of_cha[cha];
    for(int i=0; i < total; i++){
        uint64_t phy = cachelines_by_cha_phy[cha][i];
        uint64_t index_phy = (phy>>6)&0x3ff;
        if(index_phy==index){ 
            uint64_t a =phy&0x10000?1:0;
            if(a){
                one_cachelines[one_num] = cachelines_by_cha[cha][i];
                one_num++;
            }else{
                zero_cachelines[zero_num] = cachelines_by_cha[cha][i];
                zero_num++;
            }
            cachelines_found++;
        }
        if(cachelines_found==2*N)
            break;
    }

    uint64_t one=0, zero=0, q=0;
    while(one<one_num && zero<zero_num){
        if(one<=zero){
            cachelines[q]= one_cachelines[one];
            one++;
        }else{
            cachelines[q]= zero_cachelines[zero];
            zero++;
        }
        q++;
        // printf("%lx\n",q);
        if(q==N)
            break;
    }

    free(one_cachelines);
    free(zero_cachelines);

    return q;
}


void read_phyaddr(int cha){
    char cha_name[50];
    sprintf(cha_name,"./phy_addr_by_cha/cha%d",cha);
    FILE *fp = fopen(cha_name, "rb");
    struct stat statbuf;
    stat(cha_name, &statbuf);
    uint64_t size=statbuf.st_size;

    assert(size%8==0);
    size/=sizeof(uint64_t);

    cacheline_nums_of_cha[cha] = size;

    for(int j=0; j<size; j++){
        fread(&cachelines_by_cha_phy[cha][j], sizeof(uint64_t),1,fp);
    }
    fclose(fp);
    return;
}


uint64_t phy_2_vitual(uint64_t phy){ 
    uint64_t huge_virtual = 0;
    uint64_t huge_phy=0;
    for(int i=0; i<NUMPAGES; i++){
        if(phy>= paddr_by_page[i] && phy-paddr_by_page[i] < MYPAGESIZE){
            huge_virtual = page_pointers[i];
            huge_phy = paddr_by_page[i];
            break;
        }
    }

    if(huge_virtual==0){
        puts("Mistake phy address");
        exit(0);
    }
    uint64_t vitual = huge_virtual + phy - huge_phy;
    return vitual;
}

void Generate_head_file(uint64_t* cachelines, int total_found, const char* filename){
    FILE * pFile = fopen (filename,"w");
    char DEFINE[100];
    for(long i=0; i<total_found; i++){
        fprintf(pFile,"#define READ_POS_%ld 0x%lx \n" ,i, cachelines[i]);
    }
    fprintf(pFile, "#define FETCH_CACHELINE(x) \\\n");
    for(long i=0; i<total_found; i++){
        fprintf(pFile,"local_sum += *(uint64_t *)(READ_POS_%ld); \\\n" ,i);
    }
    fprintf(pFile, "\n");
    fclose(pFile);
}


void Generate_module_file(int id, char* source_filename){
    FILE *rFile = fopen("hard_code_module_template.c","r");
    fseek (rFile , 0 , SEEK_END);
    long lSize = ftell (rFile);
    rewind (rFile);
    char* content=(char*)malloc(lSize);
    fread(content,1, lSize,rFile);

    FILE * pFile = fopen (source_filename,"w");
    fprintf(pFile, content,id,id,id);
    fclose(pFile);
    free(content);
}

void Generate_target_module_file(int id, char* source_filename){
    FILE *rFile = fopen("target_module_template.c","r");
    fseek (rFile , 0 , SEEK_END);
    long lSize = ftell (rFile);
    rewind (rFile);
    char* content=(char*)malloc(lSize+10);
    memset(content,0, lSize+10);
    fread(content, 1, lSize, rFile);

    FILE * pFile = fopen (source_filename,"w");
    fprintf(pFile, content, id, id, id);
    fclose(pFile);
    free(content);
}

void *Compile_Load_SO(void* h, const char* source_file, const char* target_file, const char* func_name){
    char cmd[0x100];
    snprintf(cmd, 0xff, "gcc -shared -fPIC -o %s %s", target_file, source_file);
    system(cmd);

    h = dlopen(target_file, RTLD_NOW|RTLD_GLOBAL);
    if (!h) {
        printf("dlopen error:%s\n", dlerror());
        exit(1);
    }
    void (*func_pr)(void);
    func_pr = dlsym(h, func_name);
    if (dlerror() != NULL) {
        printf("dlsym m_pr error:%s\n", dlerror());
        exit(2);
    }
    return (void*)func_pr;
}
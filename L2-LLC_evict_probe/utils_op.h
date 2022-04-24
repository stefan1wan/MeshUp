/* 定义了一些基本操作和全局变量, 一个binary只允许一个源文件引用这个头文件 */
#include "utils.h"

double * page_pointers[NUMPAGES];    // 每个大页的虚拟地址
uint64_t paddr_by_page[NUMPAGES];    // 每个大页的物理地址

uint64_t* cacheline_nums_of_cha; //每个cha对应cacheline的个数
uint64_t** cachelines_by_cha_phy; //每个cha对应cacheline的物理地址
uint64_t** cachelines_by_cha; //每个cha对应cacheline的虚拟地址

long cha_counts[NUM_CHA_BOXES][NUM_CHA_COUNTERS][2]; // 通过msr监控PMON事件

extern unsigned long long get_pagemap_entry( void * va );


void Init_V(){
    // 0. 每个cha对应cacheline相关的结构;
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

    // 1. map 2G(1024*2M)的大页 并写1初始化, 初始化msr
    uint64_t TIME_1 = rdtscp();
    uint64_t len = NUMPAGES * MYPAGESIZE;
    uint64_t* array = (uint64_t*)  mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB | MAP_HUGE_1GB, -1, 0);
    if (array == (void *)(-1)) {
        perror("ERROR: mmap of array a failed! ");
        exit(1);
    }
    // 给开出的数组赋值;
    size_t arraylen = len/sizeof(uint64_t);
    for(uint64_t j=0;j<arraylen;j++){
        array[j]=101;
    }
    uint64_t TIME_2 = rdtscp();
    printf("Map Hugepage(s): %lf\n",(TIME_2-TIME_1)/CPU_FREQ);

    // 2. initialize page_pointers to point to the beginning of each page in the array
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

// 给定L2 index,CHA, 旬斋符合条件的cacheline, 并存储在cachelines位置
uint64_t find_cachelines(uint64_t index, int cha, uint64_t* cachelines, int N){
    // index:指定L2的index  , cha: LLC所在的核, N: 所需要的个数
    long cachelines_found=0;
    uint64_t*  one_cachelines = (uint64_t*) malloc(N*sizeof(uint64_t));
    uint64_t* zero_cachelines = (uint64_t*) malloc(N*sizeof(uint64_t));
    int one_num=0, zero_num=0;
    
    uint64_t total = cacheline_nums_of_cha[cha];
    for(int i=0; i < total; i++){
        uint64_t phy = cachelines_by_cha_phy[cha][i];
        uint64_t index_phy = (phy>>6)&0x3ff;
        if(index_phy==index){ // 如果物理地址index相同，就使用虚拟地址
            uint64_t a =phy&0x10000?1:0;
            if(a){
                one_cachelines[one_num] = cachelines_by_cha[cha][i];
                one_num++;
            }else{
                zero_cachelines[zero_num] = cachelines_by_cha[cha][i];
                zero_num++;
            }
            cachelines_found++;
            // for debug
            // printf("index_phy: %lx\n", index_phy);
            // printf("found: %lx\n", cachelines_by_cha[cha][i]);
            // printf("phy: %lx\n",   cachelines_by_cha_phy[cha][i]);
            // printf("LLC slice: %lx\n", a);
        }
        if(cachelines_found==2*N)
            break;
    }
    // printf("one_num:%ld, zero_num: %ld\n", one_num, zero_num);
    // 进行微调, 将分布在两个LLC set中的靠在一起;
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
    // for(int i=0; i<N; i++){
    //     printf("%d : %#lx \t", i,cachelines[i]);
    // }
    // puts("");

    free(one_cachelines);
    free(zero_cachelines);
    //printf("cache line found:%ld\nphy_index:%lx, cha:%d\n", q, index, cha);
    return q;
}

// 给定CHA, 读出其所有物理地址;(这个地址已经由calc_map计算好)
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
        //printf("%lx\n", cachelines_by_cha_phy[cha][j]);
    }
    fclose(fp);
    return;
}

// 给定物理地址, 先确定其对应大页; 根据大页虚拟地址, 寻找phy对应的虚拟地址;
uint64_t phy_2_vitual(uint64_t phy){ //double * page_pointers[NUMPAGES]; 
    // 1. 搜索每个大页的物理地址
    uint64_t huge_virtual = 0;
    uint64_t huge_phy=0;
    for(int i=0; i<NUMPAGES; i++){
        //printf("phy: %lx,H_paddr: %lx, H_vaddr:%lx \n", phy, paddr_by_page[i], page_pointers[i]);
        if(phy>= paddr_by_page[i] && phy-paddr_by_page[i] < MYPAGESIZE){
            huge_virtual = page_pointers[i];
            huge_phy = paddr_by_page[i];
            break;
        }
    }
    // printf("Phy: %lx \n", phy);
    // printf("Phy_huge: %lx \n", paddr_by_page[0]);
    // printf("Vir_huge_page: %lx \n", page_pointers[0]);
    if(huge_virtual==0){
        puts("Mistake phy address");
        exit(0);
    }
    uint64_t vitual = huge_virtual + phy - huge_phy;
    return vitual;
}


/*硬编码相关需求函数*/
void Generate_head_file(uint64_t* cachelines, int total_found, const char* filename){
    FILE * pFile = fopen (filename,"w");
    char DEFINE[100];
    for(long i=0; i<total_found; i++){
        fprintf(pFile,"#define READ_POS_%ld 0x%lx \n" ,i, cachelines[i]);
    }
    fprintf(pFile, "#define FETCH_CACHELINE(x) \\\n");
    for(long i=0; i<total_found; i++){
        fprintf(pFile,"local_sum += *(uint64_t *)(READ_POS_%ld); \\\n" ,i);
        //fprintf(pFile,"*(uint64_t *)(READ_POS_%ld) = x * 100; \\\n" , i);
    }
    fprintf(pFile, "\n");
    fclose(pFile);
}


void Generate_module_file(int id, char* source_filename){
    // 从模版中读取文件
    FILE *rFile = fopen("hard_code_module_template.c","r");
    fseek (rFile , 0 , SEEK_END);
    long lSize = ftell (rFile);
    rewind (rFile);
    char* content=(char*)malloc(lSize);
    fread(content,1, lSize,rFile);

    // 生成文件
    FILE * pFile = fopen (source_filename,"w");
    fprintf(pFile, content,id,id,id);
    fclose(pFile);
    free(content);
}

void Generate_target_module_file(int id, char* source_filename){
    // 从模版中读取文件
    FILE *rFile = fopen("target_module_template.c","r");
    fseek (rFile , 0 , SEEK_END);
    long lSize = ftell (rFile);
    rewind (rFile);
    char* content=(char*)malloc(lSize+10);
    memset(content,0, lSize+10);
    fread(content, 1, lSize, rFile);

    // 生成文件
    FILE * pFile = fopen (source_filename,"w");
    fprintf(pFile, content, id, id, id);
    fclose(pFile);
    free(content);
}

void *Compile_Load_SO(void* h, const char* source_file, const char* target_file, const char* func_name){
    // 生成动态库;
    char cmd[0x100];
    snprintf(cmd, 0xff, "gcc -shared -fPIC -o %s %s", target_file, source_file);
    system(cmd);

    // 加载编译好的动态库;
    h = dlopen(target_file, RTLD_NOW|RTLD_GLOBAL);
    if (!h) {
        printf("dlopen error:%s\n", dlerror());
        exit(1);
    }
    // 加载动态库中的congest函数;
    void (*func_pr)(void);
    func_pr = dlsym(h, func_name);
    if (dlerror() != NULL) {
        printf("dlsym m_pr error:%s\n", dlerror());
        exit(2);
    }
    return (void*)func_pr;
}
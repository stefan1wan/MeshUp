#include <stdio.h>
#include <dlfcn.h>
#include "utils_op.h"

//104机器的信息
int CHA_CPU_LIST[NUM_CHA_USED][2] = {{0,0},{1,12},{2,6},{3,18},{4,1},{5,13},{6,7},{7,19},{8,2},{9,14},{10,8},{11,20},{12,3},{13,15},{14,9},{15,21},{16,4},{17,16},{18,10},{19,22},{20,5},{21,17},{22,11},{23,23}};

#define FLOW_MAKE_PAIR_NUM 2
#define TARGET_NUM 0 // In this experiment, the target is java;

uint64_t L2_INDEX_LIST[L2_INDEX_NUM]; //= {0x2a7,0x2a8,0xa9,0x2aa,0x0a1,0x1b2,0xc2,0xde,0x2ef,0xf0};
int FLOW_MADE_LIST[FLOW_MAKE_PAIR_NUM][2];  // = {{11,21},{21,11}};
int TARGET_LIST[TARGET_NUM][2] = {{4, 22}};

void log_local(thread_args *thread_args_list);

int main(int argc, char *argv[]){
// 0.0 检查变量
    if(argc < 5){
        printf("Usage: ./L2_evict_congest_args [core pair list]\n");
        exit(0);
    }

// 0.1 通过参数解析出index
    for(int i=0; i<L2_INDEX_NUM; i++){
        L2_INDEX_LIST[i] = (uint64_t)atol(argv[i+1]);
    }

// 0.2 通过参数解析出probe的core
    for(int i=0; i<FLOW_MAKE_PAIR_NUM; i++){
        FLOW_MADE_LIST[i][0] = atoi(argv[L2_INDEX_NUM+1 + i*2]);
        FLOW_MADE_LIST[i][1] = atoi(argv[L2_INDEX_NUM+1 + i*2 + 1]);
    }
    for(int i=0; i< FLOW_MAKE_PAIR_NUM; i++){
        printf("attack: %d -> %d\n", FLOW_MADE_LIST[i][0], FLOW_MADE_LIST[i][1]);
    }

// 0. 初始化,求得必要变量的值
    Init_V();

// 1. 从计算好的文件中读取需要的CHA对应的cachelines, 并计算其虚拟地址
    for(int i=0; i<FLOW_MAKE_PAIR_NUM; i++){
        int cha_id = FLOW_MADE_LIST[i][1];
        // 读取物理地址
        //printf("chaid:%d\n", cha_id);
        read_phyaddr(cha_id);
        // 转化为虚拟地址
        for(uint64_t i=0; i< cacheline_nums_of_cha[cha_id]; i++){
            uint64_t phy = cachelines_by_cha_phy[cha_id][i];
            //printf("phy:%lx\n", phy);
            uint64_t vitual = phy_2_vitual(phy);
            //printf("GOT vitual: %lx\n", vitual);
            cachelines_by_cha[cha_id][i] = vitual;
        }
    }

    // 计算target
    for(int i=0; i<TARGET_NUM; i++){
        int cha_id = TARGET_LIST[i][1];
        // 读取物理地址
        //printf("chaid:%d\n", cha_id);
        read_phyaddr(cha_id);
        // 转化为虚拟地址
        for(uint64_t i=0; i< cacheline_nums_of_cha[cha_id]; i++){
            uint64_t phy = cachelines_by_cha_phy[cha_id][i];
            //printf("phy:%lx\n", phy);
            uint64_t vitual = phy_2_vitual(phy);
            //printf("GOT vitual: %lx\n", vitual);
            cachelines_by_cha[cha_id][i] = vitual;
        }
    }



// 2. 对于每条路径, 找到针对特定index和特定cha的cacheline
    pthread_t thread_id_list[FLOW_MAKE_PAIR_NUM];
    thread_args thread_args_list[FLOW_MAKE_PAIR_NUM];

    pthread_t target_thread_id_list[TARGET_NUM];
    thread_args target_thread_args_list[TARGET_NUM];

    for(int i=0; i<FLOW_MAKE_PAIR_NUM; i++){
        int cha_id_0 = FLOW_MADE_LIST[i][0];
        int cha_id_1 = FLOW_MADE_LIST[i][1];
        int cpu_id = CHA_CPU_LIST[cha_id_0][1];
        thread_args_list[i].cpuid = cpu_id;

        uint64_t *cachelines  = (uint64_t*)malloc(CACHELINES_READ*(L2_INDEX_NUM+1)*sizeof(uint64_t));
        long total_found = 0;
        for(int j=0; j<L2_INDEX_NUM; j++){
            //printf("index:%ld, cha_id:%d\n",L2_INDEX_LIST[j], cha_id_1);
            //uint64_t index = cpu_id*L2_INDEX_NUM + L2_INDEX_LIST[j];
            uint64_t index = L2_INDEX_LIST[j];
            find_cachelines(index, cha_id_1, cachelines+ j*CACHELINES_READ, CACHELINES_READ);
            total_found+=CACHELINES_READ;
        }
        //  TODO: 改变cachelines排序顺序, 使各个index错开
        thread_args_list[i].cachelines = cachelines;
        thread_args_list[i].cachelines_found = total_found;

        // 设置timeline记录信息;
        uint64_t* time_line = (uint64_t *)malloc(EXCHAGE_RATE*sizeof(uint64_t));
        //uint64_t* time_line = (uint64_t *) mmap(NULL, EXCHAGE_RATE*sizeof(uint64_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB | MAP_HUGE_1GB, -1, 0);
        if (time_line == (void *)(-1)) {
            perror("ERROR: mmap of array a failed! ");
            exit(1);
        }

        uint64_t *p = (uint64_t* )malloc(sizeof(uint64_t));
        thread_args_list[i].timeline_local =time_line;
        thread_args_list[i].timeline_p = p;

    }


    //puts("hello world");
    for(int i=0; i<TARGET_NUM; i++){
        int cha_id_0 = TARGET_LIST[i][0];
        int cha_id_1 = TARGET_LIST[i][1];
        int cpu_id = CHA_CPU_LIST[cha_id_0][1];
        target_thread_args_list[i].cpuid = cpu_id;

        uint64_t *cachelines  = (uint64_t*)malloc(CACHELINES_READ*(L2_INDEX_NUM+1)*sizeof(uint64_t));
        long total_found = 0;
        for(int j=0; j<L2_INDEX_NUM; j++){
            //printf("index:%ld, cha_id:%d\n",L2_INDEX_LIST[j], cha_id_1);
            uint64_t index = cpu_id*L2_INDEX_NUM + L2_INDEX_LIST[j];
            //printf("index: %#lx\n", index);
            find_cachelines(index, cha_id_1, cachelines+ j*CACHELINES_READ, CACHELINES_READ);
            total_found+=CACHELINES_READ;
        }
        //  TODO: 改变cachelines排序顺序, 使各个index错开
        target_thread_args_list[i].cachelines = cachelines;
        target_thread_args_list[i].cachelines_found = total_found;
    }


    
// 3 生成模块需要的hard_code.h头文件, 并编译加载SO库, 取得函数指针;
    void *H_list[FLOW_MAKE_PAIR_NUM];
    void *FUNC_POINTER[FLOW_MAKE_PAIR_NUM];

    void *TARGET_H_list[TARGET_NUM];
    void *TARGET_FUNC_POINTER[TARGET_NUM];

    for(int i=0; i<FLOW_MAKE_PAIR_NUM; i++){
        char head_filename[0x100];
        char target_filename[0x100];
        char func_name[0x100];
        char source_filename[0x100];

        snprintf(head_filename,0xff, "hard_code_%d.h",i);
        snprintf(source_filename,0xff,"hard_code_module_%d.c", i);
        snprintf(target_filename,0xff,"./hard_code_module_%d.so",i);
        snprintf(func_name,0xff,"congest_%d", i);
        
        Generate_head_file(thread_args_list[i].cachelines, thread_args_list[i].cachelines_found, head_filename);
        Generate_module_file(i, source_filename);

        FUNC_POINTER[i] = Compile_Load_SO(H_list[i], source_filename, target_filename, func_name);
    }

    for(int i=0; i<TARGET_NUM; i++){
        char head_filename[0x100];
        char target_filename[0x100];
        char func_name[0x100];
        char source_filename[0x100];

        snprintf(head_filename,0xff, "target_%d.h",i);
        snprintf(source_filename,0xff,"target_module_%d.c", i);
        snprintf(target_filename,0xff,"./target_module_%d.so",i);
        snprintf(func_name,0xff,"target_%d", i);
        
        Generate_head_file(target_thread_args_list[i].cachelines, target_thread_args_list[i].cachelines_found, head_filename);
        Generate_target_module_file(i, source_filename);

        TARGET_FUNC_POINTER[i] = Compile_Load_SO(TARGET_H_list[i], source_filename, target_filename, func_name);
    }

// 6.1 准备记录PMON读数;
    // int fd;
    // uint64_t msr_val, msr_num;
    // // 通过msr控制ctrl
    // fd = prepare_msr();
    // // 记录cha;
    // // ------------------- read the origin values of the CHA mesh counters ----------------
    // for (int tile=0; tile<NUM_CHA_USED; tile++) {
    //     for (int counter=0; counter<4; counter++) {
    //         msr_num = 0xe00 + 0x10*tile + 0x8 + counter;
    //         pread(fd,&msr_val,sizeof(msr_val),msr_num);
    //         cha_counts[tile][counter][0] = msr_val;
    //     }
    // }

// 4. 流量的制造
    uint64_t TIME_5, TIME_6;
    
    TIME_5 = rdtscp(); 
    // 运行线程;
    for(int i=0; i<FLOW_MAKE_PAIR_NUM; i++)
        pthread_create(&(thread_id_list[i]), NULL, FUNC_POINTER[i], (void*)&(thread_args_list[i])); 
    
    for(int i=0; i<TARGET_NUM; i++)
        pthread_create(&(target_thread_id_list[i]), NULL, TARGET_FUNC_POINTER[i], (void*)&(target_thread_args_list[i])); 

    // 中止线程;
    for(int i=0; i<FLOW_MAKE_PAIR_NUM; i++)
        pthread_join(thread_id_list[i], NULL);

    for(int i=0; i<TARGET_NUM; i++)
        pthread_join(target_thread_id_list[i], NULL);
    TIME_6 = rdtscp();

    // for(int i=0; i<FLOW_MAKE_PAIR_NUM; i++){
    //     //dlclose(H_list[i]);
    //     //dlerror();
    // }

// 6.2 ------------------- read the final values of the CHA mesh counters ----------------
    // for (int tile=0; tile<NUM_CHA_USED; tile++) {
    //     for (int counter=0; counter<4; counter++) {
    //         msr_num = 0xe00 + 0x10*tile + 0x8 + counter;
    //         pread(fd,&msr_val,sizeof(msr_val),msr_num);
    //         cha_counts[tile][counter][1] = msr_val;
    //     }
    // }
    // print_cha_counters();

// 统计工作时间
    uint64_t cycles = TIME_6 - TIME_5;
    printf("TOTAL CYCLES: %ld\n", cycles);
    printf("TIME: %lf\n", cycles/CPU_FREQ);

// 7. 统计统计所有打点;
	puts("Detect done! Start analyzing..."); 
    log_local(thread_args_list);
    return 0;
}

void log_local(thread_args *thread_args_list){
    for(int i=0; i<FLOW_MAKE_PAIR_NUM; i++){
        uint64_t *time_line = thread_args_list[i].timeline_local ;
        uint64_t *p         = thread_args_list[i].timeline_p ;
        uint64_t N = *p;

        char log_name[0x100];
        snprintf(log_name, 0xff, "time_line_%d.log", i);
        FILE *fp=fopen(log_name,"w");
        printf("%ld\n", time_line[0]);
        printf("%ld\n", time_line[N-1]);

        double total_time=0;
        for(uint64_t k=0; k<N; k++){
            // double t = (all_line[k+1] - all_line[k])/CPU_FREQ;
            // fprintf(fp,"%.15lf\n", t);
            fwrite(&(time_line[k]), sizeof(uint64_t), 1, fp);
            // total_time += t;
        }
        fclose(fp);
    }
}

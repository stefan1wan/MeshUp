CC = gcc
CFLAGS = -lpthread -ldl

default: L2_evict_congest_args calc_map

L2_evict_congest_args: L2_evict_congest_args.o va2pa_lib.o
	$(CC) L2_evict_congest_args.o va2pa_lib.o -o $@ $(CFLAGS)


calc_map: calc_map.o get_hugepage2cha_map.o va2pa_lib.o
	mkdir -p phy_addr_by_cha
	gcc calc_map.o get_hugepage2cha_map.o va2pa_lib.o -o calc_map 


%.o:	%.c
	$(CC) -c $< -o $@ 

clean:
	rm L2_evict_congest_args calc_map *.o 
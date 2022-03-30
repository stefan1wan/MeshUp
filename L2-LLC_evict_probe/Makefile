CC = gcc
CFLAGS = -lpthread -ldl

default: L2_evict_congest_args

L2_evict_congest_args: L2_evict_congest_args.o va2pa_lib.o
	$(CC) L2_evict_congest_args.o va2pa_lib.o -o $@ $(CFLAGS)

%.o:	%.c
	$(CC) -c $< -o $@ 

clean:
	rm L2_evict_congest_args *.o 
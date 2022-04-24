#!/usr/bin/python3
import os
import random

def gen_random_index(NUM=10):
    indexes= []
    for i in range(NUM):
        ran = random.randint(0, 0x3ff)
        while ran in indexes:
            ran = random.randint(0, 0x3ff)
        indexes.append(ran)
    return indexes

def attackProbe(L2_index, attack_list):
    X = []
    for x,y in attack_list:
        X.append(x)
        X.append(y)
    
    # print(target_list)
    # print(X)
    cmd = "./L2_evict_congest_args "+  " " + " ".join([str(x) for x in L2_index]) +" " + " ".join([str(x) for x in X])

    print(cmd)
    os.system(cmd)

if __name__ == "__main__":
    L2_index = gen_random_index()
    attack_list = [[2, 5],[5, 2]] # the cha ids
    attackProbe(L2_index, attack_list)
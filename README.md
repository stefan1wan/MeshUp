# MeshUp: Stateless Cache Side-channel Attack on CPU Mesh

## Introduction
This is the source code of our paper
[MeshUp: Stateless Cache Side-channel Attack on CPU Mesh](https://www.computer.org/csdl/proceedings-article/sp/2022/131600b396/1A4Q4hofHe8), which will be published on [IEEE S&P 2022](https://www.ieee-security.org/TC/SP2022/).
This repo is the cross-core attack upon one CPU mesh. [here](TBD) is the cross-CPU attack upon UPI.

## Mesh NOC Reverse Engineering
Our cross-core experiment envrionment is [Intel Xeon 8260](https://ark.intel.com/content/www/us/en/ark/products/192474/intel-xeon-platinum-8260-processor-35-75m-cache-2-40-ghz.html), which is a 28-tile CPU chip with 4 tile disabled. 

The layout is as following:

| UPI |PCIE|PCIE|RLINK|UPI2|PCIE|
|--|--|--|--|--|--|
|0|4|9|14|19|24|
|IMC0|5|10|15|20|IMC1|
|1|6|11|16|21|25|
|2|7|12|17|22|26|
|3|8|13|18|23|27|

Firstly, we should confirm which 4 cores are disabled. We are able to do that by reading msr register CAPID6, like this:
```
> sudo setpci -s 16:1e.3 0x9c.l
07dffff3
```
Then CPAID6[27:0] is 0x7dffff3(0111, 1101, 1111, 1111, 1111, 1111, 0011), we could learn that the disabled tiles' ID are 2, 3, 21, and 27.

By the way, the ID of tiles and CHAs are growing from top to down and left to right. As a result, CHA 2 is in tile 4. In this way, we could map the CHA ID with tile ID for all CHAs, like this:
| UPI |PCIE|PCIE|RLINK|UPI2|PCIE|
|--|--|--|--|--|--|
|0|2|7|12|17|21|
|IMC0|3|8|13|18|IMC1|
|1|4|9|14|X|22|
|X|5|10|15|19|23|
|X|6|11|16|20|X|

We also need the relationships between core ID(core ID is the physical core ID in OS) and CHA ID. We found this information could be got by a PMU event - LCORE_PMA GV (Core Power Management Agent Global system state Value). Firstly, we bind a process to a core(ID=X), and do a lot of operations with that process(e.g. access a large volume of memory.) At the same time, we monitor the LCORE_PMA GV counter of every CHA. We could observe that the counter on one CHA(ID=Y) is higher than others. So we can confirm that core X and CHA Y lay on the same tile because the activities of core X could change the power management state of CHA Y. Repete the above procedure from core 0 to core 23, we could learn the mappings between core and CHA as the following picture shows:
| UPI |PCIE|PCIE|RLINK|UPI2|PCIE|
|--|--|--|--|--|--|
|0,0|2,16|7,19|12,3|17,16|21,17|
|IMC0|3,18|8,2|13,15|18,10|IMC1|
|1,12|4,1|9,14|14,9|X|22,11|
|X|5,13|10,8|15,21|19,22|23,23|
|X|6,7|11,20|16,4|20,5|X|

Code is [here](./MeshReverseEngineering/).

[How to find the disabled tiles](TBD)

## Cross-Core Attack

## Cross-CPU Attack
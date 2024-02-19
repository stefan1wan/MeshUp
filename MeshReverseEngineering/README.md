# Mesh Reverse Engineering

## Usage
```
modprobe msr
make
sudo ./core2cha_layout
```

For example, in our machine `Intel(R) Xeon(R) Platinum 8175M CPU @ 2.50GHz`, the result is as below.
```
cpu 0: cha 0
cpu 1: cha 4
cpu 2: cha 8
cpu 3: cha 12
cpu 4: cha 16
cpu 5: cha 20
cpu 6: cha 2
cpu 7: cha 6
cpu 8: cha 10
cpu 9: cha 14
cpu 10: cha 18
cpu 11: cha 22
cpu 12: cha 1
cpu 13: cha 5
cpu 14: cha 9
cpu 15: cha 13
cpu 16: cha 17
cpu 17: cha 21
cpu 18: cha 3
cpu 19: cha 7
cpu 20: cha 11
cpu 21: cha 15
cpu 22: cha 19
cpu 23: cha 23
```

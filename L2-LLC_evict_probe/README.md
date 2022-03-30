# L2-LLC Evict Probe



### how to use cat to isolate LLC
For example, if victim runs on core 7, this two commands will isolate core 7's LLC:
```
sudo pqos -e "llc:1=0x0001;llc:2=0x07fe;"
sudo pqos -a "llc:1=7;llc:2=0-6,8-24;"
```
Core 7 will use 1 way and other core will use the other ways.

And this command could show the status:
```
sudo pqos -s
```

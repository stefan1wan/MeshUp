#!/usr/bin/python3

import numpy as np
import matplotlib.pyplot as plt
import struct
import os
import sys



def read_records(filename="records.bin"):
    fin = open(filename, 'rb')
    fin.seek(0, os.SEEK_END)
    length = fin.tell()
    fin.seek(0, os.SEEK_SET)
    #print("records length:", length)
    assert length % 0x8 == 0
    records_rdtscp = []

    for i in range(length//0x8):
        r1 = fin.read(8)
        d1 = struct.unpack('1Q', r1)
        records_rdtscp.append(d1[0])
    fin.close()
    return np.array(records_rdtscp)

def smooth_window(data, window_len=25, window="hanning"):
    if data.ndim != 1:
        raise(ValueError, "smooth only accepts 1 dimension arrays.")

    if data.size < window_len:
        raise(ValueError, "Input vector needs to be bigger than window size.")

    if window_len<3:
        return

    if not window in ['flat', 'hanning', 'hamming', 'bartlett', 'blackman']:
        raise(ValueError, "Window is on of 'flat', 'hanning', 'hamming', 'bartlett', 'blackman'")

    s=np.r_[data[window_len-1:0:-1], data, data[-2:-window_len-1:-1]]
    #print(len(s))
    if window == 'flat': #moving average
        w = np.ones(window_len,'d')
    else:
        w = eval('np.'+window+'(window_len)')

    data = np.convolve(w/w.sum(),s, mode='valid')
    return data

def draw_picture(data):
    fig = plt.figure(figsize=(15, 8))
    ax = fig.add_subplot(1, 1, 1)
    ax.plot(range(data.shape[0]), data, linewidth=0.15)
    plt.show()



if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 show_logs.py <trace name>")
    else:
        data = read_records(sys.argv[1])                                                       
        print(len(data))
        data = data[1:] - data[:-1]
        data = smooth_window(np.array(data))
        draw_picture(data)

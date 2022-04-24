#!/bin/bash

if [! -n "$1" ]; then
    echo "./run_target.sh <bind core>"
fi

javac RSAmodpow.java
taskset -c $1 java -verbose:gc -Xmx8G -Xmn6G -Xint RSAmodpow
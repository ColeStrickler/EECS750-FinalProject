#!/bin/bash


for ((i =  165000; i <  1000000; i += 1)); do
    ./ghostrace 20000 100000 2000
done

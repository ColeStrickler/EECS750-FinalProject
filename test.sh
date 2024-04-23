#!/bin/bash


for ((i =  10000; i <  1000000; i += 1)); do
    ./io_driver_com $i 0
done

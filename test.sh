#!/bin/bash

make clean
make driver
make remove
make install

for ((i =  100; i < 1000000; i += 100)); do
    ./io_driver_com $i 0
done

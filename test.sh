#!/bin/bash


for ((i = 21000; i < 30000; i += 10)); do
    ./ghostrace $i 10000
done

#!/bin/bash


for ((i = 39000; i <= 60000; i += 10)); do
    for ((j = 1; j <= 10; j += 1)); do
        ./attacker $i
    done
done

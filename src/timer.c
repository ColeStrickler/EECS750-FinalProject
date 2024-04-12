#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h> // For timeval struct
#include <unistd.h>   // For usleep() function
#include <sched.h>


#define VICTIM_CPU          0



void timer_handler(int signum) {
    if (signum == SIGALRM) {
        printf("Timer expired.\n");
        // Handle timer expiration here
    }
}

int main(int argc, char** argv) {
    cpu_set_t mask;
    CPU_ZERO(&mask);    // Initialize CPU mask to all zeroes
    CPU_SET(VICTIM_CPU, &mask);  

    if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) == -1) {
        perror("sched_setaffinity");
        exit(EXIT_FAILURE);
    }


    struct sigaction sa;
    struct sigevent sev;
    timer_t timerid;
    struct itimerspec its;
    long long unsigned int interval_ns = 1000000000; // 1 second in nanoseconds

    // Setup signal handler
    sa.sa_flags = SA_SIGINFO;
    sa.sa_handler = timer_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // Create timer
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGALRM;
    sev.sigev_value.sival_ptr = &timerid;
    if (timer_create(CLOCK_MONOTONIC, &sev, &timerid) == -1) {
        perror("timer_create");
        exit(EXIT_FAILURE);
    }

    // Set timer interval
    its.it_value.tv_sec = interval_ns / 1000000000; // seconds
    its.it_value.tv_nsec = interval_ns % 1000000000; // nanoseconds
    its.it_interval.tv_sec = interval_ns / 1000000000;
    its.it_interval.tv_nsec = interval_ns % 1000000000;

    // Start the timer
    if (timer_settime(timerid, 0, &its, NULL) == -1) {
        perror("timer_settime");
        exit(EXIT_FAILURE);
    }

    // Wait for the timer to expire
    while(1) {
        // Do other tasks while waiting for the timer to expire
    }

    return 0;
}
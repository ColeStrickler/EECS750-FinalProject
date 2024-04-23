#ifndef IPI_H
#define IPI_H
#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>
#include <sched.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <linux/membarrier.h>  /* Definition of  MEMBARRIER_*  constants */
#define SYS_membarrier      324
#define VICTIM_CPU          0


void ipi_register();

void ipi_register(unsigned long num_threads);

void pin_cpu(int cpu);

void ipi_storm_thread(void *cpu);

void begin_ipi_storm();
int kill_ipi();

#endif


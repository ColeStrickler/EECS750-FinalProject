#ifndef TIMER_H
#define TIMER_H
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h> // for epoll_create1()
#include <sys/timerfd.h> // for timerfd_*()
#include <sys/select.h> // for select()
#include <unistd.h>
#include <sched.h>
#include "ipi.h"
#define EPOLL_INSTANCES     100
#define FD_PER_EPOLL        500
#define VICTIM_CPU          0

int cleanup(int *epollfds, int **tfdups, int num_epolls, int num_tfdups);

void timer_init(uint32_t);

void timer_start();

void timer_wait();

void timer_cleanup();

#endif


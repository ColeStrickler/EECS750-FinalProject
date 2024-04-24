# EECS 750 - Advanced Operating Systems - Final Project

This project reimplements the techniques shown in the GhostRace paper https://www.vusec.net/projects/ghostrace/.

GhostRace investigates the impact of microarchitectural attacks on synchronization primitives and also the feasibility of their exploitation.


GhostRace Contributions that we reimplemented:
- technique to surgically interrupt arbitrary kernel threads
- technique to arbitrarily extend UAF window
- exploitation of speculative race conditions to leak kernel memory
- investigation of prevalence of SCUAF gadgets in the most recent kernel versions
- Flush+Reload covert channel implemtentation



The GhostRace authors provided a simple POC (https://github.com/vusec/ghostrace), which we extended to utilize all of the forementioned techniques.

# Build
All code can be built via the Makefile. Run test.sh to test driver/

NOTE: you will need to adjust various parameters such as the number of threads and cache line size to make this work on your machine

# Source
There are 4 sections of source code that successively build on top of each other. These roughly mirror the order we went about building out this project.
- flushreloaddemo/ --> This project demonstrates how flush+reload techniques can be used to leak arbitrary process memory
- window/ --> This project demonstrates how to combine high precision timers, timer interrupt extension techniques, and  an IPI storm to arbitrarily unbound interrupt lifetime
- ghostrace/ --> This project combines the two previous projects to implement the GhostRace attack in user space
- driver/ --> This project implements a driver vulnerable to GhostRace and entering kernel mode allows us to use the surgical interruption technique using high precision timers to facilitate GhostRace in the kernel

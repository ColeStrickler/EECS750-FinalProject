# EECS 750 - Advanced Operating Systems - Final Project

This project reimplements the techniques shown in the GhostRace paper https://www.vusec.net/projects/ghostrace/.

GhostRace investigates the impact of microarchitectural attacks on synchronization primitives and also the feasibility of their exploitation.


GhostRace Contributions that we reimplemented:
- technique to surgically interrupt arbitrary kernel threads
- technique to arbitrarily extend UAF window
- exploitation of speculative race conditions to leak kernel memory
- investigation of prevalence of SCUAF gadgets in the most recent kernel versions



The GhostRace authors provided a simple POC (https://github.com/vusec/ghostrace), which we extended to utilize all of the forementioned techniques.



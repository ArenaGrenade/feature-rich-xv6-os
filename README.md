# Better xv6 Operating System

## Changes Done

- Added multiple scheduling algorithms - MLFQ (Multi-level Feedback Queue), FCFS (First Come First Serve), PBS (Priority Based Scheduling) along with the default RR (Round Robin) implementation.

- `ps` user command that displays the process table.

- `time` user command has been added, that when another user command is passed as an argument, it prints the run time and wait time of the process.

- `waitx` system call that returns the total run time and wait time for a process to complete.

- `set_priority` system call can be used to set the priority of a process. Used in PBS.

- `setPriority` user command extends the `set_priority` system call so that a user can run this command to set priorities.

## To Run

### Install Qemu Emulator

> apt-get install qemu

### Build the OS

> make clean

The clean is to ensure that even  if any changes are made, they do get reflected in the executable.

> make qemu [ARGS]

`ARGS` can take multiple values and primarily these setting will turn out very useful:

> - SCHEDULER=(MLFQ/FCFS/PBS/RR)
> - CPUS=1/2/3/...
> - AGETHRES=1/2/3/...

Above `SCHEDULER` is the type of scheduler that needs to be run, this value defaults to `RR` - Round Robin Scheduling. `CPUS` is the number of CPUs that the emulator would run on at peak potential. `AGETHRES` is used in MLFQ scheduling and would be the threshold wait time to decide if a process would get its priority queue elevated or not.

Additionally, one can change the number of queues in MLFQ scheduling using an internal variable defined in `param.h`. Here the lowest priority queue will run a Round Robin Scheduling.

## Report of the different scheduling algorithms

The benchmark used for this report forks 10 processes and in the order of forking, the amount of I/O time or sleep time is increased and the amount to CPU time needed by the process is reduced gradually. This gives a rough idea of how the scheduling algorithms behave and using the `time` user command we can run this and get an average estimate of t he waiting time, run time of an average process that might enter the scheduling process.

It was also observed that having multiple CPUs changed the way that these scheduling algorithms perform, not only from a time taken pointt of view but rather different types of processes performed differently - some better, while some worse. Adding to this the number rof processes currently running also changes the behaviour of the algrithms - this is very evident in the case of MLFQ scheduling.

### Round Robin Scheduling Algorithm (RR)

1. Average wait time = 469.3
2. Standard deviation of wait time = 109.50

### First Come First Serve Scheduling algorithm (FCFS)

1. Average wait time = 314.7
2. Standard deviation of wait time = 182.98

### Multi-Level Feedback Queue Scheduling Algorithm (MLFQ)

1. Average wait time = 297.7
2. Standard deviation of wait time = 194.04

### Priority Based Scheduling Algorithm (PBS)

Higher prioritites were allocated to processes that have larger I/O time or sleep time.

1. Average wait time = 146.4
2. Standard deviation of wait time = 168.75

For the conditions given, we had RR performing the worst of all and PBS performing the best - this applies only the current benchmark code and is in no way representative what the OS might experience normally. As said earlier MLFQ outperforms every else by a large margin at higher number of processes.

## README of the original repository

NOTE: we have stopped maintaining the x86 version of xv6, and switched
our efforts to the RISC-V version
[https:
//github.com/mit-pdos/xv6-riscv.git](https://github.com/mit-pdos/xv6-riscv.git)

xv6 is a re-implementation of Dennis Ritchie's and Ken Thompson's Unix
Version 6 (v6).  xv6 loosely follows the structure and style of v6,
but is implemented for a modern x86-based multiprocessor using ANSI C.

### ACKNOWLEDGMENTS

xv6 is inspired by John Lions's Commentary on UNIX 6th Edition (Peer
to Peer Communications; ISBN: 1-57398-013-7; 1st edition (June 14,
2000)). See also [https://pdos.csail.mit.edu/6.828/](https://pdos.csail.mit.edu/6.828/), which
provides pointers to on-line resources for v6.

xv6 borrows code from the following sources:
    JOS (asm.h, elf.h, mmu.h, bootasm.S, ide.c, console.c, and others)
    Plan 9 (entryother.S, mp.h, mp.c, lapic.c)
    FreeBSD (ioapic.c)
    NetBSD (console.c)

The following people have made contributions: Russ Cox (context switching,
locking), Cliff Frey (MP), Xiao Yu (MP), Nickolai Zeldovich, and Austin
Clements.

We are also grateful for the bug reports and patches contributed by Silas
Boyd-Wickizer, Anton Burtsev, Cody Cutler, Mike CAT, Tej Chajed, eyalz800,
Nelson Elhage, Saar Ettinger, Alice Ferrazzi, Nathaniel Filardo, Peter
Froehlich, Yakir Goaron,Shivam Handa, Bryan Henry, Jim Huang, Alexander
Kapshuk, Anders Kaseorg, kehao95, Wolfgang Keller, Eddie Kohler, Austin
Liew, Imbar Marinescu, Yandong Mao, Matan Shabtay, Hitoshi Mitake, Carmi
Merimovich, Mark Morrissey, mtasm, Joel Nider, Greg Price, Ayan Shafqat,
Eldar Sehayek, Yongming Shen, Cam Tenny, tyfkda, Rafael Ubal, Warren
Toomey, Stephen Tu, Pablo Ventura, Xi Wang, Keiichi Watanabe, Nicolas
Wolovick, wxdao, Grant Wu, Jindong Zhang, Icenowy Zheng, and Zou Chang Wei.

The code in the files that constitute xv6 is
Copyright 2006-2018 Frans Kaashoek, Robert Morris, and Russ Cox.

### ERROR REPORTS

We don't process error reports (see note on top of this file).

### BUILDING AND RUNNING XV6

To build xv6 on an x86 ELF machine (like Linux or FreeBSD), run
"make". On non-x86 or non-ELF machines (like OS X, even on x86), you
will need to install a cross-compiler gcc suite capable of producing
x86 ELF binaries (see [https://pdos.csail.mit.edu/6.828/](https://pdos.csail.mit.edu/6.828/)).
Then run "make TOOLPREFIX=i386-jos-elf-". Now install the QEMU PC
simulator and run "make qemu".

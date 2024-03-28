# P5 Scheduler
    Tanvin Thiagarajan
    Saket Dhore
    Group 69 - :) -  (dream team)
    LEC 002


# Implementation
- All user lock functions are implemented as syscalls
- process struct modified to hold priority value
- priority value modified on lock aquire and release
- lock struct holds process struct pointer of holder as int (cast to proc* when used)
- All sycalls for implementation written in sysproc.c
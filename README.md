# EpochManager
An implementation of an epoch based garbage collector to be used together with BwTree as its memory reclaimer. The epoch manager implements the local write protocol where all writes to the epoch counter are conducted locally, and is supposed to be fast.

# Build
Run make benchmark to build the benchmark

Please do not put any file or directory ending with "-bin" under the project directory, otherwise tthey will be removed by "make clean" command

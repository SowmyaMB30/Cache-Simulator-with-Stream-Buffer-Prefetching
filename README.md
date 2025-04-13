# Cache Simulator with Stream-Buffer Prefetching

A cache simulator that models a multi-level memory hierarchy (L1, L2) with configurable parameters. The simulator includes a prefetch unit using Stream Buffers for optimized cache performance.

### Features
- Configurable cache size, block size, and associativity
- Supports LRU replacement and write-back + write-allocate policies
- Includes stream-buffer prefetching for L2 cache

### Build Instructions
Type `make` to build the simulator.  
(Type `make clean` first if recompiling from scratch.)

### Running the Simulator
To run without throttling output:
```bash
./sim <BLOCKSIZE> <L1_SIZE> <L1_ASSOC> <L2_SIZE> <L2_ASSOC> <PREF_N> <PREF_M> <trace_file>

#include <iostream>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <string>
#include <fstream>

using namespace std;
int l2Size;

struct CacheBlock {
    bool valid;
    bool dirty;
    uint32_t tag;
};

struct CacheSet {
    vector<CacheBlock> blocks;
    vector<int> lruOrder;
};

class Cache {
public:
    Cache(int size, int associativity, int blockSize)
        : size(size), associativity(associativity), blockSize(blockSize) {
        numSets = size / (associativity * blockSize);
        sets.resize(numSets);
        for (int i = 0; i < numSets; ++i) {
            sets[i].blocks.resize(associativity);
            sets[i].lruOrder.resize(associativity);
            for (int j = 0; j < associativity; ++j) {
                sets[i].lruOrder[j] = j;
                sets[i].blocks[j].valid = false;
                sets[i].blocks[j].dirty = false;
            }
        }
    }

    bool access(uint32_t address, bool isWrite, Cache* nextLevel, int& memoryTraffic,
                int& l1Writebacks, int& l2Writes, int& l2Reads, int& l2ReadMisses,
                int& l2Writebacks, int& l2WriteMisses);
    void printCacheContents(const string& cacheName);
    int getBlockSize() const { return blockSize; }

private:
    void updateLRU(CacheSet& set, int accessedIndex);
    void writeBack(uint32_t address, Cache* nextLevel, int& memoryTraffic,
                   int& l1Writebacks, int& l2Writes, int& l2Reads,
                   int& l2ReadMisses, int& l2Writebacks, int& l2WriteMisses);

    int size;
    int associativity;
    int blockSize;
    int numSets;
    vector<CacheSet> sets;
};

bool Cache::access(uint32_t address, bool isWrite, Cache* nextLevel, int& memoryTraffic,
                   int& l1Writebacks, int& l2Writes, int& l2Reads, int& l2ReadMisses,
                   int& l2Writebacks, int& l2WriteMisses) {
    uint32_t blockAddress = address / blockSize;
    uint32_t setIndex = blockAddress % numSets;
    uint32_t tag = blockAddress / numSets;

    auto& set = sets[setIndex];

    for (int i = 0; i < associativity; ++i) {
        if (set.blocks[i].valid && set.blocks[i].tag == tag) {
            if (isWrite) set.blocks[i].dirty = true;
            updateLRU(set, i);
            return true;
        }
    }

    int victimIndex = set.lruOrder.back();
    auto& victimBlock = set.blocks[victimIndex];

    if (victimBlock.valid && victimBlock.dirty) {
        uint32_t victimAddress = (victimBlock.tag * numSets + setIndex) * blockSize;
        if (nextLevel == nullptr) {
            if(l2Size==0){l1Writebacks++;}
            else{l2Writebacks++;}
            memoryTraffic++;
        } 
         else     
         {
         //
            writeBack(victimAddress, nextLevel, memoryTraffic, l1Writebacks,    
                      l2Writes, l2Reads, l2ReadMisses, l2Writebacks, l2WriteMisses);
        }
    }

    if (nextLevel) {
        l2Reads++;
        bool l2Hit = nextLevel->access(address, false, nullptr, memoryTraffic,
                                       l1Writebacks, l2Writes, l2Reads,
                                       l2ReadMisses, l2Writebacks, l2WriteMisses);
        if (!l2Hit) 
        l2ReadMisses++;
    } else {
        memoryTraffic++;
    }

    victimBlock = {true, isWrite, tag};
    updateLRU(set, victimIndex);

    return false;
}

void Cache::writeBack(uint32_t address, Cache* nextLevel, int& memoryTraffic,
                      int& l1Writebacks, int& l2Writes, int& l2Reads,
                      int& l2ReadMisses, int& l2Writebacks, int& l2WriteMisses) {
    if (nextLevel) {
        l2Writes++;
        l1Writebacks++;

        bool l2Hit = nextLevel->access(address, true, nullptr, memoryTraffic,
                                       l1Writebacks, l2Writes, l2Reads,
                                       l2ReadMisses, l2Writebacks, l2WriteMisses);
        if (!l2Hit) l2WriteMisses++;
    } else {
        memoryTraffic++;
    }
}

void Cache::printCacheContents(const string& cacheName) {
    printf("===== %s contents =====\n", cacheName.c_str());
    for (int i = 0; i < numSets; ++i) {
        printf("set %6d:", i);
        auto& set = sets[i];
        for (int index : set.lruOrder) {
            auto& block = set.blocks[index];
            if (block.valid) {
                printf(" %8x", block.tag);
                if (block.dirty) printf(" D");
                else printf("  ");
            } else {
                printf(" %10s", "");
            }
        }
        printf("\n");
    } printf("\n");
}

void Cache::updateLRU(CacheSet& set, int accessedIndex) {
    auto it = std::find(set.lruOrder.begin(), set.lruOrder.end(), accessedIndex);
    if (it != set.lruOrder.end()) set.lruOrder.erase(it);
    set.lruOrder.insert(set.lruOrder.begin(), accessedIndex);
}

class MemoryHierarchy {
public:
    MemoryHierarchy(int l1Size, int l1Associativity, int l2Size, int l2Associativity, int blockSize)
        : blockSize(blockSize), l1(l1Size, l1Associativity, blockSize), l2(nullptr) {
        if (l2Size > 0) {
            l2 = new Cache(l2Size, l2Associativity, blockSize);
        }
    }

    ~MemoryHierarchy() {
        if (l2) delete l2;
    }

    void access(uint32_t address, bool isWrite);
    void printStats();

    Cache& getL1Cache() { return l1; }
    Cache* getL2Cache() { return l2; }

private:
    Cache l1;
    Cache* l2;
    int blockSize;
    int l1Reads = 0, l1Writes = 0, l1WriteMisses = 0, l1ReadMisses = 0;
    int l1Writebacks = 0, l2Reads = 0, l2Writes = 0, l2ReadMisses = 0, l2Writebacks = 0;
    int l2WriteMisses = 0, memoryTraffic = 0;
};

void MemoryHierarchy::access(uint32_t address, bool isWrite) {
    bool l1Hit = l1.access(address, isWrite, l2, memoryTraffic, l1Writebacks,
                           l2Writes, l2Reads, l2ReadMisses, l2Writebacks, l2WriteMisses);

    if (isWrite) {
        l1Writes++;
        if (!l1Hit) 
        l1WriteMisses++;
    } else {
        l1Reads++;
        if (!l1Hit)
         l1ReadMisses++;
    }
}

void MemoryHierarchy::printStats() {
    printf("===== Measurements =====\n");
    printf("a. L1 reads:                   %d\n", l1Reads);
    printf("b. L1 read misses:             %d\n", l1ReadMisses);
    printf("c. L1 writes:                  %d\n", l1Writes);
    printf("d. L1 write misses:            %d\n", l1WriteMisses);
    printf("e. L1 miss rate:               %.4f\n", static_cast<float>(l1ReadMisses + l1WriteMisses) / (l1Reads + l1Writes));
    printf("f. L1 writebacks:              %d\n", l1Writebacks);
    printf("g. L1 prefetches:              0\n"); 
    printf("h. L2 reads (demand):          %d\n", l2Reads);
    printf("i. L2 read misses (demand):    %d\n", l2ReadMisses);
    printf("j. L2 reads (prefetch):        0\n");  
    printf("k. L2 read misses (prefetch):  0\n");
    printf("l. L2 writes:                  %d\n", l2Writes);
    printf("m. L2 write misses:            %d\n", l2WriteMisses);
    printf("n. L2 miss rate:               %.4f\n", 
           l2Reads > 0 ? static_cast<float>(l2ReadMisses) / l2Reads : 0.0);
    printf("o. L2 writebacks:              %d\n", l2Writebacks);
    printf("p. L2 prefetches:              0\n");
    printf("q. memory traffic:             %d\n", memoryTraffic);
}

int main(int argc, char* argv[]) {
    if (argc != 9) {
        cerr << "Usage: " << argv[0] << " <BLOCKSIZE> <L1_SIZE> <L1_ASSOC> <L2_SIZE> <L2_ASSOC> <PREF_N> <PREF_M> <trace_file>\n";
        return 1;
    }

    int blockSize = stoi(argv[1]);
    int l1Size = stoi(argv[2]);
    int l1Assoc = stoi(argv[3]);
    l2Size = stoi(argv[4]);
    int l2Assoc = stoi(argv[5]);
    string traceFile = argv[8];

    printf("===== Simulator configuration =====\n");
    printf("BLOCKSIZE:  %d\n", blockSize);
    printf("L1_SIZE:    %d\n", l1Size);
    printf("L1_ASSOC:   %d\n", l1Assoc);
    printf("L2_SIZE:    %d\n", l2Size);
    printf("L2_ASSOC:   %d\n", l2Assoc);
    printf("PREF_N:     %d\n", stoi(argv[6]));  
    printf("PREF_M:     %d\n", stoi(argv[7]));
    printf("trace_file: %s\n", traceFile.c_str());
    printf("\n");

    MemoryHierarchy hierarchy(l1Size, l1Assoc, l2Size, l2Assoc, blockSize);

    ifstream trace(traceFile);
    string line;
    while (getline(trace, line)) {
        if (line.empty()) continue;
        bool isWrite = line[0] == 'w';
        uint32_t address = stoul(line.substr(2), nullptr, 16);
        hierarchy.access(address, isWrite);
    }

    hierarchy.getL1Cache().printCacheContents("L1");
    if (l2Size > 0) hierarchy.getL2Cache()->printCacheContents("L2");
    hierarchy.printStats();

    return 0;
}



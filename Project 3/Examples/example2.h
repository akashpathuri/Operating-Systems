#ifndef MY_VM_H_INCLUDED
#define MY_VM_H_INCLUDED
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

//Assume the address space is 32 bits, so the max memory size is 4GB
//Page size is 4KB

//Add any important includes here which you may need

#define PGSIZE 4096

// Maximum size of your memory
#define MAX_MEMSIZE 4ULL*1024*1024*1024 //4GB

#define MEMSIZE 1024*1024*1024

// NEW: Amnt of physical/virtual pages
#define NUM_PHYS_PGS MEMSIZE / PGSIZE
#define NUM_VIRT_PGS MAX_MEMSIZE / PGSIZE

//#define NUM_PHYS_PGS 1024*1024*1024/4096
//#define NUM_VIRT_PGS 4ULL*1024*1024*1024/4096

// Represents a page table entry
//typedef void* pte_t; //used to be unsigned long, maybe we make this char* instead of void* though 
typedef unsigned long pte_t;

// Represents a page directory entry
//typedef pte_t* pde_t; //used to be unsigned long, TA said it was ok to change these and it might be easier to work with in this way. Can think of it as 
typedef unsigned long pde_t;

// NEW: physical memory, page states
char* physicalMem;
//char* virtualMem;
char* physBitMap;
char* virtBitMap;
pde_t* pageDirectory;
// pde_t pageDirectory;

#define TLB_SIZE 120

//reps a tlb entry
struct tlbEntry{
    int used;
    int time;
    void* virtAddr;
    pte_t* physAddr;
};

//Structure to represents TLB
struct tlb {

    //Assume your TLB is a direct mapped TLB of TBL_SIZE (entries)
    // You must also define wth TBL_SIZE in this file.
    //Assume each bucket to be 4 bytes
    struct tlbEntry entries[TLB_SIZE];
    int currentLoad;
    double missRate;
    int hits;
    int misses;
};
struct tlb tlb_store;

struct pair{
    int used;
    unsigned long addr;
    int* pages;
    int numPages;
};


void SetPhysicalMem();
pte_t* Translate(pde_t *pgdir, void *va);
int PageMap(pde_t *pgdir, void *va, void* pa);
void *get_next_avail( int num_pages );
bool check_in_tlb(void *va);
void put_in_tlb(void *va, void *pa);
void *myalloc(unsigned int num_bytes);
void myfree(void *va, int size);
void PutVal(void *va, void *val, int size);
void GetVal(void *va, void *val, int size);
void MatMult(void *mat1, void *mat2, int size, void *answer);
void print_TLB_missrate();
pte_t* check_TLB(void *va);
int add_TLB(void *va, void *pa);

void oneBit( int page );
void zeroBit( int page );
int isBit( int page );

#endif
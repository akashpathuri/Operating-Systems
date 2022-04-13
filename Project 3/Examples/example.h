#ifndef MY_VM_H_INCLUDED
#define MY_VM_H_INCLUDED
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

//Assume the address space is 32 bits, so the max memory size is 4GB
//Page size is 4KB

#define PGSIZE 4096
// Maximum size of your memory
#define MAX_MEMSIZE 4ULL*1024*1024*1024 //4GB

#define MEMSIZE 1024*1024*1024

// Represents a page table entry
typedef unsigned long pte_t;

// Represents a page directory entry
typedef unsigned long pde_t;

#define TLB_SIZE 120

//Structure to represents TLB
struct tlb {
    //Assume your TLB is a direct mapped TLB of TBL_SIZE (entries)
    // You must also define wth TBL_SIZE in this file.
    //Assume each bucket to be 4 bytes
    int index;
    int last_used;
    int page_number;
    pte_t frame_number;
    struct tlb *next;
};

struct tlb *tlb_store;

char *memory;
pde_t *directory;
pte_t **page_tables;
char *virtual_bitmap;
char *physical_bitmap;

unsigned int checks;
unsigned int misses;

pthread_mutex_t mutex;

void SetPhysicalMem();
pte_t* Translate(pde_t *pgdir, void *va);
int PageMap(pde_t *pgdir, void *va, void* pa);
bool check_in_tlb(void *va);
void put_in_tlb(void *va, void *pa);
void *myalloc(unsigned int num_bytes);
void myfree(void *va, int size);
void PutVal(void *va, void *val, int size);
void GetVal(void *va, void *val, int size);
void MatMult(void *mat1, void *mat2, int size, void *answer);
void print_TLB_missrate();
int get_physical_bit(int bit);
int get_virtual_bit(int bit);
void set_physical_bit(int bit);
void set_virtual_bit(int bit);
void clear_physical_bit(int bit);
void clear_virtual_bit(int bit);

#endif 
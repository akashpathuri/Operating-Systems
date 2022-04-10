#ifndef MY_VM_H_INCLUDED
#define MY_VM_H_INCLUDED
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <sys/mman.h>
#include <stdint.h>

//Assume the address space is 32 bits, so the max memory size is 4GB
//Page size is 4KB

//Add any important includes here which you may need

#define PGSIZE 4096

// Maximum size of virtual memory
#define MAX_MEMSIZE 4ULL*1024*1024*1024

// Size of "physcial memory"
#define MEMSIZE 1024*1024*1024

// Represents a page table entry
typedef unsigned long pte_t;

// Represents a page directory entry
typedef unsigned long pde_t;

#define TLB_ENTRIES 512

//Structure to represents TLB
struct tlb {
    /*Assume your TLB is a direct mapped TLB with number of entries as TLB_ENTRIES
    * Think about the size of each TLB entry that performs virtual to physical
    * address translation.
    */
    void* virtAddr;
    void* physAddr;
    int index; //index in the linked list of the current tlb entry
    int age; // represents if this entry is used or not, the larger it is the older.
   // int page_number; // possibly represents directory number  
   // pte_t frame; // frame number of current entry as a page table entry 
    struct tlb *link; //next item in linked list
};
struct tlb *tlb_store;

pthread_mutex_t mutex;
char *physical_memory;
pde_t *outer_directory_table;
pte_t **inner_page_tables;
int total_frames;
char *virtual_address_bitmap;
char *physical_address_bitmap;
double checks;
double misses;


void set_physical_mem();
pte_t* translate(pde_t *pgdir, void *va);
int page_map(pde_t *pgdir, void *va, void* pa);
bool check_in_tlb(void *va);
void put_in_tlb(void *va, void *pa);
void *t_malloc(unsigned int num_bytes);
void t_free(void *va, int size);
void put_value(void *va, void *val, int size);
void get_value(void *va, void *val, int size);
void mat_mult(void *mat1, void *mat2, int size, void *answer);
void print_TLB_missrate();
void set_bit_at_index(char *bitmap, int index);
int get_bit_at_index(char *bitmap, int index);

#endif

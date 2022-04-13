
#include "example.h"

// whether myalloc() has been called yet
bool initialized = false;

// the number of bits in a VA representing the frame offset
int offset_bits;

// the number of bits in a VA representing the directory index
int directory_bits;

// the number of bits in a VA representing the page table index
int page_bits;

// the number of page tables in the directory
int directory_size;

// the number of pages in a page table
int page_table_size;

// the total number of frames
unsigned int num_frames;

/*
 * Function responsible for allocating and setting your physical memory
 */
void SetPhysicalMem() {
 	offset_bits = 0;
   	int page_size = PGSIZE;
   	while(page_size >>= 1) {
   		offset_bits++;
   	}
   	
	directory_bits = (32 - offset_bits) / 2;
	page_bits = 32 - offset_bits - directory_bits;
   	directory_size = 1 << directory_bits;
   	page_table_size = 1 << page_bits;
   	num_frames = directory_size * page_table_size;
	printf("num_frames: %p\n", num_frames);
	directory = (pde_t *) malloc(sizeof(pde_t) * directory_size);
	page_tables = (pte_t **) malloc(sizeof(pte_t *) * directory_size);
	
	int i;
	for(i = 0; i < directory_size; i++) {
		directory[i] = -1;
	}
	
	memory = mmap(NULL, MEMSIZE, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);

	virtual_bitmap = (char *) malloc(num_frames / 8);
	physical_bitmap = (char *) malloc(num_frames / 8);
	
	tlb_store = NULL;
	
	pthread_mutex_init(&mutex, NULL);
}

/*
 * Part 2: Add a virtual to physical page translation to the TLB.
 * Feel free to extend the function arguments or return type.
 */
int add_TLB(void *va, void *pa) {
    struct tlb *tlb;
    
    if(tlb_store == NULL) {
    	tlb_store = malloc(sizeof(struct tlb));
    	tlb_store->index = 0;
    	tlb = tlb_store;
    } else {
		for(tlb = tlb_store; tlb->next != NULL; tlb = tlb->next) {}
		
		tlb->next = malloc(sizeof(struct tlb));
		tlb->next->index = tlb->index + 1;
		tlb = tlb->next;
	}
    
    tlb->last_used = 0;
   	tlb->page_number = ((uintptr_t) va) >> offset_bits;
   	tlb->frame_number = ((char *) pa - memory) / PGSIZE;
   	tlb->next = NULL;
   	
   	// find a TLB entry to evict
   	if(tlb->index == TLB_SIZE) {
   		int lru = 0;
   		int lru_index = 0;
   		
   		// find the highest last_used value
   		for(tlb = tlb_store; tlb != NULL; tlb = tlb->next) {
   			if(tlb->last_used > lru) {
   				lru = tlb->last_used;
   				lru_index = tlb->index;
   			}
   		}
   		
   		// remove the entry from the linked list and free it
   		if(lru_index == 0) {
   			struct tlb *old = tlb_store;
   			tlb_store = tlb_store->next;
   			tlb = tlb_store;
   			free(old);
   		} else {
			for(tlb = tlb_store; tlb != NULL; tlb = tlb->next) {
	   			if(tlb->next->index == lru_index) {
	   				struct tlb *old = tlb->next;
	   				tlb->next = tlb->next->next;
	   				tlb = tlb->next;
	   				free(old);
	   				break;
	   			}
	   		}
   		}
   		
   		// lower the index of each TLB entry after the removed entry
   		while(tlb != NULL) {
   			tlb->index--;
   			tlb = tlb->next;
   		}
   	}

    return 1;
}

/*
 * Part 2: Check TLB for a valid translation.
 * Returns the physical page address.
 * Feel free to extend this function and change the return type.
 */
pte_t *check_TLB(void *va) {
	checks++;

	if(tlb_store == NULL) {
		misses++;
		return NULL;
	}

	int page_number = ((uintptr_t) va) >> offset_bits;
	struct tlb *tlb = tlb_store;
	pte_t *frame = NULL;
	
	while(tlb != NULL) {
		if(tlb->page_number == page_number) {
			tlb->last_used = 0;
			frame = (pte_t *) ((char *) memory + tlb->frame_number * PGSIZE);
		} else {
			// increment last_used of every other TLB entry
			tlb->last_used++;
		}
	
		tlb = tlb->next;
	}
	
	if(frame == NULL) {
		misses++;
	}
	
	return frame;
}

/*
 * Part 2: Print TLB miss rate.
 * Feel free to extend the function arguments or return type.
 */
void print_TLB_missrate() {
    double miss_rate = (double) misses / checks;

    fprintf(stderr, "TLB miss rate %d/%d = %lf%%\n", misses, checks, miss_rate * 100);
}

/*
 * The function takes a virtual address and page directories starting address and
 * performs translation to return the physical address
 */
pte_t *Translate(pde_t *pgdir, void *va) {
    uintptr_t ptr = (uintptr_t) va;
	int offset = ptr & ((1 << offset_bits) - 1);
    pte_t *tlb_entry = check_TLB(va);
    
    if(tlb_entry == NULL) {
    	// calculate the PA and add it to the TLB
		int directory_index = ptr >> (32 - directory_bits);
		pte_t *table = page_tables[pgdir[directory_index]];
		
		int table_index = (ptr >> offset_bits) & ((1 << (page_bits)) - 1);
		pte_t index = table[table_index];
		void *pa = memory + index * PGSIZE + offset;
		
		add_TLB(va, pa);
		return (pte_t *) pa;
    } else {
    	return (pte_t *) ((char *) tlb_entry + offset);
    }
}

/*
 * The function takes a page directory address, virtual address, physical address
 * as an argument, and sets a page table entry. This function will walk the page
 * directory to see if there is an existing mapping for a virtual address. If the
 * virtual address is not present, then a new entry will be added
 */
int PageMap(pde_t *pgdir, void *va, void *pa) {
    uintptr_t ptr = (uintptr_t) va;
    int directory_index = ptr >> (32 - directory_bits);
    int table_index = (ptr >> offset_bits) & ((1 << page_bits) - 1);
    pde_t index = pgdir[directory_index];
	pte_t *table = page_tables[index];
	
	// table not initialized yet, malloc and set all entries to -1
	if(table == NULL) {
		page_tables[index] = (pte_t *) malloc(sizeof(pte_t) * page_table_size);
		table = page_tables[index];
		
		int j;
		for(j = 0; j < page_table_size; j++) {
			table[j] = -1;
		}
	}
	
	if(table[table_index] == -1) {
		table[table_index] = ((char *) pa - memory) / PGSIZE;
		return 1;
	}

    return 0;
}


/*
 * Function that gets the next available page
 */
void *get_next_avail(int num_pages) {
	int i;
	for(i = 1; i < num_frames; i++) {
		bool valid = true;
		
		int j;
		for(j = i; j < num_pages + i; j++) {
			if(get_virtual_bit(j)) {
				valid = false;
			}
		}
		
		if(valid) {
			for(j = i; j < num_pages + i; j++) {
				set_virtual_bit(j);
			}
			
			break;
		}
	}
	
	if(i == num_frames) {
		return NULL;
	}
	
	return (void *) (i * PGSIZE);
}

/* Function responsible for allocating pages
 * and used by the benchmark
 */
void *myalloc(unsigned int num_bytes) {
	pthread_mutex_lock(&mutex);
	
	if(!initialized) {
		initialized = true;
		SetPhysicalMem();
	}

	int num_pages = num_bytes / PGSIZE + (num_bytes % PGSIZE != 0);
	void *va = get_next_avail(num_pages);
	
	if(va == NULL) {
		return NULL;
	}
	
    int directory_index = ((uintptr_t) va) >> (32 - directory_bits);
    int table_index = (((uintptr_t) va) >> offset_bits) & ((1 << page_bits) - 1);
	
	if(directory[directory_index] == -1) {
		// find a page table with a number of consecutive free entries equal to num_pages
		int i;
		for(i = 0; i < directory_size && table_index == -1; i++) {
			pte_t *table = page_tables[i];
			
			int j;
			for(j = 0; j <= page_table_size - num_pages; j++) {
				bool valid = true;
				
				int k;
				for(k = 0; k < num_pages; k++) {
					if(table[j + k] != -1) {
						valid = false;
						break;
					}
				}
				
				if(valid) {
					i--;
					break;
				}
			}
		}
		
		directory[directory_index] = i;
	}
	
	// find an available frame for each page and map them together
	void *ptr;
	int k;
	for(k = 0; k < num_pages; k++) {
		// find a free frame
		int i;
		for(i = 0; i < num_frames; i++) {
			if(!get_physical_bit(i)) {
				set_physical_bit(i);
				break;
			}
		}
	
		PageMap(directory, va + k * PGSIZE, memory + i * PGSIZE);
		
		if(k == 0) {
			ptr = va;
		}
	}
	
	pthread_mutex_unlock(&mutex);
	return ptr;
}

/* 
 * Responsible for releasing one or more memory pages using virtual address (va)
 */
void myfree(void *va, int size) {
    // Free the page table entries starting from this virtual address (va)
    // Also mark the pages free in the bitmap
    // Only free if the memory from "va" to va+size is valid
    int num_pages = size / PGSIZE + (size % PGSIZE != 0);
    int i;
    for(i = 0; i < num_pages; i++) {
		int directory_index = ((uintptr_t) va + i * PGSIZE) >> (32 - directory_bits);
		int table_index = (((uintptr_t) va + i * PGSIZE) >> offset_bits) & ((1 << page_bits) - 1);
		int offset = ((uintptr_t) va) & ((1 << offset_bits) - 1);
		
		// not pointing to base of page, invalid pointer
		if(offset) {
			return;
		}
		
		int page_num = (uintptr_t) va / PGSIZE + i;
		
		pthread_mutex_lock(&mutex);
		if(get_virtual_bit(page_num)) {
			clear_virtual_bit(page_num);
			
			pte_t *table = page_tables[directory[directory_index]];
			int frame_num = table[table_index];

			if(get_physical_bit(frame_num)) {
				clear_physical_bit(frame_num);
			}
			
			table[table_index] = -1;
			pthread_mutex_unlock(&mutex);
		} else { // pointing to unused page, invalid pointer
			pthread_mutex_unlock(&mutex);
			return;
		}
    }
}

/* 
 * The function copies data pointed by "val" to physical
 * memory pages using virtual address (va)
 */
void PutVal(void *va, void *val, int size) {
    /* HINT: Using the virtual address and Translate(), find the physical page. Copy
       the contents of "val" to a physical page. NOTE: The "size" value can be larger
       than one page. Therefore, you may have to find multiple pages using Translate()
       function.*/
	int offset = ((uintptr_t) va) & ((1 << offset_bits) - 1);
	
	pthread_mutex_lock(&mutex);
	void *pa = Translate(directory, va);

	// invalid virtual address
	if(!get_virtual_bit(((uintptr_t) va) / PGSIZE)) {
		return;
	}

	int remaining_in_page = PGSIZE - offset;
	if(size <= remaining_in_page) {
		memcpy(pa, val, size);
	} else {
		memcpy(pa, val, remaining_in_page);
		size -= remaining_in_page;
		va += remaining_in_page;
		val += remaining_in_page;
		
		while(size > 0) {
			if(!get_virtual_bit(((uintptr_t) va) / PGSIZE)) return;
		
			pa = Translate(directory, va);
			
			if(size % PGSIZE == 0) {
				memcpy(pa, val, PGSIZE);
			} else {
				memcpy(pa, val, size % PGSIZE);
			}
			
			size -= PGSIZE;
			va += PGSIZE;
			val += PGSIZE;
		}
	}
	
	pthread_mutex_unlock(&mutex);
}

/*Given a virtual address, this function copies the contents of the page to val*/
void GetVal(void *va, void *val, int size) {
    /* HINT: put the values pointed to by "va" inside the physical memory at given
    "val" address. Assume you can access "val" directly by derefencing them.
    If you are implementing TLB,  always check first the presence of translation
    in TLB before proceeding forward */
	int offset = ((uintptr_t) va) & ((1 << offset_bits) - 1);
	
	pthread_mutex_lock(&mutex);
	void *pa = Translate(directory, va);

	// invalid virtual address
	if(!get_virtual_bit(((uintptr_t) va) / PGSIZE)) {
		return;
	}

	int remaining_in_page = PGSIZE - offset;
	if(size <= remaining_in_page) {
		memcpy(val, pa, size);
	} else {
		memcpy(val, pa, remaining_in_page);
		size -= remaining_in_page;
		va += remaining_in_page;
		val += remaining_in_page;
		
		while(size > 0) {
			if(!get_virtual_bit((uintptr_t) va) / PGSIZE) return;
		
			pa = Translate(directory, va);
			
			if(size % PGSIZE == 0) {
				memcpy(val, pa, PGSIZE);
			} else {
				memcpy(val, pa, size % PGSIZE);
			}
			
			size -= PGSIZE;
			va += PGSIZE;
			val += PGSIZE;
		}
	}
	
	pthread_mutex_unlock(&mutex);
}

/*
 * This function receives two matrices mat1 and mat2 as an argument with size
 * argument representing the number of rows and columns. After performing matrix
 * multiplication, copy the result to answer.
 */
/*void MatMult(void *mat1, void *mat2, int size, void *answer) {
	/* Hint: You will index as [i * size + j] where  "i, j" are the indices of the
	matrix accessed. Similar to the code in test.c, you will use GetVal() to
	load each element and perform multiplication. Take a look at test.c! In addition to
	getting the values from two matrices, you will perform multiplication and
	store the result to the "answer array"
	int k;
	int j;
	int i;
	int y;
	int z;
	int sum = 0;
	int address_a = 0;
	int address_b = 0;
	int address_c = 0;
	unsigned long getval_time = 0;
	unsigned long putval_time = 0;
	struct timespec start, end;
	
	for(k = 0; k < size; k++) { //row
		for(j = 0; j < size; j++) { //column
			for(i = 0; i < size; i++) {
				address_a = (unsigned int) mat1 + (k * size * sizeof(int)) + (i * sizeof(int));
				address_b = (unsigned int) mat2 + (i * size * sizeof(int)) + (j * sizeof(int));
                
				clock_gettime(CLOCK_REALTIME, &start);
				GetVal((void *) address_a, &y, sizeof(int));
				clock_gettime(CLOCK_REALTIME, &end);
				getval_time += (end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec);
				
				clock_gettime(CLOCK_REALTIME, &start);
				GetVal((void *) address_b, &z, sizeof(int));
				clock_gettime(CLOCK_REALTIME, &end);
				getval_time += (end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec);
                
                sum += y * z;
            }
            
            address_c = (unsigned int) answer + ((k * size * sizeof(int))) + (j * sizeof(int));
            
			clock_gettime(CLOCK_REALTIME, &start);
            PutVal((void *)address_c, &sum, sizeof(int));
			clock_gettime(CLOCK_REALTIME, &end);
			putval_time += (end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec);

			sum = 0;
        }
	}
	
	printf("Average time over %d calls to GetVal(): %lu microseconds\n", 2 * size * size * size, getval_time / (2 * size * size * size));
	printf("Average time over %d calls to PutVal(): %lu microseconds\n", size * size * size, putval_time / (size * size * size));
}*/

int get_physical_bit(int bit) {
	char *bitmap = physical_bitmap + bit / 8;
	return *bitmap & (1 << (bit % 8));
}

int get_virtual_bit(int bit) {
	char *bitmap = virtual_bitmap + bit / 8;
	return *bitmap & (1 << (bit % 8));
}

void set_physical_bit(int bit) {
	char *bitmap = physical_bitmap + bit / 8;
	*bitmap |= 1 << bit % 8;
}

void set_virtual_bit(int bit) {
	char *bitmap = virtual_bitmap + bit / 8;
	*bitmap |= 1 << bit % 8;
}

void clear_physical_bit(int bit) {
	char *bitmap = physical_bitmap + bit / 8;
	*bitmap &= ~(1 << bit % 8);
}

void clear_virtual_bit(int bit) {
	char *bitmap = virtual_bitmap + bit / 8;
	*bitmap &= ~(1 << bit % 8);
}
void MatMult(void *mat1, void *mat2, int size, void *answer) {

    /* Hint: You will index as [i * size + j] where  "i, j" are the indices of the
     * matrix accessed. Similar to the code in test.c, you will use get_value() to
     * load each element and perform multiplication. Take a look at test.c! In addition to 
     * getting the values from two matrices, you will perform multiplication and 
     * store the result to the "answer array"
     */
    int x, y, val_size = sizeof(int);
    int i, j, k;
    for (i = 0; i < size; i++) {
        for(j = 0; j < size; j++) {
            unsigned int a, b, c = 0;
            for (k = 0; k < size; k++) {
                int address_a = (unsigned int)mat1 + ((i * size * sizeof(int))) + (k * sizeof(int));
                int address_b = (unsigned int)mat2 + ((k * size * sizeof(int))) + (j * sizeof(int));
                GetVal( (void *)address_a, &a, sizeof(int));
                GetVal( (void *)address_b, &b, sizeof(int));
                // printf("Values at the index: %d, %d, %d, %d, %d\n", 
                //     a, b, size, (i * size + k), (k * size + j));
                c += (a * b);
            }
            int address_c = (unsigned int)answer + ((i * size * sizeof(int))) + (j * sizeof(int));
            // printf("This is the c: %d, address: %x!\n", c, address_c);
            PutVal((void *)address_c, (void *)&c, sizeof(int));
        }
    }
}




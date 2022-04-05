#include "my_vm.h"
// whether myalloc() has been called yet
bool initialized = false;

// the number of bits in a VA representing the frame offset
int offset;

// the number of bits in a VA representing the directory index
int directory_bits;

// the number of bits in a VA representing the page table index
int page;

// the number of page tables in the directory
int num_page_tables;

// the number of pages in a page table
int pages;

// the total number of frames
unsigned int num_frames;

/*
Function responsible for allocating and setting your physical memory 
*/
void set_physical_mem() {

    //Allocate physical memory using mmap or malloc; this is the total size of
    //your memory you are simulating

    
    //HINT: Also calculate the number of physical and virtual pages and allocate
    //virtual and physical bitmaps and initialize them
   
    offset = 0;
   	int page_size = PGSIZE;
    while(page_size >>= 1) {
   		offset++;
   	}
    directory_bits = (32 - offset) / 2;
	page = 32 - offset - directory_bits;
   	page_tables = 1 << directory_bits;
   	pages = 1 << page;
   	num_frames = num_page_tables * pages;
	directory = (pde_t *) malloc(sizeof(pde_t) * num_page_tables);
	page_tables = (pte_t **) malloc(sizeof(pte_t *) * num_page_tables);
	
	int i;
	for(i = 0; i < num_page_tables; i++) {
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
int add_TLB(void *va, void *pa)
{

    /*Part 2 HINT: Add a virtual to physical page translation to the TLB */
    struct tlb *tlb;
    
    if(tlb_store == NULL) {
    	tlb_store = malloc(sizeof(struct tlb));
    	tlb_store->curr_index = 0;
    	tlb = tlb_store;
    } else {
		for(tlb = tlb_store; tlb->next != NULL; tlb = tlb->next) {}
		
		tlb->next = malloc(sizeof(struct tlb));
		tlb->next->curr_index = tlb->curr_index + 1;
		tlb = tlb->next;
	}
    
    tlb->prev_index = 0;
   	tlb->page_num = ((uintptr_t) va) >> offset;
   	tlb->frame_number = ((char *) pa - memory) / PGSIZE;
   	tlb->next = NULL;
   	
   	// find a TLB entry to evict
   	if(tlb->curr_index == TLB_ENTRIES) {
   		int lru = 0;
   		int lru_index = 0;
   		
   		// find the highest last_used value
   		for(tlb = tlb_store; tlb != NULL; tlb = tlb->next) {
   			if(tlb->prev_index > lru) {
   				lru = tlb->prev_index;
   				lru_index = tlb->curr_index;
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
	   			if(tlb->next->curr_index == lru_index) {
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
   			tlb->curr_index--;
   			tlb = tlb->next;
   		}
   	}

    return 1;







    return -1;
}


/*
 * Part 2: Check TLB for a valid translation.
 * Returns the physical page address.
 * Feel free to extend this function and change the return type.
 */
pte_t * check_TLB(void *va) {

    /* Part 2: TLB lookup code here */
    checks++;

	if(tlb_store == NULL) {
		misses++;
		return NULL;
	}

	int page_number = ((uintptr_t) va) >> offset;
	struct tlb *tlb = tlb_store;
	pte_t *frame = NULL;
	
	while(tlb != NULL) {
		if(tlb->page_num == page_number) {
			tlb->prev_index = 0;
			frame = (pte_t *) ((char *) memory + tlb->frame_number * PGSIZE);
		} else {
			// increment last_used of every other TLB entry
			tlb->prev_index++;
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
void print_TLB_missrate()
{

    /*Part 2 Code here to calculate and print the TLB miss rate*/
    double miss_rate = (double) misses / checks;

    fprintf(stderr, "TLB miss rate %lf \n", miss_rate);
}



/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t *translate(pde_t *pgdir, void *va) {
    /* Part 1 HINT: Get the Page directory index (1st level) Then get the
    * 2nd-level-page table index using the virtual address.  Using the page
    * directory index and page table index get the physical address.
    *
    * Part 2 HINT: Check the TLB before performing the translation. If
    * translation exists, then you can return physical address from the TLB.
    */

    uintptr_t ptr = (uintptr_t) va;
	int offset = ptr & ((1 << offset) - 1);
    pte_t *tlb_entry = check_TLB(va);
    
    if(tlb_entry == NULL) {
    	// calculate the PA and add it to the TLB
		int directory_index = ptr >> (32 - directory_bits);
		pte_t *table = page_tables[pgdir[directory_index]];
		
		int table_index = (ptr >> offset) & ((1 << (page)) - 1);
		pte_t index = table[table_index];
		void *pa = memory + index * PGSIZE + offset;
		
		add_TLB(va, pa);
		return (pte_t *) pa;
    } else {
    	return (pte_t *) ((char *) tlb_entry + offset);
    }








    //If translation not successful, then return NULL
    return NULL; 
}


/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int
page_map(pde_t *pgdir, void *va, void *pa)
{

    /*HINT: Similar to translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping */

    uintptr_t ptr = (uintptr_t) va;
    int directory_index = ptr >> (32 - directory_bits);
    int table_index = (ptr >> offset) & ((1 << page) - 1);
    pde_t index = pgdir[directory_index];
	pte_t *table = page_tables[index];
	
	// table not initialized yet, malloc and set all entries to -1
	if(table == NULL) {
		page_tables[index] = (pte_t *) malloc(sizeof(pte_t) * num_page_tables);
		table = page_tables[index];
		
		int j;
		for(j = 0; j < num_page_tables; j++) {
			table[j] = -1;
		}
	}
	
	if(table[table_index] == -1) {
		table[table_index] = ((char *) pa - memory) / PGSIZE;
		return 1;
	}

 
    return -1;
}


/*Function that gets the next available page
*/
void *get_next_avail(int num_pages) {
 
    //Use virtual address bitmap to find the next free page
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
and used by the benchmark
*/
void *t_malloc(unsigned int num_bytes) {

    /* 
     * HINT: If the physical memory is not yet initialized, then allocate and initialize.
     */

   /* 
    * HINT: If the page directory is not initialized, then initialize the
    * page directory. Next, using get_next_avail(), check if there are free pages. If
    * free pages are available, set the bitmaps and map a new page. Note, you will 
    * have to mark which physical pages are used. 
    */
   pthread_mutex_lock(&mutex);
	
	if(!initialized) {
		initialized = true;
		set_physical_mem();
	}

	int num_pages = num_bytes / PGSIZE + (num_bytes % PGSIZE != 0);
	void *va = get_next_avail(num_pages);
	
	if(va == NULL) {
		return NULL;
	}
	
    int directory_index = ((uintptr_t) va) >> (32 - directory_bits);
    int table_index = (((uintptr_t) va) >> offset) & ((1 << page) - 1);
	
	if(directory[directory_index] == -1) {
		// find a page table with a number of consecutive free entries equal to num_pages
		int i;
		for(i = 0; i < num_page_tables && table_index == -1; i++) {
			pte_t *table = page_tables[i];
			
			int j;
			for(j = 0; j <= pages - num_pages; j++) {
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
	
		page_map(directory, va + k * PGSIZE, memory + i * PGSIZE);
		
		if(k == 0) {
			ptr = va;
		}
	}
	
	pthread_mutex_unlock(&mutex);
    if(ptr != NULL){
        return ptr;
    }







    return NULL;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
void t_free(void *va, int size) {

    /* Part 1: Free the page table entries starting from this virtual address
     * (va). Also mark the pages free in the bitmap. Perform free only if the 
     * memory from "va" to va+size is valid.
     *
     * Part 2: Also, remove the translation from the TLB
     */
     int num_pages = size / PGSIZE + (size % PGSIZE != 0);
    int i;
    for(i = 0; i < num_pages; i++) {
		int directory_index = ((uintptr_t) va + i * PGSIZE) >> (32 - directory_bits);
		int table_index = (((uintptr_t) va + i * PGSIZE) >> offset) & ((1 << page) - 1);
		int offset = ((uintptr_t) va) & ((1 << offset) - 1);
		
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


/* The function copies data pointed by "val" to physical
 * memory pages using virtual address (va)
 * The function returns 0 if the put is successfull and -1 otherwise.
*/
void put_value(void *va, void *val, int size) {

    /* HINT: Using the virtual address and translate(), find the physical page. Copy
     * the contents of "val" to a physical page. NOTE: The "size" value can be larger 
     * than one page. Therefore, you may have to find multiple pages using translate()
     * function.
     */
    int offset = ((uintptr_t) va) & ((1 << offset) - 1);
	
	pthread_mutex_lock(&mutex);
	void *pa = translate(directory, va);

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
		
			pa = translate(directory, va);
			
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
void get_value(void *va, void *val, int size) {

    /* HINT: put the values pointed to by "va" inside the physical memory at given
    * "val" address. Assume you can access "val" directly by derefencing them.
    */
   int offset = ((uintptr_t) va) & ((1 << offset) - 1);
	
	pthread_mutex_lock(&mutex);
	void *pa = translate(directory, va);

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
		
			pa = translate(directory, va);
			
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







/*
This function receives two matrices mat1 and mat2 as an argument with size
argument representing the number of rows and columns. After performing matrix
multiplication, copy the result to answer.
*/
void mat_mult(void *mat1, void *mat2, int size, void *answer) {

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
                get_value( (void *)address_a, &a, sizeof(int));
                get_value( (void *)address_b, &b, sizeof(int));
                // printf("Values at the index: %d, %d, %d, %d, %d\n", 
                //     a, b, size, (i * size + k), (k * size + j));
                c += (a * b);
            }
            int address_c = (unsigned int)answer + ((i * size * sizeof(int))) + (j * sizeof(int));
            // printf("This is the c: %d, address: %x!\n", c, address_c);
            put_value((void *)address_c, (void *)&c, sizeof(int));
        }
    }
}




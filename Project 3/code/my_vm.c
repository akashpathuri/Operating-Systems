#include "my_vm.h"

// int page_offset;
// int outer_directory_size;
int outer_bits;
int inner_bits;
int offset_bits;
checks = 0;
misses = 0;
/*
Function responsible for allocating and setting your physical memory 
*/
void set_physical_mem() {

    //Allocate physical memory using mmap or malloc; this is the total size of
    //your memory you are simulating
	physical_memory = mmap(NULL, MEMSIZE, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);

    
    //HINT: Also calculate the number of physical and virtual pages and allocate
    //virtual and physical bitmaps and initialize them
	offset_bits = (int)log2(PGSIZE);
	int virtual_page_bits = 32-offset_bits;
	outer_bits = virtual_page_bits/2;
	inner_bits = 32-offset_bits-outer_bits;

	int outer_directory_size = 1<<outer_bits;
	int inner_page_size = 1<<inner_bits;
	total_frames = outer_directory_size * inner_page_size;
	outer_directory_table = (pde_t *) malloc(sizeof(pde_t) * outer_directory_size);
	inner_page_tables = (pte_t **) malloc(sizeof(pte_t *) * outer_directory_size);
	// virtual_address_bitmap = (char *) malloc(frame_count / 8);
	// physical_address_bitmap = (char *) malloc(frame_count / 8);
	
	// for(int i = 0; i < outer_directory_size; i++) {
	// 	outer_directory_table[i] = -1;
	// }

   // tlb_store = NULL;
	
	pthread_mutex_init(&mutex, NULL);
}


/*
 * Part 2: Add a virtual to physical page translation to the TLB.
 * Feel free to extend the function arguments or return type.
 */
int add_TLB(void *va, void *pa)
{
    struct tlb *ptr_tlb;
    
    if(tlb_store == NULL) { //Checking if any tlb entries exist
    	tlb_store = malloc(sizeof(struct tlb));
    	tlb_store->index = 0;
    	ptr_tlb = tlb_store;
    }
    //the average case is when 1 or more entries exist meaning we must traverse til the end of the linked list
	for(ptr_tlb = tlb_store; ptr_tlb->link != NULL; ptr_tlb = ptr_tlb->link) {}
	if(ptr_tlb->index!=TLB_ENTRIES){
        ptr_tlb->link = malloc(sizeof(struct tlb));
        ptr_tlb->link->index = ptr_tlb->index + 1;
        ptr_tlb = ptr_tlb->link;
        ptr_tlb->physAddr = pa;
        ptr_tlb->virtAddr = va;
        ptr_tlb->age = 0;
    //	ptr_tlb->page_number = ((uintptr_t) va) >> offset_bits;
    //	ptr_tlb->frame = ((char *) pa - physical_memory) / PGSIZE;
   	    ptr_tlb->link = NULL;
    }
    
   	// find a TLB entry to replace
   	else{
   		int maxAge = 0;
   		int maxAge_index = 0;
   		
   		// find the max age value
        ptr_tlb = tlb_store;
   		while(ptr_tlb != NULL) {
   			if(ptr_tlb->age > maxAge) {
   				maxAge = ptr_tlb->age;
   				maxAge_index = ptr_tlb->index;
   			}
            ptr_tlb = ptr_tlb->link;
   		}
   		
   		// replace the entry from the linked list with updated data
   		if(maxAge_index == 0) {
   			tlb_store->age = 0;
            tlb_store->virtAddr = va;
            tlb_store->physAddr = pa;
   		} else {
            //if it is not the first element in the list, we must increment to the maxAge_index and replace the oldest entry
			for(ptr_tlb = tlb_store; ptr_tlb != NULL; ptr_tlb = ptr_tlb->link) {
	   			if(ptr_tlb->index == maxAge_index) {
	   				ptr_tlb->age = 0;
                    ptr_tlb->virtAddr = va;
                    ptr_tlb->physAddr = pa;
	   				break;
	   			}
	   		}
   		}
   		
   		// lower the index of each TLB entry after the removed entry
   		// while(ptr_tlb != NULL) {
   		// 	tlb->index--;
   		// 	tlb = tlb->next;
   		// }
   	}

    return -1;
}


/*
 * Part 2: Check TLB for a valid translation.
 * Returns the physical page address.
 * Feel free to extend this function and change the return type.
 */
pte_t *check_TLB(void *va) {

    /* Part 2: TLB lookup code here */
    checks++;

	if(tlb_store == NULL) {
		misses++;
		return NULL;
	}

	struct tlb *ptr_tlb; 
	
	for(ptr_tlb = tlb_store; ptr_tlb != NULL; ptr_tlb = tlb_store->link) {
		if(ptr_tlb->virtAddr == va) {
			ptr_tlb->age = 0;
            return ptr_tlb->physAddr;
            
		} else {
			// increment last_used of every other TLB entry
			ptr_tlb->age++;
    	}
	
	
	}
    //If it has reached here then it could not find it, meaning we return null and increment misses.
	
	misses++;
	
	return NULL;





}


/*
 * Part 2: Print TLB miss rate.
 * Feel free to extend the function arguments or return type.
 */
void print_TLB_missrate()
{
    double miss_rate = 0;	

    /*Part 2 Code here to calculate and print the TLB miss rate*/

    miss_rate =  misses/checks; 


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
    uintptr_t virtual_address = (uintptr_t) va;
    pte_t *checker = check_TLB(va);
    //Since we are checking before instead of after performing numerous operations we save some runtime.
    int offset = virtual_address & ((1 << offset_bits) -1);
    if(checker != NULL) {
        //if found then return
    	return (pte_t *) ((char *) checker + offset);
      
    } else {
        //create the physical address and add it to the TLB since it was not found, TLB miss
	    int firstLevel = virtual_address & ((1 << offset_bits) - 1);
        int secondLevelIndex = virtual_address >> (32 - outer_bits);
		int page_table_index = (virtual_address >> offset_bits) & ((1 << (inner_bits)) - 1);
		pte_t *page_table = inner_page_tables[pgdir[secondLevelIndex]];
		pte_t index = page_table[page_table_index];
		
		void *pa = index * PGSIZE + physical_memory + firstLevel; 
		
		add_TLB(va, pa);
        
		return (pte_t *) pa;
    	
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
int page_map(pde_t *pgdir, void *va, void *pa)
{

    /*HINT: Similar to translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping */

    return -1;
}


/*Function that gets the next available page
*/
void *get_next_avail(int num_pages) {
 
    //Use virtual address bitmap to find the next free page
	for(int x = 0; x<total_frames; x++){
		int enoughtSpace = 0;
		for(int y = x; y < num_pages+x; y++) {
			if(!get_bit_at_index(virtual_address_bitmap, y)) {
				enoughtSpace++;
			}
		}
		if(enoughtSpace == num_pages ){
			for(int y = x; y < num_pages+x; y++) {
				set_bit_at_index(virtual_address_bitmap, y);
			}
			return (void *) (x*PGSIZE);
		}
	}
	return NULL;
}

void set_bit_at_index(char *bitmap, int index)
{
    //Implement your code here	
    int element = index /8;
    bitmap[element] |= (1 << (index%8));
    return;
}

int get_bit_at_index(char *bitmap, int index)
{
    //Get to the location in the character bitmap array
    //Implement your code here
    int element = index /8;
    int bit = bitmap[element] & (1 << (index%8));
    return bit >> (index%8);
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
    * page directory. Next, using get_next_avail(),
	* check if there are free pages. If free pages are available, 
	* set the bitmaps and map a new page. Note, you will 
    * have to mark which physical pages are used. 
    */
   	pthread_mutex_lock(&mutex);
	
   	if(physical_memory == NULL)
		set_physical_mem();

	int pages_required = num_bytes/PGSIZE;
	if(num_bytes%PGSIZE >0)
		pages_required++;
	
	void *virtual_address = get_next_avail(pages_required);
	if(virtual_address== NULL)
		return NULL;
    
    int outer_index = (uintptr_t)virtual_address >> (32- (offset_bits+inner_bits));


    int inner_index = (uintptr_t)virtual_address <<outer_bits;
    inner_index >>= (32-inner_bits);
    

	

	pthread_mutex_unlock(&mutex);
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


}


/*Given a virtual address, this function copies the contents of the page to val*/
void get_value(void *va, void *val, int size) {

    /* HINT: put the values pointed to by "va" inside the physical memory at given
    * "val" address. Assume you can access "val" directly by derefencing them.
    */


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



#include "my_vm.h"

#define DEBUG 0

int outer_directory_size;
int inner_table_size;
int outer_bits;
int inner_bits;
int offset_bits;
char *directory_bitmap;
char *table_bitmap;
int total_frames;


/*
Function responsible for allocating and setting your physical memory 
*/

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void set_physical_mem() {
	//Allocate physical memory using mmap or malloc; this is the total size of
    //your memory you are simulating

	physical_memory = mmap(NULL, MEMSIZE, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);

    //HINT: Also calculate the number of physical and virtual pages and allocate
    //virtual and physical bitmaps and initialize them
	offset_bits = (int)log2(PGSIZE);
	int address_bits = (int)log2(MAX_MEMSIZE);
	int virtual_page_bits = address_bits-offset_bits;
	outer_bits = virtual_page_bits/2;
	inner_bits = address_bits-offset_bits-outer_bits;

	outer_directory_size = 1<<outer_bits;
	inner_table_size = 1<<inner_bits;
	total_pages = outer_directory_size * inner_table_size;
	outer_directory_table = (pde_t *) malloc(sizeof(pde_t) * outer_directory_size);
	inner_page_tables = (pte_t **) malloc(sizeof(pte_t *) * outer_directory_size);
	
	virtual_address_bitmap = (char *) malloc(total_pages / 8);
	directory_bitmap = (char *) malloc(outer_directory_size / 8);
	table_bitmap = (char *) malloc(outer_directory_size / 8);
	total_frames = MEMSIZE/PGSIZE;
	physical_address_bitmap = (char *) malloc(total_frames / 8);
	
	tlb_store = malloc(sizeof(struct tlb)*TLB_ENTRIES);
	tlb_bitmap = (char *) malloc(TLB_ENTRIES / 8);
	checks = 0;
	misses = 0;
}


/*
 * Part 2: Add a virtual to physical page translation to the TLB.
 * Feel free to extend the function arguments or return type.
 */
int add_TLB(void *va, void *pa){
	if(tlb_store == NULL){
		tlb_store = malloc(sizeof(struct tlb)*TLB_ENTRIES);
		tlb_bitmap = (char *) malloc(TLB_ENTRIES / 8);
	}
	//If va already exists but is hidden because of empty indices beforehand
	for(int x = 0; x<TLB_ENTRIES; x++){
		if(get_bit_at_index(tlb_bitmap,x) && tlb_store[x].virtual == va){
			tlb_store[x].least_used = 0;
			tlb_store[x].virtual = va;
			tlb_store[x].physical = pa;
			return 1;
		}
	}
	//Avg case when size < tlb entries and size >=1
	for(int x = 0; x<TLB_ENTRIES; x++){
		if(!get_bit_at_index(tlb_bitmap,x)){
			set_bit_at_index(tlb_bitmap,x);
			tlb_store[x].virtual = va;
			tlb_store[x].physical = pa;
			tlb_store[x].least_used = 0;
			return 1;
		}
	}
	// If it gets past the previous loop, tlb is maxed out so we proceed to eviction policy
	int maxage = 0;
	int maxindex = 0;
	for(int x = 0; x<TLB_ENTRIES; x++){
		if(tlb_store[x].least_used > maxage){
			maxage = tlb_store[x].least_used;
			maxindex = x;
		}
	}
	tlb_store[maxindex].virtual = va;
	tlb_store[maxindex].physical = pa;
	tlb_store[maxindex].least_used = 0;
	return 1;
}
/*
 * Part 2: Check TLB for a valid translation.
 * Returns the physical page address.
 * Feel free to extend this function and change the return type.
 */
void* check_TLB(void* va){
	checks++;
	for(int x = 0; x<TLB_ENTRIES; x++){
		if(get_bit_at_index(tlb_bitmap, x) && tlb_store[x].virtual == va){
			tlb_store[x].least_used = 0;
			return tlb_store[x].physical;
		}
	    tlb_store[x].least_used++; //increment least_used when index is not the target 
	}
	misses++;
	return NULL;
}
/*
 * Part 2: Print TLB miss rate.
 * Feel free to extend the function arguments or return type.
 */
void print_TLB_missrate(){
    /*Part 2 Code here to calculate and print the TLB miss rate*/
    fprintf(stderr, "TLB miss rate %lf \n", misses/checks);
}

void print_TLB(){
	for(int x = 0; x<TLB_ENTRIES; x++){
		if(get_bit_at_index(tlb_bitmap, x)){
			printf("(%p,%p)\t",tlb_store[x].virtual, tlb_store[x].physical);
		}else{
			printf("NULL\t");
		}
	}
	printf("\n");
	
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
	
    int offset = (uintptr_t)va << (outer_bits+inner_bits);
	offset >>= (outer_bits+inner_bits);
	pte_t virtual_wo_offset = (uintptr_t)va >> offset_bits;
	virtual_wo_offset <<= offset_bits;
	void* tlb_translation = check_TLB((void*)virtual_wo_offset);
	//checking if translation already exists
	if(tlb_translation != NULL) {
        //if found then return
		return (pte_t *)(tlb_translation+offset);
    } else {
        //create the physical address and add it to the TLB since it was not found, TLB miss (incremented in add_tlb)
	    int outer_index = (uintptr_t)va >> (inner_bits+offset_bits);

		int inner_index = (uintptr_t)va <<outer_bits;
		inner_index >>= (outer_bits+offset_bits);

		pde_t directory = outer_directory_table[outer_index];
		pte_t *page_table = inner_page_tables[directory];
		pte_t frame = page_table[inner_index];

		void *pa = frame * PGSIZE + physical_memory ; 

		add_TLB((void*)virtual_wo_offset, pa);
		if(DEBUG)
			print_TLB();
		pa+=offset;
		return (pte_t *) pa;
    	
    }
    return NULL; 
}


/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int page_map(pde_t *pgdir, void *va, void *pa){	
	
    /*HINT: Similar to translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping */
    int outer_index = (uintptr_t)va >> offset_bits;
	outer_index >>= inner_bits;

    int inner_index = (uintptr_t)va <<outer_bits;
    inner_index >>= (outer_bits +offset_bits);

	pde_t directory = outer_directory_table[outer_index];
	pte_t *table = inner_page_tables[directory];
	if(!get_bit_at_index(table_bitmap, outer_index)) {
		set_bit_at_index(table_bitmap, outer_index);
		inner_page_tables[directory] = (pte_t *) malloc(sizeof(pte_t) * inner_table_size);
		table = inner_page_tables[directory];
		
	}
	table[inner_index] = ((char *) pa - physical_memory) / PGSIZE;
	
	return 1;
}


/*Function that gets the next available page
*/
void *get_next_avail(int num_pages){
 
    //Use virtual address bitmap to find the next free page
	for(int x = 1; x<=total_pages-num_pages; x++){
		int enoughtSpace = 0;
		for(int y = 0; y < num_pages; y++) {
			if(!get_bit_at_index(virtual_address_bitmap, x+y)) {
				enoughtSpace++;
			}
			if(enoughtSpace == num_pages){
				for(int z = x; z < num_pages+x; z++) {
					set_bit_at_index(virtual_address_bitmap, z);
				}
				return (void *) (x*PGSIZE);
			}
		}
		
	}
	return NULL;
}

void set_bit_at_index(char *bitmap, int index){
    int element = index /8;
    bitmap[element] |= (1 << (index%8));
    return;
}

void clear_bit_at_index(char *bitmap, int index){
    int element = index /8;
    bitmap[element] &= ~(1 << (index%8));
    return;
}

int get_bit_at_index(char *bitmap, int index){
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
	pthread_mutex_lock(&mutex);
   	if(physical_memory == NULL){
		set_physical_mem();
	}

   /* 
    * HINT: If the page directory is not initialized, then initialize the
    * page directory. Next, using get_next_avail(),
	* check if there are free pages. If free pages are available, 
	* set the bitmaps and map a new page. Note, you will 
    * have to mark which physical pages are used. 
    */
	int pages_required = num_bytes/PGSIZE;
	if(num_bytes%PGSIZE){
		pages_required++;
	}

	void *virtual_address = get_next_avail(pages_required);

    int outer_index = (uintptr_t)virtual_address >> (inner_bits+offset_bits);
    
    if(!get_bit_at_index(directory_bitmap, outer_index)) {
		set_bit_at_index(directory_bitmap, outer_index);
		outer_directory_table[outer_index] = outer_index;
		}
	
	// find an available frame for each page and map them together
	int page_to_map = 0;
	for(int i = 0; i < total_frames; i++) {
		if(!get_bit_at_index(physical_address_bitmap, i)) {
			set_bit_at_index(physical_address_bitmap, i);
			page_map(outer_directory_table, virtual_address+(page_to_map*PGSIZE), physical_memory+(i*PGSIZE));
			page_to_map++;
			if(page_to_map == pages_required)
				break;
		}
	}

	pthread_mutex_unlock(&mutex);
    return virtual_address;
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
	int offset = (uintptr_t)va << (outer_bits+inner_bits);
	offset >>= (outer_bits+inner_bits);
	if(offset) return;

    int page_count = size/PGSIZE;
	if(size%PGSIZE >0)
		page_count++;

	for(int x = 0; x < page_count; x++) {
		va =  va + (x * PGSIZE);
		int outer_index = (uintptr_t)va >> (outer_bits+offset_bits);
		int inner_index = (uintptr_t)va <<outer_bits;
		inner_index >>= (outer_bits+offset_bits);
		int page_bit = (uintptr_t) va / PGSIZE + x;

		
		pthread_mutex_lock(&mutex);
		if(get_bit_at_index(virtual_address_bitmap, page_bit)){
			clear_bit_at_index(virtual_address_bitmap, page_bit);

			pde_t directory = outer_directory_table[outer_index];
			pte_t *table = inner_page_tables[directory];
			int frame = table[inner_index];
		
			if(get_bit_at_index(physical_address_bitmap, frame))
				clear_bit_at_index(physical_address_bitmap, frame);

		} 
		for(int y = 0; y<TLB_ENTRIES; y++){
			if(get_bit_at_index(tlb_bitmap, y) && tlb_store[y].virtual==va){
				clear_bit_at_index(tlb_bitmap, y);
				tlb_store[y].virtual = NULL;
				tlb_store[y].physical = NULL;
				tlb_store[y].least_used = 0;
			}
		}
		pthread_mutex_unlock(&mutex);
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
	int offset = (uintptr_t)va << (outer_bits+inner_bits);
	offset >>= (outer_bits+inner_bits);
	int page_bit = ((uintptr_t) va) / PGSIZE;

	pthread_mutex_lock(&mutex);
	if(!get_bit_at_index(virtual_address_bitmap, page_bit)){
		pthread_mutex_unlock(&mutex);
		return;
	}
	void *physical_address = translate(outer_directory_table, va);

	int rest_of_page = (PGSIZE - offset);
	if(size <= rest_of_page) {
		memcpy(physical_address, val, size);
		pthread_mutex_unlock(&mutex);
		return;
	}else{
		memcpy(physical_address, val, rest_of_page);
		size-=rest_of_page;
	}

	va += rest_of_page;
	val += rest_of_page;
	while(size>PGSIZE){
		if(!get_bit_at_index(virtual_address_bitmap, ((uintptr_t) va) / PGSIZE)){
			pthread_mutex_unlock(&mutex);
			return;
		} 
		physical_address = translate(outer_directory_table, va);
		memcpy(physical_address, val, PGSIZE);
		size -= PGSIZE;
		va += PGSIZE;
		val += PGSIZE;
	}

	if(!get_bit_at_index(virtual_address_bitmap, ((uintptr_t) va) / PGSIZE)){
		pthread_mutex_unlock(&mutex);
		return;
	} 
	physical_address = translate(outer_directory_table, va);
	memcpy(physical_address, val, size);
	pthread_mutex_unlock(&mutex);	

}


/*Given a virtual address, this function copies the contents of the page to val*/
void get_value(void *va, void *val, int size) {

    /* HINT: put the values pointed to by "va" inside the physical memory at given
    * "val" address. Assume you can access "val" directly by derefencing them.
    */

	int offset = (uintptr_t)va << (outer_bits+inner_bits);
	offset >>= (outer_bits+inner_bits);
	int page_bit = ((uintptr_t) va) / PGSIZE;

	pthread_mutex_lock(&mutex);
	if(!get_bit_at_index(virtual_address_bitmap, page_bit)){
		pthread_mutex_unlock(&mutex);
		return;
	}
	void *physical_address = translate(outer_directory_table, va);

	int rest_of_page = (PGSIZE - offset);
	if(size <= rest_of_page) {
		memcpy(val, physical_address, size);
		pthread_mutex_unlock(&mutex);
		return;
	}else{
		memcpy(val, physical_address, rest_of_page);
		size-=rest_of_page;
	}

	va += rest_of_page;
	val += rest_of_page;
	while(size>PGSIZE){
		if(!get_bit_at_index(virtual_address_bitmap, ((uintptr_t) va) / PGSIZE)){
			pthread_mutex_unlock(&mutex);
			return;
		} 
		physical_address = translate(outer_directory_table, va);
		memcpy(val, physical_address, PGSIZE);
		size -= PGSIZE;
		va += PGSIZE;
		val += PGSIZE;
	}

	if(!get_bit_at_index(virtual_address_bitmap, ((uintptr_t) va) / PGSIZE)){
		pthread_mutex_unlock(&mutex);
		return;
	} 
	physical_address = translate(outer_directory_table, va);
	memcpy(val, physical_address, size);
	pthread_mutex_unlock(&mutex);	
   

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
			if(DEBUG){
				int getting;
				get_value((void *)address_c, &getting, sizeof(int));
				printf("(%d, %d, %d)%d ", a, b, c, getting);
			}
        }
		if(DEBUG)
			printf("\n");
    }
}




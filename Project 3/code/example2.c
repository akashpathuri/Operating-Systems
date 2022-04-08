#include "example2.h"

/*
    To do list
    1. figure out data structures
    2. SetPhysicalMem(), Translate(), PageMap()
    3. myalloc(), myfree()
    4. matrix multiplication
    5. implemenet direct mapped TLB
*/

int DEBUG = 0;
int nextPage = -1;
struct pair pairAddrPairs[262144];
pthread_mutex_t lock;
int timeTLB = 0;
/*
Function responsible for allocating and setting your physical memory 
*/
void SetPhysicalMem() {
    //Allocate physical memory using mmap or malloc; this is the total size of
    //your memory you are simulating
    physicalMem = malloc(MEMSIZE * sizeof(char));  //had it as MEM_SIZE, I think it is just MEMSIZE though
    //virtualMem = malloc(MAX_MEMSIZE * sizeof(char));

    memset(physicalMem, '0', MEMSIZE * sizeof(char));
    //memset(virtualMem, '0', MAX_MEMSIZE * sizeof(char));
    //HINT: Also calculate the number of physical and virtual pages and allocate
    //virtual and physical bitmaps and initialize them
    //physBitMap = malloc( NUM_PHYS_PGS * sizeof(char));
    virtBitMap = malloc( NUM_VIRT_PGS * sizeof(char));

    //memset(physBitMap, '0', NUM_PHYS_PGS * sizeof(char));
    memset(virtBitMap, '0', NUM_VIRT_PGS * sizeof(char));

    int i;
    for(i = 0; i < 262144; i++){
        pairAddrPairs[i].used = -1;
        pairAddrPairs[i].addr = 1;
        pairAddrPairs[i].pages = NULL;
        pairAddrPairs[i].numPages = -1;
    }

    //initialize mutex lock
    if(pthread_mutex_init(&lock, NULL) != 0){
        printf("mutex init failure\n");
    }

    //initialize tlb
    for(i = 0; i < TLB_SIZE; i++){
        tlb_store.entries[i].used = -1;
        tlb_store.entries[i].time = -1;
        tlb_store.entries[i].physAddr = NULL;
        tlb_store.entries[i].virtAddr = NULL;
    }
    tlb_store.currentLoad = 0;
    tlb_store.missRate = 0;
    tlb_store.hits = 0;
    tlb_store.misses = 0;

    if(DEBUG)printf("phys mem set\n");
}



/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t * Translate(pde_t *pgdir, void *va) {
    //HINT: Get the Page directory index (1st level) Then get the
    //2nd-level-page table index using the virtual address.  Using the page
    //directory index and page table index get the physical address
    
    //uintptr_t virtualAddress = (uintptr_t)va;
    timeTLB++;
    uintptr_t virtualAddress = (pte_t)va;
    unsigned long virtualAddrLU = (unsigned long)va;

    //check tlb for va, if not there then add mapping
    int i;
    pte_t* mapping;
    
    mapping = check_TLB(va);
    if(mapping != NULL){
        return mapping;
    }
    
    //aint there dawg
    //int outerIndex = virtualAddress >> 22; //bit shift 22 times to get outer
    int outerIndex = (((1 << 10) - 1) & (virtualAddress >> (23 - 1)));
    int innerIndex = (((1 << 10) - 1) & (virtualAddress >> (13 - 1)));
    mapping = (pgdir + outerIndex * 1024 + innerIndex);

    add_TLB(va, mapping);
    return (pte_t*)mapping;
}


/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int
PageMap(pde_t *pgdir, void *va, void *pa)
{

    /*HINT: Similar to Translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping */
    pthread_mutex_lock(&lock);
    //uintptr_t virtualAddress = (uintptr_t)va;
    pte_t virtualAddress = (pte_t)va;

    int outerIndex = (((1 << 10) - 1) & (virtualAddress >> (23 - 1)));
    int innerIndex = (((1 << 10) - 1) & (virtualAddress >> (13 - 1)));

    pte_t* mapping = (pgdir + outerIndex * 1024 + innerIndex);

    //no mapping = set to pa
    if(*mapping == 0){
        mapping = (pte_t*)pa;
        pthread_mutex_unlock(&lock);
        return 1;
    }
    pthread_mutex_unlock(&lock);
    //has mapping just return 0
    return 0;
}


/*Function that gets the next available page
*/
void *get_next_avail(int num_pages) {
 
    //Use virtual address bitmap to find the next free page
    //walk thru virt page bitmap
    int i = 0, j = i, jCount = 0, success = 0;

    for(i = 0; i < NUM_VIRT_PGS; i++){
        jCount = 0, success = 0;
        //candidate
        if(virtBitMap[i] == '0'){
            //check for the contigs
            for(j = i; j < NUM_VIRT_PGS; j++){
                if(virtBitMap[j] == '0'){
                    jCount++;
                    //check if we found all we need
                    if(jCount == num_pages){
                        success = 1;
                        break;
                    }
                }
                else{
                    break;
                }
            }
            //found them
            if(success == 1){
                if(DEBUG)printf("next avail page %d\n", i);
                nextPage = i;
                break;
            }
        }
    }

    if(success == 0){
        nextPage = -1;
    }
    return NULL;
}


/* Function responsible for allocating pages
and used by the benchmark
*/
void *myalloc(unsigned int num_bytes) {

    //HINT: If the physical memory is not yet initialized, then allocate and initialize.
    //if(DEBUG) printf("%d\n", numPages);
    pthread_mutex_lock(&lock);
   /* HINT: If the page directory is not initialized, then initialize the
   page directory. Next, using get_next_avail(), check if there are free pages. If
   free pages are available, set the bitmaps and map a new page. Note, you will 
   have to mark which physical pages are used. */
    if(physicalMem == NULL){
        SetPhysicalMem();
    }

    if(pageDirectory == NULL) {
       pageDirectory = malloc(1024 * 1024 * sizeof(pte_t*));
       memset(pageDirectory, 0, 1024 * 1024 * sizeof(pte_t*));
    }

    int numPages = (num_bytes / PGSIZE) + 1;
    if(num_bytes % PGSIZE == 0) numPages--;
    //get next page to use
    get_next_avail(numPages);
    if(nextPage == -1){ //can't find any
        if(DEBUG)printf("malloc error: no space\n");
        pthread_mutex_unlock(&lock);
        return NULL;
    }
    //set bitmap for the pages
    int i = nextPage, j = 0;
    while(j < numPages){
        virtBitMap[i] = '1';
        i++;
        j++;
    }

    pde_t pageAddr = (pde_t)physicalMem + (PGSIZE * nextPage);

    int outerIndex = (((1 << 10) - 1) & (pageAddr >> (23 - 1)));
    int innerIndex = (((1 << 10) - 1) & (pageAddr >> (13 - 1)));

    pte_t* mapping = (pageDirectory + outerIndex * 1024 + innerIndex);

    if(*mapping == 0){
        mapping = (pte_t*)pageAddr;
    }

    //fill in part in pairs list
    unsigned long toCompare = -1;
    for(i = 0; i < 262144; i++){
        //found a place
        if(pairAddrPairs[i].used == -1){
            //give it the addres
            pairAddrPairs[i].addr = (unsigned long)pageAddr;
            //give it all the pages it owns
            pairAddrPairs[i].pages = malloc(numPages * sizeof(int));
            for(j = 0; j < numPages; j++){
                pairAddrPairs[i].pages[j] = nextPage + j;
            }
            //call it used
            pairAddrPairs[i].used = 1;
            //mark how many pages it has
            pairAddrPairs[i].numPages = numPages;
            break;
        }
    }

    if(DEBUG)printf("MALLOC PAGEADDR:%lu\n", pageAddr);
    pthread_mutex_unlock(&lock);
    return (void*)pageAddr;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
void myfree(void *va, int size) {
    pthread_mutex_lock(&lock);
    //Free the page table entries starting from this virtual address (va)
    pte_t* physAddr = Translate(pageDirectory, va);
    unsigned long physAddrNum = (unsigned long)physAddr;
    unsigned long vaNum = (unsigned long)va;
    if(DEBUG)printf("MUST FREE VA:%ld HEX VALUE:%p\n", physAddrNum, va);
    if(DEBUG)printf("(FREE) VAL AT PHYS ADDR:%ld ADDR:%lu\n", *physAddr, physAddrNum);
    //get bits for addr
    int outerIndex = (((1 << 10) - 1) & (physAddrNum >> (23 - 1)));
    int innerIndex = (((1 << 10) - 1) & (physAddrNum >> (13 - 1)));
    //get page start to free
    uintptr_t physicalMemStart = (uintptr_t)physicalMem;
    unsigned int pageStartPoint = (physAddrNum - physicalMemStart) / PGSIZE;
    if(DEBUG)printf("FREE PAGE: %u\n", pageStartPoint);
    int i, j;
    for(i = 0; i < 262144; i++){
        if(pairAddrPairs[i].used == 1) {
            if(pairAddrPairs[i].addr == vaNum){
                for(j = 0; j < pairAddrPairs[i].numPages; j++){
                    if(pairAddrPairs[i].pages[j] == '0'){
                        //can't free all this memory
                        printf("free error not all valid memory\n");
                        return;
                    }
                    virtBitMap[pairAddrPairs[i].pages[j]] = '0';
                }
            }

            pairAddrPairs[i].used = -1;
            pairAddrPairs[i].addr = 1;
            free(pairAddrPairs[i].pages);
            pairAddrPairs[i].numPages = -1;
        }
    }
    pthread_mutex_unlock(&lock);
    // // Also mark the pages free in the bitmap
    // //Only free if the memory from "va" to va+size is valid
    // i = pageStartPoint, j = 0;

    // int numPages = (size / PGSIZE) + 1;
    // if(size % PGSIZE == 0) numPages--;
    // //check for validity of pages
    // while(j < numPages){
    //     if(virtBitMap[i] == '0'){
    //         printf("free error, not all valid memory\n");
    //         return;
    //     }
    //     i++;
    //     j++;
    // }
    // //free
    // i = pageStartPoint, j = 0;

    // while(j < numPages){
    //     virtBitMap[i] = '0';
    //     i++;
    //     j++;
    // }

    // pte_t* mapping = (pageDirectory + outerIndex * 1024 + innerIndex);
    // *mapping = 0;
}


/* The function copies data pointed by "val" to physical
 * memory pages using virtual address (va)
*/
void PutVal(void *va, void *val, int size) {

    /* HINT: Using the virtual address and Translate(), find the physical page. Copy
       the contents of "val" to a physical page. NOTE: The "size" value can be larger
       than one page. Therefore, you may have to find multiple pages using Translate()
       function.*/
    pte_t* physAddr = Translate( pageDirectory, va );
    memcpy( physAddr, val, size );
}


/*Given a virtual address, this function copies the contents of the page to val*/
void GetVal(void *va, void *val, int size) {

    /* HINT: put the values pointed to by "va" inside the physical memory at given
    "val" address. Assume you can access "val" directly by derefencing them.
    If you are implementing TLB,  always check first the presence of translation
    in TLB before proceeding forward */
    pte_t* physAddr = Translate( pageDirectory, va ); 
    memcpy( val, physAddr, size );
}



/*
This function receives two matrices mat1 and mat2 as an argument with size
argument representing the number of rows and columns. After performing matrix
multiplication, copy the result to answer.
*/
void MatMult(void *mat1, void *mat2, int size, void *answer) {

    /* Hint: You will index as [i * size + j] where  "i, j" are the indices of the
    matrix accessed. Similar to the code in test.c, you will use GetVal() to
    load each element and perform multiplication. Take a look at test.c! In addition to 
    getting the values from two matrices, you will perform multiplication and 
    store the result to the "answer array"*/

    //allocate storage of each matrices' numbers and matrix for final answer
    pthread_mutex_lock(&lock);
    int* resultVector = malloc(size * size * sizeof(int));
    int* mat1Vector = malloc(size * size * sizeof(int));
    int* mat2Vector = malloc(size * size * sizeof(int));

    int i, j, r = 0;
    int temp1, temp2;

    //copy values into malloc'd vectors
    for(i = 0; i < size; i++){
        for(j = 0; j < size; j++){
            uintptr_t mat1addr = (uintptr_t)mat1 + (((i * size) * sizeof(int)) + (j * sizeof(int)));
            uintptr_t mat2addr = (uintptr_t)mat2 + (((i * size) * sizeof(int)) + (j * sizeof(int)));
            GetVal((void*)mat1addr, &temp1, sizeof(int));
            GetVal((void*)mat2addr, &temp2, sizeof(int));
            mat1Vector[(i * size) + j] = temp1;
            mat2Vector[(i * size) + j] = temp2;
        }
    }

    //do the multiplication using mat1/mat2 vectors
    for(i = 0; i < size; i++){
        for(j = 0; j < size; j++){
            resultVector[(i * size) + j] = 0;
            for(r = 0; r < size; r++){
                resultVector[(i * size) + j] += (mat1Vector[(i * size) + r] * mat2Vector[(r * size) + j]);
            }
        }
    }

    //put back into answer from arguments
    for(i = 0; i < size; i++){
        for(j = 0; j < size; j++){
            uintptr_t resultAddr = (uintptr_t)answer + (((i * size) * sizeof(int)) + (j * sizeof(int)));
            PutVal((void*)resultAddr, (void*)&resultVector[(i*size) + j], sizeof(int)); 
        }
    }
    pthread_mutex_unlock(&lock);
}
/*
 * Part 2: Add a virtual to physical page translation to the TLB.
 * Feel free to extend the function arguments or return type.
 */


int
add_TLB(void *va, void *pa)
{

    /*Part 2 HINT: Add a virtual to physical page translation to the TLB */
    int i;
    //if can just add an entry freely
    if(tlb_store.currentLoad < TLB_SIZE){
        for(i = 0; i < TLB_SIZE; i++){
            if(tlb_store.entries[i].used == -1){
                tlb_store.entries[i].used == 1;
                tlb_store.entries[i].virtAddr = va;
                tlb_store.entries[i].physAddr = pa;
            }
        }
        tlb_store.currentLoad++;
    }
    //if an eviction is needed
    else{
        int lowest = 2147483647;
        int lowestIndex = -1;
        for(i = 0; i < TLB_SIZE; i++){
            //found candidate for LRU
            if(tlb_store.entries[i].time < lowest){
                lowest = tlb_store.entries[i].time;
                lowestIndex = i;
            }
        }

        tlb_store.entries[lowestIndex].virtAddr = va;
        tlb_store.entries[lowestIndex].physAddr = pa;
        tlb_store.entries[lowestIndex].time = timeTLB;
    }

    return -1;
}


/*
 * Part 2: Check TLB for a valid translation.
 * Returns the physical page address.
 * Feel free to extend this function and change the return type.
 */
pte_t *
check_TLB(void *va) {

    /* Part 2: TLB lookup code here */
    int i;
    for( i = 0; i < TLB_SIZE; i++){
        if(va == tlb_store.entries[i].virtAddr){
            tlb_store.hits++;
            tlb_store.entries[i].time = timeTLB;
            return tlb_store.entries[i].physAddr;
        }
    }

    tlb_store.misses++;
    return NULL;
}


/*
 * Part 2: Print TLB miss rate.
 * Feel free to extend the function arguments or return type.
 */
void
print_TLB_missrate()
{
    double miss_rate = 0;

    /*Part 2 Code here to calculate and print the TLB miss rate*/
    miss_rate = (double)tlb_store.hits / (double)tlb_store.misses;

    fprintf(stderr, "TLB miss rate %lf \n", miss_rate);
}

//sets the bit to 1 in virtual bit map, indicating it is in use
void oneBit( int page )
{
    int index = page / 32;
    int pos = page % 32;

    virtBitMap[index] = virtBitMap[index] | ( 1 << pos );
}

//sets the bit to 0 in virtual bit map, indicating not in use
void zeroBit( int page )
{
    int index = page / 32;
    int pos = page % 32;

    virtBitMap[index] = virtBitMap[index] & ~( 1 << pos );
}

//tests whether a certain bit is 0 or 1, to see if the bit is taken or not
int isBit( int page )
{
    int index = page / 32;
    int pos = page % 32;

    if( virtBitMap[index] & ( 1 << pos ) )
    {
        return 1;
    }
    else
    {
        return 0;
    }
    
}
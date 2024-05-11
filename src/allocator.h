#ifndef ALLOC_H
#define ALLOC_H

#include <stdint.h>
#include <stdalign.h>

#ifdef allocator_debug_enable
    #include <stdio.h>
    #define __allocdebugprintf(...) printf(__VA_ARGS__)
#else
    #define __allocdebugprintf(...)
#endif

// pointer to the base of the heap, where allocations will start at
void* __heap_base;

// pointer to the top of the heap, where the sector data will start at
void* __heap_top;

typedef union {
    uint32_t raw;
    struct {
        uint32_t allocated: 1;
        // true if the next sector (below this one in memory) exists
        uint32_t next_sector_exists: 1;
        // flag for use in garbage collection
        uint32_t sector_is_reachable: 1;
        // number of bytes allocated * allocation alignment
        uint32_t allocation_size: 29;
    } fields;
} __heap_sector_data_t;

#define __heap_minimum_allocation_size 1

#define __heap_allocation_alignment alignof(max_align_t)

// set to log_2 of heap allocation alignment
#define __heap_allocation_alignment_shift 4

#define __heap_maximum_allocation_size ((1<<29)-1)

void* __user_ptr_from_sector(__heap_sector_data_t* sector) {
    __heap_sector_data_t* sector_cur = (__heap_sector_data_t*)__heap_top;
    char* data_ptr = (char*)__heap_base;
    while ( sector != sector_cur ) {
        data_ptr += sector_cur->fields.allocation_size * __heap_allocation_alignment;
        if ( !sector_cur->fields.next_sector_exists ) { return NULL; }
        sector_cur -= 1;
    }
    return (void*)data_ptr;
}

__heap_sector_data_t* __heap_sector_from_user_pointer(void* usr_ptr) {

    __heap_sector_data_t* sector_cur = (__heap_sector_data_t*)__heap_top;
    size_t user_byte_idx = (char*)usr_ptr - (char*)__heap_base;
    size_t byte_idx = 0;
    if ( byte_idx == user_byte_idx ) { return sector_cur; }
    do {
        byte_idx += sector_cur->fields.allocation_size * __heap_allocation_alignment;
        sector_cur -= 1;
        if ( byte_idx == user_byte_idx ) { return sector_cur; }
    } while ( sector_cur->fields.next_sector_exists );

    return NULL;
}

size_t heap_used_bytes() {
    __heap_sector_data_t*  sector_cur = (__heap_sector_data_t*)__heap_top;
    size_t ret = 0;

    while ( 1 ) {
        ret += (sector_cur->fields.allocation_size * __heap_allocation_alignment) + sizeof(__heap_sector_data_t);
        if ( !sector_cur->fields.next_sector_exists ) { break; }
        sector_cur -= 1;
    }

    return ret;
}

size_t heap_n_allocs() {
    size_t n_allocations = 0;
    __heap_sector_data_t* sector_cur = (__heap_sector_data_t*)__heap_top;
    
    if ( !sector_cur->fields.next_sector_exists && !sector_cur->fields.allocated ) { return 0; }

    do {
        n_allocations += 1;
        if ( !sector_cur->fields.next_sector_exists ) {break;}
        sector_cur -= 1;
    } while(1);

    return n_allocations;
}

size_t heap_size() {
    return ((char*)__heap_top) - ((char*)__heap_base);
}

void __heap_remove_unused_chunks() {
    
}

void memalloc_init(void* heap_base, size_t heap_size) {
    
    // initialize heap variables
    __heap_base = heap_base;\
    __heap_top = (void*)((char*)__heap_base + heap_size - sizeof(__heap_sector_data_t));
    
    // initialize top pointer
    __heap_sector_data_t* heap_top_ptr = (__heap_sector_data_t*)__heap_top;
    heap_top_ptr->fields.allocated = 0;
    heap_top_ptr->fields.allocation_size = 0;
    heap_top_ptr->fields.sector_is_reachable = 0;
    heap_top_ptr->fields.next_sector_exists = 0;

}

void* memalloc(size_t _size) {

    __allocdebugprintf("memalloc init:\n");

    size_t size_alloc = _size;

    // align the allocation
    size_alloc += (__heap_allocation_alignment - 1);
    size_alloc >>= __heap_allocation_alignment_shift;

    __allocdebugprintf("\tbytes used for allocation: %llu\n", size_alloc * __heap_allocation_alignment);

    if ( size_alloc > __heap_maximum_allocation_size ) {
        __allocdebugprintf("\tallocation size is greater than the maximum, exiting\n");
        return NULL;
    }

    size_alloc = size_alloc > __heap_minimum_allocation_size ? size_alloc : __heap_minimum_allocation_size;

    // search for existing sector that could work
    __heap_sector_data_t* sector_cur = (__heap_sector_data_t*)__heap_top;
    
    __heap_sector_data_t* smallest_viable_sector = NULL;
    uint32_t smallest_viable_sector_size = INT32_MAX;

    __allocdebugprintf("\tsearching for first available sector\n");

    while( sector_cur->fields.next_sector_exists ) {
        if ( !sector_cur->fields.allocated && sector_cur->fields.allocation_size < smallest_viable_sector_size && sector_cur->fields.allocation_size >= size_alloc ) {
            smallest_viable_sector_size = sector_cur->fields.allocation_size;
            smallest_viable_sector = sector_cur;
        }
        sector_cur -= 1;
    }

    if ( smallest_viable_sector != NULL ) {

        __allocdebugprintf("\tfound pre-existing sector that works\n\tdone\n");
        smallest_viable_sector->fields.allocated = 1;
        return __user_ptr_from_sector(smallest_viable_sector);

    }

    __allocdebugprintf("\tfinding the top of the heap\n");

    // check that there is free space in the heap for this allocation
    if ( heap_size() - heap_used_bytes() < size_alloc*__heap_allocation_alignment ) {     
        __allocdebugprintf("\tERROR: not enough space in the heap!\n");
        __allocdebugprintf("\t%llu bytes free, %llu bytes needed\n", heap_size() - heap_used_bytes(), size_alloc*__heap_allocation_alignment);
        return NULL; 
    }

    if ( (void*)sector_cur == __heap_top && !sector_cur->fields.allocated && !sector_cur->fields.next_sector_exists ) {
        __allocdebugprintf("\tallocating heap base\n");
        sector_cur->fields.allocated = 1;
        sector_cur->fields.allocation_size = size_alloc;
        sector_cur->fields.next_sector_exists = 0;
        __allocdebugprintf("\tdone\n");
        return __user_ptr_from_sector(sector_cur);
    }

    sector_cur->fields.next_sector_exists = 1;

    __allocdebugprintf("\tinitializing next sector\n");

    // iterate to the next sector
    sector_cur -= 1;

    sector_cur->fields.allocation_size = size_alloc;
    sector_cur->fields.allocated = 1;
    sector_cur->fields.next_sector_exists = 0;

    __allocdebugprintf("\tdone\n");

    return __user_ptr_from_sector(sector_cur);

}

void* memrealloc(void* ptr, size_t size_realloc) {

    __allocdebugprintf("realloc init:\n");

    __heap_sector_data_t* sector_realloc = __heap_sector_from_user_pointer(ptr);

    if ( sector_realloc == NULL ) {
        __allocdebugprintf("\tsector doesnt exist!\n");
        return NULL;
    }

    if ( size_realloc <= sector_realloc->fields.allocated ) { 
        __allocdebugprintf("\tsector can already store the data needed\n");
        return ptr; 
    }

    __allocdebugprintf("\tfreeing old memory\t");

    // mark the current sector as freed
    // but dont actually remove it from the heap so that we can keep the data if a new allocation fails

    sector_realloc->fields.allocated = 0;
    
    if ( sector_realloc != (__heap_sector_data_t*)__heap_top ) { 
        sector_realloc += 1;
        sector_realloc->fields.next_sector_exists = 0;
        sector_realloc -= 1;
    }

    __allocdebugprintf("\tallocating new memory block\n");

    void* ptr_new = memalloc(size_realloc);

    if ( ptr_new == NULL ) { 
        __allocdebugprintf("\tnot enough memory for new allocation!\n");
        
        sector_realloc->fields.allocated = 1;
        if ( sector_realloc != (__heap_sector_data_t*)__heap_top ) { 
            sector_realloc += 1;
            sector_realloc->fields.next_sector_exists = 1;
            sector_realloc -= 1;
        }
        
        return NULL;
    }

    for ( size_t i = 0; i < sector_realloc->fields.allocation_size; i++ ) {
        ((char*)ptr_new)[i] = ((char*)ptr)[i];
    }

    __allocdebugprintf("\tdone\n");

    return ptr_new;

}

void memfree(void* user_ptr) {

    __allocdebugprintf("memfree init:\n");
    
    __heap_sector_data_t* dealloc_sector = __heap_sector_from_user_pointer(user_ptr);
    if ( dealloc_sector == NULL ) { 
        __allocdebugprintf("\tcould not find heap sector cooresponding to user pointer\n");
        return; 
    }

    dealloc_sector->fields.allocated = 0;

    // if there is no sector following this one, delete it
    if ( !dealloc_sector->fields.next_sector_exists ) {
        __allocdebugprintf("\tdeleting top sector\n");
        dealloc_sector += 1; 
        dealloc_sector->fields.next_sector_exists = 0;
        __allocdebugprintf("\tdone\n");
        return;
    }

    if ( (void*)dealloc_sector == __heap_top ) {
        __allocdebugprintf("\theap top, done\n");
        return; 
    }    

    __heap_sector_data_t* sector_prev = dealloc_sector + 1;
    __heap_sector_data_t* sector_next = dealloc_sector - 1;

    if ( sector_next->fields.allocated && sector_prev->fields.allocated ) {
        __allocdebugprintf("\tdone\n");
        return;
    }

    // if 2 adjacent sectors are free (merges 3 sectors)
    if ( !sector_next->fields.allocated && !sector_prev->fields.allocated ) {

        __allocdebugprintf("\tmerging three sectors\n");
        
        uint32_t total_free_bytes = dealloc_sector->fields.allocation_size + \
                                    sector_prev->fields.allocation_size + \
                                    sector_next->fields.allocation_size;

        if ( total_free_bytes > __heap_maximum_allocation_size ) {
            uint32_t split_bytes = total_free_bytes - __heap_maximum_allocation_size;

            sector_next->fields.allocation_size = split_bytes/2;
            dealloc_sector->fields.allocation_size = split_bytes/2 + split_bytes&1;
            sector_prev->fields.allocation_size = __heap_maximum_allocation_size;
            
            __allocdebugprintf("\tdone\n");
            return;
        }

        sector_next->fields.allocation_size = 0;
        dealloc_sector->fields.allocation_size = 0;
        sector_prev->fields.allocation_size = total_free_bytes;

        __allocdebugprintf("\tdone\n");
        
        return;
    }

    // if the previous sector is free
    if ( !sector_prev->fields.allocated ) {

        __allocdebugprintf("\tmerging current sector and previous\n");

        uint32_t total_free_bytes = dealloc_sector->fields.allocation_size;
        total_free_bytes += sector_prev->fields.allocation_size;

        if ( total_free_bytes > __heap_maximum_allocation_size ) {
            
            sector_prev->fields.allocation_size = __heap_maximum_allocation_size;
            dealloc_sector->fields.allocation_size = total_free_bytes - __heap_maximum_allocation_size;

            __allocdebugprintf("\tdone\n");
            return;
        }

        dealloc_sector->fields.allocation_size = 0;
        sector_prev->fields.allocation_size = total_free_bytes;

    }

    // if the next sector is free
    if ( !sector_next->fields.allocated ) {

        __allocdebugprintf("\tmerging current sector and next\n");

        uint32_t total_free_bytes = dealloc_sector->fields.allocation_size;
        total_free_bytes += sector_next->fields.allocation_size;

        if ( total_free_bytes > __heap_maximum_allocation_size ) {
            
            sector_next->fields.allocation_size = __heap_maximum_allocation_size;
            dealloc_sector->fields.allocation_size = total_free_bytes - __heap_maximum_allocation_size;

            __allocdebugprintf("\tdone\n");
            return;
        } 

        dealloc_sector->fields.allocation_size = 0;
        sector_next->fields.allocation_size = total_free_bytes;

    }

}

void memprint() {

    __heap_sector_data_t* sector_cur = (__heap_sector_data_t*)__heap_top;
    int sector_idx = 0;
    int next_sector_exists = 0;
    do {
        next_sector_exists = sector_cur->fields.next_sector_exists;
        __allocdebugprintf("sector %i:\n", sector_idx);
        __allocdebugprintf("\tsize: %i\n", sector_cur->fields.allocation_size * __heap_allocation_alignment);
        __allocdebugprintf("\tallocated: %i\n", sector_cur->fields.allocated);
        sector_cur -= 1;
        sector_idx++;
    } while (next_sector_exists);

}

#endif
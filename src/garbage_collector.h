#pragma once

#include "allocator.h"

#include <stdint.h>

#ifdef gc_debug_enable
    #include <stdio.h>
    #define __gcdebugprintf(...) printf(__VA_ARGS__)
#else
    #define __gcdebugprintf(...)
#endif

// this is the platform specific code to get the current stack pointer
#define __gc_get_stackptr() __builtin_frame_address(0)

// this MUST be the first thing to run in program execution
#define gc_stack_init() __stack_base = (uintptr_t*)__gc_get_stackptr()

uintptr_t* __stack_base;

void __gc_scan_memory_against_allocs(uintptr_t scan_start, uintptr_t scan_end) {

    __heap_sector_data_t* sector_cur = (__heap_sector_data_t*)__heap_top;
    do {

        uintptr_t* ptr = (uintptr_t*)scan_start;
        for ( ; ptr < (uintptr_t*)scan_end; ptr += 1 ) {

            uintptr_t pointer_cur = *ptr;
            uintptr_t sector_data_start = (uintptr_t)__user_ptr_from_sector(sector_cur);
            uintptr_t sector_data_end = sector_data_start + (sector_cur->fields.allocation_size * __heap_allocation_alignment);

            // printf("pointer comparrison:\n");
            // printf("pointer addr: %llu\n", pointer_cur);
            // printf("start addr: %llu\n", sector_data_start);
            // printf("end addr: %llu\n\n", sector_data_end);
            

            if ( pointer_cur >= sector_data_start && pointer_cur < sector_data_end ) {

                if ( sector_cur->fields.allocated && !sector_cur->fields.sector_is_reachable ) {
                    __gcdebugprintf("\tmarked reachable region %i\n", (__heap_sector_data_t*)__heap_top - sector_cur);
                    sector_cur->fields.sector_is_reachable = 1;
                    __gc_scan_memory_against_allocs(sector_data_start, sector_data_end);
                }

            }   
            
        }

        if ( sector_cur->fields.next_sector_exists ) {
            sector_cur -= 1;
        } else {
            break;
        }

    } while(1);
    

}

void __gc_free_unused_memory() {

    __gcdebugprintf("GC free init:\n");

    size_t n_allocs = heap_n_allocs();
    __gcdebugprintf("\tthere are %llu allocations\n", n_allocs);

    if ( n_allocs == 0 ) {
        __gcdebugprintf("\tthere is nothing in the heap...\n");
        return;
    }

    __gcdebugprintf("\tscanning the stack\n");

    uintptr_t* stack_start = (uintptr_t*)__gc_get_stackptr();

    __heap_sector_data_t* sector_cur = (__heap_sector_data_t*)__heap_top;

    __gcdebugprintf("\tresetting all flags in heap memory\n");

    do {
        sector_cur->fields.sector_is_reachable = 0;
        if ( sector_cur->fields.next_sector_exists ) {
            sector_cur -= 1;
        } else {
            break;
        }
    } while(1);

    __gcdebugprintf("\tscanning stack against heap\n");
    __gc_scan_memory_against_allocs((uintptr_t)stack_start, (uintptr_t)__stack_base);

    sector_cur = (__heap_sector_data_t*)__heap_top;

    do {

        if ( !sector_cur->fields.sector_is_reachable && sector_cur->fields.allocated ) {
            __gcdebugprintf("\tfound unreachable region %i\n", (__heap_sector_data_t*)__heap_top - sector_cur);
            memfree(__user_ptr_from_sector(sector_cur));
        }

        if ( !sector_cur->fields.next_sector_exists ) {
            break;
        }

        sector_cur -= 1;

    } while (1);

    __gcdebugprintf("\tdone\n");

}


void *gc_malloc(size_t sz) {
    void* ret = memalloc(sz);
    if ( ret == NULL ) {
        __gc_free_unused_memory();
        return memalloc(sz);
    }
    return ret;
}

void* gc_realloc(void* ptr, size_t sz_new) {
    void* ret = memrealloc(ptr, sz_new);
    if ( ret == NULL ) {
        __gc_free_unused_memory();
        return memrealloc(ptr, sz_new);
    }
    return ret;
}

void gc_free(void* ptr) {
    memfree(ptr);
}

void gc_run() {
    __gc_free_unused_memory();
}
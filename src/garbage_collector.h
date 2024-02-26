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

void __gc_free_unused_memory() {

    __gcdebugprintf("GC free init\n");

    size_t n_allocs = heap_n_allocs();
    __gcdebugprintf("\t%llu allocations\n", n_allocs);

    if ( n_allocs == 0 ) {
        __gcdebugprintf("\tthere is nothing in the heap...\n");
        return;
    }

    uintptr_t* stack_start = (uintptr_t*)__gc_get_stackptr();

    for ( ; stack_start < (uintptr_t*)__stack_base; stack_start += 1 ) {
        
    }

}
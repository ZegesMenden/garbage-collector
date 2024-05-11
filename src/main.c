#include <stdlib.h>
#include <stdio.h>

#define gc_debug_enable
#define allocator_debug_enable

#include "allocator.h"
#include "garbage_collector.h"
#include <stdint.h>
#include <time.h>



void initfun() {

    void* hbase = malloc(1<<16);
    memalloc_init(hbase, 1<<16);

}

int main() {

    gc_stack_init();

    initfun();    

    while(1) {
        gc_malloc((int)(1<<12));
        _sleep(1);
    }

    return 0;

}
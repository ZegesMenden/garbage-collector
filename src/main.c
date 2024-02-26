#include <stdlib.h>
#include <stdio.h>
#include "allocator.h"
#include "garbage_collector.h"
#include <stdint.h>

#define gc_debug_enable
#define allocator_debug_enable

int main() {

    gc_stack_init();

    void* hbase = malloc(1024);

    memalloc_init(hbase, 1024);

    void* test = memalloc(100);

    free(hbase);


    return 0;

}
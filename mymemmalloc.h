#include <asm-generic/param.h>
#define PAGESIZE EXEC_PAGESIZE
// In asm-generic/param.h => EXEC_PAGESIZE 4096
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>


/* how many directories per (preallocated) page
 * -> (PAGESIZE/16) = 256 for 4KiB pages.
 * each path can be in the medium (PAGESIZE - 4 * 256 - 20) / 256 Bytes.
 * The notify dirlist ist dynamically grown.
 */
// #define N_VM_ELEMENTS (PAGESIZE / 16) // N_VM_ELEMENTS 256
#define N_VM_ELEMENTS (PAGESIZE / 8)

/* Avoids with uint32 the penalty of unaligned memory access */
typedef unsigned int p_rel;



 // Collect freed memory to reuse
typedef struct reuse_block {
    struct reuse_block *next;
} reuse_block_t;
typedef struct Stack {
    reuse_block_t *next;
    int size;
} my_stack_t;


// Memory allocator
typedef struct __vm {
    p_rel array[N_VM_ELEMENTS];
    struct __vm *next;
    _Atomic int max, use; 
    /* dynamic string section starts here */
    char raw[0];
} vm_t; // 4096
typedef struct vm_head {
    vm_t *next;
    my_stack_t freed[256]; // 8,16,24,...,2048     
} vm_head_t;


vm_t *vm_new();


void vm_destroy(vm_head_t *head);

static vm_t *vm_extend_map(vm_head_t *head);

uintptr_t vm_add(size_t sz, vm_head_t *head);

void vm_remove(uintptr_t ptr, int sz, struct vm_head *head);

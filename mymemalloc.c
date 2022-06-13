#include "mymemmalloc.h"

/* the macro OPTFENCE(...) can be invoked with any parameter.
 * The parameters will get calculated, even if gcc doesn't recognize
 * the use of the parameters, e.g. cause they are needed for an inlined asm
 * syscall.
 *
 * The macro translates to an asm jmp and a function call to the function
 * opt_fence, which is defined with the attribute "noipa" -
 * (the compiler "forgets" the function body, so gcc is forced to generate
 * all arguments for the function.)
 */
void __attribute__((noipa, cold, naked)) opt_fence(void *p, ...) {}
#define _optjmp(a, b) __asm__(a "OPTFENCE_" #b)
#define _optlabel(a) __asm__("OPTFENCE_" #a ":")
#define __optfence(a, ...)  \
    _optjmp("jmp ", a);     \
    opt_fence(__VA_ARGS__); \
    _optlabel(a)
#define OPTFENCE(...) __optfence(__COUNTER__, __VA_ARGS__)


static inline char *_getaddr(p_rel *i, p_rel addr)
{
    return ((char *) i + addr);
}
/* translate a relative pointer to an absolute address */
#define getaddr(addr) _getaddr(&addr, addr) // &addr + addr

static inline p_rel _setaddr(p_rel *i, char *p)
{
    return (*i = (p - (char *) i));
}
/* store the absolute pointer as relative address in relative_p */
#define setaddr(relative_p, pointer) _setaddr(&relative_p, pointer) // pointer - &relative_p





// size align to 8 byte
int align_up(int sz) {
    return ((sz + 7) >> 3) << 3; 
}
void push(my_stack_t *s, reuse_block_t *rb) {
    rb->next = s->next;
    s->next = rb;
}

uintptr_t pop(my_stack_t *s) {
    if(!s->next)
        return 0;
    uintptr_t block = (uintptr_t)s->next;
    s->next = s->next->next;
    return block;
}

vm_t *vm_new()
{
    vm_t *node = mmap(0, PAGESIZE, PROT_READ | PROT_WRITE,
                      MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (node == MAP_FAILED)
        err(ENOMEM, "Failed to map memory");

    node->max = N_VM_ELEMENTS;
    node->next = NULL; /* for clarity */

    setaddr(node->array[0], node->str);

    /* prevent compilers from optimizing assignments out */
    OPTFENCE(node);
    return node;
}


void vm_destroy(vm_head_t *head)
{
    vm_t *nod = head->next;
    while (nod) {
        char *tmp = (char *) nod;
        nod = nod->next;
        munmap(tmp, PAGESIZE);
    }
}

static vm_t *vm_extend_map(vm_head_t *head)
{
    vm_t *nod = head->next;
    vm_t *new_nod = vm_new();
    head->next = new_nod;
    
    return new_nod;
}

// malloc
uintptr_t vm_add(size_t sz, struct vm_head *head)
{
    sz = align_up(sz);

    uintptr_t block = pop(&head->pool.s[(sz >> 3) - 1]);
    if(block)
        return block;

    vm_t *nod = head->next;
    if(!nod)
        nod = vm_extend_map(head);

    retry:
    if ((int) ((nod->array[nod->use] + (sizeof(p_rel) * nod->use) +
                sz)) >= PAGESIZE || nod->use >= nod->max) {
        nod->max = nod->use + 1; /* addr > map */
        nod = vm_extend_map(head);
        goto retry;
    }

    char *p = getaddr(nod->array[nod->use]) + sz;
    setaddr(nod->array[nod->use + 1], p);
    nod->use++;
    return getaddr(nod->array[nod->use - 1]);
}

// free
void vm_remove(uintptr_t ptr, int sz, struct vm_head *head) {
    if(sz == 0 || !ptr)
        return;
    sz = align_up(sz);
    push(&head->pool.s[(sz >> 3) - 1],ptr);
}


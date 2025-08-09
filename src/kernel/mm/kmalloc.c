// kmalloc.c — Early kernel heap (first-fit) for pre-VMM allocations.
// Replace with page-backed slab/arena allocator after PMM/VMM is online.
#include "kmalloc.h"

// Very simple first-fit allocator with headers, alignment to 16 bytes.
// Replace with a slab/arena allocator once paging is online.

typedef struct block_header {
    size_t size;          // payload size
    int free;
    struct block_header* next;
} block_header_t;

#define ALIGN_UP(x, a) (((x) + ((a)-1)) & ~((a)-1))
#define HEADER_SIZE ALIGN_UP(sizeof(block_header_t), 16)

static uint8_t* g_heap_base = 0;
static size_t g_heap_size = 0;
static block_header_t* g_head = 0;

void kmalloc_init(void* heap_start, size_t heap_size) {
    g_heap_base = (uint8_t*)heap_start;
    g_heap_size = heap_size;
    g_head = (block_header_t*)g_heap_base;
    g_head->size = heap_size - HEADER_SIZE;
    g_head->free = 1;
    g_head->next = NULL;
}

static void split_block(block_header_t* blk, size_t asize) {
    size_t total = blk->size;
    if (total >= asize + HEADER_SIZE + 16) {
        uint8_t* new_hdr_addr = (uint8_t*)blk + HEADER_SIZE + asize;
        block_header_t* new_hdr = (block_header_t*)new_hdr_addr;
        new_hdr->size = total - asize - HEADER_SIZE;
        new_hdr->free = 1;
        new_hdr->next = blk->next;
        blk->size = asize;
        blk->next = new_hdr;
    }
}

void* kmalloc(size_t size) {
    if (size == 0 || !g_head) return NULL;
    size_t asize = ALIGN_UP(size, 16);
    block_header_t* cur = g_head;
    while (cur) {
        if (cur->free && cur->size >= asize) {
            split_block(cur, asize);
            cur->free = 0;
            return (uint8_t*)cur + HEADER_SIZE;
        }
        cur = cur->next;
    }
    return NULL;
}

static block_header_t* ptr_to_hdr(void* p) {
    return (block_header_t*)((uint8_t*)p - HEADER_SIZE);
}

void kfree(void* ptr) {
    if (!ptr) return;
    block_header_t* hdr = ptr_to_hdr(ptr);
    hdr->free = 1;
    // Coalesce simple next-adjacent
    block_header_t* cur = g_head;
    while (cur && cur->next) {
        uint8_t* cur_end = (uint8_t*)cur + HEADER_SIZE + cur->size;
        if (cur->free && cur->next->free && cur_end == (uint8_t*)cur->next) {
            cur->size += HEADER_SIZE + cur->next->size;
            cur->next = cur->next->next;
            continue;
        }
        cur = cur->next;
    }
}

size_t kmalloc_usable_size(void* ptr) {
    if (!ptr) return 0;
    return ptr_to_hdr(ptr)->size;
}

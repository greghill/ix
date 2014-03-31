#ifndef __LWIP_MEM_H__
#define __LWIP_MEM_H__

typedef size_t mem_size_t;

/** mem_init is not used when using pools instead of a heap */
#define mem_init()
/** mem_trim is not used when using pools instead of a heap:
    we can't free part of a pool element and don't want to copy the rest */
#define mem_trim(mem, size) (mem)
void *mem_malloc(mem_size_t size);
void  mem_free(void *mem);

/** Calculate memory size for an aligned buffer - returns the next highest
 * multiple of MEM_ALIGNMENT (e.g. LWIP_MEM_ALIGN_SIZE(3) and
 * LWIP_MEM_ALIGN_SIZE(4) will both yield 4 for MEM_ALIGNMENT == 4).
 */
#ifndef LWIP_MEM_ALIGN_SIZE
#define LWIP_MEM_ALIGN_SIZE(size) (((size) + MEM_ALIGNMENT - 1) & ~(MEM_ALIGNMENT-1))
#endif

/** Align a memory pointer to the alignment defined by MEM_ALIGNMENT
 * so that ADDR % MEM_ALIGNMENT == 0
 */
#ifndef LWIP_MEM_ALIGN
#define LWIP_MEM_ALIGN(addr) ((void *)(((mem_ptr_t)(addr) + MEM_ALIGNMENT - 1) & ~(mem_ptr_t)(MEM_ALIGNMENT-1)))
#endif

#endif /* __LWIP_MEM_H__ */

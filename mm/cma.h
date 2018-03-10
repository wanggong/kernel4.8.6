#ifndef __MM_CMA_H__
#define __MM_CMA_H__
/******************************************************************************
cma的使用时这样的，
在系统启动时调用cma_init_reserved_mem将一些连续的页面的migrate type标记为CMA,
同时初始化cma的数据结构，保存起始的fn和count，这些页面也仅仅是标记为CMA,其实
还在伙伴管理系统中，当系统内存不足时可以分配使用的(根据分配标志确认是否能分配)。
当系统需要连续页面时可以调用cma_alloc来从cma中分配一些连续内存。
*******************************************************************************/
struct cma {
//开始页面的pfn
	unsigned long   base_pfn;
//所占的页面的数量    
	unsigned long   count;
//cma中的页面，每个bit对应于1<<order_per_bit个连续的页面。
//初始化时这些bit都为0，当调用cma_alloc分配内存之后，将分配出去的
//那些page对应的bit设置为1.
	unsigned long   *bitmap;
//每个bitmap的bit对应的连续页面的order
	unsigned int order_per_bit; /* Order of pages represented by one bit */
	struct mutex    lock;
#ifdef CONFIG_CMA_DEBUGFS
	struct hlist_head mem_head;
	spinlock_t mem_head_lock;
#endif
};

extern struct cma cma_areas[MAX_CMA_AREAS];
extern unsigned cma_area_count;

//cma最大的bit number
static inline unsigned long cma_bitmap_maxno(struct cma *cma)
{
	return cma->count >> cma->order_per_bit;
}

#endif

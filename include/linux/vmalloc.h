#ifndef _LINUX_VMALLOC_H
#define _LINUX_VMALLOC_H

#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/llist.h>
#include <asm/page.h>		/* pgprot_t */
#include <linux/rbtree.h>

struct vm_area_struct;		/* vma defining user mapping in mm_types.h */
struct notifier_block;		/* in notifier.h */

/* bits in flags of vmalloc's vm_struct below */
#define VM_IOREMAP		0x00000001	/* ioremap() and friends */
#define VM_ALLOC		0x00000002	/* vmalloc() */
#define VM_MAP			0x00000004	/* vmap()ed pages */
#define VM_USERMAP		0x00000008	/* suitable for remap_vmalloc_range */
#define VM_UNINITIALIZED	0x00000020	/* vm_struct is not fully initialized */
#define VM_NO_GUARD		0x00000040      /* don't add guard page */
#define VM_KASAN		0x00000080      /* has allocated kasan shadow memory */
/* bits [20..32] reserved for arch specific ioremap internals */

/*
 * Maximum alignment for ioremap() regions.
 * Can be overriden by arch-specific value.
 */
#ifndef IOREMAP_MAX_ORDER
#define IOREMAP_MAX_ORDER	(7 + PAGE_SHIFT)	/* 128 pages */
#endif

/****************************************************************************
下面两个数据结构用于管理kernel空间的（VMALLOC_START, VMALLOC_END）的内存虚拟空间，
vmalloc，iomap分配的地址会在这个地址空间中，
vm_struct用于管理pages等一些信息
vmap_area用于链接到rbtree和链表等

这里管理的信息可通过 cat /proc/vmallocinfo 查看
******************************************************************************/
struct vm_struct {
//目前仅仅看到这个会在 vm_area_add_early 中使用，其他地方没有使用，用处
//好像是仅仅为了early kernel map，就是说没什么用
	struct vm_struct	*next;
	//开始地址，这里的addr和vmap_area.va_start是一样的值，下面的size和vmap_area.va_end也是重合的
	//为什么同样的内容要分配的两个不同的数据结构中，是否可以合并。
	void			*addr;
	//分配内存的大小，包括用于隔离的一个page的空洞
	unsigned long		size;
	unsigned long		flags;
	//分配的pages
	struct page		**pages;
	//page的个数
	unsigned int		nr_pages;
	phys_addr_t		phys_addr;
	//调用者的函数指针
	const void		*caller;
};

/*
vmap_area是管理vmalloc空间(VMALLOC_START, VMALLOC_END)的数据结构,
空间分配方法：从低地址开始分配，先从rb_node中找到合适的开始地址，然后遍历这个list，
对于vmalloc而言相当于每次都遍历整个list，效率较差。但算法中使用了cache记住上次
搜索的状况，大部分情况下都从cache开始直接分配了，可能对效率有较大的提升，为什么
不像vma那样也添加一个子树最大空洞的字段呢？
分配函数是： alloc_vmap_area
*/
struct vmap_area {
	//虚拟地址的开始和结束，包括一个page的空洞
	unsigned long va_start;
	unsigned long va_end;
	unsigned long flags;
	//链接到 vmap_area_root
	struct rb_node rb_node;         /* address sorted rbtree */
	//链接到 vmap_area_list
	struct list_head list;          /* address sorted list */
	struct llist_node purge_list;    /* "lazy purge" list */
	//指向 vm_struct
	struct vm_struct *vm;
	struct rcu_head rcu_head;
};

/*
 *	Highlevel APIs for driver use
 */
extern void vm_unmap_ram(const void *mem, unsigned int count);
extern void *vm_map_ram(struct page **pages, unsigned int count,
				int node, pgprot_t prot);
extern void vm_unmap_aliases(void);

#ifdef CONFIG_MMU
extern void __init vmalloc_init(void);
#else
static inline void vmalloc_init(void)
{
}
#endif

extern void *vmalloc(unsigned long size);
extern void *vzalloc(unsigned long size);
extern void *vmalloc_user(unsigned long size);
extern void *vmalloc_node(unsigned long size, int node);
extern void *vzalloc_node(unsigned long size, int node);
extern void *vmalloc_exec(unsigned long size);
extern void *vmalloc_32(unsigned long size);
extern void *vmalloc_32_user(unsigned long size);
extern void *__vmalloc(unsigned long size, gfp_t gfp_mask, pgprot_t prot);
extern void *__vmalloc_node_range(unsigned long size, unsigned long align,
			unsigned long start, unsigned long end, gfp_t gfp_mask,
			pgprot_t prot, unsigned long vm_flags, int node,
			const void *caller);

extern void vfree(const void *addr);

extern void *vmap(struct page **pages, unsigned int count,
			unsigned long flags, pgprot_t prot);
extern void vunmap(const void *addr);

extern int remap_vmalloc_range_partial(struct vm_area_struct *vma,
				       unsigned long uaddr, void *kaddr,
				       unsigned long size);

extern int remap_vmalloc_range(struct vm_area_struct *vma, void *addr,
							unsigned long pgoff);
void vmalloc_sync_all(void);
 
/*
 *	Lowlevel-APIs (not for driver use!)
 */

static inline size_t get_vm_area_size(const struct vm_struct *area)
{
	if (!(area->flags & VM_NO_GUARD))
		/* return actual size without guard page */
		return area->size - PAGE_SIZE;
	else
		return area->size;

}

extern struct vm_struct *get_vm_area(unsigned long size, unsigned long flags);
extern struct vm_struct *get_vm_area_caller(unsigned long size,
					unsigned long flags, const void *caller);
extern struct vm_struct *__get_vm_area(unsigned long size, unsigned long flags,
					unsigned long start, unsigned long end);
extern struct vm_struct *__get_vm_area_caller(unsigned long size,
					unsigned long flags,
					unsigned long start, unsigned long end,
					const void *caller);
extern struct vm_struct *remove_vm_area(const void *addr);
extern struct vm_struct *find_vm_area(const void *addr);

extern int map_vm_area(struct vm_struct *area, pgprot_t prot,
			struct page **pages);
#ifdef CONFIG_MMU
extern int map_kernel_range_noflush(unsigned long start, unsigned long size,
				    pgprot_t prot, struct page **pages);
extern void unmap_kernel_range_noflush(unsigned long addr, unsigned long size);
extern void unmap_kernel_range(unsigned long addr, unsigned long size);
#else
static inline int
map_kernel_range_noflush(unsigned long start, unsigned long size,
			pgprot_t prot, struct page **pages)
{
	return size >> PAGE_SHIFT;
}
static inline void
unmap_kernel_range_noflush(unsigned long addr, unsigned long size)
{
}
static inline void
unmap_kernel_range(unsigned long addr, unsigned long size)
{
}
#endif

/* Allocate/destroy a 'vmalloc' VM area. */
extern struct vm_struct *alloc_vm_area(size_t size, pte_t **ptes);
extern void free_vm_area(struct vm_struct *area);

/* for /dev/kmem */
extern long vread(char *buf, char *addr, unsigned long count);
extern long vwrite(char *buf, char *addr, unsigned long count);

/*
 *	Internals.  Dont't use..
 */
extern struct list_head vmap_area_list;
extern __init void vm_area_add_early(struct vm_struct *vm);
extern __init void vm_area_register_early(struct vm_struct *vm, size_t align);

#ifdef CONFIG_SMP
# ifdef CONFIG_MMU
struct vm_struct **pcpu_get_vm_areas(const unsigned long *offsets,
				     const size_t *sizes, int nr_vms,
				     size_t align);

void pcpu_free_vm_areas(struct vm_struct **vms, int nr_vms);
# else
static inline struct vm_struct **
pcpu_get_vm_areas(const unsigned long *offsets,
		const size_t *sizes, int nr_vms,
		size_t align)
{
	return NULL;
}

static inline void
pcpu_free_vm_areas(struct vm_struct **vms, int nr_vms)
{
}
# endif
#endif

#ifdef CONFIG_MMU
#define VMALLOC_TOTAL (VMALLOC_END - VMALLOC_START)
#else
#define VMALLOC_TOTAL 0UL
#endif

int register_vmap_purge_notifier(struct notifier_block *nb);
int unregister_vmap_purge_notifier(struct notifier_block *nb);

#endif /* _LINUX_VMALLOC_H */

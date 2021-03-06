#ifndef _LINUX_MM_TYPES_H
#define _LINUX_MM_TYPES_H

#include <linux/auxvec.h>
#include <linux/types.h>
#include <linux/threads.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/rbtree.h>
#include <linux/rwsem.h>
#include <linux/completion.h>
#include <linux/cpumask.h>
#include <linux/uprobes.h>
#include <linux/page-flags-layout.h>
#include <linux/workqueue.h>
#include <asm/page.h>
#include <asm/mmu.h>

#ifndef AT_VECTOR_SIZE_ARCH
#define AT_VECTOR_SIZE_ARCH 0
#endif
#define AT_VECTOR_SIZE (2*(AT_VECTOR_SIZE_ARCH + AT_VECTOR_SIZE_BASE + 1))

struct address_space;
struct mem_cgroup;
//在1861上CONFIG_SPLIT_PTLOCK_CPUS=999999，所以是不用USE_SPLIT_PTE_PTLOCKS的
#define USE_SPLIT_PTE_PTLOCKS	(NR_CPUS >= CONFIG_SPLIT_PTLOCK_CPUS)
#define USE_SPLIT_PMD_PTLOCKS	(USE_SPLIT_PTE_PTLOCKS && \
		IS_ENABLED(CONFIG_ARCH_ENABLE_SPLIT_PMD_PTLOCK))
#define ALLOC_SPLIT_PTLOCKS	(SPINLOCK_SIZE > BITS_PER_LONG/8)

/*
 * Each physical page in the system has a struct page associated with
 * it to keep track of whatever it is we are using the page for at the
 * moment. Note that we have no way to track which tasks are using
 * a page, though if it is a pagecache page, rmap structures can tell us
 * who is mapping it.
 *
 * The objects in struct page are organized in double word blocks in
 * order to allows us to use atomic double word operations on portions
 * of struct page. That is currently only used by slub but the arrangement
 * allows the use of atomic double word operations on the flags/mapping
 * and lru list pointers also.
 */
struct page {
	/* First double word block */
//page的一些flags，具体见pageflags    
	unsigned long flags;		/* Atomic flags, some possibly
					 * updated asynchronously */
	union {
/***********************************************
 首先，PFRA必须要确定待回收页是共享的还是非共享的，以及是映射页或是匿名页。
 为做到这一点，内核要查看页描述符的两个字段：_mapcount和mapping。
 _mapcount字段存放引用页框的页表项数目，确定其是否共享；
 mapping字段用于确定页是映射的或是匿名的：
 为空表示该页属于交换高速缓存；非空，且最低位是1，表示该页为匿名页，
 同时mapping字段中存放的是指向anon_vma描述符的指针；
 如果mapping字段非空，且最低位是0，表示该页为映射页；
 同时mapping字段指向对应文件的address_space对象。
**********************************************/
		struct address_space *mapping;	/* If low bit clear, points to
						 * inode address_space, or NULL.
						 * If page mapped as anonymous
						 * memory, low bit is set, and
						 * it points to anon_vma object:
						 * see PAGE_MAPPING_ANON below.
						 */
//当page用于slab时，表示第一个obj的地址						 
		void *s_mem;			/* slab first object */
//和多个page连到一块有关，compound的计数，放在page[1]的这个字段中                         
		atomic_t compound_mapcount;	/* first tail page */
		/* page_deferred_list().next	 -- second tail page */
	};

	/* Second double word */
	union {
//对文件cache有用，表示此page在文件的offset	，
//对于anon的page，表示此page在其vma(第一个映射此page的vma)中的offset，好像没什么用
		pgoff_t index;		/* Our offset within mapping. */
		void *freelist;		/* sl[aou]b first free object */
		/* page_deferred_list().prev	-- second tail page */
	};

	union {
#if defined(CONFIG_HAVE_CMPXCHG_DOUBLE) && \
	defined(CONFIG_HAVE_ALIGNED_STRUCT_PAGE)
		/* Used for cmpxchg_double in slub */
		unsigned long counters;
#else
		/*
		 * Keep _refcount separate from slub cmpxchg_double data.
		 * As the rest of the double word is protected by slab_lock
		 * but _refcount is not.
		 */
		unsigned counters;
#endif
		struct {

			union {
				/*
				 * Count of ptes mapped in mms, to show when
				 * page is mapped & limit reverse map searches.
				 *
				 * Extra information about page type may be
				 * stored here for pages that are never mapped,
				 * in which case the value MUST BE <= -2.
				 * See page-flags.h for more details.
				 */

//page在使用时要通过pte索引的，一个page可以在多个用户空间共享使用，这就会将此page映射到
//多个pte中，这个字段就是用来统计page映射到了多少个pte中。
//这个在share memory或COW过程中很有用，当mapcount大于零时表示有多个进程map到此page，需要进行cow
//操作，当mapcount==0时，表示现在只有一个进程使用了，可以直接修改pte的值，不需要分配页然后拷贝了。
//这个字段在初始化时设置为-1
				atomic_t _mapcount;

				unsigned int active;		/* SLAB */
				struct {			/* SLUB */
					unsigned inuse:16;
					unsigned objects:15;
					unsigned frozen:1;
				};
				int units;			/* SLOB */
			};
			/*
			 * Usage count, *USE WRAPPER FUNCTION* when manual
			 * accounting. See page_ref.h
			 */
//此page的引用计数，见get_page和put_page			 
//在第一次加入buddy系统是被设置为0，见__free_pages_boot_core，所以0应该表示free的，
//这个参数表示page是否可以free，只有当 _refcount=0 时才能free
			atomic_t _refcount;
		};
	};

	/*
	 * Third double word block
	 *
	 * WARNING: bit 0 of the first word encode PageTail(). That means
	 * the rest users of the storage space MUST NOT use the bit to
	 * avoid collision and false-positive PageTail().
	 */
	union {
/***************************************************************************
根据不同的情况加入到不同的链表中
1. 如果此页是某个进程的页，则此page会加入到lru链表或lru缓存中
2. 如果是空闲页，则将第一个page加到到伙伴系统空闲链表中
3. 如果是slab也，加入到slab链表
4. 将页隔离时，加入到隔离链表中
****************************************************************************/
		struct list_head lru;	/* Pageout list, eg. active_list
					 * protected by zone_lru_lock !
					 * Can be used as a generic list
					 * by the page owner.
					 */
		struct dev_pagemap *pgmap; /* ZONE_DEVICE pages are never on an
					    * lru or handled by a slab
					    * allocator, this points to the
					    * hosting device page map.
					    */
		struct {		/* slub per cpu partial pages */
			struct page *next;	/* Next partial slab */
#ifdef CONFIG_64BIT
			int pages;	/* Nr of partial slabs left */
			int pobjects;	/* Approximate # of objects */
#else
			short int pages;
			short int pobjects;
#endif
		};

		struct rcu_head rcu_head;	/* Used by SLAB
						 * when destroying via RCU
						 */
		/* Tail pages of compound page */
		struct {
//如果此页是compound的tail页，此处存储compound的首页		
			unsigned long compound_head; /* If bit zero is set */

			/* First tail page only */
#ifdef CONFIG_64BIT
			/*
			 * On 64 bit system we have enough space in struct page
			 * to encode compound_dtor and compound_order with
			 * unsigned int. It can help compiler generate better or
			 * smaller code on some archtectures.
			 */
			unsigned int compound_dtor;
//对于compound page，第一个tail page的这个字段存放此compound page的order
			unsigned int compound_order;
#else
			unsigned short int compound_dtor;
			unsigned short int compound_order;
#endif
		};

#if defined(CONFIG_TRANSPARENT_HUGEPAGE) && USE_SPLIT_PMD_PTLOCKS
		struct {
			unsigned long __pad;	/* do not overlay pmd_huge_pte
						 * with compound_head to avoid
						 * possible bit 0 collision.
						 */
			pgtable_t pmd_huge_pte; /* protected by page->ptl */
		};
#endif
	};

	/* Remainder is not double word aligned */
	union {
		unsigned long private;		/* Mapping-private opaque data:
					 	 * usually used for buffer_heads
						 * if PagePrivate set; used for
						 * swp_entry_t if PageSwapCache;
						 * indicates order in the buddy
						 * system if PG_buddy is set.
						 */
//对于pagetable的页面来说，private是不需要的，所以可以用来做锁的字段使用
//注意锁是在pmd上用的，pte对应的page可能和slab_cache有关联
#if USE_SPLIT_PTE_PTLOCKS
#if ALLOC_SPLIT_PTLOCKS
		spinlock_t *ptl;
#else
		spinlock_t ptl;
#endif
#endif
		struct kmem_cache *slab_cache;	/* SL[AU]B: Pointer to slab */
	};

#ifdef CONFIG_MEMCG
	struct mem_cgroup *mem_cgroup;
#endif

	/*
	 * On machines where all RAM is mapped into kernel address space,
	 * we can simply calculate the virtual address. On machines with
	 * highmem some memory is mapped into kernel virtual memory
	 * dynamically, so we need a place to store that address.
	 * Note that this field could be 16 bits on x86 ... ;)
	 *
	 * Architectures with slow multiplication can define
	 * WANT_PAGE_VIRTUAL in asm/page.h
	 */
#if defined(WANT_PAGE_VIRTUAL)
	void *virtual;			/* Kernel virtual address (NULL if
					   not kmapped, ie. highmem) */
#endif /* WANT_PAGE_VIRTUAL */

#ifdef CONFIG_KMEMCHECK
	/*
	 * kmemcheck wants to track the status of each byte in a page; this
	 * is a pointer to such a status block. NULL if not tracked.
	 */
	void *shadow;
#endif

#ifdef LAST_CPUPID_NOT_IN_PAGE_FLAGS
	int _last_cpupid;
#endif
}
/*
 * The struct page can be forced to be double word aligned so that atomic ops
 * on double words work. The SLUB allocator can make use of such a feature.
 */
#ifdef CONFIG_HAVE_ALIGNED_STRUCT_PAGE
	__aligned(2 * sizeof(unsigned long))
#endif
;

struct page_frag {
	struct page *page;
#if (BITS_PER_LONG > 32) || (PAGE_SIZE >= 65536)
	__u32 offset;
	__u32 size;
#else
	__u16 offset;
	__u16 size;
#endif
};

#define PAGE_FRAG_CACHE_MAX_SIZE	__ALIGN_MASK(32768, ~PAGE_MASK)
#define PAGE_FRAG_CACHE_MAX_ORDER	get_order(PAGE_FRAG_CACHE_MAX_SIZE)

struct page_frag_cache {
	void * va;
#if (PAGE_SIZE < PAGE_FRAG_CACHE_MAX_SIZE)
	__u16 offset;
	__u16 size;
#else
	__u32 offset;
#endif
	/* we maintain a pagecount bias, so that we dont dirty cache line
	 * containing page->_refcount every time we allocate a fragment.
	 */
	unsigned int		pagecnt_bias;
	bool pfmemalloc;
};

typedef unsigned long vm_flags_t;

/*
 * A region containing a mapping of a non-memory backed file under NOMMU
 * conditions.  These are held in a global tree and are pinned by the VMAs that
 * map parts of them.
 */
struct vm_region {
	struct rb_node	vm_rb;		/* link in global region tree */
	vm_flags_t	vm_flags;	/* VMA vm_flags */
	unsigned long	vm_start;	/* start address of region */
	unsigned long	vm_end;		/* region initialised to here */
	unsigned long	vm_top;		/* region allocated to here */
	unsigned long	vm_pgoff;	/* the offset in vm_file corresponding to vm_start */
	struct file	*vm_file;	/* the backing file or NULL */

	int		vm_usage;	/* region usage count (access under nommu_region_sem) */
	bool		vm_icache_flushed : 1; /* true if the icache has been flushed for
						* this region */
};

#ifdef CONFIG_USERFAULTFD
#define NULL_VM_UFFD_CTX ((struct vm_userfaultfd_ctx) { NULL, })
struct vm_userfaultfd_ctx {
	struct userfaultfd_ctx *ctx;
};
#else /* CONFIG_USERFAULTFD */
#define NULL_VM_UFFD_CTX ((struct vm_userfaultfd_ctx) {})
struct vm_userfaultfd_ctx {};
#endif /* CONFIG_USERFAULTFD */

/*
 * This struct defines a memory VMM memory area. There is one of these
 * per VM-area/task.  A VM area is any part of the process virtual memory
 * space that has a special rule for the page-fault handlers (ie a shared
 * library, the executable area etc).
 */
 /*
用途：管理进程的某一段内存空间
操作：
1. 插入：首先从rb_node树上查找需要插入的位置，然后将新的vma插入到树上，
		 同时查找需要插入的链表的位置(二叉树的prev节点)，插入到链表中，
		 所以插入树和链表总的时间是log(n).
2. 查找：从二叉树中查找，时间log(n)
3. 分配：从高端地址尽可能深的查找二叉树分配

在armv8中，user内存管理的工作简单了很多，具体动作如下：
1.	stack的内存放在内存的顶端(TASK_SIZE的位置)，保留128M的空间。
2.	在TASK_SIZE-128M是mmap能分配的空间的最大值，mmap从这个地址向下分配。
3.	可执行文件的内存是需要固定位置的，这个用mmap(FIXED)分配到固定空间。
4.	在可执行文件分配完之后，在其上面分配brk的空间，但是这个从来不使用。
	可读写全局变量占用一个单独的map空间，标记是可读写。
	const变量占用单独的一个map空间，标记为只读
5.	.so的内存通过mmap分配
6.	所有的通过malloc的内存分配都是通过mmap进行的。
*/
struct vm_area_struct {
	/* The first cache line has the info for VMA tree walking. */

	unsigned long vm_start;		/* Our start address within vm_mm. */
	unsigned long vm_end;		/* The first byte after our end address
					   within vm_mm. */

	/* linked list of VM areas per task, sorted by address */
	struct vm_area_struct *vm_next, *vm_prev;

	struct rb_node vm_rb;

	/*
	 * Largest free memory gap in bytes to the left of this VMA.
	 * Either between this VMA and vma->vm_prev, or between one of the
	 * VMAs below us in the VMA rbtree and its ->vm_prev. This helps
	 * get_unmapped_area find a free area of the right size.
	 */
//所有的vma都在rb_tree中，同时也在VM链表中，而这些链表在使用之后可能会被删除
//这样就会在链表中存在空洞，当我们需要需要新的VMA时，可能需要重新使用这些空间
//如果从链表或rb_tree中一个一个的查找两个之间的空洞，效率会比较差，所以就在每个
//VM上添加了一个 b_subtree_gap 这个字段表示以当前节点为根的所有的子树中最大的空洞，
//这样在申请空间时，只需要查看根节点rb_subtree_gap的大小就可以确认是否有足够大的空洞了。
	unsigned long rb_subtree_gap;

	/* Second cache line starts here. */

	struct mm_struct *vm_mm;	/* The address space we belong to. */
//vm_page_prot和vm_flags是同一个东西，供不同时期使用，其中vm_flags是用户层传过来的标志
//通过vm_get_page_prot转换成vm_page_prot供硬件使用
//这两个标志的使用参见do_brk和mmap_region  
//vm_flags枚举是 VM_NONE 到 VM_MERGEABLE (file:include/linux/mm.h)
//vm_page_prot在mmap.c中对应protection_map，这里边的定义是跟具体平台相关的，要写入到
//mmu的pagetable中的值，在arm64中对应于(file:arch/arm64/include/asm/pgtable-prot.h)中的
//PAGE_xxx,最终对应于(file:arch/arm64/include/asm/pgtable-hwdef.h)中的PTE_xxxx的值

//从上面的分析可以知道vm_flags是给Linux软件部分内存管理使用的，而vm_page_prot则是给具体mmu
//使用的硬件相关的东西

	pgprot_t vm_page_prot;		/* Access permissions of this VMA. */
	unsigned long vm_flags;		/* Flags, see mm.h. */

	/*
	 * For areas with an address space and backing store,
	 * linkage into the address_space->i_mmap interval tree.
	 */
	struct {
/*****************************************************************	
这两个字段用于interval tree(线段树)，rb用于将当前vma连接到address_space->i_mmap，此树是以
vma->mv_start为键值的二叉树，每个节点上有一个rb_subtree_last，对于叶子节点，这个值是
vm_end，对于其他节点，此值表示以当前节点为树根的rb_subtree_last的最大值。
rb_subtree_last是用于加快search的速度。这是一种对范围搜索的手段。
*****************************************************************/
		struct rb_node rb;
		unsigned long rb_subtree_last;
	} shared;

	/*
	 * A file's MAP_PRIVATE vma can be in both i_mmap tree and anon_vma
	 * list, after a COW of one of the file pages.	A MAP_SHARED vma
	 * can only be in the i_mmap tree.  An anonymous MAP_PRIVATE, stack
	 * or brk vma (with NULL file) can only be in an anon_vma list.
	 */
//链接avc->same_vma	 
//这个head的意义是链接此vm_area_struct和其父、祖父、曾祖父的vma_chain，
//因为当fork时需要将子进程的vm_area_struct添加到各个上级vma_anon中，所以
//才需要在这里保留当前vm_area_struct的所有上级chain。
	struct list_head anon_vma_chain; /* Serialized by mmap_sem &
					  * page_table_lock */
//此字段是在用户分配内存之后，第一次访问时分配的，见do_anonymous_page                        
	struct anon_vma *anon_vma;	/* Serialized by page_table_lock */

	/* Function pointers to deal with this struct. */
//从vma_is_anonymous函数中看，对于anon的vma，此值是0.
//所以只有vma是文件映射时此字段才有意义
	const struct vm_operations_struct *vm_ops;

	/* Information about our backing store: */
//vm_start对应文件中的偏移，用于计算一个page在此vma中对应的地址   
//对于file mapped page，page->index表示的是映射到文件内的偏移（Byte为单位），
//而vma->vm_pgoff表示的是该VMA映射到文件内的偏移（page为单位）

//对于anon+private的映射，这个就是addr>>PAGE_SHIFT，即vma的起始地址
	unsigned long vm_pgoff;		/* Offset (within vm_file) in PAGE_SIZE
//vma map的file					   units */
	struct file * vm_file;		/* File we map to (can be NULL). */
	void * vm_private_data;		/* was vm_pte (shared mem) */

#ifndef CONFIG_MMU
	struct vm_region *vm_region;	/* NOMMU mapping region */
#endif
#ifdef CONFIG_NUMA
	struct mempolicy *vm_policy;	/* NUMA policy for the VMA */
#endif
	struct vm_userfaultfd_ctx vm_userfaultfd_ctx;
};

struct core_thread {
	struct task_struct *task;
	struct core_thread *next;
};

struct core_state {
	atomic_t nr_threads;
	struct core_thread dumper;
	struct completion startup;
};

enum {
//更新函数 insert_page，insert_page，	
//更新时机是在将页面插入到vma并设置pte时，所以有理由猜测这个统计不是物理内存，也不是虚拟内存
//而是映射过物理内存的虚拟内存(因为一个物理内存可能会被多次映射)，但在task_mem中，这个数据被
//当作rss使用了，准确吗?重复映射怎么算?
	MM_FILEPAGES,	/* Resident file mapping pages */
	MM_ANONPAGES,	/* Resident anonymous pages */
	MM_SWAPENTS,	/* Anonymous swap entries */
	MM_SHMEMPAGES,	/* Resident shared memory pages */
	NR_MM_COUNTERS
};

#if USE_SPLIT_PTE_PTLOCKS && defined(CONFIG_MMU)
#define SPLIT_RSS_COUNTING
/* per-thread cached information, */
struct task_rss_stat {
	int events;	/* for synchronization threshold */
	int count[NR_MM_COUNTERS];
};
#endif /* USE_SPLIT_PTE_PTLOCKS */

struct mm_rss_stat {
	atomic_long_t count[NR_MM_COUNTERS];
};

struct kioctx_table;
//同一进程的多个线程共享此结构
/*
内存的种类分为以下几种（默认情况下）：
1. 堆栈，此内存顶端位于 TASK_SIZE 处，向下扩展，一般是保留128M
2. mmap的内存，这段内存从TASK_SIZE-128M处开始，每次分配也是尽可能
	从高向低的顺序查找(没有空洞的情况下)，有空洞时，按区间树的查找
	方式尽可能的从底层的树中查找。
	对于下面的内存也是通过mmap申请的但是会指定建议地址：
	a. 可执行文件，可执行文件会使用FIXED标志表示必须加载到固定地址。
	b. so库文件，这些文件会有建议地址，一般会使用建议地址加载，这大概就是
		通过mmap看到stack下面的空洞大于128M的原因吧。

*/
struct mm_struct {
//链表形式存放的vma    
	struct vm_area_struct *mmap;		/* list of VMAs */
//以树的形式存放的vma，此树为扩展的线段树，没有重叠的区间存在，区间从左向右排列，
//扩展的部分为树种最大的空洞。
	struct rb_root mm_rb;
/******************************************************************************
一个进程的vma可能比较多，当一个需要查找一个地址在哪个vma中时，正常通过
的节点树查找可能也需要较多的时间，根据局部原理，task在此处存放几个最近
使用过的vma，当查找时首先查看是否在这些cache的vma中，如果查找失败才开始
通过mm的树进行搜索。
vmacache:就是上面说的cache
vmacache_seqnum:当mm删除了vma时，那么这里cache的vma就需要失效，这个字段
就是判断此处的cache是否失效的，如果vmacache_seqnum!=mm.vmacache_seqnum,
表示这里的cache需要失效了.
见task_struct->vmacache_seqnum
*******************************************************************************/
	u32 vmacache_seqnum;                   /* per-thread vmacache */
#ifdef CONFIG_MMU
//这个在函数 arch_pick_mmap_layout 中设置，在1861上对应的是 arch_get_unmapped_area_topdown
	unsigned long (*get_unmapped_area) (struct file *filp,
				unsigned long addr, unsigned long len,
				unsigned long pgoff, unsigned long flags);
#endif
/******************************************************************************
这两个值在 arch_pick_mmap_layout 中设置。
******************************************************************************/
	unsigned long mmap_base;		/* base of mmap area */
	unsigned long mmap_legacy_base;         /* base of mmap area in bottom-up allocations */
//在setup_new_exec中将其设置为TASK_SIZE，具体什么用
//没搞清楚，好像是此mm vm的大小
	unsigned long task_size;		/* size of task vm space */
 //vma中end的最大值，也没搞明白具体用途   
	unsigned long highest_vm_end;		/* highest vma end address */
	pgd_t * pgd;
//见copy_mm，当指定CLONE_VM时，会增加此计数，表示多了一个task_struct使用者。    
	atomic_t mm_users;			/* How many users with user space? */
	atomic_t mm_count;			/* How many references to "struct mm_struct" (users count as 1) */
//pte所占的page的个数，仅用于统计
	atomic_long_t nr_ptes;			/* PTE page table pages */
#if CONFIG_PGTABLE_LEVELS > 2
//pmd所占的page的个数
	atomic_long_t nr_pmds;			/* PMD page table pages */
#endif
//vm_area_struct的个数
	int map_count;				/* number of VMAs */

	spinlock_t page_table_lock;		/* Protects page tables and some counters */
	struct rw_semaphore mmap_sem;
//没看明白
	struct list_head mmlist;		/* List of maybe swapped mm's.	These are globally strung
						 * together off init_mm.mmlist, and are protected
						 * by mmlist_lock
						 */

//没看明白
	unsigned long hiwater_rss;	/* High-watermark of RSS usage */
	unsigned long hiwater_vm;	/* High-water virtual memory usage */

	unsigned long total_vm;		/* Total pages mapped */
	unsigned long locked_vm;	/* Pages that have PG_mlocked set */
	unsigned long pinned_vm;	/* Refcount permanently increased */
	unsigned long data_vm;		/* VM_WRITE & ~VM_SHARED & ~VM_STACK */
	unsigned long exec_vm;		/* VM_EXEC & ~VM_WRITE & ~VM_STACK */
//stack 所占的page数
	unsigned long stack_vm;		/* VM_STACK */
	unsigned long def_flags;
	unsigned long start_code, end_code, start_data, end_data;
//start_brk:libc的malloc申请内存开始的地址
//brk:通过malloc申请的最后的地址
//start_stack:进程主线程的stack(不知道是否是开始地址)，见 setup_arg_pages
	unsigned long start_brk, brk, start_stack;
	unsigned long arg_start, arg_end, env_start, env_end;

	unsigned long saved_auxv[AT_VECTOR_SIZE]; /* for /proc/PID/auxv */

	/*
	 * Special counters, in some configurations protected by the
	 * page_table_lock, in other configurations by being atomic.
	 */
	struct mm_rss_stat rss_stat;

	struct linux_binfmt *binfmt;

	cpumask_var_t cpu_vm_mask_var;

	/* Architecture-specific MM context */
	mm_context_t context;

	unsigned long flags; /* Must use atomic bitops to access the bits */

	struct core_state *core_state; /* coredumping support */
#ifdef CONFIG_AIO
	spinlock_t			ioctx_lock;
	struct kioctx_table __rcu	*ioctx_table;
#endif
#ifdef CONFIG_MEMCG
	/*
	 * "owner" points to a task that is regarded as the canonical
	 * user/owner of this mm. All of the following must be true in
	 * order for it to be changed:
	 *
	 * current == mm->owner
	 * current->mm != mm
	 * new_owner->mm == mm
	 * new_owner->alloc_lock is held
	 */
	struct task_struct __rcu *owner;
#endif

	/* store ref to file /proc/<pid>/exe symlink points to */
	struct file __rcu *exe_file;
#ifdef CONFIG_MMU_NOTIFIER
	struct mmu_notifier_mm *mmu_notifier_mm;
#endif
#if defined(CONFIG_TRANSPARENT_HUGEPAGE) && !USE_SPLIT_PMD_PTLOCKS
	pgtable_t pmd_huge_pte; /* protected by page_table_lock */
#endif
#ifdef CONFIG_CPUMASK_OFFSTACK
	struct cpumask cpumask_allocation;
#endif
#ifdef CONFIG_NUMA_BALANCING
	/*
	 * numa_next_scan is the next time that the PTEs will be marked
	 * pte_numa. NUMA hinting faults will gather statistics and migrate
	 * pages to new nodes if necessary.
	 */
	unsigned long numa_next_scan;

	/* Restart point for scanning and setting pte_numa */
	unsigned long numa_scan_offset;

	/* numa_scan_seq prevents two threads setting pte_numa */
	int numa_scan_seq;
#endif
#if defined(CONFIG_NUMA_BALANCING) || defined(CONFIG_COMPACTION)
	/*
	 * An operation with batched TLB flushing is going on. Anything that
	 * can move process memory needs to flush the TLB when moving a
	 * PROT_NONE or PROT_NUMA mapped page.
	 */
	bool tlb_flush_pending;
#endif
	struct uprobes_state uprobes_state;
#ifdef CONFIG_X86_INTEL_MPX
	/* address of the bounds directory */
	void __user *bd_addr;
#endif
//此mm使用多少个hugepage
#ifdef CONFIG_HUGETLB_PAGE
	atomic_long_t hugetlb_usage;
#endif
#ifdef CONFIG_MMU
//跟最后释放此mm相关，没看明白
	struct work_struct async_put_work;
#endif
};

static inline void mm_init_cpumask(struct mm_struct *mm)
{
#ifdef CONFIG_CPUMASK_OFFSTACK
	mm->cpu_vm_mask_var = &mm->cpumask_allocation;
#endif
	cpumask_clear(mm->cpu_vm_mask_var);
}

/* Future-safe accessor for struct mm_struct's cpu_vm_mask. */
static inline cpumask_t *mm_cpumask(struct mm_struct *mm)
{
	return mm->cpu_vm_mask_var;
}

#if defined(CONFIG_NUMA_BALANCING) || defined(CONFIG_COMPACTION)
/*
 * Memory barriers to keep this state in sync are graciously provided by
 * the page table locks, outside of which no page table modifications happen.
 * The barriers below prevent the compiler from re-ordering the instructions
 * around the memory barriers that are already present in the code.
 */
static inline bool mm_tlb_flush_pending(struct mm_struct *mm)
{
	barrier();
	return mm->tlb_flush_pending;
}
static inline void set_tlb_flush_pending(struct mm_struct *mm)
{
	mm->tlb_flush_pending = true;

	/*
	 * Guarantee that the tlb_flush_pending store does not leak into the
	 * critical section updating the page tables
	 */
	smp_mb__before_spinlock();
}
/* Clearing is done after a TLB flush, which also provides a barrier. */
static inline void clear_tlb_flush_pending(struct mm_struct *mm)
{
	barrier();
	mm->tlb_flush_pending = false;
}
#else
static inline bool mm_tlb_flush_pending(struct mm_struct *mm)
{
	return false;
}
static inline void set_tlb_flush_pending(struct mm_struct *mm)
{
}
static inline void clear_tlb_flush_pending(struct mm_struct *mm)
{
}
#endif

struct vm_fault;

struct vm_special_mapping {
	const char *name;	/* The name, e.g. "[vdso]". */

	/*
	 * If .fault is not provided, this points to a
	 * NULL-terminated array of pages that back the special mapping.
	 *
	 * This must not be NULL unless .fault is provided.
	 */
	struct page **pages;

	/*
	 * If non-NULL, then this is called to resolve page faults
	 * on the special mapping.  If used, .pages is not checked.
	 */
	int (*fault)(const struct vm_special_mapping *sm,
		     struct vm_area_struct *vma,
		     struct vm_fault *vmf);

	int (*mremap)(const struct vm_special_mapping *sm,
		     struct vm_area_struct *new_vma);
};

enum tlb_flush_reason {
	TLB_FLUSH_ON_TASK_SWITCH,
	TLB_REMOTE_SHOOTDOWN,
	TLB_LOCAL_SHOOTDOWN,
	TLB_LOCAL_MM_SHOOTDOWN,
	TLB_REMOTE_SEND_IPI,
	NR_TLB_FLUSH_REASONS,
};

 /*
  * A swap entry has to fit into a "unsigned long", as the entry is hidden
  * in the "index" field of the swapper address space.
  */
typedef struct {
	unsigned long val;
} swp_entry_t;

#endif /* _LINUX_MM_TYPES_H */

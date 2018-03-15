#ifndef _LINUX_RMAP_H
#define _LINUX_RMAP_H
/*
 * Declarations for Reverse Mapping functions in mm/rmap.c
 */

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/rwsem.h>
#include <linux/memcontrol.h>

/*
 * The anon_vma heads a list of private "related" vmas, to scan if
 * an anonymous page pointing to this anon_vma needs to be unmapped:
 * the vmas on the list will be related by forking, or by splitting.
 *
 * Since vmas come and go as they are split and merged (particularly
 * in mprotect), the mapping field of an anonymous page cannot point
 * directly to a vma: instead it points to an anon_vma, on whose list
 * the related vmas can be easily linked or unlinked.
 *
 * After unlinking the last vma on the list, we must garbage collect
 * the anon_vma object itself: we're guaranteed no page can be
 * pointing to this anon_vma once its vma list is empty.
 */
/******************************************************************
终于理清了rmap的关系：
1. 对于file rmap，因为所有以file为背景的内存都只能是共享的(不能共享的会转入anon管理)，
	所以file rmap可以比较简单，只需要在mapping.immap链接上所有的vma_area即可。
2. 对于anon rmap，难点在于cow机制。具体实现如下：
	a. 每个anon的page->mapping指向一个anon_vma
	b. 所有映射到此page的虚拟地址都应该能通过anon_vma找到，这一点通过anon_vma.rb_root
		来实现，anon_vma.rb_root保存的是anon_vma_chain，anon_vma_chain有指向一个
		vm_area_struct。
		考虑下面场景：
		01. 进程A fork一个进程B，进程B会拷贝进程A的vm_area_struct，此时会创建
		一个anon_vma_chain将B的vm_area_struct添加到A的anon_vma.rb_root，这样共享的page
		才能rmap出进程B的pte。进程B还会创建自己的anon_vma，并连接自己的vm_area_struct。
		02. 然后进程B写一个页面，此时会执行COW分配一个页面给进程B，此页面会执行进程B的anon_vma，
		03. 之后进程B fork 一个进程C，考虑进程C中页面的使用情况，
			a. 可能是进程A分配的，进程B没有改写，所有进程A的anon_vma需要能找到C的vm_area_struct，
				同理进程B也需要能找到C的vm_area_struct，所以就需要将进程C的vm_area_struct分别
				添加到进程A和B的anon_vma中。
		综合上面，就是说父进程必须能找到子进程的vm_area_struct
		（上面不是很准确，假设是一个进程只有一个vm_area_struct）
******************************************************************/

struct anon_vma {
	struct anon_vma *root;		/* Root of this anon_vma tree */
	struct rw_semaphore rwsem;	/* W: modification, R: walking the list */
	/*
	 * The refcount is taken on an anon_vma when there is no
	 * guarantee that the vma of page tables will exist for
	 * the duration of the operation. A caller that takes
	 * the reference is responsible for clearing up the
	 * anon_vma if they are the last user on release
	 */
//引用计数	 
	atomic_t refcount;

	/*
	 * Count of child anon_vmas and VMAs which points to this anon_vma.
	 *
	 * This counter is used for making decision about reusing anon_vma
	 * instead of forking new one. See comments in function anon_vma_clone.
	 */
	unsigned degree;

	struct anon_vma *parent;	/* Parent of this anon_vma */

	/*
	 * NOTE: the LSB of the rb_root.rb_node is set by
	 * mm_take_all_locks() _after_ taking the above lock. So the
	 * rb_root must only be read/written after taking the above lock
	 * to be sure to see a valid next pointer. The LSB bit itself
	 * is serialized by a system wide lock only visible to
	 * mm_take_all_locks() (mm_all_locks_mutex).
	 */
//链接anon_vma_chain->rb,表示同一个anon_vma在不同的进程中	 
	struct rb_root rb_root;	/* Interval tree of private "related" vmas */
};

/*
 * The copy-on-write semantics of fork mean that an anon_vma
 * can become associated with multiple processes. Furthermore,
 * each child process will have its own anon_vma, where new
 * pages for that process are instantiated.
 *
 * This structure allows us to find the anon_vmas associated
 * with a VMA, or the VMAs associated with an anon_vma.
 * The "same_vma" list contains the anon_vma_chains linking
 * all the anon_vmas associated with this VMA.
 * The "rb" field indexes on an interval tree the anon_vma_chains
 * which link all the VMAs associated with this anon_vma.
 */
struct anon_vma_chain {
	struct vm_area_struct *vma;
	struct anon_vma *anon_vma;
//链接到vma->anon_vma_chain
	struct list_head same_vma;   /* locked by mmap_sem & page_table_lock */
//链接anon_vma->rb_root,表示同一个anon_vma在不同的进程中	     
	struct rb_node rb;			/* locked by anon_vma->rwsem */
	unsigned long rb_subtree_last;
#ifdef CONFIG_DEBUG_VM_RB
	unsigned long cached_vma_start, cached_vma_last;
#endif
};

enum ttu_flags {
	TTU_UNMAP = 1,			/* unmap mode */
	TTU_MIGRATION = 2,		/* migration mode */
	TTU_MUNLOCK = 4,		/* munlock mode */
	TTU_LZFREE = 8,			/* lazy free mode */
	TTU_SPLIT_HUGE_PMD = 16,	/* split huge PMD if any */

	TTU_IGNORE_MLOCK = (1 << 8),	/* ignore mlock */
	TTU_IGNORE_ACCESS = (1 << 9),	/* don't age */
	TTU_IGNORE_HWPOISON = (1 << 10),/* corrupted page is recoverable */
	TTU_BATCH_FLUSH = (1 << 11),	/* Batch TLB flushes where possible
					 * and caller guarantees they will
					 * do a final flush if necessary */
	TTU_RMAP_LOCKED = (1 << 12)	/* do not grab rmap lock:
					 * caller holds it */
};

#ifdef CONFIG_MMU
static inline void get_anon_vma(struct anon_vma *anon_vma)
{
	atomic_inc(&anon_vma->refcount);
}

void __put_anon_vma(struct anon_vma *anon_vma);

//减少引用计数，如果等于0，则free此vma
static inline void put_anon_vma(struct anon_vma *anon_vma)
{
	if (atomic_dec_and_test(&anon_vma->refcount))
		__put_anon_vma(anon_vma);
}

static inline void anon_vma_lock_write(struct anon_vma *anon_vma)
{
	down_write(&anon_vma->root->rwsem);
}

static inline void anon_vma_unlock_write(struct anon_vma *anon_vma)
{
	up_write(&anon_vma->root->rwsem);
}

static inline void anon_vma_lock_read(struct anon_vma *anon_vma)
{
	down_read(&anon_vma->root->rwsem);
}

static inline void anon_vma_unlock_read(struct anon_vma *anon_vma)
{
	up_read(&anon_vma->root->rwsem);
}


/*
 * anon_vma helper functions.
 */
void anon_vma_init(void);	/* create anon_vma_cachep */
int  anon_vma_prepare(struct vm_area_struct *);
void unlink_anon_vmas(struct vm_area_struct *);
int anon_vma_clone(struct vm_area_struct *, struct vm_area_struct *);
int anon_vma_fork(struct vm_area_struct *, struct vm_area_struct *);

static inline void anon_vma_merge(struct vm_area_struct *vma,
				  struct vm_area_struct *next)
{
	VM_BUG_ON_VMA(vma->anon_vma != next->anon_vma, vma);
	unlink_anon_vmas(next);
}

struct anon_vma *page_get_anon_vma(struct page *page);

/* bitflags for do_page_add_anon_rmap() */
#define RMAP_EXCLUSIVE 0x01
#define RMAP_COMPOUND 0x02

/*
 * rmap interfaces called when adding or removing pte of page
 */
void page_move_anon_rmap(struct page *, struct vm_area_struct *);
void page_add_anon_rmap(struct page *, struct vm_area_struct *,
		unsigned long, bool);
void do_page_add_anon_rmap(struct page *, struct vm_area_struct *,
			   unsigned long, int);
void page_add_new_anon_rmap(struct page *, struct vm_area_struct *,
		unsigned long, bool);
void page_add_file_rmap(struct page *, bool);
void page_remove_rmap(struct page *, bool);

void hugepage_add_anon_rmap(struct page *, struct vm_area_struct *,
			    unsigned long);
void hugepage_add_new_anon_rmap(struct page *, struct vm_area_struct *,
				unsigned long);

static inline void page_dup_rmap(struct page *page, bool compound)
{
	atomic_inc(compound ? compound_mapcount_ptr(page) : &page->_mapcount);
}

/*
 * Called from mm/vmscan.c to handle paging out
 */
int page_referenced(struct page *, int is_locked,
			struct mem_cgroup *memcg, unsigned long *vm_flags);

#define TTU_ACTION(x) ((x) & TTU_ACTION_MASK)

int try_to_unmap(struct page *, enum ttu_flags flags);

/*
 * Used by uprobes to replace a userspace page safely
 */
pte_t *__page_check_address(struct page *, struct mm_struct *,
				unsigned long, spinlock_t **, int);

static inline pte_t *page_check_address(struct page *page, struct mm_struct *mm,
					unsigned long address,
					spinlock_t **ptlp, int sync)
{
	pte_t *ptep;

	__cond_lock(*ptlp, ptep = __page_check_address(page, mm, address,
						       ptlp, sync));
	return ptep;
}

/*
 * Used by idle page tracking to check if a page was referenced via page
 * tables.
 */
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
bool page_check_address_transhuge(struct page *page, struct mm_struct *mm,
				  unsigned long address, pmd_t **pmdp,
				  pte_t **ptep, spinlock_t **ptlp);
#else
static inline bool page_check_address_transhuge(struct page *page,
				struct mm_struct *mm, unsigned long address,
				pmd_t **pmdp, pte_t **ptep, spinlock_t **ptlp)
{
	*ptep = page_check_address(page, mm, address, ptlp, 0);
	*pmdp = NULL;
	return !!*ptep;
}
#endif

/*
 * Used by swapoff to help locate where page is expected in vma.
 */
unsigned long page_address_in_vma(struct page *, struct vm_area_struct *);

/*
 * Cleans the PTEs of shared mappings.
 * (and since clean PTEs should also be readonly, write protects them too)
 *
 * returns the number of cleaned PTEs.
 */
int page_mkclean(struct page *);

/*
 * called in munlock()/munmap() path to check for other vmas holding
 * the page mlocked.
 */
int try_to_munlock(struct page *);

void remove_migration_ptes(struct page *old, struct page *new, bool locked);

/*
 * Called by memory-failure.c to kill processes.
 */
struct anon_vma *page_lock_anon_vma_read(struct page *page);
void page_unlock_anon_vma_read(struct anon_vma *anon_vma);
int page_mapped_in_vma(struct page *page, struct vm_area_struct *vma);

/*
 * rmap_walk_control: To control rmap traversing for specific needs
 *
 * arg: passed to rmap_one() and invalid_vma()
 * rmap_one: executed on each vma where page is mapped
 * done: for checking traversing termination condition
 * anon_lock: for getting anon_lock by optimized way rather than default
 * invalid_vma: for skipping uninterested vma
 */
struct rmap_walk_control {
	void *arg;
	int (*rmap_one)(struct page *page, struct vm_area_struct *vma,
					unsigned long addr, void *arg);
	int (*done)(struct page *page);
	struct anon_vma *(*anon_lock)(struct page *page);
	bool (*invalid_vma)(struct vm_area_struct *vma, void *arg);
};

int rmap_walk(struct page *page, struct rmap_walk_control *rwc);
int rmap_walk_locked(struct page *page, struct rmap_walk_control *rwc);

#else	/* !CONFIG_MMU */

#define anon_vma_init()		do {} while (0)
#define anon_vma_prepare(vma)	(0)
#define anon_vma_link(vma)	do {} while (0)

static inline int page_referenced(struct page *page, int is_locked,
				  struct mem_cgroup *memcg,
				  unsigned long *vm_flags)
{
	*vm_flags = 0;
	return 0;
}

#define try_to_unmap(page, refs) SWAP_FAIL

static inline int page_mkclean(struct page *page)
{
	return 0;
}


#endif	/* CONFIG_MMU */

/*
 * Return values of try_to_unmap
 */
#define SWAP_SUCCESS	0
#define SWAP_AGAIN	1
#define SWAP_FAIL	2
#define SWAP_MLOCK	3
#define SWAP_LZFREE	4

#endif	/* _LINUX_RMAP_H */

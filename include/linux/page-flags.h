/*
 * Macros for manipulating and testing page->flags
 */

#ifndef PAGE_FLAGS_H
#define PAGE_FLAGS_H

#include <linux/types.h>
#include <linux/bug.h>
#include <linux/mmdebug.h>
#ifndef __GENERATING_BOUNDS_H
#include <linux/mm_types.h>
#include <generated/bounds.h>
#endif /* !__GENERATING_BOUNDS_H */

/*
 * Various page->flags bits:
 *
 * PG_reserved is set for special pages, which can never be swapped out. Some
 * of them might not even exist (eg empty_bad_page)...
 *
 * The PG_private bitflag is set on pagecache pages if they contain filesystem
 * specific data (which is normally at page->private). It can be used by
 * private allocations for its own usage.
 *
 * During initiation of disk I/O, PG_locked is set. This bit is set before I/O
 * and cleared when writeback _starts_ or when read _completes_. PG_writeback
 * is set before writeback starts and cleared when it finishes.
 *
 * PG_locked also pins a page in pagecache, and blocks truncation of the file
 * while it is held.
 *
 * page_waitqueue(page) is a wait queue of all tasks waiting for the page
 * to become unlocked.
 *
 * PG_uptodate tells whether the page's contents is valid.  When a read
 * completes, the page becomes uptodate, unless a disk I/O error happened.
 *
 * PG_referenced, PG_reclaim are used for page reclaim for anonymous and
 * file-backed pagecache (see mm/vmscan.c).
 *
 * PG_error is set to indicate that an I/O error occurred on this page.
 *
 * PG_arch_1 is an architecture specific page state bit.  The generic code
 * guarantees that this bit is cleared for a page when it first is entered into
 * the page cache.
 *
 * PG_highmem pages are not permanently mapped into the kernel virtual address
 * space, they need to be kmapped separately for doing IO on the pages.  The
 * struct page (these bits with information) are always mapped into kernel
 * address space...
 *
 * PG_hwpoison indicates that a page got corrupted in hardware and contains
 * data with incorrect ECC bits that triggered a machine check. Accessing is
 * not safe since it may cause another machine check. Don't touch!
 */

/*
 * Don't use the *_dontuse flags.  Use the macros.  Otherwise you'll break
 * locked- and dirty-page accounting.
 *
 * The page flags field is split into two parts, the main flags area
 * which extends from the low bits upwards, and the fields area which
 * extends from the high bits downwards.
 *
 *  | FIELD | ... | FLAGS |
 *  N-1           ^       0
 *               (NR_PAGEFLAGS)
 *
 * The fields area is reserved for fields mapping zone, node (for NUMA) and
 * SPARSEMEM section (for variants of SPARSEMEM that require section ids like
 * SPARSEMEM_EXTREME with !SPARSEMEM_VMEMMAP).
 */
//page是否是dirty和是否最近被访问了，都是通过软件实现的
enum pageflags {
	PG_locked,		/* Page is locked. Don't touch. */
	PG_error,
//表示此page被访问到了	
	PG_referenced,
	PG_uptodate,
//dirty的概念是和后备文件系统(包括swap)相联系的，当内存中的内容被写时(就会和磁盘的内容不一致)，
//就设置page的dirty标志，anon(还没有被swap的)的页面是没有dirty概念的。
	PG_dirty,
//表示page被加入到了lru中	
	PG_lru,
//表示page在lru的active链表中或准备添加到active链表中	
	PG_active,
	PG_slab,
	PG_owner_priv_1,	/* Owner use. If pagecache, fs may use*/
	PG_arch_1,
/*
reserved，此类page是不能换出的	
属于这部分内存的有:
1. 在buddy内存管理开始运作之前分配的所有的内存，这些都存都是从memblock的
	reserved_mem直接标记为 PG_reserved。
	
*/
	PG_reserved,
//表示此page的private字段是有内容的，例如fs会将buffer_head放在page的private字段	
	PG_private,		/* If pagecache, has fs-private data */
	PG_private_2,		/* If pagecache, has fs aux data */
	PG_writeback,		/* Page is under writeback */
	PG_head,		/* A head page */
	//一个anon的page是否有swapcache
	PG_swapcache,		/* Swap page: swp_entry_t in private */
	PG_mappedtodisk,	/* Has blocks allocated on-disk */
//在内存回收时，当此页面可以回收，但是需要先写回时，在writeback之前设置此标志，
//这样在writeback后台写完之后判断如果有此标志，就可以将此页面回收了，见 pageout
	PG_reclaim,		/* To be reclaimed asap */
//此page属于swap的page，包括应用的堆，栈等都属于此,
//在anon的pagefault是会对anon页面设置此标志，调用路径
//do_anonymous_page->page_add_new_anon_rmap->__SetPageSwapBacked
//最终判断一个page是加入到file_lru还是anon_lru的判断条件就是
//这个标志，如果设置了此标志加入anon，如果没有设置此标志，则
//加入到file_lru,见 page_lru_base_type->page_is_file_cache,
//对于设置了PG_unevictable的page，加入到unevictable_lru,
//所以控制加入到哪个lru的就是通过这两个标志位来判断的。
	PG_swapbacked,		/* Page is backed by RAM/swap */
//此page已经被加入和将要加入unevictable 的list	
	PG_unevictable,		/* Page is "unevictable"  */
#ifdef CONFIG_MMU
	PG_mlocked,		/* Page is vma mlocked */
#endif
#ifdef CONFIG_ARCH_USES_PG_UNCACHED
	PG_uncached,		/* Page has been mapped as uncached */
#endif
#ifdef CONFIG_MEMORY_FAILURE
	PG_hwpoison,		/* hardware poisoned page. Don't touch */
#endif
#if defined(CONFIG_IDLE_PAGE_TRACKING) && defined(CONFIG_64BIT)
	PG_young,
	PG_idle,
#endif
	__NR_PAGEFLAGS,

	/* Filesystems */
	PG_checked = PG_owner_priv_1,

	/* Two page bits are conscripted by FS-Cache to maintain local caching
	 * state.  These bits are set on pages belonging to the netfs's inodes
	 * when those inodes are being locally cached.
	 */
	PG_fscache = PG_private_2,	/* page backed by cache */

	/* XEN */
	/* Pinned in Xen as a read-only pagetable page. */
	PG_pinned = PG_owner_priv_1,
	/* Pinned as part of domain save (see xen_mm_pin_all()). */
	PG_savepinned = PG_dirty,
	/* Has a grant mapping of another (foreign) domain's page. */
	PG_foreign = PG_owner_priv_1,

	/* SLOB */
	PG_slob_free = PG_private,

	/* Compound pages. Stored in first tail page's flags */
	PG_double_map = PG_private_2,

	/* non-lru isolated movable page */
	PG_isolated = PG_reclaim,
};

#ifndef __GENERATING_BOUNDS_H

struct page;	/* forward declaration */

//返回page的compound head
static inline struct page *compound_head(struct page *page)
{
	unsigned long head = READ_ONCE(page->compound_head);

	if (unlikely(head & 1))
		return (struct page *) (head - 1);
	return page;
}

//判断当前page是否是compound的一个page
static __always_inline int PageTail(struct page *page)
{
	return READ_ONCE(page->compound_head) & 1;
}

//判断一个页面是否是compound
static __always_inline int PageCompound(struct page *page)
{
	return test_bit(PG_head, &page->flags) || PageTail(page);
}

/*
 * Page flags policies wrt compound pages
 *
 * PF_ANY:
 *     the page flag is relevant for small, head and tail pages.
 *
 * PF_HEAD:
 *     for compound page all operations related to the page flag applied to
 *     head page.
 *
 * PF_NO_TAIL:
 *     modifications of the page flag must be done on small or head pages,
 *     checks can be done on tail pages too.
 *
 * PF_NO_COMPOUND:
 *     the page flag is not relevant for compound pages.
 */
#if 0
#define PF_ANY(page, enforce)	page
#define PF_HEAD(page, enforce)	compound_head(page)
#define PF_NO_TAIL(page, enforce) ({					\
		VM_BUG_ON_PGFLAGS(enforce && PageTail(page), page);	\
		compound_head(page);})
#define PF_NO_COMPOUND(page, enforce) ({				\
		VM_BUG_ON_PGFLAGS(enforce && PageCompound(page), page);	\
		page;})

/*
 * Macros to create function definitions for page flags
 */
#define TESTPAGEFLAG(uname, lname, policy)				\
static __always_inline int Page##uname(struct page *page)		\
	{ return test_bit(PG_##lname, &policy(page, 0)->flags); }

#define SETPAGEFLAG(uname, lname, policy)				\
static __always_inline void SetPage##uname(struct page *page)		\
	{ set_bit(PG_##lname, &policy(page, 1)->flags); }

#define CLEARPAGEFLAG(uname, lname, policy)				\
static __always_inline void ClearPage##uname(struct page *page)		\
	{ clear_bit(PG_##lname, &policy(page, 1)->flags); }

#define __SETPAGEFLAG(uname, lname, policy)				\
static __always_inline void __SetPage##uname(struct page *page)		\
	{ __set_bit(PG_##lname, &policy(page, 1)->flags); }

#define __CLEARPAGEFLAG(uname, lname, policy)				\
static __always_inline void __ClearPage##uname(struct page *page)	\
	{ __clear_bit(PG_##lname, &policy(page, 1)->flags); }

#define TESTSETFLAG(uname, lname, policy)				\
static __always_inline int TestSetPage##uname(struct page *page)	\
	{ return test_and_set_bit(PG_##lname, &policy(page, 1)->flags); }

#define TESTCLEARFLAG(uname, lname, policy)				\
static __always_inline int TestClearPage##uname(struct page *page)	\
	{ return test_and_clear_bit(PG_##lname, &policy(page, 1)->flags); }

#define PAGEFLAG(uname, lname, policy)					\
	TESTPAGEFLAG(uname, lname, policy)				\
	SETPAGEFLAG(uname, lname, policy)				\
	CLEARPAGEFLAG(uname, lname, policy)

#define __PAGEFLAG(uname, lname, policy)				\
	TESTPAGEFLAG(uname, lname, policy)				\
	__SETPAGEFLAG(uname, lname, policy)				\
	__CLEARPAGEFLAG(uname, lname, policy)

#define TESTSCFLAG(uname, lname, policy)				\
	TESTSETFLAG(uname, lname, policy)				\
	TESTCLEARFLAG(uname, lname, policy)

#define TESTPAGEFLAG_FALSE(uname)					\
static inline int Page##uname(const struct page *page) { return 0; }

#define SETPAGEFLAG_NOOP(uname)						\
static inline void SetPage##uname(struct page *page) {  }

#define CLEARPAGEFLAG_NOOP(uname)					\
static inline void ClearPage##uname(struct page *page) {  }

#define __CLEARPAGEFLAG_NOOP(uname)					\
static inline void __ClearPage##uname(struct page *page) {  }

#define TESTSETFLAG_FALSE(uname)					\
static inline int TestSetPage##uname(struct page *page) { return 0; }

#define TESTCLEARFLAG_FALSE(uname)					\
static inline int TestClearPage##uname(struct page *page) { return 0; }

#define PAGEFLAG_FALSE(uname) TESTPAGEFLAG_FALSE(uname)			\
	SETPAGEFLAG_NOOP(uname) CLEARPAGEFLAG_NOOP(uname)

#define TESTSCFLAG_FALSE(uname)						\
	TESTSETFLAG_FALSE(uname) TESTCLEARFLAG_FALSE(uname)

__PAGEFLAG(Locked, locked, PF_NO_TAIL)
PAGEFLAG(Error, error, PF_NO_COMPOUND) TESTCLEARFLAG(Error, error, PF_NO_COMPOUND)
PAGEFLAG(Referenced, referenced, PF_HEAD)
	TESTCLEARFLAG(Referenced, referenced, PF_HEAD)
	__SETPAGEFLAG(Referenced, referenced, PF_HEAD)
PAGEFLAG(Dirty, dirty, PF_HEAD) TESTSCFLAG(Dirty, dirty, PF_HEAD)
	__CLEARPAGEFLAG(Dirty, dirty, PF_HEAD)
PAGEFLAG(LRU, lru, PF_HEAD) __CLEARPAGEFLAG(LRU, lru, PF_HEAD)
PAGEFLAG(Active, active, PF_HEAD) __CLEARPAGEFLAG(Active, active, PF_HEAD)
	TESTCLEARFLAG(Active, active, PF_HEAD)
__PAGEFLAG(Slab, slab, PF_NO_TAIL)
__PAGEFLAG(SlobFree, slob_free, PF_NO_TAIL)
PAGEFLAG(Checked, checked, PF_NO_COMPOUND)	   /* Used by some filesystems */

/* Xen */
PAGEFLAG(Pinned, pinned, PF_NO_COMPOUND)
	TESTSCFLAG(Pinned, pinned, PF_NO_COMPOUND)
PAGEFLAG(SavePinned, savepinned, PF_NO_COMPOUND);
PAGEFLAG(Foreign, foreign, PF_NO_COMPOUND);

PAGEFLAG(Reserved, reserved, PF_NO_COMPOUND)
	__CLEARPAGEFLAG(Reserved, reserved, PF_NO_COMPOUND)
PAGEFLAG(SwapBacked, swapbacked, PF_NO_TAIL)
	__CLEARPAGEFLAG(SwapBacked, swapbacked, PF_NO_TAIL)
	__SETPAGEFLAG(SwapBacked, swapbacked, PF_NO_TAIL)

/*
 * Private page markings that may be used by the filesystem that owns the page
 * for its own purposes.
 * - PG_private and PG_private_2 cause releasepage() and co to be invoked
 */
PAGEFLAG(Private, private, PF_ANY) __SETPAGEFLAG(Private, private, PF_ANY)
	__CLEARPAGEFLAG(Private, private, PF_ANY)
PAGEFLAG(Private2, private_2, PF_ANY) TESTSCFLAG(Private2, private_2, PF_ANY)
PAGEFLAG(OwnerPriv1, owner_priv_1, PF_ANY)
	TESTCLEARFLAG(OwnerPriv1, owner_priv_1, PF_ANY)

/*
 * Only test-and-set exist for PG_writeback.  The unconditional operators are
 * risky: they bypass page accounting.
 */
TESTPAGEFLAG(Writeback, writeback, PF_NO_COMPOUND)
	TESTSCFLAG(Writeback, writeback, PF_NO_COMPOUND)
PAGEFLAG(MappedToDisk, mappedtodisk, PF_NO_TAIL)

/* PG_readahead is only used for reads; PG_reclaim is only for writes */
PAGEFLAG(Reclaim, reclaim, PF_NO_TAIL)
	TESTCLEARFLAG(Reclaim, reclaim, PF_NO_TAIL)
PAGEFLAG(Readahead, reclaim, PF_NO_COMPOUND)
	TESTCLEARFLAG(Readahead, reclaim, PF_NO_COMPOUND)

#ifdef CONFIG_HIGHMEM
/*
 * Must use a macro here due to header dependency issues. page_zone() is not
 * available at this point.
 */
#define PageHighMem(__p) is_highmem_idx(page_zonenum(__p))
#else
PAGEFLAG_FALSE(HighMem)
#endif

#ifdef CONFIG_SWAP
PAGEFLAG(SwapCache, swapcache, PF_NO_COMPOUND)
#else
PAGEFLAG_FALSE(SwapCache)
#endif

PAGEFLAG(Unevictable, unevictable, PF_HEAD)
	__CLEARPAGEFLAG(Unevictable, unevictable, PF_HEAD)
	TESTCLEARFLAG(Unevictable, unevictable, PF_HEAD)

#ifdef CONFIG_MMU
PAGEFLAG(Mlocked, mlocked, PF_NO_TAIL)
	__CLEARPAGEFLAG(Mlocked, mlocked, PF_NO_TAIL)
	TESTSCFLAG(Mlocked, mlocked, PF_NO_TAIL)
#else
PAGEFLAG_FALSE(Mlocked) __CLEARPAGEFLAG_NOOP(Mlocked)
	TESTSCFLAG_FALSE(Mlocked)
#endif

#ifdef CONFIG_ARCH_USES_PG_UNCACHED
PAGEFLAG(Uncached, uncached, PF_NO_COMPOUND)
#else
PAGEFLAG_FALSE(Uncached)
#endif

#ifdef CONFIG_MEMORY_FAILURE
PAGEFLAG(HWPoison, hwpoison, PF_ANY)
TESTSCFLAG(HWPoison, hwpoison, PF_ANY)
#define __PG_HWPOISON (1UL << PG_hwpoison)
#else
PAGEFLAG_FALSE(HWPoison)
#define __PG_HWPOISON 0
#endif

#if defined(CONFIG_IDLE_PAGE_TRACKING) && defined(CONFIG_64BIT)
TESTPAGEFLAG(Young, young, PF_ANY)
SETPAGEFLAG(Young, young, PF_ANY)
TESTCLEARFLAG(Young, young, PF_ANY)
PAGEFLAG(Idle, idle, PF_ANY)
#endif

/*
 * On an anonymous page mapped into a user virtual memory area,
 * page->mapping points to its anon_vma, not to a struct address_space;
 * with the PAGE_MAPPING_ANON bit set to distinguish it.  See rmap.h.
 *
 * On an anonymous page in a VM_MERGEABLE area, if CONFIG_KSM is enabled,
 * the PAGE_MAPPING_MOVABLE bit may be set along with the PAGE_MAPPING_ANON
 * bit; and then page->mapping points, not to an anon_vma, but to a private
 * structure which KSM associates with that merged page.  See ksm.h.
 *
 * PAGE_MAPPING_KSM without PAGE_MAPPING_ANON is used for non-lru movable
 * page and then page->mapping points a struct address_space.
 *
 * Please note that, confusingly, "page_mapping" refers to the inode
 * address_space which maps the page from disk; whereas "page_mapped"
 * refers to user virtual address space into which the page is mapped.
 */
#else
//宏的处理结果如下
static inline   int PageLocked(struct page *page)
{
    return (__builtin_constant_p((PG_locked)) ? constant_test_bit((PG_locked), (&({ ((void)(sizeof((long)(0 && PageTail(page))))); compound_head(page);})->flags)) : variable_test_bit((PG_locked), (&({ ((void)(sizeof((long)(0 && PageTail(page))))); compound_head(page);})->flags)));
}
static inline   void __SetPageLocked(struct page *page)
{
    __set_bit(PG_locked, &({ ((void)(sizeof((long)(1 && PageTail(page))))); compound_head(page);})->flags);
}
static inline   void __ClearPageLocked(struct page *page)
{
    __clear_bit(PG_locked, &({ ((void)(sizeof((long)(1 && PageTail(page))))); compound_head(page);})->flags);
}
static inline   int PageError(struct page *page)
{
    return (__builtin_constant_p((PG_error)) ? constant_test_bit((PG_error), (&({ ((void)(sizeof((long)(0 && PageCompound(page))))); page;})->flags)) : variable_test_bit((PG_error), (&({ ((void)(sizeof((long)(0 && PageCompound(page))))); page;})->flags)));
}
static inline   void SetPageError(struct page *page)
{
    set_bit(PG_error, &({ ((void)(sizeof((long)(1 && PageCompound(page))))); page;})->flags);
}
static inline   void ClearPageError(struct page *page)
{
    clear_bit(PG_error, &({ ((void)(sizeof((long)(1 && PageCompound(page))))); page;})->flags);
}
static inline   int TestClearPageError(struct page *page)
{
    return test_and_clear_bit(PG_error, &({ ((void)(sizeof((long)(1 && PageCompound(page))))); page;})->flags);
}
static inline   int PageReferenced(struct page *page)
{
    return (__builtin_constant_p((PG_referenced)) ? constant_test_bit((PG_referenced), (&compound_head(page)->flags)) : variable_test_bit((PG_referenced), (&compound_head(page)->flags)));
}
static inline   void SetPageReferenced(struct page *page)
{
    set_bit(PG_referenced, &compound_head(page)->flags);
}
static inline   void ClearPageReferenced(struct page *page)
{
    clear_bit(PG_referenced, &compound_head(page)->flags);
}
static inline   int TestClearPageReferenced(struct page *page)
{
    return test_and_clear_bit(PG_referenced, &compound_head(page)->flags);
}
static inline   void __SetPageReferenced(struct page *page)
{
    __set_bit(PG_referenced, &compound_head(page)->flags);
}
static inline   int PageDirty(struct page *page)
{
    return (__builtin_constant_p((PG_dirty)) ? constant_test_bit((PG_dirty), (&compound_head(page)->flags)) : variable_test_bit((PG_dirty), (&compound_head(page)->flags)));
}
static inline   void SetPageDirty(struct page *page)
{
    set_bit(PG_dirty, &compound_head(page)->flags);
}
static inline   void ClearPageDirty(struct page *page)
{
    clear_bit(PG_dirty, &compound_head(page)->flags);
}
static inline   int TestSetPageDirty(struct page *page)
{
    return test_and_set_bit(PG_dirty, &compound_head(page)->flags);
}
static inline   int TestClearPageDirty(struct page *page)
{
    return test_and_clear_bit(PG_dirty, &compound_head(page)->flags);
}
static inline   void __ClearPageDirty(struct page *page)
{
    __clear_bit(PG_dirty, &compound_head(page)->flags);
}
static inline   int PageLRU(struct page *page)
{
    return (__builtin_constant_p((PG_lru)) ? constant_test_bit((PG_lru), (&compound_head(page)->flags)) : variable_test_bit((PG_lru), (&compound_head(page)->flags)));
}
//设置lru，此位表示page是否在lru中
static inline   void SetPageLRU(struct page *page)
{
    set_bit(PG_lru, &compound_head(page)->flags);
}
static inline   void ClearPageLRU(struct page *page)
{
    clear_bit(PG_lru, &compound_head(page)->flags);
}
static inline   void __ClearPageLRU(struct page *page)
{
    __clear_bit(PG_lru, &compound_head(page)->flags);
}
static inline   int PageActive(struct page *page)
{
    return (__builtin_constant_p((PG_active)) ? constant_test_bit((PG_active), (&compound_head(page)->flags)) : variable_test_bit((PG_active), (&compound_head(page)->flags)));
}
static inline   void SetPageActive(struct page *page)
{
    set_bit(PG_active, &compound_head(page)->flags);
}
static inline   void ClearPageActive(struct page *page)
{
    clear_bit(PG_active, &compound_head(page)->flags);
}
static inline   void __ClearPageActive(struct page *page)
{
    __clear_bit(PG_active, &compound_head(page)->flags);
}
static inline   int TestClearPageActive(struct page *page)
{
    return test_and_clear_bit(PG_active, &compound_head(page)->flags);
}
static inline   int PageSlab(struct page *page)
{
    return (__builtin_constant_p((PG_slab)) ? constant_test_bit((PG_slab), (&({ ((void)(sizeof((long)(0 && PageTail(page))))); compound_head(page);})->flags)) : variable_test_bit((PG_slab), (&({ ((void)(sizeof((long)(0 && PageTail(page))))); compound_head(page);})->flags)));
}
static inline   void __SetPageSlab(struct page *page)
{
    __set_bit(PG_slab, &({ ((void)(sizeof((long)(1 && PageTail(page))))); compound_head(page);})->flags);
}
static inline   void __ClearPageSlab(struct page *page)
{
    __clear_bit(PG_slab, &({ ((void)(sizeof((long)(1 && PageTail(page))))); compound_head(page);})->flags);
}
static inline   int PageSlobFree(struct page *page)
{
    return (__builtin_constant_p((PG_slob_free)) ? constant_test_bit((PG_slob_free), (&({ ((void)(sizeof((long)(0 && PageTail(page))))); compound_head(page);})->flags)) : variable_test_bit((PG_slob_free), (&({ ((void)(sizeof((long)(0 && PageTail(page))))); compound_head(page);})->flags)));
}
static inline   void __SetPageSlobFree(struct page *page)
{
    __set_bit(PG_slob_free, &({ ((void)(sizeof((long)(1 && PageTail(page))))); compound_head(page);})->flags);
}
static inline   void __ClearPageSlobFree(struct page *page)
{
    __clear_bit(PG_slob_free, &({ ((void)(sizeof((long)(1 && PageTail(page))))); compound_head(page);})->flags);
}
static inline   int PageChecked(struct page *page)
{
    return (__builtin_constant_p((PG_checked)) ? constant_test_bit((PG_checked), (&({ ((void)(sizeof((long)(0 && PageCompound(page))))); page;})->flags)) : variable_test_bit((PG_checked), (&({ ((void)(sizeof((long)(0 && PageCompound(page))))); page;})->flags)));
}
static inline   void SetPageChecked(struct page *page)
{
    set_bit(PG_checked, &({ ((void)(sizeof((long)(1 && PageCompound(page))))); page;})->flags);
}
static inline   void ClearPageChecked(struct page *page)
{
    clear_bit(PG_checked, &({ ((void)(sizeof((long)(1 && PageCompound(page))))); page;})->flags);
}


static inline   int PagePinned(struct page *page)
{
    return (__builtin_constant_p((PG_pinned)) ? constant_test_bit((PG_pinned), (&({ ((void)(sizeof((long)(0 && PageCompound(page))))); page;})->flags)) : variable_test_bit((PG_pinned), (&({ ((void)(sizeof((long)(0 && PageCompound(page))))); page;})->flags)));
}
static inline   void SetPagePinned(struct page *page)
{
    set_bit(PG_pinned, &({ ((void)(sizeof((long)(1 && PageCompound(page))))); page;})->flags);
}
static inline   void ClearPagePinned(struct page *page)
{
    clear_bit(PG_pinned, &({ ((void)(sizeof((long)(1 && PageCompound(page))))); page;})->flags);
}
static inline   int TestSetPagePinned(struct page *page)
{
    return test_and_set_bit(PG_pinned, &({ ((void)(sizeof((long)(1 && PageCompound(page))))); page;})->flags);
}
static inline   int TestClearPagePinned(struct page *page)
{
    return test_and_clear_bit(PG_pinned, &({ ((void)(sizeof((long)(1 && PageCompound(page))))); page;})->flags);
}
static inline   int PageSavePinned(struct page *page)
{
    return (__builtin_constant_p((PG_savepinned)) ? constant_test_bit((PG_savepinned), (&({ ((void)(sizeof((long)(0 && PageCompound(page))))); page;})->flags)) : variable_test_bit((PG_savepinned), (&({ ((void)(sizeof((long)(0 && PageCompound(page))))); page;})->flags)));
}
static inline   void SetPageSavePinned(struct page *page)
{
    set_bit(PG_savepinned, &({ ((void)(sizeof((long)(1 && PageCompound(page))))); page;})->flags);
}
static inline   void ClearPageSavePinned(struct page *page)
{
    clear_bit(PG_savepinned, &({ ((void)(sizeof((long)(1 && PageCompound(page))))); page;})->flags);
};
static inline   int PageForeign(struct page *page)
{
    return (__builtin_constant_p((PG_foreign)) ? constant_test_bit((PG_foreign), (&({ ((void)(sizeof((long)(0 && PageCompound(page))))); page;})->flags)) : variable_test_bit((PG_foreign), (&({ ((void)(sizeof((long)(0 && PageCompound(page))))); page;})->flags)));
}
static inline   void SetPageForeign(struct page *page)
{
    set_bit(PG_foreign, &({ ((void)(sizeof((long)(1 && PageCompound(page))))); page;})->flags);
}
static inline   void ClearPageForeign(struct page *page)
{
    clear_bit(PG_foreign, &({ ((void)(sizeof((long)(1 && PageCompound(page))))); page;})->flags);
};

static inline   int PageReserved(struct page *page)
{
    return (__builtin_constant_p((PG_reserved)) ? constant_test_bit((PG_reserved), (&({ ((void)(sizeof((long)(0 && PageCompound(page))))); page;})->flags)) : variable_test_bit((PG_reserved), (&({ ((void)(sizeof((long)(0 && PageCompound(page))))); page;})->flags)));
}
static inline   void SetPageReserved(struct page *page)
{
    set_bit(PG_reserved, &({ ((void)(sizeof((long)(1 && PageCompound(page))))); page;})->flags);
}
static inline   void ClearPageReserved(struct page *page)
{
    clear_bit(PG_reserved, &({ ((void)(sizeof((long)(1 && PageCompound(page))))); page;})->flags);
}
static inline   void __ClearPageReserved(struct page *page)
{
    __clear_bit(PG_reserved, &({ ((void)(sizeof((long)(1 && PageCompound(page))))); page;})->flags);
}
static inline   int PageSwapBacked(struct page *page)
{
    return (__builtin_constant_p((PG_swapbacked)) ? constant_test_bit((PG_swapbacked), (&({ ((void)(sizeof((long)(0 && PageTail(page))))); compound_head(page);})->flags)) : variable_test_bit((PG_swapbacked), (&({ ((void)(sizeof((long)(0 && PageTail(page))))); compound_head(page);})->flags)));
}
static inline   void SetPageSwapBacked(struct page *page)
{
    set_bit(PG_swapbacked, &({ ((void)(sizeof((long)(1 && PageTail(page))))); compound_head(page);})->flags);
}
static inline   void ClearPageSwapBacked(struct page *page)
{
    clear_bit(PG_swapbacked, &({ ((void)(sizeof((long)(1 && PageTail(page))))); compound_head(page);})->flags);
}
static inline   void __ClearPageSwapBacked(struct page *page)
{
    __clear_bit(PG_swapbacked, &({ ((void)(sizeof((long)(1 && PageTail(page))))); compound_head(page);})->flags);
}
static inline   void __SetPageSwapBacked(struct page *page)
{
    __set_bit(PG_swapbacked, &({ ((void)(sizeof((long)(1 && PageTail(page))))); compound_head(page);})->flags);
}






static inline   int PagePrivate(struct page *page)
{
    return (__builtin_constant_p((PG_private)) ? constant_test_bit((PG_private), (&page->flags)) : variable_test_bit((PG_private), (&page->flags)));
}
static inline   void SetPagePrivate(struct page *page)
{
    set_bit(PG_private, &page->flags);
}
static inline   void ClearPagePrivate(struct page *page)
{
    clear_bit(PG_private, &page->flags);
}
static inline   void __SetPagePrivate(struct page *page)
{
    __set_bit(PG_private, &page->flags);
}
static inline   void __ClearPagePrivate(struct page *page)
{
    __clear_bit(PG_private, &page->flags);
}
static inline   int PagePrivate2(struct page *page)
{
    return (__builtin_constant_p((PG_private_2)) ? constant_test_bit((PG_private_2), (&page->flags)) : variable_test_bit((PG_private_2), (&page->flags)));
}
static inline   void SetPagePrivate2(struct page *page)
{
    set_bit(PG_private_2, &page->flags);
}
static inline   void ClearPagePrivate2(struct page *page)
{
    clear_bit(PG_private_2, &page->flags);
}
static inline   int TestSetPagePrivate2(struct page *page)
{
    return test_and_set_bit(PG_private_2, &page->flags);
}
static inline   int TestClearPagePrivate2(struct page *page)
{
    return test_and_clear_bit(PG_private_2, &page->flags);
}
static inline   int PageOwnerPriv1(struct page *page)
{
    return (__builtin_constant_p((PG_owner_priv_1)) ? constant_test_bit((PG_owner_priv_1), (&page->flags)) : variable_test_bit((PG_owner_priv_1), (&page->flags)));
}
static inline   void SetPageOwnerPriv1(struct page *page)
{
    set_bit(PG_owner_priv_1, &page->flags);
}
static inline   void ClearPageOwnerPriv1(struct page *page)
{
    clear_bit(PG_owner_priv_1, &page->flags);
}
static inline   int TestClearPageOwnerPriv1(struct page *page)
{
    return test_and_clear_bit(PG_owner_priv_1, &page->flags);
}





static inline   int PageWriteback(struct page *page)
{
    return (__builtin_constant_p((PG_writeback)) ? constant_test_bit((PG_writeback), (&({ ((void)(sizeof((long)(0 && PageCompound(page))))); page;})->flags)) : variable_test_bit((PG_writeback), (&({ ((void)(sizeof((long)(0 && PageCompound(page))))); page;})->flags)));
}
static inline   int TestSetPageWriteback(struct page *page)
{
    return test_and_set_bit(PG_writeback, &({ ((void)(sizeof((long)(1 && PageCompound(page))))); page;})->flags);
}
static inline   int TestClearPageWriteback(struct page *page)
{
    return test_and_clear_bit(PG_writeback, &({ ((void)(sizeof((long)(1 && PageCompound(page))))); page;})->flags);
}
static inline   int PageMappedToDisk(struct page *page)
{
    return (__builtin_constant_p((PG_mappedtodisk)) ? constant_test_bit((PG_mappedtodisk), (&({ ((void)(sizeof((long)(0 && PageTail(page))))); compound_head(page);})->flags)) : variable_test_bit((PG_mappedtodisk), (&({ ((void)(sizeof((long)(0 && PageTail(page))))); compound_head(page);})->flags)));
}
static inline   void SetPageMappedToDisk(struct page *page)
{
    set_bit(PG_mappedtodisk, &({ ((void)(sizeof((long)(1 && PageTail(page))))); compound_head(page);})->flags);
}
static inline   void ClearPageMappedToDisk(struct page *page)
{
    clear_bit(PG_mappedtodisk, &({ ((void)(sizeof((long)(1 && PageTail(page))))); compound_head(page);})->flags);
}


static inline   int PageReclaim(struct page *page)
{
    return (__builtin_constant_p((PG_reclaim)) ? constant_test_bit((PG_reclaim), (&({ ((void)(sizeof((long)(0 && PageTail(page))))); compound_head(page);})->flags)) : variable_test_bit((PG_reclaim), (&({ ((void)(sizeof((long)(0 && PageTail(page))))); compound_head(page);})->flags)));
}
static inline   void SetPageReclaim(struct page *page)
{
    set_bit(PG_reclaim, &({ ((void)(sizeof((long)(1 && PageTail(page))))); compound_head(page);})->flags);
}
static inline   void ClearPageReclaim(struct page *page)
{
    clear_bit(PG_reclaim, &({ ((void)(sizeof((long)(1 && PageTail(page))))); compound_head(page);})->flags);
}
static inline   int TestClearPageReclaim(struct page *page)
{
    return test_and_clear_bit(PG_reclaim, &({ ((void)(sizeof((long)(1 && PageTail(page))))); compound_head(page);})->flags);
}
static inline   int PageReadahead(struct page *page)
{
    return (__builtin_constant_p((PG_reclaim)) ? constant_test_bit((PG_reclaim), (&({ ((void)(sizeof((long)(0 && PageCompound(page))))); page;})->flags)) : variable_test_bit((PG_reclaim), (&({ ((void)(sizeof((long)(0 && PageCompound(page))))); page;})->flags)));
}
static inline   void SetPageReadahead(struct page *page)
{
    set_bit(PG_reclaim, &({ ((void)(sizeof((long)(1 && PageCompound(page))))); page;})->flags);
}
static inline   void ClearPageReadahead(struct page *page)
{
    clear_bit(PG_reclaim, &({ ((void)(sizeof((long)(1 && PageCompound(page))))); page;})->flags);
}
static inline   int TestClearPageReadahead(struct page *page)
{
    return test_and_clear_bit(PG_reclaim, &({ ((void)(sizeof((long)(1 && PageCompound(page))))); page;})->flags);
}
// 313 "./include/linux/page-flags.h"
static inline  int PageHighMem(const struct page *page)
{
    return 0;
}
static inline  void SetPageHighMem(struct page *page) { } static inline  void ClearPageHighMem(struct page *page) { }



static inline   int PageSwapCache(struct page *page)
{
    return (__builtin_constant_p((PG_swapcache)) ? constant_test_bit((PG_swapcache), (&({ ((void)(sizeof((long)(0 && PageCompound(page))))); page;})->flags)) : variable_test_bit((PG_swapcache), (&({ ((void)(sizeof((long)(0 && PageCompound(page))))); page;})->flags)));
}
static inline   void SetPageSwapCache(struct page *page)
{
    set_bit(PG_swapcache, &({ ((void)(sizeof((long)(1 && PageCompound(page))))); page;})->flags);
}
static inline   void ClearPageSwapCache(struct page *page)
{
    clear_bit(PG_swapcache, &({ ((void)(sizeof((long)(1 && PageCompound(page))))); page;})->flags);
}




static inline   int PageUnevictable(struct page *page)
{
    return (__builtin_constant_p((PG_unevictable)) ? constant_test_bit((PG_unevictable), (&compound_head(page)->flags)) : variable_test_bit((PG_unevictable), (&compound_head(page)->flags)));
}
static inline   void SetPageUnevictable(struct page *page)
{
    set_bit(PG_unevictable, &compound_head(page)->flags);
}
static inline   void ClearPageUnevictable(struct page *page)
{
    clear_bit(PG_unevictable, &compound_head(page)->flags);
}
static inline   void __ClearPageUnevictable(struct page *page)
{
    __clear_bit(PG_unevictable, &compound_head(page)->flags);
}
static inline   int TestClearPageUnevictable(struct page *page)
{
    return test_and_clear_bit(PG_unevictable, &compound_head(page)->flags);
}


static inline   int PageMlocked(struct page *page)
{
    return (__builtin_constant_p((PG_mlocked)) ? constant_test_bit((PG_mlocked), (&({ ((void)(sizeof((long)(0 && PageTail(page))))); compound_head(page);})->flags)) : variable_test_bit((PG_mlocked), (&({ ((void)(sizeof((long)(0 && PageTail(page))))); compound_head(page);})->flags)));
}
static inline   void SetPageMlocked(struct page *page)
{
    set_bit(PG_mlocked, &({ ((void)(sizeof((long)(1 && PageTail(page))))); compound_head(page);})->flags);
}
static inline   void ClearPageMlocked(struct page *page)
{
    clear_bit(PG_mlocked, &({ ((void)(sizeof((long)(1 && PageTail(page))))); compound_head(page);})->flags);
}
static inline   void __ClearPageMlocked(struct page *page)
{
    __clear_bit(PG_mlocked, &({ ((void)(sizeof((long)(1 && PageTail(page))))); compound_head(page);})->flags);
}
static inline   int TestSetPageMlocked(struct page *page)
{
    return test_and_set_bit(PG_mlocked, &({ ((void)(sizeof((long)(1 && PageTail(page))))); compound_head(page);})->flags);
}
static inline   int TestClearPageMlocked(struct page *page)
{
    return test_and_clear_bit(PG_mlocked, &({ ((void)(sizeof((long)(1 && PageTail(page))))); compound_head(page);})->flags);
}






static inline   int PageUncached(struct page *page)
{
    return (__builtin_constant_p((PG_uncached)) ? constant_test_bit((PG_uncached), (&({ ((void)(sizeof((long)(0 && PageCompound(page))))); page;})->flags)) : variable_test_bit((PG_uncached), (&({ ((void)(sizeof((long)(0 && PageCompound(page))))); page;})->flags)));
}
static inline   void SetPageUncached(struct page *page)
{
    set_bit(PG_uncached, &({ ((void)(sizeof((long)(1 && PageCompound(page))))); page;})->flags);
}
static inline   void ClearPageUncached(struct page *page)
{
    clear_bit(PG_uncached, &({ ((void)(sizeof((long)(1 && PageCompound(page))))); page;})->flags);
}
// 346 "./include/linux/page-flags.h"
static inline  int PageHWPoison(const struct page *page)
{
    return 0;
}
static inline  void SetPageHWPoison(struct page *page) { } static inline  void ClearPageHWPoison(struct page *page) { }

#endif
#define PAGE_MAPPING_ANON	0x1
#define PAGE_MAPPING_MOVABLE	0x2
#define PAGE_MAPPING_KSM	(PAGE_MAPPING_ANON | PAGE_MAPPING_MOVABLE)
#define PAGE_MAPPING_FLAGS	(PAGE_MAPPING_ANON | PAGE_MAPPING_MOVABLE)

static __always_inline int PageMappingFlags(struct page *page)
{
	return ((unsigned long)page->mapping & PAGE_MAPPING_FLAGS) != 0;
}
//page是否是匿名page
static __always_inline int PageAnon(struct page *page)
{
	page = compound_head(page);
	return ((unsigned long)page->mapping & PAGE_MAPPING_ANON) != 0;
}
//page是否是可以移动的
static __always_inline int __PageMovable(struct page *page)
{
	return ((unsigned long)page->mapping & PAGE_MAPPING_FLAGS) ==
				PAGE_MAPPING_MOVABLE;
}

#ifdef CONFIG_KSM
/*
 * A KSM page is one of those write-protected "shared pages" or "merged pages"
 * which KSM maps into multiple mms, wherever identical anonymous page content
 * is found in VM_MERGEABLE vmas.  It's a PageAnon page, pointing not to any
 * anon_vma, but to that page's node of the stable tree.
 */
static __always_inline int PageKsm(struct page *page)
{
	page = compound_head(page);
	return ((unsigned long)page->mapping & PAGE_MAPPING_FLAGS) ==
				PAGE_MAPPING_KSM;
}
#else
TESTPAGEFLAG_FALSE(Ksm)
#endif

u64 stable_page_flags(struct page *page);

static inline int PageUptodate(struct page *page)
{
	int ret;
	page = compound_head(page);
	ret = test_bit(PG_uptodate, &(page)->flags);
	/*
	 * Must ensure that the data we read out of the page is loaded
	 * _after_ we've loaded page->flags to check for PageUptodate.
	 * We can skip the barrier if the page is not uptodate, because
	 * we wouldn't be reading anything from it.
	 *
	 * See SetPageUptodate() for the other side of the story.
	 */
	if (ret)
		smp_rmb();

	return ret;
}

static __always_inline void __SetPageUptodate(struct page *page)
{
	VM_BUG_ON_PAGE(PageTail(page), page);
	smp_wmb();
	__set_bit(PG_uptodate, &page->flags);
}

static __always_inline void SetPageUptodate(struct page *page)
{
	VM_BUG_ON_PAGE(PageTail(page), page);
	/*
	 * Memory barrier must be issued before setting the PG_uptodate bit,
	 * so that all previous stores issued in order to bring the page
	 * uptodate are actually visible before PageUptodate becomes true.
	 */
	smp_wmb();
	set_bit(PG_uptodate, &page->flags);
}

CLEARPAGEFLAG(Uptodate, uptodate, PF_NO_TAIL)

int test_clear_page_writeback(struct page *page);
int __test_set_page_writeback(struct page *page, bool keep_write);

#define test_set_page_writeback(page)			\
	__test_set_page_writeback(page, false)
#define test_set_page_writeback_keepwrite(page)	\
	__test_set_page_writeback(page, true)

static inline void set_page_writeback(struct page *page)
{
	test_set_page_writeback(page);
}

static inline void set_page_writeback_keepwrite(struct page *page)
{
	test_set_page_writeback_keepwrite(page);
}
#if 0
__PAGEFLAG(Head, head, PF_ANY) CLEARPAGEFLAG(Head, head, PF_ANY)
#else
//处理之后如下
static inline int PageHead(struct page *page)
{
    return (__builtin_constant_p((PG_head)) ? constant_test_bit((PG_head), (&page->flags)) : variable_test_bit((PG_head), (&page->flags)));
}
static inline void __SetPageHead(struct page *page)
{
    __set_bit(PG_head, &page->flags);
}
static inline void __ClearPageHead(struct page *page)
{
    __clear_bit(PG_head, &page->flags);
}
static inline void ClearPageHead(struct page *page)
{
    clear_bit(PG_head, &page->flags);
}

static void set_compound_head(struct page *page, struct page *head)
{
	WRITE_ONCE(page->compound_head, (unsigned long)head + 1);
}

static void clear_compound_head(struct page *page)
{
	WRITE_ONCE(page->compound_head, 0);
}

#ifdef CONFIG_TRANSPARENT_HUGEPAGE
static inline void ClearPageCompound(struct page *page)
{
	BUG_ON(!PageHead(page));
	ClearPageHead(page);
}
#endif

#define PG_head_mask ((1UL << PG_head))

#ifdef CONFIG_HUGETLB_PAGE
int PageHuge(struct page *page);
int PageHeadHuge(struct page *page);
bool page_huge_active(struct page *page);
#else
TESTPAGEFLAG_FALSE(Huge)
TESTPAGEFLAG_FALSE(HeadHuge)

static inline bool page_huge_active(struct page *page)
{
	return 0;
}
#endif


#ifdef CONFIG_TRANSPARENT_HUGEPAGE
/*
 * PageHuge() only returns true for hugetlbfs pages, but not for
 * normal or transparent huge pages.
 *
 * PageTransHuge() returns true for both transparent huge and
 * hugetlbfs pages, but not normal pages. PageTransHuge() can only be
 * called only in the core VM paths where hugetlbfs pages can't exist.
 */
static inline int PageTransHuge(struct page *page)
{
	VM_BUG_ON_PAGE(PageTail(page), page);
	return PageHead(page);
}

/*
 * PageTransCompound returns true for both transparent huge pages
 * and hugetlbfs pages, so it should only be called when it's known
 * that hugetlbfs pages aren't involved.
 */
static inline int PageTransCompound(struct page *page)
{
	return PageCompound(page);
}

/*
 * PageTransCompoundMap is the same as PageTransCompound, but it also
 * guarantees the primary MMU has the entire compound page mapped
 * through pmd_trans_huge, which in turn guarantees the secondary MMUs
 * can also map the entire compound page. This allows the secondary
 * MMUs to call get_user_pages() only once for each compound page and
 * to immediately map the entire compound page with a single secondary
 * MMU fault. If there will be a pmd split later, the secondary MMUs
 * will get an update through the MMU notifier invalidation through
 * split_huge_pmd().
 *
 * Unlike PageTransCompound, this is safe to be called only while
 * split_huge_pmd() cannot run from under us, like if protected by the
 * MMU notifier, otherwise it may result in page->_mapcount < 0 false
 * positives.
 */
static inline int PageTransCompoundMap(struct page *page)
{
	return PageTransCompound(page) && atomic_read(&page->_mapcount) < 0;
}

/*
 * PageTransTail returns true for both transparent huge pages
 * and hugetlbfs pages, so it should only be called when it's known
 * that hugetlbfs pages aren't involved.
 */
static inline int PageTransTail(struct page *page)
{
	return PageTail(page);
}

/*
 * PageDoubleMap indicates that the compound page is mapped with PTEs as well
 * as PMDs.
 *
 * This is required for optimization of rmap operations for THP: we can postpone
 * per small page mapcount accounting (and its overhead from atomic operations)
 * until the first PMD split.
 *
 * For the page PageDoubleMap means ->_mapcount in all sub-pages is offset up
 * by one. This reference will go away with last compound_mapcount.
 *
 * See also __split_huge_pmd_locked() and page_remove_anon_compound_rmap().
 */
static inline int PageDoubleMap(struct page *page)
{
	return PageHead(page) && test_bit(PG_double_map, &page[1].flags);
}

static inline void SetPageDoubleMap(struct page *page)
{
	VM_BUG_ON_PAGE(!PageHead(page), page);
	set_bit(PG_double_map, &page[1].flags);
}

static inline void ClearPageDoubleMap(struct page *page)
{
	VM_BUG_ON_PAGE(!PageHead(page), page);
	clear_bit(PG_double_map, &page[1].flags);
}
static inline int TestSetPageDoubleMap(struct page *page)
{
	VM_BUG_ON_PAGE(!PageHead(page), page);
	return test_and_set_bit(PG_double_map, &page[1].flags);
}

static inline int TestClearPageDoubleMap(struct page *page)
{
	VM_BUG_ON_PAGE(!PageHead(page), page);
	return test_and_clear_bit(PG_double_map, &page[1].flags);
}

#else
TESTPAGEFLAG_FALSE(TransHuge)
TESTPAGEFLAG_FALSE(TransCompound)
TESTPAGEFLAG_FALSE(TransCompoundMap)
TESTPAGEFLAG_FALSE(TransTail)
PAGEFLAG_FALSE(DoubleMap)
	TESTSETFLAG_FALSE(DoubleMap)
	TESTCLEARFLAG_FALSE(DoubleMap)
#endif

/*
 * For pages that are never mapped to userspace, page->mapcount may be
 * used for storing extra information about page type. Any value used
 * for this purpose must be <= -2, but it's better start not too close
 * to -2 so that an underflow of the page_mapcount() won't be mistaken
 * for a special page.
 */
#define PAGE_MAPCOUNT_OPS(uname, lname)					\
static __always_inline int Page##uname(struct page *page)		\
{									\
	return atomic_read(&page->_mapcount) ==				\
				PAGE_##lname##_MAPCOUNT_VALUE;		\
}									\
static __always_inline void __SetPage##uname(struct page *page)		\
{									\
	VM_BUG_ON_PAGE(atomic_read(&page->_mapcount) != -1, page);	\
	atomic_set(&page->_mapcount, PAGE_##lname##_MAPCOUNT_VALUE);	\
}									\
static __always_inline void __ClearPage##uname(struct page *page)	\
{									\
	VM_BUG_ON_PAGE(!Page##uname(page), page);			\
	atomic_set(&page->_mapcount, -1);				\
}

/*
 * PageBuddy() indicate that the page is free and in the buddy system
 * (see mm/page_alloc.c).
 */
#define PAGE_BUDDY_MAPCOUNT_VALUE		(-128)
PAGE_MAPCOUNT_OPS(Buddy, BUDDY)

/*
 * PageBalloon() is set on pages that are on the balloon page list
 * (see mm/balloon_compaction.c).
 */
#define PAGE_BALLOON_MAPCOUNT_VALUE		(-256)
PAGE_MAPCOUNT_OPS(Balloon, BALLOON)

/*
 * If kmemcg is enabled, the buddy allocator will set PageKmemcg() on
 * pages allocated with __GFP_ACCOUNT. It gets cleared on page free.
 */
#define PAGE_KMEMCG_MAPCOUNT_VALUE		(-512)
PAGE_MAPCOUNT_OPS(Kmemcg, KMEMCG)

extern bool is_free_buddy_page(struct page *page);

__PAGEFLAG(Isolated, isolated, PF_ANY);

/*
 * If network-based swap is enabled, sl*b must keep track of whether pages
 * were allocated from pfmemalloc reserves.
 */
static inline int PageSlabPfmemalloc(struct page *page)
{
	VM_BUG_ON_PAGE(!PageSlab(page), page);
	return PageActive(page);
}

static inline void SetPageSlabPfmemalloc(struct page *page)
{
	VM_BUG_ON_PAGE(!PageSlab(page), page);
	SetPageActive(page);
}

static inline void __ClearPageSlabPfmemalloc(struct page *page)
{
	VM_BUG_ON_PAGE(!PageSlab(page), page);
	__ClearPageActive(page);
}

static inline void ClearPageSlabPfmemalloc(struct page *page)
{
	VM_BUG_ON_PAGE(!PageSlab(page), page);
	ClearPageActive(page);
}

#ifdef CONFIG_MMU
#define __PG_MLOCKED		(1UL << PG_mlocked)
#else
#define __PG_MLOCKED		0
#endif

/*
 * Flags checked when a page is freed.  Pages being freed should not have
 * these flags set.  It they are, there is a problem.
 */
#define PAGE_FLAGS_CHECK_AT_FREE \
	(1UL << PG_lru	 | 1UL << PG_locked    | \
	 1UL << PG_private | 1UL << PG_private_2 | \
	 1UL << PG_writeback | 1UL << PG_reserved | \
	 1UL << PG_slab	 | 1UL << PG_swapcache | 1UL << PG_active | \
	 1UL << PG_unevictable | __PG_MLOCKED)

/*
 * Flags checked when a page is prepped for return by the page allocator.
 * Pages being prepped should not have these flags set.  It they are set,
 * there has been a kernel bug or struct page corruption.
 *
 * __PG_HWPOISON is exceptional because it needs to be kept beyond page's
 * alloc-free cycle to prevent from reusing the page.
 */
#define PAGE_FLAGS_CHECK_AT_PREP	\
	(((1UL << NR_PAGEFLAGS) - 1) & ~__PG_HWPOISON)

#define PAGE_FLAGS_PRIVATE				\
	(1UL << PG_private | 1UL << PG_private_2)
/**
 * page_has_private - Determine if page has private stuff
 * @page: The page to be checked
 *
 * Determine if a page has private stuff, indicating that release routines
 * should be invoked upon it.
 */
static inline int page_has_private(struct page *page)
{
	return !!(page->flags & PAGE_FLAGS_PRIVATE);
}

#undef PF_ANY
#undef PF_HEAD
#undef PF_NO_TAIL
#undef PF_NO_COMPOUND
#endif /* !__GENERATING_BOUNDS_H */

#endif	/* PAGE_FLAGS_H */

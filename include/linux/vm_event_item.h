#ifndef VM_EVENT_ITEM_H_INCLUDED
#define VM_EVENT_ITEM_H_INCLUDED

#ifdef CONFIG_ZONE_DMA
#define DMA_ZONE(xx) xx##_DMA,
#else
#define DMA_ZONE(xx)
#endif

#ifdef CONFIG_ZONE_DMA32
#define DMA32_ZONE(xx) xx##_DMA32,
#else
#define DMA32_ZONE(xx)
#endif

#ifdef CONFIG_HIGHMEM
#define HIGHMEM_ZONE(xx) xx##_HIGH,
#else
#define HIGHMEM_ZONE(xx)
#endif

#define FOR_ALL_ZONES(xx) DMA_ZONE(xx) DMA32_ZONE(xx) xx##_NORMAL, HIGHMEM_ZONE(xx) xx##_MOVABLE

#if 0
enum vm_event_item { PGPGIN, PGPGOUT, PSWPIN, PSWPOUT,
		FOR_ALL_ZONES(PGALLOC),
		FOR_ALL_ZONES(ALLOCSTALL),
		FOR_ALL_ZONES(PGSCAN_SKIP),
		PGFREE, PGACTIVATE, PGDEACTIVATE,
		PGFAULT, PGMAJFAULT,
		PGLAZYFREED,
		PGREFILL,
		PGSTEAL_KSWAPD,
		PGSTEAL_DIRECT,
		PGSCAN_KSWAPD,
		PGSCAN_DIRECT,
		PGSCAN_DIRECT_THROTTLE,
#ifdef CONFIG_NUMA
		PGSCAN_ZONE_RECLAIM_FAILED,
#endif
		PGINODESTEAL, SLABS_SCANNED, KSWAPD_INODESTEAL,
		KSWAPD_LOW_WMARK_HIT_QUICKLY, KSWAPD_HIGH_WMARK_HIT_QUICKLY,
		PAGEOUTRUN, PGROTATED,
		DROP_PAGECACHE, DROP_SLAB,
#ifdef CONFIG_NUMA_BALANCING
		NUMA_PTE_UPDATES,
		NUMA_HUGE_PTE_UPDATES,
		NUMA_HINT_FAULTS,
		NUMA_HINT_FAULTS_LOCAL,
		NUMA_PAGE_MIGRATE,
#endif
#ifdef CONFIG_MIGRATION
		PGMIGRATE_SUCCESS, PGMIGRATE_FAIL,
#endif
#ifdef CONFIG_COMPACTION
		COMPACTMIGRATE_SCANNED, COMPACTFREE_SCANNED,
		COMPACTISOLATED,
		COMPACTSTALL, COMPACTFAIL, COMPACTSUCCESS,
		KCOMPACTD_WAKE,
#endif
#ifdef CONFIG_HUGETLB_PAGE
		HTLB_BUDDY_PGALLOC, HTLB_BUDDY_PGALLOC_FAIL,
#endif
		UNEVICTABLE_PGCULLED,	/* culled to noreclaim list */
		UNEVICTABLE_PGSCANNED,	/* scanned for reclaimability */
		UNEVICTABLE_PGRESCUED,	/* rescued from noreclaim list */
		UNEVICTABLE_PGMLOCKED,
		UNEVICTABLE_PGMUNLOCKED,
		UNEVICTABLE_PGCLEARED,	/* on COW, page truncate */
		UNEVICTABLE_PGSTRANDED,	/* unable to isolate on unlock */
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
		THP_FAULT_ALLOC,
		THP_FAULT_FALLBACK,
		THP_COLLAPSE_ALLOC,
		THP_COLLAPSE_ALLOC_FAILED,
		THP_FILE_ALLOC,
		THP_FILE_MAPPED,
		THP_SPLIT_PAGE,
		THP_SPLIT_PAGE_FAILED,
		THP_DEFERRED_SPLIT_PAGE,
		THP_SPLIT_PMD,
		THP_ZERO_PAGE_ALLOC,
		THP_ZERO_PAGE_ALLOC_FAILED,
#endif
#ifdef CONFIG_MEMORY_BALLOON
		BALLOON_INFLATE,
		BALLOON_DEFLATE,
#ifdef CONFIG_BALLOON_COMPACTION
		BALLOON_MIGRATE,
#endif
#endif
#ifdef CONFIG_DEBUG_TLBFLUSH
#ifdef CONFIG_SMP
		NR_TLB_REMOTE_FLUSH,	/* cpu tried to flush others' tlbs */
		NR_TLB_REMOTE_FLUSH_RECEIVED,/* cpu received ipi for flush */
#endif /* CONFIG_SMP */
		NR_TLB_LOCAL_FLUSH_ALL,
		NR_TLB_LOCAL_FLUSH_ONE,
#endif /* CONFIG_DEBUG_TLBFLUSH */
#ifdef CONFIG_DEBUG_VM_VMACACHE
		VMACACHE_FIND_CALLS,
		VMACACHE_FIND_HITS,
		VMACACHE_FULL_FLUSHES,
#endif
		NR_VM_EVENT_ITEMS
};
#else
enum vm_event_item {
  PGPGIN, PGPGOUT, PSWPIN, PSWPOUT,
  /*
  下面的值会在内存被分配是增加，增加的是哪一个与具体zone有关，见 buffered_rmqueue
  后面还有一个PGFREE的统计，因为buddy的内存是通过memblock系统free进来的，所以PGFREE
  的值要大于PGALLOC的值，猜测PGFREE-PGALLOC=free的页面数，实际情况如下：
	lc1861evb_arm64:/proc # cat vmstat
	nr_free_pages 374215
	pgalloc_dma 1877642
	pgalloc_normal 0
	pgalloc_movable 0
	pgfree 2253016
	2253016-1877642=375374,跟实际的freepage比多了1000个page左右，哪里去了？
*/
  PGALLOC_DMA, PGALLOC_DMA32, PGALLOC_NORMAL, PGALLOC_MOVABLE,
  ALLOCSTALL_DMA, ALLOCSTALL_DMA32, ALLOCSTALL_NORMAL, ALLOCSTALL_MOVABLE,
  PGSCAN_SKIP_DMA, PGSCAN_SKIP_DMA32, PGSCAN_SKIP_NORMAL, PGSCAN_SKIP_MOVABLE,
  //free的page的个数，见__free_pages_ok ， free_hot_cold_page
  PGFREE, 
  PGACTIVATE, PGDEACTIVATE,
  //系统出现pagefault的次数，在 handle_mm_fault 中加一
  PGFAULT, 
  //文件为背景的pagefault，在文件cache中没有查到对应的page，需要申请page，然后
  //读入文件的fault
  //对于写入文件的操作，并不更新此值，只是对读入的page才更新此值，可以理解为此值
  //是发生pagefault之后需要从文件系统读入的次数（包括普通文件和swap文件）
  PGMAJFAULT,
  PGLAZYFREED,
  //在内存回收时如果inactive的lru数量太少的话，会从active
  //lru的链表中取一些过来，在这个过程中需要扫描active链表，这个
  //值保存的是扫描active链表的页的数量。 见 shrink_active_list
  PGREFILL,
  PGSTEAL_KSWAPD,
  PGSTEAL_DIRECT,
  PGSCAN_KSWAPD,
  PGSCAN_DIRECT,
  PGSCAN_DIRECT_THROTTLE,

  PGSCAN_ZONE_RECLAIM_FAILED,

  PGINODESTEAL, SLABS_SCANNED, KSWAPD_INODESTEAL,
  //表示kswapd刚睡眠0.1s不到就被唤醒
  KSWAPD_LOW_WMARK_HIT_QUICKLY, 
  //表示kswapd睡眠了0.1s，没有被其他进程唤醒，但是重新检查内存时已
  //不满足睡眠要求。
  KSWAPD_HIGH_WMARK_HIT_QUICKLY,
  //调用 balance_pgdat 时会增加，统计的应该是内存回收运行的次数
  PAGEOUTRUN, 
  PGROTATED,
  DROP_PAGECACHE, DROP_SLAB,

  PGMIGRATE_SUCCESS, PGMIGRATE_FAIL,


  COMPACTMIGRATE_SCANNED, COMPACTFREE_SCANNED,
  COMPACTISOLATED,
  COMPACTSTALL, COMPACTFAIL, COMPACTSUCCESS,
  KCOMPACTD_WAKE,
  KCOMPACTD_MIGRATE_SCANNED, KCOMPACTD_FREE_SCANNED,


  HTLB_BUDDY_PGALLOC, HTLB_BUDDY_PGALLOC_FAIL,

  UNEVICTABLE_PGCULLED,
  UNEVICTABLE_PGSCANNED,
  UNEVICTABLE_PGRESCUED,
  UNEVICTABLE_PGMLOCKED,
  UNEVICTABLE_PGMUNLOCKED,
  UNEVICTABLE_PGCLEARED,
  UNEVICTABLE_PGSTRANDED,

  NR_VM_EVENT_ITEMS
};

#endif

#ifndef CONFIG_TRANSPARENT_HUGEPAGE
#define THP_FILE_ALLOC ({ BUILD_BUG(); 0; })
#define THP_FILE_MAPPED ({ BUILD_BUG(); 0; })
#endif

#endif		/* VM_EVENT_ITEM_H_INCLUDED */

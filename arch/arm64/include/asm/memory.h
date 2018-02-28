/*
 * Based on arch/arm/include/asm/memory.h
 *
 * Copyright (C) 2000-2002 Russell King
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Note: this file should not be included by non-asm/.h files
 */
#ifndef __ASM_MEMORY_H
#define __ASM_MEMORY_H

#include <linux/compiler.h>
#include <linux/const.h>
#include <linux/types.h>
#include <asm/bug.h>
#include <asm/sizes.h>

/*
 * Allow for constants defined here to be used from assembly code
 * by prepending the UL suffix only with actual C code compilation.
 */
#define UL(x) _AC(x, UL)

/*
 * Size of the PCI I/O space. This must remain a power of two so that
 * IO_SPACE_LIMIT acts as a mask for the low bits of I/O addresses.
 */
#define PCI_IO_SIZE		SZ_16M

/*
 * Log2 of the upper bound of the size of a struct page. Used for sizing
 * the vmemmap region only, does not affect actual memory footprint.
 * We don't use sizeof(struct page) directly since taking its size here
 * requires its definition to be available at this point in the inclusion
 * chain, and it may not be a power of 2 in the first place.
 */
#define STRUCT_PAGE_MAX_SHIFT	6

/*
 * VMEMMAP_SIZE - allows the whole linear region to be covered by
 *                a struct page array
 */
#define VMEMMAP_SIZE (UL(1) << (VA_BITS - PAGE_SHIFT - 1 + STRUCT_PAGE_MAX_SHIFT))

/*
 * PAGE_OFFSET - the virtual address of the start of the linear map (top
 *		 (VA_BITS - 1))
 * KIMAGE_VADDR - the virtual address of the start of the kernel image
 * VA_BITS - the maximum number of bits for virtual addresses.
 * VA_START - the first kernel virtual address.
 * TASK_SIZE - the maximum size of a user space task.
 * TASK_UNMAPPED_BASE - the lower boundary of the mmap VM area.
 */
//这个在.config中配置，在lc1861中配置是39
#define VA_BITS			(CONFIG_ARM64_VA_BITS)

//	内核地址空间的起始地址
#define VA_START		(UL(0xffffffffffffffff) << VA_BITS)

/*
VA_BITS定义了虚拟地址空间的bit数（该值也就是定义了用户态程序或者内核能够访问的虚拟地址空间
的size），假设VA_BITS被设定为39个bit，那么PAGE_OFFSET就是0xffffffc0-00000000。PAGE_OFFSET的
名字也不好（个人观点，可能有误），OFFSET表明的是一个偏移，内核空间被划分成一个个的page，
PAGE_OFFSET看起来应该是定义以page为单位的偏移。但是，以什么为基准的偏移呢？PAGE_OFFSET的名字
中没有给出，当然实际上，这个符号是定义以整个address space的起始地址（也就是0）为基准。另外，
虽然这个地址是要求page对齐，但是实际上，这个符号仍然定义的是虚拟地址的offset（而不是page的offset）。
根据上面的理由，我觉得定义成KERNEL_IMG_OFFSET会更好理解一些。一句话总结：PAGE_OFFSET定义了将kernel 
image安放在虚拟地址空间的哪个位置上。
*/
//kernel的空间实际上也是分成连部分，低地址（VA_START-PAGE_OFFSET）是给动态分配用的，高地址(>PAGE_OFFSET)
//是线性映射用的。
#define PAGE_OFFSET		(UL(0xffffffffffffffff) << (VA_BITS - 1))
#define KIMAGE_VADDR		(MODULES_END)
#define MODULES_END		(MODULES_VADDR + MODULES_VSIZE)
#define MODULES_VADDR		(VA_START + KASAN_SHADOW_SIZE)
#define MODULES_VSIZE		(SZ_128M)
#define VMEMMAP_START		(PAGE_OFFSET - VMEMMAP_SIZE)
#define PCI_IO_END		(VMEMMAP_START - SZ_2M)
#define PCI_IO_START		(PCI_IO_END - PCI_IO_SIZE)
#define FIXADDR_TOP		(PCI_IO_START - SZ_2M)
#define TASK_SIZE_64		(UL(1) << VA_BITS)

//	一般而言，用户地址空间从0开始，大小就是TASK_SIZE，因此，
//这个宏定义的全称应该是task userspace size。对于ARM64的用
//户空间进程而言，有两种，一种是运行在AArch64状态下，另外一
//种是运行在AArch32状态，因此，实际上代码中又定义了
//TASK_SIZE_32和TASK_SIZE_64两个宏定义。

#ifdef CONFIG_COMPAT
#define TASK_SIZE_32		UL(0x100000000)
#define TASK_SIZE		(test_thread_flag(TIF_32BIT) ? \
				TASK_SIZE_32 : TASK_SIZE_64)
#define TASK_SIZE_OF(tsk)	(test_tsk_thread_flag(tsk, TIF_32BIT) ? \
				TASK_SIZE_32 : TASK_SIZE_64)
#else
#define TASK_SIZE		TASK_SIZE_64
#endif /* CONFIG_COMPAT */

#define TASK_UNMAPPED_BASE	(PAGE_ALIGN(TASK_SIZE / 4))
/*
KERNEL_START是kernel开始运行的虚拟地址，更确切的说是内核正文段开始的虚拟地址。 
在链接脚本文件中（参考arch/arm64/kernel下的vmlinux.lds.S）
*/
#define KERNEL_START      _text
#define KERNEL_END        _end

/*
 * The size of the KASAN shadow region. This should be 1/8th of the
 * size of the entire kernel virtual address space.
 */
#ifdef CONFIG_KASAN
#define KASAN_SHADOW_SIZE	(UL(1) << (VA_BITS - 3))
#else
#define KASAN_SHADOW_SIZE	(0)
#endif

/*
 * Physical vs virtual RAM address space conversion.  These are
 * private definitions which should NOT be used outside memory.h
 * files.  Use virt_to_phys/phys_to_virt/__pa/__va instead.
 */
#define __virt_to_phys(x) ({						\
	phys_addr_t __x = (phys_addr_t)(x);				\
	__x & BIT(VA_BITS - 1) ? (__x & ~PAGE_OFFSET) + PHYS_OFFSET :	\
				 (__x - kimage_voffset); })

#define __phys_to_virt(x)	((unsigned long)((x) - PHYS_OFFSET) | PAGE_OFFSET)
#define __phys_to_kimg(x)	((unsigned long)((x) + kimage_voffset))

/*
 * Convert a page to/from a physical address
 */
#define page_to_phys(page)	(__pfn_to_phys(page_to_pfn(page)))
#define phys_to_page(phys)	(pfn_to_page(__phys_to_pfn(phys)))

/*
 * Memory types available.
 */
#define MT_DEVICE_nGnRnE	0
#define MT_DEVICE_nGnRE		1
#define MT_DEVICE_GRE		2
#define MT_NORMAL_NC		3
#define MT_NORMAL		4
#define MT_NORMAL_WT		5

/*
 * Memory types for Stage-2 translation
 */
#define MT_S2_NORMAL		0xf
#define MT_S2_DEVICE_nGnRE	0x1

#ifdef CONFIG_ARM64_4K_PAGES
#define IOREMAP_MAX_ORDER	(PUD_SHIFT)
#else
#define IOREMAP_MAX_ORDER	(PMD_SHIFT)
#endif

#ifdef CONFIG_BLK_DEV_INITRD
#define __early_init_dt_declare_initrd(__start, __end)			\
	do {								\
		initrd_start = (__start);				\
		initrd_end = (__end);					\
	} while (0)
#endif

#ifndef __ASSEMBLY__

#include <linux/bitops.h>
#include <linux/mmdebug.h>
//	系统内存的起始物理地址。在系统初始化的过程中，
//会把PHYS_OFFSET开始的物理内存映射到PAGE_OFFSET的虚拟内存上去。
extern s64			memstart_addr;
/* PHYS_OFFSET - the physical address of the start of memory. */
#define PHYS_OFFSET		({ VM_BUG_ON(memstart_addr & 1); memstart_addr; })

/* the virtual base of the kernel image (minus TEXT_OFFSET) */
extern u64			kimage_vaddr;

/* the offset between the kernel virtual and physical mappings */
extern u64			kimage_voffset;

/*
 * Allow all memory at the discovery stage. We will clip it later.
 */
#define MIN_MEMBLOCK_ADDR	0
#define MAX_MEMBLOCK_ADDR	U64_MAX

/*
 * PFNs are used to describe any physical page; this means
 * PFN 0 == physical address 0.
 *
 * This is the PFN of the first RAM page in the kernel
 * direct-mapped view.  We assume this is the first page
 * of RAM in the mem_map as well.
 */
#define PHYS_PFN_OFFSET	(PHYS_OFFSET >> PAGE_SHIFT)

/*
 * Note: Drivers should NOT use these.  They are the wrong
 * translation for translating DMA addresses.  Use the driver
 * DMA support - see dma-mapping.h.
 */
#define virt_to_phys virt_to_phys
static inline phys_addr_t virt_to_phys(const volatile void *x)
{
	return __virt_to_phys((unsigned long)(x));
}

#define phys_to_virt phys_to_virt
static inline void *phys_to_virt(phys_addr_t x)
{
	return (void *)(__phys_to_virt(x));
}

/*
 * Drivers should NOT use these either.
 */
#define __pa(x)			__virt_to_phys((unsigned long)(x))
#define __va(x)			((void *)__phys_to_virt((phys_addr_t)(x)))
#define pfn_to_kaddr(pfn)	__va((pfn) << PAGE_SHIFT)
#define virt_to_pfn(x)      __phys_to_pfn(__virt_to_phys(x))

/*
 *  virt_to_page(k)	convert a _valid_ virtual address to struct page *
 *  virt_addr_valid(k)	indicates whether a virtual address is valid
 */
#define ARCH_PFN_OFFSET		((unsigned long)PHYS_PFN_OFFSET)

#ifndef CONFIG_SPARSEMEM_VMEMMAP
#define virt_to_page(kaddr)	pfn_to_page(__pa(kaddr) >> PAGE_SHIFT)
#define virt_addr_valid(kaddr)	pfn_valid(__pa(kaddr) >> PAGE_SHIFT)
#else
#define __virt_to_pgoff(kaddr)	(((u64)(kaddr) & ~PAGE_OFFSET) / PAGE_SIZE * sizeof(struct page))
#define __page_to_voff(kaddr)	(((u64)(page) & ~VMEMMAP_START) * PAGE_SIZE / sizeof(struct page))

#define page_to_virt(page)	((void *)((__page_to_voff(page)) | PAGE_OFFSET))
#define virt_to_page(vaddr)	((struct page *)((__virt_to_pgoff(vaddr)) | VMEMMAP_START))

#define virt_addr_valid(kaddr)	pfn_valid((((u64)(kaddr) & ~PAGE_OFFSET) \
					   + PHYS_OFFSET) >> PAGE_SHIFT)
#endif
#endif

#include <asm-generic/memory_model.h>

#endif

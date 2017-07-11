/*
 * Based on arch/arm/include/asm/uaccess.h
 *
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
 */
#ifndef __ASM_UACCESS_H
#define __ASM_UACCESS_H

/*
 * User space memory access functions
 */
#include <linux/bitops.h>
#include <linux/kasan-checks.h>
#include <linux/string.h>
#include <linux/thread_info.h>

#include <asm/alternative.h>
#include <asm/cpufeature.h>
#include <asm/ptrace.h>
#include <asm/sysreg.h>
#include <asm/errno.h>
#include <asm/memory.h>
#include <asm/compiler.h>

#define VERIFY_READ 0
#define VERIFY_WRITE 1

/*
 * The exception table consists of pairs of relative offsets: the first
 * is the relative offset to an instruction that is allowed to fault,
 * and the second is the relative offset at which the program should
 * continue. No registers are modified, so it is entirely up to the
 * continuation code to figure out what to do.
 *
 * All the routines below use bits of fixup code that are out of line
 * with the main instruction path.  This means when everything is well,
 * we don't even have to jump over them.  Further, they do not intrude
 * on our cache or tlb entries.
 */

struct exception_table_entry
{
	int insn, fixup;
};

#define ARCH_HAS_RELATIVE_EXTABLE

extern int fixup_exception(struct pt_regs *regs);

#define KERNEL_DS	(-1UL)
#define get_ds()	(KERNEL_DS)

#define USER_DS		TASK_SIZE_64
#define get_fs()	(current_thread_info()->addr_limit)

static inline void set_fs(mm_segment_t fs)
{
	current_thread_info()->addr_limit = fs;

	/*
	 * Enable/disable UAO so that copy_to_user() etc can access
	 * kernel memory with the unprivileged instructions.
	 */
	if (IS_ENABLED(CONFIG_ARM64_UAO) && fs == KERNEL_DS)
		asm(ALTERNATIVE("nop", SET_PSTATE_UAO(1), ARM64_HAS_UAO));
	else
		asm(ALTERNATIVE("nop", SET_PSTATE_UAO(0), ARM64_HAS_UAO,
				CONFIG_ARM64_UAO));
}

#define segment_eq(a, b)	((a) == (b))

/*
 * Test whether a block of memory is a valid user space address.
 * Returns 1 if the range is valid, 0 otherwise.
 *
 * This is equivalent to the following test:
 * (u65)addr + (u65)size <= current->addr_limit
 *
 * This needs 65-bit arithmetic.
 */
#define __range_ok(addr, size)						\
({									\
	unsigned long flag, roksum;					\
	__chk_user_ptr(addr);						\
	asm("adds %1, %1, %3; ccmp %1, %4, #2, cc; cset %0, ls"		\
		: "=&r" (flag), "=&r" (roksum)				\
		: "1" (addr), "Ir" (size),				\
		  "r" (current_thread_info()->addr_limit)		\
		: "cc");						\
	flag;								\
})

/*
 * When dealing with data aborts or instruction traps we may end up with
 * a tagged userland pointer. Clear the tag to get a sane pointer to pass
 * on to access_ok(), for instance.
 */
#define untagged_addr(addr)		sign_extend64(addr, 55)

#define access_ok(type, addr, size)	__range_ok(addr, size)
#define user_addr_max			get_fs

#define _ASM_EXTABLE(from, to)						\
	"	.pushsection	__ex_table, \"a\"\n"			\
	"	.align		3\n"					\
	"	.long		(" #from " - .), (" #to " - .)\n"	\
	"	.popsection\n"

/*
 * The "__xxx" versions of the user access functions do not verify the address
 * space - it must have been done previously with a separate "access_ok()"
 * call.
 *
 * The "__xxx_error" versions set the third argument to -EFAULT if an error
 * occurs, and leave it unchanged on success.
 */
#define __get_user_asm(instr, alt_instr, reg, x, addr, err, feature)	\
	asm volatile(							\
	"1:"ALTERNATIVE(instr "     " reg "1, [%2]\n",			\
			alt_instr " " reg "1, [%2]\n", feature)		\
	"2:\n"								\
	"	.section .fixup, \"ax\"\n"				\
	"	.align	2\n"						\
	"3:	mov	%w0, %3\n"					\
	"	mov	%1, #0\n"					\
	"	b	2b\n"						\
	"	.previous\n"						\
	_ASM_EXTABLE(1b, 3b)						\
	: "+r" (err), "=&r" (x)						\
	: "r" (addr), "i" (-EFAULT))

#define __get_user_err(x, ptr, err)					\
do {									\
	unsigned long __gu_val;						\
	__chk_user_ptr(ptr);						\
	asm(ALTERNATIVE("nop", SET_PSTATE_PAN(0), ARM64_ALT_PAN_NOT_UAO,\
			CONFIG_ARM64_PAN));				\
	switch (sizeof(*(ptr))) {					\
	case 1:								\
		__get_user_asm("ldrb", "ldtrb", "%w", __gu_val, (ptr),  \
			       (err), ARM64_HAS_UAO);			\
		break;							\
	case 2:								\
		__get_user_asm("ldrh", "ldtrh", "%w", __gu_val, (ptr),  \
			       (err), ARM64_HAS_UAO);			\
		break;							\
	case 4:								\
		__get_user_asm("ldr", "ldtr", "%w", __gu_val, (ptr),	\
			       (err), ARM64_HAS_UAO);			\
		break;							\
	case 8:								\
		__get_user_asm("ldr", "ldtr", "%",  __gu_val, (ptr),	\
			       (err), ARM64_HAS_UAO);			\
		break;							\
	default:							\
		BUILD_BUG();						\
	}								\
	(x) = (__force __typeof__(*(ptr)))__gu_val;			\
	asm(ALTERNATIVE("nop", SET_PSTATE_PAN(1), ARM64_ALT_PAN_NOT_UAO,\
			CONFIG_ARM64_PAN));				\
} while (0)

#define __get_user(x, ptr)						\
({									\
	int __gu_err = 0;						\
	__get_user_err((x), (ptr), __gu_err);				\
	__gu_err;							\
})

#define __get_user_error(x, ptr, err)					\
({									\
	__get_user_err((x), (ptr), (err));				\
	(void)0;							\
})

#define __get_user_unaligned __get_user

#define get_user(x, ptr)						\
({									\
	__typeof__(*(ptr)) __user *__p = (ptr);				\
	might_fault();							\
	access_ok(VERIFY_READ, __p, sizeof(*__p)) ?			\
		__get_user((x), __p) :					\
		((x) = 0, -EFAULT);					\
})

#define __put_user_asm(instr, alt_instr, reg, x, addr, err, feature)	\
	asm volatile(							\
	"1:"ALTERNATIVE(instr "     " reg "1, [%2]\n",			\
			alt_instr " " reg "1, [%2]\n", feature)		\
	"2:\n"								\
	"	.section .fixup,\"ax\"\n"				\
	"	.align	2\n"						\
	"3:	mov	%w0, %3\n"					\
	"	b	2b\n"						\
	"	.previous\n"						\
	_ASM_EXTABLE(1b, 3b)						\
	: "+r" (err)							\
	: "r" (x), "r" (addr), "i" (-EFAULT))

#define __put_user_err(x, ptr, err)					\
do {									\
	__typeof__(*(ptr)) __pu_val = (x);				\
	__chk_user_ptr(ptr);						\
	asm(ALTERNATIVE("nop", SET_PSTATE_PAN(0), ARM64_ALT_PAN_NOT_UAO,\
			CONFIG_ARM64_PAN));				\
	switch (sizeof(*(ptr))) {					\
	case 1:								\
		__put_user_asm("strb", "sttrb", "%w", __pu_val, (ptr),	\
			       (err), ARM64_HAS_UAO);			\
		break;							\
	case 2:								\
		__put_user_asm("strh", "sttrh", "%w", __pu_val, (ptr),	\
			       (err), ARM64_HAS_UAO);			\
		break;							\
	case 4:								\
		__put_user_asm("str", "sttr", "%w", __pu_val, (ptr),	\
			       (err), ARM64_HAS_UAO);			\
		break;							\
	case 8:								\
		__put_user_asm("str", "sttr", "%", __pu_val, (ptr),	\
			       (err), ARM64_HAS_UAO);			\
		break;							\
	default:							\
		BUILD_BUG();						\
	}								\
	asm(ALTERNATIVE("nop", SET_PSTATE_PAN(1), ARM64_ALT_PAN_NOT_UAO,\
			CONFIG_ARM64_PAN));				\
} while (0)

#define __put_user(x, ptr)						\
({									\
	int __pu_err = 0;						\
	__put_user_err((x), (ptr), __pu_err);				\
	__pu_err;							\
})

#define __put_user_error(x, ptr, err)					\
({									\
	__put_user_err((x), (ptr), (err));				\
	(void)0;							\
})

#define __put_user_unaligned __put_user

#define put_user(x, ptr)						\
({									\
	__typeof__(*(ptr)) __user *__p = (ptr);				\
	might_fault();							\
	access_ok(VERIFY_WRITE, __p, sizeof(*__p)) ?			\
		__put_user((x), __p) :					\
		-EFAULT;						\
})



//见<<Linux内核情景分析>>3.8系统调用，第253页
/****************************************************************************
目前对copy_from_user和copy_to_user的理解
首先，kernel中是能直接访问user的数据的，就是说如果我们不调用copy_xx_user而是直接
使用用户传过来的指针在代码没有bug的情况下，也是可以正常工作的。
之所以有copy_xx_user这两个函数是因为，如果我们直接使用用户空间的指针，当发生缺页
异常时将无法回到正常的流程中。
copy_xx_user能回到正常的流程的原因是将访问用户空间指针的地方添加到了异常的列表中，
并设置异常返回的地址，这样当传入的用户空间的指针有异常时，将cp的地址跳到异常列表中
的地址上，然后返回失败。
见:extable.c文件和<<Linux内核情景分析>>3.8系统调用，第253页

以下来自www
在深入讲解之前，我们先想一个问题：为什么要使用copy_from_user函数？？？
理论上，内核空间可以直接使用用户空间传过来的指针，即使要做数据拷贝的动作，
也可以直接使用memcpy，事实上，在没有MMU的体系架构上，copy_form_user最终的
实现就是利用了memcpy。但对于大多数有MMU的平台，情况就有了一些变化：用户空
间传过来的指针是在虚拟地址空间上的，它指向的虚拟地址空间很可能还没有真正映
射到实际的物理页面上。但这又能怎样呢？缺页导致的异常会透明的被内核予以修复
(为缺页的地址空间提交新的物理页面)，访问到缺页的指令会继续运行仿佛什么都没
有发生一样。但这只是用户空间缺页异常的行为，在内核空间这样却因一场必须被显
示的修复，这是由内核提供的缺页异常处理函数的设计模式决定的，其背后的思想后：
在内核态中，如果程序试图访问一个尚未提交物理页面的用户空间地址，内核必须对
此保持警惕而不能像用户空间那样毫无察觉。
如果内核访问一个尚未被提交物理页面的空间，将产生缺页异常，内核会调用
do_page_fault，因为异常发生在内核空间，do_page_fault将调用
search_exception_tables在“ __ex_table”中查找异常指令的修复指令，
在__arch_copy_from_user函数中经常使用USER宏，这个宏中了定义了
“__ex_table”section。
*****************************************************************************/

extern unsigned long __must_check __arch_copy_from_user(void *to, const void __user *from, unsigned long n);
extern unsigned long __must_check __arch_copy_to_user(void __user *to, const void *from, unsigned long n);
extern unsigned long __must_check __copy_in_user(void __user *to, const void __user *from, unsigned long n);
extern unsigned long __must_check __clear_user(void __user *addr, unsigned long n);

static inline unsigned long __must_check __copy_from_user(void *to, const void __user *from, unsigned long n)
{
	kasan_check_write(to, n);
	check_object_size(to, n, false);
	return __arch_copy_from_user(to, from, n);
}

static inline unsigned long __must_check __copy_to_user(void __user *to, const void *from, unsigned long n)
{
	kasan_check_read(from, n);
	check_object_size(from, n, true);
	return __arch_copy_to_user(to, from, n);
}

static inline unsigned long __must_check copy_from_user(void *to, const void __user *from, unsigned long n)
{
	kasan_check_write(to, n);

	if (access_ok(VERIFY_READ, from, n)) {
		check_object_size(to, n, false);
		n = __arch_copy_from_user(to, from, n);
	} else /* security hole - plug it */
		memset(to, 0, n);
	return n;
}

static inline unsigned long __must_check copy_to_user(void __user *to, const void *from, unsigned long n)
{
	kasan_check_read(from, n);

	if (access_ok(VERIFY_WRITE, to, n)) {
		check_object_size(from, n, true);
		n = __arch_copy_to_user(to, from, n);
	}
	return n;
}

static inline unsigned long __must_check copy_in_user(void __user *to, const void __user *from, unsigned long n)
{
	if (access_ok(VERIFY_READ, from, n) && access_ok(VERIFY_WRITE, to, n))
		n = __copy_in_user(to, from, n);
	return n;
}

#define __copy_to_user_inatomic __copy_to_user
#define __copy_from_user_inatomic __copy_from_user

static inline unsigned long __must_check clear_user(void __user *to, unsigned long n)
{
	if (access_ok(VERIFY_WRITE, to, n))
		n = __clear_user(to, n);
	return n;
}

extern long strncpy_from_user(char *dest, const char __user *src, long count);

extern __must_check long strlen_user(const char __user *str);
extern __must_check long strnlen_user(const char __user *str, long n);

#endif /* __ASM_UACCESS_H */

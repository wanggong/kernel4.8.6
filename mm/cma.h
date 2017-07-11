#ifndef __MM_CMA_H__
#define __MM_CMA_H__
/******************************************************************************
cma��ʹ��ʱ�����ģ�
��ϵͳ����ʱ����cma_init_reserved_mem��һЩ������ҳ���migrate type���ΪCMA,
ͬʱ��ʼ��cma�����ݽṹ��������ʼ��fn��count����Щҳ��Ҳ�����Ǳ��ΪCMA,��ʵ
���ڻ�����ϵͳ�У���ϵͳ�ڴ治��ʱ���Է���ʹ�õ�(���ݷ����־ȷ���Ƿ��ܷ���)��
��ϵͳ��Ҫ����ҳ��ʱ���Ե���cma_alloc����cma�з���һЩ�����ڴ档
*******************************************************************************/
struct cma {
//��ʼҳ���pfn
	unsigned long   base_pfn;
//��ռ��ҳ�������    
	unsigned long   count;
//cma�е�ҳ�棬ÿ��bit��Ӧ��1<<order_per_bit��������ҳ�档
//��ʼ��ʱ��Щbit��Ϊ0��������cma_alloc�����ڴ�֮�󣬽������ȥ��
//��Щpage��Ӧ��bit����Ϊ1.
	unsigned long   *bitmap;
//ÿ��bitmap��bit��Ӧ������ҳ���order
	unsigned int order_per_bit; /* Order of pages represented by one bit */
	struct mutex    lock;
#ifdef CONFIG_CMA_DEBUGFS
	struct hlist_head mem_head;
	spinlock_t mem_head_lock;
#endif
};

extern struct cma cma_areas[MAX_CMA_AREAS];
extern unsigned cma_area_count;

//cma����bit number
static inline unsigned long cma_bitmap_maxno(struct cma *cma)
{
	return cma->count >> cma->order_per_bit;
}

#endif

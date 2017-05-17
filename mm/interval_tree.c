/*
 * mm/interval_tree.c - interval tree for mapping->i_mmap
 *
 * Copyright (C) 2012, Michel Lespinasse <walken@google.com>
 *
 * This file is released under the GPL v2.
 */

#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/rmap.h>
#include <linux/interval_tree_generic.h>

//��vmaӳ����ļ��Ŀ�ʼ��λ��
static inline unsigned long vma_start_pgoff(struct vm_area_struct *v)
{
	return v->vm_pgoff;
}

//��vmaӳ����ļ�������λ��
static inline unsigned long vma_last_pgoff(struct vm_area_struct *v)
{
	return v->vm_pgoff + ((v->vm_end - v->vm_start) >> PAGE_SHIFT) - 1;
}
#if 0
INTERVAL_TREE_DEFINE(struct vm_area_struct, shared.rb,
		     unsigned long, shared.rb_subtree_last,
		     vma_start_pgoff, vma_last_pgoff,, vma_interval_tree)

#else
//wgz �괦��֮������

//����node���������ӽڵ�����rb_subtree_last
static inline unsigned long vma_interval_tree_compute_subtree_last(struct vm_area_struct *node)
{
    unsigned long max = vma_last_pgoff(node), subtree_last;
    if (node->shared.rb.rb_left) {
        subtree_last = ({ const typeof(((struct vm_area_struct *)0)->shared.rb) *__mptr = (node->shared.rb.rb_left); (struct vm_area_struct *)((char *)__mptr - __builtin_offsetof(struct vm_area_struct, shared.rb));})->shared.rb_subtree_last;
        if (max < subtree_last) {
            max = subtree_last;
        }
    }
    if (node->shared.rb.rb_right) {
        subtree_last = ({ const typeof(((struct vm_area_struct *)0)->shared.rb) *__mptr = (node->shared.rb.rb_right); (struct vm_area_struct *)((char *)__mptr - __builtin_offsetof(struct vm_area_struct, shared.rb));})->shared.rb_subtree_last;
        if (max < subtree_last) {
            max = subtree_last;
        }
    }
    return max;
}

//stop ��rb��һ�����Ƚڵ㣬��rbһֱ����ֱ��stop����rb_subtree_last��ֵ
//��ǰnode��rb_subtree_last��ֵ��:�䱾����������ӽڵ�����ֵ
//Ҷ�ӽڵ�rb_subtree_last��ֵ��:vma_last_pgoff
static inline void vma_interval_tree_augment_propagate(struct rb_node *rb, struct rb_node *stop)
{
    while (rb != stop) {
        struct vm_area_struct *node = ( {
                                            const typeof(((struct vm_area_struct *)0)->shared.rb) *__mptr = (rb);
                                            (struct vm_area_struct *)((char *)__mptr - __builtin_offsetof(struct vm_area_struct, shared.rb));
                                        });
        unsigned long augmented = vma_interval_tree_compute_subtree_last(node);
        if (node->shared.rb_subtree_last == augmented) {
            break;
        }
        node->shared.rb_subtree_last = augmented;
        rb = ((struct rb_node *)((&node->shared.rb)->__rb_parent_color & ~3));
    }
}

//��ʵ��δ����ʲô�����������ǽ�old��rb_subtree_last��ֵ������new
static inline void vma_interval_tree_augment_copy(struct rb_node *rb_old, struct rb_node *rb_new)
{
    struct vm_area_struct *old = ( {
                                       const typeof(((struct vm_area_struct *)0)->shared.rb) *__mptr = (rb_old);
                                       (struct vm_area_struct *)((char *)__mptr - __builtin_offsetof(struct vm_area_struct, shared.rb));
                                   });
    struct vm_area_struct *new = ( {
                                       const typeof(((struct vm_area_struct *)0)->shared.rb) *__mptr = (rb_new);
                                       (struct vm_area_struct *)((char *)__mptr - __builtin_offsetof(struct vm_area_struct, shared.rb));
                                   });
    new->shared.rb_subtree_last = old -> shared.rb_subtree_last;
}

//���û����
static void vma_interval_tree_augment_rotate(struct rb_node *rb_old, struct rb_node *rb_new)
{
    struct vm_area_struct *old = ( {
                                       const typeof(((struct vm_area_struct *)0)->shared.rb) *__mptr = (rb_old);
                                       (struct vm_area_struct *)((char *)__mptr - __builtin_offsetof(struct vm_area_struct, shared.rb));
                                   });
    struct vm_area_struct *new = ( {
                                       const typeof(((struct vm_area_struct *)0)->shared.rb) *__mptr = (rb_new);
                                       (struct vm_area_struct *)((char *)__mptr - __builtin_offsetof(struct vm_area_struct, shared.rb));
                                   });
    new->shared.rb_subtree_last = old->shared.rb_subtree_last;
    old->shared.rb_subtree_last = vma_interval_tree_compute_subtree_last(old);
}
static const struct rb_augment_callbacks vma_interval_tree_augment = { vma_interval_tree_augment_propagate, vma_interval_tree_augment_copy, vma_interval_tree_augment_rotate };


//��node��vma_start_pgoffΪ��ֵ���뵽root��
void vma_interval_tree_insert(struct vm_area_struct *node, struct rb_root *root)
{
    struct rb_node **link = &root->rb_node, *rb_parent = ((void *)0);
    unsigned long start = vma_start_pgoff(node), last = vma_last_pgoff(node);
    struct vm_area_struct *parent;
    while (*link) {
        rb_parent = *link;
        parent = ({ const typeof(((struct vm_area_struct *)0)->shared.rb) *__mptr = (rb_parent); (struct vm_area_struct *)((char *)__mptr - __builtin_offsetof(struct vm_area_struct, shared.rb));});
        if (parent->shared.rb_subtree_last < last) {
            parent->shared.rb_subtree_last = last;
        }
        if (start < vma_start_pgoff(parent)) {
            link = &parent->shared.rb.rb_left;
        } else {
            link = &parent->shared.rb.rb_right;
        }
    }
    node->shared.rb_subtree_last = last;
    rb_link_node(&node->shared.rb, rb_parent, link);
    rb_insert_augmented(&node->shared.rb, root, &vma_interval_tree_augment);
}

void vma_interval_tree_remove(struct vm_area_struct *node, struct rb_root *root)
{
    rb_erase_augmented(&node->shared.rb, root, &vma_interval_tree_augment);
}

/*************************************************************
����node�����ӽڵ㣬�ҵ���һ����(start,last)�н����Ľڵ�
���ҷ�ʽ��:
��Ϊÿ��node�������д�node�����ӽڵ��rb_subtree_last�����ֵ������
����ʹ��rb_subtree_last���ң�
1. ˳��node�����ӽڵ���ң��ҵ�һ���ӽڵ�node1�����ӽڵ�����ӽڵ�Ϊ��
����rb_subtree_lastֵС��start��������Ҫ���ҵ�һ����node1�������ӽڵ㡣
2. ��ʱnode1��start������С��ֵ�ˣ��������(start,last)�Ƿ��ཻ���ཻ�򷵻ء�
    ������ཻ�����������ӽڵ���node2�ϡ�
3. ��node2�ظ�����1��2    
*************************************************************/
static struct vm_area_struct *vma_interval_tree_subtree_search(struct vm_area_struct *node, unsigned long start, unsigned long last)
{
    while (true) {
        if (node->shared.rb.rb_left) {
            struct vm_area_struct *left = ( {
                                     const typeof(((struct vm_area_struct *)0)->shared.rb) *__mptr = (node->shared.rb.rb_left);
                                     (struct vm_area_struct *)((char *)__mptr - __builtin_offsetof(struct vm_area_struct, shared.rb));
                                 });
            if (start <= left->shared.rb_subtree_last) {
                node = left;
                continue;
            }
        }
        if (vma_start_pgoff(node) <= last) {
            if (start <= vma_last_pgoff(node)) {
                return node;
            }
            if (node->shared.rb.rb_right) {
                node = ({ const typeof(((struct vm_area_struct *)0)->shared.rb) *__mptr = (node->shared.rb.rb_right); (struct vm_area_struct *)((char *)__mptr - __builtin_offsetof(struct vm_area_struct, shared.rb));});
                if (start <= node->shared.rb_subtree_last) {
                    continue;
                }
            }
        }
        return ((void *)0);
    }
}
//���ҵ�һ����(start , last)�н�����node
struct vm_area_struct *vma_interval_tree_iter_first(struct rb_root *root, unsigned long start, unsigned long last)
{
    struct vm_area_struct *node;
    if (!root->rb_node) {
        return ((void *)0);
    }
    node = ({ const typeof(((struct vm_area_struct *)0)->shared.rb) *__mptr = (root->rb_node); (struct vm_area_struct *)((char *)__mptr - __builtin_offsetof(struct vm_area_struct, shared.rb));});
    if (node->shared.rb_subtree_last < start) {
        return ((void *)0);
    }
    return vma_interval_tree_subtree_search(node, start, last);
}

//������һ����(start , last)�н�����node
struct vm_area_struct *vma_interval_tree_iter_next(struct vm_area_struct *node, unsigned long start, unsigned long last)
{
    struct rb_node *rb = node->shared.rb.rb_right, *prev;
    while (true) {
        if (rb) {
            struct vm_area_struct *right = ( {
                                                 const typeof(((struct vm_area_struct *)0)->shared.rb) *__mptr = (rb);
                                                 (struct vm_area_struct *)((char *)__mptr - __builtin_offsetof(struct vm_area_struct, shared.rb));
                                             });
            if (start <= right->shared.rb_subtree_last) {
                return vma_interval_tree_subtree_search(right, start, last);
            }
        }
        do {
            rb = ((struct rb_node *)((&node->shared.rb)->__rb_parent_color & ~3));
            if (!rb) {
                return ((void *)0);
            }
            prev = &node->shared.rb;
            node = ({ const typeof(((struct vm_area_struct *)0)->shared.rb) *__mptr = (rb); (struct vm_area_struct *)((char *)__mptr - __builtin_offsetof(struct vm_area_struct, shared.rb));});
            rb = node->shared.rb.rb_right;
        } while (prev == rb);
        if (last < vma_start_pgoff(node)) {
            return ((void *)0);
        } else if (start <= vma_last_pgoff(node)) {
            return node;
        }
    }
}

#endif

/* Insert node immediately after prev in the interval tree */
//��node���뵽prev�ĺ��棬������ĽǶȿ�����node��ֵҪ����prev������Ҫ��prev��
//���е��ӽڵ㶼С(��Ȼ�Ͳ�����뵽prev�ĺ�����������ĺ�����)�����Ե�prev->right
//Ϊ�յĻ���ֱ�ӷŵ�prev->right�������Ҫһֱ����prev��left���뵽�����
void vma_interval_tree_insert_after(struct vm_area_struct *node,
				    struct vm_area_struct *prev,
				    struct rb_root *root)
{
	struct rb_node **link;
	struct vm_area_struct *parent;
	unsigned long last = vma_last_pgoff(node);

	VM_BUG_ON_VMA(vma_start_pgoff(node) != vma_start_pgoff(prev), node);

	if (!prev->shared.rb.rb_right) {
		parent = prev;
		link = &prev->shared.rb.rb_right;
	} else {
		parent = rb_entry(prev->shared.rb.rb_right,
				  struct vm_area_struct, shared.rb);
		if (parent->shared.rb_subtree_last < last)
			parent->shared.rb_subtree_last = last;
		while (parent->shared.rb.rb_left) {
			parent = rb_entry(parent->shared.rb.rb_left,
				struct vm_area_struct, shared.rb);
			if (parent->shared.rb_subtree_last < last)
				parent->shared.rb_subtree_last = last;
		}
		link = &parent->shared.rb.rb_left;
	}

	node->shared.rb_subtree_last = last;
	rb_link_node(&node->shared.rb, &parent->shared.rb, link);
	rb_insert_augmented(&node->shared.rb, root,
			    &vma_interval_tree_augment);
}

static inline unsigned long avc_start_pgoff(struct anon_vma_chain *avc)
{
	return vma_start_pgoff(avc->vma);
}

static inline unsigned long avc_last_pgoff(struct anon_vma_chain *avc)
{
	return vma_last_pgoff(avc->vma);
}
#if 0
INTERVAL_TREE_DEFINE(struct anon_vma_chain, rb, unsigned long, rb_subtree_last,
		     avc_start_pgoff, avc_last_pgoff,
		     static inline, __anon_vma_interval_tree)

#else
//wgz ����ĺ괦��֮������(ɾ��һЩattribute)
static inline __attribute__((no_instrument_function)) unsigned long __anon_vma_interval_tree_compute_subtree_last(struct anon_vma_chain *node)
{
    unsigned long max = avc_last_pgoff(node), subtree_last;
    if (node->rb.rb_left) {
        subtree_last = ({ const typeof(((struct anon_vma_chain *)0)->rb) *__mptr = (node->rb.rb_left); (struct anon_vma_chain *)((char *)__mptr - __builtin_offsetof(struct anon_vma_chain, rb));})->rb_subtree_last;
        if (max < subtree_last) {
            max = subtree_last;
        }
    }
    if (node->rb.rb_right) {
        subtree_last = ({ const typeof(((struct anon_vma_chain *)0)->rb) *__mptr = (node->rb.rb_right); (struct anon_vma_chain *)((char *)__mptr - __builtin_offsetof(struct anon_vma_chain, rb));})->rb_subtree_last;
        if (max < subtree_last) {
            max = subtree_last;
        }
    }
    return max;
}
static inline __attribute__((no_instrument_function)) void __anon_vma_interval_tree_augment_propagate(struct rb_node *rb, struct rb_node *stop)
{
    while (rb != stop) {
        struct anon_vma_chain *node = ( {
                                            const typeof(((struct anon_vma_chain *)0)->rb) *__mptr = (rb);
                                            (struct anon_vma_chain *)((char *)__mptr - __builtin_offsetof(struct anon_vma_chain, rb));
                                        });
        unsigned long augmented = __anon_vma_interval_tree_compute_subtree_last(node);
        if (node->rb_subtree_last == augmented) {
            break;
        }
        node->rb_subtree_last = augmented;
        rb = ((struct rb_node *)((&node->rb)->__rb_parent_color & ~3));
    }
}
static inline __attribute__((no_instrument_function)) void __anon_vma_interval_tree_augment_copy(struct rb_node *rb_old, struct rb_node *rb_new)
{
    struct anon_vma_chain *old = ( {
                                       const typeof(((struct anon_vma_chain *)0)->rb) *__mptr = (rb_old);
                                       (struct anon_vma_chain *)((char *)__mptr - __builtin_offsetof(struct anon_vma_chain, rb));
                                   });
    struct anon_vma_chain *new = ( {
                                       const typeof(((struct anon_vma_chain *)0)->rb) *__mptr = (rb_new);
                                       (struct anon_vma_chain *)((char *)__mptr - __builtin_offsetof(struct anon_vma_chain, rb));
                                   });
    new->rb_subtree_last = old->rb_subtree_last;
}
static void __anon_vma_interval_tree_augment_rotate(struct rb_node *rb_old, struct rb_node *
        rb_new)
{
    struct anon_vma_chain *old = ( {
                                       const typeof(((struct anon_vma_chain *)0)->rb) *__mptr = (rb_old);
                                       (struct anon_vma_chain *)((char *)__mptr - __builtin_offsetof(struct anon_vma_chain, rb));
                                   });
    struct anon_vma_chain *new = ( {
                                       const typeof(((struct anon_vma_chain *)0)->rb) *__mptr = (rb_new);
                                       (struct anon_vma_chain *)((char *)__mptr - __builtin_offsetof(struct anon_vma_chain, rb));
                                   });
    new->rb_subtree_last = old->rb_subtree_last;
    old->rb_subtree_last = __anon_vma_interval_tree_compute_subtree_last(old);
}
static const struct rb_augment_callbacks __anon_vma_interval_tree_augment = { __anon_vma_interval_tree_augment_propagate, __anon_vma_interval_tree_augment_copy, __anon_vma_interval_tree_augment_rotate };
static inline __attribute__((no_instrument_function)) void __anon_vma_interval_tree_insert(struct anon_vma_chain *node, struct rb_root *root)
{
    struct rb_node **link = &root->rb_node, *rb_parent = ((void *)0);
    unsigned long start = avc_start_pgoff(node), last = avc_last_pgoff(node);
    struct anon_vma_chain *parent;
    while (*link) {
        rb_parent = *link;
        parent = ({ const typeof(((struct anon_vma_chain *)0)->rb) *__mptr = (rb_parent); (struct anon_vma_chain *)((char *)__mptr - __builtin_offsetof(struct anon_vma_chain, rb));});
        if (parent->rb_subtree_last < last) {
            parent->rb_subtree_last = last;
        }
        if (start < avc_start_pgoff(parent)) {
            link = &parent->rb.rb_left;
        } else {
            link = &parent->rb.rb_right;
        }
    }
    node->rb_subtree_last = last;
    rb_link_node(&node->rb, rb_parent, link);
    rb_insert_augmented(&node->rb, root, &__anon_vma_interval_tree_augment);
}
static inline __attribute__((no_instrument_function)) void __anon_vma_interval_tree_remove(struct anon_vma_chain *node, struct rb_root *root)
{
    rb_erase_augmented(&node->rb, root, &__anon_vma_interval_tree_augment);
}
static struct anon_vma_chain *__anon_vma_interval_tree_subtree_search(struct anon_vma_chain *node, unsigned long start, unsigned long last)
{
    while (true) {
        if (node->rb.rb_left) {
            struct anon_vma_chain *left = ( {
                                                const typeof(((struct anon_vma_chain
                                                        *)0)->rb) *__mptr = (node->rb.rb_left);
                                                (struct anon_vma_chain *)((char *)__mptr - __builtin_offsetof(struct anon_vma_chain, rb));
                                            });
            if (start <= left->rb_subtree_last) {
                node = left;
                continue;
            }
        }
        if (avc_start_pgoff(node) <= last) {
            if (start <= avc_last_pgoff(node)) {
                return node;
            }
            if (node->rb.rb_right) {
                node = ({ const typeof(((struct anon_vma_chain *)0)->rb) *__mptr = (node->rb.rb_right); (struct anon_vma_chain *)((char *)__mptr - __builtin_offsetof(struct anon_vma_chain, rb));});
                if (start <= node->rb_subtree_last) {
                    continue;
                }
            }
        }
        return ((void *)0);
    }
}
static inline __attribute__((no_instrument_function)) struct anon_vma_chain *__anon_vma_interval_tree_iter_first(struct rb_root *root, unsigned long start, unsigned long last)
{
    struct anon_vma_chain *node;
    if (!root->rb_node) {
        return ((void *)0);
    }
    node = ({ const typeof(((struct anon_vma_chain *)0)->rb) *__mptr = (root->rb_node); (struct anon_vma_chain *)((char *)__mptr - __builtin_offsetof(struct anon_vma_chain, rb));});
    if (node->rb_subtree_last < start) {
        return ((void *)0);
    }
    return __anon_vma_interval_tree_subtree_search(node, start, last);
}
static inline __attribute__((no_instrument_function)) struct anon_vma_chain *__anon_vma_interval_tree_iter_next(struct anon_vma_chain *node, unsigned long start, unsigned long last)
{
    struct rb_node *rb = node->rb.rb_right, *prev;
    while (true) {
        if (rb) {
            struct anon_vma_chain *right = ( {
                                                 const typeof(((struct anon_vma_chain *)0)->rb) *__mptr = (rb);
                                                 (struct anon_vma_chain *)((char *)__mptr - __builtin_offsetof(struct anon_vma_chain, rb));
                                             });
            if (start <= right->rb_subtree_last) {
                return __anon_vma_interval_tree_subtree_search(right, start, last);
            }
        }
        do {
            rb = ((struct rb_node *)((&node->rb)->__rb_parent_color & ~3));
            if (!rb) {
                return ((void *)0);
            }
            prev = &node->rb;
            node = ({ const typeof(((struct anon_vma_chain *)0)->rb) *__mptr = (rb); (struct anon_vma_chain *)((char *)__mptr - __builtin_offsetof(struct anon_vma_chain, rb));});
            rb = node->rb.rb_right;
        } while (prev == rb);
        if (last < avc_start_pgoff(no
                                   de)) {
            return ((void *)0);
        } else if (start <= avc_last_pgoff(node)) {
            return node;
        }
    }
}


#endif
void anon_vma_interval_tree_insert(struct anon_vma_chain *node,
				   struct rb_root *root)
{
#ifdef CONFIG_DEBUG_VM_RB
	node->cached_vma_start = avc_start_pgoff(node);
	node->cached_vma_last = avc_last_pgoff(node);
#endif
	__anon_vma_interval_tree_insert(node, root);
}

void anon_vma_interval_tree_remove(struct anon_vma_chain *node,
				   struct rb_root *root)
{
	__anon_vma_interval_tree_remove(node, root);
}

struct anon_vma_chain *
anon_vma_interval_tree_iter_first(struct rb_root *root,
				  unsigned long first, unsigned long last)
{
	return __anon_vma_interval_tree_iter_first(root, first, last);
}

struct anon_vma_chain *
anon_vma_interval_tree_iter_next(struct anon_vma_chain *node,
				 unsigned long first, unsigned long last)
{
	return __anon_vma_interval_tree_iter_next(node, first, last);
}

#ifdef CONFIG_DEBUG_VM_RB
void anon_vma_interval_tree_verify(struct anon_vma_chain *node)
{
	WARN_ON_ONCE(node->cached_vma_start != avc_start_pgoff(node));
	WARN_ON_ONCE(node->cached_vma_last != avc_last_pgoff(node));
}
#endif

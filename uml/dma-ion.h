class dma_buf_ops {
	int attach( dma_buf *,  device *,
			 dma_buf_attachment *);

	void detach( dma_buf *,  dma_buf_attachment *);

	/* For {map,unmap}_dma_buf below, any specific buffer attributes
	 * required should get added to device_dma_parameters accessible
	 * via dev->dma_params.
	 */
	 sg_table * map_dma_buf( dma_buf_attachment *,
						enum dma_data_direction);
	void unmap_dma_buf( dma_buf_attachment *,
						 sg_table *,
						enum dma_data_direction);
	/* TODO: Add try_map_dma_buf version, to return immed with -EBUSY
	 * if the call would block.
	 */

	/* after final dma_buf_put() */
	void release( dma_buf *);

	int begin_cpu_access( dma_buf *, enum dma_data_direction);
	int end_cpu_access( dma_buf *, enum dma_data_direction);
	void *kmap_atomic( dma_buf *, unsigned long);
	void kunmap_atomic( dma_buf *, unsigned long, void *);
	void *kmap( dma_buf *, unsigned long);
	void kunmap( dma_buf *, unsigned long, void *);

	int mmap( dma_buf *,  vm_area_ *vma);

	void *vmap( dma_buf *);
	void vunmap( dma_buf *, void *vaddr);
};


class dma_buf {
	size_t size;
	 file *file;
	 list_head attachments;
	const  dma_buf_ops *ops;
	 mutex lock;
	unsigned vmapping_counter;
	void *vmap_ptr;
	const char *exp_name;
	 module *owner;
	 list_head list_node;
	void *priv;
	 reservation_object *resv;

	/* poll support */
	wait_queue_head_t poll;

	 class dma_buf_poll_cb_t {
		 fence_cb cb;
		wait_queue_head_t *poll;

		unsigned long active;
	} cb_excl, cb_shared;

static inline void get_dma_buf( dma_buf *dmabuf);

dma_buf_attachment *dma_buf_attach( dma_buf *dmabuf,
							 device *dev);
void dma_buf_detach( dma_buf *dmabuf,
				 dma_buf_attachment *dmabuf_attach);

 dma_buf *dma_buf_export(const  dma_buf_export_info *exp_info);

int dma_buf_fd( dma_buf *dmabuf, int flags);
 dma_buf *dma_buf_get(int fd);
void dma_buf_put( dma_buf *dmabuf);

 sg_table *dma_buf_map_attachment( dma_buf_attachment *,
					enum dma_data_direction);
void dma_buf_unmap_attachment( dma_buf_attachment *,  sg_table *,
				enum dma_data_direction);
int dma_buf_begin_cpu_access( dma_buf *dma_buf,
			     enum dma_data_direction dir);
int dma_buf_end_cpu_access( dma_buf *dma_buf,
			   enum dma_data_direction dir);
void *dma_buf_kmap_atomic( dma_buf *, unsigned long);
void dma_buf_kunmap_atomic( dma_buf *, unsigned long, void *);
void *dma_buf_kmap( dma_buf *, unsigned long);
void dma_buf_kunmap( dma_buf *, unsigned long, void *);

int dma_buf_mmap( dma_buf *,  vm_area_ *,
		 unsigned long);
void *dma_buf_vmap( dma_buf *);
void dma_buf_vunmap( dma_buf *, void *vaddr);	
	
};


class dma_buf_attachment {
	 dma_buf *dmabuf;
	 device *dev;
	 list_head node;
	void *priv;
};

class dma_buf_export_info {
	const char *exp_name;
	 module *owner;
	const  dma_buf_ops *ops;
	size_t size;
	int flags;
	 reservation_object *resv;
	void *priv;
};



class ion_client {
	 rb_node node;
	 ion_device *dev;
//所有属于此client的handle链接到这里    
	 rb_root handles;
	 idr idr;
	 mutex lock;
	const char *name;
	char *display_name;
	int display_serial;
	 task_ *task;
	pid_t pid;
	 dentry *debug_root;
};


class ion_handle {
	 kref ref;
	 ion_client *client;
	 ion_buffer *buffer;
	 rb_node node;
	unsigned int kmap_cnt;
	int id;
};


class ion_buffer {
	 kref ref;
	union {
		 rb_node node;
		 list_head list;
	};
	 ion_device *dev;
	 ion_heap *heap;
	unsigned long flags;
	unsigned long private_flags;
	size_t size;
	union {
		void *priv_virt;
		ion_phys_addr_t priv_phys;
	};
	 mutex lock;
	int kmap_cnt;
	void *vaddr;
	int dmap_cnt;
	 sg_table *sg_table;
	 page **pages;
	 list_head vmas;
	/* used to track orphaned buffers */
	int handle_count;
	char task_comm[TASK_COMM_LEN];
	pid_t pid;
};


class ion_heap_ops {
	int allocate( ion_heap *heap,
			 ion_buffer *buffer, unsigned long len,
			unsigned long align, unsigned long flags);
	void free( ion_buffer *buffer);
	int phys( ion_heap *heap,  ion_buffer *buffer,
		    ion_phys_addr_t *addr, size_t *len);
	 sg_table * map_dma( ion_heap *heap,
				      ion_buffer *buffer);
	void unmap_dma( ion_heap *heap,  ion_buffer *buffer);
	void * map_kernel( ion_heap *heap,  ion_buffer *buffer);
	void unmap_kernel( ion_heap *heap,  ion_buffer *buffer);
	int map_user( ion_heap *mapper,  ion_buffer *buffer,
			 vm_area_ *vma);
	int shrink( ion_heap *heap, gfp_t gfp_mask, int nr_to_scan);
};


class ion_heap {
	 plist_node node;
	 ion_device *dev;
	enum ion_heap_type type;
	 ion_heap_ops *ops;
	unsigned long flags;
	unsigned int id;
	const char *name;
	 shrinker shrinker;
	 list_head free_list;
	size_t free_list_size;
	spinlock_t free_lock;
	wait_queue_head_t waitqueue;
	 task_ *task;

	int debug_show( ion_heap *heap,  seq_file *, void *);
};








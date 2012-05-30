/**
 * Header file of MobiCore Driver Kernel Module.
 *
 * @addtogroup MobiCore_Driver_Kernel_Module
 * @{
 * Internal structures of the McDrvModule
 * @file
 *
 * Header file the MobiCore Driver Kernel Module,
 * its internal structures and defines.
 *
 * <!-- Copyright Giesecke & Devrient GmbH 2009-2012 -->
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _MC_DRV_KMOD_H_
#define _MC_DRV_KMOD_H_

#include "mcDrvModuleLinuxApi.h"
#include "public/mcDrvModuleApi.h"
/** Platform specific settings */
#include "platform.h"

/** ARM Specific masks and modes */
#define ARM_CPSR_MASK 0x1F
#define ARM_MONITOR_MODE 0b10110
#define ARM_SECURITY_EXTENSION_MASK 0x30

/** Number of page table entries in one L2 table. This is ARM specific, an
 *  L2 table covers 1 MiB by using 256 entry referring to 4KiB pages each.
 */
#define MC_ARM_L2_TABLE_ENTRIES		256

/** Maximum number of contiguous buffer allocations for one driver instance. */
#define MC_DRV_KMOD_CONTG_BUFFER_MAX	16

/** Number of pages for L2 tables. There are 4 table in each page. */
#define MC_DRV_KMOD_L2_TABLE_PER_PAGES	4
#define MC_DRV_KMOD_L2_TABLE_PAGES	8

struct l2table {
	pte_t	table_entries[MC_ARM_L2_TABLE_ENTRIES];
};

#define INVALID_ADDRESS     ((void *)(-1))

/** ARM L2 PTE bits */
#define L2_FLAG_SMALL_XN    (1U <<  0)
#define L2_FLAG_SMALL       (1U <<  1)
#define L2_FLAG_B           (1U <<  2)
#define L2_FLAG_C           (1U <<  3)
#define L2_FLAG_AP0         (1U <<  4)
#define L2_FLAG_AP1         (1U <<  5)
#define L2_FLAG_SMALL_TEX0  (1U <<  6)
#define L2_FLAG_SMALL_TEX1  (1U <<  7)
#define L2_FLAG_SMALL_TEX2  (1U <<  8)
#define L2_FLAG_APX         (1U <<  9)
#define L2_FLAG_S           (1U << 10)
#define L2_FLAG_NG          (1U << 11)

/**
 * Contiguous buffer allocated to TLCs.
 * Used for kernel module bookkeeping.
 * The virtual kernel address is added for a simpler search algorithm.
 */
struct mc_contg_buffer {
	unsigned int	handle; /* unique handle */
	void		*virt_user_addr; /**< virtual User start address */
	void		*virt_kernel_addr; /**< virtual Kernel start address */
	void		*phys_addr; /**< physical start address */
	unsigned int	num_pages; /**< number of pages */
};

/**
 * Driver instance data.
 */
struct mc_instance {
	/** unique handle */
	unsigned int	handle;
	/** process that opened this instance */
	pid_t		pid_vnr;
	struct {
		/** number of pages */
		unsigned int	num_pages;
		/** virtual start address kernel address space generated
			by mmap command */
		void		*virt_addr;
		/** physical start address kernel address space generated
			by mmap command */
		void		*phys_addr;
	} map;
	/** buffer list for mmap generated address space and
		its virtual client address */
	struct mc_contg_buffer	contg_buffers[MC_DRV_KMOD_CONTG_BUFFER_MAX];
};



/** data structure of 4 L2 tables within one 4kb page*/
struct mc_l2_table_store {
	struct l2table table[MC_DRV_KMOD_L2_TABLE_PER_PAGES];
};


/** bookkeeping data structure that manages 4 L2 tables in one page */
struct mc_l2_tables_set {
	struct list_head		list;
	unsigned int			usage_bitmap;	/**< usage bitmap */
	struct mc_l2_table_store	*kernel_virt;	/**< kernel virtual address */
	struct mc_l2_table_store	*phys;		/**< physical address */
	struct page			*page;		/**< pointer to page struct */
};

/** bookkeeping data structure that binds one L2 table
	to its chunk(page) and its user */
struct mc_used_l2_table {
	struct list_head	list;
	unsigned int		handle;
	unsigned int		flags;
	struct mc_instance	*owner;
	struct mc_l2_tables_set	*set;
	unsigned int		idx;
	unsigned int		nr_of_pages;
};

#define MC_WSM_L2_CONTAINER_WSM_LOCKED_BY_APP   (1U << 0)
#define MC_WSM_L2_CONTAINER_WSM_LOCKED_BY_MC    (1U << 1)


/** MobiCore S-SIQ interrupt context data. */
struct mc_ssiq_ctx {
	/**< S-SIQ interrupt counter */
	atomic_t	counter;
};


/** MobiCore Daemon context data. */
struct mc_daemon_ctx {
	/**< event semaphore */
	struct semaphore	sem;
	struct fasync_struct	*async_queue;
	/**< event counter */
	unsigned int		ssiq_counter;
};


/** MobiCore Driver Kernel Module context data. */
struct mc_drv_kmod_ctx {
	/**< ever incrementing counter */
	atomic_t		unique_counter;
	/**< S-SIQ interrupt context */
	struct mc_ssiq_ctx	ssiq_ctx;
	/**< MobiCore Daemon context */
	struct mc_daemon_ctx	daemon_ctx;
	/**< pointer to instance of daemon */
	struct mc_instance	*daemon_inst;
	/**< Backing store for L2 tables */
	struct list_head	mc_l2_tables_sets;
	/**< Bookkeeping for L2 tables */
	struct list_head	mc_used_l2_tables;
	/**< semaphore to synchronize access to above lists */
	struct semaphore	wsm_l2_sem;
};

/** MobiCore internal trace buffer structure. */
struct mc_trace_buf {
	uint32_t version; /**< version of trace buffer */
	uint32_t length; /**< length of allocated buffer(includes header) */
	uint32_t write_pos; /**< last write position */
	char  buff[1]; /**< start of the log buffer */
};

/*** MobiCore internal trace log setup. */
void mobicore_log_read(void);
long mobicore_log_setup(void *);
void mobicore_log_free(void);

#define MCDRV_DBG_ERROR(txt, ...) \
	printk(KERN_ERR "mcDrvKMod [%d] %s() ### ERROR: " txt, \
		task_pid_vnr(current), \
		__func__, \
		##__VA_ARGS__)

/* dummy function helper macro. */
#define DUMMY_FUNCTION()    do {} while (0)

#if defined(DEBUG)

/* #define DEBUG_VERBOSE */
#if defined(DEBUG_VERBOSE)
#define MCDRV_DBG_VERBOSE          MCDRV_DBG
#else
#define MCDRV_DBG_VERBOSE(...)     DUMMY_FUNCTION()
#endif

#define MCDRV_DBG(txt, ...) \
	printk(KERN_INFO "mcDrvKMod [%d on CPU%d] %s(): " txt, \
		task_pid_vnr(current), \
		raw_smp_processor_id(), \
		__func__, \
		##__VA_ARGS__)

#define MCDRV_DBG_WARN(txt, ...) \
	printk(KERN_WARNING "mcDrvKMod [%d] %s() WARNING: " txt, \
		task_pid_vnr(current), \
		__func__, \
		##__VA_ARGS__)

#define MCDRV_ASSERT(cond) \
	do { \
		if (unlikely(!(cond))) { \
			panic("mcDrvKMod Assertion failed: %s:%d\n", \
				__FILE__, __LINE__); \
		} \
	} while (0)

#else

#define MCDRV_DBG_VERBOSE(...)	DUMMY_FUNCTION()
#define MCDRV_DBG(...)		DUMMY_FUNCTION()
#define MCDRV_DBG_WARN(...)	DUMMY_FUNCTION()

#define MCDRV_ASSERT(...)	DUMMY_FUNCTION()

#endif /* [not] defined(DEBUG) */


#endif /* _MC_DRV_KMOD_H_ */
/** @} */

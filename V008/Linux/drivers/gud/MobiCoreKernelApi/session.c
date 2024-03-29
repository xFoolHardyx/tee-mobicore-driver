/** @addtogroup MCD_IMPL_LIB
 * @{
 * @file
 * <!-- Copyright Giesecke & Devrient GmbH 2009 - 2012 -->
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/types.h>
#include <linux/slab.h>
#include "mcKernelApi.h"
#include "public/MobiCoreDriverApi.h"

#include "session.h"

/*****************************************************************************/
struct bulkBufferDescriptor_t *bulkBufferDescriptor_create(
	void	*virtAddr,
	uint32_t  len,
	uint32_t  handle,
	void	*physAddrWsmL2
) {
	struct bulkBufferDescriptor_t *desc =
		kzalloc(sizeof(struct bulkBufferDescriptor_t), GFP_KERNEL);
	desc->virtAddr = virtAddr;
	desc->len = len;
	desc->handle = handle;
	desc->physAddrWsmL2 = physAddrWsmL2;
	return desc;
}

/*****************************************************************************/
struct session_t *session_create(
	uint32_t	 sessionId,
	void	   *pInstance,
	struct connection_t *connection
) {
	struct session_t *session =
			kzalloc(sizeof(struct session_t), GFP_KERNEL);
	session->sessionId = sessionId;
	session->pInstance = pInstance;
	session->notificationConnection = connection;

	session->sessionInfo.lastErr = SESSION_ERR_NO;
	session->sessionInfo.state = SESSION_STATE_INITIAL;

	INIT_LIST_HEAD(&(session->bulkBufferDescriptors));
	return session;
}


/*****************************************************************************/
void session_cleanup(
	struct session_t *session
) {
	struct bulkBufferDescriptor_t  *pBlkBufDescr;
	struct list_head *pos, *q;

	/* Unmap still mapped buffers */
	list_for_each_safe(pos, q, &session->bulkBufferDescriptors) {
		pBlkBufDescr =
			list_entry(pos, struct bulkBufferDescriptor_t, list);

		MCDRV_DBG_VERBOSE("Physical Address of L2 Table = 0x%X, "
				"handle= %d",
				(unsigned int)pBlkBufDescr->physAddrWsmL2,
				pBlkBufDescr->handle);

		/* ignore any error, as we cannot do anything in this case. */
		int ret = mobicore_unmap_vmem(session->pInstance,
						pBlkBufDescr->handle);
		if (0 != ret)
			MCDRV_DBG_ERROR("mobicore_unmap_vmem failed: %d", ret);

		list_del(pos);
		kfree(pBlkBufDescr);
	}

	/* Finally delete notification connection */
	connection_cleanup(session->notificationConnection);
	kfree(session);
}


/*****************************************************************************/
void session_setErrorInfo(
	struct session_t *session,
	int32_t   err
) {
	session->sessionInfo.lastErr = err;
}


/*****************************************************************************/
int32_t session_getLastErr(
	struct session_t *session
) {
	return session->sessionInfo.lastErr;
}


/*****************************************************************************/
struct bulkBufferDescriptor_t *session_addBulkBuf(
	struct session_t	*session,
	void		*buf,
	uint32_t	len
) {
	struct bulkBufferDescriptor_t *blkBufDescr = NULL;
	struct bulkBufferDescriptor_t  *tmp;
	struct list_head *pos;

	/* Search bulk buffer descriptors for existing vAddr
	   At the moment a virtual address can only be added one time */
	list_for_each(pos, &session->bulkBufferDescriptors) {
		tmp = list_entry(pos, struct bulkBufferDescriptor_t, list);
		if (tmp->virtAddr == buf)
			return NULL;
	}

	do {
		/* Prepare the interface structure for memory registration in
		   Kernel Module */
		void	*pPhysWsmL2;
		uint32_t  handle;

		int ret = mobicore_map_vmem(session->pInstance,
					buf,
					len,
					&handle,
					&pPhysWsmL2);

		if (0 != ret) {
			MCDRV_DBG_ERROR("mobicore_map_vmem failed, ret=%d",
					ret);
			break;
		}

		MCDRV_DBG_VERBOSE("Physical Address of L2 Table = 0x%X, "
				"handle=%d",
				(unsigned int)pPhysWsmL2,
				handle);

		/* Create new descriptor */
		blkBufDescr = bulkBufferDescriptor_create(
							buf,
							len,
							handle,
							pPhysWsmL2);

		/* Add to vector of descriptors */
		list_add_tail(&(blkBufDescr->list),
			&(session->bulkBufferDescriptors));
	} while (0);

	return blkBufDescr;
}


/*****************************************************************************/
bool session_removeBulkBuf(
	struct session_t *session,
	void	*virtAddr
) {
	bool ret = true;
	struct bulkBufferDescriptor_t  *pBlkBufDescr = NULL;
	struct bulkBufferDescriptor_t  *tmp;
	struct list_head *pos, *q;

	MCDRV_DBG_VERBOSE("Virtual Address = 0x%X", (unsigned int) virtAddr);

	/* Search and remove bulk buffer descriptor */
	list_for_each_safe(pos, q, &session->bulkBufferDescriptors) {
		tmp = list_entry(pos, struct bulkBufferDescriptor_t, list);
		if (tmp->virtAddr == virtAddr) {
			pBlkBufDescr = tmp;
			list_del(pos);
			break;
		}
	}

	if (NULL == pBlkBufDescr) {
		MCDRV_DBG_ERROR("Virtual Address not found");
		ret = false;
	} else {
		MCDRV_DBG_VERBOSE("WsmL2 phys=0x%X, handle=%d",
			(unsigned int)pBlkBufDescr->physAddrWsmL2,
			pBlkBufDescr->handle);

		/* ignore any error, as we cannot do anything */
		int ret = mobicore_unmap_vmem(session->pInstance,
						pBlkBufDescr->handle);
		if (0 != ret)
			MCDRV_DBG_ERROR("mobicore_unmap_vmem failed: %d", ret);

		kfree(pBlkBufDescr);
	}

	return ret;
}

/** @} */

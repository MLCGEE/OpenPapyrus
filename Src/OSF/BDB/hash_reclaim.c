/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 2011 Oracle and/or its affiliates.  All rights reserved.
 *
 * $Id$
 */
#include "db_config.h"
#include "db_int.h"
// @v9.5.5 #include "dbinc/db_page.h"
// @v9.5.5 #include "dbinc/crypto.h"
// @v9.5.5 #include "dbinc/btree.h"
// @v9.5.5 #include "dbinc/hash.h"
#pragma hdrstop

/*
 * __ham_reclaim --
 *	Reclaim the pages from a subdatabase and return them to the
 * parent free list.  For now, we link each freed page on the list
 * separately.  If people really store hash databases in subdatabases
 * and do a lot of creates and deletes, this is going to be a problem,
 * because hash needs chunks of contiguous storage.  We may eventually
 * need to go to a model where we maintain the free list with chunks of
 * contiguous pages as well.
 *
 * PUBLIC: int __ham_reclaim __P((DB *,
 * PUBLIC:      DB_THREAD_INFO *, DB_TXN *txn, uint32));
 */
int __ham_reclaim(DB*dbp, DB_THREAD_INFO * ip, DB_TXN * txn, uint32 flags)
{
	DBC * dbc;
	HASH_CURSOR * hcp;
	int ret;
	/* Open up a cursor that we'll use for traversing. */
	if((ret = __db_cursor(dbp, ip, txn, &dbc, 0)) != 0)
		return ret;
	hcp = (HASH_CURSOR *)dbc->internal;
	if((ret = __ham_get_meta(dbc)) != 0)
		goto err;
	/* Write lock the metapage for deallocations. */
	if((ret = __ham_dirty_meta(dbc, 0)) != 0)
		goto err;
	/* Avoid locking every page, we have the handle locked exclusive. */
	F_SET(dbc, DBC_DONTLOCK);
	if((ret = __ham_traverse(dbc, DB_LOCK_WRITE, __db_reclaim_callback, (void *)(uintptr_t)flags, 1)) != 0)
		goto err;
	if((ret = __dbc_close(dbc)) != 0)
		goto err;
	if((ret = __ham_release_meta(dbc)) != 0)
		goto err;
	return 0;
err:    
	if(hcp->hdr != NULL)
		__ham_release_meta(dbc);
	__dbc_close(dbc);
	return ret;
}
/*
 * __ham_truncate --
 *	Reclaim the pages from a subdatabase and return them to the
 * parent free list.
 *
 * PUBLIC: int __ham_truncate __P((DBC *, uint32 *));
 */
int __ham_truncate(DBC*dbc, uint32 * countp)
{
	uint32 count;
	int ret, t_ret;
	if((ret = __ham_get_meta(dbc)) != 0)
		return ret;
	count = 0;
	ret = __ham_traverse(dbc, DB_LOCK_WRITE, __db_truncate_callback, &count, 1);
	if((t_ret = __ham_release_meta(dbc)) != 0 && ret == 0)
		ret = t_ret;
	if(countp != NULL)
		*countp = count;
	return ret;
}

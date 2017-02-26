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
// @v9.5.5 #include "dbinc/lock.h"
// @v9.5.5 #include "dbinc/mp.h"
// @v9.5.5 #include "dbinc/crypto.h"
// @v9.5.5 #include "dbinc/btree.h"
// @v9.5.5 #include "dbinc/hash.h"
#pragma hdrstop
#include "dbinc/db_upgrade.h"
/*
 * __bam_30_btreemeta --
 *	Upgrade the metadata pages from version 6 to version 7.
 *
 * PUBLIC: int __bam_30_btreemeta __P((DB *, char *, uint8 *));
 */
int __bam_30_btreemeta(DB * dbp, char * real_name, uint8 * buf)
{
	int ret;
	ENV * env = dbp->env;
	BTMETA30 * newmeta = (BTMETA30 *)buf;
	BTMETA2X * oldmeta = (BTMETA2X *)buf;
	/*
	 * Move things from the end up, so we do not overwrite things.
	 * We are going to create a new uid, so we can move the stuff
	 * at the end of the structure first, overwriting the uid.
	 */
	newmeta->re_pad = oldmeta->re_pad;
	newmeta->re_len = oldmeta->re_len;
	newmeta->minkey = oldmeta->minkey;
	newmeta->maxkey = oldmeta->maxkey;
	newmeta->dbmeta.free = oldmeta->free;
	newmeta->dbmeta.flags = oldmeta->flags;
	newmeta->dbmeta.type  = P_BTREEMETA;
	newmeta->dbmeta.version = 7;
	/* Replace the unique ID. */
	if((ret = __os_fileid(env, real_name, 1, buf+36)) != 0)
		return ret;
	newmeta->root = 1;
	return 0;
}
/*
 * __bam_31_btreemeta --
 *	Upgrade the database from version 7 to version 8.
 *
 * PUBLIC: int __bam_31_btreemeta
 * PUBLIC:      __P((DB *, char *, uint32, DB_FH *, PAGE *, int *));
 */
int __bam_31_btreemeta(DB * dbp, char * real_name, uint32 flags, DB_FH * fhp, PAGE * h, int * dirtyp)
{
	BTMETA30 * oldmeta;
	BTMETA31 * newmeta;
	COMPQUIET(dbp, NULL);
	COMPQUIET(real_name, NULL);
	COMPQUIET(fhp, NULL);
	newmeta = (BTMETA31 *)h;
	oldmeta = (BTMETA30 *)h;
	/*
	 * Copy the effected fields down the page.
	 * The fields may overlap each other so we
	 * start at the bottom and use memmove.
	 */
	newmeta->root = oldmeta->root;
	newmeta->re_pad = oldmeta->re_pad;
	newmeta->re_len = oldmeta->re_len;
	newmeta->minkey = oldmeta->minkey;
	newmeta->maxkey = oldmeta->maxkey;
	memmove(newmeta->dbmeta.uid, oldmeta->dbmeta.uid, sizeof(oldmeta->dbmeta.uid));
	newmeta->dbmeta.flags = oldmeta->dbmeta.flags;
	newmeta->dbmeta.record_count = 0;
	newmeta->dbmeta.key_count = 0;
	ZERO_LSN(newmeta->dbmeta.unused3);

	/* Set the version number. */
	newmeta->dbmeta.version = 8;
	/* Upgrade the flags. */
	if(LF_ISSET(DB_DUPSORT))
		F_SET(&newmeta->dbmeta, BTM_DUPSORT);
	*dirtyp = 1;
	return 0;
}
/*
 * __bam_31_lbtree --
 *	Upgrade the database btree leaf pages.
 *
 * PUBLIC: int __bam_31_lbtree
 * PUBLIC:      __P((DB *, char *, uint32, DB_FH *, PAGE *, int *));
 */
int __bam_31_lbtree(DB * dbp, char * real_name, uint32 flags, DB_FH * fhp, PAGE * h, int * dirtyp)
{
	BKEYDATA * bk;
	db_pgno_t pgno;
	int ret = 0;
	for(db_indx_t indx = O_INDX; indx < NUM_ENT(h); indx += P_INDX) {
		bk = GET_BKEYDATA(dbp, h, indx);
		if(B_TYPE(bk->type) == B_DUPLICATE) {
			pgno = GET_BOVERFLOW(dbp, h, indx)->pgno;
			if((ret = __db_31_offdup(dbp, real_name, fhp, LF_ISSET(DB_DUPSORT) ? 1 : 0, &pgno)) != 0)
				break;
			if(pgno != GET_BOVERFLOW(dbp, h, indx)->pgno) {
				*dirtyp = 1;
				GET_BOVERFLOW(dbp, h, indx)->pgno = pgno;
			}
		}
	}
	return ret;
}

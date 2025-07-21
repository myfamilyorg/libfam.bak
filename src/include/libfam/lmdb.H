/********************************************************************************
 * MIT License
 *
 * Copyright (c) 2025 Christopher Gilliard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *******************************************************************************/

#ifndef _LMDB_H
#define _LMDB_H

#include <libfam/error.H>
#include <libfam/limits.H>
#include <libfam/syscall_const.H>
#include <libfam/types.H>

typedef i32 mdb_mode_t;
typedef u64 mdb_size_t;
typedef i32(MDB_msg_func)(const u8 *msg, void *ctx);
typedef struct MDB_env MDB_env;
typedef struct MDB_txn MDB_txn;
typedef u32 MDB_dbi;
typedef struct MDB_cursor MDB_cursor;
typedef void MDB_assert_func(MDB_env *env, const u8 *msg);

typedef struct MDB_stat {
	u32 ms_psize;
	u32 ms_depth;
	mdb_size_t ms_branch_pages;
	mdb_size_t ms_leaf_pages;
	mdb_size_t ms_overflow_pages;
	mdb_size_t ms_entries;
} MDB_stat;

typedef struct MDB_envinfo {
	void *me_mapaddr;
	mdb_size_t me_mapsize;
	mdb_size_t me_last_pgno;
	mdb_size_t me_last_txnid;
	u32 me_maxreaders;
	u32 me_numreaders;
} MDB_envinfo;
typedef i32 mdb_filehandle_t;

typedef struct MDB_val {
	u64 mv_size;   /**< size of the data item */
	void *mv_data; /**< address of the data item */
} MDB_val;

typedef i32(MDB_cmp_func)(const MDB_val *a, const MDB_val *b);

typedef void(MDB_rel_func)(MDB_val *item, void *oldptr, void *newptr,
			   void *relctx);

typedef enum MDB_cursor_op {
	MDB_FIRST,     /**< Position at first key/data item */
	MDB_FIRST_DUP, /**< Position at first data item of current key.
					       Only for #MDB_DUPSORT */
	MDB_GET_BOTH,  /**< Position at key/data pair. Only for #MDB_DUPSORT */
	MDB_GET_BOTH_RANGE, /**< position at key, nearest data. Only for
			       #MDB_DUPSORT */
	MDB_GET_CURRENT,    /**< Return key/data at current cursor position */
	MDB_GET_MULTIPLE,   /**< Return up to a page of duplicate data items
						    from current cursor position.
			       Move cursor to prepare   for #MDB_NEXT_MULTIPLE.
			       Only   for #MDB_DUPFIXED */
	MDB_LAST,	    /**< Position at last key/data item */
	MDB_LAST_DUP,	    /**< Position at last data item of current key.
						    Only for #MDB_DUPSORT */
	MDB_NEXT,	    /**< Position at next data item */
	MDB_NEXT_DUP,	    /**< Position at next data item of current key.
						    Only for #MDB_DUPSORT */
	MDB_NEXT_MULTIPLE,  /**< Return up to a page of duplicate data items
						    from next cursor position.
			       Move cursor to prepare  for #MDB_NEXT_MULTIPLE.
			       Only for #MDB_DUPFIXED */
	MDB_NEXT_NODUP,	    /**< Position at first data item of next key */
	MDB_PREV,	    /**< Position at previous data item */
	MDB_PREV_DUP,	    /**< Position at previous data item of current key.
						    Only for #MDB_DUPSORT */
	MDB_PREV_NODUP,	    /**< Position at last data item of previous key */
	MDB_SET,	    /**< Position at specified key */
	MDB_SET_KEY,	    /**< Position at specified key, return key + data */
	MDB_SET_RANGE,	    /**< Position at first key greater than or equal to
			       specified key. */
	MDB_PREV_MULTIPLE   /**< Position at previous page and return up to
						    a page of duplicate data
			       items. Only for #MDB_DUPFIXED */
} MDB_cursor_op;

#define mdb_open(txn, name, flags, dbi) mdb_dbi_open(txn, name, flags, dbi)
#define mdb_close(env, dbi) mdb_dbi_close(env, dbi)

#define MDB_SIZE_MAX U64_MAX
#define MDB_PRIy(t) PRI##t##64
#define MDB_SCNy(t) SCN##t##64
#define mdb_env_create mdb_env_create_vl32

#define MDB_VERSION_MAJOR 0
#define MDB_VERSION_MINOR 9
#define MDB_VERSION_PATCH 70

#define MDB_VERINT(a, b, c) (((a) << 24) | ((b) << 16) | (c))

#define MDB_VERSION_FULL \
	MDB_VERINT(MDB_VERSION_MAJOR, MDB_VERSION_MINOR, MDB_VERSION_PATCH)

#define MDB_VERSION_DATE "December 19, 2015"

#define MDB_VERSTR(a, b, c, d) "LMDB " #a "." #b "." #c ": (" d ")"

#define MDB_VERFOO(a, b, c, d) MDB_VERSTR(a, b, c, d)

#define MDB_VERSION_STRING                                                  \
	MDB_VERFOO(MDB_VERSION_MAJOR, MDB_VERSION_MINOR, MDB_VERSION_PATCH, \
		   MDB_VERSION_DATE)

#define MDB_FIXEDMAP 0x01
#define MDB_NOSUBDIR 0x4000
#define MDB_NOSYNC 0x10000
#define MDB_RDONLY 0x20000
#define MDB_NOMETASYNC 0x40000
#define MDB_WRITEMAP 0x80000
#define MDB_MAPASYNC 0x100000
#define MDB_NOTLS 0x200000
#define MDB_NOLOCK 0x400000
#define MDB_NORDAHEAD 0x800000
#define MDB_NOMEMINIT 0x1000000
#define MDB_PREVSNAPSHOT 0x2000000
#define MDB_REVERSEKEY 0x02
#define MDB_DUPSORT 0x04
#define MDB_INTEGERKEY 0x08
#define MDB_DUPFIXED 0x10
#define MDB_INTEGERDUP 0x20
#define MDB_REVERSEDUP 0x40
#define MDB_CREATE 0x40000
#define MDB_NOOVERWRITE 0x10
#define MDB_NODUPDATA 0x20
#define MDB_CURRENT 0x40
#define MDB_RESERVE 0x10000
#define MDB_APPEND 0x20000
#define MDB_APPENDDUP 0x40000
#define MDB_MULTIPLE 0x80000
#define MDB_CP_COMPACT 0x01
#define MDB_SUCCESS 0
#define MDB_KEYEXIST (-30799)
#define MDB_NOTFOUND (-30798)
#define MDB_PAGE_NOTFOUND (-30797)
#define MDB_CORRUPTED (-30796)
#define MDB_PANIC (-30795)
#define MDB_VERSION_MISMATCH (-30794)
#define MDB_INVALID (-30793)
#define MDB_MAP_FULL (-30792)
#define MDB_DBS_FULL (-30791)
#define MDB_READERS_FULL (-30790)
#define MDB_TLS_FULL (-30789)
#define MDB_TXN_FULL (-30788)
#define MDB_CURSOR_FULL (-30787)
#define MDB_PAGE_FULL (-30786)
#define MDB_MAP_RESIZED (-30785)
#define MDB_INCOMPATIBLE (-30784)
#define MDB_BAD_RSLOT (-30783)
#define MDB_BAD_TXN (-30782)
#define MDB_BAD_VALSIZE (-30781)
#define MDB_BAD_DBI (-30780)
#define MDB_PROBLEM (-30779)
#define MDB_LAST_ERRCODE MDB_PROBLEM

u8 *mdb_version(i32 *major, i32 *minor, i32 *patch);
const u8 *mdb_strerror(i32 err);
i32 mdb_env_create(MDB_env **env);
i32 mdb_env_open(MDB_env *env, const u8 *path, u32 flags, mdb_mode_t mode);
i32 mdb_env_copy(MDB_env *env, const u8 *path);
i32 mdb_env_copy2(MDB_env *env, const u8 *path, u32 flags);
i32 mdb_env_copyfd2(MDB_env *env, mdb_filehandle_t fd, u32 flags);
i32 mdb_env_stat(MDB_env *env, MDB_stat *stat);
i32 mdb_env_info(MDB_env *env, MDB_envinfo *stat);
i32 mdb_env_sync(MDB_env *env, i32 force);
void mdb_env_close(MDB_env *env);
i32 mdb_env_set_flags(MDB_env *env, u32 flags, i32 onoff);
i32 mdb_env_get_flags(MDB_env *env, u32 *flags);
i32 mdb_env_get_path(MDB_env *env, const u8 **path);
i32 mdb_env_get_fd(MDB_env *env, mdb_filehandle_t *fd);
i32 mdb_env_set_mapsize(MDB_env *env, mdb_size_t size);
i32 mdb_env_set_maxreaders(MDB_env *env, u32 readers);
i32 mdb_env_get_maxreaders(MDB_env *env, u32 *readers);
i32 mdb_env_set_maxdbs(MDB_env *env, MDB_dbi dbs);
i32 mdb_env_get_maxkeysize(MDB_env *env);
i32 mdb_env_set_userctx(MDB_env *env, void *ctx);
void *mdb_env_get_userctx(MDB_env *env);
i32 mdb_env_set_assert(MDB_env *env, MDB_assert_func *func);
i32 mdb_txn_begin(MDB_env *env, MDB_txn *parent, u32 flags, MDB_txn **txn);
MDB_env *mdb_txn_env(MDB_txn *txn);
mdb_size_t mdb_txn_id(MDB_txn *txn);
i32 mdb_txn_commit(MDB_txn *txn);
void mdb_txn_abort(MDB_txn *txn);
void mdb_txn_reset(MDB_txn *txn);
i32 mdb_txn_renew(MDB_txn *txn);

i32 mdb_dbi_open(MDB_txn *txn, const u8 *name, u32 flags, MDB_dbi *dbi);
i32 mdb_stat(MDB_txn *txn, MDB_dbi dbi, MDB_stat *stat);
i32 mdb_dbi_flags(MDB_txn *txn, MDB_dbi dbi, u32 *flags);
void mdb_dbi_close(MDB_env *env, MDB_dbi dbi);
i32 mdb_drop(MDB_txn *txn, MDB_dbi dbi, i32 del);
i32 mdb_set_compare(MDB_txn *txn, MDB_dbi dbi, MDB_cmp_func *cmp);
i32 mdb_set_dupsort(MDB_txn *txn, MDB_dbi dbi, MDB_cmp_func *cmp);
i32 mdb_set_relfunc(MDB_txn *txn, MDB_dbi dbi, MDB_rel_func *rel);
i32 mdb_set_relctx(MDB_txn *txn, MDB_dbi dbi, void *ctx);
i32 mdb_get(MDB_txn *txn, MDB_dbi dbi, MDB_val *key, MDB_val *data);
i32 mdb_put(MDB_txn *txn, MDB_dbi dbi, MDB_val *key, MDB_val *data, u32 flags);

i32 mdb_del(MDB_txn *txn, MDB_dbi dbi, MDB_val *key, MDB_val *data);
i32 mdb_cursor_open(MDB_txn *txn, MDB_dbi dbi, MDB_cursor **cursor);
void mdb_cursor_close(MDB_cursor *cursor);
i32 mdb_cursor_renew(MDB_txn *txn, MDB_cursor *cursor);
MDB_txn *mdb_cursor_txn(MDB_cursor *cursor);

MDB_dbi mdb_cursor_dbi(MDB_cursor *cursor);

i32 mdb_cursor_get(MDB_cursor *cursor, MDB_val *key, MDB_val *data,
		   MDB_cursor_op op);

i32 mdb_cursor_put(MDB_cursor *cursor, MDB_val *key, MDB_val *data, u32 flags);

i32 mdb_cursor_del(MDB_cursor *cursor, u32 flags);
i32 mdb_cursor_count(MDB_cursor *cursor, mdb_size_t *countp);
i32 mdb_cmp(MDB_txn *txn, MDB_dbi dbi, const MDB_val *a, const MDB_val *b);
i32 mdb_dcmp(MDB_txn *txn, MDB_dbi dbi, const MDB_val *a, const MDB_val *b);

i32 mdb_reader_list(MDB_env *env, MDB_msg_func *func, void *ctx);
i32 mdb_reader_check(MDB_env *env, i32 *dead);

#endif /* _LMDB_H */

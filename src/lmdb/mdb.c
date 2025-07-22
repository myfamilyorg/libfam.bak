/********************************************************************************
 * MIT License
 *
 * Copyright (c) 2025 Christopher Gilliard
 *
 * Permission is hereby granted, free of usage, to any person obtaining a copy
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

#include <libfam/alloc.H>
#include <libfam/error.H>
#include <libfam/format.H>
#include <libfam/lmdb.H>
#include <libfam/midl.H>
#include <libfam/misc.H>
#include <libfam/pthread.H>
#include <libfam/types.H>

#if defined(__i386) || defined(__x86_64) || defined(_M_IX86)
#define MISALIGNED_OK
#endif

#define MDB_DEVEL 0
#define MDB_NO_ROOT (MDB_LAST_ERRCODE + 10)
#define LOCK_MUTEX0(mutex) pthread_mutex_lock(mutex)
#define UNLOCK_MUTEX(mutex) pthread_mutex_unlock(mutex)
#define mdb_mutex_consistent(mutex) pthread_mutex_consistent(mutex)
#define ErrCode() err
#define HANDLE i32
#define INVALID_HANDLE_VALUE (-1)
#define GET_PAGESIZE(x) ((x) = PAGE_SIZE)
#define MNAME_LEN (sizeof(pthread_mutex_t))
#define LOCK_MUTEX(rc, env, mutex) ((rc) = LOCK_MUTEX0(mutex))
#define mdb_mutex_failed(env, mutex, rc) (rc)
#define MDB_DSYNC O_SYNC
#define MS_SYNC 1
#define MS_ASYNC 0

#define MAX_PAGESIZE (PAGEBASE ? 0x10000 : 0x8000)
#define MDB_MINKEYS 2
#define MDB_MAGIC 0xBEEFC0DE
#define MDB_DATA_VERSION ((MDB_DEVEL) ? 999 : 1)
#define MDB_LOCK_VERSION ((MDB_DEVEL) ? 999 : 2)
#define MDB_LOCK_VERSION_BITS 12
#define MDB_MAXKEYSIZE ((MDB_DEVEL) ? 0 : 511)
#define ENV_MAXKEY(env) (MDB_MAXKEYSIZE)
#define MAXDATASIZE 0xffffffffUL
#define P_INVALID (~(u64)0)
#define F_ISSET(w, f) (((w) & (f)) == (f))
#define EVEN(n) (((n) + 1U) & -2)
#define LOW_BIT(n) ((n) & (-(n)))
#define LOG2_MOD(p2, n) (7 - 86 / ((p2) % ((1U << (n)) - 1) + 11))
#define ALIGNOF2(type)           \
	LOW_BIT(offsetof(        \
	    struct {             \
		    u8 ch_;      \
		    type align_; \
	    },                   \
	    align_))
#define DEFAULT_MAPSIZE 1048576
#define DEFAULT_READERS 126
#define CACHELINE 64
#define MDB_LOCK_FORMAT                                               \
	((u32)(((MDB_LOCK_VERSION) % (1U << MDB_LOCK_VERSION_BITS)) + \
	       MDB_lock_desc * (1U << MDB_LOCK_VERSION_BITS)))
#define MDB_LOCK_TYPE                                  \
	(10 + LOG2_MOD(ALIGNOF2(pthread_mutex_t), 5) + \
	 sizeof(pthread_mutex_t) / 4U % 22 * 5)
#define MP_PGNO(p) (((MDB_page2 *)(void *)(p))->mp2_p)
#define MP_PAD(p) (((MDB_page2 *)(void *)(p))->mp2_pad)
#define MP_FLAGS(p) (((MDB_page2 *)(void *)(p))->mp2_flags)
#define MP_LOWER(p) (((MDB_page2 *)(void *)(p))->mp2_lower)
#define MP_UPPER(p) (((MDB_page2 *)(void *)(p))->mp2_upper)
#define MP_PTRS(p) (((MDB_page2 *)(void *)(p))->mp2_ptrs)

#define PAGEHDRSZ sizeof(MDB_page)
#define METADATA(p) ((void *)((u8 *)(p) + PAGEHDRSZ))
#define PAGEBASE ((MDB_DEVEL) ? PAGEHDRSZ : 0)
#define NUMKEYS(p) ((MP_LOWER(p) - (PAGEHDRSZ - PAGEBASE)) >> 1)
#define SIZELEFT(p) (u16)(MP_UPPER(p) - MP_LOWER(p))
#define PAGEFILL(env, p)                                       \
	(1000L * ((env)->me_psize - PAGEHDRSZ - SIZELEFT(p)) / \
	 ((env)->me_psize - PAGEHDRSZ))
#define FILL_THRESHOLD 250
#define IS_LEAF(p) F_ISSET(MP_FLAGS(p), P_LEAF)
#define IS_LEAF2(p) F_ISSET(MP_FLAGS(p), P_LEAF2)
#define IS_BRANCH(p) F_ISSET(MP_FLAGS(p), P_BRANCH)
#define IS_OVERFLOW(p) F_ISSET(MP_FLAGS(p), P_OVERFLOW)
#define IS_SUBP(p) F_ISSET(MP_FLAGS(p), P_SUBP)
#define OVPAGES(size, psize) ((PAGEHDRSZ - 1 + (size)) / (psize) + 1)
#define NEXT_LOOSE_PAGE(p) (*(MDB_page **)((p) + 2))
#define NODESIZE offsetof(MDB_node, mn_data)
#define PGNO_TOPWORD ((u64) - 1 > 0xffffffffu ? 32 : 0)
#define INDXSIZE(k) (NODESIZE + ((k) == NULL ? 0 : (k)->mv_size))
#define LEAFSIZE(k, d) (NODESIZE + (k)->mv_size + (d)->mv_size)
#define NODEPTR(p, i) ((MDB_node *)((u8 *)(p) + MP_PTRS(p)[i] + PAGEBASE))
#define NODEKEY(node) (void *)((node)->mn_data)
#define NODEDATA(node) (void *)((u8 *)(node)->mn_data + (node)->mn_ksize)
#define NODEPGNO(node)                                \
	((node)->mn_lo | ((u64)(node)->mn_hi << 16) | \
	 (PGNO_TOPWORD ? ((u64)(node)->mn_flags << PGNO_TOPWORD) : 0))
#define SETPGNO(node, pgno)                                                  \
	do {                                                                 \
		(node)->mn_lo = (pgno) & 0xffff;                             \
		(node)->mn_hi = (pgno) >> 16;                                \
		if (PGNO_TOPWORD) (node)->mn_flags = (pgno) >> PGNO_TOPWORD; \
	} while (0)
#define NODEDSZ(node) ((node)->mn_lo | ((u32)(node)->mn_hi << 16))
#define SETDSZ(node, size)                       \
	do {                                     \
		(node)->mn_lo = (size) & 0xffff; \
		(node)->mn_hi = (size) >> 16;    \
	} while (0)
#define NODEKSZ(node) ((node)->mn_ksize)
#ifdef MISALIGNED_OK
#define COPY_PGNO(dst, src) dst = src
#undef MP_PGNO
#define MP_PGNO(p) ((p)->mp_pgno)
#else
#define COPY_PGNO(dst, src)        \
	do {                       \
		u16 *s, *d;        \
		s = (u16 *)&(src); \
		d = (u16 *)&(dst); \
		*d++ = *s++;       \
		*d = *s;           \
	} while (0)
#endif
#define LEAF2KEY(p, i, ks) ((u8 *)(p) + PAGEHDRSZ + ((i) * (ks)))
#define MDB_GET_KEY(node, keyptr)                          \
	{                                                  \
		if ((keyptr) != NULL) {                    \
			(keyptr)->mv_size = NODEKSZ(node); \
			(keyptr)->mv_data = NODEKEY(node); \
		}                                          \
	}
#define MDB_GET_KEY2(node, key)              \
	{                                    \
		key.mv_size = NODEKSZ(node); \
		key.mv_data = NODEKEY(node); \
	}
#define MDB_VALID 0x8000
#define PERSISTENT_FLAGS (0xffff & ~(MDB_VALID))
#define VALID_FLAGS                                                     \
	(MDB_REVERSEKEY | MDB_DUPSORT | MDB_INTEGERKEY | MDB_DUPFIXED | \
	 MDB_INTEGERDUP | MDB_REVERSEDUP | MDB_CREATE)
#define FREE_DBI 0
#define MAIN_DBI 1
#define CORE_DBS 2
#define NUM_METAS 2
#define CURSOR_STACK 32
#define XCURSOR_INITED(mc)   \
	((mc)->mc_xcursor && \
	 ((mc)->mc_xcursor->mx_cursor.mc_flags & C_INITIALIZED))
#define XCURSOR_REFRESH(mc, top, mp)                                           \
	do {                                                                   \
		MDB_page *xr_pg = (mp);                                        \
		MDB_node *xr_node;                                             \
		if (!XCURSOR_INITED(mc) || (mc)->mc_ki[top] >= NUMKEYS(xr_pg)) \
			break;                                                 \
		xr_node = NODEPTR(xr_pg, (mc)->mc_ki[top]);                    \
		if ((xr_node->mn_flags & (F_DUPDATA | F_SUBDATA)) ==           \
		    F_DUPDATA)                                                 \
			(mc)->mc_xcursor->mx_cursor.mc_pg[0] =                 \
			    NODEDATA(xr_node);                                 \
	} while (0)
#define MDB_COMMIT_PAGES 64
#if defined(IOV_MAX) && IOV_MAX < MDB_COMMIT_PAGES
#undef MDB_COMMIT_PAGES
#define MDB_COMMIT_PAGES IOV_MAX
#endif
#define MAX_WRITE (0x40000000U >> (sizeof(i64) == 4))
#define TXN_DBI_EXIST(txn, dbi, validity)     \
	((txn) && (dbi) < (txn)->mt_numdbs && \
	 ((txn)->mt_dbflags[dbi] & (validity)))
#define TXN_DBI_CHANGED(txn, dbi) \
	((txn)->mt_dbiseqs[dbi] != (txn)->mt_env->me_dbiseqs[dbi])

#define MDB_END_NAMES                                            \
	{"committed", "empty-commit", "abort",		"reset", \
	 "reset-tmp", "fail-begin",   "fail-beginchild"}

#define MDB_END_OPMASK 0x0F
#define MDB_END_UPDATE 0x10
#define MDB_END_FREE 0x20
#define MDB_END_SLOT MDB_NOTLS
#define MDB_PS_MODIFY 1
#define MDB_PS_ROOTONLY 2
#define MDB_PS_FIRST 4
#define MDB_PS_LAST 8
#define MDB_SPLIT_REPLACE MDB_APPENDDUP
#define mdb_env_close0(env, excl) mdb_env_close1(env)

#ifdef MISALIGNED_OK
#define mdb_cmp_clong mdb_cmp_long
#else
#define mdb_cmp_clong mdb_cmp_ci32
#endif

#define NEED_CMP_CLONG(cmp, ksize)                         \
	(U32_MAX < MDB_SIZE_MAX && (cmp) == mdb_cmp_i32 && \
	 (ksize) == sizeof(u64))

#define mdb_cassert(mc, expr) mdb_assert0((mc)->mc_txn->mt_env, expr, #expr)
#define mdb_tassert(txn, expr) mdb_assert0((txn)->mt_env, expr, #expr)
#define mdb_eassert(env, expr) mdb_assert0(env, expr, #expr)
#define mdb_assert0(env, expr, expr_txt) ((void)0)

#define MDB_PAGE_UNREF(txn, mp)
#define MDB_CURSOR_UNREF(mc, force) ((void)0)

#define CHANGEABLE (MDB_NOSYNC | MDB_NOMETASYNC | MDB_MAPASYNC | MDB_NOMEMINIT)
#define CHANGELESS                                                             \
	(MDB_FIXEDMAP | MDB_NOSUBDIR | MDB_RDONLY | MDB_WRITEMAP | MDB_NOTLS | \
	 MDB_NOLOCK | MDB_NORDAHEAD | MDB_PREVSNAPSHOT)

#if VALID_FLAGS & PERSISTENT_FLAGS & (CHANGEABLE | CHANGELESS)
#error "Persistent DB flags & env flags overlap, but both go in mm_flags"
#endif

#define MDB_NOSPILL 0x8000

#define WITH_CURSOR_TRACKING(mn, act)                            \
	do {                                                     \
		MDB_cursor dummy, *tracked,                      \
		    **tp = &(mn).mc_txn->mt_cursors[mn.mc_dbi];  \
		if ((mn).mc_flags & C_SUB) {                     \
			dummy.mc_flags = C_INITIALIZED;          \
			dummy.mc_xcursor = (MDB_xcursor *)&(mn); \
			tracked = &dummy;                        \
		} else {                                         \
			tracked = &(mn);                         \
		}                                                \
		tracked->mc_next = *tp;                          \
		*tp = tracked;                                   \
		{                                                \
			act;                                     \
		}                                                \
		*tp = tracked->mc_next;                          \
	} while (0)

#define MDB_NAME(str) str
#define mdb_name_cpy strcpy
#define MDB_SUFFLEN 9

#define mdb_fname_destroy(fname)                                 \
	do {                                                     \
		if ((fname).mn_alloced) release((fname).mn_val); \
	} while (0)

#define MDB_CLOEXEC 0

#ifndef MDB_WBUF
#define MDB_WBUF (1024 * 1024)
#endif
#define MDB_EOF 0x10

#define STATIC_ASSERT(condition, message) \
	typedef u8 STATIC_assert_##message[(condition) ? 1 : -1]

typedef pthread_mutex_t mdb_mutex_t[1];
typedef pthread_mutex_t *mdb_mutexref_t;

typedef struct MDB_rxbody {
	volatile u64 mrb_txnid;
	volatile i32 mrb_pid;
} MDB_rxbody;

typedef struct MDB_reader {
	union {
		MDB_rxbody mrx;
#define mr_txnid mru.mrx.mrb_txnid
#define mr_pid mru.mrx.mrb_pid
		u8 pad[(sizeof(MDB_rxbody) + CACHELINE - 1) & ~(CACHELINE - 1)];
	} mru;
} MDB_reader;

typedef struct MDB_txbody {
	u32 mtb_magic;
	u32 mtb_format;
	volatile u64 mtb_txnid;
	volatile u32 mtb_numreaders;
	mdb_mutex_t mtb_rmutex;
} MDB_txbody;

typedef struct MDB_txninfo {
	union {
		MDB_txbody mtb;
#define mti_magic mt1.mtb.mtb_magic
#define mti_format mt1.mtb.mtb_format
#define mti_rmutex mt1.mtb.mtb_rmutex
#define mti_txnid mt1.mtb.mtb_txnid
#define mti_numreaders mt1.mtb.mtb_numreaders
#define mti_mutexid mt1.mtb.mtb_mutexid
		u8 pad[(sizeof(MDB_txbody) + CACHELINE - 1) & ~(CACHELINE - 1)];
	} mt1;
	union {
		mdb_mutex_t mt2_wmutex;
#define mti_wmutex mt2.mt2_wmutex
		u8 pad[(MNAME_LEN + CACHELINE - 1) & ~(CACHELINE - 1)];
	} mt2;
	MDB_reader mti_readers[1];
} MDB_txninfo;

enum {
	MDB_lock_desc =
	    (CACHELINE == 64 ? 0
			     : 1 + LOG2_MOD(CACHELINE >> (CACHELINE > 64), 5)) +
	    6 * (sizeof(i32) / 4 % 3) + 18 * (sizeof(pthread_t) / 4 % 5) +
	    90 * (sizeof(MDB_txbody) / CACHELINE % 3) +
	    270 * (MDB_LOCK_TYPE % 120) + ((sizeof(u64) == 8) << 15) +
	    ((sizeof(MDB_reader) > CACHELINE) << 16) + ((1 != 0) << 17)
};

typedef struct MDB_page {
#define mp_pgno mp_p.p_pgno
#define mp_next mp_p.p_next
	union {
		u64 p_pgno;
		struct MDB_page *p_next;
	} mp_p;
	u16 mp_pad;
#define P_BRANCH 0x01
#define P_LEAF 0x02
#define P_OVERFLOW 0x04
#define P_META 0x08
#define P_DIRTY 0x10
#define P_LEAF2 0x20
#define P_SUBP 0x40
#define P_LOOSE 0x4000
#define P_KEEP 0x8000
	u16 mp_flags;
#define mp_lower mp_pb.pb.pb_lower
#define mp_upper mp_pb.pb.pb_upper
#define mp_pages mp_pb.pb_pages
	union {
		struct {
			u16 pb_lower;
			u16 pb_upper;
		} pb;
		u32 pb_pages;
	} mp_pb;
	u16 mp_ptrs[0];
} MDB_page;

typedef struct MDB_page2 {
	u16 mp2_p[sizeof(u64) / 2];
	u16 mp2_pad;
	u16 mp2_flags;
	u16 mp2_lower;
	u16 mp2_upper;
	u16 mp2_ptrs[0];
} MDB_page2;

typedef struct MDB_node {
	u16 mn_lo, mn_hi;
#define F_BIGDATA 0x01
#define F_SUBDATA 0x02
#define F_DUPDATA 0x04
#define NODE_ADD_FLAGS (F_DUPDATA | F_SUBDATA | MDB_RESERVE | MDB_APPEND)
	u16 mn_flags;
	u16 mn_ksize;
	u8 mn_data[1];
} MDB_node;

typedef struct MDB_db {
	u32 md_pad;
	u16 md_flags;
	u16 md_depth;
	u64 md_branch_pages;
	u64 md_leaf_pages;
	u64 md_overflow_pages;
	u64 md_entries;
	u64 md_root;
} MDB_db;

typedef struct MDB_meta {
	u32 mm_magic;
	u32 mm_version;
	void *mm_address;
	u64 mm_mapsize;
	MDB_db mm_dbs[CORE_DBS];
#define mm_psize mm_dbs[FREE_DBI].md_pad
#define mm_flags mm_dbs[FREE_DBI].md_flags
	u64 mm_last_pg;
	volatile u64 mm_txnid;
} MDB_meta;

typedef union MDB_metabuf {
	MDB_page mb_page;
	struct {
		u8 mm_pad[PAGEHDRSZ];
		MDB_meta mm_meta;
	} mb_metabuf;
} MDB_metabuf;

typedef struct MDB_dbx {
	MDB_val md_name;
	MDB_cmp_func *md_cmp;
	MDB_cmp_func *md_dcmp;
	MDB_rel_func *md_rel;
	void *md_relctx;
} MDB_dbx;

struct MDB_txn {
	MDB_txn *mt_parent;
	MDB_txn *mt_child;
	u64 mt_next_pgno;
	u64 mt_txnid;
	MDB_env *mt_env;
	MDB_IDL mt_free_pgs;
	MDB_page *mt_loose_pgs;
	i32 mt_loose_count;
	MDB_IDL mt_spill_pgs;
	union {
		MDB_ID2L dirty_list;
		MDB_reader *reader;
	} mt_u;
	MDB_dbx *mt_dbxs;
	MDB_db *mt_dbs;
	u32 *mt_dbiseqs;
#define DB_DIRTY 0x01
#define DB_STALE 0x02
#define DB_NEW 0x04
#define DB_VALID 0x08
#define DB_USRVALID 0x10
#define DB_DUPDATA 0x20
	MDB_cursor **mt_cursors;
	u8 *mt_dbflags;
	MDB_dbi mt_numdbs;
#define MDB_TXN_BEGIN_FLAGS (MDB_NOMETASYNC | MDB_NOSYNC | MDB_RDONLY)
#define MDB_TXN_NOMETASYNC MDB_NOMETASYNC
#define MDB_TXN_NOSYNC MDB_NOSYNC
#define MDB_TXN_RDONLY MDB_RDONLY
#define MDB_TXN_WRITEMAP MDB_WRITEMAP
#define MDB_TXN_FINISHED 0x01
#define MDB_TXN_ERROR 0x02
#define MDB_TXN_DIRTY 0x04
#define MDB_TXN_SPILLS 0x08
#define MDB_TXN_HAS_CHILD 0x10
#define MDB_TXN_BLOCKED (MDB_TXN_FINISHED | MDB_TXN_ERROR | MDB_TXN_HAS_CHILD)
	u32 mt_flags;
	u32 mt_dirty_room;
};

struct MDB_xcursor;

struct MDB_cursor {
	MDB_cursor *mc_next;
	MDB_cursor *mc_backup;
	struct MDB_xcursor *mc_xcursor;
	MDB_txn *mc_txn;
	MDB_dbi mc_dbi;
	MDB_db *mc_db;
	MDB_dbx *mc_dbx;
	u8 *mc_dbflag;
	u16 mc_snum;
	u16 mc_top;
#define C_INITIALIZED 0x01
#define C_EOF 0x02
#define C_SUB 0x04
#define C_DEL 0x08
#define C_UNTRACK 0x40
#define C_WRITEMAP MDB_TXN_WRITEMAP
#define C_ORIG_RDONLY MDB_TXN_RDONLY
	u32 mc_flags;
	MDB_page *mc_pg[CURSOR_STACK];
	u16 mc_ki[CURSOR_STACK];
#define MC_OVPG(mc) ((MDB_page *)0)
#define MC_SET_OVPG(mc, pg) ((void)0)
};

typedef struct MDB_xcursor {
	MDB_cursor mx_cursor;
	MDB_db mx_db;
	MDB_dbx mx_dbx;
	u8 mx_dbflag;
} MDB_xcursor;

typedef struct MDB_pgstate {
	u64 *mf_pghead;
	u64 mf_pglast;
} MDB_pgstate;

struct MDB_env {
	HANDLE me_fd;
	HANDLE me_lfd;
	HANDLE me_mfd;
#define MDB_FATAL_ERROR 0x80000000U
#define MDB_ENV_ACTIVE 0x20000000U
#define MDB_ENV_TXKEY 0x10000000U
#define MDB_FSYNCONLY 0x08000000U
	u32 me_flags;
	u32 me_psize;
	u32 me_os_psize;
	u32 me_maxreaders;
	volatile i32 me_close_readers;
	MDB_dbi me_numdbs;
	MDB_dbi me_maxdbs;
	i32 me_pid;
	u8 *me_path;
	u8 *me_map;
	MDB_txninfo *me_txns;
	MDB_meta *me_metas[NUM_METAS];
	void *me_pbuf;
	MDB_txn *me_txn;
	MDB_txn *me_txn0;
	u64 me_mapsize;
	i64 me_size;
	u64 me_maxpg;
	MDB_dbx *me_dbxs;
	u16 *me_dbflags;
	u32 *me_dbiseqs;
	pthread_key_t me_txkey;
	u64 me_pgoldest;
	MDB_pgstate me_pgstate;
#define me_pglast me_pgstate.mf_pglast
#define me_pghead me_pgstate.mf_pghead
	MDB_page *me_dpages;
	MDB_IDL me_free_pgs;
	MDB_ID2L me_dirty_list;
	i32 me_maxfree_1pg;
	u32 me_nodemax;
#if !(MDB_MAXKEYSIZE)
	u32 me_maxkey;
#endif
	i32 me_live_reader;
#define me_rmutex me_txns->mti_rmutex
#define me_wmutex me_txns->mti_wmutex
	void *me_userctx;
	MDB_assert_func *me_assert_func;
};

typedef struct MDB_ntxn {
	MDB_txn mnt_txn;
	MDB_pgstate mnt_pgstate;
} MDB_ntxn;

enum {
	MDB_END_COMMITTED,
	MDB_END_EMPTY_COMMIT,
	MDB_END_ABORT,
	MDB_END_RESET,
	MDB_END_RESET_TMP,
	MDB_END_FAIL_BEGIN,
	MDB_END_FAIL_BEGINCHILD
};

enum Pidlock_op { Pidset = F_SETLK, Pidcheck = F_GETLK };

enum mdb_fopen_type {
	MDB_O_RDONLY = O_RDONLY,
	MDB_O_RDWR = O_RDWR | O_CREAT,
	MDB_O_META = O_WRONLY | MDB_DSYNC | MDB_CLOEXEC,
	MDB_O_COPY = O_WRONLY | O_CREAT | O_EXCL | MDB_CLOEXEC,
	MDB_O_MASK =
	    MDB_O_RDWR | MDB_CLOEXEC | MDB_O_RDONLY | MDB_O_META | MDB_O_COPY,
	MDB_O_LOCKS =
	    MDB_O_RDWR | MDB_CLOEXEC | ((MDB_O_MASK + 1) & ~MDB_O_MASK)
};

typedef u8 mdb_nchar_t;

typedef struct MDB_name {
	i32 mn_len;
	i32 mn_alloced;
	mdb_nchar_t *mn_val;
} MDB_name;

STATIC const mdb_nchar_t *const mdb_suffixes[2][2] = {
    {MDB_NAME("/data.mdb"), MDB_NAME("")},
    {MDB_NAME("/lock.mdb"), MDB_NAME("-lock")}};

STATIC u8 *const mdb_errstr[] = {
    "MDB_KEYEXIST: Key/data pair already exists",
    "MDB_NOTFOUND: No matching key/data pair found",
    "MDB_PAGE_NOTFOUND: Requested page not found",
    "MDB_CORRUPTED: Located page was wrong type",
    "MDB_PANIC: Update of meta page failed or environment had fatal error",
    "MDB_VERSION_MISMATCH: Database environment version mismatch",
    "MDB_INVALID: File is not an LMDB file",
    "MDB_MAP_FULL: Environment mapsize limit reached",
    "MDB_DBS_FULL: Environment maxdbs limit reached",
    "MDB_READERS_FULL: Environment maxreaders limit reached",
    "MDB_TLS_FULL: Thread-local storage keys full - too many environments open",
    "MDB_TXN_FULL: Transaction has too many dirty pages - transaction too big",
    "MDB_CURSOR_FULL: Internal error - cursor stack limit reached",
    "MDB_PAGE_FULL: Internal error - page has no more space",
    "MDB_MAP_RESIZED: Database contents grew beyond environment mapsize",
    "MDB_INCOMPATIBLE: Operation and DB incompatible, or DB flags changed",
    "MDB_BAD_RSLOT: Invalid reuse of reader locktable slot",
    "MDB_BAD_TXN: Transaction must abort, has a child, or is invalid",
    "MDB_BAD_VALSIZE: Unsupported size of key/DB name/data, or wrong DUPFIXED",
    "MDB_BAD_DBI: The specified DBI handle was closed/changed unexpectedly",
    "MDB_PROBLEM: Unexpected problem - txn should abort",
};

STATIC_ASSERT(sizeof(MDB_page) == 16, MDB_page_size_16);

STATIC i32 mdb_page_alloc(MDB_cursor *mc, i32 num, MDB_page **mp);
STATIC i32 mdb_page_new(MDB_cursor *mc, u32 flags, i32 num, MDB_page **mp);
STATIC i32 mdb_page_touch(MDB_cursor *mc);
STATIC void mdb_txn_end(MDB_txn *txn, u32 mode);

STATIC i32 mdb_page_get(MDB_cursor *mc, u64 pgno, MDB_page **mp, i32 *lvl);
STATIC i32 mdb_page_search_root(MDB_cursor *mc, MDB_val *key, i32 modify);
STATIC i32 mdb_page_search(MDB_cursor *mc, MDB_val *key, i32 flags);
STATIC i32 mdb_page_merge(MDB_cursor *csrc, MDB_cursor *cdst);
STATIC i32 mdb_page_split(MDB_cursor *mc, MDB_val *newkey, MDB_val *newdata,
			  u64 newpgno, u32 nflags);

STATIC i32 mdb_env_read_header(MDB_env *env, i32 prev, MDB_meta *meta);
STATIC MDB_meta *mdb_env_pick_meta(const MDB_env *env);
STATIC i32 mdb_env_write_meta(MDB_txn *txn);
STATIC void mdb_env_close0(MDB_env *env, i32 excl);

STATIC MDB_node *mdb_node_search(MDB_cursor *mc, MDB_val *key, i32 *exactp);
STATIC i32 mdb_node_add(MDB_cursor *mc, u16 indx, MDB_val *key, MDB_val *data,
			u64 pgno, u32 flags);
STATIC void mdb_node_del(MDB_cursor *mc, i32 ksize);
STATIC void mdb_node_shrink(MDB_page *mp, u16 indx);
STATIC i32 mdb_node_move(MDB_cursor *csrc, MDB_cursor *cdst, i32 fromleft);
STATIC i32 mdb_node_read(MDB_cursor *mc, MDB_node *leaf, MDB_val *data);
STATIC u64 mdb_leaf_size(MDB_env *env, MDB_val *key, MDB_val *data);
STATIC u64 mdb_branch_size(MDB_env *env, MDB_val *key);

STATIC i32 mdb_rebalance(MDB_cursor *mc);
STATIC i32 mdb_update_key(MDB_cursor *mc, MDB_val *key);

STATIC void mdb_cursor_copy(const MDB_cursor *csrc, MDB_cursor *cdst);
STATIC void mdb_cursor_pop(MDB_cursor *mc);
STATIC i32 mdb_cursor_push(MDB_cursor *mc, MDB_page *mp);
STATIC i32 _mdb_cursor_del(MDB_cursor *mc, u32 flags);
STATIC i32 _mdb_cursor_put(MDB_cursor *mc, MDB_val *key, MDB_val *data,
			   u32 flags);

STATIC i32 mdb_cursor_del0(MDB_cursor *mc);
STATIC i32 mdb_del0(MDB_txn *txn, MDB_dbi dbi, MDB_val *key, MDB_val *data,
		    u32 flags);
STATIC i32 mdb_cursor_sibling(MDB_cursor *mc, i32 move_right);
STATIC i32 mdb_cursor_next(MDB_cursor *mc, MDB_val *key, MDB_val *data,
			   MDB_cursor_op op);
STATIC i32 mdb_cursor_prev(MDB_cursor *mc, MDB_val *key, MDB_val *data,
			   MDB_cursor_op op);
STATIC i32 mdb_cursor_set(MDB_cursor *mc, MDB_val *key, MDB_val *data,
			  MDB_cursor_op op, i32 *exactp);
STATIC i32 mdb_cursor_first(MDB_cursor *mc, MDB_val *key, MDB_val *data);
STATIC i32 mdb_cursor_last(MDB_cursor *mc, MDB_val *key, MDB_val *data);

STATIC void mdb_cursor_init(MDB_cursor *mc, MDB_txn *txn, MDB_dbi dbi,
			    MDB_xcursor *mx);
STATIC void mdb_xcursor_init0(MDB_cursor *mc);
STATIC void mdb_xcursor_init1(MDB_cursor *mc, MDB_node *node);
STATIC void mdb_xcursor_init2(MDB_cursor *mc, MDB_xcursor *src_mx, i32 force);

STATIC i32 mdb_drop0(MDB_cursor *mc, i32 subs);
STATIC void mdb_default_cmp(MDB_txn *txn, MDB_dbi dbi);
STATIC i32 mdb_reader_check0(MDB_env *env, i32 rlocked, i32 *dead);
STATIC i32 mdb_page_flush(MDB_txn *txn, i32 keep);

STATIC MDB_cmp_func mdb_cmp_memn, mdb_cmp_memnr, mdb_cmp_i32, mdb_cmp_ci32,
    mdb_cmp_long;

u8 *mdb_version(i32 *major, i32 *minor, i32 *patch) {
	if (major) *major = MDB_VERSION_MAJOR;
	if (minor) *minor = MDB_VERSION_MINOR;
	if (patch) *patch = MDB_VERSION_PATCH;
	return MDB_VERSION_STRING;
}

const u8 *mdb_strerror(i32 err) {
	i32 i;
	if (!err) return ("Successful return: 0");

	if (err >= MDB_KEYEXIST && err <= MDB_LAST_ERRCODE) {
		i = err - MDB_KEYEXIST;
		return mdb_errstr[i];
	}

	if (err < 0) return "Invalid error code";
	return strerror(err);
}

i32 mdb_cmp(MDB_txn *txn, MDB_dbi dbi, const MDB_val *a, const MDB_val *b) {
	return txn->mt_dbxs[dbi].md_cmp(a, b);
}

i32 mdb_dcmp(MDB_txn *txn, MDB_dbi dbi, const MDB_val *a, const MDB_val *b) {
	MDB_cmp_func *dcmp = txn->mt_dbxs[dbi].md_dcmp;
	if (NEED_CMP_CLONG(dcmp, a->mv_size)) dcmp = mdb_cmp_clong;
	return dcmp(a, b);
}

STATIC MDB_page *mdb_page_malloc(MDB_txn *txn, u32 num) {
	MDB_env *env = txn->mt_env;
	MDB_page *ret = env->me_dpages;
	u64 psize = env->me_psize, sz = psize, off;
	if (num == 1) {
		if (ret) {
			env->me_dpages = ret->mp_next;
			return ret;
		}
		psize -= off = PAGEHDRSZ;
	} else {
		sz *= num;
		off = sz - psize;
	}
	if ((ret = alloc(sz)) != NULL) {
		if (!(env->me_flags & MDB_NOMEMINIT)) {
			memset((u8 *)ret + off, 0, psize);
			ret->mp_pad = 0;
		}
	} else {
		txn->mt_flags |= MDB_TXN_ERROR;
	}
	return ret;
}

STATIC void mdb_page_free(MDB_env *env, MDB_page *mp) {
	mp->mp_next = env->me_dpages;
	env->me_dpages = mp;
}

STATIC void mdb_dpage_free(MDB_env *env, MDB_page *dp) {
	if (!IS_OVERFLOW(dp) || dp->mp_pages == 1) {
		mdb_page_free(env, dp);
	} else
		release(dp);
}

STATIC void mdb_dlist_free(MDB_txn *txn) {
	MDB_env *env = txn->mt_env;
	MDB_ID2L dl = txn->mt_u.dirty_list;
	u32 i, n = dl[0].mid;

	for (i = 1; i <= n; i++) {
		mdb_dpage_free(env, dl[i].mptr);
	}
	dl[0].mid = 0;
}

STATIC i32 mdb_page_loose(MDB_cursor *mc, MDB_page *mp) {
	i32 loose = 0;
	u64 pgno = mp->mp_pgno;
	MDB_txn *txn = mc->mc_txn;

	if ((mp->mp_flags & P_DIRTY) && mc->mc_dbi != FREE_DBI) {
		if (txn->mt_parent) {
			MDB_ID2 *dl = txn->mt_u.dirty_list;
			if (dl[0].mid) {
				u32 x = mdb_mid2l_search(dl, pgno);
				if (x <= dl[0].mid && dl[x].mid == pgno) {
					if (mp != dl[x].mptr) {
						mc->mc_flags &=
						    ~(C_INITIALIZED | C_EOF);
						txn->mt_flags |= MDB_TXN_ERROR;
						return MDB_PROBLEM;
					}
					loose = 1;
				}
			}
		} else
			loose = 1;
	}
	if (loose) {
		NEXT_LOOSE_PAGE(mp) = txn->mt_loose_pgs;
		txn->mt_loose_pgs = mp;
		txn->mt_loose_count++;
		mp->mp_flags |= P_LOOSE;
	} else {
		i32 rc = mdb_midl_append(&txn->mt_free_pgs, pgno);
		if (rc) return rc;
	}

	return MDB_SUCCESS;
}

STATIC i32 mdb_pages_xkeep(MDB_cursor *mc, u32 pflags, i32 all) {
	enum { Mask = P_SUBP | P_DIRTY | P_LOOSE | P_KEEP };
	MDB_txn *txn = mc->mc_txn;
	MDB_cursor *m3, *m0 = mc;
	MDB_xcursor *mx;
	MDB_page *dp, *mp;
	MDB_node *leaf;
	u32 i, j;
	i32 rc = MDB_SUCCESS, level;

	for (i = txn->mt_numdbs;;) {
		if (mc->mc_flags & C_INITIALIZED) {
			for (m3 = mc;; m3 = &mx->mx_cursor) {
				mp = NULL;
				for (j = 0; j < m3->mc_snum; j++) {
					mp = m3->mc_pg[j];
					if ((mp->mp_flags & Mask) == pflags)
						mp->mp_flags ^= P_KEEP;
				}
				mx = m3->mc_xcursor;
				if (!(mx &&
				      (mx->mx_cursor.mc_flags & C_INITIALIZED)))
					break;
				if (!(mp && (mp->mp_flags & P_LEAF))) break;
				leaf = NODEPTR(mp, m3->mc_ki[j - 1]);
				if (!(leaf->mn_flags & F_SUBDATA)) break;
			}
		}
		mc = mc->mc_next;
		for (; !mc || mc == m0; mc = txn->mt_cursors[--i])
			if (i == 0) goto mark_done;
	}

mark_done:
	if (all) {
		for (i = 0; i < txn->mt_numdbs; i++) {
			if (txn->mt_dbflags[i] & DB_DIRTY) {
				u64 pgno = txn->mt_dbs[i].md_root;
				if (pgno == P_INVALID) continue;
				if ((rc = mdb_page_get(m0, pgno, &dp,
						       &level)) != MDB_SUCCESS)
					break;
				if ((dp->mp_flags & Mask) == pflags &&
				    level <= 1)
					dp->mp_flags ^= P_KEEP;
			}
		}
	}

	return rc;
}

STATIC i32 mdb_page_spill(MDB_cursor *m0, MDB_val *key, MDB_val *data) {
	MDB_txn *txn = m0->mc_txn;
	MDB_page *dp;
	MDB_ID2L dl = txn->mt_u.dirty_list;
	u32 i, j, need;
	i32 rc;

	if (m0->mc_flags & C_SUB) return MDB_SUCCESS;

	i = m0->mc_db->md_depth;
	if (m0->mc_dbi >= CORE_DBS) i += txn->mt_dbs[MAIN_DBI].md_depth;
	if (key)
		i += (LEAFSIZE(key, data) + txn->mt_env->me_psize) /
		     txn->mt_env->me_psize;
	i += i;
	need = i;

	if (txn->mt_dirty_room > i) return MDB_SUCCESS;

	if (!txn->mt_spill_pgs) {
		txn->mt_spill_pgs = mdb_midl_alloc(MDB_IDL_UM_MAX);
		if (!txn->mt_spill_pgs) return ENOMEM;
	} else {
		MDB_IDL sl = txn->mt_spill_pgs;
		u32 num = sl[0];
		j = 0;
		for (i = 1; i <= num; i++) {
			if (!(sl[i] & 1)) sl[++j] = sl[i];
		}
		sl[0] = j;
	}

	if ((rc = mdb_pages_xkeep(m0, P_DIRTY, 1)) != MDB_SUCCESS) goto done;

	if (need < MDB_IDL_UM_MAX / 8) need = MDB_IDL_UM_MAX / 8;

	for (i = dl[0].mid; i && need; i--) {
		MDB_ID pn = dl[i].mid << 1;
		dp = dl[i].mptr;
		if (dp->mp_flags & (P_LOOSE | P_KEEP)) continue;
		if (txn->mt_parent) {
			MDB_txn *tx2;
			for (tx2 = txn->mt_parent; tx2; tx2 = tx2->mt_parent) {
				if (tx2->mt_spill_pgs) {
					j = mdb_midl_search(tx2->mt_spill_pgs,
							    pn);
					if (j <= tx2->mt_spill_pgs[0] &&
					    tx2->mt_spill_pgs[j] == pn) {
						dp->mp_flags |= P_KEEP;
						break;
					}
				}
			}
			if (tx2) continue;
		}
		if ((rc = mdb_midl_append(&txn->mt_spill_pgs, pn))) goto done;
		need--;
	}
	mdb_midl_sort(txn->mt_spill_pgs);

	if ((rc = mdb_page_flush(txn, i)) != MDB_SUCCESS) goto done;
	rc = mdb_pages_xkeep(m0, P_DIRTY | P_KEEP, i);

done:
	txn->mt_flags |= rc ? MDB_TXN_ERROR : MDB_TXN_SPILLS;
	return rc;
}

STATIC u64 mdb_find_oldest(MDB_txn *txn) {
	i32 i;
	u64 mr, oldest = txn->mt_txnid - 1;
	if (txn->mt_env->me_txns) {
		MDB_reader *r = txn->mt_env->me_txns->mti_readers;
		for (i = txn->mt_env->me_txns->mti_numreaders; --i >= 0;) {
			if (r[i].mr_pid) {
				mr = r[i].mr_txnid;
				if (oldest > mr) oldest = mr;
			}
		}
	}
	return oldest;
}

STATIC void mdb_page_dirty(MDB_txn *txn, MDB_page *mp) {
	MDB_ID2 mid;
	i32 __attribute__((unused)) rc, (*insert)(MDB_ID2L, MDB_ID2 *);
	if (txn->mt_flags & MDB_TXN_WRITEMAP)
		insert = mdb_mid2l_append;
	else
		insert = mdb_mid2l_insert;
	mid.mid = mp->mp_pgno;
	mid.mptr = mp;
	rc = insert(txn->mt_u.dirty_list, &mid);
	mdb_tassert(txn, rc == 0);
	txn->mt_dirty_room--;
}

STATIC i32 mdb_page_alloc(MDB_cursor *mc, i32 num, MDB_page **mp) {
	enum { Paranoid = 0, Max_retries = I32_MAX };
	i32 rc, retry = num * 60;
	MDB_txn *txn = mc->mc_txn;
	MDB_env *env = txn->mt_env;
	u64 pgno, *mop = env->me_pghead;
	u32 i, j, mop_len = mop ? mop[0] : 0, n2 = num - 1;
	MDB_page *np;
	u64 oldest = 0, last;
	MDB_cursor_op op;
	MDB_cursor m2;
	i32 found_old = 0;

	if (num == 1 && txn->mt_loose_pgs) {
		np = txn->mt_loose_pgs;
		txn->mt_loose_pgs = NEXT_LOOSE_PAGE(np);
		txn->mt_loose_count--;
		*mp = np;
		return MDB_SUCCESS;
	}

	*mp = NULL;

	if (txn->mt_dirty_room == 0) {
		rc = MDB_TXN_FULL;
		goto fail;
	}

	for (op = MDB_FIRST;; op = MDB_NEXT) {
		MDB_val key, data;
		MDB_node *leaf;
		u64 *idl;

		if (mop_len > n2) {
			i = mop_len;
			do {
				pgno = mop[i];
				if (mop[i - n2] == pgno + n2) goto search_done;
			} while (--i > n2);
			if (--retry < 0) break;
		}

		if (op == MDB_FIRST) {
			last = env->me_pglast;
			oldest = env->me_pgoldest;
			mdb_cursor_init(&m2, txn, FREE_DBI, NULL);
			if (last) {
				op = MDB_SET_RANGE;
				key.mv_data = &last;
				key.mv_size = sizeof(last);
			}
			if (Paranoid && mc->mc_dbi == FREE_DBI) retry = -1;
		}
		if (Paranoid && retry < 0 && mop_len) break;

		last++;
		if (oldest <= last) {
			if (!found_old) {
				oldest = mdb_find_oldest(txn);
				env->me_pgoldest = oldest;
				found_old = 1;
			}
			if (oldest <= last) break;
		}
		rc = mdb_cursor_get(&m2, &key, NULL, op);
		if (rc) {
			if (rc == MDB_NOTFOUND) break;
			goto fail;
		}
		last = *(u64 *)key.mv_data;
		if (oldest <= last) {
			if (!found_old) {
				oldest = mdb_find_oldest(txn);
				env->me_pgoldest = oldest;
				found_old = 1;
			}
			if (oldest <= last) break;
		}
		np = m2.mc_pg[m2.mc_top];
		leaf = NODEPTR(np, m2.mc_ki[m2.mc_top]);
		if ((rc = mdb_node_read(&m2, leaf, &data)) != MDB_SUCCESS)
			goto fail;

		idl = (MDB_ID *)data.mv_data;
		i = idl[0];
		if (!mop) {
			if (!(env->me_pghead = mop = mdb_midl_alloc(i))) {
				rc = ENOMEM;
				goto fail;
			}
		} else {
			if ((rc = mdb_midl_need(&env->me_pghead, i)) != 0)
				goto fail;
			mop = env->me_pghead;
		}
		env->me_pglast = last;
		mdb_midl_xmerge(mop, idl);
		mop_len = mop[0];
	}

	i = 0;
	pgno = txn->mt_next_pgno;
	if (pgno + num >= env->me_maxpg) {
		rc = MDB_MAP_FULL;
		goto fail;
	}

search_done:
	if (env->me_flags & MDB_WRITEMAP) {
		np = (MDB_page *)(env->me_map + env->me_psize * pgno);
	} else {
		if (!(np = mdb_page_malloc(txn, num))) {
			rc = ENOMEM;
			goto fail;
		}
	}
	if (i) {
		mop[0] = mop_len -= num;
		for (j = i - num; j < mop_len;) mop[++j] = mop[++i];
	} else {
		txn->mt_next_pgno = pgno + num;
	}
	np->mp_pgno = pgno;
	mdb_page_dirty(txn, np);
	*mp = np;

	return MDB_SUCCESS;

fail:
	txn->mt_flags |= MDB_TXN_ERROR;
	return rc;
}

STATIC void mdb_page_copy(MDB_page *dst, MDB_page *src, u32 psize) {
	enum { Align = sizeof(u64) };
	u16 upper = src->mp_upper, lower = src->mp_lower,
	    unused = upper - lower;

	if ((unused &= -Align) && !IS_LEAF2(src)) {
		upper = (upper + PAGEBASE) & -Align;
		memcpy(dst, src, (lower + PAGEBASE + (Align - 1)) & -Align);
		memcpy((u64 *)((u8 *)dst + upper), (u64 *)((u8 *)src + upper),
		       psize - upper);
	} else {
		memcpy(dst, src, psize - unused);
	}
}

STATIC i32 mdb_page_unspill(MDB_txn *txn, MDB_page *mp, MDB_page **ret) {
	MDB_env *env = txn->mt_env;
	const MDB_txn *tx2;
	u32 x;
	u64 pgno = mp->mp_pgno, pn = pgno << 1;

	for (tx2 = txn; tx2; tx2 = tx2->mt_parent) {
		if (!tx2->mt_spill_pgs) continue;
		x = mdb_midl_search(tx2->mt_spill_pgs, pn);
		if (x <= tx2->mt_spill_pgs[0] && tx2->mt_spill_pgs[x] == pn) {
			MDB_page *np;
			i32 num;
			if (txn->mt_dirty_room == 0) return MDB_TXN_FULL;
			if (IS_OVERFLOW(mp))
				num = mp->mp_pages;
			else
				num = 1;
			if (env->me_flags & MDB_WRITEMAP) {
				np = mp;
			} else {
				np = mdb_page_malloc(txn, num);
				if (!np) return ENOMEM;
				if (num > 1)
					memcpy(np, mp, num * env->me_psize);
				else
					mdb_page_copy(np, mp, env->me_psize);
			}
			if (tx2 == txn) {
				if (x == txn->mt_spill_pgs[0])
					txn->mt_spill_pgs[0]--;
				else
					txn->mt_spill_pgs[x] |= 1;
			}
			mdb_page_dirty(txn, np);
			np->mp_flags |= P_DIRTY;
			*ret = np;
			break;
		}
	}
	return MDB_SUCCESS;
}

STATIC i32 mdb_page_touch(MDB_cursor *mc) {
	MDB_page *mp = mc->mc_pg[mc->mc_top], *np;
	MDB_txn *txn = mc->mc_txn;
	MDB_cursor *m2, *m3;
	u64 pgno;
	i32 rc;

	if (!F_ISSET(MP_FLAGS(mp), P_DIRTY)) {
		if (txn->mt_flags & MDB_TXN_SPILLS) {
			np = NULL;
			rc = mdb_page_unspill(txn, mp, &np);
			if (rc) goto fail;
			if (np) goto done;
		}
		if ((rc = mdb_midl_need(&txn->mt_free_pgs, 1)) ||
		    (rc = mdb_page_alloc(mc, 1, &np)))
			goto fail;
		pgno = np->mp_pgno;
		mdb_cassert(mc, mp->mp_pgno != pgno);
		mdb_midl_xappend(txn->mt_free_pgs, mp->mp_pgno);
		if (mc->mc_top) {
			MDB_page *parent = mc->mc_pg[mc->mc_top - 1];
			MDB_node *node =
			    NODEPTR(parent, mc->mc_ki[mc->mc_top - 1]);
			SETPGNO(node, pgno);
		} else {
			mc->mc_db->md_root = pgno;
		}
	} else if (txn->mt_parent && !IS_SUBP(mp)) {
		MDB_ID2 mid, *dl = txn->mt_u.dirty_list;
		pgno = mp->mp_pgno;
		if (dl[0].mid) {
			u32 x = mdb_mid2l_search(dl, pgno);
			if (x <= dl[0].mid && dl[x].mid == pgno) {
				if (mp != dl[x].mptr) {
					mc->mc_flags &=
					    ~(C_INITIALIZED | C_EOF);
					txn->mt_flags |= MDB_TXN_ERROR;
					return MDB_PROBLEM;
				}
				return 0;
			}
		}
		mdb_cassert(mc, dl[0].mid < MDB_IDL_UM_MAX);
		np = mdb_page_malloc(txn, 1);
		if (!np) return ENOMEM;
		mid.mid = pgno;
		mid.mptr = np;
		rc = mdb_mid2l_insert(dl, &mid);
		mdb_cassert(mc, rc == 0);
	} else {
		return 0;
	}

	mdb_page_copy(np, mp, txn->mt_env->me_psize);
	np->mp_pgno = pgno;
	np->mp_flags |= P_DIRTY;

done:
	mc->mc_pg[mc->mc_top] = np;
	m2 = txn->mt_cursors[mc->mc_dbi];
	if (mc->mc_flags & C_SUB) {
		for (; m2; m2 = m2->mc_next) {
			m3 = &m2->mc_xcursor->mx_cursor;
			if (m3->mc_snum < mc->mc_snum) continue;
			if (m3->mc_pg[mc->mc_top] == mp)
				m3->mc_pg[mc->mc_top] = np;
		}
	} else {
		for (; m2; m2 = m2->mc_next) {
			if (m2->mc_snum < mc->mc_snum) continue;
			if (m2 == mc) continue;
			if (m2->mc_pg[mc->mc_top] == mp) {
				m2->mc_pg[mc->mc_top] = np;
				if (IS_LEAF(np))
					XCURSOR_REFRESH(m2, mc->mc_top, np);
			}
		}
	}
	MDB_PAGE_UNREF(mc->mc_txn, mp);
	return 0;

fail:
	txn->mt_flags |= MDB_TXN_ERROR;
	return rc;
}

i32 mdb_env_sync0(MDB_env *env, i32 force, u64 numpgs) {
	i32 rc = 0;
	if (env->me_flags & MDB_RDONLY) return EACCES;
	if (force) {
		if (env->me_flags & MDB_WRITEMAP) {
			i32 flags = ((env->me_flags & MDB_MAPASYNC) && !force)
					? MS_ASYNC
					: MS_SYNC;
			if (msync(env->me_map, env->me_psize * numpgs, flags))
				rc = ErrCode();
		} else {
			if (fdatasync(env->me_fd)) rc = ErrCode();
		}
	}
	return rc;
}

i32 mdb_env_sync(MDB_env *env, i32 force) {
	MDB_meta *m = mdb_env_pick_meta(env);
	return mdb_env_sync0(env, force, m->mm_last_pg + 1);
}

STATIC i32 mdb_cursor_shadow(MDB_txn *src, MDB_txn *dst) {
	MDB_cursor *mc, *bk;
	MDB_xcursor *mx;
	u64 size;
	i32 i;

	for (i = src->mt_numdbs; --i >= 0;) {
		if ((mc = src->mt_cursors[i]) != NULL) {
			size = sizeof(MDB_cursor);
			if (mc->mc_xcursor) size += sizeof(MDB_xcursor);
			for (; mc; mc = bk->mc_next) {
				bk = alloc(size);
				if (!bk) return ENOMEM;
				*bk = *mc;
				mc->mc_backup = bk;
				mc->mc_db = &dst->mt_dbs[i];
				mc->mc_txn = dst;
				mc->mc_dbflag = &dst->mt_dbflags[i];
				if ((mx = mc->mc_xcursor) != NULL) {
					*(MDB_xcursor *)(bk + 1) = *mx;
					mx->mx_cursor.mc_txn = dst;
				}
				mc->mc_next = dst->mt_cursors[i];
				dst->mt_cursors[i] = mc;
			}
		}
	}
	return MDB_SUCCESS;
}

STATIC void mdb_cursors_close(MDB_txn *txn, u32 merge) {
	MDB_cursor **cursors = txn->mt_cursors, *mc, *next, *bk;
	MDB_xcursor *mx;
	i32 i;

	for (i = txn->mt_numdbs; --i >= 0;) {
		for (mc = cursors[i]; mc; mc = next) {
			next = mc->mc_next;
			if ((bk = mc->mc_backup) != NULL) {
				if (merge) {
					mc->mc_next = bk->mc_next;
					mc->mc_backup = bk->mc_backup;
					mc->mc_txn = bk->mc_txn;
					mc->mc_db = bk->mc_db;
					mc->mc_dbflag = bk->mc_dbflag;
					if ((mx = mc->mc_xcursor) != NULL)
						mx->mx_cursor.mc_txn =
						    bk->mc_txn;
				} else {
					*mc = *bk;
					if ((mx = mc->mc_xcursor) != NULL)
						*mx = *(MDB_xcursor *)(bk + 1);
				}
				mc = bk;
			}
			release(mc);
		}
		cursors[i] = NULL;
	}
}

STATIC i32 mdb_reader_pid(MDB_env *env, enum Pidlock_op op, i32 pid) {
	for (;;) {
		i32 rc;
		struct flock lock_info;
		memset(&lock_info, 0, sizeof(lock_info));
		lock_info.l_type = F_WRLCK;
		lock_info.l_whence = SEEK_SET;
		lock_info.l_start = pid;
		lock_info.l_len = 1;
		if ((rc = fcntl(env->me_lfd, op, &lock_info)) == 0) {
			if (op == F_GETLK && lock_info.l_type != F_UNLCK)
				rc = -1;
		} else if ((rc = ErrCode()) == EINTR) {
			continue;
		}
		return rc;
	}
}

STATIC i32 mdb_txn_renew0(MDB_txn *txn) {
	MDB_env *env = txn->mt_env;
	MDB_txninfo *ti = env->me_txns;
	MDB_meta *meta;
	u32 i, nr, flags = txn->mt_flags;
	u16 x;
	i32 rc, new_notls = 0;

	if ((flags &= MDB_TXN_RDONLY) != 0) {
		if (!ti) {
			meta = mdb_env_pick_meta(env);
			txn->mt_txnid = meta->mm_txnid;
			txn->mt_u.reader = NULL;
		} else {
			MDB_reader *r =
			    (env->me_flags & MDB_NOTLS)
				? txn->mt_u.reader
				: pthread_getspecific(env->me_txkey);
			if (r) {
				if (r->mr_pid != env->me_pid ||
				    r->mr_txnid != (u64)-1)
					return MDB_BAD_RSLOT;
			} else {
				i32 pid = env->me_pid;
				mdb_mutexref_t rmutex = env->me_rmutex;

				if (!env->me_live_reader) {
					rc = mdb_reader_pid(env, Pidset, pid);
					if (rc) return rc;
					env->me_live_reader = 1;
				}

				if (LOCK_MUTEX(rc, env, rmutex)) return rc;
				nr = ti->mti_numreaders;
				for (i = 0; i < nr; i++)
					if (ti->mti_readers[i].mr_pid == 0)
						break;
				if (i == env->me_maxreaders) {
					UNLOCK_MUTEX(rmutex);
					return MDB_READERS_FULL;
				}
				r = &ti->mti_readers[i];
				r->mr_pid = 0;
				r->mr_txnid = (u64)-1;
				if (i == nr) ti->mti_numreaders = ++nr;
				env->me_close_readers = nr;
				r->mr_pid = pid;
				UNLOCK_MUTEX(rmutex);

				new_notls = (env->me_flags & MDB_NOTLS);
				if (!new_notls && (rc = pthread_setspecific(
						       env->me_txkey, r))) {
					r->mr_pid = 0;
					return rc;
				}
			}
			do r->mr_txnid = ti->mti_txnid;
			while (r->mr_txnid != ti->mti_txnid);
			if (!r->mr_txnid && (env->me_flags & MDB_RDONLY)) {
				meta = mdb_env_pick_meta(env);
				r->mr_txnid = meta->mm_txnid;
			} else {
				meta = env->me_metas[r->mr_txnid & 1];
			}
			txn->mt_txnid = r->mr_txnid;
			txn->mt_u.reader = r;
		}

	} else {
		if (ti) {
			if (LOCK_MUTEX(rc, env, env->me_wmutex)) return rc;
			txn->mt_txnid = ti->mti_txnid;
			meta = env->me_metas[txn->mt_txnid & 1];
		} else {
			meta = mdb_env_pick_meta(env);
			txn->mt_txnid = meta->mm_txnid;
		}
		txn->mt_txnid++;
		txn->mt_child = NULL;
		txn->mt_loose_pgs = NULL;
		txn->mt_loose_count = 0;
		txn->mt_dirty_room = MDB_IDL_UM_MAX;
		txn->mt_u.dirty_list = env->me_dirty_list;
		txn->mt_u.dirty_list[0].mid = 0;
		txn->mt_free_pgs = env->me_free_pgs;
		txn->mt_free_pgs[0] = 0;
		txn->mt_spill_pgs = NULL;
		env->me_txn = txn;
		memcpy(txn->mt_dbiseqs, env->me_dbiseqs,
		       env->me_maxdbs * sizeof(u32));
	}

	memcpy(txn->mt_dbs, meta->mm_dbs, CORE_DBS * sizeof(MDB_db));
	txn->mt_next_pgno = meta->mm_last_pg + 1;
	txn->mt_flags = flags;
	txn->mt_numdbs = env->me_numdbs;
	for (i = CORE_DBS; i < txn->mt_numdbs; i++) {
		x = env->me_dbflags[i];
		txn->mt_dbs[i].md_flags = x & PERSISTENT_FLAGS;
		txn->mt_dbflags[i] =
		    (x & MDB_VALID) ? DB_VALID | DB_USRVALID | DB_STALE : 0;
	}
	txn->mt_dbflags[MAIN_DBI] = DB_VALID | DB_USRVALID;
	txn->mt_dbflags[FREE_DBI] = DB_VALID;

	if (env->me_flags & MDB_FATAL_ERROR) {
		rc = MDB_PANIC;
	} else if (env->me_maxpg < txn->mt_next_pgno) {
		rc = MDB_MAP_RESIZED;
	} else {
		return MDB_SUCCESS;
	}
	mdb_txn_end(txn, new_notls | MDB_END_FAIL_BEGIN);
	return rc;
}

i32 mdb_txn_renew(MDB_txn *txn) {
	i32 rc;

	if (!txn || !F_ISSET(txn->mt_flags, MDB_TXN_RDONLY | MDB_TXN_FINISHED))
		return EINVAL;

	rc = mdb_txn_renew0(txn);
	return rc;
}

i32 mdb_txn_begin(MDB_env *env, MDB_txn *parent, u32 flags, MDB_txn **ret) {
	MDB_txn *txn;
	MDB_ntxn *ntxn;
	i32 rc, size, tsize;

	flags &= MDB_TXN_BEGIN_FLAGS;
	flags |= env->me_flags & MDB_WRITEMAP;

	if (env->me_flags & MDB_RDONLY & ~flags) return EACCES;

	if (parent) {
		flags |= parent->mt_flags;
		if (flags & (MDB_RDONLY | MDB_WRITEMAP | MDB_TXN_BLOCKED)) {
			return (parent->mt_flags & MDB_TXN_RDONLY)
				   ? EINVAL
				   : MDB_BAD_TXN;
		}
		size = env->me_maxdbs *
		       (sizeof(MDB_db) + sizeof(MDB_cursor *) + 1);
		size += tsize = sizeof(MDB_ntxn);
	} else if (flags & MDB_RDONLY) {
		size = env->me_maxdbs * (sizeof(MDB_db) + 1);
		size += tsize = sizeof(MDB_txn);
	} else {
		txn = env->me_txn0;
		goto renew;
	}
	if ((txn = calloc(1, size)) == NULL) {
		return ENOMEM;
	}
	txn->mt_dbxs = env->me_dbxs;
	txn->mt_dbs = (MDB_db *)((u8 *)txn + tsize);
	txn->mt_dbflags = (u8 *)txn + size - env->me_maxdbs;
	txn->mt_flags = flags;
	txn->mt_env = env;

	if (parent) {
		u32 i;
		txn->mt_cursors = (MDB_cursor **)(txn->mt_dbs + env->me_maxdbs);
		txn->mt_dbiseqs = parent->mt_dbiseqs;
		txn->mt_u.dirty_list = alloc(sizeof(MDB_ID2) * MDB_IDL_UM_SIZE);
		if (!txn->mt_u.dirty_list ||
		    !(txn->mt_free_pgs = mdb_midl_alloc(MDB_IDL_UM_MAX))) {
			release(txn->mt_u.dirty_list);
			release(txn);
			return ENOMEM;
		}
		txn->mt_txnid = parent->mt_txnid;
		txn->mt_dirty_room = parent->mt_dirty_room;
		txn->mt_u.dirty_list[0].mid = 0;
		txn->mt_spill_pgs = NULL;
		txn->mt_next_pgno = parent->mt_next_pgno;
		parent->mt_flags |= MDB_TXN_HAS_CHILD;
		parent->mt_child = txn;
		txn->mt_parent = parent;
		txn->mt_numdbs = parent->mt_numdbs;
		memcpy(txn->mt_dbs, parent->mt_dbs,
		       txn->mt_numdbs * sizeof(MDB_db));
		for (i = 0; i < txn->mt_numdbs; i++)
			txn->mt_dbflags[i] = parent->mt_dbflags[i] & ~DB_NEW;
		rc = 0;
		ntxn = (MDB_ntxn *)txn;
		ntxn->mnt_pgstate = env->me_pgstate;
		if (env->me_pghead) {
			size = MDB_IDL_SIZEOF(env->me_pghead);
			env->me_pghead = mdb_midl_alloc(env->me_pghead[0]);
			if (env->me_pghead)
				memcpy(env->me_pghead,
				       ntxn->mnt_pgstate.mf_pghead, size);
			else
				rc = ENOMEM;
		}
		if (!rc) rc = mdb_cursor_shadow(parent, txn);
		if (rc) mdb_txn_end(txn, MDB_END_FAIL_BEGINCHILD);
	} else {
		txn->mt_dbiseqs = env->me_dbiseqs;
	renew:
		rc = mdb_txn_renew0(txn);
	}
	if (rc) {
		if (txn != env->me_txn0) {
			release(txn->mt_u.dirty_list);
			release(txn);
		}
	} else {
		txn->mt_flags |= flags;
		*ret = txn;
	}

	return rc;
}

MDB_env *mdb_txn_env(MDB_txn *txn) {
	if (!txn) return NULL;
	return txn->mt_env;
}

u64 mdb_txn_id(MDB_txn *txn) {
	if (!txn) return 0;
	return txn->mt_txnid;
}

STATIC void mdb_dbis_update(MDB_txn *txn, i32 keep) {
	i32 i;
	MDB_dbi n = txn->mt_numdbs;
	MDB_env *env = txn->mt_env;
	u8 *tdbflags = txn->mt_dbflags;

	for (i = n; --i >= CORE_DBS;) {
		if (tdbflags[i] & DB_NEW) {
			if (keep) {
				env->me_dbflags[i] =
				    txn->mt_dbs[i].md_flags | MDB_VALID;
			} else {
				u8 *ptr = env->me_dbxs[i].md_name.mv_data;
				if (ptr) {
					env->me_dbxs[i].md_name.mv_data = NULL;
					env->me_dbxs[i].md_name.mv_size = 0;
					env->me_dbflags[i] = 0;
					env->me_dbiseqs[i]++;
					release(ptr);
				}
			}
		}
	}
	if (keep && env->me_numdbs < n) env->me_numdbs = n;
}

STATIC void mdb_txn_end(MDB_txn *txn, u32 mode) {
	MDB_env *env = txn->mt_env;

	mdb_dbis_update(txn, mode & MDB_END_UPDATE);

	if (F_ISSET(txn->mt_flags, MDB_TXN_RDONLY)) {
		if (txn->mt_u.reader) {
			txn->mt_u.reader->mr_txnid = (u64)-1;
			if (!(env->me_flags & MDB_NOTLS)) {
				txn->mt_u.reader = NULL;
			} else if (mode & MDB_END_SLOT) {
				txn->mt_u.reader->mr_pid = 0;
				txn->mt_u.reader = NULL;
			}
		}
		txn->mt_numdbs = 0;
		txn->mt_flags |= MDB_TXN_FINISHED;

	} else if (!F_ISSET(txn->mt_flags, MDB_TXN_FINISHED)) {
		u64 *pghead = env->me_pghead;

		if (!(mode & MDB_END_UPDATE)) mdb_cursors_close(txn, 0);
		if (!(env->me_flags & MDB_WRITEMAP)) {
			mdb_dlist_free(txn);
		}

		txn->mt_numdbs = 0;
		txn->mt_flags = MDB_TXN_FINISHED;

		if (!txn->mt_parent) {
			mdb_midl_shrink(&txn->mt_free_pgs);
			env->me_free_pgs = txn->mt_free_pgs;
			env->me_pghead = NULL;
			env->me_pglast = 0;

			env->me_txn = NULL;
			mode = 0;
			if (env->me_txns) UNLOCK_MUTEX(env->me_wmutex);
		} else {
			txn->mt_parent->mt_child = NULL;
			txn->mt_parent->mt_flags &= ~MDB_TXN_HAS_CHILD;
			env->me_pgstate = ((MDB_ntxn *)txn)->mnt_pgstate;
			mdb_midl_free(txn->mt_free_pgs);
			release(txn->mt_u.dirty_list);
		}
		mdb_midl_free(txn->mt_spill_pgs);

		mdb_midl_free(pghead);
	}
	if (mode & MDB_END_FREE) release(txn);
}

void mdb_txn_reset(MDB_txn *txn) {
	if (txn == NULL) return;
	if (!(txn->mt_flags & MDB_TXN_RDONLY)) return;
	mdb_txn_end(txn, MDB_END_RESET);
}

STATIC void _mdb_txn_abort(MDB_txn *txn) {
	if (txn == NULL) return;

	if (txn->mt_child) _mdb_txn_abort(txn->mt_child);

	mdb_txn_end(txn, MDB_END_ABORT | MDB_END_SLOT | MDB_END_FREE);
}

void mdb_txn_abort(MDB_txn *txn) { _mdb_txn_abort(txn); }

STATIC i32 mdb_freelist_save(MDB_txn *txn) {
	MDB_cursor mc;
	MDB_env *env = txn->mt_env;
	i32 rc, maxfree_1pg = env->me_maxfree_1pg, more = 1;
	u64 pglast = 0, head_id = 0;
	u64 freecnt = 0, *free_pgs, *mop;
	i64 head_room = 0, total_room = 0, mop_len, clean_limit;

	mdb_cursor_init(&mc, txn, FREE_DBI, NULL);

	if (env->me_pghead) {
		rc = mdb_page_search(&mc, NULL, MDB_PS_FIRST | MDB_PS_MODIFY);
		if (rc && rc != MDB_NOTFOUND) return rc;
	}

	if (!env->me_pghead && txn->mt_loose_pgs) {
		MDB_page *mp = txn->mt_loose_pgs;
		MDB_ID2 *dl = txn->mt_u.dirty_list;
		u32 x, y;
		if ((rc = mdb_midl_need(&txn->mt_free_pgs,
					txn->mt_loose_count)) != 0)
			return rc;
		for (; mp; mp = NEXT_LOOSE_PAGE(mp)) {
			mdb_midl_xappend(txn->mt_free_pgs, mp->mp_pgno);
			if (txn->mt_flags & MDB_TXN_WRITEMAP) {
				for (x = 1; x <= dl[0].mid; x++)
					if (dl[x].mid == mp->mp_pgno) break;
				mdb_tassert(txn, x <= dl[0].mid);
			} else {
				x = mdb_mid2l_search(dl, mp->mp_pgno);
				mdb_tassert(txn, dl[x].mid == mp->mp_pgno);
				mdb_dpage_free(env, mp);
			}
			dl[x].mptr = NULL;
		}
		for (y = 1; dl[y].mptr && y <= dl[0].mid; y++);
		if (y <= dl[0].mid) {
			for (x = y, y++;;) {
				while (!dl[y].mptr && y <= dl[0].mid) y++;
				if (y > dl[0].mid) break;
				dl[x++] = dl[y++];
			}
			dl[0].mid = x - 1;
		} else {
			dl[0].mid = 0;
		}
		txn->mt_loose_pgs = NULL;
		txn->mt_loose_count = 0;
	}
	clean_limit = (env->me_flags & (MDB_NOMEMINIT | MDB_WRITEMAP))
			  ? I64_MAX
			  : maxfree_1pg;

	for (;;) {
		MDB_val key, data;
		u64 *pgs;
		i64 j;

		while (pglast < env->me_pglast) {
			rc = mdb_cursor_first(&mc, &key, NULL);
			if (rc) return rc;
			pglast = head_id = *(u64 *)key.mv_data;
			total_room = head_room = 0;
			mdb_tassert(txn, pglast <= env->me_pglast);
			rc = _mdb_cursor_del(&mc, 0);
			if (rc) return rc;
		}

		if (freecnt < txn->mt_free_pgs[0]) {
			if (!freecnt) {
				rc = mdb_page_search(
				    &mc, NULL, MDB_PS_LAST | MDB_PS_MODIFY);
				if (rc && rc != MDB_NOTFOUND) return rc;
			}
			free_pgs = txn->mt_free_pgs;
			key.mv_size = sizeof(txn->mt_txnid);
			key.mv_data = &txn->mt_txnid;
			do {
				freecnt = free_pgs[0];
				data.mv_size = MDB_IDL_SIZEOF(free_pgs);
				rc = _mdb_cursor_put(&mc, &key, &data,
						     MDB_RESERVE);
				if (rc) return rc;
				free_pgs = txn->mt_free_pgs;
			} while (freecnt < free_pgs[0]);
			mdb_midl_sort(free_pgs);
			memcpy(data.mv_data, free_pgs, data.mv_size);
			continue;
		}

		mop = env->me_pghead;
		mop_len = (mop ? mop[0] : 0) + txn->mt_loose_count;

		if (total_room >= mop_len) {
			if (total_room == mop_len || --more < 0) break;
		} else if (head_room >= maxfree_1pg && head_id > 1) {
			head_id--;
			head_room = 0;
		}
		total_room -= head_room;
		head_room = mop_len - total_room;
		if (head_room > maxfree_1pg && head_id > 1) {
			head_room /= head_id;
			head_room +=
			    maxfree_1pg - head_room % (maxfree_1pg + 1);
		} else if (head_room < 0)
			head_room = 0;
		key.mv_size = sizeof(head_id);
		key.mv_data = &head_id;
		data.mv_size = (head_room + 1) * sizeof(u64);
		rc = _mdb_cursor_put(&mc, &key, &data, MDB_RESERVE);
		if (rc) return rc;
		pgs = (u64 *)data.mv_data;
		j = head_room > clean_limit ? head_room : 0;
		do {
			pgs[j] = 0;
		} while (--j >= 0);
		total_room += head_room;
	}

	if (txn->mt_loose_pgs) {
		MDB_page *mp = txn->mt_loose_pgs;
		u32 count = txn->mt_loose_count;
		MDB_IDL loose;
		if ((rc = mdb_midl_need(&env->me_pghead, 2 * count + 1)) != 0)
			return rc;
		mop = env->me_pghead;
		loose = mop + MDB_IDL_ALLOCLEN(mop) - count;
		for (count = 0; mp; mp = NEXT_LOOSE_PAGE(mp))
			loose[++count] = mp->mp_pgno;
		loose[0] = count;
		mdb_midl_sort(loose);
		mdb_midl_xmerge(mop, loose);
		txn->mt_loose_pgs = NULL;
		txn->mt_loose_count = 0;
		mop_len = mop[0];
	}

	rc = MDB_SUCCESS;
	if (mop_len) {
		MDB_val key, data;

		mop += mop_len;
		rc = mdb_cursor_first(&mc, &key, &data);
		for (; !rc; rc = mdb_cursor_next(&mc, &key, &data, MDB_NEXT)) {
			u64 id = *(u64 *)key.mv_data;
			i64 len = (i64)(data.mv_size / sizeof(MDB_ID)) - 1;
			MDB_ID save;

			mdb_tassert(txn, len >= 0 && id <= env->me_pglast);
			key.mv_data = &id;
			if (len > mop_len) {
				len = mop_len;
				data.mv_size = (len + 1) * sizeof(MDB_ID);
			}
			data.mv_data = mop -= len;
			save = mop[0];
			mop[0] = len;
			rc = _mdb_cursor_put(&mc, &key, &data, MDB_CURRENT);
			mop[0] = save;
			if (rc || !(mop_len -= len)) break;
		}
	}
	return rc;
}

STATIC i32 mdb_page_flush(MDB_txn *txn, i32 keep) {
	MDB_env *env = txn->mt_env;
	MDB_ID2L dl = txn->mt_u.dirty_list;
	u32 psize = env->me_psize, j;
	i32 i, pagecount = dl[0].mid, rc;
	u64 size = 0;
	i64 pos = 0;
	u64 pgno = 0;
	MDB_page *dp = NULL;
	struct iovec iov[MDB_COMMIT_PAGES];
	HANDLE fd = env->me_fd;
	i64 wsize = 0, wres;
	i64 wpos = 0, next_pos = 1;
	i32 n = 0;

	j = i = keep;
	if (env->me_flags & MDB_WRITEMAP) {
		while (++i <= pagecount) {
			dp = dl[i].mptr;
			if (dp->mp_flags & (P_LOOSE | P_KEEP)) {
				dp->mp_flags &= ~P_KEEP;
				dl[++j] = dl[i];
				continue;
			}
			dp->mp_flags &= ~P_DIRTY;
		}
		goto done;
	}

	for (;;) {
		if (++i <= pagecount) {
			dp = dl[i].mptr;
			if (dp->mp_flags & (P_LOOSE | P_KEEP)) {
				dp->mp_flags &= ~P_KEEP;
				dl[i].mid = 0;
				continue;
			}
			pgno = dl[i].mid;
			dp->mp_flags &= ~P_DIRTY;
			pos = pgno * psize;
			size = psize;
			if (IS_OVERFLOW(dp)) size *= dp->mp_pages;
		}
		if (pos != next_pos || n == MDB_COMMIT_PAGES ||
		    wsize + size > MAX_WRITE) {
			if (n) {
			retry_write:
				if (n == 1) {
					wres = pwrite(fd, iov[0].iov_base,
						      wsize, wpos);
				} else {
				retry_seek:
					if (lseek(fd, wpos, SEEK_SET) == -1) {
						rc = ErrCode();
						if (rc == EINTR)
							goto retry_seek;
						return rc;
					}
					wres = writev(fd, iov, n);
				}
				if (wres != wsize) {
					if (wres < 0) {
						rc = ErrCode();
						if (rc == EINTR)
							goto retry_write;
					} else {
						rc = EIO;
					}
					return rc;
				}
				n = 0;
			}
			if (i > pagecount) break;
			wpos = pos;
			wsize = 0;
		}
		iov[n].iov_len = size;
		iov[n].iov_base = (u8 *)dp;
		next_pos = pos + size;
		wsize += size;
		n++;
	}

	if (!(env->me_flags & MDB_WRITEMAP)) {
		for (i = keep; ++i <= pagecount;) {
			dp = dl[i].mptr;
			if (!dl[i].mid) {
				dl[++j] = dl[i];
				dl[j].mid = dp->mp_pgno;
				continue;
			}
			mdb_dpage_free(env, dp);
		}
	}

done:
	i--;
	txn->mt_dirty_room += i - j;
	dl[0].mid = j;
	return MDB_SUCCESS;
}

STATIC i32 mdb_env_share_locks(MDB_env *env, i32 *excl);

STATIC i32 _mdb_txn_commit(MDB_txn *txn) {
	i32 rc;
	u32 i, end_mode;
	MDB_env *env;

	if (txn == NULL) return EINVAL;

	end_mode =
	    MDB_END_EMPTY_COMMIT | MDB_END_UPDATE | MDB_END_SLOT | MDB_END_FREE;

	if (txn->mt_child) {
		rc = _mdb_txn_commit(txn->mt_child);
		if (rc) goto fail;
	}

	env = txn->mt_env;

	if (F_ISSET(txn->mt_flags, MDB_TXN_RDONLY)) {
		goto done;
	}

	if (txn->mt_flags & (MDB_TXN_FINISHED | MDB_TXN_ERROR)) {
		if (txn->mt_parent) txn->mt_parent->mt_flags |= MDB_TXN_ERROR;
		rc = MDB_BAD_TXN;
		goto fail;
	}

	if (txn->mt_parent) {
		MDB_txn *parent = txn->mt_parent;
		MDB_page **lp;
		MDB_ID2L dst, src;
		MDB_IDL pspill;
		u32 x, y, len, ps_len;

		rc = mdb_midl_append_list(&parent->mt_free_pgs,
					  txn->mt_free_pgs);
		if (rc) goto fail;
		mdb_midl_free(txn->mt_free_pgs);

		parent->mt_next_pgno = txn->mt_next_pgno;
		parent->mt_flags = txn->mt_flags;
		mdb_cursors_close(txn, 1);
		memcpy(parent->mt_dbs, txn->mt_dbs,
		       txn->mt_numdbs * sizeof(MDB_db));
		parent->mt_numdbs = txn->mt_numdbs;
		parent->mt_dbflags[FREE_DBI] = txn->mt_dbflags[FREE_DBI];
		parent->mt_dbflags[MAIN_DBI] = txn->mt_dbflags[MAIN_DBI];
		for (i = CORE_DBS; i < txn->mt_numdbs; i++) {
			x = parent->mt_dbflags[i] & DB_NEW;
			parent->mt_dbflags[i] = txn->mt_dbflags[i] | x;
		}

		dst = parent->mt_u.dirty_list;
		src = txn->mt_u.dirty_list;
		if ((pspill = parent->mt_spill_pgs) && (ps_len = pspill[0])) {
			x = y = ps_len;
			pspill[0] = (u64)-1;
			for (i = 0, len = src[0].mid; ++i <= len;) {
				MDB_ID pn = src[i].mid << 1;
				while (pn > pspill[x]) x--;
				if (pn == pspill[x]) {
					pspill[x] = 1;
					y = --x;
				}
			}
			for (x = y; ++x <= ps_len;)
				if (!(pspill[x] & 1)) pspill[++y] = pspill[x];
			pspill[0] = y;
		}

		if (txn->mt_spill_pgs && txn->mt_spill_pgs[0]) {
			for (i = 1; i <= txn->mt_spill_pgs[0]; i++) {
				MDB_ID pn = txn->mt_spill_pgs[i];
				if (pn & 1) continue;
				pn >>= 1;
				y = mdb_mid2l_search(dst, pn);
				if (y <= dst[0].mid && dst[y].mid == pn) {
					release(dst[y].mptr);
					while (y < dst[0].mid) {
						dst[y] = dst[y + 1];
						y++;
					}
					dst[0].mid--;
				}
			}
		}

		x = dst[0].mid;
		dst[0].mid = 0;
		if (parent->mt_parent) {
			len = x + src[0].mid;
			y = mdb_mid2l_search(src, dst[x].mid + 1) - 1;
			for (i = x; y && i; y--) {
				u64 yp = src[y].mid;
				while (yp < dst[i].mid) i--;
				if (yp == dst[i].mid) {
					i--;
					len--;
				}
			}
		} else
			len = MDB_IDL_UM_MAX - txn->mt_dirty_room;
		y = src[0].mid;
		for (i = len; y; dst[i--] = src[y--]) {
			u64 yp = src[y].mid;
			while (yp < dst[x].mid) dst[i--] = dst[x--];
			if (yp == dst[x].mid) release(dst[x--].mptr);
		}
		mdb_tassert(txn, i == x);
		dst[0].mid = len;
		release(txn->mt_u.dirty_list);
		parent->mt_dirty_room = txn->mt_dirty_room;
		if (txn->mt_spill_pgs) {
			if (parent->mt_spill_pgs) {
				rc = mdb_midl_append_list(&parent->mt_spill_pgs,
							  txn->mt_spill_pgs);
				if (rc) parent->mt_flags |= MDB_TXN_ERROR;
				mdb_midl_free(txn->mt_spill_pgs);
				mdb_midl_sort(parent->mt_spill_pgs);
			} else {
				parent->mt_spill_pgs = txn->mt_spill_pgs;
			}
		}
		for (lp = &parent->mt_loose_pgs; *lp;
		     lp = &NEXT_LOOSE_PAGE(*lp));
		*lp = txn->mt_loose_pgs;
		parent->mt_loose_count += txn->mt_loose_count;

		parent->mt_child = NULL;
		mdb_midl_free(((MDB_ntxn *)txn)->mnt_pgstate.mf_pghead);
		release(txn);
		return rc;
	}

	if (txn != env->me_txn) {
		rc = EINVAL;
		goto fail;
	}

	mdb_cursors_close(txn, 0);

	if (!txn->mt_u.dirty_list[0].mid &&
	    !(txn->mt_flags & (MDB_TXN_DIRTY | MDB_TXN_SPILLS)))
		goto done;

	if (txn->mt_numdbs > CORE_DBS) {
		MDB_cursor mc;
		MDB_dbi i;
		MDB_val data;
		data.mv_size = sizeof(MDB_db);

		mdb_cursor_init(&mc, txn, MAIN_DBI, NULL);
		for (i = CORE_DBS; i < txn->mt_numdbs; i++) {
			if (txn->mt_dbflags[i] & DB_DIRTY) {
				if (TXN_DBI_CHANGED(txn, i)) {
					rc = MDB_BAD_DBI;
					goto fail;
				}
				data.mv_data = &txn->mt_dbs[i];
				rc = _mdb_cursor_put(&mc,
						     &txn->mt_dbxs[i].md_name,
						     &data, F_SUBDATA);
				if (rc) goto fail;
			}
		}
	}

	rc = mdb_freelist_save(txn);
	if (rc) goto fail;

	mdb_midl_free(env->me_pghead);
	env->me_pghead = NULL;
	mdb_midl_shrink(&txn->mt_free_pgs);

	if ((rc = mdb_page_flush(txn, 0))) goto fail;
	if (!F_ISSET(txn->mt_flags, MDB_TXN_NOSYNC) &&
	    (rc = mdb_env_sync0(env, 0, txn->mt_next_pgno)))
		goto fail;
	if ((rc = mdb_env_write_meta(txn))) goto fail;
	end_mode = MDB_END_COMMITTED | MDB_END_UPDATE;
	if (env->me_flags & MDB_PREVSNAPSHOT) {
		if (!(env->me_flags & MDB_NOLOCK)) {
			i32 excl;
			rc = mdb_env_share_locks(env, &excl);
			if (rc) goto fail;
		}
		env->me_flags ^= MDB_PREVSNAPSHOT;
	}

done:
	mdb_txn_end(txn, end_mode);
	return MDB_SUCCESS;

fail:
	_mdb_txn_abort(txn);
	return rc;
}

i32 mdb_txn_commit(MDB_txn *txn) { return _mdb_txn_commit(txn); }

STATIC i32 mdb_env_read_header(MDB_env *env, i32 prev, MDB_meta *meta) {
	MDB_metabuf pbuf;
	MDB_page *p;
	MDB_meta *m;
	i32 i, rc, off;
	enum { Size = sizeof(pbuf) };

	for (i = off = 0; i < NUM_METAS; i++, off += meta->mm_psize) {
		rc = pread(env->me_fd, &pbuf, Size, off);
		if (rc != Size) {
			if (rc == 0 && off == 0) {
				return ENOENT;
			}
			rc = rc < 0 ? (i32)ErrCode() : MDB_INVALID;
			return rc;
		}

		p = (MDB_page *)&pbuf;

		if (!F_ISSET(p->mp_flags, P_META)) {
			return MDB_INVALID;
		}

		m = METADATA(p);
		if (m->mm_magic != MDB_MAGIC) {
			return MDB_INVALID;
		}

		if (m->mm_version != MDB_DATA_VERSION) {
			return MDB_VERSION_MISMATCH;
		}

		if (off == 0 || (prev ? m->mm_txnid < meta->mm_txnid
				      : m->mm_txnid > meta->mm_txnid))
			*meta = *m;
	}
	return 0;
}

STATIC void mdb_env_init_meta0(MDB_env *env, MDB_meta *meta) {
	meta->mm_magic = MDB_MAGIC;
	meta->mm_version = MDB_DATA_VERSION;
	meta->mm_mapsize = env->me_mapsize;
	meta->mm_psize = env->me_psize;
	meta->mm_last_pg = NUM_METAS - 1;
	meta->mm_flags = env->me_flags & 0xffff;
	meta->mm_flags |= MDB_INTEGERKEY;
	meta->mm_dbs[FREE_DBI].md_root = P_INVALID;
	meta->mm_dbs[MAIN_DBI].md_root = P_INVALID;
}

STATIC i32 mdb_env_init_meta(MDB_env *env, MDB_meta *meta) {
	MDB_page *p, *q;
	i32 rc;
	u32 psize;
	i32 len;
#define DO_PWRITE(rc, fd, ptr, size, len, pos)                 \
	do {                                                   \
		len = pwrite(fd, ptr, size, pos);              \
		if (len == -1 && ErrCode() == EINTR) continue; \
		rc = (len >= 0);                               \
		break;                                         \
	} while (1)

	psize = env->me_psize;

	p = calloc(NUM_METAS, psize);
	if (!p) return ENOMEM;
	p->mp_pgno = 0;
	p->mp_flags = P_META;
	*(MDB_meta *)METADATA(p) = *meta;

	q = (MDB_page *)((u8 *)p + psize);
	q->mp_pgno = 1;
	q->mp_flags = P_META;
	*(MDB_meta *)METADATA(q) = *meta;

	DO_PWRITE(rc, env->me_fd, p, psize * NUM_METAS, len, 0);
	if (!rc)
		rc = ErrCode();
	else if ((u32)len == psize * NUM_METAS)
		rc = MDB_SUCCESS;
	else
		rc = ENOSPC;
	release(p);
	return rc;
}

STATIC i32 mdb_env_write_meta(MDB_txn *txn) {
	MDB_env *env;
	MDB_meta meta, metab, *mp;
	u32 flags;
	u64 mapsize;
	i64 off;
	i32 rc, len, toggle;
	u8 *ptr;
	HANDLE mfd;
	i32 r2;

	toggle = txn->mt_txnid & 1;

	env = txn->mt_env;
	flags = txn->mt_flags | env->me_flags;
	mp = env->me_metas[toggle];
	mapsize = env->me_metas[toggle ^ 1]->mm_mapsize;
	if (mapsize < env->me_mapsize) mapsize = env->me_mapsize;

	if (flags & MDB_WRITEMAP) {
		mp->mm_mapsize = mapsize;
		mp->mm_dbs[FREE_DBI] = txn->mt_dbs[FREE_DBI];
		mp->mm_dbs[MAIN_DBI] = txn->mt_dbs[MAIN_DBI];
		mp->mm_last_pg = txn->mt_next_pgno - 1;
		__sync_synchronize();
		mp->mm_txnid = txn->mt_txnid;
		if (!(flags & (MDB_NOMETASYNC | MDB_NOSYNC))) {
			u32 meta_size = env->me_psize;
			rc =
			    (env->me_flags & MDB_MAPASYNC) ? MS_ASYNC : MS_SYNC;
			ptr = (u8 *)mp - PAGEHDRSZ;
			r2 = (ptr - env->me_map) & (env->me_os_psize - 1);
			ptr -= r2;
			meta_size += r2;
			if (msync(ptr, meta_size, rc)) {
				rc = ErrCode();
				goto fail;
			}
		}
		goto done;
	}
	metab.mm_txnid = mp->mm_txnid;
	metab.mm_last_pg = mp->mm_last_pg;

	meta.mm_mapsize = mapsize;
	meta.mm_dbs[FREE_DBI] = txn->mt_dbs[FREE_DBI];
	meta.mm_dbs[MAIN_DBI] = txn->mt_dbs[MAIN_DBI];
	meta.mm_last_pg = txn->mt_next_pgno - 1;
	meta.mm_txnid = txn->mt_txnid;

	off = offsetof(MDB_meta, mm_mapsize);
	ptr = (u8 *)&meta + off;
	len = sizeof(MDB_meta) - off;
	off += (u8 *)mp - env->me_map;

	mfd =
	    (flags & (MDB_NOSYNC | MDB_NOMETASYNC)) ? env->me_fd : env->me_mfd;
retry_write:
	rc = pwrite(mfd, ptr, len, off);
	if (rc != len) {
		rc = rc < 0 ? ErrCode() : EIO;
		if (rc == EINTR) goto retry_write;
		meta.mm_last_pg = metab.mm_last_pg;
		meta.mm_txnid = metab.mm_txnid;
		r2 = pwrite(env->me_fd, ptr, len, off);
		(void)r2;
	fail:
		env->me_flags |= MDB_FATAL_ERROR;
		return rc;
	}
done:
	if (env->me_txns) env->me_txns->mti_txnid = txn->mt_txnid;

	return MDB_SUCCESS;
}

STATIC MDB_meta *mdb_env_pick_meta(const MDB_env *env) {
	MDB_meta *const *metas = env->me_metas;
	return metas[(metas[0]->mm_txnid < metas[1]->mm_txnid) ^
		     ((env->me_flags & MDB_PREVSNAPSHOT) != 0)];
}

i32 mdb_env_create(MDB_env **env) {
	MDB_env *e;

	e = calloc(1, sizeof(MDB_env));
	if (!e) return ENOMEM;

	e->me_maxreaders = DEFAULT_READERS;
	e->me_maxdbs = e->me_numdbs = CORE_DBS;
	e->me_fd = INVALID_HANDLE_VALUE;
	e->me_lfd = INVALID_HANDLE_VALUE;
	e->me_mfd = INVALID_HANDLE_VALUE;
	e->me_pid = getpid();
	GET_PAGESIZE(e->me_os_psize);
	*env = e;
	return MDB_SUCCESS;
}

STATIC i32 mdb_env_map(MDB_env *env, void *addr) {
	MDB_page *p;
	u32 flags = env->me_flags;
	i32 mmap_flags = MAP_SHARED;
	i32 prot = PROT_READ;
	if (flags & MDB_WRITEMAP) {
		prot |= PROT_WRITE;
		if (ftruncate(env->me_fd, env->me_mapsize) < 0)
			return ErrCode();
	}
	env->me_map =
	    mmap(addr, env->me_mapsize, prot, mmap_flags, env->me_fd, 0);
	if (env->me_map == MAP_FAILED) {
		env->me_map = NULL;
		return ErrCode();
	}

	if (addr && env->me_map != addr) return EBUSY;

	p = (MDB_page *)env->me_map;
	env->me_metas[0] = METADATA(p);
	env->me_metas[1] = (MDB_meta *)((u8 *)env->me_metas[0] + env->me_psize);

	return MDB_SUCCESS;
}

i32 mdb_env_set_mapsize(MDB_env *env, u64 size) {
	if (env->me_map) {
		MDB_meta *meta;
		void *old;
		i32 rc;
		if (env->me_txn) return EINVAL;
		meta = mdb_env_pick_meta(env);
		if (!size) size = meta->mm_mapsize;
		{
			u64 minsize = (meta->mm_last_pg + 1) * env->me_psize;
			if (size < minsize) size = minsize;
		}
		munmap(env->me_map, env->me_mapsize);
		env->me_mapsize = size;
		old = (env->me_flags & MDB_FIXEDMAP) ? env->me_map : NULL;
		rc = mdb_env_map(env, old);
		if (rc) return rc;
	}
	env->me_mapsize = size;
	if (env->me_psize) env->me_maxpg = env->me_mapsize / env->me_psize;
	return MDB_SUCCESS;
}

i32 mdb_env_set_maxdbs(MDB_env *env, MDB_dbi dbs) {
	if (env->me_map) return EINVAL;
	env->me_maxdbs = dbs + CORE_DBS;
	return MDB_SUCCESS;
}

i32 mdb_env_set_maxreaders(MDB_env *env, u32 readers) {
	if (env->me_map || readers < 1) return EINVAL;
	env->me_maxreaders = readers;
	return MDB_SUCCESS;
}

i32 mdb_env_get_maxreaders(MDB_env *env, u32 *readers) {
	if (!env || !readers) return EINVAL;
	*readers = env->me_maxreaders;
	return MDB_SUCCESS;
}

STATIC i32 mdb_fname_init(const u8 *path, u32 envflags, MDB_name *fname) {
	i32 no_suffix = F_ISSET(envflags, MDB_NOSUBDIR | MDB_NOLOCK);
	fname->mn_alloced = 0;
	fname->mn_len = strlen(path);
	if (no_suffix)
		fname->mn_val = (u8 *)path;
	else if ((fname->mn_val = alloc(fname->mn_len + MDB_SUFFLEN + 1)) !=
		 NULL) {
		fname->mn_alloced = 1;
		strcpy(fname->mn_val, path);
	} else
		return ENOMEM;
	return MDB_SUCCESS;
}

STATIC i32 mdb_fopen(const MDB_env *env, MDB_name *fname,
		     enum mdb_fopen_type which, mdb_mode_t mode, HANDLE *res) {
	i32 rc = MDB_SUCCESS;
	HANDLE fd;
	i32 flags;

	if (fname->mn_alloced)
		mdb_name_cpy(fname->mn_val + fname->mn_len,
			     mdb_suffixes[which == MDB_O_LOCKS][F_ISSET(
				 env->me_flags, MDB_NOSUBDIR)]);

	fd = open(fname->mn_val, which & MDB_O_MASK, mode);

	if (fd == INVALID_HANDLE_VALUE)
		rc = ErrCode();
	else {
		if (which != MDB_O_RDONLY && which != MDB_O_RDWR) {
			if (!MDB_CLOEXEC && (flags = fcntl(fd, F_GETFD)) != -1)
				(void)fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
		}
	}

	*res = fd;
	return rc;
}

STATIC i32 mdb_env_open2(MDB_env *env, i32 prev) {
	u32 flags = env->me_flags;
	i32 i, newenv = 0, rc;
	MDB_meta meta;
	u64 minsize;

	if ((i = mdb_env_read_header(env, prev, &meta)) != 0) {
		if (i != ENOENT) return i;
		newenv = 1;
		env->me_psize = env->me_os_psize;
		if (env->me_psize > MAX_PAGESIZE) env->me_psize = MAX_PAGESIZE;
		memset(&meta, 0, sizeof(meta));
		mdb_env_init_meta0(env, &meta);
		meta.mm_mapsize = DEFAULT_MAPSIZE;
	} else
		env->me_psize = meta.mm_psize;

	if (!env->me_mapsize) env->me_mapsize = meta.mm_mapsize;
	minsize = (meta.mm_last_pg + 1) * meta.mm_psize;
	if (env->me_mapsize < minsize) env->me_mapsize = minsize;
	meta.mm_mapsize = env->me_mapsize;

	if (newenv && !(flags & MDB_FIXEDMAP)) {
		rc = mdb_env_init_meta(env, &meta);
		if (rc) return rc;
		newenv = 0;
	}

	rc = mdb_env_map(env, (flags & MDB_FIXEDMAP) ? meta.mm_address : NULL);
	if (rc) return rc;

	if (newenv) {
		if (flags & MDB_FIXEDMAP) meta.mm_address = env->me_map;
		i = mdb_env_init_meta(env, &meta);
		if (i != MDB_SUCCESS) return i;
	}

	env->me_maxfree_1pg = (env->me_psize - PAGEHDRSZ) / sizeof(u64) - 1;
	env->me_nodemax =
	    (((env->me_psize - PAGEHDRSZ) / MDB_MINKEYS) & -2) - sizeof(u16);
#if !(MDB_MAXKEYSIZE)
	env->me_maxkey = env->me_nodemax - (NODESIZE + sizeof(MDB_db));
#endif
	env->me_maxpg = env->me_mapsize / env->me_psize;

	if (prev && env->me_txns) env->me_txns->mti_txnid = meta.mm_txnid;

	return MDB_SUCCESS;
}

STATIC void mdb_env_reader_dest(void *ptr) {
	MDB_reader *reader = ptr;
	if (reader->mr_pid == getpid()) reader->mr_pid = 0;
}

STATIC i32 mdb_env_share_locks(MDB_env *env, i32 *excl) {
	i32 rc = 0;
	struct flock lock_info;
	MDB_meta *meta = mdb_env_pick_meta(env);

	env->me_txns->mti_txnid = meta->mm_txnid;

	memset((void *)&lock_info, 0, sizeof(lock_info));
	lock_info.l_type = F_RDLCK;
	lock_info.l_whence = SEEK_SET;
	lock_info.l_start = 0;
	lock_info.l_len = 1;
	while ((rc = fcntl(env->me_lfd, F_SETLK, &lock_info)) &&
	       (rc = ErrCode()) == EINTR);
	*excl = rc ? -1 : 0;

	return rc;
}

STATIC i32 mdb_env_excl_lock(MDB_env *env, i32 *excl) {
	i32 rc = 0;
	struct flock lock_info;
	memset((void *)&lock_info, 0, sizeof(lock_info));
	lock_info.l_type = F_WRLCK;
	lock_info.l_whence = SEEK_SET;
	lock_info.l_start = 0;
	lock_info.l_len = 1;
	while ((rc = fcntl(env->me_lfd, F_SETLK, &lock_info)) &&
	       (rc = ErrCode()) == EINTR);
	if (!rc) {
		*excl = 1;
	} else if (*excl < 0) {
		lock_info.l_type = F_RDLCK;
		while ((rc = fcntl(env->me_lfd, F_SETLKW, &lock_info)) &&
		       (rc = ErrCode()) == EINTR);
		if (rc == 0) *excl = 0;
	}
	return rc;
}

STATIC i32 mdb_env_setup_locks(MDB_env *env, MDB_name *fname, i32 mode,
			       i32 *excl) {
#define MDB_ERRCODE_ROFS EROFS
	i32 rc;
	i64 size, rsize;

	rc = mdb_fopen(env, fname, MDB_O_LOCKS, mode, &env->me_lfd);
	if (rc) {
		if (rc == MDB_ERRCODE_ROFS && (env->me_flags & MDB_RDONLY)) {
			return MDB_SUCCESS;
		}
		goto fail;
	}

	if (!(env->me_flags & MDB_NOTLS)) {
		rc = pthread_key_create(&env->me_txkey, mdb_env_reader_dest);
		if (rc) goto fail;
		env->me_flags |= MDB_ENV_TXKEY;
	}

	if ((rc = mdb_env_excl_lock(env, excl))) goto fail;

	size = lseek(env->me_lfd, 0, SEEK_END);
	if (size == -1) goto fail_err;
	rsize =
	    (env->me_maxreaders - 1) * sizeof(MDB_reader) + sizeof(MDB_txninfo);
	if (size < rsize && *excl > 0) {
		if (ftruncate(env->me_lfd, rsize) != 0) goto fail_err;
	} else {
		rsize = size;
		size = rsize - sizeof(MDB_txninfo);
		env->me_maxreaders = size / sizeof(MDB_reader) + 1;
	}
	{
		void *m = mmap(NULL, rsize, PROT_READ | PROT_WRITE, MAP_SHARED,
			       env->me_lfd, 0);
		if (m == MAP_FAILED) goto fail_err;
		env->me_txns = m;
	}
	if (*excl > 0) {
		pthread_mutexattr_t mattr;

		memset(env->me_txns->mti_rmutex, 0,
		       sizeof(*env->me_txns->mti_rmutex));
		memset(env->me_txns->mti_wmutex, 0,
		       sizeof(*env->me_txns->mti_wmutex));

		if ((rc = pthread_mutexattr_init(&mattr)) != 0) goto fail;
		rc = pthread_mutexattr_setpshared(&mattr,
						  PTHREAD_PROCESS_SHARED);
		if (!rc)
			rc = pthread_mutex_init(env->me_txns->mti_rmutex,
						&mattr);
		if (!rc)
			rc = pthread_mutex_init(env->me_txns->mti_wmutex,
						&mattr);
		pthread_mutexattr_destroy(&mattr);
		if (rc) goto fail;

		env->me_txns->mti_magic = MDB_MAGIC;
		env->me_txns->mti_format = MDB_LOCK_FORMAT;
		env->me_txns->mti_txnid = 0;
		env->me_txns->mti_numreaders = 0;

	} else {
		if (env->me_txns->mti_magic != MDB_MAGIC) {
			rc = MDB_INVALID;
			goto fail;
		}
		if (env->me_txns->mti_format != MDB_LOCK_FORMAT) {
			rc = MDB_VERSION_MISMATCH;
			goto fail;
		}
		rc = ErrCode();
		if (rc && rc != EACCES && rc != EAGAIN) {
			goto fail;
		}
	}

	return MDB_SUCCESS;

fail_err:
	rc = ErrCode();
fail:
	return rc;
}

i32 mdb_env_open(MDB_env *env, const u8 *path, u32 flags, mdb_mode_t mode) {
	i32 rc, excl = -1;
	MDB_name fname;

	if (env->me_fd != INVALID_HANDLE_VALUE ||
	    (flags & ~(CHANGEABLE | CHANGELESS)))
		return EINVAL;

	flags |= env->me_flags;

	rc = mdb_fname_init(path, flags, &fname);
	if (rc) return rc;

	flags |= MDB_ENV_ACTIVE;

	if (flags & MDB_RDONLY)
		flags &= ~MDB_WRITEMAP;
	else {
		if (!((env->me_free_pgs = mdb_midl_alloc(MDB_IDL_UM_MAX)) &&
		      (env->me_dirty_list =
			   calloc(MDB_IDL_UM_SIZE, sizeof(MDB_ID2)))))
			rc = ENOMEM;
	}

	/* We don't support TLS */
	env->me_flags = flags | MDB_NOTLS;
	if (rc) goto leave;

	env->me_path = strdup(path);
	env->me_dbxs = calloc(env->me_maxdbs, sizeof(MDB_dbx));
	env->me_dbflags = calloc(env->me_maxdbs, sizeof(u16));
	env->me_dbiseqs = calloc(env->me_maxdbs, sizeof(u32));
	if (!(env->me_dbxs && env->me_path && env->me_dbflags &&
	      env->me_dbiseqs)) {
		rc = ENOMEM;
		goto leave;
	}
	env->me_dbxs[FREE_DBI].md_cmp = mdb_cmp_long;

	if (!(flags & (MDB_RDONLY | MDB_NOLOCK))) {
		rc = mdb_env_setup_locks(env, &fname, mode, &excl);
		if (rc) goto leave;
		if ((flags & MDB_PREVSNAPSHOT) && !excl) {
			rc = EAGAIN;
			goto leave;
		}
	}

	rc = mdb_fopen(env, &fname,
		       (flags & MDB_RDONLY) ? MDB_O_RDONLY : MDB_O_RDWR, mode,
		       &env->me_fd);
	if (rc) goto leave;

	if ((flags & (MDB_RDONLY | MDB_NOLOCK)) == MDB_RDONLY) {
		rc = mdb_env_setup_locks(env, &fname, mode, &excl);
		if (rc) goto leave;
	}

	if ((rc = mdb_env_open2(env, flags & MDB_PREVSNAPSHOT)) ==
	    MDB_SUCCESS) {
		if (!(flags & (MDB_RDONLY | MDB_WRITEMAP))) {
			rc = mdb_fopen(env, &fname, MDB_O_META, mode,
				       &env->me_mfd);
			if (rc) goto leave;
		}
		if (excl > 0 && !(flags & MDB_PREVSNAPSHOT)) {
			rc = mdb_env_share_locks(env, &excl);
			if (rc) goto leave;
		}
		if (!(flags & MDB_RDONLY)) {
			MDB_txn *txn;
			i32 tsize = sizeof(MDB_txn),
			    size =
				tsize + env->me_maxdbs * (sizeof(MDB_db) +
							  sizeof(MDB_cursor *) +
							  sizeof(u32) + 1);
			if ((env->me_pbuf = calloc(1, env->me_psize)) &&
			    (txn = calloc(1, size))) {
				txn->mt_dbs = (MDB_db *)((u8 *)txn + tsize);
				txn->mt_cursors =
				    (MDB_cursor **)(txn->mt_dbs +
						    env->me_maxdbs);
				txn->mt_dbiseqs =
				    (u32 *)(txn->mt_cursors + env->me_maxdbs);
				txn->mt_dbflags =
				    (u8 *)(txn->mt_dbiseqs + env->me_maxdbs);
				txn->mt_env = env;
				txn->mt_dbxs = env->me_dbxs;
				txn->mt_flags = MDB_TXN_FINISHED;
				env->me_txn0 = txn;
			} else {
				rc = ENOMEM;
			}
		}
	}

leave:
	if (rc) {
		mdb_env_close0(env, excl);
	}
	mdb_fname_destroy(fname);
	return rc;
}

STATIC void mdb_env_close0(MDB_env *env, i32 excl __attribute__((unused))) {
	i32 i;

	if (!(env->me_flags & MDB_ENV_ACTIVE)) return;

	if (env->me_dbxs) {
		for (i = env->me_maxdbs; --i >= CORE_DBS;)
			release(env->me_dbxs[i].md_name.mv_data);
		release(env->me_dbxs);
	}

	release(env->me_pbuf);
	release(env->me_dbiseqs);
	release(env->me_dbflags);
	release(env->me_path);
	release(env->me_dirty_list);
	release(env->me_txn0);
	mdb_midl_free(env->me_free_pgs);

	if (env->me_flags & MDB_ENV_TXKEY) {
		pthread_key_delete(env->me_txkey);
	}

	if (env->me_map) {
		munmap(env->me_map, env->me_mapsize);
	}
	if (env->me_mfd != INVALID_HANDLE_VALUE) (void)close(env->me_mfd);
	if (env->me_fd != INVALID_HANDLE_VALUE) (void)close(env->me_fd);
	if (env->me_txns) {
		i32 pid = getpid();
		for (i = env->me_close_readers; --i >= 0;)
			if (env->me_txns->mti_readers[i].mr_pid == pid)
				env->me_txns->mti_readers[i].mr_pid = 0;
		munmap((void *)env->me_txns,
		       (env->me_maxreaders - 1) * sizeof(MDB_reader) +
			   sizeof(MDB_txninfo));
	}
	if (env->me_lfd != INVALID_HANDLE_VALUE) {
		(void)close(env->me_lfd);
	}

	env->me_flags &= ~(MDB_ENV_ACTIVE | MDB_ENV_TXKEY);
}

void mdb_env_close(MDB_env *env) {
	MDB_page *dp;

	if (env == NULL) return;

	while ((dp = env->me_dpages) != NULL) {
		env->me_dpages = dp->mp_next;
		release(dp);
	}

	mdb_env_close0(env, 0);
	release(env);
}

STATIC i32 mdb_cmp_long(const MDB_val *a, const MDB_val *b) {
	return (*(u64 *)a->mv_data < *(u64 *)b->mv_data)
		   ? -1
		   : *(u64 *)a->mv_data > *(u64 *)b->mv_data;
}

STATIC i32 mdb_cmp_i32(const MDB_val *a, const MDB_val *b) {
	return (*(u32 *)a->mv_data < *(u32 *)b->mv_data)
		   ? -1
		   : *(u32 *)a->mv_data > *(u32 *)b->mv_data;
}

STATIC i32 mdb_cmp_ci32(const MDB_val *a, const MDB_val *b) {
	u16 *u, *c;
	i32 x;

	u = (u16 *)((u8 *)a->mv_data + a->mv_size);
	c = (u16 *)((u8 *)b->mv_data + a->mv_size);
	do {
		x = *--u - *--c;
	} while (!x && u > (u16 *)a->mv_data);
	return x;
}

STATIC i32 mdb_cmp_memn(const MDB_val *a, const MDB_val *b) {
	i32 diff;
	i64 len_diff;
	u32 len;

	len = a->mv_size;
	len_diff = (i64)a->mv_size - (i64)b->mv_size;
	if (len_diff > 0) {
		len = b->mv_size;
		len_diff = 1;
	}

	diff = memcmp(a->mv_data, b->mv_data, len);
	return diff ? diff : len_diff < 0 ? -1 : len_diff;
}

STATIC i32 mdb_cmp_memnr(const MDB_val *a, const MDB_val *b) {
	const u8 *p1, *p2, *p1_lim;
	i64 len_diff;
	i32 diff;

	p1_lim = (const u8 *)a->mv_data;
	p1 = (const u8 *)a->mv_data + a->mv_size;
	p2 = (const u8 *)b->mv_data + b->mv_size;

	len_diff = (i64)a->mv_size - (i64)b->mv_size;
	if (len_diff > 0) {
		p1_lim += len_diff;
		len_diff = 1;
	}

	while (p1 > p1_lim) {
		diff = *--p1 - *--p2;
		if (diff) return diff;
	}
	return len_diff < 0 ? -1 : len_diff;
}

STATIC MDB_node *mdb_node_search(MDB_cursor *mc, MDB_val *key, i32 *exactp) {
	u32 i = 0, nkeys;
	i32 low, high;
	i32 rc = 0;
	MDB_page *mp = mc->mc_pg[mc->mc_top];
	MDB_node *node = NULL;
	MDB_val nodekey;
	MDB_cmp_func *cmp;

	nkeys = NUMKEYS(mp);

	low = IS_LEAF(mp) ? 0 : 1;
	high = nkeys - 1;
	cmp = mc->mc_dbx->md_cmp;

	if (cmp == mdb_cmp_ci32 && IS_BRANCH(mp)) {
		if (NODEPTR(mp, 1)->mn_ksize == sizeof(u64))
			cmp = mdb_cmp_long;
		else
			cmp = mdb_cmp_i32;
	}

	if (IS_LEAF2(mp)) {
		nodekey.mv_size = mc->mc_db->md_pad;
		node = NODEPTR(mp, 0);
		while (low <= high) {
			i = (low + high) >> 1;
			nodekey.mv_data = LEAF2KEY(mp, i, nodekey.mv_size);
			rc = cmp(key, &nodekey);
			if (rc == 0) break;
			if (rc > 0)
				low = i + 1;
			else
				high = i - 1;
		}
	} else {
		while (low <= high) {
			i = (low + high) >> 1;

			node = NODEPTR(mp, i);
			nodekey.mv_size = NODEKSZ(node);
			nodekey.mv_data = NODEKEY(node);

			rc = cmp(key, &nodekey);
			if (rc == 0) break;
			if (rc > 0)
				low = i + 1;
			else
				high = i - 1;
		}
	}

	if (rc > 0) {
		i++;
		if (!IS_LEAF2(mp)) node = NODEPTR(mp, i);
	}
	if (exactp) *exactp = (rc == 0 && nkeys > 0);
	mc->mc_ki[mc->mc_top] = i;
	if (i >= nkeys) return NULL;
	return node;
}

STATIC void mdb_cursor_pop(MDB_cursor *mc) {
	if (mc->mc_snum) {
		mc->mc_snum--;
		if (mc->mc_snum) {
			mc->mc_top--;
		} else {
			mc->mc_flags &= ~C_INITIALIZED;
		}
	}
}

STATIC i32 mdb_cursor_push(MDB_cursor *mc, MDB_page *mp) {
	if (mc->mc_snum >= CURSOR_STACK) {
		mc->mc_txn->mt_flags |= MDB_TXN_ERROR;
		return MDB_CURSOR_FULL;
	}

	mc->mc_top = mc->mc_snum++;
	mc->mc_pg[mc->mc_top] = mp;
	mc->mc_ki[mc->mc_top] = 0;

	return MDB_SUCCESS;
}

STATIC i32 mdb_page_get(MDB_cursor *mc, u64 pgno, MDB_page **ret, i32 *lvl) {
	MDB_txn *txn = mc->mc_txn;
	MDB_page *p = NULL;
	i32 level;

	if (!(mc->mc_flags & (C_ORIG_RDONLY | C_WRITEMAP))) {
		MDB_txn *tx2 = txn;
		level = 1;
		do {
			MDB_ID2L dl = tx2->mt_u.dirty_list;
			u32 x;
			if (tx2->mt_spill_pgs) {
				MDB_ID pn = pgno << 1;
				x = mdb_midl_search(tx2->mt_spill_pgs, pn);
				if (x <= tx2->mt_spill_pgs[0] &&
				    tx2->mt_spill_pgs[x] == pn) {
					goto mapped;
				}
			}
			if (dl[0].mid) {
				u32 x = mdb_mid2l_search(dl, pgno);
				if (x <= dl[0].mid && dl[x].mid == pgno) {
					p = dl[x].mptr;
					goto done;
				}
			}
			level++;
		} while ((tx2 = tx2->mt_parent) != NULL);
	}

	if (pgno >= txn->mt_next_pgno) {
		txn->mt_flags |= MDB_TXN_ERROR;
		return MDB_PAGE_NOTFOUND;
	}

	level = 0;

mapped: {
	MDB_env *env = txn->mt_env;
	p = (MDB_page *)(env->me_map + env->me_psize * pgno);
}

done:
	*ret = p;
	if (lvl) *lvl = level;
	return MDB_SUCCESS;
}

STATIC i32 mdb_page_search_root(MDB_cursor *mc, MDB_val *key, i32 flags) {
	MDB_page *mp = mc->mc_pg[mc->mc_top];
	i32 rc;

	while (IS_BRANCH(mp)) {
		MDB_node *node;
		u16 i;

		mdb_cassert(mc, !mc->mc_dbi || NUMKEYS(mp) > 1);

		if (flags & (MDB_PS_FIRST | MDB_PS_LAST)) {
			i = 0;
			if (flags & MDB_PS_LAST) {
				i = NUMKEYS(mp) - 1;
				if (mc->mc_flags & C_INITIALIZED) {
					if (mc->mc_ki[mc->mc_top] == i) {
						mc->mc_top = mc->mc_snum++;
						mp = mc->mc_pg[mc->mc_top];
						goto ready;
					}
				}
			}
		} else {
			i32 exact;
			node = mdb_node_search(mc, key, &exact);
			if (node == NULL)
				i = NUMKEYS(mp) - 1;
			else {
				i = mc->mc_ki[mc->mc_top];
				if (!exact) {
					mdb_cassert(mc, i > 0);
					i--;
				}
			}
		}

		mdb_cassert(mc, i < NUMKEYS(mp));
		node = NODEPTR(mp, i);

		if ((rc = mdb_page_get(mc, NODEPGNO(node), &mp, NULL)) != 0)
			return rc;

		mc->mc_ki[mc->mc_top] = i;
		if ((rc = mdb_cursor_push(mc, mp))) return rc;

	ready:
		if (flags & MDB_PS_MODIFY) {
			if ((rc = mdb_page_touch(mc)) != 0) return rc;
			mp = mc->mc_pg[mc->mc_top];
		}
	}

	if (!IS_LEAF(mp)) {
		mc->mc_txn->mt_flags |= MDB_TXN_ERROR;
		return MDB_CORRUPTED;
	}

	mc->mc_flags |= C_INITIALIZED;
	mc->mc_flags &= ~C_EOF;

	return MDB_SUCCESS;
}

STATIC i32 mdb_page_search_lowest(MDB_cursor *mc) {
	MDB_page *mp = mc->mc_pg[mc->mc_top];
	MDB_node *node = NODEPTR(mp, 0);
	i32 rc;

	if ((rc = mdb_page_get(mc, NODEPGNO(node), &mp, NULL)) != 0) return rc;

	mc->mc_ki[mc->mc_top] = 0;
	if ((rc = mdb_cursor_push(mc, mp))) return rc;
	return mdb_page_search_root(mc, NULL, MDB_PS_FIRST);
}

STATIC i32 mdb_page_search(MDB_cursor *mc, MDB_val *key, i32 flags) {
	i32 rc;
	u64 root;

	if (mc->mc_txn->mt_flags & MDB_TXN_BLOCKED) {
		return MDB_BAD_TXN;
	} else {
		if (*mc->mc_dbflag & DB_STALE) {
			MDB_cursor mc2;
			if (TXN_DBI_CHANGED(mc->mc_txn, mc->mc_dbi))
				return MDB_BAD_DBI;
			mdb_cursor_init(&mc2, mc->mc_txn, MAIN_DBI, NULL);
			rc = mdb_page_search(&mc2, &mc->mc_dbx->md_name, 0);
			if (rc) return rc;
			{
				MDB_val data;
				i32 exact = 0;
				u16 flags;
				MDB_node *leaf = mdb_node_search(
				    &mc2, &mc->mc_dbx->md_name, &exact);
				if (!exact) return MDB_BAD_DBI;
				if ((leaf->mn_flags &
				     (F_DUPDATA | F_SUBDATA)) != F_SUBDATA)
					return MDB_INCOMPATIBLE;
				rc = mdb_node_read(&mc2, leaf, &data);
				if (rc) return rc;
				memcpy(&flags,
				       ((u8 *)data.mv_data +
					offsetof(MDB_db, md_flags)),
				       sizeof(u16));
				if ((mc->mc_db->md_flags & PERSISTENT_FLAGS) !=
				    flags)
					return MDB_INCOMPATIBLE;
				memcpy(mc->mc_db, data.mv_data, sizeof(MDB_db));
			}
			*mc->mc_dbflag &= ~DB_STALE;
		}
		root = mc->mc_db->md_root;

		if (root == P_INVALID) {
			return MDB_NOTFOUND;
		}
	}

	mdb_cassert(mc, root > 1);
	if (!mc->mc_pg[0] || mc->mc_pg[0]->mp_pgno != root) {
		if ((rc = mdb_page_get(mc, root, &mc->mc_pg[0], NULL)) != 0)
			return rc;
	}

	mc->mc_snum = 1;
	mc->mc_top = 0;

	if (flags & MDB_PS_MODIFY) {
		if ((rc = mdb_page_touch(mc))) return rc;
	}

	if (flags & MDB_PS_ROOTONLY) return MDB_SUCCESS;

	return mdb_page_search_root(mc, key, flags);
}

STATIC i32 mdb_ovpage_free(MDB_cursor *mc, MDB_page *mp) {
	MDB_txn *txn = mc->mc_txn;
	u64 pg = mp->mp_pgno;
	u32 x = 0, ovpages = mp->mp_pages;
	MDB_env *env = txn->mt_env;
	MDB_IDL sl = txn->mt_spill_pgs;
	MDB_ID pn = pg << 1;
	i32 rc;

	if (env->me_pghead && !txn->mt_parent &&
	    ((mp->mp_flags & P_DIRTY) ||
	     (sl && (x = mdb_midl_search(sl, pn)) <= sl[0] && sl[x] == pn))) {
		u32 i, j;
		u64 *mop;
		MDB_ID2 *dl, ix, iy;
		rc = mdb_midl_need(&env->me_pghead, ovpages);
		if (rc) return rc;
		if (!(mp->mp_flags & P_DIRTY)) {
			if (x == sl[0])
				sl[0]--;
			else
				sl[x] |= 1;
			goto release;
		}
		dl = txn->mt_u.dirty_list;
		x = dl[0].mid--;
		for (ix = dl[x]; ix.mptr != mp; ix = iy) {
			if (x > 1) {
				x--;
				iy = dl[x];
				dl[x] = ix;
			} else {
				mdb_cassert(mc, x > 1);
				j = ++(dl[0].mid);
				dl[j] = ix;
				txn->mt_flags |= MDB_TXN_ERROR;
				return MDB_PROBLEM;
			}
		}
		txn->mt_dirty_room++;
		if (!(env->me_flags & MDB_WRITEMAP)) mdb_dpage_free(env, mp);
	release:
		mop = env->me_pghead;
		j = mop[0] + ovpages;
		for (i = mop[0]; i && mop[i] < pg; i--) mop[j--] = mop[i];
		while (j > i) mop[j--] = pg++;
		mop[0] += ovpages;
	} else {
		rc = mdb_midl_append_range(&txn->mt_free_pgs, pg, ovpages);
		if (rc) return rc;
	}
	mc->mc_db->md_overflow_pages -= ovpages;
	return 0;
}

STATIC i32 mdb_node_read(MDB_cursor *mc, MDB_node *leaf, MDB_val *data) {
	MDB_page *omp;
	u64 pgno;
	i32 rc;

	if (MC_OVPG(mc)) {
		MDB_PAGE_UNREF(mc->mc_txn, MC_OVPG(mc));
		MC_SET_OVPG(mc, NULL);
	}
	if (!F_ISSET(leaf->mn_flags, F_BIGDATA)) {
		data->mv_size = NODEDSZ(leaf);
		data->mv_data = NODEDATA(leaf);
		return MDB_SUCCESS;
	}

	data->mv_size = NODEDSZ(leaf);
	memcpy(&pgno, NODEDATA(leaf), sizeof(pgno));
	if ((rc = mdb_page_get(mc, pgno, &omp, NULL)) != 0) {
		return rc;
	}
	data->mv_data = METADATA(omp);
	MC_SET_OVPG(mc, omp);

	return MDB_SUCCESS;
}

i32 mdb_get(MDB_txn *txn, MDB_dbi dbi, MDB_val *key, MDB_val *data) {
	MDB_cursor mc;
	MDB_xcursor mx;
	i32 exact = 0, rc;

	if (!key || !data || !TXN_DBI_EXIST(txn, dbi, DB_USRVALID))
		return EINVAL;

	if (txn->mt_flags & MDB_TXN_BLOCKED) return MDB_BAD_TXN;

	mdb_cursor_init(&mc, txn, dbi, &mx);
	rc = mdb_cursor_set(&mc, key, data, MDB_SET, &exact);
	MDB_CURSOR_UNREF(&mc, 1);
	return rc;
}

STATIC i32 mdb_cursor_sibling(MDB_cursor *mc, i32 move_right) {
	i32 rc;
	MDB_node *indx;
	MDB_page *mp;

	if (mc->mc_snum < 2) {
		return MDB_NOTFOUND;
	}

	mdb_cursor_pop(mc);

	if (move_right
		? (mc->mc_ki[mc->mc_top] + 1u >= NUMKEYS(mc->mc_pg[mc->mc_top]))
		: (mc->mc_ki[mc->mc_top] == 0)) {
		if ((rc = mdb_cursor_sibling(mc, move_right)) != MDB_SUCCESS) {
			mc->mc_top++;
			mc->mc_snum++;
			return rc;
		}
	} else {
		if (move_right)
			mc->mc_ki[mc->mc_top]++;
		else
			mc->mc_ki[mc->mc_top]--;
	}
	mdb_cassert(mc, IS_BRANCH(mc->mc_pg[mc->mc_top]));

	MDB_PAGE_UNREF(mc->mc_txn, op);

	indx = NODEPTR(mc->mc_pg[mc->mc_top], mc->mc_ki[mc->mc_top]);
	if ((rc = mdb_page_get(mc, NODEPGNO(indx), &mp, NULL)) != 0) {
		mc->mc_flags &= ~(C_INITIALIZED | C_EOF);
		return rc;
	}

	mdb_cursor_push(mc, mp);
	if (!move_right) mc->mc_ki[mc->mc_top] = NUMKEYS(mp) - 1;

	return MDB_SUCCESS;
}

STATIC i32 mdb_cursor_next(MDB_cursor *mc, MDB_val *key, MDB_val *data,
			   MDB_cursor_op op) {
	MDB_page *mp;
	MDB_node *leaf;
	i32 rc;

	if ((mc->mc_flags & C_DEL && op == MDB_NEXT_DUP)) return MDB_NOTFOUND;

	if (!(mc->mc_flags & C_INITIALIZED))
		return mdb_cursor_first(mc, key, data);

	mp = mc->mc_pg[mc->mc_top];

	if (mc->mc_flags & C_EOF) {
		if (mc->mc_ki[mc->mc_top] >= NUMKEYS(mp) - 1)
			return MDB_NOTFOUND;
		mc->mc_flags ^= C_EOF;
	}

	if (mc->mc_db->md_flags & MDB_DUPSORT) {
		leaf = NODEPTR(mp, mc->mc_ki[mc->mc_top]);
		if (F_ISSET(leaf->mn_flags, F_DUPDATA)) {
			if (op == MDB_NEXT || op == MDB_NEXT_DUP) {
				rc = mdb_cursor_next(&mc->mc_xcursor->mx_cursor,
						     data, NULL, MDB_NEXT);
				if (op != MDB_NEXT || rc != MDB_NOTFOUND) {
					if (rc == MDB_SUCCESS)
						MDB_GET_KEY(leaf, key);
					return rc;
				}
			} else {
				MDB_CURSOR_UNREF(&mc->mc_xcursor->mx_cursor, 0);
			}
		} else {
			mc->mc_xcursor->mx_cursor.mc_flags &=
			    ~(C_INITIALIZED | C_EOF);
			if (op == MDB_NEXT_DUP) return MDB_NOTFOUND;
		}
	}

	if (mc->mc_flags & C_DEL) {
		mc->mc_flags ^= C_DEL;
		goto skip;
	}

	if (mc->mc_ki[mc->mc_top] + 1u >= NUMKEYS(mp)) {
		if ((rc = mdb_cursor_sibling(mc, 1)) != MDB_SUCCESS) {
			mc->mc_flags |= C_EOF;
			return rc;
		}
		mp = mc->mc_pg[mc->mc_top];
	} else
		mc->mc_ki[mc->mc_top]++;

skip:
	if (IS_LEAF2(mp)) {
		key->mv_size = mc->mc_db->md_pad;
		key->mv_data =
		    LEAF2KEY(mp, mc->mc_ki[mc->mc_top], key->mv_size);
		return MDB_SUCCESS;
	}

	mdb_cassert(mc, IS_LEAF(mp));
	leaf = NODEPTR(mp, mc->mc_ki[mc->mc_top]);

	if (F_ISSET(leaf->mn_flags, F_DUPDATA)) {
		mdb_xcursor_init1(mc, leaf);
		rc = mdb_cursor_first(&mc->mc_xcursor->mx_cursor, data, NULL);
		if (rc != MDB_SUCCESS) return rc;
	} else if (data) {
		if ((rc = mdb_node_read(mc, leaf, data)) != MDB_SUCCESS)
			return rc;
	}

	MDB_GET_KEY(leaf, key);
	return MDB_SUCCESS;
}

STATIC i32 mdb_cursor_prev(MDB_cursor *mc, MDB_val *key, MDB_val *data,
			   MDB_cursor_op op) {
	MDB_page *mp;
	MDB_node *leaf;
	i32 rc;

	if (!(mc->mc_flags & C_INITIALIZED)) {
		rc = mdb_cursor_last(mc, key, data);
		if (rc) return rc;
		mc->mc_ki[mc->mc_top]++;
	}

	mp = mc->mc_pg[mc->mc_top];

	if ((mc->mc_db->md_flags & MDB_DUPSORT) &&
	    mc->mc_ki[mc->mc_top] < NUMKEYS(mp)) {
		leaf = NODEPTR(mp, mc->mc_ki[mc->mc_top]);
		if (F_ISSET(leaf->mn_flags, F_DUPDATA)) {
			if (op == MDB_PREV || op == MDB_PREV_DUP) {
				rc = mdb_cursor_prev(&mc->mc_xcursor->mx_cursor,
						     data, NULL, MDB_PREV);
				if (op != MDB_PREV || rc != MDB_NOTFOUND) {
					if (rc == MDB_SUCCESS) {
						MDB_GET_KEY(leaf, key);
						mc->mc_flags &= ~C_EOF;
					}
					return rc;
				}
			} else {
				MDB_CURSOR_UNREF(&mc->mc_xcursor->mx_cursor, 0);
			}
		} else {
			mc->mc_xcursor->mx_cursor.mc_flags &=
			    ~(C_INITIALIZED | C_EOF);
			if (op == MDB_PREV_DUP) return MDB_NOTFOUND;
		}
	}

	mc->mc_flags &= ~(C_EOF | C_DEL);

	if (mc->mc_ki[mc->mc_top] == 0) {
		if ((rc = mdb_cursor_sibling(mc, 0)) != MDB_SUCCESS) {
			return rc;
		}
		mp = mc->mc_pg[mc->mc_top];
		mc->mc_ki[mc->mc_top] = NUMKEYS(mp) - 1;
	} else
		mc->mc_ki[mc->mc_top]--;

	if (!IS_LEAF(mp)) return MDB_CORRUPTED;

	if (IS_LEAF2(mp)) {
		key->mv_size = mc->mc_db->md_pad;
		key->mv_data =
		    LEAF2KEY(mp, mc->mc_ki[mc->mc_top], key->mv_size);
		return MDB_SUCCESS;
	}

	leaf = NODEPTR(mp, mc->mc_ki[mc->mc_top]);

	if (F_ISSET(leaf->mn_flags, F_DUPDATA)) {
		mdb_xcursor_init1(mc, leaf);
		rc = mdb_cursor_last(&mc->mc_xcursor->mx_cursor, data, NULL);
		if (rc != MDB_SUCCESS) return rc;
	} else if (data) {
		if ((rc = mdb_node_read(mc, leaf, data)) != MDB_SUCCESS)
			return rc;
	}

	MDB_GET_KEY(leaf, key);
	return MDB_SUCCESS;
}

STATIC i32 mdb_cursor_set(MDB_cursor *mc, MDB_val *key, MDB_val *data,
			  MDB_cursor_op op, i32 *exactp) {
	i32 rc;
	MDB_page *mp;
	MDB_node *leaf = NULL;

	if (key->mv_size == 0) return MDB_BAD_VALSIZE;

	if (mc->mc_xcursor) {
		MDB_CURSOR_UNREF(&mc->mc_xcursor->mx_cursor, 0);
		mc->mc_xcursor->mx_cursor.mc_flags &= ~(C_INITIALIZED | C_EOF);
	}

	if (mc->mc_flags & C_INITIALIZED) {
		MDB_val nodekey;

		mp = mc->mc_pg[mc->mc_top];
		if (!NUMKEYS(mp)) {
			mc->mc_ki[mc->mc_top] = 0;
			return MDB_NOTFOUND;
		}
		if (MP_FLAGS(mp) & P_LEAF2) {
			nodekey.mv_size = mc->mc_db->md_pad;
			nodekey.mv_data = LEAF2KEY(mp, 0, nodekey.mv_size);
		} else {
			leaf = NODEPTR(mp, 0);
			MDB_GET_KEY2(leaf, nodekey);
		}
		rc = mc->mc_dbx->md_cmp(key, &nodekey);
		if (rc == 0) {
			mc->mc_ki[mc->mc_top] = 0;
			if (exactp) *exactp = 1;
			goto set1;
		}
		if (rc > 0) {
			u32 i;
			u32 nkeys = NUMKEYS(mp);
			if (nkeys > 1) {
				if (MP_FLAGS(mp) & P_LEAF2) {
					nodekey.mv_data = LEAF2KEY(
					    mp, nkeys - 1, nodekey.mv_size);
				} else {
					leaf = NODEPTR(mp, nkeys - 1);
					MDB_GET_KEY2(leaf, nodekey);
				}
				rc = mc->mc_dbx->md_cmp(key, &nodekey);
				if (rc == 0) {
					mc->mc_ki[mc->mc_top] = nkeys - 1;
					if (exactp) *exactp = 1;
					goto set1;
				}
				if (rc < 0) {
					if (mc->mc_ki[mc->mc_top] <
					    NUMKEYS(mp)) {
						if (MP_FLAGS(mp) & P_LEAF2) {
							nodekey
							    .mv_data = LEAF2KEY(
							    mp,
							    mc->mc_ki
								[mc->mc_top],
							    nodekey.mv_size);
						} else {
							leaf = NODEPTR(
							    mp,
							    mc->mc_ki
								[mc->mc_top]);
							MDB_GET_KEY2(leaf,
								     nodekey);
						}
						rc = mc->mc_dbx->md_cmp(
						    key, &nodekey);
						if (rc == 0) {
							if (exactp) *exactp = 1;
							goto set1;
						}
					}
					rc = 0;
					mc->mc_flags &= ~C_EOF;
					goto set2;
				}
			}
			for (i = 0; i < mc->mc_top; i++)
				if (mc->mc_ki[i] < NUMKEYS(mc->mc_pg[i]) - 1)
					break;
			if (i == mc->mc_top) {
				mc->mc_ki[mc->mc_top] = nkeys;
				return MDB_NOTFOUND;
			}
		}
		if (!mc->mc_top) {
			mc->mc_ki[mc->mc_top] = 0;
			if (op == MDB_SET_RANGE && !exactp) {
				rc = 0;
				goto set1;
			} else
				return MDB_NOTFOUND;
		}
	} else {
		mc->mc_pg[0] = 0;
	}

	rc = mdb_page_search(mc, key, 0);
	if (rc != MDB_SUCCESS) return rc;

	mp = mc->mc_pg[mc->mc_top];
	mdb_cassert(mc, IS_LEAF(mp));

set2:
	leaf = mdb_node_search(mc, key, exactp);
	if (exactp != NULL && !*exactp) {
		return MDB_NOTFOUND;
	}

	if (leaf == NULL) {
		if ((rc = mdb_cursor_sibling(mc, 1)) != MDB_SUCCESS) {
			mc->mc_flags |= C_EOF;
			return rc;
		}
		mp = mc->mc_pg[mc->mc_top];
		mdb_cassert(mc, IS_LEAF(mp));
		leaf = NODEPTR(mp, 0);
	}

set1:
	mc->mc_flags |= C_INITIALIZED;
	mc->mc_flags &= ~C_EOF;

	if (IS_LEAF2(mp)) {
		if (op == MDB_SET_RANGE || op == MDB_SET_KEY) {
			key->mv_size = mc->mc_db->md_pad;
			key->mv_data =
			    LEAF2KEY(mp, mc->mc_ki[mc->mc_top], key->mv_size);
		}
		return MDB_SUCCESS;
	}

	if (F_ISSET(leaf->mn_flags, F_DUPDATA)) {
		mdb_xcursor_init1(mc, leaf);
		if (op == MDB_SET || op == MDB_SET_KEY || op == MDB_SET_RANGE) {
			rc = mdb_cursor_first(&mc->mc_xcursor->mx_cursor, data,
					      NULL);
		} else {
			i32 ex2, *ex2p;
			if (op == MDB_GET_BOTH) {
				ex2p = &ex2;
				ex2 = 0;
			} else {
				ex2p = NULL;
			}
			rc = mdb_cursor_set(&mc->mc_xcursor->mx_cursor, data,
					    NULL, MDB_SET_RANGE, ex2p);
			if (rc != MDB_SUCCESS) return rc;
		}
	} else if (data) {
		if (op == MDB_GET_BOTH || op == MDB_GET_BOTH_RANGE) {
			MDB_val olddata;
			MDB_cmp_func *dcmp;
			if ((rc = mdb_node_read(mc, leaf, &olddata)) !=
			    MDB_SUCCESS)
				return rc;
			dcmp = mc->mc_dbx->md_dcmp;
			if (NEED_CMP_CLONG(dcmp, olddata.mv_size))
				dcmp = mdb_cmp_clong;
			rc = dcmp(data, &olddata);
			if (rc) {
				if (op == MDB_GET_BOTH || rc > 0)
					return MDB_NOTFOUND;
				rc = 0;
			}
			*data = olddata;

		} else {
			if (mc->mc_xcursor)
				mc->mc_xcursor->mx_cursor.mc_flags &=
				    ~(C_INITIALIZED | C_EOF);
			if ((rc = mdb_node_read(mc, leaf, data)) != MDB_SUCCESS)
				return rc;
		}
	}

	if (op == MDB_SET_RANGE || op == MDB_SET_KEY) MDB_GET_KEY(leaf, key);

	return rc;
}

STATIC i32 mdb_cursor_first(MDB_cursor *mc, MDB_val *key, MDB_val *data) {
	i32 rc;
	MDB_node *leaf;

	if (mc->mc_xcursor) {
		MDB_CURSOR_UNREF(&mc->mc_xcursor->mx_cursor, 0);
		mc->mc_xcursor->mx_cursor.mc_flags &= ~(C_INITIALIZED | C_EOF);
	}

	if (!(mc->mc_flags & C_INITIALIZED) || mc->mc_top) {
		rc = mdb_page_search(mc, NULL, MDB_PS_FIRST);
		if (rc != MDB_SUCCESS) return rc;
	}
	mdb_cassert(mc, IS_LEAF(mc->mc_pg[mc->mc_top]));

	leaf = NODEPTR(mc->mc_pg[mc->mc_top], 0);
	mc->mc_flags |= C_INITIALIZED;
	mc->mc_flags &= ~C_EOF;

	mc->mc_ki[mc->mc_top] = 0;

	if (IS_LEAF2(mc->mc_pg[mc->mc_top])) {
		if (key) {
			key->mv_size = mc->mc_db->md_pad;
			key->mv_data =
			    LEAF2KEY(mc->mc_pg[mc->mc_top], 0, key->mv_size);
		}
		return MDB_SUCCESS;
	}

	if (F_ISSET(leaf->mn_flags, F_DUPDATA)) {
		mdb_xcursor_init1(mc, leaf);
		rc = mdb_cursor_first(&mc->mc_xcursor->mx_cursor, data, NULL);
		if (rc) return rc;
	} else if (data) {
		if ((rc = mdb_node_read(mc, leaf, data)) != MDB_SUCCESS)
			return rc;
	}

	MDB_GET_KEY(leaf, key);
	return MDB_SUCCESS;
}

STATIC i32 mdb_cursor_last(MDB_cursor *mc, MDB_val *key, MDB_val *data) {
	i32 rc;
	MDB_node *leaf;

	if (mc->mc_xcursor) {
		MDB_CURSOR_UNREF(&mc->mc_xcursor->mx_cursor, 0);
		mc->mc_xcursor->mx_cursor.mc_flags &= ~(C_INITIALIZED | C_EOF);
	}

	if (!(mc->mc_flags & C_INITIALIZED) || mc->mc_top) {
		rc = mdb_page_search(mc, NULL, MDB_PS_LAST);
		if (rc != MDB_SUCCESS) return rc;
	}
	mdb_cassert(mc, IS_LEAF(mc->mc_pg[mc->mc_top]));

	mc->mc_ki[mc->mc_top] = NUMKEYS(mc->mc_pg[mc->mc_top]) - 1;
	mc->mc_flags |= C_INITIALIZED | C_EOF;
	leaf = NODEPTR(mc->mc_pg[mc->mc_top], mc->mc_ki[mc->mc_top]);

	if (IS_LEAF2(mc->mc_pg[mc->mc_top])) {
		if (key) {
			key->mv_size = mc->mc_db->md_pad;
			key->mv_data =
			    LEAF2KEY(mc->mc_pg[mc->mc_top],
				     mc->mc_ki[mc->mc_top], key->mv_size);
		}
		return MDB_SUCCESS;
	}

	if (F_ISSET(leaf->mn_flags, F_DUPDATA)) {
		mdb_xcursor_init1(mc, leaf);
		rc = mdb_cursor_last(&mc->mc_xcursor->mx_cursor, data, NULL);
		if (rc) return rc;
	} else if (data) {
		if ((rc = mdb_node_read(mc, leaf, data)) != MDB_SUCCESS)
			return rc;
	}

	MDB_GET_KEY(leaf, key);
	return MDB_SUCCESS;
}

i32 mdb_cursor_get(MDB_cursor *mc, MDB_val *key, MDB_val *data,
		   MDB_cursor_op op) {
	i32 rc;
	i32 exact = 0;
	i32 (*mfunc)(MDB_cursor *mc, MDB_val *key, MDB_val *data);

	if (mc == NULL) return EINVAL;

	if (mc->mc_txn->mt_flags & MDB_TXN_BLOCKED) return MDB_BAD_TXN;

	switch (op) {
		case MDB_GET_CURRENT:
			if (!(mc->mc_flags & C_INITIALIZED)) {
				rc = EINVAL;
			} else {
				MDB_page *mp = mc->mc_pg[mc->mc_top];
				i32 nkeys = NUMKEYS(mp);
				if (!nkeys || mc->mc_ki[mc->mc_top] >= nkeys) {
					mc->mc_ki[mc->mc_top] = nkeys;
					rc = MDB_NOTFOUND;
					break;
				}
				rc = MDB_SUCCESS;
				if (IS_LEAF2(mp)) {
					key->mv_size = mc->mc_db->md_pad;
					key->mv_data =
					    LEAF2KEY(mp, mc->mc_ki[mc->mc_top],
						     key->mv_size);
				} else {
					MDB_node *leaf =
					    NODEPTR(mp, mc->mc_ki[mc->mc_top]);
					MDB_GET_KEY(leaf, key);
					if (data) {
						if (F_ISSET(leaf->mn_flags,
							    F_DUPDATA)) {
							rc = mdb_cursor_get(
							    &mc->mc_xcursor
								 ->mx_cursor,
							    data, NULL,
							    MDB_GET_CURRENT);
						} else {
							rc = mdb_node_read(
							    mc, leaf, data);
						}
					}
				}
			}
			break;
		case MDB_GET_BOTH:
		case MDB_GET_BOTH_RANGE:
			if (data == NULL) {
				rc = EINVAL;
				break;
			}
			if (mc->mc_xcursor == NULL) {
				rc = MDB_INCOMPATIBLE;
				break;
			}
			/* FALLTHRU */
		case MDB_SET:
		case MDB_SET_KEY:
		case MDB_SET_RANGE:
			if (key == NULL) {
				rc = EINVAL;
			} else {
				rc = mdb_cursor_set(
				    mc, key, data, op,
				    op == MDB_SET_RANGE ? NULL : &exact);
			}
			break;
		case MDB_GET_MULTIPLE:
			if (data == NULL || !(mc->mc_flags & C_INITIALIZED)) {
				rc = EINVAL;
				break;
			}
			if (!(mc->mc_db->md_flags & MDB_DUPFIXED)) {
				rc = MDB_INCOMPATIBLE;
				break;
			}
			rc = MDB_SUCCESS;
			if (!(mc->mc_xcursor->mx_cursor.mc_flags &
			      C_INITIALIZED) ||
			    (mc->mc_xcursor->mx_cursor.mc_flags & C_EOF))
				break;
			goto fetchm;
		case MDB_NEXT_MULTIPLE:
			if (data == NULL) {
				rc = EINVAL;
				break;
			}
			if (!(mc->mc_db->md_flags & MDB_DUPFIXED)) {
				rc = MDB_INCOMPATIBLE;
				break;
			}
			rc = mdb_cursor_next(mc, key, data, MDB_NEXT_DUP);
			if (rc == MDB_SUCCESS) {
				if (mc->mc_xcursor->mx_cursor.mc_flags &
				    C_INITIALIZED) {
					MDB_cursor *mx;
				fetchm:
					mx = &mc->mc_xcursor->mx_cursor;
					data->mv_size =
					    NUMKEYS(mx->mc_pg[mx->mc_top]) *
					    mx->mc_db->md_pad;
					data->mv_data =
					    METADATA(mx->mc_pg[mx->mc_top]);
					mx->mc_ki[mx->mc_top] =
					    NUMKEYS(mx->mc_pg[mx->mc_top]) - 1;
				} else {
					rc = MDB_NOTFOUND;
				}
			}
			break;
		case MDB_PREV_MULTIPLE:
			if (data == NULL) {
				rc = EINVAL;
				break;
			}
			if (!(mc->mc_db->md_flags & MDB_DUPFIXED)) {
				rc = MDB_INCOMPATIBLE;
				break;
			}
			if (!(mc->mc_flags & C_INITIALIZED))
				rc = mdb_cursor_last(mc, key, data);
			else
				rc = MDB_SUCCESS;
			if (rc == MDB_SUCCESS) {
				MDB_cursor *mx = &mc->mc_xcursor->mx_cursor;
				if (mx->mc_flags & C_INITIALIZED) {
					rc = mdb_cursor_sibling(mx, 0);
					if (rc == MDB_SUCCESS) goto fetchm;
				} else {
					rc = MDB_NOTFOUND;
				}
			}
			break;
		case MDB_NEXT:
		case MDB_NEXT_DUP:
		case MDB_NEXT_NODUP:
			rc = mdb_cursor_next(mc, key, data, op);
			break;
		case MDB_PREV:
		case MDB_PREV_DUP:
		case MDB_PREV_NODUP:
			rc = mdb_cursor_prev(mc, key, data, op);
			break;
		case MDB_FIRST:
			rc = mdb_cursor_first(mc, key, data);
			break;
		case MDB_FIRST_DUP:
			mfunc = mdb_cursor_first;
		mmove:
			if (data == NULL || !(mc->mc_flags & C_INITIALIZED)) {
				rc = EINVAL;
				break;
			}
			if (mc->mc_xcursor == NULL) {
				rc = MDB_INCOMPATIBLE;
				break;
			}
			if (mc->mc_ki[mc->mc_top] >=
			    NUMKEYS(mc->mc_pg[mc->mc_top])) {
				mc->mc_ki[mc->mc_top] =
				    NUMKEYS(mc->mc_pg[mc->mc_top]);
				rc = MDB_NOTFOUND;
				break;
			}
			mc->mc_flags &= ~C_EOF;
			{
				MDB_node *leaf = NODEPTR(mc->mc_pg[mc->mc_top],
							 mc->mc_ki[mc->mc_top]);
				if (!F_ISSET(leaf->mn_flags, F_DUPDATA)) {
					MDB_GET_KEY(leaf, key);
					rc = mdb_node_read(mc, leaf, data);
					break;
				}
			}
			if (!(mc->mc_xcursor->mx_cursor.mc_flags &
			      C_INITIALIZED)) {
				rc = EINVAL;
				break;
			}
			rc = mfunc(&mc->mc_xcursor->mx_cursor, data, NULL);
			break;
		case MDB_LAST:
			rc = mdb_cursor_last(mc, key, data);
			break;
		case MDB_LAST_DUP:
			mfunc = mdb_cursor_last;
			goto mmove;
		default:
			rc = EINVAL;
			break;
	}

	if (mc->mc_flags & C_DEL) mc->mc_flags ^= C_DEL;

	return rc;
}

STATIC i32 mdb_cursor_touch(MDB_cursor *mc) {
	i32 rc = MDB_SUCCESS;

	if (mc->mc_dbi >= CORE_DBS &&
	    !(*mc->mc_dbflag & (DB_DIRTY | DB_DUPDATA))) {
		MDB_cursor mc2;
		MDB_xcursor mcx;
		if (TXN_DBI_CHANGED(mc->mc_txn, mc->mc_dbi)) return MDB_BAD_DBI;
		mdb_cursor_init(&mc2, mc->mc_txn, MAIN_DBI, &mcx);
		rc = mdb_page_search(&mc2, &mc->mc_dbx->md_name, MDB_PS_MODIFY);
		if (rc) return rc;
		*mc->mc_dbflag |= DB_DIRTY;
	}
	mc->mc_top = 0;
	if (mc->mc_snum) {
		do {
			rc = mdb_page_touch(mc);
		} while (!rc && ++(mc->mc_top) < mc->mc_snum);
		mc->mc_top = mc->mc_snum - 1;
	}
	return rc;
}

STATIC i32 _mdb_cursor_put(MDB_cursor *mc, MDB_val *key, MDB_val *data,
			   u32 flags) {
	MDB_env *env;
	MDB_node *leaf = NULL;
	MDB_page *fp, *mp, *sub_root = NULL;
	u16 fp_flags;
	MDB_val xdata, *rdata, dkey, olddata;
	MDB_db dummy;
	i32 do_sub = 0, insert_key, insert_data;
	u32 mcount = 0, dcount = 0, nospill;
	u64 nsize;
	i32 rc, rc2;
	u32 nflags;

	if (mc == NULL || key == NULL) return EINVAL;

	env = mc->mc_txn->mt_env;

	if (flags & MDB_MULTIPLE) {
		dcount = data[1].mv_size;
		data[1].mv_size = 0;
		if (!F_ISSET(mc->mc_db->md_flags, MDB_DUPFIXED))
			return MDB_INCOMPATIBLE;
	}

	nospill = flags & MDB_NOSPILL;
	flags &= ~MDB_NOSPILL;

	if (mc->mc_txn->mt_flags & (MDB_TXN_RDONLY | MDB_TXN_BLOCKED))
		return (mc->mc_txn->mt_flags & MDB_TXN_RDONLY) ? EACCES
							       : MDB_BAD_TXN;

	if (key->mv_size - 1 >= ENV_MAXKEY(env)) return MDB_BAD_VALSIZE;

#if U64_MAX > MAXDATASIZE
	if (data->mv_size > ((mc->mc_db->md_flags & MDB_DUPSORT)
				 ? ENV_MAXKEY(env)
				 : MAXDATASIZE))
		return MDB_BAD_VALSIZE;
#else
	if ((mc->mc_db->md_flags & MDB_DUPSORT) &&
	    data->mv_size > ENV_MAXKEY(env))
		return MDB_BAD_VALSIZE;
#endif

	dkey.mv_size = 0;

	if (flags & MDB_CURRENT) {
		if (!(mc->mc_flags & C_INITIALIZED)) return EINVAL;
		rc = MDB_SUCCESS;
	} else if (mc->mc_db->md_root == P_INVALID) {
		mc->mc_snum = 0;
		mc->mc_top = 0;
		mc->mc_flags &= ~C_INITIALIZED;
		rc = MDB_NO_ROOT;
	} else {
		i32 exact = 0;
		MDB_val d2;
		if (flags & MDB_APPEND) {
			MDB_val k2;
			rc = mdb_cursor_last(mc, &k2, &d2);
			if (rc == 0) {
				rc = mc->mc_dbx->md_cmp(key, &k2);
				if (rc > 0) {
					rc = MDB_NOTFOUND;
					mc->mc_ki[mc->mc_top]++;
				} else {
					rc = MDB_KEYEXIST;
				}
			}
		} else {
			rc = mdb_cursor_set(mc, key, &d2, MDB_SET, &exact);
		}
		if ((flags & MDB_NOOVERWRITE) && rc == 0) {
			*data = d2;
			return MDB_KEYEXIST;
		}
		if (rc && rc != MDB_NOTFOUND) return rc;
	}

	if (mc->mc_flags & C_DEL) mc->mc_flags ^= C_DEL;

	if (!nospill) {
		if (flags & MDB_MULTIPLE) {
			rdata = &xdata;
			xdata.mv_size = data->mv_size * dcount;
		} else {
			rdata = data;
		}
		if ((rc2 = mdb_page_spill(mc, key, rdata))) return rc2;
	}

	if (rc == MDB_NO_ROOT) {
		MDB_page *np;
		if ((rc2 = mdb_page_new(mc, P_LEAF, 1, &np))) {
			return rc2;
		}
		mdb_cursor_push(mc, np);
		mc->mc_db->md_root = np->mp_pgno;
		mc->mc_db->md_depth++;
		*mc->mc_dbflag |= DB_DIRTY;
		if ((mc->mc_db->md_flags & (MDB_DUPSORT | MDB_DUPFIXED)) ==
		    MDB_DUPFIXED)
			MP_FLAGS(np) |= P_LEAF2;
		mc->mc_flags |= C_INITIALIZED;
	} else {
		rc2 = mdb_cursor_touch(mc);
		if (rc2) return rc2;
	}

	insert_key = insert_data = rc;
	if (insert_key) {
		if ((mc->mc_db->md_flags & MDB_DUPSORT) &&
		    LEAFSIZE(key, data) > env->me_nodemax) {
			fp_flags = P_LEAF | P_DIRTY;
			fp = env->me_pbuf;
			fp->mp_pad = data->mv_size;
			MP_LOWER(fp) = MP_UPPER(fp) = (PAGEHDRSZ - PAGEBASE);
			olddata.mv_size = PAGEHDRSZ;
			goto prep_subDB;
		}
	} else {
		if (IS_LEAF2(mc->mc_pg[mc->mc_top])) {
			u8 *ptr;
			u32 ksize = mc->mc_db->md_pad;
			if (key->mv_size != ksize) return MDB_BAD_VALSIZE;
			ptr = LEAF2KEY(mc->mc_pg[mc->mc_top],
				       mc->mc_ki[mc->mc_top], ksize);
			memcpy(ptr, key->mv_data, ksize);
		fix_parent:
			if (mc->mc_top && !mc->mc_ki[mc->mc_top]) {
				u16 dtop = 1;
				mc->mc_top--;
				while (mc->mc_top && !mc->mc_ki[mc->mc_top]) {
					mc->mc_top--;
					dtop++;
				}
				if (mc->mc_ki[mc->mc_top])
					rc2 = mdb_update_key(mc, key);
				else
					rc2 = MDB_SUCCESS;
				mc->mc_top += dtop;
				if (rc2) return rc2;
			}
			return MDB_SUCCESS;
		}

	more:
		leaf = NODEPTR(mc->mc_pg[mc->mc_top], mc->mc_ki[mc->mc_top]);
		olddata.mv_size = NODEDSZ(leaf);
		olddata.mv_data = NODEDATA(leaf);

		if (F_ISSET(mc->mc_db->md_flags, MDB_DUPSORT)) {
			u32 i, offset = 0;
			mp = fp = xdata.mv_data = env->me_pbuf;
			mp->mp_pgno = mc->mc_pg[mc->mc_top]->mp_pgno;

			if (!F_ISSET(leaf->mn_flags, F_DUPDATA)) {
				MDB_cmp_func *dcmp;
				if (flags == MDB_CURRENT) goto current;
				dcmp = mc->mc_dbx->md_dcmp;
				if (NEED_CMP_CLONG(dcmp, olddata.mv_size))
					dcmp = mdb_cmp_clong;
				if (!dcmp(data, &olddata)) {
					if (flags &
					    (MDB_NODUPDATA | MDB_APPENDDUP))
						return MDB_KEYEXIST;
					goto current;
				}

				dkey.mv_size = olddata.mv_size;
				dkey.mv_data = memcpy(fp + 1, olddata.mv_data,
						      olddata.mv_size);

				MP_FLAGS(fp) = P_LEAF | P_DIRTY | P_SUBP;
				MP_LOWER(fp) = (PAGEHDRSZ - PAGEBASE);
				xdata.mv_size =
				    PAGEHDRSZ + dkey.mv_size + data->mv_size;
				if (mc->mc_db->md_flags & MDB_DUPFIXED) {
					MP_FLAGS(fp) |= P_LEAF2;
					fp->mp_pad = data->mv_size;
					xdata.mv_size += 2 * data->mv_size;
				} else {
					xdata.mv_size +=
					    2 * (sizeof(u16) + NODESIZE) +
					    (dkey.mv_size & 1) +
					    (data->mv_size & 1);
				}
				MP_UPPER(fp) = xdata.mv_size - PAGEBASE;
				olddata.mv_size = xdata.mv_size;
			} else if (leaf->mn_flags & F_SUBDATA) {
				flags |= F_DUPDATA | F_SUBDATA;
				goto put_sub;
			} else {
				fp = olddata.mv_data;
				switch (flags) {
					default:
						if (!(mc->mc_db->md_flags &
						      MDB_DUPFIXED)) {
							offset =
							    EVEN(NODESIZE +
								 sizeof(u16) +
								 data->mv_size);
							break;
						}
						offset = fp->mp_pad;
						if (SIZELEFT(fp) < offset) {
							offset *= 4;
							break;
						}
						/* FALLTHRU */
					case MDB_CURRENT:
						MP_FLAGS(fp) |= P_DIRTY;
						COPY_PGNO(MP_PGNO(fp),
							  MP_PGNO(mp));
						mc->mc_xcursor->mx_cursor
						    .mc_pg[0] = fp;
						flags |= F_DUPDATA;
						goto put_sub;
				}
				xdata.mv_size = olddata.mv_size + offset;
			}

			fp_flags = MP_FLAGS(fp);
			if (NODESIZE + NODEKSZ(leaf) + xdata.mv_size >
			    env->me_nodemax) {
				fp_flags &= ~P_SUBP;
			prep_subDB:
				if (mc->mc_db->md_flags & MDB_DUPFIXED) {
					fp_flags |= P_LEAF2;
					dummy.md_pad = fp->mp_pad;
					dummy.md_flags = MDB_DUPFIXED;
					if (mc->mc_db->md_flags &
					    MDB_INTEGERDUP)
						dummy.md_flags |=
						    MDB_INTEGERKEY;
				} else {
					dummy.md_pad = 0;
					dummy.md_flags = 0;
				}
				dummy.md_depth = 1;
				dummy.md_branch_pages = 0;
				dummy.md_leaf_pages = 1;
				dummy.md_overflow_pages = 0;
				dummy.md_entries = NUMKEYS(fp);
				xdata.mv_size = sizeof(MDB_db);
				xdata.mv_data = &dummy;
				if ((rc = mdb_page_alloc(mc, 1, &mp)))
					return rc;
				offset = env->me_psize - olddata.mv_size;
				flags |= F_DUPDATA | F_SUBDATA;
				dummy.md_root = mp->mp_pgno;
				sub_root = mp;
			}
			if (mp != fp) {
				MP_FLAGS(mp) = fp_flags | P_DIRTY;
				MP_PAD(mp) = MP_PAD(fp);
				MP_LOWER(mp) = MP_LOWER(fp);
				MP_UPPER(mp) = MP_UPPER(fp) + offset;
				if (fp_flags & P_LEAF2) {
					memcpy(METADATA(mp), METADATA(fp),
					       NUMKEYS(fp) * fp->mp_pad);
				} else {
					memcpy(
					    (u8 *)mp + MP_UPPER(mp) + PAGEBASE,
					    (u8 *)fp + MP_UPPER(fp) + PAGEBASE,
					    olddata.mv_size - MP_UPPER(fp) -
						PAGEBASE);
					memcpy((u8 *)MP_PTRS(mp),
					       (u8 *)MP_PTRS(fp),
					       NUMKEYS(fp) *
						   sizeof(mp->mp_ptrs[0]));
					for (i = 0; i < NUMKEYS(fp); i++)
						mp->mp_ptrs[i] += offset;
				}
			}

			rdata = &xdata;
			flags |= F_DUPDATA;
			do_sub = 1;
			if (!insert_key) mdb_node_del(mc, 0);
			goto new_sub;
		}
	current:
		if ((leaf->mn_flags ^ flags) & F_SUBDATA)
			return MDB_INCOMPATIBLE;
		if (F_ISSET(leaf->mn_flags, F_BIGDATA)) {
			MDB_page *omp;
			u64 pg;
			i32 level, ovpages,
			    dpages = OVPAGES(data->mv_size, env->me_psize);

			memcpy(&pg, olddata.mv_data, sizeof(pg));
			if ((rc2 = mdb_page_get(mc, pg, &omp, &level)) != 0)
				return rc2;
			ovpages = omp->mp_pages;

			if (ovpages >= dpages) {
				if (!(omp->mp_flags & P_DIRTY) &&
				    (level || (env->me_flags & MDB_WRITEMAP))) {
					rc = mdb_page_unspill(mc->mc_txn, omp,
							      &omp);
					if (rc) return rc;
					level = 0;
				}
				if (omp->mp_flags & P_DIRTY) {
					if (level > 1) {
						u64 sz = (u64)env->me_psize *
							 ovpages,
						    off;
						MDB_page *np = mdb_page_malloc(
						    mc->mc_txn, ovpages);
						MDB_ID2 id2;
						if (!np) return ENOMEM;
						id2.mid = pg;
						id2.mptr = np;
						rc2 = mdb_mid2l_insert(
						    mc->mc_txn->mt_u.dirty_list,
						    &id2);
						mdb_cassert(mc, rc2 == 0);
						if (!(flags & MDB_RESERVE)) {
							off = (PAGEHDRSZ +
							       data->mv_size) &
							      -(i32)sizeof(u64);
							memcpy(
							    (u64 *)((u8 *)np +
								    off),
							    (u64 *)((u8 *)omp +
								    off),
							    sz - off);
							sz = PAGEHDRSZ;
						}
						memcpy(np, omp, sz);
						omp = np;
					}
					SETDSZ(leaf, data->mv_size);
					if (F_ISSET(flags, MDB_RESERVE))
						data->mv_data = METADATA(omp);
					else
						memcpy(METADATA(omp),
						       data->mv_data,
						       data->mv_size);
					return MDB_SUCCESS;
				}
			}
			if ((rc2 = mdb_ovpage_free(mc, omp)) != MDB_SUCCESS)
				return rc2;
		} else if (data->mv_size == olddata.mv_size) {
			if (F_ISSET(flags, MDB_RESERVE))
				data->mv_data = olddata.mv_data;
			else if (!(mc->mc_flags & C_SUB))
				memcpy(olddata.mv_data, data->mv_data,
				       data->mv_size);
			else {
				if (key->mv_size != NODEKSZ(leaf))
					goto new_ksize;
				memcpy(NODEKEY(leaf), key->mv_data,
				       key->mv_size);
				goto fix_parent;
			}
			return MDB_SUCCESS;
		}
	new_ksize:
		mdb_node_del(mc, 0);
	}

	rdata = data;

new_sub:
	nflags = flags & NODE_ADD_FLAGS;
	nsize = IS_LEAF2(mc->mc_pg[mc->mc_top])
		    ? key->mv_size
		    : mdb_leaf_size(env, key, rdata);
	if (SIZELEFT(mc->mc_pg[mc->mc_top]) < nsize) {
		if ((flags & (F_DUPDATA | F_SUBDATA)) == F_DUPDATA)
			nflags &= ~MDB_APPEND;
		if (!insert_key) nflags |= MDB_SPLIT_REPLACE;
		rc = mdb_page_split(mc, key, rdata, P_INVALID, nflags);
	} else {
		rc = mdb_node_add(mc, mc->mc_ki[mc->mc_top], key, rdata, 0,
				  nflags);
		if (rc == 0) {
			MDB_cursor *m2, *m3;
			MDB_dbi dbi = mc->mc_dbi;
			u32 i = mc->mc_top;
			MDB_page *mp = mc->mc_pg[i];

			for (m2 = mc->mc_txn->mt_cursors[dbi]; m2;
			     m2 = m2->mc_next) {
				if (mc->mc_flags & C_SUB)
					m3 = &m2->mc_xcursor->mx_cursor;
				else
					m3 = m2;
				if (m3 == mc || m3->mc_snum < mc->mc_snum ||
				    m3->mc_pg[i] != mp)
					continue;
				if (m3->mc_ki[i] >= mc->mc_ki[i] &&
				    insert_key) {
					m3->mc_ki[i]++;
				}
				XCURSOR_REFRESH(m3, i, mp);
			}
		}
	}

	if (rc == MDB_SUCCESS) {
		if (do_sub) {
			i32 xflags, new_dupdata;
			u64 ecount;
		put_sub:
			xdata.mv_size = 0;
			xdata.mv_data = "";
			leaf = NODEPTR(mc->mc_pg[mc->mc_top],
				       mc->mc_ki[mc->mc_top]);
			if ((flags & (MDB_CURRENT | MDB_APPENDDUP)) ==
			    MDB_CURRENT) {
				xflags = MDB_CURRENT | MDB_NOSPILL;
			} else {
				mdb_xcursor_init1(mc, leaf);
				xflags = (flags & MDB_NODUPDATA)
					     ? MDB_NOOVERWRITE | MDB_NOSPILL
					     : MDB_NOSPILL;
			}
			if (sub_root)
				mc->mc_xcursor->mx_cursor.mc_pg[0] = sub_root;
			new_dupdata = (i32)dkey.mv_size;
			if (dkey.mv_size) {
				rc = _mdb_cursor_put(&mc->mc_xcursor->mx_cursor,
						     &dkey, &xdata, xflags);
				if (rc) goto bad_sub;
				dkey.mv_size = 0;
			}
			if (!(leaf->mn_flags & F_SUBDATA) || sub_root) {
				MDB_cursor *m2;
				MDB_xcursor *mx = mc->mc_xcursor;
				u32 i = mc->mc_top;
				MDB_page *mp = mc->mc_pg[i];

				for (m2 = mc->mc_txn->mt_cursors[mc->mc_dbi];
				     m2; m2 = m2->mc_next) {
					if (m2 == mc ||
					    m2->mc_snum < mc->mc_snum)
						continue;
					if (!(m2->mc_flags & C_INITIALIZED))
						continue;
					if (m2->mc_pg[i] == mp) {
						if (m2->mc_ki[i] ==
						    mc->mc_ki[i]) {
							mdb_xcursor_init2(
							    m2, mx,
							    new_dupdata);
						} else if (!insert_key) {
							XCURSOR_REFRESH(m2, i,
									mp);
						}
					}
				}
			}
			ecount = mc->mc_xcursor->mx_db.md_entries;
			if (flags & MDB_APPENDDUP) xflags |= MDB_APPEND;
			rc = _mdb_cursor_put(&mc->mc_xcursor->mx_cursor, data,
					     &xdata, xflags);
			if (flags & F_SUBDATA) {
				void *db = NODEDATA(leaf);
				memcpy(db, &mc->mc_xcursor->mx_db,
				       sizeof(MDB_db));
			}
			insert_data = mc->mc_xcursor->mx_db.md_entries - ecount;
		}
		if (insert_data) mc->mc_db->md_entries++;
		if (insert_key) {
			if (rc) goto bad_sub;
			mc->mc_flags |= C_INITIALIZED;
		}
		if (flags & MDB_MULTIPLE) {
			if (!rc) {
				mcount++;
				data[1].mv_size = mcount;
				if (mcount < dcount) {
					data[0].mv_data =
					    (u8 *)data[0].mv_data +
					    data[0].mv_size;
					insert_key = insert_data = 0;
					goto more;
				}
			}
		}
		return rc;
	bad_sub:
		if (rc == MDB_KEYEXIST) rc = MDB_PROBLEM;
	}
	mc->mc_txn->mt_flags |= MDB_TXN_ERROR;
	return rc;
}

i32 mdb_cursor_put(MDB_cursor *mc, MDB_val *key, MDB_val *data, u32 flags) {
	i32 rc = _mdb_cursor_put(mc, key, data, flags);
	return rc;
}

STATIC i32 _mdb_cursor_del(MDB_cursor *mc, u32 flags) {
	MDB_node *leaf;
	MDB_page *mp;
	i32 rc;

	if (mc->mc_txn->mt_flags & (MDB_TXN_RDONLY | MDB_TXN_BLOCKED))
		return (mc->mc_txn->mt_flags & MDB_TXN_RDONLY) ? EACCES
							       : MDB_BAD_TXN;

	if (!(mc->mc_flags & C_INITIALIZED)) return EINVAL;

	if (mc->mc_ki[mc->mc_top] >= NUMKEYS(mc->mc_pg[mc->mc_top]))
		return MDB_NOTFOUND;

	if (!(flags & MDB_NOSPILL) && (rc = mdb_page_spill(mc, NULL, NULL)))
		return rc;

	rc = mdb_cursor_touch(mc);
	if (rc) return rc;

	mp = mc->mc_pg[mc->mc_top];
	if (!IS_LEAF(mp)) return MDB_CORRUPTED;
	if (IS_LEAF2(mp)) goto del_key;
	leaf = NODEPTR(mp, mc->mc_ki[mc->mc_top]);

	if (F_ISSET(leaf->mn_flags, F_DUPDATA)) {
		if (flags & MDB_NODUPDATA) {
			mc->mc_db->md_entries -=
			    mc->mc_xcursor->mx_db.md_entries - 1;
			mc->mc_xcursor->mx_cursor.mc_flags &= ~C_INITIALIZED;
		} else {
			if (!F_ISSET(leaf->mn_flags, F_SUBDATA)) {
				mc->mc_xcursor->mx_cursor.mc_pg[0] =
				    NODEDATA(leaf);
			}
			rc = _mdb_cursor_del(&mc->mc_xcursor->mx_cursor,
					     MDB_NOSPILL);
			if (rc) return rc;
			if (mc->mc_xcursor->mx_db.md_entries) {
				if (leaf->mn_flags & F_SUBDATA) {
					void *db = NODEDATA(leaf);
					memcpy(db, &mc->mc_xcursor->mx_db,
					       sizeof(MDB_db));
				} else {
					MDB_cursor *m2;
					mdb_node_shrink(mp,
							mc->mc_ki[mc->mc_top]);
					leaf =
					    NODEPTR(mp, mc->mc_ki[mc->mc_top]);
					mc->mc_xcursor->mx_cursor.mc_pg[0] =
					    NODEDATA(leaf);
					for (m2 = mc->mc_txn
						      ->mt_cursors[mc->mc_dbi];
					     m2; m2 = m2->mc_next) {
						if (m2 == mc ||
						    m2->mc_snum < mc->mc_snum)
							continue;
						if (!(m2->mc_flags &
						      C_INITIALIZED))
							continue;
						if (m2->mc_pg[mc->mc_top] ==
						    mp) {
							XCURSOR_REFRESH(
							    m2, mc->mc_top, mp);
						}
					}
				}
				mc->mc_db->md_entries--;
				return rc;
			} else {
				mc->mc_xcursor->mx_cursor.mc_flags &=
				    ~C_INITIALIZED;
			}
		}

		if (leaf->mn_flags & F_SUBDATA) {
			rc = mdb_drop0(&mc->mc_xcursor->mx_cursor, 0);
			if (rc) goto fail;
		}
	} else if ((leaf->mn_flags ^ flags) & F_SUBDATA) {
		rc = MDB_INCOMPATIBLE;
		goto fail;
	}

	if (F_ISSET(leaf->mn_flags, F_BIGDATA)) {
		MDB_page *omp;
		u64 pg;

		memcpy(&pg, NODEDATA(leaf), sizeof(pg));
		if ((rc = mdb_page_get(mc, pg, &omp, NULL)) ||
		    (rc = mdb_ovpage_free(mc, omp)))
			goto fail;
	}

del_key:
	return mdb_cursor_del0(mc);

fail:
	mc->mc_txn->mt_flags |= MDB_TXN_ERROR;
	return rc;
}

i32 mdb_cursor_del(MDB_cursor *mc, u32 flags) {
	return _mdb_cursor_del(mc, flags);
}

STATIC i32 mdb_page_new(MDB_cursor *mc, u32 flags, i32 num, MDB_page **mp) {
	MDB_page *np;
	i32 rc;

	if ((rc = mdb_page_alloc(mc, num, &np))) return rc;
	np->mp_flags = flags | P_DIRTY;
	np->mp_lower = (PAGEHDRSZ - PAGEBASE);
	np->mp_upper = mc->mc_txn->mt_env->me_psize - PAGEBASE;

	if (IS_BRANCH(np))
		mc->mc_db->md_branch_pages++;
	else if (IS_LEAF(np))
		mc->mc_db->md_leaf_pages++;
	else if (IS_OVERFLOW(np)) {
		mc->mc_db->md_overflow_pages += num;
		np->mp_pages = num;
	}
	*mp = np;

	return 0;
}

STATIC u64 mdb_leaf_size(MDB_env *env, MDB_val *key, MDB_val *data) {
	u64 sz;

	sz = LEAFSIZE(key, data);
	if (sz > env->me_nodemax) {
		sz -= data->mv_size - sizeof(u64);
	}

	return EVEN(sz + sizeof(u16));
}

STATIC u64 mdb_branch_size(MDB_env *env __attribute__((unused)), MDB_val *key) {
	return INDXSIZE(key) + sizeof(u16);
}

STATIC i32 mdb_node_add(MDB_cursor *mc, u16 indx, MDB_val *key, MDB_val *data,
			u64 pgno, u32 flags) {
	u32 i;
	u64 node_size = NODESIZE;
	i64 room;
	u16 ofs;
	MDB_node *node;
	MDB_page *mp = mc->mc_pg[mc->mc_top];
	MDB_page *ofp = NULL;
	void *ndata;

	mdb_cassert(mc, MP_UPPER(mp) >= MP_LOWER(mp));

	if (IS_LEAF2(mp)) {
		i32 ksize = mc->mc_db->md_pad, dif;
		u8 *ptr = LEAF2KEY(mp, indx, ksize);
		dif = NUMKEYS(mp) - indx;
		if (dif > 0) memorymove(ptr + ksize, ptr, dif * ksize);
		memcpy(ptr, key->mv_data, ksize);

		MP_LOWER(mp) += sizeof(u16);
		MP_UPPER(mp) -= ksize - sizeof(u16);
		return MDB_SUCCESS;
	}

	room = (i64)SIZELEFT(mp) - (i64)sizeof(u16);
	if (key != NULL) node_size += key->mv_size;
	if (IS_LEAF(mp)) {
		mdb_cassert(mc, key && data);
		if (F_ISSET(flags, F_BIGDATA)) {
			node_size += sizeof(u64);
		} else if (node_size + data->mv_size >
			   mc->mc_txn->mt_env->me_nodemax) {
			i32 ovpages = OVPAGES(data->mv_size,
					      mc->mc_txn->mt_env->me_psize);
			i32 rc;
			node_size = EVEN(node_size + sizeof(u64));
			if ((i64)node_size > room) goto full;
			if ((rc = mdb_page_new(mc, P_OVERFLOW, ovpages, &ofp)))
				return rc;
			flags |= F_BIGDATA;
			goto update;
		} else {
			node_size += data->mv_size;
		}
	}
	node_size = EVEN(node_size);
	if ((i64)node_size > room) goto full;

update:
	for (i = NUMKEYS(mp); i > indx; i--)
		MP_PTRS(mp)[i] = MP_PTRS(mp)[i - 1];

	ofs = MP_UPPER(mp) - node_size;
	mdb_cassert(mc, ofs >= MP_LOWER(mp) + sizeof(u16));
	MP_PTRS(mp)[indx] = ofs;
	MP_UPPER(mp) = ofs;
	MP_LOWER(mp) += sizeof(u16);

	node = NODEPTR(mp, indx);
	node->mn_ksize = (key == NULL) ? 0 : key->mv_size;
	node->mn_flags = flags;
	if (IS_LEAF(mp))
		SETDSZ(node, data->mv_size);
	else
		SETPGNO(node, pgno);

	if (key) memcpy(NODEKEY(node), key->mv_data, key->mv_size);

	if (IS_LEAF(mp)) {
		ndata = NODEDATA(node);
		if (ofp == NULL) {
			if (F_ISSET(flags, F_BIGDATA))
				memcpy(ndata, data->mv_data, sizeof(u64));
			else if (F_ISSET(flags, MDB_RESERVE))
				data->mv_data = ndata;
			else
				memcpy(ndata, data->mv_data, data->mv_size);
		} else {
			memcpy(ndata, &ofp->mp_pgno, sizeof(u64));
			ndata = METADATA(ofp);
			if (F_ISSET(flags, MDB_RESERVE))
				data->mv_data = ndata;
			else
				memcpy(ndata, data->mv_data, data->mv_size);
		}
	}

	return MDB_SUCCESS;

full:
	mc->mc_txn->mt_flags |= MDB_TXN_ERROR;
	return MDB_PAGE_FULL;
}

STATIC void mdb_node_del(MDB_cursor *mc, i32 ksize) {
	MDB_page *mp = mc->mc_pg[mc->mc_top];
	u16 indx = mc->mc_ki[mc->mc_top];
	u32 sz;
	u16 i, j, numkeys, ptr;
	MDB_node *node;
	u8 *base;

	numkeys = NUMKEYS(mp);
	mdb_cassert(mc, indx < numkeys);

	if (IS_LEAF2(mp)) {
		i32 x = numkeys - 1 - indx;
		base = LEAF2KEY(mp, indx, ksize);
		if (x) memorymove(base, base + ksize, x * ksize);
		MP_LOWER(mp) -= sizeof(u16);
		MP_UPPER(mp) += ksize - sizeof(u16);
		return;
	}

	node = NODEPTR(mp, indx);
	sz = NODESIZE + node->mn_ksize;
	if (IS_LEAF(mp)) {
		if (F_ISSET(node->mn_flags, F_BIGDATA))
			sz += sizeof(u64);
		else
			sz += NODEDSZ(node);
	}
	sz = EVEN(sz);

	ptr = MP_PTRS(mp)[indx];
	for (i = j = 0; i < numkeys; i++) {
		if (i != indx) {
			MP_PTRS(mp)[j] = MP_PTRS(mp)[i];
			if (MP_PTRS(mp)[i] < ptr) MP_PTRS(mp)[j] += sz;
			j++;
		}
	}

	base = (u8 *)mp + MP_UPPER(mp) + PAGEBASE;
	memorymove(base + sz, base, ptr - MP_UPPER(mp));

	MP_LOWER(mp) -= sizeof(u16);
	MP_UPPER(mp) += sz;
}

STATIC void mdb_node_shrink(MDB_page *mp, u16 indx) {
	MDB_node *node;
	MDB_page *sp, *xp;
	u8 *base;
	u16 delta, nsize, len, ptr;
	i32 i;

	node = NODEPTR(mp, indx);
	sp = (MDB_page *)NODEDATA(node);
	delta = SIZELEFT(sp);
	nsize = NODEDSZ(node) - delta;

	if (IS_LEAF2(sp)) {
		len = nsize;
		if (nsize & 1) return;
	} else {
		xp = (MDB_page *)((u8 *)sp + delta);
		for (i = NUMKEYS(sp); --i >= 0;)
			MP_PTRS(xp)[i] = MP_PTRS(sp)[i] - delta;
		len = PAGEHDRSZ;
	}
	MP_UPPER(sp) = MP_LOWER(sp);
	COPY_PGNO(MP_PGNO(sp), mp->mp_pgno);
	SETDSZ(node, nsize);

	base = (u8 *)mp + mp->mp_upper + PAGEBASE;
	memorymove(base + delta, base, (u8 *)sp + len - base);

	ptr = mp->mp_ptrs[indx];
	for (i = NUMKEYS(mp); --i >= 0;) {
		if (mp->mp_ptrs[i] <= ptr) mp->mp_ptrs[i] += delta;
	}
	mp->mp_upper += delta;
}

STATIC void mdb_xcursor_init0(MDB_cursor *mc) {
	MDB_xcursor *mx = mc->mc_xcursor;

	mx->mx_cursor.mc_xcursor = NULL;
	mx->mx_cursor.mc_txn = mc->mc_txn;
	mx->mx_cursor.mc_db = &mx->mx_db;
	mx->mx_cursor.mc_dbx = &mx->mx_dbx;
	mx->mx_cursor.mc_dbi = mc->mc_dbi;
	mx->mx_cursor.mc_dbflag = &mx->mx_dbflag;
	mx->mx_cursor.mc_snum = 0;
	mx->mx_cursor.mc_top = 0;
	MC_SET_OVPG(&mx->mx_cursor, NULL);
	mx->mx_cursor.mc_flags =
	    C_SUB | (mc->mc_flags & (C_ORIG_RDONLY | C_WRITEMAP));
	mx->mx_dbx.md_name.mv_size = 0;
	mx->mx_dbx.md_name.mv_data = NULL;
	mx->mx_dbx.md_cmp = mc->mc_dbx->md_dcmp;
	mx->mx_dbx.md_dcmp = NULL;
	mx->mx_dbx.md_rel = mc->mc_dbx->md_rel;
}

STATIC void mdb_xcursor_init1(MDB_cursor *mc, MDB_node *node) {
	MDB_xcursor *mx = mc->mc_xcursor;

	mx->mx_cursor.mc_flags &= C_SUB | C_ORIG_RDONLY | C_WRITEMAP;
	if (node->mn_flags & F_SUBDATA) {
		memcpy(&mx->mx_db, NODEDATA(node), sizeof(MDB_db));
		mx->mx_cursor.mc_pg[0] = 0;
		mx->mx_cursor.mc_snum = 0;
		mx->mx_cursor.mc_top = 0;
	} else {
		MDB_page *fp = NODEDATA(node);
		mx->mx_db.md_pad = 0;
		mx->mx_db.md_flags = 0;
		mx->mx_db.md_depth = 1;
		mx->mx_db.md_branch_pages = 0;
		mx->mx_db.md_leaf_pages = 1;
		mx->mx_db.md_overflow_pages = 0;
		mx->mx_db.md_entries = NUMKEYS(fp);
		COPY_PGNO(mx->mx_db.md_root, MP_PGNO(fp));
		mx->mx_cursor.mc_snum = 1;
		mx->mx_cursor.mc_top = 0;
		mx->mx_cursor.mc_flags |= C_INITIALIZED;
		mx->mx_cursor.mc_pg[0] = fp;
		mx->mx_cursor.mc_ki[0] = 0;
		if (mc->mc_db->md_flags & MDB_DUPFIXED) {
			mx->mx_db.md_flags = MDB_DUPFIXED;
			mx->mx_db.md_pad = fp->mp_pad;
			if (mc->mc_db->md_flags & MDB_INTEGERDUP)
				mx->mx_db.md_flags |= MDB_INTEGERKEY;
		}
	}
	mx->mx_dbflag = DB_VALID | DB_USRVALID | DB_DUPDATA;
	if (NEED_CMP_CLONG(mx->mx_dbx.md_cmp, mx->mx_db.md_pad))
		mx->mx_dbx.md_cmp = mdb_cmp_clong;
}

STATIC void mdb_xcursor_init2(MDB_cursor *mc, MDB_xcursor *src_mx,
			      i32 new_dupdata) {
	MDB_xcursor *mx = mc->mc_xcursor;

	if (new_dupdata) {
		mx->mx_cursor.mc_snum = 1;
		mx->mx_cursor.mc_top = 0;
		mx->mx_cursor.mc_flags |= C_INITIALIZED;
		mx->mx_cursor.mc_ki[0] = 0;
		mx->mx_dbflag = DB_VALID | DB_USRVALID | DB_DUPDATA;
#if U32_MAX < MDB_SIZE_MAX
		mx->mx_dbx.md_cmp = src_mx->mx_dbx.md_cmp;
#endif
	} else if (!(mx->mx_cursor.mc_flags & C_INITIALIZED)) {
		return;
	}
	mx->mx_db = src_mx->mx_db;
	mx->mx_cursor.mc_pg[0] = src_mx->mx_cursor.mc_pg[0];
}

STATIC void mdb_cursor_init(MDB_cursor *mc, MDB_txn *txn, MDB_dbi dbi,
			    MDB_xcursor *mx) {
	mc->mc_next = NULL;
	mc->mc_backup = NULL;
	mc->mc_dbi = dbi;
	mc->mc_txn = txn;
	mc->mc_db = &txn->mt_dbs[dbi];
	mc->mc_dbx = &txn->mt_dbxs[dbi];
	mc->mc_dbflag = &txn->mt_dbflags[dbi];
	mc->mc_snum = 0;
	mc->mc_top = 0;
	mc->mc_pg[0] = 0;
	mc->mc_ki[0] = 0;
	MC_SET_OVPG(mc, NULL);
	mc->mc_flags = txn->mt_flags & (C_ORIG_RDONLY | C_WRITEMAP);
	if (txn->mt_dbs[dbi].md_flags & MDB_DUPSORT) {
		mdb_tassert(txn, mx != NULL);
		mc->mc_xcursor = mx;
		mdb_xcursor_init0(mc);
	} else {
		mc->mc_xcursor = NULL;
	}
	if (*mc->mc_dbflag & DB_STALE) {
		mdb_page_search(mc, NULL, MDB_PS_ROOTONLY);
	}
}

i32 mdb_cursor_open(MDB_txn *txn, MDB_dbi dbi, MDB_cursor **ret) {
	MDB_cursor *mc;
	u64 size = sizeof(MDB_cursor);

	if (!ret || !TXN_DBI_EXIST(txn, dbi, DB_VALID)) return EINVAL;

	if (txn->mt_flags & MDB_TXN_BLOCKED) return MDB_BAD_TXN;

	if (dbi == FREE_DBI && !F_ISSET(txn->mt_flags, MDB_TXN_RDONLY))
		return EINVAL;

	if (txn->mt_dbs[dbi].md_flags & MDB_DUPSORT)
		size += sizeof(MDB_xcursor);

	if ((mc = alloc(size)) != NULL) {
		mdb_cursor_init(mc, txn, dbi, (MDB_xcursor *)(mc + 1));
		if (txn->mt_cursors) {
			mc->mc_next = txn->mt_cursors[dbi];
			txn->mt_cursors[dbi] = mc;
			mc->mc_flags |= C_UNTRACK;
		}
	} else {
		return ENOMEM;
	}

	*ret = mc;

	return MDB_SUCCESS;
}

i32 mdb_cursor_renew(MDB_txn *txn, MDB_cursor *mc) {
	if (!mc || !TXN_DBI_EXIST(txn, mc->mc_dbi, DB_VALID)) return EINVAL;

	if ((mc->mc_flags & C_UNTRACK) || txn->mt_cursors) return EINVAL;

	if (txn->mt_flags & MDB_TXN_BLOCKED) return MDB_BAD_TXN;

	mdb_cursor_init(mc, txn, mc->mc_dbi, mc->mc_xcursor);
	return MDB_SUCCESS;
}

i32 mdb_cursor_count(MDB_cursor *mc, u64 *countp) {
	MDB_node *leaf;

	if (mc == NULL || countp == NULL) return EINVAL;

	if (mc->mc_xcursor == NULL) return MDB_INCOMPATIBLE;

	if (mc->mc_txn->mt_flags & MDB_TXN_BLOCKED) return MDB_BAD_TXN;

	if (!(mc->mc_flags & C_INITIALIZED)) return EINVAL;

	if (!mc->mc_snum) return MDB_NOTFOUND;

	if (mc->mc_flags & C_EOF) {
		if (mc->mc_ki[mc->mc_top] >= NUMKEYS(mc->mc_pg[mc->mc_top]))
			return MDB_NOTFOUND;
		mc->mc_flags ^= C_EOF;
	}

	leaf = NODEPTR(mc->mc_pg[mc->mc_top], mc->mc_ki[mc->mc_top]);
	if (!F_ISSET(leaf->mn_flags, F_DUPDATA)) {
		*countp = 1;
	} else {
		if (!(mc->mc_xcursor->mx_cursor.mc_flags & C_INITIALIZED))
			return EINVAL;

		*countp = mc->mc_xcursor->mx_db.md_entries;
	}
	return MDB_SUCCESS;
}

void mdb_cursor_close(MDB_cursor *mc) {
	if (mc) {
		MDB_CURSOR_UNREF(mc, 0);
	}
	if (mc && !mc->mc_backup) {
		if ((mc->mc_flags & C_UNTRACK) && mc->mc_txn->mt_cursors) {
			MDB_cursor **prev = &mc->mc_txn->mt_cursors[mc->mc_dbi];
			while (*prev && *prev != mc) prev = &(*prev)->mc_next;
			if (*prev == mc) *prev = mc->mc_next;
		}
		release(mc);
	}
}

MDB_txn *mdb_cursor_txn(MDB_cursor *mc) {
	if (!mc) return NULL;
	return mc->mc_txn;
}

MDB_dbi mdb_cursor_dbi(MDB_cursor *mc) { return mc->mc_dbi; }

STATIC i32 mdb_update_key(MDB_cursor *mc, MDB_val *key) {
	MDB_page *mp;
	MDB_node *node;
	u8 *base;
	u64 len;
	i32 delta, ksize, oksize;
	u16 ptr, i, numkeys, indx;

	indx = mc->mc_ki[mc->mc_top];
	mp = mc->mc_pg[mc->mc_top];
	node = NODEPTR(mp, indx);
	ptr = mp->mp_ptrs[indx];

	ksize = EVEN(key->mv_size);
	oksize = EVEN(node->mn_ksize);
	delta = ksize - oksize;

	if (delta) {
		if (delta > 0 && SIZELEFT(mp) < delta) {
			u64 pgno;
			pgno = NODEPGNO(node);
			mdb_node_del(mc, 0);
			return mdb_page_split(mc, key, NULL, pgno,
					      MDB_SPLIT_REPLACE);
		}

		numkeys = NUMKEYS(mp);
		for (i = 0; i < numkeys; i++) {
			if (mp->mp_ptrs[i] <= ptr) mp->mp_ptrs[i] -= delta;
		}

		base = (u8 *)mp + mp->mp_upper + PAGEBASE;
		len = ptr - mp->mp_upper + NODESIZE;
		memorymove(base - delta, base, len);
		mp->mp_upper -= delta;

		node = NODEPTR(mp, indx);
	}

	if (node->mn_ksize != key->mv_size) node->mn_ksize = key->mv_size;

	if (key->mv_size) memcpy(NODEKEY(node), key->mv_data, key->mv_size);

	return MDB_SUCCESS;
}

STATIC i32 mdb_node_move(MDB_cursor *csrc, MDB_cursor *cdst, i32 fromleft) {
	MDB_node *srcnode;
	MDB_val key, data;
	u64 srcpg;
	MDB_cursor mn;
	i32 rc;
	u16 flags;

	if ((rc = mdb_page_touch(csrc)) || (rc = mdb_page_touch(cdst)))
		return rc;

	if (IS_LEAF2(csrc->mc_pg[csrc->mc_top])) {
		key.mv_size = csrc->mc_db->md_pad;
		key.mv_data = LEAF2KEY(csrc->mc_pg[csrc->mc_top],
				       csrc->mc_ki[csrc->mc_top], key.mv_size);
		data.mv_size = 0;
		data.mv_data = NULL;
		srcpg = 0;
		flags = 0;
	} else {
		srcnode = NODEPTR(csrc->mc_pg[csrc->mc_top],
				  csrc->mc_ki[csrc->mc_top]);
		mdb_cassert(csrc, !((u64)srcnode & 1));
		srcpg = NODEPGNO(srcnode);
		flags = srcnode->mn_flags;
		if (csrc->mc_ki[csrc->mc_top] == 0 &&
		    IS_BRANCH(csrc->mc_pg[csrc->mc_top])) {
			u32 snum = csrc->mc_snum;
			MDB_node *s2;
			rc = mdb_page_search_lowest(csrc);
			if (rc) return rc;
			if (IS_LEAF2(csrc->mc_pg[csrc->mc_top])) {
				key.mv_size = csrc->mc_db->md_pad;
				key.mv_data = LEAF2KEY(
				    csrc->mc_pg[csrc->mc_top], 0, key.mv_size);
			} else {
				s2 = NODEPTR(csrc->mc_pg[csrc->mc_top], 0);
				key.mv_size = NODEKSZ(s2);
				key.mv_data = NODEKEY(s2);
			}
			csrc->mc_snum = snum--;
			csrc->mc_top = snum;
		} else {
			key.mv_size = NODEKSZ(srcnode);
			key.mv_data = NODEKEY(srcnode);
		}
		data.mv_size = NODEDSZ(srcnode);
		data.mv_data = NODEDATA(srcnode);
	}
	mn.mc_xcursor = NULL;
	if (IS_BRANCH(cdst->mc_pg[cdst->mc_top]) &&
	    cdst->mc_ki[cdst->mc_top] == 0) {
		u32 snum = cdst->mc_snum;
		MDB_node *s2;
		MDB_val bkey;
		mdb_cursor_copy(cdst, &mn);
		rc = mdb_page_search_lowest(&mn);
		if (rc) return rc;
		if (IS_LEAF2(mn.mc_pg[mn.mc_top])) {
			bkey.mv_size = mn.mc_db->md_pad;
			bkey.mv_data =
			    LEAF2KEY(mn.mc_pg[mn.mc_top], 0, bkey.mv_size);
		} else {
			s2 = NODEPTR(mn.mc_pg[mn.mc_top], 0);
			bkey.mv_size = NODEKSZ(s2);
			bkey.mv_data = NODEKEY(s2);
		}
		mn.mc_snum = snum--;
		mn.mc_top = snum;
		mn.mc_ki[snum] = 0;
		rc = mdb_update_key(&mn, &bkey);
		if (rc) return rc;
	}

	rc = mdb_node_add(cdst, cdst->mc_ki[cdst->mc_top], &key, &data, srcpg,
			  flags);
	if (rc != MDB_SUCCESS) return rc;

	mdb_node_del(csrc, key.mv_size);

	{
		MDB_cursor *m2, *m3;
		MDB_dbi dbi = csrc->mc_dbi;
		MDB_page *mpd, *mps;

		mps = csrc->mc_pg[csrc->mc_top];
		if (fromleft) {
			mpd = cdst->mc_pg[csrc->mc_top];
			for (m2 = csrc->mc_txn->mt_cursors[dbi]; m2;
			     m2 = m2->mc_next) {
				if (csrc->mc_flags & C_SUB)
					m3 = &m2->mc_xcursor->mx_cursor;
				else
					m3 = m2;
				if (!(m3->mc_flags & C_INITIALIZED) ||
				    m3->mc_top < csrc->mc_top)
					continue;
				if (m3 != cdst &&
				    m3->mc_pg[csrc->mc_top] == mpd &&
				    m3->mc_ki[csrc->mc_top] >=
					cdst->mc_ki[csrc->mc_top]) {
					m3->mc_ki[csrc->mc_top]++;
				}
				if (m3 != csrc &&
				    m3->mc_pg[csrc->mc_top] == mps &&
				    m3->mc_ki[csrc->mc_top] ==
					csrc->mc_ki[csrc->mc_top]) {
					m3->mc_pg[csrc->mc_top] =
					    cdst->mc_pg[cdst->mc_top];
					m3->mc_ki[csrc->mc_top] =
					    cdst->mc_ki[cdst->mc_top];
					m3->mc_ki[csrc->mc_top - 1]++;
				}
				if (IS_LEAF(mps))
					XCURSOR_REFRESH(
					    m3, csrc->mc_top,
					    m3->mc_pg[csrc->mc_top]);
			}
		} else {
			for (m2 = csrc->mc_txn->mt_cursors[dbi]; m2;
			     m2 = m2->mc_next) {
				if (csrc->mc_flags & C_SUB)
					m3 = &m2->mc_xcursor->mx_cursor;
				else
					m3 = m2;
				if (m3 == csrc) continue;
				if (!(m3->mc_flags & C_INITIALIZED) ||
				    m3->mc_top < csrc->mc_top)
					continue;
				if (m3->mc_pg[csrc->mc_top] == mps) {
					if (!m3->mc_ki[csrc->mc_top]) {
						m3->mc_pg[csrc->mc_top] =
						    cdst->mc_pg[cdst->mc_top];
						m3->mc_ki[csrc->mc_top] =
						    cdst->mc_ki[cdst->mc_top];
						m3->mc_ki[csrc->mc_top - 1]--;
					} else {
						m3->mc_ki[csrc->mc_top]--;
					}
					if (IS_LEAF(mps))
						XCURSOR_REFRESH(
						    m3, csrc->mc_top,
						    m3->mc_pg[csrc->mc_top]);
				}
			}
		}
	}

	if (csrc->mc_ki[csrc->mc_top] == 0) {
		if (csrc->mc_ki[csrc->mc_top - 1] != 0) {
			if (IS_LEAF2(csrc->mc_pg[csrc->mc_top])) {
				key.mv_data = LEAF2KEY(
				    csrc->mc_pg[csrc->mc_top], 0, key.mv_size);
			} else {
				srcnode = NODEPTR(csrc->mc_pg[csrc->mc_top], 0);
				key.mv_size = NODEKSZ(srcnode);
				key.mv_data = NODEKEY(srcnode);
			}
			mdb_cursor_copy(csrc, &mn);
			mn.mc_snum--;
			mn.mc_top--;
			WITH_CURSOR_TRACKING(mn,
					     rc = mdb_update_key(&mn, &key));
			if (rc) return rc;
		}
		if (IS_BRANCH(csrc->mc_pg[csrc->mc_top])) {
			MDB_val nullkey;
			u16 ix = csrc->mc_ki[csrc->mc_top];
			nullkey.mv_size = 0;
			csrc->mc_ki[csrc->mc_top] = 0;
			rc = mdb_update_key(csrc, &nullkey);
			csrc->mc_ki[csrc->mc_top] = ix;
			mdb_cassert(csrc, rc == MDB_SUCCESS);
		}
	}

	if (cdst->mc_ki[cdst->mc_top] == 0) {
		if (cdst->mc_ki[cdst->mc_top - 1] != 0) {
			if (IS_LEAF2(csrc->mc_pg[csrc->mc_top])) {
				key.mv_data = LEAF2KEY(
				    cdst->mc_pg[cdst->mc_top], 0, key.mv_size);
			} else {
				srcnode = NODEPTR(cdst->mc_pg[cdst->mc_top], 0);
				key.mv_size = NODEKSZ(srcnode);
				key.mv_data = NODEKEY(srcnode);
			}
			mdb_cursor_copy(cdst, &mn);
			mn.mc_snum--;
			mn.mc_top--;
			WITH_CURSOR_TRACKING(mn,
					     rc = mdb_update_key(&mn, &key));
			if (rc) return rc;
		}
		if (IS_BRANCH(cdst->mc_pg[cdst->mc_top])) {
			MDB_val nullkey;
			u16 ix = cdst->mc_ki[cdst->mc_top];
			nullkey.mv_size = 0;
			cdst->mc_ki[cdst->mc_top] = 0;
			rc = mdb_update_key(cdst, &nullkey);
			cdst->mc_ki[cdst->mc_top] = ix;
			mdb_cassert(cdst, rc == MDB_SUCCESS);
		}
	}

	return MDB_SUCCESS;
}

STATIC i32 mdb_page_merge(MDB_cursor *csrc, MDB_cursor *cdst) {
	MDB_page *psrc, *pdst;
	MDB_node *srcnode;
	MDB_val key, data;
	u32 nkeys;
	i32 rc;
	u16 i, j;

	psrc = csrc->mc_pg[csrc->mc_top];
	pdst = cdst->mc_pg[cdst->mc_top];

	mdb_cassert(csrc, csrc->mc_snum > 1);
	mdb_cassert(csrc, cdst->mc_snum > 1);

	if ((rc = mdb_page_touch(cdst))) return rc;
	pdst = cdst->mc_pg[cdst->mc_top];
	j = nkeys = NUMKEYS(pdst);
	if (IS_LEAF2(psrc)) {
		key.mv_size = csrc->mc_db->md_pad;
		key.mv_data = METADATA(psrc);
		for (i = 0; i < NUMKEYS(psrc); i++, j++) {
			rc = mdb_node_add(cdst, j, &key, NULL, 0, 0);
			if (rc != MDB_SUCCESS) return rc;
			key.mv_data = (u8 *)key.mv_data + key.mv_size;
		}
	} else {
		for (i = 0; i < NUMKEYS(psrc); i++, j++) {
			srcnode = NODEPTR(psrc, i);
			if (i == 0 && IS_BRANCH(psrc)) {
				MDB_cursor mn;
				MDB_node *s2;
				mdb_cursor_copy(csrc, &mn);
				mn.mc_xcursor = NULL;
				rc = mdb_page_search_lowest(&mn);
				if (rc) return rc;
				if (IS_LEAF2(mn.mc_pg[mn.mc_top])) {
					key.mv_size = mn.mc_db->md_pad;
					key.mv_data =
					    LEAF2KEY(mn.mc_pg[mn.mc_top], 0,
						     key.mv_size);
				} else {
					s2 = NODEPTR(mn.mc_pg[mn.mc_top], 0);
					key.mv_size = NODEKSZ(s2);
					key.mv_data = NODEKEY(s2);
				}
			} else {
				key.mv_size = srcnode->mn_ksize;
				key.mv_data = NODEKEY(srcnode);
			}

			data.mv_size = NODEDSZ(srcnode);
			data.mv_data = NODEDATA(srcnode);
			rc = mdb_node_add(cdst, j, &key, &data,
					  NODEPGNO(srcnode), srcnode->mn_flags);
			if (rc != MDB_SUCCESS) return rc;
		}
	}

	csrc->mc_top--;
	mdb_node_del(csrc, 0);
	if (csrc->mc_ki[csrc->mc_top] == 0) {
		key.mv_size = 0;
		rc = mdb_update_key(csrc, &key);
		if (rc) {
			csrc->mc_top++;
			return rc;
		}
	}
	csrc->mc_top++;

	psrc = csrc->mc_pg[csrc->mc_top];
	rc = mdb_page_loose(csrc, psrc);
	if (rc) return rc;
	if (IS_LEAF(psrc))
		csrc->mc_db->md_leaf_pages--;
	else
		csrc->mc_db->md_branch_pages--;
	{
		MDB_cursor *m2, *m3;
		MDB_dbi dbi = csrc->mc_dbi;
		u32 top = csrc->mc_top;

		for (m2 = csrc->mc_txn->mt_cursors[dbi]; m2; m2 = m2->mc_next) {
			if (csrc->mc_flags & C_SUB)
				m3 = &m2->mc_xcursor->mx_cursor;
			else
				m3 = m2;
			if (m3 == csrc) continue;
			if (m3->mc_snum < csrc->mc_snum) continue;
			if (m3->mc_pg[top] == psrc) {
				m3->mc_pg[top] = pdst;
				m3->mc_ki[top] += nkeys;
				m3->mc_ki[top - 1] = cdst->mc_ki[top - 1];
			} else if (m3->mc_pg[top - 1] == csrc->mc_pg[top - 1] &&
				   m3->mc_ki[top - 1] > csrc->mc_ki[top - 1]) {
				m3->mc_ki[top - 1]--;
			}
			if (IS_LEAF(psrc))
				XCURSOR_REFRESH(m3, top, m3->mc_pg[top]);
		}
	}
	{
		u32 snum = cdst->mc_snum;
		u16 depth = cdst->mc_db->md_depth;
		mdb_cursor_pop(cdst);
		rc = mdb_rebalance(cdst);
		if (depth != cdst->mc_db->md_depth)
			snum += cdst->mc_db->md_depth - depth;
		cdst->mc_snum = snum;
		cdst->mc_top = snum - 1;
	}
	return rc;
}

STATIC void mdb_cursor_copy(const MDB_cursor *csrc, MDB_cursor *cdst) {
	u32 i;

	cdst->mc_txn = csrc->mc_txn;
	cdst->mc_dbi = csrc->mc_dbi;
	cdst->mc_db = csrc->mc_db;
	cdst->mc_dbx = csrc->mc_dbx;
	cdst->mc_snum = csrc->mc_snum;
	cdst->mc_top = csrc->mc_top;
	cdst->mc_flags = csrc->mc_flags;
	MC_SET_OVPG(cdst, MC_OVPG(csrc));

	for (i = 0; i < csrc->mc_snum; i++) {
		cdst->mc_pg[i] = csrc->mc_pg[i];
		cdst->mc_ki[i] = csrc->mc_ki[i];
	}
}

STATIC i32 mdb_rebalance(MDB_cursor *mc) {
	MDB_node *node;
	i32 rc, fromleft;
	u32 ptop, minkeys, thresh;
	MDB_cursor mn;
	u16 oldki;

	if (IS_BRANCH(mc->mc_pg[mc->mc_top])) {
		minkeys = 2;
		thresh = 1;
	} else {
		minkeys = 1;
		thresh = FILL_THRESHOLD;
	}

	if (PAGEFILL(mc->mc_txn->mt_env, mc->mc_pg[mc->mc_top]) >= thresh &&
	    NUMKEYS(mc->mc_pg[mc->mc_top]) >= minkeys) {
		return MDB_SUCCESS;
	}

	if (mc->mc_snum < 2) {
		MDB_page *mp = mc->mc_pg[0];
		if (IS_SUBP(mp)) {
			return MDB_SUCCESS;
		}
		if (NUMKEYS(mp) == 0) {
			mc->mc_db->md_root = P_INVALID;
			mc->mc_db->md_depth = 0;
			mc->mc_db->md_leaf_pages = 0;
			rc = mdb_midl_append(&mc->mc_txn->mt_free_pgs,
					     mp->mp_pgno);
			if (rc) return rc;
			mc->mc_snum = 0;
			mc->mc_top = 0;
			mc->mc_flags &= ~C_INITIALIZED;
			{
				MDB_cursor *m2, *m3;
				MDB_dbi dbi = mc->mc_dbi;

				for (m2 = mc->mc_txn->mt_cursors[dbi]; m2;
				     m2 = m2->mc_next) {
					if (mc->mc_flags & C_SUB)
						m3 = &m2->mc_xcursor->mx_cursor;
					else
						m3 = m2;
					if (!(m3->mc_flags & C_INITIALIZED) ||
					    (m3->mc_snum < mc->mc_snum))
						continue;
					if (m3->mc_pg[0] == mp) {
						m3->mc_snum = 0;
						m3->mc_top = 0;
						m3->mc_flags &= ~C_INITIALIZED;
					}
				}
			}
		} else if (IS_BRANCH(mp) && NUMKEYS(mp) == 1) {
			i32 i;
			rc = mdb_midl_append(&mc->mc_txn->mt_free_pgs,
					     mp->mp_pgno);
			if (rc) return rc;
			mc->mc_db->md_root = NODEPGNO(NODEPTR(mp, 0));
			rc = mdb_page_get(mc, mc->mc_db->md_root, &mc->mc_pg[0],
					  NULL);
			if (rc) return rc;
			mc->mc_db->md_depth--;
			mc->mc_db->md_branch_pages--;
			mc->mc_ki[0] = mc->mc_ki[1];
			for (i = 1; i < mc->mc_db->md_depth; i++) {
				mc->mc_pg[i] = mc->mc_pg[i + 1];
				mc->mc_ki[i] = mc->mc_ki[i + 1];
			}
			{
				MDB_cursor *m2, *m3;
				MDB_dbi dbi = mc->mc_dbi;

				for (m2 = mc->mc_txn->mt_cursors[dbi]; m2;
				     m2 = m2->mc_next) {
					if (mc->mc_flags & C_SUB)
						m3 = &m2->mc_xcursor->mx_cursor;
					else
						m3 = m2;
					if (m3 == mc) continue;
					if (!(m3->mc_flags & C_INITIALIZED))
						continue;
					if (m3->mc_pg[0] == mp) {
						for (i = 0;
						     i < mc->mc_db->md_depth;
						     i++) {
							m3->mc_pg[i] =
							    m3->mc_pg[i + 1];
							m3->mc_ki[i] =
							    m3->mc_ki[i + 1];
						}
						m3->mc_snum--;
						m3->mc_top--;
					}
				}
			}
		}
		return MDB_SUCCESS;
	}

	ptop = mc->mc_top - 1;
	mdb_cassert(mc, NUMKEYS(mc->mc_pg[ptop]) > 1);
	mdb_cursor_copy(mc, &mn);
	mn.mc_xcursor = NULL;

	oldki = mc->mc_ki[mc->mc_top];
	if (mc->mc_ki[ptop] == 0) {
		mn.mc_ki[ptop]++;
		node = NODEPTR(mc->mc_pg[ptop], mn.mc_ki[ptop]);
		rc = mdb_page_get(mc, NODEPGNO(node), &mn.mc_pg[mn.mc_top],
				  NULL);
		if (rc) return rc;
		mn.mc_ki[mn.mc_top] = 0;
		mc->mc_ki[mc->mc_top] = NUMKEYS(mc->mc_pg[mc->mc_top]);
		fromleft = 0;
	} else {
		mn.mc_ki[ptop]--;
		node = NODEPTR(mc->mc_pg[ptop], mn.mc_ki[ptop]);
		rc = mdb_page_get(mc, NODEPGNO(node), &mn.mc_pg[mn.mc_top],
				  NULL);
		if (rc) return rc;
		mn.mc_ki[mn.mc_top] = NUMKEYS(mn.mc_pg[mn.mc_top]) - 1;
		mc->mc_ki[mc->mc_top] = 0;
		fromleft = 1;
	}

	if (PAGEFILL(mc->mc_txn->mt_env, mn.mc_pg[mn.mc_top]) >= thresh &&
	    NUMKEYS(mn.mc_pg[mn.mc_top]) > minkeys) {
		rc = mdb_node_move(&mn, mc, fromleft);
		if (fromleft) {
			oldki++;
		}
	} else {
		if (!fromleft) {
			rc = mdb_page_merge(&mn, mc);
		} else {
			oldki += NUMKEYS(mn.mc_pg[mn.mc_top]);
			mn.mc_ki[mn.mc_top] += mc->mc_ki[mn.mc_top] + 1;
			WITH_CURSOR_TRACKING(mn, rc = mdb_page_merge(mc, &mn));
			mdb_cursor_copy(&mn, mc);
		}
		mc->mc_flags &= ~C_EOF;
	}
	mc->mc_ki[mc->mc_top] = oldki;
	return rc;
}

STATIC i32 mdb_cursor_del0(MDB_cursor *mc) {
	i32 rc;
	MDB_page *mp;
	u16 ki;
	u32 nkeys;
	MDB_cursor *m2, *m3;
	MDB_dbi dbi = mc->mc_dbi;

	ki = mc->mc_ki[mc->mc_top];
	mp = mc->mc_pg[mc->mc_top];
	mdb_node_del(mc, mc->mc_db->md_pad);
	mc->mc_db->md_entries--;
	{
		for (m2 = mc->mc_txn->mt_cursors[dbi]; m2; m2 = m2->mc_next) {
			m3 = (mc->mc_flags & C_SUB) ? &m2->mc_xcursor->mx_cursor
						    : m2;
			if (!(m2->mc_flags & m3->mc_flags & C_INITIALIZED))
				continue;
			if (m3 == mc || m3->mc_snum < mc->mc_snum) continue;
			if (m3->mc_pg[mc->mc_top] == mp) {
				if (m3->mc_ki[mc->mc_top] == ki) {
					m3->mc_flags |= C_DEL;
					if (mc->mc_db->md_flags & MDB_DUPSORT) {
						m3->mc_xcursor->mx_cursor
						    .mc_flags &=
						    ~(C_INITIALIZED | C_EOF);
					}
					continue;
				} else if (m3->mc_ki[mc->mc_top] > ki) {
					m3->mc_ki[mc->mc_top]--;
				}
				XCURSOR_REFRESH(m3, mc->mc_top, mp);
			}
		}
	}
	rc = mdb_rebalance(mc);
	if (rc) goto fail;

	if (!mc->mc_snum) {
		mc->mc_flags |= C_EOF;
		return rc;
	}

	mp = mc->mc_pg[mc->mc_top];
	nkeys = NUMKEYS(mp);

	for (m2 = mc->mc_txn->mt_cursors[dbi]; !rc && m2; m2 = m2->mc_next) {
		m3 = (mc->mc_flags & C_SUB) ? &m2->mc_xcursor->mx_cursor : m2;
		if (!(m2->mc_flags & m3->mc_flags & C_INITIALIZED)) continue;
		if (m3->mc_snum < mc->mc_snum) continue;
		if (m3->mc_pg[mc->mc_top] == mp) {
			if (m3->mc_ki[mc->mc_top] >= mc->mc_ki[mc->mc_top]) {
				if (m3->mc_ki[mc->mc_top] >= nkeys) {
					rc = mdb_cursor_sibling(m3, 1);
					if (rc == MDB_NOTFOUND) {
						m3->mc_flags |= C_EOF;
						rc = MDB_SUCCESS;
						continue;
					}
					if (rc) goto fail;
				}
				if (m3->mc_xcursor && !(m3->mc_flags & C_EOF)) {
					MDB_node *node =
					    NODEPTR(m3->mc_pg[m3->mc_top],
						    m3->mc_ki[m3->mc_top]);
					if (node->mn_flags & F_DUPDATA) {
						if (m3->mc_xcursor->mx_cursor
							.mc_flags &
						    C_INITIALIZED) {
							if (!(node->mn_flags &
							      F_SUBDATA))
								m3->mc_xcursor
								    ->mx_cursor
								    .mc_pg[0] =
								    NODEDATA(
									node);
						} else {
							mdb_xcursor_init1(m3,
									  node);
							rc = mdb_cursor_first(
							    &m3->mc_xcursor
								 ->mx_cursor,
							    NULL, NULL);
							if (rc) goto fail;
						}
					}
					m3->mc_xcursor->mx_cursor.mc_flags |=
					    C_DEL;
				}
			}
		}
	}
	mc->mc_flags |= C_DEL;

fail:
	if (rc) mc->mc_txn->mt_flags |= MDB_TXN_ERROR;
	return rc;
}

i32 mdb_del(MDB_txn *txn, MDB_dbi dbi, MDB_val *key, MDB_val *data) {
	if (!key || !TXN_DBI_EXIST(txn, dbi, DB_USRVALID)) return EINVAL;

	if (txn->mt_flags & (MDB_TXN_RDONLY | MDB_TXN_BLOCKED))
		return (txn->mt_flags & MDB_TXN_RDONLY) ? EACCES : MDB_BAD_TXN;

	if (!F_ISSET(txn->mt_dbs[dbi].md_flags, MDB_DUPSORT)) {
		data = NULL;
	}

	return mdb_del0(txn, dbi, key, data, 0);
}

STATIC i32 mdb_del0(MDB_txn *txn, MDB_dbi dbi, MDB_val *key, MDB_val *data,
		    u32 flags) {
	MDB_cursor mc;
	MDB_xcursor mx;
	MDB_cursor_op op;
	MDB_val rdata, *xdata;
	i32 rc, exact = 0;
	mdb_cursor_init(&mc, txn, dbi, &mx);

	if (data) {
		op = MDB_GET_BOTH;
		rdata = *data;
		xdata = &rdata;
	} else {
		op = MDB_SET;
		xdata = NULL;
		flags |= MDB_NODUPDATA;
	}
	rc = mdb_cursor_set(&mc, key, xdata, op, &exact);
	if (rc == 0) {
		mc.mc_next = txn->mt_cursors[dbi];
		txn->mt_cursors[dbi] = &mc;
		rc = _mdb_cursor_del(&mc, flags);
		txn->mt_cursors[dbi] = mc.mc_next;
	}
	return rc;
}

STATIC i32 mdb_page_split(MDB_cursor *mc, MDB_val *newkey, MDB_val *newdata,
			  u64 newpgno, u32 nflags) {
	u32 flags;
	i32 rc = MDB_SUCCESS, new_root = 0, did_split = 0;
	u16 newindx;
	u64 pgno = 0;
	i32 i, j, split_indx, nkeys, pmax;
	MDB_env *env = mc->mc_txn->mt_env;
	MDB_node *node;
	MDB_val sepkey, rkey, xdata, *rdata = &xdata;
	MDB_page *copy = NULL;
	MDB_page *mp, *rp, *pp;
	i32 ptop;
	MDB_cursor mn;

	mp = mc->mc_pg[mc->mc_top];
	newindx = mc->mc_ki[mc->mc_top];
	nkeys = NUMKEYS(mp);

	if ((rc = mdb_page_new(mc, mp->mp_flags, 1, &rp))) return rc;
	rp->mp_pad = mp->mp_pad;

	if (mc->mc_top < 1) {
		if ((rc = mdb_page_new(mc, P_BRANCH, 1, &pp))) goto done;
		for (i = mc->mc_snum; i > 0; i--) {
			mc->mc_pg[i] = mc->mc_pg[i - 1];
			mc->mc_ki[i] = mc->mc_ki[i - 1];
		}
		mc->mc_pg[0] = pp;
		mc->mc_ki[0] = 0;
		mc->mc_db->md_root = pp->mp_pgno;
		new_root = mc->mc_db->md_depth++;

		if ((rc = mdb_node_add(mc, 0, NULL, NULL, mp->mp_pgno, 0)) !=
		    MDB_SUCCESS) {
			mc->mc_pg[0] = mc->mc_pg[1];
			mc->mc_ki[0] = mc->mc_ki[1];
			mc->mc_db->md_root = mp->mp_pgno;
			mc->mc_db->md_depth--;
			goto done;
		}
		mc->mc_snum++;
		mc->mc_top++;
		ptop = 0;
	} else {
		ptop = mc->mc_top - 1;
	}

	mdb_cursor_copy(mc, &mn);
	mn.mc_xcursor = NULL;
	mn.mc_pg[mn.mc_top] = rp;
	mn.mc_ki[ptop] = mc->mc_ki[ptop] + 1;

	if (nflags & MDB_APPEND) {
		mn.mc_ki[mn.mc_top] = 0;
		sepkey = *newkey;
		split_indx = newindx;
		nkeys = 0;
	} else {
		split_indx = (nkeys + 1) / 2;

		if (IS_LEAF2(rp)) {
			u8 *split, *ins;
			i32 x;
			u32 lsize, rsize, ksize;
			x = mc->mc_ki[mc->mc_top] - split_indx;
			ksize = mc->mc_db->md_pad;
			split = LEAF2KEY(mp, split_indx, ksize);
			rsize = (nkeys - split_indx) * ksize;
			lsize = (nkeys - split_indx) * sizeof(u16);
			mp->mp_lower -= lsize;
			rp->mp_lower += lsize;
			mp->mp_upper += rsize - lsize;
			rp->mp_upper -= rsize - lsize;
			sepkey.mv_size = ksize;
			if (newindx == split_indx) {
				sepkey.mv_data = newkey->mv_data;
			} else {
				sepkey.mv_data = split;
			}
			if (x < 0) {
				ins =
				    LEAF2KEY(mp, mc->mc_ki[mc->mc_top], ksize);
				memcpy(rp->mp_ptrs, split, rsize);
				sepkey.mv_data = rp->mp_ptrs;
				memorymove(
				    ins + ksize, ins,
				    (split_indx - mc->mc_ki[mc->mc_top]) *
					ksize);
				memcpy(ins, newkey->mv_data, ksize);
				mp->mp_lower += sizeof(u16);
				mp->mp_upper -= ksize - sizeof(u16);
			} else {
				if (x) memcpy(rp->mp_ptrs, split, x * ksize);
				ins = LEAF2KEY(rp, x, ksize);
				memcpy(ins, newkey->mv_data, ksize);
				memcpy(ins + ksize, split + x * ksize,
				       rsize - x * ksize);
				rp->mp_lower += sizeof(u16);
				rp->mp_upper -= ksize - sizeof(u16);
				mc->mc_ki[mc->mc_top] = x;
			}
		} else {
			i32 psize, nsize, k, keythresh;

			pmax = env->me_psize - PAGEHDRSZ;
			keythresh = env->me_psize >> 7;

			if (IS_LEAF(mp))
				nsize = mdb_leaf_size(env, newkey, newdata);
			else
				nsize = mdb_branch_size(env, newkey);
			nsize = EVEN(nsize);

			copy = mdb_page_malloc(mc->mc_txn, 1);
			if (copy == NULL) {
				rc = ENOMEM;
				goto done;
			}
			copy->mp_pgno = mp->mp_pgno;
			copy->mp_flags = mp->mp_flags;
			copy->mp_lower = (PAGEHDRSZ - PAGEBASE);
			copy->mp_upper = env->me_psize - PAGEBASE;

			for (i = 0, j = 0; i < nkeys; i++) {
				if (i == newindx) {
					copy->mp_ptrs[j++] = 0;
				}
				copy->mp_ptrs[j++] = mp->mp_ptrs[i];
			}

			if (nkeys < keythresh || nsize > pmax / 16 ||
			    newindx >= nkeys) {
				psize = 0;
				if (newindx <= split_indx || newindx >= nkeys) {
					i = 0;
					j = 1;
					k = newindx >= nkeys
						? nkeys
						: split_indx + 1 + IS_LEAF(mp);
				} else {
					i = nkeys;
					j = -1;
					k = split_indx - 1;
				}
				for (; i != k; i += j) {
					if (i == newindx) {
						psize += nsize;
						node = NULL;
					} else {
						node =
						    (MDB_node
							 *)((u8 *)mp +
							    copy->mp_ptrs[i] +
							    PAGEBASE);
						psize += NODESIZE +
							 NODEKSZ(node) +
							 sizeof(u16);
						if (IS_LEAF(mp)) {
							if (F_ISSET(
								node->mn_flags,
								F_BIGDATA))
								psize +=
								    sizeof(u64);
							else
								psize +=
								    NODEDSZ(
									node);
						}
						psize = EVEN(psize);
					}
					if (psize > pmax || i == k - j) {
						split_indx = i + (j < 0);
						break;
					}
				}
			}
			if (split_indx == newindx) {
				sepkey.mv_size = newkey->mv_size;
				sepkey.mv_data = newkey->mv_data;
			} else {
				node = (MDB_node *)((u8 *)mp +
						    copy->mp_ptrs[split_indx] +
						    PAGEBASE);
				sepkey.mv_size = node->mn_ksize;
				sepkey.mv_data = NODEKEY(node);
			}
		}
	}

	if (SIZELEFT(mn.mc_pg[ptop]) < mdb_branch_size(env, &sepkey)) {
		i32 snum = mc->mc_snum;
		mn.mc_snum--;
		mn.mc_top--;
		did_split = 1;
		WITH_CURSOR_TRACKING(mn, rc = mdb_page_split(&mn, &sepkey, NULL,
							     rp->mp_pgno, 0));
		if (rc) goto done;
		if (mc->mc_snum > snum) ptop++;
		if (mn.mc_pg[ptop] != mc->mc_pg[ptop] &&
		    mc->mc_ki[ptop] >= NUMKEYS(mc->mc_pg[ptop])) {
			for (i = 0; i < ptop; i++) {
				mc->mc_pg[i] = mn.mc_pg[i];
				mc->mc_ki[i] = mn.mc_ki[i];
			}
			mc->mc_pg[ptop] = mn.mc_pg[ptop];
			if (mn.mc_ki[ptop]) {
				mc->mc_ki[ptop] = mn.mc_ki[ptop] - 1;
			} else {
				mc->mc_ki[ptop] = mn.mc_ki[ptop];
				rc = mdb_cursor_sibling(mc, 0);
			}
		}
	} else {
		mn.mc_top--;
		rc = mdb_node_add(&mn, mn.mc_ki[ptop], &sepkey, NULL,
				  rp->mp_pgno, 0);
		mn.mc_top++;
	}
	if (rc != MDB_SUCCESS) {
		if (rc == MDB_NOTFOUND) rc = MDB_PROBLEM;
		goto done;
	}
	if (nflags & MDB_APPEND) {
		mc->mc_pg[mc->mc_top] = rp;
		mc->mc_ki[mc->mc_top] = 0;
		rc = mdb_node_add(mc, 0, newkey, newdata, newpgno, nflags);
		if (rc) goto done;
		for (i = 0; i < mc->mc_top; i++) mc->mc_ki[i] = mn.mc_ki[i];
	} else if (!IS_LEAF2(mp)) {
		mc->mc_pg[mc->mc_top] = rp;
		i = split_indx;
		j = 0;
		do {
			if (i == newindx) {
				rkey.mv_data = newkey->mv_data;
				rkey.mv_size = newkey->mv_size;
				if (IS_LEAF(mp)) {
					rdata = newdata;
				} else
					pgno = newpgno;
				flags = nflags;
				mc->mc_ki[mc->mc_top] = j;
			} else {
				node =
				    (MDB_node *)((u8 *)mp + copy->mp_ptrs[i] +
						 PAGEBASE);
				rkey.mv_data = NODEKEY(node);
				rkey.mv_size = node->mn_ksize;
				if (IS_LEAF(mp)) {
					xdata.mv_data = NODEDATA(node);
					xdata.mv_size = NODEDSZ(node);
					rdata = &xdata;
				} else
					pgno = NODEPGNO(node);
				flags = node->mn_flags;
			}

			if (!IS_LEAF(mp) && j == 0) rkey.mv_size = 0;
			rc = mdb_node_add(mc, j, &rkey, rdata, pgno, flags);
			if (rc) goto done;
			if (i == nkeys) {
				i = 0;
				j = 0;
				mc->mc_pg[mc->mc_top] = copy;
			} else {
				i++;
				j++;
			}
		} while (i != split_indx);

		nkeys = NUMKEYS(copy);
		for (i = 0; i < nkeys; i++) mp->mp_ptrs[i] = copy->mp_ptrs[i];
		mp->mp_lower = copy->mp_lower;
		mp->mp_upper = copy->mp_upper;
		memcpy(NODEPTR(mp, nkeys - 1), NODEPTR(copy, nkeys - 1),
		       env->me_psize - copy->mp_upper - PAGEBASE);

		if (newindx < split_indx) {
			mc->mc_pg[mc->mc_top] = mp;
		} else {
			mc->mc_pg[mc->mc_top] = rp;
			mc->mc_ki[ptop]++;
			if (mn.mc_pg[ptop] != mc->mc_pg[ptop] &&
			    mc->mc_ki[ptop] >= NUMKEYS(mc->mc_pg[ptop])) {
				for (i = 0; i <= ptop; i++) {
					mc->mc_pg[i] = mn.mc_pg[i];
					mc->mc_ki[i] = mn.mc_ki[i];
				}
			}
		}
		if (nflags & MDB_RESERVE) {
			node = NODEPTR(mc->mc_pg[mc->mc_top],
				       mc->mc_ki[mc->mc_top]);
			if (!(node->mn_flags & F_BIGDATA))
				newdata->mv_data = NODEDATA(node);
		}
	} else {
		if (newindx >= split_indx) {
			mc->mc_pg[mc->mc_top] = rp;
			mc->mc_ki[ptop]++;
			if (mn.mc_pg[ptop] != mc->mc_pg[ptop] &&
			    mc->mc_ki[ptop] >= NUMKEYS(mc->mc_pg[ptop])) {
				for (i = 0; i <= ptop; i++) {
					mc->mc_pg[i] = mn.mc_pg[i];
					mc->mc_ki[i] = mn.mc_ki[i];
				}
			}
		}
	}

	{
		MDB_cursor *m2, *m3;
		MDB_dbi dbi = mc->mc_dbi;
		nkeys = NUMKEYS(mp);

		for (m2 = mc->mc_txn->mt_cursors[dbi]; m2; m2 = m2->mc_next) {
			if (mc->mc_flags & C_SUB)
				m3 = &m2->mc_xcursor->mx_cursor;
			else
				m3 = m2;
			if (m3 == mc) continue;
			if (!(m2->mc_flags & m3->mc_flags & C_INITIALIZED))
				continue;
			if (new_root) {
				i32 k;
				if (m3->mc_pg[0] != mp) continue;
				for (k = new_root; k >= 0; k--) {
					m3->mc_ki[k + 1] = m3->mc_ki[k];
					m3->mc_pg[k + 1] = m3->mc_pg[k];
				}
				if (m3->mc_ki[0] >= nkeys) {
					m3->mc_ki[0] = 1;
				} else {
					m3->mc_ki[0] = 0;
				}
				m3->mc_pg[0] = mc->mc_pg[0];
				m3->mc_snum++;
				m3->mc_top++;
			}
			if (m3->mc_top >= mc->mc_top &&
			    m3->mc_pg[mc->mc_top] == mp) {
				if (m3->mc_ki[mc->mc_top] >= newindx &&
				    !(nflags & MDB_SPLIT_REPLACE))
					m3->mc_ki[mc->mc_top]++;
				if (m3->mc_ki[mc->mc_top] >= nkeys) {
					m3->mc_pg[mc->mc_top] = rp;
					m3->mc_ki[mc->mc_top] -= nkeys;
					for (i = 0; i < mc->mc_top; i++) {
						m3->mc_ki[i] = mn.mc_ki[i];
						m3->mc_pg[i] = mn.mc_pg[i];
					}
				}
			} else if (!did_split && m3->mc_top >= ptop &&
				   m3->mc_pg[ptop] == mc->mc_pg[ptop] &&
				   m3->mc_ki[ptop] >= mc->mc_ki[ptop]) {
				m3->mc_ki[ptop]++;
			}
			if (IS_LEAF(mp))
				XCURSOR_REFRESH(m3, mc->mc_top,
						m3->mc_pg[mc->mc_top]);
		}
	}

done:
	if (copy) mdb_page_free(env, copy);
	if (rc) mc->mc_txn->mt_flags |= MDB_TXN_ERROR;
	return rc;
}

i32 mdb_put(MDB_txn *txn, MDB_dbi dbi, MDB_val *key, MDB_val *data, u32 flags) {
	MDB_cursor mc;
	MDB_xcursor mx;
	i32 rc;

	if (!key || !data || !TXN_DBI_EXIST(txn, dbi, DB_USRVALID))
		return EINVAL;

	if (flags & ~(MDB_NOOVERWRITE | MDB_NODUPDATA | MDB_RESERVE |
		      MDB_APPEND | MDB_APPENDDUP))
		return EINVAL;

	if (txn->mt_flags & (MDB_TXN_RDONLY | MDB_TXN_BLOCKED))
		return (txn->mt_flags & MDB_TXN_RDONLY) ? EACCES : MDB_BAD_TXN;

	mdb_cursor_init(&mc, txn, dbi, &mx);
	mc.mc_next = txn->mt_cursors[dbi];
	txn->mt_cursors[dbi] = &mc;
	rc = _mdb_cursor_put(&mc, key, data, flags);
	txn->mt_cursors[dbi] = mc.mc_next;
	return rc;
}

typedef struct mdb_copy {
	MDB_env *mc_env;
	MDB_txn *mc_txn;
	pthread_mutex_t mc_mutex;
	pthread_cond_t mc_cond;
	u8 *mc_wbuf[2];
	u8 *mc_over[2];
	u64 mc_wlen[2];
	u64 mc_olen[2];
	u64 mc_next_pgno;
	HANDLE mc_fd;
	i32 mc_toggle;
	i32 mc_new;
	volatile i32 mc_error;
} mdb_copy;

i32 mdb_env_set_flags(MDB_env *env, u32 flag, i32 onoff) {
	if (flag & ~CHANGEABLE) return EINVAL;
	if (onoff)
		env->me_flags |= flag;
	else
		env->me_flags &= ~flag;
	return MDB_SUCCESS;
}

i32 mdb_env_get_flags(MDB_env *env, u32 *arg) {
	if (!env || !arg) return EINVAL;

	*arg = env->me_flags & (CHANGEABLE | CHANGELESS);
	return MDB_SUCCESS;
}

i32 mdb_env_set_userctx(MDB_env *env, void *ctx) {
	if (!env) return EINVAL;
	env->me_userctx = ctx;
	return MDB_SUCCESS;
}

void *mdb_env_get_userctx(MDB_env *env) { return env ? env->me_userctx : NULL; }

i32 mdb_env_set_assert(MDB_env *env, MDB_assert_func *func) {
	if (!env) return EINVAL;
	env->me_assert_func = func;
	return MDB_SUCCESS;
}

i32 mdb_env_get_path(MDB_env *env, const u8 **arg) {
	if (!env || !arg) return EINVAL;

	*arg = env->me_path;
	return MDB_SUCCESS;
}

i32 mdb_env_get_fd(MDB_env *env, mdb_filehandle_t *arg) {
	if (!env || !arg) return EINVAL;

	*arg = env->me_fd;
	return MDB_SUCCESS;
}

STATIC i32 mdb_stat0(MDB_env *env, MDB_db *db, MDB_stat *arg) {
	arg->ms_psize = env->me_psize;
	arg->ms_depth = db->md_depth;
	arg->ms_branch_pages = db->md_branch_pages;
	arg->ms_leaf_pages = db->md_leaf_pages;
	arg->ms_overflow_pages = db->md_overflow_pages;
	arg->ms_entries = db->md_entries;

	return MDB_SUCCESS;
}

i32 mdb_env_stat(MDB_env *env, MDB_stat *arg) {
	MDB_meta *meta;

	if (env == NULL || arg == NULL) return EINVAL;

	meta = mdb_env_pick_meta(env);

	return mdb_stat0(env, &meta->mm_dbs[MAIN_DBI], arg);
}

i32 mdb_env_info(MDB_env *env, MDB_envinfo *arg) {
	MDB_meta *meta;

	if (env == NULL || arg == NULL) return EINVAL;

	meta = mdb_env_pick_meta(env);
	arg->me_mapaddr = meta->mm_address;
	arg->me_last_pgno = meta->mm_last_pg;
	arg->me_last_txnid = meta->mm_txnid;

	arg->me_mapsize = env->me_mapsize;
	arg->me_maxreaders = env->me_maxreaders;
	arg->me_numreaders = env->me_txns ? env->me_txns->mti_numreaders : 0;
	return MDB_SUCCESS;
}

STATIC void mdb_default_cmp(MDB_txn *txn, MDB_dbi dbi) {
	u16 f = txn->mt_dbs[dbi].md_flags;

	txn->mt_dbxs[dbi].md_cmp = (f & MDB_REVERSEKEY)	  ? mdb_cmp_memnr
				   : (f & MDB_INTEGERKEY) ? mdb_cmp_ci32
							  : mdb_cmp_memn;

	txn->mt_dbxs[dbi].md_dcmp =
	    !(f & MDB_DUPSORT)
		? 0
		: ((f & MDB_INTEGERDUP)
		       ? ((f & MDB_DUPFIXED) ? mdb_cmp_i32 : mdb_cmp_ci32)
		       : ((f & MDB_REVERSEDUP) ? mdb_cmp_memnr : mdb_cmp_memn));
}

i32 mdb_dbi_open(MDB_txn *txn, const u8 *name, u32 flags, MDB_dbi *dbi) {
	MDB_val key, data;
	MDB_dbi i;
	MDB_cursor mc;
	MDB_db dummy;
	i32 rc, dbflag, exact;
	u32 unused = 0, seq;
	u8 *namedup;
	u64 len;

	if (flags & ~VALID_FLAGS) return EINVAL;
	if (txn->mt_flags & MDB_TXN_BLOCKED) return MDB_BAD_TXN;

	if (!name) {
		*dbi = MAIN_DBI;
		if (flags & PERSISTENT_FLAGS) {
			u16 f2 = flags & PERSISTENT_FLAGS;
			if ((txn->mt_dbs[MAIN_DBI].md_flags | f2) !=
			    txn->mt_dbs[MAIN_DBI].md_flags) {
				txn->mt_dbs[MAIN_DBI].md_flags |= f2;
				txn->mt_flags |= MDB_TXN_DIRTY;
			}
		}
		mdb_default_cmp(txn, MAIN_DBI);
		return MDB_SUCCESS;
	}

	if (txn->mt_dbxs[MAIN_DBI].md_cmp == NULL) {
		mdb_default_cmp(txn, MAIN_DBI);
	}

	len = strlen(name);
	for (i = CORE_DBS; i < txn->mt_numdbs; i++) {
		if (!txn->mt_dbxs[i].md_name.mv_size) {
			if (!unused) unused = i;
			continue;
		}
		if (len == txn->mt_dbxs[i].md_name.mv_size &&
		    !strcmpn(name, txn->mt_dbxs[i].md_name.mv_data, len)) {
			*dbi = i;
			return MDB_SUCCESS;
		}
	}

	if (!unused && txn->mt_numdbs >= txn->mt_env->me_maxdbs)
		return MDB_DBS_FULL;

	if (txn->mt_dbs[MAIN_DBI].md_flags & (MDB_DUPSORT | MDB_INTEGERKEY))
		return (flags & MDB_CREATE) ? MDB_INCOMPATIBLE : MDB_NOTFOUND;

	dbflag = DB_NEW | DB_VALID | DB_USRVALID;
	exact = 0;
	key.mv_size = len;
	key.mv_data = (void *)name;
	mdb_cursor_init(&mc, txn, MAIN_DBI, NULL);
	rc = mdb_cursor_set(&mc, &key, &data, MDB_SET, &exact);
	if (rc == MDB_SUCCESS) {
		MDB_node *node =
		    NODEPTR(mc.mc_pg[mc.mc_top], mc.mc_ki[mc.mc_top]);
		if ((node->mn_flags & (F_DUPDATA | F_SUBDATA)) != F_SUBDATA)
			return MDB_INCOMPATIBLE;
	} else {
		if (rc != MDB_NOTFOUND || !(flags & MDB_CREATE)) return rc;
		if (F_ISSET(txn->mt_flags, MDB_TXN_RDONLY)) return EACCES;
	}

	if ((namedup = strdup(name)) == NULL) return ENOMEM;

	if (rc) {
		data.mv_size = sizeof(MDB_db);
		data.mv_data = &dummy;
		memset(&dummy, 0, sizeof(dummy));
		dummy.md_root = P_INVALID;
		dummy.md_flags = flags & PERSISTENT_FLAGS;
		WITH_CURSOR_TRACKING(
		    mc, rc = _mdb_cursor_put(&mc, &key, &data, F_SUBDATA));
		dbflag |= DB_DIRTY;
	}

	if (rc) {
		release(namedup);
	} else {
		u32 slot = unused ? unused : txn->mt_numdbs;
		txn->mt_dbxs[slot].md_name.mv_data = namedup;
		txn->mt_dbxs[slot].md_name.mv_size = len;
		txn->mt_dbxs[slot].md_rel = NULL;
		txn->mt_dbflags[slot] = dbflag;
		seq = ++txn->mt_env->me_dbiseqs[slot];
		txn->mt_dbiseqs[slot] = seq;

		memcpy(&txn->mt_dbs[slot], data.mv_data, sizeof(MDB_db));
		*dbi = slot;
		mdb_default_cmp(txn, slot);
		if (!unused) {
			txn->mt_numdbs++;
		}
	}

	return rc;
}

i32 mdb_stat(MDB_txn *txn, MDB_dbi dbi, MDB_stat *arg) {
	if (!arg || !TXN_DBI_EXIST(txn, dbi, DB_VALID)) return EINVAL;

	if (txn->mt_flags & MDB_TXN_BLOCKED) return MDB_BAD_TXN;

	if (txn->mt_dbflags[dbi] & DB_STALE) {
		MDB_cursor mc;
		MDB_xcursor mx;
		mdb_cursor_init(&mc, txn, dbi, &mx);
	}
	return mdb_stat0(txn->mt_env, &txn->mt_dbs[dbi], arg);
}

void mdb_dbi_close(MDB_env *env, MDB_dbi dbi) {
	u8 *ptr;
	if (dbi < CORE_DBS || dbi >= env->me_maxdbs) return;
	ptr = env->me_dbxs[dbi].md_name.mv_data;
	if (ptr) {
		env->me_dbxs[dbi].md_name.mv_data = NULL;
		env->me_dbxs[dbi].md_name.mv_size = 0;
		env->me_dbflags[dbi] = 0;
		env->me_dbiseqs[dbi]++;
		release(ptr);
	}
}

i32 mdb_dbi_flags(MDB_txn *txn, MDB_dbi dbi, u32 *flags) {
	if (!TXN_DBI_EXIST(txn, dbi, DB_USRVALID)) return EINVAL;
	*flags = txn->mt_dbs[dbi].md_flags & PERSISTENT_FLAGS;
	return MDB_SUCCESS;
}

STATIC i32 mdb_drop0(MDB_cursor *mc, i32 subs) {
	i32 rc;

	rc = mdb_page_search(mc, NULL, MDB_PS_FIRST);
	if (rc == MDB_SUCCESS) {
		MDB_txn *txn = mc->mc_txn;
		MDB_node *ni;
		MDB_cursor mx;
		u32 i;

		if ((mc->mc_flags & C_SUB) ||
		    (!subs && !mc->mc_db->md_overflow_pages))
			mdb_cursor_pop(mc);

		mdb_cursor_copy(mc, &mx);
		while (mc->mc_snum > 0) {
			MDB_page *mp = mc->mc_pg[mc->mc_top];
			u32 n = NUMKEYS(mp);
			if (IS_LEAF(mp)) {
				for (i = 0; i < n; i++) {
					ni = NODEPTR(mp, i);
					if (ni->mn_flags & F_BIGDATA) {
						MDB_page *omp;
						u64 pg;
						memcpy(&pg, NODEDATA(ni),
						       sizeof(pg));
						rc = mdb_page_get(mc, pg, &omp,
								  NULL);
						if (rc != 0) goto done;
						mdb_cassert(mc,
							    IS_OVERFLOW(omp));
						rc = mdb_midl_append_range(
						    &txn->mt_free_pgs, pg,
						    omp->mp_pages);
						if (rc) goto done;
						mc->mc_db->md_overflow_pages -=
						    omp->mp_pages;
						if (!mc->mc_db
							 ->md_overflow_pages &&
						    !subs)
							break;
					} else if (subs &&
						   (ni->mn_flags & F_SUBDATA)) {
						mdb_xcursor_init1(mc, ni);
						rc = mdb_drop0(
						    &mc->mc_xcursor->mx_cursor,
						    0);
						if (rc) goto done;
					}
				}
				if (!subs && !mc->mc_db->md_overflow_pages)
					goto pop;
			} else {
				if ((rc = mdb_midl_need(&txn->mt_free_pgs,
							n)) != 0)
					goto done;
				for (i = 0; i < n; i++) {
					u64 pg;
					ni = NODEPTR(mp, i);
					pg = NODEPGNO(ni);
					mdb_midl_xappend(txn->mt_free_pgs, pg);
				}
			}
			if (!mc->mc_top) break;
			mc->mc_ki[mc->mc_top] = i;
			rc = mdb_cursor_sibling(mc, 1);
			if (rc) {
				if (rc != MDB_NOTFOUND) goto done;
			pop:
				mdb_cursor_pop(mc);
				mc->mc_ki[0] = 0;
				for (i = 1; i < mc->mc_snum; i++) {
					mc->mc_ki[i] = 0;
					mc->mc_pg[i] = mx.mc_pg[i];
				}
			}
		}
		rc = mdb_midl_append(&txn->mt_free_pgs, mc->mc_db->md_root);
	done:
		if (rc) txn->mt_flags |= MDB_TXN_ERROR;
		MDB_CURSOR_UNREF(&mx, 0);
	} else if (rc == MDB_NOTFOUND) {
		rc = MDB_SUCCESS;
	}
	mc->mc_flags &= ~C_INITIALIZED;
	return rc;
}

i32 mdb_drop(MDB_txn *txn, MDB_dbi dbi, i32 del) {
	MDB_cursor *mc, *m2;
	i32 rc;

	if ((u32)del > 1 || !TXN_DBI_EXIST(txn, dbi, DB_USRVALID))
		return EINVAL;

	if (F_ISSET(txn->mt_flags, MDB_TXN_RDONLY)) return EACCES;

	if (TXN_DBI_CHANGED(txn, dbi)) return MDB_BAD_DBI;

	rc = mdb_cursor_open(txn, dbi, &mc);
	if (rc) return rc;

	rc = mdb_drop0(mc, mc->mc_db->md_flags & MDB_DUPSORT);
	for (m2 = txn->mt_cursors[dbi]; m2; m2 = m2->mc_next)
		m2->mc_flags &= ~(C_INITIALIZED | C_EOF);
	if (rc) goto leave;

	if (del && dbi >= CORE_DBS) {
		rc = mdb_del0(txn, MAIN_DBI, &mc->mc_dbx->md_name, NULL,
			      F_SUBDATA);
		if (!rc) {
			txn->mt_dbflags[dbi] = DB_STALE;
			mdb_dbi_close(txn->mt_env, dbi);
		} else {
			txn->mt_flags |= MDB_TXN_ERROR;
		}
	} else {
		txn->mt_dbflags[dbi] |= DB_DIRTY;
		txn->mt_dbs[dbi].md_depth = 0;
		txn->mt_dbs[dbi].md_branch_pages = 0;
		txn->mt_dbs[dbi].md_leaf_pages = 0;
		txn->mt_dbs[dbi].md_overflow_pages = 0;
		txn->mt_dbs[dbi].md_entries = 0;
		txn->mt_dbs[dbi].md_root = P_INVALID;

		txn->mt_flags |= MDB_TXN_DIRTY;
	}
leave:
	mdb_cursor_close(mc);
	return rc;
}

i32 mdb_set_compare(MDB_txn *txn, MDB_dbi dbi, MDB_cmp_func *cmp) {
	if (!TXN_DBI_EXIST(txn, dbi, DB_USRVALID)) return EINVAL;

	txn->mt_dbxs[dbi].md_cmp = cmp;
	return MDB_SUCCESS;
}

i32 mdb_set_dupsort(MDB_txn *txn, MDB_dbi dbi, MDB_cmp_func *cmp) {
	if (!TXN_DBI_EXIST(txn, dbi, DB_USRVALID)) return EINVAL;

	txn->mt_dbxs[dbi].md_dcmp = cmp;
	return MDB_SUCCESS;
}

i32 mdb_set_relfunc(MDB_txn *txn, MDB_dbi dbi, MDB_rel_func *rel) {
	if (!TXN_DBI_EXIST(txn, dbi, DB_USRVALID)) return EINVAL;

	txn->mt_dbxs[dbi].md_rel = rel;
	return MDB_SUCCESS;
}

i32 mdb_set_relctx(MDB_txn *txn, MDB_dbi dbi, void *ctx) {
	if (!TXN_DBI_EXIST(txn, dbi, DB_USRVALID)) return EINVAL;

	txn->mt_dbxs[dbi].md_relctx = ctx;
	return MDB_SUCCESS;
}

i32 mdb_env_get_maxkeysize(MDB_env *env __attribute__((unused))) {
	return ENV_MAXKEY(env);
}

STATIC i32 mdb_pid_insert(i32 *ids, i32 pid) {
	u32 base = 0;
	u32 cursor = 1;
	i32 val = 0;
	u32 n = ids[0];

	while (0 < n) {
		u32 pivot = n >> 1;
		cursor = base + pivot + 1;
		val = pid - ids[cursor];

		if (val < 0) {
			n = pivot;

		} else if (val > 0) {
			base = cursor;
			n -= pivot + 1;

		} else {
			return -1;
		}
	}

	if (val > 0) {
		++cursor;
	}
	ids[0]++;
	for (n = ids[0]; n > cursor; n--) ids[n] = ids[n - 1];
	ids[n] = pid;
	return 0;
}

i32 mdb_reader_check(MDB_env *env, i32 *dead) {
	if (!env) return EINVAL;
	if (dead) *dead = 0;
	return env->me_txns ? mdb_reader_check0(env, 0, dead) : MDB_SUCCESS;
}

STATIC i32 mdb_reader_check0(MDB_env *env, i32 rlocked, i32 *dead) {
	mdb_mutexref_t rmutex = rlocked ? NULL : env->me_rmutex;
	u32 i, j, rdrs;
	MDB_reader *mr;
	i32 *pids, pid;
	i32 rc = MDB_SUCCESS, count = 0;

	rdrs = env->me_txns->mti_numreaders;
	pids = alloc((rdrs + 1) * sizeof(i32));
	if (!pids) return ENOMEM;
	pids[0] = 0;
	mr = env->me_txns->mti_readers;
	for (i = 0; i < rdrs; i++) {
		pid = mr[i].mr_pid;
		if (pid && pid != env->me_pid) {
			if (mdb_pid_insert(pids, pid) == 0) {
				if (!mdb_reader_pid(env, Pidcheck, pid)) {
					j = i;
					if (rmutex) {
						if ((rc = LOCK_MUTEX0(
							 rmutex)) != 0) {
							if ((rc =
								 mdb_mutex_failed(
								     env,
								     rmutex,
								     rc)))
								break;
							rdrs = 0;
						} else {
							if (mdb_reader_pid(
								env, Pidcheck,
								pid))
								j = rdrs;
						}
					}
					for (; j < rdrs; j++)
						if (mr[j].mr_pid == pid) {
							mr[j].mr_pid = 0;
							count++;
						}
					if (rmutex) UNLOCK_MUTEX(rmutex);
				}
			}
		}
	}
	release(pids);
	if (dead) *dead = count;
	return rc;
}


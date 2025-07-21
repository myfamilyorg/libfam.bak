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

#ifndef _MIDL_H
#define _MIDL_H

#include <libfam/lmdb.H>
#include <libfam/types.H>

typedef u64 MDB_ID;
typedef MDB_ID *MDB_IDL;
typedef struct MDB_ID2 {
	MDB_ID mid;
	void *mptr;
} MDB_ID2;
typedef MDB_ID2 *MDB_ID2L;

#ifndef MDB_IDL_LOGN
#define MDB_IDL_LOGN 16 /* DB_SIZE is 2^16, UM_SIZE is 2^17 */
#endif
#define MDB_IDL_DB_SIZE (1 << MDB_IDL_LOGN)
#define MDB_IDL_UM_SIZE (1 << (MDB_IDL_LOGN + 1))

#define MDB_IDL_DB_MAX (MDB_IDL_DB_SIZE - 1)
#define MDB_IDL_UM_MAX (MDB_IDL_UM_SIZE - 1)

#define MDB_IDL_SIZEOF(ids) (((ids)[0] + 1) * sizeof(MDB_ID))
#define MDB_IDL_IS_ZERO(ids) ((ids)[0] == 0)
#define MDB_IDL_CPY(dst, src) (memcpy(dst, src, MDB_IDL_SIZEOF(src)))
#define MDB_IDL_FIRST(ids) ((ids)[1])
#define MDB_IDL_LAST(ids) ((ids)[(ids)[0]])

#define MDB_IDL_ALLOCLEN(ids) ((ids)[-1])

#define mdb_midl_xappend(idl, id)                         \
	do {                                              \
		MDB_ID *xidl = (idl), xlen = ++(xidl[0]); \
		xidl[xlen] = (id);                        \
	} while (0)

u32 mdb_midl_search(MDB_IDL ids, MDB_ID id);
MDB_IDL mdb_midl_alloc(i32 num);
void mdb_midl_free(MDB_IDL ids);
void mdb_midl_shrink(MDB_IDL *idp);
i32 mdb_midl_need(MDB_IDL *idp, u32 num);
i32 mdb_midl_append(MDB_IDL *idp, MDB_ID id);
i32 mdb_midl_append_list(MDB_IDL *idp, MDB_IDL app);
i32 mdb_midl_append_range(MDB_IDL *idp, MDB_ID id, u32 n);
void mdb_midl_xmerge(MDB_IDL idl, MDB_IDL merge);
void mdb_midl_sort(MDB_IDL ids);
u32 mdb_mid2l_search(MDB_ID2L ids, MDB_ID id);
i32 mdb_mid2l_insert(MDB_ID2L ids, MDB_ID2 *id);
i32 mdb_mid2l_append(MDB_ID2L ids, MDB_ID2 *id);

#endif /* _MIDL_H */

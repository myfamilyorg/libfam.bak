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

#include <libfam/lmdb.H>
#include <libfam/test.H>

Test(lmdb1) {
	const u8 *path = "/tmp/lmdb1";
	const u8 *path_lock = "/tmp/lmdb1-lock";
	MDB_env *env;
	MDB_txn *txn;
	MDB_dbi dbi;

	unlink(path);
	unlink(path_lock);

	ASSERT_BYTES(0);

	ASSERT(!mdb_env_create(&env), "mdb_env_create success");

	ASSERT(!mdb_env_open(env, path, MDB_NOSUBDIR | MDB_WRITEMAP | MDB_NOTLS,
			     0664),
	       "mdb_env_open success");

	ASSERT(!mdb_txn_begin(env, NULL, 0, &txn), "mdb_txn_begin success");

	ASSERT(!mdb_dbi_open(txn, NULL, 0, &dbi), "mdb_dbi_open success");

	MDB_val key, val_put, val_get;
	char key_str[] = "test_key";
	char val_str[] = "test_value";
	key.mv_size = sizeof(key_str);
	key.mv_data = key_str;
	val_put.mv_size = sizeof(val_str);
	val_put.mv_data = val_str;

	ASSERT(!mdb_put(txn, dbi, &key, &val_put, 0), "mdb_put success");

	ASSERT(!mdb_txn_commit(txn), "mdb_txn_commit success");

	ASSERT(!mdb_txn_begin(env, NULL, MDB_RDONLY, &txn),
	       "mdb_txn_begin (read) success");

	ASSERT(!mdb_get(txn, dbi, &key, &val_get), "mdb_get success");

	ASSERT(
	    val_get.mv_size == val_put.mv_size &&
		memcmp(val_get.mv_data, val_put.mv_data, val_put.mv_size) == 0,
	    "value match");

	mdb_txn_abort(txn);

	mdb_dbi_close(env, dbi);

	mdb_env_close(env);

	unlink(path);
	unlink(path_lock);

	ASSERT_BYTES(0);
}


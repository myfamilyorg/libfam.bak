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


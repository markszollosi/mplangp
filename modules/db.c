#include <sqlite3.h>
#include "mplangpfunc.h"

#define DB_MAX 16

static sqlite3 *dbs[DB_MAX];

static sqlite3 *handle(Value v)
{
	if (!mp_is_int(v) || mp_to_int(v) < 0 || mp_to_int(v) >= DB_MAX)
		return NULL;
	return dbs[mp_to_int(v)];
}

static bool d_open(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "open(path) takes a string; use \":memory:\" for a temporary database.");
	int slot = 0;
	while (slot < DB_MAX && dbs[slot])
		slot++;
	MP_CHECK(slot < DB_MAX, "too many open databases.");
	sqlite3 *db;
	if (sqlite3_open(argv[0].as.s, &db) != SQLITE_OK) {
		bool ok = mp_errorf(ret, "cannot open database '%s': %s", argv[0].as.s, db ? sqlite3_errmsg(db) : "out of memory");
		sqlite3_close(db);
		return ok;
	}
	dbs[slot] = db;
	*ret = mp_int(slot);
	return true;
}

static bool d_close(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1, "close(db) takes a database.");
	sqlite3 *db = handle(argv[0]);
	MP_CHECK(db, "close(db): not an open database.");
	sqlite3_close(db);
	dbs[mp_to_int(argv[0])] = NULL;
	return true;
}

static bool bind_all(sqlite3 *db, sqlite3_stmt *st, Value params, Value *ret)
{
	if (params.type == VAL_NONE)
		return true;
	List *l = params.as.list;
	if (sqlite3_bind_parameter_count(st) != l->count)
		return mp_errorf(ret, "statement needs %d parameters but got %d.", sqlite3_bind_parameter_count(st), l->count);
	for (int i = 0; i < l->count; i++) {
		Value v = l->items[i];
		int rc;
		switch (v.type) {
		case VAL_NONE: rc = sqlite3_bind_null(st, i + 1); break;
		case VAL_BOOL: case VAL_INT: rc = sqlite3_bind_int64(st, i + 1, mp_to_int(v)); break;
		case VAL_FLOAT: rc = sqlite3_bind_double(st, i + 1, v.as.f); break;
		case VAL_STRING: rc = sqlite3_bind_text(st, i + 1, v.as.s, -1, SQLITE_TRANSIENT); break;
		default: return mp_error(ret, "parameters must be none, bool, int, float or string.");
		}
		if (rc != SQLITE_OK)
			return mp_errorf(ret, "cannot bind parameter: %s", sqlite3_errmsg(db));
	}
	return true;
}

static bool args_ok(int argc, Value *argv)
{
	return argc >= 2 && argc <= 3 && mp_is_str(argv[1]) &&
	    (argc == 2 || argv[2].type == VAL_LIST || argv[2].type == VAL_NONE);
}

static bool d_exec(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc >= 1 && handle(argv[0]), "exec(db, sql, params): not an open database.");
	MP_CHECK(args_ok(argc, argv), "exec(db, sql, params) takes a database, a string and an optional list.");
	sqlite3 *db = handle(argv[0]);
	if (argc == 2) {
		char *msg = NULL;
		if (sqlite3_exec(db, argv[1].as.s, NULL, NULL, &msg) != SQLITE_OK) {
			bool ok = mp_errorf(ret, "sql error: %s", msg ? msg : "unknown");
			sqlite3_free(msg);
			return ok;
		}
		*ret = mp_int(sqlite3_changes(db));
		return true;
	}
	sqlite3_stmt *st;
	if (sqlite3_prepare_v2(db, argv[1].as.s, -1, &st, NULL) != SQLITE_OK)
		return mp_errorf(ret, "sql error: %s", sqlite3_errmsg(db));
	if (!bind_all(db, st, argv[2], ret)) {
		sqlite3_finalize(st);
		return false;
	}
	int rc;
	while ((rc = sqlite3_step(st)) == SQLITE_ROW)
		;
	sqlite3_finalize(st);
	if (rc != SQLITE_DONE)
		return mp_errorf(ret, "sql error: %s", sqlite3_errmsg(db));
	*ret = mp_int(sqlite3_changes(db));
	return true;
}

static bool d_query(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc >= 1 && handle(argv[0]), "query(db, sql, params): not an open database.");
	MP_CHECK(args_ok(argc, argv), "query(db, sql, params) takes a database, a string and an optional list.");
	sqlite3 *db = handle(argv[0]);
	sqlite3_stmt *st;
	if (sqlite3_prepare_v2(db, argv[1].as.s, -1, &st, NULL) != SQLITE_OK)
		return mp_errorf(ret, "sql error: %s", sqlite3_errmsg(db));
	if (!bind_all(db, st, argc == 3 ? argv[2] : mp_none(), ret)) {
		sqlite3_finalize(st);
		return false;
	}
	Value rows = mp_list();
	int rc, n = sqlite3_column_count(st);
	while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
		Value row = mp_map();
		for (int i = 0; i < n; i++) {
			Value v;
			switch (sqlite3_column_type(st, i)) {
			case SQLITE_INTEGER: v = mp_int(sqlite3_column_int64(st, i)); break;
			case SQLITE_FLOAT: v = mp_float(sqlite3_column_double(st, i)); break;
			case SQLITE_NULL: v = mp_none(); break;
			default:
				v = mp_string_clean((const char *)sqlite3_column_text(st, i), (size_t)sqlite3_column_bytes(st, i));
			}
			mp_set(row, sqlite3_column_name(st, i), v);
		}
		mp_push(rows, row);
	}
	sqlite3_finalize(st);
	if (rc != SQLITE_DONE) {
		mp_value_release(rows);
		return mp_errorf(ret, "sql error: %s", sqlite3_errmsg(db));
	}
	*ret = rows;
	return true;
}

static bool d_last_id(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && handle(argv[0]), "last_id(db): not an open database.");
	*ret = mp_int(sqlite3_last_insert_rowid(handle(argv[0])));
	return true;
}

MP_MODULE({"open", d_open}, {"close", d_close}, {"exec", d_exec}, {"query", d_query}, {"last_id", d_last_id});

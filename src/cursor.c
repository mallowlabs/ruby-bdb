#include "bdb.h"

static void 
bdb_cursor_free(dbcst)
    bdb_DBC *dbcst;
{
    bdb_DB *dbst;
    if (dbcst->dbc && dbcst->db) {
	GetDB(dbcst->db, dbst);
        bdb_test_error(dbcst->dbc->c_close(dbcst->dbc));
        dbcst->dbc = NULL;
	dbcst->db = 0;
    }
    free(dbcst);
}

static VALUE
bdb_cursor(obj)
    VALUE obj;
{
    bdb_DB *dbst;
    DB_TXN *txnid;
    bdb_DBC *dbcst;
    DBC *dbc;
    VALUE a;
    int flags;

    init_txn(txnid, obj, dbst);
    flags = 0;
#if DB_VERSION_MAJOR == 2 && DB_VERSION_MINOR < 6
    bdb_test_error(dbst->dbp->cursor(dbst->dbp, txnid, &dbc));
#else
    bdb_test_error(dbst->dbp->cursor(dbst->dbp, txnid, &dbc, flags));
#endif
    a = Data_Make_Struct(bdb_cCursor, bdb_DBC, 0, bdb_cursor_free, dbcst);
    dbcst->dbc = dbc;
    dbcst->db = obj;
    return a;
}

static VALUE
bdb_cursor_close(obj)
    VALUE obj;
{
    bdb_DBC *dbcst;
    bdb_DB *dbst;
    
    if (!OBJ_TAINTED(obj) && rb_safe_level() >= 4)
	rb_raise(rb_eSecurityError, "Insecure: can't close the cursor");
    GetCursorDB(obj, dbcst, dbst);
    bdb_test_error(dbcst->dbc->c_close(dbcst->dbc));
    dbcst->dbc = NULL;
    return Qtrue;
}
  
static VALUE
bdb_cursor_del(obj)
    VALUE obj;
{
    int flags = 0;
    bdb_DBC *dbcst;
    bdb_DB *dbst;
    
    rb_secure(4);
    GetCursorDB(obj, dbcst, dbst);
    bdb_test_error(dbcst->dbc->c_del(dbcst->dbc, flags));
    return Qtrue;
}

#if DB_VERSION_MAJOR >= 3

static VALUE
bdb_cursor_dup(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    int ret, flags = 0;
    VALUE a, b;
    bdb_DBC *dbcst, *dbcstdup;
    bdb_DB *dbst;
    DBC *dbcdup;
    
    if (rb_scan_args(argc, argv, "01", &a))
        flags = NUM2INT(a);
    GetCursorDB(obj, dbcst, dbst);
    bdb_test_error(dbcst->dbc->c_dup(dbcst->dbc, &dbcdup, flags));
    b = Data_Make_Struct(bdb_cCursor, bdb_DBC, 0, bdb_cursor_free, dbcstdup);
    dbcstdup->dbc = dbcdup;
    dbcstdup->db = dbcst->db;
    return b;
}

#endif

static VALUE
bdb_cursor_count(obj)
    VALUE obj;
{
#if DB_VERSION_MAJOR == 2 && DB_VERSION_MINOR < 6
    rb_raise(bdb_eFatal, "DB_NEXT_DUP needs Berkeley DB 2.6 or later");
#else
    int ret;

#if !((DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR >= 1) || DB_VERSION_MAJOR >= 4)
    DBT key, data;
    DBT key_o, data_o;
#endif
    bdb_DBC *dbcst;
    bdb_DB *dbst;
    db_recno_t count;

    GetCursorDB(obj, dbcst, dbst);
#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR >= 1) || DB_VERSION_MAJOR >= 4
    bdb_test_error(dbcst->dbc->c_count(dbcst->dbc, &count, 0));
    return INT2NUM(count);
#else
    MEMZERO(&key, DBT, 1);
    key.flags |= DB_DBT_MALLOC;
    MEMZERO(&data, DBT, 1);
    data.flags |= DB_DBT_MALLOC;
    MEMZERO(&key_o, DBT, 1);
    key_o.flags |= DB_DBT_MALLOC;
    MEMZERO(&data_o, DBT, 1);
    data_o.flags |= DB_DBT_MALLOC;
    set_partial(dbst, data);
    ret = bdb_test_error(dbcst->dbc->c_get(dbcst->dbc, &key_o, &data_o, DB_CURRENT));
    if (ret == DB_NOTFOUND || ret == DB_KEYEMPTY)
        return INT2NUM(0);
    count = 1;
    while (1) {
	ret = bdb_test_error(dbcst->dbc->c_get(dbcst->dbc, &key, &data, DB_NEXT_DUP));
	if (ret == DB_NOTFOUND) {
	    bdb_test_error(dbcst->dbc->c_get(dbcst->dbc, &key_o, &data_o, DB_SET));

	    free_key(dbst, key_o);
	    free(data_o.data);
	    return INT2NUM(count);
	}
	if (ret == DB_KEYEMPTY) continue;
	free_key(dbst, key);
	free(data.data);
	count++;
    }
    return INT2NUM(-1);
#endif
#endif
}

static VALUE
bdb_cursor_get_common(argc, argv, obj, c_pget)
    int argc;
    VALUE *argv;
    VALUE obj;
    int c_pget;
{
    VALUE a, b, c;
    int flags, cnt, ret;
    DBT key, data, pkey;
    bdb_DBC *dbcst;
    bdb_DB *dbst;
    db_recno_t recno;

    cnt = rb_scan_args(argc, argv, "12", &a, &b, &c);
    flags = NUM2INT(a);
    MEMZERO(&key, DBT, 1);
    MEMZERO(&pkey, DBT, 1);
    pkey.flags |= DB_DBT_MALLOC;
    MEMZERO(&data, DBT, 1);
    GetCursorDB(obj, dbcst, dbst);
    if (flags == DB_SET_RECNO) {
	if (dbst->type != DB_BTREE || !(dbst->flags & DB_RECNUM)) {
	    rb_raise(bdb_eFatal, "database must be Btree with RECNUM for SET_RECNO");
	}
        if (cnt != 2)
            rb_raise(bdb_eFatal, "invalid number of arguments");
	recno = NUM2INT(b);
	key.data = &recno;
	key.size = sizeof(db_recno_t);
	key.flags |= DB_DBT_MALLOC;
	data.flags |= DB_DBT_MALLOC;
    }
    else if (flags == DB_SET || flags == DB_SET_RANGE) {
        if (cnt != 2)
            rb_raise(bdb_eFatal, "invalid number of arguments");
        test_recno(dbst, key, recno, b);
	data.flags |= DB_DBT_MALLOC;
    }
#if DB_VERSION_MAJOR > 2 || (DB_VERSION_MAJOR == 2 && DB_VERSION_MINOR >= 6)
    else if (flags == DB_GET_BOTH) {
        if (cnt != 3)
            rb_raise(bdb_eFatal, "invalid number of arguments");
        test_recno(dbst, key, recno, b);
        test_dump(dbst, data, c);
    }
#endif
    else {
	if (cnt != 1) {
	    rb_raise(bdb_eFatal, "invalid number of arguments");
	}
	key.flags |= DB_DBT_MALLOC;
	data.flags |= DB_DBT_MALLOC;
    }
    set_partial(dbst, data);
#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR >= 3) || DB_VERSION_MAJOR >= 4
    if (c_pget) {
	if (dbst->secondary != Qnil) {
	    rb_raise(bdb_eFatal, "pget must be used with a secondary index");
	}
	ret = bdb_test_error(dbcst->dbc->c_pget(dbcst->dbc, &key, &pkey, &data, flags));
    }
    else
#endif
    {

	ret = bdb_test_error(dbcst->dbc->c_get(dbcst->dbc, &key, &data, flags));
    }
    if (ret == DB_NOTFOUND || ret == DB_KEYEMPTY)
        return Qnil;
    if (c_pget) {
	return bdb_assoc3(dbst, key, pkey, data);
    }
    else {
	return bdb_assoc_dyna(dbcst->db, key, data);
    }
}

static VALUE
bdb_cursor_get(argc, argv, obj)
{
    return bdb_cursor_get_common(argc, argv, obj, 0);
}

#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR >= 3) || DB_VERSION_MAJOR >= 4
static VALUE
bdb_cursor_pget(argc, argv, obj)
{
    return bdb_cursor_get_common(argc, argv, obj, 1);
}
#endif

static VALUE
bdb_cursor_set_xxx(obj, a, flag)
    VALUE obj, a;
    int flag;
{
    VALUE *b;
    b = ALLOCA_N(VALUE, 2);
    b[0] = INT2NUM(flag);
    b[1] = a;
    return bdb_cursor_get(2, b, obj);
}

static VALUE 
bdb_cursor_set(obj, a)
    VALUE obj, a;
{ 
    return bdb_cursor_set_xxx(obj, a, DB_SET);
}

static VALUE 
bdb_cursor_set_range(obj, a)
    VALUE obj, a;
{ 
    return bdb_cursor_set_xxx(obj, a, DB_SET_RANGE);
}

static VALUE 
bdb_cursor_set_recno(obj, a)
    VALUE obj, a;
{ 
    return bdb_cursor_set_xxx(obj, a, DB_SET_RECNO);
}

static VALUE
bdb_cursor_xxx(obj, val)
    VALUE obj;
    int val;
{
    VALUE b;
    b = INT2NUM(val);
    return bdb_cursor_get(1, &b, obj);
}

static VALUE
bdb_cursor_next(obj)
    VALUE obj;
{
    return bdb_cursor_xxx(obj, DB_NEXT);
}

#if DB_VERSION_MAJOR >= 3 || (DB_VERSION_MAJOR == 2 && DB_VERSION_MINOR >= 6)

static VALUE
bdb_cursor_next_dup(obj)
    VALUE obj;
{
    return bdb_cursor_xxx(obj, DB_NEXT_DUP);
}

#endif

static VALUE
bdb_cursor_prev(obj)
    VALUE obj;
{
    return bdb_cursor_xxx(obj, DB_PREV);
}

static VALUE
bdb_cursor_first(obj)
    VALUE obj;
{
    return bdb_cursor_xxx(obj, DB_FIRST);
}

static VALUE
bdb_cursor_last(obj)
    VALUE obj;
{
    return bdb_cursor_xxx(obj, DB_LAST);
}

static VALUE
bdb_cursor_current(obj)
    VALUE obj;
{
    return bdb_cursor_xxx(obj, DB_CURRENT);
}

static VALUE
bdb_cursor_put(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    int flags, cnt;
    DBT key, data;
    bdb_DBC *dbcst;
    bdb_DB *dbst;
    VALUE a, b, c, f;
    db_recno_t recno;
    int ret;

    rb_secure(4);
    MEMZERO(&key, DBT, 1);
    MEMZERO(&data, DBT, 1);
    cnt = rb_scan_args(argc, argv, "21", &a, &b, &c);
    GetCursorDB(obj, dbcst, dbst);
    flags = NUM2INT(c);
    if (flags & (DB_KEYFIRST | DB_KEYLAST)) {
        if (cnt != 3)
            rb_raise(bdb_eFatal, "invalid number of arguments");
        test_recno(dbst, key, recno, b);
        test_dump(dbst, data, c);
	f = c;
    }
    else {
        test_dump(dbst, data, b);
	f = b;
    }
    set_partial(dbst, data);
    ret = bdb_test_error(dbcst->dbc->c_put(dbcst->dbc, &key, &data, flags));
    if (cnt == 3) {
	free_key(dbst, key);
    }
    if (data.flags & DB_DBT_MALLOC)
	free(data.data);
    if (ret == DB_KEYEXIST) {
	return Qfalse;
    }
    else {
	if (dbst->partial) {
	    return bdb_cursor_current(obj);
	}
	else {
	    return f;
	}
    }
}

void bdb_init_cursor()
{
    rb_define_method(bdb_cCommon, "db_cursor", bdb_cursor, 0);
    rb_define_method(bdb_cCommon, "cursor", bdb_cursor, 0);
    bdb_cCursor = rb_define_class_under(bdb_mDb, "Cursor", rb_cObject);
    rb_undef_method(CLASS_OF(bdb_cCursor), "new");
    rb_define_method(bdb_cCursor, "close", bdb_cursor_close, 0);
    rb_define_method(bdb_cCursor, "c_close", bdb_cursor_close, 0);
    rb_define_method(bdb_cCursor, "c_del", bdb_cursor_del, 0);
    rb_define_method(bdb_cCursor, "del", bdb_cursor_del, 0);
    rb_define_method(bdb_cCursor, "delete", bdb_cursor_del, 0);
#if DB_VERSION_MAJOR >= 3
    rb_define_method(bdb_cCursor, "dup", bdb_cursor_dup, -1);
    rb_define_method(bdb_cCursor, "c_dup", bdb_cursor_dup, -1);
    rb_define_method(bdb_cCursor, "clone", bdb_cursor_dup, -1);
    rb_define_method(bdb_cCursor, "c_clone", bdb_cursor_dup, -1);
#endif
    rb_define_method(bdb_cCursor, "count", bdb_cursor_count, 0);
    rb_define_method(bdb_cCursor, "c_count", bdb_cursor_count, 0);
    rb_define_method(bdb_cCursor, "get", bdb_cursor_get, -1);
    rb_define_method(bdb_cCursor, "c_get", bdb_cursor_get, -1);
#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR >= 3) || DB_VERSION_MAJOR >= 4
    rb_define_method(bdb_cCursor, "pget", bdb_cursor_pget, -1);
    rb_define_method(bdb_cCursor, "c_pget", bdb_cursor_pget, -1);
#endif
    rb_define_method(bdb_cCursor, "put", bdb_cursor_put, -1);
    rb_define_method(bdb_cCursor, "c_put", bdb_cursor_put, -1);
    rb_define_method(bdb_cCursor, "c_next", bdb_cursor_next, 0);
    rb_define_method(bdb_cCursor, "next", bdb_cursor_next, 0);
#if DB_VERSION_MAJOR == 3 || (DB_VERSION_MAJOR == 2 && DB_VERSION_MINOR >= 6)
    rb_define_method(bdb_cCursor, "c_next_dup", bdb_cursor_next_dup, 0);
    rb_define_method(bdb_cCursor, "next_dup", bdb_cursor_next_dup, 0);
#endif
    rb_define_method(bdb_cCursor, "c_first", bdb_cursor_first, 0);
    rb_define_method(bdb_cCursor, "first", bdb_cursor_first, 0);
    rb_define_method(bdb_cCursor, "c_last", bdb_cursor_last, 0);
    rb_define_method(bdb_cCursor, "last", bdb_cursor_last, 0);
    rb_define_method(bdb_cCursor, "c_current", bdb_cursor_current, 0);
    rb_define_method(bdb_cCursor, "current", bdb_cursor_current, 0);
    rb_define_method(bdb_cCursor, "c_prev", bdb_cursor_prev, 0);
    rb_define_method(bdb_cCursor, "prev", bdb_cursor_prev, 0);
    rb_define_method(bdb_cCursor, "c_set", bdb_cursor_set, 1);
    rb_define_method(bdb_cCursor, "set", bdb_cursor_set, 1);
    rb_define_method(bdb_cCursor, "c_set_range", bdb_cursor_set_range, 1);
    rb_define_method(bdb_cCursor, "set_range", bdb_cursor_set_range, 1);
    rb_define_method(bdb_cCursor, "c_set_recno", bdb_cursor_set_recno, 1);
    rb_define_method(bdb_cCursor, "set_recno", bdb_cursor_set_recno, 1);
}

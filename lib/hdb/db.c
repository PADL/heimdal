/*
 * Copyright (c) 1997 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. All advertising materials mentioning features or use of this software 
 *    must display the following acknowledgement: 
 *      This product includes software developed by Kungliga Tekniska 
 *      H�gskolan and its contributors. 
 *
 * 4. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include "hdb_locl.h"

RCSID("$Id$");

#ifdef HAVE_DB_H

static krb5_error_code
DB_close(krb5_context context, HDB *db)
{
    DB *d = (DB*)db->db;
    d->close(d);
    free(db->name);
    free(db);
    return 0;
}

static krb5_error_code
DB_lock(krb5_context context, HDB *db, int operation)
{
    DB *d = (DB*)db->db;
    int fd = (*d->fd)(d);
    if(fd < 0)
	return HDB_ERR_CANT_LOCK_DB;
    return hdb_lock(fd, operation);
}

static krb5_error_code
DB_unlock(krb5_context context, HDB *db)
{
    DB *d = (DB*)db->db;
    int fd = (*d->fd)(d);
    if(fd < 0)
	return HDB_ERR_CANT_LOCK_DB;
    return hdb_unlock(fd);
}


static krb5_error_code
DB_op(krb5_context context, HDB *db, hdb_entry *entry, int op)
{
    DB *d = (DB*)db->db;
    DBT key, value;
    krb5_data data;
    int code;

    hdb_principal2key(context, entry->principal, &data);
    key.data = data.data;
    key.size = data.length;
    switch(op){
    case 0:
	code = db->lock(context, db, HDB_RLOCK);
	if(code)
	    return code;
	code = d->get(d, &key, &value, 0);
	db->unlock(context, db); /* XXX check value */
	break;
    case 1:
	hdb_entry2value(context, entry, &data);
	value.data = data.data;
	value.size = data.length;
	code = db->lock(context, db, HDB_WLOCK);
	if(code)
	    return code;
	code = d->put(d, &key, &value, 0);
	db->unlock(context, db); /* XXX check value */
	krb5_data_free(&data);
	break;
    case 2:
	code = db->lock(context, db, HDB_WLOCK);
	if(code)
	    return code;
	code = d->del(d, &key, 0);
	db->unlock(context, db); /* XXX check value */
	break;
    }
    data.data = key.data;
    data.length = key.size;
    krb5_data_free(&data);
    if(code < 0)
	return errno;
    if(code == 1)
	return HDB_ERR_NOENTRY;
    if(op == 0){
	data.data = value.data;
	data.length = value.size;
	hdb_value2entry(context, &data, entry);
    }
    return 0;
}

static krb5_error_code
DB_fetch(krb5_context context, HDB *db, hdb_entry *entry)
{
    return DB_op(context, db, entry, 0);
}

static krb5_error_code
DB_store(krb5_context context, HDB *db, hdb_entry *entry)
{
    return DB_op(context, db, entry, 1);
}

static krb5_error_code
DB_delete(krb5_context context, HDB *db, hdb_entry *entry)
{
    return DB_op(context, db, entry, 2);
}

static krb5_error_code
DB_seq(krb5_context context, HDB *db, hdb_entry *entry, int flag)
{
    DB *d = (DB*)db->db;
    DBT key, value;
    krb5_data key_data, data;
    int code;
    krb5_principal principal;

    code = db->lock(context, db, HDB_RLOCK);
    if(code == -1)
	return HDB_ERR_DB_INUSE;
    code = d->seq(d, &key, &value, flag);
    db->unlock(context, db); /* XXX check value */
    if(code == -1)
	return errno;
    if(code == 1)
	return HDB_ERR_NOENTRY;

    key_data.data = key.data;
    key_data.length = key.size;
    data.data = value.data;
    data.length = value.size;
    hdb_value2entry(context, &data, entry);
    if (entry->principal == NULL) {
	entry->principal = malloc(sizeof(*entry->principal));
	hdb_key2principal(context, &key_data, entry->principal);
    }
    return 0;
}


static krb5_error_code
DB_firstkey(krb5_context context, HDB *db, hdb_entry *entry)
{
    return DB_seq(context, db, entry, R_FIRST);
}


static krb5_error_code
DB_nextkey(krb5_context context, HDB *db, hdb_entry *entry)
{
    return DB_seq(context, db, entry, R_NEXT);
}

static krb5_error_code
DB__get(krb5_context context, HDB *db, krb5_data key, krb5_data *reply)
{
    DB *d = (DB*)db->db;
    DBT k, v;
    int code;

    k.data = key.data;
    k.size = key.length;
    code = db->lock(context, db, HDB_RLOCK);
    if(code)
	return code;
    code = d->get(d, &k, &v, 0);
    db->unlock(context, db);
    if(code < 0)
	return errno;
    if(code == 1)
	return HDB_ERR_NOENTRY;
    
    reply->length = v.size;
    reply->data = malloc(reply->length);
    memcpy(reply->data, v.data, reply->length);
    return 0;
}

static krb5_error_code
DB_rename(krb5_context context, HDB *db, const char *new_name)
{
    int ret;
    char *old, *new;

    asprintf(&old, "%s.db", db->name);
    asprintf(&new, "%s.db", new_name);
    ret = rename(old, new);
    free(old);
    free(new);
    if(ret)
	return errno;
    
    free(db->name);
    db->name = strdup(new_name);
    return 0;
}

krb5_error_code
hdb_db_open(krb5_context context, HDB **db, 
	    const char *filename, int flags, mode_t mode)
{
    DB *d;
    char *fn;
    asprintf(&fn, "%s.db", filename);
    d = dbopen(fn, flags, mode, DB_BTREE, NULL);
    free(fn);
    if(d == NULL)
	return errno;
    *db = malloc(sizeof(**db));
    (*db)->db = d;
    (*db)->name = strdup(filename);
    (*db)->close = DB_close;
    (*db)->fetch = DB_fetch;
    (*db)->store = DB_store;
    (*db)->delete = DB_delete;
    (*db)->firstkey = DB_firstkey;
    (*db)->nextkey= DB_nextkey;
    (*db)->lock = DB_lock;
    (*db)->unlock = DB_unlock;
    (*db)->rename = DB_rename;
    return 0;
}


#endif

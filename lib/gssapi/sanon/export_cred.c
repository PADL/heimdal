/*
 * Copyright (c) 2019-2020, AuriStor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "sanon_locl.h"

OM_uint32 GSSAPI_CALLCONV
_gss_sanon_export_cred(OM_uint32 *minor,
		       gss_cred_id_t input_cred,
		       gss_buffer_t token)
{
    OM_uint32 major, junk;
    gss_buffer_desc name_token = GSS_C_EMPTY_BUFFER;
    krb5_storage *sp;
    krb5_error_code ret;
    krb5_data data;

    major = _gss_sanon_export_name(minor, (gss_name_t)input_cred, &name_token);
    if (major != GSS_S_COMPLETE)
	return major;

    sp = krb5_storage_emem();
    if (sp == NULL) {
	gss_release_buffer(&junk, &name_token);
	*minor = ENOMEM;
	return GSS_S_FAILURE;
    }

    data.length = GSS_SANON_X25519_MECHANISM->length;
    data.data = GSS_SANON_X25519_MECHANISM->elements;

    ret = krb5_store_data(sp, data);
    if (ret == 0) {
	data.length = name_token.length;
	data.data = name_token.value;

	ret = krb5_store_data(sp, data);
    }
    if (ret == 0) {
	ret = krb5_storage_to_data(sp, &data);
    }
    if (ret == 0) {
	token->length = data.length;
	token->value = data.data;
    }

    gss_release_buffer(&junk, &name_token);
    krb5_storage_free(sp);

    *minor = ret;
    return ret ? GSS_S_FAILURE : GSS_S_COMPLETE;
}


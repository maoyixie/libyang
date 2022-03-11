/**
 * @file identityref.c
 * @author Radek Krejci <rkrejci@cesnet.cz>
 * @brief Built-in identityref type plugin.
 *
 * Copyright (c) 2019-2021 CESNET, z.s.p.o.
 *
 * This source code is licensed under BSD 3-Clause License (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 */

#define _GNU_SOURCE /* asprintf */

#include "plugins_types.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "libyang.h"

/* additional internal headers for some useful simple macros */
#include "common.h"
#include "compat.h"
#include "plugins_internal.h" /* LY_TYPE_*_STR */

/**
 * @page howtoDataLYB LYB Binary Format
 * @subsection howtoDataLYBTypesIdentityref identityref (built-in)
 *
 * | Size (B) | Mandatory | Type | Meaning |
 * | :------  | :-------: | :--: | :-----: |
 * | string length | yes | `char *` | string JSON format of the identityref |
 */

/**
 * @brief Print an identityref value in a specific format.
 *
 * @param[in] ident Identityref to print.
 * @param[in] format Value format.
 * @param[in] prefix_data Format-specific data for resolving prefixes.
 * @param[out] str Printed identityref.
 * @param[out] str_len Optional length of @p str.
 * @return LY_ERR value.
 */
static LY_ERR
identityref_ident2str(const struct lysc_ident *ident, LY_VALUE_FORMAT format, void *prefix_data, char **str, size_t *str_len)
{
    int len;

    len = asprintf(str, "%s:%s", lyplg_type_get_prefix(ident->module, format, prefix_data), ident->name);
    if (len == -1) {
        return LY_EMEM;
    }

    if (str_len) {
        *str_len = (size_t)len;
    }
    return LY_SUCCESS;
}

/**
 * @brief Check that an identityref is derived from the type base.
 *
 * @param[in] ident Derived identity to which identityref points.
 * @param[in] type Identityref type.
 * @param[in] value String value for logging.
 * @param[in] value_len Length of @p value.
 * @param[out] err Error information on error.
 */
static LY_ERR
identityref_check_base(const struct lysc_ident *ident, struct lysc_type_identityref *type, const char *value,
        size_t value_len, struct ly_err_item **err)
{
    LY_ERR ret;
    size_t str_len;
    char *str;
    LY_ARRAY_COUNT_TYPE u;
    struct lysc_ident *base;

    /* check that the identity matches some of the type's base identities */
    LY_ARRAY_FOR(type->bases, u) {
        if (!lyplg_type_identity_isderived(type->bases[u], ident)) {
            /* we have match */
            break;
        }
    }

    /* it does not, generate a nice error */
    if (u == LY_ARRAY_COUNT(type->bases)) {
        str = NULL;
        str_len = 1;
        LY_ARRAY_FOR(type->bases, u) {
            base = type->bases[u];
            str_len += (u ? 2 : 0) + 1 + strlen(base->module->name) + 1 + strlen(base->name) + 1;
            str = ly_realloc(str, str_len);
            sprintf(str + (u ? strlen(str) : 0), "%s\"%s:%s\"", u ? ", " : "", base->module->name, base->name);
        }

        /* no match */
        if (u == 1) {
            ret = ly_err_new(err, LY_EVALID, LYVE_DATA, NULL, NULL,
                    "Invalid identityref \"%.*s\" value - identity not derived from the base %s.",
                    (int)value_len, value, str);
        } else {
            ret = ly_err_new(err, LY_EVALID, LYVE_DATA, NULL, NULL,
                    "Invalid identityref \"%.*s\" value - identity not derived from all the bases %s.",
                    (int)value_len, value, str);
        }
        free(str);
        return ret;
    }

    return LY_SUCCESS;
}

/**
 * @brief Check if @p ident is not disabled.
 *
 * Identity is disabled if it is located in an unimplemented model or
 * it can be disabled by if-feature. Calling this function may invoke
 * the implementation of another module.
 *
 * @param[in] ident Derived identity to which identityref points.
 * @param[in] value Value of identityref.
 * @param[in] value_len Length (number of bytes) of the given @p value.
 * @param[in] options [Type plugin store options](@ref plugintypestoreopts).
 * @param[in,out] unres Global unres structure for newly implemented modules.
 * @param[out] err Error information on error.
 * @return LY_ERR value.
 */
static LY_ERR
identityref_check_ident(const struct lysc_ident *ident, const char *value,
        size_t value_len, uint32_t options, struct lys_glob_unres *unres, struct ly_err_item **err)
{
    LY_ERR ret = LY_SUCCESS;

    if (!ident->module->implemented) {
        if (options & LYPLG_TYPE_STORE_IMPLEMENT) {
            ret = lyplg_type_make_implemented(ident->module, NULL, unres);
        } else {
            ret = ly_err_new(err, LY_EVALID, LYVE_DATA, NULL, NULL,
                    "Invalid identityref \"%.*s\" value - identity found in non-implemented module \"%s\".",
                    (int)value_len, (char *)value, ident->module->name);
        }
    } else if (lys_identity_iffeature_value(ident) == LY_ENOT) {
        ret = ly_err_new(err, LY_EVALID, LYVE_DATA, NULL, NULL,
                "Invalid identityref \"%.*s\" value - identity is disabled by if-feature.",
                (int)value_len, value);
    }

    return ret;
}

LIBYANG_API_DEF LY_ERR
lyplg_type_store_identityref(const struct ly_ctx *ctx, const struct lysc_type *type, const void *value, size_t value_len,
        uint32_t options, LY_VALUE_FORMAT format, void *prefix_data, uint32_t hints, const struct lysc_node *ctx_node,
        struct lyd_value *storage, struct lys_glob_unres *unres, struct ly_err_item **err)
{
    LY_ERR ret = LY_SUCCESS;
    struct lysc_type_identityref *type_ident = (struct lysc_type_identityref *)type;
    char *canon;
    struct lysc_ident *ident = NULL;

    /* init storage */
    memset(storage, 0, sizeof *storage);
    storage->realtype = type;

    /* check hints */
    ret = lyplg_type_check_hints(hints, value, value_len, type->basetype, NULL, err);
    LY_CHECK_GOTO(ret, cleanup);

    /* find a matching identity */
    ret = lyplg_type_identity_find(value, value_len, format, prefix_data, ctx, ctx_node, &ident, err);
    LY_CHECK_GOTO(ret, cleanup);

    /* check if the identity is enabled */
    ret = identityref_check_ident(ident, value, value_len, options, unres, err);
    LY_CHECK_GOTO(ret, cleanup);

    /* check that the identity is derived form all the bases */
    ret = identityref_check_base(ident, type_ident, value, value_len, err);
    LY_CHECK_GOTO(ret, cleanup);

    if (ctx_node) {
        /* check status */
        ret = lyplg_type_check_status(ctx_node, ident->flags, format, prefix_data, ident->name, err);
        LY_CHECK_GOTO(ret, cleanup);
    }

    /* store value */
    storage->ident = ident;

    /* store canonical value */
    if (format == LY_VALUE_CANON) {
        if (options & LYPLG_TYPE_STORE_DYNAMIC) {
            ret = lydict_insert_zc(ctx, (char *)value, &storage->_canonical);
            options &= ~LYPLG_TYPE_STORE_DYNAMIC;
            LY_CHECK_GOTO(ret, cleanup);
        } else {
            ret = lydict_insert(ctx, value, value_len, &storage->_canonical);
            LY_CHECK_GOTO(ret, cleanup);
        }
    } else {
        /* JSON format with prefix is the canonical one */
        ret = identityref_ident2str(ident, LY_VALUE_JSON, NULL, &canon, NULL);
        LY_CHECK_GOTO(ret, cleanup);

        ret = lydict_insert_zc(ctx, canon, &storage->_canonical);
        LY_CHECK_GOTO(ret, cleanup);
    }

cleanup:
    if (options & LYPLG_TYPE_STORE_DYNAMIC) {
        free((void *)value);
    }

    if (ret) {
        lyplg_type_free_simple(ctx, storage);
    }
    return ret;
}

LIBYANG_API_DEF LY_ERR
lyplg_type_compare_identityref(const struct lyd_value *val1, const struct lyd_value *val2)
{
    if (val1->realtype != val2->realtype) {
        return LY_ENOT;
    }

    if (val1->ident == val2->ident) {
        return LY_SUCCESS;
    }
    return LY_ENOT;
}

LIBYANG_API_DEF const void *
lyplg_type_print_identityref(const struct ly_ctx *UNUSED(ctx), const struct lyd_value *value, LY_VALUE_FORMAT format,
        void *prefix_data, ly_bool *dynamic, size_t *value_len)
{
    char *ret;

    if ((format == LY_VALUE_CANON) || (format == LY_VALUE_JSON) || (format == LY_VALUE_LYB)) {
        if (dynamic) {
            *dynamic = 0;
        }
        if (value_len) {
            *value_len = strlen(value->_canonical);
        }
        return value->_canonical;
    }

    /* print the value in the specific format */
    if (identityref_ident2str(value->ident, format, prefix_data, &ret, value_len)) {
        return NULL;
    }
    *dynamic = 1;
    return ret;
}

/**
 * @brief Plugin information for identityref type implementation.
 *
 * Note that external plugins are supposed to use:
 *
 *   LYPLG_TYPES = {
 */
const struct lyplg_type_record plugins_identityref[] = {
    {
        .module = "",
        .revision = NULL,
        .name = LY_TYPE_IDENT_STR,

        .plugin.id = "libyang 2 - identityref, version 1",
        .plugin.store = lyplg_type_store_identityref,
        .plugin.validate = NULL,
        .plugin.compare = lyplg_type_compare_identityref,
        .plugin.sort = NULL,
        .plugin.print = lyplg_type_print_identityref,
        .plugin.duplicate = lyplg_type_dup_simple,
        .plugin.free = lyplg_type_free_simple,
        .plugin.lyb_data_len = -1,
    },
    {0}
};

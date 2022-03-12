/**
 * @file revisions.c
 * @author Radek Krejci <rkrejci@cesnet.cz>
 * @brief libyang extension plugin - Module REvision Handling (RFC TBD)
 *
 * Copyright (c) 2022 CESNET, z.s.p.o.
 *
 * This source code is licensed under BSD 3-Clause License (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 */

#include "libyang.h"
#include "plugins_exts.h"
#include "plugins_types.h"

#define REVISIONS_NAME "ietf-yang-revisions"
#define REVISIONS_REV "2021-11-04"

/**
 * @brief Compile revision-label-scheme extension instances.
 *
 * Implementation of ::lyplg_ext_compile_clb callback set as lyext_plugin::compile.
 */
static LY_ERR
revision_label_scheme_compile(struct lysc_ctx *cctx, const struct lysp_ext_instance *p_ext, struct lysc_ext_instance *c_ext)
{
    LY_ERR ret;
    struct ly_err_item *err = NULL;
    struct lysc_module *mod_c;
    struct lys_module *mod = NULL;
    struct lysc_ident *ident = NULL, *base;
    LY_ARRAY_COUNT_TYPE u;

    /* revision-label-scheme can appear only at the top level of a YANG module or submodule */
    if ((c_ext->parent_stmt != LY_STMT_MODULE) && (c_ext->parent_stmt != LY_STMT_SUBMODULE)) {
        lyplg_ext_log(c_ext, LY_LLERR, LY_EVALID, lysc_ctx_get_path(cctx),
                "Extension %s is allowed only at the top level of a YANG module or submodule, but it is placed in \"%s\" statement.",
                p_ext->name, ly_stmt2str(c_ext->parent_stmt));
        return LY_EVALID;
    }

    mod_c = (struct lysc_module *)c_ext->parent;

    /* check for duplication */
    LY_ARRAY_FOR(mod_c->exts, u) {
        if ((&mod_c->exts[u] != c_ext) && (mod_c->exts[u].def == c_ext->def)) {
            /* duplication of the same extension in a single module */
            lyplg_ext_log(c_ext, LY_LLERR, LY_EVALID, lysc_ctx_get_path(cctx), "Extension %s is instantiated multiple times.", p_ext->name);
            return LY_EVALID;
        }
    }

    /* locate base identity revision-label-scheme-base */
    mod = ly_ctx_get_module(lysc_ctx_get_ctx(cctx), REVISIONS_NAME, REVISIONS_REV);
    if (!mod) {
        return LY_EINT;
    }
    base = mod->identities;

    ret = lyplg_type_identity_find(c_ext->argument, strlen(c_ext->argument), LY_VALUE_SCHEMA, ((struct lysc_module *)c_ext->parent)->mod->parsed,
            lysc_ctx_get_ctx(cctx), NULL, &ident, &err);
    if (ret) {
        lyplg_ext_log(c_ext, LY_LLERR, LY_EVALID, lysc_ctx_get_path(cctx), err->msg);
        ly_err_free(err);
        return LY_EVALID;
    }
    ret = lyplg_type_identity_isderived(base, ident);
    if (ret) {
        lyplg_ext_log(c_ext, LY_LLERR, LY_EVALID, lysc_ctx_get_path(cctx), "Argument of the %s extension must be an identity derived from %s:%s.",
                p_ext->name, base->module->name, base->name);
        return LY_EVALID;
    }

    /* store identity identifying revision-label scheme used by the module */
    c_ext->data = ident;

    /* compile possible substatements (none expected, but cannot exclude possible extensions) */
    return lys_compile_extension_instance(cctx, p_ext, c_ext);
}

/**
 * @brief Plugin descriptions for the Metadata's annotation extension
 *
 * Note that external plugins are supposed to use:
 *
 *   LYPLG_EXTENSIONS = {
 */
const struct lyplg_ext_record plugins_revisions[] = {
    {
        .module = REVISIONS_NAME,
        .revision = REVISIONS_REV,
        .name = "revision-label-scheme",

        .plugin.id = "libyang 2 - revisions, version 1",
        .plugin.compile = &revision_label_scheme_compile,
        .plugin.sprinter = NULL,
        .plugin.free = NULL,
        .plugin.parse = NULL,
        .plugin.validate = NULL
    },
    {0}     /* terminating zeroed record */
};

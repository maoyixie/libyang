/*
 * @file test_revisions.c
 * @author: Radek Krejci <rkrejci@cesnet.cz>
 * @brief unit tests for Metadata extension (annotation) support
 *
 * Copyright (c) 2022 CESNET, z.s.p.o.
 *
 * This source code is licensed under BSD 3-Clause License (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 */
#define _UTEST_MAIN_
#include "utests.h"

#include "libyang.h"
#include "plugins_exts.h"

static void
test_yang_label_scheme(void **state)
{
    struct lys_module *mod;
    struct lysc_ext_instance *e;

    const char *data = "module a {yang-version 1.1; namespace urn:tests:extensions:revisions:a; prefix a;"
            "rev:revision-label-scheme \"testver-scheme\";"
            "import ietf-yang-revisions {prefix rev;}"
            "identity testver-scheme {base rev:revision-label-scheme-base;}"
            "}";

    UTEST_ADD_MODULE(data, LYS_IN_YANG, NULL, &mod);
    assert_int_equal(1, LY_ARRAY_COUNT(mod->identities));
    assert_int_equal(1, LY_ARRAY_COUNT(mod->compiled->exts));
    e = &mod->compiled->exts[0];
    assert_non_null(e->data);
    assert_null(e->substmts);
    assert_ptr_equal(e->data, &mod->identities[0]);

    /* invalid */
    /* missing mandatory argument */
    data = "module aa {yang-version 1.1; namespace urn:tests:extensions:revisions:aa; prefix aa;"
            "rev:revision-label-scheme;"
            "import ietf-yang-revisions {prefix rev;}"
            "}";
    assert_int_equal(LY_EVALID, lys_parse_mem(UTEST_LYCTX, data, LYS_IN_YANG, NULL));
    CHECK_LOG_CTX("Extension instance \"rev:revision-label-scheme\" misses argument \"revision-label-scheme-base\".", "/aa:{extension='rev:revision-label-scheme'}");

    /* multiple instances */
    data = "module aa {yang-version 1.1; namespace urn:tests:extensions:revisions:aa; prefix aa;"
            "rev:revision-label-scheme \"testver-scheme\";"
            "rev:revision-label-scheme \"aa:testver-scheme\";"
            "import ietf-yang-revisions {prefix rev;}"
            "identity testver-scheme {base rev:revision-label-scheme-base;}"
            "}";
    assert_int_equal(LY_EVALID, lys_parse_mem(UTEST_LYCTX, data, LYS_IN_YANG, NULL));
    CHECK_LOG_CTX("Extension plugin \"libyang 2 - revisions, version 1\": Extension rev:revision-label-scheme is instantiated multiple times.",
            "/aa:{extension='rev:revision-label-scheme'}/aa:testver-scheme");

    /* invalid place */
    data = "module aa {yang-version 1.1; namespace urn:tests:extensions:revisions:aa; prefix aa;"
            "import ietf-yang-revisions {prefix rev;}"
            "identity testver-scheme {base rev:revision-label-scheme-base; rev:revision-label-scheme \"aa:testver-scheme\";}"
            "}";
    assert_int_equal(LY_EVALID, lys_parse_mem(UTEST_LYCTX, data, LYS_IN_YANG, NULL));
    CHECK_LOG_CTX("Extension plugin \"libyang 2 - revisions, version 1\": "
            "Extension rev:revision-label-scheme is allowed only at the top level of a YANG module or submodule, but it is placed in \"identity\" statement.",
            "/aa:{identity='testver-scheme'}/{extension='rev:revision-label-scheme'}/aa:testver-scheme");

    /* invalid scheme */
    data = "module aa {yang-version 1.1; namespace urn:tests:extensions:revisions:aa; prefix aa;"
            "rev:revision-label-scheme \"yangver:yang-semver\";"
            "import ietf-yang-revisions {prefix rev;}"
            "identity testver-scheme {base rev:revision-label-scheme-base;}"
            "}";
    assert_int_equal(LY_EVALID, lys_parse_mem(UTEST_LYCTX, data, LYS_IN_YANG, NULL));
    CHECK_LOG_CTX("Extension plugin \"libyang 2 - revisions, version 1\": "
            "Invalid identityref \"yangver:yang-semver\" value - unable to map prefix to YANG schema.",
            "/aa:{extension='rev:revision-label-scheme'}/yangver:yang-semver");

    data = "module aa {yang-version 1.1; namespace urn:tests:extensions:revisions:aa; prefix aa;"
            "rev:revision-label-scheme \"yang-semver\";"
            "import ietf-yang-revisions {prefix rev;}"
            "identity testver-scheme {base rev:revision-label-scheme-base;}"
            "}";
    assert_int_equal(LY_EVALID, lys_parse_mem(UTEST_LYCTX, data, LYS_IN_YANG, NULL));
    CHECK_LOG_CTX("Extension plugin \"libyang 2 - revisions, version 1\": "
            "Invalid identityref \"yang-semver\" value - identity not found in module \"aa\".",
            "/aa:{extension='rev:revision-label-scheme'}/yang-semver");

    data = "module aa {yang-version 1.1; namespace urn:tests:extensions:revisions:aa; prefix aa;"
            "rev:revision-label-scheme \"testver-scheme\";"
            "import ietf-yang-revisions {prefix rev;}"
            "identity testver-scheme;"
            "}";
    assert_int_equal(LY_EVALID, lys_parse_mem(UTEST_LYCTX, data, LYS_IN_YANG, NULL));
    CHECK_LOG_CTX("Extension plugin \"libyang 2 - revisions, version 1\": "
            "Argument of the rev:revision-label-scheme extension must be an identity derived from ietf-yang-revisions:revision-label-scheme-base.",
            "/aa:{extension='rev:revision-label-scheme'}/testver-scheme");
}

static void
test_yin_label_scheme(void **state)
{
    struct lys_module *mod;
    struct lysc_ext_instance *e;
    const char *data;

    data = "<module xmlns=\"urn:ietf:params:xml:ns:yang:yin:1\" xmlns:rev=\"urn:ietf:params:xml:ns:yang:ietf-yang-revisions\" name=\"a\">\n"
            "<yang-version value=\"1.1\"/><namespace uri=\"urn:tests:extensions:revisions:a\"/><prefix value=\"a\"/>\n"
            "<rev:revision-label-scheme revision-label-scheme-base=\"testver-scheme\"/>\n"
            "<import module=\"ietf-yang-revisions\"><prefix value=\"rev\"/></import>\n"
            "<identity name=\"testver-scheme\"><base name=\"rev:revision-label-scheme-base\"/></identity>"
            "</module>";

    UTEST_ADD_MODULE(data, LYS_IN_YIN, NULL, &mod);
    assert_int_equal(1, LY_ARRAY_COUNT(mod->identities));
    assert_int_equal(1, LY_ARRAY_COUNT(mod->compiled->exts));
    e = &mod->compiled->exts[0];
    assert_non_null(e->data);
    assert_null(e->substmts);
    assert_ptr_equal(e->data, &mod->identities[0]);

}

int
main(void)
{
    const struct CMUnitTest tests[] = {
        UTEST(test_yang_label_scheme),
        UTEST(test_yin_label_scheme),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

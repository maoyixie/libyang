#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "libyang.h"

extern int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    if (!Size)
    {
        return 0;
    }

    struct ly_ctx *ctx;
    struct lyd_node *tree;
    LYD_FORMAT format;
    uint32_t parse_options = 0;
    uint32_t validate_options = 0;

    // Initialize the context
    if (ly_ctx_new(NULL, 0, &ctx) != LY_SUCCESS)
    {
        return 0;
    }

    // Choose the LYD_FORMAT based on the first byte of the input data
    switch (Data[0] % 3)
    {
    case 0:
        format = LYD_XML;
        break;
    case 1:
        format = LYD_JSON;
        break;
    case 2:
        format = LYD_LYB;
        break;
    default:
        format = LYD_UNKNOWN;
        break;
    }

    // Fuzz the lyd_parse_data_mem function
    lyd_parse_data_mem(ctx, (const char *)(Data + 1), format, parse_options, validate_options, &tree);

    // Clean up
    lyd_free_tree(tree);
    ly_ctx_destroy(ctx);

    return 0;
}
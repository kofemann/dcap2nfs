#include "nfs4_prot.h"
#include "utf8_utils.h"

int compound_init(COMPOUND4args *args, COMPOUND4res *res, nfs_argop4 *op_args,
        uint32_t op_count, const char *tag) {
    args->minorversion = 1;
    args->argarray.argarray_val = op_args;
    args->argarray.argarray_len = op_count;
    char_to_utf8str_cs(tag, &args->tag);

    memset(res, 0, sizeof(COMPOUND4res));
    return 0;
}
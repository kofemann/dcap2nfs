#include <string.h>


#include "nfs4_prot.h"

void
char_to_utf8str_cs(const char *from, utf8str_cs* to)
{
    to->utf8string_val = (char *)from;
    to->utf8string_len = strlen(from);
}

#include "unique.h"

#include <stdio.h>

#include "xalloc.h"

char *unique_string_literal_name(void)
{
    static int str_id;
    char       name[32];
    snprintf(name, sizeof name, "_str%d", str_id++);
    return xstrdup(name);
}

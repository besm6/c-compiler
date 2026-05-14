#include "unique.h"

#include <stdio.h>

#include "xalloc.h"

static int str_id;

char *unique_string_literal_name(void)
{
    char name[32];
    snprintf(name, sizeof name, "_str%d", str_id++);
    return xstrdup(name);
}

void unique_string_literal_name_reset(void)
{
    str_id = 0;
}

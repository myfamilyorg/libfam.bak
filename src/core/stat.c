#include <sys/stat.h>
#include <types.h>

uint64_t stat_get_size(struct stat *s) { return s->st_size; }

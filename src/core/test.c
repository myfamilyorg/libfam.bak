#include <misc.h>
#include <test.h>

Test(core1) { ASSERT(1, "1"); }

Test(misc1) { ASSERT_EQ(strlen("abc"), 3, "strlen"); }

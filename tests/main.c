/*
 * test runner. tests in tests/*.c register themselves with UTEST(),
 * so they don't need listing here.
 *
 * utest.h is vendored from https://github.com/sheredom/utest.h
 * at commit 706f3574fec4915e78d9234870cfcdf22f7af5d9.
 */
#include "utest.h"

UTEST(runner, sanity) {
  ASSERT_EQ(2, 1 + 1);
}

UTEST_MAIN();

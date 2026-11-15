/*
 * functions referenced by the dependencies of the web build that the
 * single-threaded Emscripten C library does not provide
 */
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>

/* used by fluidsynth when it raises the priority of its (unused) threads */
int pthread_setschedparam(pthread_t thread, int policy, const struct sched_param *param) {
  return 0;
}

/* fluidsynth pins its sample data in memory; there is nothing to pin on the
   web, and the C library's version only logs "unsupported syscall" */
int mlock(const void *addr, size_t len) {
  return 0;
}

int munlock(const void *addr, size_t len) {
  return 0;
}

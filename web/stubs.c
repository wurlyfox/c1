/*
 * functions referenced by the dependencies of the web build that the
 * single-threaded Emscripten C library does not provide
 */
#include <pthread.h>
#include <sched.h>

/* used by fluidsynth when it raises the priority of its (unused) threads */
int pthread_setschedparam(pthread_t thread, int policy, const struct sched_param *param) {
  return 0;
}

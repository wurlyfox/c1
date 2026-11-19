#include "time.h"

ticks_t ticks_base = 0;

#ifdef _WIN32
#include <windows.h>
static inline ticks_t GetTicks() {
  LARGE_INTEGER _freq, _count;
  int64_t freq, count;

  QueryPerformanceFrequency(&_freq);
  QueryPerformanceCounter(&_count);
  freq = _freq.QuadPart;
  count = _count.QuadPart;
#ifdef CFLAGS_EXTENSIONS
  return ((double)count / (double)freq) * (1000000.0 * 34.0 / 32768.0);
#else
  return ((1000000*(count*34)) / 0x8000) / freq;
#endif
}
#elif __linux__
#include <time.h>
static inline ticks_t GetTicks() {
  struct timespec tp;

  clock_gettime(CLOCK_MONOTONIC_RAW, &tp);
#ifdef CFLAGS_EXTENSIONS
  return ((double)tp.tv_sec * 1000000000.0 + (double)tp.tv_nsec) / 963765.0;
#else
  return ((((int64_t)tp.tv_sec * 1000000000) + tp.tv_nsec) / 963765);
#endif
}
#endif

void SetTicksElapsed(ticks_t ticks) {
  ticks_base = GetTicks() - ticks;
}

ticks_t GetTicksElapsed() {
  ticks_t ticks;

  ticks = GetTicks() - ticks_base;
  return ticks;
}

ticks_t GetTicksMonotonic() {
  return GetTicks();
}
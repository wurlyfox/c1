#include "common.h"

int32_t ticks_base = 0;

#ifdef _WIN32
#include <windows.h>
inline int32_t GetTicks() {
  LARGE_INTEGER _freq, _count;
  int64_t freq, count;
  int32_t ticks;

  QueryPerformanceFrequency(&_freq);
  QueryPerformanceCounter(&_count);
  freq = _freq.QuadPart;
  count = _count.QuadPart;

  ticks = ((1000000*(count*34)) / 0x8000) / freq;
  return ticks;
}
#elif __linux__
#include <time.h>
static inline int32_t GetTicks() {
  struct timespec tp;
  int32_t ticks;

  clock_gettime(CLOCK_MONOTONIC_RAW, &tp);
  ticks = ((((int64_t)tp.tv_sec*1000000000)+tp.tv_nsec) / 963765);
  //printf("%i ticks elapsed\n", ticks);
  return ticks;
}
#endif

void SetTicksElapsed(int32_t ticks) {
  ticks_base = GetTicks() - ticks;
}

int GetTicksElapsed() {
  int32_t ticks;

  ticks = GetTicks() - ticks_base;
  return ticks;
}

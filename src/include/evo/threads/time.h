#ifndef EVO_TIME_EMULATION_H_DEFINED
#define EVO_TIME_EMULATION_H_DEFINED 1

#pragma once

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <time.h>

/*---------------------------- macros ---------------------------*/

#ifndef TIME_UTC
# define TIME_UTC 1
#endif

/*---------------------------- types ----------------------------*/

/*
 * On MINGW `struct timespec` present but `timespec_get` may not present;
 * On MSVC `struct timespec` and `timespec_get` present at the same time;
 * So detecting `HAVE_STRUCT_TIMESPEC` in meson script dynamically.
 */
#ifndef HAVE_STRUCT_TIMESPEC
struct timespec {
  time_t tv_sec;  // Seconds - >= 0
  long   tv_nsec; // Nanoseconds - [0, 999999999]
};
#endif

/*-------------------------- functions --------------------------*/

#ifndef HAVE_TIMESPEC_GET
/*-------------------- 7.25.7 Time functions --------------------*/
// 7.25.6.1
int
timespec_get(struct timespec *ts, int base);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* EVO_TIME_EMULATION_H_DEFINED */

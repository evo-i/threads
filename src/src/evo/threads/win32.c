#include <assert.h>
#include <limits.h>
#include <errno.h>
#include <process.h>
#include <stdlib.h>

#include <evo/threads/threads.h>

#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>

/*
Configuration macro:

  EMULATED_THREADS_USE_NATIVE_CALL_ONCE
    Use native WindowsAPI one-time initialization function.
    (requires WinVista or later)
    Otherwise emulate by mtx_trylock() + *busy loop* for WinXP.

  EMULATED_THREADS_TSS_DTOR_SLOTNUM
    Max registerable TSS dtor number.
*/

#if _WIN32_WINNT >= 0x0600
// Prefer native WindowsAPI on newer environment.
# if !defined(__MINGW32__)
#   define EMULATED_THREADS_USE_NATIVE_CALL_ONCE
# endif
#endif
# define EMULATED_THREADS_TSS_DTOR_SLOTNUM 64  // see TLS_MINIMUM_AVAILABLE

// check configuration
#if defined(EMULATED_THREADS_USE_NATIVE_CALL_ONCE) && (_WIN32_WINNT < 0x0600)
# error EMULATED_THREADS_USE_NATIVE_CALL_ONCE requires _WIN32_WINNT>=0x0600
#endif


static_assert(sizeof(cnd_t) == sizeof(CONDITION_VARIABLE), "The size of cnd_t must equal to CONDITION_VARIABLE");
static_assert(sizeof(thrd_t) == sizeof(HANDLE), "The size of thrd_t must equal to HANDLE");
static_assert(sizeof(tss_t) == sizeof(DWORD), "The size of tss_t must equal to DWORD");
static_assert(sizeof(mtx_t) == sizeof(CRITICAL_SECTION), "The size of mtx_t must equal to CRITICAL_SECTION");
static_assert(sizeof(once_flag) == sizeof(INIT_ONCE), "The size of once_flag must equal to INIT_ONCE");

/*
Implementation limits:
  - Conditionally emulation for "Initialization functions"
    (see EMULATED_THREADS_USE_NATIVE_CALL_ONCE macro)
  - Emulated `mtx_timelock()' with mtx_trylock() + *busy loop*
*/
static void impl_tss_dtor_invoke(void);  // forward decl.

struct impl_thrd_param {
  thrd_start_t func;
  void *arg;
};

static unsigned __stdcall impl_thrd_routine(void *p) {
  struct impl_thrd_param pack;
  int code;
  memcpy(&pack, p, sizeof(struct impl_thrd_param));
  free(p);
  code = pack.func(pack.arg);
  impl_tss_dtor_invoke();
  return (unsigned)code;
}

static time_t impl_timespec2msec(const struct timespec *ts) {
  return (ts->tv_sec * 1000U) + (ts->tv_nsec / 1000000L);
}

static DWORD impl_abs2relmsec(const struct timespec *abs_time) {
  const time_t abs_ms = impl_timespec2msec(abs_time);
  struct timespec now;
  timespec_get(&now, TIME_UTC);
  const time_t now_ms = impl_timespec2msec(&now);
  const DWORD rel_ms = (abs_ms > now_ms) ? (DWORD)(abs_ms - now_ms) : 0;
  return rel_ms;
}

#ifdef EMULATED_THREADS_USE_NATIVE_CALL_ONCE
struct impl_call_once_param { void (*func)(void); };
static BOOL CALLBACK impl_call_once_callback(PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context) {
  struct impl_call_once_param *param = (struct impl_call_once_param*)Parameter;
  (param->func)();
  ((void)InitOnce); ((void)Context);  // suppress warning
  return TRUE;
}
#endif  // ifdef EMULATED_THREADS_USE_NATIVE_CALL_ONCE

static struct impl_tss_dtor_entry {
  tss_t key;
  tss_dtor_t dtor;
} impl_tss_dtor_tbl[EMULATED_THREADS_TSS_DTOR_SLOTNUM];

static int impl_tss_dtor_register(tss_t key, tss_dtor_t dtor) {
  int i;
  for (i = 0; i < EMULATED_THREADS_TSS_DTOR_SLOTNUM; i++) {
    if (!impl_tss_dtor_tbl[i].dtor) {
      break;
    }
  }
  if (i == EMULATED_THREADS_TSS_DTOR_SLOTNUM) {
    return 1;
  }
  impl_tss_dtor_tbl[i].key = key;
  impl_tss_dtor_tbl[i].dtor = dtor;
  return 0;
}

static void impl_tss_dtor_invoke(void) {
  int i;
  for (i = 0; i < EMULATED_THREADS_TSS_DTOR_SLOTNUM; i++) {
    if (impl_tss_dtor_tbl[i].dtor) {
      void* val = tss_get(impl_tss_dtor_tbl[i].key);
      if (val) {
        (impl_tss_dtor_tbl[i].dtor)(val);
      }
    }
  }
}


/*--------------- 7.25.2 Initialization functions ---------------*/
// 7.25.2.1
void
call_once(once_flag *flag, void (*func)(void)) {
  assert(flag && func);
#ifdef EMULATED_THREADS_USE_NATIVE_CALL_ONCE
  {
    struct impl_call_once_param param;
    param.func = func;
    InitOnceExecuteOnce((PINIT_ONCE)flag, impl_call_once_callback, (PVOID)&param, NULL);
  }
#else
  if (InterlockedCompareExchangePointer((PVOID volatile *)&flag->status, (PVOID)1, (PVOID)0) == 0) {
    (func)();
    InterlockedExchangePointer((PVOID volatile *)&flag->status, (PVOID)2);
  } else {
    while (flag->status == 1) {
      // busy loop!
      thrd_yield();
    }
  }
#endif
}


/*------------- 7.25.3 Condition variable functions -------------*/
// 7.25.3.1
int
cnd_broadcast(cnd_t *cond) {
  assert(cond != NULL);
  WakeAllConditionVariable((PCONDITION_VARIABLE)cond);
  return thrd_success;
}

// 7.25.3.2
void
cnd_destroy(cnd_t *cond) {
  (void)cond;
  assert(cond != NULL);
  // do nothing
}

// 7.25.3.3
int
cnd_init(cnd_t *cond) {
  assert(cond != NULL);
  InitializeConditionVariable((PCONDITION_VARIABLE)cond);
  return thrd_success;
}

// 7.25.3.4
int
cnd_signal(cnd_t *cond) {
  assert(cond != NULL);
  WakeConditionVariable((PCONDITION_VARIABLE)cond);
  return thrd_success;
}

// 7.25.3.5
int
cnd_timedwait(cnd_t *cond, mtx_t *mtx, const struct timespec *abs_time) {
  assert(cond != NULL);
  assert(mtx != NULL);
  assert(abs_time != NULL);
  const DWORD timeout = impl_abs2relmsec(abs_time);
  if (SleepConditionVariableCS((PCONDITION_VARIABLE)cond, (PCRITICAL_SECTION)mtx, timeout)) {
    return thrd_success;
  }
  return
    (GetLastError() == ERROR_TIMEOUT)
      ? thrd_timedout
      : thrd_error;
}

// 7.25.3.6
int
cnd_wait(cnd_t *cond, mtx_t *mtx) {
  assert(cond != NULL);
  assert(mtx != NULL);
  SleepConditionVariableCS((PCONDITION_VARIABLE)cond, (PCRITICAL_SECTION)mtx, INFINITE);
  return thrd_success;
}


/*-------------------- 7.25.4 Mutex functions --------------------*/
// 7.25.4.1
void
mtx_destroy(mtx_t *mtx) {
  assert(mtx);
  DeleteCriticalSection((PCRITICAL_SECTION)mtx);
}

// 7.25.4.2
int
mtx_init(mtx_t *mtx, int type) {
  assert(mtx != NULL);
  if (type != mtx_plain
      && type != mtx_timed
      && type != mtx_try
      && type != (mtx_plain|mtx_recursive)
      && type != (mtx_timed|mtx_recursive)
      && type != (mtx_try|mtx_recursive)) {
    return thrd_error;
  }
  InitializeCriticalSection((PCRITICAL_SECTION)mtx);
  return thrd_success;
}

// 7.25.4.3
int
mtx_lock(mtx_t *mtx) {
  assert(mtx != NULL);
  EnterCriticalSection((PCRITICAL_SECTION)mtx);
  return thrd_success;
}

// 7.25.4.4
int
mtx_timedlock(mtx_t *mtx, const struct timespec *ts) {
  assert(mtx != NULL);
  assert(ts != NULL);
  while (mtx_trylock(mtx) != thrd_success) {
    if (impl_abs2relmsec(ts) == 0) {
      return thrd_timedout;
    }
    // busy loop!
    thrd_yield();
  }
  return thrd_success;
}

// 7.25.4.5
int
mtx_trylock(mtx_t *mtx) {
  assert(mtx != NULL);
  return
    TryEnterCriticalSection((PCRITICAL_SECTION)mtx)
      ? thrd_success
      : thrd_busy;
}

// 7.25.4.6
int
mtx_unlock(mtx_t *mtx) {
  assert(mtx != NULL);
  LeaveCriticalSection((PCRITICAL_SECTION)mtx);
  return thrd_success;
}


/*------------------- 7.25.5 Thread functions -------------------*/
// 7.25.5.1
int
thrd_create(thrd_t *thr, thrd_start_t func, void *arg) {
  struct impl_thrd_param *pack;
  uintptr_t handle;
  assert(thr != NULL);
  pack = (struct impl_thrd_param *)malloc(sizeof(struct impl_thrd_param));
  if (!pack) return thrd_nomem;
  pack->func = func;
  pack->arg = arg;
  handle = _beginthreadex(NULL, 0, impl_thrd_routine, pack, 0, NULL);
  if (handle == 0) {
    free(pack);
    if (errno == EAGAIN || errno == EACCES) {
      return thrd_nomem;
    }
    return thrd_error;
  }
  *thr = (thrd_t)handle;
  return thrd_success;
}

#if 0
// 7.25.5.2
static inline thrd_t
thrd_current(void)
{
    HANDLE hCurrentThread;
    BOOL bRet;

    /* GetCurrentThread() returns a pseudo-handle, which we need
     * to pass to DuplicateHandle(). Only the resulting handle can be used
     * from other threads.
     *
     * Note that neither handle can be compared to the one by thread_create.
     * Only the thread IDs - as returned by GetThreadId() and GetCurrentThreadId()
     * can be compared directly.
     *
     * Other potential solutions would be:
     * - define thrd_t as a thread Ids, but this would mean we'd need to OpenThread for many operations
     * - use malloc'ed memory for thrd_t. This would imply using TLS for current thread.
     *
     * Neither is particularly nice.
     *
     * Life would be much easier if C11 threads had different abstractions for
     * threads and thread IDs, just like C++11 threads does...
     */

    bRet = DuplicateHandle(GetCurrentProcess(), // source process (pseudo) handle
                           GetCurrentThread(), // source (pseudo) handle
                           GetCurrentProcess(), // target process
                           &hCurrentThread, // target handle
                           0,
                           FALSE,
                           DUPLICATE_SAME_ACCESS);
    assert(bRet);
    if (!bRet) {
	hCurrentThread = GetCurrentThread();
    }
    return hCurrentThread;
}
#endif

// 7.25.5.3
int
thrd_detach(thrd_t thr) {
  CloseHandle(thr);
  return thrd_success;
}

// 7.25.5.4
int
thrd_equal(thrd_t thr0, thrd_t thr1)
{
    return GetThreadId(thr0) == GetThreadId(thr1);
}

// 7.25.5.5
void
thrd_exit(int res) {
  impl_tss_dtor_invoke();
  _endthreadex((unsigned)res);
}

// 7.25.5.6
int
thrd_join(thrd_t thr, int *res) {
  DWORD w, code;
  w = WaitForSingleObject(thr, INFINITE);
  if (w != WAIT_OBJECT_0) {
    return thrd_error;
  }
  if (res) {
    if (!GetExitCodeThread(thr, &code)) {
      CloseHandle(thr);
      return thrd_error;
    }
    *res = (int)code;
  }
  CloseHandle(thr);
  return thrd_success;
}

// 7.25.5.7
int
thrd_sleep(const struct timespec *time_point, struct timespec *remaining) {
  (void)remaining;
  assert(time_point);
  assert(!remaining); /* not implemented */
  Sleep((DWORD)impl_timespec2msec(time_point));
  return 0;
}

// 7.25.5.8
void
thrd_yield(void) {
  SwitchToThread();
}


/*----------- 7.25.6 Thread-specific storage functions -----------*/
// 7.25.6.1
int
tss_create(tss_t *key, tss_dtor_t dtor)
{
  assert(key != NULL);
  *key = TlsAlloc();
  if (dtor) {
    if (impl_tss_dtor_register(*key, dtor)) {
      TlsFree(*key);
      return thrd_error;
    }
  }
  return
    (*key != 0xFFFFFFFF)
      ? thrd_success
      : thrd_error;
}

// 7.25.6.2
void
tss_delete(tss_t key) {
  TlsFree(key);
}

// 7.25.6.3
void *
tss_get(tss_t key) {
  return TlsGetValue(key);
}

// 7.25.6.4
int
tss_set(tss_t key, void *val) {
  return
    TlsSetValue(key, val)
      ? thrd_success
      : thrd_error;
}

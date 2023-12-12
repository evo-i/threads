#ifndef EVO_THREADS_EMULATION_EXPORTS_H
#define EVO_THREADS_EMULATION_EXPORTS_H

#if defined _WIN32 || defined __CYGWIN__
  #define EVO_THREADS_HELPER_DLL_IMPORT __declspec(dllimport)
  #define EVO_THREADS_HELPER_DLL_EXPORT __declspec(dllexport)
  #define EVO_THREADS_HELPER_DLL_LOCAL
#else
  #if __GNUC__ >= 4
    #define EVO_THREADS_HELPER_DLL_IMPORT __attribute__ ((visibility ("default")))
    #define EVO_THREADS_HELPER_DLL_EXPORT __attribute__ ((visibility ("default")))
    #define EVO_THREADS_HELPER_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
  #else
    #define EVO_THREADS_HELPER_DLL_IMPORT
    #define EVO_THREADS_HELPER_DLL_EXPORT
    #define EVO_THREADS_HELPER_DLL_LOCAL
  #endif
#endif

#ifdef EVO_THREADS_DLL
  #ifdef EVO_THREADS_DLL_EXPORTS
    #define EVO_THREADS_API EVO_THREADS_HELPER_DLL_EXPORT
  #else
    #define EVO_THREADS_API EVO_THREADS_HELPER_DLL_IMPORT
  #endif
  #define EVO_THREADS_LOCAL EVO_THREADS_HELPER_DLL_LOCAL
#else
  #define EVO_THREADS_API
  #define EVO_THREADS_LOCAL
#endif

#endif /* EVO_THREADS_EMULATION_EXPORTS_H */


#ifndef DABSDR_API_H
#define DABSDR_API_H

#ifdef DABSDR_BUILT_AS_STATIC
#  define DABSDR_API
#  define DABSDR_NO_EXPORT
#else
#  ifndef DABSDR_API
#    ifdef dabsdr_EXPORTS
        /* We are building this library */
#      define DABSDR_API __attribute__((visibility("default")))
#    else
        /* We are using this library */
#      define DABSDR_API __attribute__((visibility("default")))
#    endif
#  endif

#  ifndef DABSDR_NO_EXPORT
#    define DABSDR_NO_EXPORT __attribute__((visibility("hidden")))
#  endif
#endif

#ifndef DABSDR_DEPRECATED
#  define DABSDR_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef DABSDR_DEPRECATED_EXPORT
#  define DABSDR_DEPRECATED_EXPORT DABSDR_API DABSDR_DEPRECATED
#endif

#ifndef DABSDR_DEPRECATED_NO_EXPORT
#  define DABSDR_DEPRECATED_NO_EXPORT DABSDR_NO_EXPORT DABSDR_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef DABSDR_NO_DEPRECATED
#    define DABSDR_NO_DEPRECATED
#  endif
#endif

#endif /* DABSDR_API_H */

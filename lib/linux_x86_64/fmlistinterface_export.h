
#ifndef FMLISTINTERFACE_API_H
#define FMLISTINTERFACE_API_H

#ifdef FMLISTINTERFACE_STATIC_DEFINE
#  define FMLISTINTERFACE_API
#  define FMLISTINTERFACE_NO_EXPORT
#else
#  ifndef FMLISTINTERFACE_API
#    ifdef fmlistInterface_EXPORTS
        /* We are building this library */
#      define FMLISTINTERFACE_API __attribute__((visibility("default")))
#    else
        /* We are using this library */
#      define FMLISTINTERFACE_API __attribute__((visibility("default")))
#    endif
#  endif

#  ifndef FMLISTINTERFACE_NO_EXPORT
#    define FMLISTINTERFACE_NO_EXPORT __attribute__((visibility("hidden")))
#  endif
#endif

#ifndef FMLISTINTERFACE_DEPRECATED
#  define FMLISTINTERFACE_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef FMLISTINTERFACE_DEPRECATED_EXPORT
#  define FMLISTINTERFACE_DEPRECATED_EXPORT FMLISTINTERFACE_API FMLISTINTERFACE_DEPRECATED
#endif

#ifndef FMLISTINTERFACE_DEPRECATED_NO_EXPORT
#  define FMLISTINTERFACE_DEPRECATED_NO_EXPORT FMLISTINTERFACE_NO_EXPORT FMLISTINTERFACE_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef FMLISTINTERFACE_NO_DEPRECATED
#    define FMLISTINTERFACE_NO_DEPRECATED
#  endif
#endif

#endif /* FMLISTINTERFACE_API_H */

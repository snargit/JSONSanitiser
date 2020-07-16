
#ifndef JSONSANITISER_EXPORT_H
#define JSONSANITISER_EXPORT_H

#ifdef JSONSANITISER_STATIC_DEFINE
#  define JSONSANITISER_EXPORT
#  define JSONSANITISER_NO_EXPORT
#else
#  ifndef JSONSANITISER_EXPORT
#    ifdef JSONSanitiser_EXPORTS
        /* We are building this library */
#      define JSONSANITISER_EXPORT __declspec(dllexport)
#    else
        /* We are using this library */
#      define JSONSANITISER_EXPORT __declspec(dllimport)
#    endif
#  endif

#  ifndef JSONSANITISER_NO_EXPORT
#    define JSONSANITISER_NO_EXPORT 
#  endif
#endif

#ifndef JSONSANITISER_DEPRECATED
#  define JSONSANITISER_DEPRECATED __declspec(deprecated)
#endif

#ifndef JSONSANITISER_DEPRECATED_EXPORT
#  define JSONSANITISER_DEPRECATED_EXPORT JSONSANITISER_EXPORT JSONSANITISER_DEPRECATED
#endif

#ifndef JSONSANITISER_DEPRECATED_NO_EXPORT
#  define JSONSANITISER_DEPRECATED_NO_EXPORT JSONSANITISER_NO_EXPORT JSONSANITISER_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef JSONSANITISER_NO_DEPRECATED
#    define JSONSANITISER_NO_DEPRECATED
#  endif
#endif

#endif /* JSONSANITISER_EXPORT_H */

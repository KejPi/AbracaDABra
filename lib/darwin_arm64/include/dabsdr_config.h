#pragma once

#define HAVE_CMPLXF
/* #undef HAVE_IMAGINARY_I */
#define HAVE_COMPLEX_I
#define BUILD_64BIT
/* #undef BUILD_32BIT */
/* #undef BUILD_BIG_ENDIAN */

// optional features
// SSE optimization used for OFDM and Viterbi
/* #undef HAVE_SSE */
/* #undef HAVE_SSE2 */

// KISS FFT implementation of fft
#define USE_KISS_FFT 1

// FFTS implementation of fft
/* #undef USE_FFTS */

#ifndef HAVE_CMPLXF
#if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7))
    #define CMPLXF(x,y) __builtin_complex((float)(x), (float)(y))
#elif defined(HAVE_IMAGINARY_I)
    #define CMPLXF(x,y) ((float complex)((float)(x) + _Imaginary_I * (float)(y)))
#elif defined(HAVE_COMPLEX_I)
    #define CMPLXF(x,y) ((float complex)((float)(x) + _Complex_I * (float)(y)))
#endif
#endif /* HAVE_CMPLXF */

/* #undef LIBRARY_DEBUG_LEVEL */

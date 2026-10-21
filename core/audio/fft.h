#ifndef RCAT_FFT_H
#define RCAT_FFT_H

#include <stdbool.h>

/* A small in-place complex FFT, for the voice effects. */

typedef struct
{
    float re;
    float im;
} fft_complex_t;

#define FFT_MAX_SIZE 512

/* Transform n points (a power of two up to FFT_MAX_SIZE). The inverse isn't
 * scaled, so divide by n afterwards. */
void fft(fft_complex_t *x, int n, bool inverse);

#endif

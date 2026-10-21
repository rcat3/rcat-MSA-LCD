/*
 * Iterative radix-2 FFT with a table of twiddle factors, built on first use
 * for FFT_MAX_SIZE and shared by smaller sizes.
 */
#include <math.h>

#include "audio/fft.h"

static fft_complex_t twiddles[FFT_MAX_SIZE / 2];
static bool tables_ready;

static void make_tables(void)
{
    for (int k = 0; k < FFT_MAX_SIZE / 2; k++)
    {
        double angle = -2.0 * M_PI * k / FFT_MAX_SIZE;
        twiddles[k].re = (float)cos(angle);
        twiddles[k].im = (float)sin(angle);
    }
    tables_ready = true;
}

void fft(fft_complex_t *x, int n, bool inverse)
{
    if (!tables_ready)
    {
        make_tables();
    }

    // Reorder into bit-reversed order.
    for (int i = 1, j = 0; i < n; i++)
    {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1)
        {
            j ^= bit;
        }
        j ^= bit;
        if (i < j)
        {
            fft_complex_t t = x[i];
            x[i] = x[j];
            x[j] = t;
        }
    }

    for (int len = 2; len <= n; len <<= 1)
    {
        int half = len >> 1;
        int step = FFT_MAX_SIZE / len;
        for (int i = 0; i < n; i += len)
        {
            for (int k = 0; k < half; k++)
            {
                fft_complex_t w = twiddles[k * step];
                if (inverse)
                {
                    w.im = -w.im;
                }
                fft_complex_t a = x[i + k];
                fft_complex_t b = x[i + k + half];
                fft_complex_t t = { b.re * w.re - b.im * w.im, b.re * w.im + b.im * w.re };
                x[i + k].re = a.re + t.re;
                x[i + k].im = a.im + t.im;
                x[i + k + half].re = a.re - t.re;
                x[i + k + half].im = a.im - t.im;
            }
        }
    }
}

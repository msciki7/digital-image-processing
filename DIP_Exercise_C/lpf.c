#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <complex.h>

#define N      256
#define RADIUS 20

/* ---------- FFT (Cooley-Tukey, iterative) ---------- */
static void fft1d(double complex *x, int n, int inv) {
    /* bit-reversal */
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { double complex t = x[i]; x[i] = x[j]; x[j] = t; }
    }
    /* butterfly */
    for (int len = 2; len <= n; len <<= 1) {
        double ang = 2.0 * M_PI / len * inv;
        double complex wlen = cos(ang) + I*sin(ang);
        for (int i = 0; i < n; i += len) {
            double complex w = 1.0;
            for (int j = 0; j < len/2; j++) {
                double complex u = x[i+j];
                double complex v = x[i+j+len/2] * w;
                x[i+j]       = u + v;
                x[i+j+len/2] = u - v;
                w *= wlen;
            }
        }
    }
    if (inv == -1)
        for (int i = 0; i < n; i++) x[i] /= n;
}

static void fft2d(double complex **data, int n, int inv) {
    double complex *tmp = malloc(n * sizeof(double complex));
    /* row FFT */
    for (int i = 0; i < n; i++) {
        memcpy(tmp, data[i], n * sizeof(double complex));
        fft1d(tmp, n, inv);
        memcpy(data[i], tmp, n * sizeof(double complex));
    }
    /* col FFT */
    for (int j = 0; j < n; j++) {
        for (int i = 0; i < n; i++) tmp[i] = data[i][j];
        fft1d(tmp, n, inv);
        for (int i = 0; i < n; i++) data[i][j] = tmp[i];
    }
    free(tmp);
}

/* ---------- BMP write (8-bit grayscale) ---------- */
static void write_le16(FILE *fp, uint16_t v) {
    fputc(v&0xFF,fp); fputc((v>>8)&0xFF,fp);
}
static void write_le32(FILE *fp, uint32_t v) {
    fputc(v&0xFF,fp); fputc((v>>8)&0xFF,fp);
    fputc((v>>16)&0xFF,fp); fputc((v>>24)&0xFF,fp);
}

static void write_bmp(const char *name, unsigned char **img, int w, int h) {
    FILE *fp = fopen(name, "wb");
    int row_pad  = (w + 3) & ~3;
    int pix_data = row_pad * h;
    int offset   = 14 + 40 + 256*4;
    int filesize = offset + pix_data;

    fputc('B',fp); fputc('M',fp);
    write_le32(fp, filesize);
    write_le32(fp, 0);
    write_le32(fp, offset);
    write_le32(fp, 40);
    write_le32(fp, w);
    write_le32(fp, h);
    write_le16(fp, 1);
    write_le16(fp, 8);
    write_le32(fp, 0);
    write_le32(fp, pix_data);
    write_le32(fp, 2835);
    write_le32(fp, 2835);
    write_le32(fp, 0);
    write_le32(fp, 0);

    for (int i = 0; i < 256; i++) {
        fputc(i,fp); fputc(i,fp); fputc(i,fp); fputc(0,fp);
    }

    unsigned char *row = calloc(row_pad, 1);
    for (int i = h-1; i >= 0; i--) {
        memcpy(row, img[i], w);
        fwrite(row, 1, row_pad, fp);
    }
    free(row);
    fclose(fp);
}

static int clamp(int v) { return v<0?0:(v>255?255:v); }

/* ---------- main ---------- */
int main(void) {
    /* read 512x512 lena, use top-left 256x256 */
    unsigned char raw[512][512];
    FILE *fp = fopen("lena.img", "rb");
    if (!fp) { fprintf(stderr, "Cannot open lena.img\n"); return 1; }
    for (int i = 0; i < 512; i++)
        fread(raw[i], 1, 512, fp);
    fclose(fp);

    /* downsample 512x512 -> 256x256 by averaging 2x2 blocks */
    double complex **F = malloc(N * sizeof(double complex *));
    for (int i = 0; i < N; i++) {
        F[i] = malloc(N * sizeof(double complex));
        for (int j = 0; j < N; j++) {
            double avg = ((double)raw[2*i][2*j] + raw[2*i][2*j+1]
                        + raw[2*i+1][2*j] + raw[2*i+1][2*j+1]) / 4.0;
            /* multiply by (-1)^(i+j) to center DC */
            double sign = ((i+j) % 2 == 0) ? 1.0 : -1.0;
            F[i][j] = sign * avg + 0.0*I;
        }
    }

    /* forward 2D FFT */
    fft2d(F, N, 1);

    /* ideal LPF: keep components within radius from center (N/2, N/2) */
    int cx = N/2, cy = N/2;
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            double d = sqrt((double)((i-cx)*(i-cx) + (j-cy)*(j-cy)));
            if (d > RADIUS)
                F[i][j] = 0.0 + 0.0*I;
        }
    }

    /* inverse 2D FFT */
    fft2d(F, N, -1);

    /* build output image */
    /* de-center and collect real output values */
    double **real_out = malloc(N * sizeof(double *));
    for (int i = 0; i < N; i++) {
        real_out[i] = malloc(N * sizeof(double));
        for (int j = 0; j < N; j++) {
            double sign    = ((i+j) % 2 == 0) ? 1.0 : -1.0;
            real_out[i][j] = creal(F[i][j]) * sign;
        }
    }

    /* 수학적 IDFT 결과를 그대로 반올림 후 0~255 clipping */
    unsigned char **out = malloc(N * sizeof(unsigned char *));
    for (int i = 0; i < N; i++) {
        out[i] = malloc(N);
        for (int j = 0; j < N; j++)
            out[i][j] = (unsigned char)clamp((int)round(real_out[i][j]));
        free(real_out[i]);
    }
    free(real_out);

    write_bmp("ideal_lpf.bmp", out, N, N);
    printf("ideal_lpf.bmp saved (256x256, radius=%d).\n", RADIUS);

    for (int i = 0; i < N; i++) { free(F[i]); free(out[i]); }
    free(F); free(out);
    return 0;
}

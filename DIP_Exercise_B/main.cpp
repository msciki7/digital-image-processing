#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define N        256
#define IMG_SIZE 512
#define BLOCK    8

/* ───────────────────────── BMP writer (grayscale 8-bit) ───────────────────── */
static void write_bmp(const char *filename, unsigned char **img, int width, int height) {
    int row_size  = (width + 3) & ~3;
    int img_size  = row_size * height;
    int file_size = 54 + 1024 + img_size;
    unsigned char header[54];
    memset(header, 0, 54);

    header[0] = 'B'; header[1] = 'M';
    header[2] = file_size & 0xFF;
    header[3] = (file_size >> 8) & 0xFF;
    header[4] = (file_size >> 16) & 0xFF;
    header[5] = (file_size >> 24) & 0xFF;
    header[10] = (54 + 1024) & 0xFF;
    header[11] = ((54 + 1024) >> 8) & 0xFF;
    header[14] = 40;
    header[18] = width  & 0xFF; header[19] = (width  >> 8) & 0xFF;
    header[22] = height & 0xFF; header[23] = (height >> 8) & 0xFF;
    header[26] = 1; header[28] = 8;
    header[34] = img_size & 0xFF;
    header[35] = (img_size >> 8) & 0xFF;
    header[36] = (img_size >> 16) & 0xFF;
    header[37] = (img_size >> 24) & 0xFF;

    FILE *fp = fopen(filename, "wb");
    fwrite(header, 1, 54, fp);
    for (int i = 0; i < 256; i++) {
        unsigned char c[4] = {(unsigned char)i, (unsigned char)i, (unsigned char)i, 0};
        fwrite(c, 1, 4, fp);
    }
    unsigned char *row = (unsigned char*)calloc(row_size, 1);
    for (int i = height - 1; i >= 0; i--) {
        memcpy(row, img[i], width);
        fwrite(row, 1, row_size, fp);
    }
    free(row);
    fclose(fp);
}

/* ─────────────────────── 1. 256×256 2D DFT & IDFT ────────────────────────── */
static void run_dft_idft(unsigned char **input512) {
    int i, j, u, v;

    double        **img      = (double**)       malloc(sizeof(double*)        * N);
    double        **Re       = (double**)       malloc(sizeof(double*)        * N);
    double        **Im       = (double**)       malloc(sizeof(double*)        * N);
    double        **out      = (double**)       malloc(sizeof(double*)        * N);
    unsigned char **dft_bmp  = (unsigned char**)malloc(sizeof(unsigned char*) * N);
    unsigned char **idft_bmp = (unsigned char**)malloc(sizeof(unsigned char*) * N);
    for (i = 0; i < N; i++) {
        img[i]      = (double*)      malloc(sizeof(double) * N);
        Re[i]       = (double*)      malloc(sizeof(double) * N);
        Im[i]       = (double*)      malloc(sizeof(double) * N);
        out[i]      = (double*)      malloc(sizeof(double) * N);
        dft_bmp[i]  = (unsigned char*)malloc(N);
        idft_bmp[i] = (unsigned char*)malloc(N);
    }

    /* Step 1: subsample 512->256, multiply by (-1)^(m+n) */
    for (i = 0; i < N; i++)
        for (j = 0; j < N; j++) {
            double sign = ((i + j) % 2 == 0) ? 1.0 : -1.0;
            img[i][j] = (double)input512[2*i][2*j] * sign;
        }

    /* Step 2: 2D DFT */
    printf("[DFT] Computing DFT...\n");
    for (u = 0; u < N; u++) {
        if (u % 32 == 0) printf("  DFT row %d/%d\n", u, N);
        for (v = 0; v < N; v++) {
            double re = 0.0, im = 0.0;
            for (i = 0; i < N; i++)
                for (j = 0; j < N; j++) {
                    double angle = -2.0 * M_PI * (u*i + v*j) / N;
                    re += img[i][j] * cos(angle);
                    im += img[i][j] * sin(angle);
                }
            Re[u][v] = re / ((double)N * N);
            Im[u][v] = im / ((double)N * N);
        }
    }

    /* Magnitude -> dft.bmp */
    double DCvalue = sqrt(Re[128][128]*Re[128][128] + Im[128][128]*Im[128][128]);
    printf("[DFT] DCvalue = %f\n", DCvalue);
    for (u = 0; u < N; u++)
        for (v = 0; v < N; v++) {
            double mag = sqrt(Re[u][v]*Re[u][v] + Im[u][v]*Im[u][v]);
            double sc  = 255.0 * log10(mag + 1.0) / log10(DCvalue + 1.0);
            if (sc > 255.0) sc = 255.0;
            if (sc < 0.0)   sc = 0.0;
            dft_bmp[u][v] = (unsigned char)sc;
        }
    write_bmp("dft.bmp", dft_bmp, N, N);
    printf("[DFT] dft.bmp written\n");

    /* Step 4-6: IDFT -> idft.bmp */
    printf("[DFT] Computing IDFT...\n");
    for (i = 0; i < N; i++) {
        if (i % 32 == 0) printf("  IDFT row %d/%d\n", i, N);
        for (j = 0; j < N; j++) {
            double re = 0.0;
            for (u = 0; u < N; u++)
                for (v = 0; v < N; v++) {
                    double angle = 2.0 * M_PI * (u*i + v*j) / N;
                    re += Re[u][v]*cos(angle) - Im[u][v]*sin(angle);
                }
            out[i][j] = re;
        }
    }
    for (i = 0; i < N; i++)
        for (j = 0; j < N; j++) {
            double sign = ((i + j) % 2 == 0) ? 1.0 : -1.0;
            double val  = out[i][j] * sign;
            if (val > 255.0) val = 255.0;
            if (val < 0.0)   val = 0.0;
            idft_bmp[i][j] = (unsigned char)(val + 0.5);
        }
    write_bmp("idft.bmp", idft_bmp, N, N);
    printf("[DFT] idft.bmp written\n");

    for (i = 0; i < N; i++) {
        free(img[i]); free(Re[i]); free(Im[i]); free(out[i]);
        free(dft_bmp[i]); free(idft_bmp[i]);
    }
    free(img); free(Re); free(Im); free(out);
    free(dft_bmp); free(idft_bmp);
}

/* ─────────────────── 2. 8×8 Block DCT & IDCT (512×512) ───────────────────── */
static double dct_alpha(int k) {
    return (k == 0) ? (1.0 / sqrt((double)BLOCK)) : sqrt(2.0 / (double)BLOCK);
}

static void run_dct_idct(unsigned char **input) {
    int i, j, bx, by, x, y, u, v;

    double        **dct_coeff = (double**)       malloc(sizeof(double*)        * IMG_SIZE);
    double        **idct_out  = (double**)       malloc(sizeof(double*)        * IMG_SIZE);
    unsigned char **dct_bmp   = (unsigned char**)malloc(sizeof(unsigned char*) * IMG_SIZE);
    unsigned char **idct_bmp  = (unsigned char**)malloc(sizeof(unsigned char*) * IMG_SIZE);
    for (i = 0; i < IMG_SIZE; i++) {
        dct_coeff[i] = (double*)      malloc(sizeof(double) * IMG_SIZE);
        idct_out[i]  = (double*)      malloc(sizeof(double) * IMG_SIZE);
        dct_bmp[i]   = (unsigned char*)malloc(IMG_SIZE);
        idct_bmp[i]  = (unsigned char*)malloc(IMG_SIZE);
    }

    for (bx = 0; bx < IMG_SIZE; bx += BLOCK) {
        for (by = 0; by < IMG_SIZE; by += BLOCK) {
            double block[BLOCK][BLOCK];
            /* Forward DCT */
            for (u = 0; u < BLOCK; u++)
                for (v = 0; v < BLOCK; v++) {
                    double sum = 0.0;
                    for (x = 0; x < BLOCK; x++)
                        for (y = 0; y < BLOCK; y++)
                            sum += (double)input[bx+x][by+y]
                                   * cos(M_PI*(2*x+1)*u/(2.0*BLOCK))
                                   * cos(M_PI*(2*y+1)*v/(2.0*BLOCK));
                    block[u][v] = dct_alpha(u) * dct_alpha(v) * sum;
                    dct_coeff[bx+u][by+v] = block[u][v];
                }
            /* Inverse DCT */
            for (x = 0; x < BLOCK; x++)
                for (y = 0; y < BLOCK; y++) {
                    double sum = 0.0;
                    for (u = 0; u < BLOCK; u++)
                        for (v = 0; v < BLOCK; v++)
                            sum += dct_alpha(u) * dct_alpha(v) * block[u][v]
                                   * cos(M_PI*(2*x+1)*u/(2.0*BLOCK))
                                   * cos(M_PI*(2*y+1)*v/(2.0*BLOCK));
                    idct_out[bx+x][by+y] = sum;
                }
        }
    }

    /* Normalize DCT coefficients to [0,255] */
    double dct_min = dct_coeff[0][0], dct_max = dct_coeff[0][0];
    for (i = 0; i < IMG_SIZE; i++)
        for (j = 0; j < IMG_SIZE; j++) {
            if (dct_coeff[i][j] < dct_min) dct_min = dct_coeff[i][j];
            if (dct_coeff[i][j] > dct_max) dct_max = dct_coeff[i][j];
        }
    double dct_range = (dct_max - dct_min > 0.0) ? (dct_max - dct_min) : 1.0;
    for (i = 0; i < IMG_SIZE; i++)
        for (j = 0; j < IMG_SIZE; j++) {
            double val = (dct_coeff[i][j] - dct_min) / dct_range * 255.0;
            dct_bmp[i][j] = (unsigned char)(val + 0.5);
        }
    write_bmp("dct.bmp", dct_bmp, IMG_SIZE, IMG_SIZE);
    printf("[DCT] dct.bmp written\n");

    for (i = 0; i < IMG_SIZE; i++)
        for (j = 0; j < IMG_SIZE; j++) {
            double val = idct_out[i][j];
            if (val > 255.0) val = 255.0;
            if (val < 0.0)   val = 0.0;
            idct_bmp[i][j] = (unsigned char)(val + 0.5);
        }
    write_bmp("idct.bmp", idct_bmp, IMG_SIZE, IMG_SIZE);
    printf("[DCT] idct.bmp written\n");

    for (i = 0; i < IMG_SIZE; i++) {
        free(dct_coeff[i]); free(idct_out[i]);
        free(dct_bmp[i]);   free(idct_bmp[i]);
    }
    free(dct_coeff); free(idct_out);
    free(dct_bmp);   free(idct_bmp);
}

/* ─────────────────── 3. 8×8 Block DST & IDST (512×512) ───────────────────── */
static void run_dst_idst(unsigned char **input) {
    int i, j, bx, by, x, y, u, v;
    double scale = 2.0 / (BLOCK + 1.0);  /* 2/9 */

    double        **dst_coeff = (double**)       malloc(sizeof(double*)        * IMG_SIZE);
    double        **idst_out  = (double**)       malloc(sizeof(double*)        * IMG_SIZE);
    unsigned char **dst_bmp   = (unsigned char**)malloc(sizeof(unsigned char*) * IMG_SIZE);
    unsigned char **idst_bmp  = (unsigned char**)malloc(sizeof(unsigned char*) * IMG_SIZE);
    for (i = 0; i < IMG_SIZE; i++) {
        dst_coeff[i] = (double*)      malloc(sizeof(double) * IMG_SIZE);
        idst_out[i]  = (double*)      malloc(sizeof(double) * IMG_SIZE);
        dst_bmp[i]   = (unsigned char*)malloc(IMG_SIZE);
        idst_bmp[i]  = (unsigned char*)malloc(IMG_SIZE);
    }

    for (bx = 0; bx < IMG_SIZE; bx += BLOCK) {
        for (by = 0; by < IMG_SIZE; by += BLOCK) {
            double block[BLOCK][BLOCK];
            /* Forward DST */
            for (u = 0; u < BLOCK; u++)
                for (v = 0; v < BLOCK; v++) {
                    double sum = 0.0;
                    for (x = 0; x < BLOCK; x++)
                        for (y = 0; y < BLOCK; y++)
                            sum += (double)input[bx+x][by+y]
                                   * sin(M_PI*(x+1)*(u+1)/(BLOCK+1.0))
                                   * sin(M_PI*(y+1)*(v+1)/(BLOCK+1.0));
                    block[u][v] = scale * sum;
                    dst_coeff[bx+u][by+v] = block[u][v];
                }
            /* Inverse DST */
            for (x = 0; x < BLOCK; x++)
                for (y = 0; y < BLOCK; y++) {
                    double sum = 0.0;
                    for (u = 0; u < BLOCK; u++)
                        for (v = 0; v < BLOCK; v++)
                            sum += block[u][v]
                                   * sin(M_PI*(x+1)*(u+1)/(BLOCK+1.0))
                                   * sin(M_PI*(y+1)*(v+1)/(BLOCK+1.0));
                    idst_out[bx+x][by+y] = scale * sum;
                }
        }
    }

    /* Normalize DST coefficients to [0,255] */
    double dst_min = dst_coeff[0][0], dst_max = dst_coeff[0][0];
    for (i = 0; i < IMG_SIZE; i++)
        for (j = 0; j < IMG_SIZE; j++) {
            if (dst_coeff[i][j] < dst_min) dst_min = dst_coeff[i][j];
            if (dst_coeff[i][j] > dst_max) dst_max = dst_coeff[i][j];
        }
    double dst_range = (dst_max - dst_min > 0.0) ? (dst_max - dst_min) : 1.0;
    for (i = 0; i < IMG_SIZE; i++)
        for (j = 0; j < IMG_SIZE; j++) {
            double val = (dst_coeff[i][j] - dst_min) / dst_range * 255.0;
            dst_bmp[i][j] = (unsigned char)(val + 0.5);
        }
    write_bmp("dst.bmp", dst_bmp, IMG_SIZE, IMG_SIZE);
    printf("[DST] dst.bmp written\n");

    for (i = 0; i < IMG_SIZE; i++)
        for (j = 0; j < IMG_SIZE; j++) {
            double val = idst_out[i][j];
            if (val > 255.0) val = 255.0;
            if (val < 0.0)   val = 0.0;
            idst_bmp[i][j] = (unsigned char)(val + 0.5);
        }
    write_bmp("idst.bmp", idst_bmp, IMG_SIZE, IMG_SIZE);
    printf("[DST] idst.bmp written\n");

    for (i = 0; i < IMG_SIZE; i++) {
        free(dst_coeff[i]); free(idst_out[i]);
        free(dst_bmp[i]);   free(idst_bmp[i]);
    }
    free(dst_coeff); free(idst_out);
    free(dst_bmp);   free(idst_bmp);
}

/* ─────────────────────────────── main ────────────────────────────────────── */
int main() {
    int i;

    /* Load 512x512 lena.img once, share across all tasks */
    unsigned char **input512 = (unsigned char**)malloc(sizeof(unsigned char*) * IMG_SIZE);
    for (i = 0; i < IMG_SIZE; i++)
        input512[i] = (unsigned char*)malloc(IMG_SIZE);

    FILE *fp = fopen("lena.img", "rb");
    for (i = 0; i < IMG_SIZE; i++)
        fread(input512[i], 1, IMG_SIZE, fp);
    fclose(fp);

    run_dct_idct(input512);   /* fast */
    run_dst_idst(input512);   /* fast */
    run_dft_idft(input512);   /* ~6 min */

    for (i = 0; i < IMG_SIZE; i++)
        free(input512[i]);
    free(input512);

    return 0;
}

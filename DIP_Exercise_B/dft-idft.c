#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define N 256

void write_bmp(const char *filename, unsigned char **img, int width, int height) {
    int row_size = (width + 3) & ~3;
    int img_size = row_size * height;
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
    header[18] = width & 0xFF;
    header[19] = (width >> 8) & 0xFF;
    header[22] = height & 0xFF;
    header[23] = (height >> 8) & 0xFF;
    header[26] = 1;
    header[28] = 8;
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

int main() {
    FILE *fp;
    int i, j, u, v;

    /* Allocate 512x512 input */
    unsigned char **input512 = (unsigned char**)malloc(sizeof(unsigned char*) * 512);
    for (i = 0; i < 512; i++)
        input512[i] = (unsigned char*)malloc(512);

    fp = fopen("lena.img", "rb");
    for (i = 0; i < 512; i++)
        fread(input512[i], 1, 512, fp);
    fclose(fp);

    /* Allocate 256x256 arrays */
    double **img = (double**)malloc(sizeof(double*) * N);
    double **Re  = (double**)malloc(sizeof(double*) * N);
    double **Im  = (double**)malloc(sizeof(double*) * N);
    double **out = (double**)malloc(sizeof(double*) * N);
    unsigned char **dft_bmp  = (unsigned char**)malloc(sizeof(unsigned char*) * N);
    unsigned char **idft_bmp = (unsigned char**)malloc(sizeof(unsigned char*) * N);
    for (i = 0; i < N; i++) {
        img[i]      = (double*)malloc(sizeof(double) * N);
        Re[i]       = (double*)malloc(sizeof(double) * N);
        Im[i]       = (double*)malloc(sizeof(double) * N);
        out[i]      = (double*)malloc(sizeof(double) * N);
        dft_bmp[i]  = (unsigned char*)malloc(N);
        idft_bmp[i] = (unsigned char*)malloc(N);
    }

    /* Step 1: Subsample 512->256, multiply by (-1)^(m+n) to center */
    for (i = 0; i < N; i++)
        for (j = 0; j < N; j++) {
            double sign = ((i + j) % 2 == 0) ? 1.0 : -1.0;
            img[i][j] = (double)input512[2*i][2*j] * sign;
        }

    /* Step 2: 2D DFT  F(u,v) = (1/N^2) * sum_m sum_n img[m][n] * e^(-j2pi(um+vn)/N) */
    printf("Computing DFT...\n");
    for (u = 0; u < N; u++) {
        if (u % 32 == 0) printf("  DFT row %d/%d\n", u, N);
        for (v = 0; v < N; v++) {
            double re = 0.0, im = 0.0;
            for (i = 0; i < N; i++) {
                for (j = 0; j < N; j++) {
                    double angle = -2.0 * M_PI * (u * i + v * j) / N;
                    re += img[i][j] * cos(angle);
                    im += img[i][j] * sin(angle);
                }
            }
            Re[u][v] = re / ((double)N * N);
            Im[u][v] = im / ((double)N * N);
        }
    }

    /* Step 3-0: Magnitude BMP — ScalingCoeff = 255 * log10(|F|+1) / log10(DCvalue+1)
       DCvalue = |F(128,128)| (DC component after centering is at center) */
    double DCvalue = sqrt(Re[128][128]*Re[128][128] + Im[128][128]*Im[128][128]);
    printf("DCvalue = %f\n", DCvalue);

    for (u = 0; u < N; u++)
        for (v = 0; v < N; v++) {
            double mag = sqrt(Re[u][v]*Re[u][v] + Im[u][v]*Im[u][v]);
            double sc = 255.0 * log10(mag + 1.0) / log10(DCvalue + 1.0);
            if (sc > 255.0) sc = 255.0;
            if (sc < 0.0)   sc = 0.0;
            dft_bmp[u][v] = (unsigned char)sc;
        }
    write_bmp("dft.bmp", dft_bmp, N, N);
    printf("dft.bmp written\n");

    /* Step 4: 2D IDFT  f(m,n) = sum_u sum_v F(u,v) * e^(j2pi(um+vn)/N) */
    printf("Computing IDFT...\n");
    for (i = 0; i < N; i++) {
        if (i % 32 == 0) printf("  IDFT row %d/%d\n", i, N);
        for (j = 0; j < N; j++) {
            double re = 0.0;
            for (u = 0; u < N; u++) {
                for (v = 0; v < N; v++) {
                    double angle = 2.0 * M_PI * (u * i + v * j) / N;
                    re += Re[u][v] * cos(angle) - Im[u][v] * sin(angle);
                }
            }
            out[i][j] = re;
        }
    }

    /* Step 5 & 6: real part * (-1)^(m+n), clip to [0,255] */
    for (i = 0; i < N; i++)
        for (j = 0; j < N; j++) {
            double sign = ((i + j) % 2 == 0) ? 1.0 : -1.0;
            double val = out[i][j] * sign;
            if (val > 255.0) val = 255.0;
            if (val < 0.0)   val = 0.0;
            idft_bmp[i][j] = (unsigned char)(val + 0.5);
        }
    write_bmp("idft.bmp", idft_bmp, N, N);
    printf("idft.bmp written\n");

    /* Free */
    for (i = 0; i < 512; i++) free(input512[i]);
    free(input512);
    for (i = 0; i < N; i++) {
        free(img[i]); free(Re[i]); free(Im[i]); free(out[i]);
        free(dft_bmp[i]); free(idft_bmp[i]);
    }
    free(img); free(Re); free(Im); free(out);
    free(dft_bmp); free(idft_bmp);

    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define IMG_SIZE 512
#define BLOCK    8

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

/* alpha coefficient for DCT type-2 */
static double alpha(int k) {
    return (k == 0) ? (1.0 / sqrt((double)BLOCK)) : sqrt(2.0 / (double)BLOCK);
}

int main() {
    FILE *fp;
    int i, j;

    unsigned char **input = (unsigned char**)malloc(sizeof(unsigned char*) * IMG_SIZE);
    for (i = 0; i < IMG_SIZE; i++)
        input[i] = (unsigned char*)malloc(IMG_SIZE);

    fp = fopen("lena.img", "rb");
    for (i = 0; i < IMG_SIZE; i++)
        fread(input[i], 1, IMG_SIZE, fp);
    fclose(fp);

    /* DCT coefficient storage (double) and output images */
    double **dct_coeff = (double**)malloc(sizeof(double*) * IMG_SIZE);
    double **idct_out  = (double**)malloc(sizeof(double*) * IMG_SIZE);
    unsigned char **dct_bmp  = (unsigned char**)malloc(sizeof(unsigned char*) * IMG_SIZE);
    unsigned char **idct_bmp = (unsigned char**)malloc(sizeof(unsigned char*) * IMG_SIZE);
    for (i = 0; i < IMG_SIZE; i++) {
        dct_coeff[i] = (double*)malloc(sizeof(double) * IMG_SIZE);
        idct_out[i]  = (double*)malloc(sizeof(double) * IMG_SIZE);
        dct_bmp[i]   = (unsigned char*)malloc(IMG_SIZE);
        idct_bmp[i]  = (unsigned char*)malloc(IMG_SIZE);
    }

    /* Process each 8x8 block: 2D DCT type-2
       F(u,v) = a(u)*a(v) * sum_x sum_y f(x,y)*cos(pi(2x+1)u/2N)*cos(pi(2y+1)v/2N)  N=8 */
    int bx, by, x, y, u, v;
    for (bx = 0; bx < IMG_SIZE; bx += BLOCK) {
        for (by = 0; by < IMG_SIZE; by += BLOCK) {
            double block[BLOCK][BLOCK];
            /* Forward DCT */
            for (u = 0; u < BLOCK; u++) {
                for (v = 0; v < BLOCK; v++) {
                    double sum = 0.0;
                    for (x = 0; x < BLOCK; x++) {
                        for (y = 0; y < BLOCK; y++) {
                            sum += (double)input[bx+x][by+y]
                                   * cos(M_PI * (2*x + 1) * u / (2.0 * BLOCK))
                                   * cos(M_PI * (2*y + 1) * v / (2.0 * BLOCK));
                        }
                    }
                    block[u][v] = alpha(u) * alpha(v) * sum;
                    dct_coeff[bx+u][by+v] = block[u][v];
                }
            }
            /* Inverse DCT */
            for (x = 0; x < BLOCK; x++) {
                for (y = 0; y < BLOCK; y++) {
                    double sum = 0.0;
                    for (u = 0; u < BLOCK; u++) {
                        for (v = 0; v < BLOCK; v++) {
                            sum += alpha(u) * alpha(v) * block[u][v]
                                   * cos(M_PI * (2*x + 1) * u / (2.0 * BLOCK))
                                   * cos(M_PI * (2*y + 1) * v / (2.0 * BLOCK));
                        }
                    }
                    idct_out[bx+x][by+y] = sum;
                }
            }
        }
    }

    /* Display: F(0,0) = BLOCK * avg_pixel, so dividing by BLOCK maps DC to [0,255].
       Negative AC values clip to 0 (dark background), positive ACs are smaller bumps. */
    for (i = 0; i < IMG_SIZE; i++)
        for (j = 0; j < IMG_SIZE; j++) {
            double val = dct_coeff[i][j] / BLOCK;
            if (val > 255.0) val = 255.0;
            if (val < 0.0)   val = 0.0;
            dct_bmp[i][j] = (unsigned char)(val + 0.5);
        }
    write_bmp("dct.bmp", dct_bmp, IMG_SIZE, IMG_SIZE);
    printf("dct.bmp written\n");

    /* Clip IDCT output to [0,255] */
    for (i = 0; i < IMG_SIZE; i++)
        for (j = 0; j < IMG_SIZE; j++) {
            double val = idct_out[i][j];
            if (val > 255.0) val = 255.0;
            if (val < 0.0)   val = 0.0;
            idct_bmp[i][j] = (unsigned char)(val + 0.5);
        }
    write_bmp("idct.bmp", idct_bmp, IMG_SIZE, IMG_SIZE);
    printf("idct.bmp written\n");

    for (i = 0; i < IMG_SIZE; i++) {
        free(input[i]); free(dct_coeff[i]); free(idct_out[i]);
        free(dct_bmp[i]); free(idct_bmp[i]);
    }
    free(input); free(dct_coeff); free(idct_out);
    free(dct_bmp); free(idct_bmp);

    return 0;
}

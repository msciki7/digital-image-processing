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

    double **dst_coeff = (double**)malloc(sizeof(double*) * IMG_SIZE);
    double **idst_out  = (double**)malloc(sizeof(double*) * IMG_SIZE);
    unsigned char **dst_bmp  = (unsigned char**)malloc(sizeof(unsigned char*) * IMG_SIZE);
    unsigned char **idst_bmp = (unsigned char**)malloc(sizeof(unsigned char*) * IMG_SIZE);
    for (i = 0; i < IMG_SIZE; i++) {
        dst_coeff[i] = (double*)malloc(sizeof(double) * IMG_SIZE);
        idst_out[i]  = (double*)malloc(sizeof(double) * IMG_SIZE);
        dst_bmp[i]   = (unsigned char*)malloc(IMG_SIZE);
        idst_bmp[i]  = (unsigned char*)malloc(IMG_SIZE);
    }

    /* Process each 8x8 block: 2D DST
       F(u,v) = (2/(N+1)) * sum_x sum_y f(x,y)*sin(pi(x+1)(u+1)/(N+1))*sin(pi(y+1)(v+1)/(N+1))
       f(x,y) = (2/(N+1)) * sum_u sum_v F(u,v)*sin(pi(x+1)(u+1)/(N+1))*sin(pi(y+1)(v+1)/(N+1))
       N = 8, so N+1 = 9 */
    int bx, by, x, y, u, v;
    double scale = 2.0 / (BLOCK + 1.0);  /* = 2/9 */

    for (bx = 0; bx < IMG_SIZE; bx += BLOCK) {
        for (by = 0; by < IMG_SIZE; by += BLOCK) {
            double block[BLOCK][BLOCK];
            /* Forward DST */
            for (u = 0; u < BLOCK; u++) {
                for (v = 0; v < BLOCK; v++) {
                    double sum = 0.0;
                    for (x = 0; x < BLOCK; x++) {
                        for (y = 0; y < BLOCK; y++) {
                            sum += (double)input[bx+x][by+y]
                                   * sin(M_PI * (x+1) * (u+1) / (BLOCK + 1.0))
                                   * sin(M_PI * (y+1) * (v+1) / (BLOCK + 1.0));
                        }
                    }
                    block[u][v] = scale * sum;
                    dst_coeff[bx+u][by+v] = block[u][v];
                }
            }
            /* Inverse DST */
            for (x = 0; x < BLOCK; x++) {
                for (y = 0; y < BLOCK; y++) {
                    double sum = 0.0;
                    for (u = 0; u < BLOCK; u++) {
                        for (v = 0; v < BLOCK; v++) {
                            sum += block[u][v]
                                   * sin(M_PI * (x+1) * (u+1) / (BLOCK + 1.0))
                                   * sin(M_PI * (y+1) * (v+1) / (BLOCK + 1.0));
                        }
                    }
                    idst_out[bx+x][by+y] = scale * sum;
                }
            }
        }
    }

    /* DST has no DC component; use abs-value / (BLOCK+1) * 2 to map to [0,255].
       scale = 2/(N+1), so F_max ≈ 255 for a uniform block.  */
    for (i = 0; i < IMG_SIZE; i++)
        for (j = 0; j < IMG_SIZE; j++) {
            double val = fabs(dst_coeff[i][j]) / BLOCK * 2.0;
            if (val > 255.0) val = 255.0;
            dst_bmp[i][j] = (unsigned char)(val + 0.5);
        }
    write_bmp("dst.bmp", dst_bmp, IMG_SIZE, IMG_SIZE);
    printf("dst.bmp written\n");

    /* Clip IDST output to [0,255] */
    for (i = 0; i < IMG_SIZE; i++)
        for (j = 0; j < IMG_SIZE; j++) {
            double val = idst_out[i][j];
            if (val > 255.0) val = 255.0;
            if (val < 0.0)   val = 0.0;
            idst_bmp[i][j] = (unsigned char)(val + 0.5);
        }
    write_bmp("idst.bmp", idst_bmp, IMG_SIZE, IMG_SIZE);
    printf("idst.bmp written\n");

    for (i = 0; i < IMG_SIZE; i++) {
        free(input[i]); free(dst_coeff[i]); free(idst_out[i]);
        free(dst_bmp[i]); free(idst_bmp[i]);
    }
    free(input); free(dst_coeff); free(idst_out);
    free(dst_bmp); free(idst_bmp);

    return 0;
}

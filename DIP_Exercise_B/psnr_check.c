#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

/* ── 슬라이드 20 코드 그대로 ── */
int size;   /* 영상의 크기 */

double MSE(unsigned char **a, unsigned char **b) {
    int i, j;
    double result, sum = 0;
    for (i = 0; i < size; i++)
        for (j = 0; j < size; j++)
            sum += (a[i][j] - b[i][j]) * (a[i][j] - b[i][j]);
    result = (double)sum / (double)(size * size);
    return result;
}

void PSNR(unsigned char **a, unsigned char **b) {
    double M, psnr;
    M = MSE(a, b);
    if (M == 0)
        printf("\n    PSNR === infinity\n\n");
    else {
        psnr = 10 * log10((255.0 * 255.0) / M);
        printf("\n    PSNR === %f\n\n", psnr);
    }
}
/* ───────────────────────────── */

/* grayscale BMP 읽기 (8bpp, 1078 byte header) */
unsigned char **read_bmp(const char *filename, int w, int h) {
    FILE *fp = fopen(filename, "rb");
    /* BMP header: 14 File + 40 Info + 1024 ColorTable = 1078 bytes */
    fseek(fp, 1078, SEEK_SET);
    int row_size = (w + 3) & ~3;
    unsigned char *buf = (unsigned char*)malloc(row_size * h);
    fread(buf, 1, row_size * h, fp);
    fclose(fp);

    /* BMP는 하단부터 저장 → 뒤집어서 2D 배열로 */
    unsigned char **img = (unsigned char**)malloc(sizeof(unsigned char*) * h);
    for (int i = 0; i < h; i++) {
        img[i] = (unsigned char*)malloc(w);
        memcpy(img[i], buf + (h - 1 - i) * row_size, w);
    }
    free(buf);
    return img;
}

/* raw .img 읽기 */
unsigned char **read_img(const char *filename, int w, int h) {
    FILE *fp = fopen(filename, "rb");
    unsigned char **img = (unsigned char**)malloc(sizeof(unsigned char*) * h);
    for (int i = 0; i < h; i++) {
        img[i] = (unsigned char*)malloc(w);
        fread(img[i], 1, w, fp);
    }
    fclose(fp);
    return img;
}

void free_img(unsigned char **img, int h) {
    for (int i = 0; i < h; i++) free(img[i]);
    free(img);
}

int main() {
    /* ── 512x512 원본 읽기 ── */
    unsigned char **lena512 = read_img("lena.img", 512, 512);

    /* ── 256x256 서브샘플 원본 (DFT용) ── */
    unsigned char **lena256 = (unsigned char**)malloc(sizeof(unsigned char*) * 256);
    for (int i = 0; i < 256; i++) {
        lena256[i] = (unsigned char*)malloc(256);
        for (int j = 0; j < 256; j++)
            lena256[i][j] = lena512[2*i][2*j];
    }

    /* ══════════════════════════════════════
       1. IDFT vs 256x256 subsampled origin
       ══════════════════════════════════════ */
    printf("=== [1] IDFT (256x256) vs Original subsampled (256x256) ===");
    unsigned char **idft = read_bmp("idft.bmp", 256, 256);
    size = 256;
    PSNR(lena256, idft);
    free_img(idft, 256);

    /* ══════════════════════════════════════
       2. IDCT vs 512x512 origin
       ══════════════════════════════════════ */
    printf("=== [2] IDCT (512x512) vs Original (512x512) ===");
    unsigned char **idct = read_bmp("idct.bmp", 512, 512);
    size = 512;
    PSNR(lena512, idct);
    free_img(idct, 512);

    /* ══════════════════════════════════════
       3. IDST vs 512x512 origin
       ══════════════════════════════════════ */
    printf("=== [3] IDST (512x512) vs Original (512x512) ===");
    unsigned char **idst = read_bmp("idst.bmp", 512, 512);
    size = 512;
    PSNR(lena512, idst);
    free_img(idst, 512);

    free_img(lena512, 512);
    free_img(lena256, 256);
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdint.h>

#define IMG_SIZE 512
#define BLOCK    8
#define QP       5

/* JPEG standard luma quantization table */
static const int q_luma[8][8] = {
    {16, 11, 10, 16, 24,  40,  51,  61},
    {12, 12, 14, 19, 26,  58,  60,  55},
    {14, 13, 16, 24, 40,  57,  69,  56},
    {14, 17, 22, 29, 51,  87,  80,  62},
    {18, 22, 37, 56, 68, 109, 103,  77},
    {24, 35, 55, 64, 81, 104, 113,  92},
    {49, 64, 78, 87, 103, 121, 120, 101},
    {72, 92, 95, 98, 112, 100, 103,  99}
};

/* ---------- DCT helpers ---------- */
static double alpha(int k) {
    return (k == 0) ? (1.0 / sqrt(8.0)) : sqrt(2.0 / 8.0);
}

static void dct8x8(double in[8][8], double out[8][8]) {
    for (int u = 0; u < 8; u++) {
        for (int v = 0; v < 8; v++) {
            double s = 0.0;
            for (int x = 0; x < 8; x++)
                for (int y = 0; y < 8; y++)
                    s += in[x][y]
                         * cos(M_PI * (2*x+1) * u / 16.0)
                         * cos(M_PI * (2*y+1) * v / 16.0);
            out[u][v] = alpha(u) * alpha(v) * s;
        }
    }
}

static void idct8x8(double in[8][8], double out[8][8]) {
    for (int x = 0; x < 8; x++) {
        for (int y = 0; y < 8; y++) {
            double s = 0.0;
            for (int u = 0; u < 8; u++)
                for (int v = 0; v < 8; v++)
                    s += alpha(u) * alpha(v) * in[u][v]
                         * cos(M_PI * (2*x+1) * u / 16.0)
                         * cos(M_PI * (2*y+1) * v / 16.0);
            out[x][y] = s;
        }
    }
}

/* ---------- BMP write (8-bit grayscale) ---------- */
static void write_le16(FILE *fp, uint16_t v) {
    fputc(v & 0xFF, fp); fputc((v>>8)&0xFF, fp);
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

    /* File header */
    fputc('B',fp); fputc('M',fp);
    write_le32(fp, filesize);
    write_le32(fp, 0);
    write_le32(fp, offset);

    /* Info header */
    write_le32(fp, 40);
    write_le32(fp, w);
    write_le32(fp, h);
    write_le16(fp, 1);
    write_le16(fp, 8);
    write_le32(fp, 0);
    write_le32(fp, pix_data);
    write_le32(fp, 2835); /* 72 DPI */
    write_le32(fp, 2835);
    write_le32(fp, 0);
    write_le32(fp, 0);

    /* Grayscale palette */
    for (int i = 0; i < 256; i++) {
        fputc(i,fp); fputc(i,fp); fputc(i,fp); fputc(0,fp);
    }

    /* Pixel rows (bottom-up, padded) */
    unsigned char *row = calloc(row_pad, 1);
    for (int i = h-1; i >= 0; i--) {
        memcpy(row, img[i], w);
        fwrite(row, 1, row_pad, fp);
    }
    free(row);
    fclose(fp);
}

/* ---------- PSNR ---------- */
static void print_psnr(unsigned char **a, unsigned char **b, int size) {
    double sum = 0.0;
    for (int i = 0; i < size; i++)
        for (int j = 0; j < size; j++) {
            double d = (double)a[i][j] - (double)b[i][j];
            sum += d * d;
        }
    double mse = sum / ((double)size * size);
    if (mse == 0.0) printf("PSNR = infinity\n");
    else            printf("PSNR = %.2f dB\n", 10.0*log10(255.0*255.0/mse));
}

static int clamp(int v) { return v<0?0:(v>255?255:v); }

/* ---------- main ---------- */
int main(void) {
    unsigned char **input    = malloc(IMG_SIZE * sizeof(unsigned char *));
    unsigned char **dct_q_im = malloc(IMG_SIZE * sizeof(unsigned char *));
    unsigned char **recon    = malloc(IMG_SIZE * sizeof(unsigned char *));
    for (int i = 0; i < IMG_SIZE; i++) {
        input[i]    = malloc(IMG_SIZE);
        dct_q_im[i] = calloc(IMG_SIZE, 1);
        recon[i]    = malloc(IMG_SIZE);
    }

    FILE *fp = fopen("lena.img", "rb");
    if (!fp) { fprintf(stderr, "Cannot open lena.img\n"); return 1; }
    for (int i = 0; i < IMG_SIZE; i++)
        fread(input[i], 1, IMG_SIZE, fp);
    fclose(fp);

    double blk[8][8], dct[8][8], iq[8][8], idct[8][8];
    int    qblk[8][8];

    for (int bi = 0; bi < IMG_SIZE/BLOCK; bi++) {
        for (int bj = 0; bj < IMG_SIZE/BLOCK; bj++) {

            /* extract block (level-shift by -128) */
            for (int i = 0; i < 8; i++)
                for (int j = 0; j < 8; j++)
                    blk[i][j] = (double)input[bi*8+i][bj*8+j] - 128.0;

            /* DCT */
            dct8x8(blk, dct);

            /* Quantize: P = Round(DCT / (q * QP)) */
            for (int i = 0; i < 8; i++)
                for (int j = 0; j < 8; j++) {
                    int step = q_luma[i][j] * QP;
                    qblk[i][j] = (int)round(dct[i][j] / step);
                }

            /* Visualize DCT coefficients (before Q) with log magnitude scaling.
               This produces a dark image with bright spots at low frequencies,
               matching the expected DCT-domain visualization in the slides.   */
            for (int i = 0; i < 8; i++)
                for (int j = 0; j < 8; j++) {
                    double mag = fabs(dct[i][j]);
                    /* log(1 + |F|) / log(1 + max_expected) * 255 */
                    int v = (int)(log(1.0 + mag) / log(1.0 + 2000.0) * 255.0);
                    dct_q_im[bi*8+i][bj*8+j] = (unsigned char)clamp(v);
                }

            /* Inverse quantize: IQ = P * q * QP */
            for (int i = 0; i < 8; i++)
                for (int j = 0; j < 8; j++) {
                    int step = q_luma[i][j] * QP;
                    iq[i][j] = (double)(qblk[i][j] * step);
                }

            /* IDCT + level-shift +128, clip */
            idct8x8(iq, idct);
            for (int i = 0; i < 8; i++)
                for (int j = 0; j < 8; j++)
                    recon[bi*8+i][bj*8+j] =
                        (unsigned char)clamp((int)round(idct[i][j] + 128.0));
        }
    }

    write_bmp("dct-q.bmp",    dct_q_im, IMG_SIZE, IMG_SIZE);
    write_bmp("iq-idct.bmp",  recon,    IMG_SIZE, IMG_SIZE);
    print_psnr(input, recon, IMG_SIZE);
    printf("dct-q.bmp and iq-idct.bmp saved.\n");

    for (int i = 0; i < IMG_SIZE; i++) {
        free(input[i]); free(dct_q_im[i]); free(recon[i]);
    }
    free(input); free(dct_q_im); free(recon);
    return 0;
}

#include <stdio.h>
#include <stdlib.h>

#define IMG_WIDTH  512
#define IMG_HEIGHT 512
typedef unsigned char uint8;

/* ── 유틸 ── */
uint8** alloc_2d(int h, int w) {
    int i;
    uint8** img = (uint8**)malloc(sizeof(uint8*) * h);
    img[0] = (uint8*)malloc(sizeof(uint8) * h * w);
    for (i = 1; i < h; i++) img[i] = img[i-1] + w;
    return img;
}
void free_2d(uint8** img) { free(img[0]); free(img); }

uint8** read_raw(const char* path, int w, int h) {
    int y;
    FILE* fp = fopen(path, "rb");
    uint8** img = alloc_2d(h, w);
    for (y = 0; y < h; y++) fread(img[y], 1, w, fp);
    fclose(fp);
    return img;
}

void write_bmp(const char* path, uint8** img, int w, int h) {
    int y, i, p;
    int row_size = ((w + 3) / 4) * 4;
    int px_off   = 14 + 40 + 256 * 4;
    int file_size = px_off + row_size * h;
    unsigned char fh[14] = {0}, ih[40] = {0};
    FILE* fp = fopen(path, "wb");

    fh[0]='B'; fh[1]='M';
    fh[2]=(uint8)file_size; fh[3]=(uint8)(file_size>>8);
    fh[4]=(uint8)(file_size>>16); fh[5]=(uint8)(file_size>>24);
    fh[10]=(uint8)px_off; fh[11]=(uint8)(px_off>>8);
    fh[12]=(uint8)(px_off>>16); fh[13]=(uint8)(px_off>>24);
    fwrite(fh, 1, 14, fp);

    ih[0]=40;
    ih[4]=(uint8)w; ih[5]=(uint8)(w>>8); ih[6]=(uint8)(w>>16); ih[7]=(uint8)(w>>24);
    ih[8]=(uint8)h; ih[9]=(uint8)(h>>8); ih[10]=(uint8)(h>>16); ih[11]=(uint8)(h>>24);
    ih[12]=1; ih[14]=8;
    fwrite(ih, 1, 40, fp);

    for (i = 0; i < 256; i++) {
        unsigned char c[4] = {(uint8)i,(uint8)i,(uint8)i,0};
        fwrite(c, 1, 4, fp);
    }
    for (y = h-1; y >= 0; y--) {
        fwrite(img[y], 1, w, fp);
        for (p = 0; p < row_size - w; p++) fputc(0, fp);
    }
    fclose(fp);
}

/* ── Exercise 2: Zoom-in x2 (512x512 → 1024x1024) ── */
uint8** zoom_in_double(uint8** input) {
    int y, x;
    uint8** out = alloc_2d(1024, 1024);
    for (y = 0; y < 1024; y++)
        for (x = 0; x < 1024; x++)
            out[y][x] = input[y/2][x/2];
    return out;
}

int main(void) {
    uint8** lena   = read_raw("lena.img", IMG_WIDTH, IMG_HEIGHT);
    uint8** output = zoom_in_double(lena);
    write_bmp("output_2_zoom_in.bmp", output, 1024, 1024);
    free_2d(lena);
    free_2d(output);
    printf("output_2_zoom_in.bmp 생성 완료 (1024x1024)\n");
    return 0;
}

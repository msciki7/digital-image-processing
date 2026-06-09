#include <stdio.h>
#include <stdlib.h>
#include <math.h>

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

/* ── Exercise 6: Zoom-out → Translation(200,20) → Rotation(0.5 rad) ── */
uint8** zoom_out_half(uint8** input) {
    int y, x;
    uint8** out = alloc_2d(256, 256);
    for (y = 0; y < 256; y++)
        for (x = 0; x < 256; x++)
            out[y][x] = input[y*2][x*2];
    return out;
}

/* hole을 상하좌우 평균으로 채우기 (PDF p.39) */
void fill_holes_once(uint8** img) {
    int y, x, a, b, c, d;
    uint8** temp = alloc_2d(512, 512);

    for (y = 0; y < 512; y++)
        for (x = 0; x < 512; x++)
            temp[y][x] = img[y][x];

    for (y = 1; y < 511; y++) {
        for (x = 1; x < 511; x++) {
            if (img[y][x] == 0) {
                a = img[y-1][x]; b = img[y][x-1];
                c = img[y][x+1]; d = img[y+1][x];
                if (a != 0 || b != 0 || c != 0 || d != 0)
                    temp[y][x] = (uint8)((a + b + c + d) / 4);
            }
        }
    }

    for (y = 0; y < 512; y++)
        for (x = 0; x < 512; x++)
            img[y][x] = temp[y][x];

    free_2d(temp);
}

/*
    교수님 수식 관례: x = row(행), y = col(열)
      x' = row + dx,  y' = col + dy
      x''= x'*cos(θ) - y'*sin(θ)  → new row
      y''= x'*sin(θ) + y'*cos(θ)  → new col
*/
uint8** translation_rotation(uint8** small, int tx, int ty, double theta) {
    int y, x, new_x, new_y, i;
    double x1, y1, x2, y2;
    double cos_v = cos(theta);
    double sin_v = sin(theta);

    uint8** out = alloc_2d(512, 512);
    for (y = 0; y < 512; y++)
        for (x = 0; x < 512; x++)
            out[y][x] = 0;

    for (y = 0; y < 256; y++) {
        for (x = 0; x < 256; x++) {
            x1 = y + tx;                   /* x_formula = row + dx */
            y1 = x + ty;                   /* y_formula = col + dy */
            x2 = x1 * cos_v - y1 * sin_v; /* new row  */
            y2 = x1 * sin_v + y1 * cos_v; /* new col  */
            new_y = (int)(x2 + 0.5);
            new_x = (int)(y2 + 0.5);
            if (new_x >= 0 && new_x < 512 && new_y >= 0 && new_y < 512)
                out[new_y][new_x] = small[y][x];
        }
    }

    /* hole 채우기 (횟수: 0=hole 그대로, 3=대부분 채워짐) */
    for (i = 0; i < 1; i++)
        fill_holes_once(out);

    return out;
}

int main(void) {
    uint8** lena   = read_raw("lena.img", IMG_WIDTH, IMG_HEIGHT);
    uint8** small  = zoom_out_half(lena);
    uint8** output = translation_rotation(small, 200, 20, 0.5);
    write_bmp("output_6_rotation.bmp", output, 512, 512);
    free_2d(lena);
    free_2d(small);
    free_2d(output);
    printf("output_6_rotation.bmp 생성 완료\n");
    return 0;
}

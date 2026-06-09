#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define IMG_SIZE 512
#define SYM_COUNT 511   /* symbols: -255 to 255, index = sym + 255 */
#define MAX_NODES (SYM_COUNT * 2 + 5)

/* ---------- Huffman node pool ---------- */
typedef struct {
    double prob;
    int    sym;        /* actual symbol; -9999 for internal node */
    int    left, right;/* child indices; -1 = none */
} HNode;

static HNode nodes[MAX_NODES];
static int   n_nodes = 0;

/* ---------- Min-heap on node indices ---------- */
static int heap[MAX_NODES];
static int heap_sz = 0;

static void heap_push(int idx) {
    heap[heap_sz] = idx;
    int i = heap_sz++;
    while (i > 0) {
        int p = (i - 1) / 2;
        if (nodes[heap[p]].prob > nodes[heap[i]].prob) {
            int t = heap[p]; heap[p] = heap[i]; heap[i] = t;
            i = p;
        } else break;
    }
}

static int heap_pop(void) {
    int ret = heap[0];
    heap[0] = heap[--heap_sz];
    int i = 0;
    while (1) {
        int l = 2*i+1, r = 2*i+2, s = i;
        if (l < heap_sz && nodes[heap[l]].prob < nodes[heap[s]].prob) s = l;
        if (r < heap_sz && nodes[heap[r]].prob < nodes[heap[s]].prob) s = r;
        if (s == i) break;
        int t = heap[i]; heap[i] = heap[s]; heap[s] = t;
        i = s;
    }
    return ret;
}

/* ---------- Code table ---------- */
static char codes[SYM_COUNT][600];
static int  code_len[SYM_COUNT];

static void gen_codes(int idx, char *buf, int depth) {
    if (nodes[idx].left == -1 && nodes[idx].right == -1) {
        buf[depth] = '\0';
        int si = nodes[idx].sym + 255;
        strcpy(codes[si], buf);
        code_len[si] = depth;
        return;
    }
    if (nodes[idx].left  != -1) { buf[depth]='0'; gen_codes(nodes[idx].left,  buf, depth+1); }
    if (nodes[idx].right != -1) { buf[depth]='1'; gen_codes(nodes[idx].right, buf, depth+1); }
}

int main(void) {
    /* ------ read lena.img ------ */
    unsigned char **img = malloc(IMG_SIZE * sizeof(unsigned char *));
    for (int i = 0; i < IMG_SIZE; i++)
        img[i] = malloc(IMG_SIZE);

    FILE *fp = fopen("lena.img", "rb");
    if (!fp) { fprintf(stderr, "Cannot open lena.img\n"); return 1; }
    for (int i = 0; i < IMG_SIZE; i++)
        fread(img[i], 1, IMG_SIZE, fp);
    fclose(fp);

    /* ------ DPCM (left prediction) ------ */
    long hist[SYM_COUNT];
    memset(hist, 0, sizeof(hist));
    long total = (long)IMG_SIZE * IMG_SIZE;

    for (int i = 0; i < IMG_SIZE; i++) {
        for (int j = 0; j < IMG_SIZE; j++) {
            int diff = (j == 0) ? (int)img[i][j]
                                : ((int)img[i][j] - (int)img[i][j-1]);
            hist[diff + 255]++;
        }
    }

    /* ------ Entropy ------ */
    double entropy = 0.0;
    for (int k = 0; k < SYM_COUNT; k++) {
        if (hist[k] > 0) {
            double p = (double)hist[k] / total;
            entropy -= p * log2(p);
        }
    }
    printf("Entropy = %.4f bits\n", entropy);

    /* ------ Build Huffman tree ------ */
    n_nodes  = 0;
    heap_sz  = 0;
    memset(codes,    0, sizeof(codes));
    memset(code_len, 0, sizeof(code_len));

    for (int k = 0; k < SYM_COUNT; k++) {
        if (hist[k] > 0) {
            nodes[n_nodes].prob  = (double)hist[k] / total;
            nodes[n_nodes].sym   = k - 255;
            nodes[n_nodes].left  = -1;
            nodes[n_nodes].right = -1;
            heap_push(n_nodes++);
        }
    }

    while (heap_sz > 1) {
        int a = heap_pop(), b = heap_pop();
        nodes[n_nodes].prob  = nodes[a].prob + nodes[b].prob;
        nodes[n_nodes].sym   = -9999;
        nodes[n_nodes].left  = a;
        nodes[n_nodes].right = b;
        heap_push(n_nodes++);
    }

    char buf[600] = "";
    gen_codes(heap_pop(), buf, 0);

    /* ------ Output table ------ */
    fp = fopen("Huffman_table.txt", "w");
    fprintf(fp, "%-8s %-12s %-12s %-16s %-30s %-12s %s\n",
            "Symbol", "Prob", "-log2(P)", "-log2(P)*P",
            "Codeword", "CodeLength", "AVG_Length");

    double avg_len = 0.0;
    for (int k = 0; k < SYM_COUNT; k++) {
        if (hist[k] == 0) continue;
        int    sym  = k - 255;
        double p    = (double)hist[k] / total;
        double nlog = -log2(p);
        double cont = nlog * p;
        double avg  = p * code_len[k];
        avg_len += avg;
        fprintf(fp, "%-8d %-12.6f %-12.4f %-16.4f %-30s %-12d %.6f\n",
                sym, p, nlog, cont, codes[k], code_len[k], avg);
    }
    fprintf(fp, "\nEntropy        = %.4f bits\n", entropy);
    fprintf(fp, "Avg Code Length = %.4f bits\n", avg_len);
    fclose(fp);

    printf("Avg Code Length = %.4f bits\n", avg_len);
    printf("Huffman_table.txt saved.\n");

    for (int i = 0; i < IMG_SIZE; i++) free(img[i]);
    free(img);
    return 0;
}

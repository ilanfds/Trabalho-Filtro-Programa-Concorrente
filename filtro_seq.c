#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
    int width;
    int height;
    int max_value;
    unsigned char *data; // RGB intercalado
} Image;

// --------- Util ---------
static inline int clamp_int(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// --------- IO PPM ---------
Image *load_ppm(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror("Erro ao abrir imagem");
        return NULL;
    }

    Image *img = (Image*)malloc(sizeof(Image));
    if (!img) { fclose(f); return NULL; }

    char type[3];
    if (fscanf(f, "%2s", type) != 1) { fclose(f); free(img); return NULL; }
    if (strcmp(type, "P6") != 0) {
        fprintf(stderr, "Formato não suportado: %s\n", type);
        fclose(f);
        free(img);
        return NULL;
    }

    // Ignora comentários
    int c;
    do {
        c = fgetc(f);
        if (c == '#') { while (fgetc(f) != '\n'); }
    } while (c == '#' || c == '\n' || c == '\r' || c == ' ' || c == '\t');
    ungetc(c, f);

    if (fscanf(f, "%d %d %d", &img->width, &img->height, &img->max_value) != 3) {
        fclose(f); free(img); return NULL;
    }
    fgetc(f); // consome um único whitespace após o header

    size_t nbytes = (size_t)3 * img->width * img->height;
    img->data = (unsigned char*)malloc(nbytes);
    if (!img->data) { fclose(f); free(img); return NULL; }

    if (fread(img->data, 1, nbytes, f) != nbytes) {
        perror("Leitura incompleta dos dados PPM");
        fclose(f); free(img->data); free(img); return NULL;
    }

    fclose(f);
    return img;
}

void save_ppm(const char *filename, const Image *img) {
    FILE *f = fopen(filename, "wb");
    if (!f) {
        perror("Erro ao salvar imagem");
        return;
    }
    fprintf(f, "P6\n%d %d\n%d\n", img->width, img->height, img->max_value);
    fwrite(img->data, 1, (size_t)3 * img->width * img->height, f);
    fclose(f);
}

// --------- Edge Detection (Laplaciano) ---------
// Pipeline:
// 1) Converte para escala de cinza (luma aproximada)
// 2) Convolução 3x3 com kernel Laplaciano
// 3) Valor absoluto + clamp para [0,255]
// 4) Replica o resultado nos 3 canais (para PPM colorido)
void apply_edge_detection(Image *img) {
    const int w = img->width;
    const int h = img->height;
    const size_t N = (size_t)w * h;

    // 1) Gray
    float *gray = (float*)malloc(sizeof(float) * N);
    if (!gray) return;

    for (int i = 0; i < w * h; i++) {
        unsigned char r = img->data[3*i + 0];
        unsigned char g = img->data[3*i + 1];
        unsigned char b = img->data[3*i + 2];
        // Luma aproximada (BT.601)
        gray[i] = 0.299f * r + 0.587f * g + 0.114f * b;
    }

    // 2) Convolução Laplaciana (com clamp de borda)
    // Kernel clássico:
    // [-1 -1 -1]
    // [-1  8 -1]
    // [-1 -1 -1]
    unsigned char *out = (unsigned char*)malloc(3 * N);
    if (!out) { free(gray); return; }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {

            float acc = 0.0f;
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    int yy = clamp_int(y + ky, 0, h - 1);
                    int xx = clamp_int(x + kx, 0, w - 1);
                    float k = -1.0f;
                    if (ky == 0 && kx == 0) k = 8.0f;
                    acc += gray[yy * w + xx] * k;
                }
            }

            // 3) valor absoluto + clamp
            float v = fabsf(acc);
            if (v > 255.0f) v = 255.0f;

            unsigned char e = (unsigned char)(v + 0.5f);
            size_t idx3 = (size_t)3 * (y * w + x);
            out[idx3 + 0] = e;
            out[idx3 + 1] = e;
            out[idx3 + 2] = e;
        }
    }

    // 4) escreve na imagem
    memcpy(img->data, out, 3 * N);
    free(out);
    free(gray);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Uso: %s <imagem_entrada.ppm> <imagem_saida.ppm>\n", argv[0]);
        return 1;
    }

    Image *img = load_ppm(argv[1]);
    if (!img) return 1;

    apply_edge_detection(img);
    save_ppm(argv[2], img);

    free(img->data);
    free(img);
    printf("Edge Detection aplicado com sucesso!\n");
    return 0;
}

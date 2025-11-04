#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

typedef struct {
    unsigned char r, g, b;
} Pixel;

typedef struct {
    int largura, altura;
    Pixel **pixels;
} ImagemPPM;

typedef struct {
    int id;
    int nthreads;
    int nlinhas;
} t_args;

// variáveis globais (entrada e saída)
ImagemPPM *imagem = NULL;
ImagemPPM *imagem_saida = NULL;
int escolha = 3;

// protótipos
ImagemPPM* lerImagem(const char* nomeArquivo);
void salvarImagem(const char* nomeArquivo, ImagemPPM *img);
void liberarImagem(ImagemPPM *img);
Pixel aplicarFiltro(Pixel **pixels, int x, int y, int largura, int altura, int escolha);
void* thread_convolucao(void* arg);

void* thread_convolucao(void* arg) {
    t_args *a = (t_args*) arg;
    int id = a->id;
    int nthreads = a->nthreads;
    int altura = a->nlinhas;

    int ini = (id * altura) / nthreads;
    int fim = ((id + 1) * altura) / nthreads;
    if (ini < 0) ini = 0;
    if (fim > altura) fim = altura;

    for (int y = ini; y < fim; y++) {
        for (int x = 0; x < imagem->largura; x++) {
            imagem_saida->pixels[y][x] = aplicarFiltro(imagem->pixels, x, y, imagem->largura, imagem->altura, escolha);
        }
    }
    return NULL;
}

Pixel aplicarFiltro(Pixel **pixels, int x, int y, int largura, int altura, int escolha) {
    Pixel resultado = {0,0,0};
    int kernel[3][3];
    int somaKernel = 0;

    switch (escolha) {
        case 1: // Blur (normalizado)
            for (int i = 0; i < 3; i++)
                for (int j = 0; j < 3; j++) kernel[i][j] = 1;
            somaKernel = 9;
            break;
        case 2: // Sharpen
            kernel[0][0]=0;  kernel[0][1]=-1; kernel[0][2]=0;
            kernel[1][0]=-1; kernel[1][1]=5;  kernel[1][2]=-1;
            kernel[2][0]=0;  kernel[2][1]=-1; kernel[2][2]=0;
            somaKernel = 1; // soma do kernel (pode ser usado para normalizar, se desejar)
            break;
        case 3: // Edge Detection
            kernel[0][0]=-1; kernel[0][1]=-1; kernel[0][2]=-1;
            kernel[1][0]=-1; kernel[1][1]=8;  kernel[1][2]=-1;
            kernel[2][0]=-1; kernel[2][1]=-1; kernel[2][2]=-1;
            somaKernel = 0; // soma zero (não dividir)
            break;
        default:
            return pixels[y][x];
    }

    int somaR = 0, somaG = 0, somaB = 0;
    for (int fy = -1; fy <= 1; fy++) {
        for (int fx = -1; fx <= 1; fx++) {
            int ix = x + fx;
            int iy = y + fy;
            if (ix < 0 || ix >= largura || iy < 0 || iy >= altura) continue;
            int k = kernel[fy+1][fx+1];
            somaR += pixels[iy][ix].r * k;
            somaG += pixels[iy][ix].g * k;
            somaB += pixels[iy][ix].b * k;
        }
    }

    if (somaKernel != 0) {
        somaR /= somaKernel;
        somaG /= somaKernel;
        somaB /= somaKernel;
    }

    // clipping entre 0 e 255
    if (somaR < 0) somaR = 0;
    if (somaR > 255) somaR = 255;
    if (somaG < 0) somaG = 0;
    if (somaG > 255) somaG = 255;
    if (somaB < 0) somaB = 0;
    if (somaB > 255) somaB = 255;

    resultado.r = (unsigned char)somaR;
    resultado.g = (unsigned char)somaG;
    resultado.b = (unsigned char)somaB;
    return resultado;
}

ImagemPPM* lerImagem(const char* nomeArquivo) {
    FILE *f = fopen(nomeArquivo, "rb");
    if (!f) { perror("erro ao abrir"); return NULL; }

    char tipo[3];
    if (fscanf(f, "%2s", tipo) != 1) { fclose(f); return NULL; }
    if (tipo[0] != 'P' || tipo[1] != '6') { fprintf(stderr, "Nao eh P6\n"); fclose(f); return NULL; }

    // pular comentários e ler largura/altura/max
    int c = fgetc(f);
    while (c == '\n' || c == '\r' || c == ' ' || c == '\t') c = fgetc(f);
    if (c == '#') {
        // pula até fim da linha
        while (c != '\n' && c != EOF) c = fgetc(f);
    } else {
        ungetc(c, f);
    }

    int largura, altura, maxval;
    if (fscanf(f, "%d %d %d", &largura, &altura, &maxval) != 3) {
        fprintf(stderr, "Cabecalho invalido\n");
        fclose(f);
        return NULL;
    }
    fgetc(f); // consome um '\n' após o cabeçalho

    if (maxval != 255) {
        fprintf(stderr, "MaxVal diferente de 255 nao suportado\n");
        fclose(f);
        return NULL;
    }

    ImagemPPM *img = malloc(sizeof(ImagemPPM));
    img->largura = largura;
    img->altura = altura;
    img->pixels = malloc(altura * sizeof(Pixel*));
    for (int i = 0; i < altura; i++) {
        img->pixels[i] = malloc(largura * sizeof(Pixel));
        size_t lidos = fread(img->pixels[i], sizeof(Pixel), largura, f);
        if (lidos != (size_t)largura) {
            fprintf(stderr, "Erro lendo pixels (linha %d)\n", i);
            // liberar parcialmente
            for (int j = 0; j <= i; j++) free(img->pixels[j]);
            free(img->pixels);
            free(img);
            fclose(f);
            return NULL;
        }
    }

    fclose(f);
    return img;
}

void salvarImagem(const char* nomeArquivo, ImagemPPM *img) {
    FILE *f = fopen(nomeArquivo, "wb");
    if (!f) { perror("erro salvar"); return; }
    fprintf(f, "P6\n%d %d\n255\n", img->largura, img->altura);
    for (int i = 0; i < img->altura; i++) {
        fwrite(img->pixels[i], sizeof(Pixel), img->largura, f);
    }
    fclose(f);
}

void liberarImagem(ImagemPPM *img) {
    if (!img) return;
    for (int i = 0; i < img->altura; i++) free(img->pixels[i]);
    free(img->pixels);
    free(img);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <entrada.ppm> <saida.ppm> <nthreads>\n", argv[0]);
        return 1;
    }
    const char *arquivo_in = argv[1];
    const char *arquivo_out = argv[2];
    int nthreads = atoi(argv[3]);
    if (nthreads <= 0) nthreads = 1;

    imagem = lerImagem(arquivo_in);
    if (!imagem) return 1;

    // criar imagem de saída (mesma dimensão)
    imagem_saida = malloc(sizeof(ImagemPPM));
    imagem_saida->largura = imagem->largura;
    imagem_saida->altura = imagem->altura;
    imagem_saida->pixels = malloc(imagem->altura * sizeof(Pixel*));
    for (int i = 0; i < imagem->altura; i++) {
        imagem_saida->pixels[i] = malloc(imagem->largura * sizeof(Pixel));
    }

    pthread_t *threads = malloc(nthreads * sizeof(pthread_t));
    t_args *args = malloc(nthreads * sizeof(t_args));

    struct timeval t0, t1;
    gettimeofday(&t0, NULL);

    for (int i = 0; i < nthreads; i++) {
        args[i].id = i;
        args[i].nthreads = nthreads;
        args[i].nlinhas = imagem->altura;
        if (pthread_create(&threads[i], NULL, thread_convolucao, &args[i]) != 0) {
            perror("pthread_create");
            // fallback: continuar sem criar mais threads
            for (int j = 0; j < i; j++) pthread_join(threads[j], NULL);
            break;
        }
    }

    for (int i = 0; i < nthreads; i++) pthread_join(threads[i], NULL);

    gettimeofday(&t1, NULL);
    double tempo = (t1.tv_sec - t0.tv_sec) + (t1.tv_usec - t0.tv_usec)/1e6;
    printf("Tempo de execução: %.6f s\n", tempo);

    salvarImagem(arquivo_out, imagem_saida);

    printf("Imagem salva em '%s'", arquivo_out);

    // liberar tudo
    liberarImagem(imagem);
    liberarImagem(imagem_saida);
    free(threads);
    free(args);
    return 0;
}

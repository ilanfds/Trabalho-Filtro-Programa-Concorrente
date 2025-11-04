#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <math.h>
#include <string.h>

typedef struct {
    int wins;
    int ties;
    double sum;
    double sumsq;
    double minv;
    double maxv;
    int samples;
} Stats;

// ---- Funções auxiliares ----
static double now_s(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

static void stats_init(Stats *s) {
    s->wins = 0; s->ties = 0;
    s->sum = 0.0; s->sumsq = 0.0; s->samples = 0;
    s->minv = 1e300; s->maxv = -1e300;
}

static void stats_add(Stats *s, double v) {
    s->sum += v;
    s->sumsq += v*v;
    s->samples++;
    if (v < s->minv) s->minv = v;
    if (v > s->maxv) s->maxv = v;
}

static double stats_mean(const Stats *s) {
    return (s->samples > 0) ? s->sum / s->samples : 0.0;
}

static double stats_stddev(const Stats *s) {
    if (s->samples <= 1) return 0.0;
    double m = stats_mean(s);
    double var = (s->sumsq - s->samples*m*m) / (s->samples - 1);
    return (var > 0 ? sqrt(var) : 0.0);
}

// ---- Execução temporizada ----
static double time_cmd(const char *cmd) {
    double t0 = now_s();
    int rc = system(cmd);
    double t1 = now_s();
    if (rc != 0) fprintf(stderr, "⚠️ Erro ao executar comando: %s\n", cmd);
    return t1 - t0;
}

// ---- Gera nomes de saída ----
static void make_output_name(const char *input, const char *suffix, char *out, size_t out_sz) {
    const char *dot = strrchr(input, '.');
    size_t len = dot ? (size_t)(dot - input) : strlen(input);
    if (len >= out_sz - 1) len = out_sz - 1;
    strncpy(out, input, len);
    out[len] = '\0';
    snprintf(out + len, out_sz - len, "_%s.ppm", suffix);
}

// ---- MAIN ----
int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <imagem_entrada.ppm> <nthreads> <runs>\n", argv[0]);
        return 1;
    }

    const char *input = argv[1];
    int nthreads = atoi(argv[2]);
    int runs = atoi(argv[3]);
    if (nthreads <= 0) nthreads = 1;
    if (runs <= 0) runs = 1;

    const char *seq_bin  = "./filtro_seq";
    const char *conc_bin = "./filtro_conc";

    char seq_out[512], conc_out[512];
    make_output_name(input, "seq",  seq_out,  sizeof(seq_out));
    make_output_name(input, "conc", conc_out, sizeof(conc_out));

    printf("==============================================\n");
    printf("BENCHMARK - Edge Detection (Sequencial vs Concorrente)\n");
    printf("==============================================\n");
    printf("Entrada: %s | Threads: %d | Execuções: %d\n", input, nthreads, runs);
    printf("Saídas:  %s / %s\n\n", seq_out, conc_out);

    Stats Sseq, Sconc;
    stats_init(&Sseq);
    stats_init(&Sconc);

    int seq_wins = 0, conc_wins = 0, ties = 0;

    for (int i = 0; i < runs; i++) {
        char seq_cmd[512], conc_cmd[512];

        snprintf(seq_cmd, sizeof(seq_cmd), "%s %s %s", seq_bin, input, seq_out);
        snprintf(conc_cmd, sizeof(conc_cmd), "%s %s %s %d", conc_bin, input, conc_out, nthreads);

        printf("Execução %02d:\n", i + 1);

        double t_seq  = time_cmd(seq_cmd);
        double t_conc = time_cmd(conc_cmd);

        stats_add(&Sseq, t_seq);
        stats_add(&Sconc, t_conc);

        if (fabs(t_seq - t_conc) < 1e-6) ties++;
        else if (t_seq < t_conc) seq_wins++;
        else conc_wins++;

        printf("   Sequencial:  %.6fs\n", t_seq);
        printf("   Concorrente: %.6fs\n", t_conc);
        printf("   Resultado: %s\n\n",
               (fabs(t_seq - t_conc) < 1e-6) ? "Empate" :
               (t_seq < t_conc) ? "Sequencial venceu" : "Concorrente venceu");
    }

    double mean_seq  = stats_mean(&Sseq);
    double mean_conc = stats_mean(&Sconc);
    double sd_seq    = stats_stddev(&Sseq);
    double sd_conc   = stats_stddev(&Sconc);

    printf("------------ RESUMO ------------\n");
    printf("Sequencial:   média=%.6fs | dp=%.6fs | min=%.6fs | max=%.6fs | wins=%d\n",
           mean_seq, sd_seq, Sseq.minv, Sseq.maxv, seq_wins);
    printf("Concorrente:  média=%.6fs | dp=%.6fs | min=%.6fs | max=%.6fs | wins=%d\n",
           mean_conc, sd_conc, Sconc.minv, Sconc.maxv, conc_wins);
    printf("Empates: %d\n", ties);

    if (mean_conc > 0.0) {
        double speedup = mean_seq / mean_conc;
        printf("\nSpeedup médio = %.3fx\n", speedup);
    }

    printf("--------------------------------\n");
    printf("✅ Imagens finais geradas:\n");
    printf("   %s\n", seq_out);
    printf("   %s\n", conc_out);
    printf("==============================================\n");

    return 0;
}

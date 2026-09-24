#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "common.h"


typedef struct
{
    char *buf;
    size_t cap;
    Registro *regs;
    size_t regs_cap;

    int indice;
    Tipo tipo;
    AlgoritmoDeOrdenacao cmp;

    const char *dir;
    int np;
    long long linhas;
    long long bytes;
} Ctx;


// quebra o bloco em linhas e monta o array de registros
long long monta(Ctx *c, size_t fim)
{
    size_t n = 0;
    char *p = c->buf, *e = c->buf + fim;

    while (p < e)
    {
        char *nl = memchr(p, '\n', (size_t)(e - p));
        char *proximo;

        if (nl)
        {
            *nl = '\0';
            if (nl > p && nl[-1] == '\r')
                nl[-1] = '\0';
            proximo = nl + 1;
        }
        else
        {
            *e = '\0';
            proximo = e;
        }

        if (*p)
        {
            if (n == c->regs_cap)
            {
                size_t nova = c->regs_cap * 2;
                Registro *r = realloc(c->regs, nova * sizeof *r);
                if (!r)
                {
                    fprintf(stderr, "sem memoria para %zu registros\n", nova);
                    return -1;
                }
                c->regs = r;
                c->regs_cap = nova;
            }

            c->regs[n].linha = p;
            extrai_chave(&c->regs[n], c->indice, c->tipo);
            n++;
        }

        p = proximo;
    }

    return (long long)n;
}

// escreve o array ja ordenado num arquivo de particao
int grava_particao(Ctx *c, size_t n)
{
    char caminho[1024];
    snprintf(caminho, sizeof caminho, "%s/p%04d.csv", c->dir, c->np);

    FILE *s = fopen(caminho, "wb");
    if (!s)
    {
        perror(caminho);
        return 0;
    }

    for (size_t i = 0; i < n; i++)
    {
        fputs(c->regs[i].linha, s);
        fputc('\n', s);
    }

    long tamanho = ftell(s);
    if (ferror(s) || fclose(s))
    {
        perror(caminho);
        return 0;
    }

    printf("p%04d  %zu linhas  %.1f MB\n", c->np, n, tamanho / MEGABYTES);

    c->np++;
    c->linhas += (long long)n;
    c->bytes += tamanho;
    return 1;
}

// le um arquivo em blocos e gera uma particao por bloco
int particiona(Ctx *c, FILE *f, const char *caminho)
{
    while (1)
    {
        size_t lidos = fread(c->buf, 1, c->cap, f);
        if (lidos == 0)
            break;

        size_t fim = lidos;

        // o fread corta a ultima linha no meio, que e jogada pro proximo bloco
        if (lidos == c->cap)
        {
            while (fim > 0 && c->buf[fim - 1] != '\n') {
                fim--;
            }

            if (fim == 0)
            {
                fprintf(stderr, "%s: linha maior que o buffer\n", caminho);
                return 0;
            }
            if (fseek(f, -(long)(lidos - fim), SEEK_CUR))
            {
                perror(caminho);
                return 0;
            }
        }

        long long n = monta(c, fim);
        if (n < 0)
            return 0;
        if (n == 0)
            continue;

        qsort(c->regs, (size_t)n, sizeof *c->regs, c->cmp);
        if (!grava_particao(c, (size_t)n))
            return 0;
    }

    if (ferror(f))
    {
        perror(caminho);
        return 0;
    }
    return 1;
}

// fase 1: dos CSVs de entrada pras particoes ordenadas
int fase_particoes(
    long tamanho_mb,
    const char *caminho_saida,
    const char *coluna,
    int num_arquivos,
    const char **arquivos)
{
    if (tamanho_mb == 0)
    {
        fprintf(stderr, "mem_MB precisa ser maior que zero\n");
        return 1;
    }

    if (mkdir(caminho_saida, 0777) && errno != EEXIST)
    {
        perror(caminho_saida);
        return 1;
    }

    Ctx c = {0};
    c.cap = (size_t)tamanho_mb * MEGABYTES;
    c.regs_cap = c.cap / 100 + 1;
    c.tipo = tipo_coluna(coluna);
    c.cmp = algoritmo_de_ordenacao(c.tipo);
    c.dir = caminho_saida;
    c.indice = -1;

    // o +1 e onde cabe o \0 da ultima linha quando ela nao termina em \n
    c.buf = malloc(c.cap + 1);
    c.regs = malloc(c.regs_cap * sizeof *c.regs);
    if (!c.buf || !c.regs)
    {
        fprintf(stderr, "sem memoria para %zu MB de buffer\n", tamanho_mb);
        return 1;
    }

    // abre um arquivo de entrada por vez
    for (int i = 0; i < num_arquivos; i++)
    {
        const char *caminho = arquivos[i];

        FILE *f = fopen(caminho, "rb");
        if (!f)
        {
            perror(caminho);
            return 1;
        }

        // descarta a linha de cabecalho
        char cabecalho[MAX_LINHA];
        if (!fgets(cabecalho, sizeof cabecalho, f))
        {
            fprintf(stderr, "%s: vazio ou ilegivel\n", caminho);
            fclose(f);
            continue;
        }

        int idx = indice_no_cabecalho(coluna);
        if (idx < 0)
        {
            fprintf(stderr, "%s: nao tem a coluna \"%s\"\n", caminho, coluna);
            fclose(f);
            return 1;
        }
        if (c.indice < 0)
        {
            c.indice = idx;
        }
        else if (idx != c.indice)
        {
            fprintf(
                stderr,
                "%s: \"%s\" esta na coluna %d, era %d nos arquivos anteriores\n",
                caminho, coluna, idx, c.indice);
            fclose(f);
            return 1;
        }

        if (!particiona(&c, f, caminho))
        {
            fclose(f);
            return 1;
        }
        fclose(f);
    }

    size_t mem_indice = c.regs_cap * sizeof(Registro);

    printf("\ncoluna %s (indice %d)\n", coluna, c.indice);
    printf("%lld linhas em %d particoes\n", c.linhas, c.np);

    if (c.np) {
        printf("tamanho medio: %.1f MB\n", c.bytes / (double)c.np / MEGABYTES);
    }

    printf("memoria por particao: %.1f MB (%ld MB de texto + %.1f MB de indice)\n",
        (c.cap + mem_indice) / MEGABYTES, tamanho_mb, mem_indice / MEGABYTES);

    free(c.buf);
    free(c.regs);
    return 0;
}

int main(int argc, const char **argv)
{
    if (argc < 5) {
        fprintf(stderr, "uso: %s <coluna> <mem_MB> <saida> <entrada.csv>...\n", argv[0]);
        return 1;
    }

    const char *coluna        = argv[1];
    long         tamanho_mb   = atol(argv[2]);
    const char *caminho_saida = argv[3];
    int         num_arquivos  = argc - 4;
    const char **arquivos     = argv + 4;

    return fase_particoes(tamanho_mb, caminho_saida, coluna, num_arquivos, arquivos);
}
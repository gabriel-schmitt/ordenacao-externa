#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_LINHA 512

typedef enum { T_TXT, T_U64, T_DBL } Tipo;

// o que nao esta aqui e considerado string
static const struct { const char *nome; Tipo tipo; } TIPOS[] = {
  { "product_id",  T_U64 },
  { "category_id", T_U64 },
  { "price",       T_DBL },
  { "user_id",     T_U64 },
};

// diz se a coluna e texto, inteiro ou decimal
static Tipo tipo_da_coluna(const char *nome) {
  for (size_t i = 0; i < sizeof TIPOS / sizeof TIPOS[0]; i++)
    if (!strcmp(nome, TIPOS[i].nome)) return TIPOS[i].tipo;
  return T_TXT;
}

// acha em que posicao a coluna esta no cabecalho, ou -1 se nao tem
static int indice_no_cabecalho(const char *cabecalho, const char *coluna) {
  const char *p = cabecalho;
  size_t n = strlen(coluna);

  for (int i = 0; ; i++) {
    const char *virgula = strchr(p, ',');
    size_t len = virgula ? (size_t)(virgula - p) : strlen(p);

    if (len == n && !memcmp(p, coluna, n)) return i;
    if (!virgula) return -1;
    p = virgula + 1;
  }
}

typedef struct {
  char *linha;
  const char *chave;
  int chave_len;
  unsigned long long num;
  double dbl;
} Reg;

// compara duas chaves de texto, empate vai pro mais curto
static int cmp_txt(const void *pa, const void *pb) {
  const Reg *a = pa, *b = pb;
  int n = a->chave_len < b->chave_len ? a->chave_len : b->chave_len;
  int c = memcmp(a->chave, b->chave, (size_t)n);
  return c ? c : a->chave_len - b->chave_len;
}

// compara duas chaves inteiras
static int cmp_u64(const void *pa, const void *pb) {
  const Reg *a = pa, *b = pb;
  return (a->num > b->num) - (a->num < b->num);
}

// compara duas chaves decimais
static int cmp_dbl(const void *pa, const void *pb) {
  const Reg *a = pa, *b = pb;
  return (a->dbl > b->dbl) - (a->dbl < b->dbl);
}

typedef struct {
  char   *buf;
  size_t  cap;
  Reg    *regs;
  size_t  regs_cap;

  int     indice;
  Tipo    tipo;
  int   (*cmp)(const void *, const void *);

  const char *dir;
  int         np;
  long long   linhas;
  long long   bytes;
} Ctx;

// devolve onde comeca a coluna N da linha, e o tamanho dela
static const char *campo(const char *linha, int indice, size_t *len) {
  const char *p = linha;

  for (int i = 0; i < indice; i++) {
    p = strchr(p, ',');
    if (!p) return NULL;
    p++;
  }

  const char *virgula = strchr(p, ',');
  *len = virgula ? (size_t)(virgula - p) : strlen(p);
  return p;
}

// guarda no registro a chave ja pronta pra comparar
static void extrai_chave(Reg *r, int indice, Tipo tipo) {
  size_t len = 0;
  const char *ini = campo(r->linha, indice, &len);
  if (!ini) ini = "";

  switch (tipo) {
    // ja param na virgula, sem precisar terminar em \0
    case T_U64: r->num = strtoull(ini, NULL, 10); break;
    case T_DBL: r->dbl = strtod(ini, NULL); break;
    case T_TXT: r->chave = ini; r->chave_len = (int)len; break;
  }
}

// quebra o bloco em linhas e monta o array de registros
static long long monta(Ctx *c, size_t fim) {
  size_t n = 0;
  char *p = c->buf, *e = c->buf + fim;

  while (p < e) {
    char *nl = memchr(p, '\n', (size_t)(e - p));
    char *proximo;

    if (nl) {
      *nl = '\0';
      if (nl > p && nl[-1] == '\r') nl[-1] = '\0';
      proximo = nl + 1;
    } else {
      *e = '\0';
      proximo = e;
    }

    if (*p) {
      if (n == c->regs_cap) {
        size_t nova = c->regs_cap * 2;
        Reg *r = realloc(c->regs, nova * sizeof *r);
        if (!r) {
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
static int grava_particao(Ctx *c, size_t n) {
  char caminho[1024];
  snprintf(caminho, sizeof caminho, "%s/p%04d.csv", c->dir, c->np);

  FILE *s = fopen(caminho, "wb");
  if (!s) { perror(caminho); return 0; }

  for (size_t i = 0; i < n; i++) {
    fputs(c->regs[i].linha, s);
    fputc('\n', s);
  }

  long tamanho = ftell(s);
  if (ferror(s) || fclose(s)) { perror(caminho); return 0; }

  fprintf(stderr, "p%04d  %zu linhas  %.1f MB\n", c->np, n, tamanho / 1048576.0);

  c->np++;
  c->linhas += (long long)n;
  c->bytes += tamanho;
  return 1;
}

// le um arquivo em blocos e gera uma particao por bloco
static int particiona(Ctx *c, FILE *f, const char *caminho) {
  for (;;) {
    size_t lidos = fread(c->buf, 1, c->cap, f);
    if (lidos == 0) break;

    size_t fim = lidos;

    // o fread corta a ultima linha no meio, que e jogada pro proximo bloco
    if (lidos == c->cap) {
      while (fim > 0 && c->buf[fim - 1] != '\n') fim--;
      if (fim == 0) {
        fprintf(stderr, "%s: linha maior que o buffer\n", caminho);
        return 0;
      }
      if (fseek(f, -(long)(lidos - fim), SEEK_CUR)) { perror(caminho); return 0; }
    }

    long long n = monta(c, fim);
    if (n < 0) return 0;
    if (n == 0) continue;

    qsort(c->regs, (size_t)n, sizeof *c->regs, c->cmp);
    if (!grava_particao(c, (size_t)n)) return 0;
  }

  if (ferror(f)) { perror(caminho); return 0; }
  return 1;
}

// imprime como se usa o programa
static void uso(const char *prog) {
  fprintf(
    stderr,
    "uso: %s particoes <coluna> <mem_MB> <dir_saida> <entrada.csv>...\n"
    "     %s merge <coluna> <saida.csv> <particao.csv>...\n",
    prog, prog
  );
}

// fase 1: dos CSVs de entrada pras particoes ordenadas
static int fase_particoes(int argc, char **argv) {
  if (argc < 6) { uso(argv[0]); return 1; }

  int i = 2;
  char *coluna = argv[i++];
  size_t tamanho_mb = strtoul(argv[i++], NULL, 10);
  char *caminho_saida = argv[i++];

  if (tamanho_mb == 0) {
    fprintf(stderr, "mem_MB precisa ser maior que zero\n");
    return 1;
  }

  if (mkdir(caminho_saida, 0777) && errno != EEXIST) {
    perror(caminho_saida);
    return 1;
  }

  Ctx c = { 0 };
  c.cap = (size_t)tamanho_mb * 1024 * 1024;
  c.regs_cap = c.cap / 100 + 1;
  c.tipo = tipo_da_coluna(coluna);
  c.cmp = c.tipo == T_TXT ? cmp_txt : c.tipo == T_U64 ? cmp_u64 : cmp_dbl;
  c.dir = caminho_saida;
  c.indice = -1;

  // o +1 e onde cabe o \0 da ultima linha quando ela nao termina em \n
  c.buf = malloc(c.cap + 1);
  c.regs = malloc(c.regs_cap * sizeof *c.regs);
  if (!c.buf || !c.regs) {
    fprintf(stderr, "sem memoria para %zu MB de buffer\n", tamanho_mb);
    return 1;
  }

  // abre um arquivo de entrada por vez
  while (i < argc) {
    char *caminho = argv[i++];

    FILE *f = fopen(caminho, "rb");
    if (!f) { perror(caminho); return 1; }

    // limpa a linha de cabecalho
    char cabecalho[MAX_LINHA];
    if (!fgets(cabecalho, sizeof cabecalho, f)) {
      fprintf(stderr, "%s: vazio ou ilegivel\n", caminho);
      fclose(f);
      continue;
    }

    // tira o \n do cabecalho
    cabecalho[strcspn(cabecalho, "\r\n")] = '\0';

    int idx = indice_no_cabecalho(cabecalho, coluna);
    if (idx < 0) {
      fprintf(stderr, "%s: nao tem a coluna \"%s\"\n", caminho, coluna);
      fclose(f);
      return 1;
    }
    if (c.indice < 0) {
      c.indice = idx;
    } else if (idx != c.indice) {
      fprintf(
        stderr,
        "%s: \"%s\" esta na coluna %d, era %d nos arquivos anteriores\n",
        caminho, coluna, idx, c.indice
      );
      fclose(f);
      return 1;
    }

    if (!particiona(&c, f, caminho)) { fclose(f); return 1; }
    fclose(f);
  }

  size_t mem_indice = c.regs_cap * sizeof(Reg);

  fprintf(stderr, "\ncoluna %s (indice %d)\n", coluna, c.indice);
  fprintf(stderr, "%lld linhas em %d particoes\n", c.linhas, c.np);
  if (c.np)
    fprintf(
      stderr,
      "tamanho medio: %.1f MB\n",
      c.bytes / (double)c.np / 1048576.0
    );
  fprintf(
    stderr,
    "memoria por particao: %.1f MB (%zu MB de texto + %.1f MB de indice)\n",
    (c.cap + mem_indice) / 1048576.0, tamanho_mb, mem_indice / 1048576.0
  );

  free(c.buf);
  free(c.regs);
  return 0;
}

// fase 2: das particoes ordenadas pro arquivo unico
static int fase_merge(int argc, char **argv) {
  if (argc < 5) { uso(argv[0]); return 1; }

  fprintf(stderr, "merge: ainda nao implementado\n");
  return 1;
}

int main(int argc, char **argv) {
  if (argc >= 2 && !strcmp(argv[1], "particoes")) return fase_particoes(argc, argv);
  if (argc >= 2 && !strcmp(argv[1], "merge")) return fase_merge(argc, argv);

  uso(argv[0]);
  return 1;
}

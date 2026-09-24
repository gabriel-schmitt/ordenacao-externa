#if !defined(__common_h__)
#define __common_h__

#include <stdio.h>

#define MAX_LINHA 512
#define MEGABYTES (1024.0 * 1024.0)

// colunas fixas dos CSVs de origem (2019-Oct.csv / 2019-Nov.csv), na ordem
const char *COLUNAS[] = {
    "event_time",
    "event_type",
    "product_id",
    "category_id",
    "category_code",
    "brand",
    "price",
    "user_id",
    "user_session",
};

#define N_COLUNAS (sizeof COLUNAS / sizeof COLUNAS[0])

typedef enum
{
    TIPO_COLUNA_STRING,
    TIPO_COLUNA_LONG,
    TIPO_COLUNA_DOUBLE
} Tipo;

typedef int (*AlgoritmoDeOrdenacao)(const void *, const void *);

typedef struct
{
    char *linha;
    const char *chave;
    int chave_len;
    unsigned long long num;
    double dbl;
} Registro;

// compara duas chaves de texto, empate vai pro mais curto
int compara_string(const void *pa, const void *pb)
{
    const Registro *a = pa, *b = pb;
    int n = a->chave_len < b->chave_len ? a->chave_len : b->chave_len;
    int c = memcmp(a->chave, b->chave, (size_t)n);
    return c ? c : a->chave_len - b->chave_len;
}

// compara duas chaves inteiras
int compara_long(const void *pa, const void *pb)
{
    const Registro *a = pa, *b = pb;
    return (a->num > b->num) - (a->num < b->num);
}

// compara duas chaves decimais
int compara_double(const void *pa, const void *pb)
{
    const Registro *a = pa, *b = pb;
    return (a->dbl > b->dbl) - (a->dbl < b->dbl);
}

// escolhe o algoritmo de ordenação adequado dependendo do tipo
AlgoritmoDeOrdenacao algoritmo_de_ordenacao(Tipo tipo)
{
    if (tipo == TIPO_COLUNA_STRING) return compara_string;
    if (tipo == TIPO_COLUNA_DOUBLE) return compara_double;
    if (tipo == TIPO_COLUNA_LONG) return compara_long;

    return NULL;
}

// deduz o tipo de coluna a partir das colunas existentes
Tipo tipo_coluna(const char *nome_coluna)
{
    if (strcmp(nome_coluna, "product_id") == 0) return TIPO_COLUNA_LONG;
    if (strcmp(nome_coluna, "category_id") == 0) return TIPO_COLUNA_LONG;
    if (strcmp(nome_coluna, "price") == 0) return TIPO_COLUNA_DOUBLE;
    if (strcmp(nome_coluna, "user_id") == 0) return TIPO_COLUNA_LONG;

    return TIPO_COLUNA_STRING;
}

// acha em que posicao a coluna esta na lista fixa COLUNAS, ou -1 se nao tem
int indice_no_cabecalho(const char *coluna)
{
    for (size_t i = 0; i < N_COLUNAS; i++)
    {
        if (strcmp(coluna, COLUNAS[i]) == 0) {
            return (int)i;
        }
    }

    return -1;
}


// devolve onde comeca a coluna N da linha, e o tamanho dela
const char *campo(const char *linha, int indice, size_t *len)
{
    const char *p = linha;

    for (int i = 0; i < indice; i++)
    {
        p = strchr(p, ',');
        if (!p) {
            return NULL;
        }

        p++;
    }

    const char *virgula = strchr(p, ',');
    *len = virgula ? (size_t)(virgula - p) : strlen(p);
    return p;
}

// guarda no registro a chave ja pronta pra comparar
void extrai_chave(Registro *r, int indice, Tipo tipo)
{
    size_t len = 0;
    const char *ini = campo(r->linha, indice, &len);
    if (!ini)
        ini = "";

    switch (tipo)
    {
    // ja param na virgula, sem precisar terminar em \0
    case TIPO_COLUNA_LONG:
        r->num = strtoull(ini, NULL, 10);
        break;
    case TIPO_COLUNA_DOUBLE:
        r->dbl = strtod(ini, NULL);
        break;
    case TIPO_COLUNA_STRING:
        r->chave = ini;
        r->chave_len = (int)len;
        break;
    }
}

#endif // __common_h__

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "common.h"

// Uma particao que ainda esta sendo lida durante a intercalacao.
// So guardamos a linha atual dela (nao o arquivo inteiro), com a chave
// ja extraida, pronta pra comparar com as outras particoes.
typedef struct
{
    FILE     *arquivo;
    char      linha[MAX_LINHA]; // buffer fixo: nenhuma linha do CSV passa disso
    Registro  registro;
    int       tem_linha;
} Particao;

// tira o \n (e o \r, se tiver) do fim da linha, um caractere de cada vez,
// ate sobrar so o conteudo de verdade.
void remove_newline(char *linha, size_t *tamanho)
{
    while (1)
    {
        if (*tamanho == 0) break;

        char c = linha[*tamanho - 1];

        if (c != '\n' && c != '\r') break;

        linha[*tamanho - 1] = '\0';
        (*tamanho)--;
    }
}

// le a proxima linha dessa particao e ja extrai a chave de ordenacao.
// devolve 1 se leu uma linha nova, ou 0 se a particao ja acabou (fim de arquivo).
int particao_avanca(Particao *particao, int indice_coluna, Tipo tipo)
{
    if (fgets(particao->linha, sizeof particao->linha, particao->arquivo) == NULL)
    {
        particao->tem_linha = 0;
        return 0;
    }

    size_t tamanho = strlen(particao->linha);

    // se nao achou \n e nao terminou por causa do fim do arquivo, a linha
    // era maior que MAX_LINHA e foi cortada -- melhor avisar do que seguir
    // com dado incompleto
    // int achou_fim_de_linha = (tamanho > 0 && particao->linha[tamanho - 1] == '\n');
    // int chegou_no_fim_do_arquivo = feof(particao->arquivo);
    // if (!achou_fim_de_linha && !chegou_no_fim_do_arquivo)
    // {
    //     fprintf(stderr, "linha maior que MAX_LINHA (%d)\n", MAX_LINHA);
    //     particao->tem_linha = 0;
    //     return 0;
    // }

    remove_newline(particao->linha, &tamanho);

    particao->registro.linha = particao->linha;
    extrai_chave(&particao->registro, indice_coluna, tipo);
    particao->tem_linha = 1;
    return 1;
}

void libera_particoes(Particao *particoes, int num_particoes)
{
    for (int i = 0; i < num_particoes; i++)
    {
        if (particoes[i].arquivo) {
            fclose(particoes[i].arquivo);
        }
        free(particoes[i].linha);
    }
    free(particoes);
}

int fase_merge(
    const char *coluna,
    const char *caminho_saida,
    int num_caminho_particoes,
    const char **caminho_particoes)
{
    int indice = indice_no_cabecalho(coluna);
    if (indice < 0)
    {
        fprintf(stderr, "coluna \"%s\" desconhecida (veja CABECALHO_PADRAO)\n", coluna);
        return 1;
    }

    Tipo tipo = tipo_coluna(coluna);
    AlgoritmoDeOrdenacao cmp = algoritmo_de_ordenacao(tipo);

    Particao *particoes = malloc(num_caminho_particoes * sizeof (Particao));
    if (!particoes)
    {
        fprintf(stderr, "sem memoria para %d fontes\n", num_caminho_particoes);
        return 1;
    }

    int abertas = 0;
    for (int i = 0; i < num_caminho_particoes; i++)
    {
        particoes[i].arquivo = fopen(caminho_particoes[i], "rb");

        if (!particoes[i].arquivo)
        {
            perror(caminho_particoes[i]);
            libera_particoes(particoes, num_caminho_particoes);
            return 1;
        }

        if (particao_avanca(&particoes[i], indice, tipo)) {
            abertas++;
        }
    }

    FILE *saida = fopen(caminho_saida, "wb");
    if (!saida)
    {
        perror(caminho_saida);
        libera_particoes(particoes, num_caminho_particoes);
        return 1;
    }

    long long linhas_escritas = 0;
    long long comparacoes = 0;

    while (abertas > 0)
    {
        int melhor = -1;

        for (int i = 0; i < num_caminho_particoes; i++)
        {
            if (!particoes[i].tem_linha) {
                continue;
            }

            if (melhor < 0)
            {
                melhor = i;
                continue;
            }

            comparacoes++;

            if (cmp(&particoes[i].registro, &particoes[melhor].registro) < 0) {
                melhor = i;
            }
        }

        fputs(particoes[melhor].registro.linha, saida);
        fputc('\n', saida);
        linhas_escritas++;

        if (!particao_avanca(&particoes[melhor], indice, tipo)) {
            abertas--;
        }
    }

    int falhou = ferror(saida) || fclose(saida);
    if (falhou) {
        perror(caminho_saida);
    }

    printf("\ncoluna %s (indice %d)\n", coluna, indice);
    printf("%d particoes intercaladas em 1 passo (algoritmo base, sem arquivos intermediarios)\n",
        num_caminho_particoes);
    printf("%lld linhas escritas, %lld comparacoes\n", linhas_escritas, comparacoes);

    libera_particoes(particoes, num_caminho_particoes);
    return falhou ? 1 : 0;
}

int main(int argc, const char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "uso: %s <coluna> <saida.csv> <particao.csv>...\n", argv[0]);
        return 1;
    }

    const char  *coluna                = argv[1];
    const char  *caminho_saida         = argv[2];
    int          num_caminho_particoes = argc - 3;
    const char **caminho_particoes     = argv + 3;

    return fase_merge(coluna, caminho_saida, num_caminho_particoes, caminho_particoes);
}
# Ordenação externa - geração de um arquivo único ordenado

Ordenar um conjunto de dados que não cabe na memória principal, em três etapas:
dividir em partições, ordenar cada partição, intercalar as partições ordenadas.

Dois programas, um por etapa:

```
make
./particoes user_session 512 ./parts 2019-Oct.csv 2019-Nov.csv
./merge user_session saida.csv ./parts/*.csv
```

`particoes` lê cada CSV em blocos do tamanho dado em MB, ordena cada bloco com
`qsort` e grava um arquivo por bloco. `merge` abre todas as partições e escreve,
uma linha por vez, a menor chave entre as que ainda restam em cada uma - o
"algoritmo base" visto em aula: sem árvore de vencedores e sem passos
intermediários, porque o número de partições deste trabalho é pequeno o
suficiente pra não justificar a complexidade extra.

Estado: as duas etapas implementadas e testadas.

## Os dados

Dois CSVs de comportamento de e-commerce (out/2019 a abr/2020), 14,68 GB no
total, 9 colunas, originalmente ordenados por `event_time`. Cerca de
110 milhões de registros.

https://www.kaggle.com/datasets/mkechinov/ecommerce-behavior-data-from-multi-category-store/data

## Coluna de ordenação

A coluna de ordenação é fornecida pela usuário mediante execução do programa.
A coluna deve ser uma das colunas identificadas e estabelecidas como válidas
pelo programa. Cada coluna conta com um tipo de ordenação diferente, seguindo
a ordem numérica esperada no caso de colunas de tipo inteiro e real, e seguindo
uma ordenação lexicográfica no caso de colunas de valor textual. 

# Algoritmos usados

1. Partições

Técnica usada: leitura da entrada em blocos de tamanho fixo (configurável em MB), ordenação de cada bloco em memória com qsort (quicksort), e gravação do bloco já ordenado como um arquivo de partição. Cada bloco lido gera exatamente uma partição — não há técnica de seleção por torneio (replacement selection).

2. Intercalação

Técnica usada: algoritmo básico. Mantemos uma única linha carregada por partição (a "cabeça" dela). 
A cada iteração percorremos linearmente essas cabeças pra achar a menor chave,
escrevemos essa linha no arquivo final e avançamos só a partição vencedora.

Descartamos a árvore de vencedores e a intercalação balanceada de N caminhos
porque o número de partições deste trabalho é pequeno o suficiente pra não
justificar a complexidade extra delas (a árvore reduziria o custo por linha de
O(n) pra O(log n), e a balanceada resolve um limite de arquivos abertos
simultaneamente que não é um problema aqui).

A intercalação ocorre em um único passo. Todas as N partições são abertas e
lidas diretamente, e a mescla é gravada de uma vez só no arquivo final, sem haver
passos intermediários nem arquivos temporários gerados no meio do caminho
(0 passos intermediários, 1 passo final).

## Uso de IA

Usamos o Claude Sonnet 5 da Anthropic como apoio de implementação ao longo do trabalho,
principalmente nestas partes:

- Discussão a respeito do custo de cada técnica vista em aula com base na escala do trabalho real;
- Ajuda na implementação da leitura estruturada dos arquivos;
- Ajuda para correção de bugs a respeito de tratamento de strings;
- Escrita do parse dos parâmetros de entradas da função `main`;
- Escrita do `Makefile`;

Toda decisão de algoritmo, formato de dados e critério de correção foi nossa;
a IA funcionou como par de programação para escrever, revisar e testar trechos
de código C a partir dessas decisões.
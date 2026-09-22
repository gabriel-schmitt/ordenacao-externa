# Ordenação externa - geração de um arquivo único ordenado

Ordenar um conjunto de dados que não cabe na memória principal, em três etapas:
dividir em partições, ordenar cada partição, intercalar as partições ordenadas.

Um programa só, com dois modos:

```
make
./ordena particoes user_session 512 ./parts 2019-Oct.csv 2019-Nov.csv
./ordena merge user_session saida.csv ./parts/*.csv
```

O modo `particoes` lê cada CSV em blocos do tamanho dado em MB, ordena cada bloco
com `qsort` e grava um arquivo por bloco. O modo `merge` abre todas as partições e
escolhe a menor linha entre elas, uma por vez, até o arquivo único.

Estado: `particoes` funcionando e validado, `merge` ainda não implementado.

## Os dados

Dois CSVs de comportamento de e-commerce (out/2019 a abr/2020), 14,68 GB no
total, 9 colunas, originalmente ordenados por `event_time`.

https://www.kaggle.com/datasets/mkechinov/ecommerce-behavior-data-from-multi-category-store/data

Os CSVs não estão versionados. São ~110 milhões de registros, 133 bytes por linha
em média.

## Coluna de ordenação

`user_session` - UUID de 36 caracteres, poucos valores repetidos, e diferente de
`event_time` como o enunciado exige.

A coluna é um argumento, não uma constante: o índice sai do cabeçalho do próprio
arquivo em tempo de execução e o tipo sai de uma tabela de exceções. Coluna que
não está na tabela é ordenada como texto.

`price` é comparada como `double`, porque preço tem grandeza - ordem
lexicográfica colocaria `35.79` antes de `9.50`. Os IDs são comparados como
`unsigned long long` porque têm larguras diferentes entre si, e aí `"99"`
ordenaria depois de `"100"`. `category_id` tem 19 dígitos e não caberia num
`double` sem perder precisão.

## O que descrever no AVA

**Partições:** técnica(s) usada(s), quantas partições, tamanho médio,
quanta memória principal por partição.

**Intercalação:** técnica(s) usada(s), quantos passos de intercalação
(iniciais e intermediários).

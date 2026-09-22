# Ordenação externa — geração de um arquivo único ordenado

Ordenar um conjunto de dados que não cabe na memória principal, em três etapas:
dividir em partições, ordenar cada partição, intercalar as partições ordenadas.

Dois programas, como pede a entrega (máximo 2 arquivos de código):

- `particoes` — lê o CSV e grava as partições já ordenadas
- `intercalacao` — lê as partições ordenadas e grava o arquivo único

## Compilar e rodar

```
make
./particoes
./intercalacao
```

## Os dados

Dois CSVs de comportamento de e-commerce (out/2019 a abr/2020), 14,68 GB no
total, 9 colunas, originalmente ordenados por `event_time`.

https://www.kaggle.com/datasets/mkechinov/ecommerce-behavior-data-from-multi-category-store/data

Os CSVs não estão versionados.

## Coluna de ordenação

Precisa ser diferente de `event_time` e ter poucos valores repetidos.
Candidatas: `user_session`, `price`, `user_id`.

## O que descrever no AVA

**Partições:** técnica(s) usada(s), quantas partições, tamanho médio,
quanta memória principal por partição.

**Intercalação:** técnica(s) usada(s), quantos passos de intercalação
(iniciais e intermediários).

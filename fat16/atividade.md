RELATORIO_ESTUDO.md (12 seções)

Relatório completo e detalhado do projeto
Estruturas de dados, layout FAT16, todas as 7 operações
Detalhes técnicos e perguntas frequentes
ROTEIRO_APRESENTACAO.md (7 partes)

Roteiro estruturado para apresentação ao professor (15-20 min)
Exemplos práticos, demonstração ao vivo
Respostas prontas para perguntas comuns
PERGUNTAS_TECNICAS.md (3 níveis)

Perguntas fundamentais (obrigatório dominar)
Perguntas intermediárias (muito importante)
Perguntas avançadas (para impressionar)
Desafios práticos e testes rápidos
GUIA_ESTUDO.md (Índice + Planejamento)

Guia completo de estudo com cronograma
Plano de 2 horas de preparação
Checklist pré-apresentação
Dicas e ordem de leitura recomendada
DIAGRAMAS_VISUAIS.md (Referência rápida)

Diagramas ASCII do layout FAT16
Tabelas de valores e formatos
Fluxogramas das operações
Resumo de 1 página
Implementação de Funções de Manipulação de Sistema de Arquivos FAT16 (grupo de até 03 alunos)
Objetivo:
Neste trabalho vocês deverão desenvolver um programa em linguagem C/C++ para manipular um sistema de
arquivos FAT16. A execução final, a ser postada, deve ocorrer exclusivamente na máquina disponibilizada pelo
professor no GitHub Codespaces. O objetivo principal é implementar operações básicas para gerenciamento
de arquivos, possibilitando, por exemplo, a criação, leitura, escrita e remoção de arquivos. Observação: por
questões de simplicidade, não consideraremos a criação e manipulação de subdiretórios.
Requisitos:
Implementação de Operações Fundamentais: O programa deve ser capaz de realizar as seguintes operações
básicas sobre o sistema de arquivos FAT16:
Listar o conteúdo do disco: exibir em uma lista os nomes dos arquivos (e seus respectivos tamanhos)
existentes no diretório raiz.
Listar o conteúdo de um arquivo: mostrar (pode ser na tela) o conteúdo de um arquivo do diretório raiz.
Exibir os atributos de um arquivo: mostrar data/hora da criação/última modificação e os seguintes atributos:
se é somente leitura; se é oculto; se é arquivo de sistema
Renomear um arquivo: trocar o nome de um arquivo existente
Inserir/criar um novo arquivo: permitir que se armazene no diretório raiz um novo arquivo externo.
Apagar/remover um arquivo: apagar um arquivo do diretório raiz.
Todas as operações devem ser realizadas sobre um arquivo que contenha uma imagem de disco formatado
como FAT16 (um exemplo de arquivo está disponibilizado no AVA Univali). O programa deve ser capaz de ler essa
imagem, interpretar sua estrutura e realizar as operações de acordo com as especificações do sistema de
arquivos FAT16. Observação: a solução dada deverá funcionar para qualquer arquivo contendo imagem de disco
no formato FAT16, sem considerar a existência de subdiretórios.
O programa deverá oferecer um menu que permita realizar cada uma das operações sem que precise ser
reiniciado.
Avaliação:
A avaliação do trabalho será baseada nos seguintes critérios:
Número de Operações Implementadas: Será avaliado o número de operações sobre arquivos que foram
implementadas, atribuindo-se os seguintes pesos as realizadas:
Listar o conteúdo do disco 10%
Listar o conteúdo de um arquivo 10%
Exibir os atributos de um arquivo 10%
Renomear um arquivo 10%
Apagar/remover um arquivo 15%
Inserir/criar um novo arquivo 35%
Facilidade de uso 15%
Cada uma das operações será avaliada de acordo com a sua corretude: As operações devem ser
implementadas de forma correta, seguindo as especificações do sistema de arquivos FAT16 e atualizando as
estruturas correspondentes de forma correta.
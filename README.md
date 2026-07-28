<div align="center">
  <img src="Assets/NoteMD-AppIcon-1024.png" alt="Ícone do NoteMD" width="180">

  # NoteMD

  **Um editor Markdown nativo, elegante e organizado para macOS.**

  Escreve, organiza e guarda as tuas ideias em ficheiros que continuam a ser teus.
</div>

## Escrever sem distrações

O NoteMD combina a simplicidade de um bloco de notas com as possibilidades do
Markdown. Podes escrever diretamente em Markdown, trabalhar num editor visual
ou utilizar os dois lado a lado.

O editor inclui uma barra de formatação completa para títulos, negrito,
itálico, texto rasurado, listas, listas numeradas, tarefas, citações, ligações,
separadores, tabelas, código inline e blocos de código.

## Organizar com notebooks

Agrupa as notas em notebooks expansíveis e atribui cores aos notebooks e às
notas para os encontrares rapidamente. Os nomes podem ser alterados diretamente
na barra lateral e as respetivas pastas acompanham automaticamente a alteração.

Várias notas podem permanecer abertas ao mesmo tempo em separadores no topo da
janela, permitindo alternar entre conteúdos sem perder espaço de trabalho.

## Markdown e edição visual

O NoteMD oferece três formas de trabalhar:

- editor Markdown com realce de sintaxe;
- editor visual por blocos;
- vista dividida para editar e acompanhar o resultado lado a lado.

O editor apresenta ainda a contagem de palavras, caracteres e linhas da nota.

## Imagens e conteúdo avançado

É possível colar imagens diretamente com `⌘V`. Cada imagem é guardada na pasta
`assets` da respetiva nota e a referência Markdown é inserida automaticamente.

As notas também podem incluir:

- tabelas;
- listas de tarefas;
- blocos de código com linguagem;
- citações e ligações;
- listas e separadores;
- texto colorido;
- imagens locais.

## Ficheiros locais e portáteis

No primeiro arranque, o NoteMD pede uma pasta onde guardar a biblioteca. Cada
nota é armazenada como um ficheiro Markdown individual, juntamente com os seus
recursos:

```text
Pasta das notas/
└── Notebook/
    └── Nota/
        ├── note.md
        ├── .note.json
        └── assets/
            └── imagem.png
```

Esta estrutura é aberta e portátil: os ficheiros `note.md` podem ser lidos e
editados por qualquer outro editor Markdown.

## Guardar e exportar

As alterações podem ser guardadas através do botão dedicado ou com `⌘S`. Ao
fechar uma nota ou sair da aplicação, o NoteMD pergunta se pretendes guardar
alterações pendentes.

Qualquer nota pode também ser exportada diretamente para PDF.

## Idiomas

A interface está disponível em:

- Português;
- English;
- Français.

## Privacidade

As notas e imagens permanecem localmente na pasta escolhida. O NoteMD não exige
conta e não envia o conteúdo das notas para servidores externos.

## Compatibilidade

O NoteMD é uma aplicação nativa para macOS, construída em Swift com SwiftUI e
AppKit.

# Arquitetura do NoteMD Linux

A aplicação é composta por unidades de tradução C17 independentes. Cada ficheiro `.c` é compilado
para um objeto em `build/` e ligado no executável final. `main.c` contém apenas o ponto de entrada.

## Módulos

- `main.c`: argumentos de linha de comandos e ciclo de vida do processo.
- `core.c`: estado global, localização, símbolos GTK carregados por `dlsym`, preferências e temas.
- `library.c`: armazenamento, notebooks, notas, metadados, recuperação, imagens e arquivos ZIP.
- `export.c`: parser Markdown, preview, HTML, PDF, DOCX, histórico e pesquisa.
- `editor.c`: editor visual, blocos, formatação e sincronização entre modos.
- `ui.c`: ações, menus, atalhos, janela principal e testes de interface.
- `tests.c`: testes de armazenamento, configuração e exportação.
- `internal.h`: tipos e API privada partilhada entre os módulos.

Os headers `markdown.h`, `pdf.h`, `preview.h`, `source-editor.h` e `markdown-tests.h` contêm
implementações especializadas pertencentes a uma única unidade de tradução; não devem ser incluídos
noutro módulo sem rever a propriedade dos símbolos.

## Regras de dependência

1. Todo ficheiro `.c` inclui primeiro `internal.h`.
2. Estado partilhado vive em `State`; o único objeto global é definido em `core.c`.
3. Detalhes exclusivos de um módulo permanecem `static`.
4. Funções usadas por outro módulo são declaradas em `internal.h` e compiladas com
   `-Wmissing-prototypes` e `-Werror=implicit-function-declaration`.
5. O GTK, GtkSourceView, Cairo e Pango continuam carregados dinamicamente; os respetivos ponteiros
   são definidos em `core.c` e declarados no header interno.
6. Novos módulos devem ser adicionados a `SOURCES` no `Makefile`.

## Qualidade do build

O build usa `-ffunction-sections`, `-fdata-sections` e `--gc-sections`. Funções privadas não usadas
são reportadas por `-Wall -Wextra`; secções globais inacessíveis são removidas pelo linker. O alvo
`check` compila e executa todos os testes não gráficos.

# NoteMD para Linux (GTK 4)

Implementação nativa compilada em C17 com GTK 4 para Linux.

## Compilar e Executar

A aplicação usa GTK 4, GtkSourceView 5 (>= 5.4) e MD4C do sistema, sem serviços adicionais.
O pacote Debian declara estas dependências (`libgtk-4-1`, `libgtksourceview-5-0`, `libmd4c0`).

```sh
make
./build/notemd
```

Para instalar no menu de aplicações do utilizador com o ícone oficial:

```sh
make install-user
```

Para criar e instalar o pacote Debian/Ubuntu (`.deb`):

```sh
make deb
sudo apt install ./build/notemd_1.3.0_amd64.deb
```

## Importar e exportar notebooks

No menu de contexto de um notebook, escolha **Exportar notebook…** para criar um arquivo
`.notemd.zip`. O arquivo inclui todas as notas, metadados, ordem, cores, tags e assets do notebook.

Para restaurar ou transferir um notebook, abra o menu da aplicação e escolha **Importar notebook…**.
Se já existir um notebook com o mesmo nome, a aplicação cria automaticamente um nome único sem
substituir os dados existentes. A importação requer o comando `unzip`; a exportação requer `zip`.

## Diretório de Notas

Por predefinição, a aplicação usa `~/Documents/NoteMD`. Para especificar um diretório personalizado:

```sh
NOTEMD_NOTES_DIR=/caminho/para/notas ./build/notemd
```

## Testes

```sh
./build/notemd --self-test
./build/notemd --settings-self-test
./build/notemd --export-self-test
./build/notemd --markdown-self-test
python3 tools/test-pdf.py
./build/notemd --ui-self-test
./build/notemd --empty-ui-self-test
```

Os testes de interface precisam de uma sessão gráfica GTK 4. Os restantes podem ser executados sem ecrã.
O teste de PDF usa as ferramentas Poppler apenas para verificação: `pdfinfo`, `pdftotext`, `pdfimages` e `pdftohtml`.

## Markdown e PDF

O editor e o modo dividido usam GtkSourceView para colorir títulos, ênfase, ligações, listas, tarefas,
rasurado e código. As cores acompanham cada tema. Blocos com linguagem indicada, como
`python`, `js`, `typescript`, `c`, `cpp`, `bash`, `json`, `css`, `html`, `sql`, `rust` e `go`, têm
destaque de sintaxe próprio. A coloração é apenas visual e não altera o texto guardado.

No modo visual, o código aparece numa caixa com fundo contrastante, margem de destaque e
campo de linguagem. A pré-visualização apresenta também a linguagem e o destaque de sintaxe,
com fundo contínuo mesmo nas linhas vazias. Títulos, ênfase, listas e ligações usam cores
coordenadas com o tema; mudar entre claro e escuro atualiza também os blocos já abertos.

A pré-visualização e as exportações HTML/PDF partilham o parser MD4C, com listas aninhadas,
formatação combinada, tabelas alinhadas, tarefas, ligações por referência e blocos de código.

No botão **Exportar → PDF**, escolha o destino para exportar o conteúdo atual, incluindo alterações
ainda não guardadas. O documento usa A4, margens de impressão, títulos hierárquicos, tabelas com
cabeçalhos repetidos, imagens locais, citações, código e números de página. O texto é selecionável,
as ligações externas são clicáveis e os títulos aparecem nos marcadores do PDF.
O PDF usa uma apresentação clara independente do tema da aplicação. Imagens indisponíveis são
identificadas no documento e na mensagem de exportação; imagens remotas não são descarregadas.

Também é possível exportar sem abrir uma janela:

```sh
./build/notemd --export-pdf examples/markdown-demo.md /tmp/exemplo.pdf "A minha nota"
```

As paletas de sintaxe e a extensão da gramática encontram-se em `syntax/` e podem ser regeneradas
com `python3 tools/generate-syntax.py`.

## Leitura e navegação

A fonte do editor começa em 16 px. Pode ser ajustada entre 12 e 24 px em **Preferências → Tamanho do texto**;
a escolha é aplicada ao editor, aos blocos visuais e à pré-visualização e fica guardada para a próxima sessão.

| Atalho | Ação |
| --- | --- |
| `Ctrl++` ou `Ctrl+=` | Aumentar o texto |
| `Ctrl+-` | Diminuir o texto |
| `Ctrl+0` | Repor 16 px |
| `Ctrl+1` | Editor Markdown |
| `Ctrl+2` | Editor visual |
| `Ctrl+3` | Editor e pré-visualização |
| `Ctrl+B` | Mostrar ou ocultar a biblioteca |

O tamanho da fonte altera a apresentação da aplicação; os ficheiros Markdown e as exportações mantêm a sua formatação.

As tabs ficam na barra de título, junto ao botão da biblioteca, com fecho e indicador de alterações.
O scroll horizontal permite abrir várias notas sem aumentar a largura mínima da janela.
O nome fica estático até fazer duplo clique sobre ele. Enter ou sair do campo termina a edição;
as alterações seguem a preferência de gravação automática.

A barra de formatação agrupa estilo do texto, negrito/itálico/código/cor, listas e inserções.
O menu de estilo apresenta Parágrafo e Título 1–6, com prévias de tamanho e o nível da linha atual.
Escolher um nível substitui o título da linha ou das linhas selecionadas; Parágrafo remove os marcadores.
Negrito, itálico, rasurado e código mantêm o texto selecionado; repetir a ação retira os marcadores.
Sem seleção, o cursor fica entre os marcadores, pronto para escrever. A formatação também usa
a seleção do editor no modo dividido. O ícone de cor abre a paleta. Em janelas estreitas, a barra
permite deslocamento horizontal sem impor a sua largura à janela. A pega no canto inferior direito
permite redimensionar a janela com o rato, além das bordas nativas.
A coluna dos números de linha partilha o scroll do editor sem impor a altura do documento à janela.
O teste de interface compara a altura mínima com uma linha e com 2000 linhas nos três modos de edição.

## Temas e ícone

As preferências mostram amostras das paletas **Claro**, **Escuro**, **Sistema**, **Monokai**, **Tokyo Night**,
**Dracula**, **Solarized Light** e **Catppuccin Latte**. Cada paleta distingue a página de escrita, a biblioteca e as barras da aplicação.
As cores do Markdown e da pré-visualização acompanham o tema escolhido.
Catppuccin Latte usa a [paleta oficial clara do Catppuccin](https://catppuccin.com/palette/),
com ajustes de contraste no texto de sintaxe. Também está incluído na instalação e no pacote Debian.

Na biblioteca, a cor atribuída a cada caderno ou nota aparece no próprio título, incluindo na seleção.
Os tons saturados acompanham o tema para manter a leitura, tanto nos títulos como nos ícones.
Os nomes usam Noto Sans, com alternativa DejaVu Sans, independentemente da fonte do ambiente de trabalho.
Ícones distinguem cadernos e notas, as ações aparecem
ao passar o rato ou navegar com o teclado, e as contagens de tags ficam alinhadas à direita.
O painel usa uma superfície derivada da paleta ativa, com seleção neutra. Os menus da biblioteca e
da aplicação também usam realces neutros ao passar o rato, clicar ou navegar com o teclado.
O menu de contexto da biblioteca tem uma superfície neutra própria, sem degradês do ambiente de trabalho,
abre junto ao ponto clicado e mostra cores sólidas com indicação da cor escolhida. A aplicação arranca com a
biblioteca oculta e os cadernos recolhidos; o botão da biblioteca ou `Ctrl+B` mostra o painel.

O ícone de Linux é desenhado em [SVG](assets/pt.notemd.NoteMD.svg), com margem transparente e versões PNG
de 32 a 1024 px. A instalação usa o nome `pt.notemd.NoteMD` no tema de ícones do sistema.
Para voltar a gerar os PNG a partir do SVG, com Python GI, librsvg e pycairo instalados:

```sh
python3 tools/render-icons.py
```

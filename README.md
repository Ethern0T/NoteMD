<div align="center">
  <img src="Assets/NoteMD-AppIcon-1024.png" alt="Ícone do NoteMD" width="140">

  # NoteMD

  Editor Markdown local para macOS — versão 1.2. A versão nativa GTK 4 para Linux
  está agora em desenvolvimento em [`linux/`](linux/README.md).
</div>

## Funções

- Notas organizadas em notebooks, separadores, tags e cores.
- Editor Markdown, editor visual e vista dividida com pré-visualização.
- Temas Sistema, Monokai, Tokyo Night, Dracula e Solarized Light.
- Formatação de títulos, listas, tarefas, citações, tabelas, ligações e código.
- Pesquisa global e pesquisa/substituição dentro da nota.
- Ligações `[[entre notas]]` e backlinks.
- Abertura e edição direta de ficheiros `.md` externos.
- Deteção de alterações feitas noutras aplicações.
- Drag-and-drop de Markdown e imagens.
- Movimento e ordenação manual de notas.
- Gravação automática, recuperação e histórico de versões.
- Templates para reuniões, diário, projetos e checklists.
- Exportação para PDF com imagens, HTML e DOCX.
- Interface em português, inglês e francês.

## Armazenamento

As notas são guardadas localmente na pasta escolhida:

```text
Pasta das notas/
└── Notebook/
    └── Nota/
        ├── note.md
        ├── .note.json
        └── assets/
```

Ficheiros externos permanecem no local original. O NoteMD não exige conta nem
envia o conteúdo das notas para serviços externos.

## Requisitos

- macOS 14 ou posterior.
- Swift 6 para compilação.

## Compilar

```shell
swift build -c release
```

O executável é criado em `.build/arm64-apple-macosx/release/NoteMD` em Macs com
Apple Silicon.

## Licença

Consulte [LICENSE](LICENSE).

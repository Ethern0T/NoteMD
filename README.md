<div align="center">
  <img src="macOS/Assets/NoteMD-AppIcon-1024.png" alt="Ícone do NoteMD" width="120">

  # NoteMD

  Editor Markdown local e nativo para **macOS** e **Linux**.
</div>
<p align="center">
  <img src="https://img.shields.io/badge/macOS-14+-black?logo=apple" alt="macOS">
  <img src="https://img.shields.io/badge/Linux-GTK%204-FCC624?logo=linux" alt="Linux">
  <img src="https://img.shields.io/badge/Swift-6-orange?logo=swift" alt="Swift">
  <img src="https://img.shields.io/badge/C-C17-A8B9CC?logo=c" alt="C17">
  <img src="https://img.shields.io/github/license/Ethern0T/NoteMD" alt="License">
  <img src="https://img.shields.io/github/last-commit/Ethern0T/NoteMD" alt="Last Commit">
</p>

## Funcionalidades

- **Organização**: Notebooks, separadores, tags, cores e pesquisa global.
- **Modos de edição**: Editor Markdown, editor visual e modo dividido com pré-visualização.
- **Suporte Markdown**: Títulos, tabelas, código, listas, tarefas, citações e ligações `[[wiki]]`.
- **Ficheiros e armazenamento**: Leitura/edição de ficheiros `.md` locais e externos, deteção de alterações externas.
- **Histórico e segurança**: Gravação automática, recuperação de rascunhos e histórico de versões.
- **Exportação e utilitários**: Exportação para PDF, HTML e DOCX. Templates para reuniões, diários e projetos.
- **Personalização**: Temas de cor (Sistema, Claro, Escuro, Monokai, Tokyo Night, Dracula, Solarized Light) e suporte multilíngue (Português, Inglês, Francês).

## Estrutura de Ficheiros

As notas são guardadas localmente em ficheiros Markdown numa estrutura transparente:

```text
Pasta das notas/
└── Notebook/
    └── Nota/
        ├── note.md
        ├── .note.json
        └── assets/
```

O NoteMD é 100% local, não exige conta nem envia dados para a nuvem.

## Instalação e Compilação

### Linux (GTK 4 / C17)

Ver detalhes no diretório [`linux/`](linux/).

```shell
cd linux
make
make install-user
# Gerar pacote Debian (.deb):
make deb
```

**Requisitos**: GTK 4, compilador C17 (`gcc` ou `clang`).

---

### macOS (SwiftUI)

Ver detalhes no diretório [`macOS/`](macOS/).

```shell
cd macOS
swift build -c release
```

**Requisitos**: macOS 14 ou posterior, Swift 6.

## Licença

[MIT License](LICENSE)

## SCREENSHOTS

### Linux Debian

<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/6858e979-7f2a-4484-a617-9b38ade86a88" />
<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/ab32281d-b5d5-4c65-8247-f8abe71efdd8" />

### macOS

<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/22b29388-4c18-4140-9060-e8070973ced6" />
<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/44461b8b-c662-40de-ba6c-ad0ede31dbd7" />



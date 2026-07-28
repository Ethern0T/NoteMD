import SwiftUI

private enum EditorMode: Equatable {
    case editor
    case preview
    case split
}

struct NoteWorkspace: View {
    let sidebarIsHidden: Bool

    @EnvironmentObject private var library: LibraryStore
    @State private var mode = EditorMode.editor
    @State private var editorSelection = NSRange(location: 0, length: 0)

    var body: some View {
        VStack(spacing: 0) {
            NoteTabBar(mode: $mode, sidebarIsHidden: sidebarIsHidden)
            
            if let note = library.selectedNote {
                TextField(
                    "Título",
                    text: Binding(
                        get: { library.selectedNote?.title ?? "" },
                        set: { library.updateSelectedNote(title: $0) }
                    )
                )
                .textFieldStyle(.plain)
                .font(.title.bold())
                .padding(.horizontal, 24)
                .padding(.vertical, 18)

                Divider()

                Group {
                    switch mode {
                    case .editor:
                        editorPane

                    case .preview:
                        visualEditorPane(note.id)

                    case .split:
                        HSplitView {
                            editorPane
                                .frame(minWidth: 380, idealWidth: 520)
                            visualEditorPane(note.id)
                                .frame(minWidth: 380, idealWidth: 520)
                        }
                    }
                }
            } else {
                ContentUnavailableView(
                    tr("Selecione uma nota"),
                    systemImage: "doc.text"
                )
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            }
        }
        .ignoresSafeArea(.container, edges: .top)
    }

    private var markdownBinding: Binding<String> {
        Binding(
            get: { library.selectedNote?.markdown ?? "" },
            set: { library.updateSelectedNote(markdown: $0) }
        )
    }

    private var editorPane: some View {
        VStack(spacing: 0) {
            MarkdownFormattingToolbar(
                text: markdownBinding,
                selection: $editorSelection,
                storeImage: library.storePastedImage
            )
            MarkdownEditor(
                text: markdownBinding,
                selection: $editorSelection,
                pasteImage: library.storePastedImage
            )
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            EditorStatusBar(markdown: markdownBinding.wrappedValue)
        }
    }

    private func previewPane(_ markdown: String) -> some View {
        MarkdownPreview(
            source: markdown,
            assetsURL: library.activeNoteAssetsURL
        )
    }

    private func visualEditorPane(_ noteID: Note.ID) -> some View {
        VisualMarkdownEditor(
            markdown: markdownBinding,
            assetsURL: library.activeNoteAssetsURL
        )
        .id(noteID)
    }
}

private struct EditorStatusBar: View {
    let markdown: String

    private var words: Int {
        markdown.split { $0.isWhitespace || $0.isNewline }.count
    }

    private var lines: Int {
        max(1, markdown.components(separatedBy: .newlines).count)
    }

    var body: some View {
        HStack(spacing: 6) {
            Text("\(words) \(tr("palavras"))")
            Text("•")
            Text("\(markdown.count) \(tr("carateres"))")
            Text("•")
            Text("\(lines) \(tr("linhas"))")
            Spacer()
            Text("Markdown")
        }
        .font(.caption)
        .foregroundStyle(.secondary)
        .padding(.horizontal, 12)
        .frame(height: 24)
        .background(Color(nsColor: .windowBackgroundColor))
        .overlay(alignment: .top) { Divider() }
    }
}

private struct NoteTabBar: View {
    @EnvironmentObject private var library: LibraryStore
    @Binding var mode: EditorMode
    let sidebarIsHidden: Bool

    var body: some View {
        HStack(spacing: 8) {
            ScrollView(.horizontal) {
                HStack(spacing: 2) {
                    ForEach(library.openNotes) { note in
                        tab(for: note)
                    }
                }
                .padding(.leading, 6)
                .padding(.top, 6)
            }
            .scrollIndicators(.hidden)

            modeButtons
                .padding(.trailing, 8)

            Button {
                _ = library.saveActiveNote()
            } label: {
                Image(systemName: "square.and.arrow.down")
                    .frame(width: 24, height: 22)
            }
            .buttonStyle(.plain)
            .disabled(library.activeNoteID == nil)
            .help(tr("Guardar nota (⌘S)"))
            .keyboardShortcut("s", modifiers: .command)
            .padding(.trailing, 8)

            Button {
                library.exportActiveNoteToPDF()
            } label: {
                Image(systemName: "doc.richtext")
                    .frame(width: 24, height: 22)
            }
            .buttonStyle(.plain)
            .disabled(library.activeNoteID == nil)
            .help(tr("Exportar nota para PDF"))
            .padding(.trailing, 8)
        }
        .background(.bar)
        .overlay(alignment: .bottom) {
            Divider()
        }
        .frame(height: 40)
        .padding(.leading, sidebarIsHidden ? 72 : 0)
    }

    private var modeButtons: some View {
        HStack(spacing: 2) {
            modeButton(
                .editor,
                icon: "pencil",
                help: tr("Editar nota")
            )
            modeButton(
                .preview,
                icon: "eye",
                help: tr("Visualizar nota")
            )
            modeButton(
                .split,
                icon: "rectangle.split.2x1",
                help: tr("Editor e visualização")
            )
        }
        .padding(2)
        .background(Color(nsColor: .controlBackgroundColor))
        .clipShape(RoundedRectangle(cornerRadius: 7))
    }

    private func modeButton(
        _ targetMode: EditorMode,
        icon: String,
        help: String
    ) -> some View {
        Button {
            mode = targetMode
        } label: {
            Image(systemName: icon)
                .frame(width: 24, height: 22)
                .background {
                    if mode == targetMode {
                        RoundedRectangle(cornerRadius: 5)
                            .fill(Color(nsColor: .selectedContentBackgroundColor))
                    }
                }
                .foregroundStyle(mode == targetMode ? .white : .primary)
        }
        .buttonStyle(.plain)
        .help(help)
    }

    private func tab(for note: Note) -> some View {
        let isActive = library.activeNoteID == note.id

        return HStack(spacing: 8) {
            Button {
                library.openNote(note.id)
            } label: {
                HStack(spacing: 7) {
                    Image(systemName: "doc.text")
                        .foregroundStyle(
                            note.colorHex.flatMap(Color.init(hex:))
                                ?? (isActive ? Color.accentColor : Color.secondary)
                        )
                    Text(note.title)
                        .lineLimit(1)
                        .frame(maxWidth: 150, alignment: .leading)
                        .fontWeight(isActive ? .semibold : .regular)
                        .foregroundStyle(
                            note.colorHex.flatMap(Color.init(hex:))
                                ?? Color.primary
                        )
                    if library.isDirty(note.id) {
                        Circle()
                            .fill(Color.accentColor)
                            .frame(width: 6, height: 6)
                            .help(tr("Alterações por guardar"))
                    }
                }
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)

            Button {
                library.closeNoteWithConfirmation(note.id)
            } label: {
                Image(systemName: "xmark")
                    .font(.system(size: 9, weight: .bold))
                    .frame(width: 16, height: 16)
                    .background {
                        Circle()
                            .fill(isActive ? Color.primary.opacity(0.08) : .clear)
                    }
                    .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .help(tr("Fechar nota"))
        }
        .padding(.horizontal, 10)
        .frame(height: 33)
        .background {
            UnevenRoundedRectangle(
                topLeadingRadius: 9,
                bottomLeadingRadius: 5,
                bottomTrailingRadius: 5,
                topTrailingRadius: 9
            )
            .fill(
                isActive
                    ? Color(nsColor: .textBackgroundColor)
                    : Color.primary.opacity(0.035)
            )
            .overlay {
                UnevenRoundedRectangle(
                    topLeadingRadius: 9,
                    bottomLeadingRadius: 5,
                    bottomTrailingRadius: 5,
                    topTrailingRadius: 9
                )
                .stroke(
                    isActive
                        ? Color.accentColor.opacity(0.45)
                        : Color.primary.opacity(0.07),
                    lineWidth: 1
                )
            }
        }
        .shadow(
            color: isActive ? Color.black.opacity(0.12) : .clear,
            radius: 2,
            y: 1
        )
    }
}

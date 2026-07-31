import SwiftUI

private enum EditorMode: Equatable {
    case editor
    case preview
    case split
}

struct NoteWorkspace: View {
    let sidebarIsHidden: Bool

    @EnvironmentObject private var library: LibraryStore
    @Environment(\.colorScheme) private var colorScheme
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
                .foregroundStyle(.primary)
                .padding(.horizontal, 24)
                .padding(.vertical, 18)

                NoteTagBar(note: note)

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
                            previewPane(note.markdown)
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
        .background(workspaceBackground)
        .ignoresSafeArea(.container, edges: .top)
    }

    private var workspaceBackground: Color {
        colorScheme == .dark
            ? Color(red: 0.10, green: 0.10, blue: 0.11)
            : Color(red: 0.99, green: 0.99, blue: 0.99)
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
            HStack(spacing: 0) {
                EditorLineNumberColumn(
                    markdown: markdownBinding.wrappedValue,
                    selection: editorSelection
                )

                Divider()

                MarkdownEditor(
                    text: markdownBinding,
                    selection: $editorSelection,
                    pasteImage: library.storePastedImage
                )
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            }
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

private struct NoteTagBar: View {
    @EnvironmentObject private var library: LibraryStore
    let note: Note
    @State private var newTag = ""

    var body: some View {
        HStack(spacing: 7) {
            if note.externalFilePath != nil {
                Label(tr("Ficheiro externo"), systemImage: "link")
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .help(note.externalFilePath ?? "")
            }

            ScrollView(.horizontal) {
                HStack(spacing: 6) {
                    ForEach(note.tags, id: \.self) { tag in
                        HStack(spacing: 4) {
                            Text("#\(tag)")
                            Button {
                                library.updateTags(
                                    note.tags.filter { $0 != tag },
                                    for: note.id
                                )
                            } label: {
                                Image(systemName: "xmark")
                                    .font(.system(size: 8, weight: .bold))
                            }
                            .buttonStyle(.plain)
                        }
                        .font(.caption)
                        .padding(.horizontal, 8)
                        .padding(.vertical, 4)
                        .background(Color.accentColor.opacity(0.13), in: Capsule())
                    }
                }
            }
            .scrollIndicators(.hidden)

            TextField(tr("Adicionar tag"), text: $newTag)
                .textFieldStyle(.plain)
                .frame(width: 110)
                .onSubmit(addTag)
            Button(action: addTag) {
                Image(systemName: "plus.circle")
            }
            .buttonStyle(.plain)
            .disabled(newTag.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
        }
        .padding(.horizontal, 24)
        .padding(.bottom, 10)
    }

    private func addTag() {
        let value = newTag.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !value.isEmpty else { return }
        library.updateTags(note.tags + [value], for: note.id)
        newTag = ""
    }
}

private struct EditorLineNumberColumn: View {
    @Environment(\.colorScheme) private var colorScheme
    let markdown: String
    let selection: NSRange

    private var lineCount: Int {
        max(1, markdown.components(separatedBy: .newlines).count)
    }

    private var activeLine: Int {
        let source = markdown as NSString
        let location = min(selection.location, source.length)
        guard location > 0 else { return 1 }
        return source.substring(to: location).reduce(into: 1) { count, character in
            if character == "\n" { count += 1 }
        }
    }

    var body: some View {
        ScrollView(.vertical) {
            LazyVStack(spacing: 0) {
                ForEach(1...lineCount, id: \.self) { line in
                    Text("\(line)")
                        .font(.system(size: 11, weight: line == activeLine ? .semibold : .regular))
                        .monospacedDigit()
                        .foregroundStyle(
                            line == activeLine ? Color.accentColor : Color.secondary
                        )
                        .frame(width: 31, height: 17, alignment: .trailing)
                        .padding(.horizontal, 6)
                        .background {
                            if line == activeLine {
                                RoundedRectangle(cornerRadius: 6)
                                    .fill(Color.accentColor.opacity(
                                        colorScheme == .dark ? 0.28 : 0.15
                                    ))
                            }
                        }
                }
            }
            .padding(.top, 18)
        }
        .scrollIndicators(.hidden)
        .frame(width: 44)
        .background(
            colorScheme == .dark
                ? Color(red: 0.13, green: 0.13, blue: 0.14)
                : Color(red: 0.95, green: 0.95, blue: 0.96)
        )
        .clipped()
    }
}

private struct EditorStatusBar: View {
    @Environment(\.colorScheme) private var colorScheme
    let markdown: String

    private var words: Int {
        markdown.split { $0.isWhitespace || $0.isNewline }.count
    }

    private var lines: Int {
        max(1, markdown.components(separatedBy: .newlines).count)
    }

    var body: some View {
        HStack(spacing: 8) {
            Text("\(words) \(tr("palavras"))")
                .foregroundStyle(statusTextColor)
            Text("•")
                .foregroundStyle(statusTextColor.opacity(0.65))
            Text("\(markdown.count) \(tr("carateres"))")
                .foregroundStyle(statusTextColor)
            Spacer()
            Label(
                "\(lines) \(tr("linhas"))",
                systemImage: "text.line.first.and.arrowtriangle.forward"
            )
            .font(.caption.weight(.semibold))
            .monospacedDigit()
            .foregroundStyle(Color.white)
            .padding(.horizontal, 9)
            .padding(.vertical, 5)
            .background(
                Capsule()
                    .fill(counterColor)
            )
            .overlay {
                Capsule()
                    .stroke(Color.white.opacity(0.28), lineWidth: 1)
            }
            Text("Markdown")
                .foregroundStyle(statusTextColor)
        }
        .font(.caption)
        .padding(.horizontal, 12)
        .frame(height: 34)
        .background(barBackground)
        .overlay(alignment: .top) { Divider() }
    }

    private var barBackground: Color {
        colorScheme == .dark
            ? Color(red: 0.13, green: 0.13, blue: 0.14)
            : Color(red: 0.96, green: 0.96, blue: 0.97)
    }

    private var counterColor: Color {
        colorScheme == .dark
            ? Color(red: 0.16, green: 0.48, blue: 0.92)
            : Color(red: 0.03, green: 0.30, blue: 0.72)
    }

    private var statusTextColor: Color {
        colorScheme == .dark ? .white : .black
    }
}

private struct NoteTabBar: View {
    @EnvironmentObject private var library: LibraryStore
    @Environment(\.colorScheme) private var colorScheme
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
                    .frame(width: 30, height: 30)
                    .background(.thinMaterial, in: Circle())
            }
            .buttonStyle(.plain)
            .disabled(library.activeNoteID == nil)
            .help(tr("Guardar nota (⌘S)"))
            .keyboardShortcut("s", modifiers: .command)
            .padding(.trailing, 2)

            Menu {
                if let noteID = library.activeNoteID {
                    let versions = library.versions(for: noteID)
                    if versions.isEmpty {
                        Text(tr("Sem versões anteriores"))
                    } else {
                        ForEach(versions) { version in
                            Button {
                                library.restoreVersion(version)
                            } label: {
                                Text(version.createdAt.formatted(
                                    date: .abbreviated,
                                    time: .shortened
                                ))
                            }
                        }
                    }
                }
            } label: {
                Image(systemName: "clock.arrow.circlepath")
                    .frame(width: 30, height: 30)
                    .background(.thinMaterial, in: Circle())
            }
            .menuStyle(.borderlessButton)
            .frame(width: 34)
            .disabled(library.activeNoteID == nil)
            .help(tr("Histórico de versões"))

            Button {
                library.exportActiveNoteToPDF()
            } label: {
                Image(systemName: "doc.richtext")
                    .frame(width: 30, height: 30)
                    .background(.thinMaterial, in: Circle())
            }
            .buttonStyle(.plain)
            .disabled(library.activeNoteID == nil)
            .help(tr("Exportar nota para PDF"))
            .padding(.trailing, 10)
        }
        .foregroundStyle(colorScheme == .dark ? Color.white : Color.black)
        .background(.ultraThinMaterial)
        .overlay(alignment: .bottom) {
            Divider()
        }
        .frame(height: 48)
        .padding(.leading, sidebarIsHidden ? 72 : 0)
    }

    private var tabBarBackground: Color {
        colorScheme == .dark
            ? Color(red: 0.13, green: 0.13, blue: 0.14)
            : Color(red: 0.96, green: 0.96, blue: 0.97)
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
                icon: "text.viewfinder",
                help: tr("Editar visualmente")
            )
            modeButton(
                .split,
                icon: "rectangle.split.2x1",
                help: tr("Editor e visualização")
            )
        }
        .padding(3)
        .background(.thinMaterial, in: Capsule())
        .overlay {
            Capsule()
                .stroke(Color.primary.opacity(0.10), lineWidth: 1)
        }
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
                .frame(width: 28, height: 26)
                .background {
                    if mode == targetMode {
                        Capsule()
                            .fill(Color.accentColor)
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

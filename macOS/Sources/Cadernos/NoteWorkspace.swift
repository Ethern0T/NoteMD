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
    @State private var showsFind = false
    @State private var findText = ""
    @State private var replaceText = ""
    @State private var previewScrollFraction = 0.0

    var body: some View {
        VStack(spacing: 0) {
            NoteTabBar(mode: $mode, sidebarIsHidden: sidebarIsHidden)
            
            if let note = library.selectedNote {
                HStack(spacing: 14) {
                    TextField(
                        "Título",
                        text: Binding(
                            get: { library.selectedNote?.title ?? "" },
                            set: { library.updateSelectedNote(title: $0) }
                        )
                    )
                    .textFieldStyle(.plain)
                    .font(.headline.bold())
                    .foregroundStyle(.primary)
                    .frame(minWidth: 160, idealWidth: 240, maxWidth: 340)

                    NoteTagBar(note: note)
                        .frame(maxWidth: .infinity)
                }
                .padding(.horizontal, 18)
                .padding(.vertical, 7)
                DocumentNavigationBar(note: note)

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
                            MarkdownPreview(
                                source: note.markdown,
                                assetsURL: library.activeNoteAssetsURL,
                                scrollFraction: previewScrollFraction
                            )
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
        .onChange(of: editorSelection) { _, selection in
            let length = max(1, (markdownBinding.wrappedValue as NSString).length)
            previewScrollFraction = min(1, Double(selection.location) / Double(length))
        }
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
            HStack(spacing: 0) {
                MarkdownFormattingToolbar(
                    text: markdownBinding,
                    selection: $editorSelection,
                    storeImage: library.storePastedImage
                )
                Button {
                    showsFind.toggle()
                } label: {
                    Image(systemName: "magnifyingglass")
                        .frame(width: 28, height: 28)
                }
                .buttonStyle(.plain)
                .keyboardShortcut("f", modifiers: .command)
                .help(tr("Pesquisar e substituir"))
                .padding(.trailing, 8)
            }
            if showsFind {
                FindReplaceBar(
                    text: markdownBinding,
                    selection: $editorSelection,
                    findText: $findText,
                    replaceText: $replaceText,
                    close: { showsFind = false }
                )
            }
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
    }

    private func addTag() {
        let value = newTag.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !value.isEmpty else { return }
        library.updateTags(note.tags + [value], for: note.id)
        newTag = ""
    }
}

private struct DocumentNavigationBar: View {
    @EnvironmentObject private var library: LibraryStore
    let note: Note

    var body: some View {
        if !outgoingTitles.isEmpty || !backlinks.isEmpty {
            HStack(spacing: 12) {
                if !outgoingTitles.isEmpty {
                    Menu {
                        ForEach(outgoingTitles, id: \.self) { title in
                            Button(title) { openNote(titled: title) }
                        }
                    } label: {
                        Label(tr("Ligações"), systemImage: "link")
                    }
                }
                if !backlinks.isEmpty {
                    Menu {
                        ForEach(backlinks) { linkedNote in
                            Button(linkedNote.title) { library.openNote(linkedNote.id) }
                        }
                    } label: {
                        Label("\(tr("Backlinks")) (\(backlinks.count))", systemImage: "arrowshape.turn.up.backward")
                    }
                }
                Spacer()
            }
            .font(.caption)
            .menuStyle(.borderlessButton)
            .padding(.horizontal, 24)
            .padding(.bottom, 8)
        }
    }

    private var outgoingTitles: [String] {
        let source = note.markdown as NSString
        let regex = try? NSRegularExpression(pattern: #"\[\[([^\]]+)\]\]"#)
        return Array(Set((regex?.matches(
            in: note.markdown,
            range: NSRange(location: 0, length: source.length)
        ) ?? []).map { source.substring(with: $0.range(at: 1)) })).sorted()
    }

    private var backlinks: [Note] {
        let token = "[[\(note.title)]]"
        return library.notebooks.flatMap(\.notes).filter {
            $0.id != note.id && $0.markdown.localizedCaseInsensitiveContains(token)
        }
    }

    private func openNote(titled title: String) {
        if let target = library.notebooks.flatMap(\.notes).first(where: {
            $0.title.caseInsensitiveCompare(title) == .orderedSame
        }) {
            library.openNote(target.id)
        } else if let notebookID = library.selectedNotebookID {
            library.addNote(
                to: notebookID,
                template: NoteTemplate(
                    id: UUID().uuidString,
                    name: title,
                    title: title,
                    markdown: "# \(title)\n"
                )
            )
        }
    }
}

private struct FindReplaceBar: View {
    @Binding var text: String
    @Binding var selection: NSRange
    @Binding var findText: String
    @Binding var replaceText: String
    let close: () -> Void

    var body: some View {
        HStack(spacing: 7) {
            TextField(tr("Pesquisar"), text: $findText)
                .textFieldStyle(.roundedBorder)
                .frame(maxWidth: 190)
                .onSubmit(findNext)
            TextField(tr("Substituir por"), text: $replaceText)
                .textFieldStyle(.roundedBorder)
                .frame(maxWidth: 190)
            Button(tr("Seguinte"), action: findNext)
            Button(tr("Substituir"), action: replaceCurrent)
            Button(tr("Substituir tudo"), action: replaceAll)
            Spacer()
            Button(action: close) { Image(systemName: "xmark") }
                .buttonStyle(.plain)
        }
        .controlSize(.small)
        .padding(.horizontal, 10)
        .padding(.vertical, 6)
        .background(Color(nsColor: .controlBackgroundColor))
    }

    private func findNext() {
        guard !findText.isEmpty else { return }
        let source = text as NSString
        let start = min(NSMaxRange(selection), source.length)
        var range = source.range(
            of: findText,
            options: [.caseInsensitive],
            range: NSRange(location: start, length: source.length - start)
        )
        if range.location == NSNotFound {
            range = source.range(of: findText, options: [.caseInsensitive])
        }
        if range.location != NSNotFound { selection = range }
    }

    private func replaceCurrent() {
        let source = text as NSString
        guard selection.location != NSNotFound,
              NSMaxRange(selection) <= source.length,
              source.substring(with: selection).caseInsensitiveCompare(findText) == .orderedSame
        else { findNext(); return }
        text = source.replacingCharacters(in: selection, with: replaceText)
        selection = NSRange(location: selection.location + (replaceText as NSString).length, length: 0)
        findNext()
    }

    private func replaceAll() {
        guard !findText.isEmpty else { return }
        text = text.replacingOccurrences(
            of: findText,
            with: replaceText,
            options: [.caseInsensitive]
        )
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
    @State private var showsHistory = false

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

            Button {
                showsHistory = true
            } label: {
                Image(systemName: "clock.arrow.circlepath")
                    .frame(width: 30, height: 30)
                    .background(.thinMaterial, in: Circle())
            }
            .buttonStyle(.plain)
            .disabled(library.activeNoteID == nil)
            .help(tr("Histórico de versões"))
            .sheet(isPresented: $showsHistory) {
                VersionHistoryView()
                    .environmentObject(library)
            }

            Menu {
                Button("PDF") { library.exportActiveNoteToPDF() }
                Button("HTML") { library.exportActiveNoteToHTML() }
                Button("DOCX") { library.exportActiveNoteToDOCX() }
            } label: {
                Image(systemName: "doc.richtext")
                    .frame(width: 30, height: 30)
                    .background(.thinMaterial, in: Circle())
            }
            .menuStyle(.borderlessButton)
            .frame(width: 34)
            .disabled(library.activeNoteID == nil)
            .help(tr("Exportar nota"))
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

private struct VersionHistoryView: View {
    @EnvironmentObject private var library: LibraryStore
    @Environment(\.dismiss) private var dismiss
    @State private var selectedVersion: NoteVersion?

    var body: some View {
        VStack(spacing: 0) {
            HStack {
                Text(tr("Histórico de versões"))
                    .font(.title2.bold())
                Spacer()
                Button(tr("Concluído")) { dismiss() }
            }
            .padding()
            Divider()
            HSplitView {
                List(versions, selection: Binding(
                    get: { selectedVersion?.id },
                    set: { id in selectedVersion = versions.first { $0.id == id } }
                )) { version in
                    VStack(alignment: .leading, spacing: 3) {
                        Text(version.createdAt.formatted(date: .abbreviated, time: .shortened))
                        Text(library.versionDifference(version))
                            .font(.caption.monospaced())
                            .foregroundStyle(.secondary)
                    }
                    .tag(version.id)
                }
                .frame(minWidth: 210, idealWidth: 240)

                if let version = selectedVersion {
                    VStack(spacing: 0) {
                        HStack {
                            Text(tr("Versão guardada")).font(.headline)
                            Spacer()
                            Button(tr("Restaurar esta versão")) {
                                library.restoreVersion(version)
                                dismiss()
                            }
                            Button(role: .destructive) {
                                library.deleteVersion(version)
                                selectedVersion = nil
                            } label: { Text(tr("Eliminar versão")) }
                        }
                        .padding(10)
                        HSplitView {
                            versionText(title: tr("Versão guardada"), text: version.markdown)
                            versionText(
                                title: tr("Versão atual"),
                                text: library.selectedNote?.markdown ?? ""
                            )
                        }
                    }
                } else {
                    ContentUnavailableView(
                        tr("Selecione uma versão"),
                        systemImage: "clock.arrow.circlepath"
                    )
                }
            }
        }
        .frame(width: 900, height: 560)
        .onAppear { selectedVersion = versions.first }
    }

    private var versions: [NoteVersion] {
        library.activeNoteID.map(library.versions(for:)) ?? []
    }

    private func versionText(title: String, text: String) -> some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(title).font(.caption.weight(.semibold)).foregroundStyle(.secondary)
            ScrollView {
                Text(text)
                    .font(.system(.body, design: .monospaced))
                    .textSelection(.enabled)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(8)
            }
        }
        .padding(8)
    }
}

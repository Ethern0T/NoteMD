import AppKit
import SwiftUI
import UniformTypeIdentifiers

struct LibraryView: View {
    @EnvironmentObject private var library: LibraryStore
    @State private var expandedNotebookIDs = Set<Notebook.ID>()
    @State private var showsSettings = false
    @State private var columnVisibility = NavigationSplitViewVisibility.all
    @State private var editingNotebookID: Notebook.ID?
    @State private var notebookTitleDraft = ""
    @State private var searchText = ""
    @State private var showsSearch = false
    @State private var selectedTag: String?
    @FocusState private var focusedNotebookID: Notebook.ID?
    @FocusState private var searchIsFocused: Bool

    var body: some View {
        NavigationSplitView(columnVisibility: $columnVisibility) {
            sidebar
        } detail: {
            NoteWorkspace(sidebarIsHidden: columnVisibility == .detailOnly)
        }
        .onDrop(
            of: [UTType.fileURL.identifier],
            isTargeted: nil,
            perform: handleFileDrop
        )
        .onAppear {
            if let firstID = library.notebooks.first?.id {
                expandedNotebookIDs.insert(firstID)
            }
            let storagePath = UserDefaults.standard.string(
                forKey: "notesFolderPath"
            )?.trimmingCharacters(in: .whitespacesAndNewlines)
            if storagePath?.isEmpty != false
                || !FileManager.default.fileExists(atPath: storagePath ?? "") {
                showsSettings = true
            }
        }
        .sheet(isPresented: $showsSettings) {
            StorageSettingsView()
        }
    }

    private var sidebar: some View {
        VStack(spacing: 0) {
        List {
            Section {
                ForEach(filteredNotebooks) { notebook in
                    DisclosureGroup(
                        isExpanded: expansionBinding(for: notebook.id)
                    ) {
                        ForEach(notebook.notes) { note in
                            noteRow(note)
                        }

                        Menu {
                            Button(tr("Nota vazia")) {
                                library.addNote(to: notebook.id)
                                expandedNotebookIDs.insert(notebook.id)
                            }
                            Divider()
                            ForEach(NoteTemplate.builtIn) { template in
                                Button(tr(template.name)) {
                                    library.addNote(to: notebook.id, template: template)
                                    expandedNotebookIDs.insert(notebook.id)
                                }
                            }
                        } label: {
                            Label(tr("Nova nota"), systemImage: "plus")
                                .foregroundStyle(.secondary)
                        }
                        .buttonStyle(.plain)
                        .padding(.leading, 4)
                    } label: {
                        Label {
                            if editingNotebookID == notebook.id {
                                TextField(
                                    tr("Nome do notebook"),
                                    text: $notebookTitleDraft
                                )
                                .textFieldStyle(.plain)
                                .focused($focusedNotebookID, equals: notebook.id)
                                .onSubmit {
                                    finishRenamingNotebook(notebook)
                                }
                                .onChange(of: focusedNotebookID) { _, focusedID in
                                    if focusedID != notebook.id,
                                       editingNotebookID == notebook.id {
                                        finishRenamingNotebook(notebook)
                                    }
                                }
                            } else {
                                Text(notebook.title)
                                    .foregroundStyle(
                                        notebook.colorHex.flatMap(Color.init(hex:))
                                            ?? Color.primary
                                    )
                            }
                        } icon: {
                            Image(systemName: "book.closed.fill")
                                .foregroundStyle(
                                    notebook.colorHex.flatMap(Color.init(hex:))
                                        ?? Color.secondary
                                )
                        }
                            .fontWeight(.medium)
                            .contentShape(Rectangle())
                            .onTapGesture {
                                guard editingNotebookID != notebook.id else { return }
                                library.selectNotebook(notebook.id)
                                toggleExpansion(notebook.id)
                            }
                            .onTapGesture(count: 2) {
                                beginRenamingNotebook(notebook)
                            }
                            .contextMenu {
                                Button {
                                    beginRenamingNotebook(notebook)
                                } label: {
                                    Label(tr("Renomear notebook"), systemImage: "pencil")
                                }

                                Menu {
                                    ForEach(noteColors) { color in
                                        Button {
                                            library.setNotebookColor(
                                                color.hex.isEmpty ? nil : color.hex,
                                                notebookID: notebook.id
                                            )
                                        } label: {
                                            Label {
                                                Text(tr(color.name))
                                            } icon: {
                                                Image(nsImage: colorSwatchImage(hex: color.hex))
                                                    .renderingMode(.original)
                                            }
                                        }
                                    }
                                } label: {
                                    Label(tr("Cor do notebook"), systemImage: "paintpalette")
                                }

                                Menu(tr("Ordenar notas")) {
                                    Button(tr("Por título")) {
                                        library.sortNotes(in: notebook.id, by: .title)
                                    }
                                    Button(tr("Mais recentes")) {
                                        library.sortNotes(in: notebook.id, by: .updated)
                                    }
                                }

                                Button(role: .destructive) {
                                    library.deleteNotebook(notebook.id)
                                } label: {
                                    Label(tr("Remover notebook"), systemImage: "trash")
                                }
                            }
                            .dropDestination(for: String.self) { items, _ in
                                var moved = false
                                for value in items {
                                    guard let id = UUID(uuidString: value) else { continue }
                                    library.moveNote(id, to: notebook.id)
                                    moved = true
                                }
                                return moved
                            }
                    }
                }
            } header: {
                HStack {
                    if showsSearch {
                        HStack(spacing: 6) {
                            Image(systemName: "magnifyingglass")
                                .foregroundStyle(.secondary)
                            TextField(tr("Pesquisar notas"), text: $searchText)
                                .textFieldStyle(.plain)
                                .focused($searchIsFocused)
                                .onSubmit {
                                    if searchText.isEmpty { closeSearch() }
                                }
                            Button(action: closeSearch) {
                                Image(systemName: "xmark.circle.fill")
                                    .foregroundStyle(.secondary)
                            }
                            .buttonStyle(.plain)
                        }
                        .padding(.horizontal, 7)
                        .padding(.vertical, 5)
                        .background(.regularMaterial, in: RoundedRectangle(cornerRadius: 7))
                    } else {
                        Text(tr("Notebooks"))
                        Spacer()
                        Button {
                            showsSearch = true
                            DispatchQueue.main.async { searchIsFocused = true }
                        } label: {
                            Image(systemName: "magnifyingglass")
                        }
                        .buttonStyle(.plain)
                        .help(tr("Pesquisar notas"))
                        Button {
                            showsSettings = true
                        } label: {
                            Image(systemName: "gearshape")
                        }
                        .buttonStyle(.plain)
                        .help(tr("Configurações"))
                        Button {
                            let notebookID = library.addNotebook()
                            expandedNotebookIDs.insert(notebookID)
                            editingNotebookID = notebookID
                            notebookTitleDraft = tr("Novo notebook")
                            DispatchQueue.main.async {
                                focusedNotebookID = notebookID
                            }
                        } label: {
                            Image(systemName: "plus")
                        }
                        .buttonStyle(.plain)
                        .help(tr("Novo notebook"))
                    }
                }
            }
        }

        Divider()
        VStack(alignment: .leading, spacing: 7) {
            HStack {
                Label(tr("Tags"), systemImage: "tag")
                    .font(.caption.weight(.semibold))
                Spacer()
                if selectedTag != nil {
                    Button(tr("Limpar")) { selectedTag = nil }
                        .buttonStyle(.plain)
                        .font(.caption)
                }
            }
            ScrollView {
                LazyVStack(alignment: .leading, spacing: 3) {
                    ForEach(library.allTags, id: \.self) { tag in
                        Button {
                            selectedTag = selectedTag == tag ? nil : tag
                        } label: {
                            HStack {
                                Image(systemName: "tag")
                                    .foregroundStyle(.secondary)
                                Text(tag)
                                Spacer()
                                Text("\(tagCount(tag))")
                                    .foregroundStyle(.secondary)
                            }
                            .contentShape(Rectangle())
                            .padding(.horizontal, 7)
                            .padding(.vertical, 5)
                            .background(
                                selectedTag == tag
                                    ? Color.accentColor.opacity(0.16)
                                    : Color.clear,
                                in: RoundedRectangle(cornerRadius: 6)
                            )
                        }
                        .buttonStyle(.plain)
                    }
                    if library.allTags.isEmpty {
                        Text(tr("Sem tags"))
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                }
            }
            .frame(maxHeight: 150)
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 8)
        }
        .navigationSplitViewColumnWidth(min: 220, ideal: 260, max: 310)
    }

    private var filteredNotebooks: [Notebook] {
        let matchingIDs = library.notes(matching: searchText, tag: selectedTag)
        let filtering = !searchText.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
            || selectedTag != nil
        guard filtering else { return library.notebooks }
        return library.notebooks.compactMap { notebook in
            let notes = notebook.notes.filter { matchingIDs.contains($0.id) }
            guard !notes.isEmpty else { return nil }
            var result = notebook
            result.notes = notes
            return result
        }
    }

    private func tagCount(_ tag: String) -> Int {
        library.notebooks.flatMap(\.notes).filter {
            $0.tags.contains { $0.caseInsensitiveCompare(tag) == .orderedSame }
        }.count
    }

    private func closeSearch() {
        searchText = ""
        showsSearch = false
        searchIsFocused = false
    }

    private func handleFileDrop(_ providers: [NSItemProvider]) -> Bool {
        var accepted = false
        for provider in providers where provider.hasItemConformingToTypeIdentifier(
            UTType.fileURL.identifier
        ) {
            accepted = true
            provider.loadItem(
                forTypeIdentifier: UTType.fileURL.identifier,
                options: nil
            ) { item, _ in
                let url: URL?
                if let data = item as? Data {
                    url = URL(dataRepresentation: data, relativeTo: nil)
                } else {
                    url = item as? URL
                }
                guard let url else { return }
                Task { @MainActor in
                    let ext = url.pathExtension.lowercased()
                    if ["md", "markdown", "mdown", "mkd"].contains(ext) {
                        _ = library.openMarkdownFile(at: url)
                    } else if let image = NSImage(contentsOf: url),
                              let insertion = library.storePastedImage(image) {
                        let current = library.selectedNote?.markdown ?? ""
                        let separator = current.hasSuffix("\n") ? "" : "\n"
                        library.updateSelectedNote(
                            markdown: current + separator + insertion + "\n"
                        )
                    }
                }
            }
        }
        return accepted
    }

    private func noteRow(_ note: Note) -> some View {
        Button {
            library.openNote(note.id)
        } label: {
            HStack(spacing: 8) {
                Image(systemName: "doc.text")
                    .foregroundStyle(
                        note.colorHex.flatMap(Color.init(hex:))
                            ?? Color.secondary
                    )
                Text(note.title)
                    .lineLimit(1)
                    .foregroundStyle(
                        note.colorHex.flatMap(Color.init(hex:))
                            ?? Color.primary
                    )
                Spacer()
            }
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .draggable(note.id.uuidString)
        .dropDestination(for: String.self) { items, _ in
            var reordered = false
            for value in items {
                guard let id = UUID(uuidString: value) else { continue }
                library.reorderNote(id, before: note.id)
                reordered = true
            }
            return reordered
        }
        .padding(.leading, 4)
        .contextMenu {
            Menu {
                ForEach(noteColors) { color in
                    Button {
                        library.setNoteColor(
                            color.hex.isEmpty ? nil : color.hex,
                            noteID: note.id
                        )
                    } label: {
                        Label {
                            Text(tr(color.name))
                        } icon: {
                            Image(nsImage: colorSwatchImage(hex: color.hex))
                                .renderingMode(.original)
                        }
                    }
                }
            } label: {
                Label(tr("Cor da nota"), systemImage: "paintpalette")
            }

            Menu(tr("Mover para notebook")) {
                ForEach(library.notebooks.filter { notebook in
                    !notebook.notes.contains(where: { $0.id == note.id })
                }) { notebook in
                    Button(notebook.title) {
                        library.moveNote(note.id, to: notebook.id)
                    }
                }
            }

            Button(role: .destructive) {
                library.deleteNote(note.id)
            } label: {
                Label(tr("Remover nota"), systemImage: "trash")
            }
        }
        .listRowBackground(
            library.activeNoteID == note.id
                ? Color.accentColor.opacity(0.14)
                : Color.clear
        )
    }

    private func expansionBinding(for id: Notebook.ID) -> Binding<Bool> {
        Binding(
            get: {
                let filtering = !searchText.trimmingCharacters(
                    in: .whitespacesAndNewlines
                ).isEmpty || selectedTag != nil
                return filtering || expandedNotebookIDs.contains(id)
            },
            set: { isExpanded in
                if isExpanded {
                    expandedNotebookIDs.insert(id)
                } else {
                    expandedNotebookIDs.remove(id)
                }
            }
        )
    }

    private func toggleExpansion(_ id: Notebook.ID) {
        if expandedNotebookIDs.contains(id) {
            expandedNotebookIDs.remove(id)
        } else {
            expandedNotebookIDs.insert(id)
        }
    }

    private func beginRenamingNotebook(_ notebook: Notebook) {
        editingNotebookID = notebook.id
        notebookTitleDraft = notebook.title
        DispatchQueue.main.async {
            focusedNotebookID = notebook.id
        }
    }

    private func finishRenamingNotebook(_ notebook: Notebook) {
        if library.renameNotebook(notebook.id, to: notebookTitleDraft) {
            editingNotebookID = nil
            focusedNotebookID = nil
        } else {
            notebookTitleDraft = notebook.title
            DispatchQueue.main.async {
                focusedNotebookID = notebook.id
            }
        }
    }
}

import SwiftUI

struct LibraryView: View {
    @EnvironmentObject private var library: LibraryStore
    @State private var expandedNotebookIDs = Set<Notebook.ID>()
    @State private var showsSettings = false
    @State private var columnVisibility = NavigationSplitViewVisibility.all

    var body: some View {
        NavigationSplitView(columnVisibility: $columnVisibility) {
            sidebar
        } detail: {
            NoteWorkspace(sidebarIsHidden: columnVisibility == .detailOnly)
        }
        .onAppear {
            if let firstID = library.notebooks.first?.id {
                expandedNotebookIDs.insert(firstID)
            }
        }
        .sheet(isPresented: $showsSettings) {
            StorageSettingsView()
        }
    }

    private var sidebar: some View {
        List {
            Section {
                ForEach(library.notebooks) { notebook in
                    DisclosureGroup(
                        isExpanded: expansionBinding(for: notebook.id)
                    ) {
                        ForEach(notebook.notes) { note in
                            noteRow(note)
                        }

                        Button {
                            library.addNote(to: notebook.id)
                            expandedNotebookIDs.insert(notebook.id)
                        } label: {
                            Label(tr("Nova nota"), systemImage: "plus")
                                .foregroundStyle(.secondary)
                        }
                        .buttonStyle(.plain)
                        .padding(.leading, 4)
                    } label: {
                        Label {
                            Text(notebook.title)
                                .foregroundStyle(
                                    notebook.colorHex.flatMap(Color.init(hex:))
                                        ?? Color.primary
                                )
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
                                library.selectNotebook(notebook.id)
                                toggleExpansion(notebook.id)
                            }
                            .contextMenu {
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

                                Button(role: .destructive) {
                                    library.deleteNotebook(notebook.id)
                                } label: {
                                    Label(tr("Remover notebook"), systemImage: "trash")
                                }
                            }
                    }
                }
            } header: {
                HStack {
                    Text(tr("Notebooks"))
                    Spacer()
                    Button {
                        showsSettings = true
                    } label: {
                        Image(systemName: "gearshape")
                    }
                    .buttonStyle(.plain)
                    .help(tr("Configurações"))
                    Button(action: library.addNotebook) {
                        Image(systemName: "plus")
                    }
                    .buttonStyle(.plain)
                    .help(tr("Novo notebook"))
                }
            }
        }
        .navigationSplitViewColumnWidth(min: 220, ideal: 260, max: 310)
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
            get: { expandedNotebookIDs.contains(id) },
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
}

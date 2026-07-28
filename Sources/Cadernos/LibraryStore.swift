import Combine
import Foundation

@MainActor
final class LibraryStore: ObservableObject {
    @Published private(set) var notebooks: [Notebook]
    @Published var selectedNotebookID: Notebook.ID?
    @Published private(set) var openNoteIDs: [Note.ID]
    @Published var activeNoteID: Note.ID?
    @Published private(set) var dirtyNoteIDs = Set<Note.ID>()
    var loadedStoragePath: String?

    init(notebooks: [Notebook]) {
        self.notebooks = notebooks
        selectedNotebookID = notebooks.first?.id
        let firstNoteID = notebooks.first?.notes.first?.id
        openNoteIDs = firstNoteID.map { [$0] } ?? []
        activeNoteID = firstNoteID
    }

    var openNotes: [Note] {
        openNoteIDs.compactMap(note(withID:))
    }

    var selectedNote: Note? {
        note(withID: activeNoteID)
    }

    func selectNotebook(_ id: Notebook.ID) {
        selectedNotebookID = id
    }

    func openNote(_ id: Note.ID?) {
        guard let id,
              let (notebookIndex, _) = noteLocation(withID: id)
        else { return }

        selectedNotebookID = notebooks[notebookIndex].id
        if !openNoteIDs.contains(id) {
            openNoteIDs.append(id)
        }
        activeNoteID = id
    }

    func closeNote(_ id: Note.ID) {
        guard let closingIndex = openNoteIDs.firstIndex(of: id) else { return }
        openNoteIDs.remove(at: closingIndex)

        guard activeNoteID == id else { return }
        if openNoteIDs.indices.contains(closingIndex) {
            openNote(openNoteIDs[closingIndex])
        } else if let lastID = openNoteIDs.last {
            openNote(lastID)
        } else {
            activeNoteID = nil
        }
    }

    func updateSelectedNote(title: String? = nil, markdown: String? = nil) {
        guard let (notebookIndex, noteIndex) = noteLocation(withID: activeNoteID) else {
            return
        }

        if let title {
            notebooks[notebookIndex].notes[noteIndex].title = title
        }
        if let markdown {
            notebooks[notebookIndex].notes[noteIndex].markdown = markdown
        }
        notebooks[notebookIndex].notes[noteIndex].updatedAt = .now
        dirtyNoteIDs.insert(notebooks[notebookIndex].notes[noteIndex].id)
    }

    func addNotebook() {
        let notebook = Notebook(title: "Novo notebook")
        notebooks.append(notebook)
        selectedNotebookID = notebook.id
    }

    func addNote(to notebookID: Notebook.ID) {
        guard let notebookIndex = notebooks.firstIndex(where: {
            $0.id == notebookID
        }) else { return }

        let note = Note(title: "Nova nota", markdown: "# Nova nota\n")
        notebooks[notebookIndex].notes.append(note)
        dirtyNoteIDs.insert(note.id)
        openNote(note.id)
    }

    func isDirty(_ id: Note.ID) -> Bool {
        dirtyNoteIDs.contains(id)
    }

    func markSaved(_ id: Note.ID) {
        dirtyNoteIDs.remove(id)
    }

    func replaceLibrary(with loadedNotebooks: [Notebook]) {
        notebooks = loadedNotebooks
        selectedNotebookID = loadedNotebooks.first?.id
        let firstNoteID = loadedNotebooks.first?.notes.first?.id
        openNoteIDs = firstNoteID.map { [$0] } ?? []
        activeNoteID = firstNoteID
        dirtyNoteIDs.removeAll()
    }

    func setStorageFolderName(_ folderName: String, for noteID: Note.ID) {
        for notebookIndex in notebooks.indices {
            guard let noteIndex = notebooks[notebookIndex].notes.firstIndex(
                where: { $0.id == noteID }
            ) else { continue }
            notebooks[notebookIndex].notes[noteIndex].storageFolderName = folderName
            if notebooks[notebookIndex].storageFolderName == nil {
                notebooks[notebookIndex].storageFolderName = safeStorageName(
                    notebooks[notebookIndex].title
                )
            }
            return
        }
    }

    private func safeStorageName(_ value: String) -> String {
        let result = value.components(separatedBy: CharacterSet(charactersIn: "/:"))
            .joined(separator: "-")
            .trimmingCharacters(in: .whitespacesAndNewlines)
        return result.isEmpty ? "Sem título" : result
    }

    func removeNoteFromLibrary(_ noteID: Note.ID) {
        closeNote(noteID)
        dirtyNoteIDs.remove(noteID)
        for notebookIndex in notebooks.indices {
            guard let noteIndex = notebooks[notebookIndex].notes.firstIndex(
                where: { $0.id == noteID }
            ) else { continue }
            notebooks[notebookIndex].notes.remove(at: noteIndex)
            return
        }
    }

    func removeNotebookFromLibrary(_ notebookID: Notebook.ID) {
        guard let index = notebooks.firstIndex(where: { $0.id == notebookID }) else {
            return
        }
        let noteIDs = notebooks[index].notes.map(\.id)
        for noteID in noteIDs {
            closeNote(noteID)
            dirtyNoteIDs.remove(noteID)
        }
        notebooks.remove(at: index)
        if selectedNotebookID == notebookID {
            selectedNotebookID = notebooks.first?.id
        }
    }

    func updateNotebookColor(_ colorHex: String?, notebookID: Notebook.ID) {
        guard let index = notebooks.firstIndex(where: { $0.id == notebookID }) else {
            return
        }
        notebooks[index].colorHex = colorHex
    }

    func markNotebookStored(_ notebookID: Notebook.ID, folderName: String) {
        guard let index = notebooks.firstIndex(where: { $0.id == notebookID }) else {
            return
        }
        notebooks[index].storageFolderName = folderName
    }

    func updateNoteColor(_ colorHex: String?, noteID: Note.ID) {
        for notebookIndex in notebooks.indices {
            guard let noteIndex = notebooks[notebookIndex].notes.firstIndex(
                where: { $0.id == noteID }
            ) else { continue }
            notebooks[notebookIndex].notes[noteIndex].colorHex = colorHex
            return
        }
    }

    func note(withID id: Note.ID?) -> Note? {
        guard let (notebookIndex, noteIndex) = noteLocation(withID: id) else {
            return nil
        }
        return notebooks[notebookIndex].notes[noteIndex]
    }

    private func noteLocation(withID id: Note.ID?) -> (Int, Int)? {
        guard let id else { return nil }

        for notebookIndex in notebooks.indices {
            if let noteIndex = notebooks[notebookIndex].notes.firstIndex(
                where: { $0.id == id }
            ) {
                return (notebookIndex, noteIndex)
            }
        }
        return nil
    }
}

extension LibraryStore {
    static let sample = LibraryStore(
        notebooks: [
            Notebook(
                title: "Começar",
                notes: [
                    Note(
                        title: "Bem-vindo",
                        markdown: """
                        # Bem-vindo

                        Este é o início do seu caderno.

                        - Organize o conteúdo por notebooks
                        - Escreva notas em **Markdown**
                        - Alterne entre edição e leitura
                        """
                    ),
                    Note(
                        title: "Ideias",
                        markdown: """
                        ## Ideias

                        Escreva aqui a sua próxima ideia.

                        ```swift
                        let aplicação = "NoteMD"
                        print(aplicação)
                        ```

                        ![Exemplo de imagem](https://picsum.photos/900/420)
                        """
                    )
                ]
            ),
            Notebook(title: "Arquivo")
        ]
    )
}

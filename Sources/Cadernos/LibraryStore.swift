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
    private var autosaveTasks: [Note.ID: Task<Void, Never>] = [:]

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
              let (notebookIndex, noteIndex) = noteLocation(withID: id)
        else { return }

        if !dirtyNoteIDs.contains(id),
           let path = notebooks[notebookIndex].notes[noteIndex].externalFilePath,
           let markdown = try? String(
               contentsOf: URL(fileURLWithPath: path),
               encoding: .utf8
           ) {
            notebooks[notebookIndex].notes[noteIndex].markdown = markdown
            notebooks[notebookIndex].notes[noteIndex].updatedAt =
                (try? URL(fileURLWithPath: path).resourceValues(
                    forKeys: [.contentModificationDateKey]
                ).contentModificationDate) ?? .now
        }

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
        let note = notebooks[notebookIndex].notes[noteIndex]
        writeRecoveryDraft(note)
        scheduleAutosave(for: note.id)
    }

    @discardableResult
    func addNotebook() -> Notebook.ID {
        let notebook = Notebook(title: "Novo notebook")
        notebooks.append(notebook)
        selectedNotebookID = notebook.id
        return notebook.id
    }

    func addNote(to notebookID: Notebook.ID) {
        guard let notebookIndex = notebooks.firstIndex(where: {
            $0.id == notebookID
        }) else { return }

        let note = Note(title: "Nova nota", markdown: "# Nova nota\n")
        notebooks[notebookIndex].notes.append(note)
        dirtyNoteIDs.insert(note.id)
        writeRecoveryDraft(note)
        scheduleAutosave(for: note.id)
        openNote(note.id)
    }

    @discardableResult
    func openMarkdownFile(at url: URL) -> Bool {
        let hasAccess = url.startAccessingSecurityScopedResource()
        defer {
            if hasAccess { url.stopAccessingSecurityScopedResource() }
        }

        do {
            if let existing = notebooks.flatMap(\.notes).first(where: {
                $0.externalFilePath == url.path
            }) {
                openNote(existing.id)
                return true
            }
            let markdown = try String(contentsOf: url, encoding: .utf8)
            let title = url.deletingPathExtension().lastPathComponent
            let savedRecord = externalFileRecords.first(where: { $0.path == url.path })
            let savedTags = savedRecord?.tags ?? []
            let note = Note(
                id: savedRecord?.id ?? UUID(),
                title: title.isEmpty ? tr("Sem título") : title,
                markdown: markdown,
                updatedAt: (try? url.resourceValues(
                    forKeys: [.contentModificationDateKey]
                ).contentModificationDate) ?? .now,
                tags: savedTags,
                externalFilePath: url.path
            )

            let notebookIndex: Int
            if let selectedNotebookID,
               let index = notebooks.firstIndex(where: { $0.id == selectedNotebookID }) {
                notebookIndex = index
            } else if !notebooks.isEmpty {
                notebookIndex = notebooks.startIndex
            } else {
                notebooks.append(Notebook(title: tr("Ficheiros abertos")))
                notebookIndex = notebooks.startIndex
            }

            notebooks[notebookIndex].notes.append(note)
            selectedNotebookID = notebooks[notebookIndex].id
            openNote(note.id)
            storeExternalFileRecord(id: note.id, path: url.path, tags: savedTags)
            return true
        } catch {
            return false
        }
    }

    var allTags: [String] {
        Array(Set(notebooks.flatMap(\.notes).flatMap(\.tags))).sorted {
            $0.localizedCaseInsensitiveCompare($1) == .orderedAscending
        }
    }

    func updateTags(_ tags: [String], for noteID: Note.ID) {
        guard let (notebookIndex, noteIndex) = noteLocation(withID: noteID) else { return }
        let normalized = Array(Set(tags.compactMap { value -> String? in
            let tag = value.trimmingCharacters(in: .whitespacesAndNewlines)
            return tag.isEmpty ? nil : tag
        })).sorted { $0.localizedCaseInsensitiveCompare($1) == .orderedAscending }
        notebooks[notebookIndex].notes[noteIndex].tags = normalized
        dirtyNoteIDs.insert(noteID)
        let note = notebooks[notebookIndex].notes[noteIndex]
        writeRecoveryDraft(note)
        scheduleAutosave(for: noteID)
        if let path = note.externalFilePath {
            storeExternalFileRecord(id: note.id, path: path, tags: normalized)
        }
    }

    func reopenExternalFiles() {
        for record in externalFileRecords
        where FileManager.default.fileExists(atPath: record.path) {
            _ = openMarkdownFile(at: URL(fileURLWithPath: record.path))
        }
    }

    func forgetExternalFile(at path: String) {
        var records = externalFileRecords
        records.removeAll { $0.path == path }
        saveExternalFileRecords(records)
    }

    func notes(matching query: String, tag: String?) -> Set<Note.ID> {
        let needle = query.trimmingCharacters(in: .whitespacesAndNewlines)
        return Set(notebooks.flatMap(\.notes).filter { note in
            let matchesTag = tag.map { selected in
                note.tags.contains {
                    $0.caseInsensitiveCompare(selected) == .orderedSame
                }
            } ?? true
            let matchesQuery = needle.isEmpty
                || note.title.localizedCaseInsensitiveContains(needle)
                || note.markdown.localizedCaseInsensitiveContains(needle)
                || note.tags.contains { $0.localizedCaseInsensitiveContains(needle) }
            return matchesTag && matchesQuery
        }.map(\.id))
    }

    func restoreRecoveryDrafts() {
        let decoder = JSONDecoder()
        guard let urls = try? FileManager.default.contentsOfDirectory(
            at: recoveryDirectory,
            includingPropertiesForKeys: nil
        ) else { return }
        let existingIDs = Set(notebooks.flatMap(\.notes).map(\.id))
        let drafts = urls.compactMap { url -> RecoveryDraft? in
            guard let data = try? Data(contentsOf: url) else { return nil }
            return try? decoder.decode(RecoveryDraft.self, from: data)
        }.filter { !existingIDs.contains($0.id) }
        guard !drafts.isEmpty else { return }

        let recovered = drafts.map {
            Note(id: $0.id, title: $0.title, markdown: $0.markdown,
                 updatedAt: $0.updatedAt, tags: $0.tags,
                 externalFilePath: $0.externalFilePath)
        }
        let notebook = Notebook(title: tr("Recuperadas"), notes: recovered)
        notebooks.append(notebook)
        selectedNotebookID = notebook.id
        dirtyNoteIDs.formUnion(recovered.map(\.id))
        if let first = recovered.first { openNote(first.id) }
    }

    func clearRecoveryDraft(for id: Note.ID) {
        try? FileManager.default.removeItem(
            at: recoveryDirectory.appendingPathComponent("\(id.uuidString).json")
        )
    }

    private func scheduleAutosave(for id: Note.ID) {
        if UserDefaults.standard.object(forKey: "autosaveEnabled") != nil,
           !UserDefaults.standard.bool(forKey: "autosaveEnabled") {
            return
        }
        autosaveTasks[id]?.cancel()
        autosaveTasks[id] = Task { [weak self] in
            try? await Task.sleep(for: .seconds(1.2))
            guard !Task.isCancelled else { return }
            await MainActor.run { _ = self?.autosaveNote(id) }
        }
    }

    private func writeRecoveryDraft(_ note: Note) {
        do {
            try FileManager.default.createDirectory(
                at: recoveryDirectory,
                withIntermediateDirectories: true
            )
            let draft = RecoveryDraft(
                id: note.id, title: note.title, markdown: note.markdown,
                updatedAt: note.updatedAt, tags: note.tags,
                externalFilePath: note.externalFilePath
            )
            try JSONEncoder().encode(draft).write(
                to: recoveryDirectory.appendingPathComponent("\(note.id.uuidString).json"),
                options: .atomic
            )
        } catch { }
    }

    private var recoveryDirectory: URL {
        FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("NoteMD/Recovery", isDirectory: true)
    }

    private var externalFileRecords: [ExternalFileRecord] {
        guard let data = UserDefaults.standard.data(forKey: "externalFileRecords") else {
            return []
        }
        return (try? JSONDecoder().decode([ExternalFileRecord].self, from: data)) ?? []
    }

    private func storeExternalFileRecord(id: UUID, path: String, tags: [String]) {
        var records = externalFileRecords
        records.removeAll { $0.path == path }
        records.append(ExternalFileRecord(id: id, path: path, tags: tags))
        saveExternalFileRecords(records)
    }

    private func saveExternalFileRecords(_ records: [ExternalFileRecord]) {
        guard let data = try? JSONEncoder().encode(records) else { return }
        UserDefaults.standard.set(data, forKey: "externalFileRecords")
    }

    func isDirty(_ id: Note.ID) -> Bool {
        dirtyNoteIDs.contains(id)
    }

    func markSaved(_ id: Note.ID) {
        dirtyNoteIDs.remove(id)
        clearRecoveryDraft(for: id)
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
        clearRecoveryDraft(for: noteID)
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

    func updateNotebookTitle(_ title: String, notebookID: Notebook.ID) {
        guard let index = notebooks.firstIndex(where: { $0.id == notebookID }) else {
            return
        }
        notebooks[index].title = title
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

private struct RecoveryDraft: Codable {
    let id: UUID
    let title: String
    let markdown: String
    let updatedAt: Date
    let tags: [String]
    let externalFilePath: String?
}

private struct ExternalFileRecord: Codable {
    let id: UUID?
    let path: String
    let tags: [String]

    init(id: UUID, path: String, tags: [String]) {
        self.id = id
        self.path = path
        self.tags = tags
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

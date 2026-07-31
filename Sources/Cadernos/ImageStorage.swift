import AppKit
import Foundation

extension LibraryStore {
    @discardableResult
    func renameNotebook(_ notebookID: Notebook.ID, to proposedTitle: String) -> Bool {
        guard let notebook = notebooks.first(where: { $0.id == notebookID }) else {
            return false
        }

        let title = proposedTitle.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !title.isEmpty else { return false }

        let desiredFolderName = safeFilename(title)
        if let rootURL = storageRootURL,
           let previousFolderName = notebook.storageFolderName,
           previousFolderName != desiredFolderName {
            let previousURL = rootURL.appendingPathComponent(
                previousFolderName,
                isDirectory: true
            )
            let desiredURL = rootURL.appendingPathComponent(
                desiredFolderName,
                isDirectory: true
            )

            do {
                if FileManager.default.fileExists(atPath: desiredURL.path) {
                    showStorageError(
                        title: "Já existe um notebook com este nome",
                        message: "Escolha outro nome."
                    )
                    return false
                }
                if FileManager.default.fileExists(atPath: previousURL.path) {
                    try FileManager.default.moveItem(at: previousURL, to: desiredURL)
                }
                markNotebookStored(notebookID, folderName: desiredFolderName)
            } catch {
                showStorageError(
                    title: "Não foi possível renomear o notebook",
                    message: error.localizedDescription
                )
                return false
            }
        }

        updateNotebookTitle(title, notebookID: notebookID)
        return true
    }

    func setNotebookColor(_ colorHex: String?, notebookID: Notebook.ID) {
        guard let notebook = notebooks.first(where: { $0.id == notebookID }) else {
            return
        }
        updateNotebookColor(colorHex, notebookID: notebookID)

        guard let rootURL = storageRootURL else { return }
        let folderName = notebook.storageFolderName ?? safeFilename(notebook.title)
        let folderURL = rootURL.appendingPathComponent(folderName, isDirectory: true)
        let metadataURL = folderURL.appendingPathComponent(".notebook.json")

        do {
            try FileManager.default.createDirectory(
                at: folderURL,
                withIntermediateDirectories: true
            )
            let metadata = NotebookMetadata(
                colorHex: colorHex,
                noteOrder: notebook.notes.map(\.id)
            )
            try JSONEncoder().encode(metadata).write(to: metadataURL, options: .atomic)
            markNotebookStored(notebookID, folderName: folderName)
        } catch {
            showStorageError(
                title: "Não foi possível guardar a cor",
                message: error.localizedDescription
            )
        }
    }

    func setNoteColor(_ colorHex: String?, noteID: Note.ID) {
        updateNoteColor(colorHex, noteID: noteID)
        guard let location = noteLocationForStorage(for: noteID) else { return }

        guard let rootURL = storageRootURL else { return }
        let folderName = location.note.storageFolderName
            ?? safeFilename(location.note.title)
        let folderURL = rootURL
            .appendingPathComponent(location.notebookFolder, isDirectory: true)
            .appendingPathComponent(folderName, isDirectory: true)
        let metadataURL = folderURL.appendingPathComponent(".note.json")

        do {
            try FileManager.default.createDirectory(
                at: folderURL,
                withIntermediateDirectories: true
            )
            let metadata = NoteMetadata(
                id: location.note.id,
                colorHex: colorHex,
                tags: location.note.tags
            )
            try JSONEncoder().encode(metadata).write(to: metadataURL, options: .atomic)
            setStorageFolderName(folderName, for: noteID)
        } catch {
            showStorageError(
                title: "Não foi possível guardar a cor",
                message: error.localizedDescription
            )
        }
    }

    func deleteNote(_ noteID: Note.ID) {
        guard let location = noteLocationForStorage(for: noteID) else { return }
        let alert = NSAlert()
        alert.alertStyle = .warning
        alert.messageText = tr("Remover esta nota?")
        let isExternal = location.note.externalFilePath != nil
        alert.informativeText = isExternal
            ? tr("O ficheiro original não será apagado.")
            : tr("A nota e as suas imagens serão movidas para o Lixo.")
        alert.addButton(withTitle: isExternal ? tr("Fechar ficheiro") : tr("Mover para o Lixo"))
        alert.addButton(withTitle: tr("Cancelar"))
        guard alert.runModal() == .alertFirstButtonReturn else { return }

        if let rootURL = storageRootURL,
           let storedFolder = location.note.storageFolderName {
            let folderURL = rootURL
                .appendingPathComponent(location.notebookFolder, isDirectory: true)
                .appendingPathComponent(storedFolder, isDirectory: true)
            guard moveToTrashIfPresent(folderURL) else { return }
        }
        if let path = location.note.externalFilePath {
            forgetExternalFile(at: path)
        }
        removeNoteFromLibrary(noteID)
    }

    func deleteNotebook(_ notebookID: Notebook.ID) {
        guard let notebook = notebooks.first(where: { $0.id == notebookID }) else {
            return
        }
        let alert = NSAlert()
        alert.alertStyle = .warning
        alert.messageText = tr("Remover este notebook?")
        alert.informativeText = tr("Todas as notas e imagens deste notebook serão movidas para o Lixo.")
        alert.addButton(withTitle: tr("Mover para o Lixo"))
        alert.addButton(withTitle: tr("Cancelar"))
        guard alert.runModal() == .alertFirstButtonReturn else { return }

        if let rootURL = storageRootURL,
           let storedFolder = notebook.storageFolderName {
            let folderURL = rootURL.appendingPathComponent(
                storedFolder,
                isDirectory: true
            )
            guard moveToTrashIfPresent(folderURL) else { return }
        }
        removeNotebookFromLibrary(notebookID)
    }

    func loadFromStorage(force: Bool = false) {
        guard let rootPath = UserDefaults.standard.string(forKey: "notesFolderPath"),
              !rootPath.isEmpty
        else { return }
        guard force || loadedStoragePath != rootPath else { return }

        let rootURL = URL(fileURLWithPath: rootPath, isDirectory: true)
        let keys: Set<URLResourceKey> = [
            .isDirectoryKey,
            .isHiddenKey,
            .contentModificationDateKey
        ]

        do {
            let notebookURLs = try FileManager.default.contentsOfDirectory(
                at: rootURL,
                includingPropertiesForKeys: Array(keys),
                options: [.skipsHiddenFiles]
            )
            var loaded: [Notebook] = []

            for notebookURL in notebookURLs.sorted(by: filenameOrder) {
                let values = try notebookURL.resourceValues(forKeys: keys)
                guard values.isDirectory == true, values.isHidden != true else { continue }

                let childURLs = try FileManager.default.contentsOfDirectory(
                    at: notebookURL,
                    includingPropertiesForKeys: Array(keys),
                    options: [.skipsHiddenFiles]
                )
                var notes: [Note] = []

                for noteFolderURL in childURLs.sorted(by: filenameOrder) {
                    let folderValues = try noteFolderURL.resourceValues(forKeys: keys)
                    guard folderValues.isDirectory == true,
                          folderValues.isHidden != true
                    else { continue }

                    let markdownURL = noteFolderURL.appendingPathComponent("note.md")
                    guard FileManager.default.fileExists(atPath: markdownURL.path) else {
                        continue
                    }

                    let markdown = try String(contentsOf: markdownURL, encoding: .utf8)
                    let fileValues = try markdownURL.resourceValues(forKeys: keys)
                    let metadata = noteMetadata(at: noteFolderURL)
                    notes.append(
                        Note(
                            id: metadata?.id ?? UUID(),
                            title: noteFolderURL.lastPathComponent,
                            markdown: markdown,
                            updatedAt: fileValues.contentModificationDate ?? .now,
                            storageFolderName: noteFolderURL.lastPathComponent,
                            colorHex: metadata?.colorHex,
                            tags: metadata?.tags ?? []
                        )
                    )
                }

                let notebookMetadata = notebookMetadata(at: notebookURL)
                if let order = notebookMetadata?.noteOrder {
                    notes.sort {
                        (order.firstIndex(of: $0.id) ?? Int.max)
                            < (order.firstIndex(of: $1.id) ?? Int.max)
                    }
                }
                loaded.append(
                    Notebook(
                        title: notebookURL.lastPathComponent,
                        notes: notes,
                        storageFolderName: notebookURL.lastPathComponent,
                        colorHex: notebookMetadata?.colorHex
                    )
                )
            }

            replaceLibrary(with: loaded)
            loadedStoragePath = rootPath
        } catch {
            showStorageError(
                title: "Não foi possível carregar as notas",
                message: error.localizedDescription
            )
        }
    }

    func saveActiveNote() -> Bool {
        guard let activeNoteID else { return false }
        return saveNote(activeNoteID, showingErrors: true)
    }

    func autosaveNote(_ id: Note.ID) -> Bool {
        saveNote(id, showingErrors: false)
    }

    func saveAllNotes() -> Bool {
        var succeeded = true
        for id in dirtyNoteIDs {
            if !saveNote(id, showingErrors: true) {
                succeeded = false
            }
        }
        return succeeded
    }

    func closeNoteWithConfirmation(_ id: Note.ID) {
        guard isDirty(id) else {
            closeNote(id)
            return
        }

        let alert = NSAlert()
        alert.messageText = tr("Guardar alterações?")
        alert.informativeText = tr("A nota foi alterada desde a última gravação.")
        alert.addButton(withTitle: tr("Guardar"))
        alert.addButton(withTitle: tr("Não guardar"))
        alert.addButton(withTitle: tr("Cancelar"))

        switch alert.runModal() {
        case .alertFirstButtonReturn:
            if saveNote(id, showingErrors: true) {
                closeNote(id)
            }
        case .alertSecondButtonReturn:
            markSaved(id)
            closeNote(id)
        default:
            break
        }
    }

    private func saveNote(_ id: Note.ID, showingErrors: Bool) -> Bool {
        guard let location = noteLocationForStorage(for: id) else { return false }

        if let path = location.note.externalFilePath {
            do {
                let url = URL(fileURLWithPath: path)
                let diskDate = try? url.resourceValues(
                    forKeys: [.contentModificationDateKey]
                ).contentModificationDate
                if let diskDate,
                   let knownDate = location.note.externalModificationDate,
                   diskDate > knownDate.addingTimeInterval(0.01) {
                    guard showingErrors else { return false }
                    let alert = NSAlert()
                    alert.messageText = tr("O ficheiro foi alterado noutra aplicação")
                    alert.informativeText = tr("Escolha qual versão pretende manter.")
                    alert.addButton(withTitle: tr("Substituir ficheiro"))
                    alert.addButton(withTitle: tr("Recarregar ficheiro"))
                    alert.addButton(withTitle: tr("Cancelar"))
                    switch alert.runModal() {
                    case .alertFirstButtonReturn:
                        break
                    case .alertSecondButtonReturn:
                        let diskMarkdown = try String(contentsOf: url, encoding: .utf8)
                        updateExternalState(
                            for: id,
                            markdown: diskMarkdown,
                            modificationDate: diskDate
                        )
                        markSaved(id)
                        return true
                    default:
                        return false
                    }
                }
                createVersionSnapshot(for: location.note)
                try Data(location.note.markdown.utf8).write(
                    to: url,
                    options: .atomic
                )
                let savedDate = try? url.resourceValues(
                    forKeys: [.contentModificationDateKey]
                ).contentModificationDate
                updateExternalState(for: id, modificationDate: savedDate)
                markSaved(id)
                clearRecoveryDraft(for: id)
                return true
            } catch {
                if showingErrors {
                    showStorageError(
                        title: tr("Não foi possível guardar"),
                        message: error.localizedDescription
                    )
                }
                return false
            }
        }

        guard let rootPath = UserDefaults.standard.string(forKey: "notesFolderPath"),
              !rootPath.isEmpty
        else {
            if showingErrors {
                showStorageError(
                    title: "Escolha uma pasta",
                    message: "Abra as Configurações e escolha onde pretende guardar as notas."
                )
            }
            return false
        }

        let notebookFolder = URL(fileURLWithPath: rootPath, isDirectory: true)
            .appendingPathComponent(location.notebookFolder, isDirectory: true)
        let desiredFolderName = safeFilename(location.note.title)
        let previousFolderName = location.note.storageFolderName ?? desiredFolderName
        let previousFolder = notebookFolder.appendingPathComponent(
            previousFolderName,
            isDirectory: true
        )
        let noteFolder = notebookFolder.appendingPathComponent(
            desiredFolderName,
            isDirectory: true
        )
        let markdownURL = noteFolder.appendingPathComponent("note.md")

        do {
            createVersionSnapshot(for: location.note)
            try FileManager.default.createDirectory(
                at: notebookFolder,
                withIntermediateDirectories: true
            )
            if previousFolderName != desiredFolderName,
               FileManager.default.fileExists(atPath: previousFolder.path) {
                guard !FileManager.default.fileExists(atPath: noteFolder.path) else {
                    showStorageError(
                        title: "Já existe uma nota com este nome",
                        message: "Escolha outro nome antes de guardar."
                    )
                    return false
                }
                try FileManager.default.moveItem(at: previousFolder, to: noteFolder)
            }
            try FileManager.default.createDirectory(
                at: noteFolder,
                withIntermediateDirectories: true
            )
            try Data(location.note.markdown.utf8).write(to: markdownURL, options: .atomic)
            let metadata = NoteMetadata(
                id: location.note.id,
                colorHex: location.note.colorHex,
                tags: location.note.tags
            )
            try JSONEncoder().encode(metadata).write(
                to: noteFolder.appendingPathComponent(".note.json"),
                options: .atomic
            )
            setStorageFolderName(desiredFolderName, for: id)
            markSaved(id)
            clearRecoveryDraft(for: id)
            return true
        } catch {
            if showingErrors {
                showStorageError(
                    title: "Não foi possível guardar",
                    message: error.localizedDescription
                )
            }
            return false
        }
    }

    var activeNoteAssetsURL: URL? {
        if let path = selectedNote?.externalFilePath {
            return URL(fileURLWithPath: path).deletingLastPathComponent()
        }
        guard let rootPath = UserDefaults.standard.string(forKey: "notesFolderPath"),
              !rootPath.isEmpty,
              let activeNoteID,
              let location = noteLocationForStorage(for: activeNoteID)
        else { return nil }

        return URL(fileURLWithPath: rootPath, isDirectory: true)
            .appendingPathComponent(location.notebookFolder, isDirectory: true)
            .appendingPathComponent(
                location.note.storageFolderName ?? safeFilename(location.note.title),
                isDirectory: true
            )
            .appendingPathComponent("assets", isDirectory: true)
    }

    func storePastedImage(_ image: NSImage) -> String? {
        guard let rootPath = UserDefaults.standard.string(forKey: "notesFolderPath"),
              !rootPath.isEmpty
        else {
            showStorageError(
                title: "Escolha uma pasta",
                message: "Antes de colar imagens, escolha a pasta das notas nas Configurações."
            )
            return nil
        }

        guard let activeNoteID,
              let location = noteLocationForStorage(for: activeNoteID),
              let data = pngData(for: image)
        else {
            showStorageError(
                title: "Não foi possível colar a imagem",
                message: "Selecione uma nota válida e tente novamente."
            )
            return nil
        }

        let assetsURL = URL(fileURLWithPath: rootPath, isDirectory: true)
            .appendingPathComponent(location.notebookFolder, isDirectory: true)
            .appendingPathComponent(
                location.note.storageFolderName ?? safeFilename(location.note.title),
                isDirectory: true
            )
            .appendingPathComponent("assets", isDirectory: true)
        let filename = "image-\(UUID().uuidString.lowercased()).png"
        let imageURL = assetsURL.appendingPathComponent(filename)

        do {
            try FileManager.default.createDirectory(
                at: assetsURL,
                withIntermediateDirectories: true
            )
            try data.write(to: imageURL, options: .atomic)
            if location.note.storageFolderName == nil {
                setStorageFolderName(
                    safeFilename(location.note.title),
                    for: activeNoteID
                )
            }
            return "![Imagem](assets/\(filename))"
        } catch {
            showStorageError(
                title: "Não foi possível guardar a imagem",
                message: error.localizedDescription
            )
            return nil
        }
    }

    private func noteLocationForStorage(
        for noteID: Note.ID
    ) -> (notebookFolder: String, note: Note)? {
        for notebook in notebooks {
            if let note = notebook.notes.first(where: { $0.id == noteID }) {
                return (
                    notebook.storageFolderName ?? safeFilename(notebook.title),
                    note
                )
            }
        }
        return nil
    }

    private func pngData(for image: NSImage) -> Data? {
        guard let tiff = image.tiffRepresentation,
              let representation = NSBitmapImageRep(data: tiff)
        else { return nil }
        return representation.representation(using: .png, properties: [:])
    }

    private func safeFilename(_ value: String) -> String {
        let forbidden = CharacterSet(charactersIn: "/:")
        let result = value.components(separatedBy: forbidden)
            .joined(separator: "-")
            .trimmingCharacters(in: .whitespacesAndNewlines)
        return result.isEmpty ? "Sem título" : result
    }

    private var storageRootURL: URL? {
        guard let path = UserDefaults.standard.string(forKey: "notesFolderPath"),
              !path.isEmpty
        else { return nil }
        return URL(fileURLWithPath: path, isDirectory: true)
    }

    private func moveToTrashIfPresent(_ url: URL) -> Bool {
        guard FileManager.default.fileExists(atPath: url.path) else { return true }
        do {
            var resultingURL: NSURL?
            try FileManager.default.trashItem(
                at: url,
                resultingItemURL: &resultingURL
            )
            return true
        } catch {
            showStorageError(
                title: "Não foi possível remover",
                message: error.localizedDescription
            )
            return false
        }
    }

    private func filenameOrder(_ left: URL, _ right: URL) -> Bool {
        left.lastPathComponent.localizedStandardCompare(right.lastPathComponent) == .orderedAscending
    }

    private func notebookMetadata(at folderURL: URL) -> NotebookMetadata? {
        let url = folderURL.appendingPathComponent(".notebook.json")
        guard let data = try? Data(contentsOf: url),
              let metadata = try? JSONDecoder().decode(NotebookMetadata.self, from: data)
        else { return nil }
        return metadata
    }

    private func noteMetadata(at folderURL: URL) -> NoteMetadata? {
        let url = folderURL.appendingPathComponent(".note.json")
        guard let data = try? Data(contentsOf: url),
              let metadata = try? JSONDecoder().decode(NoteMetadata.self, from: data)
        else { return nil }
        return metadata
    }

    private func showStorageError(title: String, message: String) {
        let alert = NSAlert()
        alert.alertStyle = .warning
        alert.messageText = title
        alert.informativeText = message
        alert.runModal()
    }

    func versions(for noteID: Note.ID) -> [NoteVersion] {
        let folder = versionsDirectory.appendingPathComponent(
            noteID.uuidString,
            isDirectory: true
        )
        guard let urls = try? FileManager.default.contentsOfDirectory(
            at: folder,
            includingPropertiesForKeys: nil
        ) else { return [] }
        return urls.compactMap { url in
            guard let data = try? Data(contentsOf: url) else { return nil }
            return try? JSONDecoder().decode(NoteVersion.self, from: data)
        }.sorted { $0.createdAt > $1.createdAt }
    }

    func restoreVersion(_ version: NoteVersion) {
        guard activeNoteID == version.noteID else { return }
        updateSelectedNote(title: version.title, markdown: version.markdown)
        updateTags(version.tags, for: version.noteID)
    }

    func deleteVersion(_ version: NoteVersion) {
        let url = versionsDirectory
            .appendingPathComponent(version.noteID.uuidString, isDirectory: true)
            .appendingPathComponent("\(version.id.uuidString).json")
        try? FileManager.default.removeItem(at: url)
        objectWillChange.send()
    }

    func versionDifference(_ version: NoteVersion) -> String {
        guard let current = note(withID: version.noteID) else { return "" }
        let oldLines = Set(version.markdown.components(separatedBy: .newlines))
        let newLines = Set(current.markdown.components(separatedBy: .newlines))
        return "+\(newLines.subtracting(oldLines).count)  −\(oldLines.subtracting(newLines).count)"
    }

    func persistNoteOrder(in notebookID: Notebook.ID) {
        guard let notebook = notebooks.first(where: { $0.id == notebookID }),
              let rootURL = storageRootURL
        else { return }
        let folderName = notebook.storageFolderName ?? safeFilename(notebook.title)
        let folderURL = rootURL.appendingPathComponent(folderName, isDirectory: true)
        do {
            try FileManager.default.createDirectory(
                at: folderURL,
                withIntermediateDirectories: true
            )
            let metadata = NotebookMetadata(
                colorHex: notebook.colorHex,
                noteOrder: notebook.notes.map(\.id)
            )
            try JSONEncoder().encode(metadata).write(
                to: folderURL.appendingPathComponent(".notebook.json"),
                options: .atomic
            )
            markNotebookStored(notebookID, folderName: folderName)
        } catch { }
    }

    private func createVersionSnapshot(for note: Note) {
        let folder = versionsDirectory.appendingPathComponent(
            note.id.uuidString,
            isDirectory: true
        )
        do {
            try FileManager.default.createDirectory(
                at: folder,
                withIntermediateDirectories: true
            )
            let version = NoteVersion(
                id: UUID(), noteID: note.id, createdAt: .now,
                title: note.title, markdown: note.markdown, tags: note.tags
            )
            try JSONEncoder().encode(version).write(
                to: folder.appendingPathComponent("\(version.id.uuidString).json"),
                options: .atomic
            )
            let urls = try FileManager.default.contentsOfDirectory(
                at: folder,
                includingPropertiesForKeys: [.contentModificationDateKey]
            ).sorted {
                let left = try? $0.resourceValues(
                    forKeys: [.contentModificationDateKey]
                ).contentModificationDate
                let right = try? $1.resourceValues(
                    forKeys: [.contentModificationDateKey]
                ).contentModificationDate
                return (left ?? .distantPast) > (right ?? .distantPast)
            }
            for oldURL in urls.dropFirst(30) {
                try? FileManager.default.removeItem(at: oldURL)
            }
        } catch { }
    }

    private var versionsDirectory: URL {
        FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("NoteMD/Versions", isDirectory: true)
    }
}

private struct NotebookMetadata: Codable {
    let colorHex: String?
    let noteOrder: [UUID]?

    init(colorHex: String?, noteOrder: [UUID]) {
        self.colorHex = colorHex
        self.noteOrder = noteOrder
    }
}

private struct NoteMetadata: Codable {
    let id: UUID?
    let colorHex: String?
    let tags: [String]?

    init(id: UUID? = nil, colorHex: String?, tags: [String]) {
        self.id = id
        self.colorHex = colorHex
        self.tags = tags
    }
}

struct NoteVersion: Codable, Identifiable {
    let id: UUID
    let noteID: UUID
    let createdAt: Date
    let title: String
    let markdown: String
    let tags: [String]
}

import AppKit
import Foundation

extension LibraryStore {
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
            let metadata = NotebookMetadata(colorHex: colorHex)
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
            let metadata = NoteMetadata(colorHex: colorHex)
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
        alert.informativeText = tr("A nota e as suas imagens serão movidas para o Lixo.")
        alert.addButton(withTitle: tr("Mover para o Lixo"))
        alert.addButton(withTitle: tr("Cancelar"))
        guard alert.runModal() == .alertFirstButtonReturn else { return }

        if let rootURL = storageRootURL,
           let storedFolder = location.note.storageFolderName {
            let folderURL = rootURL
                .appendingPathComponent(location.notebookFolder, isDirectory: true)
                .appendingPathComponent(storedFolder, isDirectory: true)
            guard moveToTrashIfPresent(folderURL) else { return }
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
                    notes.append(
                        Note(
                            title: noteFolderURL.lastPathComponent,
                            markdown: markdown,
                            updatedAt: fileValues.contentModificationDate ?? .now,
                            storageFolderName: noteFolderURL.lastPathComponent,
                            colorHex: noteColor(at: noteFolderURL)
                        )
                    )
                }

                loaded.append(
                    Notebook(
                        title: notebookURL.lastPathComponent,
                        notes: notes,
                        storageFolderName: notebookURL.lastPathComponent,
                        colorHex: notebookColor(at: notebookURL)
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

        guard let location = noteLocationForStorage(for: id) else { return false }
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
            setStorageFolderName(desiredFolderName, for: id)
            markSaved(id)
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

    private func notebookColor(at folderURL: URL) -> String? {
        let url = folderURL.appendingPathComponent(".notebook.json")
        guard let data = try? Data(contentsOf: url),
              let metadata = try? JSONDecoder().decode(NotebookMetadata.self, from: data)
        else { return nil }
        return metadata.colorHex
    }

    private func noteColor(at folderURL: URL) -> String? {
        let url = folderURL.appendingPathComponent(".note.json")
        guard let data = try? Data(contentsOf: url),
              let metadata = try? JSONDecoder().decode(NoteMetadata.self, from: data)
        else { return nil }
        return metadata.colorHex
    }

    private func showStorageError(title: String, message: String) {
        let alert = NSAlert()
        alert.alertStyle = .warning
        alert.messageText = title
        alert.informativeText = message
        alert.runModal()
    }
}

private struct NotebookMetadata: Codable {
    let colorHex: String?
}

private struct NoteMetadata: Codable {
    let colorHex: String?
}

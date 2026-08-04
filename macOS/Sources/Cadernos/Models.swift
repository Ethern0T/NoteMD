import Foundation

enum NoteSortOrder: String, CaseIterable {
    case manual, title, updated
}

struct NoteTemplate: Identifiable {
    let id: String
    let name: String
    let title: String
    let markdown: String

    static let builtIn: [NoteTemplate] = [
        .init(id: "meeting", name: "Reunião", title: "Reunião", markdown: "# Reunião\n\n**Data:** \n**Participantes:** \n\n## Agenda\n\n- \n\n## Notas\n\n## Ações\n\n- [ ] \n"),
        .init(id: "daily", name: "Diário", title: "Diário", markdown: "# Diário\n\n## Prioridades\n\n- [ ] \n\n## Notas\n\n"),
        .init(id: "project", name: "Projeto", title: "Projeto", markdown: "# Projeto\n\n## Objetivo\n\n## Tarefas\n\n- [ ] \n\n## Referências\n\n"),
        .init(id: "checklist", name: "Checklist", title: "Checklist", markdown: "# Checklist\n\n- [ ] \n")
    ]
}

struct Notebook: Identifiable, Hashable {
    let id: UUID
    var title: String
    var notes: [Note]
    var storageFolderName: String?
    var colorHex: String?

    init(
        id: UUID = UUID(),
        title: String,
        notes: [Note] = [],
        storageFolderName: String? = nil,
        colorHex: String? = nil
    ) {
        self.id = id
        self.title = title
        self.notes = notes
        self.storageFolderName = storageFolderName
        self.colorHex = colorHex
    }
}

struct Note: Identifiable, Hashable {
    let id: UUID
    var title: String
    var markdown: String
    var updatedAt: Date
    var storageFolderName: String?
    var colorHex: String?
    var tags: [String]
    var externalFilePath: String?
    var externalModificationDate: Date?

    init(
        id: UUID = UUID(),
        title: String,
        markdown: String = "",
        updatedAt: Date = .now,
        storageFolderName: String? = nil,
        colorHex: String? = nil,
        tags: [String] = [],
        externalFilePath: String? = nil,
        externalModificationDate: Date? = nil
    ) {
        self.id = id
        self.title = title
        self.markdown = markdown
        self.updatedAt = updatedAt
        self.storageFolderName = storageFolderName
        self.colorHex = colorHex
        self.tags = tags
        self.externalFilePath = externalFilePath
        self.externalModificationDate = externalModificationDate
    }
}

import Foundation

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

    init(
        id: UUID = UUID(),
        title: String,
        markdown: String = "",
        updatedAt: Date = .now,
        storageFolderName: String? = nil,
        colorHex: String? = nil,
        tags: [String] = [],
        externalFilePath: String? = nil
    ) {
        self.id = id
        self.title = title
        self.markdown = markdown
        self.updatedAt = updatedAt
        self.storageFolderName = storageFolderName
        self.colorHex = colorHex
        self.tags = tags
        self.externalFilePath = externalFilePath
    }
}

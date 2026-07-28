import SwiftUI

struct VisualMarkdownEditor: View {
    @Binding var markdown: String
    let assetsURL: URL?

    @State private var blocks: [VisualBlock] = []
    @State private var isLoading = true
    @State private var lastWrittenMarkdown = ""

    var body: some View {
        ScrollView {
            LazyVStack(alignment: .leading, spacing: 14) {
                ForEach($blocks) { $block in
                    VisualBlockView(block: $block, assetsURL: assetsURL)
                        .contextMenu {
                            Button {
                                duplicate(block.id)
                            } label: {
                                Label(tr("Duplicar bloco"), systemImage: "plus.square.on.square")
                            }
                            Button(role: .destructive) {
                                remove(block.id)
                            } label: {
                                Label(tr("Remover bloco"), systemImage: "trash")
                            }
                        }
                }

                Menu {
                    Button(tr("Parágrafo")) { append(.paragraph()) }
                    Button(tr("Título")) { append(.heading()) }
                    Button(tr("Lista")) { append(.bullet()) }
                    Button(tr("Lista de tarefas")) { append(.task()) }
                    Button(tr("Citação")) { append(.quote()) }
                    Button(tr("Bloco de código")) { append(.code()) }
                    Button(tr("Tabela")) { append(.table()) }
                    Button(tr("Separador horizontal")) { append(.divider()) }
                } label: {
                    Label(tr("Adicionar bloco"), systemImage: "plus.circle")
                }
                .menuStyle(.borderlessButton)
                .padding(.top, 8)
            }
            .frame(maxWidth: 780, alignment: .leading)
            .padding(28)
            .frame(maxWidth: .infinity)
        }
        .background(Color(nsColor: .textBackgroundColor))
        .onAppear {
            blocks = VisualMarkdownParser.parse(markdown)
            if blocks.isEmpty {
                blocks = [.paragraph()]
            }
            lastWrittenMarkdown = markdown
            DispatchQueue.main.async {
                isLoading = false
            }
        }
        .onChange(of: blocks) { _, newBlocks in
            guard !isLoading else { return }
            let serialized = VisualMarkdownParser.serialize(newBlocks)
            lastWrittenMarkdown = serialized
            markdown = serialized
        }
        .onChange(of: markdown) { _, newMarkdown in
            guard !isLoading, newMarkdown != lastWrittenMarkdown else { return }
            isLoading = true
            blocks = VisualMarkdownParser.parse(newMarkdown)
            lastWrittenMarkdown = newMarkdown
            DispatchQueue.main.async {
                isLoading = false
            }
        }
    }

    private func append(_ block: VisualBlock) {
        blocks.append(block)
    }

    private func duplicate(_ id: VisualBlock.ID) {
        guard let index = blocks.firstIndex(where: { $0.id == id }) else { return }
        var copy = blocks[index]
        copy = copy.copy()
        blocks.insert(copy, at: index + 1)
    }

    private func remove(_ id: VisualBlock.ID) {
        blocks.removeAll { $0.id == id }
        if blocks.isEmpty {
            blocks = [.paragraph()]
        }
    }
}

private struct VisualBlockView: View {
    @Binding var block: VisualBlock
    let assetsURL: URL?

    var body: some View {
        HStack(alignment: .top, spacing: 10) {
            Image(systemName: block.kind.icon)
                .foregroundStyle(.tertiary)
                .frame(width: 18, height: 24)

            content
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(10)
        .background(Color.primary.opacity(0.025))
        .clipShape(RoundedRectangle(cornerRadius: 8))
    }

    @ViewBuilder
    private var content: some View {
        switch block.kind {
        case .paragraph:
            growingEditor(font: .body)

        case .heading:
            HStack {
                Picker("H", selection: $block.level) {
                    ForEach(1...6, id: \.self) { Text("H\($0)").tag($0) }
                }
                .labelsHidden()
                .frame(width: 64)
                TextField(tr("Título"), text: $block.text)
                    .textFieldStyle(.plain)
                    .font(headingFont)
                    .fontWeight(.bold)
            }

        case .bullet:
            HStack(alignment: .firstTextBaseline) {
                Text("•")
                TextField(tr("Conteúdo"), text: $block.text)
                    .textFieldStyle(.plain)
            }

        case .numbered:
            HStack(alignment: .firstTextBaseline) {
                Text("\(block.number).")
                    .foregroundStyle(.secondary)
                TextField(tr("Conteúdo"), text: $block.text)
                    .textFieldStyle(.plain)
            }

        case .task:
            HStack {
                Toggle("", isOn: $block.isChecked)
                    .labelsHidden()
                TextField(tr("Conteúdo"), text: $block.text)
                    .textFieldStyle(.plain)
                    .strikethrough(block.isChecked)
            }

        case .quote:
            HStack {
                RoundedRectangle(cornerRadius: 2)
                    .fill(Color.secondary.opacity(0.5))
                    .frame(width: 4)
                growingEditor(font: .body)
                    .foregroundStyle(.secondary)
            }

        case .code:
            VStack(alignment: .leading, spacing: 6) {
                TextField(tr("Linguagem"), text: $block.language)
                    .textFieldStyle(.plain)
                    .font(.caption)
                    .foregroundStyle(.secondary)
                growingEditor(font: .system(.body, design: .monospaced))
            }
            .padding(10)
            .background(Color(nsColor: .controlBackgroundColor))
            .clipShape(RoundedRectangle(cornerRadius: 7))

        case .table:
            tableEditor

        case .image:
            VStack(alignment: .leading, spacing: 8) {
                MarkdownPreview(
                    source: "![\(block.text)](\(block.location))",
                    assetsURL: assetsURL
                )
                .frame(minHeight: 140, maxHeight: 320)
                TextField(tr("Descrição da imagem"), text: $block.text)
                TextField(tr("Localização da imagem"), text: $block.location)
                    .font(.caption.monospaced())
            }

        case .divider:
            Divider()
                .padding(.vertical, 8)
        }
    }

    private func growingEditor(font: Font) -> some View {
        TextEditor(text: $block.text)
            .font(font)
            .scrollContentBackground(.hidden)
            .frame(minHeight: 44)
            .fixedSize(horizontal: false, vertical: true)
    }

    private var tableEditor: some View {
        VStack(alignment: .leading, spacing: 8) {
            Grid(horizontalSpacing: 4, verticalSpacing: 4) {
                ForEach(block.table.indices, id: \.self) { row in
                    GridRow {
                        ForEach(block.table[row].indices, id: \.self) { column in
                            TextField(
                                tr("Conteúdo"),
                                text: tableCell(row: row, column: column)
                            )
                            .textFieldStyle(.roundedBorder)
                            .fontWeight(row == 0 ? .semibold : .regular)
                        }
                    }
                }
            }
            HStack {
                Button(tr("Adicionar linha")) {
                    let columns = block.table.first?.count ?? 2
                    block.table.append(Array(repeating: "", count: columns))
                }
                Button(tr("Adicionar coluna")) {
                    for index in block.table.indices {
                        block.table[index].append("")
                    }
                }
            }
            .buttonStyle(.borderless)
            .font(.caption)
        }
    }

    private func tableCell(row: Int, column: Int) -> Binding<String> {
        Binding(
            get: { block.table[row][column] },
            set: { block.table[row][column] = $0 }
        )
    }

    private var headingFont: Font {
        switch block.level {
        case 1: .largeTitle
        case 2: .title
        case 3: .title2
        default: .headline
        }
    }
}

private struct VisualBlock: Identifiable, Equatable {
    enum Kind: Equatable {
        case paragraph, heading, bullet, numbered, task, quote, code, table, image, divider

        var icon: String {
            switch self {
            case .paragraph: "text.alignleft"
            case .heading: "textformat"
            case .bullet: "list.bullet"
            case .numbered: "list.number"
            case .task: "checklist"
            case .quote: "text.quote"
            case .code: "chevron.left.forwardslash.chevron.right"
            case .table: "tablecells"
            case .image: "photo"
            case .divider: "minus"
            }
        }
    }

    let id = UUID()
    var kind: Kind
    var text = ""
    var level = 1
    var number = 1
    var isChecked = false
    var language = ""
    var location = ""
    var table: [[String]] = []

    static func paragraph(_ text: String = "") -> Self { .init(kind: .paragraph, text: text) }
    static func heading(_ text: String = "", level: Int = 1) -> Self {
        .init(kind: .heading, text: text, level: level)
    }
    static func bullet(_ text: String = "") -> Self { .init(kind: .bullet, text: text) }
    static func task(_ text: String = "", checked: Bool = false) -> Self {
        .init(kind: .task, text: text, isChecked: checked)
    }
    static func quote(_ text: String = "") -> Self { .init(kind: .quote, text: text) }
    static func code(_ text: String = "", language: String = "") -> Self {
        .init(kind: .code, text: text, language: language)
    }
    static func table() -> Self {
        .init(kind: .table, table: [["", ""], ["", ""]])
    }
    static func divider() -> Self { .init(kind: .divider) }

    func copy() -> Self {
        .init(
            kind: kind,
            text: text,
            level: level,
            number: number,
            isChecked: isChecked,
            language: language,
            location: location,
            table: table
        )
    }
}

private enum VisualMarkdownParser {
    static func parse(_ source: String) -> [VisualBlock] {
        let lines = source.components(separatedBy: .newlines)
        var result: [VisualBlock] = []
        var paragraph: [String] = []
        var index = 0

        func finishParagraph() {
            guard !paragraph.isEmpty else { return }
            result.append(.paragraph(paragraph.joined(separator: "\n")))
            paragraph.removeAll()
        }

        while index < lines.count {
            let line = lines[index]
            if line.hasPrefix("```") {
                finishParagraph()
                let language = String(line.dropFirst(3))
                var code: [String] = []
                index += 1
                while index < lines.count, !lines[index].hasPrefix("```") {
                    code.append(lines[index])
                    index += 1
                }
                result.append(.code(code.joined(separator: "\n"), language: language))
            } else if index + 1 < lines.count,
                      let headers = cells(lines[index]),
                      isSeparator(lines[index + 1], columns: headers.count) {
                finishParagraph()
                var table = [headers]
                index += 2
                while index < lines.count, let row = cells(lines[index]) {
                    table.append(row)
                    index += 1
                }
                result.append(.init(kind: .table, table: table))
                continue
            } else if let heading = heading(line) {
                finishParagraph()
                result.append(.heading(heading.text, level: heading.level))
            } else if line.hasPrefix("- [ ] ") {
                finishParagraph()
                result.append(.task(String(line.dropFirst(6))))
            } else if line.hasPrefix("- [x] ") || line.hasPrefix("- [X] ") {
                finishParagraph()
                result.append(.task(String(line.dropFirst(6)), checked: true))
            } else if line.hasPrefix("- ") || line.hasPrefix("* ") {
                finishParagraph()
                result.append(.bullet(String(line.dropFirst(2))))
            } else if let numbered = numbered(line) {
                finishParagraph()
                result.append(.init(
                    kind: .numbered,
                    text: numbered.text,
                    number: numbered.number
                ))
            } else if line.hasPrefix("> ") {
                finishParagraph()
                result.append(.quote(String(line.dropFirst(2))))
            } else if let image = image(line) {
                finishParagraph()
                result.append(.init(
                    kind: .image,
                    text: image.alt,
                    location: image.location
                ))
            } else if line == "---" || line == "***" {
                finishParagraph()
                result.append(.divider())
            } else if line.isEmpty {
                finishParagraph()
            } else {
                paragraph.append(line)
            }
            index += 1
        }
        finishParagraph()
        return result
    }

    static func serialize(_ blocks: [VisualBlock]) -> String {
        blocks.map { block in
            switch block.kind {
            case .paragraph:
                return block.text
            case .heading:
                return "\(String(repeating: "#", count: block.level)) \(block.text)"
            case .bullet:
                return "- \(block.text)"
            case .numbered:
                return "\(block.number). \(block.text)"
            case .task:
                return "- [\(block.isChecked ? "x" : " ")] \(block.text)"
            case .quote:
                return block.text.components(separatedBy: .newlines)
                    .map { "> \($0)" }.joined(separator: "\n")
            case .code:
                return "```\(block.language)\n\(block.text)\n```"
            case .table:
                guard let headers = block.table.first else { return "" }
                let header = "| " + headers.joined(separator: " | ") + " |"
                let separator = "| " + headers.map { _ in "---" }.joined(separator: " | ") + " |"
                let rows = block.table.dropFirst().map {
                    "| " + $0.joined(separator: " | ") + " |"
                }
                return ([header, separator] + rows).joined(separator: "\n")
            case .image:
                return "![\(block.text)](\(block.location))"
            case .divider:
                return "---"
            }
        }
        .joined(separator: "\n\n")
    }

    private static func heading(_ line: String) -> (level: Int, text: String)? {
        let marker = line.prefix { $0 == "#" }
        guard (1...6).contains(marker.count) else { return nil }
        let rest = line.dropFirst(marker.count)
        guard rest.first == " " else { return nil }
        return (marker.count, String(rest.dropFirst()))
    }

    private static func numbered(_ line: String) -> (number: Int, text: String)? {
        guard let dot = line.firstIndex(of: "."), let number = Int(line[..<dot]) else {
            return nil
        }
        let rest = line[line.index(after: dot)...]
        guard rest.first == " " else { return nil }
        return (number, String(rest.dropFirst()))
    }

    private static func cells(_ line: String) -> [String]? {
        guard line.contains("|") else { return nil }
        var value = line.trimmingCharacters(in: .whitespaces)
        if value.hasPrefix("|") { value.removeFirst() }
        if value.hasSuffix("|") { value.removeLast() }
        let result = value.split(separator: "|", omittingEmptySubsequences: false)
            .map { $0.trimmingCharacters(in: .whitespaces) }
        return result.count > 1 ? result : nil
    }

    private static func isSeparator(_ line: String, columns: Int) -> Bool {
        guard let values = cells(line), values.count == columns else { return false }
        return values.allSatisfy {
            $0.trimmingCharacters(in: CharacterSet(charactersIn: " :-")).isEmpty
                && $0.filter { $0 == "-" }.count >= 3
        }
    }

    private static func image(_ line: String) -> (alt: String, location: String)? {
        guard line.hasPrefix("!["),
              let end = line.firstIndex(of: "]"),
              line.index(after: end) < line.endIndex,
              line[line.index(after: end)] == "(",
              line.last == ")"
        else { return nil }
        let altStart = line.index(line.startIndex, offsetBy: 2)
        let locationStart = line.index(end, offsetBy: 2)
        return (
            String(line[altStart..<end]),
            String(line[locationStart..<line.index(before: line.endIndex)])
        )
    }
}

import SwiftUI

struct MarkdownPreview: View {
    let source: String
    var assetsURL: URL?
    var scrollFraction: Double? = nil

    var body: some View {
        ScrollViewReader { proxy in
            ScrollView {
                LazyVStack(alignment: .leading, spacing: 14) {
                    ForEach(Array(blocks.enumerated()), id: \.offset) { index, block in
                        blockView(block)
                            .id(index)
                    }
                }
                .textSelection(.enabled)
                .frame(maxWidth: 760, alignment: .leading)
                .padding(28)
                .frame(maxWidth: .infinity, alignment: .center)
            }
            .onChange(of: scrollFraction) { _, fraction in
                guard let fraction, !blocks.isEmpty else { return }
                let index = min(
                    blocks.count - 1,
                    max(0, Int((Double(blocks.count - 1) * fraction).rounded()))
                )
                withAnimation(.easeOut(duration: 0.15)) {
                    proxy.scrollTo(index, anchor: .top)
                }
            }
        }
    }

    @ViewBuilder
    private func blockView(_ block: MarkdownBlock) -> some View {
        switch block {
        case let .heading(level, text):
            Text(inlineMarkdown(text))
                .font(headingFont(level))
                .fontWeight(.bold)
                .padding(.top, level == 1 ? 8 : 2)

        case let .paragraph(text):
            Text(inlineMarkdown(text))
                .font(.body)
                .lineSpacing(4)

        case let .bullet(text):
            HStack(alignment: .firstTextBaseline, spacing: 9) {
                Text("•")
                Text(inlineMarkdown(text))
            }
            .padding(.leading, 8)

        case let .numbered(number, text):
            HStack(alignment: .firstTextBaseline, spacing: 9) {
                Text("\(number).")
                    .foregroundStyle(.secondary)
                Text(inlineMarkdown(text))
            }
            .padding(.leading, 8)

        case let .task(isComplete, text):
            HStack(alignment: .firstTextBaseline, spacing: 9) {
                Image(systemName: isComplete ? "checkmark.square.fill" : "square")
                    .foregroundStyle(isComplete ? Color.accentColor : .secondary)
                Text(inlineMarkdown(text))
                    .strikethrough(isComplete)
            }
            .padding(.leading, 8)

        case let .quote(text):
            HStack(spacing: 12) {
                RoundedRectangle(cornerRadius: 2)
                    .fill(Color.secondary.opacity(0.5))
                    .frame(width: 4)
                Text(inlineMarkdown(text))
                    .foregroundStyle(.secondary)
            }

        case let .table(headers, rows):
            Grid(horizontalSpacing: 0, verticalSpacing: 0) {
                GridRow {
                    ForEach(headers.indices, id: \.self) { index in
                        tableCell(headers[index], isHeader: true)
                    }
                }
                ForEach(rows.indices, id: \.self) { rowIndex in
                    GridRow {
                        ForEach(headers.indices, id: \.self) { columnIndex in
                            tableCell(
                                rows[rowIndex].indices.contains(columnIndex)
                                    ? rows[rowIndex][columnIndex]
                                    : "",
                                isHeader: false
                            )
                        }
                    }
                }
            }
            .overlay {
                RoundedRectangle(cornerRadius: 6)
                    .stroke(Color.secondary.opacity(0.3))
            }

        case let .code(language, code):
            VStack(alignment: .leading, spacing: 0) {
                if let language, !language.isEmpty {
                    Text(language)
                        .font(.caption2.weight(.semibold))
                        .foregroundStyle(.secondary)
                        .padding(.horizontal, 12)
                        .padding(.top, 8)
                }
                ScrollView(.horizontal) {
                    Text(code)
                        .font(.system(.body, design: .monospaced))
                        .padding(12)
                        .textSelection(.enabled)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(Color(nsColor: .controlBackgroundColor))
            .clipShape(RoundedRectangle(cornerRadius: 8))

        case let .image(alt, location):
            MarkdownImage(alt: alt, location: location, assetsURL: assetsURL)

        case .divider:
            Divider()
                .padding(.vertical, 4)
        }
    }

    private var blocks: [MarkdownBlock] {
        MarkdownParser.parse(source)
    }

    private func inlineMarkdown(_ source: String) -> AttributedString {
        let pattern = #"<span\s+style="color:(#[0-9a-fA-F]{6})">(.*?)</span>"#
        guard let expression = try? NSRegularExpression(
            pattern: pattern,
            options: [.dotMatchesLineSeparators]
        ) else { return parsedInline(source) }

        let sourceString = source as NSString
        let matches = expression.matches(
            in: source,
            range: NSRange(location: 0, length: sourceString.length)
        )
        guard !matches.isEmpty else { return parsedInline(source) }

        var result = AttributedString()
        var cursor = 0
        for match in matches {
            if match.range.location > cursor {
                result.append(parsedInline(sourceString.substring(
                    with: NSRange(
                        location: cursor,
                        length: match.range.location - cursor
                    )
                )))
            }

            var colored = parsedInline(sourceString.substring(with: match.range(at: 2)))
            let hex = sourceString.substring(with: match.range(at: 1))
            if let color = Color(hex: hex) {
                colored.foregroundColor = color
            }
            result.append(colored)
            cursor = NSMaxRange(match.range)
        }

        if cursor < sourceString.length {
            result.append(parsedInline(sourceString.substring(
                with: NSRange(location: cursor, length: sourceString.length - cursor)
            )))
        }
        return result
    }

    private func parsedInline(_ source: String) -> AttributedString {
        (try? AttributedString(
            markdown: source,
            options: .init(interpretedSyntax: .inlineOnlyPreservingWhitespace)
        )) ?? AttributedString(source)
    }

    private func headingFont(_ level: Int) -> Font {
        switch level {
        case 1: .largeTitle
        case 2: .title
        case 3: .title2
        default: .headline
        }
    }

    private func tableCell(_ value: String, isHeader: Bool) -> some View {
        Text(inlineMarkdown(value))
            .fontWeight(isHeader ? .semibold : .regular)
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(.horizontal, 10)
            .padding(.vertical, 8)
            .background(
                isHeader
                    ? Color.secondary.opacity(0.12)
                    : Color.clear
            )
            .overlay {
                Rectangle()
                    .stroke(Color.secondary.opacity(0.18), lineWidth: 0.5)
            }
    }
}

private struct MarkdownImage: View {
    let alt: String
    let location: String
    let assetsURL: URL?

    var body: some View {
        if let localImage {
            Image(nsImage: localImage)
                .resizable()
                .scaledToFit()
                .frame(maxWidth: .infinity)
                .clipShape(RoundedRectangle(cornerRadius: 8))
        } else if let url = URL(string: location), !url.isFileURL {
            AsyncImage(url: url) { phase in
                switch phase {
                case let .success(image):
                    image
                        .resizable()
                        .scaledToFit()
                case .failure:
                    unavailable
                case .empty:
                    ProgressView()
                        .frame(maxWidth: .infinity, minHeight: 120)
                @unknown default:
                    unavailable
                }
            }
            .frame(maxWidth: .infinity)
            .clipShape(RoundedRectangle(cornerRadius: 8))
        } else {
            unavailable
        }
    }

    private var localImage: NSImage? {
        guard location.hasPrefix("assets/"), let assetsURL else { return nil }
        return NSImage(
            contentsOf: assetsURL.appendingPathComponent(String(location.dropFirst(7)))
        )
    }

    private var unavailable: some View {
        ContentUnavailableView(
            alt.isEmpty ? "Imagem indisponível" : alt,
            systemImage: "photo"
        )
        .frame(maxWidth: .infinity, minHeight: 120)
        .background(Color(nsColor: .controlBackgroundColor))
        .clipShape(RoundedRectangle(cornerRadius: 8))
    }
}

private enum MarkdownBlock {
    case heading(Int, String)
    case paragraph(String)
    case bullet(String)
    case numbered(Int, String)
    case task(Bool, String)
    case quote(String)
    case table([String], [[String]])
    case code(String?, String)
    case image(String, String)
    case divider
}

private enum MarkdownParser {
    static func parse(_ source: String) -> [MarkdownBlock] {
        let lines = source.components(separatedBy: .newlines)
        var result: [MarkdownBlock] = []
        var paragraph: [String] = []
        var code: [String] = []
        var codeLanguage: String?
        var insideCode = false

        func finishParagraph() {
            guard !paragraph.isEmpty else { return }
            result.append(.paragraph(paragraph.joined(separator: " ")))
            paragraph.removeAll()
        }

        var index = 0
        while index < lines.count {
            let line = lines[index]

            if line.hasPrefix("```") {
                if insideCode {
                    result.append(.code(codeLanguage, code.joined(separator: "\n")))
                    code.removeAll()
                    codeLanguage = nil
                } else {
                    finishParagraph()
                    codeLanguage = String(line.dropFirst(3))
                }
                insideCode.toggle()
                index += 1
                continue
            }

            if insideCode {
                code.append(line)
                index += 1
                continue
            }

            if index + 1 < lines.count,
               let headers = tableCells(from: line),
               isTableSeparator(lines[index + 1], columns: headers.count) {
                finishParagraph()
                var rows: [[String]] = []
                index += 2
                while index < lines.count,
                      let row = tableCells(from: lines[index]) {
                    rows.append(row)
                    index += 1
                }
                result.append(.table(headers, rows))
                continue
            } else if line.isEmpty {
                finishParagraph()
            } else if line == "---" || line == "***" {
                finishParagraph()
                result.append(.divider)
            } else if let heading = heading(from: line) {
                finishParagraph()
                result.append(.heading(heading.level, heading.text))
            } else if line.hasPrefix("- [ ] ") || line.hasPrefix("* [ ] ") {
                finishParagraph()
                result.append(.task(false, String(line.dropFirst(6))))
            } else if line.hasPrefix("- [x] ") || line.hasPrefix("- [X] ")
                        || line.hasPrefix("* [x] ") || line.hasPrefix("* [X] ") {
                finishParagraph()
                result.append(.task(true, String(line.dropFirst(6))))
            } else if line.hasPrefix("- ") || line.hasPrefix("* ") {
                finishParagraph()
                result.append(.bullet(String(line.dropFirst(2))))
            } else if let numbered = numberedItem(from: line) {
                finishParagraph()
                result.append(.numbered(numbered.number, numbered.text))
            } else if line.hasPrefix("> ") {
                finishParagraph()
                result.append(.quote(String(line.dropFirst(2))))
            } else if let image = image(from: line) {
                finishParagraph()
                result.append(.image(image.alt, image.location))
            } else {
                paragraph.append(line)
            }
            index += 1
        }

        finishParagraph()
        if insideCode {
            result.append(.code(codeLanguage, code.joined(separator: "\n")))
        }
        return result
    }

    private static func heading(from line: String) -> (level: Int, text: String)? {
        let marker = line.prefix { $0 == "#" }
        guard (1...6).contains(marker.count) else { return nil }
        let remainder = line.dropFirst(marker.count)
        guard remainder.first == " " else { return nil }
        return (marker.count, String(remainder.dropFirst()))
    }

    private static func numberedItem(from line: String) -> (number: Int, text: String)? {
        guard let dot = line.firstIndex(of: "."),
              let number = Int(line[..<dot])
        else { return nil }
        let remainder = line[line.index(after: dot)...]
        guard remainder.first == " " else { return nil }
        return (number, String(remainder.dropFirst()))
    }

    private static func tableCells(from line: String) -> [String]? {
        guard line.contains("|") else { return nil }
        var value = line.trimmingCharacters(in: .whitespaces)
        if value.hasPrefix("|") { value.removeFirst() }
        if value.hasSuffix("|") { value.removeLast() }
        let cells = value.split(separator: "|", omittingEmptySubsequences: false)
            .map { $0.trimmingCharacters(in: .whitespaces) }
        return cells.count >= 2 ? cells : nil
    }

    private static func isTableSeparator(_ line: String, columns: Int) -> Bool {
        guard let cells = tableCells(from: line), cells.count == columns else {
            return false
        }
        return cells.allSatisfy { cell in
            let marker = cell.trimmingCharacters(
                in: CharacterSet(charactersIn: " :-")
            )
            return marker.isEmpty && cell.filter { $0 == "-" }.count >= 3
        }
    }

    private static func image(from line: String) -> (alt: String, location: String)? {
        guard line.hasPrefix("!["),
              let closingAlt = line.firstIndex(of: "]"),
              line.index(after: closingAlt) < line.endIndex,
              line[line.index(after: closingAlt)] == "(",
              line.last == ")"
        else { return nil }

        let altStart = line.index(line.startIndex, offsetBy: 2)
        let locationStart = line.index(closingAlt, offsetBy: 2)
        return (
            String(line[altStart..<closingAlt]),
            String(line[locationStart..<line.index(before: line.endIndex)])
        )
    }
}

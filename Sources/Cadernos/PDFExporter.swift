import AppKit
import Foundation
import UniformTypeIdentifiers
import WebKit

extension LibraryStore {
    func exportActiveNoteToPDF() {
        guard let note = selectedNote else { return }

        let panel = NSSavePanel()
        panel.title = tr("Exportar nota para PDF")
        panel.prompt = tr("Exportar")
        panel.allowedContentTypes = [.pdf]
        panel.nameFieldStringValue = "\(safePDFName(note.title)).pdf"

        guard panel.runModal() == .OK, let destination = panel.url else { return }
        let baseURL = activeNoteAssetsURL?.deletingLastPathComponent()
        let html = PDFMarkdownRenderer.html(title: note.title, markdown: note.markdown)

        Task { @MainActor in
            do {
                let exporter = WebPDFExporter()
                let data = try await exporter.render(html: html, baseURL: baseURL)
                try data.write(to: destination, options: .atomic)
            } catch {
                let alert = NSAlert()
                alert.alertStyle = .warning
                alert.messageText = tr("Não foi possível exportar o PDF")
                alert.informativeText = error.localizedDescription
                alert.runModal()
            }
        }
    }

    private func safePDFName(_ value: String) -> String {
        let forbidden = CharacterSet(charactersIn: "/:")
        let result = value.components(separatedBy: forbidden)
            .joined(separator: "-")
            .trimmingCharacters(in: .whitespacesAndNewlines)
        return result.isEmpty ? "Nota" : result
    }
}

@MainActor
private final class WebPDFExporter: NSObject, WKNavigationDelegate {
    private var continuation: CheckedContinuation<Void, Error>?

    func render(html: String, baseURL: URL?) async throws -> Data {
        let configuration = WKWebViewConfiguration()
        let webView = WKWebView(
            frame: NSRect(x: 0, y: 0, width: 794, height: 1123),
            configuration: configuration
        )
        webView.navigationDelegate = self

        try await withCheckedThrowingContinuation {
            (continuation: CheckedContinuation<Void, Error>) in
            self.continuation = continuation
            webView.loadHTMLString(html, baseURL: baseURL)
        }

        return try await webView.pdf(configuration: .init())
    }

    func webView(_ webView: WKWebView, didFinish navigation: WKNavigation?) {
        continuation?.resume()
        continuation = nil
    }

    func webView(
        _ webView: WKWebView,
        didFail navigation: WKNavigation?,
        withError error: Error
    ) {
        continuation?.resume(throwing: error)
        continuation = nil
    }

    func webView(
        _ webView: WKWebView,
        didFailProvisionalNavigation navigation: WKNavigation?,
        withError error: Error
    ) {
        continuation?.resume(throwing: error)
        continuation = nil
    }
}

private enum PDFMarkdownRenderer {
    static func html(title: String, markdown: String) -> String {
        """
        <!doctype html>
        <html>
        <head>
        <meta charset="utf-8">
        <style>
        @page { margin: 54pt; }
        body {
          color: #1f2328;
          font: 15px -apple-system, BlinkMacSystemFont, sans-serif;
          line-height: 1.55;
          margin: 0 auto;
          max-width: 760px;
        }
        h1 { font-size: 30px; margin: 0 0 22px; }
        h2 { font-size: 24px; margin-top: 28px; }
        h3 { font-size: 19px; margin-top: 24px; }
        p { margin: 10px 0; }
        blockquote { border-left: 4px solid #d0d7de; color: #57606a; margin-left: 0; padding-left: 16px; }
        code { background: #f3f4f6; border-radius: 4px; font-family: ui-monospace, monospace; padding: 2px 5px; }
        pre { background: #f3f4f6; border-radius: 8px; overflow-wrap: anywhere; padding: 14px; white-space: pre-wrap; }
        pre code { padding: 0; }
        img { display: block; height: auto; margin: 18px auto; max-width: 100%; }
        table { border-collapse: collapse; margin: 18px 0; width: 100%; }
        th, td { border: 1px solid #d0d7de; padding: 8px 10px; text-align: left; }
        th { background: #f3f4f6; }
        hr { border: 0; border-top: 1px solid #d0d7de; margin: 24px 0; }
        </style>
        </head>
        <body>
        <h1>\(escape(title))</h1>
        \(render(markdown))
        </body>
        </html>
        """
    }

    private static func render(_ markdown: String) -> String {
        let lines = markdown.components(separatedBy: .newlines)
        var output: [String] = []
        var codeLines: [String] = []
        var paragraph: [String] = []
        var inCode = false

        func finishParagraph() {
            guard !paragraph.isEmpty else { return }
            output.append("<p>\(inline(paragraph.joined(separator: " ")))</p>")
            paragraph.removeAll()
        }

        var index = 0
        while index < lines.count {
            let line = lines[index]
            if line.hasPrefix("```") {
                finishParagraph()
                if inCode {
                    output.append("<pre><code>\(escape(codeLines.joined(separator: "\n")))</code></pre>")
                    codeLines.removeAll()
                }
                inCode.toggle()
                index += 1
                continue
            }
            if inCode {
                codeLines.append(line)
                index += 1
                continue
            }
            if index + 1 < lines.count,
               let headers = tableCells(line),
               isTableSeparator(lines[index + 1], columns: headers.count) {
                finishParagraph()
                index += 2
                var rows: [[String]] = []
                while index < lines.count, let row = tableCells(lines[index]) {
                    rows.append(row)
                    index += 1
                }
                let headerHTML = headers.map { "<th>\(inline($0))</th>" }.joined()
                let bodyHTML = rows.map { row in
                    "<tr>" + headers.indices.map {
                        "<td>\(inline(row.indices.contains($0) ? row[$0] : ""))</td>"
                    }.joined() + "</tr>"
                }.joined()
                output.append("<table><thead><tr>\(headerHTML)</tr></thead><tbody>\(bodyHTML)</tbody></table>")
                continue
            } else if line.isEmpty {
                finishParagraph()
            } else if line == "---" || line == "***" {
                finishParagraph()
                output.append("<hr>")
            } else if let heading = heading(line) {
                finishParagraph()
                output.append("<h\(heading.level)>\(inline(heading.text))</h\(heading.level)>")
            } else if line.hasPrefix("- [ ] ") || line.hasPrefix("* [ ] ") {
                finishParagraph()
                output.append("<p>☐ \(inline(String(line.dropFirst(6))))</p>")
            } else if line.hasPrefix("- [x] ") || line.hasPrefix("- [X] ")
                        || line.hasPrefix("* [x] ") || line.hasPrefix("* [X] ") {
                finishParagraph()
                output.append("<p>☑ <s>\(inline(String(line.dropFirst(6))))</s></p>")
            } else if line.hasPrefix("- ") || line.hasPrefix("* ") {
                finishParagraph()
                output.append("<ul><li>\(inline(String(line.dropFirst(2))))</li></ul>")
            } else if let numbered = numberedItem(line) {
                finishParagraph()
                output.append("<ol start=\"\(numbered.number)\"><li>\(inline(numbered.text))</li></ol>")
            } else if line.hasPrefix("> ") {
                finishParagraph()
                output.append("<blockquote>\(inline(String(line.dropFirst(2))))</blockquote>")
            } else if let image = image(line) {
                finishParagraph()
                output.append("<img src=\"\(attribute(image.location))\" alt=\"\(attribute(image.alt))\">")
            } else {
                paragraph.append(line)
            }
            index += 1
        }

        finishParagraph()
        if inCode {
            output.append("<pre><code>\(escape(codeLines.joined(separator: "\n")))</code></pre>")
        }
        return output.joined(separator: "\n")
    }

    private static func inline(_ source: String) -> String {
        let pattern = #"<span\s+style="color:(#[0-9a-fA-F]{6})">(.*?)</span>"#
        guard let expression = try? NSRegularExpression(pattern: pattern) else {
            return basicInline(source)
        }
        let value = source as NSString
        let matches = expression.matches(
            in: source,
            range: NSRange(location: 0, length: value.length)
        )
        guard !matches.isEmpty else { return basicInline(source) }

        var output = ""
        var cursor = 0
        for match in matches {
            if match.range.location > cursor {
                output += basicInline(value.substring(with: NSRange(
                    location: cursor,
                    length: match.range.location - cursor
                )))
            }
            let color = attribute(value.substring(with: match.range(at: 1)))
            let content = value.substring(with: match.range(at: 2))
            output += "<span style=\"color:\(color)\">\(basicInline(content))</span>"
            cursor = NSMaxRange(match.range)
        }
        if cursor < value.length {
            output += basicInline(value.substring(
                with: NSRange(location: cursor, length: value.length - cursor)
            ))
        }
        return output
    }

    private static func basicInline(_ source: String) -> String {
        var value = escape(source)
        value = replacePairs(in: value, marker: "**", open: "<strong>", close: "</strong>")
        value = replacePairs(in: value, marker: "_", open: "<em>", close: "</em>")
        value = replacePairs(in: value, marker: "`", open: "<code>", close: "</code>")
        return value
    }

    private static func replacePairs(
        in source: String,
        marker: String,
        open: String,
        close: String
    ) -> String {
        var value = source
        var opens = true
        while let range = value.range(of: marker) {
            value.replaceSubrange(range, with: opens ? open : close)
            opens.toggle()
        }
        return value
    }

    private static func heading(_ line: String) -> (level: Int, text: String)? {
        let marker = line.prefix { $0 == "#" }
        guard (1...6).contains(marker.count) else { return nil }
        let remainder = line.dropFirst(marker.count)
        guard remainder.first == " " else { return nil }
        return (marker.count, String(remainder.dropFirst()))
    }

    private static func numberedItem(_ line: String) -> (number: Int, text: String)? {
        guard let dot = line.firstIndex(of: "."),
              let number = Int(line[..<dot])
        else { return nil }
        let remainder = line[line.index(after: dot)...]
        guard remainder.first == " " else { return nil }
        return (number, String(remainder.dropFirst()))
    }

    private static func tableCells(_ line: String) -> [String]? {
        guard line.contains("|") else { return nil }
        var value = line.trimmingCharacters(in: .whitespaces)
        if value.hasPrefix("|") { value.removeFirst() }
        if value.hasSuffix("|") { value.removeLast() }
        let cells = value.split(separator: "|", omittingEmptySubsequences: false)
            .map { $0.trimmingCharacters(in: .whitespaces) }
        return cells.count >= 2 ? cells : nil
    }

    private static func isTableSeparator(_ line: String, columns: Int) -> Bool {
        guard let cells = tableCells(line), cells.count == columns else { return false }
        return cells.allSatisfy {
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

    private static func escape(_ value: String) -> String {
        value
            .replacingOccurrences(of: "&", with: "&amp;")
            .replacingOccurrences(of: "<", with: "&lt;")
            .replacingOccurrences(of: ">", with: "&gt;")
            .replacingOccurrences(of: "\"", with: "&quot;")
    }

    private static func attribute(_ value: String) -> String {
        escape(value)
    }
}

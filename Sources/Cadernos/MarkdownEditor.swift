import AppKit
import SwiftUI
import UniformTypeIdentifiers

struct MarkdownEditor: NSViewRepresentable {
    @Environment(\.colorScheme) private var colorScheme
    @Binding var text: String
    @Binding var selection: NSRange
    let pasteImage: (NSImage) -> String?

    func makeCoordinator() -> Coordinator { Coordinator(parent: self) }

    func makeNSView(context: Context) -> NSScrollView {
        let scrollView = NSScrollView()
        let textView = FocusableTextView(frame: scrollView.contentView.bounds)
        textView.delegate = context.coordinator
        textView.pasteImage = pasteImage
        textView.isEditable = true
        textView.isSelectable = true
        textView.isFieldEditor = false
        textView.isRichText = false
        textView.isAutomaticQuoteSubstitutionEnabled = false
        textView.isAutomaticDashSubstitutionEnabled = false
        textView.isAutomaticTextReplacementEnabled = false
        textView.allowsUndo = true
        textView.font = .monospacedSystemFont(ofSize: 14, weight: .regular)
        textView.drawsBackground = true
        textView.textContainerInset = NSSize(width: 18, height: 18)
        textView.minSize = .zero
        textView.maxSize = NSSize(
            width: CGFloat.greatestFiniteMagnitude,
            height: CGFloat.greatestFiniteMagnitude
        )
        textView.isVerticallyResizable = true
        textView.isHorizontallyResizable = false
        textView.autoresizingMask = [.width]
        textView.textContainer?.widthTracksTextView = true
        textView.string = text
        applyTheme(to: textView)
        scrollView.documentView = textView
        scrollView.hasVerticalScroller = true
        scrollView.autohidesScrollers = true
        scrollView.drawsBackground = true
        context.coordinator.textView = textView
        context.coordinator.highlightMarkdown(in: textView)
        return scrollView
    }

    func updateNSView(_ scrollView: NSScrollView, context: Context) {
        context.coordinator.parent = self
        guard let textView = scrollView.documentView as? NSTextView else { return }
        let viewport = scrollView.contentSize
        let contentHeight = max(
            viewport.height,
            (textView.layoutManager?.usedRect(
                for: textView.textContainer!
            ).height ?? 0) + (textView.textContainerInset.height * 2)
        )
        textView.setFrameSize(
            NSSize(
                width: max(1, viewport.width),
                height: max(1, contentHeight)
            )
        )
        (textView as? FocusableTextView)?.pasteImage = pasteImage
        applyTheme(to: textView)
        if textView.string != text {
            textView.string = text
            applyTheme(to: textView)
        }
        context.coordinator.highlightMarkdown(in: textView)
        if textView.selectedRange() != selection,
           NSMaxRange(selection) <= (text as NSString).length {
            textView.setSelectedRange(selection)
        }
    }

    private func applyTheme(to textView: NSTextView) {
        let foreground = colorScheme == .dark ? NSColor.white : NSColor.black
        let background = colorScheme == .dark
            ? NSColor(calibratedWhite: 0.08, alpha: 1)
            : NSColor.white

        textView.textColor = foreground
        textView.backgroundColor = background
        textView.enclosingScrollView?.backgroundColor = background
        textView.insertionPointColor = foreground
        textView.typingAttributes = [
            .foregroundColor: foreground,
            .font: NSFont.monospacedSystemFont(ofSize: 14, weight: .regular)
        ]
        textView.needsDisplay = true
    }

    final class Coordinator: NSObject, NSTextViewDelegate {
        var parent: MarkdownEditor
        weak var textView: NSTextView?

        init(parent: MarkdownEditor) { self.parent = parent }

        func textDidChange(_ notification: Notification) {
            guard let textView else { return }
            parent.text = textView.string
            parent.selection = textView.selectedRange()
            highlightMarkdown(in: textView)
        }

        func textViewDidChangeSelection(_ notification: Notification) {
            guard let textView else { return }
            parent.selection = textView.selectedRange()
            textView.needsDisplay = true
        }

        func highlightMarkdown(in textView: NSTextView) {
            guard let layoutManager = textView.layoutManager else { return }
            let fullRange = NSRange(
                location: 0,
                length: (textView.string as NSString).length
            )
            for key in [
                NSAttributedString.Key.foregroundColor,
                .backgroundColor,
                .font
            ] {
                layoutManager.removeTemporaryAttribute(key, forCharacterRange: fullRange)
            }

            apply(
                #"(?m)^#{1,6}\s.+$"#,
                attributes: [
                    .foregroundColor: NSColor.systemBlue,
                    .font: NSFont.monospacedSystemFont(ofSize: 14, weight: .bold)
                ],
                to: textView
            )
            apply(
                #"(?s)```.*?```"#,
                attributes: [.backgroundColor: NSColor.controlBackgroundColor],
                to: textView
            )
            apply(
                #"`[^`\n]+`"#,
                attributes: [.foregroundColor: NSColor.systemPurple],
                to: textView
            )
            apply(
                #"\*\*[^*\n]+\*\*"#,
                attributes: [.foregroundColor: NSColor.systemOrange],
                to: textView
            )
            apply(
                #"!?\[[^\]\n]+\]\([^)]+\)"#,
                attributes: [.foregroundColor: NSColor.systemTeal],
                to: textView
            )
            apply(
                #"(?m)^>\s.+$"#,
                attributes: [.foregroundColor: NSColor.secondaryLabelColor],
                to: textView
            )
        }

        private func apply(
            _ pattern: String,
            attributes: [NSAttributedString.Key: Any],
            to textView: NSTextView
        ) {
            guard let expression = try? NSRegularExpression(pattern: pattern),
                  let layoutManager = textView.layoutManager
            else { return }
            let range = NSRange(
                location: 0,
                length: (textView.string as NSString).length
            )
            for match in expression.matches(in: textView.string, range: range) {
                layoutManager.addTemporaryAttributes(
                    attributes,
                    forCharacterRange: match.range
                )
            }
        }
    }
}

private final class LineNumberRulerView: NSRulerView {
    private weak var textView: NSTextView?
    var usesDarkTheme: Bool

    init(
        scrollView: NSScrollView,
        textView: NSTextView,
        usesDarkTheme: Bool
    ) {
        self.textView = textView
        self.usesDarkTheme = usesDarkTheme
        super.init(scrollView: scrollView, orientation: .verticalRuler)
        clientView = textView
        ruleThickness = 44

        scrollView.contentView.postsBoundsChangedNotifications = true
        let center = NotificationCenter.default
        for name in [
            NSView.boundsDidChangeNotification,
            NSText.didChangeNotification,
            NSTextView.didChangeSelectionNotification
        ] {
            center.addObserver(
                self,
                selector: #selector(redraw),
                name: name,
                object: name == NSView.boundsDidChangeNotification
                    ? scrollView.contentView
                    : textView
            )
        }
    }

    required init(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    deinit {
        NotificationCenter.default.removeObserver(self)
    }

    @objc private func redraw() {
        needsDisplay = true
    }

    override func drawHashMarksAndLabels(in rect: NSRect) {
        guard let textView,
              let layoutManager = textView.layoutManager,
              let textContainer = textView.textContainer,
              let scrollView
        else { return }

        let background = usesDarkTheme
            ? NSColor(calibratedWhite: 0.13, alpha: 1)
            : NSColor(calibratedWhite: 0.95, alpha: 1)
        background.setFill()
        rect.fill()

        NSColor.separatorColor.setFill()
        NSRect(
            x: bounds.maxX - 1,
            y: bounds.minY,
            width: 1,
            height: bounds.height
        ).fill()

        let source = textView.string as NSString
        let selected = min(textView.selectedRange().location, source.length)
        let activeLine = lineNumber(at: selected, in: source)
        let visible = scrollView.contentView.bounds
        let containerVisible = visible.offsetBy(
            dx: -textView.textContainerOrigin.x,
            dy: -textView.textContainerOrigin.y
        )
        let glyphRange = layoutManager.glyphRange(
            forBoundingRect: containerVisible,
            in: textContainer
        )
        var rendered = Set<Int>()

        layoutManager.enumerateLineFragments(
            forGlyphRange: glyphRange
        ) { fragment, _, _, glyphRange, _ in
            let index = min(
                layoutManager.characterIndexForGlyph(at: glyphRange.location),
                source.length
            )
            let line = self.lineNumber(at: index, in: source)
            guard rendered.insert(line).inserted else { return }

            let y = fragment.minY
                + textView.textContainerOrigin.y
                - visible.minY
            self.drawNumber(
                line,
                y: y,
                height: fragment.height,
                active: line == activeLine
            )
        }

        if source.length == 0 {
            drawNumber(
                1,
                y: textView.textContainerOrigin.y - visible.minY,
                height: 18,
                active: true
            )
        }
    }

    private func lineNumber(at location: Int, in source: NSString) -> Int {
        guard location > 0 else { return 1 }
        var result = 1
        for index in 0..<min(location, source.length)
        where source.character(at: index) == 10 {
            result += 1
        }
        return result
    }

    private func drawNumber(
        _ number: Int,
        y: CGFloat,
        height: CGFloat,
        active: Bool
    ) {
        if active {
            NSColor.controlAccentColor
                .withAlphaComponent(usesDarkTheme ? 0.30 : 0.16)
                .setFill()
            NSBezierPath(
                roundedRect: NSRect(
                    x: 6,
                    y: y - 1,
                    width: 31,
                    height: max(18, height + 2)
                ),
                xRadius: 6,
                yRadius: 6
            ).fill()
        }

        let paragraph = NSMutableParagraphStyle()
        paragraph.alignment = .right
        ("\(number)" as NSString).draw(
            in: NSRect(x: 7, y: y + 1, width: 26, height: 16),
            withAttributes: [
                .font: NSFont.monospacedDigitSystemFont(
                    ofSize: 11,
                    weight: active ? .semibold : .regular
                ),
                .foregroundColor: active
                    ? NSColor.controlAccentColor
                    : (usesDarkTheme ? NSColor.lightGray : NSColor.darkGray),
                .paragraphStyle: paragraph
            ]
        )
    }
}

private final class FocusableTextView: NSTextView {
    var pasteImage: ((NSImage) -> String?)?

    override var acceptsFirstResponder: Bool { true }

    override func mouseDown(with event: NSEvent) {
        window?.makeKey()
        window?.makeFirstResponder(self)
        super.mouseDown(with: event)
    }

    override func menu(for event: NSEvent) -> NSMenu? {
        let menu = NSMenu()
        menu.addItem(menuItem(tr("Desfazer"), action: #selector(menuUndo), key: "z"))
        menu.addItem(menuItem(tr("Refazer"), action: #selector(menuRedo), key: "Z"))
        menu.addItem(.separator())
        menu.addItem(menuItem(tr("Cortar"), action: #selector(cut(_:)), key: "x"))
        menu.addItem(menuItem(tr("Copiar"), action: #selector(copy(_:)), key: "c"))
        menu.addItem(menuItem(tr("Colar"), action: #selector(paste(_:)), key: "v"))
        menu.addItem(.separator())
        menu.addItem(menuItem(
            tr("Selecionar tudo"),
            action: #selector(selectAll(_:)),
            key: "a"
        ))
        menu.addItem(.separator())

        let colorItem = NSMenuItem(
            title: tr("Cor do texto"),
            action: nil,
            keyEquivalent: ""
        )
        let colorMenu = NSMenu(title: tr("Cor do texto"))
        for color in noteColors.dropFirst() {
            let item = NSMenuItem(
                title: tr(color.name),
                action: #selector(applyTextColor(_:)),
                keyEquivalent: ""
            )
            item.target = self
            item.representedObject = color.hex
            item.image = colorSwatchImage(hex: color.hex)
            item.isEnabled = selectedRange().length > 0
            colorMenu.addItem(item)
        }
        colorItem.submenu = colorMenu
        menu.addItem(colorItem)
        return menu
    }

    private func menuItem(
        _ title: String,
        action: Selector,
        key: String
    ) -> NSMenuItem {
        let item = NSMenuItem(title: title, action: action, keyEquivalent: key)
        item.target = self
        return item
    }

    @objc private func menuUndo() {
        undoManager?.undo()
    }

    @objc private func menuRedo() {
        undoManager?.redo()
    }

    @objc private func applyTextColor(_ sender: NSMenuItem) {
        guard let hex = sender.representedObject as? String,
              selectedRange().length > 0
        else { return }
        let range = selectedRange()
        let selected = (string as NSString).substring(with: range)
        let markup = "<span style=\"color:\(hex)\">\(selected)</span>"
        insertText(markup, replacementRange: range)
        setSelectedRange(
            NSRange(
                location: range.location + "<span style=\"color:\(hex)\">".utf16.count,
                length: selected.utf16.count
            )
        )
    }

    override func performKeyEquivalent(with event: NSEvent) -> Bool {
        guard event.modifierFlags.contains(.command),
              let key = event.charactersIgnoringModifiers?.lowercased()
        else {
            return super.performKeyEquivalent(with: event)
        }

        switch key {
        case "v":
            if insertImageFromPasteboard() { return true }
        case "b":
            wrapSelection(prefix: "**", suffix: "**", placeholder: "texto")
            return true
        case "i":
            wrapSelection(prefix: "_", suffix: "_", placeholder: "texto")
            return true
        case "k":
            wrapSelection(prefix: "[", suffix: "](https://)", placeholder: "texto")
            return true
        default:
            break
        }
        return super.performKeyEquivalent(with: event)
    }

    override func insertTab(_ sender: Any?) {
        insertText("    ", replacementRange: selectedRange())
    }

    override func paste(_ sender: Any?) {
        if !insertImageFromPasteboard() {
            super.paste(sender)
        }
    }

    private func insertImageFromPasteboard() -> Bool {
        guard let image = pastedImage(),
              let markdown = pasteImage?(image)
        else { return false }
        insertText("\n\(markdown)\n", replacementRange: selectedRange())
        return true
    }

    private func wrapSelection(
        prefix: String,
        suffix: String,
        placeholder: String
    ) {
        let range = selectedRange()
        let selected = range.length > 0
            ? (string as NSString).substring(with: range)
            : placeholder
        insertText(prefix + selected + suffix, replacementRange: range)
        setSelectedRange(
            NSRange(
                location: range.location + prefix.utf16.count,
                length: selected.utf16.count
            )
        )
    }

    private func pastedImage() -> NSImage? {
        let pasteboard = NSPasteboard.general

        if let image = NSImage(pasteboard: pasteboard) {
            return image
        }

        let pngType = NSPasteboard.PasteboardType("public.png")
        if let data = pasteboard.data(forType: pngType),
           let image = NSImage(data: data) {
            return image
        }

        if let data = pasteboard.data(forType: .tiff),
           let image = NSImage(data: data) {
            return image
        }

        for type in pasteboard.types ?? [] {
            guard let uniformType = UTType(type.rawValue),
                  uniformType.conforms(to: .image),
                  let data = pasteboard.data(forType: type),
                  let image = NSImage(data: data)
            else { continue }
            return image
        }

        let urls = pasteboard.readObjects(
            forClasses: [NSURL.self],
            options: nil
        ) as? [URL]
        if let url = urls?.first,
           let image = NSImage(contentsOf: url) {
            return image
        }

        if let value = pasteboard.string(forType: .string),
           let url = URL(string: value),
           let image = NSImage(contentsOf: url) {
            return image
        }

        return nil
    }
}

struct MarkdownFormattingToolbar: View {
    @Binding var text: String
    @Binding var selection: NSRange
    let storeImage: (NSImage) -> String?

    var body: some View {
        ScrollView(.horizontal) {
            HStack(spacing: 4) {
                headingMenu
                button("bold", tr("Negrito")) { wrap("**", "**") }
                button("italic", tr("Itálico")) { wrap("_", "_") }
                button("strikethrough", tr("Rasurado")) { wrap("~~", "~~") }
                button("chevron.left.forwardslash.chevron.right", tr("Código")) { wrap("`", "`") }
                separator
                button("list.bullet", tr("Lista")) { prefixLines("- ") }
                button("list.number", tr("Lista numerada")) { numberedLines() }
                button("checklist", tr("Lista de tarefas")) { prefixLines("- [ ] ") }
                button("text.quote", tr("Citação")) { prefixLines("> ") }
                button("tablecells", tr("Tabela")) { insertTable() }
                button("minus", tr("Separador horizontal")) { replace("\n---\n") }
                button("link", tr("Ligação")) { wrap("[", "](https://)") }
                button("photo", tr("Adicionar imagem")) { chooseImage() }
                button("curlybraces", tr("Bloco de código")) { wrap("```\n", "\n```") }
            }
            .padding(.horizontal, 12)
            .padding(.vertical, 5)
        }
        .scrollIndicators(.hidden)
        .frame(height: 44)
        .background(.ultraThinMaterial)
        .overlay(alignment: .bottom) { Divider() }
    }

    private var separator: some View { Divider().frame(height: 20) }

    private var headingMenu: some View {
        Menu {
            Button(tr("Título 1")) { prefixLines("# ") }
            Button(tr("Título 2")) { prefixLines("## ") }
            Button(tr("Título 3")) { prefixLines("### ") }
            Button(tr("Título 4")) { prefixLines("#### ") }
            Button(tr("Título 5")) { prefixLines("##### ") }
            Button(tr("Título 6")) { prefixLines("###### ") }
        } label: {
            Label(tr("Título"), systemImage: "textformat")
                .padding(.horizontal, 10)
                .frame(height: 28)
                .background(.thinMaterial, in: Capsule())
        }
        .menuStyle(.borderlessButton)
        .fixedSize()
    }

    private func button(_ icon: String, _ help: String, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Image(systemName: icon)
                .frame(width: 28, height: 28)
                .background(
                    Color.primary.opacity(0.055),
                    in: RoundedRectangle(cornerRadius: 7)
                )
        }
        .buttonStyle(.plain)
        .help(help)
    }

    private func wrap(_ prefix: String, _ suffix: String) {
        let content = selectedText.isEmpty ? "texto" : selectedText
        replace(prefix + content + suffix, selecting: content)
    }

    private func prefixLines(_ prefix: String) {
        let content = selectedText.isEmpty ? "texto" : selectedText
        replace(content.components(separatedBy: .newlines)
            .map { prefix + $0 }.joined(separator: "\n"))
    }

    private func numberedLines() {
        let content = selectedText.isEmpty ? "item" : selectedText
        replace(content.components(separatedBy: .newlines).enumerated()
            .map { "\($0.offset + 1). \($0.element)" }.joined(separator: "\n"))
    }

    private func insertTable() {
        replace(
            """

            | \(tr("Coluna")) 1 | \(tr("Coluna")) 2 | \(tr("Coluna")) 3 |
            | --- | --- | --- |
            | \(tr("Conteúdo")) | \(tr("Conteúdo")) | \(tr("Conteúdo")) |

            """
        )
    }

    private func chooseImage() {
        let panel = NSOpenPanel()
        panel.title = tr("Adicionar imagem")
        panel.prompt = tr("Adicionar imagem")
        panel.canChooseDirectories = false
        panel.allowsMultipleSelection = false
        panel.allowedContentTypes = [.image]

        guard panel.runModal() == .OK,
              let url = panel.url,
              let image = NSImage(contentsOf: url),
              let markdown = storeImage(image)
        else { return }
        replace("\n\(markdown)\n")
    }

    private var selectedText: String {
        let source = text as NSString
        guard NSMaxRange(selection) <= source.length else { return "" }
        return source.substring(with: selection)
    }

    private func replace(_ replacement: String, selecting content: String? = nil) {
        let source = text as NSString
        let range = NSMaxRange(selection) <= source.length
            ? selection : NSRange(location: source.length, length: 0)
        text = source.replacingCharacters(in: range, with: replacement)
        if let content, let found = replacement.range(of: content) {
            selection = NSRange(
                location: range.location + replacement[..<found.lowerBound].utf16.count,
                length: content.utf16.count
            )
        } else {
            selection = NSRange(location: range.location + replacement.utf16.count, length: 0)
        }
    }
}

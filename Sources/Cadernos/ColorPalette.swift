import AppKit
import SwiftUI

enum EditorTheme: String, CaseIterable, Identifiable {
    case system, monokai, tokyoNight, dracula, solarizedLight

    var id: String { rawValue }

    var name: String {
        switch self {
        case .system: tr("Sistema")
        case .monokai: "Monokai"
        case .tokyoNight: "Tokyo Night"
        case .dracula: "Dracula"
        case .solarizedLight: "Solarized Light"
        }
    }

    func palette(colorScheme: ColorScheme) -> EditorPalette {
        switch self {
        case .system:
            let dark = colorScheme == .dark
            return EditorPalette(
                background: dark ? NSColor(hex: "#151517")! : .white,
                foreground: dark ? NSColor(hex: "#e8e8ea")! : NSColor(hex: "#24292f")!,
                heading: dark ? NSColor(hex: "#6cb6ff")! : NSColor(hex: "#0969da")!,
                emphasis: NSColor(hex: dark ? "#ffa657" : "#bc4c00")!,
                code: NSColor(hex: dark ? "#d2a8ff" : "#8250df")!,
                link: NSColor(hex: dark ? "#56d4dd" : "#087f8c")!,
                list: NSColor(hex: dark ? "#7ee787" : "#1a7f37")!,
                quote: NSColor(hex: dark ? "#a5d6ff" : "#57606a")!,
                codeBackground: NSColor(hex: dark ? "#25252a" : "#f2f4f7")!
            )
        case .monokai:
            return .init(hex: ["#272822", "#f8f8f2", "#66d9ef", "#fd971f", "#ae81ff", "#a6e22e", "#f92672", "#e6db74", "#3e3d32"])
        case .tokyoNight:
            return .init(hex: ["#1a1b26", "#c0caf5", "#7aa2f7", "#ff9e64", "#bb9af7", "#7dcfff", "#9ece6a", "#e0af68", "#24283b"])
        case .dracula:
            return .init(hex: ["#282a36", "#f8f8f2", "#8be9fd", "#ffb86c", "#bd93f9", "#50fa7b", "#ff79c6", "#f1fa8c", "#44475a"])
        case .solarizedLight:
            return .init(hex: ["#fdf6e3", "#586e75", "#268bd2", "#cb4b16", "#6c71c4", "#2aa198", "#859900", "#b58900", "#eee8d5"])
        }
    }
}

struct EditorPalette {
    let background: NSColor
    let foreground: NSColor
    let heading: NSColor
    let emphasis: NSColor
    let code: NSColor
    let link: NSColor
    let list: NSColor
    let quote: NSColor
    let codeBackground: NSColor

    init(
        background: NSColor, foreground: NSColor, heading: NSColor,
        emphasis: NSColor, code: NSColor, link: NSColor, list: NSColor,
        quote: NSColor, codeBackground: NSColor
    ) {
        self.background = background
        self.foreground = foreground
        self.heading = heading
        self.emphasis = emphasis
        self.code = code
        self.link = link
        self.list = list
        self.quote = quote
        self.codeBackground = codeBackground
    }

    init(hex: [String]) {
        self.init(
            background: NSColor(hex: hex[0])!, foreground: NSColor(hex: hex[1])!,
            heading: NSColor(hex: hex[2])!, emphasis: NSColor(hex: hex[3])!,
            code: NSColor(hex: hex[4])!, link: NSColor(hex: hex[5])!,
            list: NSColor(hex: hex[6])!, quote: NSColor(hex: hex[7])!,
            codeBackground: NSColor(hex: hex[8])!
        )
    }
}

struct NoteColor: Identifiable {
    let name: String
    let hex: String
    var id: String { hex }
}

let noteColors = [
    NoteColor(name: "Sem cor", hex: ""),
    NoteColor(name: "Vermelho", hex: "#d1242f"),
    NoteColor(name: "Laranja", hex: "#bc4c00"),
    NoteColor(name: "Amarelo", hex: "#9a6700"),
    NoteColor(name: "Verde", hex: "#1a7f37"),
    NoteColor(name: "Azul", hex: "#0969da"),
    NoteColor(name: "Roxo", hex: "#8250df")
]

extension Color {
    init?(hex: String) {
        let value = hex.trimmingCharacters(in: CharacterSet(charactersIn: "#"))
        guard value.count == 6, let number = Int(value, radix: 16) else { return nil }
        self.init(
            red: Double((number >> 16) & 0xff) / 255,
            green: Double((number >> 8) & 0xff) / 255,
            blue: Double(number & 0xff) / 255
        )
    }
}

func colorSwatchImage(hex: String) -> NSImage {
    let size = NSSize(width: 14, height: 14)
    let image = NSImage(size: size, flipped: false) { rect in
        if hex.isEmpty {
            NSColor.secondaryLabelColor.setStroke()
            let path = NSBezierPath(ovalIn: rect.insetBy(dx: 1.5, dy: 1.5))
            path.lineWidth = 1.5
            path.stroke()
            let slash = NSBezierPath()
            slash.move(to: NSPoint(x: 3, y: 3))
            slash.line(to: NSPoint(x: 11, y: 11))
            slash.lineWidth = 1.5
            slash.stroke()
        } else {
            NSColor(hex: hex)?.setFill()
            NSBezierPath(ovalIn: rect.insetBy(dx: 1, dy: 1)).fill()
        }
        return true
    }
    image.isTemplate = false
    return image
}

extension NSColor {
    convenience init?(hex: String) {
        let value = hex.trimmingCharacters(in: CharacterSet(charactersIn: "#"))
        guard value.count == 6, let number = Int(value, radix: 16) else { return nil }
        self.init(
            red: CGFloat((number >> 16) & 0xff) / 255,
            green: CGFloat((number >> 8) & 0xff) / 255,
            blue: CGFloat(number & 0xff) / 255,
            alpha: 1
        )
    }
}

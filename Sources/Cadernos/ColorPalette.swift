import AppKit
import SwiftUI

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

private extension NSColor {
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

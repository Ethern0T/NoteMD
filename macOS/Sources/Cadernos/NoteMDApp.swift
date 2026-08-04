import AppKit
import SwiftUI
import UniformTypeIdentifiers

@main
struct NoteMDApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate
    @StateObject private var library = LibraryStore.sample
    @AppStorage("appLanguage") private var appLanguage = AppLanguage.portuguese.rawValue

    var body: some Scene {
        WindowGroup {
            LibraryView()
                .environmentObject(library)
                .frame(
                    minWidth: 1_100,
                    idealWidth: 1_280,
                    minHeight: 650,
                    idealHeight: 800
                )
                .onAppear {
                    appDelegate.library = library
                    library.loadFromStorage()
                    library.reopenExternalFiles()
                    library.restoreRecoveryDrafts()
                }
                .id(appLanguage)
        }
        .windowStyle(.hiddenTitleBar)
        .defaultSize(width: 1_280, height: 800)
        .commands {
            CommandGroup(after: .newItem) {
                Button(tr("Abrir ficheiro Markdown…")) {
                    appDelegate.showOpenPanel()
                }
                .keyboardShortcut("o", modifiers: .command)
            }
        }
    }
}

@MainActor
private final class AppDelegate: NSObject, NSApplicationDelegate {
    weak var library: LibraryStore? {
        didSet {
            guard library != nil, !pendingURLs.isEmpty else { return }
            let urls = pendingURLs
            pendingURLs.removeAll()
            open(urls)
        }
    }
    private var pendingURLs: [URL] = []

    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApplication.shared.setActivationPolicy(.regular)
        NSApplication.shared.activate(ignoringOtherApps: true)
        NSApplication.shared.windows.first?.makeKeyAndOrderFront(nil)
    }

    func application(_ application: NSApplication, open urls: [URL]) {
        guard library != nil else {
            pendingURLs.append(contentsOf: urls)
            return
        }
        open(urls)
    }

    func showOpenPanel() {
        let panel = NSOpenPanel()
        panel.title = tr("Abrir ficheiro Markdown")
        panel.prompt = tr("Abrir")
        panel.allowedContentTypes = [
            UTType(filenameExtension: "md") ?? .plainText
        ]
        panel.allowsMultipleSelection = true
        panel.canChooseDirectories = false

        guard panel.runModal() == .OK else { return }
        open(panel.urls)
    }

    private func open(_ urls: [URL]) {
        let markdownURLs = urls.filter {
            ["md", "markdown", "mdown", "mkd"].contains($0.pathExtension.lowercased())
        }
        guard let library else {
            pendingURLs.append(contentsOf: markdownURLs)
            return
        }

        let failed = markdownURLs.filter { !library.openMarkdownFile(at: $0) }
        guard !failed.isEmpty else { return }
        let alert = NSAlert()
        alert.messageText = tr("Não foi possível abrir o ficheiro")
        alert.informativeText = failed.map(\.lastPathComponent).joined(separator: "\n")
        alert.runModal()
    }

    func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply {
        guard let library, !library.dirtyNoteIDs.isEmpty else {
            return .terminateNow
        }

        let alert = NSAlert()
        alert.messageText = tr("Guardar alterações antes de sair?")
        alert.informativeText = tr("Existem notas com alterações que ainda não foram guardadas.")
        alert.addButton(withTitle: tr("Guardar tudo"))
        alert.addButton(withTitle: tr("Não guardar"))
        alert.addButton(withTitle: tr("Cancelar"))

        switch alert.runModal() {
        case .alertFirstButtonReturn:
            return library.saveAllNotes() ? .terminateNow : .terminateCancel
        case .alertSecondButtonReturn:
            return .terminateNow
        default:
            return .terminateCancel
        }
    }
}

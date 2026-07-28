import AppKit
import SwiftUI

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
                }
                .id(appLanguage)
        }
        .windowStyle(.hiddenTitleBar)
        .defaultSize(width: 1_280, height: 800)
    }
}

@MainActor
private final class AppDelegate: NSObject, NSApplicationDelegate {
    weak var library: LibraryStore?

    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApplication.shared.setActivationPolicy(.regular)
        NSApplication.shared.activate(ignoringOtherApps: true)
        NSApplication.shared.windows.first?.makeKeyAndOrderFront(nil)
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

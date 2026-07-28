import AppKit
import SwiftUI

struct StorageSettingsView: View {
    @Environment(\.dismiss) private var dismiss
    @EnvironmentObject private var library: LibraryStore
    @AppStorage("notesFolderPath") private var notesFolderPath = ""
    @AppStorage("appLanguage") private var appLanguage = AppLanguage.portuguese.rawValue

    var body: some View {
        VStack(alignment: .leading, spacing: 20) {
            HStack {
                Image(systemName: "gearshape")
                    .font(.title2)
                Text(tr("Configurações"))
                    .font(.title2.bold())
                Spacer()
            }

            Picker(tr("Idioma"), selection: $appLanguage) {
                ForEach(AppLanguage.allCases) { language in
                    Text(language.name).tag(language.rawValue)
                }
            }
            .pickerStyle(.segmented)

            GroupBox(tr("Localização das notas")) {
                VStack(alignment: .leading, spacing: 12) {
                    Text(tr("Os notebooks, notas e respetivas imagens serão guardados nesta pasta."))
                        .foregroundStyle(.secondary)

                    HStack {
                        Image(systemName: "folder")
                            .foregroundStyle(.secondary)
                        if notesFolderPath.isEmpty {
                            Text(tr("Nenhuma pasta selecionada"))
                                .foregroundStyle(.secondary)
                        } else {
                            Text(notesFolderPath)
                                .lineLimit(1)
                                .truncationMode(.middle)
                                .textSelection(.enabled)
                        }
                        Spacer()
                        Button(tr("Selecionar pasta…"), action: selectFolder)
                    }

                    Text(tr("Para atribuir um nome novo, use o botão Nova Pasta no seletor do Finder."))
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
                .padding(8)
            }

            Text(tr("Estrutura planeada: Notebook / Nota / note.md + assets"))
                .font(.caption)
                .foregroundStyle(.secondary)

            HStack {
                Spacer()
                Button(tr("Concluído")) {
                    dismiss()
                }
                .keyboardShortcut(.defaultAction)
            }
        }
        .padding(24)
        .frame(width: 560)
    }

    private func selectFolder() {
        let panel = NSOpenPanel()
        panel.title = tr("Escolher pasta das notas")
        panel.prompt = tr("Escolher")
        panel.canChooseFiles = false
        panel.canChooseDirectories = true
        panel.allowsMultipleSelection = false
        panel.canCreateDirectories = true

        guard panel.runModal() == .OK, let url = panel.url else { return }
        notesFolderPath = url.path
        library.loadFromStorage(force: true)
    }
}

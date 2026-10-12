#include "internal.h"

int main(int argc, char **argv) {
    if ((argc == 4 || argc == 5) && strcmp(argv[1], "--export-pdf") == 0) {
        load_gtk();
        if (access(argv[2], R_OK)) { fprintf(stderr, "Não foi possível ler a nota.\n"); return 1; }
        char *markdown = read_file(argv[2]), base[PATH_MAX], title[256];
        snprintf(base, sizeof base, "%s", argv[2]); char *slash = strrchr(base, '/');
        snprintf(title, sizeof title, "%s", argc == 5 ? argv[4] : slash ? slash + 1 : base);
        if (argc != 5) { char *dot = strrchr(title, '.'); if (dot && !strcasecmp(dot, ".md")) *dot = 0; }
        if (slash) { if (slash == base) slash[1] = 0; else *slash = 0; } else snprintf(base, sizeof base, ".");
        bool ok = export_pdf_content(argv[3], title, markdown, base); free(markdown);
        if (pdf_missing_image_count()) fprintf(stderr, "PDF: %u imagem(ns) indisponível(is).\n", pdf_missing_image_count());
        if (!ok) fprintf(stderr, "Não foi possível exportar o PDF.\n");
        return ok ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1], "--self-test") == 0) return self_test();
    if (argc == 2 && strcmp(argv[1], "--settings-self-test") == 0) return settings_self_test();
    if (argc == 2 && strcmp(argv[1], "--markdown-self-test") == 0) { load_gtk(); resolve_data_dir(); return markdown_self_test(); }
    if (argc == 2 && strcmp(argv[1], "--export-self-test") == 0) { load_gtk(); return export_self_test(); }
    bool empty_ui_test = argc == 2 && strcmp(argv[1], "--empty-ui-self-test") == 0;
    bool ui_test = (argc == 2 && strcmp(argv[1], "--ui-self-test") == 0) || empty_ui_test;
    bool new_instance = argc == 2 && strcmp(argv[1], "--new-instance") == 0;
    if (new_instance) setenv("GSK_RENDERER", "gl", 0);
    if (ui_test) {
        snprintf(state.ui_test_dir, sizeof state.ui_test_dir, "/tmp/notemd-ui-test-XXXXXX");
        if (!mkdtemp(state.ui_test_dir)) return 1;
        char config[PATH_MAX], data[PATH_MAX]; snprintf(config, sizeof config, "%s/xdg-config", state.ui_test_dir);
        snprintf(data, sizeof data, "%s/xdg-data", state.ui_test_dir); setenv("XDG_CONFIG_HOME", config, 1); setenv("XDG_DATA_HOME", data, 1);
    }
    load_gtk(); load_settings(); resolve_root(); resolve_data_dir();
    if (ui_test) { state.ui_test = true; state.empty_ui_test = empty_ui_test; state.external_count = 0; if (!prepare_ui_test()) return 1; }
    load_library(); if (!ui_test) restore_recovery();
    if (ui_test) {
        for (Notebook *book = state.notebooks; book; book = book->next) extract_notebook_metadata(book);
        for (Note *note = state.notes; note; note = note->next) {
            extract_metadata(note);
            note->metadata_loaded = true;
        }
        resort_library_notes();
    } else {
        start_background_metadata_indexer();
    }
    for (size_t index = 0; index < state.external_count; index++) add_external_note(state.external_paths[index]);
    int application_flags = G_APPLICATION_HANDLES_OPEN | (new_instance ? G_APPLICATION_NON_UNIQUE : 0);
    state.app = gtk_application_new("pt.notemd.NoteMD", application_flags);
    g_signal_connect_data(state.app, "activate", (void *)activate, NULL, NULL, 0);
    g_signal_connect_data(state.app, "open", (void *)open_application_files, NULL, NULL, 0);
    int run_argc = (ui_test || new_instance) ? 1 : argc;
    int result = g_application_run(state.app, run_argc, argv);
    g_object_unref(state.app);
    if (ui_test) { cleanup_ui_test(); return state.ui_test_failed ? 1 : result; }
    return result;
}

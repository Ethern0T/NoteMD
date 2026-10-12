#include "internal.h"

int self_test(void) {
    char temporary[] = "/tmp/notemd-selftest-XXXXXX";
    if (!mkdtemp(temporary)) { fprintf(stderr, "self-test: mkdtemp falhou\n"); return 1; }
    char notebook_path[PATH_MAX], note_path[PATH_MAX], markdown_path[PATH_MAX];
    snprintf(notebook_path, sizeof notebook_path, "%s/Testes", temporary);
    snprintf(note_path, sizeof note_path, "%s/Nota", notebook_path);
    snprintf(markdown_path, sizeof markdown_path, "%s/note.md", note_path);
    bool ok = make_directory(note_path) && write_atomic(markdown_path, "# Teste\n");
    char *restored = read_file(markdown_path); ok = ok && strcmp(restored, "# Teste\n") == 0; free(restored);
    Notebook notebook = {0}; Note note = {0}; snprintf(notebook.title, sizeof notebook.title, "Testes");
    snprintf(notebook.path, sizeof notebook.path, "%s", notebook_path); snprintf(note.id, sizeof note.id, "self-test-id");
    note.notebook = &notebook; state.notes = &note; save_notebook_metadata(&notebook);
    char metadata_path[PATH_MAX]; snprintf(metadata_path, sizeof metadata_path, "%s/.notebook.json", notebook_path);
    char *metadata = read_file(metadata_path); ok = ok && strstr(metadata, "self-test-id") != NULL; free(metadata);
    char note_metadata[PATH_MAX], assets[PATH_MAX], asset[PATH_MAX], archive[PATH_MAX], library[PATH_MAX], imported[PATH_MAX] = "";
    snprintf(note_metadata, sizeof note_metadata, "%s/.note.json", note_path); write_atomic(note_metadata, "{\"id\":\"self-test-id\",\"tags\":[\"arquivo\"]}\n");
    snprintf(assets, sizeof assets, "%s/assets", note_path); make_directory(assets);
    snprintf(asset, sizeof asset, "%s/test.txt", assets); write_atomic(asset, "asset completo\n");
    snprintf(archive, sizeof archive, "%s/Testes.notemd.zip", temporary);
    snprintf(library, sizeof library, "%s/imported", temporary); make_directory(library); snprintf(state.root, sizeof state.root, "%s", library);
    ok = export_notebook_archive(&notebook, archive) && import_notebook_archive(archive, imported) && ok;
    char imported_markdown[PATH_MAX], imported_metadata[PATH_MAX], imported_asset[PATH_MAX];
    snprintf(imported_markdown, sizeof imported_markdown, "%s/Nota/note.md", imported);
    snprintf(imported_metadata, sizeof imported_metadata, "%s/Nota/.note.json", imported);
    snprintf(imported_asset, sizeof imported_asset, "%s/Nota/assets/test.txt", imported);
    char *archive_markdown = read_file(imported_markdown), *archive_metadata = read_file(imported_metadata), *archive_asset = read_file(imported_asset);
    ok = ok && strcmp(archive_markdown, "# Teste\n") == 0 && strstr(archive_metadata, "self-test-id") && strcmp(archive_asset, "asset completo\n") == 0;
    free(archive_markdown); free(archive_metadata); free(archive_asset);
    snprintf(state.language, sizeof state.language, "en");
    ok = ok && strcmp(tr("Pesquisar e substituir"), "Find and replace") == 0 && strcmp(tr("Duplicar bloco"), "Duplicate block") == 0;
    snprintf(state.language, sizeof state.language, "fr");
    ok = ok && strcmp(tr("Sem tags"), "Aucun tag") == 0 && strcmp(tr("Exportar PDF"), "Exporter en PDF") == 0;
    snprintf(state.language, sizeof state.language, "pt");
    state.notes = NULL; remove_tree(temporary);
    printf("NoteMD self-test: %s\n", ok ? "OK" : "FALHOU"); return ok ? 0 : 1;
}



int export_self_test(void) {
    const char *markdown = "## Secção\n\nTexto com **negrito**, _itálico_, `código`, <span style=\"color:#0969da\">azul</span> e [ligação](https://example.com).\n\n"
        "- item\n1. primeiro\n- [ ] tarefa\n- [x] concluída\n\n> citação\n\n"
        "| Nome | Valor |\n| --- | --- |\n| Um | Dois |\n\n```c\nint main(void);\n```\n\n![Imagem](assets/teste.png)\n";
    char pdf[] = "/tmp/notemd-export-test.pdf", docx[] = "/tmp/notemd-export-test.docx";
    bool ok = export_pdf_content(pdf, "Teste NoteMD", markdown, ".") && docx_export_content(docx, markdown);
    char *html = html_document("Teste NoteMD", markdown);
    const char *html_markers[] = {"<h2>", "<strong>", "<em>", "<code>", "<a href=", "<ul>", "<ol start=",
        "class=\"task\"", "<s>", "<span style=\"color:#0969da\">", "<blockquote>", "<table>", "<pre><code>", "<img src="};
    for (size_t index = 0; index < sizeof html_markers / sizeof html_markers[0]; index++) {
        bool present = strstr(html, html_markers[index]) != NULL;
        if (!present) fprintf(stderr, "export-self-test: HTML em falta: %s\n", html_markers[index]);
        ok = ok && present;
    }
    char *document_xml = docx_document_xml(markdown);
    const char *docx_markers[] = {"<w:b/>", "<w:i/>", "Consolas", "<w:numId w:val=\"1\"/>",
        "<w:numId w:val=\"2\"/>", "<w:strike/>", "<w:color w:val=\"0969da\"/>", "<w:pBdr>", "<w:tbl>"};
    for (size_t index = 0; index < sizeof docx_markers / sizeof docx_markers[0]; index++) {
        bool present = strstr(document_xml, docx_markers[index]) != NULL;
        if (!present) fprintf(stderr, "export-self-test: DOCX em falta: %s\n", docx_markers[index]);
        ok = ok && present;
    }
    char *pdf_data = read_file(pdf), *docx_data = read_file(docx);
    ok = ok && strncmp(pdf_data, "%PDF-", 5) == 0 && (unsigned char)docx_data[0] == 'P' && (unsigned char)docx_data[1] == 'K';
    free(document_xml); free(html); free(pdf_data); free(docx_data); unlink(pdf); unlink(docx);
    printf("NoteMD export self-test: %s\n", ok ? "OK" : "FALHOU"); return ok ? 0 : 1;
}

bool prepare_ui_test(void) {
    if (!state.ui_test_dir[0]) {
        snprintf(state.ui_test_dir, sizeof state.ui_test_dir, "/tmp/notemd-ui-test-XXXXXX");
        if (!mkdtemp(state.ui_test_dir)) return false;
    }
    char test_config[PATH_MAX]; snprintf(test_config, sizeof test_config, "%s/config", state.ui_test_dir);
    if (setenv("XDG_CONFIG_HOME", test_config, 1) != 0) return false;
    char library[PATH_MAX], notebook[PATH_MAX], note[PATH_MAX], markdown[PATH_MAX], metadata[PATH_MAX];
    snprintf(library, sizeof library, "%s/library", state.ui_test_dir); if (!make_directory(library)) return false;
    if (state.empty_ui_test) { snprintf(state.root, sizeof state.root, "%s", library); return true; }
    snprintf(notebook, sizeof notebook, "%s/Testes", library);
    snprintf(note, sizeof note, "%s/Nota dividida", notebook); if (!make_directory(note)) return false;
    snprintf(markdown, sizeof markdown, "%s/note.md", note); snprintf(metadata, sizeof metadata, "%s/.note.json", note);
    if (!write_atomic(markdown, "# Teste\n\nConteúdo dividido\n\n- item\n")) return false;
    if (!write_atomic(metadata, "{\"id\":\"ui-test-note\",\"colorHex\":null,\"tags\":[\"teste\"]}")) return false;
    snprintf(state.root, sizeof state.root, "%s", library); return true;
}

void remove_test_tree(const char *path) {
    struct stat info; if (lstat(path, &info) != 0) return;
    if (!S_ISDIR(info.st_mode)) { unlink(path); return; }
    DIR *directory = opendir(path); if (!directory) return; struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char child[PATH_MAX]; snprintf(child, sizeof child, "%s/%s", path, entry->d_name); remove_test_tree(child);
    }
    closedir(directory); rmdir(path);
}

void cleanup_ui_test(void) {
    if (strncmp(state.ui_test_dir, "/tmp/notemd-ui-test-", 20) == 0) remove_test_tree(state.ui_test_dir);
}

int settings_self_test(void) {
    char temporary[] = "/tmp/notemd-settings-test-XXXXXX";
    if (!mkdtemp(temporary) || setenv("XDG_CONFIG_HOME", temporary, 1) != 0) return 1;
    load_settings();
    bool ok = state.editor_font_size == EDITOR_FONT_DEFAULT;
    snprintf(state.root, sizeof state.root, "/tmp/NoteMD-notas-de-teste");
    snprintf(state.theme, sizeof state.theme, "dracula");
    snprintf(state.language, sizeof state.language, "fr");
    state.autosave = false; state.editor_font_size = 19;
    Note external = {0}; external.external = true;
    snprintf(external.markdown_path, sizeof external.markdown_path, "/tmp/nota externa.md");
    state.notes = &external; save_settings(); state = (State){0}; load_settings();
    ok = ok && state.editor_font_size == 19 && strcmp(state.theme, "dracula") == 0 &&
        strcmp(state.language, "fr") == 0 && !state.autosave && state.external_count == 1 &&
        strcmp(state.external_paths[0], "/tmp/nota externa.md") == 0 &&
        strcmp(state.root, "/tmp/NoteMD-notas-de-teste") == 0;
    char path[PATH_MAX]; config_path(path);
    const char *cases[] = {"", "11", "25", "-1", "16oops", "999999999999999999999999", "12", "24"};
    for (size_t index = 0; index < sizeof cases / sizeof cases[0]; index++) {
        char contents[128]; snprintf(contents, sizeof contents, "theme=white\neditor_font_size=%s\n", cases[index]);
        ok = write_atomic(path, contents) && ok;
        state = (State){0}; load_settings();
        int expected = index == 6 ? EDITOR_FONT_MIN : index == 7 ? EDITOR_FONT_MAX : EDITOR_FONT_DEFAULT;
        ok = ok && state.editor_font_size == expected && strcmp(state.theme, "white") == 0;
    }
    ok = write_atomic(path, "theme=monokai\nautosave=1\n") && ok;
    state = (State){0}; load_settings();
    ok = ok && state.editor_font_size == EDITOR_FONT_DEFAULT && strcmp(state.theme, "monokai") == 0 && state.autosave;
    remove_test_tree(temporary);
    printf("NoteMD settings self-test: %s\n", ok ? "OK" : "FALHOU"); return ok ? 0 : 1;
}


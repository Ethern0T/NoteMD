#include "internal.h"

char *html_escape(const char *source) {
    size_t capacity = strlen(source) * 6 + 1, used = 0;
    char *result = calloc(capacity, 1);
    for (; *source; source++) {
        const char *replacement = NULL;
        if (*source == '&') replacement = "&amp;";
        else if (*source == '<') replacement = "&lt;";
        else if (*source == '>') replacement = "&gt;";
        if (replacement) { size_t length = strlen(replacement); memcpy(result + used, replacement, length); used += length; }
        else result[used++] = *source;
    }
    return result;
}



void builder_append_n(StringBuilder *builder, const char *value, size_t length) {
    if (builder->length + length + 1 > builder->capacity) {
        size_t capacity = builder->capacity ? builder->capacity : 1024;
        while (capacity < builder->length + length + 1) capacity *= 2;
        builder->value = realloc(builder->value, capacity); builder->capacity = capacity;
    }
    memcpy(builder->value + builder->length, value, length); builder->length += length;
    builder->value[builder->length] = '\0';
}

void builder_append(StringBuilder *builder, const char *value) {
    builder_append_n(builder, value, strlen(value));
}

void builder_printf(StringBuilder *builder, const char *format, ...) {
    va_list arguments; va_start(arguments, format); va_list copy; va_copy(copy, arguments);
    int required = vsnprintf(NULL, 0, format, copy); va_end(copy);
    if (required > 0) {
        char *value = malloc((size_t)required + 1); vsnprintf(value, (size_t)required + 1, format, arguments);
        builder_append_n(builder, value, (size_t)required); free(value);
    }
    va_end(arguments);
}

void builder_escape_n(StringBuilder *builder, const char *source, size_t length) {
    for (size_t index = 0; index < length; index++) {
        if (source[index] == '&') builder_append(builder, "&amp;");
        else if (source[index] == '<') builder_append(builder, "&lt;");
        else if (source[index] == '>') builder_append(builder, "&gt;");
        else if (source[index] == '\"') builder_append(builder, "&quot;");
        else builder_append_n(builder, source + index, 1);
    }
}

#include "markdown.h"

bool markdown_heading(const char *line, int *level, const char **text) {
    int count = 0; while (line[count] == '#' && count < 6) count++;
    if (!count || line[count] != ' ') return false;
    *level = count; *text = line + count + 1; return true;
}

bool markdown_image(const char *line, const char **alt, size_t *alt_length, const char **source, size_t *source_length) {
    if (strncmp(line, "![", 2) != 0) return false;
    const char *middle = strstr(line + 2, "]("), *end = middle ? strrchr(middle + 2, ')') : NULL;
    if (!middle || !end || end[1]) return false;
    *alt = line + 2; *alt_length = (size_t)(middle - line - 2);
    *source = middle + 2; *source_length = (size_t)(end - middle - 2); return true;
}

char *html_document(const char *title, const char *markdown) {
    char *body = markdown_to_html(markdown), *safe_title = html_escape(title); StringBuilder document = {0};
    builder_append(&document, "<!doctype html><html lang=\"pt\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width\">");
    builder_printf(&document, "<title>%s</title><style>", safe_title);
    builder_append(&document, "body{max-width:820px;margin:48px auto;padding:0 24px;font:16px/1.62 system-ui;color:#24292f}h1{border-bottom:2px solid #e8eaed;padding-bottom:12px}h2{color:#174ea6}blockquote{background:#f7f9fc;border-left:4px solid #4285f4;padding:10px 16px}code,pre{background:#f3f4f6;border-radius:6px}code{padding:2px 5px}pre{padding:14px;white-space:pre-wrap}img{display:block;max-width:100%;height:auto;margin:24px auto;border-radius:8px}table{border-collapse:collapse;width:100%;margin:18px 0}th,td{border:1px solid #d0d7de;padding:8px 10px;text-align:left}th{background:#f3f4f6}.done{opacity:.7}.wikilink{color:#0969da}@media(prefers-color-scheme:dark){body{background:#11141a;color:#e8eaf0}h2,.wikilink{color:#78a9ff}blockquote,code,pre,th{background:#1b2029}th,td{border-color:#394150}}</style></head><body>");
    builder_printf(&document, "<h1>%s</h1>", safe_title); builder_append(&document, body); builder_append(&document, "</body></html>");
    free(body); free(safe_title); return document.value;
}

void export_html_response(Ptr dialog, int response, Ptr unused) {
    (void)unused;
    if (response == GTK_RESPONSE_ACCEPT && state.active) {
        Ptr file = gtk_file_chooser_get_file(dialog); char *path = file ? g_file_get_path(file) : NULL;
        if (path) {
            char *markdown = editor_text(), *document = html_document(state.active->title, markdown);
            bool ok = write_atomic(path, document); gtk_label_set_text(state.status, ok ? "HTML exportado" : "Erro ao exportar HTML");
            free(document); g_free(markdown); g_free(path);
        }
    }
    g_object_unref(dialog);
}

void export_html(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    if (!state.active) return;
    Ptr dialog = gtk_file_chooser_native_new(tr("Exportar HTML"), state.window,
        GTK_FILE_CHOOSER_ACTION_SAVE, tr("Exportar"), tr("Cancelar"));
    char filename[300]; snprintf(filename, sizeof filename, "%s.html", state.active->title);
    gtk_file_chooser_set_current_name(dialog, filename);
    g_signal_connect_data(dialog, "response", (void *)export_html_response, NULL, NULL, 0);
    gtk_native_dialog_show(dialog);
}

#include "pdf.h"

bool pdf_export(const char *destination) {
    char *markdown = editor_text();
    bool ok = pdf_export_content(destination, note_display_title(state.active), markdown, state.active->directory);
    g_free(markdown); return ok;
}

void docx_run(StringBuilder *output, const char *text, size_t length, const char *properties) {
    builder_append(output, "<w:r>"); if (properties && *properties) builder_printf(output, "<w:rPr>%s</w:rPr>", properties);
    builder_append(output, "<w:t xml:space=\"preserve\">"); builder_escape_n(output, text, length);
    builder_append(output, "</w:t></w:r>");
}

void docx_inline(StringBuilder *output, const char *source) {
    const char *cursor = source;
    while (*cursor) {
        const char *close = NULL;
        if (strncmp(cursor, "<span style=\"color:#", 20) == 0) {
            const char *content = strchr(cursor + 20, '>'), *end = content ? strstr(content + 1, "</span>") : NULL;
            if (content && end && content - cursor == 27) {
                char color[7]; memcpy(color, cursor + 20, 6); color[6] = '\0'; char properties[64];
                bool valid = true; for (size_t index = 0; index < 6; index++) if (!isxdigit((unsigned char)color[index])) valid = false;
                if (valid) {
                    snprintf(properties, sizeof properties, "<w:color w:val=\"%s\"/>", color);
                    docx_run(output, content + 1, (size_t)(end - content - 1), properties); cursor = end + 7; continue;
                }
            }
        }
        if (strncmp(cursor, "**", 2) == 0 && (close = strstr(cursor + 2, "**"))) {
            docx_run(output, cursor + 2, (size_t)(close - cursor - 2), "<w:b/>"); cursor = close + 2; continue;
        }
        if (strncmp(cursor, "~~", 2) == 0 && (close = strstr(cursor + 2, "~~"))) {
            docx_run(output, cursor + 2, (size_t)(close - cursor - 2), "<w:strike/>"); cursor = close + 2; continue;
        }
        if (*cursor == '_' && (close = strchr(cursor + 1, '_'))) {
            docx_run(output, cursor + 1, (size_t)(close - cursor - 1), "<w:i/>"); cursor = close + 1; continue;
        }
        if (*cursor == '`' && (close = strchr(cursor + 1, '`'))) {
            docx_run(output, cursor + 1, (size_t)(close - cursor - 1), "<w:rFonts w:ascii=\"Consolas\" w:hAnsi=\"Consolas\"/><w:shd w:fill=\"F3F4F6\"/>");
            cursor = close + 1; continue;
        }
        if (*cursor == '[' && cursor[1] != '[' && (close = strstr(cursor + 1, "]("))) {
            const char *end = strchr(close + 2, ')');
            if (end) {
                docx_run(output, cursor + 1, (size_t)(close - cursor - 1), "<w:color w:val=\"0969DA\"/><w:u w:val=\"single\"/>");
                cursor = end + 1; continue;
            }
        }
        const char *special = cursor + 1;
        while (*special && *special != '*' && *special != '~' && *special != '_' && *special != '`' && *special != '[' && *special != '<') special++;
        docx_run(output, cursor, (size_t)(special - cursor), NULL); cursor = special;
    }
}

void docx_paragraph(StringBuilder *output, const char *text, const char *paragraph_properties, const char *run_properties) {
    builder_append(output, "<w:p>");
    if (paragraph_properties && *paragraph_properties) builder_printf(output, "<w:pPr>%s</w:pPr>", paragraph_properties);
    if (run_properties) docx_run(output, text, strlen(text), run_properties); else docx_inline(output, text);
    builder_append(output, "</w:p>");
}

void docx_table_row(StringBuilder *output, const char *line, bool header) {
    char *copy = strdup(line), *value = copy; while (*value == ' ' || *value == '|') value++;
    size_t length = strlen(value); while (length && (value[length - 1] == ' ' || value[length - 1] == '|')) value[--length] = '\0';
    builder_append(output, "<w:tr>"); char *save = NULL;
    for (char *cell = strtok_r(value, "|", &save); cell; cell = strtok_r(NULL, "|", &save)) {
        while (*cell == ' ') cell++;
        size_t cell_length = strlen(cell);
        while (cell_length && cell[cell_length - 1] == ' ') cell[--cell_length] = '\0';
        builder_append(output, "<w:tc><w:tcPr><w:tcW w:w=\"0\" w:type=\"auto\"/></w:tcPr><w:p>");
        if (header) docx_run(output, cell, strlen(cell), "<w:b/>"); else docx_inline(output, cell);
        builder_append(output, "</w:p></w:tc>");
    }
    builder_append(output, "</w:tr>"); free(copy);
}

char *docx_document_xml(const char *markdown) {
    size_t markdown_length = strlen(markdown), line_count = 1;
    char *copy = strdup(markdown); for (size_t index = 0; index < markdown_length; index++) if (copy[index] == '\n') line_count++;
    char **lines = calloc(line_count + 1, sizeof *lines); size_t count = 0; lines[count++] = copy;
    for (size_t index = 0; index < markdown_length; index++) if (copy[index] == '\n') { copy[index] = '\0'; lines[count++] = copy + index + 1; }
    StringBuilder xml = {0}; builder_append(&xml, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\"><w:body>");
    bool code = false;
    for (size_t index = 0; index < count; index++) {
        const char *line = lines[index];
        if (strncmp(line, "```", 3) == 0) { code = !code; continue; }
        if (code) { docx_paragraph(&xml, line, "<w:shd w:fill=\"F3F4F6\"/><w:spacing w:after=\"0\"/>",
            "<w:rFonts w:ascii=\"Consolas\" w:hAnsi=\"Consolas\"/><w:sz w:val=\"20\"/>"); continue; }
        if (!*line) { builder_append(&xml, "<w:p/>"); continue; }
        if (strchr(line, '|') && index + 1 < count && strstr(lines[index + 1], "---")) {
            builder_append(&xml, "<w:tbl><w:tblPr><w:tblBorders><w:top w:val=\"single\" w:sz=\"4\"/><w:left w:val=\"single\" w:sz=\"4\"/><w:bottom w:val=\"single\" w:sz=\"4\"/><w:right w:val=\"single\" w:sz=\"4\"/><w:insideH w:val=\"single\" w:sz=\"4\"/><w:insideV w:val=\"single\" w:sz=\"4\"/></w:tblBorders></w:tblPr>");
            docx_table_row(&xml, line, true); index += 2;
            while (index < count && strchr(lines[index], '|') && *lines[index]) { docx_table_row(&xml, lines[index], false); index++; }
            builder_append(&xml, "</w:tbl>"); index--; continue;
        }
        int level; const char *heading;
        if (markdown_heading(line, &level, &heading)) {
            char properties[160]; int size = 38 - (level - 1) * 4; if (size < 24) size = 24;
            snprintf(properties, sizeof properties, "<w:b/><w:sz w:val=\"%d\"/><w:color w:val=\"%s\"/>", size, level == 2 ? "174EA6" : "202124");
            docx_paragraph(&xml, heading, "<w:keepNext/><w:spacing w:before=\"240\" w:after=\"100\"/>", properties);
        } else if (strncmp(line, "- [ ] ", 6) == 0 || strncmp(line, "* [ ] ", 6) == 0) {
            StringBuilder task = {0}; builder_append(&task, "☐ "); builder_append(&task, line + 6); docx_paragraph(&xml, task.value, "<w:ind w:left=\"360\"/>", NULL); free(task.value);
        } else if (strncmp(line, "- [x] ", 6) == 0 || strncmp(line, "- [X] ", 6) == 0) {
            StringBuilder task = {0}; builder_append(&task, "☑ "); builder_append(&task, line + 6); docx_paragraph(&xml, task.value, "<w:ind w:left=\"360\"/>", "<w:strike/><w:color w:val=\"6B7280\"/>"); free(task.value);
        } else if (strncmp(line, "- ", 2) == 0 || strncmp(line, "* ", 2) == 0) {
            docx_paragraph(&xml, line + 2, "<w:numPr><w:ilvl w:val=\"0\"/><w:numId w:val=\"1\"/></w:numPr>", NULL);
        } else if (line[0] >= '0' && line[0] <= '9' && strchr(line, '.') && strchr(line, '.')[1] == ' ') {
            docx_paragraph(&xml, strchr(line, '.') + 2, "<w:numPr><w:ilvl w:val=\"0\"/><w:numId w:val=\"2\"/></w:numPr>", NULL);
        } else if (strncmp(line, "> ", 2) == 0) {
            docx_paragraph(&xml, line + 2, "<w:ind w:left=\"420\"/><w:pBdr><w:left w:val=\"single\" w:sz=\"18\" w:color=\"4285F4\"/></w:pBdr>", "<w:i/><w:color w:val=\"4A5568\"/>");
        } else if (strcmp(line, "---") == 0 || strcmp(line, "***") == 0) {
            builder_append(&xml, "<w:p><w:pPr><w:pBdr><w:bottom w:val=\"single\" w:sz=\"6\" w:color=\"D0D7DE\"/></w:pBdr></w:pPr></w:p>");
        } else docx_paragraph(&xml, line, NULL, NULL);
    }
    builder_append(&xml, "<w:sectPr><w:pgSz w:w=\"11906\" w:h=\"16838\"/><w:pgMar w:top=\"1440\" w:right=\"1440\" w:bottom=\"1440\" w:left=\"1440\"/></w:sectPr></w:body></w:document>");
    free(lines); free(copy); return xml.value;
}

bool docx_export_content(const char *destination, const char *markdown) {
    char temporary[] = "/tmp/notemd-docx-XXXXXX"; if (!mkdtemp(temporary)) return false;
    char rels_dir[PATH_MAX], word_dir[PATH_MAX], word_rels_dir[PATH_MAX], content_path[PATH_MAX], rels_path[PATH_MAX];
    char document_path[PATH_MAX], numbering_path[PATH_MAX], document_rels_path[PATH_MAX];
    snprintf(rels_dir, sizeof rels_dir, "%s/_rels", temporary); snprintf(word_dir, sizeof word_dir, "%s/word", temporary);
    snprintf(word_rels_dir, sizeof word_rels_dir, "%s/_rels", word_dir);
    make_directory(rels_dir); make_directory(word_dir); make_directory(word_rels_dir);
    snprintf(content_path, sizeof content_path, "%s/[Content_Types].xml", temporary);
    snprintf(rels_path, sizeof rels_path, "%s/.rels", rels_dir); snprintf(document_path, sizeof document_path, "%s/document.xml", word_dir);
    snprintf(numbering_path, sizeof numbering_path, "%s/numbering.xml", word_dir);
    snprintf(document_rels_path, sizeof document_rels_path, "%s/document.xml.rels", word_rels_dir);
    const char *types = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/><Override PartName=\"/word/document.xml\" "
        "ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
        "<Override PartName=\"/word/numbering.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.numbering+xml\"/></Types>";
    const char *rels = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/></Relationships>";
    const char *document_rels = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/numbering\" Target=\"numbering.xml\"/></Relationships>";
    const char *numbering = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><w:numbering xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:abstractNum w:abstractNumId=\"1\"><w:lvl w:ilvl=\"0\"><w:start w:val=\"1\"/><w:numFmt w:val=\"bullet\"/><w:lvlText w:val=\"•\"/><w:pPr><w:ind w:left=\"720\" w:hanging=\"360\"/></w:pPr></w:lvl></w:abstractNum>"
        "<w:abstractNum w:abstractNumId=\"2\"><w:lvl w:ilvl=\"0\"><w:start w:val=\"1\"/><w:numFmt w:val=\"decimal\"/><w:lvlText w:val=\"%1.\"/><w:pPr><w:ind w:left=\"720\" w:hanging=\"360\"/></w:pPr></w:lvl></w:abstractNum>"
        "<w:num w:numId=\"1\"><w:abstractNumId w:val=\"1\"/></w:num><w:num w:numId=\"2\"><w:abstractNumId w:val=\"2\"/></w:num></w:numbering>";
    char *document = docx_document_xml(markdown);
    bool ok = write_atomic(content_path, types) && write_atomic(rels_path, rels) && write_atomic(document_path, document)
        && write_atomic(numbering_path, numbering) && write_atomic(document_rels_path, document_rels); free(document);
    unlink(destination);
    if (ok) {
        pid_t child = fork();
        if (child == 0) { chdir(temporary); execlp("zip", "zip", "-q", "-r", destination, ".", (char *)NULL); _exit(127); }
        int status = 0; ok = child > 0 && waitpid(child, &status, 0) > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
    unlink(content_path); unlink(rels_path); unlink(document_path); unlink(numbering_path); unlink(document_rels_path);
    rmdir(word_rels_dir); rmdir(rels_dir); rmdir(word_dir); rmdir(temporary); return ok;
}

bool docx_export(const char *destination) {
    char *markdown = editor_text(); bool ok = docx_export_content(destination, markdown); g_free(markdown); return ok;
}

bool office_export(const char *destination, const char *format) {
    return strcmp(format, "pdf") == 0 ? pdf_export(destination) : docx_export(destination);
}

void office_export_response(Ptr dialog, int response, Ptr user_data) {
    const char *format = user_data;
    if (response == GTK_RESPONSE_ACCEPT && state.active) {
        Ptr file = gtk_file_chooser_get_file(dialog); char *path = file ? g_file_get_path(file) : NULL;
        if (path) {
            bool ok = office_export(path, format);
            gtk_label_set_text(state.status, ok ? (strcmp(format, "pdf") == 0 ?
                (pdf_missing_images ? "PDF exportado · algumas imagens estão indisponíveis" : "PDF exportado") : "DOCX exportado")
                                                 : "Não foi possível exportar o documento");
            g_free(path);
        }
    }
    g_object_unref(dialog);
}

void export_office(Ptr button, Ptr user_data) {
    (void)button; const char *format = user_data; if (!state.active) return;
    const char *title = strcmp(format, "pdf") == 0 ? tr("Exportar PDF") : tr("Exportar DOCX");
    Ptr dialog = gtk_file_chooser_native_new(title, state.window, GTK_FILE_CHOOSER_ACTION_SAVE, tr("Exportar"), tr("Cancelar"));
    char filename[300], safe[256]; safe_title(note_display_title(state.active), safe);
    snprintf(filename, sizeof filename, "%s.%s", safe, format);
    gtk_native_dialog_set_modal(dialog, true);
    Ptr filter = g_object_ref_sink(gtk_file_filter_new());
    gtk_file_filter_set_name(filter, strcmp(format, "pdf") == 0 ? "PDF (*.pdf)" : "Word (*.docx)");
    gtk_file_filter_add_pattern(filter, strcmp(format, "pdf") == 0 ? "*.pdf" : "*.docx");
    gtk_file_filter_add_mime_type(filter, strcmp(format, "pdf") == 0 ? "application/pdf" :
        "application/vnd.openxmlformats-officedocument.wordprocessingml.document");
    gtk_file_chooser_add_filter(dialog, filter); gtk_file_chooser_set_filter(dialog, filter); g_object_unref(filter);
    gtk_file_chooser_set_current_name(dialog, filename);
    g_signal_connect_data(dialog, "response", (void *)office_export_response, user_data, NULL, 0);
    gtk_native_dialog_show(dialog);
}



int newest_version_first(const void *left, const void *right) {
    const char *a = left, *b = right;
    return strcmp(b, a);
}

void preview_version(Ptr button, Ptr user_data) {
    (void)button; RestoreData *version = user_data;
    if (version->parent) for (Ptr row = gtk_widget_get_first_child(version->parent); row; row = gtk_widget_get_next_sibling(row))
        gtk_widget_remove_css_class(row, "history-current");
    if (version->row) gtk_widget_add_css_class(version->row, "history-current");
    char *content = read_file(version->path); render_markdown(version->preview_buffer, content); free(content);
}

void restore_version(Ptr button, Ptr user_data) {
    (void)button;
    RestoreData *version = user_data;
    char *content = read_file(version->path);
    gtk_text_buffer_set_text(state.editor_buffer, content, -1); update_views(content);
    gtk_label_set_text(state.status, "Versão restaurada — guarde para confirmar");
    free(content);
}

void delete_version(Ptr button, Ptr user_data) {
    (void)button; RestoreData *version = user_data;
    if (unlink(version->path) == 0) {
        gtk_label_set_text(state.status, "Versão removida");
        if (version->parent && version->row) gtk_box_remove(version->parent, version->row);
    } else gtk_label_set_text(state.status, "Não foi possível remover a versão");
}

void show_history(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    if (!state.active || !state.active->id[0]) return;
    Ptr window = gtk_window_new(); gtk_window_set_title(window, tr("Histórico de versões"));
    state.history_window = window;
    gtk_window_set_default_size(window, 1040, 600); gtk_window_set_transient_for(window, state.window);
    gtk_window_set_modal(window, true);
    Ptr box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10); margins(box, 16, 16);
    Ptr heading = gtk_label_new(state.active->title); gtk_label_set_xalign(heading, 0.0f);
    gtk_widget_add_css_class(heading, "history-title"); gtk_box_append(box, heading);
    Ptr subtitle = gtk_label_new(tr("Selecione uma versão para a pré-visualizar. Restaurar não grava automaticamente."));
    gtk_label_set_xalign(subtitle, 0.0f); gtk_widget_add_css_class(subtitle, "history-subtitle");
    gtk_box_append(box, subtitle);
    Ptr body = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL); gtk_paned_set_position(body, 280);
    gtk_widget_set_vexpand(body, true); gtk_widget_add_css_class(body, "history-body");
    Ptr list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6); margins(list, 8, 8);
    state.history_list = list;
    gtk_widget_add_css_class(list, "history-list");
    Ptr list_scroll = gtk_scrolled_window_new(); gtk_scrolled_window_set_child(list_scroll, list);
    gtk_widget_set_hexpand(list_scroll, true); gtk_widget_set_vexpand(list_scroll, true);
    Ptr comparison = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL); gtk_paned_set_position(comparison, 370);
    gtk_widget_set_hexpand(comparison, true); gtk_widget_set_vexpand(comparison, true);
    Ptr saved_column = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6); gtk_widget_add_css_class(saved_column, "history-column");
    Ptr saved_label = gtk_label_new(tr("Versão guardada")); gtk_label_set_xalign(saved_label, 0.0f);
    gtk_widget_add_css_class(saved_label, "history-column-title"); gtk_box_append(saved_column, saved_label);
    Ptr preview_buffer = NULL; Ptr preview = readonly_markdown_view(&preview_buffer, "preview");
    state.history_saved_buffer = preview_buffer;
    gtk_widget_add_css_class(preview, "history-preview"); gtk_box_append(saved_column, editor_scroller(preview));
    Ptr current_column = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6); gtk_widget_add_css_class(current_column, "history-column");
    Ptr current_label = gtk_label_new(tr("Versão atual")); gtk_label_set_xalign(current_label, 0.0f);
    gtk_widget_add_css_class(current_label, "history-column-title"); gtk_box_append(current_column, current_label);
    Ptr current_buffer = NULL; Ptr current = readonly_markdown_view(&current_buffer, "preview");
    state.history_current_buffer = current_buffer;
    gtk_widget_add_css_class(current, "history-preview"); gtk_box_append(current_column, editor_scroller(current));
    char *current_text = editor_text(); render_markdown(current_buffer, current_text); g_free(current_text);
    gtk_paned_set_start_child(comparison, saved_column); gtk_paned_set_end_child(comparison, current_column);
    gtk_paned_set_start_child(body, list_scroll); gtk_paned_set_end_child(body, comparison);
    const char *data_home = getenv("XDG_DATA_HOME"), *home = getenv("HOME"); char directory[PATH_MAX];
    if (data_home && *data_home) snprintf(directory, sizeof directory, "%s/notemd/versions/%s", data_home, state.active->id);
    else snprintf(directory, sizeof directory, "%s/.local/share/notemd/versions/%s", home ? home : ".", state.active->id);
    DIR *versions = opendir(directory); unsigned count = 0;
    if (versions) {
        char names[512][256]; struct dirent *entry;
        while (count < 512 && (entry = readdir(versions))) {
            if (entry->d_name[0] == '.') continue;
            snprintf(names[count++], sizeof names[count], "%s", entry->d_name);
        }
        qsort(names, count, sizeof names[0], newest_version_first);
        for (unsigned index = 0; index < count; index++) {
            RestoreData *preview_data = calloc(1, sizeof *preview_data);
            RestoreData *restore_data = calloc(1, sizeof *restore_data);
            RestoreData *delete_data = calloc(1, sizeof *delete_data);
            snprintf(preview_data->path, sizeof preview_data->path, "%s/%s", directory, names[index]);
            snprintf(restore_data->path, sizeof restore_data->path, "%s/%s", directory, names[index]);
            snprintf(delete_data->path, sizeof delete_data->path, "%s/%s", directory, names[index]);
            preview_data->preview_buffer = preview_buffer;
            long long stamp = atoll(names[index]); time_t when = (time_t)stamp; struct tm local;
            localtime_r(&when, &local); char label[128];
            strftime(label, sizeof label, "%d/%m/%Y  %H:%M:%S", &local);
            Ptr row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 7); gtk_widget_add_css_class(row, "history-row");
            Ptr date = navigation_button("document-open-recent-symbolic", label);
            gtk_widget_add_css_class(date, "history-date"); gtk_widget_set_hexpand(date, true);
            Ptr restore = icon_button("document-revert-symbolic", tr("Restaurar esta versão"));
            Ptr remove = icon_button("user-trash-symbolic", tr("Apagar esta versão"));
            restore_data->row = delete_data->row = row; restore_data->parent = delete_data->parent = list;
            preview_data->row = row; preview_data->parent = list;
            g_signal_connect_data(date, "clicked", (void *)preview_version, preview_data, (void *)free_signal_data, 0);
            g_signal_connect_data(restore, "clicked", (void *)restore_version, restore_data, (void *)free_signal_data, 0);
            g_signal_connect_data(remove, "clicked", (void *)delete_version, delete_data, (void *)free_signal_data, 0);
            gtk_box_append(row, date); gtk_box_append(row, restore); gtk_box_append(row, remove); gtk_box_append(list, row);
            if (index == 0) {
                char *content = read_file(preview_data->path); render_markdown(preview_buffer, content); free(content);
                gtk_widget_add_css_class(row, "history-current");
            }
        }
        closedir(versions);
    }
    if (!count) {
        Ptr empty = gtk_label_new(tr("Ainda não existem versões anteriores."));
        gtk_widget_add_css_class(empty, "empty-message"); margins(empty, 12, 16);
        gtk_box_append(list, empty); render_markdown(preview_buffer, tr("Crie uma versão editando e guardando a nota."));
    }
    gtk_box_append(box, body); gtk_window_set_child(window, box); gtk_window_present(window);
}

int utf8_offset(const char *start, const char *position) {
    int offset = 0;
    for (const unsigned char *p = (const unsigned char *)start; (const char *)p < position; p++)
        if ((*p & 0xc0) != 0x80) offset++;
    return offset;
}

void find_next(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    const char *needle = gtk_editable_get_text(state.find_entry);
    if (!needle[0]) return;
    char *source = editor_text(); size_t length = strlen(source);
    if (state.find_offset > length) state.find_offset = 0;
    char *match = strcasestr(source + state.find_offset, needle);
    if (!match && state.find_offset) { state.find_offset = 0; match = strcasestr(source, needle); }
    if (match) {
        int start_offset = utf8_offset(source, match);
        int end_offset = start_offset + utf8_offset(match, match + strlen(needle));
        TextIter start, end; gtk_text_buffer_get_iter_at_offset(state.editor_buffer, &start, start_offset);
        gtk_text_buffer_get_iter_at_offset(state.editor_buffer, &end, end_offset);
        gtk_text_buffer_select_range(state.editor_buffer, &start, &end);
        gtk_text_view_scroll_to_iter(state.editor, &start, 0.15, false, 0, 0);
        state.find_offset = (size_t)(match - source) + strlen(needle);
        gtk_label_set_text(state.status, tr("Correspondência encontrada"));
    } else gtk_label_set_text(state.status, tr("Sem mais correspondências"));
    g_free(source);
}

void replace_current(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    const char *needle = gtk_editable_get_text(state.find_entry);
    const char *replacement = gtk_editable_get_text(state.replace_entry);
    if (!needle[0]) return;
    TextIter start, end;
    if (!gtk_text_buffer_get_selection_bounds(state.editor_buffer, &start, &end)) { find_next(NULL, NULL); return; }
    char *selected = gtk_text_buffer_get_text(state.editor_buffer, &start, &end, true);
    if (strcasecmp(selected, needle) != 0) { g_free(selected); find_next(NULL, NULL); return; }
    g_free(selected); gtk_text_buffer_delete(state.editor_buffer, &start, &end);
    gtk_text_buffer_insert(state.editor_buffer, &start, replacement, -1);
    state.find_offset = 0; gtk_label_set_text(state.status, tr("Correspondência substituída")); find_next(NULL, NULL);
}

void replace_all(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    const char *needle = gtk_editable_get_text(state.find_entry);
    const char *replacement = gtk_editable_get_text(state.replace_entry);
    if (!needle[0]) return;
    char *source = editor_text(); size_t count = 0;
    for (char *p = source; (p = strcasestr(p, needle)); p += strlen(needle)) count++;
    if (!count) { gtk_label_set_text(state.status, tr("Texto não encontrado")); g_free(source); return; }
    size_t source_length = strlen(source), needle_length = strlen(needle), replacement_length = strlen(replacement);
    size_t result_size = source_length + 1;
    if (replacement_length > needle_length) result_size += count * (replacement_length - needle_length);
    char *result = calloc(result_size, 1), *output = result, *cursor = source, *match;
    while ((match = strcasestr(cursor, needle))) {
        size_t prefix = (size_t)(match - cursor); memcpy(output, cursor, prefix); output += prefix;
        memcpy(output, replacement, replacement_length); output += replacement_length; cursor = match + needle_length;
    }
    strcpy(output, cursor); gtk_text_buffer_set_text(state.editor_buffer, result, -1);
    char message[128]; snprintf(message, sizeof message, "%zu correspondências substituídas", count);
    gtk_label_set_text(state.status, message); free(result); g_free(source); state.find_offset = 0;
}

gboolean find_closed(Ptr window, Ptr unused) {
    (void)window; (void)unused; state.find_window = NULL; return false;
}

void close_find(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    if (state.find_window) { Ptr window = state.find_window; state.find_window = NULL; gtk_window_destroy(window); }
}

void show_find(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    if (state.find_window) { gtk_window_present(state.find_window); return; }
    state.find_window = gtk_window_new(); gtk_window_set_title(state.find_window, tr("Pesquisar e substituir"));
    g_signal_connect_data(state.find_window, "close-request", (void *)find_closed, NULL, NULL, 0);
    gtk_window_set_default_size(state.find_window, 460, 180); gtk_window_set_transient_for(state.find_window, state.window);
    Ptr box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8); margins(box, 16, 16);
    state.find_entry = gtk_entry_new(); gtk_entry_set_placeholder_text(state.find_entry, tr("Pesquisar"));
    state.replace_entry = gtk_entry_new(); gtk_entry_set_placeholder_text(state.replace_entry, tr("Substituir por"));
    Ptr actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    Ptr next = icon_button("go-down-symbolic", tr("Correspondência seguinte"));
    Ptr replace = icon_button("edit-redo-symbolic", tr("Substituir correspondência atual"));
    Ptr all = icon_button("edit-select-all-symbolic", tr("Substituir todas as correspondências"));
    Ptr spacer = gtk_label_new(""); gtk_widget_set_hexpand(spacer, true);
    Ptr close = icon_button("window-close-symbolic", tr("Fechar pesquisa"));
    g_signal_connect_data(next, "clicked", (void *)find_next, NULL, NULL, 0);
    g_signal_connect_data(replace, "clicked", (void *)replace_current, NULL, NULL, 0);
    g_signal_connect_data(all, "clicked", (void *)replace_all, NULL, NULL, 0);
    g_signal_connect_data(close, "clicked", (void *)close_find, NULL, NULL, 0);
    g_signal_connect_data(state.find_entry, "activate", (void *)find_next, NULL, NULL, 0);
    gtk_box_append(actions, next); gtk_box_append(actions, replace); gtk_box_append(actions, all);
    gtk_box_append(actions, spacer); gtk_box_append(actions, close);
    gtk_box_append(box, state.find_entry); gtk_box_append(box, state.replace_entry); gtk_box_append(box, actions);
    gtk_window_set_child(state.find_window, box); gtk_window_present(state.find_window); state.find_offset = 0;
}


#include "preview.h"
#include "markdown-tests.h"

bool export_pdf_content(const char *destination, const char *title, const char *markdown, const char *base) {
    return pdf_export_content(destination, title, markdown, base);
}
unsigned pdf_missing_image_count(void) { return pdf_missing_images; }

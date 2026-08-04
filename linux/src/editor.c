#include "state.h"

void config_path(char output[PATH_MAX]) {
    const char *config = getenv("XDG_CONFIG_HOME"), *home = getenv("HOME");
    if (config && *config) snprintf(output, PATH_MAX, "%s/notemd/settings.conf", config);
    else snprintf(output, PATH_MAX, "%s/.config/notemd/settings.conf", home ? home : ".");
}
void make_uuid(char output[64]) {
    char *value = read_file("/proc/sys/kernel/random/uuid");
    value[strcspn(value, "\r\n")] = '\0';
    if (strlen(value) >= 32) { snprintf(output, 64, "%s", value); free(value); return; }
    free(value);
    unsigned char bytes[16] = {0}; FILE *random = fopen("/dev/urandom", "rb");
    bool generated = random && fread(bytes, 1, sizeof bytes, random) == sizeof bytes;
    if (random) fclose(random);
    if (!generated) {
        struct timespec now; clock_gettime(CLOCK_REALTIME, &now);
        unsigned long seed = (unsigned long)now.tv_nsec ^ (unsigned long)now.tv_sec ^ (unsigned long)getpid();
        for (size_t index = 0; index < sizeof bytes; index++) { seed = seed * 1103515245UL + 12345UL; bytes[index] = (unsigned char)(seed >> 16); }
    }
    bytes[6] = (unsigned char)((bytes[6] & 0x0fU) | 0x40U); bytes[8] = (unsigned char)((bytes[8] & 0x3fU) | 0x80U);
    snprintf(output, 64,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
        bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
}
void safe_title(const char *input, char output[256]) {
    size_t used = 0;
    while (*input && used < 255) {
        char value = *input++; output[used++] = (value == '/' || value == ':') ? '-' : value;
    }
    while (used && output[used - 1] == ' ') used--;
    output[used] = '\0'; if (!used) snprintf(output, 256, "Sem título");
}
void recovery_directory(char output[PATH_MAX]) {
    const char *data = getenv("XDG_DATA_HOME"), *home = getenv("HOME");
    if (data && *data) snprintf(output, PATH_MAX, "%s/notemd/recovery", data);
    else snprintf(output, PATH_MAX, "%s/.local/share/notemd/recovery", home ? home : ".");
}
void recovery_path(Note *note, char output[PATH_MAX]) {
    char directory[PATH_MAX]; recovery_directory(directory); make_directory(directory);
    snprintf(output, PATH_MAX, "%s/%s.draft", directory, note->id);
}
void clear_tag_filter(Ptr button, Ptr unused) {
    (void)button; (void)unused; state.selected_tag[0] = '\0'; rebuild_sidebar();
}
void select_notebook(Ptr button, Ptr user_data) {
    (void)button;
    if (state.active_notebook && state.active_notebook->button)
        gtk_widget_remove_css_class(state.active_notebook->button, "selected-item");
    state.active_notebook = user_data;
    if (state.active_notebook && state.active_notebook->button)
        gtk_widget_add_css_class(state.active_notebook->button, "selected-item");
    char message[320]; snprintf(message, sizeof message, "Notebook selecionado: %s", state.active_notebook->title);
    gtk_label_set_text(state.status, message);
}
void create_note_in_notebook(Ptr button, Ptr user_data) {
    (void)button; state.active_notebook = user_data; new_note(NULL, NULL);
}
void notebook_expanded_changed(Ptr expander, Ptr unused, Ptr user_data) {
    (void)unused; Notebook *book = user_data;
    if (book) book->expanded = gtk_expander_get_expanded(expander);
}
void close_note_now(Note *closing, bool save_changes) {
    if (!closing) return;
    if (closing == state.active) {
        if (state.autosave_source) { g_source_remove(state.autosave_source); state.autosave_source = 0; }
        if (save_changes) save_active(NULL);
        else { closing->dirty = false; discard_note_draft(closing); clear_recovery(closing); }
        closing->opened = false; state.active = NULL;
        for (Note *note = state.notes; note; note = note->next) if (note->opened) { select_note(NULL, note); break; }
        if (!state.active) {
            state.loading = true; gtk_editable_set_text(state.title, ""); gtk_editable_set_text(state.tags, "");
            rebuild_note_tags(); gtk_text_buffer_set_text(state.editor_buffer, "", -1); update_views(""); state.loading = false;
        }
    } else {
        if (save_changes && closing->dirty && closing->draft)
            save_note_payload(closing, closing->draft, closing->draft_title, closing->draft_tags, false);
        else { closing->dirty = false; discard_note_draft(closing); clear_recovery(closing); }
        closing->opened = false;
    }
    rebuild_tabs();
}
void close_note_response(Ptr dialog, int response, Ptr user_data) {
    Note *closing = user_data;
    if (response == 1) close_note_now(closing, true);
    else if (response == 2) close_note_now(closing, false);
    state.close_note_dialog = NULL; gtk_window_destroy(dialog);
}
void close_tab(Ptr button, Ptr user_data) {
    (void)button; Note *closing = user_data; if (!closing) return;
    if (!closing->dirty) { close_note_now(closing, false); return; }
    Ptr dialog = gtk_message_dialog_new(state.window, GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, 0,
        "%s", tr("Guardar alterações?"));
    gtk_message_dialog_format_secondary_text(dialog, "%s", tr("A nota foi alterada desde a última gravação."));
    gtk_dialog_add_button(dialog, tr("Guardar"), 1); gtk_dialog_add_button(dialog, tr("Não guardar"), 2);
    gtk_dialog_add_button(dialog, tr("Cancelar"), 3); state.close_note_dialog = dialog;
    g_signal_connect_data(dialog, "response", (void *)close_note_response, closing, NULL, 0);
    gtk_window_present(dialog);
}
void active_tab_title_changed(Ptr entry, Ptr user_data) {
    Note *note = user_data;
    if (!note || note != state.active || state.loading) return;
    const char *text = gtk_editable_get_text(entry);
    state.loading = true;
    gtk_editable_set_text(state.title, text);
    state.loading = false;
    note->dirty = true;
    if (state.autosave_source) g_source_remove(state.autosave_source);
    state.autosave_source = state.autosave ? g_timeout_add(1200, (void *)save_active, NULL) : 0;
    capture_active_draft();
    if (note->dirty_indicator) gtk_widget_set_visible(note->dirty_indicator, true);
    rebuild_sidebar();
    gtk_label_set_text(state.status, state.autosave ? tr("a guardar…") : tr("alterações por guardar"));
}
void rebuild_tabs(void) {
    if (!state.tabbar) return;
    while (gtk_widget_get_first_child(state.tabbar))
        gtk_box_remove(state.tabbar, gtk_widget_get_first_child(state.tabbar));
    for (Note *note = state.notes; note; note = note->next) note->dirty_indicator = NULL;
    for (Note *note = state.notes; note; note = note->next) if (note->opened) {
        Ptr tab;
        if (note == state.active) {
            tab = gtk_entry_new();
            gtk_editable_set_text(tab, note_display_title(note));
            gtk_widget_add_css_class(tab, "tab");
            gtk_widget_add_css_class(tab, "active-tab");
            gtk_widget_add_css_class(tab, "tab-title");
            gtk_widget_set_size_request(tab, 100, -1);
            g_signal_connect_data(tab, "changed", (void *)active_tab_title_changed, note, NULL, 0);
        } else {
            tab = text_button(note_display_title(note), "tab");
            gtk_widget_set_size_request(tab, 100, -1);
            g_signal_connect_data(tab, "clicked", (void *)select_note, note, NULL, 0);
        }
        gtk_box_append(state.tabbar, tab);
        Ptr dirty = gtk_label_new("●"); gtk_widget_add_css_class(dirty, "dirty-indicator");
        gtk_widget_set_tooltip_text(dirty, tr("Alterações por guardar")); gtk_widget_set_visible(dirty, note->dirty);
        gtk_box_append(state.tabbar, dirty); note->dirty_indicator = dirty;
        Ptr close = icon_button("window-close-symbolic", "Fechar separador"); gtk_widget_add_css_class(close, "tab-close");
        g_signal_connect_data(close, "clicked", (void *)close_tab, note, NULL, 0); gtk_box_append(state.tabbar, close);
    }
}
void search_changed(Ptr entry, Ptr unused) {
    (void)entry; (void)unused; rebuild_sidebar();
}
void use_template(Ptr button, Ptr user_data) {
    (void)button;
    TemplateData *template = user_data;
    new_note(NULL, NULL);
    if (state.active) {
        state.loading = true;
        gtk_editable_set_text(state.title, template->title);
        gtk_text_buffer_set_text(state.editor_buffer, template->markdown, -1);
        update_views(template->markdown); state.loading = false; save_active(NULL);
    }
    gtk_window_destroy(template->window); free(template);
}
void show_templates(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    Ptr window = gtk_window_new(); gtk_window_set_title(window, tr("Nova nota a partir de template"));
    gtk_window_set_default_size(window, 440, 360); gtk_window_set_transient_for(window, state.window);
    gtk_window_set_modal(window, true);
    Ptr box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8); margins(box, 18, 18);
    Ptr heading = gtk_label_new(tr("Escolha um template")); gtk_label_set_xalign(heading, 0.0f);
    gtk_widget_add_css_class(heading, "history-title"); gtk_box_append(box, heading);
    struct { const char *name, *title, *markdown; } templates[] = {
        {"Reunião", "Reunião", "# Reunião\n\n**Data:** \n**Participantes:** \n\n## Agenda\n\n- \n\n## Notas\n\n## Ações\n\n- [ ] \n"},
        {"Diário", "Diário", "# Diário\n\n## Prioridades\n\n- [ ] \n\n## Notas\n\n"},
        {"Projeto", "Projeto", "# Projeto\n\n## Objetivo\n\n## Tarefas\n\n- [ ] \n\n## Referências\n\n"},
        {"Checklist", "Checklist", "# Checklist\n\n- [ ] \n"}
    };
    for (size_t index = 0; index < sizeof templates / sizeof templates[0]; index++) {
        TemplateData *data = calloc(1, sizeof *data); data->window = window;
        data->title = templates[index].title; data->markdown = templates[index].markdown;
        Ptr choice = gtk_button_new_with_label(templates[index].name);
        g_signal_connect_data(choice, "clicked", (void *)use_template, data, NULL, 0); gtk_box_append(box, choice);
    }
    gtk_window_set_child(window, box); gtk_window_present(window);
}
void choose_folder_response(Ptr dialog, int response, Ptr unused) {
    (void)unused;
    if (response == GTK_RESPONSE_ACCEPT) {
        Ptr file = gtk_file_chooser_get_file(dialog);
        char *path = file ? g_file_get_path(file) : NULL;
        if (path) {
            if (state.autosave_source) { g_source_remove(state.autosave_source); save_active(NULL); }
            free_notes(); snprintf(state.root, sizeof state.root, "%s", path); state.active_notebook = NULL;
            save_settings();
            load_library(); rebuild_sidebar();
            gtk_label_set_text(state.status, state.notes ? "Biblioteca carregada" : "Pasta vazia — crie um notebook");
            if (state.notes) select_note(NULL, state.notes);
            g_free(path);
        }
    }
    g_object_unref(dialog);
}
void choose_folder(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    Ptr dialog = gtk_file_chooser_native_new("Escolher pasta das notas", state.window,
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, "Escolher", "Cancelar");
    g_signal_connect_data(dialog, "response", (void *)choose_folder_response, NULL, NULL, 0);
    gtk_native_dialog_show(dialog);
}
void open_markdown_response(Ptr dialog, int response, Ptr unused) {
    (void)unused;
    if (response == GTK_RESPONSE_ACCEPT) {
        Ptr file = gtk_file_chooser_get_file(dialog);
        char *path = file ? g_file_get_path(file) : NULL;
        if (path) {
            Note *note = add_external_note(path);
            if (note) { rebuild_sidebar(); select_note(NULL, note); save_settings(); }
            g_free(path);
        }
    }
    g_object_unref(dialog);
}
void open_markdown(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    Ptr dialog = gtk_file_chooser_native_new(tr("Abrir ficheiro Markdown"), state.window,
        GTK_FILE_CHOOSER_ACTION_OPEN, tr("Abrir"), tr("Cancelar"));
    g_signal_connect_data(dialog, "response", (void *)open_markdown_response, NULL, NULL, 0);
    gtk_native_dialog_show(dialog);
}
bool copy_file(const char *source, const char *destination) {
    FILE *input = fopen(source, "rb"); if (!input) return false;
    FILE *output = fopen(destination, "wb"); if (!output) { fclose(input); return false; }
    char buffer[65536]; size_t amount; bool ok = true;
    while ((amount = fread(buffer, 1, sizeof buffer, input)) > 0)
        if (fwrite(buffer, 1, amount, output) != amount) { ok = false; break; }
    if (ferror(input)) ok = false;
    if (fclose(output) != 0) ok = false;
    fclose(input);
    if (!ok) unlink(destination);
    return ok;
}
bool store_image(const char *source) {
    if (!state.active || !source) return false;
    char assets[PATH_MAX]; snprintf(assets, sizeof assets, "%s/assets", state.active->directory);
    if (!make_directory(assets)) return false;
    const char *extension = strrchr(source, '.');
    if (!extension || strlen(extension) > 8) extension = ".png";
    char id[64]; make_uuid(id); char filename[96]; snprintf(filename, sizeof filename, "image-%s%s", id, extension);
    char destination[PATH_MAX]; snprintf(destination, sizeof destination, "%s/%s", assets, filename);
    if (!copy_file(source, destination)) return false;
    char markdown[168]; snprintf(markdown, sizeof markdown, "\n![Imagem](assets/%s)\n", filename);
    gtk_text_buffer_insert_at_cursor(state.editor_buffer, markdown, -1);
    rebuild_block_editor();
    gtk_label_set_text(state.status, tr("Imagem copiada para assets")); return true;
}
void clipboard_texture_ready(Ptr clipboard, Ptr result, Ptr user_data) {
    ClipboardImageRequest *request = user_data; Ptr error = NULL;
    Ptr texture = gdk_clipboard_read_texture_finish(clipboard, result, &error);
    if (texture && request && request->note == state.active) {
        char id[64], temporary[PATH_MAX]; make_uuid(id);
        snprintf(temporary, sizeof temporary, "/tmp/notemd-clipboard-%s.png", id);
        if (gdk_texture_save_to_png(texture, temporary)) {
            if (store_image(temporary)) gtk_label_set_text(state.status, tr("Imagem colada e guardada em assets"));
            else gtk_label_set_text(state.status, "Não foi possível guardar a imagem colada");
            unlink(temporary);
        } else gtk_label_set_text(state.status, "Não foi possível ler a imagem copiada");
    }
    if (texture) g_object_unref(texture);
    if (error) g_error_free(error);
    free(request);
}
gboolean paste_image_key(Ptr controller, guint keyval, guint keycode, guint modifiers, Ptr unused) {
    (void)controller; (void)keycode; (void)unused;
    if (!(modifiers & GDK_CONTROL_MASK) || (keyval != 'v' && keyval != 'V') || !state.active) return false;
    Ptr clipboard = gdk_display_get_clipboard(gdk_display_get_default());
    Ptr formats = clipboard ? gdk_clipboard_get_formats(clipboard) : NULL;
    if (!formats || !gdk_content_formats_contain_gtype(formats, gdk_texture_get_type())) return false;
    ClipboardImageRequest *request = calloc(1, sizeof *request); request->note = state.active;
    gdk_clipboard_read_texture_async(clipboard, NULL, (void *)clipboard_texture_ready, request);
    return true;
}
void image_response(Ptr dialog, int response, Ptr unused) {
    (void)unused;
    if (response == GTK_RESPONSE_ACCEPT && state.active) {
        Ptr file = gtk_file_chooser_get_file(dialog); char *source = file ? g_file_get_path(file) : NULL;
        if (source) {
            if (!store_image(source)) gtk_label_set_text(state.status, "Erro ao copiar imagem");
            g_free(source);
        }
    }
    g_object_unref(dialog);
}
void insert_image(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    if (!state.active) return;
    Ptr dialog = gtk_file_chooser_native_new(tr("Inserir imagem"), state.window,
        GTK_FILE_CHOOSER_ACTION_OPEN, tr("Inserir"), tr("Cancelar"));
    g_signal_connect_data(dialog, "response", (void *)image_response, NULL, NULL, 0);
    gtk_native_dialog_show(dialog);
}
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
void html_inline(StringBuilder *output, const char *source) {
    const char *cursor = source;
    while (*cursor) {
        const char *close = NULL;
        if (strncmp(cursor, "<span style=\"color:#", 20) == 0) {
            const char *content = strchr(cursor + 20, '>'), *end = content ? strstr(content + 1, "</span>") : NULL;
            if (content && end && content - cursor == 27) {
                char color[8]; memcpy(color, cursor + 19, 7); color[7] = '\0';
                const char *allowed[] = {"#d1242f", "#bc4c00", "#9a6700", "#1a7f37", "#0969da", "#8250df"}; bool valid = false;
                for (size_t index = 0; index < sizeof allowed / sizeof allowed[0]; index++) if (strcasecmp(color, allowed[index]) == 0) valid = true;
                if (valid) {
                    char *inner = strndup(content + 1, (size_t)(end - content - 1)); builder_printf(output, "<span style=\"color:%s\">", color);
                    html_inline(output, inner); builder_append(output, "</span>"); free(inner); cursor = end + 7; continue;
                }
            }
        }
        if (strncmp(cursor, "**", 2) == 0 && (close = strstr(cursor + 2, "**"))) {
            builder_append(output, "<strong>"); builder_escape_n(output, cursor + 2, (size_t)(close - cursor - 2));
            builder_append(output, "</strong>"); cursor = close + 2; continue;
        }
        if (strncmp(cursor, "~~", 2) == 0 && (close = strstr(cursor + 2, "~~"))) {
            builder_append(output, "<s>"); builder_escape_n(output, cursor + 2, (size_t)(close - cursor - 2));
            builder_append(output, "</s>"); cursor = close + 2; continue;
        }
        if (*cursor == '_' && (close = strchr(cursor + 1, '_'))) {
            builder_append(output, "<em>"); builder_escape_n(output, cursor + 1, (size_t)(close - cursor - 1));
            builder_append(output, "</em>"); cursor = close + 1; continue;
        }
        if (*cursor == '`' && (close = strchr(cursor + 1, '`'))) {
            builder_append(output, "<code>"); builder_escape_n(output, cursor + 1, (size_t)(close - cursor - 1));
            builder_append(output, "</code>"); cursor = close + 1; continue;
        }
        if (*cursor == '[' && cursor[1] != '[' && (close = strstr(cursor + 1, "]("))) {
            const char *end = strchr(close + 2, ')');
            if (end) {
                builder_append(output, "<a href=\""); builder_escape_n(output, close + 2, (size_t)(end - close - 2));
                builder_append(output, "\">"); builder_escape_n(output, cursor + 1, (size_t)(close - cursor - 1));
                builder_append(output, "</a>"); cursor = end + 1; continue;
            }
        }
        if (strncmp(cursor, "[[", 2) == 0 && (close = strstr(cursor + 2, "]]"))) {
            builder_append(output, "<span class=\"wikilink\">");
            builder_escape_n(output, cursor + 2, (size_t)(close - cursor - 2));
            builder_append(output, "</span>"); cursor = close + 2; continue;
        }
        builder_escape_n(output, cursor, 1); cursor++;
    }
}
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
void html_table_row(StringBuilder *output, const char *line, const char *cell_tag) {
    char *copy = strdup(line), *value = copy;
    while (*value == ' ' || *value == '|') value++;
    size_t length = strlen(value); while (length && (value[length - 1] == ' ' || value[length - 1] == '|')) value[--length] = '\0';
    builder_append(output, "<tr>"); char *save = NULL;
    for (char *cell = strtok_r(value, "|", &save); cell; cell = strtok_r(NULL, "|", &save)) {
        while (*cell == ' ') cell++;
        size_t cell_length = strlen(cell);
        while (cell_length && cell[cell_length - 1] == ' ') cell[--cell_length] = '\0';
        builder_printf(output, "<%s>", cell_tag); html_inline(output, cell); builder_printf(output, "</%s>", cell_tag);
    }
    builder_append(output, "</tr>"); free(copy);
}
char *markdown_to_html(const char *markdown) {
    char *copy = strdup(markdown); size_t markdown_length = strlen(copy), line_count = 1;
    for (size_t position = 0; position < markdown_length; position++) if (copy[position] == '\n') line_count++;
    char **lines = calloc(line_count + 1, sizeof *lines); size_t count = 0; lines[count++] = copy;
    for (size_t position = 0; position < markdown_length; position++) if (copy[position] == '\n') {
        copy[position] = '\0'; lines[count++] = copy + position + 1;
    }
    StringBuilder output = {0}; bool paragraph = false, code = false;
    for (size_t index = 0; index < count; index++) {
        const char *line = lines[index];
        if (strncmp(line, "```", 3) == 0) {
            if (paragraph) { builder_append(&output, "</p>\n"); paragraph = false; }
            builder_append(&output, code ? "</code></pre>\n" : "<pre><code>"); code = !code; continue;
        }
        if (code) { builder_escape_n(&output, line, strlen(line)); builder_append(&output, "\n"); continue; }
        if (!*line) { if (paragraph) { builder_append(&output, "</p>\n"); paragraph = false; } continue; }
        if (strchr(line, '|') && index + 1 < count && strstr(lines[index + 1], "---")) {
            if (paragraph) { builder_append(&output, "</p>\n"); paragraph = false; }
            builder_append(&output, "<table><thead>"); html_table_row(&output, line, "th"); builder_append(&output, "</thead><tbody>"); index += 2;
            while (index < count && strchr(lines[index], '|') && *lines[index]) { html_table_row(&output, lines[index], "td"); index++; }
            builder_append(&output, "</tbody></table>\n"); index--; continue;
        }
        int level; const char *heading;
        const char *alt, *image_source; size_t alt_length, source_length;
        if (markdown_heading(line, &level, &heading)) {
            if (paragraph) { builder_append(&output, "</p>\n"); paragraph = false; }
            builder_printf(&output, "<h%d>", level); html_inline(&output, heading); builder_printf(&output, "</h%d>\n", level);
        } else if (strcmp(line, "---") == 0 || strcmp(line, "***") == 0) {
            if (paragraph) { builder_append(&output, "</p>\n"); paragraph = false; } builder_append(&output, "<hr>\n");
        } else if (markdown_image(line, &alt, &alt_length, &image_source, &source_length)) {
            if (paragraph) { builder_append(&output, "</p>\n"); paragraph = false; }
            builder_append(&output, "<img src=\""); builder_escape_n(&output, image_source, source_length);
            builder_append(&output, "\" alt=\""); builder_escape_n(&output, alt, alt_length); builder_append(&output, "\">\n");
        } else if (strncmp(line, "- [ ] ", 6) == 0 || strncmp(line, "* [ ] ", 6) == 0) {
            if (paragraph) { builder_append(&output, "</p>\n"); paragraph = false; }
            builder_append(&output, "<p class=\"task\">☐ "); html_inline(&output, line + 6); builder_append(&output, "</p>\n");
        } else if (strncmp(line, "- [x] ", 6) == 0 || strncmp(line, "- [X] ", 6) == 0) {
            if (paragraph) { builder_append(&output, "</p>\n"); paragraph = false; }
            builder_append(&output, "<p class=\"task done\">☑ <s>"); html_inline(&output, line + 6); builder_append(&output, "</s></p>\n");
        } else if (strncmp(line, "- ", 2) == 0 || strncmp(line, "* ", 2) == 0) {
            if (paragraph) { builder_append(&output, "</p>\n"); paragraph = false; }
            builder_append(&output, "<ul><li>"); html_inline(&output, line + 2); builder_append(&output, "</li></ul>\n");
        } else if (line[0] >= '0' && line[0] <= '9' && strchr(line, '.') && strchr(line, '.')[1] == ' ') {
            const char *dot = strchr(line, '.');
            if (paragraph) { builder_append(&output, "</p>\n"); paragraph = false; }
            builder_append(&output, "<ol><li>"); html_inline(&output, dot + 2); builder_append(&output, "</li></ol>\n");
        } else if (strncmp(line, "> ", 2) == 0) {
            if (paragraph) { builder_append(&output, "</p>\n"); paragraph = false; }
            builder_append(&output, "<blockquote>"); html_inline(&output, line + 2); builder_append(&output, "</blockquote>\n");
        } else {
            if (!paragraph) { builder_append(&output, "<p>"); paragraph = true; } else builder_append(&output, " ");
            html_inline(&output, line);
        }
    }
    if (paragraph) builder_append(&output, "</p>\n");
    if (code) builder_append(&output, "</code></pre>\n");
    free(lines); free(copy); if (!output.value) output.value = strdup(""); return output.value;
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
void markdown_plain_inline(const char *source, char *output, size_t size) {
    size_t used = 0; const char *cursor = source;
    while (*cursor && used + 1 < size) {
        if (strncmp(cursor, "**", 2) == 0) { cursor += 2; continue; }
        if (*cursor == '_' || *cursor == '`') { cursor++; continue; }
        if (strncmp(cursor, "[[", 2) == 0) { cursor += 2; continue; }
        if (strncmp(cursor, "]]", 2) == 0) { cursor += 2; continue; }
        if (*cursor == '[') {
            const char *middle = strstr(cursor + 1, "]("), *end = middle ? strchr(middle + 2, ')') : NULL;
            if (middle && end) {
                size_t length = (size_t)(middle - cursor - 1); if (length > size - used - 1) length = size - used - 1;
                memcpy(output + used, cursor + 1, length); used += length; cursor = end + 1; continue;
            }
        }
        output[used++] = *cursor++;
    }
    output[used] = '\0';
}
void pdf_line(Ptr context, double *y, const char *text, double size, int weight, bool monospace, double indent) {
    const size_t maximum = monospace ? 72 : 82; const char *cursor = text;
    if (!*cursor) { *y += size + 5; return; }
    while (*cursor) {
        size_t remaining = strlen(cursor), length = remaining > maximum ? maximum : remaining;
        if (length < remaining) { size_t split = length; while (split > maximum / 2 && cursor[split] != ' ') split--; if (split > maximum / 2) length = split; }
        char line[4096]; if (length >= sizeof line) length = sizeof line - 1;
        memcpy(line, cursor, length); line[length] = '\0';
        if (*y > 790) { cairo_show_page(context); *y = 54; }
        cairo_select_font_face(context, monospace ? "Monospace" : "Sans", 0, weight); cairo_set_font_size(context, size);
        cairo_move_to(context, 54 + indent, *y); cairo_show_text(context, line); *y += size + 6;
        cursor += length; while (*cursor == ' ') cursor++;
    }
}
bool pdf_export_content(const char *destination, const char *title, const char *markdown) {
    Ptr surface = cairo_pdf_surface_create(destination, 595.0, 842.0); if (!surface) return false;
    Ptr context = cairo_create(surface); cairo_select_font_face(context, "Sans", 0, 1);
    cairo_set_font_size(context, 22); cairo_move_to(context, 54, 58); cairo_show_text(context, title);
    char *copy = strdup(markdown), *save = NULL; double y = 92; bool code = false;
    for (char *line = strtok_r(copy, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        double size = 11; int weight = 0; double indent = 0; const char *text = line; char plain[4096], decorated[4096];
        if (strncmp(line, "```", 3) == 0) { code = !code; y += 4; continue; }
        if (strncmp(line, "### ", 4) == 0) { size = 14; weight = 1; text += 4; }
        else if (strncmp(line, "## ", 3) == 0) { size = 17; weight = 1; text += 3; }
        else if (strncmp(line, "# ", 2) == 0) { size = 20; weight = 1; text += 2; }
        else if (strncmp(line, "- [ ] ", 6) == 0 || strncmp(line, "* [ ] ", 6) == 0) { snprintf(decorated, sizeof decorated, "☐  %s", line + 6); text = decorated; indent = 12; }
        else if (strncmp(line, "- [x] ", 6) == 0 || strncmp(line, "- [X] ", 6) == 0) { snprintf(decorated, sizeof decorated, "☑  %s", line + 6); text = decorated; indent = 12; }
        else if (strncmp(line, "- ", 2) == 0 || strncmp(line, "* ", 2) == 0) { snprintf(decorated, sizeof decorated, "•  %s", line + 2); text = decorated; indent = 12; }
        else if (strncmp(line, "> ", 2) == 0) { snprintf(decorated, sizeof decorated, "│  %s", line + 2); text = decorated; indent = 14; }
        else if (strncmp(line, "![", 2) == 0) {
            const char *middle = strstr(line + 2, "](");
            if (middle) snprintf(decorated, sizeof decorated, "[Imagem: %.*s]", (int)(middle - line - 2), line + 2);
            else snprintf(decorated, sizeof decorated, "[Imagem]");
            text = decorated; indent = 12;
        } else if (strcmp(line, "---") == 0 || strcmp(line, "***") == 0) text = "────────────────────────────────────────────────";
        markdown_plain_inline(text, plain, sizeof plain);
        if (line[0] == '|') for (char *cell = plain; *cell; cell++) if (*cell == '|') *cell = ' ';
        pdf_line(context, &y, plain, size, weight, code, indent);
    }
    free(copy); cairo_destroy(context); cairo_surface_finish(surface);
    bool ok = cairo_surface_status(surface) == 0; cairo_surface_destroy(surface); return ok;
}
bool pdf_export(const char *destination) {
    char *markdown = editor_text(); bool ok = pdf_export_content(destination, state.active->title, markdown);
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
            gtk_label_set_text(state.status, ok ? (strcmp(format, "pdf") == 0 ? "PDF exportado" : "DOCX exportado")
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
    char filename[300]; snprintf(filename, sizeof filename, "%s.%s", state.active->title, format);
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
const char *block_kind(const char *text) {
    if (text[0] == '#') return tr("Título");
    if (strncmp(text, "- [", 3) == 0) return tr("Tarefa");
    if (strncmp(text, "- ", 2) == 0 || strncmp(text, "* ", 2) == 0) return tr("Lista");
    if (isdigit((unsigned char)text[0])) { const char *dot = strchr(text, '.'); if (dot && dot[1] == ' ') return tr("Lista numerada"); }
    if (strncmp(text, "> ", 2) == 0) return tr("Citação");
    if (strncmp(text, "```", 3) == 0) return tr("Código");
    if (text[0] == '|') return tr("Tabela");
    if (strncmp(text, "---", 3) == 0) return tr("Separador");
    return tr("Parágrafo");
}
bool visual_special_line(const char *line) {
    if (!*line) return true;
    if (*line == '#' || *line == '|' || strncmp(line, "```", 3) == 0 || strncmp(line, "> ", 2) == 0 ||
        strncmp(line, "- ", 2) == 0 || strncmp(line, "* ", 2) == 0 || strncmp(line, "![", 2) == 0 ||
        strcmp(line, "---") == 0 || strcmp(line, "***") == 0) return true;
    if (*line >= '0' && *line <= '9') { const char *dot = strchr(line, '.'); if (dot && dot[1] == ' ') return true; }
    return false;
}
size_t visual_block_length(const char *cursor) {
    const char *line_end = strchr(cursor, '\n'); if (!line_end) return strlen(cursor);
    size_t first_length = (size_t)(line_end - cursor); char first[4096];
    if (first_length >= sizeof first) first_length = sizeof first - 1;
    memcpy(first, cursor, first_length); first[first_length] = '\0';
    if (strncmp(first, "```", 3) == 0) {
        const char *scan = line_end + 1;
        while (*scan) {
            const char *end = strchr(scan, '\n'); if (!end) end = scan + strlen(scan);
            if (end - scan >= 3 && strncmp(scan, "```", 3) == 0) return (size_t)(end - cursor);
            if (!*end) return strlen(cursor);
            scan = end + 1;
        }
    }
    if (first[0] == '|') {
        const char *scan = line_end + 1, *end = strchr(scan, '\n'); if (!end) end = scan + strlen(scan);
        if (strstr(scan, "---") && strchr(scan, '|')) {
            const char *last = end;
            while (*last) { const char *next = last + 1; if (*next != '|') break; last = strchr(next, '\n'); if (!last) return strlen(cursor); }
            return (size_t)(last - cursor);
        }
    }
    if (visual_special_line(first)) {
        if (strncmp(first, "> ", 2) == 0) {
            const char *end = line_end;
            while (*end && strncmp(end + 1, "> ", 2) == 0) { end = strchr(end + 1, '\n'); if (!end) return strlen(cursor); }
            return (size_t)(end - cursor);
        }
        return first_length;
    }
    const char *end = line_end;
    while (*end) {
        const char *next = end + 1, *next_end = strchr(next, '\n'); if (!next_end) next_end = next + strlen(next);
        size_t length = (size_t)(next_end - next); char line[4096]; if (length >= sizeof line) length = sizeof line - 1;
        memcpy(line, next, length); line[length] = '\0'; if (!*line || visual_special_line(line)) break;
        end = next_end;
    }
    return (size_t)(end - cursor);
}
char *visual_block_markdown(size_t index) {
    char *content = buffer_text(state.block_buffers[index]);
    if (state.block_is_heading[index]) {
        guint level = state.block_headings[index] ? gtk_drop_down_get_selected(state.block_headings[index]) + 1 : 1;
        size_t size = level + strlen(content) + 2; char *markdown = calloc(size, 1);
        memset(markdown, '#', level); markdown[level] = ' '; strcpy(markdown + level + 1, content); g_free(content); return markdown;
    }
    if (state.block_is_task[index]) {
        bool checked = state.block_checks[index] && gtk_check_button_get_active(state.block_checks[index]);
        size_t size = strlen(content) + 7; char *markdown = calloc(size, 1);
        snprintf(markdown, size, "- [%c] %s", checked ? 'x' : ' ', content); g_free(content); return markdown;
    }
    if (state.block_is_code[index]) {
        const char *language = state.block_languages[index] ? gtk_editable_get_text(state.block_languages[index]) : "";
        size_t size = strlen(language) + strlen(content) + 9; char *markdown = calloc(size, 1);
        snprintf(markdown, size, "```%s\n%s\n```", language, content); g_free(content); return markdown;
    }
    if (state.block_is_image[index]) {
        const char *alt = state.block_image_alt[index] ? gtk_editable_get_text(state.block_image_alt[index]) : "";
        const char *location = state.block_image_location[index] ? gtk_editable_get_text(state.block_image_location[index]) : "";
        size_t size = strlen(alt) + strlen(location) + 7; char *markdown = calloc(size, 1);
        snprintf(markdown, size, "![%s](%s)", alt, location); g_free(content); return markdown;
    }
    if (state.block_is_divider[index]) { g_free(content); return strdup("---"); }
    if (state.block_is_bullet[index] || state.block_is_numbered[index]) {
        char prefix[32]; snprintf(prefix, sizeof prefix, state.block_is_bullet[index] ? "- " : "%d. ",
            state.block_numbers[index] > 0 ? state.block_numbers[index] : 1);
        size_t size = strlen(prefix) + strlen(content) + 1; char *markdown = calloc(size, 1);
        snprintf(markdown, size, "%s%s", prefix, content); g_free(content); return markdown;
    }
    if (state.block_is_quote[index]) {
        StringBuilder markdown = {0}; const char *line = content;
        while (true) {
            const char *end = strchr(line, '\n'); if (!end) end = line + strlen(line);
            if (markdown.length) builder_append(&markdown, "\n");
            builder_append(&markdown, "> ");
            builder_append_n(&markdown, line, (size_t)(end - line)); if (!*end) break; line = end + 1;
        }
        g_free(content); return markdown.value ? markdown.value : strdup("> ");
    }
    char *copy = strdup(content); g_free(content); return copy;
}
void task_toggled(Ptr check, Ptr user_data) {
    (void)check; size_t index = (size_t)user_data; if (index >= state.block_count) return;
    blocks_changed(state.block_buffers[index], NULL);
}
void heading_level_changed(Ptr dropdown, Ptr unused, Ptr user_data) {
    (void)dropdown; (void)unused; size_t index = (size_t)user_data; if (index >= state.block_count) return;
    blocks_changed(state.block_buffers[index], NULL);
}
void code_language_changed(Ptr entry, Ptr user_data) {
    (void)entry; size_t index = (size_t)user_data; if (index >= state.block_count) return;
    blocks_changed(state.block_buffers[index], NULL);
}
void image_field_changed(Ptr entry, Ptr user_data) {
    (void)entry; size_t index = (size_t)user_data; if (index >= state.block_count) return;
    blocks_changed(state.block_buffers[index], NULL);
}
gboolean refresh_visual_editor(Ptr unused) {
    (void)unused; rebuild_block_editor(); return false;
}
void table_action(Ptr button, Ptr user_data) {
    (void)button; size_t encoded = (size_t)user_data, index = encoded / 2; bool add_column = (encoded % 2) != 0;
    if (index >= state.block_count) return;
    char *table = buffer_text(state.block_buffers[index]); StringBuilder result = {0};
    if (!add_column) {
        builder_append(&result, table); size_t columns = 0;
        const char *first_end = strchr(table, '\n'); if (!first_end) first_end = table + strlen(table);
        for (const char *p = table; p < first_end; p++) if (*p == '|') columns++;
        if (columns > 1) columns--;
        builder_append(&result, "\n|"); for (size_t column = 0; column < columns; column++) builder_append(&result, "  |");
    } else {
        char *copy = strdup(table), *save = NULL; bool first = true;
        for (char *line = strtok_r(copy, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
            if (!first) builder_append(&result, "\n");
            first = false;
            size_t length = strlen(line); while (length && line[length - 1] == ' ') length--;
            bool trailing_pipe = length && line[length - 1] == '|';
            if (trailing_pipe) builder_append_n(&result, line, length - 1); else builder_append_n(&result, line, length);
            builder_append(&result, strstr(line, "---") ? " --- |" : "  |");
        }
        free(copy);
    }
    gtk_text_buffer_set_text(state.block_buffers[index], result.value ? result.value : table, -1);
    g_timeout_add(1, (void *)refresh_visual_editor, NULL);
    free(result.value); g_free(table);
}
void append_table_cell(StringBuilder *builder, const char *value) {
    builder_append(builder, " ");
    for (const char *cursor = value ? value : ""; *cursor; cursor++) {
        if (*cursor == '|') builder_append(builder, "\\|");
        else builder_append_n(builder, cursor, 1);
    }
    builder_append(builder, " |");
}
void table_cell_changed(Ptr entry, Ptr user_data) {
    (void)entry; TableEditor *table = user_data;
    if (!table || table->syncing || table->block_index >= state.block_count) return;
    StringBuilder markdown = {0};
    for (size_t row = 0; row < table->rows; row++) {
        if (row) builder_append(&markdown, "\n");
        builder_append(&markdown, "|");
        for (size_t column = 0; column < table->columns; column++)
            append_table_cell(&markdown, gtk_editable_get_text(table->cells[row][column]));
        if (row == 0) {
            builder_append(&markdown, "\n|");
            for (size_t column = 0; column < table->columns; column++) builder_append(&markdown, " --- |");
        }
    }
    table->syncing = true;
    gtk_text_buffer_set_text(state.block_buffers[table->block_index], markdown.value ? markdown.value : "", -1);
    table->syncing = false; free(markdown.value);
}
size_t table_column_count(const char *line) {
    size_t pipes = 0; for (const char *cursor = line; *cursor; cursor++) if (*cursor == '|' && (cursor == line || cursor[-1] != '\\')) pipes++;
    return pipes > 1 ? pipes - 1 : 1;
}
void table_cell_text(const char *line, size_t column, char output[512]) {
    const char *cursor = line; if (*cursor == '|') cursor++;
    for (size_t current = 0; current < column; current++) {
        const char *pipe = strchr(cursor, '|'); if (!pipe) { output[0] = '\0'; return; } cursor = pipe + 1;
    }
    const char *end = cursor; while (*end && *end != '|') end++;
    while (cursor < end && isspace((unsigned char)*cursor)) cursor++;
    while (end > cursor && isspace((unsigned char)end[-1])) end--;
    size_t length = (size_t)(end - cursor); if (length >= 512) length = 511;
    memcpy(output, cursor, length); output[length] = '\0';
}
Ptr table_visual_editor(const char *markdown, size_t block_index) {
    TableEditor *table = calloc(1, sizeof *table); table->block_index = block_index;
    char *copy = strdup(markdown), *save = NULL; char *lines[33] = {0}; size_t line_count = 0;
    for (char *line = strtok_r(copy, "\n", &save); line && line_count < 33; line = strtok_r(NULL, "\n", &save))
        lines[line_count++] = line;
    table->columns = line_count ? table_column_count(lines[0]) : 2;
    if (table->columns > 16) table->columns = 16;
    Ptr grid = gtk_grid_new(); gtk_grid_set_row_spacing(grid, 5); gtk_grid_set_column_spacing(grid, 5);
    gtk_widget_add_css_class(grid, "table-grid");
    for (size_t source_row = 0; source_row < line_count && table->rows < 32; source_row++) {
        if (source_row == 1 && strstr(lines[source_row], "---")) continue;
        size_t row = table->rows++;
        for (size_t column = 0; column < table->columns; column++) {
            char value[512]; table_cell_text(lines[source_row], column, value);
            Ptr cell = gtk_entry_new(); gtk_editable_set_text(cell, value); gtk_entry_set_placeholder_text(cell, tr("Conteúdo"));
            gtk_widget_set_hexpand(cell, true); gtk_widget_add_css_class(cell, row == 0 ? "table-header-cell" : "table-cell");
            table->cells[row][column] = cell; gtk_grid_attach(grid, cell, (int)column, (int)row, 1, 1);
            g_signal_connect_data(cell, "changed", (void *)table_cell_changed, table, NULL, 0);
        }
    }
    if (!table->rows) table->rows = 1;
    state.block_tables[block_index] = table; free(copy); return grid;
}
Ptr visual_image_widget(const char *block) {
    const char *alt, *location; size_t alt_length, location_length;
    if (!state.active || !markdown_image(block, &alt, &alt_length, &location, &location_length)) return NULL;
    char relative[PATH_MAX], path[PATH_MAX]; snprintf(relative, sizeof relative, "%.*s", (int)location_length, location);
    if (relative[0] == '/') snprintf(path, sizeof path, "%s", relative);
    else snprintf(path, sizeof path, "%s/%s", state.active->directory, relative);
    if (access(path, R_OK) != 0) return NULL;
    Ptr picture = gtk_picture_new_for_filename(path); gtk_picture_set_content_fit(picture, 3);
    gtk_widget_set_size_request(picture, -1, 260); gtk_widget_add_css_class(picture, "visual-image"); return picture;
}
void blocks_changed(Ptr buffer, Ptr unused) {
    (void)buffer; (void)unused; size_t size = 1;
    for (size_t index = 0; index < state.block_count; index++) {
        char *text = visual_block_markdown(index); size += strlen(text) + 2; free(text);
    }
    char *markdown = calloc(size, 1); size_t used = 0;
    for (size_t index = 0; index < state.block_count; index++) {
        char *text = visual_block_markdown(index);
        used += (size_t)snprintf(markdown + used, size - used, "%s%s", index ? "\n\n" : "", text); free(text);
    }
    state.block_syncing = true;
    gtk_text_buffer_set_text(state.editor_buffer, markdown, -1);
    state.block_syncing = false;
    free(markdown);
}
void block_action(Ptr button, Ptr user_data) {
    (void)button;
    size_t encoded = (size_t)user_data, target = encoded / 2;
    bool duplicate = (encoded % 2) == 0;
    if (target >= state.block_count) return;
    size_t size = 1;
    for (size_t index = 0; index < state.block_count; index++) {
        char *text = visual_block_markdown(index);
        size += strlen(text) * (duplicate && index == target ? 2 : 1) + 4; free(text);
    }
    char *markdown = calloc(size, 1); size_t used = 0, written = 0;
    for (size_t index = 0; index < state.block_count; index++) {
        if (!duplicate && index == target) continue;
        char *text = visual_block_markdown(index);
        used += (size_t)snprintf(markdown + used, size - used, "%s%s", written++ ? "\n\n" : "", text);
        if (duplicate && index == target)
            used += (size_t)snprintf(markdown + used, size - used, "\n\n%s", text);
        free(text);
    }
    gtk_text_buffer_set_text(state.editor_buffer, markdown, -1); free(markdown);
}
char *quote_visual_text(const char *markdown) {
    StringBuilder content = {0}; const char *line = markdown;
    while (true) {
        const char *end = strchr(line, '\n'); if (!end) end = line + strlen(line);
        const char *start = strncmp(line, "> ", 2) == 0 ? line + 2 : line;
        if (content.length) builder_append(&content, "\n");
        builder_append_n(&content, start, (size_t)(end - start));
        if (!*end) break;
        line = end + 1;
    }
    return content.value ? content.value : strdup("");
}
char *code_visual_text(const char *markdown, char language[64]) {
    language[0] = '\0'; const char *first_end = strchr(markdown, '\n');
    if (!first_end) return strdup("");
    const char *language_start = markdown + 3; while (language_start < first_end && isspace((unsigned char)*language_start)) language_start++;
    const char *language_end = first_end; while (language_end > language_start && isspace((unsigned char)language_end[-1])) language_end--;
    snprintf(language, 64, "%.*s", (int)(language_end - language_start), language_start);
    const char *content = first_end + 1, *closing = strstr(content, "\n```");
    if (!closing) closing = content + strlen(content);
    size_t length = (size_t)(closing - content); char *result = calloc(length + 1, 1);
    memcpy(result, content, length); return result;
}
void rebuild_block_editor(void) {
    if (!state.block_box) return;
    while (gtk_widget_get_first_child(state.block_box))
        gtk_box_remove(state.block_box, gtk_widget_get_first_child(state.block_box));
    for (size_t index = 0; index < 256; index++) { free(state.block_tables[index]); state.block_tables[index] = NULL; }
    memset(state.block_checks, 0, sizeof state.block_checks);
    memset(state.block_headings, 0, sizeof state.block_headings);
    memset(state.block_is_heading, 0, sizeof state.block_is_heading);
    memset(state.block_is_task, 0, sizeof state.block_is_task);
    memset(state.block_is_bullet, 0, sizeof state.block_is_bullet);
    memset(state.block_is_numbered, 0, sizeof state.block_is_numbered);
    memset(state.block_is_quote, 0, sizeof state.block_is_quote);
    memset(state.block_is_code, 0, sizeof state.block_is_code);
    memset(state.block_languages, 0, sizeof state.block_languages);
    memset(state.block_is_image, 0, sizeof state.block_is_image);
    memset(state.block_image_alt, 0, sizeof state.block_image_alt);
    memset(state.block_image_location, 0, sizeof state.block_image_location);
    memset(state.block_is_divider, 0, sizeof state.block_is_divider);
    memset(state.block_numbers, 0, sizeof state.block_numbers);
    state.visual_image_count = 0;
    state.block_count = 0; char *source = editor_text(), *cursor = source;
    do {
        while (*cursor == '\n') cursor++;
        size_t length = visual_block_length(cursor); char *end = cursor + length;
        char *block = calloc(length + 1, 1); memcpy(block, cursor, length);
        Ptr card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3); gtk_widget_add_css_class(card, "block-card"); margins(card, 5, 4);
        Ptr header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        Ptr label = gtk_label_new(block_kind(block)); gtk_label_set_xalign(label, 0.0f); gtk_widget_set_hexpand(label, true);
        gtk_widget_add_css_class(label, "block-kind"); gtk_box_append(header, label);
        size_t block_index = state.block_count;
        Ptr task = NULL, heading_dropdown = NULL, language_entry = NULL, image_alt_entry = NULL, image_location_entry = NULL;
        char code_language[64] = ""; char *code_content = NULL;
        if (block[0] == '#') {
            state.block_is_heading[block_index] = true;
            static const char *levels[] = {"H1", "H2", "H3", "H4", "H5", "H6", NULL};
            guint level = 0; while (level < 6 && block[level] == '#') level++;
            heading_dropdown = gtk_drop_down_new_from_strings(levels); gtk_drop_down_set_selected(heading_dropdown, level ? level - 1 : 0);
            gtk_widget_set_tooltip_text(heading_dropdown, tr("Título")); gtk_box_append(header, heading_dropdown);
        }
        if (block[0] == '|') {
            Ptr add_row = icon_button("list-add-symbolic", tr("Adicionar linha"));
            Ptr add_column = icon_button("insert-table-symbolic", tr("Adicionar coluna"));
            g_signal_connect_data(add_row, "clicked", (void *)table_action, (Ptr)(block_index * 2), NULL, 0);
            g_signal_connect_data(add_column, "clicked", (void *)table_action, (Ptr)(block_index * 2 + 1), NULL, 0);
            gtk_box_append(header, add_row); gtk_box_append(header, add_column);
        }
        if (strncmp(block, "- [ ] ", 6) == 0 || strncmp(block, "- [x] ", 6) == 0 || strncmp(block, "- [X] ", 6) == 0) {
            state.block_is_task[block_index] = true;
            task = gtk_check_button_new_with_label(""); gtk_widget_set_tooltip_text(task, tr("Marcar tarefa como concluída"));
            gtk_check_button_set_active(task, block[3] == 'x' || block[3] == 'X');
        }
        if (!state.block_is_task[block_index] && (strncmp(block, "- ", 2) == 0 || strncmp(block, "* ", 2) == 0))
            state.block_is_bullet[block_index] = true;
        if (isdigit((unsigned char)block[0])) {
            const char *dot = strchr(block, '.'); if (dot && dot[1] == ' ') {
                state.block_is_numbered[block_index] = true; state.block_numbers[block_index] = atoi(block);
            }
        }
        if (strncmp(block, "> ", 2) == 0) state.block_is_quote[block_index] = true;
        if (strncmp(block, "```", 3) == 0) {
            state.block_is_code[block_index] = true; code_content = code_visual_text(block, code_language);
            language_entry = gtk_entry_new(); gtk_entry_set_placeholder_text(language_entry, tr("Linguagem"));
            gtk_editable_set_text(language_entry, code_language); gtk_widget_add_css_class(language_entry, "code-language");
            gtk_widget_set_tooltip_text(language_entry, tr("Linguagem")); gtk_box_append(header, language_entry);
        }
        const char *image_alt = NULL, *image_location = NULL; size_t image_alt_length = 0, image_location_length = 0;
        if (markdown_image(block, &image_alt, &image_alt_length, &image_location, &image_location_length)) {
            state.block_is_image[block_index] = true;
            char alt_value[512], location_value[PATH_MAX];
            snprintf(alt_value, sizeof alt_value, "%.*s", (int)image_alt_length, image_alt);
            snprintf(location_value, sizeof location_value, "%.*s", (int)image_location_length, image_location);
            image_alt_entry = gtk_entry_new(); gtk_entry_set_placeholder_text(image_alt_entry, tr("Descrição da imagem"));
            gtk_editable_set_text(image_alt_entry, alt_value); gtk_widget_add_css_class(image_alt_entry, "image-description");
            image_location_entry = gtk_entry_new(); gtk_entry_set_placeholder_text(image_location_entry, tr("Localização da imagem"));
            gtk_editable_set_text(image_location_entry, location_value); gtk_widget_add_css_class(image_location_entry, "image-location");
        }
        if (strcmp(block, "---") == 0 || strcmp(block, "***") == 0) state.block_is_divider[block_index] = true;
        Ptr duplicate = icon_button("edit-copy-symbolic", tr("Duplicar bloco"));
        Ptr remove = icon_button("user-trash-symbolic", tr("Remover bloco"));
        g_signal_connect_data(duplicate, "clicked", (void *)block_action, (Ptr)(block_index * 2), NULL, 0);
        g_signal_connect_data(remove, "clicked", (void *)block_action, (Ptr)(block_index * 2 + 1), NULL, 0);
        gtk_box_append(header, duplicate); gtk_box_append(header, remove);
        Ptr view = gtk_text_view_new(); gtk_text_view_set_editable(view, true);
        gtk_widget_add_css_class(view, "block-source");
        gtk_text_view_set_wrap_mode(view, GTK_WRAP_WORD_CHAR); gtk_text_view_set_monospace(view, true);
        gtk_widget_set_size_request(view, -1, 74); Ptr buffer = gtk_text_view_get_buffer(view);
        gtk_text_buffer_set_enable_undo(buffer, true);
        const char *visual_text = block;
        char *owned_visual_text = NULL;
        if (state.block_is_heading[block_index]) { while (*visual_text == '#') visual_text++; if (*visual_text == ' ') visual_text++; }
        else if (state.block_is_task[block_index] && strlen(visual_text) >= 6) visual_text += 6;
        else if (state.block_is_bullet[block_index] && strlen(visual_text) >= 2) visual_text += 2;
        else if (state.block_is_numbered[block_index]) { const char *dot = strchr(visual_text, '.'); if (dot && dot[1] == ' ') visual_text = dot + 2; }
        else if (state.block_is_quote[block_index]) { owned_visual_text = quote_visual_text(block); visual_text = owned_visual_text; }
        else if (state.block_is_code[block_index]) visual_text = code_content ? code_content : "";
        else if (state.block_is_divider[block_index]) visual_text = "";
        gtk_text_buffer_set_text(buffer, visual_text, -1); state.block_buffers[state.block_count++] = buffer;
        free(owned_visual_text);
        free(code_content);
        g_signal_connect_data(buffer, "changed", (void *)blocks_changed, NULL, NULL, 0);
        if (heading_dropdown) {
            state.block_headings[block_index] = heading_dropdown;
            g_signal_connect_data(heading_dropdown, "notify::selected", (void *)heading_level_changed, (Ptr)block_index, NULL, 0);
        }
        if (task) {
            state.block_checks[block_index] = task;
            g_signal_connect_data(task, "toggled", (void *)task_toggled, (Ptr)block_index, NULL, 0);
        }
        if (language_entry) {
            state.block_languages[block_index] = language_entry;
            g_signal_connect_data(language_entry, "changed", (void *)code_language_changed, (Ptr)block_index, NULL, 0);
        }
        if (image_alt_entry && image_location_entry) {
            state.block_image_alt[block_index] = image_alt_entry; state.block_image_location[block_index] = image_location_entry;
            g_signal_connect_data(image_alt_entry, "changed", (void *)image_field_changed, (Ptr)block_index, NULL, 0);
            g_signal_connect_data(image_location_entry, "changed", (void *)image_field_changed, (Ptr)block_index, NULL, 0);
        }
        gtk_box_append(card, header);
        if (block[0] == '|') gtk_box_append(card, table_visual_editor(block, block_index));
        else if (state.block_is_divider[block_index]) {
            Ptr divider = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL); gtk_widget_add_css_class(divider, "visual-divider");
            gtk_box_append(card, divider);
        }
        else {
            Ptr visual_image = visual_image_widget(block);
            if (visual_image) { gtk_box_append(card, visual_image); state.visual_image_count++; }
            if (state.block_is_image[block_index]) {
                Ptr image_fields = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5); gtk_widget_add_css_class(image_fields, "image-fields");
                gtk_box_append(image_fields, image_alt_entry); gtk_box_append(image_fields, image_location_entry);
                gtk_box_append(card, image_fields);
            } else if (state.block_is_task[block_index] || state.block_is_bullet[block_index] ||
                state.block_is_numbered[block_index] || state.block_is_quote[block_index]) {
                Ptr content_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 7); gtk_widget_add_css_class(content_row, "structured-line");
                if (task) gtk_box_append(content_row, task);
                else {
                    char marker[24] = "•";
                    if (state.block_is_numbered[block_index]) snprintf(marker, sizeof marker, "%d.", state.block_numbers[block_index]);
                    else if (state.block_is_quote[block_index]) snprintf(marker, sizeof marker, "▌");
                    Ptr marker_label = gtk_label_new(marker); gtk_widget_add_css_class(marker_label,
                        state.block_is_quote[block_index] ? "quote-marker" : "list-marker");
                    gtk_box_append(content_row, marker_label);
                }
                gtk_widget_set_hexpand(view, true); gtk_box_append(content_row, view); gtk_box_append(card, content_row);
            } else gtk_box_append(card, view);
        }
        gtk_box_append(state.block_box, card);
        free(block); cursor = end; while (*cursor == '\n') cursor++; if (!*cursor) break;
    } while (state.block_count < 256);
    g_free(source);
}
void add_visual_block(Ptr button, Ptr user_data) {
    (void)button; const char *template = user_data; TextIter end; gtk_text_buffer_get_end_iter(state.editor_buffer, &end);
    char insertion[1024]; snprintf(insertion, sizeof insertion, "%s%s", state.block_count ? "\n\n" : "", template);
    gtk_text_buffer_insert(state.editor_buffer, &end, insertion, -1); rebuild_block_editor();
}
void show_block_editor(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    if (!state.active) return;
    rebuild_block_editor();
    gtk_stack_set_visible_child_name(state.mode_stack, "visual");
    if (state.mode_editor) gtk_widget_remove_css_class(state.mode_editor, "active-mode");
    if (state.mode_split) gtk_widget_remove_css_class(state.mode_split, "active-mode");
    if (state.mode_visual) gtk_widget_add_css_class(state.mode_visual, "active-mode");
}
void select_editor_mode(Ptr button, Ptr user_data) {
    (void)button; const char *mode = user_data;
    if (!mode) return;
    if (strcmp(mode, "visual") == 0) rebuild_block_editor();
    if (strcmp(mode, "split") == 0) {
        char *text = buffer_text(state.editor_buffer);
        state.split_origin = false; update_views(text); g_free(text);
    }
    gtk_stack_set_visible_child_name(state.mode_stack, mode);
    Ptr buttons[] = {state.mode_editor, state.mode_visual, state.mode_split};
    const char *names[] = {"editor", "visual", "split"};
    for (size_t index = 0; index < 3; index++) {
        if (!buttons[index]) continue;
        gtk_widget_remove_css_class(buttons[index], "active-mode");
        if (strcmp(mode, names[index]) == 0) gtk_widget_add_css_class(buttons[index], "active-mode");
    }
}
void remove_note_from_model(Note *target) {
    Note **cursor = &state.notes;
    while (*cursor && *cursor != target) cursor = &(*cursor)->next;
    if (*cursor) *cursor = target->next;
    Note *next = state.notes;
    if (state.active == target) state.active = NULL;
    discard_note_draft(target); free(target); rebuild_sidebar();
    rebuild_tabs();
    save_settings();
    if (next) select_note(NULL, next);
    else {
        state.loading = true; gtk_editable_set_text(state.title, ""); gtk_editable_set_text(state.tags, "");
        gtk_text_buffer_set_text(state.editor_buffer, "", -1); update_views(""); state.loading = false;
        gtk_label_set_text(state.status, "Crie ou abra uma nota");
    }
}
void delete_note_response(Ptr dialog, int response, Ptr user_data) {
    Note *target = user_data;
    if (response == GTK_RESPONSE_YES && target) {
        bool removed = target->external;
        if (!target->external) {
            Ptr file = g_file_new_for_path(target->directory);
            removed = g_file_trash(file, NULL, NULL); g_object_unref(file);
        }
        if (removed) remove_note_from_model(target);
        else gtk_label_set_text(state.status, "Não foi possível mover a nota para o Lixo");
    }
    gtk_window_destroy(dialog);
}
void delete_active_note(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    if (!state.active) return;
    Ptr dialog = gtk_message_dialog_new(state.window, GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING,
        GTK_BUTTONS_YES_NO, state.active->external ? "Fechar este ficheiro?" : "Remover esta nota?");
    gtk_message_dialog_format_secondary_text(dialog, "%s", state.active->external
        ? "O ficheiro original não será apagado."
        : "A nota e as suas imagens serão movidas para o Lixo.");
    g_signal_connect_data(dialog, "response", (void *)delete_note_response, state.active, NULL, 0);
    gtk_window_present(dialog);
}
void choose_theme(Ptr button, Ptr user_data) {
    (void)button; apply_theme((const char *)user_data);
    gtk_label_set_text(state.status, "Tema aplicado");
}
void autosave_changed(Ptr check, Ptr unused) {
    (void)unused; state.autosave = gtk_check_button_get_active(check); save_settings();
    gtk_label_set_text(state.status, state.autosave ? "Gravação automática ativada" : "Gravação automática desativada");
}
void language_changed(Ptr button, Ptr user_data) {
    (void)button; snprintf(state.language, sizeof state.language, "%s", (const char *)user_data); save_settings();
    gtk_label_set_text(state.status, "Idioma guardado; será aplicado integralmente ao reiniciar");
}
void note_color_changed(Ptr button, Ptr user_data) {
    (void)button; if (!state.active || state.active->external) return;
    snprintf(state.active->color, sizeof state.active->color, "%s", (const char *)user_data);
    save_active(NULL); rebuild_sidebar();
}
void notebook_color_changed(Ptr button, Ptr user_data) {
    (void)button; if (!state.active_notebook || state.active_notebook->external) return;
    snprintf(state.active_notebook->color, sizeof state.active_notebook->color, "%s", (const char *)user_data);
    save_notebook_metadata(state.active_notebook); rebuild_sidebar();
}
bool move_note_between_notebooks(Note *note, Notebook *destination) {
    if (!note || note->external || !destination || destination->external || note->notebook == destination) return false;
    char target[PATH_MAX]; snprintf(target, sizeof target, "%s/%s", destination->path, note->title);
    if (access(target, F_OK) == 0) { gtk_label_set_text(state.status, "Já existe uma nota com esse nome no destino"); return false; }
    Notebook *source = note->notebook;
    if (rename(note->directory, target) != 0) { gtk_label_set_text(state.status, "Não foi possível mover a nota"); return false; }
    snprintf(note->directory, sizeof note->directory, "%s", target);
    snprintf(note->markdown_path, sizeof note->markdown_path, "%s/note.md", target);
    note->notebook = destination; if (state.active == note) state.active_notebook = destination;
    save_notebook_metadata(source); save_notebook_metadata(destination); rebuild_sidebar();
    gtk_label_set_text(state.status, "Nota movida"); return true;
}
void move_note_to_notebook(Ptr button, Ptr user_data) {
    (void)button; move_note_between_notebooks(state.active, user_data);
}
bool reorder_note_before(Note *source, Note *target) {
    if (!source || !target || source == target || source->external || target->external) return false;
    if (source->notebook != target->notebook && !move_note_between_notebooks(source, target->notebook)) return false;
    Note **remove = &state.notes; while (*remove && *remove != source) remove = &(*remove)->next;
    if (!*remove) return false;
    *remove = source->next;
    Note **place = &state.notes; while (*place && *place != target) place = &(*place)->next;
    if (!*place) return false;
    source->next = target; *place = source;
    save_notebook_metadata(target->notebook); rebuild_sidebar(); gtk_label_set_text(state.status, "Nota reordenada"); return true;
}
void rename_notebook_apply(Ptr button, Ptr user_data) {
    (void)button; RenameNotebookData *data = user_data;
    char title[256]; safe_title(gtk_editable_get_text(data->entry), title);
    char destination[PATH_MAX]; snprintf(destination, sizeof destination, "%s/%s", state.root, title);
    size_t old_length = strlen(data->notebook->path);
    if (strcmp(destination, data->notebook->path) == 0 ||
        (access(destination, F_OK) != 0 && rename(data->notebook->path, destination) == 0)) {
        for (Note *note = state.notes; note; note = note->next) if (note->notebook == data->notebook) {
            char old_directory[PATH_MAX]; snprintf(old_directory, sizeof old_directory, "%s", note->directory);
            snprintf(note->directory, sizeof note->directory, "%s%s", destination, old_directory + old_length);
            snprintf(note->markdown_path, sizeof note->markdown_path, "%s/note.md", note->directory);
        }
        snprintf(data->notebook->path, sizeof data->notebook->path, "%s", destination);
        snprintf(data->notebook->title, sizeof data->notebook->title, "%s", title);
        rebuild_sidebar(); gtk_label_set_text(state.status, "Notebook renomeado");
        gtk_window_destroy(data->window); free(data);
    } else gtk_label_set_text(state.status, "Já existe um notebook com esse nome");
}
int note_compare(Note *left, Note *right, bool recent) {
    if (!recent) return strcasecmp(left->title, right->title);
    struct stat a = {0}, b = {0}; stat(left->markdown_path, &a); stat(right->markdown_path, &b);
    if (a.st_mtime == b.st_mtime) return strcasecmp(left->title, right->title);
    return a.st_mtime > b.st_mtime ? -1 : 1;
}
void sort_notes(bool recent) {
    Note *sorted = NULL;
    while (state.notes) {
        Note *moving = state.notes; state.notes = moving->next;
        Note **place = &sorted;
        while (*place && note_compare(*place, moving, recent) <= 0) place = &(*place)->next;
        moving->next = *place; *place = moving;
    }
    state.notes = sorted;
    for (Notebook *book = state.notebooks; book; book = book->next) save_notebook_metadata(book);
    rebuild_sidebar();
}
void reorder_active_note(int direction) {
    if (!state.active || state.active->external) return;
    Note *items[512]; size_t count = 0, current = 0;
    for (Note *note = state.notes; note && count < 512; note = note->next) if (note->notebook == state.active->notebook) {
        items[count] = note; if (note == state.active) current = count; note->order = (int)count++;
    }
    if (!count || (direction < 0 && current == 0) || (direction > 0 && current + 1 >= count)) return;
    size_t other = direction < 0 ? current - 1 : current + 1;
    int value = items[current]->order; items[current]->order = items[other]->order; items[other]->order = value;
    Note *sorted = NULL;
    while (state.notes) {
        Note *moving = state.notes; state.notes = moving->next; Note **place = &sorted;
        while (*place) {
            int books = strcmp((*place)->notebook->title, moving->notebook->title);
            if (books > 0 || (books == 0 && (*place)->order > moving->order)) break;
            place = &(*place)->next;
        }
        moving->next = *place; *place = moving;
    }
    state.notes = sorted; save_notebook_metadata(state.active->notebook); rebuild_sidebar();
}
void delete_notebook_response(Ptr dialog, int response, Ptr user_data) {
    Notebook *target = user_data;
    if (response == GTK_RESPONSE_YES && target) {
        bool removed = target->external;
        if (!target->external) {
            Ptr file = g_file_new_for_path(target->path); removed = g_file_trash(file, NULL, NULL); g_object_unref(file);
        }
        if (removed) {
            Note **note = &state.notes;
            while (*note) {
                if ((*note)->notebook == target) { Note *old = *note; *note = old->next; discard_note_draft(old); free(old); }
                else note = &(*note)->next;
            }
            Notebook **book = &state.notebooks;
            while (*book && *book != target) book = &(*book)->next;
            if (*book) *book = target->next;
            if (state.active_notebook == target) state.active_notebook = state.notebooks;
            state.active = NULL; free(target); rebuild_sidebar();
            if (state.notes) select_note(NULL, state.notes);
            else { state.loading = true; gtk_editable_set_text(state.title, ""); gtk_editable_set_text(state.tags, "");
                gtk_text_buffer_set_text(state.editor_buffer, "", -1); update_views(""); state.loading = false; }
        } else gtk_label_set_text(state.status, "Não foi possível mover o notebook para o Lixo");
    }
    gtk_window_destroy(dialog);
}
Ptr context_action(const char *icon, const char *label, void *callback, Ptr data) {
    Ptr button = navigation_button(icon, label); gtk_widget_add_css_class(button, "context-action");
    g_signal_connect_data(button, "clicked", callback, data, NULL, 0); return button;
}
Ptr color_actions(bool notebook) {
    static const char *names[] = {"Sem cor", "Vermelho", "Laranja", "Amarelo", "Verde", "Azul", "Roxo"};
    static const char *values[] = {"", "#d1242f", "#bc4c00", "#9a6700", "#1a7f37", "#0969da", "#8250df"};
    static const char *classes[] = {"color-none", "color-red", "color-orange", "color-yellow", "color-green", "color-blue", "color-purple"};
    Ptr row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3); gtk_widget_add_css_class(row, "color-palette");
    for (size_t index = 0; index < sizeof values / sizeof values[0]; index++) {
        Ptr choice = icon_button(index ? "media-record-symbolic" : "edit-clear-symbolic", names[index]);
        gtk_widget_add_css_class(choice, "color-swatch"); gtk_widget_add_css_class(choice, classes[index]);
        g_signal_connect_data(choice, "clicked", (void *)(notebook ? notebook_color_changed : note_color_changed),
            (Ptr)values[index], NULL, 0); gtk_box_append(row, choice);
    }
    return row;
}
void library_menu_clicked(Ptr button, Ptr user_data) {
    (void)button; ContextTarget *context = user_data;
    if (!context || !context->target) return;
    Ptr anchor = context->widget;
    Ptr popover = gtk_popover_new(); Ptr box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(box, "context-menu");
    if (context->notebook) {
        Notebook *book = context->target; state.active_notebook = book;
        Ptr heading = gtk_label_new(book->title); gtk_label_set_xalign(heading, 0.0f);
        gtk_widget_add_css_class(heading, "context-title"); gtk_box_append(box, heading);
        if (!book->external) {
            gtk_box_append(box, context_action("document-new-symbolic", "Nova nota", (void *)create_note_in_notebook, book));
            gtk_box_append(box, context_action("document-properties-symbolic", "Criar a partir de template", (void *)show_templates, NULL));
            gtk_box_append(box, context_action("edit-rename-symbolic", "Renomear notebook", (void *)rename_selected_notebook, NULL));
            Ptr color_label = gtk_label_new("Cor do notebook"); gtk_label_set_xalign(color_label, 0.0f);
            gtk_widget_add_css_class(color_label, "context-section"); gtk_box_append(box, color_label);
            gtk_box_append(box, color_actions(true));
            gtk_box_append(box, context_action("view-sort-ascending-symbolic", "Ordenar por título", (void *)sort_title, NULL));
            gtk_box_append(box, context_action("document-open-recent-symbolic", "Mais recentes", (void *)sort_recent, NULL));
        }
        Ptr remove = context_action("user-trash-symbolic", book->external ? "Fechar ficheiros externos" : "Remover notebook",
            (void *)delete_selected_notebook, NULL); gtk_widget_add_css_class(remove, "destructive-action"); gtk_box_append(box, remove);
    } else {
        Note *note = context->target; select_note(NULL, note);
        Ptr heading = gtk_label_new(note_display_title(note)); gtk_label_set_xalign(heading, 0.0f);
        gtk_widget_add_css_class(heading, "context-title"); gtk_box_append(box, heading);
        if (!note->external) {
            Ptr color_label = gtk_label_new("Cor da nota"); gtk_label_set_xalign(color_label, 0.0f);
            gtk_widget_add_css_class(color_label, "context-section"); gtk_box_append(box, color_label);
            gtk_box_append(box, color_actions(false));
            gtk_box_append(box, context_action("go-up-symbolic", "Mover para cima", (void *)move_note_up, NULL));
            gtk_box_append(box, context_action("go-down-symbolic", "Mover para baixo", (void *)move_note_down, NULL));
            Ptr move_label = gtk_label_new("Mover para notebook"); gtk_label_set_xalign(move_label, 0.0f);
            gtk_widget_add_css_class(move_label, "context-section"); gtk_box_append(box, move_label);
            for (Notebook *book = state.notebooks; book; book = book->next)
                if (!book->external && book != note->notebook)
                    gtk_box_append(box, context_action("folder-symbolic", book->title, (void *)move_note_to_notebook, book));
        }
        Ptr remove = context_action("user-trash-symbolic", note->external ? "Fechar ficheiro" : "Remover nota",
            (void *)delete_active_note, NULL); gtk_widget_add_css_class(remove, "destructive-action"); gtk_box_append(box, remove);
    }
    margins(box, 10, 10); gtk_popover_set_child(popover, box); present_popover(popover, anchor);
}
Ptr library_menu_button(Ptr target, bool notebook) {
    Ptr button = icon_button("view-more-symbolic", notebook ? "Ações do notebook" : "Ações da nota");
    gtk_widget_add_css_class(button, "item-menu");
    ContextTarget *context = calloc(1, sizeof *context); context->widget = button; context->target = target; context->notebook = notebook;
    g_signal_connect_data(button, "clicked", (void *)library_menu_clicked, context, (void *)free_signal_data, 0);
    state.context_menu_count++; return button;
}
void library_context_pressed(Ptr gesture, int presses, double x, double y, Ptr user_data) {
    (void)gesture; (void)presses; (void)x; (void)y; library_menu_clicked(NULL, user_data);
}
void show_export_menu(Ptr button, Ptr unused) {
    (void)unused; Ptr popover = gtk_popover_new(); Ptr box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(box, "context-menu");
    Ptr heading = gtk_label_new("Exportar nota"); gtk_label_set_xalign(heading, 0.0f);
    gtk_widget_add_css_class(heading, "context-title"); gtk_box_append(box, heading);
    gtk_box_append(box, context_action("text-html-symbolic", "HTML", (void *)export_html, NULL));
    gtk_box_append(box, context_action("application-pdf-symbolic", "PDF", (void *)export_office, "pdf"));
    gtk_box_append(box, context_action("x-office-document-symbolic", "DOCX", (void *)export_office, "docx"));
    margins(box, 10, 10); gtk_popover_set_child(popover, box); present_popover(popover, button);
}
char *editor_text(void) {
    TextIter start, end;
    gtk_text_buffer_get_bounds(state.editor_buffer, &start, &end);
    return gtk_text_buffer_get_text(state.editor_buffer, &start, &end, true);
}
void preview_line(Ptr buffer, const char *text, const char *tag) {
    int start_offset = gtk_text_buffer_get_char_count(buffer);
    TextIter cursor; gtk_text_buffer_get_end_iter(buffer, &cursor);
    gtk_text_buffer_insert(buffer, &cursor, text, -1); gtk_text_buffer_insert(buffer, &cursor, "\n", 1);
    if (tag) {
        TextIter start, end; gtk_text_buffer_get_iter_at_offset(buffer, &start, start_offset);
        gtk_text_buffer_get_end_iter(buffer, &end); gtk_text_buffer_apply_tag_by_name(buffer, tag, &start, &end);
    }
}
void preview_tagged_segment(Ptr buffer, const char *text, size_t length, const char *tag) {
    int start_offset = gtk_text_buffer_get_char_count(buffer); TextIter cursor;
    gtk_text_buffer_get_end_iter(buffer, &cursor); gtk_text_buffer_insert(buffer, &cursor, text, (int)length);
    if (tag) {
        TextIter start, end; gtk_text_buffer_get_iter_at_offset(buffer, &start, start_offset);
        gtk_text_buffer_get_end_iter(buffer, &end); gtk_text_buffer_apply_tag_by_name(buffer, tag, &start, &end);
    }
}
void preview_inline_line(Ptr buffer, const char *text, const char *line_tag) {
    int line_start = gtk_text_buffer_get_char_count(buffer); const char *cursor = text;
    while (*cursor) {
        const char *close = NULL;
        if (strncmp(cursor, "<span style=\"color:#", 20) == 0) {
            const char *content = strchr(cursor + 20, '>'), *end = content ? strstr(content + 1, "</span>") : NULL;
            if (content && end && content - cursor >= 27) {
                char color[8]; memcpy(color, cursor + 19, 7); color[7] = '\0'; const char *tag = NULL;
                if (strcasecmp(color, "#d1242f") == 0) tag = "text-red";
                else if (strcasecmp(color, "#bc4c00") == 0) tag = "text-orange";
                else if (strcasecmp(color, "#9a6700") == 0) tag = "text-yellow";
                else if (strcasecmp(color, "#1a7f37") == 0) tag = "text-green";
                else if (strcasecmp(color, "#0969da") == 0) tag = "text-blue";
                else if (strcasecmp(color, "#8250df") == 0) tag = "text-purple";
                if (tag) { preview_tagged_segment(buffer, content + 1, (size_t)(end - content - 1), tag); cursor = end + 7; continue; }
            }
        }
        if (strncmp(cursor, "**", 2) == 0 && (close = strstr(cursor + 2, "**"))) {
            preview_tagged_segment(buffer, cursor + 2, (size_t)(close - cursor - 2), "strong"); cursor = close + 2; continue;
        }
        if (strncmp(cursor, "~~", 2) == 0 && (close = strstr(cursor + 2, "~~"))) {
            preview_tagged_segment(buffer, cursor + 2, (size_t)(close - cursor - 2), "strike"); cursor = close + 2; continue;
        }
        if (*cursor == '_' && (close = strchr(cursor + 1, '_'))) {
            preview_tagged_segment(buffer, cursor + 1, (size_t)(close - cursor - 1), "emphasis"); cursor = close + 1; continue;
        }
        if (*cursor == '`' && (close = strchr(cursor + 1, '`'))) {
            preview_tagged_segment(buffer, cursor + 1, (size_t)(close - cursor - 1), "inline-code"); cursor = close + 1; continue;
        }
        if (*cursor == '[' && cursor[1] != '[' && (close = strstr(cursor + 1, "]("))) {
            const char *end = strchr(close + 2, ')');
            if (end) { preview_tagged_segment(buffer, cursor + 1, (size_t)(close - cursor - 1), "link"); cursor = end + 1; continue; }
        }
        if (strncmp(cursor, "[[", 2) == 0 && (close = strstr(cursor + 2, "]]"))) {
            preview_tagged_segment(buffer, cursor + 2, (size_t)(close - cursor - 2), "link"); cursor = close + 2; continue;
        }
        const char *next = cursor + 1;
        while (*next && *next != '*' && *next != '~' && *next != '_' && *next != '`' && *next != '[' && *next != '<') next++;
        preview_tagged_segment(buffer, cursor, (size_t)(next - cursor), NULL); cursor = next;
    }
    preview_tagged_segment(buffer, "\n", 1, NULL);
    if (line_tag) {
        TextIter start, end; gtk_text_buffer_get_iter_at_offset(buffer, &start, line_start);
        gtk_text_buffer_get_end_iter(buffer, &end); gtk_text_buffer_apply_tag_by_name(buffer, line_tag, &start, &end);
    }
}
void preview_table_line(Ptr buffer, const char *line, bool header) {
    char *copy = strdup(line), *cell = copy; while (*cell == ' ' || *cell == '|') cell++;
    size_t length = strlen(cell); while (length && (cell[length - 1] == ' ' || cell[length - 1] == '|')) cell[--length] = '\0';
    StringBuilder row = {0}; char *save = NULL; bool first = true;
    for (char *value = strtok_r(cell, "|", &save); value; value = strtok_r(NULL, "|", &save)) {
        while (*value == ' ') value++;
        size_t value_length = strlen(value);
        while (value_length && value[value_length - 1] == ' ') value[--value_length] = '\0';
        if (!first) builder_append(&row, "   │   ");
        builder_append(&row, value); first = false;
    }
    preview_inline_line(buffer, row.value ? row.value : "", header ? "table-header" : "table-row");
    free(row.value); free(copy);
}
bool preview_image(Ptr buffer, const char *line) {
    if (!state.active || strncmp(line, "![", 2) != 0) return false;
    const char *open = strstr(line, "]("), *close = open ? strrchr(open + 2, ')') : NULL;
    if (!open || !close || close <= open + 2) return false;
    char relative[PATH_MAX], path[PATH_MAX];
    snprintf(relative, sizeof relative, "%.*s", (int)(close - open - 2), open + 2);
    if (relative[0] == '/') snprintf(path, sizeof path, "%s", relative);
    else snprintf(path, sizeof path, "%s/%s", state.active->directory, relative);
    if (access(path, R_OK) != 0) return false;
    Ptr view = buffer == state.preview_buffer ? state.preview_view :
        buffer == state.split_preview_buffer ? state.split_preview_view : NULL;
    if (!view) return false;
    TextIter end; gtk_text_buffer_get_end_iter(buffer, &end);
    Ptr anchor = gtk_text_buffer_create_child_anchor(buffer, &end);
    Ptr picture = gtk_picture_new_for_filename(path); gtk_picture_set_content_fit(picture, 3);
    gtk_widget_set_size_request(picture, 520, 280); gtk_text_view_add_child_at_anchor(view, picture, anchor);
    gtk_text_buffer_get_end_iter(buffer, &end); gtk_text_buffer_insert(buffer, &end, "\n", 1); return true;
}
void render_markdown(Ptr buffer, const char *source) {
    gtk_text_buffer_set_text(buffer, "", -1); char *copy = strdup(source), *save = NULL; bool code = false, table = false;
    for (char *line = strtok_r(copy, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        if (!code && preview_image(buffer, line)) continue;
        if (!code && strncmp(line, "![", 2) == 0) {
            const char *alt, *location; size_t alt_length, location_length;
            if (markdown_image(line, &alt, &alt_length, &location, &location_length)) {
                char unavailable[1024]; snprintf(unavailable, sizeof unavailable, "▧  %.*s  ·  imagem indisponível", (int)alt_length, alt);
                preview_inline_line(buffer, unavailable, "quote"); continue;
            }
        }
        if (strncmp(line, "```", 3) == 0) { code = !code; continue; }
        if (code) preview_line(buffer, line, "code");
        else if (line[0] == '|' && strstr(line, "---")) { table = true; continue; }
        else if (line[0] == '|') { preview_table_line(buffer, line, !table); table = true; }
        else if (strncmp(line, "###### ", 7) == 0) preview_inline_line(buffer, line + 7, "h3");
        else if (strncmp(line, "##### ", 6) == 0) preview_inline_line(buffer, line + 6, "h3");
        else if (strncmp(line, "#### ", 5) == 0) preview_inline_line(buffer, line + 5, "h3");
        else if (strncmp(line, "### ", 4) == 0) preview_inline_line(buffer, line + 4, "h3");
        else if (strncmp(line, "## ", 3) == 0) preview_inline_line(buffer, line + 3, "h2");
        else if (strncmp(line, "# ", 2) == 0) preview_inline_line(buffer, line + 2, "h1");
        else if (strncmp(line, "> ", 2) == 0) preview_inline_line(buffer, line + 2, "quote");
        else if (strncmp(line, "- [ ] ", 6) == 0) { char value[2048]; snprintf(value, sizeof value, "☐  %s", line + 6); preview_inline_line(buffer, value, "list"); }
        else if (strncmp(line, "- [x] ", 6) == 0 || strncmp(line, "- [X] ", 6) == 0) {
            char value[2048]; snprintf(value, sizeof value, "☑  %s", line + 6); preview_inline_line(buffer, value, "done");
        } else if (strncmp(line, "- ", 2) == 0 || strncmp(line, "* ", 2) == 0) {
            char value[2048]; snprintf(value, sizeof value, "•  %s", line + 2); preview_inline_line(buffer, value, "list");
        } else if (line[0] >= '0' && line[0] <= '9' && strchr(line, '.') && strchr(line, '.')[1] == ' ')
            preview_inline_line(buffer, line, "list");
        else if (strcmp(line, "---") == 0 || strcmp(line, "***") == 0)
            preview_line(buffer, "────────────────────────────", "divider");
        else { table = false; preview_inline_line(buffer, line, NULL); }
    }
    free(copy);
}
void update_views(const char *text) {
    bool previous_sync = state.view_syncing; state.view_syncing = true;
    if (state.line_buffer) {
        size_t lines = 1; for (const char *p = text; *p; p++) if (*p == '\n') lines++;
        size_t size = lines * 16 + 1, used = 0; char *numbers = calloc(size, 1);
        for (size_t line = 1; line <= lines; line++) used += (size_t)snprintf(numbers + used, size - used, "%zu\n", line);
        gtk_text_buffer_set_text(state.line_buffer, numbers, -1); free(numbers); apply_active_line_tag();
    }
    if (state.preview_buffer) render_markdown(state.preview_buffer, text);
    if (state.split_buffer && !state.split_origin) gtk_text_buffer_set_text(state.split_buffer, text, -1);
    if (state.split_preview_buffer) render_markdown(state.split_preview_buffer, text);
    rebuild_links(text);
    state.view_syncing = previous_sync;
}
void split_content_changed(Ptr buffer, Ptr unused) {
    (void)unused;
    if (state.loading || state.view_syncing || !state.active) return;
    char *text = buffer_text(buffer); state.loading = true;
    gtk_text_buffer_set_text(state.editor_buffer, text, -1); state.loading = false;
    g_free(text); state.split_origin = true; content_changed(state.editor_buffer, NULL); state.split_origin = false;
}
void apply_editor_tag(Ptr buffer, const char *source, const char *start_pointer, const char *end_pointer, const char *tag) {
    TextIter start, end; gtk_text_buffer_get_iter_at_offset(buffer, &start, utf8_offset(source, start_pointer));
    gtk_text_buffer_get_iter_at_offset(buffer, &end, utf8_offset(source, end_pointer));
    gtk_text_buffer_apply_tag_by_name(buffer, tag, &start, &end);
}
void highlight_editor(const char *source) {
    TextIter document_start, document_end; gtk_text_buffer_get_bounds(state.editor_buffer, &document_start, &document_end);
    gtk_text_buffer_remove_all_tags(state.editor_buffer, &document_start, &document_end);
    bool code = false; const char *line = source;
    while (*line) {
        const char *end = strchr(line, '\n'); if (!end) end = line + strlen(line);
        if (end - line >= 3 && strncmp(line, "```", 3) == 0) { apply_editor_tag(state.editor_buffer, source, line, end, "syntax-code"); code = !code; }
        else if (code) apply_editor_tag(state.editor_buffer, source, line, end, "syntax-code");
        else if (end - line >= 2 && line[0] == '#' && line[1] == ' ') apply_editor_tag(state.editor_buffer, source, line, end, "syntax-heading");
        else if (end - line >= 3 && line[0] == '#' && line[1] == '#' && line[2] == ' ') apply_editor_tag(state.editor_buffer, source, line, end, "syntax-heading");
        else if (end - line >= 2 && ((line[0] == '-' || line[0] == '*') && line[1] == ' ')) apply_editor_tag(state.editor_buffer, source, line, end, "syntax-list");
        else if (end - line >= 2 && line[0] == '>' && line[1] == ' ') apply_editor_tag(state.editor_buffer, source, line, end, "syntax-quote");
        const char *cursor = line;
        while (!code && (cursor = strstr(cursor, "**")) && cursor < end) {
            const char *close = strstr(cursor + 2, "**"); if (!close || close >= end) break;
            apply_editor_tag(state.editor_buffer, source, cursor, close + 2, "syntax-emphasis"); cursor = close + 2;
        }
        cursor = line;
        while (!code && (cursor = strstr(cursor, "[[")) && cursor < end) {
            const char *close = strstr(cursor + 2, "]]" ); if (!close || close >= end) break;
            apply_editor_tag(state.editor_buffer, source, cursor, close + 2, "syntax-link"); cursor = close + 2;
        }
        cursor = line;
        while (!code && (cursor = strstr(cursor, "`")) && cursor < end) {
            const char *close = strstr(cursor + 1, "`"); if (!close || close >= end) break;
            apply_editor_tag(state.editor_buffer, source, cursor, close + 1, "syntax-code"); cursor = close + 1;
        }
        line = *end ? end + 1 : end;
    }
}
Note *note_named(const char *title) {
    for (Note *note = state.notes; note; note = note->next)
        if (strcasecmp(note_display_title(note), title) == 0) return note;
    return NULL;
}
void open_linked_note(Ptr button, Ptr user_data) {
    (void)button; select_note(NULL, user_data);
}
void create_linked_note(Ptr button, Ptr user_data) {
    (void)button; const char *title = user_data;
    if (!title || !title[0] || !state.active_notebook) return;
    Note *existing = note_named(title); if (existing) { select_note(NULL, existing); return; }
    new_note(NULL, NULL); if (!state.active) return;
    char markdown[320]; snprintf(markdown, sizeof markdown, "# %s\n", title);
    state.loading = true; gtk_editable_set_text(state.title, title);
    gtk_text_buffer_set_text(state.editor_buffer, markdown, -1); state.loading = false;
    state.active->dirty = true; capture_active_draft(); save_active(NULL);
}
void rebuild_links(const char *text) {
    if (!state.linksbar) return;
    while (gtk_widget_get_first_child(state.linksbar))
        gtk_box_remove(state.linksbar, gtk_widget_get_first_child(state.linksbar));
    if (!state.active) {
        gtk_widget_set_visible(state.linksbar, false);
        return;
    }
    size_t visible_count = 0;
    const char *cursor = text; char outgoing[128][256]; size_t outgoing_count = 0;
    while ((cursor = strstr(cursor, "[["))) {
        const char *end = strstr(cursor + 2, "]]" ); if (!end) break;
        char title[256]; size_t length = (size_t)(end - cursor - 2);
        if (length >= sizeof title) length = sizeof title - 1;
        memcpy(title, cursor + 2, length); title[length] = '\0';
        bool duplicate = false; for (size_t index = 0; index < outgoing_count; index++)
            if (strcasecmp(outgoing[index], title) == 0) { duplicate = true; break; }
        if (duplicate || !title[0]) { cursor = end + 2; continue; }
        if (outgoing_count < 128) snprintf(outgoing[outgoing_count++], sizeof outgoing[0], "%s", title);
        Note *linked = note_named(title);
        Ptr button = gtk_button_new_with_label(title);
        if (linked)
            g_signal_connect_data(button, "clicked", (void *)open_linked_note, linked, NULL, 0);
        else {
            char *new_title = strdup(title); gtk_widget_add_css_class(button, "missing-link");
            gtk_widget_set_tooltip_text(button, tr("Criar esta nota"));
            g_signal_connect_data(button, "clicked", (void *)create_linked_note, new_title, (void *)free_signal_data, 0);
        }
        gtk_box_append(state.linksbar, button);
        visible_count++;
        cursor = end + 2;
    }
    if (!state.active) return;
    char needle[300]; snprintf(needle, sizeof needle, "[[%s]]", note_display_title(state.active));
    for (Note *note = state.notes; note; note = note->next) {
        if (note == state.active) continue;
        char *source = note_current_markdown(note);
        if (strstr(source, needle)) {
            char backlink[300]; snprintf(backlink, sizeof backlink, "← %s", note_display_title(note));
            Ptr button = gtk_button_new_with_label(backlink);
            g_signal_connect_data(button, "clicked", (void *)open_linked_note, note, NULL, 0);
            gtk_box_append(state.linksbar, button);
            visible_count++;
        }
        free(source);
    }
    gtk_widget_set_visible(state.linksbar, visible_count > 0);
}
void apply_active_line_tag(void) {
    if (!state.line_buffer || state.active_line < 1) return;
    int start_offset = 0; char number[32];
    for (int line = 1; line < state.active_line; line++) start_offset += snprintf(number, sizeof number, "%d", line) + 1;
    int end_offset = start_offset + snprintf(number, sizeof number, "%d", state.active_line);
    TextIter document_start, document_end, start, end;
    gtk_text_buffer_get_bounds(state.line_buffer, &document_start, &document_end);
    gtk_text_buffer_remove_all_tags(state.line_buffer, &document_start, &document_end);
    gtk_text_buffer_get_iter_at_offset(state.line_buffer, &start, start_offset);
    gtk_text_buffer_get_iter_at_offset(state.line_buffer, &end, end_offset);
    gtk_text_buffer_apply_tag_by_name(state.line_buffer, "active-line", &start, &end);
}
void cursor_mark_set(Ptr buffer, const void *location, Ptr mark, Ptr unused) {
    (void)unused; if (!state.line_buffer || mark != gtk_text_buffer_get_insert(buffer)) return;
    state.active_line = gtk_text_iter_get_line(location) + 1; apply_active_line_tag();
}
void update_statistics(const char *text) {
    if (!text) text = "";
    size_t words = 0, lines = 1; bool inside = false;
    for (const char *p = text; *p; p++) {
        bool now = !strchr(" \t\r\n", *p);
        if (now && !inside) words++;
        if (*p == '\n') lines++;
        inside = now;
    }
    size_t characters = (size_t)utf8_offset(text, text + strlen(text));
    if (state.stats) {
        char value[160]; snprintf(value, sizeof value, "%zu %s  •  %zu %s", words, tr("palavras"), characters, tr("carateres"));
        gtk_label_set_text(state.stats, value);
    }
    if (state.line_badge) {
        char value[80]; snprintf(value, sizeof value, "%zu %s", lines, tr("linhas")); gtk_label_set_text(state.line_badge, value);
    }
}
void insert_markdown(Ptr button, Ptr user_data) {
    (void)button;
    if (!state.active) return;
    const char *format = user_data, *separator = strchr(format, '|');
    size_t prefix_length = separator ? (size_t)(separator - format) : strlen(format);
    const char *suffix = separator ? separator + 1 : "";
    
    Ptr buffer = state.editor_buffer;
    const char *mode = gtk_stack_get_visible_child_name(state.mode_stack);
    if (mode) {
        if (strcmp(mode, "split") == 0) {
            Ptr focused = gtk_window_get_focus(state.window);
            if (focused && gtk_text_view_get_buffer(focused) == state.split_buffer) {
                buffer = state.split_buffer;
            } else {
                TextIter t1, t2;
                if (!gtk_text_buffer_get_selection_bounds(state.editor_buffer, &t1, &t2) &&
                    gtk_text_buffer_get_selection_bounds(state.split_buffer, &t1, &t2)) {
                    buffer = state.split_buffer;
                }
            }
        } else if (strcmp(mode, "visual") == 0) {
            Ptr focused = gtk_window_get_focus(state.window);
            if (focused) {
                buffer = gtk_text_view_get_buffer(focused);
            }
        }
    }

    TextIter start, end;
    if (gtk_text_buffer_get_selection_bounds(buffer, &start, &end)) {
        char *selected = gtk_text_buffer_get_text(buffer, &start, &end, true);
        size_t size = prefix_length + strlen(selected) + strlen(suffix) + 1; char *replacement = calloc(size, 1);
        memcpy(replacement, format, prefix_length); strcat(replacement, selected); strcat(replacement, suffix);
        gtk_text_buffer_delete(buffer, &start, &end); gtk_text_buffer_insert(buffer, &start, replacement, -1);
        free(replacement); g_free(selected);
    } else {
        size_t size = prefix_length + strlen(suffix) + 1; char *insertion = calloc(size, 1);
        memcpy(insertion, format, prefix_length); strcat(insertion, suffix);
        // Note: gtk_text_buffer_insert_at_cursor is specifically for GtkTextBuffer, 
        // we can do a fallback or manually get the cursor position and insert there.
        // Actually, we can get the insert mark and insert at its iterator:
        Ptr insert_mark = gtk_text_buffer_get_insert(buffer);
        TextIter iter;
        gtk_text_buffer_get_iter_at_mark(buffer, &iter, insert_mark);
        gtk_text_buffer_insert(buffer, &iter, insertion, -1);
        free(insertion);
    }
}
void heading_format_changed(Ptr dropdown, Ptr unused, Ptr user_data) {
    (void)unused; (void)user_data; if (!state.active || state.loading) return;
    guint level = gtk_drop_down_get_selected(dropdown); if (level == 0 || level > 6) return; char format[16];
    memset(format, '#', level); format[level] = ' '; format[level + 1] = '|'; format[level + 2] = '\0';
    insert_markdown(NULL, format); state.loading = true; gtk_drop_down_set_selected(dropdown, 0); state.loading = false;
}
void text_color_changed(Ptr dropdown, Ptr unused, Ptr user_data) {
    (void)unused; (void)user_data; if (!state.active || state.loading) return;
    static const char *colors[] = {NULL, "#d1242f", "#bc4c00", "#9a6700", "#1a7f37", "#0969da", "#8250df"};
    guint selected = gtk_drop_down_get_selected(dropdown); if (selected == 0 || selected >= sizeof colors / sizeof colors[0]) return;
    char format[96]; snprintf(format, sizeof format, "<span style=\"color:%s\">|</span>", colors[selected]);
    insert_markdown(NULL, format); state.loading = true; gtk_drop_down_set_selected(dropdown, 0); state.loading = false;
}
void insert_literal(Ptr button, Ptr user_data) {
    (void)button; if (state.active) gtk_text_buffer_insert_at_cursor(state.editor_buffer, (const char *)user_data, -1);
}
Ptr editor_scroller(Ptr view) {
    Ptr scroll = gtk_scrolled_window_new(); gtk_widget_set_hexpand(scroll, true);
    gtk_widget_set_vexpand(scroll, true); gtk_scrolled_window_set_child(scroll, view);
    return scroll;
}
Ptr readonly_markdown_view(Ptr *buffer, const char *css_class) {
    Ptr view = gtk_text_view_new(); gtk_text_view_set_editable(view, false);
    gtk_text_view_set_cursor_visible(view, false); gtk_text_view_set_wrap_mode(view, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(view, 30); gtk_text_view_set_right_margin(view, 30);
    gtk_text_view_set_top_margin(view, 22); gtk_widget_add_css_class(view, css_class);
    *buffer = gtk_text_view_get_buffer(view);
    if (strcmp(css_class, "preview") == 0) {
        gtk_text_buffer_create_tag(*buffer, "h1", "scale", 1.8, "weight", 700, "pixels-above-lines", 14, "pixels-below-lines", 7, NULL);
        gtk_text_buffer_create_tag(*buffer, "h2", "scale", 1.45, "weight", 700, "pixels-above-lines", 11, NULL);
        gtk_text_buffer_create_tag(*buffer, "h3", "scale", 1.2, "weight", 700, "pixels-above-lines", 8, NULL);
        gtk_text_buffer_create_tag(*buffer, "code", "family", "monospace", "background", "#252a34", NULL);
        gtk_text_buffer_create_tag(*buffer, "quote", "style", 2, "left-margin", 22, "foreground", "#8992a8", NULL);
        gtk_text_buffer_create_tag(*buffer, "list", "left-margin", 12, NULL);
        gtk_text_buffer_create_tag(*buffer, "done", "strikethrough", true, "foreground", "#737d93", NULL);
        gtk_text_buffer_create_tag(*buffer, "strong", "weight", 700, NULL);
        gtk_text_buffer_create_tag(*buffer, "emphasis", "style", 2, NULL);
        gtk_text_buffer_create_tag(*buffer, "strike", "strikethrough", true, "foreground", "#8992a8", NULL);
        gtk_text_buffer_create_tag(*buffer, "inline-code", "family", "monospace", "background", "#252a34", "foreground", "#c29df1", NULL);
        gtk_text_buffer_create_tag(*buffer, "link", "foreground", "#78a9ff", "underline", 1, NULL);
        gtk_text_buffer_create_tag(*buffer, "table-header", "weight", 700, "background", "#202530", "pixels-above-lines", 5, "pixels-below-lines", 5, NULL);
        gtk_text_buffer_create_tag(*buffer, "table-row", "family", "monospace", "pixels-above-lines", 3, "pixels-below-lines", 3, NULL);
        gtk_text_buffer_create_tag(*buffer, "divider", "foreground", "#596174", "justification", 2, NULL);
        gtk_text_buffer_create_tag(*buffer, "text-red", "foreground", "#ff657a", NULL);
        gtk_text_buffer_create_tag(*buffer, "text-orange", "foreground", "#ff9f66", NULL);
        gtk_text_buffer_create_tag(*buffer, "text-yellow", "foreground", "#e7c664", NULL);
        gtk_text_buffer_create_tag(*buffer, "text-green", "foreground", "#68d391", NULL);
        gtk_text_buffer_create_tag(*buffer, "text-blue", "foreground", "#78a9ff", NULL);
        gtk_text_buffer_create_tag(*buffer, "text-purple", "foreground", "#c29df1", NULL);
    }
    return view;
}
gboolean key_pressed(Ptr controller, guint keyval, guint keycode, guint modifiers, Ptr unused) {
    (void)controller; (void)keycode; (void)unused;
    if (!(modifiers & GDK_CONTROL_MASK)) return false;
    switch (keyval) {
        case 's': case 'S': save_active(NULL); return true;
        case 'f': case 'F': show_find(NULL, NULL); return true;
        case 'o': case 'O': open_markdown(NULL, NULL); return true;
        case 'b': case 'B': toggle_sidebar(NULL, NULL); return true;
        case 'n': case 'N':
            if (modifiers & GDK_SHIFT_MASK) new_notebook(NULL, NULL); else new_note(NULL, NULL);
            return true;
        default: return false;
    }
}
gboolean files_dropped(Ptr target, const void *value, double x, double y, Ptr unused) {
    (void)target; (void)x; (void)y; (void)unused; bool accepted = false;
    GSList *files = g_value_get_boxed(value);
    for (GSList *item = files; item; item = item->next) {
        char *path = g_file_get_path(item->data); if (!path) continue;
        const char *extension = strrchr(path, '.');
        if (extension && (strcasecmp(extension, ".md") == 0 || strcasecmp(extension, ".markdown") == 0 ||
                          strcasecmp(extension, ".mdown") == 0 || strcasecmp(extension, ".mkd") == 0)) {
            Note *note = add_external_note(path); if (note) { rebuild_sidebar(); select_note(NULL, note); accepted = true; }
        } else if (extension && (strcasecmp(extension, ".png") == 0 || strcasecmp(extension, ".jpg") == 0 ||
                                 strcasecmp(extension, ".jpeg") == 0 || strcasecmp(extension, ".gif") == 0 ||
                                 strcasecmp(extension, ".webp") == 0 || strcasecmp(extension, ".svg") == 0)) {
            accepted = store_image(path) || accepted;
        }
        g_free(path);
    }
    if (accepted) save_settings();
    return accepted;
}
void external_conflict_response(Ptr dialog, int response, Ptr user_data) {
    Note *note = user_data; state.external_conflict_dialog = NULL;
    if (note && note == state.active) {
        if (response == 1) {
            save_active(NULL); gtk_label_set_text(state.status, "Ficheiro externo substituído");
        } else if (response == 2) {
            char *content = read_file(note->markdown_path); state.loading = true;
            gtk_text_buffer_set_text(state.editor_buffer, content, -1); highlight_editor(content); update_views(content);
            note->dirty = false; clear_recovery(note); state.loading = false; free(content);
            gtk_label_set_text(state.status, "Alterações externas recarregadas");
        } else gtk_label_set_text(state.status, "Conflito externo mantido sem alterações");
    }
    gtk_window_destroy(dialog);
}
gboolean poll_external_file(Ptr unused) {
    (void)unused;
    if (!state.active || !state.active->external) return true;
    struct stat info; if (stat(state.active->markdown_path, &info) != 0 || info.st_mtime == state.active->external_mtime) return true;
    state.active->external_mtime = info.st_mtime;
    if (state.active->dirty) {
        Ptr dialog = gtk_message_dialog_new(state.window, GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, 0,
            "O ficheiro foi alterado noutra aplicação");
        gtk_message_dialog_format_secondary_text(dialog, "%s",
            "Escolha se pretende substituir o ficheiro com esta versão ou recarregar as alterações externas.");
        gtk_dialog_add_button(dialog, "Substituir ficheiro", 1); gtk_dialog_add_button(dialog, "Recarregar ficheiro", 2);
        gtk_dialog_add_button(dialog, "Cancelar", 3); state.external_conflict_dialog = dialog;
        g_signal_connect_data(dialog, "response", (void *)external_conflict_response, state.active, NULL, 0);
        gtk_window_present(dialog);
        return true;
    }
    char *content = read_file(state.active->markdown_path); state.loading = true;
    gtk_text_buffer_set_text(state.editor_buffer, content, -1); highlight_editor(content); update_views(content); state.loading = false;
    gtk_label_set_text(state.status, "Alterações externas recarregadas"); free(content); return true;
}
void quit_response(Ptr dialog, int response, Ptr unused) {
    (void)unused;
    if (response == 1) {
        for (Note *note = state.notes; note; note = note->next) if (note->dirty) { select_note(NULL, note); save_active(NULL); }
        state.force_quit = true; gtk_window_destroy(state.window);
    } else if (response == 2) {
        for (Note *note = state.notes; note; note = note->next) if (note->dirty) {
            note->dirty = false; discard_note_draft(note); clear_recovery(note);
        }
        state.force_quit = true; gtk_window_destroy(state.window);
    }
    gtk_window_destroy(dialog);
}
gboolean close_requested(Ptr window, Ptr unused) {
    (void)window; (void)unused;
    dismiss_active_popover();
    bool dirty = false; for (Note *note = state.notes; note; note = note->next) if (note->dirty) { dirty = true; break; }
    if (state.force_quit || !dirty) return false;
    Ptr dialog = gtk_message_dialog_new(state.window, GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, 0,
        "Guardar alterações antes de sair?");
    gtk_message_dialog_format_secondary_text(dialog, "%s", "Existem alterações que ainda não foram guardadas.");
    gtk_dialog_add_button(dialog, "Guardar e sair", 1); gtk_dialog_add_button(dialog, "Não guardar", 2);
    gtk_dialog_add_button(dialog, "Cancelar", 3);
    g_signal_connect_data(dialog, "response", (void *)quit_response, NULL, NULL, 0); gtk_window_present(dialog);
    return true;
}
char *buffer_text(Ptr buffer) {
    TextIter start, end; gtk_text_buffer_get_bounds(buffer, &start, &end);
    return gtk_text_buffer_get_text(buffer, &start, &end, true);
}
gboolean run_ui_assertions(Ptr unused) {
    (void)unused; const char *needle = "Conteúdo dividido";
    struct { const char *name; Ptr buffer; bool exact; } checks[] = {
        {"editor", state.editor_buffer, true}, {"dividido-fonte", state.split_buffer, true},
        {"dividido-preview", state.split_preview_buffer, false},
        {"numeros-linha", state.line_buffer, false}
    };
    bool ok = state.window && state.sidebar && state.tag_box && state.tabbar && state.title && state.tags && state.linksbar && state.stats && state.line_badge;
    printf("UI widgets: %s\n", ok ? "OK" : "FALHOU");
    char original_theme[32]; snprintf(original_theme, sizeof original_theme, "%s", state.theme);
    const char *theme_ids[] = {"white", "black", "system", "monokai", "tokyo-night", "dracula", "solarized-light"};
    bool themes_ok = state.syntax_heading && state.syntax_emphasis && state.syntax_code && state.syntax_link && state.syntax_list && state.syntax_quote;
    for (size_t index = 0; index < sizeof theme_ids / sizeof theme_ids[0]; index++) {
        apply_theme(theme_ids[index]); themes_ok = themes_ok && strcmp(state.theme, theme_ids[index]) == 0 && state.theme_provider;
    }
    apply_theme(original_theme);
    printf("UI temas-apenas-markdown-paletas: %s\n", themes_ok ? "OK" : "FALHOU"); ok = ok && themes_ok;
    bool tree_ok = state.active_notebook && state.active_notebook->expander;
    if (tree_ok) {
        gtk_expander_set_expanded(state.active_notebook->expander, false);
        tree_ok = !gtk_expander_get_expanded(state.active_notebook->expander) && !state.active_notebook->expanded;
        gtk_expander_set_expanded(state.active_notebook->expander, true);
        tree_ok = tree_ok && gtk_expander_get_expanded(state.active_notebook->expander) && state.active_notebook->expanded;
    }
    printf("UI arvore-notebooks-expandir: %s\n", tree_ok ? "OK" : "FALHOU"); ok = ok && tree_ok;
    gtk_editable_set_text(state.tag_input, "nova"); g_signal_emit_by_name(state.tag_input, "activate");
    bool tag_chips_ok = strstr(gtk_editable_get_text(state.tags), "nova") != NULL && gtk_widget_get_first_child(state.note_tags_box);
    Ptr last_chip = gtk_widget_get_first_child(state.note_tags_box), next_chip;
    while (last_chip && (next_chip = gtk_widget_get_next_sibling(last_chip))) last_chip = next_chip;
    if (last_chip) g_signal_emit_by_name(last_chip, "clicked");
    tag_chips_ok = tag_chips_ok && strstr(gtk_editable_get_text(state.tags), "nova") == NULL;
    printf("UI tags-chips-adicionar-remover: %s\n", tag_chips_ok ? "OK" : "FALHOU"); ok = ok && tag_chips_ok;
    g_signal_emit_by_name(state.mode_editor, "clicked");
    bool mode_clicks_ok = strcmp(gtk_stack_get_visible_child_name(state.mode_stack), "editor") == 0;
    g_signal_emit_by_name(state.mode_visual, "clicked");
    mode_clicks_ok = mode_clicks_ok && strcmp(gtk_stack_get_visible_child_name(state.mode_stack), "visual") == 0 && state.block_count > 0;
    g_signal_emit_by_name(state.mode_split, "clicked");
    char *mode_preview = buffer_text(state.split_preview_buffer);
    mode_clicks_ok = mode_clicks_ok && strcmp(gtk_stack_get_visible_child_name(state.mode_stack), "split") == 0 && mode_preview[0] != '\0';
    g_free(mode_preview); printf("UI cliques-modos-editor-visual-dividido: %s\n", mode_clicks_ok ? "OK" : "FALHOU"); ok = ok && mode_clicks_ok;
    bool sidebar_ok = state.sidebar_panel && gtk_widget_get_visible(state.sidebar_panel);
    toggle_sidebar(NULL, NULL); sidebar_ok = sidebar_ok && !gtk_widget_get_visible(state.sidebar_panel) && state.sidebar_hidden;
    toggle_sidebar(NULL, NULL); sidebar_ok = sidebar_ok && gtk_widget_get_visible(state.sidebar_panel) && !state.sidebar_hidden;
    printf("UI ocultar-biblioteca: %s\n", sidebar_ok ? "OK" : "FALHOU"); ok = ok && sidebar_ok;
    int split_width = state.split_source_view ? gtk_widget_get_width(state.split_source_view) : 0;
    bool split_visible = split_width > 100; printf("UI dividido-largura (%dpx): %s\n", split_width, split_visible ? "OK" : "FALHOU");
    ok = ok && split_visible;
    for (size_t index = 0; index < sizeof checks / sizeof checks[0]; index++) {
        char *text = checks[index].buffer ? buffer_text(checks[index].buffer) : NULL;
        bool passed = text && (checks[index].exact ? strstr(text, needle) != NULL : text[0] != '\0');
        printf("UI %s: %s\n", checks[index].name, passed ? "OK" : "FALHOU"); ok = ok && passed;
        if (text) g_free(text);
    }
    gtk_text_buffer_set_text(state.editor_buffer, "linha um\nlinha dois\nlinha três", -1);
    TextIter active_cursor; gtk_text_buffer_get_iter_at_offset(state.editor_buffer, &active_cursor, 10);
    gtk_text_buffer_select_range(state.editor_buffer, &active_cursor, &active_cursor);
    bool active_line_ok = state.active_line == 2;
    printf("UI linha-ativa: %s\n", active_line_ok ? "OK" : "FALHOU"); ok = ok && active_line_ok;
    gtk_text_buffer_set_text(state.split_buffer, "## Edição dividida\n\nTexto sincronizado", -1);
    char *split_editor_text = editor_text(), *split_preview_text = buffer_text(state.split_preview_buffer);
    bool split_edit_ok = strstr(split_editor_text, "Texto sincronizado") && strstr(split_preview_text, "Edição dividida");
    printf("UI dividido-edicao-live: %s\n", split_edit_ok ? "OK" : "FALHOU"); ok = ok && split_edit_ok;
    g_free(split_editor_text); g_free(split_preview_text);
    gtk_text_buffer_set_text(state.split_buffer,
        "Texto **forte**, _itálico_, ~~rasurado~~, <span style=\"color:#0969da\">azul</span>, `código` e [site](https://example.com).\n\n| Nome | Valor |\n| --- | --- |\n| Um | Dois |", -1);
    char *rich_preview = buffer_text(state.split_preview_buffer);
    bool rich_preview_ok = strstr(rich_preview, "forte") && strstr(rich_preview, "Nome") && strstr(rich_preview, "│") &&
        strstr(rich_preview, "azul") && !strstr(rich_preview, "**") && !strstr(rich_preview, "~~") &&
        !strstr(rich_preview, "<span") && !strstr(rich_preview, "`código`") && !strstr(rich_preview, "](https://");
    printf("UI preview-markdown-rico: %s\n", rich_preview_ok ? "OK" : "FALHOU"); ok = ok && rich_preview_ok; g_free(rich_preview);
    Note *initial = state.active;
    gtk_editable_set_text(state.search, "não-existe");
    /* GtkSearchEntry deliberately debounces "search-changed".  Rebuild here so
       the self-test verifies the filtering result, not the debounce timer. */
    rebuild_sidebar();
    bool search_ok = initial && initial->button == NULL; printf("UI pesquisa-filtro: %s\n", search_ok ? "OK" : "FALHOU"); ok = ok && search_ok;
    gtk_editable_set_text(state.search, ""); snprintf(state.selected_tag, sizeof state.selected_tag, "teste"); rebuild_sidebar();
    bool tag_ok = initial->button != NULL; snprintf(state.selected_tag, sizeof state.selected_tag, "ausente"); rebuild_sidebar();
    tag_ok = tag_ok && initial->button == NULL; state.selected_tag[0] = '\0'; rebuild_sidebar();
    printf("UI filtro-tags: %s\n", tag_ok ? "OK" : "FALHOU"); ok = ok && tag_ok;

    state.loading = true; gtk_text_buffer_set_text(state.editor_buffer, "texto", -1); state.loading = false;
    TextIter selection_start, selection_end; gtk_text_buffer_get_iter_at_offset(state.editor_buffer, &selection_start, 0);
    gtk_text_buffer_get_iter_at_offset(state.editor_buffer, &selection_end, 5);
    gtk_text_buffer_select_range(state.editor_buffer, &selection_start, &selection_end); insert_markdown(NULL, "**|**");
    char *formatted = editor_text(); bool format_ok = strcmp(formatted, "**texto**") == 0; g_free(formatted);
    state.loading = true; gtk_text_buffer_set_text(state.editor_buffer, "título", -1); state.loading = false;
    gtk_text_buffer_get_bounds(state.editor_buffer, &selection_start, &selection_end);
    gtk_text_buffer_select_range(state.editor_buffer, &selection_start, &selection_end); gtk_drop_down_set_selected(state.format_heading, 4);
    formatted = editor_text(); format_ok = format_ok && strcmp(formatted, "#### título") == 0; g_free(formatted);
    state.loading = true; gtk_text_buffer_set_text(state.editor_buffer, "apagado", -1); state.loading = false;
    gtk_text_buffer_get_bounds(state.editor_buffer, &selection_start, &selection_end);
    gtk_text_buffer_select_range(state.editor_buffer, &selection_start, &selection_end); insert_markdown(NULL, "~~|~~");
    formatted = editor_text(); format_ok = format_ok && strcmp(formatted, "~~apagado~~") == 0; g_free(formatted);
    state.loading = true; gtk_text_buffer_set_text(state.editor_buffer, "colorido", -1); state.loading = false;
    gtk_text_buffer_get_bounds(state.editor_buffer, &selection_start, &selection_end);
    gtk_text_buffer_select_range(state.editor_buffer, &selection_start, &selection_end); gtk_drop_down_set_selected(state.format_color, 5);
    formatted = editor_text(); format_ok = format_ok && strcmp(formatted, "<span style=\"color:#0969da\">colorido</span>") == 0; g_free(formatted);
    printf("UI formatacao-selecao: %s\n", format_ok ? "OK" : "FALHOU"); ok = ok && format_ok;

    gtk_text_buffer_set_text(state.editor_buffer, "Alpha beta ALPHA", -1); show_find(NULL, NULL);
    gtk_editable_set_text(state.find_entry, "alpha"); gtk_editable_set_text(state.replace_entry, "omega"); find_next(NULL, NULL);
    TextIter found_start, found_end; bool find_ok = gtk_text_buffer_get_selection_bounds(state.editor_buffer, &found_start, &found_end);
    char *found = find_ok ? gtk_text_buffer_get_text(state.editor_buffer, &found_start, &found_end, true) : NULL;
    find_ok = find_ok && found && strcasecmp(found, "alpha") == 0; if (found) g_free(found);
    replace_current(NULL, NULL); replace_all(NULL, NULL); char *replaced = editor_text();
    bool replace_ok = strcmp(replaced, "omega beta omega") == 0; g_free(replaced); close_find(NULL, NULL);
    printf("UI pesquisar-substituir: %s\n", find_ok && replace_ok ? "OK" : "FALHOU"); ok = ok && find_ok && replace_ok;

    size_t books_before = 0; for (Notebook *book = state.notebooks; book; book = book->next) books_before++;
    new_notebook(NULL, NULL); size_t books_after = 0; for (Notebook *book = state.notebooks; book; book = book->next) books_after++;
    Notebook *created_book = state.active_notebook;
    bool notebook_ok = books_after == books_before + 1 && created_book && is_directory(created_book->path) && created_book->button;
    if (initial && initial->notebook && initial->notebook->button) {
        g_signal_emit_by_name(initial->notebook->button, "clicked"); notebook_ok = notebook_ok && state.active_notebook == initial->notebook;
        g_signal_emit_by_name(created_book->button, "clicked"); notebook_ok = notebook_ok && state.active_notebook == created_book;
    }
    printf("UI criar-notebook: %s\n", notebook_ok ? "OK" : "FALHOU"); ok = ok && notebook_ok;
    create_note_in_notebook(NULL, created_book);
    bool note_ok = state.active && state.active->notebook == created_book && access(state.active->markdown_path, R_OK) == 0 && state.active->opened;
    Note *created_note = state.active;
    if (initial && initial->button && created_note && created_note->button) {
        g_signal_emit_by_name(initial->button, "clicked"); note_ok = note_ok && state.active == initial;
        g_signal_emit_by_name(created_note->button, "clicked"); note_ok = note_ok && state.active == created_note;
    }
    gtk_editable_set_text(state.tags, "ui, actions"); gtk_text_buffer_set_text(state.editor_buffer, "# Ação\n\nConteúdo\n", -1); save_active(NULL);
    char *saved = read_file(state.active->markdown_path); note_ok = note_ok && strstr(saved, "Conteúdo") && note_has_tag(state.active, "actions"); free(saved);
    printf("UI criar-guardar-nota: %s\n", note_ok ? "OK" : "FALHOU"); ok = ok && note_ok;
    bool previous_autosave = state.autosave; state.autosave = false;
    gtk_editable_set_text(state.title, "Título rascunho A"); gtk_editable_set_text(state.tags, "draft-tag");
    gtk_text_buffer_set_text(state.editor_buffer, "# Rascunho A não gravado\n", -1);
    select_note(NULL, initial);
    char *created_disk = read_file(created_note->markdown_path);
    bool drafts_ok = created_note->dirty && created_note->draft && strstr(created_note->draft, "Rascunho A") &&
        !strstr(created_disk, "Rascunho A"); free(created_disk);
    gtk_text_buffer_set_text(state.editor_buffer, "# Rascunho B não gravado\n", -1);
    select_note(NULL, created_note); char *draft_a = editor_text();
    drafts_ok = drafts_ok && initial->dirty && initial->draft && strstr(initial->draft, "Rascunho B") && strstr(draft_a, "Rascunho A") &&
        strcmp(gtk_editable_get_text(state.title), "Título rascunho A") == 0 &&
        strcmp(gtk_editable_get_text(state.tags), "draft-tag") == 0 && note_named("Título rascunho A") == created_note &&
        note_has_tag(created_note, "draft-tag");
    g_free(draft_a); save_active(NULL); drafts_ok = drafts_ok && !created_note->dirty && initial->dirty;
    select_note(NULL, initial); char *draft_b = editor_text();
    drafts_ok = drafts_ok && strstr(draft_b, "Rascunho B"); g_free(draft_b); save_active(NULL);
    drafts_ok = drafts_ok && !initial->dirty; select_note(NULL, created_note); state.autosave = previous_autosave;
    printf("UI rascunhos-multiplas-abas-sem-autosave: %s\n", drafts_ok ? "OK" : "FALHOU"); ok = ok && drafts_ok;
    bool close_save_autosave = state.autosave;
    state.autosave = false;
    select_note(NULL, initial);
    gtk_editable_set_text(state.title, "Fecho guardado");
    gtk_editable_set_text(state.tags, "close-save");
    gtk_text_buffer_set_text(state.editor_buffer, "# Fecho guardado\n\nPersistido\n", -1);
    select_note(NULL, created_note);
    char *close_before = read_file(initial->markdown_path);
    bool close_save_ok = initial->dirty && initial->draft && strstr(initial->draft, "Fecho guardado") &&
        !strstr(close_before, "Persistido");
    g_free(close_before);
    close_tab(NULL, initial); close_save_ok = close_save_ok && state.close_note_dialog != NULL;
    if (state.close_note_dialog) g_signal_emit_by_name(state.close_note_dialog, "response", 1);
    char *close_after = read_file(initial->markdown_path);
    close_save_ok = close_save_ok && state.close_note_dialog == NULL && state.active == created_note &&
        created_note->opened && !initial->opened && !initial->dirty && !initial->draft &&
        strstr(close_after, "Persistido");
    g_free(close_after); state.autosave = close_save_autosave;
    printf("UI fechar-nota-inativa-guardar: %s\n", close_save_ok ? "OK" : "FALHOU"); ok = ok && close_save_ok;
    note_color_changed(NULL, "#d1242f"); notebook_color_changed(NULL, "#0969da");
    char note_metadata_path[PATH_MAX], book_metadata_path[PATH_MAX];
    snprintf(note_metadata_path, sizeof note_metadata_path, "%s/.note.json", state.active->directory);
    snprintf(book_metadata_path, sizeof book_metadata_path, "%s/.notebook.json", state.active_notebook->path);
    char *note_metadata = read_file(note_metadata_path), *book_metadata = read_file(book_metadata_path);
    bool colors_ok = strcmp(state.active->color, "#d1242f") == 0 && strcmp(state.active_notebook->color, "#0969da") == 0 &&
        strstr(note_metadata, "#d1242f") && strstr(book_metadata, "#0969da") &&
        state.active->button && gtk_widget_has_css_class(state.active->button, "color-red") &&
        state.active->menu_button && gtk_widget_has_css_class(state.active->menu_button, "color-red") &&
        state.context_menu_count >= 2;
    if (state.active_notebook->menu_button) g_signal_emit_by_name(state.active_notebook->menu_button, "clicked");
    bool menus_ok = state.active_popover != NULL; dismiss_active_popover(); menus_ok = menus_ok && state.active_popover == NULL;
    if (state.active && state.active->menu_button) g_signal_emit_by_name(state.active->menu_button, "clicked");
    menus_ok = menus_ok && state.active_popover != NULL; dismiss_active_popover(); menus_ok = menus_ok && state.active_popover == NULL;
    if (state.active_notebook && state.active_notebook->context_gesture)
        g_signal_emit_by_name(state.active_notebook->context_gesture, "pressed", 1, 4.0, 4.0);
    menus_ok = menus_ok && state.active_popover != NULL; dismiss_active_popover();
    if (state.active && state.active->context_gesture)
        g_signal_emit_by_name(state.active->context_gesture, "pressed", 1, 4.0, 4.0);
    menus_ok = menus_ok && state.active_popover != NULL; dismiss_active_popover();
    colors_ok = colors_ok && menus_ok;
    free(note_metadata); free(book_metadata);
    printf("UI cores-e-menus-contexto: %s\n", colors_ok ? "OK" : "FALHOU"); ok = ok && colors_ok;
    Notebook *move_source = state.active->notebook, *move_target = initial ? initial->notebook : NULL;
    char original_directory[PATH_MAX]; snprintf(original_directory, sizeof original_directory, "%s", state.active->directory);
    bool move_ok = move_source && move_target && move_source != move_target && move_note_between_notebooks(state.active, move_target);
    move_ok = move_ok && state.active->notebook == move_target && access(state.active->directory, R_OK) == 0 && access(original_directory, F_OK) != 0;
    move_ok = move_ok && move_note_between_notebooks(state.active, move_source) && state.active->notebook == move_source;
    printf("UI arrastar-mover-nota: %s\n", move_ok ? "OK" : "FALHOU"); ok = ok && move_ok;
    bool reorder_ok = initial && reorder_note_before(state.active, initial);
    bool found_adjacency = false;
    for (Note *note = state.notes; note && note->next; note = note->next)
        if (note == state.active && note->next == initial) found_adjacency = true;
    reorder_ok = reorder_ok && found_adjacency && state.active->notebook == initial->notebook;
    reorder_ok = reorder_ok && move_note_between_notebooks(state.active, move_source);
    printf("UI arrastar-reordenar-nota: %s\n", reorder_ok ? "OK" : "FALHOU"); ok = ok && reorder_ok;

    gtk_text_buffer_set_text(state.editor_buffer, "# Visual\n\n- [ ] Tarefa\n\n```c\nlinha 1\n\nlinha 2\n```\n\n| A | B |\n| --- | --- |\n| 1 | 2 |", -1);
    show_block_editor(NULL, NULL); bool blocks_ok = state.block_box && state.block_count == 4;
    if (blocks_ok) {
        char *code_block = buffer_text(state.block_buffers[2]), *table_block = buffer_text(state.block_buffers[3]);
        blocks_ok = strstr(code_block, "linha 1\n\nlinha 2") && strstr(table_block, "| --- | --- |");
        g_free(code_block); g_free(table_block);
    }
    if (blocks_ok && state.block_checks[1]) {
        gtk_check_button_set_active(state.block_checks[1], true);
        char *task_block = buffer_text(state.block_buffers[1]);
        char *task_markdown = editor_text();
        blocks_ok = strcmp(task_block, "Tarefa") == 0 && strstr(task_markdown, "- [x] Tarefa");
        g_free(task_block); g_free(task_markdown);
    } else blocks_ok = false;
    if (blocks_ok && state.block_headings[0]) {
        gtk_drop_down_set_selected(state.block_headings[0], 2);
        char *heading_block = buffer_text(state.block_buffers[0]);
        char *heading_markdown = editor_text();
        blocks_ok = strcmp(heading_block, "Visual") == 0 && strstr(heading_markdown, "### Visual");
        g_free(heading_block); g_free(heading_markdown);
    } else blocks_ok = false;
    if (blocks_ok && state.block_is_code[2] && state.block_languages[2]) {
        char *code_content = buffer_text(state.block_buffers[2]);
        blocks_ok = strstr(code_content, "linha 1\n\nlinha 2") && !strstr(code_content, "```") &&
            strcmp(gtk_editable_get_text(state.block_languages[2]), "c") == 0;
        g_free(code_content); gtk_editable_set_text(state.block_languages[2], "rust");
        char *code_markdown = editor_text(); blocks_ok = blocks_ok && strstr(code_markdown, "```rust\nlinha 1"); g_free(code_markdown);
    } else blocks_ok = false;
    if (blocks_ok && state.block_tables[3] && state.block_tables[3]->rows == 2 && state.block_tables[3]->columns == 2) {
        gtk_editable_set_text(state.block_tables[3]->cells[1][0], "Editado na grelha");
        char *grid_markdown = buffer_text(state.block_buffers[3]);
        blocks_ok = strstr(grid_markdown, "| Editado na grelha | 2 |") != NULL; g_free(grid_markdown);
    } else blocks_ok = false;
    if (blocks_ok) {
        table_action(NULL, (Ptr)(3 * 2)); table_action(NULL, (Ptr)(3 * 2 + 1));
        char *expanded_table = buffer_text(state.block_buffers[3]); size_t separators = 0;
        for (char *marker = expanded_table; (marker = strstr(marker, "---")); marker += 3) separators++;
        blocks_ok = separators == 3 && strstr(expanded_table, "\n|  |") != NULL; g_free(expanded_table);
    }
    if (blocks_ok) {
        gtk_text_buffer_set_text(state.block_buffers[0], "Alterado visualmente", -1);
        char *synchronized = editor_text();
        blocks_ok = strstr(synchronized, "### Alterado visualmente") != NULL; g_free(synchronized);
    }
    printf("UI editor-blocos: %s\n", blocks_ok ? "OK" : "FALHOU"); ok = ok && blocks_ok;

    gtk_text_buffer_set_text(state.editor_buffer, "- Item\n\n2. Segundo\n\n> Uma\n> Duas\n\n---", -1);
    show_block_editor(NULL, NULL);
    bool structured_ok = state.block_count == 4 && state.block_is_bullet[0] && state.block_is_numbered[1] &&
        state.block_is_quote[2] && state.block_is_divider[3];
    if (structured_ok) {
        char *bullet = buffer_text(state.block_buffers[0]), *numbered = buffer_text(state.block_buffers[1]);
        char *quote = buffer_text(state.block_buffers[2]);
        structured_ok = strcmp(bullet, "Item") == 0 && strcmp(numbered, "Segundo") == 0 && strcmp(quote, "Uma\nDuas") == 0;
        g_free(bullet); g_free(numbered); g_free(quote);
        gtk_text_buffer_set_text(state.block_buffers[0], "Alterado", -1);
        gtk_text_buffer_set_text(state.block_buffers[2], "Linha A\nLinha B", -1);
        char *serialized = editor_text();
        structured_ok = structured_ok && strstr(serialized, "- Alterado") && strstr(serialized, "2. Segundo") &&
            strstr(serialized, "> Linha A\n> Linha B") && strstr(serialized, "\n\n---"); g_free(serialized);
    }
    printf("UI blocos-listas-citacao-estruturados: %s\n", structured_ok ? "OK" : "FALHOU"); ok = ok && structured_ok;

    char fake_image[PATH_MAX]; snprintf(fake_image, sizeof fake_image, "%s/test-image.png", state.ui_test_dir);
    char source_icon[PATH_MAX]; snprintf(source_icon, sizeof source_icon, "%s/NoteMD-AppIcon-1024.png", state.data_dir);
    if (access(source_icon, R_OK) != 0) snprintf(source_icon, sizeof source_icon, "%s/../Assets/NoteMD-AppIcon-1024.png", state.data_dir);
    bool image_ok = copy_file(source_icon, fake_image) && store_image(fake_image);
    char *with_image = editor_text(); image_ok = image_ok && strstr(with_image, "assets/image-") != NULL; g_free(with_image);
    image_ok = image_ok && state.visual_image_count > 0;
    size_t image_block = 0; while (image_block < state.block_count && !state.block_is_image[image_block]) image_block++;
    image_ok = image_ok && image_block < state.block_count && state.block_image_alt[image_block] && state.block_image_location[image_block];
    if (image_ok) {
        gtk_editable_set_text(state.block_image_alt[image_block], "Descrição alterada");
        char *image_markdown = editor_text();
        image_ok = strstr(image_markdown, "![Descrição alterada](assets/image-") != NULL; g_free(image_markdown);
    }
    printf("UI imagem-assets (%zu widgets): %s\n", state.visual_image_count, image_ok ? "OK" : "FALHOU"); ok = ok && image_ok;

    char external_path[PATH_MAX]; snprintf(external_path, sizeof external_path, "%s/external.md", state.ui_test_dir);
    write_atomic(external_path, "# Externo\n"); Ptr external_file = g_file_new_for_path(external_path);
    Ptr files[] = {external_file}; open_application_files(state.app, files, 1, "", NULL); g_object_unref(external_file);
    bool external_ok = state.active && state.active->external && strcmp(state.active->markdown_path, external_path) == 0;
    if (external_ok) {
        gtk_text_buffer_set_text(state.editor_buffer, "# Versão local\n", -1);
        write_atomic(external_path, "# Versão externa atualizada\n"); state.active->external_mtime = 0;
        poll_external_file(NULL); external_ok = state.external_conflict_dialog != NULL;
        if (state.external_conflict_dialog) g_signal_emit_by_name(state.external_conflict_dialog, "response", 2);
        char *reloaded = editor_text(); external_ok = external_ok && strstr(reloaded, "Versão externa atualizada") &&
            state.active && !state.active->dirty && state.external_conflict_dialog == NULL; g_free(reloaded);
    }
    printf("UI abrir-ficheiro-desktop: %s\n", external_ok ? "OK" : "FALHOU"); ok = ok && external_ok;
    Note *close_candidate = state.active;
    gtk_text_buffer_set_text(state.editor_buffer, "# Alteração ainda por guardar\n", -1);
    bool close_ok = close_candidate && close_candidate->dirty && close_candidate->dirty_indicator &&
        gtk_widget_get_visible(close_candidate->dirty_indicator);
    close_tab(NULL, close_candidate); close_ok = close_ok && state.close_note_dialog != NULL;
    if (state.close_note_dialog) g_signal_emit_by_name(state.close_note_dialog, "response", 3);
    close_ok = close_ok && state.close_note_dialog == NULL && state.active == close_candidate && close_candidate->opened;
    save_active(NULL); close_ok = close_ok && !close_candidate->dirty &&
        close_candidate->dirty_indicator && !gtk_widget_get_visible(close_candidate->dirty_indicator);
    printf("UI fechar-nota-alterada-cancelar: %s\n", close_ok ? "OK" : "FALHOU"); ok = ok && close_ok;
    if (created_note) {
        select_note(NULL, created_note);
        gtk_text_buffer_set_text(state.editor_buffer, "# Versão atual de teste\n", -1);
        save_active(NULL);
    }
    show_history(NULL, NULL);
    char *saved_version = state.history_saved_buffer ? buffer_text(state.history_saved_buffer) : NULL;
    char *current_version = state.history_current_buffer ? buffer_text(state.history_current_buffer) : NULL;
    bool history_ok = state.history_window && state.history_list && gtk_widget_get_first_child(state.history_list) &&
        saved_version && saved_version[0] && current_version && strstr(current_version, "Versão atual de teste");
    printf("UI historico-comparacao-versao-atual: %s\n", history_ok ? "OK" : "FALHOU"); ok = ok && history_ok;
    g_free(saved_version); g_free(current_version);
    if (state.history_window) gtk_window_destroy(state.history_window);
    state.history_window = state.history_list = state.history_saved_buffer = state.history_current_buffer = NULL;
    bool wiki_ok = created_note != NULL;
    if (wiki_ok) {
        select_note(NULL, created_note);
        gtk_text_buffer_set_text(state.editor_buffer, "# Ligações\n\n[[Nota criada por ligação]]\n[[Nota criada por ligação]]\n", -1);
        Ptr missing_link = gtk_widget_get_first_child(state.linksbar);
        wiki_ok = missing_link && gtk_widget_get_next_sibling(missing_link) == NULL;
        if (missing_link) g_signal_emit_by_name(missing_link, "clicked");
        Note *linked = note_named("Nota criada por ligação");
        char *linked_text = linked ? read_file(linked->markdown_path) : NULL;
        Ptr backlink = gtk_widget_get_first_child(state.linksbar);
        wiki_ok = wiki_ok && linked && state.active == linked && linked_text &&
            strstr(linked_text, "# Nota criada por ligação") && backlink;
        free(linked_text);
    }
    printf("UI ligacao-wiki-criar-deduplicar-backlink: %s\n", wiki_ok ? "OK" : "FALHOU"); ok = ok && wiki_ok;
    state.ui_test_failed = !ok; g_application_quit(state.app); return false;
}
gboolean run_empty_ui_assertions(Ptr unused) {
    (void)unused;
    bool ok = !state.notebooks && !state.notes && !state.active && state.side_new_note;
    printf("UI vazio-estado-inicial: %s\n", ok ? "OK" : "FALHOU");
    g_signal_emit_by_name(state.mode_split, "clicked");
    bool split_without_note = strcmp(gtk_stack_get_visible_child_name(state.mode_stack), "split") == 0;
    printf("UI vazio-clique-dividido: %s\n", split_without_note ? "OK" : "FALHOU"); ok = ok && split_without_note;
    g_signal_emit_by_name(state.side_new_note, "clicked");
    bool created = state.notebooks && state.active_notebook && state.active && state.active->notebook == state.active_notebook &&
        access(state.active->markdown_path, R_OK) == 0;
    printf("UI vazio-nova-nota-cria-tudo: %s\n", created ? "OK" : "FALHOU"); ok = ok && created;
    if (created) {
        gtk_text_buffer_set_text(state.editor_buffer, "# Primeira nota\n\nTexto visível no dividido", -1);
        g_signal_emit_by_name(state.mode_split, "clicked"); char *preview = buffer_text(state.split_preview_buffer);
        bool visible = strstr(preview, "Primeira nota") && strstr(preview, "Texto visível no dividido");
        printf("UI vazio-dividido-com-conteudo: %s\n", visible ? "OK" : "FALHOU"); ok = ok && visible; g_free(preview);
    }
    state.ui_test_failed = !ok; g_application_quit(state.app); return false;
}
void open_application_files(Ptr app, Ptr *files, int count, const char *hint, Ptr unused) {
    (void)hint; (void)unused;
    if (!state.window) activate(app, NULL);
    Note *last = NULL;
    for (int index = 0; index < count; index++) {
        char *path = g_file_get_path(files[index]);
        if (path) {
            Note *opened = add_external_note(path); if (opened) last = opened;
            g_free(path);
        }
    }
    if (last) { rebuild_sidebar(); select_note(NULL, last); }
}

#include "internal.h"
#include "source-editor.h"

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
    size_t index = (size_t)user_data; if (index >= state.block_count) return;
    source_code_buffer(state.block_buffers[index], gtk_editable_get_text(entry));
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
    gtk_widget_set_size_request(picture, -1, 200); gtk_widget_add_css_class(picture, "visual-image"); return picture;
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
        Ptr card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2); gtk_widget_add_css_class(card, "block-card");
        Ptr header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        gtk_widget_add_css_class(header, "block-header");
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
            gtk_widget_add_css_class(card, "code-card");
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
        Ptr view = source_editor_new(); gtk_text_view_set_editable(view, true);
        gtk_source_buffer_set_style_scheme(gtk_text_view_get_buffer(view), source_scheme(source_is_light()));
        gtk_widget_add_css_class(view, "block-source");
        gtk_text_view_set_wrap_mode(view, GTK_WRAP_WORD_CHAR);
        gtk_text_view_set_monospace(view, state.block_is_code[block_index]);
        if (state.block_is_code[block_index]) gtk_widget_add_css_class(view, "block-code");
        if (state.block_is_heading[block_index]) gtk_widget_add_css_class(view, "block-heading");
        if (state.block_is_quote[block_index]) gtk_widget_add_css_class(card, "quote-card");
        gtk_widget_set_size_request(view, -1, 34); Ptr buffer = gtk_text_view_get_buffer(view);
        if (state.block_is_code[block_index]) source_code_buffer(buffer, code_language);
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
    if (strcmp(mode, "editor") == 0 && state.editor) gtk_widget_grab_focus(state.editor);
    else if (strcmp(mode, "split") == 0 && state.split_source_view) gtk_widget_grab_focus(state.split_source_view);
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
    apply_theme((const char *)user_data);
    if (button) {
        Ptr rows = gtk_widget_get_parent(gtk_widget_get_parent(button));
        for (Ptr row = gtk_widget_get_first_child(rows); row; row = gtk_widget_get_next_sibling(row)) {
            for (Ptr choice = gtk_widget_get_first_child(row); choice; choice = gtk_widget_get_next_sibling(choice)) {
                gtk_widget_remove_css_class(choice, "selected-setting"); gtk_widget_remove_css_class(choice, "setting-choice");
                gtk_widget_add_css_class(choice, choice == button ? "selected-setting" : "setting-choice");
            }
        }
    }
    gtk_label_set_text(state.status, tr("Tema aplicado"));
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

Ptr note_drag_prepare(Ptr source, double x, double y, Ptr user_data) {
    (void)source; (void)x; (void)y; Note *note = user_data;
    if (!note || note->external || !note->id[0]) return NULL;
    return gdk_content_provider_new_typed(g_type_from_name("gchararray"), note->id);
}

gboolean note_dropped_on_notebook(Ptr target, const void *value, double x, double y, Ptr user_data) {
    (void)target; (void)x; (void)y; const char *id = g_value_get_string(value); if (!id) return false;
    for (Note *note = state.notes; note; note = note->next)
        if (strcmp(note->id, id) == 0) return move_note_between_notebooks(note, user_data);
    return false;
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

gboolean note_dropped_on_note(Ptr target_widget, const void *value, double x, double y, Ptr user_data) {
    (void)target_widget; (void)x; (void)y; const char *id = g_value_get_string(value); if (!id) return false;
    Note *target = user_data;
    for (Note *source = state.notes; source; source = source->next)
        if (strcmp(source->id, id) == 0) return reorder_note_before(source, target);
    return false;
}

void export_notebook_response(Ptr dialog, int response, Ptr user_data) {
    Notebook *notebook = user_data;
    if (response == GTK_RESPONSE_ACCEPT && notebook) {
        Ptr file = gtk_file_chooser_get_file(dialog); char *path = file ? g_file_get_path(file) : NULL;
        if (path) {
            if (state.active && state.active->notebook == notebook) save_active(NULL);
            bool ok = export_notebook_archive(notebook, path);
            gtk_label_set_text(state.status, ok ? "Notebook exportado" : "Não foi possível exportar o notebook (verifique se o comando zip está instalado)");
            g_free(path);
        }
    }
    g_object_unref(dialog);
}

void export_selected_notebook(Ptr button, Ptr user_data) {
    (void)button; Notebook *notebook = user_data ? user_data : state.active_notebook;
    if (!notebook || notebook->external) return;
    Ptr dialog = gtk_file_chooser_native_new("Exportar notebook", state.window,
        GTK_FILE_CHOOSER_ACTION_SAVE, tr("Exportar"), tr("Cancelar"));
    char safe[256], filename[300]; safe_title(notebook->title, safe); snprintf(filename, sizeof filename, "%s.notemd.zip", safe);
    Ptr filter = g_object_ref_sink(gtk_file_filter_new()); gtk_file_filter_set_name(filter, "Notebook NoteMD (*.notemd.zip)");
    gtk_file_filter_add_pattern(filter, "*.notemd.zip"); gtk_file_filter_add_pattern(filter, "*.zip");
    gtk_file_filter_add_mime_type(filter, "application/zip");
    gtk_file_chooser_add_filter(dialog, filter); gtk_file_chooser_set_filter(dialog, filter); g_object_unref(filter);
    gtk_file_chooser_set_current_name(dialog, filename); gtk_native_dialog_set_modal(dialog, true);
    g_signal_connect_data(dialog, "response", (void *)export_notebook_response, notebook, NULL, 0); gtk_native_dialog_show(dialog);
}

void import_notebook_response(Ptr dialog, int response, Ptr unused) {
    (void)unused;
    if (response == GTK_RESPONSE_ACCEPT) {
        Ptr file = gtk_file_chooser_get_file(dialog); char *path = file ? g_file_get_path(file) : NULL;
        if (path) {
            if (state.active) save_active(NULL);
            char imported[PATH_MAX] = ""; bool ok = import_notebook_archive(path, imported);
            if (ok) {
                Notebook *notebook = load_imported_notebook(imported); state.active_notebook = notebook;
                rebuild_sidebar(); rebuild_tabs();
                Note *first = NULL; for (Note *note = state.notes; note; note = note->next) if (note->notebook == notebook) { first = note; break; }
                if (first) select_note(NULL, first);
                gtk_label_set_text(state.status, "Notebook importado");
            } else gtk_label_set_text(state.status, "Arquivo inválido ou não foi possível importar o notebook");
            g_free(path);
        }
    }
    g_object_unref(dialog);
}

void import_notebook(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    Ptr dialog = gtk_file_chooser_native_new("Importar notebook", state.window,
        GTK_FILE_CHOOSER_ACTION_OPEN, "Importar", tr("Cancelar"));
    Ptr filter = g_object_ref_sink(gtk_file_filter_new()); gtk_file_filter_set_name(filter, "Notebook NoteMD (*.notemd.zip, *.zip)");
    gtk_file_filter_add_pattern(filter, "*.notemd.zip"); gtk_file_filter_add_pattern(filter, "*.zip");
    gtk_file_filter_add_mime_type(filter, "application/zip");
    gtk_file_chooser_add_filter(dialog, filter); gtk_file_chooser_set_filter(dialog, filter); g_object_unref(filter);
    gtk_native_dialog_set_modal(dialog, true);
    g_signal_connect_data(dialog, "response", (void *)import_notebook_response, NULL, NULL, 0); gtk_native_dialog_show(dialog);
}

void show_preferences(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    Ptr window = gtk_window_new(); gtk_window_set_title(window, tr("Preferências"));
    gtk_window_set_default_size(window, 540, 400); gtk_window_set_transient_for(window, state.window);
    gtk_window_set_modal(window, true);
    Ptr box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10); margins(box, 20, 20);
    Ptr heading = gtk_label_new(tr("Aparência e armazenamento")); gtk_label_set_xalign(heading, 0.0f);
    gtk_widget_add_css_class(heading, "history-title"); gtk_box_append(box, heading);
    Ptr storage_label = gtk_label_new("Pasta das notas"); gtk_label_set_xalign(storage_label, 0.0f);
    gtk_widget_add_css_class(storage_label, "settings-label"); gtk_box_append(box, storage_label);
    Ptr storage = gtk_label_new(state.root); gtk_label_set_xalign(storage, 0.0f);
    gtk_label_set_ellipsize(storage, 1); gtk_label_set_max_width_chars(storage, 54);
    gtk_widget_set_tooltip_text(storage, state.root);
    gtk_widget_add_css_class(storage, "storage-path");
    Ptr storage_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_hexpand(storage, true); gtk_box_append(storage_row, storage);
    Ptr change = icon_button("folder-open-symbolic", tr("Selecionar pasta das notas…"));
    g_signal_connect_data(change, "clicked", (void *)choose_folder, NULL, NULL, 0);
    gtk_box_append(storage_row, change); gtk_box_append(box, storage_row);
    Ptr autosave = gtk_check_button_new_with_label(tr("Gravação automática"));
    gtk_check_button_set_active(autosave, state.autosave);
    g_signal_connect_data(autosave, "toggled", (void *)autosave_changed, NULL, NULL, 0); gtk_box_append(box, autosave);
    gtk_box_append(box, gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    Ptr themes_label = gtk_label_new(tr("Tema do editor")); gtk_label_set_xalign(themes_label, 0.0f);
    gtk_widget_add_css_class(themes_label, "settings-label"); gtk_box_append(box, themes_label);
    static const char *theme_ids[] = {"white", "black", "system", "monokai", "tokyo-night", "dracula", "solarized-light", "catppuccin-latte"};
    const char *theme_labels[] = {tr("Claro"), tr("Escuro"), tr("Sistema"), "Monokai", "Tokyo Night", "Dracula", "Solarized Light", "Catppuccin Latte"};
    Ptr theme_rows = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5); Ptr row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    for (size_t index = 0; index < sizeof theme_ids / sizeof theme_ids[0]; index++) {
        if (index && index % 3 == 0) { gtk_box_append(theme_rows, row); row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5); }
        Ptr choice = gtk_button_new_with_label("");
        Ptr tile = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        Ptr sample = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0); gtk_widget_set_halign(sample, GTK_ALIGN_CENTER);
        gtk_widget_add_css_class(sample, "palette-sample");
        char palette_class[64]; snprintf(palette_class, sizeof palette_class, "palette-%s", theme_ids[index]);
        gtk_widget_add_css_class(sample, palette_class);
        const char *swatch_classes[] = {"sample-sidebar", "sample-paper", "sample-accent"};
        for (size_t swatch = 0; swatch < sizeof swatch_classes / sizeof swatch_classes[0]; swatch++) {
            Ptr color = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
            gtk_widget_add_css_class(color, swatch_classes[swatch]); gtk_box_append(sample, color);
        }
        gtk_box_append(tile, sample); gtk_box_append(tile, gtk_label_new(theme_labels[index])); gtk_button_set_child(choice, tile);
        gtk_widget_add_css_class(choice, strcmp(state.theme, theme_ids[index]) == 0 ? "selected-setting" : "setting-choice");
        g_signal_connect_data(choice, "clicked", (void *)choose_theme, (Ptr)theme_ids[index], NULL, 0);
        gtk_box_append(row, choice);
    }
    gtk_box_append(theme_rows, row); gtk_box_append(box, theme_rows);
    Ptr font_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    Ptr font_labels = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    Ptr font_label = gtk_label_new(tr("Tamanho do texto")); gtk_label_set_xalign(font_label, 0.0f);
    gtk_widget_add_css_class(font_label, "settings-label"); gtk_box_append(font_labels, font_label);
    Ptr font_description = gtk_label_new(tr("Editor e pré-visualização")); gtk_label_set_xalign(font_description, 0.0f);
    gtk_widget_add_css_class(font_description, "settings-description"); gtk_box_append(font_labels, font_description);
    gtk_widget_set_hexpand(font_labels, true); gtk_box_append(font_row, font_labels);
    static const char *font_sizes[] = {"12 px", "13 px", "14 px", "15 px", "16 px", "17 px", "18 px", "19 px",
        "20 px", "21 px", "22 px", "23 px", "24 px", NULL};
    Ptr font_choice = gtk_drop_down_new_from_strings(font_sizes);
    gtk_drop_down_set_selected(font_choice, (guint)(state.editor_font_size - EDITOR_FONT_MIN));
    gtk_widget_set_tooltip_text(font_choice, tr("Tamanho do texto"));
    g_signal_connect_data(font_choice, "notify::selected", (void *)editor_font_changed, NULL, NULL, 0);
    gtk_box_append(font_row, font_choice); gtk_box_append(box, font_row);
    Ptr font_hint = gtk_label_new(tr("Aumentar: Ctrl++   ·   Diminuir: Ctrl+−   ·   Repor: Ctrl+0"));
    gtk_label_set_xalign(font_hint, 0.0f); gtk_widget_add_css_class(font_hint, "shortcut-hint"); gtk_box_append(box, font_hint);
    Ptr language_label = gtk_label_new("Idioma / Language / Langue"); gtk_label_set_xalign(language_label, 0.0f);
    gtk_widget_add_css_class(language_label, "settings-label"); gtk_box_append(box, language_label);
    Ptr languages = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    static const char *lang_ids[] = {"pt", "en", "fr"};
    const char *lang_labels[] = {"Português", "English", "Français"};
    for (size_t index = 0; index < sizeof lang_ids / sizeof lang_ids[0]; index++) {
        Ptr choice = gtk_button_new_with_label(lang_labels[index]);
        gtk_widget_add_css_class(choice, strcmp(state.language, lang_ids[index]) == 0 ? "selected-setting" : "setting-choice");
        g_signal_connect_data(choice, "clicked", (void *)language_changed, (Ptr)lang_ids[index], NULL, 0);
        gtk_box_append(languages, choice);
    }
    gtk_box_append(box, languages);
    Ptr scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(scroll, box); gtk_window_set_child(window, scroll); gtk_window_present(window);
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

void rename_selected_notebook(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    if (!state.active_notebook || state.active_notebook->external) return;
    RenameNotebookData *data = calloc(1, sizeof *data); data->notebook = state.active_notebook;
    data->window = gtk_window_new(); gtk_window_set_title(data->window, "Renomear notebook");
    gtk_window_set_default_size(data->window, 420, 150); gtk_window_set_transient_for(data->window, state.window);
    gtk_window_set_modal(data->window, true); Ptr box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8); margins(box, 16, 16);
    data->entry = gtk_entry_new(); gtk_editable_set_text(data->entry, data->notebook->title);
    Ptr apply = gtk_button_new_with_label("Renomear"); gtk_widget_add_css_class(apply, "suggested-action");
    g_signal_connect_data(apply, "clicked", (void *)rename_notebook_apply, data, NULL, 0);
    gtk_box_append(box, data->entry); gtk_box_append(box, apply); gtk_window_set_child(data->window, box); gtk_window_present(data->window);
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

void sort_title(Ptr button, Ptr unused) { (void)button; (void)unused; sort_notes(false); }
void sort_recent(Ptr button, Ptr unused) { (void)button; (void)unused; sort_notes(true); }

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

void move_note_up(Ptr button, Ptr unused) { (void)button; (void)unused; reorder_active_note(-1); }
void move_note_down(Ptr button, Ptr unused) { (void)button; (void)unused; reorder_active_note(1); }

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
                gtk_text_buffer_set_text(state.editor_buffer, "", -1); update_views(""); state.loading = false; rebuild_tabs(); }
        } else gtk_label_set_text(state.status, "Não foi possível mover o notebook para o Lixo");
    }
    gtk_window_destroy(dialog);
}

void delete_selected_notebook(Ptr button, Ptr unused) {
    (void)button; (void)unused; if (!state.active_notebook) return;
    Ptr dialog = gtk_message_dialog_new(state.window, GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING,
        GTK_BUTTONS_YES_NO, state.active_notebook->external ? "Fechar ficheiros externos?" : "Remover este notebook?");
    gtk_message_dialog_format_secondary_text(dialog, "%s", state.active_notebook->external
        ? "Os ficheiros originais não serão apagados."
        : "Todas as notas e imagens serão movidas para o Lixo.");
    g_signal_connect_data(dialog, "response", (void *)delete_notebook_response, state.active_notebook, NULL, 0);
    gtk_window_present(dialog);
}

Ptr context_action(const char *icon, const char *label, void *callback, Ptr data) {
    Ptr button = navigation_button(icon, label); gtk_widget_add_css_class(button, "context-action");
    g_signal_connect_data(button, "clicked", callback, data, NULL, 0); return button;
}

Ptr color_actions(bool notebook) {
    static const char *names[] = {"Sem cor", "Vermelho", "Laranja", "Amarelo", "Verde", "Azul", "Roxo"};
    static const char *values[] = {"", "#d1242f", "#bc4c00", "#9a6700", "#1a7f37", "#0969da", "#8250df"};
    static const char *classes[] = {"color-none", "color-red", "color-orange", "color-yellow", "color-green", "color-blue", "color-purple"};
    Ptr row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6); gtk_widget_add_css_class(row, "color-palette");
    const char *current = notebook ? state.active_notebook->color : state.active->color;
    for (size_t index = 0; index < sizeof values / sizeof values[0]; index++) {
        bool selected = !strcasecmp(current, values[index]);
        Ptr choice = icon_button(index ? (selected ? "object-select-symbolic" : "media-record-symbolic") : "edit-clear-symbolic", tr(names[index]));
        gtk_widget_add_css_class(choice, "color-swatch"); gtk_widget_add_css_class(choice, classes[index]);
        if (selected) gtk_widget_add_css_class(choice, "chosen-color");
        g_signal_connect_data(choice, "clicked", (void *)(notebook ? notebook_color_changed : note_color_changed),
            (Ptr)values[index], NULL, 0); gtk_box_append(row, choice);
    }
    return row;
}

void library_menu_present(ContextTarget *context, const void *point) {
    if (!context || !context->target) return;
    Ptr anchor = context->widget;
    Ptr popover = gtk_popover_new(); Ptr box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(popover, "library-menu");
    gtk_popover_set_has_arrow(popover, false);
    if (point) gtk_popover_set_pointing_to(popover, point);
    gtk_widget_add_css_class(box, "notemd-context-menu");
    if (context->notebook) {
        Notebook *book = context->target; state.active_notebook = book;
        Ptr header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_add_css_class(header, "context-header"); add_library_color_class(header, book->color);
        Ptr icon = gtk_image_new_from_icon_name(book->external ? "folder-remote-symbolic" : "folder-symbolic");
        Ptr heading = gtk_label_new(book->title); gtk_label_set_xalign(heading, 0.0f);
        gtk_label_set_ellipsize(heading, 3); gtk_label_set_max_width_chars(heading, 28);
        gtk_widget_set_tooltip_text(heading, book->title); add_library_color_class(heading, book->color);
        gtk_widget_add_css_class(heading, "context-title");
        gtk_box_append(header, icon); gtk_box_append(header, heading); gtk_box_append(box, header);
        gtk_box_append(box, gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
        if (!book->external) {
            gtk_box_append(box, context_action("document-new-symbolic", "Nova nota", (void *)create_note_in_notebook, book));
            gtk_box_append(box, context_action("document-properties-symbolic", "Criar a partir de template", (void *)show_templates, NULL));
            gtk_box_append(box, context_action("edit-rename-symbolic", "Renomear notebook", (void *)rename_selected_notebook, NULL));
            gtk_box_append(box, context_action("document-send-symbolic", "Exportar notebook…", (void *)export_selected_notebook, book));
            gtk_box_append(box, gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
            Ptr color_label = gtk_label_new(tr("Cor do notebook")); gtk_label_set_xalign(color_label, 0.0f);
            gtk_widget_add_css_class(color_label, "context-section"); gtk_box_append(box, color_label);
            gtk_box_append(box, color_actions(true));
            gtk_box_append(box, gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
            Ptr sort_label = gtk_label_new(tr("Ordenar notas")); gtk_label_set_xalign(sort_label, 0.0f);
            gtk_widget_add_css_class(sort_label, "context-section"); gtk_box_append(box, sort_label);
            gtk_box_append(box, context_action("view-sort-ascending-symbolic", "Ordenar por título", (void *)sort_title, NULL));
            gtk_box_append(box, context_action("document-open-recent-symbolic", "Mais recentes", (void *)sort_recent, NULL));
        }
        gtk_box_append(box, gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
        Ptr remove = context_action("user-trash-symbolic", book->external ? "Fechar ficheiros externos" : "Remover notebook",
            (void *)delete_selected_notebook, NULL); gtk_widget_add_css_class(remove, "destructive-action"); gtk_box_append(box, remove);
    } else {
        Note *note = context->target; select_note(NULL, note);
        Ptr heading = gtk_label_new(note_display_title(note)); gtk_label_set_xalign(heading, 0.0f);
        add_library_color_class(heading, note->color);
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
    margins(box, 6, 6); gtk_popover_set_child(popover, box); present_popover(popover, anchor);
}

void library_menu_clicked(Ptr button, Ptr user_data) {
    (void)button; library_menu_present(user_data, NULL);
}

Ptr library_menu_button(Ptr target, bool notebook) {
    Ptr button = icon_button("view-more-symbolic", notebook ? "Ações do notebook" : "Ações da nota");
    gtk_widget_add_css_class(button, "item-menu");
    ContextTarget *context = calloc(1, sizeof *context); context->widget = button; context->target = target; context->notebook = notebook;
    g_signal_connect_data(button, "clicked", (void *)library_menu_clicked, context, (void *)free_signal_data, 0);
    state.context_menu_count++; return button;
}

void library_context_pressed(Ptr gesture, int presses, double x, double y, Ptr user_data) {
    (void)gesture; (void)presses;
    struct { int x, y, width, height; } point = {(int)x, (int)y, 1, 1};
    library_menu_present(user_data, &point);
}

Ptr attach_library_context_menu(Ptr widget, Ptr target, bool notebook) {
    Ptr gesture = gtk_gesture_click_new(); gtk_gesture_single_set_button(gesture, 3);
    ContextTarget *context = calloc(1, sizeof *context);
    context->widget = widget; context->target = target; context->notebook = notebook;
    g_signal_connect_data(gesture, "pressed", (void *)library_context_pressed, context, (void *)free_signal_data, 0);
    gtk_widget_add_controller(widget, gesture);
    return gesture;
}

void show_export_menu(Ptr button, Ptr unused) {
    (void)unused; Ptr popover = gtk_popover_new(); Ptr box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(box, "notemd-context-menu");
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
    gtk_widget_set_size_request(picture, 240, 160); gtk_text_view_add_child_at_anchor(view, picture, anchor);
    gtk_text_buffer_get_end_iter(buffer, &end); gtk_text_buffer_insert(buffer, &end, "\n", 1); return true;
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
        if (state.links_scroll) gtk_widget_set_visible(state.links_scroll, false);
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
    if (state.links_scroll) gtk_widget_set_visible(state.links_scroll, visible_count > 0);
}

gboolean save_active(Ptr unused) {
    (void)unused;
    state.autosave_source = 0;
    if (!state.active) return 0;
    char *text = editor_text();
    update_views(text);
    bool ok = save_note_payload(state.active, text, gtk_editable_get_text(state.title), gtk_editable_get_text(state.tags), false);
    gtk_label_set_text(state.status, ok ? tr("Guardado") : tr("Erro ao guardar"));
    g_free(text);
    return 0;
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
    sync_heading_choice(buffer);
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

void content_changed(Ptr object, Ptr unused) {
    (void)object; (void)unused;
    if (state.loading || !state.active) return;
    state.active->dirty = true;
    if (state.active->dirty_indicator) gtk_widget_set_visible(state.active->dirty_indicator, true);
    if (state.autosave_source) g_source_remove(state.autosave_source);
    state.autosave_source = state.autosave ? g_timeout_add(1200, (void *)save_active, NULL) : 0;
    char *text = editor_text();
    free(state.active->draft); state.active->draft = strdup(text);
    snprintf(state.active->draft_title, sizeof state.active->draft_title, "%s", gtk_editable_get_text(state.title));
    snprintf(state.active->draft_tags, sizeof state.active->draft_tags, "%s", gtk_editable_get_text(state.tags));
    if (object == state.title || object == state.tags) {
        rebuild_sidebar();
        if (object == state.title) rebuild_tabs();
    }
    write_recovery(state.active, text);

    update_views(text);
    if (!state.block_syncing && object == state.editor_buffer) rebuild_block_editor();
    update_statistics(text);
    sync_heading_choice(state.split_origin ? state.split_buffer : state.editor_buffer);
    gtk_label_set_text(state.status, state.autosave ? tr("a guardar…") : tr("alterações por guardar")); g_free(text);
}

void select_note(Ptr button, Ptr user_data) {
    (void)button; Note *target = user_data; if (!target) return;
    ensure_metadata_loaded(target);
    if (state.active == target) {
        state.active_notebook = target->notebook;
        if (target->button) gtk_widget_add_css_class(target->button, "selected-item");
        return;
    }
    if (state.active && state.active->button) gtk_widget_remove_css_class(state.active->button, "selected-item");
    if (state.active) {
        if (state.autosave_source) { g_source_remove(state.autosave_source); state.autosave_source = 0; }
        if (state.autosave) save_active(NULL); else capture_active_draft();
    }
    state.active = target; state.loading = true;
    if (state.active && state.active->button) gtk_widget_add_css_class(state.active->button, "selected-item");
    state.active->opened = true; rebuild_tabs();
    state.active_notebook = state.active->notebook;
    char *content = state.active->dirty && state.active->draft ? strdup(state.active->draft) : read_file(state.active->markdown_path);
    gtk_editable_set_text(state.title, state.active->dirty && state.active->draft ? state.active->draft_title : state.active->title);
    gtk_editable_set_text(state.tags, state.active->dirty && state.active->draft ? state.active->draft_tags : state.active->tags);
    rebuild_note_tags(); gtk_editable_set_text(state.tag_input, "");
    gtk_text_buffer_set_text(state.editor_buffer, content, -1);

    update_views(content);
    rebuild_block_editor();
    update_statistics(content); gtk_label_set_text(state.status, tr("Pronto"));
    free(content); state.loading = false;
    const char *mode = gtk_stack_get_visible_child_name(state.mode_stack);
    if (mode && strcmp(mode, "editor") == 0) gtk_widget_grab_focus(state.editor);
    else if (mode && strcmp(mode, "split") == 0) gtk_widget_grab_focus(state.split_source_view);
}

Ptr formatting_buffer(void) {
    const char *mode = state.mode_stack ? gtk_stack_get_visible_child_name(state.mode_stack) : NULL;
    return mode && !strcmp(mode, "split") && state.split_buffer ? state.split_buffer : state.editor_buffer;
}

void focus_formatting_editor(void) {
    const char *mode = gtk_stack_get_visible_child_name(state.mode_stack);
    if (mode && !strcmp(mode, "split")) gtk_widget_grab_focus(state.split_source_view);
    else if (mode && !strcmp(mode, "editor")) gtk_widget_grab_focus(state.editor);
}

void insert_markdown(Ptr button, Ptr user_data) {
    (void)button;
    if (!state.active || !user_data) return;
    Ptr buffer = formatting_buffer();
    const char *format = user_data, *separator = strchr(format, '|');
    size_t prefix_length = separator ? (size_t)(separator - format) : strlen(format);
    const char *suffix = separator ? separator + 1 : "";
    TextIter start, end;
    bool selected = gtk_text_buffer_get_selection_bounds(buffer, &start, &end);
    if (!selected) {
        gtk_text_buffer_get_iter_at_mark(buffer, &start, gtk_text_buffer_get_insert(buffer)); end = start;
    }
    int offset = gtk_text_iter_get_offset(&start), end_offset = gtk_text_iter_get_offset(&end);
    char *text = gtk_text_buffer_get_text(buffer, &start, &end, true);
    bool unwrap = false;
    /* Repeating an inline action removes its delimiters while keeping the text selected. */
    if (prefix_length && strlen(suffix) == prefix_length && !strncmp(format, suffix, prefix_length) &&
        offset >= (int)prefix_length && end_offset + (int)prefix_length <= gtk_text_buffer_get_char_count(buffer)) {
        TextIter before, after;
        gtk_text_buffer_get_iter_at_offset(buffer, &before, offset - (int)prefix_length);
        gtk_text_buffer_get_iter_at_offset(buffer, &after, end_offset + (int)prefix_length);
        char *surrounding = gtk_text_buffer_get_text(buffer, &before, &after, true);
        size_t length = strlen(surrounding);
        unwrap = !strncmp(surrounding, format, prefix_length) && !strcmp(surrounding + length - prefix_length, suffix);
        if (unwrap) { start = before; end = after; offset -= (int)prefix_length; }
        g_free(surrounding);
    }
    size_t size = prefix_length + strlen(text) + strlen(suffix) + 1;
    char *replacement = calloc(size, 1);
    if (unwrap) strcpy(replacement, text);
    else { memcpy(replacement, format, prefix_length); strcat(replacement, text); strcat(replacement, suffix); }
    int inner_offset = offset + (unwrap ? 0 : (int)prefix_length);
    int inner_length = utf8_offset(text, text + strlen(text));
    gtk_text_buffer_begin_user_action(buffer);
    gtk_text_buffer_delete(buffer, &start, &end);
    gtk_text_buffer_insert(buffer, &start, replacement, -1);
    gtk_text_buffer_end_user_action(buffer);
    gtk_text_buffer_get_iter_at_offset(buffer, &start, inner_offset);
    gtk_text_buffer_get_iter_at_offset(buffer, &end, inner_offset + inner_length);
    gtk_text_buffer_select_range(buffer, &end, &start);
    free(replacement); g_free(text);
    focus_formatting_editor();
}

const char *heading_label(unsigned level) {
    static const char *labels[] = {"Parágrafo", "Título 1", "Título 2", "Título 3", "Título 4", "Título 5", "Título 6"};
    return tr(labels[level <= 6 ? level : 0]);
}

const char *heading_content(const char *line, size_t *indent, unsigned *level) {
    *indent = 0; *level = 0;
    while (line[*indent] == ' ' && *indent < 4) (*indent)++;
    if (*indent == 4) { *indent = 0; return line; }
    const char *start = line + *indent, *p = start;
    while (*p == '#') p++;
    size_t hashes = (size_t)(p - start);
    if (hashes && hashes <= 6 && (!*p || *p == ' ' || *p == '\t')) {
        *level = (unsigned)hashes;
        while (*p == ' ' || *p == '\t') p++;
        return p;
    }
    return start;
}

void sync_heading_choice(Ptr buffer) {
    if (!state.format_heading || !buffer || state.loading || state.view_syncing) return;
    TextIter start, end;
    gtk_text_buffer_get_iter_at_mark(buffer, &start, gtk_text_buffer_get_insert(buffer));
    gtk_text_iter_set_line_offset(&start, 0); end = start; if (!gtk_text_iter_ends_line(&end)) gtk_text_iter_forward_to_line_end(&end);
    char *line = gtk_text_buffer_get_text(buffer, &start, &end, true);
    size_t indent; unsigned level; heading_content(line, &indent, &level); g_free(line);
    gtk_menu_button_set_label(state.format_heading, heading_label(level));
    for (unsigned i = 0; i <= 6; i++) if (state.heading_choices[i]) {
        if (i == level) gtk_widget_add_css_class(state.heading_choices[i], "current-heading");
        else gtk_widget_remove_css_class(state.heading_choices[i], "current-heading");
    }
}

void heading_format_clicked(Ptr button, Ptr user_data) {
    (void)button;
    unsigned level = (unsigned)(size_t)user_data;
    if (!state.active || state.loading || level > 6) return;
    gtk_popover_popdown(state.format_heading_popover);
    Ptr buffer = formatting_buffer();
    TextIter start, end;
    bool selected = gtk_text_buffer_get_selection_bounds(buffer, &start, &end);
    if (!selected) { gtk_text_buffer_get_iter_at_mark(buffer, &start, gtk_text_buffer_get_insert(buffer)); end = start; }
    int cursor = gtk_text_iter_get_offset(&start);
    if (selected && gtk_text_iter_get_line_offset(&end) == 0) gtk_text_iter_backward_char(&end);
    gtk_text_iter_set_line_offset(&start, 0);
    if (!gtk_text_iter_ends_line(&end)) gtk_text_iter_forward_to_line_end(&end);
    int offset = gtk_text_iter_get_offset(&start);
    char *text = gtk_text_buffer_get_text(buffer, &start, &end, true);
    size_t lines = 1; for (const char *p = text; *p; p++) if (*p == '\n') lines++;
    char *replacement = calloc(strlen(text) + lines * 7 + 1, 1), *out = replacement;
    int cursor_delta = 0;
    for (char *line = text; line;) {
        char *next = strchr(line, '\n'); if (next) *next++ = '\0';
        size_t indent; unsigned old_level; const char *content = heading_content(line, &indent, &old_level);
        if (line == text) cursor_delta = (level ? (int)level + 1 : 0) - (int)(content - line - indent);
        memcpy(out, line, indent); out += indent;
        if (level) { memset(out, '#', level); out += level; *out++ = ' '; }
        size_t length = strlen(content); memcpy(out, content, length); out += length;
        if (next) *out++ = '\n';
        line = next;
    }
    gtk_text_buffer_begin_user_action(buffer);
    gtk_text_buffer_delete(buffer, &start, &end); gtk_text_buffer_insert(buffer, &start, replacement, -1);
    int new_end = gtk_text_iter_get_offset(&start);
    gtk_text_buffer_end_user_action(buffer);
    if (selected) {
        gtk_text_buffer_get_iter_at_offset(buffer, &end, new_end);
        gtk_text_buffer_get_iter_at_offset(buffer, &start, offset);
    } else {
        int position = cursor + cursor_delta;
        if (position < offset) position = offset;
        if (position > new_end) position = new_end;
        gtk_text_buffer_get_iter_at_offset(buffer, &start, position); end = start;
    }
    gtk_text_buffer_select_range(buffer, &end, &start);
    free(replacement); g_free(text);
    sync_heading_choice(buffer); focus_formatting_editor();
}

void insert_text_color(Ptr button, Ptr user_data) {
    (void)button; if (!state.active || state.loading || !user_data) return;
    gtk_popover_popdown(state.format_color_popover);
    char format[96]; snprintf(format, sizeof format, "<span style=\"color:%s\">|</span>", (const char *)user_data);
    insert_markdown(NULL, format);
}

void insert_literal(Ptr button, Ptr user_data) {
    (void)button; if (state.active) { gtk_text_buffer_insert_at_cursor(formatting_buffer(), (const char *)user_data, -1); focus_formatting_editor(); }
}

Ptr editor_scroller(Ptr view) {
    Ptr scroll = gtk_scrolled_window_new(); gtk_widget_set_hexpand(scroll, true);
    gtk_scrolled_window_set_policy(scroll, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_propagate_natural_height(scroll, false);
    gtk_widget_set_vexpand(scroll, true); gtk_scrolled_window_set_child(scroll, view);
    return scroll;
}

void create_preview_tags(Ptr buffer) {
    gtk_text_buffer_create_tag(buffer, "h1", "scale", 1.8, "weight", 700, "pixels-above-lines", 14, "pixels-below-lines", 7, NULL);
    gtk_text_buffer_create_tag(buffer, "h2", "scale", 1.45, "weight", 700, "pixels-above-lines", 11, NULL);
    gtk_text_buffer_create_tag(buffer, "h3", "scale", 1.2, "weight", 700, "pixels-above-lines", 8, NULL);
    gtk_text_buffer_create_tag(buffer, "code", "family", "monospace", "paragraph-background", "#2d3847",
        "left-margin", 18, "right-margin", 18, "pixels-above-lines", 2, "pixels-below-lines", 2,
        "pixels-inside-wrap", 2, "weight", 400, "style", 0, NULL);
    gtk_text_buffer_create_tag(buffer, "code-header", "family", "monospace", "scale", .72, "weight", 700,
        "paragraph-background", "#2d3847", "foreground", "#8db8ff", "left-margin", 18, "right-margin", 18,
        "pixels-above-lines", 12, "pixels-below-lines", 8, "letter-spacing", 700, NULL);
    gtk_text_buffer_create_tag(buffer, "code-footer", "scale", .35, "paragraph-background", "#2d3847",
        "left-margin", 18, "right-margin", 18, "pixels-below-lines", 4, NULL);
    gtk_text_buffer_create_tag(buffer, "quote", "style", 2, "left-margin", 22, "foreground", "#8992a8", NULL);
    gtk_text_buffer_create_tag(buffer, "list", "left-margin", 12, NULL);
    gtk_text_buffer_create_tag(buffer, "done", "strikethrough", true, "foreground", "#737d93", NULL);
    gtk_text_buffer_create_tag(buffer, "strong", "weight", 700, NULL);
    gtk_text_buffer_create_tag(buffer, "emphasis", "style", 2, NULL);
    gtk_text_buffer_create_tag(buffer, "strike", "strikethrough", true, "foreground", "#8992a8", NULL);
    gtk_text_buffer_create_tag(buffer, "inline-code", "family", "monospace", "background", "#252a34", "foreground", "#c29df1", NULL);
    gtk_text_buffer_create_tag(buffer, "link", "foreground", "#78a9ff", "underline", 1, NULL);
    gtk_text_buffer_create_tag(buffer, "table-header", "weight", 700, "background", "#202530", "pixels-above-lines", 5, "pixels-below-lines", 5, NULL);
    gtk_text_buffer_create_tag(buffer, "table-row", "family", "monospace", "pixels-above-lines", 3, "pixels-below-lines", 3, NULL);
    gtk_text_buffer_create_tag(buffer, "divider", "foreground", "#596174", "justification", 2, NULL);
    gtk_text_buffer_create_tag(buffer, "text-red", "foreground", "#ff657a", NULL);
    gtk_text_buffer_create_tag(buffer, "text-orange", "foreground", "#ff9f66", NULL);
    gtk_text_buffer_create_tag(buffer, "text-yellow", "foreground", "#e7c664", NULL);
    gtk_text_buffer_create_tag(buffer, "text-green", "foreground", "#68d391", NULL);
    gtk_text_buffer_create_tag(buffer, "text-blue", "foreground", "#78a9ff", NULL);
    gtk_text_buffer_create_tag(buffer, "text-purple", "foreground", "#c29df1", NULL);
    apply_preview_theme(buffer);
}

Ptr readonly_markdown_view(Ptr *buffer, const char *css_class) {
    Ptr view = gtk_text_view_new(); gtk_text_view_set_editable(view, false);
    gtk_text_view_set_cursor_visible(view, false); gtk_text_view_set_wrap_mode(view, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(view, 20); gtk_text_view_set_right_margin(view, 20);
    gtk_text_view_set_top_margin(view, 14); gtk_widget_add_css_class(view, css_class);
    *buffer = gtk_text_view_get_buffer(view);
    if (strcmp(css_class, "preview") == 0) create_preview_tags(*buffer);
    return view;
}

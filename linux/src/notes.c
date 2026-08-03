#include "state.h"

void free_notes(void) {
    while (state.notes) {
        Note *next = state.notes->next;
        free(state.notes->draft);
        free(state.notes);
        state.notes = next;
    }
    while (state.notebooks) {
        Notebook *next = state.notebooks->next; free(state.notebooks); state.notebooks = next;
    }
    state.active = NULL;
    state.active_notebook = NULL;
}
void discard_note_draft(Note *note) {
    if (!note) return;
    free(note->draft); note->draft = NULL;
    note->draft_title[0] = '\0'; note->draft_tags[0] = '\0';
}
void capture_active_draft(void) {
    if (!state.active || !state.active->dirty || !state.editor_buffer) return;
    char *text = editor_text(); free(state.active->draft); state.active->draft = strdup(text); g_free(text);
    snprintf(state.active->draft_title, sizeof state.active->draft_title, "%s", gtk_editable_get_text(state.title));
    snprintf(state.active->draft_tags, sizeof state.active->draft_tags, "%s", gtk_editable_get_text(state.tags));
}
const char *note_display_title(const Note *note) {
    return note && note->dirty && note->draft ? note->draft_title : note ? note->title : "";
}
const char *note_display_tags(const Note *note) {
    return note && note->dirty && note->draft ? note->draft_tags : note ? note->tags : "";
}
char *note_current_markdown(const Note *note) {
    if (!note) return strdup("");
    return note->dirty && note->draft ? strdup(note->draft) : read_file(note->markdown_path);
}
bool save_note_payload(Note *note, const char *text, const char *requested_title, const char *tags_text, bool update_status) {
    if (!note || !text || !requested_title || !tags_text) return false;
    bool ok = true;
    char requested[256];
    safe_title(requested_title, requested);
    if (!note->external && strcmp(requested, note->title) != 0) {
        char destination[PATH_MAX];
        snprintf(destination, sizeof destination, "%s/%s", note->notebook->path, requested);
        if (access(destination, F_OK) != 0 && rename(note->directory, destination) == 0) {
            snprintf(note->directory, sizeof note->directory, "%s", destination);
            snprintf(note->markdown_path, sizeof note->markdown_path, "%s/note.md", destination);
            snprintf(note->title, sizeof note->title, "%s", requested);
            rebuild_sidebar();
            rebuild_tabs();
        }
    }
    char *previous = read_file(note->markdown_path);
    if (strcmp(previous, text) != 0) create_snapshot(note, previous);
    free(previous);
    ok = write_atomic(note->markdown_path, text);
    snprintf(note->tags, sizeof note->tags, "%s", tags_text);
    char metadata_path[PATH_MAX];
    snprintf(metadata_path, sizeof metadata_path, "%s/.note.json", note->directory);
    char metadata[2048];
    char tags_json[1200] = "";
    size_t used = 0;
    char tags_copy[512];
    snprintf(tags_copy, sizeof tags_copy, "%s", note->tags);
    for (char *tag = strtok(tags_copy, ","); tag; tag = strtok(NULL, ",")) {
        while (*tag == ' ') tag++;
        size_t length = strlen(tag);
        while (length && tag[length - 1] == ' ') tag[--length] = '\0';
        if (!length) continue;
        used += (size_t)snprintf(tags_json + used, sizeof tags_json - used, "%s\"%s\"", used ? ", " : "", tag);
        if (used >= sizeof tags_json) break;
    }
    snprintf(metadata, sizeof metadata, "{\n  \"id\": \"%s\",\n  \"colorHex\": %s%s%s,\n  \"tags\": [%s]\n}\n",
             note->id, note->color[0] ? "\"" : "", note->color[0] ? note->color : "null",
             note->color[0] ? "\"" : "", tags_json);
    if (!note->external) ok = write_atomic(metadata_path, metadata) && ok;
    if (ok) clear_recovery(note);
    if (ok) {
        note->dirty = false;
        discard_note_draft(note);
        if (note->dirty_indicator) gtk_widget_set_visible(note->dirty_indicator, false);
        if (note->external) { struct stat info; if (stat(note->markdown_path, &info) == 0) note->external_mtime = info.st_mtime; }
    }
    if (update_status) gtk_label_set_text(state.status, ok ? tr("Guardado") : tr("Erro ao guardar"));
    return ok;
}
void extract_metadata(Note *note) {
    char path[PATH_MAX]; snprintf(path, sizeof path, "%s/.note.json", note->directory);
    char *json = read_file(path);
    char *id = strstr(json, "\"id\"");
    if (id && (id = strchr(id + 4, '"'))) {
        id++; char *end = strchr(id, '"');
        if (end) snprintf(note->id, sizeof note->id, "%.*s", (int)(end - id), id);
    }
    char *tags = strstr(json, "\"tags\"");
    if (tags && (tags = strchr(tags, '['))) {
        char *end = strchr(tags, ']'); size_t used = 0;
        while (end && (tags = strchr(tags + 1, '"')) && tags < end) {
            char *close = strchr(tags + 1, '"'); if (!close || close > end) break;
            if (used) { note->tags[used++] = ','; note->tags[used++] = ' '; }
            size_t length = (size_t)(close - tags - 1);
            if (used + length >= sizeof note->tags) break;
            memcpy(note->tags + used, tags + 1, length); used += length; note->tags[used] = '\0'; tags = close;
        }
    }
    char *color = strstr(json, "\"colorHex\"");
    if (color && (color = strchr(color, '#'))) {
        char *end = strchr(color, '"'); if (end) snprintf(note->color, sizeof note->color, "%.*s", (int)(end - color), color);
    }
    free(json);
}
void extract_notebook_metadata(Notebook *notebook) {
    char path[PATH_MAX]; snprintf(path, sizeof path, "%s/.notebook.json", notebook->path);
    char *json = read_file(path), *color = strstr(json, "\"colorHex\"");
    if (color && (color = strchr(color, '#'))) {
        char *end = strchr(color, '"'); if (end) snprintf(notebook->color, sizeof notebook->color, "%.*s", (int)(end - color), color);
    }
    char *order = strstr(json, "\"noteOrder\"");
    if (order && (order = strchr(order, '['))) {
        char *end = strchr(order, ']');
        while (end && notebook->note_order_count < 128 && (order = strchr(order + 1, '"')) && order < end) {
            char *close = strchr(order + 1, '"'); if (!close || close > end) break;
            snprintf(notebook->note_order[notebook->note_order_count++], 64, "%.*s", (int)(close - order - 1), order + 1);
            order = close;
        }
    }
    free(json);
}
void save_notebook_metadata(Notebook *notebook) {
    if (!notebook || notebook->external) return;
    char path[PATH_MAX]; snprintf(path, sizeof path, "%s/.notebook.json", notebook->path);
    char order[8192] = "", *cursor = order; size_t remaining = sizeof order;
    for (Note *note = state.notes; note; note = note->next) if (note->notebook == notebook) {
        int written = snprintf(cursor, remaining, "%s\"%s\"", cursor == order ? "" : ", ", note->id);
        if (written < 0 || (size_t)written >= remaining) break;
        cursor += written; remaining -= (size_t)written;
    }
    char json[9000]; snprintf(json, sizeof json, "{\n  \"colorHex\": %s%s%s,\n  \"noteOrder\": [%s]\n}\n",
        notebook->color[0] ? "\"" : "", notebook->color[0] ? notebook->color : "null", notebook->color[0] ? "\"" : "", order);
    write_atomic(path, json);
}
void create_snapshot(Note *note, const char *previous) {
    if (!previous || !*previous || !note->id[0]) return;
    const char *data_home = getenv("XDG_DATA_HOME");
    const char *home = getenv("HOME");
    char directory[PATH_MAX];
    if (data_home && *data_home)
        snprintf(directory, sizeof directory, "%s/notemd/versions/%s", data_home, note->id);
    else
        snprintf(directory, sizeof directory, "%s/.local/share/notemd/versions/%s", home ? home : ".", note->id);
    if (!make_directory(directory)) return;
    char path[PATH_MAX];
    snprintf(path, sizeof path, "%s/%lld.md", directory, (long long)time(NULL));
    write_atomic(path, previous);
}
void write_recovery(Note *note, const char *markdown) {
    if (!note || !note->id[0]) return;
    char path[PATH_MAX]; recovery_path(note, path);
    const char *title = note_display_title(note), *tags = note_display_tags(note);
    size_t size = strlen(title) + strlen(tags) + strlen(markdown) + 4;
    char *draft = calloc(size, 1); snprintf(draft, size, "%s\n%s\n%s", title, tags, markdown);
    write_atomic(path, draft); free(draft);
}
void clear_recovery(Note *note) {
    if (!note || !note->id[0]) return;
    char path[PATH_MAX]; recovery_path(note, path); unlink(path);
}
void restore_recovery(void) {
    char directory[PATH_MAX]; recovery_directory(directory); DIR *drafts = opendir(directory); if (!drafts) return;
    Notebook *recovered = NULL; struct dirent *entry;
    while ((entry = readdir(drafts))) {
        if (entry->d_name[0] == '.') continue;
        char source_path[PATH_MAX]; snprintf(source_path, sizeof source_path, "%s/%s", directory, entry->d_name);
        char *draft = read_file(source_path); if (!draft[0]) { free(draft); continue; }
        char *first = strchr(draft, '\n'); if (!first) { free(draft); continue; } *first++ = '\0';
        char *second = strchr(first, '\n'); if (!second) { free(draft); continue; } *second++ = '\0';
        if (!recovered) {
            recovered = calloc(1, sizeof *recovered); snprintf(recovered->title, sizeof recovered->title, "Recuperadas");
            unique_path(recovered->path, sizeof recovered->path, state.root, "Recuperadas"); make_directory(recovered->path);
            recovered->next = state.notebooks; state.notebooks = recovered;
        }
        Note *note = calloc(1, sizeof *note); safe_title(draft, note->title); snprintf(note->tags, sizeof note->tags, "%s", first);
        char *dot = strrchr(entry->d_name, '.'); size_t id_length = dot ? (size_t)(dot - entry->d_name) : strlen(entry->d_name);
        snprintf(note->id, sizeof note->id, "%.*s", (int)id_length, entry->d_name); note->notebook = recovered;
        unique_path(note->directory, sizeof note->directory, recovered->path, note->title); make_directory(note->directory);
        snprintf(note->markdown_path, sizeof note->markdown_path, "%s/note.md", note->directory); write_atomic(note->markdown_path, second);
        note->next = state.notes; state.notes = note; unlink(source_path); free(draft);
    }
    closedir(drafts);
}
void load_library(void) {
    make_directory(state.root);
    DIR *root = opendir(state.root);
    if (!root) return;
    struct dirent *book_entry;
    while ((book_entry = readdir(root))) {
        if (book_entry->d_name[0] == '.') continue;
        char book_path[PATH_MAX];
        snprintf(book_path, sizeof book_path, "%s/%s", state.root, book_entry->d_name);
        if (!is_directory(book_path)) continue;
        Notebook *notebook = calloc(1, sizeof *notebook);
        notebook->expanded = true;
        snprintf(notebook->title, sizeof notebook->title, "%s", book_entry->d_name);
        snprintf(notebook->path, sizeof notebook->path, "%s", book_path);
        extract_notebook_metadata(notebook);
        notebook->next = state.notebooks; state.notebooks = notebook;
        DIR *book = opendir(book_path);
        if (!book) continue;
        struct dirent *note_entry;
        while ((note_entry = readdir(book))) {
            if (note_entry->d_name[0] == '.') continue;
            char note_path[PATH_MAX], markdown_path[PATH_MAX];
            snprintf(note_path, sizeof note_path, "%s/%s", book_path, note_entry->d_name);
            snprintf(markdown_path, sizeof markdown_path, "%s/note.md", note_path);
            if (!is_directory(note_path) || access(markdown_path, R_OK) != 0) continue;
            Note *note = calloc(1, sizeof *note);
            snprintf(note->title, sizeof note->title, "%s", note_entry->d_name);
            snprintf(note->directory, sizeof note->directory, "%s", note_path);
            snprintf(note->markdown_path, sizeof note->markdown_path, "%s", markdown_path);
            note->notebook = notebook;
            extract_metadata(note);
            if (!note->id[0]) make_uuid(note->id);
            note->order = 100000;
            for (size_t index = 0; index < notebook->note_order_count; index++)
                if (strcmp(notebook->note_order[index], note->id) == 0) { note->order = (int)index; break; }
            Note **place = &state.notes;
            while (*place && ((*place)->notebook != notebook || (*place)->order <= note->order)) place = &(*place)->next;
            note->next = *place; *place = note;
        }
        closedir(book);
    }
    closedir(root);
}
void add_library_color_class(Ptr widget, const char *color) {
    if (!widget || !color || !color[0]) return;
    if (strcasecmp(color, "#d1242f") == 0) gtk_widget_add_css_class(widget, "color-red");
    else if (strcasecmp(color, "#bc4c00") == 0) gtk_widget_add_css_class(widget, "color-orange");
    else if (strcasecmp(color, "#9a6700") == 0) gtk_widget_add_css_class(widget, "color-yellow");
    else if (strcasecmp(color, "#1a7f37") == 0) gtk_widget_add_css_class(widget, "color-green");
    else if (strcasecmp(color, "#0969da") == 0) gtk_widget_add_css_class(widget, "color-blue");
    else if (strcasecmp(color, "#8250df") == 0) gtk_widget_add_css_class(widget, "color-purple");
}
void append_note_button(Note *note, Ptr container) {
    Ptr button = text_button(note_display_title(note), "sidebar-note");
    add_library_color_class(button, note->color);
    g_signal_connect_data(button, "clicked", (void *)select_note, note, NULL, 0);
    if (note == state.active) gtk_widget_add_css_class(button, "selected-item");
    Ptr row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2); gtk_widget_add_css_class(row, "note-row");
    gtk_widget_set_hexpand(button, true); gtk_box_append(row, button);
    if (!note->external) {
        Ptr drag = gtk_drag_source_new();
        gtk_drag_source_set_actions(drag, GDK_ACTION_MOVE);
        g_signal_connect_data(drag, "prepare", (void *)note_drag_prepare, note, NULL, 0);
        gtk_widget_add_controller(row, drag);
        Ptr reorder_drop = gtk_drop_target_new(g_type_from_name("gchararray"), GDK_ACTION_MOVE);
        g_signal_connect_data(reorder_drop, "drop", (void *)note_dropped_on_note, note, NULL, 0);
        gtk_widget_add_controller(row, reorder_drop);
    }
    note->menu_button = library_menu_button(note, false); add_library_color_class(note->menu_button, note->color);
    gtk_box_append(row, note->menu_button); note->context_gesture = attach_library_context_menu(row, note, false);
    gtk_box_append(container, row);
    note->button = button;
}
bool note_has_tag(Note *note, const char *selected) {
    if (!selected || !selected[0]) return true;
    char copy[512]; snprintf(copy, sizeof copy, "%s", note_display_tags(note));
    for (char *tag = strtok(copy, ","); tag; tag = strtok(NULL, ",")) {
        while (*tag == ' ') tag++;
        size_t length = strlen(tag);
        while (length && tag[length - 1] == ' ') tag[--length] = '\0';
        if (strcasecmp(tag, selected) == 0) return true;
    }
    return false;
}
void remove_note_tag(Ptr button, Ptr user_data) {
    (void)button; const char *removed = user_data; if (!state.active || !removed) return;
    char source[512]; snprintf(source, sizeof source, "%s", gtk_editable_get_text(state.tags));
    char result[512] = ""; bool first = true;
    for (char *tag = strtok(source, ","); tag; tag = strtok(NULL, ",")) {
        while (*tag == ' ') tag++;
        size_t length = strlen(tag);
        while (length && tag[length - 1] == ' ') tag[--length] = '\0';
        if (!length || strcasecmp(tag, removed) == 0) continue;
        if (!first) strncat(result, ", ", sizeof result - strlen(result) - 1);
        strncat(result, tag, sizeof result - strlen(result) - 1); first = false;
    }
    gtk_editable_set_text(state.tags, result); rebuild_note_tags();
}
void rebuild_note_tags(void) {
    if (!state.note_tags_box || !state.tags) return;
    while (gtk_widget_get_first_child(state.note_tags_box))
        gtk_box_remove(state.note_tags_box, gtk_widget_get_first_child(state.note_tags_box));
    if (state.active && state.active->external) {
        Ptr external = navigation_button("emblem-symbolic-link", tr("Ficheiro externo"));
        gtk_widget_add_css_class(external, "external-chip"); gtk_box_append(state.note_tags_box, external);
    }
    char source[512]; snprintf(source, sizeof source, "%s", gtk_editable_get_text(state.tags));
    for (char *tag = strtok(source, ","); tag; tag = strtok(NULL, ",")) {
        while (*tag == ' ') tag++;
        size_t length = strlen(tag);
        while (length && tag[length - 1] == ' ') tag[--length] = '\0';
        if (!length) continue;
        Ptr chip = gtk_button_new_with_label(""); Ptr row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        char label_text[160]; snprintf(label_text, sizeof label_text, "#%s", tag);
        gtk_box_append(row, gtk_label_new(label_text)); Ptr close = gtk_image_new_from_icon_name("window-close-symbolic");
        gtk_image_set_pixel_size(close, 10); gtk_box_append(row, close); gtk_button_set_child(chip, row);
        gtk_widget_add_css_class(chip, "tag-chip"); gtk_widget_set_tooltip_text(chip, "Remover tag");
        char *value = strdup(tag); g_signal_connect_data(chip, "clicked", (void *)remove_note_tag, value, (void *)free_signal_data, 0);
        gtk_box_append(state.note_tags_box, chip);
    }
}
void add_note_tag(Ptr entry, Ptr unused) {
    (void)unused; if (!state.active || !entry) return;
    char value[128]; snprintf(value, sizeof value, "%s", gtk_editable_get_text(entry));
    char *start = value; while (*start == ' ' || *start == '#') start++;
    size_t length = strlen(start); while (length && start[length - 1] == ' ') start[--length] = '\0';
    if (!length) return;
    char current[512]; snprintf(current, sizeof current, "%s", gtk_editable_get_text(state.tags));
    char copy[512]; snprintf(copy, sizeof copy, "%s", current);
    for (char *tag = strtok(copy, ","); tag; tag = strtok(NULL, ",")) {
        while (*tag == ' ') tag++;
        if (strcasecmp(tag, start) == 0) { gtk_editable_set_text(entry, ""); return; }
    }
    char combined[512]; snprintf(combined, sizeof combined, "%s%s%s", current, current[0] ? ", " : "", start);
    gtk_editable_set_text(state.tags, combined); gtk_editable_set_text(entry, ""); rebuild_note_tags();
}
void add_note_tag_clicked(Ptr button, Ptr unused) { (void)button; add_note_tag(state.tag_input, unused); }
void free_signal_data(Ptr data, Ptr closure) { (void)closure; free(data); }
void tag_selected(Ptr button, Ptr user_data) {
    (void)button; const char *tag = user_data;
    if (strcmp(state.selected_tag, tag) == 0) state.selected_tag[0] = '\0';
    else snprintf(state.selected_tag, sizeof state.selected_tag, "%s", tag);
    rebuild_sidebar();
}
void rebuild_tags(void) {
    if (!state.tag_box) return;
    while (gtk_widget_get_first_child(state.tag_box))
        gtk_box_remove(state.tag_box, gtk_widget_get_first_child(state.tag_box));
    Ptr header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4); Ptr label = gtk_label_new(tr("Tags"));
    gtk_label_set_xalign(label, 0.0f); gtk_widget_set_hexpand(label, true); gtk_widget_add_css_class(label, "section-title");
    gtk_box_append(header, label);
    if (state.selected_tag[0]) {
        Ptr clear = icon_button("edit-clear-symbolic", "Limpar filtro de tag");
        g_signal_connect_data(clear, "clicked", (void *)clear_tag_filter, NULL, NULL, 0); gtk_box_append(header, clear);
    }
    gtk_box_append(state.tag_box, header);
    char values[256][128]; unsigned counts[256] = {0}; size_t total = 0;
    for (Note *note = state.notes; note; note = note->next) {
        char copy[512]; snprintf(copy, sizeof copy, "%s", note_display_tags(note));
        for (char *tag = strtok(copy, ","); tag; tag = strtok(NULL, ",")) {
            while (*tag == ' ') tag++;
            size_t length = strlen(tag);
            while (length && tag[length - 1] == ' ') tag[--length] = '\0';
            if (!length) continue;
            size_t index = 0; while (index < total && strcasecmp(values[index], tag) != 0) index++;
            if (index == total && total < 256) { snprintf(values[total], 128, "%s", tag); total++; }
            if (index < 256) counts[index]++;
        }
    }
    for (size_t index = 0; index < total; index++) {
        char text[180]; snprintf(text, sizeof text, "#%s   %u", values[index], counts[index]);
        Ptr choice = navigation_button("tag-symbolic", text); if (strcasecmp(state.selected_tag, values[index]) == 0)
            gtk_widget_add_css_class(choice, "selected-tag");
        char *value = strdup(values[index]);
        g_signal_connect_data(choice, "clicked", (void *)tag_selected, value, (void *)free_signal_data, 0);
        gtk_box_append(state.tag_box, choice);
    }
    if (!total) gtk_box_append(state.tag_box, gtk_label_new(tr("Sem tags")));
}
void new_notebook(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    Notebook *notebook = calloc(1, sizeof *notebook);
    notebook->expanded = true;
    unique_path(notebook->path, sizeof notebook->path, state.root, "Novo notebook");
    const char *name = strrchr(notebook->path, '/'); snprintf(notebook->title, sizeof notebook->title, "%s", name ? name + 1 : notebook->path);
    if (make_directory(notebook->path)) {
        notebook->next = state.notebooks; state.notebooks = notebook; state.active_notebook = notebook;
        save_notebook_metadata(notebook); rebuild_sidebar();
        gtk_label_set_text(state.status, "Notebook criado. Pode agora criar uma nota.");
    } else free(notebook);
}
void new_note(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    if (!state.active_notebook) {
        if (state.active) state.active_notebook = state.active->notebook;
        else new_notebook(NULL, NULL);
    }
    if (!state.active_notebook) return;
    char directory[PATH_MAX]; unique_path(directory, sizeof directory, state.active_notebook->path, "Nova nota");
    if (!make_directory(directory)) { gtk_label_set_text(state.status, "Erro ao criar a pasta da nota"); return; }
    char markdown[PATH_MAX]; snprintf(markdown, sizeof markdown, "%s/note.md", directory);
    if (!write_atomic(markdown, "# Nova nota\n")) return;
    char metadata[PATH_MAX]; snprintf(metadata, sizeof metadata, "%s/.note.json", directory);
    write_atomic(metadata, "{\n  \"colorHex\": null,\n  \"tags\": []\n}\n");
    Note *note = calloc(1, sizeof *note); snprintf(note->title, sizeof note->title, "Nova nota");
    make_uuid(note->id);
    snprintf(note->directory, sizeof note->directory, "%s", directory);
    snprintf(note->markdown_path, sizeof note->markdown_path, "%s", markdown);
    note->notebook = state.active_notebook;
    note->next = state.notes; state.notes = note; rebuild_sidebar(); select_note(NULL, note);
}
Note *add_external_note(const char *path) {
    if (!path || access(path, R_OK) != 0) return NULL;
    for (Note *existing = state.notes; existing; existing = existing->next)
        if (existing->external && strcmp(existing->markdown_path, path) == 0) return existing;
    Note *note = calloc(1, sizeof *note);
    snprintf(note->markdown_path, sizeof note->markdown_path, "%s", path);
    snprintf(note->directory, sizeof note->directory, "%s", path);
    char *slash = strrchr(note->directory, '/');
    if (slash) *slash = '\0';
    Notebook *external = NULL;
    for (Notebook *book = state.notebooks; book; book = book->next) if (book->external) { external = book; break; }
    if (!external) {
        external = calloc(1, sizeof *external); external->expanded = true;
        snprintf(external->title, sizeof external->title, "%s", tr("Ficheiros externos"));
        external->external = true; external->next = state.notebooks; state.notebooks = external;
    }
    note->notebook = external;
    const char *filename = strrchr(path, '/'); filename = filename ? filename + 1 : path;
    snprintf(note->title, sizeof note->title, "%s", filename);
    char *dot = strrchr(note->title, '.'); if (dot) *dot = '\0';
    make_uuid(note->id); note->external = true; note->next = state.notes; state.notes = note;
    struct stat info; if (stat(path, &info) == 0) note->external_mtime = info.st_mtime;
    return note;
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
gboolean note_dropped_on_note(Ptr target_widget, const void *value, double x, double y, Ptr user_data) {
    (void)target_widget; (void)x; (void)y; const char *id = g_value_get_string(value); if (!id) return false;
    Note *target = user_data;
    for (Note *source = state.notes; source; source = source->next)
        if (strcmp(source->id, id) == 0) return reorder_note_before(source, target);
    return false;
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
void sort_title(Ptr button, Ptr unused) { (void)button; (void)unused; sort_notes(false); }
void sort_recent(Ptr button, Ptr unused) { (void)button; (void)unused; sort_notes(true); }
void move_note_up(Ptr button, Ptr unused) { (void)button; (void)unused; reorder_active_note(-1); }
void move_note_down(Ptr button, Ptr unused) { (void)button; (void)unused; reorder_active_note(1); }
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
void select_note(Ptr button, Ptr user_data) {
    (void)button; Note *target = user_data; if (!target) return;
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
    highlight_editor(content);
    update_views(content);
    rebuild_block_editor();
    update_statistics(content); gtk_label_set_text(state.status, tr("Pronto"));
    free(content); state.loading = false;
}
void resolve_root(void) {
    const char *argument = getenv("NOTEMD_NOTES_DIR");
    if (argument && *argument) snprintf(state.root, sizeof state.root, "%s", argument);
    else if (!state.root[0]) {
        argument = getenv("HOME"); if (!argument) argument = ".";
        snprintf(state.root, sizeof state.root, "%s/Documents/NoteMD", argument);
    }
}
void resolve_data_dir(void) {
    const char *override = getenv("NOTEMD_DATA_DIR");
    if (override && *override) { snprintf(state.data_dir, sizeof state.data_dir, "%s", override); return; }
    char development[PATH_MAX]; snprintf(development, sizeof development, "%s/style.css", NOTEMD_DEV_DATA_DIR);
    if (access(development, R_OK) == 0) { snprintf(state.data_dir, sizeof state.data_dir, "%s", NOTEMD_DEV_DATA_DIR); return; }
    if (access("/usr/share/notemd/style.css", R_OK) == 0) {
        snprintf(state.data_dir, sizeof state.data_dir, "/usr/share/notemd"); return;
    }
    const char *data = getenv("XDG_DATA_HOME"), *home = getenv("HOME");
    if (data && *data) snprintf(state.data_dir, sizeof state.data_dir, "%s/notemd", data);
    else snprintf(state.data_dir, sizeof state.data_dir, "%s/.local/share/notemd", home ? home : ".");
}

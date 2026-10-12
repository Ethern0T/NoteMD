#include "internal.h"

bool is_directory(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

bool make_directory(const char *path) {
    char partial[PATH_MAX];
    snprintf(partial, sizeof partial, "%s", path);
    for (char *p = partial + 1; *p; p++) {
        if (*p != '/') continue;
        *p = '\0';
        if (mkdir(partial, 0755) != 0 && errno != EEXIST) return false;
        *p = '/';
    }
    return mkdir(partial, 0755) == 0 || errno == EEXIST;
}

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
    ensure_metadata_loaded((Note *)note);
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

char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return strdup("");
    fseek(file, 0, SEEK_END); long size = ftell(file); rewind(file);
    char *data = calloc((size_t)size + 1, 1);
    if (data) fread(data, 1, (size_t)size, file);
    fclose(file);
    return data;
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

void ensure_metadata_loaded(Note *note) {
    if (note && !note->metadata_loaded && !note->external) {
        extract_metadata(note);
        note->metadata_loaded = true;
    }
}

void resort_library_notes(void) {
    if (!state.notes) return;
    bool swapped;
    do {
        swapped = false;
        Note **prev = &state.notes;
        Note *curr = state.notes;
        while (curr && curr->next) {
            Note *next = curr->next;
            bool swap_needed = false;
            if (curr->notebook != next->notebook) {
                if (curr->notebook > next->notebook) swap_needed = true;
            } else {
                if (curr->order > next->order) swap_needed = true;
            }
            if (swap_needed) {
                curr->next = next->next;
                next->next = curr;
                *prev = next;
                swapped = true;
            }
            prev = &(*prev)->next;
            curr = *prev;
        }
    } while (swapped);
}

static bool indexer_running = false;

gboolean ui_metadata_updated(Ptr unused) {
    (void)unused;
    resort_library_notes();
    if (state.sidebar) rebuild_sidebar();
    if (state.tag_box) rebuild_tags();
    return 0;
}

void extract_notebook_metadata(Notebook *notebook);

void *background_metadata_indexer(void *arg) {
    (void)arg;
    for (Notebook *book = state.notebooks; book; book = book->next) {
        if (!book->metadata_loaded) {
            extract_notebook_metadata(book);
            book->metadata_loaded = true;
        }
    }
    unsigned count = 0;
    for (Note *note = state.notes; note; note = note->next) {
        if (!note->metadata_loaded && !note->external) {
            extract_metadata(note);
            note->metadata_loaded = true;
            note->order = 100000;
            if (note->notebook) {
                for (size_t index = 0; index < note->notebook->note_order_count; index++) {
                    if (strcmp(note->notebook->note_order[index], note->id) == 0) {
                        note->order = (int)index;
                        break;
                    }
                }
            }
            count++;
            if (count % 5 == 0) {
                if (g_idle_add) g_idle_add((void *)ui_metadata_updated, NULL);
                usleep(5000);
            }
        }
    }
    if (g_idle_add) g_idle_add((void *)ui_metadata_updated, NULL);
    indexer_running = false;
    return NULL;
}

void start_background_metadata_indexer(void) {
    if (indexer_running) return;
    indexer_running = true;
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, background_metadata_indexer, NULL) == 0) {
        pthread_detach(thread_id);
    } else {
        indexer_running = false;
    }
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

bool write_atomic(const char *path, const char *content) {
    if (strstr(path, "/gvfs/") != NULL || strstr(path, "/dav:") != NULL || strstr(path, "/davs:") != NULL) {
        FILE *file = fopen(path, "wb");
        if (!file) return false;
        size_t length = strlen(content);
        bool ok = fwrite(content, 1, length, file) == length && fclose(file) == 0;
        return ok;
    }
    char temporary[PATH_MAX];
    snprintf(temporary, sizeof temporary, "%s.tmp", path);
    FILE *file = fopen(temporary, "wb");
    if (!file) return false;
    size_t length = strlen(content);
    bool ok = fwrite(content, 1, length, file) == length && fclose(file) == 0;
    if (ok) ok = rename(temporary, path) == 0;
    if (!ok) unlink(temporary);
    return ok;
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

void recovery_directory(char output[PATH_MAX]) {
    const char *data = getenv("XDG_DATA_HOME"), *home = getenv("HOME");
    if (data && *data) snprintf(output, PATH_MAX, "%s/notemd/recovery", data);
    else snprintf(output, PATH_MAX, "%s/.local/share/notemd/recovery", home ? home : ".");
}

void recovery_path(Note *note, char output[PATH_MAX]) {
    char directory[PATH_MAX]; recovery_directory(directory); make_directory(directory);
    snprintf(output, PATH_MAX, "%s/%s.draft", directory, note->id);
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
        bool is_book_dir = (book_entry->d_type == DT_DIR) || (book_entry->d_type == DT_UNKNOWN && is_directory(book_path));
        if (!is_book_dir) continue;
        Notebook *notebook = calloc(1, sizeof *notebook);
        notebook->expanded = false;
        snprintf(notebook->title, sizeof notebook->title, "%s", book_entry->d_name);
        snprintf(notebook->path, sizeof notebook->path, "%s", book_path);
        notebook->next = state.notebooks; state.notebooks = notebook;
        DIR *book = opendir(book_path);
        if (!book) continue;
        struct dirent *note_entry;
        while ((note_entry = readdir(book))) {
            if (note_entry->d_name[0] == '.') continue;
            char note_path[PATH_MAX], markdown_path[PATH_MAX];
            snprintf(note_path, sizeof note_path, "%s/%s", book_path, note_entry->d_name);
            snprintf(markdown_path, sizeof markdown_path, "%s/note.md", note_path);
            bool is_note_dir = (note_entry->d_type == DT_DIR) || (note_entry->d_type == DT_UNKNOWN && is_directory(note_path));
            if (!is_note_dir) continue;
            Note *note = calloc(1, sizeof *note);
            snprintf(note->title, sizeof note->title, "%s", note_entry->d_name);
            snprintf(note->directory, sizeof note->directory, "%s", note_path);
            snprintf(note->markdown_path, sizeof note->markdown_path, "%s", markdown_path);
            note->notebook = notebook;
            make_uuid(note->id); // temporary UUID until metadata is loaded in background
            note->order = 100000;
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
    ensure_metadata_loaded(note);
    Ptr button = navigation_button("text-x-generic-symbolic", note_display_title(note));
    gtk_widget_add_css_class(button, "sidebar-note");
    gtk_widget_set_tooltip_text(button, note_display_title(note));
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

void clear_tag_filter(Ptr button, Ptr unused) {
    (void)button; (void)unused; state.selected_tag[0] = '\0'; rebuild_sidebar();
}

void rebuild_tags(void) {
    if (!state.tag_box) return;
    while (gtk_widget_get_first_child(state.tag_box))
        gtk_box_remove(state.tag_box, gtk_widget_get_first_child(state.tag_box));
    if (state.selected_tag[0]) {
        Ptr header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        Ptr label = gtk_label_new(state.selected_tag);
        gtk_label_set_xalign(label, 0.0f); gtk_widget_set_hexpand(label, true);
        gtk_label_set_ellipsize(label, 3); gtk_widget_add_css_class(label, "section-title");
        gtk_box_append(header, label);
        Ptr clear = icon_button("edit-clear-symbolic", "Limpar filtro de tag");
        g_signal_connect_data(clear, "clicked", (void *)clear_tag_filter, NULL, NULL, 0); gtk_box_append(header, clear);
        gtk_box_append(state.tag_box, header);
    }
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
        char text[180]; snprintf(text, sizeof text, "#%s", values[index]);
        Ptr choice = navigation_button("tag-symbolic", text);
        char count[16]; snprintf(count, sizeof count, "%u", counts[index]);
        Ptr count_label = gtk_label_new(count); gtk_widget_add_css_class(count_label, "tag-count");
        gtk_box_append(gtk_widget_get_first_child(choice), count_label);
        gtk_widget_set_tooltip_text(choice, text);
        if (strcasecmp(state.selected_tag, values[index]) == 0)
            gtk_widget_add_css_class(choice, "selected-tag");
        char *value = strdup(values[index]);
        g_signal_connect_data(choice, "clicked", (void *)tag_selected, value, (void *)free_signal_data, 0);
        gtk_box_append(state.tag_box, choice);
    }
    if (state.tags_section) gtk_widget_set_visible(state.tags_section, total > 0 || state.selected_tag[0]);
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

void rebuild_sidebar(void) {
    dismiss_active_popover();
    while (gtk_widget_get_first_child(state.sidebar))
        gtk_box_remove(state.sidebar, gtk_widget_get_first_child(state.sidebar));
    if (state.root_label) {
        gtk_label_set_text(state.root_label, state.root);
        gtk_widget_set_tooltip_text(state.root_label, state.root);
    }
    state.context_menu_count = 0;
    for (Notebook *book = state.notebooks; book; book = book->next) {
        book->button = NULL; book->add_button = NULL; book->menu_button = NULL; book->expander = NULL; book->context_gesture = NULL;
    }
    for (Note *note = state.notes; note; note = note->next) { note->button = NULL; note->menu_button = NULL; note->context_gesture = NULL; }
    Ptr heading = gtk_label_new(tr("Notebooks"));
    gtk_label_set_xalign(heading, 0.0f); gtk_widget_add_css_class(heading, "section-title");
    margins(heading, 8, 8); gtk_box_append(state.sidebar, heading);
    if (!state.notebooks) {
        Ptr empty = gtk_label_new("Ainda não existem notebooks"); gtk_label_set_xalign(empty, 0.0f);
        gtk_widget_add_css_class(empty, "empty-message"); margins(empty, 12, 14); gtk_box_append(state.sidebar, empty);
        Ptr create = navigation_button("folder-new-symbolic", "Criar primeiro notebook");
        gtk_widget_add_css_class(create, "empty-action");
        g_signal_connect_data(create, "clicked", (void *)new_notebook, NULL, NULL, 0); gtk_box_append(state.sidebar, create);
    }
    const char *query = state.search ? gtk_editable_get_text(state.search) : "";
    size_t visible_notebooks = 0;
    for (Notebook *book = state.notebooks; book; book = book->next) {
        bool any = false;
        for (Note *note = state.notes; note; note = note->next) if (note->notebook == book && note_has_tag(note, state.selected_tag)) {
            any = !query[0] || strcasestr(note_display_title(note), query) || strcasestr(note_display_tags(note), query);
            if (!any) { char *contents = note_current_markdown(note); any = strcasestr(contents, query) != NULL; free(contents); }
            if (any) break;
        }
        if (!query[0] && !state.selected_tag[0]) any = true;
        if (!any) continue;
        visible_notebooks++;
        Ptr notebook = navigation_button(book->external ? "folder-remote-symbolic" : "folder-symbolic", book->title);
        gtk_widget_set_tooltip_text(notebook, book->title);
        add_library_color_class(notebook, book->color);
        gtk_widget_add_css_class(notebook, "notebook-title");
        if (book == state.active_notebook) gtk_widget_add_css_class(notebook, "selected-item");
        book->button = notebook;
        g_signal_connect_data(notebook, "clicked", (void *)select_notebook, book, NULL, 0);
        Ptr notebook_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2); gtk_widget_add_css_class(notebook_row, "notebook-row");
        gtk_widget_set_hexpand(notebook, true); gtk_box_append(notebook_row, notebook);
        if (!book->external) {
            Ptr note_drop = gtk_drop_target_new(g_type_from_name("gchararray"), GDK_ACTION_MOVE);
            g_signal_connect_data(note_drop, "drop", (void *)note_dropped_on_notebook, book, NULL, 0);
            gtk_widget_add_controller(notebook_row, note_drop);
        }
        book->menu_button = library_menu_button(book, true); add_library_color_class(book->menu_button, book->color);
        gtk_box_append(notebook_row, book->menu_button); book->context_gesture = attach_library_context_menu(notebook_row, book, true);
        Ptr note_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2); gtk_widget_add_css_class(note_container, "tree-children");
        bool has_note = false; for (Note *candidate = state.notes; candidate; candidate = candidate->next)
            if (candidate->notebook == book) { has_note = true; break; }
        if (!has_note && !query[0] && !state.selected_tag[0] && !book->external) {
            Ptr first_note = navigation_button("document-new-symbolic", "Criar primeira nota");
            gtk_widget_add_css_class(first_note, "empty-action");
            g_signal_connect_data(first_note, "clicked", (void *)create_note_in_notebook, book, NULL, 0);
            gtk_box_append(note_container, first_note);
        }
        for (Note *note = state.notes; note; note = note->next) {
            if (note->notebook != book) continue;
            bool matches = note_has_tag(note, state.selected_tag) &&
                (!query[0] || strcasestr(note_display_title(note), query) || strcasestr(note_display_tags(note), query));
            if (!matches) { char *contents = note_current_markdown(note); matches = strcasestr(contents, query) != NULL; free(contents); }
            if (matches && note_has_tag(note, state.selected_tag)) append_note_button(note, note_container);
        }
        Ptr expander = gtk_expander_new(NULL); gtk_widget_add_css_class(expander, "notebook-tree");
        book->expander = expander;
        gtk_expander_set_label_widget(expander, notebook_row); gtk_expander_set_child(expander, note_container);
        gtk_expander_set_expanded(expander, (book->expanded || query[0] || state.selected_tag[0]));
        g_signal_connect_data(expander, "notify::expanded", (void *)notebook_expanded_changed, book, NULL, 0);
        gtk_box_append(state.sidebar, expander);
    }
    if (state.notebooks && !visible_notebooks) {
        Ptr empty = gtk_label_new(tr("Sem resultados")); gtk_label_set_xalign(empty, 0.0f);
        gtk_widget_add_css_class(empty, "section-title"); margins(empty, 8, 8); gtk_box_append(state.sidebar, empty);
        Ptr hint = gtk_label_new(tr("Experimente outra pesquisa ou remova o filtro de tags."));
        gtk_label_set_wrap(hint, true); gtk_label_set_max_width_chars(hint, 24); gtk_label_set_xalign(hint, 0.0f);
        gtk_widget_add_css_class(hint, "empty-message"); margins(hint, 8, 4); gtk_box_append(state.sidebar, hint);
    }
    rebuild_tags();
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
    if (!note || note != state.active || state.loading || !gtk_editable_get_editable(entry)) return;
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

void finish_tab_title_edit(Ptr entry) {
    if (!gtk_editable_get_editable(entry)) return;
    gtk_editable_set_editable(entry, false);
    gtk_editable_select_region(entry, 0, 0);
    gtk_widget_set_can_focus(entry, false);
    gtk_widget_set_cursor_from_name(entry, "default");
    gtk_widget_remove_css_class(entry, "editing-title");
}

void tab_title_activated(Ptr entry, Ptr unused) {
    (void)unused;
    finish_tab_title_edit(entry);
    const char *mode = gtk_stack_get_visible_child_name(state.mode_stack);
    if (mode && !strcmp(mode, "split")) gtk_widget_grab_focus(state.split_source_view);
    else if (mode && !strcmp(mode, "editor")) gtk_widget_grab_focus(state.editor);
}

void tab_title_focus_left(Ptr controller, Ptr entry) {
    (void)controller;
    finish_tab_title_edit(entry);
}

void tab_title_pressed(Ptr gesture, int presses, double x, double y, Ptr unused) {
    (void)unused;
    if (presses != 2) return;
    Ptr picked = gtk_widget_pick(state.tabbar, x, y, 0);
    for (Ptr widget = picked; widget && widget != state.tabbar; widget = gtk_widget_get_parent(widget)) {
        for (Note *note = state.notes; note; note = note->next) if (note->tab_title == widget) {
            select_note(NULL, note);
            Ptr entry = note->tab_title;
            gtk_editable_set_editable(entry, true);
            gtk_widget_set_can_focus(entry, true);
            gtk_widget_set_cursor_from_name(entry, "text");
            gtk_widget_add_css_class(entry, "editing-title");
            gtk_widget_grab_focus(entry);
            gtk_editable_select_region(entry, 0, -1);
            gtk_gesture_set_state(gesture, GTK_EVENT_SEQUENCE_CLAIMED);
            return;
        }
    }
}

void sync_workspace(void) {
    bool active = state.active != NULL;
    if (state.workspace_stack) gtk_stack_set_visible_child_name(state.workspace_stack, active ? "document" : "welcome");
    if (state.formatbar) gtk_widget_set_sensitive(state.formatbar, active);
    if (state.metadata_button) gtk_widget_set_sensitive(state.metadata_button, active);
    if (state.export_button) gtk_widget_set_sensitive(state.export_button, active);
    if (state.stats) gtk_widget_set_visible(state.stats, active);
    if (state.line_badge) gtk_widget_set_visible(state.line_badge, active);
    if (state.header_title) gtk_widget_set_visible(state.header_title, !active);
    if (state.header_tabs) gtk_widget_set_visible(state.header_tabs, active);
    if (!active && state.status) gtk_label_set_text(state.status, tr("Crie ou abra uma nota"));
}

void rebuild_tabs(void) {
    sync_workspace();
    if (!state.tabbar) return;
    while (gtk_widget_get_first_child(state.tabbar))
        gtk_box_remove(state.tabbar, gtk_widget_get_first_child(state.tabbar));
    for (Note *note = state.notes; note; note = note->next) { note->dirty_indicator = NULL; note->tab_title = NULL; }
    for (Note *note = state.notes; note; note = note->next) if (note->opened) {
        Ptr tab;
        if (note == state.active) {
            tab = gtk_entry_new();
            gtk_editable_set_text(tab, note_display_title(note));
            if (gtk_editable_set_width_chars) gtk_editable_set_width_chars(tab, 14);
            gtk_widget_add_css_class(tab, "tab");
            gtk_widget_add_css_class(tab, "active-tab");
            gtk_widget_add_css_class(tab, "tab-title");
            gtk_editable_set_editable(tab, false);
            gtk_widget_set_can_focus(tab, false);
            gtk_widget_set_cursor_from_name(tab, "default");
            g_signal_connect_data(tab, "changed", (void *)active_tab_title_changed, note, NULL, 0);
            g_signal_connect_data(tab, "activate", (void *)tab_title_activated, NULL, NULL, 0);
            Ptr focus = gtk_event_controller_focus_new();
            g_signal_connect_data(focus, "leave", (void *)tab_title_focus_left, tab, NULL, 0);
            gtk_widget_add_controller(tab, focus);
        } else {
            tab = gtk_button_new_with_label("");
            Ptr label = gtk_label_new(note_display_title(note));
            if (gtk_label_set_max_width_chars) gtk_label_set_max_width_chars(label, 16);
            if (gtk_label_set_ellipsize) gtk_label_set_ellipsize(label, 3);
            gtk_label_set_xalign(label, 0.0f);
            gtk_button_set_child(tab, label);
            gtk_widget_add_css_class(tab, "tab");
            g_signal_connect_data(tab, "clicked", (void *)select_note, note, NULL, 0);
        }
        note->tab_title = tab;
        gtk_widget_set_tooltip_text(tab, note_display_title(note));
        Ptr group = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_add_css_class(group, "document-tab");
        if (note == state.active) gtk_widget_add_css_class(group, "current-tab");
        gtk_box_append(group, tab);
        Ptr dirty = gtk_label_new("●"); gtk_widget_add_css_class(dirty, "dirty-indicator");
        gtk_widget_set_tooltip_text(dirty, tr("Alterações por guardar")); gtk_widget_set_visible(dirty, note->dirty);
        gtk_box_append(group, dirty); note->dirty_indicator = dirty;
        Ptr close = icon_button("window-close-symbolic", "Fechar separador"); gtk_widget_add_css_class(close, "tab-close");
        g_signal_connect_data(close, "clicked", (void *)close_tab, note, NULL, 0); gtk_box_append(group, close);
        gtk_box_append(state.tabbar, group);
    }
}

void search_changed(Ptr entry, Ptr unused) {
    (void)entry; (void)unused; rebuild_sidebar();
}

void unique_path(char *output, size_t size, const char *parent, const char *base) {
    snprintf(output, size, "%s/%s", parent, base);
    for (unsigned index = 2; access(output, F_OK) == 0; index++)
        snprintf(output, size, "%s/%s %u", parent, base, index);
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
    state.active_notebook->expanded = true;
    note->next = state.notes; state.notes = note; rebuild_sidebar(); select_note(NULL, note);
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
            load_library(); rebuild_sidebar(); rebuild_tabs();
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

bool copy_tree(const char *source, const char *destination) {
    struct stat info;
    if (lstat(source, &info) != 0) return false;
    if (S_ISREG(info.st_mode)) return copy_file(source, destination);
    if (!S_ISDIR(info.st_mode) || !make_directory(destination)) return false;
    DIR *directory = opendir(source); if (!directory) return false;
    bool ok = true; struct dirent *entry;
    while (ok && (entry = readdir(directory))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char from[PATH_MAX], to[PATH_MAX];
        if (snprintf(from, sizeof from, "%s/%s", source, entry->d_name) >= (int)sizeof from ||
            snprintf(to, sizeof to, "%s/%s", destination, entry->d_name) >= (int)sizeof to) { ok = false; break; }
        ok = copy_tree(from, to);
    }
    closedir(directory); return ok;
}

void remove_tree(const char *path) {
    struct stat info; if (lstat(path, &info) != 0) return;
    if (!S_ISDIR(info.st_mode)) { unlink(path); return; }
    DIR *directory = opendir(path); if (!directory) return; struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char child[PATH_MAX];
        if (snprintf(child, sizeof child, "%s/%s", path, entry->d_name) < (int)sizeof child) remove_tree(child);
    }
    closedir(directory); rmdir(path);
}

bool archive_path_safe(const char *path, char top[256]) {
    if (!path[0] || path[0] == '/' || strchr(path, '\\') || strchr(path, '\n') || strchr(path, '\r')) return false;
    char copy[PATH_MAX]; if (snprintf(copy, sizeof copy, "%s", path) >= (int)sizeof copy) return false;
    char *slash = strchr(copy, '/'); if (slash) *slash = '\0';
    if (!copy[0] || strcmp(copy, ".") == 0 || strcmp(copy, "..") == 0) return false;
    for (const char *part = path; part && *part;) {
        const char *next = strchr(part, '/'); size_t length = next ? (size_t)(next - part) : strlen(part);
        if ((length == 1 && part[0] == '.') || (length == 2 && part[0] == '.' && part[1] == '.')) return false;
        part = next ? next + 1 : NULL;
    }
    if (!top[0]) snprintf(top, 256, "%s", copy);
    return strcmp(top, copy) == 0;
}

bool archive_entries_safe(const char *archive, char top[256]) {
    int output[2]; if (pipe(output) != 0) return false;
    pid_t child = fork();
    if (child == 0) {
        close(output[0]); dup2(output[1], STDOUT_FILENO); close(output[1]);
        execlp("unzip", "unzip", "-Z1", archive, (char *)NULL); _exit(127);
    }
    close(output[1]); FILE *listing = fdopen(output[0], "r"); bool ok = child > 0 && listing != NULL, any = false;
    char entry[PATH_MAX + 2];
    while (ok && fgets(entry, sizeof entry, listing)) {
        size_t length = strlen(entry);
        if (!length || (entry[length - 1] != '\n' && !feof(listing))) { ok = false; break; }
        entry[strcspn(entry, "\r\n")] = '\0';
        if (!archive_path_safe(entry, top)) ok = false; else any = true;
    }
    if (listing) fclose(listing); else close(output[0]);
    int status = 0; bool exited = child > 0 && waitpid(child, &status, 0) > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    return ok && any && exited && top[0];
}

bool export_notebook_archive(Notebook *notebook, const char *destination) {
    if (!notebook || notebook->external || !destination || !*destination) return false;
    save_notebook_metadata(notebook);
    char parent[PATH_MAX], name[NAME_MAX + 1]; snprintf(parent, sizeof parent, "%s", notebook->path);
    char *slash = strrchr(parent, '/');
    if (!slash || !slash[1]) return false;
    snprintf(name, sizeof name, "%s", slash + 1); if (slash == parent) slash[1] = '\0'; else *slash = '\0';
    unlink(destination); pid_t child = fork();
    if (child == 0) { if (chdir(parent) != 0) _exit(127); execlp("zip", "zip", "-q", "-r", destination, name, (char *)NULL); _exit(127); }
    int status = 0; return child > 0 && waitpid(child, &status, 0) > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool import_notebook_archive(const char *archive, char imported_path[PATH_MAX]) {
    char top[256] = ""; if (!archive_entries_safe(archive, top)) return false;
    char temporary[] = "/tmp/notemd-import-XXXXXX"; if (!mkdtemp(temporary)) return false;
    pid_t child = fork();
    if (child == 0) { execlp("unzip", "unzip", "-qq", archive, "-d", temporary, (char *)NULL); _exit(127); }
    int status = 0; bool ok = child > 0 && waitpid(child, &status, 0) > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    char source[PATH_MAX]; snprintf(source, sizeof source, "%s/%s", temporary, top);
    char safe[256]; safe_title(top, safe);
    if (safe[0] == '.') { char visible[256]; snprintf(visible, sizeof visible, "Notebook %s", safe + 1); snprintf(safe, sizeof safe, "%s", visible); }
    unique_path(imported_path, PATH_MAX, state.root, safe);
    if (ok) ok = is_directory(source) && copy_tree(source, imported_path);
    if (!ok) { remove_tree(imported_path); imported_path[0] = '\0'; }
    remove_tree(temporary); return ok;
}

Notebook *load_imported_notebook(const char *path) {
    Notebook *notebook = calloc(1, sizeof *notebook); if (!notebook) return NULL;
    const char *name = strrchr(path, '/'); snprintf(notebook->title, sizeof notebook->title, "%s", name ? name + 1 : path);
    snprintf(notebook->path, sizeof notebook->path, "%s", path); notebook->expanded = true;
    extract_notebook_metadata(notebook); notebook->metadata_loaded = true;
    notebook->next = state.notebooks; state.notebooks = notebook;
    DIR *directory = opendir(path); if (!directory) return notebook; struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (entry->d_name[0] == '.') continue;
        char note_path[PATH_MAX], markdown[PATH_MAX]; snprintf(note_path, sizeof note_path, "%s/%s", path, entry->d_name);
        snprintf(markdown, sizeof markdown, "%s/note.md", note_path);
        if (!is_directory(note_path) || access(markdown, R_OK) != 0) continue;
        Note *note = calloc(1, sizeof *note); if (!note) continue;
        snprintf(note->title, sizeof note->title, "%s", entry->d_name); snprintf(note->directory, sizeof note->directory, "%s", note_path);
        snprintf(note->markdown_path, sizeof note->markdown_path, "%s", markdown); note->notebook = notebook; note->order = 100000;
        make_uuid(note->id); extract_metadata(note); note->metadata_loaded = true;
        for (size_t index = 0; index < notebook->note_order_count; index++)
            if (strcmp(notebook->note_order[index], note->id) == 0) { note->order = (int)index; break; }
        note->next = state.notes; state.notes = note;
    }
    closedir(directory); resort_library_notes(); return notebook;
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


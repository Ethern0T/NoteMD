#include "state.h"

void die(const char *message) {
    fprintf(stderr, "NoteMD: %s\n", message);
    exit(EXIT_FAILURE);
}
void margins(Ptr widget, int horizontal, int vertical) {
    gtk_widget_set_margin_start(widget, horizontal); gtk_widget_set_margin_end(widget, horizontal);
    gtk_widget_set_margin_top(widget, vertical); gtk_widget_set_margin_bottom(widget, vertical);
}
Ptr icon_button(const char *icon, const char *tooltip) {
    Ptr button = gtk_button_new_from_icon_name(icon); gtk_widget_set_tooltip_text(button, tooltip);
    gtk_widget_add_css_class(button, "icon-button"); return button;
}
Ptr navigation_button(const char *icon, const char *text) {
    Ptr button = gtk_button_new_with_label(""); Ptr row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    Ptr image = gtk_image_new_from_icon_name(icon); Ptr label = gtk_label_new(text); gtk_label_set_xalign(label, 0.0f);
    gtk_widget_set_hexpand(label, true); gtk_box_append(row, image); gtk_box_append(row, label); gtk_button_set_child(button, row);
    return button;
}
Ptr text_button(const char *text, const char *css_class) {
    Ptr button = gtk_button_new_with_label("");
    Ptr label = gtk_label_new(text);
    gtk_label_set_xalign(label, 0.0f);
    gtk_widget_set_hexpand(label, true);
    gtk_button_set_child(button, label);
    if (css_class) gtk_widget_add_css_class(button, css_class);
    return button;
}
gboolean release_popover_reference(Ptr popover) {
    g_object_unref(popover); return false;
}
void active_popover_closed(Ptr popover, Ptr unused) {
    (void)unused;
    if (state.active_popover == popover) state.active_popover = NULL;
    g_object_ref(popover); gtk_widget_unparent(popover);
    g_timeout_add(1, (void *)release_popover_reference, popover);
}
void dismiss_active_popover(void) {
    Ptr popover = state.active_popover; if (!popover) return;
    gtk_popover_popdown(popover);
    if (state.active_popover == popover) {
        state.active_popover = NULL; gtk_widget_unparent(popover);
    }
}
void present_popover(Ptr popover, Ptr anchor) {
    dismiss_active_popover(); state.active_popover = popover;
    g_signal_connect_data(popover, "closed", (void *)active_popover_closed, NULL, NULL, 0);
    gtk_widget_set_parent(popover, anchor); gtk_popover_popup(popover);
}
void load_style(void) {
    Ptr display = gdk_display_get_default();
    if (!display) return;
    Ptr provider = gtk_css_provider_new();
    char path[PATH_MAX]; snprintf(path, sizeof path, "%s/style.css", state.data_dir);
    gtk_css_provider_load_from_path(provider, path);
    gtk_style_context_add_provider_for_display(display, provider, 600);
    g_object_unref(provider);
}
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
char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return strdup("");
    fseek(file, 0, SEEK_END); long size = ftell(file); rewind(file);
    char *data = calloc((size_t)size + 1, 1);
    if (data) fread(data, 1, (size_t)size, file);
    fclose(file);
    return data;
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
void unique_path(char *output, size_t size, const char *parent, const char *base) {
    snprintf(output, size, "%s/%s", parent, base);
    for (unsigned index = 2; access(output, F_OK) == 0; index++)
        snprintf(output, size, "%s/%s %u", parent, base, index);
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

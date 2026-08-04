#include "state.h"

void save_settings(void) {
    char path[PATH_MAX], directory[PATH_MAX]; config_path(path);
    snprintf(directory, sizeof directory, "%s", path); char *slash = strrchr(directory, '/');
    if (slash) { *slash = '\0'; make_directory(directory); }
    size_t size = PATH_MAX + 256;
    for (Note *note = state.notes; note; note = note->next) if (note->external) size += strlen(note->markdown_path) + 12;
    char *contents = calloc(size, 1); size_t used = (size_t)snprintf(contents, size,
        "root=%s\ntheme=%s\nlanguage=%s\nautosave=%d\n", state.root, state.theme, state.language, state.autosave ? 1 : 0);
    for (Note *note = state.notes; note && used < size; note = note->next) if (note->external)
        used += (size_t)snprintf(contents + used, size - used, "external=%s\n", note->markdown_path);
    write_atomic(path, contents); free(contents);
}
void load_settings(void) {
    snprintf(state.theme, sizeof state.theme, "system");
    snprintf(state.language, sizeof state.language, "pt");
    state.autosave = true;
    char path[PATH_MAX]; config_path(path); char *contents = read_file(path), *line = contents;
    while (line && *line) {
        char *next = strchr(line, '\n'); if (next) *next = '\0';
        if (strncmp(line, "root=", 5) == 0) snprintf(state.root, sizeof state.root, "%s", line + 5);
        else if (strncmp(line, "theme=", 6) == 0 && line[6])
            snprintf(state.theme, sizeof state.theme, "%s", line + 6);
        else if (strncmp(line, "autosave=", 9) == 0) state.autosave = line[9] != '0';
        else if (strncmp(line, "language=", 9) == 0 && line[9]) snprintf(state.language, sizeof state.language, "%s", line + 9);
        else if (strncmp(line, "external=", 9) == 0 && state.external_count < 32)
            snprintf(state.external_paths[state.external_count++], PATH_MAX, "%s", line + 9);
        line = next ? next + 1 : NULL;
    }
    free(contents);
}
static bool is_system_dark(void) {
    gboolean prefer_dark = false;
    Ptr settings = gtk_settings_get_default ? gtk_settings_get_default() : NULL;
    if (settings) {
        g_object_get(settings, "gtk-application-prefer-dark-theme", &prefer_dark, NULL);
        if (prefer_dark) return true;
        char *theme_name = NULL;
        g_object_get(settings, "gtk-theme-name", &theme_name, NULL);
        if (theme_name) {
            bool dark = (strcasestr(theme_name, "dark") != NULL ||
                         strcasestr(theme_name, "black") != NULL ||
                         strcasestr(theme_name, "night") != NULL ||
                         strcasestr(theme_name, "breeze-dark") != NULL);
            g_free(theme_name);
            return dark;
        }
    }
    return false;
}

void apply_syntax_theme(void) {
    const char *heading = "#78a9ff", *emphasis = "#ff9f66", *code = "#c29df1";
    const char *code_background = "#252a34", *link = "#68d8e8", *list = "#68d391", *quote = "#9fa9bf";

    bool use_light = strcmp(state.theme, "white") == 0 || strcmp(state.theme, "solarized-light") == 0;
    if (strcmp(state.theme, "system") == 0 && !is_system_dark()) {
        use_light = true;
    }
    if (use_light) {
        heading = strcmp(state.theme, "solarized-light") == 0 ? "#268bd2" : "#0969da";
        emphasis = strcmp(state.theme, "solarized-light") == 0 ? "#cb4b16" : "#bc4c00";
        code = strcmp(state.theme, "solarized-light") == 0 ? "#6c71c4" : "#8250df";
        code_background = strcmp(state.theme, "solarized-light") == 0 ? "#eee8d5" : "#f3f4f6";
        link = heading; list = strcmp(state.theme, "solarized-light") == 0 ? "#859900" : "#1a7f37";
        quote = strcmp(state.theme, "solarized-light") == 0 ? "#839496" : "#57606a";
    } else if (strcmp(state.theme, "black") == 0) {
        heading = "#79c0ff"; emphasis = "#ffa657"; code = "#d2a8ff"; code_background = "#161b22";
        link = "#58a6ff"; list = "#7ee787"; quote = "#8b949e";
    } else if (strcmp(state.theme, "dracula") == 0) {
        heading = "#bd93f9"; emphasis = "#ffb86c"; code = "#ff79c6"; code_background = "#44475a";
        link = "#8be9fd"; list = "#50fa7b"; quote = "#6272a4";
    } else if (strcmp(state.theme, "monokai") == 0) {
        heading = "#a6e22e"; emphasis = "#fd971f"; code = "#ae81ff"; code_background = "#3e3d32";
        link = "#66d9ef"; list = "#a6e22e"; quote = "#75715e";
    } else if (strcmp(state.theme, "tokyo-night") == 0) {
        heading = "#7aa2f7"; emphasis = "#ff9e64"; code = "#bb9af7"; code_background = "#24283b";
        link = "#7dcfff"; list = "#9ece6a"; quote = "#565f89";
    }
    if (state.syntax_heading) g_object_set(state.syntax_heading, "foreground", heading, NULL);
    if (state.syntax_emphasis) g_object_set(state.syntax_emphasis, "foreground", emphasis, NULL);
    if (state.syntax_code) g_object_set(state.syntax_code, "foreground", code, "background", code_background, NULL);
    if (state.syntax_code_block) g_object_set(state.syntax_code_block, "foreground", code, "background", code_background, "paragraph-background", code_background, NULL);
    if (state.syntax_link) g_object_set(state.syntax_link, "foreground", link, NULL);
    if (state.syntax_list) g_object_set(state.syntax_list, "foreground", list, NULL);
    if (state.syntax_quote) g_object_set(state.syntax_quote, "foreground", quote, NULL);
}
void apply_theme(const char *theme) {
    if (!theme || !*theme) theme = "system";
    if (theme != state.theme) snprintf(state.theme, sizeof state.theme, "%s", theme);
    char path[PATH_MAX]; snprintf(path, sizeof path, "%s/themes/%s.css", state.data_dir, theme);
    Ptr display = gdk_display_get_default();
    if (state.theme_provider && display) {
        gtk_style_context_remove_provider_for_display(display, state.theme_provider);
        g_object_unref(state.theme_provider); state.theme_provider = NULL;
    }
    state.theme_provider = gtk_css_provider_new(); gtk_css_provider_load_from_path(state.theme_provider, path);
    if (display) gtk_style_context_add_provider_for_display(display, state.theme_provider, 700);

    Ptr settings = gtk_settings_get_default ? gtk_settings_get_default() : NULL;
    if (settings) {
        gboolean prefer_dark = false;
        if (strcmp(theme, "system") == 0) {
            prefer_dark = is_system_dark();
        } else if (strcmp(theme, "black") == 0 || strcmp(theme, "dracula") == 0 ||
                   strcmp(theme, "monokai") == 0 || strcmp(theme, "tokyo-night") == 0) {
            prefer_dark = true;
        } else {
            prefer_dark = false;
        }
        g_object_set(settings, "gtk-application-prefer-dark-theme", prefer_dark, NULL);
    }

    apply_syntax_theme();
    load_style();
    save_settings();
}
void rebuild_sidebar(void) {
    dismiss_active_popover();
    while (gtk_widget_get_first_child(state.sidebar))
        gtk_box_remove(state.sidebar, gtk_widget_get_first_child(state.sidebar));
    state.context_menu_count = 0;
    for (Notebook *book = state.notebooks; book; book = book->next) {
        book->button = NULL; book->add_button = NULL; book->menu_button = NULL; book->expander = NULL; book->context_gesture = NULL;
    }
    for (Note *note = state.notes; note; note = note->next) { note->button = NULL; note->menu_button = NULL; note->context_gesture = NULL; }
    Ptr heading = gtk_label_new(tr("Notebooks"));
    gtk_label_set_xalign(heading, 0.0f); gtk_widget_add_css_class(heading, "section-title");
    margins(heading, 8, 8); gtk_box_append(state.sidebar, heading);
    state.root_label = gtk_label_new(state.root); gtk_label_set_xalign(state.root_label, 0.0f);
    gtk_widget_add_css_class(state.root_label, "storage-path");
    if (!state.notebooks) {
        Ptr empty = gtk_label_new("Ainda não existem notebooks"); gtk_label_set_xalign(empty, 0.0f);
        gtk_widget_add_css_class(empty, "empty-message"); margins(empty, 12, 14); gtk_box_append(state.sidebar, empty);
        Ptr create = navigation_button("folder-new-symbolic", "Criar primeiro notebook");
        gtk_widget_add_css_class(create, "empty-action");
        g_signal_connect_data(create, "clicked", (void *)new_notebook, NULL, NULL, 0); gtk_box_append(state.sidebar, create);
    }
    const char *query = state.search ? gtk_editable_get_text(state.search) : "";
    for (Notebook *book = state.notebooks; book; book = book->next) {
        bool any = false;
        for (Note *note = state.notes; note; note = note->next) if (note->notebook == book && note_has_tag(note, state.selected_tag)) {
            any = !query[0] || strcasestr(note_display_title(note), query) || strcasestr(note_display_tags(note), query);
            if (!any) { char *contents = note_current_markdown(note); any = strcasestr(contents, query) != NULL; free(contents); }
            if (any) break;
        }
        if (!query[0] && !state.selected_tag[0]) any = true;
        if (!any) continue;
        Ptr notebook = navigation_button(book->external ? "folder-remote-symbolic" : "folder-symbolic", book->title);
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
    rebuild_tags();
}
void show_preferences(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    Ptr window = gtk_window_new(); gtk_window_set_title(window, tr("Preferências"));
    gtk_window_set_default_size(window, 620, 430); gtk_window_set_transient_for(window, state.window);
    gtk_window_set_modal(window, true);
    Ptr box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12); margins(box, 24, 24);
    Ptr heading = gtk_label_new(tr("Aparência e armazenamento")); gtk_label_set_xalign(heading, 0.0f);
    gtk_widget_add_css_class(heading, "history-title"); gtk_box_append(box, heading);
    Ptr storage_label = gtk_label_new("Pasta das notas"); gtk_label_set_xalign(storage_label, 0.0f);
    gtk_widget_add_css_class(storage_label, "settings-label"); gtk_box_append(box, storage_label);
    Ptr storage = gtk_label_new(state.root); gtk_label_set_xalign(storage, 0.0f);
    gtk_widget_add_css_class(storage, "storage-path"); gtk_box_append(box, storage);
    Ptr change = icon_button("folder-open-symbolic", tr("Selecionar pasta das notas…"));
    g_signal_connect_data(change, "clicked", (void *)choose_folder, NULL, NULL, 0); gtk_box_append(box, change);
    Ptr autosave = gtk_check_button_new_with_label(tr("Gravação automática"));
    gtk_check_button_set_active(autosave, state.autosave);
    g_signal_connect_data(autosave, "toggled", (void *)autosave_changed, NULL, NULL, 0); gtk_box_append(box, autosave);
    gtk_box_append(box, gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    Ptr themes_label = gtk_label_new(tr("Tema do editor")); gtk_label_set_xalign(themes_label, 0.0f);
    gtk_widget_add_css_class(themes_label, "settings-label"); gtk_box_append(box, themes_label);
    static const char *theme_ids[] = {"white", "black", "system", "monokai", "tokyo-night", "dracula", "solarized-light"};
    const char *theme_labels[] = {"Branco", "Preto", tr("Sistema"), "Monokai", "Tokyo Night", "Dracula", "Solarized Light"};
    Ptr theme_rows = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5); Ptr row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    for (size_t index = 0; index < sizeof theme_ids / sizeof theme_ids[0]; index++) {
        if (index == 4) { gtk_box_append(theme_rows, row); row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5); }
        Ptr choice = gtk_button_new_with_label(theme_labels[index]);
        gtk_widget_add_css_class(choice, strcmp(state.theme, theme_ids[index]) == 0 ? "selected-setting" : "setting-choice");
        g_signal_connect_data(choice, "clicked", (void *)choose_theme, (Ptr)theme_ids[index], NULL, 0);
        gtk_box_append(row, choice);
    }
    gtk_box_append(theme_rows, row); gtk_box_append(box, theme_rows);
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
    gtk_window_set_child(window, box); gtk_window_present(window);
}
Ptr attach_library_context_menu(Ptr widget, Ptr target, bool notebook) {
    Ptr gesture = gtk_gesture_click_new(); gtk_gesture_single_set_button(gesture, 3);
    ContextTarget *context = calloc(1, sizeof *context);
    context->widget = widget; context->target = target; context->notebook = notebook;
    g_signal_connect_data(gesture, "pressed", (void *)library_context_pressed, context, (void *)free_signal_data, 0);
    gtk_widget_add_controller(widget, gesture);
    return gesture;
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
    highlight_editor(text);
    update_views(text);
    if (!state.block_syncing && object == state.editor_buffer) rebuild_block_editor();
    update_statistics(text);
    gtk_label_set_text(state.status, state.autosave ? tr("a guardar…") : tr("alterações por guardar")); g_free(text);
}
void toggle_sidebar(Ptr button, Ptr unused) {
    (void)button; (void)unused; if (!state.sidebar_panel) return;
    state.sidebar_hidden = !state.sidebar_hidden; gtk_widget_set_visible(state.sidebar_panel, !state.sidebar_hidden);
    if (state.sidebar_toggle) {
        gtk_button_set_icon_name(state.sidebar_toggle, state.sidebar_hidden ? "sidebar-show-symbolic" : "sidebar-hide-symbolic");
        gtk_widget_set_tooltip_text(state.sidebar_toggle, tr(state.sidebar_hidden ? "Mostrar biblioteca" : "Ocultar biblioteca"));
    }
}
void activate(Ptr app, Ptr unused) {
    (void)unused;
    load_style(); apply_theme(state.theme);
    state.window = gtk_application_window_new(app);
    gtk_window_set_title(state.window, "NoteMD");
    gtk_window_set_default_size(state.window, 1280, 800);
    g_signal_connect_data(state.window, "close-request", (void *)close_requested, NULL, NULL, 0);
    Ptr keys = gtk_event_controller_key_new();
    g_signal_connect_data(keys, "key-pressed", (void *)key_pressed, NULL, NULL, 0);
    gtk_widget_add_controller(state.window, keys);
    Ptr drop = gtk_drop_target_new(gdk_file_list_get_type(), GDK_ACTION_COPY);
    g_signal_connect_data(drop, "drop", (void *)files_dropped, NULL, NULL, 0);
    gtk_widget_add_controller(state.window, drop);
    g_timeout_add(2000, (void *)poll_external_file, NULL);

    Ptr outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    state.sidebar_toggle = icon_button("sidebar-hide-symbolic", tr("Ocultar biblioteca"));
    Ptr add_book = icon_button("folder-new-symbolic", tr("Novo notebook"));
    Ptr add_note = icon_button("document-new-symbolic", tr("Nova nota"));
    Ptr templates = icon_button("document-properties-symbolic", tr("Templates"));
    Ptr open = icon_button("document-open-symbolic", tr("Abrir…"));
    Ptr preferences = icon_button("preferences-system-symbolic", tr("Preferências"));
    Ptr history = icon_button("document-open-recent-symbolic", tr("Histórico"));
    Ptr export = icon_button("x-office-document-symbolic", "Exportar nota");
    Ptr save = icon_button("document-save-symbolic", tr("Guardar"));
    g_signal_connect_data(state.sidebar_toggle, "clicked", (void *)toggle_sidebar, NULL, NULL, 0);
    g_signal_connect_data(add_book, "clicked", (void *)new_notebook, NULL, NULL, 0);
    g_signal_connect_data(add_note, "clicked", (void *)new_note, NULL, NULL, 0);
    g_signal_connect_data(templates, "clicked", (void *)show_templates, NULL, NULL, 0);
    g_signal_connect_data(open, "clicked", (void *)open_markdown, NULL, NULL, 0);
    g_signal_connect_data(preferences, "clicked", (void *)show_preferences, NULL, NULL, 0);
    g_signal_connect_data(history, "clicked", (void *)show_history, NULL, NULL, 0);
    g_signal_connect_data(export, "clicked", (void *)show_export_menu, NULL, NULL, 0);
    g_signal_connect_data(save, "clicked", (void *)save_active, NULL, NULL, 0);
    gtk_widget_add_css_class(save, "suggested-action");

    Ptr paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL); gtk_paned_set_position(paned, 280);
    gtk_widget_set_vexpand(paned, true);
    Ptr side_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4); state.sidebar_panel = side_panel;
    gtk_widget_set_size_request(side_panel, 220, -1);
    gtk_widget_add_css_class(side_panel, "sidebar-panel");
    Ptr side_actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4); gtk_widget_add_css_class(side_actions, "sidebar-actions");
    Ptr side_new_book = icon_button("folder-new-symbolic", tr("Novo notebook"));
    Ptr side_new_note = icon_button("document-new-symbolic", tr("Nova nota")); state.side_new_note = side_new_note;
    Ptr side_templates = icon_button("document-properties-symbolic", tr("Templates"));
    Ptr side_preferences = icon_button("preferences-system-symbolic", tr("Preferências"));
    g_signal_connect_data(side_new_book, "clicked", (void *)new_notebook, NULL, NULL, 0);
    g_signal_connect_data(side_new_note, "clicked", (void *)new_note, NULL, NULL, 0);
    g_signal_connect_data(side_templates, "clicked", (void *)show_templates, NULL, NULL, 0);
    g_signal_connect_data(side_preferences, "clicked", (void *)show_preferences, NULL, NULL, 0);
    gtk_box_append(side_actions, side_new_book); gtk_box_append(side_actions, side_new_note);
    gtk_box_append(side_actions, side_templates); Ptr side_action_spacer = gtk_label_new("");
    gtk_widget_set_hexpand(side_action_spacer, true); gtk_box_append(side_actions, side_action_spacer);
    gtk_box_append(side_actions, side_preferences); gtk_box_append(side_panel, side_actions);
    state.search = gtk_search_entry_new(); gtk_search_entry_set_placeholder_text(state.search, tr("Pesquisar notas"));
    margins(state.search, 10, 8); g_signal_connect_data(state.search, "search-changed", (void *)search_changed, NULL, NULL, 0);
    gtk_box_append(side_panel, state.search);
    Ptr side_scroll = gtk_scrolled_window_new(); gtk_widget_set_vexpand(side_scroll, true);
    state.sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4); gtk_widget_add_css_class(state.sidebar, "sidebar");
    rebuild_sidebar();
    gtk_scrolled_window_set_child(side_scroll, state.sidebar); gtk_box_append(side_panel, side_scroll);
    gtk_box_append(side_panel, gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    state.tag_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3); gtk_widget_set_size_request(state.tag_box, -1, 130);
    margins(state.tag_box, 10, 6); gtk_box_append(side_panel, state.tag_box); rebuild_tags();
    gtk_paned_set_start_child(paned, side_panel);

    Ptr workspace = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    state.tabbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2); gtk_widget_add_css_class(state.tabbar, "tabbar");
    gtk_box_append(workspace, state.tabbar);
    Ptr identity = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12); margins(identity, 16, 7);
    Ptr title_actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4); gtk_widget_add_css_class(title_actions, "title-actions");
    gtk_box_append(title_actions, state.sidebar_toggle);
    gtk_box_append(title_actions, add_book); gtk_box_append(title_actions, add_note);
    gtk_box_append(title_actions, templates); gtk_box_append(title_actions, open);
    gtk_box_append(title_actions, preferences); gtk_box_append(title_actions, history);
    gtk_box_append(title_actions, export); gtk_box_append(title_actions, save);
    gtk_box_append(identity, title_actions);
    state.title = gtk_entry_new(); gtk_entry_set_placeholder_text(state.title, tr("Título da nota"));
    gtk_widget_add_css_class(state.title, "note-title"); gtk_widget_set_size_request(state.title, 320, -1);
    gtk_widget_set_visible(state.title, false);
    gtk_widget_set_hexpand(state.title, true);
    Ptr tag_area = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5); gtk_widget_add_css_class(tag_area, "note-tag-area");
    state.note_tags_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    Ptr tags_scroll = gtk_scrolled_window_new(); gtk_widget_set_size_request(tags_scroll, 210, -1);
    gtk_scrolled_window_set_policy(tags_scroll, GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    gtk_scrolled_window_set_child(tags_scroll, state.note_tags_box); gtk_box_append(tag_area, tags_scroll);
    state.tag_input = gtk_entry_new(); gtk_entry_set_placeholder_text(state.tag_input, tr("Adicionar tag"));
    gtk_widget_add_css_class(state.tag_input, "tag-input"); gtk_widget_set_size_request(state.tag_input, 108, -1);
    g_signal_connect_data(state.tag_input, "activate", (void *)add_note_tag, NULL, NULL, 0); gtk_box_append(tag_area, state.tag_input);
    Ptr add_tag = icon_button("list-add-symbolic", tr("Adicionar tag")); gtk_widget_add_css_class(add_tag, "tag-add");
    g_signal_connect_data(add_tag, "clicked", (void *)add_note_tag_clicked, NULL, NULL, 0); gtk_box_append(tag_area, add_tag);
    state.tags = gtk_entry_new(); gtk_widget_set_visible(state.tags, false); gtk_box_append(tag_area, state.tags);
    gtk_box_append(identity, state.title); gtk_box_append(identity, tag_area); gtk_box_append(workspace, identity);
    g_signal_connect_data(state.title, "changed", (void *)content_changed, NULL, NULL, 0);
    g_signal_connect_data(state.tags, "changed", (void *)content_changed, NULL, NULL, 0);
    state.linksbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5); margins(state.linksbar, 16, 3);
    gtk_widget_set_visible(state.linksbar, false);
    gtk_widget_add_css_class(state.linksbar, "linksbar"); gtk_box_append(workspace, state.linksbar);
    Ptr formatbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3); margins(formatbar, 12, 4);
    gtk_widget_add_css_class(formatbar, "formatbar");
    static const char *heading_levels[] = {"H", "H1", "H2", "H3", "H4", "H5", "H6", NULL};
    Ptr heading_selector = gtk_drop_down_new_from_strings(heading_levels); state.format_heading = heading_selector;
    gtk_drop_down_set_selected(heading_selector, 0);
    gtk_widget_set_tooltip_text(heading_selector, tr("Título")); gtk_widget_add_css_class(heading_selector, "heading-selector");
    g_signal_connect_data(heading_selector, "notify::selected", (void *)heading_format_changed, NULL, NULL, 0);
    gtk_box_append(formatbar, heading_selector);
    const char *color_options[] = {tr("Cor do texto"), tr("Vermelho"), tr("Laranja"), tr("Amarelo"),
        tr("Verde"), tr("Azul"), tr("Roxo"), NULL};
    Ptr color_selector = gtk_drop_down_new_from_strings(color_options); state.format_color = color_selector;
    gtk_drop_down_set_selected(color_selector, 0); gtk_widget_set_tooltip_text(color_selector, tr("Cor do texto"));
    gtk_widget_add_css_class(color_selector, "color-selector");
    g_signal_connect_data(color_selector, "notify::selected", (void *)text_color_changed, NULL, NULL, 0);
    gtk_box_append(formatbar, color_selector);
    struct { const char *icon, *tooltip, *markdown; } formats[] = {
        {"format-text-bold-symbolic", "Negrito", "**|**"}, {"format-text-italic-symbolic", "Itálico", "_|_"},
        {"format-text-strikethrough-symbolic", "Rasurado", "~~|~~"},
        {"insert-text-symbolic", "Código", "`|`"}, {"view-list-symbolic", "Lista", "- |"},
        {"format-justify-left-symbolic", "Lista numerada", "1. |"},
        {"checkbox-symbolic", "Tarefa", "- [ ] |"}, {"format-indent-more-symbolic", "Citação", "> |"},
        {"insert-link-symbolic", "Ligação", "[|](https://)"}
    };
    for (size_t i = 0; i < sizeof formats / sizeof formats[0]; i++) {
        Ptr button = icon_button(formats[i].icon, tr(formats[i].tooltip));
        g_signal_connect_data(button, "clicked", (void *)insert_markdown, (Ptr)formats[i].markdown, NULL, 0);
        gtk_box_append(formatbar, button);
    }
    Ptr table = icon_button("insert-table-symbolic", tr("Tabela"));
    g_signal_connect_data(table, "clicked", (void *)insert_literal,
        "| Coluna 1 | Coluna 2 |\n| --- | --- |\n| Valor | Valor |\n", NULL, 0); gtk_box_append(formatbar, table);
    Ptr image = icon_button("insert-image-symbolic", tr("Imagem…"));
    g_signal_connect_data(image, "clicked", (void *)insert_image, NULL, NULL, 0);
    gtk_box_append(formatbar, image);
    Ptr format_spacer = gtk_label_new(""); gtk_widget_set_hexpand(format_spacer, true); gtk_box_append(formatbar, format_spacer);
    Ptr modes = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2); gtk_widget_add_css_class(modes, "mode-switcher");
    state.mode_editor = icon_button("document-edit-symbolic", tr("Editar nota"));
    state.mode_visual = icon_button("view-grid-symbolic", tr("Editar visualmente"));
    state.mode_split = icon_button("view-dual-symbolic", tr("Editor e visualização"));
    gtk_widget_add_css_class(state.mode_editor, "mode-button"); gtk_widget_add_css_class(state.mode_editor, "active-mode");
    gtk_widget_add_css_class(state.mode_visual, "mode-button"); gtk_widget_add_css_class(state.mode_split, "mode-button");
    g_signal_connect_data(state.mode_editor, "clicked", (void *)select_editor_mode, "editor", NULL, 0);
    g_signal_connect_data(state.mode_visual, "clicked", (void *)select_editor_mode, "visual", NULL, 0);
    g_signal_connect_data(state.mode_split, "clicked", (void *)select_editor_mode, "split", NULL, 0);
    gtk_box_append(modes, state.mode_editor); gtk_box_append(modes, state.mode_visual); gtk_box_append(modes, state.mode_split);
    gtk_box_append(formatbar, modes); gtk_box_append(workspace, formatbar);

    Ptr stack = gtk_stack_new(); state.mode_stack = stack; gtk_widget_set_hexpand(stack, true); gtk_widget_set_vexpand(stack, true);
    state.editor = gtk_text_view_new(); gtk_text_view_set_editable(state.editor, true); gtk_text_view_set_cursor_visible(state.editor, true);
    gtk_widget_add_css_class(state.editor, "markdown-source");
    gtk_text_view_set_monospace(state.editor, true);
    gtk_text_view_set_wrap_mode(state.editor, GTK_WRAP_WORD_CHAR); gtk_text_view_set_left_margin(state.editor, 24);
    gtk_text_view_set_right_margin(state.editor, 24); gtk_text_view_set_top_margin(state.editor, 18);
    Ptr paste_keys = gtk_event_controller_key_new(); gtk_event_controller_set_propagation_phase(paste_keys, 1);
    g_signal_connect_data(paste_keys, "key-pressed", (void *)paste_image_key, NULL, NULL, 0);
    gtk_widget_add_controller(state.editor, paste_keys);
    state.editor_buffer = gtk_text_view_get_buffer(state.editor);
    gtk_text_buffer_set_enable_undo(state.editor_buffer, true);
    state.syntax_heading = gtk_text_buffer_create_tag(state.editor_buffer, "syntax-heading", "foreground", "#78a9ff", "weight", 700, NULL);
    state.syntax_emphasis = gtk_text_buffer_create_tag(state.editor_buffer, "syntax-emphasis", "foreground", "#ff9f66", "weight", 700, NULL);
    state.syntax_code = gtk_text_buffer_create_tag(state.editor_buffer, "syntax-code", "foreground", "#c29df1", "background", "#252a34", NULL);
    state.syntax_code_block = gtk_text_buffer_create_tag(state.editor_buffer, "syntax-code-block", "foreground", "#c29df1", "background", "#252a34", "paragraph-background", "#252a34", NULL);
    state.syntax_link = gtk_text_buffer_create_tag(state.editor_buffer, "syntax-link", "foreground", "#68d8e8", "underline", 1, NULL);
    state.syntax_list = gtk_text_buffer_create_tag(state.editor_buffer, "syntax-list", "foreground", "#68d391", NULL);
    state.syntax_quote = gtk_text_buffer_create_tag(state.editor_buffer, "syntax-quote", "foreground", "#9fa9bf", "style", 2, NULL);
    apply_syntax_theme();
    g_signal_connect_data(state.editor_buffer, "changed", (void *)content_changed, NULL, NULL, 0);
    Ptr editor_scroll = editor_scroller(state.editor);
    Ptr line_view = gtk_text_view_new(); gtk_text_view_set_editable(line_view, false); gtk_text_view_set_cursor_visible(line_view, false);
    gtk_text_view_set_monospace(line_view, true); gtk_text_view_set_top_margin(line_view, 18); gtk_text_view_set_right_margin(line_view, 8);
    gtk_widget_add_css_class(line_view, "line-numbers"); state.line_buffer = gtk_text_view_get_buffer(line_view);
    gtk_text_buffer_create_tag(state.line_buffer, "active-line", "foreground", "#78a9ff", "weight", 700, "background", "#253552", NULL);
    g_signal_connect_data(state.editor_buffer, "mark-set", (void *)cursor_mark_set, NULL, NULL, 0);
    Ptr line_scroll = gtk_scrolled_window_new(); gtk_widget_set_size_request(line_scroll, 52, -1);
    gtk_scrolled_window_set_policy(line_scroll, GTK_POLICY_NEVER, GTK_POLICY_NEVER); gtk_scrolled_window_set_child(line_scroll, line_view);
    gtk_scrolled_window_set_vadjustment(line_scroll, gtk_scrolled_window_get_vadjustment(editor_scroll));
    Ptr editor_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0); gtk_box_append(editor_row, line_scroll); gtk_box_append(editor_row, editor_scroll);
    gtk_stack_add_titled(stack, editor_row, "editor", tr("Editor"));
    Ptr visual_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(visual_page, "visual-editor");
    Ptr visual_toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4); margins(visual_toolbar, 18, 8);
    struct { const char *icon, *label, *text; } block_types[] = {
        {"insert-text-symbolic", "Parágrafo", "Texto"}, {"format-text-larger-symbolic", "Título", "## Título"},
        {"view-list-symbolic", "Lista", "- Item"}, {"checkbox-symbolic", "Tarefa", "- [ ] Tarefa"},
        {"format-indent-more-symbolic", "Citação", "> Citação"}, {"insert-text-symbolic", "Código", "```\nCódigo\n```"},
        {"insert-table-symbolic", "Tabela", "| Coluna 1 | Coluna 2 |\n| --- | --- |\n| Valor | Valor |"},
        {"list-remove-symbolic", "Separador", "---"}};
    for (size_t index = 0; index < sizeof block_types / sizeof block_types[0]; index++) {
        Ptr add = icon_button(block_types[index].icon, tr(block_types[index].label));
        g_signal_connect_data(add, "clicked", (void *)add_visual_block, (Ptr)block_types[index].text, NULL, 0);
        gtk_box_append(visual_toolbar, add);
    }
    state.block_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(state.block_box, "visual-blocks"); margins(state.block_box, 28, 18);
    Ptr visual_scroll = gtk_scrolled_window_new(); gtk_widget_set_vexpand(visual_scroll, true);
    gtk_scrolled_window_set_child(visual_scroll, state.block_box);
    gtk_box_append(visual_page, visual_toolbar); gtk_box_append(visual_page, visual_scroll);
    gtk_stack_add_titled(stack, visual_page, "visual", tr("Visual"));
    Ptr divided = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL); gtk_paned_set_position(divided, 480);
    state.split_source_view = gtk_text_view_new(); gtk_text_view_set_editable(state.split_source_view, true);
    gtk_text_view_set_cursor_visible(state.split_source_view, true); state.split_buffer = gtk_text_view_get_buffer(state.split_source_view);
    gtk_text_buffer_set_enable_undo(state.split_buffer, true);
    gtk_text_view_set_monospace(state.split_source_view, true); gtk_text_view_set_wrap_mode(state.split_source_view, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(state.split_source_view, 24); gtk_text_view_set_right_margin(state.split_source_view, 24);
    gtk_text_view_set_top_margin(state.split_source_view, 18); gtk_widget_add_css_class(state.split_source_view, "split-source");
    gtk_widget_add_css_class(state.split_source_view, "markdown-source");
    gtk_widget_set_hexpand(state.split_source_view, true);
    gtk_widget_set_size_request(state.split_source_view, 360, -1);
    g_signal_connect_data(state.split_buffer, "changed", (void *)split_content_changed, NULL, NULL, 0);
    g_signal_connect_data(state.split_buffer, "mark-set", (void *)cursor_mark_set, NULL, NULL, 0);
    Ptr split_paste_keys = gtk_event_controller_key_new(); gtk_event_controller_set_propagation_phase(split_paste_keys, 1);
    g_signal_connect_data(split_paste_keys, "key-pressed", (void *)paste_image_key, NULL, NULL, 0);
    gtk_widget_add_controller(state.split_source_view, split_paste_keys);
    state.split_preview_view = readonly_markdown_view(&state.split_preview_buffer, "preview");
    gtk_widget_set_size_request(state.split_preview_view, 360, -1);
    gtk_paned_set_start_child(divided, editor_scroller(state.split_source_view));
    gtk_paned_set_end_child(divided, editor_scroller(state.split_preview_view));
    gtk_stack_add_titled(stack, divided, "split", tr("Dividido"));
    gtk_stack_set_visible_child_name(stack, "editor");
    gtk_box_append(workspace, stack);
    Ptr statusbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9); gtk_widget_add_css_class(statusbar, "statusbar");
    state.status = gtk_label_new(tr("Selecione uma nota")); gtk_label_set_xalign(state.status, 0.0f);
    gtk_widget_set_hexpand(state.status, true); gtk_widget_add_css_class(state.status, "status");
    state.stats = gtk_label_new("0 palavras  •  0 carateres"); gtk_widget_add_css_class(state.stats, "document-stats");
    state.line_badge = gtk_label_new("1 linha"); gtk_widget_add_css_class(state.line_badge, "line-badge");
    Ptr markdown_badge = gtk_label_new("Markdown"); gtk_widget_add_css_class(markdown_badge, "markdown-badge");
    gtk_box_append(statusbar, state.status); gtk_box_append(statusbar, state.stats);
    gtk_box_append(statusbar, state.line_badge); gtk_box_append(statusbar, markdown_badge);
    gtk_box_append(workspace, statusbar); gtk_paned_set_end_child(paned, workspace);
    gtk_box_append(outer, paned); gtk_window_set_child(state.window, outer); gtk_window_present(state.window);
    if (state.notes) select_note(NULL, state.notes);
    if (state.ui_test) {
        if (state.empty_ui_test) g_timeout_add(350, (void *)run_empty_ui_assertions, NULL);
        else { select_editor_mode(NULL, "split"); g_timeout_add(350, (void *)run_ui_assertions, NULL); }
    }
}

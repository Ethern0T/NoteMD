#include "internal.h"

void toggle_sidebar(Ptr button, Ptr unused) {
    (void)button; (void)unused; if (!state.sidebar_panel) return;
    state.sidebar_hidden = !state.sidebar_hidden; gtk_widget_set_visible(state.sidebar_panel, !state.sidebar_hidden);
    if (state.sidebar_toggle) {
        gtk_button_set_icon_name(state.sidebar_toggle, state.sidebar_hidden ? "sidebar-show-symbolic" : "sidebar-hide-symbolic");
        gtk_widget_set_tooltip_text(state.sidebar_toggle, tr(state.sidebar_hidden ? "Mostrar biblioteca" : "Ocultar biblioteca"));
    }
}

void resize_window_pressed(Ptr gesture, int presses, double x, double y, Ptr unused) {
    (void)x; (void)y; (void)unused;
    if (presses != 1 || gtk_window_is_maximized(state.window) || gtk_window_is_fullscreen(state.window)) return;
    Ptr event = gtk_event_controller_get_current_event(gesture);
    Ptr surface = gtk_native_get_surface(state.window);
    double surface_x, surface_y;
    if (!event || !surface || !gdk_event_get_position(event, &surface_x, &surface_y)) return;
    gtk_gesture_set_state(gesture, GTK_EVENT_SEQUENCE_CLAIMED);
    gdk_toplevel_begin_resize(surface, GDK_SURFACE_EDGE_SOUTH_EAST, gdk_event_get_device(event),
        1, surface_x, surface_y, gdk_event_get_time(event));
}

gboolean key_pressed(Ptr controller, guint keyval, guint keycode, guint modifiers, Ptr unused) {
    (void)controller; (void)keycode; (void)unused;
    if (!(modifiers & GDK_CONTROL_MASK)) return false;
    switch (keyval) {
        case '+': case '=': case 0xffab: set_editor_font_size(state.editor_font_size + 1); return true;
        case '-': case 0xffad: set_editor_font_size(state.editor_font_size - 1); return true;
        case '0': case 0xffb0: set_editor_font_size(EDITOR_FONT_DEFAULT); return true;
        case '1': select_editor_mode(NULL, "editor"); return true;
        case '2': select_editor_mode(NULL, "visual"); return true;
        case '3': select_editor_mode(NULL, "split"); return true;
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
            gtk_text_buffer_set_text(state.editor_buffer, content, -1); update_views(content);
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
    gtk_text_buffer_set_text(state.editor_buffer, content, -1); update_views(content); state.loading = false;
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

bool check_long_note_resize(void) {
    char *original = editor_text(); bool was_loading = state.loading;
    char original_mode[32]; snprintf(original_mode, sizeof original_mode, "%s", gtk_stack_get_visible_child_name(state.mode_stack));
    const char *modes[] = {"editor", "visual", "split"};
    int short_heights[3] = {0}, long_heights[3] = {0};
    const char *line = "Linha para testar o redimensionamento.\n";
    size_t line_length = strlen(line), count = 2000;
    char *long_text = malloc(count * line_length + 1);
    if (!long_text) { g_free(original); return false; }
    for (size_t index = 0; index < count; index++) memcpy(long_text + index * line_length, line, line_length);
    long_text[count * line_length] = '\0';
    state.loading = true;
    bool ok = true;
    for (size_t sample = 0; sample < 2; sample++) {
        const char *text = sample ? long_text : line;
        gtk_text_buffer_set_text(state.editor_buffer, text, -1); update_views(text);
        for (size_t index = 0; index < 3; index++) {
            select_editor_mode(NULL, (Ptr)modes[index]);
            int minimum = 0, natural = 0;
            gtk_widget_measure(state.window, GTK_ORIENTATION_VERTICAL, 800, &minimum, &natural, NULL, NULL);
            if (sample) long_heights[index] = minimum; else short_heights[index] = minimum;
        }
    }
    for (size_t index = 0; index < 3; index++) {
        bool mode_ok = long_heights[index] <= short_heights[index] + 2 && long_heights[index] <= 500;
        printf("UI resize-2000-linhas-%s (altura mínima %d → %dpx): %s\n",
            modes[index], short_heights[index], long_heights[index], mode_ok ? "OK" : "FALHOU");
        ok = ok && mode_ok;
    }
    gtk_text_buffer_set_text(state.editor_buffer, original, -1); update_views(original);
    select_editor_mode(NULL, original_mode); state.loading = was_loading;
    free(long_text); g_free(original); return ok;
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
    bool collapsed_ok = state.sidebar_hidden && !gtk_widget_get_visible(state.sidebar_panel) &&
        !gtk_expander_get_expanded(state.tags_section) &&
        gtk_widget_has_css_class(state.sidebar, "library-tree") && !gtk_widget_has_css_class(state.sidebar, "sidebar");
    for (Notebook *book = state.notebooks; book; book = book->next)
        collapsed_ok = collapsed_ok && !book->expanded && book->expander && !gtk_expander_get_expanded(book->expander);
    printf("UI arranque-painel-cadernos-recolhidos: %s\n", collapsed_ok ? "OK" : "FALHOU"); ok = ok && collapsed_ok;
    bool workspace_ok = state.workspace_stack && state.metadata_button && state.export_button &&
        strcmp(gtk_stack_get_visible_child_name(state.workspace_stack), "document") == 0;
    Ptr metadata_title = state.title, metadata_tags = state.tags, metadata_input = state.tag_input;
    rebuild_sidebar();
    workspace_ok = workspace_ok && state.title == metadata_title && state.tags == metadata_tags && state.tag_input == metadata_input;
    printf("UI workspace-detalhes-persistentes: %s\n", workspace_ok ? "OK" : "FALHOU"); ok = ok && workspace_ok;
    Ptr tab_ancestor = state.tabbar;
    while (tab_ancestor && !gtk_widget_has_css_class(tab_ancestor, "app-header")) tab_ancestor = gtk_widget_get_parent(tab_ancestor);
    gboolean tabs_natural_width = true; int tabs_min_width = -1;
    if (state.header_tabs) g_object_get(state.header_tabs, "propagate-natural-width", &tabs_natural_width,
        "min-content-width", &tabs_min_width, NULL);
    bool header_tabs_ok = tab_ancestor && state.header_tabs && gtk_widget_get_visible(state.header_tabs) &&
        !gtk_widget_get_visible(state.header_title) && !tabs_natural_width && tabs_min_width == 0;
    printf("UI tabs-na-barra-titulo-sem-largura-imposta: %s\n", header_tabs_ok ? "OK" : "FALHOU"); ok = ok && header_tabs_ok;
    Ptr initial_tab = state.active ? state.active->tab_title : NULL;
    bool rename_ok = initial_tab && !gtk_editable_get_editable(initial_tab);
    g_signal_emit_by_name(state.tab_click, "pressed", 1, 12.0, 12.0);
    rename_ok = rename_ok && !gtk_editable_get_editable(initial_tab);
    g_signal_emit_by_name(state.tab_click, "pressed", 2, 12.0, 12.0);
    rename_ok = rename_ok && gtk_editable_get_editable(initial_tab) &&
        gtk_widget_has_css_class(initial_tab, "editing-title");
    if (rename_ok) {
        char *original_title = strdup(gtk_editable_get_text(initial_tab));
        gtk_editable_set_text(initial_tab, "ECTEG — título editado");
        rename_ok = !strcmp(gtk_editable_get_text(state.title), "ECTEG — título editado") && state.active->dirty;
        gtk_editable_set_text(initial_tab, original_title); free(original_title);
        g_signal_emit_by_name(initial_tab, "activate");
        rename_ok = rename_ok && !gtk_editable_get_editable(initial_tab) &&
            !gtk_widget_has_css_class(initial_tab, "editing-title");
        g_signal_emit_by_name(state.tab_click, "pressed", 2, 12.0, 12.0);
        rename_ok = rename_ok && gtk_editable_get_editable(initial_tab);
        tab_title_focus_left(NULL, initial_tab);
        rename_ok = rename_ok && !gtk_editable_get_editable(initial_tab);
    }
    printf("UI tabs-renomear-duplo-clique-enter-foco: %s\n", rename_ok ? "OK" : "FALHOU"); ok = ok && rename_ok;
    gboolean resizable = false, homogeneous_h = true, homogeneous_v = true;
    g_object_get(state.window, "resizable", &resizable, NULL);
    g_object_get(state.workspace_stack, "hhomogeneous", &homogeneous_h, "vhomogeneous", &homogeneous_v, NULL);
    bool resize_ok = resizable && !homogeneous_h && !homogeneous_v && state.resize_grip && state.format_scroll;
    gboolean natural_width = true; int min_content_width = -1;
    g_object_get(state.format_scroll, "propagate-natural-width", &natural_width, "min-content-width", &min_content_width, NULL);
    resize_ok = resize_ok && !natural_width && min_content_width == 0;
    g_object_get(state.mode_stack, "hhomogeneous", &homogeneous_h, "vhomogeneous", &homogeneous_v, NULL);
    resize_ok = resize_ok && !homogeneous_h && !homogeneous_v;
    Ptr dividers[] = {state.library_split, state.document_split};
    for (size_t index = 0; index < sizeof dividers / sizeof dividers[0]; index++) {
        gboolean shrink_start = false, shrink_end = false, wide = false;
        g_object_get(dividers[index], "shrink-start-child", &shrink_start, "shrink-end-child", &shrink_end, "wide-handle", &wide, NULL);
        resize_ok = resize_ok && shrink_start && shrink_end && wide;
    }
    int minimum_width = 0, measured_width = 0, minimum_height = 0, measured_height = 0;
    const char *resize_modes[] = {"editor", "visual", "split"};
    for (size_t index = 0; index < 3; index++) {
        select_editor_mode(NULL, (Ptr)resize_modes[index]);
        int width = 0, height = 0;
        gtk_widget_measure(state.window, GTK_ORIENTATION_HORIZONTAL, -1, &width, &measured_width, NULL, NULL);
        gtk_widget_measure(state.window, GTK_ORIENTATION_VERTICAL, 800, &height, &measured_height, NULL, NULL);
        if (width > minimum_width) minimum_width = width;
        if (height > minimum_height) minimum_height = height;
        resize_ok = resize_ok && width <= 800 && height <= 500;
    }
    select_editor_mode(NULL, "split");
    printf("UI redimensionar-janela-paineis (mínimo %d × %dpx): %s\n", minimum_width, minimum_height, resize_ok ? "OK" : "FALHOU"); ok = ok && resize_ok;
    bool long_note_resize_ok = check_long_note_resize(); ok = ok && long_note_resize_ok;
    size_t inline_actions = 0;
    size_t format_groups = 0;
    for (Ptr group = gtk_widget_get_first_child(state.format_tools); group; group = gtk_widget_get_next_sibling(group)) {
        if (gtk_widget_has_css_class(group, "format-group")) format_groups++;
        for (Ptr child = gtk_widget_get_first_child(group); child; child = gtk_widget_get_next_sibling(child))
            if (gtk_widget_has_css_class(child, "icon-button")) inline_actions++;
    }
    bool toolbar_ok = inline_actions == 11 && format_groups == 4 && state.format_color && state.format_color_popover &&
        state.format_heading_popover && state.heading_choices[0] && state.heading_choices[6];
    printf("UI barra-formatacao-onze-acoes-diretas: %s\n", toolbar_ok ? "OK" : "FALHOU"); ok = ok && toolbar_ok;
    char original_theme[32]; snprintf(original_theme, sizeof original_theme, "%s", state.theme);
    const char *theme_ids[] = {"white", "black", "system", "monokai", "tokyo-night", "dracula", "solarized-light", "catppuccin-latte"};
    bool themes_ok = gtk_source_buffer_get_language(state.editor_buffer) && gtk_source_buffer_get_language(state.split_buffer) &&
        gtk_source_buffer_get_highlight_syntax(state.editor_buffer) && gtk_source_buffer_get_highlight_syntax(state.split_buffer) &&
        gtk_source_buffer_get_style_scheme(state.editor_buffer) && gtk_source_buffer_get_style_scheme(state.split_buffer);
    for (size_t index = 0; index < sizeof theme_ids / sizeof theme_ids[0]; index++) {
        apply_theme(theme_ids[index]); themes_ok = themes_ok && strcmp(state.theme, theme_ids[index]) == 0 && state.theme_provider;
        const char *scheme_theme = !strcmp(state.theme, "system") ? (source_is_light() ? "white" : "black") : state.theme;
        char scheme_id[96]; snprintf(scheme_id, sizeof scheme_id, "notemd-%s", scheme_theme);
        Ptr expected_scheme = gtk_source_style_scheme_manager_get_scheme(source_style_manager_current(), scheme_id);
        themes_ok = themes_ok && expected_scheme &&
            gtk_source_buffer_get_style_scheme(state.editor_buffer) == expected_scheme &&
            gtk_source_buffer_get_style_scheme(state.split_buffer) == expected_scheme;
        if (!strcmp(state.theme, "catppuccin-latte")) themes_ok = themes_ok && source_is_light();
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
    bool sidebar_ok = state.sidebar_panel && !gtk_widget_get_visible(state.sidebar_panel) && state.sidebar_hidden;
    toggle_sidebar(NULL, NULL); sidebar_ok = sidebar_ok && gtk_widget_get_visible(state.sidebar_panel) && !state.sidebar_hidden;
    toggle_sidebar(NULL, NULL); sidebar_ok = sidebar_ok && !gtk_widget_get_visible(state.sidebar_panel) && state.sidebar_hidden;
    toggle_sidebar(NULL, NULL);
    printf("UI biblioteca-arranca-recolhida-alterna: %s\n", sidebar_ok ? "OK" : "FALHOU"); ok = ok && sidebar_ok;
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

    select_editor_mode(NULL, "editor");
    state.loading = true; gtk_text_buffer_set_text(state.editor_buffer, "texto", -1); state.loading = false;
    TextIter selection_start, selection_end; gtk_text_buffer_get_iter_at_offset(state.editor_buffer, &selection_start, 0);
    gtk_text_buffer_get_iter_at_offset(state.editor_buffer, &selection_end, 5);
    gtk_text_buffer_select_range(state.editor_buffer, &selection_start, &selection_end); insert_markdown(NULL, "**|**");
    char *formatted = editor_text(); bool format_ok = strcmp(formatted, "**texto**") == 0; g_free(formatted);
    insert_markdown(NULL, "**|**");
    formatted = editor_text(); format_ok = format_ok && !strcmp(formatted, "texto"); g_free(formatted);
    state.loading = true; gtk_text_buffer_set_text(state.editor_buffer, "título", -1); state.loading = false;
    gtk_text_buffer_get_bounds(state.editor_buffer, &selection_start, &selection_end);
    gtk_text_buffer_select_range(state.editor_buffer, &selection_start, &selection_end); g_signal_emit_by_name(state.heading_choices[4], "clicked");
    formatted = editor_text(); format_ok = format_ok && strcmp(formatted, "#### título") == 0; g_free(formatted);
    state.loading = true; gtk_text_buffer_set_text(state.editor_buffer, "apagado", -1); state.loading = false;
    gtk_text_buffer_get_bounds(state.editor_buffer, &selection_start, &selection_end);
    gtk_text_buffer_select_range(state.editor_buffer, &selection_start, &selection_end); insert_markdown(NULL, "~~|~~");
    formatted = editor_text(); format_ok = format_ok && strcmp(formatted, "~~apagado~~") == 0; g_free(formatted);
    state.loading = true; gtk_text_buffer_set_text(state.editor_buffer, "colorido", -1); state.loading = false;
    gtk_text_buffer_get_bounds(state.editor_buffer, &selection_start, &selection_end);
    gtk_text_buffer_select_range(state.editor_buffer, &selection_start, &selection_end); g_signal_emit_by_name(state.format_colors[4], "clicked");
    formatted = editor_text(); format_ok = format_ok && strcmp(formatted, "<span style=\"color:#0969da\">colorido</span>") == 0; g_free(formatted);
    printf("UI formatacao-selecao: %s\n", format_ok ? "OK" : "FALHOU"); ok = ok && format_ok;

    gtk_text_buffer_set_text(state.editor_buffer, "## maçã\n# Segunda\nnormal\n", -1);
    gtk_text_buffer_get_iter_at_offset(state.editor_buffer, &selection_start, 0);
    gtk_text_buffer_get_iter_at_offset(state.editor_buffer, &selection_end, 8);
    gtk_text_buffer_select_range(state.editor_buffer, &selection_end, &selection_start);
    g_signal_emit_by_name(state.heading_choices[3], "clicked");
    formatted = editor_text(); bool headings_ok = !strcmp(formatted, "### maçã\n# Segunda\nnormal\n"); g_free(formatted);
    char *heading_name = NULL; g_object_get(state.format_heading, "label", &heading_name, NULL);
    headings_ok = headings_ok && heading_name && !strcmp(heading_name, heading_label(3)); g_free(heading_name);
    gtk_text_buffer_undo(state.editor_buffer);
    formatted = editor_text(); headings_ok = headings_ok && !strcmp(formatted, "## maçã\n# Segunda\nnormal\n"); g_free(formatted);
    gtk_text_buffer_get_bounds(state.editor_buffer, &selection_start, &selection_end);
    gtk_text_buffer_select_range(state.editor_buffer, &selection_end, &selection_start);
    g_signal_emit_by_name(state.heading_choices[0], "clicked");
    formatted = editor_text(); headings_ok = headings_ok && !strcmp(formatted, "maçã\nSegunda\nnormal\n"); g_free(formatted);
    gtk_text_buffer_get_iter_at_offset(state.editor_buffer, &selection_start, 2);
    gtk_text_buffer_select_range(state.editor_buffer, &selection_start, &selection_start);
    g_signal_emit_by_name(state.heading_choices[2], "clicked");
    formatted = editor_text(); headings_ok = headings_ok && !strcmp(formatted, "## maçã\nSegunda\nnormal\n"); g_free(formatted);
    printf("UI titulos-substituir-paragrafo-multilinha-undo: %s\n", headings_ok ? "OK" : "FALHOU"); ok = ok && headings_ok;

    gtk_text_buffer_set_text(state.editor_buffer, "", -1); insert_markdown(NULL, "_|_");
    gtk_text_buffer_insert_at_cursor(state.editor_buffer, "á", -1);
    formatted = editor_text(); bool caret_ok = !strcmp(formatted, "_á_"); g_free(formatted);
    printf("UI formatacao-cursor-entre-marcadores: %s\n", caret_ok ? "OK" : "FALHOU"); ok = ok && caret_ok;

    select_editor_mode(NULL, "split");
    gtk_text_buffer_set_text(state.split_buffer, "alpha β gamma", -1);
    gtk_text_buffer_get_iter_at_offset(state.split_buffer, &selection_start, 6);
    gtk_text_buffer_get_iter_at_offset(state.split_buffer, &selection_end, 7);
    gtk_text_buffer_select_range(state.split_buffer, &selection_end, &selection_start); insert_markdown(NULL, "**|**");
    formatted = editor_text(); bool split_format_ok = !strcmp(formatted, "alpha **β** gamma"); g_free(formatted);
    insert_markdown(NULL, "**|**");
    formatted = editor_text(); split_format_ok = split_format_ok && !strcmp(formatted, "alpha β gamma"); g_free(formatted);
    g_signal_emit_by_name(state.heading_choices[1], "clicked");
    formatted = editor_text(); split_format_ok = split_format_ok && !strcmp(formatted, "# alpha β gamma"); g_free(formatted);
    printf("UI formatacao-dividido-selecao-unicode: %s\n", split_format_ok ? "OK" : "FALHOU"); ok = ok && split_format_ok;

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
            strcmp(gtk_editable_get_text(state.block_languages[2]), "c") == 0 &&
            gtk_source_buffer_get_language(state.block_buffers[2]) == source_code_language("c");
        g_free(code_content); gtk_editable_set_text(state.block_languages[2], "rust");
        char *code_markdown = editor_text(); blocks_ok = blocks_ok && strstr(code_markdown, "```rust\nlinha 1") &&
            gtk_source_buffer_get_language(state.block_buffers[2]) == source_code_language("rust"); g_free(code_markdown);
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
    if (access(source_icon, R_OK) != 0) snprintf(source_icon, sizeof source_icon, "%s/assets/icons/1024x1024/pt.notemd.NoteMD.png", state.data_dir);
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
    for (Note *note = state.notes; note; note = note->next) if (note->opened) close_note_now(note, false);
    bool welcome_ok = !state.active && strcmp(gtk_stack_get_visible_child_name(state.workspace_stack), "welcome") == 0 &&
        !gtk_widget_get_visible(state.stats) && !gtk_widget_get_first_child(state.tabbar) &&
        !gtk_widget_get_visible(state.header_tabs) && gtk_widget_get_visible(state.header_title);
    printf("UI fechar-todas-mostra-inicio: %s\n", welcome_ok ? "OK" : "FALHOU"); ok = ok && welcome_ok;
    state.ui_test_failed = !ok; g_application_quit(state.app); return false;
}

gboolean run_empty_ui_assertions(Ptr unused) {
    (void)unused;
    bool ok = !state.notebooks && !state.notes && !state.active && state.side_new_note &&
        state.workspace_stack && strcmp(gtk_stack_get_visible_child_name(state.workspace_stack), "welcome") == 0 &&
        !gtk_widget_get_visible(state.tags_section) && state.sidebar_hidden && !gtk_widget_get_visible(state.sidebar_panel) &&
        !gtk_widget_get_visible(state.header_tabs) && gtk_widget_get_visible(state.header_title);
    printf("UI vazio-estado-inicial: %s\n", ok ? "OK" : "FALHOU");
    g_signal_emit_by_name(state.mode_split, "clicked");
    bool split_without_note = strcmp(gtk_stack_get_visible_child_name(state.mode_stack), "split") == 0;
    printf("UI vazio-clique-dividido: %s\n", split_without_note ? "OK" : "FALHOU"); ok = ok && split_without_note;
    g_signal_emit_by_name(state.side_new_note, "clicked");
    bool created = state.notebooks && state.active_notebook && state.active && state.active->notebook == state.active_notebook &&
        access(state.active->markdown_path, R_OK) == 0 && strcmp(gtk_stack_get_visible_child_name(state.workspace_stack), "document") == 0;
    printf("UI vazio-nova-nota-cria-tudo: %s\n", created ? "OK" : "FALHOU"); ok = ok && created;
    if (created) {
        gtk_text_buffer_set_text(state.editor_buffer, "# Primeira nota\n\nTexto visível no dividido", -1);
        g_signal_emit_by_name(state.mode_split, "clicked"); char *preview = buffer_text(state.split_preview_buffer);
        bool visible = strstr(preview, "Primeira nota") && strstr(preview, "Texto visível no dividido");
        printf("UI vazio-dividido-com-conteudo: %s\n", visible ? "OK" : "FALHOU"); ok = ok && visible; g_free(preview);
    }
    state.ui_test_failed = !ok; g_application_quit(state.app); return false;
}

/* Persistent menu popovers keep note fields alive while the library is rebuilt. */


void menu_command_clicked(Ptr button, Ptr user_data) {
    MenuCommand *command = user_data;
    gtk_popover_popdown(command->popover);
    command->callback(button, command->data);
}

Ptr workspace_menu(const char *icon, const char *tooltip, Ptr content, Ptr *popover_out) {
    Ptr button = gtk_menu_button_new(), popover = gtk_popover_new();
    gtk_menu_button_set_icon_name(button, icon);
    gtk_widget_set_tooltip_text(button, tooltip);
    gtk_widget_add_css_class(button, "workspace-menu");
    gtk_popover_set_child(popover, content); gtk_menu_button_set_popover(button, popover);
    if (popover_out) *popover_out = popover;
    return button;
}

void append_menu_command(Ptr box, Ptr popover, const char *icon, const char *label,
                                const char *shortcut, void (*callback)(Ptr, Ptr), Ptr data) {
    Ptr button = navigation_button(icon, label); gtk_widget_add_css_class(button, "context-action");
    if (shortcut) {
        Ptr hint = gtk_label_new(shortcut); gtk_widget_add_css_class(hint, "shortcut-hint");
        gtk_box_append(gtk_widget_get_first_child(button), hint);
    }
    MenuCommand *command = malloc(sizeof *command);
    *command = (MenuCommand){popover, callback, data};
    g_signal_connect_data(button, "clicked", (void *)menu_command_clicked, command, (void *)free_signal_data, 0);
    gtk_box_append(box, button);
}

void save_clicked(Ptr button, Ptr unused) { (void)button; (void)unused; save_active(NULL); }

Ptr welcome_page(void) {
    Ptr page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_add_css_class(page, "welcome-page");
    gtk_widget_set_halign(page, GTK_ALIGN_CENTER); gtk_widget_set_valign(page, GTK_ALIGN_CENTER); margins(page, 24, 24);
    Ptr icon = gtk_image_new_from_icon_name("accessories-text-editor-symbolic");
    gtk_image_set_pixel_size(icon, 40); gtk_widget_add_css_class(icon, "welcome-icon"); gtk_box_append(page, icon);
    Ptr title = gtk_label_new(tr("Um espaço para as suas ideias"));
    gtk_label_set_wrap(title, true); gtk_label_set_max_width_chars(title, 26);
    gtk_widget_add_css_class(title, "welcome-title"); gtk_box_append(page, title);
    Ptr description = gtk_label_new(tr("Crie uma nota ou abra um ficheiro Markdown para começar."));
    gtk_label_set_wrap(description, true); gtk_label_set_max_width_chars(description, 42);
    gtk_widget_add_css_class(description, "welcome-description"); gtk_box_append(page, description);
    Ptr actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8); gtk_widget_set_halign(actions, GTK_ALIGN_CENTER);
    Ptr create = navigation_button("document-new-symbolic", tr("Nova nota"));
    gtk_widget_add_css_class(create, "suggested-action");
    Ptr open = navigation_button("document-open-symbolic", tr("Abrir…"));
    g_signal_connect_data(create, "clicked", (void *)new_note, NULL, NULL, 0);
    g_signal_connect_data(open, "clicked", (void *)open_markdown, NULL, NULL, 0);
    gtk_box_append(actions, create); gtk_box_append(actions, open); gtk_box_append(page, actions);
    Ptr hint = gtk_label_new(tr("Nova nota  Ctrl+N   ·   Abrir  Ctrl+O"));
    gtk_label_set_wrap(hint, true); gtk_label_set_max_width_chars(hint, 36);
    gtk_widget_add_css_class(hint, "shortcut-hint"); gtk_box_append(page, hint);
    return page;
}

void activate(Ptr app, Ptr unused) {
    (void)unused;
    for (Notebook *book = state.notebooks; book; book = book->next) book->expanded = false;
    apply_theme(state.theme);
    state.window = gtk_application_window_new(app);
    gtk_widget_add_css_class(state.window, "notemd-window");
    gtk_window_set_title(state.window, "NoteMD");
    gtk_window_set_icon_name(state.window, "pt.notemd.NoteMD");
    gtk_window_set_resizable(state.window, true);
    gtk_window_set_default_size(state.window, 1120, 720);
    g_signal_connect_data(state.window, "close-request", (void *)close_requested, NULL, NULL, 0);
    Ptr keys = gtk_event_controller_key_new();
    g_signal_connect_data(keys, "key-pressed", (void *)key_pressed, NULL, NULL, 0);
    gtk_widget_add_controller(state.window, keys);
    Ptr drop = gtk_drop_target_new(gdk_file_list_get_type(), GDK_ACTION_COPY);
    g_signal_connect_data(drop, "drop", (void *)files_dropped, NULL, NULL, 0);
    gtk_widget_add_controller(state.window, drop);
    g_timeout_add(2000, (void *)poll_external_file, NULL);

    Ptr header_handle = gtk_window_handle_new();
    Ptr header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6); gtk_widget_add_css_class(header, "app-header");
    gtk_window_handle_set_child(header_handle, header);
    Ptr start_controls = gtk_window_controls_new(0), end_controls = gtk_window_controls_new(1);
    /* SYNC_CREATE | INVERT_BOOLEAN: respect the desktop's button placement. */
    g_object_bind_property(start_controls, "empty", start_controls, "visible", 6);
    g_object_bind_property(end_controls, "empty", end_controls, "visible", 6);
    gtk_box_append(header, start_controls);
    state.sidebar_hidden = true;
    state.sidebar_toggle = icon_button("sidebar-show-symbolic", tr("Mostrar biblioteca"));
    g_signal_connect_data(state.sidebar_toggle, "clicked", (void *)toggle_sidebar, NULL, NULL, 0);
    gtk_box_append(header, state.sidebar_toggle);
    state.header_title = gtk_label_new("NoteMD"); gtk_widget_add_css_class(state.header_title, "brand");
    gtk_label_set_ellipsize(state.header_title, 3); gtk_label_set_max_width_chars(state.header_title, 30);
    gtk_box_append(header, state.header_title);
    state.header_tabs = gtk_scrolled_window_new(); gtk_widget_add_css_class(state.header_tabs, "header-tabs");
    gtk_scrolled_window_set_policy(state.header_tabs, GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    g_object_set(state.header_tabs, "propagate-natural-width", false, "min-content-width", 0, NULL);
    gtk_widget_set_hexpand(state.header_tabs, true); gtk_widget_set_valign(state.header_tabs, GTK_ALIGN_CENTER);
    gtk_widget_set_visible(state.header_tabs, false);
    state.tabbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4); gtk_widget_add_css_class(state.tabbar, "tabbar");
    /* Keep the double-click recognizer alive when selecting a tab rebuilds its title. */
    Ptr tab_click = gtk_gesture_click_new(); gtk_gesture_single_set_button(tab_click, 1);
    state.tab_click = tab_click;
    gtk_event_controller_set_propagation_phase(tab_click, 1);
    g_signal_connect_data(tab_click, "pressed", (void *)tab_title_pressed, NULL, NULL, 0);
    gtk_widget_add_controller(state.tabbar, tab_click);
    gtk_scrolled_window_set_child(state.header_tabs, state.tabbar); gtk_box_append(header, state.header_tabs);
    Ptr title_drag_space = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_size_request(title_drag_space, 24, -1); gtk_box_append(header, title_drag_space);
    gtk_widget_set_hexpand(state.header_title, true); gtk_label_set_xalign(state.header_title, 0.0f);

    Ptr app_actions = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2); margins(app_actions, 5, 5);
    Ptr app_popover = NULL;
    Ptr app_menu = workspace_menu("open-menu-symbolic", tr("Menu da aplicação"), app_actions, &app_popover);
    append_menu_command(app_actions, app_popover, "document-new-symbolic", tr("Nova nota"), "Ctrl+N", new_note, NULL);
    append_menu_command(app_actions, app_popover, "folder-new-symbolic", tr("Novo notebook"), "Ctrl+Shift+N", new_notebook, NULL);
    append_menu_command(app_actions, app_popover, "document-open-symbolic", tr("Abrir…"), "Ctrl+O", open_markdown, NULL);
    append_menu_command(app_actions, app_popover, "folder-download-symbolic", "Importar notebook…", NULL, import_notebook, NULL);
    append_menu_command(app_actions, app_popover, "document-properties-symbolic", tr("Templates"), NULL, show_templates, NULL);
    gtk_box_append(app_actions, gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    append_menu_command(app_actions, app_popover, "edit-find-symbolic", tr("Pesquisar"), "Ctrl+F", show_find, NULL);
    append_menu_command(app_actions, app_popover, "document-save-symbolic", tr("Guardar"), "Ctrl+S", save_clicked, NULL);
    append_menu_command(app_actions, app_popover, "document-open-recent-symbolic", tr("Histórico"), NULL, show_history, NULL);
    gtk_box_append(app_actions, gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    append_menu_command(app_actions, app_popover, "preferences-system-symbolic", tr("Preferências"), NULL, show_preferences, NULL);
    state.export_button = icon_button("document-send-symbolic", tr("Exportar"));
    g_signal_connect_data(state.export_button, "clicked", (void *)show_export_menu, NULL, NULL, 0);

    Ptr details = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8); margins(details, 10, 10);
    gtk_widget_add_css_class(details, "note-details");
    Ptr details_heading = gtk_label_new(tr("Detalhes da nota")); gtk_label_set_xalign(details_heading, 0.0f);
    gtk_widget_add_css_class(details_heading, "context-title"); gtk_box_append(details, details_heading);
    state.title = gtk_entry_new(); gtk_entry_set_placeholder_text(state.title, tr("Título da nota"));
    gtk_widget_set_tooltip_text(state.title, tr("Título da nota")); gtk_box_append(details, state.title);
    state.tags = gtk_entry_new(); gtk_widget_set_visible(state.tags, false); gtk_box_append(details, state.tags);
    Ptr tags_label = gtk_label_new(tr("Tags")); gtk_label_set_xalign(tags_label, 0.0f);
    gtk_widget_add_css_class(tags_label, "section-title"); gtk_box_append(details, tags_label);
    state.note_tags_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    Ptr tags_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(tags_scroll, GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    gtk_scrolled_window_set_child(tags_scroll, state.note_tags_box); gtk_box_append(details, tags_scroll);
    Ptr tag_input_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    state.tag_input = gtk_entry_new(); gtk_entry_set_placeholder_text(state.tag_input, tr("Adicionar tag"));
    gtk_widget_add_css_class(state.tag_input, "tag-input"); gtk_widget_set_hexpand(state.tag_input, true);
    g_signal_connect_data(state.tag_input, "activate", (void *)add_note_tag, NULL, NULL, 0);
    gtk_box_append(tag_input_row, state.tag_input);
    Ptr add_tag = icon_button("list-add-symbolic", tr("Adicionar tag"));
    g_signal_connect_data(add_tag, "clicked", (void *)add_note_tag_clicked, NULL, NULL, 0);
    gtk_box_append(tag_input_row, add_tag); gtk_box_append(details, tag_input_row);
    state.metadata_button = workspace_menu("document-properties-symbolic", tr("Detalhes da nota"), details, NULL);
    gtk_box_append(header, state.metadata_button); gtk_box_append(header, state.export_button);
    gtk_box_append(header, app_menu); gtk_box_append(header, end_controls);
    gtk_window_set_titlebar(state.window, header_handle);
    g_signal_connect_data(state.title, "changed", (void *)content_changed, NULL, NULL, 0);
    g_signal_connect_data(state.tags, "changed", (void *)content_changed, NULL, NULL, 0);

    Ptr paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL); gtk_paned_set_position(paned, 232);
    state.library_split = paned;
    gtk_paned_set_resize_start_child(paned, false); gtk_paned_set_resize_end_child(paned, true);
    gtk_paned_set_shrink_start_child(paned, true); gtk_paned_set_shrink_end_child(paned, true);
    gtk_paned_set_wide_handle(paned, true);
    gtk_widget_set_vexpand(paned, true);
    Ptr side_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); state.sidebar_panel = side_panel;
    gtk_widget_set_size_request(side_panel, 200, -1); gtk_widget_add_css_class(side_panel, "sidebar-panel");
    gtk_widget_set_visible(side_panel, false);
    Ptr side_actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2); gtk_widget_add_css_class(side_actions, "sidebar-actions");
    Ptr library_title = gtk_label_new(tr("Biblioteca")); gtk_label_set_xalign(library_title, 0.0f);
    gtk_widget_add_css_class(library_title, "library-title"); gtk_widget_set_hexpand(library_title, true);
    gtk_box_append(side_actions, library_title);
    Ptr side_new_book = icon_button("folder-new-symbolic", tr("Novo notebook"));
    state.side_new_note = icon_button("document-new-symbolic", tr("Nova nota"));
    g_signal_connect_data(side_new_book, "clicked", (void *)new_notebook, NULL, NULL, 0);
    g_signal_connect_data(state.side_new_note, "clicked", (void *)new_note, NULL, NULL, 0);
    gtk_box_append(side_actions, side_new_book); gtk_box_append(side_actions, state.side_new_note);
    gtk_box_append(side_panel, side_actions);
    state.search = gtk_search_entry_new(); gtk_search_entry_set_placeholder_text(state.search, tr("Pesquisar notas"));
    margins(state.search, 10, 5); g_signal_connect_data(state.search, "search-changed", (void *)search_changed, NULL, NULL, 0);
    gtk_box_append(side_panel, state.search);
    Ptr side_scroll = gtk_scrolled_window_new(); gtk_widget_set_vexpand(side_scroll, true);
    gtk_scrolled_window_set_policy(side_scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    state.sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2); gtk_widget_add_css_class(state.sidebar, "library-tree");
    gtk_scrolled_window_set_child(side_scroll, state.sidebar); gtk_box_append(side_panel, side_scroll);
    state.tags_section = gtk_expander_new(tr("Tags")); margins(state.tags_section, 10, 6);
    gtk_widget_add_css_class(state.tags_section, "tags-section");
    state.tag_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    Ptr tag_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(tag_scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_max_content_height(tag_scroll, 160);
    gtk_scrolled_window_set_propagate_natural_height(tag_scroll, true);
    gtk_scrolled_window_set_child(tag_scroll, state.tag_box); gtk_expander_set_child(state.tags_section, tag_scroll);
    gtk_expander_set_expanded(state.tags_section, false); gtk_box_append(side_panel, state.tags_section);
    state.root_label = gtk_label_new(state.root); gtk_label_set_xalign(state.root_label, 0.0f);
    gtk_label_set_ellipsize(state.root_label, 1); gtk_label_set_max_width_chars(state.root_label, 24);
    gtk_widget_set_tooltip_text(state.root_label, state.root); gtk_widget_add_css_class(state.root_label, "storage-path");
    margins(state.root_label, 12, 10); gtk_box_append(side_panel, state.root_label);
    rebuild_sidebar(); gtk_paned_set_start_child(paned, side_panel);

    Ptr workspace = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); gtk_widget_add_css_class(workspace, "workspace");
    state.workspace_stack = gtk_stack_new(); gtk_widget_set_hexpand(state.workspace_stack, true);
    gtk_stack_set_hhomogeneous(state.workspace_stack, false); gtk_stack_set_vhomogeneous(state.workspace_stack, false);
    gtk_widget_set_vexpand(state.workspace_stack, true);
    gtk_stack_add_titled(state.workspace_stack, welcome_page(), "welcome", "NoteMD");
    gtk_stack_add_titled(state.workspace_stack, workspace, "document", tr("Editor"));
    state.linksbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4); margins(state.linksbar, 12, 2);
    gtk_widget_set_visible(state.linksbar, false); gtk_widget_add_css_class(state.linksbar, "linksbar");
    state.links_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(state.links_scroll, GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    gtk_scrolled_window_set_child(state.links_scroll, state.linksbar); gtk_widget_set_visible(state.links_scroll, false);
    gtk_box_append(workspace, state.links_scroll);
    Ptr formatbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2); state.formatbar = formatbar;
    gtk_widget_add_css_class(formatbar, "formatbar");
    state.format_tools = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_add_css_class(state.format_tools, "format-tools");
    state.format_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(state.format_scroll, GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    gtk_scrolled_window_set_propagate_natural_height(state.format_scroll, true);
    g_object_set(state.format_scroll, "propagate-natural-width", false, "min-content-width", 0, NULL);
    gtk_widget_set_hexpand(state.format_scroll, true);
    gtk_scrolled_window_set_child(state.format_scroll, state.format_tools);
    gtk_box_append(formatbar, state.format_scroll);
    Ptr heading_group = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_add_css_class(heading_group, "format-group"); gtk_box_append(state.format_tools, heading_group);
    Ptr heading_options = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2); margins(heading_options, 6, 6);
    state.format_heading = gtk_menu_button_new();
    gtk_widget_add_css_class(state.format_heading, "workspace-menu");
    gtk_widget_add_css_class(state.format_heading, "heading-selector");
    gtk_menu_button_set_label(state.format_heading, heading_label(0));
    gtk_menu_button_set_always_show_arrow(state.format_heading, true);
    gtk_widget_set_tooltip_text(state.format_heading, tr("Estilo do texto"));
    state.format_heading_popover = gtk_popover_new();
    gtk_widget_add_css_class(state.format_heading_popover, "heading-menu");
    gtk_popover_set_child(state.format_heading_popover, heading_options);
    gtk_menu_button_set_popover(state.format_heading, state.format_heading_popover);
    for (unsigned level = 0; level <= 6; level++) {
        Ptr choice = gtk_button_new_with_label(""); state.heading_choices[level] = choice;
        gtk_widget_add_css_class(choice, "context-action"); gtk_widget_add_css_class(choice, "heading-option");
        char css_class[32]; snprintf(css_class, sizeof css_class, "heading-level-%u", level);
        gtk_widget_add_css_class(choice, css_class);
        Ptr row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        char badge_text[8]; if (level) snprintf(badge_text, sizeof badge_text, "H%u", level); else strcpy(badge_text, "¶");
        Ptr badge = gtk_label_new(badge_text); gtk_widget_add_css_class(badge, "heading-badge");
        Ptr label = gtk_label_new(heading_label(level)); gtk_label_set_xalign(label, 0.0f);
        gtk_widget_add_css_class(label, "heading-preview"); gtk_widget_set_hexpand(label, true);
        gtk_box_append(row, badge); gtk_box_append(row, label); gtk_button_set_child(choice, row);
        g_signal_connect_data(choice, "clicked", (void *)heading_format_clicked, (Ptr)(size_t)level, NULL, 0);
        gtk_box_append(heading_options, choice);
        if (!level) gtk_box_append(heading_options, gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    }
    gtk_box_append(heading_group, state.format_heading);
    Ptr inline_group = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    Ptr list_group = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    Ptr insert_group = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    Ptr groups[] = {inline_group, list_group, insert_group};
    for (size_t i = 0; i < sizeof groups / sizeof groups[0]; i++) {
        gtk_widget_add_css_class(groups[i], "format-group"); gtk_box_append(state.format_tools, groups[i]);
    }
    struct { const char *icon, *tooltip, *markdown; } formats[] = {
        {"format-text-bold-symbolic", "Negrito", "**|**"}, {"format-text-italic-symbolic", "Itálico", "_|_"},
        {"format-text-strikethrough-symbolic", "Rasurado", "~~|~~"}, {"insert-text-symbolic", "Código", "`|`"},
        {"view-list-bullet-symbolic", "Lista", "- |"}, {"view-list-ordered-symbolic", "Lista numerada", "1. |"},
        {"checkbox-symbolic", "Tarefa", "- [ ] |"}, {"format-indent-more-symbolic", "Citação", "> |"},
        {"insert-link-symbolic", "Ligação", "[|](https://)"}
    };
    for (size_t i = 0; i < sizeof formats / sizeof formats[0]; i++) {
        Ptr button = i == 3 ? gtk_button_new_with_label("</>") : icon_button(formats[i].icon, tr(formats[i].tooltip));
        gtk_widget_add_css_class(button, "icon-button");
        if (i == 3) { gtk_widget_add_css_class(button, "code-format"); gtk_widget_set_tooltip_text(button, tr("Código")); }
        g_signal_connect_data(button, "clicked", (void *)insert_markdown, (Ptr)formats[i].markdown, NULL, 0);
        gtk_box_append(i < 4 ? inline_group : i < 8 ? list_group : insert_group, button);
    }
    Ptr table = icon_button("insert-table-symbolic", tr("Tabela"));
    g_signal_connect_data(table, "clicked", (void *)insert_literal,
        "| Coluna 1 | Coluna 2 |\n| --- | --- |\n| Valor | Valor |\n", NULL, 0);
    gtk_box_append(insert_group, table);
    Ptr image = icon_button("insert-image-symbolic", tr("Imagem…"));
    g_signal_connect_data(image, "clicked", (void *)insert_image, NULL, NULL, 0); gtk_box_append(insert_group, image);
    Ptr color_choices = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3); margins(color_choices, 8, 8);
    state.format_color = workspace_menu("color-select-symbolic", tr("Cor do texto"), color_choices, &state.format_color_popover);
    static const char *color_names[] = {"Vermelho", "Laranja", "Amarelo", "Verde", "Azul", "Roxo"};
    static const char *color_values[] = {"#d1242f", "#bc4c00", "#9a6700", "#1a7f37", "#0969da", "#8250df"};
    static const char *color_classes[] = {"color-red", "color-orange", "color-yellow", "color-green", "color-blue", "color-purple"};
    for (size_t index = 0; index < 6; index++) {
        Ptr choice = icon_button("media-record-symbolic", tr(color_names[index])); state.format_colors[index] = choice;
        gtk_widget_add_css_class(choice, "color-swatch"); gtk_widget_add_css_class(choice, color_classes[index]);
        g_signal_connect_data(choice, "clicked", (void *)insert_text_color, (Ptr)color_values[index], NULL, 0);
        gtk_box_append(color_choices, choice);
    }
    gtk_box_append(inline_group, state.format_color);
    Ptr modes = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1); gtk_widget_add_css_class(modes, "mode-switcher");
    state.mode_editor = icon_button("document-edit-symbolic", tr("Editar nota"));
    state.mode_visual = icon_button("view-grid-symbolic", tr("Editar visualmente"));
    state.mode_split = icon_button("view-dual-symbolic", tr("Editor e visualização"));
    Ptr mode_buttons[] = {state.mode_editor, state.mode_visual, state.mode_split};
    const char *mode_ids[] = {"editor", "visual", "split"};
    const char *mode_tooltips[] = {"Editar nota", "Editar visualmente", "Editor e visualização"};
    for (size_t index = 0; index < 3; index++) {
        char tooltip[128]; snprintf(tooltip, sizeof tooltip, "%s · Ctrl+%zu", tr(mode_tooltips[index]), index + 1);
        gtk_widget_set_tooltip_text(mode_buttons[index], tooltip);
        gtk_widget_add_css_class(mode_buttons[index], "mode-button");
        g_signal_connect_data(mode_buttons[index], "clicked", (void *)select_editor_mode, (Ptr)mode_ids[index], NULL, 0);
        gtk_box_append(modes, mode_buttons[index]);
    }
    gtk_widget_add_css_class(state.mode_editor, "active-mode");
    gtk_box_append(formatbar, modes); gtk_box_append(workspace, formatbar);
    Ptr stack = gtk_stack_new(); state.mode_stack = stack; gtk_widget_set_hexpand(stack, true); gtk_widget_set_vexpand(stack, true);
    gtk_stack_set_hhomogeneous(stack, false); gtk_stack_set_vhomogeneous(stack, false);
    state.editor = source_editor_new(); gtk_text_view_set_editable(state.editor, true); gtk_text_view_set_cursor_visible(state.editor, true);
    gtk_widget_add_css_class(state.editor, "markdown-source");
    gtk_text_view_set_monospace(state.editor, true);
    gtk_text_view_set_wrap_mode(state.editor, GTK_WRAP_WORD_CHAR); gtk_text_view_set_left_margin(state.editor, 16);
    gtk_text_view_set_right_margin(state.editor, 20); gtk_text_view_set_top_margin(state.editor, 14);
    Ptr paste_keys = gtk_event_controller_key_new(); gtk_event_controller_set_propagation_phase(paste_keys, 1);
    g_signal_connect_data(paste_keys, "key-pressed", (void *)paste_image_key, NULL, NULL, 0);
    gtk_widget_add_controller(state.editor, paste_keys);
    state.editor_buffer = gtk_text_view_get_buffer(state.editor);
    gtk_text_buffer_set_enable_undo(state.editor_buffer, true);
    apply_syntax_theme();
    g_signal_connect_data(state.editor_buffer, "changed", (void *)content_changed, NULL, NULL, 0);
    Ptr editor_scroll = editor_scroller(state.editor);
    Ptr line_view = gtk_text_view_new(); gtk_text_view_set_editable(line_view, false); gtk_text_view_set_cursor_visible(line_view, false);
    gtk_text_view_set_monospace(line_view, true); gtk_text_view_set_top_margin(line_view, 14); gtk_text_view_set_right_margin(line_view, 8);
    gtk_widget_add_css_class(line_view, "line-numbers"); state.line_buffer = gtk_text_view_get_buffer(line_view);
    gtk_text_buffer_create_tag(state.line_buffer, "active-line", "foreground", "#78a9ff", "weight", 700, "background", "#253552", NULL);
    g_signal_connect_data(state.editor_buffer, "mark-set", (void *)cursor_mark_set, NULL, NULL, 0);
    Ptr line_scroll = gtk_scrolled_window_new(); gtk_widget_set_size_request(line_scroll, 40, -1);
    /* NEVER makes the full gutter content determine the window's minimum height.
     * EXTERNAL shares the editor's scrolling without imposing the line count. */
    gtk_scrolled_window_set_policy(line_scroll, GTK_POLICY_NEVER, GTK_POLICY_EXTERNAL); gtk_scrolled_window_set_child(line_scroll, line_view);
    gtk_scrolled_window_set_vadjustment(line_scroll, gtk_scrolled_window_get_vadjustment(editor_scroll));
    Ptr editor_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0); gtk_box_append(editor_row, line_scroll); gtk_box_append(editor_row, editor_scroll);
    gtk_stack_add_titled(stack, editor_row, "editor", tr("Editor"));
    Ptr visual_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(visual_page, "visual-editor");
    Ptr visual_toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4); margins(visual_toolbar, 12, 4);
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
    state.block_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(state.block_box, "visual-blocks"); margins(state.block_box, 18, 12);
    Ptr visual_scroll = gtk_scrolled_window_new(); gtk_widget_set_vexpand(visual_scroll, true);
    gtk_scrolled_window_set_child(visual_scroll, state.block_box);
    gtk_box_append(visual_page, visual_toolbar); gtk_box_append(visual_page, visual_scroll);
    gtk_stack_add_titled(stack, visual_page, "visual", tr("Visual"));
    Ptr divided = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL); gtk_paned_set_position(divided, 430);
    gtk_widget_add_css_class(divided, "document-split");
    state.document_split = divided;
    gtk_paned_set_resize_start_child(divided, true); gtk_paned_set_resize_end_child(divided, true);
    gtk_paned_set_shrink_start_child(divided, true); gtk_paned_set_shrink_end_child(divided, true);
    gtk_paned_set_wide_handle(divided, true);
    state.split_source_view = source_editor_new(); gtk_text_view_set_editable(state.split_source_view, true);
    gtk_text_view_set_cursor_visible(state.split_source_view, true); state.split_buffer = gtk_text_view_get_buffer(state.split_source_view);
    gtk_text_buffer_set_enable_undo(state.split_buffer, true);
    gtk_text_view_set_monospace(state.split_source_view, true); gtk_text_view_set_wrap_mode(state.split_source_view, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(state.split_source_view, 18); gtk_text_view_set_right_margin(state.split_source_view, 18);
    gtk_text_view_set_top_margin(state.split_source_view, 14); gtk_widget_add_css_class(state.split_source_view, "split-source");
    gtk_widget_add_css_class(state.split_source_view, "markdown-source");
    gtk_widget_set_hexpand(state.split_source_view, true);
    g_signal_connect_data(state.split_buffer, "changed", (void *)split_content_changed, NULL, NULL, 0);
    g_signal_connect_data(state.split_buffer, "mark-set", (void *)cursor_mark_set, NULL, NULL, 0);
    Ptr split_paste_keys = gtk_event_controller_key_new(); gtk_event_controller_set_propagation_phase(split_paste_keys, 1);
    g_signal_connect_data(split_paste_keys, "key-pressed", (void *)paste_image_key, NULL, NULL, 0);
    gtk_widget_add_controller(state.split_source_view, split_paste_keys);
    state.split_preview_view = readonly_markdown_view(&state.split_preview_buffer, "preview");
    gtk_paned_set_start_child(divided, editor_scroller(state.split_source_view));
    gtk_paned_set_end_child(divided, editor_scroller(state.split_preview_view));
    gtk_stack_add_titled(stack, divided, "split", tr("Dividido"));
    gtk_stack_set_visible_child_name(stack, "editor");
    gtk_box_append(workspace, stack);
    Ptr statusbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12); gtk_widget_add_css_class(statusbar, "statusbar");
    state.status = gtk_label_new(tr("Crie ou abra uma nota")); gtk_label_set_xalign(state.status, 0.0f);
    gtk_label_set_ellipsize(state.status, 3); gtk_widget_set_hexpand(state.status, true);
    gtk_widget_add_css_class(state.status, "status");
    state.stats = gtk_label_new(""); gtk_widget_add_css_class(state.stats, "document-stats");
    state.line_badge = gtk_label_new(""); gtk_widget_add_css_class(state.line_badge, "line-badge");
    gtk_box_append(statusbar, state.status); gtk_box_append(statusbar, state.stats); gtk_box_append(statusbar, state.line_badge);
    state.resize_grip = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(state.resize_grip, "resize-grip");
    gtk_widget_set_tooltip_text(state.resize_grip, "Arrastar para redimensionar a janela");
    gtk_widget_set_cursor_from_name(state.resize_grip, "se-resize");
    Ptr resize_gesture = gtk_gesture_click_new(); gtk_gesture_single_set_button(resize_gesture, 1);
    g_signal_connect_data(resize_gesture, "pressed", (void *)resize_window_pressed, NULL, NULL, 0);
    gtk_widget_add_controller(state.resize_grip, resize_gesture); gtk_box_append(statusbar, state.resize_grip);
    Ptr main_area = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(main_area, state.workspace_stack); gtk_box_append(main_area, statusbar);
    gtk_paned_set_end_child(paned, main_area); gtk_window_set_child(state.window, paned);
    apply_syntax_theme();
    sync_workspace();
    if (state.notes) select_note(NULL, state.notes);
    gtk_window_present(state.window);
    if (state.ui_test) {
        if (state.empty_ui_test) g_timeout_add(350, (void *)run_empty_ui_assertions, NULL);
        else { select_editor_mode(NULL, "split"); g_timeout_add(350, (void *)run_ui_assertions, NULL); }
    }
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

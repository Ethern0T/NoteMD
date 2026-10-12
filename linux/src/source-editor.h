static Ptr source_language_manager, source_style_manager;

static void source_managers(void) {
    if (source_language_manager) return;
    char path[PATH_MAX]; snprintf(path, sizeof path, "%s/syntax", state.data_dir);
    source_language_manager = gtk_source_language_manager_get_default();
    gtk_source_language_manager_prepend_search_path(source_language_manager, path);
    source_style_manager = gtk_source_style_scheme_manager_get_default();
    gtk_source_style_scheme_manager_prepend_search_path(source_style_manager, path);
}

static Ptr source_editor_new(void) {
    source_managers(); Ptr view = gtk_source_view_new(), buffer = gtk_text_view_get_buffer(view);
    Ptr language = gtk_source_language_manager_get_language(source_language_manager, "notemd-markdown");
    if (!language) language = gtk_source_language_manager_get_language(source_language_manager, "markdown");
    gtk_source_buffer_set_language(buffer, language); gtk_source_buffer_set_highlight_syntax(buffer, true);
    gtk_source_buffer_set_highlight_matching_brackets(buffer, false);
    return view;
}

static Ptr source_scheme(bool light) {
    source_managers(); char id[96];
    const char *theme = strcmp(state.theme, "system") == 0 ? (light ? "white" : "black") : state.theme;
    snprintf(id, sizeof id, "notemd-%s", theme);
    Ptr scheme = gtk_source_style_scheme_manager_get_scheme(source_style_manager, id);
    if (!scheme) scheme = gtk_source_style_scheme_manager_get_scheme(source_style_manager, light ? "Adwaita" : "Adwaita-dark");
    return scheme;
}

static Ptr source_code_language(const char *name) {
    source_managers(); char id[80]; snprintf(id, sizeof id, "%s", name ? name : "");
    for (char *p = id; *p; p++) *p = (char)tolower((unsigned char)*p);
    const char *aliases[][2] = {{"python", "python3"}, {"py", "python3"}, {"javascript", "js"},
        {"jsx", "js"}, {"ts", "typescript"}, {"tsx", "typescript"}, {"bash", "sh"}, {"shell", "sh"},
        {"c++", "cpp"}, {"cxx", "cpp"}, {"rs", "rust"}, {"yml", "yaml"}};
    for (size_t i = 0; i < sizeof aliases / sizeof aliases[0]; i++)
        if (!strcmp(id, aliases[i][0])) { snprintf(id, sizeof id, "%s", aliases[i][1]); break; }
    return id[0] ? gtk_source_language_manager_get_language(source_language_manager, id) : NULL;
}

static bool source_is_light(void) {
    return !strcmp(state.theme, "white") || !strcmp(state.theme, "solarized-light") || !strcmp(state.theme, "catppuccin-latte") ||
        (!strcmp(state.theme, "system") && !is_system_dark());
}

static void source_code_buffer(Ptr buffer, const char *language) {
    gtk_source_buffer_set_language(buffer, source_code_language(language));
    gtk_source_buffer_set_highlight_syntax(buffer, true);
    gtk_source_buffer_set_style_scheme(buffer, source_scheme(source_is_light()));
}

static void apply_source_scheme(bool light) {
    Ptr scheme = source_scheme(light);
    if (state.editor_buffer) gtk_source_buffer_set_style_scheme(state.editor_buffer, scheme);
    if (state.split_buffer) gtk_source_buffer_set_style_scheme(state.split_buffer, scheme);
    for (size_t index = 0; index < state.block_count; index++)
        if (state.block_buffers[index]) gtk_source_buffer_set_style_scheme(state.block_buffers[index], scheme);
}

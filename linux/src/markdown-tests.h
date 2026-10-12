static bool syntax_color_at(Ptr buffer, const char *source, const char *needle, unsigned expected) {
    const char *position = strstr(source, needle); if (!position) return false;
    TextIter iter; gtk_text_buffer_get_iter_at_offset(buffer, &iter, utf8_offset(source, position));
    GSList *tags = gtk_text_iter_get_tags(&iter); bool found = false;
    for (GSList *item = tags; item; item = item->next) {
        gboolean set = false; g_object_get(item->data, "foreground-set", &set, NULL);
        if (set) {
            struct { float red, green, blue, alpha; } *rgba = NULL;
            g_object_get(item->data, "foreground-rgba", &rgba, NULL);
            if (rgba) {
                unsigned color = (unsigned)(rgba->red * 255 + .5) << 16 | (unsigned)(rgba->green * 255 + .5) << 8 | (unsigned)(rgba->blue * 255 + .5);
                if (color == expected) found = true;
                gdk_rgba_free(rgba);
            }
        }
    }
    g_slist_free(tags); return found;
}

static bool preview_has_tag_at(Ptr buffer, const char *text, const char *needle, const char *name) {
    const char *position = strstr(text, needle); if (!position) return false;
    TextIter iter; gtk_text_buffer_get_iter_at_offset(buffer, &iter, utf8_offset(text, position));
    Ptr tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(buffer), name);
    GSList *tags = gtk_text_iter_get_tags(&iter); bool found = false;
    for (GSList *item = tags; item; item = item->next) if (item->data == tag) found = true;
    g_slist_free(tags); return found;
}

int markdown_self_test(void) {
    const char *sample = "### Secção\n\nTexto com **negrito e _itálico_**, *ênfase*, ~~rasurado~~, `a_*b` e \\*literal\\*.\n"
        "Continua aqui.  \nNova linha &amp; &#xE3;.\n\n"
        "- Primeiro\n  - Segundo **forte**\n- [X] Feito\n\n3. Terceiro\n4. Quarto\n\n"
        "[Consulta][ref]\n\n[ref]: https://example.com/?a=1&b=2\n\n"
        "| Nome | Valor |\n| :--- | ---: |\n| A \\| B | |\n\n"
        "~~~python\ndef ola():\n    return \"Olá\"\n~~~\n\n"
        "<span style=\"color:#0969da\">Texto **azul**</span>\n";
    MarkdownNode *root = markdown_parse(sample); bool ok = root != NULL;
    char *html = markdown_to_html(sample);
    const char *markers[] = {"<h3>Secção</h3>", "<strong>negrito e <em>itálico</em></strong>",
        "<em>ênfase</em>", "<s>rasurado</s>", "<code>a_*b</code>", "*literal*", "Continua aqui.<br>",
        "Nova linha &amp; ã", "<ol start=\"3\">", "https://example.com/?a=1&amp;b=2", "A | B",
        "<td style=\"text-align:right\"></td>", "<pre><code>def ola():", "<span style=\"color:#0969da\">Texto <strong>azul</strong></span>"};
    for (size_t i = 0; i < sizeof markers / sizeof markers[0]; i++) if (!strstr(html, markers[i])) {
        fprintf(stderr, "Markdown em falta: %s\n", markers[i]); ok = false;
    }
    free(html); markdown_free(root);
    char *setext = markdown_to_html("Título\n======\n\n## Outro\n");
    ok = ok && strstr(setext, "<h1>Título</h1>"); free(setext);

    /* Exercise the actual native grammar and every palette without a window. */
    source_managers(); Ptr language = gtk_source_language_manager_get_language(source_language_manager_current(), "notemd-markdown");
    Ptr buffer = gtk_source_buffer_new(NULL); gtk_source_buffer_set_language(buffer, language);
    gtk_text_buffer_set_text(buffer, sample, -1);
    const char *themes[] = {"white", "black", "monokai", "tokyo-night", "dracula", "solarized-light"};
    const unsigned heading[] = {0x3d668c, 0xa4c5eb, 0xa8c775, 0x82aaff, 0xb69af5, 0x326c84};
    const unsigned emphasis[] = {0x9b501f, 0xe9b788, 0xdfb47b, 0xe6b48d, 0xefbd8b, 0xa25029};
    const unsigned keyword[] = {0x6b4fa4, 0xc7b1ea, 0xcfb5f0, 0xbda8f2, 0xefadc9, 0x6d5491};
    const unsigned strings[] = {0x397148, 0xa9c795, 0xa8c775, 0xa9ca97, 0xa6d1ad, 0x577423};
    for (size_t i = 0; i < sizeof themes / sizeof themes[0]; i++) {
        char id[80]; snprintf(id, sizeof id, "notemd-%s", themes[i]);
        Ptr scheme = gtk_source_style_scheme_manager_get_scheme(source_style_manager_current(), id);
        gtk_source_buffer_set_style_scheme(buffer, scheme); TextIter begin, end;
        gtk_text_buffer_get_bounds(buffer, &begin, &end); gtk_source_buffer_ensure_highlight(buffer, &begin, &end);
        bool valid = language && scheme && syntax_color_at(buffer, sample, "Secção", heading[i]) &&
            syntax_color_at(buffer, sample, "negrito", emphasis[i]) && syntax_color_at(buffer, sample, "return", keyword[i]) &&
            syntax_color_at(buffer, sample, "Olá", strings[i]);
        if (!valid) fprintf(stderr, "Sintaxe/paleta inválida: %s\n", themes[i]);
        ok = valid && ok;
    }
    char *unchanged = buffer_text(buffer); ok = ok && !strcmp(unchanged, sample); g_free(unchanged); g_object_unref(buffer);

    /* The preview must keep tight/nested lists, blank code lines and entities. */
    buffer = gtk_source_buffer_new(NULL);
    create_preview_tags(buffer);
    snprintf(state.theme, sizeof state.theme, "white");
    render_markdown(buffer, sample); char *preview = buffer_text(buffer);
    const char *preview_markers[] = {"Primeiro", "Segundo forte", "☑  Feito", "3.  Terceiro", "4.  Quarto", "Nova linha & ã", "Texto azul", "return \"Olá\""};
    for (size_t i = 0; i < sizeof preview_markers / sizeof preview_markers[0]; i++) if (!strstr(preview, preview_markers[i])) {
        fprintf(stderr, "Pré-visualização em falta: %s\n", preview_markers[i]); ok = false;
    }
    g_free(preview);

    /* Rendered code keeps native token colors through every theme change.
     * Paragraph shading also covers empty lines; it is not just a glyph fill. */
    state.preview_buffer = buffer;
    const char *code_sample = "```python\ndef ola():\n\n    return \"Olá\"\n```\n\nDepois **forte**.";
    state.editor_buffer = gtk_source_buffer_new(NULL);
    gtk_text_buffer_set_text(state.editor_buffer, code_sample, -1);
    for (size_t i = 0; i < sizeof themes / sizeof themes[0]; i++) {
        snprintf(state.theme, sizeof state.theme, "%s", themes[i]); apply_syntax_theme();
        preview = buffer_text(buffer);
        bool valid = syntax_color_at(buffer, preview, "return", keyword[i]) && syntax_color_at(buffer, preview, "Olá", strings[i]) &&
            strstr(preview, "PYTHON\ndef ola():\n\n    return \"Olá\"\n") && strstr(preview, "Depois forte");
        gboolean shaded = false; int margin = 0;
        Ptr code_tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(buffer), "code");
        g_object_get(code_tag, "paragraph-background-set", &shaded, "left-margin", &margin, NULL);
        valid = valid && shaded && margin >= 12 &&
            preview_has_tag_at(buffer, preview, "PYTHON", "code-header") &&
            preview_has_tag_at(buffer, preview, "def ola", "code") &&
            preview_has_tag_at(buffer, preview, "\n    return", "code") &&
            !preview_has_tag_at(buffer, preview, "def ola", "code-header") &&
            !preview_has_tag_at(buffer, preview, "Depois", "code");
        if (!valid) fprintf(stderr, "Bloco de código visual inválido: %s\n", themes[i]);
        ok = valid && ok; g_free(preview);
    }
    render_markdown(buffer, "```unknown-language\n**literal**\n\n<texto>\n```\n"); preview = buffer_text(buffer);
    ok = ok && strstr(preview, "**literal**\n\n<texto>\n"); g_free(preview);
    ok = ok && source_code_language("PY") == source_code_language("python3") && source_code_language("bash") == source_code_language("sh") &&
        source_code_language("javascript") == source_code_language("js") && !source_code_language("unknown-language");
    source_code_buffer(state.editor_buffer, "python");
    ok = ok && gtk_source_buffer_get_language(state.editor_buffer) == source_code_language("python3");
    source_code_buffer(state.editor_buffer, "unknown-language");
    unchanged = buffer_text(state.editor_buffer);
    ok = ok && !gtk_source_buffer_get_language(state.editor_buffer) && !strcmp(unchanged, code_sample);
    g_free(unchanged); g_object_unref(state.editor_buffer); state.editor_buffer = NULL;
    g_object_unref(buffer); state.preview_buffer = NULL;
    printf("NoteMD Markdown/syntax self-test: %s\n", ok ? "OK" : "FALHOU"); return ok ? 0 : 1;
}

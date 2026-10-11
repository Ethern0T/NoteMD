static void preview_range_tag(Ptr buffer, int offset, const char *tag) {
    TextIter start, end; gtk_text_buffer_get_iter_at_offset(buffer, &start, offset);
    gtk_text_buffer_get_end_iter(buffer, &end); gtk_text_buffer_apply_tag_by_name(buffer, tag, &start, &end);
}

/* Reuse the native code lexer, retaining selectable text in the preview.
 * Resolve each source run's effective style before copying it so nested
 * comments/strings preserve their priority. Cache by style, not by text. */
static void preview_code_highlight(Ptr target, int offset, const char *text, const char *language) {
    Ptr source = gtk_source_buffer_new(NULL); source_code_buffer(source, language);
    if (!gtk_source_buffer_get_language(source)) { g_object_unref(source); return; }
    gtk_text_buffer_set_text(source, text, -1); TextIter cursor, end;
    gtk_text_buffer_get_bounds(source, &cursor, &end); gtk_source_buffer_ensure_highlight(source, &cursor, &end);
    int length = gtk_text_buffer_get_char_count(source), position = 0;
    while (position < length) {
        GSList *tags = gtk_text_iter_get_tags(&cursor); bool foreground = false, weight_set = false, style_set = false;
        unsigned rgb = 0; int weight = 400, style = 0;
        for (GSList *item = tags; item; item = item->next) {
            gboolean fg = false, bold = false, italic = false;
            g_object_get(item->data, "foreground-set", &fg, "weight-set", &bold, "style-set", &italic, NULL);
            if (fg) {
                struct { float red, green, blue, alpha; } *rgba = NULL;
                g_object_get(item->data, "foreground-rgba", &rgba, NULL);
                if (rgba) {
                    rgb = (unsigned)(rgba->red * 255 + .5) << 16 | (unsigned)(rgba->green * 255 + .5) << 8 | (unsigned)(rgba->blue * 255 + .5);
                    foreground = true; gdk_rgba_free(rgba);
                }
            }
            if (bold) { g_object_get(item->data, "weight", &weight, NULL); weight_set = true; }
            if (italic) { g_object_get(item->data, "style", &style, NULL); style_set = true; }
        }
        g_slist_free(tags); TextIter next = cursor; gtk_text_iter_forward_to_tag_toggle(&next, NULL);
        int boundary = gtk_text_iter_get_offset(&next); if (boundary <= position) break;
        if (foreground || weight_set || style_set) {
            char name[96]; snprintf(name, sizeof name, "code-syntax-%d-%06x-%d-%d", foreground, rgb,
                weight_set ? weight : -1, style_set ? style : -1);
            Ptr table = gtk_text_buffer_get_tag_table(target), tag = gtk_text_tag_table_lookup(table, name);
            if (!tag) {
                tag = gtk_text_buffer_create_tag(target, name, NULL);
                if (foreground) { char color[8]; snprintf(color, sizeof color, "#%06x", rgb); g_object_set(tag, "foreground", color, NULL); }
                if (weight_set) g_object_set(tag, "weight", weight, NULL);
                if (style_set) g_object_set(tag, "style", style, NULL);
            }
            TextIter start, stop; gtk_text_buffer_get_iter_at_offset(target, &start, offset + position);
            gtk_text_buffer_get_iter_at_offset(target, &stop, offset + boundary);
            gtk_text_buffer_apply_tag(target, tag, &start, &stop);
        }
        cursor = next; position = boundary;
    }
    g_object_unref(source);
}

static void preview_code_block(Ptr buffer, const MarkdownNode *node) {
    StringBuilder text = {0}; markdown_plain(&text, node->children);
    char caption[96]; snprintf(caption, sizeof caption, "%s", node->destination && node->destination[0] ? node->destination : tr("Código"));
    for (char *p = caption; *p; p++) if ((unsigned char)*p < 128) *p = (char)toupper((unsigned char)*p);
    preview_line(buffer, caption, "code-header");
    int body = gtk_text_buffer_get_char_count(buffer);
    preview_tagged_segment(buffer, text.value ? text.value : "", text.length, NULL);
    if (!text.length || text.value[text.length - 1] != '\n') preview_tagged_segment(buffer, "\n", 1, NULL);
    preview_range_tag(buffer, body, "code");
    preview_code_highlight(buffer, body, text.value ? text.value : "", node->destination);
    preview_line(buffer, "", "code-footer"); preview_tagged_segment(buffer, "\n", 1, NULL);
    free(text.value);
}

static void preview_ast_inline(Ptr buffer, const MarkdownNode *node) {
    for (; node; node = node->next) {
        if (node->kind == MD_NODE_TEXT) { preview_tagged_segment(buffer, node->value, strlen(node->value), NULL); continue; }
        int start = gtk_text_buffer_get_char_count(buffer); const char *tag = NULL;
        switch (node->type) {
            case MD_SPAN_EM: tag = "emphasis"; break;
            case MD_SPAN_STRONG: tag = "strong"; break;
            case MD_SPAN_DEL: tag = "strike"; break;
            case MD_SPAN_CODE: tag = "inline-code"; break;
            case MD_SPAN_A: case MD_SPAN_WIKILINK: tag = "link"; break;
            case MD_SPAN_COLOR: {
                const char *colors[] = {"#d1242f", "#bc4c00", "#9a6700", "#1a7f37", "#0969da", "#8250df"};
                const char *tags[] = {"text-red", "text-orange", "text-yellow", "text-green", "text-blue", "text-purple"};
                for (size_t i = 0; i < 6; i++) if (!strcasecmp(node->destination, colors[i])) tag = tags[i];
                break;
            }
            case MD_SPAN_IMG: {
                StringBuilder alt = {0}; markdown_plain(&alt, node->children); StringBuilder line = {0};
                builder_printf(&line, "![%s](%s)", alt.value ? alt.value : "", node->destination);
                bool shown = preview_image(buffer, line.value); free(line.value);
                if (!shown) { preview_tagged_segment(buffer, "▧  ", strlen("▧  "), "quote"); preview_tagged_segment(buffer, alt.value ? alt.value : "Imagem indisponível", alt.length ? alt.length : strlen("Imagem indisponível"), "quote"); }
                free(alt.value); continue;
            }
        }
        preview_ast_inline(buffer, node->children);
        if (tag) preview_range_tag(buffer, start, tag);
    }
}

static void preview_ast_blocks(Ptr buffer, const MarkdownNode *node, unsigned depth, bool compact, const char *marker) {
    for (; node; node = node->next) {
        if (node->kind != MD_NODE_BLOCK) {
            for (unsigned i = 1; i < depth; i++) preview_tagged_segment(buffer, "    ", 4, NULL);
            if (marker) preview_tagged_segment(buffer, marker, strlen(marker), "list");
            /* Render one inline sibling at a time, without walking into the
             * next nested block or losing tight-list text. */
            while (node) {
                MarkdownNode single = *node; single.next = NULL; preview_ast_inline(buffer, &single);
                if (!node->next || node->next->kind == MD_NODE_BLOCK) break;
                node = node->next;
            }
            preview_tagged_segment(buffer, "\n", 1, NULL); marker = NULL; continue;
        }
        int start = gtk_text_buffer_get_char_count(buffer);
        switch (node->type) {
            case MD_BLOCK_DOC: preview_ast_blocks(buffer, node->children, depth, compact, marker); break;
            case MD_BLOCK_QUOTE:
                preview_ast_blocks(buffer, node->children, depth, true, marker); preview_range_tag(buffer, start, "quote");
                preview_tagged_segment(buffer, "\n", 1, NULL); break;
            case MD_BLOCK_UL: case MD_BLOCK_OL: {
                unsigned index = node->start;
                for (const MarkdownNode *item = node->children; item; item = item->next) {
                    char bullet[40];
                    if (item->task) snprintf(bullet, sizeof bullet, "%s  ", item->checked ? "☑" : "☐");
                    else if (node->type == MD_BLOCK_OL) snprintf(bullet, sizeof bullet, "%u.  ", index++);
                    else snprintf(bullet, sizeof bullet, "•  ");
                    int item_start = gtk_text_buffer_get_char_count(buffer);
                    preview_ast_blocks(buffer, item->children, depth + 1, node->tight, bullet);
                    if (item->checked) preview_range_tag(buffer, item_start, "done");
                }
                if (!depth) preview_tagged_segment(buffer, "\n", 1, NULL);
                break;
            }
            case MD_BLOCK_P: case MD_BLOCK_H: {
                for (unsigned i = 1; i < depth; i++) preview_tagged_segment(buffer, "    ", 4, NULL);
                if (marker) preview_tagged_segment(buffer, marker, strlen(marker), "list");
                preview_ast_inline(buffer, node->children); preview_tagged_segment(buffer, "\n", 1, NULL);
                if (node->type == MD_BLOCK_H) preview_range_tag(buffer, start, node->level == 1 ? "h1" : node->level == 2 ? "h2" : "h3");
                if (!compact) preview_tagged_segment(buffer, "\n", 1, NULL);
                break;
            }
            case MD_BLOCK_CODE:
                preview_code_block(buffer, node); break;
            case MD_BLOCK_HR: preview_line(buffer, "────────────────────────────", "divider"); break;
            case MD_BLOCK_TABLE:
                preview_ast_blocks(buffer, node->children, depth, true, NULL); preview_tagged_segment(buffer, "\n", 1, NULL); break;
            case MD_BLOCK_TR: {
                bool first = true;
                for (const MarkdownNode *cell = node->children; cell; cell = cell->next) {
                    if (!first) preview_tagged_segment(buffer, "   │   ", strlen("   │   "), "divider");
                    preview_ast_inline(buffer, cell->children); first = false;
                }
                preview_tagged_segment(buffer, "\n", 1, NULL);
                preview_range_tag(buffer, start, node->parent->type == MD_BLOCK_THEAD ? "table-header" : "table-row"); break;
            }
            default: preview_ast_blocks(buffer, node->children, depth, compact, marker); break;
        }
        marker = NULL;
    }
}

static void render_markdown(Ptr buffer, const char *source) {
    gtk_text_buffer_set_text(buffer, "", -1); MarkdownNode *root = markdown_parse(source);
    if (root) preview_ast_blocks(buffer, root, 0, false, NULL);
    else preview_tagged_segment(buffer, source, strlen(source), NULL);
    markdown_free(root);
}

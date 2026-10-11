/* Native A4 typesetting with Pango/Cairo; no display or external converter. */
typedef struct { size_t start, end; const char *url; } PdfLink;
typedef struct {
    StringBuilder markup, plain;
    PdfLink *links;
    size_t link_count;
} PdfRich;
typedef struct {
    Ptr layout;
    Ptr *lines;
    IntRect *rects;
    double *baselines;
    int count;
} PdfLayout;
typedef struct {
    Ptr surface, cr;
    double y;
    unsigned page, missing_images;
    const char *title, *base;
    int outlines[6];
} PdfDocument;
static unsigned pdf_missing_images;
enum { PDF_MARGIN = 54, PDF_BOTTOM = 779 };
static const double PDF_WIDTH = 595.276, PDF_HEIGHT = 841.89;

static void pdf_rich_free(PdfRich *rich) {
    free(rich->markup.value); free(rich->plain.value); free(rich->links); *rich = (PdfRich){0};
}

static void pdf_rich_text(PdfRich *out, const char *text) {
    builder_escape_n(&out->markup, text, strlen(text)); builder_append(&out->plain, text);
}

static void pdf_rich_node(PdfRich *out, const MarkdownNode *node) {
    if (node->kind == MD_NODE_TEXT) { pdf_rich_text(out, node->value); return; }
    const char *open = "", *close = ""; size_t link_start = out->plain.length;
    if (node->kind == MD_NODE_SPAN) switch (node->type) {
        case MD_SPAN_STRONG: open = "<b>"; close = "</b>"; break;
        case MD_SPAN_EM: open = "<i>"; close = "</i>"; break;
        case MD_SPAN_DEL: open = "<s>"; close = "</s>"; break;
        case MD_SPAN_CODE: open = "<span font_family=\"monospace\" size=\"92%\" background=\"#eef1f4\" foreground=\"#7d3855\">"; close = "</span>"; break;
        case MD_SPAN_A: case MD_SPAN_WIKILINK: open = "<span foreground=\"#356384\" underline=\"single\">"; close = "</span>"; break;
        case MD_SPAN_COLOR: builder_printf(&out->markup, "<span foreground=\"%s\">", node->destination); close = "</span>"; break;
        case MD_SPAN_IMG: pdf_rich_text(out, "[Imagem: "); break;
    }
    builder_append(&out->markup, open);
    for (const MarkdownNode *child = node->children; child; child = child->next) pdf_rich_node(out, child);
    builder_append(&out->markup, close);
    if (node->kind == MD_NODE_SPAN && node->type == MD_SPAN_IMG) pdf_rich_text(out, "]");
    if (node->kind == MD_NODE_SPAN && node->type == MD_SPAN_A && markdown_link_safe(node->destination)) {
        PdfLink *links = realloc(out->links, (out->link_count + 1) * sizeof *links);
        if (!links) die("Memória insuficiente");
        out->links = links;
        out->links[out->link_count++] = (PdfLink){link_start, out->plain.length, node->destination};
    }
}

static PdfRich pdf_rich(const MarkdownNode *children) {
    PdfRich rich = {0}; for (const MarkdownNode *child = children; child; child = child->next) pdf_rich_node(&rich, child);
    return rich;
}

static PdfLayout pdf_layout(PdfDocument *doc, const PdfRich *rich, double size, double width, bool mono, int align) {
    PdfLayout result = {0}; result.layout = pango_cairo_create_layout(doc->cr);
    pango_cairo_context_set_resolution(pango_layout_get_context(result.layout), 72);
    Ptr font = pango_font_description_from_string(mono ? "Monospace" : "Sans");
    pango_font_description_set_absolute_size(font, size * 1024);
    pango_layout_set_font_description(result.layout, font); pango_font_description_free(font);
    pango_layout_set_width(result.layout, (int)(width * 1024)); pango_layout_set_wrap(result.layout, 2);
    pango_layout_set_alignment(result.layout, align == MD_ALIGN_RIGHT ? 2 : align == MD_ALIGN_CENTER ? 1 : 0);
    pango_layout_set_markup(result.layout, rich->markup.value ? rich->markup.value : "", -1);
    result.count = pango_layout_get_line_count(result.layout);
    result.lines = calloc((size_t)result.count, sizeof *result.lines);
    result.rects = calloc((size_t)result.count, sizeof *result.rects);
    result.baselines = calloc((size_t)result.count, sizeof *result.baselines);
    Ptr iter = pango_layout_get_iter(result.layout);
    for (int i = 0; i < result.count; i++) {
        result.lines[i] = pango_layout_iter_get_line_readonly(iter);
        pango_layout_iter_get_line_extents(iter, NULL, &result.rects[i]);
        result.baselines[i] = pango_layout_iter_get_baseline(iter) / 1024.0;
        pango_layout_iter_next_line(iter);
    }
    pango_layout_iter_free(iter); return result;
}

static void pdf_layout_free(PdfLayout *layout) {
    g_object_unref(layout->layout); free(layout->lines); free(layout->rects); free(layout->baselines);
}

static void pdf_color(Ptr cr, unsigned rgb) {
    cairo_set_source_rgb(cr, ((rgb >> 16) & 255) / 255.0, ((rgb >> 8) & 255) / 255.0, (rgb & 255) / 255.0);
}

static void pdf_rect(PdfDocument *doc, double x, double y, double w, double h, unsigned color) {
    pdf_color(doc->cr, color); cairo_rectangle(doc->cr, x, y, w, h); cairo_fill(doc->cr);
}

static void pdf_small_text(PdfDocument *doc, const char *text, double x, double y, double width, int align) {
    PdfRich rich = {0}; pdf_rich_text(&rich, text); PdfLayout layout = pdf_layout(doc, &rich, 8, width, false, align);
    /* A long note title must never push the page number or wrap over content. */
    if (layout.count) {
        pdf_color(doc->cr, 0x7b8792); cairo_move_to(doc->cr, x + layout.rects[0].x / 1024.0, y - layout.rects[0].y / 1024.0 + layout.baselines[0]);
        pango_cairo_show_layout_line(doc->cr, layout.lines[0]);
    }
    pdf_layout_free(&layout); pdf_rich_free(&rich);
}

static void pdf_end_page(PdfDocument *doc) {
    double width = PDF_WIDTH - 2 * PDF_MARGIN;
    pdf_rect(doc, PDF_MARGIN, 799, width, .6, 0xdde3e8);
    pdf_small_text(doc, doc->title, PDF_MARGIN, 809, width - 60, MD_ALIGN_LEFT);
    char page[32]; snprintf(page, sizeof page, "%u", doc->page);
    pdf_small_text(doc, page, PDF_WIDTH - PDF_MARGIN - 40, 809, 40, MD_ALIGN_RIGHT);
    cairo_show_page(doc->cr);
}

static void pdf_new_page(PdfDocument *doc) {
    if (doc->page) pdf_end_page(doc);
    doc->page++; doc->y = PDF_MARGIN;
}

static void pdf_ensure(PdfDocument *doc, double height) {
    if (doc->y + height > PDF_BOTTOM && doc->y > PDF_MARGIN + .1) pdf_new_page(doc);
}

static char *pdf_tag_string(const char *text) {
    StringBuilder escaped = {0};
    for (const char *p = text; *p; p++) {
        if (*p == '\\' || *p == '\'') builder_append(&escaped, "\\");
        if ((unsigned char)*p >= 32) builder_append_n(&escaped, p, 1);
    }
    return escaped.value ? escaped.value : strdup("");
}

static double pdf_line_height(const PdfLayout *layout, int index) {
    return layout->rects[index].height / 1024.0 + 3;
}

static void pdf_draw_line(PdfDocument *doc, const PdfLayout *layout, const PdfRich *rich, int index, double x, double y) {
    IntRect rect = layout->rects[index];
    pdf_color(doc->cr, 0x293744);
    cairo_move_to(doc->cr, x + rect.x / 1024.0, y + layout->baselines[index] - rect.y / 1024.0);
    pango_cairo_show_layout_line(doc->cr, layout->lines[index]);
    int begin = pango_layout_line_get_start_index(layout->lines[index]);
    int end = begin + pango_layout_line_get_length(layout->lines[index]);
    for (size_t k = 0; k < rich->link_count; k++) {
        const PdfLink *link = &rich->links[k];
        int from = (int)link->start > begin ? (int)link->start : begin;
        int to = (int)link->end < end ? (int)link->end : end;
        if (from >= to) continue;
        int *ranges = NULL, n = 0; pango_layout_line_get_x_ranges(layout->lines[index], from, to, &ranges, &n);
        char *uri = pdf_tag_string(link->url);
        for (int r = 0; r < n; r++) {
            char attributes[8192]; snprintf(attributes, sizeof attributes, "rect=[%.3f %.3f %.3f %.3f] uri='%s'",
                x + ranges[2*r] / 1024.0, y, (ranges[2*r+1] - ranges[2*r]) / 1024.0, rect.height / 1024.0, uri);
            if (strlen(uri) + 120 < sizeof attributes) { cairo_tag_begin(doc->cr, "Link", attributes); cairo_tag_end(doc->cr, "Link"); }
        }
        free(uri); g_free(ranges);
    }
}

static void pdf_marker(PdfDocument *doc, const char *marker, double x, double y, double width) {
    PdfRich rich = {0}; pdf_rich_text(&rich, marker); PdfLayout layout = pdf_layout(doc, &rich, 11, width, false, MD_ALIGN_LEFT);
    pdf_draw_line(doc, &layout, &rich, 0, x, y); pdf_layout_free(&layout); pdf_rich_free(&rich);
}

static void pdf_paragraph(PdfDocument *doc, const PdfRich *rich, double size, double indent,
                          bool code, bool quote, bool heading, const char *marker) {
    double x = PDF_MARGIN + indent, width = PDF_WIDTH - PDF_MARGIN - x;
    double padding = code || quote ? 12 : 0;
    PdfLayout layout = pdf_layout(doc, rich, size, width - 2 * padding, code, MD_ALIGN_LEFT);
    double initial = layout.count ? pdf_line_height(&layout, 0) : size + 3;
    /* Keep headings with the following text, and avoid one-line paragraph tails. */
    pdf_ensure(doc, initial * (heading ? 1 : layout.count > 1 ? 2 : 1) + 2 * padding + (heading ? 26 : 0));
    if (padding) doc->y += 8;
    for (int i = 0; i < layout.count; i++) {
        double height = pdf_line_height(&layout, i);
        double reserve = (i == layout.count - 2 && !code) ? height + pdf_line_height(&layout, i + 1) : height;
        pdf_ensure(doc, reserve + (padding ? 8 : 0));
        if (code || quote) {
            pdf_rect(doc, x, doc->y - 4, width, height + 8, code ? 0xf3f5f7 : 0xf5f7fa);
            if (quote) pdf_rect(doc, x, doc->y - 4, 3, height + 8, 0x8fa8bb);
        }
        if (!i && marker) pdf_marker(doc, marker, x - 25, doc->y, 25);
        pdf_draw_line(doc, &layout, rich, i, x + padding, doc->y);
        doc->y += height;
    }
    doc->y += padding ? 14 : heading ? 9 : marker ? 3 : 9;
    pdf_layout_free(&layout);
}

/* Cairo can embed PNG/JPEG directly, including in a headless session where
 * the desktop's image loader service is unavailable. JPEG bytes stay intact. */
static Ptr pdf_raster_image(const char *path) {
    Ptr surface = cairo_image_surface_create_from_png(path);
    if (cairo_surface_status(surface) == 0) return surface;
    cairo_surface_destroy(surface);
    struct stat info;
    if (stat(path, &info) || info.st_size < 4 || info.st_size > 128 * 1024 * 1024) return NULL;
    FILE *file = fopen(path, "rb"); if (!file) return NULL;
    unsigned char *bytes = malloc((size_t)info.st_size); if (!bytes) { fclose(file); return NULL; }
    size_t length = fread(bytes, 1, (size_t)info.st_size, file); fclose(file);
    int width = 0, height = 0;
    if (length >= 4 && bytes[0] == 0xff && bytes[1] == 0xd8) {
        size_t offset = 2;
        while (offset + 4 <= length) {
            if (bytes[offset++] != 0xff) break;
            while (offset < length && bytes[offset] == 0xff) offset++;
            if (offset >= length) break;
            unsigned marker = bytes[offset++];
            if (marker == 0xda || marker == 0xd9) break;
            if (marker == 1 || (marker >= 0xd0 && marker <= 0xd7)) continue;
            if (offset + 2 > length) break;
            size_t size = (size_t)bytes[offset] * 256 + bytes[offset+1];
            if (size < 2 || size > length - offset) break;
            if (size >= 8 && marker >= 0xc0 && marker <= 0xcf && marker != 0xc4 && marker != 0xc8 && marker != 0xcc) {
                height = bytes[offset+3] * 256 + bytes[offset+4]; width = bytes[offset+5] * 256 + bytes[offset+6]; break;
            }
            offset += size;
        }
    }
    if (width > 0 && height > 0 && (size_t)width * height <= 64000000) {
        surface = cairo_image_surface_create(1, width, height);
        if (cairo_surface_status(surface) == 0 && cairo_surface_set_mime_data(surface, "image/jpeg", bytes, length, free, bytes) == 0) return surface;
        cairo_surface_destroy(surface);
    }
    free(bytes); return NULL;
}

static void pdf_image(PdfDocument *doc, const MarkdownNode *node, double indent) {
    char *decoded = g_uri_unescape_string(node->destination, NULL);
    const char *location = decoded ? decoded : node->destination;
    char path[PATH_MAX]; Ptr error = NULL, image = NULL, raster = NULL;
    if (!strstr(location, "://") && strncmp(location, "data:", 5)) {
        if (location[0] == '/') snprintf(path, sizeof path, "%s", location);
        else snprintf(path, sizeof path, "%s/%s", doc->base, location);
        raster = pdf_raster_image(path);
        if (!raster) image = gdk_pixbuf_new_from_file_at_scale(path, 1800, 1800, true, &error);
    }
    if (error) g_error_free(error);
    StringBuilder alt = {0}; markdown_plain(&alt, node->children);
    if (!image && !raster) {
        doc->missing_images++;
        PdfRich rich = {0}; pdf_rich_text(&rich, "Imagem indisponível");
        if (alt.length) { pdf_rich_text(&rich, " — "); pdf_rich_text(&rich, alt.value); }
        pdf_paragraph(doc, &rich, 10, indent, false, true, false, NULL); pdf_rich_free(&rich);
    } else {
        double available = PDF_WIDTH - 2 * PDF_MARGIN - indent;
        double w = raster ? cairo_image_surface_get_width(raster) : gdk_pixbuf_get_width(image);
        double h = raster ? cairo_image_surface_get_height(raster) : gdk_pixbuf_get_height(image), scale = .75;
        if (w * scale > available) scale = available / w;
        if (h * scale > 430) scale = 430 / h;
        w *= scale; h *= scale; pdf_ensure(doc, h + (alt.length ? 35 : 18));
        doc->y += 6; double x = PDF_MARGIN + indent + (available - w) / 2;
        cairo_save(doc->cr); cairo_translate(doc->cr, x, doc->y); cairo_scale(doc->cr, scale, scale);
        if (raster) cairo_set_source_surface(doc->cr, raster, 0, 0);
        else gdk_cairo_set_source_pixbuf(doc->cr, image, 0, 0);
        cairo_paint(doc->cr); cairo_restore(doc->cr);
        doc->y += h + 5;
        if (alt.length) {
            pdf_small_text(doc, alt.value, PDF_MARGIN + indent, doc->y, available, MD_ALIGN_CENTER); doc->y += 16;
        }
        doc->y += 10;
        if (image) g_object_unref(image);
        if (raster) cairo_surface_destroy(raster);
    }
    g_free(decoded); free(alt.value);
}

static void pdf_table_row(PdfDocument *doc, const MarkdownNode *row, const double *widths,
                           unsigned columns, double indent, bool header, bool stripe,
                           const MarkdownNode *repeat_header) {
    PdfLayout *layouts = calloc(columns, sizeof *layouts); PdfRich *rich = calloc(columns, sizeof *rich);
    int total_lines = 1; unsigned column = 0; double line_height = 16;
    for (const MarkdownNode *cell = row->children; cell && column < columns; cell = cell->next, column++) {
        rich[column] = pdf_rich(cell->children);
        if (header) {
            StringBuilder bold = {0}; builder_append(&bold, "<b>"); builder_append(&bold, rich[column].markup.value ? rich[column].markup.value : ""); builder_append(&bold, "</b>");
            free(rich[column].markup.value); rich[column].markup = bold;
        }
        layouts[column] = pdf_layout(doc, &rich[column], 9.5, widths[column] - 16, false, cell->align);
        if (layouts[column].count > total_lines) total_lines = layouts[column].count;
        for (int l = 0; l < layouts[column].count; l++) if (pdf_line_height(&layouts[column], l) > line_height) line_height = pdf_line_height(&layouts[column], l);
    }
    int first = 0;
    while (first < total_lines) {
        double all_height = (total_lines - first) * line_height + 14;
        if (doc->y + all_height > PDF_BOTTOM && doc->y > PDF_MARGIN + .1 &&
            (all_height <= PDF_BOTTOM - PDF_MARGIN - 45 || doc->y + line_height + 14 > PDF_BOTTOM)) {
            pdf_new_page(doc);
            if (!header && repeat_header) pdf_table_row(doc, repeat_header, widths, columns, indent, true, false, NULL);
        }
        int fit = (int)((PDF_BOTTOM - doc->y - 14) / line_height);
        if (fit < 1) fit = 1;
        int remaining = total_lines - first, amount = remaining < fit ? remaining : fit;
        double height = amount * line_height + 14, x = PDF_MARGIN + indent;
        for (unsigned c = 0; c < columns; c++) {
            pdf_rect(doc, x, doc->y, widths[c], height, header ? 0xeaf0f5 : stripe ? 0xf7f9fb : 0xffffff);
            if (c) pdf_rect(doc, x, doc->y, .5, height, 0xe0e6eb);
            for (int l = first; layouts[c].layout && l < first + amount && l < layouts[c].count; l++)
                pdf_draw_line(doc, &layouts[c], &rich[c], l, x + 8, doc->y + 7 + (l - first) * line_height);
            x += widths[c];
        }
        pdf_rect(doc, PDF_MARGIN + indent, doc->y + height - .6, x - PDF_MARGIN - indent, .6, header ? 0xb8c8d5 : 0xdde4ea);
        doc->y += height; first += amount;
        if (first < total_lines) {
            pdf_new_page(doc);
            if (!header && repeat_header) pdf_table_row(doc, repeat_header, widths, columns, indent, true, false, NULL);
        }
    }
    for (unsigned c = 0; c < columns; c++) { if (layouts[c].layout) pdf_layout_free(&layouts[c]); pdf_rich_free(&rich[c]); }
    free(layouts); free(rich);
}

static double *pdf_table_widths(PdfDocument *doc, const MarkdownNode *table, double indent) {
    unsigned columns = table->columns;
    double available = PDF_WIDTH - 2 * PDF_MARGIN - indent;
    double *widths = calloc(columns, sizeof *widths), *weights = calloc(columns, sizeof *weights);
    /* Use actual text widths, capped so long prose does not starve other columns. */
    for (const MarkdownNode *group = table->children; group; group = group->next) {
        for (const MarkdownNode *row = group->children; row; row = row->next) {
            unsigned c = 0;
            for (const MarkdownNode *cell = row->children; cell && c < columns; cell = cell->next, c++) {
                PdfRich rich = pdf_rich(cell->children); PdfLayout layout = pdf_layout(doc, &rich, 9.5, 10000, false, MD_ALIGN_LEFT);
                int w = 0, h = 0; pango_layout_get_size(layout.layout, &w, &h); double weight = w / 1024.0 + 16;
                if (weight > 200) weight = 200;
                if (weight < 55) weight = 55;
                if (weight > weights[c]) weights[c] = weight;
                pdf_layout_free(&layout); pdf_rich_free(&rich);
            }
        }
    }
    double total = 0; for (unsigned c = 0; c < columns; c++) total += weights[c] ? weights[c] : 55;
    for (unsigned c = 0; c < columns; c++) widths[c] = available * (weights[c] ? weights[c] : 55) / total;
    free(weights); return widths;
}

static double pdf_table_height(PdfDocument *doc, const MarkdownNode *table, const double *widths) {
    double height = 18;
    for (const MarkdownNode *group = table->children; group; group = group->next)
        for (const MarkdownNode *row = group->children; row; row = row->next) {
            unsigned column = 0; double row_height = 16;
            for (const MarkdownNode *cell = row->children; cell && column < table->columns; cell = cell->next, column++) {
                PdfRich rich = pdf_rich(cell->children);
                if (group->type == MD_BLOCK_THEAD) {
                    StringBuilder bold = {0}; builder_printf(&bold, "<b>%s</b>", rich.markup.value ? rich.markup.value : "");
                    free(rich.markup.value); rich.markup = bold;
                }
                PdfLayout layout = pdf_layout(doc, &rich, 9.5, widths[column] - 16, false, cell->align);
                double cell_height = 0;
                for (int i = 0; i < layout.count; i++) cell_height += pdf_line_height(&layout, i);
                if (cell_height > row_height) row_height = cell_height;
                pdf_layout_free(&layout); pdf_rich_free(&rich);
            }
            height += row_height + 14;
        }
    return height;
}

static void pdf_table(PdfDocument *doc, const MarkdownNode *table, double indent) {
    unsigned columns = table->columns; if (!columns) return;
    double *widths = pdf_table_widths(doc, table, indent);
    double height = pdf_table_height(doc, table, widths);
    const MarkdownNode *header = table->children && table->children->type == MD_BLOCK_THEAD ? table->children->children : NULL;
    pdf_ensure(doc, height < 360 ? height : 70); doc->y += 4; unsigned index = 0;
    for (const MarkdownNode *group = table->children; group; group = group->next)
        for (const MarkdownNode *row = group->children; row; row = row->next)
            pdf_table_row(doc, row, widths, columns, indent, group->type == MD_BLOCK_THEAD, index++ % 2 == 0, header);
    doc->y += 14; free(widths);
}

static void pdf_blocks(PdfDocument *doc, const MarkdownNode *nodes, double indent, bool quote, const char *marker) {
    for (const MarkdownNode *node = nodes; node; node = node->next) {
        /* Tight list items contain inline nodes directly, without a P block. */
        if (node->kind != MD_NODE_BLOCK) {
            PdfRich rich = {0};
            while (node) {
                pdf_rich_node(&rich, node);
                if (!node->next || node->next->kind == MD_NODE_BLOCK) break;
                node = node->next;
            }
            pdf_paragraph(doc, &rich, 11, indent, false, quote, false, marker);
            pdf_rich_free(&rich); marker = NULL; continue;
        }
        switch (node->type) {
            case MD_BLOCK_DOC: pdf_blocks(doc, node->children, indent, quote, marker); break;
            case MD_BLOCK_QUOTE: pdf_blocks(doc, node->children, indent + 8, true, marker); break;
            case MD_BLOCK_UL: case MD_BLOCK_OL: {
                unsigned index = node->start;
                for (const MarkdownNode *item = node->children; item; item = item->next) {
                    char bullet[40];
                    if (item->task) snprintf(bullet, sizeof bullet, "%s", item->checked ? "☑" : "☐");
                    else if (node->type == MD_BLOCK_OL) snprintf(bullet, sizeof bullet, "%u.", index++);
                    else snprintf(bullet, sizeof bullet, "•");
                    pdf_blocks(doc, item->children, indent + 26, quote, bullet);
                }
                doc->y += 5; break;
            }
            case MD_BLOCK_HR:
                pdf_ensure(doc, 18); pdf_rect(doc, PDF_MARGIN + indent, doc->y + 6, PDF_WIDTH - 2 * PDF_MARGIN - indent, 1, 0xcbd6df); doc->y += 20; break;
            case MD_BLOCK_TABLE: pdf_table(doc, node, indent); break;
            case MD_BLOCK_CODE: {
                StringBuilder text = {0}; markdown_plain(&text, node->children);
                if (text.length && text.value[text.length-1] == '\n') text.value[--text.length] = 0;
                PdfRich rich = {0}; pdf_rich_text(&rich, text.value ? text.value : "");
                pdf_paragraph(doc, &rich, 9.5, indent, true, false, false, marker);
                pdf_rich_free(&rich); free(text.value); break;
            }
            case MD_BLOCK_P: case MD_BLOCK_H: {
                bool heading = node->type == MD_BLOCK_H; PdfRich rich = {0};
                if (heading) {
                    double size = node->level == 1 ? 25 : node->level == 2 ? 17 : node->level == 3 ? 13.5 : 11.5;
                    double following = 30;
                    if (node->next && node->next->type == MD_BLOCK_TABLE && node->next->columns) {
                        double *widths = pdf_table_widths(doc, node->next, indent);
                        double height = pdf_table_height(doc, node->next, widths); free(widths);
                        if (height < 360) following = height + 20;
                    }
                    pdf_ensure(doc, size * 2 + following); if (doc->y > PDF_MARGIN) doc->y += node->level <= 2 ? 12 : 7;
                    StringBuilder label = {0}; markdown_plain(&label, node->children);
                    char attributes[128]; snprintf(attributes, sizeof attributes, "page=%u pos=[54 %.2f]", doc->page, doc->y);
                    int parent = 0; for (int level = node->level - 2; level >= 0; level--) if (doc->outlines[level]) { parent = doc->outlines[level]; break; }
                    doc->outlines[node->level-1] = cairo_pdf_surface_add_outline(doc->surface, parent, label.value ? label.value : "", attributes, 1);
                    for (int level = node->level; level < 6; level++) doc->outlines[level] = 0;
                    free(label.value); builder_append(&rich.markup, "<b><span foreground=\"#253f54\">");
                    for (const MarkdownNode *child = node->children; child; child = child->next) pdf_rich_node(&rich, child);
                    builder_append(&rich.markup, "</span></b>");
                    pdf_paragraph(doc, &rich, size, indent, false, quote, true, marker);
                } else {
                    for (const MarkdownNode *child = node->children; child; child = child->next) {
                        const MarkdownNode *image = child->kind == MD_NODE_SPAN && child->type == MD_SPAN_IMG ? child :
                            child->kind == MD_NODE_SPAN && child->type == MD_SPAN_A && child->children &&
                            child->children->kind == MD_NODE_SPAN && child->children->type == MD_SPAN_IMG && !child->children->next ? child->children : NULL;
                        if (image) {
                            if (rich.plain.length) { pdf_paragraph(doc, &rich, 11, indent, false, quote, false, marker); marker = NULL; pdf_rich_free(&rich); }
                            pdf_image(doc, image, indent);
                        } else pdf_rich_node(&rich, child);
                    }
                    if (rich.plain.length) pdf_paragraph(doc, &rich, 11, indent, false, quote, false, marker);
                }
                pdf_rich_free(&rich); break;
            }
            default: pdf_blocks(doc, node->children, indent, quote, marker); break;
        }
        marker = NULL;
    }
}

static bool pdf_export_content(const char *destination, const char *title, const char *markdown, const char *base) {
    pdf_missing_images = 0;
    MarkdownNode *root = markdown_parse(markdown); if (!root) return false;
    char temporary[PATH_MAX]; if (snprintf(temporary, sizeof temporary, "%s.tmp-XXXXXX", destination) >= (int)sizeof temporary) { markdown_free(root); return false; }
    int fd = mkstemp(temporary); if (fd < 0) { markdown_free(root); return false; } close(fd);
    PdfDocument doc = { .surface = cairo_pdf_surface_create(temporary, PDF_WIDTH, PDF_HEIGHT), .title = title, .base = base ? base : "." };
    doc.cr = cairo_create(doc.surface);
    cairo_pdf_surface_set_metadata(doc.surface, 0, title); cairo_pdf_surface_set_metadata(doc.surface, 4, "NoteMD");
    pdf_new_page(&doc);
    const MarkdownNode *first = root->children;
    if (!first || first->type != MD_BLOCK_H || first->level != 1) {
        PdfRich rich = {0}; builder_append(&rich.markup, "<b><span foreground=\"#253f54\">"); pdf_rich_text(&rich, title); builder_append(&rich.markup, "</span></b>");
        pdf_paragraph(&doc, &rich, 25, 0, false, false, true, NULL); pdf_rich_free(&rich);
        pdf_rect(&doc, PDF_MARGIN, doc.y + 2, 42, 2, 0x6486a0); doc.y += 20;
    }
    pdf_blocks(&doc, root->children, 0, false, NULL); pdf_end_page(&doc);
    cairo_destroy(doc.cr); cairo_surface_finish(doc.surface);
    bool ok = cairo_surface_status(doc.surface) == 0; cairo_surface_destroy(doc.surface); markdown_free(root);
    if (ok) ok = rename(temporary, destination) == 0;
    if (!ok) unlink(temporary);
    pdf_missing_images = doc.missing_images; return ok;
}

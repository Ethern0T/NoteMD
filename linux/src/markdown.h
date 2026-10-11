/* Shared Markdown tree for preview, HTML and PDF. MD4C handles CommonMark
 * parsing and the table, task, strike, autolink and wiki-link extensions. */
typedef enum { MD_NODE_BLOCK, MD_NODE_SPAN, MD_NODE_TEXT } MarkdownKind;
enum { MD_SPAN_COLOR = 100 };
typedef struct MarkdownNode {
    MarkdownKind kind;
    int type, level, align;
    unsigned start, columns;
    bool tight, task, checked;
    char *value, *destination;
    struct MarkdownNode *parent, *children, *last, *next;
} MarkdownNode;
typedef struct { MarkdownNode *root, *current; unsigned depth; } MarkdownParser;
static int (*notemd_md_parse)(const char *, MD_SIZE, const MD_PARSER *, void *);

static void markdown_free(MarkdownNode *node) {
    while (node) {
        MarkdownNode *next = node->next;
        markdown_free(node->children); free(node->value); free(node->destination); free(node); node = next;
    }
}

static MarkdownNode *markdown_node(MarkdownParser *parser, MarkdownKind kind, int type) {
    MarkdownNode *node = calloc(1, sizeof *node); if (!node) die("Memória insuficiente");
    node->kind = kind; node->type = type; node->parent = parser->current;
    if (parser->current) {
        if (parser->current->last) parser->current->last->next = node;
        else parser->current->children = node;
        parser->current->last = node;
    } else parser->root = node;
    return node;
}

static char *markdown_entity(const char *text, size_t length) {
    char *entity = strndup(text, length);
    const char *names[] = {"&amp;", "&lt;", "&gt;", "&quot;", "&apos;", "&nbsp;", "&copy;", "&reg;", "&mdash;", "&ndash;", "&hellip;"};
    const char *values[] = {"&", "<", ">", "\"", "'", " ", "©", "®", "—", "–", "…"};
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) if (!strcmp(entity, names[i])) {
        free(entity); return strdup(values[i]);
    }
    if (length > 3 && entity[0] == '&' && entity[1] == '#') {
        char *digits = entity + 2, *end; int base = 10;
        if (*digits == 'x' || *digits == 'X') { digits++; base = 16; }
        unsigned long code = strtoul(digits, &end, base);
        if (end != digits && *end == ';' && !end[1]) {
            if (!code || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) code = 0xfffd;
            char utf8[8] = {0}; g_unichar_to_utf8((unsigned)code, utf8); free(entity); return strdup(utf8);
        }
    }
    return entity;
}

static char *markdown_attribute(MD_ATTRIBUTE attribute) {
    StringBuilder result = {0}; unsigned index = 0;
    while (attribute.size && attribute.substr_offsets[index] < attribute.size) {
        unsigned begin = attribute.substr_offsets[index], end = attribute.substr_offsets[index + 1];
        if (attribute.substr_types[index] == MD_TEXT_ENTITY) {
            char *decoded = markdown_entity(attribute.text + begin, end - begin); builder_append(&result, decoded); free(decoded);
        } else builder_append_n(&result, attribute.text + begin, end - begin);
        index++;
    }
    return result.value ? result.value : strdup("");
}

static int markdown_enter_block(MD_BLOCKTYPE type, void *detail, void *data) {
    MarkdownParser *parser = data; if (++parser->depth > 128) return 1;
    MarkdownNode *node = markdown_node(parser, MD_NODE_BLOCK, type); parser->current = node;
    if (type == MD_BLOCK_H) node->level = ((MD_BLOCK_H_DETAIL *)detail)->level;
    if (type == MD_BLOCK_UL) node->tight = ((MD_BLOCK_UL_DETAIL *)detail)->is_tight;
    if (type == MD_BLOCK_OL) { node->start = ((MD_BLOCK_OL_DETAIL *)detail)->start; node->tight = ((MD_BLOCK_OL_DETAIL *)detail)->is_tight; }
    if (type == MD_BLOCK_LI) {
        MD_BLOCK_LI_DETAIL *item = detail; node->task = item->is_task;
        node->checked = item->is_task && (item->task_mark == 'x' || item->task_mark == 'X');
    }
    if (type == MD_BLOCK_CODE) node->destination = markdown_attribute(((MD_BLOCK_CODE_DETAIL *)detail)->lang);
    if (type == MD_BLOCK_TABLE) node->columns = ((MD_BLOCK_TABLE_DETAIL *)detail)->col_count;
    if (type == MD_BLOCK_TH || type == MD_BLOCK_TD) node->align = ((MD_BLOCK_TD_DETAIL *)detail)->align;
    return 0;
}

static int markdown_leave_block(MD_BLOCKTYPE type, void *detail, void *data) {
    (void)type; (void)detail; MarkdownParser *parser = data;
    parser->current = parser->current->parent; parser->depth--; return 0;
}

static int markdown_enter_span(MD_SPANTYPE type, void *detail, void *data) {
    MarkdownParser *parser = data; if (++parser->depth > 128) return 1;
    MarkdownNode *node = markdown_node(parser, MD_NODE_SPAN, type); parser->current = node;
    if (type == MD_SPAN_A) node->destination = markdown_attribute(((MD_SPAN_A_DETAIL *)detail)->href);
    if (type == MD_SPAN_IMG) node->destination = markdown_attribute(((MD_SPAN_IMG_DETAIL *)detail)->src);
    if (type == MD_SPAN_WIKILINK) node->destination = markdown_attribute(((MD_SPAN_WIKILINK_DETAIL *)detail)->target);
    return 0;
}

static int markdown_leave_span(MD_SPANTYPE type, void *detail, void *data) {
    (void)type; (void)detail; MarkdownParser *parser = data;
    parser->current = parser->current->parent; parser->depth--; return 0;
}

static int markdown_text(MD_TEXTTYPE type, const char *text, MD_SIZE length, void *data) {
    MarkdownNode *node = markdown_node(data, MD_NODE_TEXT, type);
    if (type == MD_TEXT_NULLCHAR) node->value = strdup("�");
    else if (type == MD_TEXT_BR) node->value = strdup("\n");
    else if (type == MD_TEXT_SOFTBR) node->value = strdup(" ");
    else if (type == MD_TEXT_ENTITY) node->value = markdown_entity(text, length);
    else node->value = strndup(text, length);
    return 0;
}

/* NoteMD's colour toolbar emits this narrow HTML subset. Keep other HTML
 * visible as text, including malformed/unbalanced colour spans. */
static bool markdown_color(const MarkdownNode *node, char color[8]) {
    if (node->kind != MD_NODE_TEXT || node->type != MD_TEXT_HTML || strlen(node->value) != 28 ||
        strncmp(node->value, "<span style=\"color:#", 19) || strcmp(node->value + 26, "\">")) return false;
    memcpy(color, node->value + 19, 7); color[7] = 0;
    const char *allowed[] = {"#d1242f", "#bc4c00", "#9a6700", "#1a7f37", "#0969da", "#8250df"};
    for (size_t i = 0; i < sizeof allowed / sizeof allowed[0]; i++) if (!strcasecmp(color, allowed[i])) return true;
    return false;
}

static void markdown_colors(MarkdownNode *parent) {
    for (MarkdownNode *node = parent->children; node; node = node->next) {
        char color[8];
        if (markdown_color(node, color)) {
            MarkdownNode *end = node->next, *previous = node; unsigned depth = 1;
            for (; end; previous = end, end = end->next) {
                char nested[8];
                if (markdown_color(end, nested)) depth++;
                if (end->kind == MD_NODE_TEXT && end->type == MD_TEXT_HTML && !strcmp(end->value, "</span>") && !--depth) break;
            }
            if (end && previous != node) {
                node->kind = MD_NODE_SPAN; node->type = MD_SPAN_COLOR; node->destination = strdup(color);
                node->children = node->next; node->last = previous; previous->next = NULL; node->next = end->next;
                for (MarkdownNode *child = node->children; child; child = child->next) child->parent = node;
                end->next = NULL; markdown_free(end); if (parent->last == end) parent->last = node;
            }
        }
        markdown_colors(node);
    }
}

static MarkdownNode *markdown_parse(const char *source) {
    if (!notemd_md_parse) {
        static Ptr library;
        if (!library) library = dlopen("libmd4c.so.0", RTLD_NOW | RTLD_LOCAL);
        if (!library) return NULL;
        *(void **)&notemd_md_parse = dlsym(library, "md_parse"); if (!notemd_md_parse) return NULL;
    }
    if (strlen(source) > UINT_MAX) return NULL;
    MD_PARSER callbacks = { .flags = MD_DIALECT_GITHUB | MD_FLAG_WIKILINKS | MD_FLAG_NOHTMLBLOCKS,
        .enter_block = markdown_enter_block, .leave_block = markdown_leave_block,
        .enter_span = markdown_enter_span, .leave_span = markdown_leave_span, .text = markdown_text };
    MarkdownParser parser = {0};
    if (notemd_md_parse(source, (MD_SIZE)strlen(source), &callbacks, &parser)) { markdown_free(parser.root); return NULL; }
    markdown_colors(parser.root); return parser.root;
}

static void markdown_plain(StringBuilder *out, const MarkdownNode *node) {
    for (; node; node = node->next) {
        if (node->kind == MD_NODE_TEXT) builder_append(out, node->value);
        else markdown_plain(out, node->children);
    }
}

static bool markdown_link_safe(const char *url) {
    return url && (strncasecmp(url, "https://", 8) == 0 || strncasecmp(url, "http://", 7) == 0 ||
                   strncasecmp(url, "mailto:", 7) == 0);
}

static void markdown_html(StringBuilder *out, const MarkdownNode *node) {
    for (; node; node = node->next) {
        if (node->kind == MD_NODE_TEXT) {
            if (node->type == MD_TEXT_BR) builder_append(out, "<br>\n");
            else builder_escape_n(out, node->value, strlen(node->value));
            continue;
        }
        const char *tag = NULL; char heading[8]; bool special = false;
        if (node->kind == MD_NODE_SPAN) {
            switch (node->type) {
                case MD_SPAN_EM: tag = "em"; break;
                case MD_SPAN_STRONG: tag = "strong"; break;
                case MD_SPAN_DEL: tag = "s"; break;
                case MD_SPAN_CODE: tag = "code"; break;
                case MD_SPAN_A:
                    if (markdown_link_safe(node->destination)) {
                        builder_append(out, "<a href=\""); builder_escape_n(out, node->destination, strlen(node->destination));
                        builder_append(out, "\">"); special = true; tag = "a";
                    }
                    break;
                case MD_SPAN_IMG: {
                    StringBuilder alt = {0}; markdown_plain(&alt, node->children);
                    builder_append(out, "<img src=\""); builder_escape_n(out, node->destination, strlen(node->destination));
                    builder_append(out, "\" alt=\""); if (alt.value) builder_escape_n(out, alt.value, alt.length);
                    builder_append(out, "\">"); free(alt.value); continue;
                }
                case MD_SPAN_COLOR: builder_printf(out, "<span style=\"color:%s\">", node->destination); special = true; tag = "span"; break;
                case MD_SPAN_WIKILINK: builder_append(out, "<span class=\"wikilink\">"); special = true; tag = "span"; break;
            }
        } else switch (node->type) {
            case MD_BLOCK_P: tag = "p"; break;
            case MD_BLOCK_H: snprintf(heading, sizeof heading, "h%d", node->level); tag = heading; break;
            case MD_BLOCK_QUOTE: tag = "blockquote"; break;
            case MD_BLOCK_UL: tag = "ul"; break;
            case MD_BLOCK_OL: builder_printf(out, "<ol start=\"%u\">", node->start); tag = "ol"; special = true; break;
            case MD_BLOCK_LI:
                tag = "li";
                if (node->task) { builder_printf(out, "<li class=\"task%s\">%s ", node->checked ? " done" : "", node->checked ? "☑" : "☐"); special = true; }
                break;
            case MD_BLOCK_HR: builder_append(out, "<hr>\n"); continue;
            case MD_BLOCK_CODE: builder_append(out, "<pre><code>"); markdown_html(out, node->children); builder_append(out, "</code></pre>\n"); continue;
            case MD_BLOCK_TABLE: tag = "table"; break;
            case MD_BLOCK_THEAD: tag = "thead"; break;
            case MD_BLOCK_TBODY: tag = "tbody"; break;
            case MD_BLOCK_TR: tag = "tr"; break;
            case MD_BLOCK_TH: case MD_BLOCK_TD:
                tag = node->type == MD_BLOCK_TH ? "th" : "td";
                builder_printf(out, "<%s style=\"text-align:%s\">", tag, node->align == MD_ALIGN_RIGHT ? "right" : node->align == MD_ALIGN_CENTER ? "center" : "left");
                special = true; break;
        }
        if (tag && !special) builder_printf(out, "<%s>", tag);
        if (node->kind == MD_NODE_BLOCK && node->type == MD_BLOCK_LI && node->checked) builder_append(out, "<s>");
        markdown_html(out, node->children);
        if (node->kind == MD_NODE_BLOCK && node->type == MD_BLOCK_LI && node->checked) builder_append(out, "</s>");
        if (tag) builder_printf(out, "</%s>%s", tag, node->kind == MD_NODE_BLOCK ? "\n" : "");
    }
}

static char *markdown_to_html(const char *source) {
    MarkdownNode *root = markdown_parse(source); StringBuilder out = {0};
    if (root) markdown_html(&out, root);
    else { builder_append(&out, "<pre>"); builder_escape_n(&out, source, strlen(source)); builder_append(&out, "</pre>"); }
    markdown_free(root); return out.value ? out.value : strdup("");
}

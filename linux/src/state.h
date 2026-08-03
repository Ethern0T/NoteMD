#ifndef STATE_H
#define STATE_H

#define _GNU_SOURCE
#include <dlfcn.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef void *Ptr;
typedef int gboolean;
typedef unsigned int guint;
typedef unsigned long GType;
typedef long gssize;
typedef struct GSList { void *data; struct GSList *next; } GSList;

enum { GTK_ORIENTATION_HORIZONTAL, GTK_ORIENTATION_VERTICAL };
enum { GTK_WRAP_NONE, GTK_WRAP_CHAR, GTK_WRAP_WORD, GTK_WRAP_WORD_CHAR };
enum { GTK_POLICY_ALWAYS, GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER, GTK_POLICY_EXTERNAL };
enum { G_APPLICATION_DEFAULT_FLAGS = 0, G_APPLICATION_HANDLES_OPEN = 1 << 2, G_APPLICATION_NON_UNIQUE = 1 << 5 };
enum { GTK_FILE_CHOOSER_ACTION_OPEN, GTK_FILE_CHOOSER_ACTION_SAVE, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER };
enum { GTK_RESPONSE_ACCEPT = -3 };
enum { GTK_RESPONSE_YES = -8 };
enum { GTK_DIALOG_MODAL = 1, GTK_MESSAGE_WARNING = 1, GTK_BUTTONS_YES_NO = 4 };
enum { GDK_CONTROL_MASK = 1 << 2, GDK_SHIFT_MASK = 1 << 0 };
enum { GDK_ACTION_COPY = 1 << 0, GDK_ACTION_MOVE = 1 << 1 };

typedef struct Notebook {
    char title[256];
    char color[16];
    char path[PATH_MAX];
    bool external;
    bool expanded;
    char note_order[128][64];
    size_t note_order_count;
    Ptr button, add_button, menu_button, expander, context_gesture;
    struct Notebook *next;
} Notebook;

typedef struct Note {
    char title[256];
    char color[16];
    char tags[512];
    char id[64];
    char directory[PATH_MAX];
    char markdown_path[PATH_MAX];
    Notebook *notebook;
    bool external;
    bool dirty;
    bool opened;
    int order;
    time_t external_mtime;
    char *draft;
    char draft_title[256], draft_tags[512];
    Ptr button, menu_button, dirty_indicator, context_gesture;
    struct Note *next;
} Note;

typedef struct {
    Ptr cells[32][16];
    size_t rows, columns, block_index;
    bool syncing;
} TableEditor;

typedef struct {
    void *gtk;
    Ptr app, window, sidebar, sidebar_panel, sidebar_toggle, tag_box, search, tabbar, title, tags, note_tags_box, tag_input, linksbar, editor, status, stats, line_badge, root_label;
    Ptr find_window, find_entry, replace_entry, side_new_note, external_conflict_dialog, close_note_dialog;
    Ptr history_window, history_list, history_saved_buffer, history_current_buffer;
    Ptr block_box, block_buffers[256], block_checks[256], block_headings[256];
    bool block_is_heading[256], block_is_task[256];
    bool block_is_bullet[256], block_is_numbered[256], block_is_quote[256];
    bool block_is_code[256];
    Ptr block_languages[256];
    bool block_is_image[256];
    Ptr block_image_alt[256], block_image_location[256];
    bool block_is_divider[256];
    int block_numbers[256];
    TableEditor *block_tables[256];
    size_t block_count;
    size_t visual_image_count;
    size_t context_menu_count;
    Ptr mode_stack, mode_editor, mode_visual, mode_split, editor_buffer, line_buffer, preview_view, split_source_view, split_preview_view, format_heading, format_color;
    Ptr preview_buffer, split_buffer, split_preview_buffer, theme_provider, active_popover;
    Ptr syntax_heading, syntax_emphasis, syntax_code, syntax_code_block, syntax_link, syntax_list, syntax_quote;
    Notebook *notebooks, *active_notebook;
    Note *notes, *active;
    char root[PATH_MAX];
    char data_dir[PATH_MAX];
    char theme[32];
    char language[8];
    char selected_tag[128];
    char external_paths[32][PATH_MAX];
    size_t external_count;
    bool loading;
    bool block_syncing;
    bool view_syncing;
    bool split_origin;
    bool sidebar_hidden;
    bool autosave;
    bool force_quit;
    bool ui_test;
    bool empty_ui_test;
    bool ui_test_failed;
    char ui_test_dir[PATH_MAX];
    guint autosave_source;
    size_t find_offset;
    int active_line;
} State;





typedef struct { char opaque[128]; } TextIter;
typedef struct { Ptr widget, target; bool notebook; } ContextTarget;
typedef struct { int x, y, width, height; } IntRect;





#define LOAD(name) load_symbol((void **)&name, #name)






























































typedef struct { Ptr window; const char *title; const char *markdown; } TemplateData;










typedef struct { Note *note; } ClipboardImageRequest;






typedef struct { char *value; size_t length, capacity; } StringBuilder;



























typedef struct { char path[PATH_MAX]; Ptr row, parent, preview_buffer; } RestoreData;




















































typedef struct { Ptr window, entry; Notebook *notebook; } RenameNotebookData;































































extern State state;

const char *tr(const char *pt);

/* GTK Symbols */
extern Ptr (*gtk_application_new) (const char *, int);
extern int (*g_application_run) (Ptr, int, char **);
extern void (*g_application_quit) (Ptr);
extern void (*g_object_unref) (Ptr);
extern Ptr (*g_object_ref) (Ptr);
extern void (*g_object_set) (Ptr, const char *, ...);
extern void (*g_object_get) (Ptr, const char *, ...);
extern unsigned long (*g_signal_connect_data) (Ptr, const char *, void *, Ptr, void *, int);
extern void (*g_signal_emit_by_name) (Ptr, const char *, ...);
extern Ptr (*gtk_application_window_new) (Ptr);
extern void (*gtk_window_set_title) (Ptr, const char *);
extern void (*gtk_window_set_default_size) (Ptr, int, int);
extern void (*gtk_window_set_child) (Ptr, Ptr);
extern void (*gtk_window_present) (Ptr);
extern Ptr (*gtk_window_new) (void);
extern Ptr (*gtk_settings_get_default) (void);
extern Ptr (*gtk_window_get_focus) (Ptr);
extern void (*gtk_window_set_transient_for) (Ptr, Ptr);
extern void (*gtk_window_set_modal) (Ptr, gboolean);
extern void (*gtk_window_destroy) (Ptr);
extern Ptr (*gtk_message_dialog_new) (Ptr, int, int, int, const char *, ...);
extern void (*gtk_message_dialog_format_secondary_text) (Ptr, const char *, ...);
extern Ptr (*gtk_dialog_add_button) (Ptr, const char *, int);
extern Ptr (*gtk_box_new) (int, int);
extern void (*gtk_box_append) (Ptr, Ptr);
extern void (*gtk_box_remove) (Ptr, Ptr);
extern Ptr (*gtk_grid_new) (void);
extern void (*gtk_grid_attach) (Ptr, Ptr, int, int, int, int);
extern void (*gtk_grid_set_row_spacing) (Ptr, unsigned);
extern void (*gtk_grid_set_column_spacing) (Ptr, unsigned);
extern Ptr (*gtk_expander_new) (const char *);
extern void (*gtk_expander_set_child) (Ptr, Ptr);
extern void (*gtk_expander_set_label_widget) (Ptr, Ptr);
extern void (*gtk_expander_set_expanded) (Ptr, gboolean);
extern gboolean (*gtk_expander_get_expanded) (Ptr);
extern Ptr (*gtk_widget_get_first_child) (Ptr);
extern Ptr (*gtk_widget_get_next_sibling) (Ptr);
extern Ptr (*gtk_paned_new) (int);
extern void (*gtk_paned_set_start_child) (Ptr, Ptr);
extern void (*gtk_paned_set_end_child) (Ptr, Ptr);
extern void (*gtk_paned_set_position) (Ptr, int);
extern Ptr (*gtk_scrolled_window_new) (void);
extern void (*gtk_scrolled_window_set_child) (Ptr, Ptr);
extern void (*gtk_scrolled_window_set_policy) (Ptr, int, int);
extern Ptr (*gtk_scrolled_window_get_vadjustment) (Ptr);
extern void (*gtk_scrolled_window_set_vadjustment) (Ptr, Ptr);
extern void (*gtk_widget_set_hexpand) (Ptr, gboolean);
extern void (*gtk_widget_set_vexpand) (Ptr, gboolean);
extern void (*gtk_widget_set_size_request) (Ptr, int, int);
extern void (*gtk_widget_set_visible) (Ptr, gboolean);
extern gboolean (*gtk_widget_get_visible) (Ptr);
extern void (*gtk_widget_add_css_class) (Ptr, const char *);
extern void (*gtk_widget_remove_css_class) (Ptr, const char *);
extern gboolean (*gtk_widget_has_css_class) (Ptr, const char *);
extern void (*gtk_widget_set_tooltip_text) (Ptr, const char *);
extern int (*gtk_widget_get_width) (Ptr);
extern void (*gtk_widget_add_controller) (Ptr, Ptr);
extern Ptr (*gtk_event_controller_key_new) (void);
extern void (*gtk_event_controller_set_propagation_phase) (Ptr, int);
extern Ptr (*gtk_gesture_click_new) (void);
extern void (*gtk_gesture_single_set_button) (Ptr, unsigned);
extern void (*gtk_widget_set_parent) (Ptr, Ptr);
extern void (*gtk_widget_unparent) (Ptr);
extern Ptr (*gtk_popover_new) (void);
extern void (*gtk_popover_set_child) (Ptr, Ptr);
extern void (*gtk_popover_set_pointing_to) (Ptr, const void *);
extern void (*gtk_popover_popup) (Ptr);
extern void (*gtk_popover_popdown) (Ptr);
extern Ptr (*gtk_drop_target_new) (GType, int);
extern Ptr (*gtk_drag_source_new) (void);
extern void (*gtk_drag_source_set_actions) (Ptr, int);
extern GType (*gdk_file_list_get_type) (void);
extern Ptr (*g_value_get_boxed) (const void *);
extern const char * (*g_value_get_string) (const void *);
extern GType (*g_type_from_name) (const char *);
extern Ptr (*gdk_content_provider_new_typed) (GType, ...);
extern Ptr (*gtk_label_new) (const char *);
extern void (*gtk_label_set_text) (Ptr, const char *);
extern void (*gtk_label_set_xalign) (Ptr, float);
extern Ptr (*gtk_entry_new) (void);
extern Ptr (*gtk_search_entry_new) (void);
extern void (*gtk_search_entry_set_placeholder_text) (Ptr, const char *);
extern void (*gtk_entry_set_placeholder_text) (Ptr, const char *);
extern const char * (*gtk_editable_get_text) (Ptr);
extern void (*gtk_editable_set_text) (Ptr, const char *);
extern Ptr (*gtk_button_new_with_label) (const char *);
extern Ptr (*gtk_button_new_from_icon_name) (const char *);
extern void (*gtk_button_set_child) (Ptr, Ptr);
extern Ptr (*gtk_check_button_new_with_label) (const char *);
extern void (*gtk_check_button_set_active) (Ptr, gboolean);
extern gboolean (*gtk_check_button_get_active) (Ptr);
extern void (*gtk_button_set_label) (Ptr, const char *);
extern void (*gtk_button_set_icon_name) (Ptr, const char *);
extern Ptr (*gtk_text_view_new) (void);
extern void (*gtk_text_view_set_monospace) (Ptr, gboolean);
extern void (*gtk_text_view_set_wrap_mode) (Ptr, int);
extern void (*gtk_text_view_set_left_margin) (Ptr, int);
extern void (*gtk_text_view_set_right_margin) (Ptr, int);
extern void (*gtk_text_view_set_top_margin) (Ptr, int);
extern Ptr (*gtk_text_view_get_buffer) (Ptr);
extern Ptr (*gtk_text_buffer_get_tag_table) (Ptr);
extern Ptr (*gtk_text_buffer_new) (Ptr);
extern void (*gtk_text_view_set_buffer) (Ptr, Ptr);
extern void (*gtk_text_buffer_set_text) (Ptr, const char *, int);
extern void (*gtk_text_buffer_get_bounds) (Ptr, void *, void *);
extern char * (*gtk_text_buffer_get_text) (Ptr, const void *, const void *, gboolean);
extern void (*gtk_text_buffer_insert_at_cursor) (Ptr, const char *, int);
extern void (*gtk_text_buffer_get_iter_at_offset) (Ptr, void *, int);
extern void (*gtk_text_buffer_select_range) (Ptr, const void *, const void *);
extern gboolean (*gtk_text_buffer_get_selection_bounds) (Ptr, void *, void *);
extern void (*gtk_text_buffer_delete) (Ptr, void *, void *);
extern void (*gtk_text_buffer_get_end_iter) (Ptr, void *);
extern void (*gtk_text_buffer_insert) (Ptr, void *, const char *, int);
extern int (*gtk_text_buffer_get_char_count) (Ptr);
extern Ptr (*gtk_text_buffer_get_insert) (Ptr);
extern void (*gtk_text_buffer_get_iter_at_mark) (Ptr, void *, Ptr);
extern int (*gtk_text_iter_get_line) (const void *);
extern Ptr (*gtk_text_buffer_create_tag) (Ptr, const char *, const char *, ...);
extern void (*gtk_text_buffer_apply_tag_by_name) (Ptr, const char *, const void *, const void *);
extern void (*gtk_text_buffer_remove_all_tags) (Ptr, const void *, const void *);
extern void (*gtk_text_buffer_set_enable_undo) (Ptr, gboolean);
extern Ptr (*gtk_text_buffer_create_child_anchor) (Ptr, void *);
extern void (*gtk_text_view_add_child_at_anchor) (Ptr, Ptr, Ptr);
extern Ptr (*gtk_picture_new_for_filename) (const char *);
extern void (*gtk_picture_set_content_fit) (Ptr, int);
extern void (*gtk_text_view_scroll_to_iter) (Ptr, void *, double, gboolean, double, double);
extern void (*gtk_text_view_set_editable) (Ptr, gboolean);
extern void (*gtk_text_view_set_cursor_visible) (Ptr, gboolean);
extern Ptr (*gtk_stack_new) (void);
extern Ptr (*gtk_stack_add_titled) (Ptr, Ptr, const char *, const char *);
extern void (*gtk_stack_set_visible_child_name) (Ptr, const char *);
extern const char * (*gtk_stack_get_visible_child_name) (Ptr);
extern Ptr (*gtk_stack_switcher_new) (void);
extern void (*gtk_stack_switcher_set_stack) (Ptr, Ptr);
extern Ptr (*gtk_drop_down_new_from_strings) (const char *const *);
extern void (*gtk_drop_down_set_selected) (Ptr, guint);
extern guint (*gtk_drop_down_get_selected) (Ptr);
extern void (*g_free) (Ptr);
extern void (*g_error_free) (Ptr);
extern guint (*g_timeout_add) (guint, void *, Ptr);
extern gboolean (*g_source_remove) (guint);
extern Ptr (*gtk_image_new_from_file) (const char *);
extern Ptr (*gtk_image_new_from_icon_name) (const char *);
extern void (*gtk_image_set_pixel_size) (Ptr, int);
extern void (*gtk_widget_set_margin_start) (Ptr, int);
extern void (*gtk_widget_set_margin_end) (Ptr, int);
extern void (*gtk_widget_set_margin_top) (Ptr, int);
extern void (*gtk_widget_set_margin_bottom) (Ptr, int);
extern Ptr (*gtk_separator_new) (int);
extern Ptr (*gtk_css_provider_new) (void);
extern void (*gtk_css_provider_load_from_path) (Ptr, const char *);
extern Ptr (*gdk_display_get_default) (void);
extern Ptr (*gdk_display_get_clipboard) (Ptr);
extern Ptr (*gdk_clipboard_get_formats) (Ptr);
extern gboolean (*gdk_content_formats_contain_gtype) (Ptr, GType);
extern GType (*gdk_texture_get_type) (void);
extern void (*gdk_clipboard_read_texture_async) (Ptr, Ptr, void *, Ptr);
extern Ptr (*gdk_clipboard_read_texture_finish) (Ptr, Ptr, Ptr *);
extern gboolean (*gdk_texture_save_to_png) (Ptr, const char *);
extern void (*gtk_style_context_add_provider_for_display) (Ptr, Ptr, guint);
extern void (*gtk_style_context_remove_provider_for_display) (Ptr, Ptr);
extern Ptr (*gtk_file_chooser_native_new) (const char *, Ptr, int, const char *, const char *);
extern Ptr (*gtk_file_chooser_get_file) (Ptr);
extern char * (*g_file_get_path) (Ptr);
extern void (*gtk_native_dialog_show) (Ptr);
extern void (*gtk_file_chooser_set_current_name) (Ptr, const char *);
extern Ptr (*g_file_new_for_path) (const char *);
extern gboolean (*g_file_trash) (Ptr, Ptr, Ptr);
extern Ptr (*cairo_pdf_surface_create) (const char *, double, double);
extern Ptr (*cairo_create) (Ptr);
extern void (*cairo_select_font_face) (Ptr, const char *, int, int);
extern void (*cairo_set_font_size) (Ptr, double);
extern void (*cairo_move_to) (Ptr, double, double);
extern void (*cairo_show_text) (Ptr, const char *);
extern void (*cairo_show_page) (Ptr);
extern void (*cairo_destroy) (Ptr);
extern void (*cairo_surface_destroy) (Ptr);
extern void (*cairo_surface_finish) (Ptr);
extern int (*cairo_surface_status) (Ptr);

/* Function Declarations */
const char *tr(const char *pt);
void die(const char *message);
void load_symbol(void **target, const char *name);
void load_gtk(void);
void margins(Ptr widget, int horizontal, int vertical);
Ptr icon_button(const char *icon, const char *tooltip);
Ptr navigation_button(const char *icon, const char *text);
Ptr text_button(const char *text, const char *css_class);
gboolean release_popover_reference(Ptr popover);
void active_popover_closed(Ptr popover, Ptr unused);
void dismiss_active_popover(void);
void present_popover(Ptr popover, Ptr anchor);
void load_style(void);
void config_path(char output[PATH_MAX]);
void save_settings(void);
void load_settings(void);
void apply_syntax_theme(void);
void apply_theme(const char *theme);
bool is_directory(const char *path);
bool make_directory(const char *path);
void free_notes(void);
void discard_note_draft(Note *note);
void capture_active_draft(void);
const char *note_display_title(const Note *note);
const char *note_display_tags(const Note *note);
char *note_current_markdown(const Note *note);
bool save_note_payload(Note *note, const char *text, const char *requested_title, const char *tags_text, bool update_status);
char *read_file(const char *path);
void extract_metadata(Note *note);
void extract_notebook_metadata(Notebook *notebook);
void save_notebook_metadata(Notebook *notebook);
void make_uuid(char output[64]);
void safe_title(const char *input, char output[256]);
bool write_atomic(const char *path, const char *content);
void create_snapshot(Note *note, const char *previous);
void recovery_directory(char output[PATH_MAX]);
void recovery_path(Note *note, char output[PATH_MAX]);
void write_recovery(Note *note, const char *markdown);
void clear_recovery(Note *note);
void restore_recovery(void);
void load_library(void);
void add_library_color_class(Ptr widget, const char *color);
void append_note_button(Note *note, Ptr container);
bool note_has_tag(Note *note, const char *selected);
void remove_note_tag(Ptr button, Ptr user_data);
void rebuild_note_tags(void);
void add_note_tag(Ptr entry, Ptr unused);
void add_note_tag_clicked(Ptr button, Ptr unused);
void free_signal_data(Ptr data, Ptr closure);
void tag_selected(Ptr button, Ptr user_data);
void clear_tag_filter(Ptr button, Ptr unused);
void rebuild_tags(void);
void select_notebook(Ptr button, Ptr user_data);
void create_note_in_notebook(Ptr button, Ptr user_data);
void notebook_expanded_changed(Ptr expander, Ptr unused, Ptr user_data);
void rebuild_sidebar(void);
void close_note_now(Note *closing, bool save_changes);
void close_note_response(Ptr dialog, int response, Ptr user_data);
void close_tab(Ptr button, Ptr user_data);
void active_tab_title_changed(Ptr entry, Ptr user_data);
void rebuild_tabs(void);
void search_changed(Ptr entry, Ptr unused);
void unique_path(char *output, size_t size, const char *parent, const char *base);
void new_notebook(Ptr button, Ptr unused);
void new_note(Ptr button, Ptr unused);
void use_template(Ptr button, Ptr user_data);
void show_templates(Ptr button, Ptr unused);
void choose_folder_response(Ptr dialog, int response, Ptr unused);
void choose_folder(Ptr button, Ptr unused);
Note *add_external_note(const char *path);
void open_markdown_response(Ptr dialog, int response, Ptr unused);
void open_markdown(Ptr button, Ptr unused);
bool copy_file(const char *source, const char *destination);
bool store_image(const char *source);
void clipboard_texture_ready(Ptr clipboard, Ptr result, Ptr user_data);
gboolean paste_image_key(Ptr controller, guint keyval, guint keycode, guint modifiers, Ptr unused);
void image_response(Ptr dialog, int response, Ptr unused);
void insert_image(Ptr button, Ptr unused);
char *html_escape(const char *source);
void builder_append_n(StringBuilder *builder, const char *value, size_t length);
void builder_append(StringBuilder *builder, const char *value);
void builder_printf(StringBuilder *builder, const char *format, ...);
void builder_escape_n(StringBuilder *builder, const char *source, size_t length);
void html_inline(StringBuilder *output, const char *source);
bool markdown_heading(const char *line, int *level, const char **text);
bool markdown_image(const char *line, const char **alt, size_t *alt_length, const char **source, size_t *source_length);
void html_table_row(StringBuilder *output, const char *line, const char *cell_tag);
char *markdown_to_html(const char *markdown);
char *html_document(const char *title, const char *markdown);
void export_html_response(Ptr dialog, int response, Ptr unused);
void export_html(Ptr button, Ptr unused);
void markdown_plain_inline(const char *source, char *output, size_t size);
void pdf_line(Ptr context, double *y, const char *text, double size, int weight, bool monospace, double indent);
bool pdf_export_content(const char *destination, const char *title, const char *markdown);
bool pdf_export(const char *destination);
void docx_run(StringBuilder *output, const char *text, size_t length, const char *properties);
void docx_inline(StringBuilder *output, const char *source);
void docx_paragraph(StringBuilder *output, const char *text, const char *paragraph_properties, const char *run_properties);
void docx_table_row(StringBuilder *output, const char *line, bool header);
char *docx_document_xml(const char *markdown);
bool docx_export_content(const char *destination, const char *markdown);
bool docx_export(const char *destination);
bool office_export(const char *destination, const char *format);
void office_export_response(Ptr dialog, int response, Ptr user_data);
void export_office(Ptr button, Ptr user_data);
int newest_version_first(const void *left, const void *right);
void preview_version(Ptr button, Ptr user_data);
void restore_version(Ptr button, Ptr user_data);
void delete_version(Ptr button, Ptr user_data);
void show_history(Ptr button, Ptr unused);
int utf8_offset(const char *start, const char *position);
void find_next(Ptr button, Ptr unused);
void replace_current(Ptr button, Ptr unused);
void replace_all(Ptr button, Ptr unused);
gboolean find_closed(Ptr window, Ptr unused);
void close_find(Ptr button, Ptr unused);
void show_find(Ptr button, Ptr unused);
const char *block_kind(const char *text);
bool visual_special_line(const char *line);
size_t visual_block_length(const char *cursor);
char *visual_block_markdown(size_t index);
void task_toggled(Ptr check, Ptr user_data);
void heading_level_changed(Ptr dropdown, Ptr unused, Ptr user_data);
void code_language_changed(Ptr entry, Ptr user_data);
void image_field_changed(Ptr entry, Ptr user_data);
gboolean refresh_visual_editor(Ptr unused);
void table_action(Ptr button, Ptr user_data);
void append_table_cell(StringBuilder *builder, const char *value);
void table_cell_changed(Ptr entry, Ptr user_data);
size_t table_column_count(const char *line);
void table_cell_text(const char *line, size_t column, char output[512]);
Ptr table_visual_editor(const char *markdown, size_t block_index);
Ptr visual_image_widget(const char *block);
void blocks_changed(Ptr buffer, Ptr unused);
void block_action(Ptr button, Ptr user_data);
char *quote_visual_text(const char *markdown);
char *code_visual_text(const char *markdown, char language[64]);
void rebuild_block_editor(void);
void add_visual_block(Ptr button, Ptr user_data);
void show_block_editor(Ptr button, Ptr unused);
void select_editor_mode(Ptr button, Ptr user_data);
void remove_note_from_model(Note *target);
void delete_note_response(Ptr dialog, int response, Ptr user_data);
void delete_active_note(Ptr button, Ptr unused);
void choose_theme(Ptr button, Ptr user_data);
void autosave_changed(Ptr check, Ptr unused);
void language_changed(Ptr button, Ptr user_data);
void note_color_changed(Ptr button, Ptr user_data);
void notebook_color_changed(Ptr button, Ptr user_data);
bool move_note_between_notebooks(Note *note, Notebook *destination);
void move_note_to_notebook(Ptr button, Ptr user_data);
Ptr note_drag_prepare(Ptr source, double x, double y, Ptr user_data);
gboolean note_dropped_on_notebook(Ptr target, const void *value, double x, double y, Ptr user_data);
bool reorder_note_before(Note *source, Note *target);
gboolean note_dropped_on_note(Ptr target_widget, const void *value, double x, double y, Ptr user_data);
void show_preferences(Ptr button, Ptr unused);
void rename_notebook_apply(Ptr button, Ptr user_data);
void rename_selected_notebook(Ptr button, Ptr unused);
int note_compare(Note *left, Note *right, bool recent);
void sort_notes(bool recent);
void sort_title(Ptr button, Ptr unused);
void sort_recent(Ptr button, Ptr unused);
void reorder_active_note(int direction);
void move_note_up(Ptr button, Ptr unused);
void move_note_down(Ptr button, Ptr unused);
void delete_notebook_response(Ptr dialog, int response, Ptr user_data);
void delete_selected_notebook(Ptr button, Ptr unused);
Ptr context_action(const char *icon, const char *label, void *callback, Ptr data);
Ptr color_actions(bool notebook);
void library_menu_clicked(Ptr button, Ptr user_data);
Ptr library_menu_button(Ptr target, bool notebook);
void library_context_pressed(Ptr gesture, int presses, double x, double y, Ptr user_data);
Ptr attach_library_context_menu(Ptr widget, Ptr target, bool notebook);
void show_export_menu(Ptr button, Ptr unused);
char *editor_text(void);
void preview_line(Ptr buffer, const char *text, const char *tag);
void preview_tagged_segment(Ptr buffer, const char *text, size_t length, const char *tag);
void preview_inline_line(Ptr buffer, const char *text, const char *line_tag);
void preview_table_line(Ptr buffer, const char *line, bool header);
bool preview_image(Ptr buffer, const char *line);
void render_markdown(Ptr buffer, const char *source);
void update_views(const char *text);
void split_content_changed(Ptr buffer, Ptr unused);
void apply_editor_tag(Ptr buffer, const char *source, const char *start_pointer, const char *end_pointer, const char *tag);
void highlight_editor(const char *source);
Note *note_named(const char *title);
void open_linked_note(Ptr button, Ptr user_data);
void create_linked_note(Ptr button, Ptr user_data);
void rebuild_links(const char *text);
gboolean save_active(Ptr unused);
void apply_active_line_tag(void);
void cursor_mark_set(Ptr buffer, const void *location, Ptr mark, Ptr unused);
void update_statistics(const char *text);
void content_changed(Ptr object, Ptr unused);
void select_note(Ptr button, Ptr user_data);
void insert_markdown(Ptr button, Ptr user_data);
void heading_format_changed(Ptr dropdown, Ptr unused, Ptr user_data);
void text_color_changed(Ptr dropdown, Ptr unused, Ptr user_data);
void insert_literal(Ptr button, Ptr user_data);
Ptr editor_scroller(Ptr view);
Ptr readonly_markdown_view(Ptr *buffer, const char *css_class);
void toggle_sidebar(Ptr button, Ptr unused);
gboolean key_pressed(Ptr controller, guint keyval, guint keycode, guint modifiers, Ptr unused);
gboolean files_dropped(Ptr target, const void *value, double x, double y, Ptr unused);
void external_conflict_response(Ptr dialog, int response, Ptr user_data);
gboolean poll_external_file(Ptr unused);
void quit_response(Ptr dialog, int response, Ptr unused);
gboolean close_requested(Ptr window, Ptr unused);
char *buffer_text(Ptr buffer);
gboolean run_ui_assertions(Ptr unused);
gboolean run_empty_ui_assertions(Ptr unused);
void activate(Ptr app, Ptr unused);
void open_application_files(Ptr app, Ptr *files, int count, const char *hint, Ptr unused);
void resolve_root(void);
void resolve_data_dir(void);
int self_test(void);
int export_self_test(void);
bool prepare_ui_test(void);
void remove_test_tree(const char *path);
void cleanup_ui_test(void);

#endif /* STATE_H */

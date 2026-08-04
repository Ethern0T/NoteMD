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
#include <pthread.h>

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
    bool metadata_loaded;
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
    bool metadata_loaded;
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
    Ptr syntax_heading, syntax_emphasis, syntax_code, syntax_link, syntax_list, syntax_quote;
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

static State state;

static char *buffer_text(Ptr buffer);
static void update_views(const char *text);
static void open_application_files(Ptr app, Ptr *files, int count, const char *hint, Ptr unused);

static const char *tr(const char *pt) {
    struct Translation { const char *pt, *en, *fr; };
    static const struct Translation values[] = {
        {"Novo notebook", "New notebook", "Nouveau carnet"}, {"Nova nota", "New note", "Nouvelle note"},
        {"Templates", "Templates", "Modèles"}, {"Abrir…", "Open…", "Ouvrir…"},
        {"Preferências", "Preferences", "Préférences"}, {"Pesquisar", "Find", "Rechercher"},
        {"Histórico", "History", "Historique"}, {"Remover", "Delete", "Supprimer"},
        {"Guardar", "Save", "Enregistrer"}, {"Pesquisar notas", "Search notes", "Rechercher des notes"},
        {"Título da nota", "Note title", "Titre de la note"},
        {"Tags separadas por vírgulas", "Comma-separated tags", "Tags séparés par des virgules"},
        {"Editor", "Editor", "Éditeur"}, {"Visual", "Visual", "Visuel"}, {"Dividido", "Split", "Partagé"},
        {"Imagem…", "Image…", "Image…"}, {"NOTEMD · BIBLIOTECA", "NOTEMD · LIBRARY", "NOTEMD · BIBLIOTHÈQUE"},
        {"Ficheiros externos", "External files", "Fichiers externes"},
        {"Crie ou abra uma nota", "Create or open a note", "Créez ou ouvrez une note"},
        {"Sistema", "System", "Système"}, {"Tema do editor", "Editor theme", "Thème de l’éditeur"},
        {"Gravação automática", "Automatic saving", "Enregistrement automatique"},
        {"Aparência e armazenamento", "Appearance and storage", "Apparence et stockage"},
        {"Selecionar pasta das notas…", "Choose notes folder…", "Choisir le dossier des notes…"},
        {"Cor da nota", "Note color", "Couleur de la note"}, {"Cor do notebook", "Notebook color", "Couleur du carnet"},
        {"Mover nota ativa para", "Move active note to", "Déplacer la note active vers"},
        {"Tags", "Tags", "Tags"}, {"Sem tags", "No tags", "Aucun tag"},
        {"Selecione uma nota", "Select a note", "Sélectionnez une note"},
        {"Nova nota a partir de template", "New note from template", "Nouvelle note depuis un modèle"},
        {"Escolha um template", "Choose a template", "Choisissez un modèle"},
        {"Nota vazia", "Blank note", "Note vide"}, {"Reunião", "Meeting", "Réunion"},
        {"Diário", "Daily note", "Note quotidienne"}, {"Projeto", "Project", "Projet"},
        {"Checklist", "Checklist", "Checklist"}, {"Histórico de versões", "Version history", "Historique des versions"},
        {"Ainda não existem versões anteriores.", "There are no previous versions yet.", "Il n’existe pas encore de version précédente."},
        {"Restaurar esta versão", "Restore this version", "Restaurer cette version"},
        {"Apagar esta versão", "Delete this version", "Supprimer cette version"},
        {"Pesquisar e substituir", "Find and replace", "Rechercher et remplacer"},
        {"Substituir por", "Replace with", "Remplacer par"},
        {"Correspondência seguinte", "Next match", "Correspondance suivante"},
        {"Substituir correspondência atual", "Replace current match", "Remplacer la correspondance actuelle"},
        {"Substituir todas as correspondências", "Replace all matches", "Remplacer toutes les correspondances"},
        {"Fechar pesquisa", "Close search", "Fermer la recherche"},
        {"Parágrafo", "Paragraph", "Paragraphe"}, {"Título", "Heading", "Titre"},
        {"Lista", "Bullet list", "Liste à puces"}, {"Tarefa", "Task", "Tâche"},
        {"Citação", "Quote", "Citation"}, {"Código", "Code", "Code"}, {"Tabela", "Table", "Tableau"},
        {"Separador", "Divider", "Séparateur"}, {"Duplicar bloco", "Duplicate block", "Dupliquer le bloc"},
        {"Remover bloco", "Delete block", "Supprimer le bloc"},
        {"Adicionar linha", "Add row", "Ajouter une ligne"}, {"Adicionar coluna", "Add column", "Ajouter une colonne"},
        {"Marcar tarefa como concluída", "Mark task as completed", "Marquer la tâche comme terminée"},
        {"Negrito", "Bold", "Gras"}, {"Itálico", "Italic", "Italique"}, {"Ligação", "Link", "Lien"},
        {"Título 1", "Heading 1", "Titre 1"}, {"Título 2", "Heading 2", "Titre 2"},
        {"Título 3", "Heading 3", "Titre 3"}, {"Título 4", "Heading 4", "Titre 4"},
        {"Título 5", "Heading 5", "Titre 5"}, {"Título 6", "Heading 6", "Titre 6"},
        {"Rasurado", "Strikethrough", "Barré"}, {"Lista numerada", "Numbered list", "Liste numérotée"},
        {"Cor do texto", "Text color", "Couleur du texte"}, {"Sem cor", "No color", "Sans couleur"},
        {"Vermelho", "Red", "Rouge"}, {"Laranja", "Orange", "Orange"}, {"Amarelo", "Yellow", "Jaune"},
        {"Verde", "Green", "Vert"}, {"Azul", "Blue", "Bleu"}, {"Roxo", "Purple", "Violet"},
        {"Abrir ficheiro Markdown", "Open Markdown file", "Ouvrir un fichier Markdown"},
        {"Inserir imagem", "Insert image", "Insérer une image"},
        {"Exportar HTML", "Export HTML", "Exporter en HTML"}, {"Exportar PDF", "Export PDF", "Exporter en PDF"},
        {"Exportar DOCX", "Export DOCX", "Exporter en DOCX"}, {"Exportar", "Export", "Exporter"},
        {"Cancelar", "Cancel", "Annuler"}, {"Abrir", "Open", "Ouvrir"}, {"Inserir", "Insert", "Insérer"},
        {"Renomear notebook", "Rename notebook", "Renommer le carnet"}, {"Renomear", "Rename", "Renommer"},
        {"Ligações", "Links", "Liens"}, {"Pronto", "Ready", "Prêt"},
        {"Guardado", "Saved", "Enregistré"}, {"Erro ao guardar", "Could not save", "Impossible d’enregistrer"},
        {"Imagem copiada para assets", "Image copied to assets", "Image copiée dans assets"},
        {"Imagem colada e guardada em assets", "Image pasted and saved to assets", "Image collée et enregistrée dans assets"},
        {"Tema aplicado", "Theme applied", "Thème appliqué"},
        {"Gravação automática ativada", "Automatic saving enabled", "Enregistrement automatique activé"},
        {"Gravação automática desativada", "Automatic saving disabled", "Enregistrement automatique désactivé"},
        {"Sem mais correspondências", "No more matches", "Aucune autre correspondance"},
        {"Correspondência encontrada", "Match found", "Correspondance trouvée"},
        {"Texto não encontrado", "Text not found", "Texte introuvable"},
        {"Correspondência substituída", "Match replaced", "Correspondance remplacée"},
        {"Ocultar biblioteca", "Hide library", "Masquer la bibliothèque"},
        {"Mostrar biblioteca", "Show library", "Afficher la bibliothèque"},
        {"palavras", "words", "mots"}, {"carateres", "characters", "caractères"}, {"linhas", "lines", "lignes"},
        {"a guardar…", "saving…", "enregistrement…"},
        {"alterações por guardar", "unsaved changes", "modifications non enregistrées"},
        {"Alterações por guardar", "Unsaved changes", "Modifications non enregistrées"},
        {"Guardar alterações?", "Save changes?", "Enregistrer les modifications ?"},
        {"A nota foi alterada desde a última gravação.", "The note has changed since it was last saved.", "La note a été modifiée depuis son dernier enregistrement."},
        {"Não guardar", "Don't save", "Ne pas enregistrer"},
        {"Versão guardada", "Saved version", "Version enregistrée"},
        {"Versão atual", "Current version", "Version actuelle"},
        {"Selecione uma versão para a pré-visualizar. Restaurar não grava automaticamente.",
         "Select a version to preview it. Restoring does not save automatically.",
         "Sélectionnez une version pour la prévisualiser. La restauration n’enregistre pas automatiquement."},
        {"Crie uma versão editando e guardando a nota.", "Create a version by editing and saving the note.",
         "Créez une version en modifiant puis en enregistrant la note."},
        {"Criar esta nota", "Create this note", "Créer cette note"},
        {"Linguagem", "Language", "Langage"},
        {"Descrição da imagem", "Image description", "Description de l’image"},
        {"Localização da imagem", "Image location", "Emplacement de l’image"}
    };
    int column = strcmp(state.language, "en") == 0 ? 1 : strcmp(state.language, "fr") == 0 ? 2 : 0;
    for (size_t index = 0; index < sizeof values / sizeof values[0]; index++) if (strcmp(values[index].pt, pt) == 0)
        return column == 1 ? values[index].en : column == 2 ? values[index].fr : values[index].pt;
    return pt;
}

#define FN(ret, name, args) static ret (*name) args
FN(Ptr, gtk_application_new, (const char *, int));
FN(int, g_application_run, (Ptr, int, char **));
FN(void, g_application_quit, (Ptr));
FN(void, g_object_unref, (Ptr));
FN(Ptr, g_object_ref, (Ptr));
FN(void, g_object_set, (Ptr, const char *, ...));
FN(void, g_object_get, (Ptr, const char *, ...));
FN(Ptr, gtk_settings_get_default, (void));
FN(unsigned long, g_signal_connect_data, (Ptr, const char *, void *, Ptr, void *, int));
FN(void, g_signal_emit_by_name, (Ptr, const char *, ...));
FN(Ptr, gtk_application_window_new, (Ptr));
FN(void, gtk_window_set_title, (Ptr, const char *));
FN(void, gtk_window_set_default_size, (Ptr, int, int));
FN(void, gtk_window_set_child, (Ptr, Ptr));
FN(void, gtk_window_present, (Ptr));
FN(Ptr, gtk_window_new, (void));
FN(void, gtk_window_set_transient_for, (Ptr, Ptr));
FN(void, gtk_window_set_modal, (Ptr, gboolean));
FN(void, gtk_window_destroy, (Ptr));
FN(Ptr, gtk_message_dialog_new, (Ptr, int, int, int, const char *, ...));
FN(void, gtk_message_dialog_format_secondary_text, (Ptr, const char *, ...));
FN(Ptr, gtk_dialog_add_button, (Ptr, const char *, int));
FN(Ptr, gtk_box_new, (int, int));
FN(void, gtk_box_append, (Ptr, Ptr));
FN(void, gtk_box_remove, (Ptr, Ptr));
FN(Ptr, gtk_grid_new, (void));
FN(void, gtk_grid_attach, (Ptr, Ptr, int, int, int, int));
FN(void, gtk_grid_set_row_spacing, (Ptr, unsigned));
FN(void, gtk_grid_set_column_spacing, (Ptr, unsigned));
FN(Ptr, gtk_expander_new, (const char *));
FN(void, gtk_expander_set_child, (Ptr, Ptr));
FN(void, gtk_expander_set_label_widget, (Ptr, Ptr));
FN(void, gtk_expander_set_expanded, (Ptr, gboolean));
FN(gboolean, gtk_expander_get_expanded, (Ptr));
FN(Ptr, gtk_widget_get_first_child, (Ptr));
FN(Ptr, gtk_widget_get_next_sibling, (Ptr));
FN(Ptr, gtk_paned_new, (int));
FN(void, gtk_paned_set_start_child, (Ptr, Ptr));
FN(void, gtk_paned_set_end_child, (Ptr, Ptr));
FN(void, gtk_paned_set_position, (Ptr, int));
FN(Ptr, gtk_scrolled_window_new, (void));
FN(void, gtk_scrolled_window_set_child, (Ptr, Ptr));
FN(void, gtk_scrolled_window_set_policy, (Ptr, int, int));
FN(Ptr, gtk_scrolled_window_get_vadjustment, (Ptr));
FN(void, gtk_scrolled_window_set_vadjustment, (Ptr, Ptr));
FN(void, gtk_widget_set_hexpand, (Ptr, gboolean));
FN(void, gtk_widget_set_vexpand, (Ptr, gboolean));
FN(void, gtk_widget_set_size_request, (Ptr, int, int));
FN(void, gtk_widget_set_visible, (Ptr, gboolean));
FN(gboolean, gtk_widget_get_visible, (Ptr));
FN(void, gtk_widget_add_css_class, (Ptr, const char *));
FN(void, gtk_widget_remove_css_class, (Ptr, const char *));
FN(gboolean, gtk_widget_has_css_class, (Ptr, const char *));
FN(void, gtk_widget_set_tooltip_text, (Ptr, const char *));
FN(int, gtk_widget_get_width, (Ptr));
FN(void, gtk_widget_add_controller, (Ptr, Ptr));
FN(Ptr, gtk_event_controller_key_new, (void));
FN(void, gtk_event_controller_set_propagation_phase, (Ptr, int));
FN(Ptr, gtk_gesture_click_new, (void));
FN(void, gtk_gesture_single_set_button, (Ptr, unsigned));
FN(void, gtk_widget_set_parent, (Ptr, Ptr));
FN(void, gtk_widget_unparent, (Ptr));
FN(Ptr, gtk_popover_new, (void));
FN(void, gtk_popover_set_child, (Ptr, Ptr));
FN(void, gtk_popover_set_pointing_to, (Ptr, const void *));
FN(void, gtk_popover_popup, (Ptr));
FN(void, gtk_popover_popdown, (Ptr));
FN(Ptr, gtk_drop_target_new, (GType, int));
FN(Ptr, gtk_drag_source_new, (void));
FN(void, gtk_drag_source_set_actions, (Ptr, int));
FN(GType, gdk_file_list_get_type, (void));
FN(Ptr, g_value_get_boxed, (const void *));
FN(const char *, g_value_get_string, (const void *));
FN(GType, g_type_from_name, (const char *));
FN(Ptr, gdk_content_provider_new_typed, (GType, ...));
FN(Ptr, gtk_label_new, (const char *));
FN(void, gtk_label_set_text, (Ptr, const char *));
FN(void, gtk_label_set_xalign, (Ptr, float));
FN(Ptr, gtk_entry_new, (void));
FN(Ptr, gtk_search_entry_new, (void));
FN(void, gtk_search_entry_set_placeholder_text, (Ptr, const char *));
FN(void, gtk_entry_set_placeholder_text, (Ptr, const char *));
FN(const char *, gtk_editable_get_text, (Ptr));
FN(void, gtk_editable_set_text, (Ptr, const char *));
FN(Ptr, gtk_button_new_with_label, (const char *));
FN(Ptr, gtk_button_new_from_icon_name, (const char *));
FN(void, gtk_button_set_child, (Ptr, Ptr));
FN(Ptr, gtk_check_button_new_with_label, (const char *));
FN(void, gtk_check_button_set_active, (Ptr, gboolean));
FN(gboolean, gtk_check_button_get_active, (Ptr));
FN(void, gtk_button_set_label, (Ptr, const char *));
FN(void, gtk_button_set_icon_name, (Ptr, const char *));
FN(Ptr, gtk_text_view_new, (void));
FN(void, gtk_text_view_set_monospace, (Ptr, gboolean));
FN(void, gtk_text_view_set_wrap_mode, (Ptr, int));
FN(void, gtk_text_view_set_left_margin, (Ptr, int));
FN(void, gtk_text_view_set_right_margin, (Ptr, int));
FN(void, gtk_text_view_set_top_margin, (Ptr, int));
FN(Ptr, gtk_text_view_get_buffer, (Ptr));
FN(void, gtk_text_buffer_set_text, (Ptr, const char *, int));
FN(void, gtk_text_buffer_get_bounds, (Ptr, void *, void *));
FN(char *, gtk_text_buffer_get_text, (Ptr, const void *, const void *, gboolean));
FN(void, gtk_text_buffer_insert_at_cursor, (Ptr, const char *, int));
FN(void, gtk_text_buffer_get_iter_at_offset, (Ptr, void *, int));
FN(void, gtk_text_buffer_select_range, (Ptr, const void *, const void *));
FN(gboolean, gtk_text_buffer_get_selection_bounds, (Ptr, void *, void *));
FN(void, gtk_text_buffer_delete, (Ptr, void *, void *));
FN(void, gtk_text_buffer_get_end_iter, (Ptr, void *));
FN(void, gtk_text_buffer_insert, (Ptr, void *, const char *, int));
FN(int, gtk_text_buffer_get_char_count, (Ptr));
FN(Ptr, gtk_text_buffer_get_insert, (Ptr));
FN(void, gtk_text_buffer_get_iter_at_mark, (Ptr, void *, Ptr));
FN(int, gtk_text_iter_get_line, (const void *));
FN(Ptr, gtk_text_buffer_create_tag, (Ptr, const char *, const char *, ...));
FN(void, gtk_text_buffer_apply_tag_by_name, (Ptr, const char *, const void *, const void *));
FN(void, gtk_text_buffer_remove_all_tags, (Ptr, const void *, const void *));
FN(void, gtk_text_buffer_set_enable_undo, (Ptr, gboolean));
FN(Ptr, gtk_text_buffer_create_child_anchor, (Ptr, void *));
FN(void, gtk_text_view_add_child_at_anchor, (Ptr, Ptr, Ptr));
FN(Ptr, gtk_picture_new_for_filename, (const char *));
FN(void, gtk_picture_set_content_fit, (Ptr, int));
FN(void, gtk_text_view_scroll_to_iter, (Ptr, void *, double, gboolean, double, double));
FN(void, gtk_text_view_set_editable, (Ptr, gboolean));
FN(void, gtk_text_view_set_cursor_visible, (Ptr, gboolean));
FN(Ptr, gtk_stack_new, (void));
FN(Ptr, gtk_stack_add_titled, (Ptr, Ptr, const char *, const char *));
FN(void, gtk_stack_set_visible_child_name, (Ptr, const char *));
FN(const char *, gtk_stack_get_visible_child_name, (Ptr));
FN(Ptr, gtk_stack_switcher_new, (void));
FN(void, gtk_stack_switcher_set_stack, (Ptr, Ptr));
FN(Ptr, gtk_drop_down_new_from_strings, (const char *const *));
FN(void, gtk_drop_down_set_selected, (Ptr, guint));
FN(guint, gtk_drop_down_get_selected, (Ptr));
FN(void, g_free, (Ptr));
FN(void, g_error_free, (Ptr));
FN(guint, g_timeout_add, (guint, void *, Ptr));
FN(guint, g_idle_add, (void *, Ptr));
FN(gboolean, g_source_remove, (guint));
FN(Ptr, gtk_image_new_from_file, (const char *));
FN(Ptr, gtk_image_new_from_icon_name, (const char *));
FN(void, gtk_image_set_pixel_size, (Ptr, int));
FN(void, gtk_widget_set_margin_start, (Ptr, int));
FN(void, gtk_widget_set_margin_end, (Ptr, int));
FN(void, gtk_widget_set_margin_top, (Ptr, int));
FN(void, gtk_widget_set_margin_bottom, (Ptr, int));
FN(Ptr, gtk_separator_new, (int));
FN(Ptr, gtk_css_provider_new, (void));
FN(void, gtk_css_provider_load_from_path, (Ptr, const char *));
FN(void, gtk_css_provider_load_from_string, (Ptr, const char *));
FN(Ptr, gdk_display_get_default, (void));
FN(Ptr, gdk_display_get_clipboard, (Ptr));
FN(Ptr, gdk_clipboard_get_formats, (Ptr));
FN(gboolean, gdk_content_formats_contain_gtype, (Ptr, GType));
FN(GType, gdk_texture_get_type, (void));
FN(void, gdk_clipboard_read_texture_async, (Ptr, Ptr, void *, Ptr));
FN(Ptr, gdk_clipboard_read_texture_finish, (Ptr, Ptr, Ptr *));
FN(gboolean, gdk_texture_save_to_png, (Ptr, const char *));
FN(void, gtk_style_context_add_provider_for_display, (Ptr, Ptr, guint));
FN(void, gtk_style_context_remove_provider_for_display, (Ptr, Ptr));
FN(Ptr, gtk_file_chooser_native_new, (const char *, Ptr, int, const char *, const char *));
FN(Ptr, gtk_file_chooser_get_file, (Ptr));
FN(char *, g_file_get_path, (Ptr));
FN(void, gtk_native_dialog_show, (Ptr));
FN(void, gtk_file_chooser_set_current_name, (Ptr, const char *));
FN(Ptr, g_file_new_for_path, (const char *));
FN(gboolean, g_file_trash, (Ptr, Ptr, Ptr));
FN(Ptr, cairo_pdf_surface_create, (const char *, double, double));
FN(Ptr, cairo_create, (Ptr));
FN(void, cairo_select_font_face, (Ptr, const char *, int, int));
FN(void, cairo_set_font_size, (Ptr, double));
FN(void, cairo_move_to, (Ptr, double, double));
FN(void, cairo_show_text, (Ptr, const char *));
FN(void, cairo_show_page, (Ptr));
FN(void, cairo_destroy, (Ptr));
FN(void, cairo_surface_destroy, (Ptr));
FN(void, cairo_surface_finish, (Ptr));
FN(int, cairo_surface_status, (Ptr));

typedef struct { char opaque[128]; } TextIter;
typedef struct { Ptr widget, target; bool notebook; } ContextTarget;
typedef struct { int x, y, width, height; } IntRect;

static Ptr library_menu_button(Ptr target, bool notebook);
static Ptr attach_library_context_menu(Ptr widget, Ptr target, bool notebook);
static void new_notebook(Ptr button, Ptr unused);
static void new_note(Ptr button, Ptr unused);

static gboolean save_active(Ptr unused);
static void select_note(Ptr button, Ptr user_data);
static char *editor_text(void);
static char *buffer_text(Ptr buffer);
static void safe_title(const char *input, char output[256]);
static void create_snapshot(Note *note, const char *previous);
static void clear_recovery(Note *note);
static void update_views(const char *text);
static void rebuild_links(const char *text);
static void rebuild_tabs(void);
static void rebuild_block_editor(void);
static void blocks_changed(Ptr buffer, Ptr unused);
static Ptr readonly_markdown_view(Ptr *buffer, const char *css_class);
static Ptr editor_scroller(Ptr view);
static void render_markdown(Ptr buffer, const char *source);
static void rebuild_tags(void);
static void rebuild_note_tags(void);
static void free_signal_data(Ptr data, Ptr closure);
static void rebuild_sidebar(void);
static void content_changed(Ptr object, Ptr unused);
static void apply_active_line_tag(void);
static bool make_directory(const char *path);
static char *read_file(const char *path);
static void ensure_metadata_loaded(Note *note);
static bool write_atomic(const char *path, const char *content);
static void rename_selected_notebook(Ptr button, Ptr unused);
static void delete_selected_notebook(Ptr button, Ptr unused);
static void sort_title(Ptr button, Ptr unused);
static void sort_recent(Ptr button, Ptr unused);
static void move_note_up(Ptr button, Ptr unused);
static void move_note_down(Ptr button, Ptr unused);
static Ptr note_drag_prepare(Ptr source, double x, double y, Ptr user_data);
static gboolean note_dropped_on_notebook(Ptr target, const void *value, double x, double y, Ptr user_data);
static gboolean note_dropped_on_note(Ptr target, const void *value, double x, double y, Ptr user_data);
static void unique_path(char *output, size_t size, const char *parent, const char *base);

static void die(const char *message) {
    fprintf(stderr, "NoteMD: %s\n", message);
    exit(EXIT_FAILURE);
}

static void load_symbol(void **target, const char *name) {
    *target = dlsym(RTLD_DEFAULT, name);
    if (!*target) die(name);
}

#define LOAD(name) load_symbol((void **)&name, #name)
static void load_gtk(void) {
    state.gtk = dlopen("libgtk-4.so.1", RTLD_NOW | RTLD_GLOBAL);
    if (!state.gtk) die("GTK 4 não está instalada (libgtk-4.so.1)");
    LOAD(gtk_application_new); LOAD(g_application_run); LOAD(g_application_quit); LOAD(g_object_unref); LOAD(g_object_ref); LOAD(g_object_set);
    LOAD(g_object_get); LOAD(gtk_settings_get_default);
    LOAD(g_signal_connect_data); LOAD(g_signal_emit_by_name); LOAD(gtk_application_window_new); LOAD(gtk_window_new);
    LOAD(gtk_window_set_transient_for); LOAD(gtk_window_set_modal); LOAD(gtk_window_set_title);
    LOAD(gtk_window_destroy);
    LOAD(gtk_message_dialog_new); LOAD(gtk_message_dialog_format_secondary_text);
    LOAD(gtk_dialog_add_button);
    LOAD(gtk_window_set_default_size); LOAD(gtk_window_set_child); LOAD(gtk_window_present);
    LOAD(gtk_box_new); LOAD(gtk_box_append); LOAD(gtk_box_remove); LOAD(gtk_widget_get_first_child); LOAD(gtk_widget_get_next_sibling);
    LOAD(gtk_grid_new); LOAD(gtk_grid_attach); LOAD(gtk_grid_set_row_spacing); LOAD(gtk_grid_set_column_spacing);
    LOAD(gtk_expander_new); LOAD(gtk_expander_set_child); LOAD(gtk_expander_set_label_widget);
    LOAD(gtk_expander_set_expanded); LOAD(gtk_expander_get_expanded);
    LOAD(gtk_paned_new); LOAD(gtk_paned_set_start_child);
    LOAD(gtk_paned_set_end_child); LOAD(gtk_paned_set_position); LOAD(gtk_scrolled_window_new);
    LOAD(gtk_scrolled_window_set_child); LOAD(gtk_scrolled_window_set_policy);
    LOAD(gtk_scrolled_window_get_vadjustment); LOAD(gtk_scrolled_window_set_vadjustment);
    LOAD(gtk_widget_set_hexpand); LOAD(gtk_widget_set_vexpand);
    LOAD(gtk_widget_set_size_request); LOAD(gtk_widget_set_visible); LOAD(gtk_widget_get_visible);
    LOAD(gtk_widget_add_css_class); LOAD(gtk_widget_remove_css_class); LOAD(gtk_widget_has_css_class);
    LOAD(gtk_widget_set_tooltip_text);
    LOAD(gtk_widget_get_width); LOAD(gtk_label_new);
    LOAD(gtk_widget_add_controller); LOAD(gtk_event_controller_key_new); LOAD(gtk_event_controller_set_propagation_phase);
    LOAD(gtk_gesture_click_new); LOAD(gtk_gesture_single_set_button);
    LOAD(gtk_widget_set_parent); LOAD(gtk_widget_unparent);
    LOAD(gtk_popover_new); LOAD(gtk_popover_set_child); LOAD(gtk_popover_set_pointing_to); LOAD(gtk_popover_popup); LOAD(gtk_popover_popdown);
    LOAD(gtk_drop_target_new); LOAD(gtk_drag_source_new); LOAD(gtk_drag_source_set_actions);
    LOAD(gdk_file_list_get_type); LOAD(g_value_get_boxed); LOAD(g_value_get_string); LOAD(g_type_from_name);
    LOAD(gdk_content_provider_new_typed);
    LOAD(gtk_label_set_text); LOAD(gtk_label_set_xalign); LOAD(gtk_entry_new); LOAD(gtk_search_entry_new);
    LOAD(gtk_search_entry_set_placeholder_text);
    LOAD(gtk_entry_set_placeholder_text); LOAD(gtk_editable_get_text); LOAD(gtk_editable_set_text);
    LOAD(gtk_button_new_with_label); LOAD(gtk_button_new_from_icon_name); LOAD(gtk_button_set_child);
    LOAD(gtk_button_set_label); LOAD(gtk_button_set_icon_name); LOAD(gtk_check_button_new_with_label);
    LOAD(gtk_check_button_set_active); LOAD(gtk_check_button_get_active); LOAD(gtk_text_view_new);
    LOAD(gtk_text_view_set_monospace); LOAD(gtk_text_view_set_wrap_mode);
    LOAD(gtk_text_view_set_left_margin); LOAD(gtk_text_view_set_right_margin);
    LOAD(gtk_text_view_set_top_margin); LOAD(gtk_text_view_get_buffer); LOAD(gtk_text_buffer_set_text);
    LOAD(gtk_text_buffer_get_bounds); LOAD(gtk_text_buffer_get_text); LOAD(gtk_text_buffer_insert_at_cursor);
    LOAD(gtk_text_buffer_get_iter_at_offset); LOAD(gtk_text_buffer_select_range); LOAD(gtk_text_view_scroll_to_iter);
    LOAD(gtk_text_buffer_get_selection_bounds); LOAD(gtk_text_buffer_delete);
    LOAD(gtk_text_buffer_get_end_iter); LOAD(gtk_text_buffer_insert); LOAD(gtk_text_buffer_get_char_count);
    LOAD(gtk_text_buffer_get_insert); LOAD(gtk_text_buffer_get_iter_at_mark); LOAD(gtk_text_iter_get_line);
    LOAD(gtk_text_buffer_create_tag);
    LOAD(gtk_text_buffer_apply_tag_by_name);
    LOAD(gtk_text_buffer_remove_all_tags);
    LOAD(gtk_text_buffer_set_enable_undo);
    LOAD(gtk_text_buffer_create_child_anchor); LOAD(gtk_text_view_add_child_at_anchor);
    LOAD(gtk_picture_new_for_filename); LOAD(gtk_picture_set_content_fit);
    LOAD(gtk_text_view_set_editable); LOAD(gtk_text_view_set_cursor_visible); LOAD(gtk_stack_new);
    LOAD(gtk_stack_add_titled); LOAD(gtk_stack_set_visible_child_name); LOAD(gtk_stack_get_visible_child_name); LOAD(gtk_stack_switcher_new);
    LOAD(gtk_stack_switcher_set_stack); LOAD(gtk_drop_down_new_from_strings); LOAD(gtk_drop_down_set_selected);
    LOAD(gtk_drop_down_get_selected); LOAD(g_free); LOAD(g_error_free);
    LOAD(g_timeout_add); LOAD(g_idle_add); LOAD(g_source_remove); LOAD(gtk_file_chooser_native_new);
    LOAD(gtk_file_chooser_get_file); LOAD(g_file_get_path); LOAD(gtk_native_dialog_show);
    LOAD(gtk_file_chooser_set_current_name);
    LOAD(g_file_new_for_path); LOAD(g_file_trash);
    LOAD(cairo_pdf_surface_create); LOAD(cairo_create); LOAD(cairo_select_font_face);
    LOAD(cairo_set_font_size); LOAD(cairo_move_to); LOAD(cairo_show_text); LOAD(cairo_show_page);
    LOAD(cairo_destroy); LOAD(cairo_surface_finish); LOAD(cairo_surface_destroy); LOAD(cairo_surface_status);
    LOAD(gtk_image_new_from_file); LOAD(gtk_image_new_from_icon_name); LOAD(gtk_image_set_pixel_size);
    LOAD(gtk_widget_set_margin_start);
    LOAD(gtk_widget_set_margin_end); LOAD(gtk_widget_set_margin_top); LOAD(gtk_widget_set_margin_bottom);
    LOAD(gtk_separator_new); LOAD(gtk_css_provider_new); LOAD(gtk_css_provider_load_from_path); LOAD(gtk_css_provider_load_from_string);
    LOAD(gdk_display_get_default); LOAD(gdk_display_get_clipboard); LOAD(gdk_clipboard_get_formats);
    LOAD(gdk_content_formats_contain_gtype); LOAD(gdk_texture_get_type); LOAD(gdk_clipboard_read_texture_async);
    LOAD(gdk_clipboard_read_texture_finish); LOAD(gdk_texture_save_to_png); LOAD(gtk_style_context_add_provider_for_display);
    LOAD(gtk_style_context_remove_provider_for_display);
}

static void margins(Ptr widget, int horizontal, int vertical) {
    gtk_widget_set_margin_start(widget, horizontal); gtk_widget_set_margin_end(widget, horizontal);
    gtk_widget_set_margin_top(widget, vertical); gtk_widget_set_margin_bottom(widget, vertical);
}

static Ptr icon_button(const char *icon, const char *tooltip) {
    Ptr button = gtk_button_new_from_icon_name(icon); gtk_widget_set_tooltip_text(button, tooltip);
    gtk_widget_add_css_class(button, "icon-button"); return button;
}

static Ptr navigation_button(const char *icon, const char *text) {
    Ptr button = gtk_button_new_with_label(""); Ptr row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    Ptr image = gtk_image_new_from_icon_name(icon); Ptr label = gtk_label_new(text); gtk_label_set_xalign(label, 0.0f);
    gtk_widget_set_hexpand(label, true); gtk_box_append(row, image); gtk_box_append(row, label); gtk_button_set_child(button, row);
    return button;
}

static Ptr text_button(const char *text, const char *css_class) {
    Ptr button = gtk_button_new_with_label("");
    Ptr label = gtk_label_new(text);
    gtk_label_set_xalign(label, 0.0f);
    gtk_widget_set_hexpand(label, true);
    gtk_button_set_child(button, label);
    if (css_class) gtk_widget_add_css_class(button, css_class);
    return button;
}

static gboolean release_popover_reference(Ptr popover) {
    g_object_unref(popover); return false;
}

static void active_popover_closed(Ptr popover, Ptr unused) {
    (void)unused;
    if (state.active_popover == popover) state.active_popover = NULL;
    g_object_ref(popover); gtk_widget_unparent(popover);
    g_timeout_add(1, (void *)release_popover_reference, popover);
}

static void dismiss_active_popover(void) {
    Ptr popover = state.active_popover; if (!popover) return;
    gtk_popover_popdown(popover);
    if (state.active_popover == popover) {
        state.active_popover = NULL; gtk_widget_unparent(popover);
    }
}

static void present_popover(Ptr popover, Ptr anchor) {
    dismiss_active_popover(); state.active_popover = popover;
    g_signal_connect_data(popover, "closed", (void *)active_popover_closed, NULL, NULL, 0);
    gtk_widget_set_parent(popover, anchor); gtk_popover_popup(popover);
}

static void apply_theme(const char *theme);
static void load_style(void) {
    apply_theme(state.theme);
}

static void config_path(char output[PATH_MAX]) {
    const char *config = getenv("XDG_CONFIG_HOME"), *home = getenv("HOME");
    if (config && *config) snprintf(output, PATH_MAX, "%s/notemd/settings.conf", config);
    else snprintf(output, PATH_MAX, "%s/.config/notemd/settings.conf", home ? home : ".");
}

static void save_settings(void) {
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

static void load_settings(void) {
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

static void apply_syntax_theme(void) {
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
    if (state.syntax_link) g_object_set(state.syntax_link, "foreground", link, NULL);
    if (state.syntax_list) g_object_set(state.syntax_list, "foreground", list, NULL);
    if (state.syntax_quote) g_object_set(state.syntax_quote, "foreground", quote, NULL);
}

static void apply_theme(const char *theme) {
    if (!theme || !*theme) theme = "system";
    if (theme != state.theme) snprintf(state.theme, sizeof state.theme, "%s", theme);
    
    char theme_path[PATH_MAX], style_path[PATH_MAX];
    snprintf(theme_path, sizeof theme_path, "%s/themes/%s.css", state.data_dir, theme);
    snprintf(style_path, sizeof style_path, "%s/style.css", state.data_dir);
    
    char *theme_css = read_file(theme_path);
    char *style_css = read_file(style_path);
    
    size_t theme_len = theme_css ? strlen(theme_css) : 0;
    size_t style_len = style_css ? strlen(style_css) : 0;
    size_t combined_len = theme_len + style_len + 128;
    char *combined = malloc(combined_len);
    snprintf(combined, combined_len, "%s\n%s", theme_css ? theme_css : "", style_css ? style_css : "");
    
    Ptr display = gdk_display_get_default();
    if (display) {
        if (state.theme_provider) {
            gtk_style_context_remove_provider_for_display(display, state.theme_provider);
            g_object_unref(state.theme_provider);
            state.theme_provider = NULL;
        }
        state.theme_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_string(state.theme_provider, combined);
        gtk_style_context_add_provider_for_display(display, state.theme_provider, 800);
    }
    
    if (theme_css) free(theme_css);
    if (style_css) free(style_css);
    free(combined);
    
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
    save_settings();
}

static bool is_directory(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

static bool make_directory(const char *path) {
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

static void free_notes(void) {
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

static void discard_note_draft(Note *note) {
    if (!note) return;
    free(note->draft); note->draft = NULL;
    note->draft_title[0] = '\0'; note->draft_tags[0] = '\0';
}

static void capture_active_draft(void) {
    if (!state.active || !state.active->dirty || !state.editor_buffer) return;
    char *text = editor_text(); free(state.active->draft); state.active->draft = strdup(text); g_free(text);
    snprintf(state.active->draft_title, sizeof state.active->draft_title, "%s", gtk_editable_get_text(state.title));
    snprintf(state.active->draft_tags, sizeof state.active->draft_tags, "%s", gtk_editable_get_text(state.tags));
}

static const char *note_display_title(const Note *note) {
    return note && note->dirty && note->draft ? note->draft_title : note ? note->title : "";
}

static const char *note_display_tags(const Note *note) {
    ensure_metadata_loaded((Note *)note);
    return note && note->dirty && note->draft ? note->draft_tags : note ? note->tags : "";
}

static char *note_current_markdown(const Note *note) {
    if (!note) return strdup("");
    return note->dirty && note->draft ? strdup(note->draft) : read_file(note->markdown_path);
}

static bool save_note_payload(Note *note, const char *text, const char *requested_title, const char *tags_text, bool update_status) {
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

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return strdup("");
    fseek(file, 0, SEEK_END); long size = ftell(file); rewind(file);
    char *data = calloc((size_t)size + 1, 1);
    if (data) fread(data, 1, (size_t)size, file);
    fclose(file);
    return data;
}

static void extract_metadata(Note *note) {
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

static void ensure_metadata_loaded(Note *note) {
    if (note && !note->metadata_loaded && !note->external) {
        extract_metadata(note);
        note->metadata_loaded = true;
    }
}

static void resort_library_notes(void) {
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

static gboolean ui_metadata_updated(Ptr unused) {
    (void)unused;
    resort_library_notes();
    if (state.sidebar) rebuild_sidebar();
    if (state.tag_box) rebuild_tags();
    return 0;
}

static void extract_notebook_metadata(Notebook *notebook);

static void *background_metadata_indexer(void *arg) {
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

static void start_background_metadata_indexer(void) {
    if (indexer_running) return;
    indexer_running = true;
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, background_metadata_indexer, NULL) == 0) {
        pthread_detach(thread_id);
    } else {
        indexer_running = false;
    }
}

static void extract_notebook_metadata(Notebook *notebook) {
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

static void save_notebook_metadata(Notebook *notebook) {
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

static void make_uuid(char output[64]) {
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

static void safe_title(const char *input, char output[256]) {
    size_t used = 0;
    while (*input && used < 255) {
        char value = *input++; output[used++] = (value == '/' || value == ':') ? '-' : value;
    }
    while (used && output[used - 1] == ' ') used--;
    output[used] = '\0'; if (!used) snprintf(output, 256, "Sem título");
}

static bool write_atomic(const char *path, const char *content) {
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

static void create_snapshot(Note *note, const char *previous) {
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

static void recovery_directory(char output[PATH_MAX]) {
    const char *data = getenv("XDG_DATA_HOME"), *home = getenv("HOME");
    if (data && *data) snprintf(output, PATH_MAX, "%s/notemd/recovery", data);
    else snprintf(output, PATH_MAX, "%s/.local/share/notemd/recovery", home ? home : ".");
}

static void recovery_path(Note *note, char output[PATH_MAX]) {
    char directory[PATH_MAX]; recovery_directory(directory); make_directory(directory);
    snprintf(output, PATH_MAX, "%s/%s.draft", directory, note->id);
}

static void write_recovery(Note *note, const char *markdown) {
    if (!note || !note->id[0]) return;
    char path[PATH_MAX]; recovery_path(note, path);
    const char *title = note_display_title(note), *tags = note_display_tags(note);
    size_t size = strlen(title) + strlen(tags) + strlen(markdown) + 4;
    char *draft = calloc(size, 1); snprintf(draft, size, "%s\n%s\n%s", title, tags, markdown);
    write_atomic(path, draft); free(draft);
}

static void clear_recovery(Note *note) {
    if (!note || !note->id[0]) return;
    char path[PATH_MAX]; recovery_path(note, path); unlink(path);
}

static void restore_recovery(void) {
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

static void load_library(void) {
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
        notebook->expanded = true;
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

static void add_library_color_class(Ptr widget, const char *color) {
    if (!widget || !color || !color[0]) return;
    if (strcasecmp(color, "#d1242f") == 0) gtk_widget_add_css_class(widget, "color-red");
    else if (strcasecmp(color, "#bc4c00") == 0) gtk_widget_add_css_class(widget, "color-orange");
    else if (strcasecmp(color, "#9a6700") == 0) gtk_widget_add_css_class(widget, "color-yellow");
    else if (strcasecmp(color, "#1a7f37") == 0) gtk_widget_add_css_class(widget, "color-green");
    else if (strcasecmp(color, "#0969da") == 0) gtk_widget_add_css_class(widget, "color-blue");
    else if (strcasecmp(color, "#8250df") == 0) gtk_widget_add_css_class(widget, "color-purple");
}

static void append_note_button(Note *note, Ptr container) {
    ensure_metadata_loaded(note);
    Ptr button = text_button(note_display_title(note), "sidebar-note");
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

static bool note_has_tag(Note *note, const char *selected) {
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

static void remove_note_tag(Ptr button, Ptr user_data) {
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

static void rebuild_note_tags(void) {
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

static void add_note_tag(Ptr entry, Ptr unused) {
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

static void add_note_tag_clicked(Ptr button, Ptr unused) { (void)button; add_note_tag(state.tag_input, unused); }

static void free_signal_data(Ptr data, Ptr closure) { (void)closure; free(data); }

static void tag_selected(Ptr button, Ptr user_data) {
    (void)button; const char *tag = user_data;
    if (strcmp(state.selected_tag, tag) == 0) state.selected_tag[0] = '\0';
    else snprintf(state.selected_tag, sizeof state.selected_tag, "%s", tag);
    rebuild_sidebar();
}

static void clear_tag_filter(Ptr button, Ptr unused) {
    (void)button; (void)unused; state.selected_tag[0] = '\0'; rebuild_sidebar();
}

static void rebuild_tags(void) {
    if (!state.tag_box) return;
    while (gtk_widget_get_first_child(state.tag_box))
        gtk_box_remove(state.tag_box, gtk_widget_get_first_child(state.tag_box));
    Ptr header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4); Ptr label = gtk_label_new(tr("Tags"));
    gtk_label_set_xalign(label, 0.0f); gtk_widget_set_hexpand(label, true); gtk_widget_add_css_class(label, "section-title");
    gtk_box_append(header, label);
    if (state.selected_tag[0]) {
        Ptr clear = icon_button("edit-clear-symbolic", "Limpar filtro de tag");
        g_signal_connect_data(clear, "clicked", (void *)clear_tag_filter, NULL, NULL, 0); gtk_box_append(header, clear);
    }
    gtk_box_append(state.tag_box, header);
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
        char text[180]; snprintf(text, sizeof text, "#%s   %u", values[index], counts[index]);
        Ptr choice = navigation_button("tag-symbolic", text); if (strcasecmp(state.selected_tag, values[index]) == 0)
            gtk_widget_add_css_class(choice, "selected-tag");
        char *value = strdup(values[index]);
        g_signal_connect_data(choice, "clicked", (void *)tag_selected, value, (void *)free_signal_data, 0);
        gtk_box_append(state.tag_box, choice);
    }
    if (!total) gtk_box_append(state.tag_box, gtk_label_new(tr("Sem tags")));
}

static void select_notebook(Ptr button, Ptr user_data) {
    (void)button;
    if (state.active_notebook && state.active_notebook->button)
        gtk_widget_remove_css_class(state.active_notebook->button, "selected-item");
    state.active_notebook = user_data;
    if (state.active_notebook && state.active_notebook->button)
        gtk_widget_add_css_class(state.active_notebook->button, "selected-item");
    char message[320]; snprintf(message, sizeof message, "Notebook selecionado: %s", state.active_notebook->title);
    gtk_label_set_text(state.status, message);
}

static void create_note_in_notebook(Ptr button, Ptr user_data) {
    (void)button; state.active_notebook = user_data; new_note(NULL, NULL);
}

static void notebook_expanded_changed(Ptr expander, Ptr unused, Ptr user_data) {
    (void)unused; Notebook *book = user_data;
    if (book) book->expanded = gtk_expander_get_expanded(expander);
}

static void rebuild_sidebar(void) {
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

static void close_note_now(Note *closing, bool save_changes) {
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

static void close_note_response(Ptr dialog, int response, Ptr user_data) {
    Note *closing = user_data;
    if (response == 1) close_note_now(closing, true);
    else if (response == 2) close_note_now(closing, false);
    state.close_note_dialog = NULL; gtk_window_destroy(dialog);
}

static void close_tab(Ptr button, Ptr user_data) {
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

static void active_tab_title_changed(Ptr entry, Ptr user_data) {
    Note *note = user_data;
    if (!note || note != state.active || state.loading) return;
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

static void rebuild_tabs(void) {
    if (!state.tabbar) return;
    while (gtk_widget_get_first_child(state.tabbar))
        gtk_box_remove(state.tabbar, gtk_widget_get_first_child(state.tabbar));
    for (Note *note = state.notes; note; note = note->next) note->dirty_indicator = NULL;
    for (Note *note = state.notes; note; note = note->next) if (note->opened) {
        Ptr tab;
        if (note == state.active) {
            tab = gtk_entry_new();
            gtk_editable_set_text(tab, note_display_title(note));
            gtk_widget_add_css_class(tab, "tab");
            gtk_widget_add_css_class(tab, "active-tab");
            gtk_widget_add_css_class(tab, "tab-title");
            gtk_widget_set_size_request(tab, 100, -1);
            g_signal_connect_data(tab, "changed", (void *)active_tab_title_changed, note, NULL, 0);
        } else {
            tab = text_button(note_display_title(note), "tab");
            gtk_widget_set_size_request(tab, 100, -1);
            g_signal_connect_data(tab, "clicked", (void *)select_note, note, NULL, 0);
        }
        gtk_box_append(state.tabbar, tab);
        Ptr dirty = gtk_label_new("●"); gtk_widget_add_css_class(dirty, "dirty-indicator");
        gtk_widget_set_tooltip_text(dirty, tr("Alterações por guardar")); gtk_widget_set_visible(dirty, note->dirty);
        gtk_box_append(state.tabbar, dirty); note->dirty_indicator = dirty;
        Ptr close = icon_button("window-close-symbolic", "Fechar separador"); gtk_widget_add_css_class(close, "tab-close");
        g_signal_connect_data(close, "clicked", (void *)close_tab, note, NULL, 0); gtk_box_append(state.tabbar, close);
    }
}

static void search_changed(Ptr entry, Ptr unused) {
    (void)entry; (void)unused; rebuild_sidebar();
}

static void unique_path(char *output, size_t size, const char *parent, const char *base) {
    snprintf(output, size, "%s/%s", parent, base);
    for (unsigned index = 2; access(output, F_OK) == 0; index++)
        snprintf(output, size, "%s/%s %u", parent, base, index);
}

static void new_notebook(Ptr button, Ptr unused) {
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

static void new_note(Ptr button, Ptr unused) {
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
    note->next = state.notes; state.notes = note; rebuild_sidebar(); select_note(NULL, note);
}

typedef struct { Ptr window; const char *title; const char *markdown; } TemplateData;

static void use_template(Ptr button, Ptr user_data) {
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

static void show_templates(Ptr button, Ptr unused) {
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

static void choose_folder_response(Ptr dialog, int response, Ptr unused) {
    (void)unused;
    if (response == GTK_RESPONSE_ACCEPT) {
        Ptr file = gtk_file_chooser_get_file(dialog);
        char *path = file ? g_file_get_path(file) : NULL;
        if (path) {
            if (state.autosave_source) { g_source_remove(state.autosave_source); save_active(NULL); }
            free_notes(); snprintf(state.root, sizeof state.root, "%s", path); state.active_notebook = NULL;
            save_settings();
            load_library(); rebuild_sidebar();
            gtk_label_set_text(state.status, state.notes ? "Biblioteca carregada" : "Pasta vazia — crie um notebook");
            if (state.notes) select_note(NULL, state.notes);
            g_free(path);
        }
    }
    g_object_unref(dialog);
}

static void choose_folder(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    Ptr dialog = gtk_file_chooser_native_new("Escolher pasta das notas", state.window,
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, "Escolher", "Cancelar");
    g_signal_connect_data(dialog, "response", (void *)choose_folder_response, NULL, NULL, 0);
    gtk_native_dialog_show(dialog);
}

static Note *add_external_note(const char *path) {
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

static void open_markdown_response(Ptr dialog, int response, Ptr unused) {
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

static void open_markdown(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    Ptr dialog = gtk_file_chooser_native_new(tr("Abrir ficheiro Markdown"), state.window,
        GTK_FILE_CHOOSER_ACTION_OPEN, tr("Abrir"), tr("Cancelar"));
    g_signal_connect_data(dialog, "response", (void *)open_markdown_response, NULL, NULL, 0);
    gtk_native_dialog_show(dialog);
}

static bool copy_file(const char *source, const char *destination) {
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

static bool store_image(const char *source) {
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

typedef struct { Note *note; } ClipboardImageRequest;

static void clipboard_texture_ready(Ptr clipboard, Ptr result, Ptr user_data) {
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

static gboolean paste_image_key(Ptr controller, guint keyval, guint keycode, guint modifiers, Ptr unused) {
    (void)controller; (void)keycode; (void)unused;
    if (!(modifiers & GDK_CONTROL_MASK) || (keyval != 'v' && keyval != 'V') || !state.active) return false;
    Ptr clipboard = gdk_display_get_clipboard(gdk_display_get_default());
    Ptr formats = clipboard ? gdk_clipboard_get_formats(clipboard) : NULL;
    if (!formats || !gdk_content_formats_contain_gtype(formats, gdk_texture_get_type())) return false;
    ClipboardImageRequest *request = calloc(1, sizeof *request); request->note = state.active;
    gdk_clipboard_read_texture_async(clipboard, NULL, (void *)clipboard_texture_ready, request);
    return true;
}

static void image_response(Ptr dialog, int response, Ptr unused) {
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

static void insert_image(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    if (!state.active) return;
    Ptr dialog = gtk_file_chooser_native_new(tr("Inserir imagem"), state.window,
        GTK_FILE_CHOOSER_ACTION_OPEN, tr("Inserir"), tr("Cancelar"));
    g_signal_connect_data(dialog, "response", (void *)image_response, NULL, NULL, 0);
    gtk_native_dialog_show(dialog);
}

static char *html_escape(const char *source) {
    size_t capacity = strlen(source) * 6 + 1, used = 0;
    char *result = calloc(capacity, 1);
    for (; *source; source++) {
        const char *replacement = NULL;
        if (*source == '&') replacement = "&amp;";
        else if (*source == '<') replacement = "&lt;";
        else if (*source == '>') replacement = "&gt;";
        if (replacement) { size_t length = strlen(replacement); memcpy(result + used, replacement, length); used += length; }
        else result[used++] = *source;
    }
    return result;
}

typedef struct { char *value; size_t length, capacity; } StringBuilder;

static void builder_append_n(StringBuilder *builder, const char *value, size_t length) {
    if (builder->length + length + 1 > builder->capacity) {
        size_t capacity = builder->capacity ? builder->capacity : 1024;
        while (capacity < builder->length + length + 1) capacity *= 2;
        builder->value = realloc(builder->value, capacity); builder->capacity = capacity;
    }
    memcpy(builder->value + builder->length, value, length); builder->length += length;
    builder->value[builder->length] = '\0';
}

static void builder_append(StringBuilder *builder, const char *value) {
    builder_append_n(builder, value, strlen(value));
}

static void builder_printf(StringBuilder *builder, const char *format, ...) {
    va_list arguments; va_start(arguments, format); va_list copy; va_copy(copy, arguments);
    int required = vsnprintf(NULL, 0, format, copy); va_end(copy);
    if (required > 0) {
        char *value = malloc((size_t)required + 1); vsnprintf(value, (size_t)required + 1, format, arguments);
        builder_append_n(builder, value, (size_t)required); free(value);
    }
    va_end(arguments);
}

static void builder_escape_n(StringBuilder *builder, const char *source, size_t length) {
    for (size_t index = 0; index < length; index++) {
        if (source[index] == '&') builder_append(builder, "&amp;");
        else if (source[index] == '<') builder_append(builder, "&lt;");
        else if (source[index] == '>') builder_append(builder, "&gt;");
        else if (source[index] == '\"') builder_append(builder, "&quot;");
        else builder_append_n(builder, source + index, 1);
    }
}

static void html_inline(StringBuilder *output, const char *source) {
    const char *cursor = source;
    while (*cursor) {
        const char *close = NULL;
        if (strncmp(cursor, "<span style=\"color:#", 20) == 0) {
            const char *content = strchr(cursor + 20, '>'), *end = content ? strstr(content + 1, "</span>") : NULL;
            if (content && end && content - cursor == 27) {
                char color[8]; memcpy(color, cursor + 19, 7); color[7] = '\0';
                const char *allowed[] = {"#d1242f", "#bc4c00", "#9a6700", "#1a7f37", "#0969da", "#8250df"}; bool valid = false;
                for (size_t index = 0; index < sizeof allowed / sizeof allowed[0]; index++) if (strcasecmp(color, allowed[index]) == 0) valid = true;
                if (valid) {
                    char *inner = strndup(content + 1, (size_t)(end - content - 1)); builder_printf(output, "<span style=\"color:%s\">", color);
                    html_inline(output, inner); builder_append(output, "</span>"); free(inner); cursor = end + 7; continue;
                }
            }
        }
        if (strncmp(cursor, "**", 2) == 0 && (close = strstr(cursor + 2, "**"))) {
            builder_append(output, "<strong>"); builder_escape_n(output, cursor + 2, (size_t)(close - cursor - 2));
            builder_append(output, "</strong>"); cursor = close + 2; continue;
        }
        if (strncmp(cursor, "~~", 2) == 0 && (close = strstr(cursor + 2, "~~"))) {
            builder_append(output, "<s>"); builder_escape_n(output, cursor + 2, (size_t)(close - cursor - 2));
            builder_append(output, "</s>"); cursor = close + 2; continue;
        }
        if (*cursor == '_' && (close = strchr(cursor + 1, '_'))) {
            builder_append(output, "<em>"); builder_escape_n(output, cursor + 1, (size_t)(close - cursor - 1));
            builder_append(output, "</em>"); cursor = close + 1; continue;
        }
        if (*cursor == '`' && (close = strchr(cursor + 1, '`'))) {
            builder_append(output, "<code>"); builder_escape_n(output, cursor + 1, (size_t)(close - cursor - 1));
            builder_append(output, "</code>"); cursor = close + 1; continue;
        }
        if (*cursor == '[' && cursor[1] != '[' && (close = strstr(cursor + 1, "]("))) {
            const char *end = strchr(close + 2, ')');
            if (end) {
                builder_append(output, "<a href=\""); builder_escape_n(output, close + 2, (size_t)(end - close - 2));
                builder_append(output, "\">"); builder_escape_n(output, cursor + 1, (size_t)(close - cursor - 1));
                builder_append(output, "</a>"); cursor = end + 1; continue;
            }
        }
        if (strncmp(cursor, "[[", 2) == 0 && (close = strstr(cursor + 2, "]]"))) {
            builder_append(output, "<span class=\"wikilink\">");
            builder_escape_n(output, cursor + 2, (size_t)(close - cursor - 2));
            builder_append(output, "</span>"); cursor = close + 2; continue;
        }
        builder_escape_n(output, cursor, 1); cursor++;
    }
}

static bool markdown_heading(const char *line, int *level, const char **text) {
    int count = 0; while (line[count] == '#' && count < 6) count++;
    if (!count || line[count] != ' ') return false;
    *level = count; *text = line + count + 1; return true;
}

static bool markdown_image(const char *line, const char **alt, size_t *alt_length, const char **source, size_t *source_length) {
    if (strncmp(line, "![", 2) != 0) return false;
    const char *middle = strstr(line + 2, "]("), *end = middle ? strrchr(middle + 2, ')') : NULL;
    if (!middle || !end || end[1]) return false;
    *alt = line + 2; *alt_length = (size_t)(middle - line - 2);
    *source = middle + 2; *source_length = (size_t)(end - middle - 2); return true;
}

static void html_table_row(StringBuilder *output, const char *line, const char *cell_tag) {
    char *copy = strdup(line), *value = copy;
    while (*value == ' ' || *value == '|') value++;
    size_t length = strlen(value); while (length && (value[length - 1] == ' ' || value[length - 1] == '|')) value[--length] = '\0';
    builder_append(output, "<tr>"); char *save = NULL;
    for (char *cell = strtok_r(value, "|", &save); cell; cell = strtok_r(NULL, "|", &save)) {
        while (*cell == ' ') cell++;
        size_t cell_length = strlen(cell);
        while (cell_length && cell[cell_length - 1] == ' ') cell[--cell_length] = '\0';
        builder_printf(output, "<%s>", cell_tag); html_inline(output, cell); builder_printf(output, "</%s>", cell_tag);
    }
    builder_append(output, "</tr>"); free(copy);
}

static char *markdown_to_html(const char *markdown) {
    char *copy = strdup(markdown); size_t markdown_length = strlen(copy), line_count = 1;
    for (size_t position = 0; position < markdown_length; position++) if (copy[position] == '\n') line_count++;
    char **lines = calloc(line_count + 1, sizeof *lines); size_t count = 0; lines[count++] = copy;
    for (size_t position = 0; position < markdown_length; position++) if (copy[position] == '\n') {
        copy[position] = '\0'; lines[count++] = copy + position + 1;
    }
    StringBuilder output = {0}; bool paragraph = false, code = false;
    for (size_t index = 0; index < count; index++) {
        const char *line = lines[index];
        if (strncmp(line, "```", 3) == 0) {
            if (paragraph) { builder_append(&output, "</p>\n"); paragraph = false; }
            builder_append(&output, code ? "</code></pre>\n" : "<pre><code>"); code = !code; continue;
        }
        if (code) { builder_escape_n(&output, line, strlen(line)); builder_append(&output, "\n"); continue; }
        if (!*line) { if (paragraph) { builder_append(&output, "</p>\n"); paragraph = false; } continue; }
        if (strchr(line, '|') && index + 1 < count && strstr(lines[index + 1], "---")) {
            if (paragraph) { builder_append(&output, "</p>\n"); paragraph = false; }
            builder_append(&output, "<table><thead>"); html_table_row(&output, line, "th"); builder_append(&output, "</thead><tbody>"); index += 2;
            while (index < count && strchr(lines[index], '|') && *lines[index]) { html_table_row(&output, lines[index], "td"); index++; }
            builder_append(&output, "</tbody></table>\n"); index--; continue;
        }
        int level; const char *heading;
        const char *alt, *image_source; size_t alt_length, source_length;
        if (markdown_heading(line, &level, &heading)) {
            if (paragraph) { builder_append(&output, "</p>\n"); paragraph = false; }
            builder_printf(&output, "<h%d>", level); html_inline(&output, heading); builder_printf(&output, "</h%d>\n", level);
        } else if (strcmp(line, "---") == 0 || strcmp(line, "***") == 0) {
            if (paragraph) { builder_append(&output, "</p>\n"); paragraph = false; } builder_append(&output, "<hr>\n");
        } else if (markdown_image(line, &alt, &alt_length, &image_source, &source_length)) {
            if (paragraph) { builder_append(&output, "</p>\n"); paragraph = false; }
            builder_append(&output, "<img src=\""); builder_escape_n(&output, image_source, source_length);
            builder_append(&output, "\" alt=\""); builder_escape_n(&output, alt, alt_length); builder_append(&output, "\">\n");
        } else if (strncmp(line, "- [ ] ", 6) == 0 || strncmp(line, "* [ ] ", 6) == 0) {
            if (paragraph) { builder_append(&output, "</p>\n"); paragraph = false; }
            builder_append(&output, "<p class=\"task\">☐ "); html_inline(&output, line + 6); builder_append(&output, "</p>\n");
        } else if (strncmp(line, "- [x] ", 6) == 0 || strncmp(line, "- [X] ", 6) == 0) {
            if (paragraph) { builder_append(&output, "</p>\n"); paragraph = false; }
            builder_append(&output, "<p class=\"task done\">☑ <s>"); html_inline(&output, line + 6); builder_append(&output, "</s></p>\n");
        } else if (strncmp(line, "- ", 2) == 0 || strncmp(line, "* ", 2) == 0) {
            if (paragraph) { builder_append(&output, "</p>\n"); paragraph = false; }
            builder_append(&output, "<ul><li>"); html_inline(&output, line + 2); builder_append(&output, "</li></ul>\n");
        } else if (line[0] >= '0' && line[0] <= '9' && strchr(line, '.') && strchr(line, '.')[1] == ' ') {
            const char *dot = strchr(line, '.');
            if (paragraph) { builder_append(&output, "</p>\n"); paragraph = false; }
            builder_append(&output, "<ol><li>"); html_inline(&output, dot + 2); builder_append(&output, "</li></ol>\n");
        } else if (strncmp(line, "> ", 2) == 0) {
            if (paragraph) { builder_append(&output, "</p>\n"); paragraph = false; }
            builder_append(&output, "<blockquote>"); html_inline(&output, line + 2); builder_append(&output, "</blockquote>\n");
        } else {
            if (!paragraph) { builder_append(&output, "<p>"); paragraph = true; } else builder_append(&output, " ");
            html_inline(&output, line);
        }
    }
    if (paragraph) builder_append(&output, "</p>\n");
    if (code) builder_append(&output, "</code></pre>\n");
    free(lines); free(copy); if (!output.value) output.value = strdup(""); return output.value;
}

static char *html_document(const char *title, const char *markdown) {
    char *body = markdown_to_html(markdown), *safe_title = html_escape(title); StringBuilder document = {0};
    builder_append(&document, "<!doctype html><html lang=\"pt\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width\">");
    builder_printf(&document, "<title>%s</title><style>", safe_title);
    builder_append(&document, "body{max-width:820px;margin:48px auto;padding:0 24px;font:16px/1.62 system-ui;color:#24292f}h1{border-bottom:2px solid #e8eaed;padding-bottom:12px}h2{color:#174ea6}blockquote{background:#f7f9fc;border-left:4px solid #4285f4;padding:10px 16px}code,pre{background:#f3f4f6;border-radius:6px}code{padding:2px 5px}pre{padding:14px;white-space:pre-wrap}img{display:block;max-width:100%;height:auto;margin:24px auto;border-radius:8px}table{border-collapse:collapse;width:100%;margin:18px 0}th,td{border:1px solid #d0d7de;padding:8px 10px;text-align:left}th{background:#f3f4f6}.done{opacity:.7}.wikilink{color:#0969da}@media(prefers-color-scheme:dark){body{background:#11141a;color:#e8eaf0}h2,.wikilink{color:#78a9ff}blockquote,code,pre,th{background:#1b2029}th,td{border-color:#394150}}</style></head><body>");
    builder_printf(&document, "<h1>%s</h1>", safe_title); builder_append(&document, body); builder_append(&document, "</body></html>");
    free(body); free(safe_title); return document.value;
}

static void export_html_response(Ptr dialog, int response, Ptr unused) {
    (void)unused;
    if (response == GTK_RESPONSE_ACCEPT && state.active) {
        Ptr file = gtk_file_chooser_get_file(dialog); char *path = file ? g_file_get_path(file) : NULL;
        if (path) {
            char *markdown = editor_text(), *document = html_document(state.active->title, markdown);
            bool ok = write_atomic(path, document); gtk_label_set_text(state.status, ok ? "HTML exportado" : "Erro ao exportar HTML");
            free(document); g_free(markdown); g_free(path);
        }
    }
    g_object_unref(dialog);
}

static void export_html(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    if (!state.active) return;
    Ptr dialog = gtk_file_chooser_native_new(tr("Exportar HTML"), state.window,
        GTK_FILE_CHOOSER_ACTION_SAVE, tr("Exportar"), tr("Cancelar"));
    char filename[300]; snprintf(filename, sizeof filename, "%s.html", state.active->title);
    gtk_file_chooser_set_current_name(dialog, filename);
    g_signal_connect_data(dialog, "response", (void *)export_html_response, NULL, NULL, 0);
    gtk_native_dialog_show(dialog);
}

static void markdown_plain_inline(const char *source, char *output, size_t size) {
    size_t used = 0; const char *cursor = source;
    while (*cursor && used + 1 < size) {
        if (strncmp(cursor, "**", 2) == 0) { cursor += 2; continue; }
        if (*cursor == '_' || *cursor == '`') { cursor++; continue; }
        if (strncmp(cursor, "[[", 2) == 0) { cursor += 2; continue; }
        if (strncmp(cursor, "]]", 2) == 0) { cursor += 2; continue; }
        if (*cursor == '[') {
            const char *middle = strstr(cursor + 1, "]("), *end = middle ? strchr(middle + 2, ')') : NULL;
            if (middle && end) {
                size_t length = (size_t)(middle - cursor - 1); if (length > size - used - 1) length = size - used - 1;
                memcpy(output + used, cursor + 1, length); used += length; cursor = end + 1; continue;
            }
        }
        output[used++] = *cursor++;
    }
    output[used] = '\0';
}

static void pdf_line(Ptr context, double *y, const char *text, double size, int weight, bool monospace, double indent) {
    const size_t maximum = monospace ? 72 : 82; const char *cursor = text;
    if (!*cursor) { *y += size + 5; return; }
    while (*cursor) {
        size_t remaining = strlen(cursor), length = remaining > maximum ? maximum : remaining;
        if (length < remaining) { size_t split = length; while (split > maximum / 2 && cursor[split] != ' ') split--; if (split > maximum / 2) length = split; }
        char line[4096]; if (length >= sizeof line) length = sizeof line - 1;
        memcpy(line, cursor, length); line[length] = '\0';
        if (*y > 790) { cairo_show_page(context); *y = 54; }
        cairo_select_font_face(context, monospace ? "Monospace" : "Sans", 0, weight); cairo_set_font_size(context, size);
        cairo_move_to(context, 54 + indent, *y); cairo_show_text(context, line); *y += size + 6;
        cursor += length; while (*cursor == ' ') cursor++;
    }
}

static bool pdf_export_content(const char *destination, const char *title, const char *markdown) {
    Ptr surface = cairo_pdf_surface_create(destination, 595.0, 842.0); if (!surface) return false;
    Ptr context = cairo_create(surface); cairo_select_font_face(context, "Sans", 0, 1);
    cairo_set_font_size(context, 22); cairo_move_to(context, 54, 58); cairo_show_text(context, title);
    char *copy = strdup(markdown), *save = NULL; double y = 92; bool code = false;
    for (char *line = strtok_r(copy, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        double size = 11; int weight = 0; double indent = 0; const char *text = line; char plain[4096], decorated[4096];
        if (strncmp(line, "```", 3) == 0) { code = !code; y += 4; continue; }
        if (strncmp(line, "### ", 4) == 0) { size = 14; weight = 1; text += 4; }
        else if (strncmp(line, "## ", 3) == 0) { size = 17; weight = 1; text += 3; }
        else if (strncmp(line, "# ", 2) == 0) { size = 20; weight = 1; text += 2; }
        else if (strncmp(line, "- [ ] ", 6) == 0 || strncmp(line, "* [ ] ", 6) == 0) { snprintf(decorated, sizeof decorated, "☐  %s", line + 6); text = decorated; indent = 12; }
        else if (strncmp(line, "- [x] ", 6) == 0 || strncmp(line, "- [X] ", 6) == 0) { snprintf(decorated, sizeof decorated, "☑  %s", line + 6); text = decorated; indent = 12; }
        else if (strncmp(line, "- ", 2) == 0 || strncmp(line, "* ", 2) == 0) { snprintf(decorated, sizeof decorated, "•  %s", line + 2); text = decorated; indent = 12; }
        else if (strncmp(line, "> ", 2) == 0) { snprintf(decorated, sizeof decorated, "│  %s", line + 2); text = decorated; indent = 14; }
        else if (strncmp(line, "![", 2) == 0) {
            const char *middle = strstr(line + 2, "](");
            if (middle) snprintf(decorated, sizeof decorated, "[Imagem: %.*s]", (int)(middle - line - 2), line + 2);
            else snprintf(decorated, sizeof decorated, "[Imagem]");
            text = decorated; indent = 12;
        } else if (strcmp(line, "---") == 0 || strcmp(line, "***") == 0) text = "────────────────────────────────────────────────";
        markdown_plain_inline(text, plain, sizeof plain);
        if (line[0] == '|') for (char *cell = plain; *cell; cell++) if (*cell == '|') *cell = ' ';
        pdf_line(context, &y, plain, size, weight, code, indent);
    }
    free(copy); cairo_destroy(context); cairo_surface_finish(surface);
    bool ok = cairo_surface_status(surface) == 0; cairo_surface_destroy(surface); return ok;
}

static bool pdf_export(const char *destination) {
    char *markdown = editor_text(); bool ok = pdf_export_content(destination, state.active->title, markdown);
    g_free(markdown); return ok;
}

static void docx_run(StringBuilder *output, const char *text, size_t length, const char *properties) {
    builder_append(output, "<w:r>"); if (properties && *properties) builder_printf(output, "<w:rPr>%s</w:rPr>", properties);
    builder_append(output, "<w:t xml:space=\"preserve\">"); builder_escape_n(output, text, length);
    builder_append(output, "</w:t></w:r>");
}

static void docx_inline(StringBuilder *output, const char *source) {
    const char *cursor = source;
    while (*cursor) {
        const char *close = NULL;
        if (strncmp(cursor, "<span style=\"color:#", 20) == 0) {
            const char *content = strchr(cursor + 20, '>'), *end = content ? strstr(content + 1, "</span>") : NULL;
            if (content && end && content - cursor == 27) {
                char color[7]; memcpy(color, cursor + 20, 6); color[6] = '\0'; char properties[64];
                bool valid = true; for (size_t index = 0; index < 6; index++) if (!isxdigit((unsigned char)color[index])) valid = false;
                if (valid) {
                    snprintf(properties, sizeof properties, "<w:color w:val=\"%s\"/>", color);
                    docx_run(output, content + 1, (size_t)(end - content - 1), properties); cursor = end + 7; continue;
                }
            }
        }
        if (strncmp(cursor, "**", 2) == 0 && (close = strstr(cursor + 2, "**"))) {
            docx_run(output, cursor + 2, (size_t)(close - cursor - 2), "<w:b/>"); cursor = close + 2; continue;
        }
        if (strncmp(cursor, "~~", 2) == 0 && (close = strstr(cursor + 2, "~~"))) {
            docx_run(output, cursor + 2, (size_t)(close - cursor - 2), "<w:strike/>"); cursor = close + 2; continue;
        }
        if (*cursor == '_' && (close = strchr(cursor + 1, '_'))) {
            docx_run(output, cursor + 1, (size_t)(close - cursor - 1), "<w:i/>"); cursor = close + 1; continue;
        }
        if (*cursor == '`' && (close = strchr(cursor + 1, '`'))) {
            docx_run(output, cursor + 1, (size_t)(close - cursor - 1), "<w:rFonts w:ascii=\"Consolas\" w:hAnsi=\"Consolas\"/><w:shd w:fill=\"F3F4F6\"/>");
            cursor = close + 1; continue;
        }
        if (*cursor == '[' && cursor[1] != '[' && (close = strstr(cursor + 1, "]("))) {
            const char *end = strchr(close + 2, ')');
            if (end) {
                docx_run(output, cursor + 1, (size_t)(close - cursor - 1), "<w:color w:val=\"0969DA\"/><w:u w:val=\"single\"/>");
                cursor = end + 1; continue;
            }
        }
        const char *special = cursor + 1;
        while (*special && *special != '*' && *special != '~' && *special != '_' && *special != '`' && *special != '[' && *special != '<') special++;
        docx_run(output, cursor, (size_t)(special - cursor), NULL); cursor = special;
    }
}

static void docx_paragraph(StringBuilder *output, const char *text, const char *paragraph_properties, const char *run_properties) {
    builder_append(output, "<w:p>");
    if (paragraph_properties && *paragraph_properties) builder_printf(output, "<w:pPr>%s</w:pPr>", paragraph_properties);
    if (run_properties) docx_run(output, text, strlen(text), run_properties); else docx_inline(output, text);
    builder_append(output, "</w:p>");
}

static void docx_table_row(StringBuilder *output, const char *line, bool header) {
    char *copy = strdup(line), *value = copy; while (*value == ' ' || *value == '|') value++;
    size_t length = strlen(value); while (length && (value[length - 1] == ' ' || value[length - 1] == '|')) value[--length] = '\0';
    builder_append(output, "<w:tr>"); char *save = NULL;
    for (char *cell = strtok_r(value, "|", &save); cell; cell = strtok_r(NULL, "|", &save)) {
        while (*cell == ' ') cell++;
        size_t cell_length = strlen(cell);
        while (cell_length && cell[cell_length - 1] == ' ') cell[--cell_length] = '\0';
        builder_append(output, "<w:tc><w:tcPr><w:tcW w:w=\"0\" w:type=\"auto\"/></w:tcPr><w:p>");
        if (header) docx_run(output, cell, strlen(cell), "<w:b/>"); else docx_inline(output, cell);
        builder_append(output, "</w:p></w:tc>");
    }
    builder_append(output, "</w:tr>"); free(copy);
}

static char *docx_document_xml(const char *markdown) {
    size_t markdown_length = strlen(markdown), line_count = 1;
    char *copy = strdup(markdown); for (size_t index = 0; index < markdown_length; index++) if (copy[index] == '\n') line_count++;
    char **lines = calloc(line_count + 1, sizeof *lines); size_t count = 0; lines[count++] = copy;
    for (size_t index = 0; index < markdown_length; index++) if (copy[index] == '\n') { copy[index] = '\0'; lines[count++] = copy + index + 1; }
    StringBuilder xml = {0}; builder_append(&xml, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\"><w:body>");
    bool code = false;
    for (size_t index = 0; index < count; index++) {
        const char *line = lines[index];
        if (strncmp(line, "```", 3) == 0) { code = !code; continue; }
        if (code) { docx_paragraph(&xml, line, "<w:shd w:fill=\"F3F4F6\"/><w:spacing w:after=\"0\"/>",
            "<w:rFonts w:ascii=\"Consolas\" w:hAnsi=\"Consolas\"/><w:sz w:val=\"20\"/>"); continue; }
        if (!*line) { builder_append(&xml, "<w:p/>"); continue; }
        if (strchr(line, '|') && index + 1 < count && strstr(lines[index + 1], "---")) {
            builder_append(&xml, "<w:tbl><w:tblPr><w:tblBorders><w:top w:val=\"single\" w:sz=\"4\"/><w:left w:val=\"single\" w:sz=\"4\"/><w:bottom w:val=\"single\" w:sz=\"4\"/><w:right w:val=\"single\" w:sz=\"4\"/><w:insideH w:val=\"single\" w:sz=\"4\"/><w:insideV w:val=\"single\" w:sz=\"4\"/></w:tblBorders></w:tblPr>");
            docx_table_row(&xml, line, true); index += 2;
            while (index < count && strchr(lines[index], '|') && *lines[index]) { docx_table_row(&xml, lines[index], false); index++; }
            builder_append(&xml, "</w:tbl>"); index--; continue;
        }
        int level; const char *heading;
        if (markdown_heading(line, &level, &heading)) {
            char properties[160]; int size = 38 - (level - 1) * 4; if (size < 24) size = 24;
            snprintf(properties, sizeof properties, "<w:b/><w:sz w:val=\"%d\"/><w:color w:val=\"%s\"/>", size, level == 2 ? "174EA6" : "202124");
            docx_paragraph(&xml, heading, "<w:keepNext/><w:spacing w:before=\"240\" w:after=\"100\"/>", properties);
        } else if (strncmp(line, "- [ ] ", 6) == 0 || strncmp(line, "* [ ] ", 6) == 0) {
            StringBuilder task = {0}; builder_append(&task, "☐ "); builder_append(&task, line + 6); docx_paragraph(&xml, task.value, "<w:ind w:left=\"360\"/>", NULL); free(task.value);
        } else if (strncmp(line, "- [x] ", 6) == 0 || strncmp(line, "- [X] ", 6) == 0) {
            StringBuilder task = {0}; builder_append(&task, "☑ "); builder_append(&task, line + 6); docx_paragraph(&xml, task.value, "<w:ind w:left=\"360\"/>", "<w:strike/><w:color w:val=\"6B7280\"/>"); free(task.value);
        } else if (strncmp(line, "- ", 2) == 0 || strncmp(line, "* ", 2) == 0) {
            docx_paragraph(&xml, line + 2, "<w:numPr><w:ilvl w:val=\"0\"/><w:numId w:val=\"1\"/></w:numPr>", NULL);
        } else if (line[0] >= '0' && line[0] <= '9' && strchr(line, '.') && strchr(line, '.')[1] == ' ') {
            docx_paragraph(&xml, strchr(line, '.') + 2, "<w:numPr><w:ilvl w:val=\"0\"/><w:numId w:val=\"2\"/></w:numPr>", NULL);
        } else if (strncmp(line, "> ", 2) == 0) {
            docx_paragraph(&xml, line + 2, "<w:ind w:left=\"420\"/><w:pBdr><w:left w:val=\"single\" w:sz=\"18\" w:color=\"4285F4\"/></w:pBdr>", "<w:i/><w:color w:val=\"4A5568\"/>");
        } else if (strcmp(line, "---") == 0 || strcmp(line, "***") == 0) {
            builder_append(&xml, "<w:p><w:pPr><w:pBdr><w:bottom w:val=\"single\" w:sz=\"6\" w:color=\"D0D7DE\"/></w:pBdr></w:pPr></w:p>");
        } else docx_paragraph(&xml, line, NULL, NULL);
    }
    builder_append(&xml, "<w:sectPr><w:pgSz w:w=\"11906\" w:h=\"16838\"/><w:pgMar w:top=\"1440\" w:right=\"1440\" w:bottom=\"1440\" w:left=\"1440\"/></w:sectPr></w:body></w:document>");
    free(lines); free(copy); return xml.value;
}

static bool docx_export_content(const char *destination, const char *markdown) {
    char temporary[] = "/tmp/notemd-docx-XXXXXX"; if (!mkdtemp(temporary)) return false;
    char rels_dir[PATH_MAX], word_dir[PATH_MAX], word_rels_dir[PATH_MAX], content_path[PATH_MAX], rels_path[PATH_MAX];
    char document_path[PATH_MAX], numbering_path[PATH_MAX], document_rels_path[PATH_MAX];
    snprintf(rels_dir, sizeof rels_dir, "%s/_rels", temporary); snprintf(word_dir, sizeof word_dir, "%s/word", temporary);
    snprintf(word_rels_dir, sizeof word_rels_dir, "%s/_rels", word_dir);
    make_directory(rels_dir); make_directory(word_dir); make_directory(word_rels_dir);
    snprintf(content_path, sizeof content_path, "%s/[Content_Types].xml", temporary);
    snprintf(rels_path, sizeof rels_path, "%s/.rels", rels_dir); snprintf(document_path, sizeof document_path, "%s/document.xml", word_dir);
    snprintf(numbering_path, sizeof numbering_path, "%s/numbering.xml", word_dir);
    snprintf(document_rels_path, sizeof document_rels_path, "%s/document.xml.rels", word_rels_dir);
    const char *types = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/><Override PartName=\"/word/document.xml\" "
        "ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
        "<Override PartName=\"/word/numbering.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.numbering+xml\"/></Types>";
    const char *rels = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/></Relationships>";
    const char *document_rels = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/numbering\" Target=\"numbering.xml\"/></Relationships>";
    const char *numbering = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><w:numbering xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:abstractNum w:abstractNumId=\"1\"><w:lvl w:ilvl=\"0\"><w:start w:val=\"1\"/><w:numFmt w:val=\"bullet\"/><w:lvlText w:val=\"•\"/><w:pPr><w:ind w:left=\"720\" w:hanging=\"360\"/></w:pPr></w:lvl></w:abstractNum>"
        "<w:abstractNum w:abstractNumId=\"2\"><w:lvl w:ilvl=\"0\"><w:start w:val=\"1\"/><w:numFmt w:val=\"decimal\"/><w:lvlText w:val=\"%1.\"/><w:pPr><w:ind w:left=\"720\" w:hanging=\"360\"/></w:pPr></w:lvl></w:abstractNum>"
        "<w:num w:numId=\"1\"><w:abstractNumId w:val=\"1\"/></w:num><w:num w:numId=\"2\"><w:abstractNumId w:val=\"2\"/></w:num></w:numbering>";
    char *document = docx_document_xml(markdown);
    bool ok = write_atomic(content_path, types) && write_atomic(rels_path, rels) && write_atomic(document_path, document)
        && write_atomic(numbering_path, numbering) && write_atomic(document_rels_path, document_rels); free(document);
    unlink(destination);
    if (ok) {
        pid_t child = fork();
        if (child == 0) { chdir(temporary); execlp("zip", "zip", "-q", "-r", destination, ".", (char *)NULL); _exit(127); }
        int status = 0; ok = child > 0 && waitpid(child, &status, 0) > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
    unlink(content_path); unlink(rels_path); unlink(document_path); unlink(numbering_path); unlink(document_rels_path);
    rmdir(word_rels_dir); rmdir(rels_dir); rmdir(word_dir); rmdir(temporary); return ok;
}

static bool docx_export(const char *destination) {
    char *markdown = editor_text(); bool ok = docx_export_content(destination, markdown); g_free(markdown); return ok;
}

static bool office_export(const char *destination, const char *format) {
    return strcmp(format, "pdf") == 0 ? pdf_export(destination) : docx_export(destination);
}

static void office_export_response(Ptr dialog, int response, Ptr user_data) {
    const char *format = user_data;
    if (response == GTK_RESPONSE_ACCEPT && state.active) {
        Ptr file = gtk_file_chooser_get_file(dialog); char *path = file ? g_file_get_path(file) : NULL;
        if (path) {
            bool ok = office_export(path, format);
            gtk_label_set_text(state.status, ok ? (strcmp(format, "pdf") == 0 ? "PDF exportado" : "DOCX exportado")
                                                 : "Não foi possível exportar o documento");
            g_free(path);
        }
    }
    g_object_unref(dialog);
}

static void export_office(Ptr button, Ptr user_data) {
    (void)button; const char *format = user_data; if (!state.active) return;
    const char *title = strcmp(format, "pdf") == 0 ? tr("Exportar PDF") : tr("Exportar DOCX");
    Ptr dialog = gtk_file_chooser_native_new(title, state.window, GTK_FILE_CHOOSER_ACTION_SAVE, tr("Exportar"), tr("Cancelar"));
    char filename[300]; snprintf(filename, sizeof filename, "%s.%s", state.active->title, format);
    gtk_file_chooser_set_current_name(dialog, filename);
    g_signal_connect_data(dialog, "response", (void *)office_export_response, user_data, NULL, 0);
    gtk_native_dialog_show(dialog);
}

typedef struct { char path[PATH_MAX]; Ptr row, parent, preview_buffer; } RestoreData;

static int newest_version_first(const void *left, const void *right) {
    const char *a = left, *b = right;
    return strcmp(b, a);
}

static void preview_version(Ptr button, Ptr user_data) {
    (void)button; RestoreData *version = user_data;
    if (version->parent) for (Ptr row = gtk_widget_get_first_child(version->parent); row; row = gtk_widget_get_next_sibling(row))
        gtk_widget_remove_css_class(row, "history-current");
    if (version->row) gtk_widget_add_css_class(version->row, "history-current");
    char *content = read_file(version->path); render_markdown(version->preview_buffer, content); free(content);
}

static void restore_version(Ptr button, Ptr user_data) {
    (void)button;
    RestoreData *version = user_data;
    char *content = read_file(version->path);
    gtk_text_buffer_set_text(state.editor_buffer, content, -1); update_views(content);
    gtk_label_set_text(state.status, "Versão restaurada — guarde para confirmar");
    free(content);
}

static void delete_version(Ptr button, Ptr user_data) {
    (void)button; RestoreData *version = user_data;
    if (unlink(version->path) == 0) {
        gtk_label_set_text(state.status, "Versão removida");
        if (version->parent && version->row) gtk_box_remove(version->parent, version->row);
    } else gtk_label_set_text(state.status, "Não foi possível remover a versão");
}

static void show_history(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    if (!state.active || !state.active->id[0]) return;
    Ptr window = gtk_window_new(); gtk_window_set_title(window, tr("Histórico de versões"));
    state.history_window = window;
    gtk_window_set_default_size(window, 1040, 600); gtk_window_set_transient_for(window, state.window);
    gtk_window_set_modal(window, true);
    Ptr box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10); margins(box, 16, 16);
    Ptr heading = gtk_label_new(state.active->title); gtk_label_set_xalign(heading, 0.0f);
    gtk_widget_add_css_class(heading, "history-title"); gtk_box_append(box, heading);
    Ptr subtitle = gtk_label_new(tr("Selecione uma versão para a pré-visualizar. Restaurar não grava automaticamente."));
    gtk_label_set_xalign(subtitle, 0.0f); gtk_widget_add_css_class(subtitle, "history-subtitle");
    gtk_box_append(box, subtitle);
    Ptr body = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL); gtk_paned_set_position(body, 280);
    gtk_widget_set_vexpand(body, true); gtk_widget_add_css_class(body, "history-body");
    Ptr list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6); margins(list, 8, 8);
    state.history_list = list;
    gtk_widget_add_css_class(list, "history-list");
    Ptr list_scroll = gtk_scrolled_window_new(); gtk_scrolled_window_set_child(list_scroll, list);
    gtk_widget_set_hexpand(list_scroll, true); gtk_widget_set_vexpand(list_scroll, true);
    Ptr comparison = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL); gtk_paned_set_position(comparison, 370);
    gtk_widget_set_hexpand(comparison, true); gtk_widget_set_vexpand(comparison, true);
    Ptr saved_column = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6); gtk_widget_add_css_class(saved_column, "history-column");
    Ptr saved_label = gtk_label_new(tr("Versão guardada")); gtk_label_set_xalign(saved_label, 0.0f);
    gtk_widget_add_css_class(saved_label, "history-column-title"); gtk_box_append(saved_column, saved_label);
    Ptr preview_buffer = NULL; Ptr preview = readonly_markdown_view(&preview_buffer, "preview");
    state.history_saved_buffer = preview_buffer;
    gtk_widget_add_css_class(preview, "history-preview"); gtk_box_append(saved_column, editor_scroller(preview));
    Ptr current_column = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6); gtk_widget_add_css_class(current_column, "history-column");
    Ptr current_label = gtk_label_new(tr("Versão atual")); gtk_label_set_xalign(current_label, 0.0f);
    gtk_widget_add_css_class(current_label, "history-column-title"); gtk_box_append(current_column, current_label);
    Ptr current_buffer = NULL; Ptr current = readonly_markdown_view(&current_buffer, "preview");
    state.history_current_buffer = current_buffer;
    gtk_widget_add_css_class(current, "history-preview"); gtk_box_append(current_column, editor_scroller(current));
    char *current_text = editor_text(); render_markdown(current_buffer, current_text); g_free(current_text);
    gtk_paned_set_start_child(comparison, saved_column); gtk_paned_set_end_child(comparison, current_column);
    gtk_paned_set_start_child(body, list_scroll); gtk_paned_set_end_child(body, comparison);
    const char *data_home = getenv("XDG_DATA_HOME"), *home = getenv("HOME"); char directory[PATH_MAX];
    if (data_home && *data_home) snprintf(directory, sizeof directory, "%s/notemd/versions/%s", data_home, state.active->id);
    else snprintf(directory, sizeof directory, "%s/.local/share/notemd/versions/%s", home ? home : ".", state.active->id);
    DIR *versions = opendir(directory); unsigned count = 0;
    if (versions) {
        char names[512][256]; struct dirent *entry;
        while (count < 512 && (entry = readdir(versions))) {
            if (entry->d_name[0] == '.') continue;
            snprintf(names[count++], sizeof names[count], "%s", entry->d_name);
        }
        qsort(names, count, sizeof names[0], newest_version_first);
        for (unsigned index = 0; index < count; index++) {
            RestoreData *preview_data = calloc(1, sizeof *preview_data);
            RestoreData *restore_data = calloc(1, sizeof *restore_data);
            RestoreData *delete_data = calloc(1, sizeof *delete_data);
            snprintf(preview_data->path, sizeof preview_data->path, "%s/%s", directory, names[index]);
            snprintf(restore_data->path, sizeof restore_data->path, "%s/%s", directory, names[index]);
            snprintf(delete_data->path, sizeof delete_data->path, "%s/%s", directory, names[index]);
            preview_data->preview_buffer = preview_buffer;
            long long stamp = atoll(names[index]); time_t when = (time_t)stamp; struct tm local;
            localtime_r(&when, &local); char label[128];
            strftime(label, sizeof label, "%d/%m/%Y  %H:%M:%S", &local);
            Ptr row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 7); gtk_widget_add_css_class(row, "history-row");
            Ptr date = navigation_button("document-open-recent-symbolic", label);
            gtk_widget_add_css_class(date, "history-date"); gtk_widget_set_hexpand(date, true);
            Ptr restore = icon_button("document-revert-symbolic", tr("Restaurar esta versão"));
            Ptr remove = icon_button("user-trash-symbolic", tr("Apagar esta versão"));
            restore_data->row = delete_data->row = row; restore_data->parent = delete_data->parent = list;
            preview_data->row = row; preview_data->parent = list;
            g_signal_connect_data(date, "clicked", (void *)preview_version, preview_data, (void *)free_signal_data, 0);
            g_signal_connect_data(restore, "clicked", (void *)restore_version, restore_data, (void *)free_signal_data, 0);
            g_signal_connect_data(remove, "clicked", (void *)delete_version, delete_data, (void *)free_signal_data, 0);
            gtk_box_append(row, date); gtk_box_append(row, restore); gtk_box_append(row, remove); gtk_box_append(list, row);
            if (index == 0) {
                char *content = read_file(preview_data->path); render_markdown(preview_buffer, content); free(content);
                gtk_widget_add_css_class(row, "history-current");
            }
        }
        closedir(versions);
    }
    if (!count) {
        Ptr empty = gtk_label_new(tr("Ainda não existem versões anteriores."));
        gtk_widget_add_css_class(empty, "empty-message"); margins(empty, 12, 16);
        gtk_box_append(list, empty); render_markdown(preview_buffer, tr("Crie uma versão editando e guardando a nota."));
    }
    gtk_box_append(box, body); gtk_window_set_child(window, box); gtk_window_present(window);
}

static int utf8_offset(const char *start, const char *position) {
    int offset = 0;
    for (const unsigned char *p = (const unsigned char *)start; (const char *)p < position; p++)
        if ((*p & 0xc0) != 0x80) offset++;
    return offset;
}

static void find_next(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    const char *needle = gtk_editable_get_text(state.find_entry);
    if (!needle[0]) return;
    char *source = editor_text(); size_t length = strlen(source);
    if (state.find_offset > length) state.find_offset = 0;
    char *match = strcasestr(source + state.find_offset, needle);
    if (!match && state.find_offset) { state.find_offset = 0; match = strcasestr(source, needle); }
    if (match) {
        int start_offset = utf8_offset(source, match);
        int end_offset = start_offset + utf8_offset(match, match + strlen(needle));
        TextIter start, end; gtk_text_buffer_get_iter_at_offset(state.editor_buffer, &start, start_offset);
        gtk_text_buffer_get_iter_at_offset(state.editor_buffer, &end, end_offset);
        gtk_text_buffer_select_range(state.editor_buffer, &start, &end);
        gtk_text_view_scroll_to_iter(state.editor, &start, 0.15, false, 0, 0);
        state.find_offset = (size_t)(match - source) + strlen(needle);
        gtk_label_set_text(state.status, tr("Correspondência encontrada"));
    } else gtk_label_set_text(state.status, tr("Sem mais correspondências"));
    g_free(source);
}

static void replace_current(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    const char *needle = gtk_editable_get_text(state.find_entry);
    const char *replacement = gtk_editable_get_text(state.replace_entry);
    if (!needle[0]) return;
    TextIter start, end;
    if (!gtk_text_buffer_get_selection_bounds(state.editor_buffer, &start, &end)) { find_next(NULL, NULL); return; }
    char *selected = gtk_text_buffer_get_text(state.editor_buffer, &start, &end, true);
    if (strcasecmp(selected, needle) != 0) { g_free(selected); find_next(NULL, NULL); return; }
    g_free(selected); gtk_text_buffer_delete(state.editor_buffer, &start, &end);
    gtk_text_buffer_insert(state.editor_buffer, &start, replacement, -1);
    state.find_offset = 0; gtk_label_set_text(state.status, tr("Correspondência substituída")); find_next(NULL, NULL);
}

static void replace_all(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    const char *needle = gtk_editable_get_text(state.find_entry);
    const char *replacement = gtk_editable_get_text(state.replace_entry);
    if (!needle[0]) return;
    char *source = editor_text(); size_t count = 0;
    for (char *p = source; (p = strcasestr(p, needle)); p += strlen(needle)) count++;
    if (!count) { gtk_label_set_text(state.status, tr("Texto não encontrado")); g_free(source); return; }
    size_t source_length = strlen(source), needle_length = strlen(needle), replacement_length = strlen(replacement);
    size_t result_size = source_length + 1;
    if (replacement_length > needle_length) result_size += count * (replacement_length - needle_length);
    char *result = calloc(result_size, 1), *output = result, *cursor = source, *match;
    while ((match = strcasestr(cursor, needle))) {
        size_t prefix = (size_t)(match - cursor); memcpy(output, cursor, prefix); output += prefix;
        memcpy(output, replacement, replacement_length); output += replacement_length; cursor = match + needle_length;
    }
    strcpy(output, cursor); gtk_text_buffer_set_text(state.editor_buffer, result, -1);
    char message[128]; snprintf(message, sizeof message, "%zu correspondências substituídas", count);
    gtk_label_set_text(state.status, message); free(result); g_free(source); state.find_offset = 0;
}

static gboolean find_closed(Ptr window, Ptr unused) {
    (void)window; (void)unused; state.find_window = NULL; return false;
}

static void close_find(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    if (state.find_window) { Ptr window = state.find_window; state.find_window = NULL; gtk_window_destroy(window); }
}

static void show_find(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    if (state.find_window) { gtk_window_present(state.find_window); return; }
    state.find_window = gtk_window_new(); gtk_window_set_title(state.find_window, tr("Pesquisar e substituir"));
    g_signal_connect_data(state.find_window, "close-request", (void *)find_closed, NULL, NULL, 0);
    gtk_window_set_default_size(state.find_window, 460, 180); gtk_window_set_transient_for(state.find_window, state.window);
    Ptr box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8); margins(box, 16, 16);
    state.find_entry = gtk_entry_new(); gtk_entry_set_placeholder_text(state.find_entry, tr("Pesquisar"));
    state.replace_entry = gtk_entry_new(); gtk_entry_set_placeholder_text(state.replace_entry, tr("Substituir por"));
    Ptr actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    Ptr next = icon_button("go-down-symbolic", tr("Correspondência seguinte"));
    Ptr replace = icon_button("edit-redo-symbolic", tr("Substituir correspondência atual"));
    Ptr all = icon_button("edit-select-all-symbolic", tr("Substituir todas as correspondências"));
    Ptr spacer = gtk_label_new(""); gtk_widget_set_hexpand(spacer, true);
    Ptr close = icon_button("window-close-symbolic", tr("Fechar pesquisa"));
    g_signal_connect_data(next, "clicked", (void *)find_next, NULL, NULL, 0);
    g_signal_connect_data(replace, "clicked", (void *)replace_current, NULL, NULL, 0);
    g_signal_connect_data(all, "clicked", (void *)replace_all, NULL, NULL, 0);
    g_signal_connect_data(close, "clicked", (void *)close_find, NULL, NULL, 0);
    g_signal_connect_data(state.find_entry, "activate", (void *)find_next, NULL, NULL, 0);
    gtk_box_append(actions, next); gtk_box_append(actions, replace); gtk_box_append(actions, all);
    gtk_box_append(actions, spacer); gtk_box_append(actions, close);
    gtk_box_append(box, state.find_entry); gtk_box_append(box, state.replace_entry); gtk_box_append(box, actions);
    gtk_window_set_child(state.find_window, box); gtk_window_present(state.find_window); state.find_offset = 0;
}

static const char *block_kind(const char *text) {
    if (text[0] == '#') return tr("Título");
    if (strncmp(text, "- [", 3) == 0) return tr("Tarefa");
    if (strncmp(text, "- ", 2) == 0 || strncmp(text, "* ", 2) == 0) return tr("Lista");
    if (isdigit((unsigned char)text[0])) { const char *dot = strchr(text, '.'); if (dot && dot[1] == ' ') return tr("Lista numerada"); }
    if (strncmp(text, "> ", 2) == 0) return tr("Citação");
    if (strncmp(text, "```", 3) == 0) return tr("Código");
    if (text[0] == '|') return tr("Tabela");
    if (strncmp(text, "---", 3) == 0) return tr("Separador");
    return tr("Parágrafo");
}

static bool visual_special_line(const char *line) {
    if (!*line) return true;
    if (*line == '#' || *line == '|' || strncmp(line, "```", 3) == 0 || strncmp(line, "> ", 2) == 0 ||
        strncmp(line, "- ", 2) == 0 || strncmp(line, "* ", 2) == 0 || strncmp(line, "![", 2) == 0 ||
        strcmp(line, "---") == 0 || strcmp(line, "***") == 0) return true;
    if (*line >= '0' && *line <= '9') { const char *dot = strchr(line, '.'); if (dot && dot[1] == ' ') return true; }
    return false;
}

static size_t visual_block_length(const char *cursor) {
    const char *line_end = strchr(cursor, '\n'); if (!line_end) return strlen(cursor);
    size_t first_length = (size_t)(line_end - cursor); char first[4096];
    if (first_length >= sizeof first) first_length = sizeof first - 1;
    memcpy(first, cursor, first_length); first[first_length] = '\0';
    if (strncmp(first, "```", 3) == 0) {
        const char *scan = line_end + 1;
        while (*scan) {
            const char *end = strchr(scan, '\n'); if (!end) end = scan + strlen(scan);
            if (end - scan >= 3 && strncmp(scan, "```", 3) == 0) return (size_t)(end - cursor);
            if (!*end) return strlen(cursor);
            scan = end + 1;
        }
    }
    if (first[0] == '|') {
        const char *scan = line_end + 1, *end = strchr(scan, '\n'); if (!end) end = scan + strlen(scan);
        if (strstr(scan, "---") && strchr(scan, '|')) {
            const char *last = end;
            while (*last) { const char *next = last + 1; if (*next != '|') break; last = strchr(next, '\n'); if (!last) return strlen(cursor); }
            return (size_t)(last - cursor);
        }
    }
    if (visual_special_line(first)) {
        if (strncmp(first, "> ", 2) == 0) {
            const char *end = line_end;
            while (*end && strncmp(end + 1, "> ", 2) == 0) { end = strchr(end + 1, '\n'); if (!end) return strlen(cursor); }
            return (size_t)(end - cursor);
        }
        return first_length;
    }
    const char *end = line_end;
    while (*end) {
        const char *next = end + 1, *next_end = strchr(next, '\n'); if (!next_end) next_end = next + strlen(next);
        size_t length = (size_t)(next_end - next); char line[4096]; if (length >= sizeof line) length = sizeof line - 1;
        memcpy(line, next, length); line[length] = '\0'; if (!*line || visual_special_line(line)) break;
        end = next_end;
    }
    return (size_t)(end - cursor);
}

static char *visual_block_markdown(size_t index) {
    char *content = buffer_text(state.block_buffers[index]);
    if (state.block_is_heading[index]) {
        guint level = state.block_headings[index] ? gtk_drop_down_get_selected(state.block_headings[index]) + 1 : 1;
        size_t size = level + strlen(content) + 2; char *markdown = calloc(size, 1);
        memset(markdown, '#', level); markdown[level] = ' '; strcpy(markdown + level + 1, content); g_free(content); return markdown;
    }
    if (state.block_is_task[index]) {
        bool checked = state.block_checks[index] && gtk_check_button_get_active(state.block_checks[index]);
        size_t size = strlen(content) + 7; char *markdown = calloc(size, 1);
        snprintf(markdown, size, "- [%c] %s", checked ? 'x' : ' ', content); g_free(content); return markdown;
    }
    if (state.block_is_code[index]) {
        const char *language = state.block_languages[index] ? gtk_editable_get_text(state.block_languages[index]) : "";
        size_t size = strlen(language) + strlen(content) + 9; char *markdown = calloc(size, 1);
        snprintf(markdown, size, "```%s\n%s\n```", language, content); g_free(content); return markdown;
    }
    if (state.block_is_image[index]) {
        const char *alt = state.block_image_alt[index] ? gtk_editable_get_text(state.block_image_alt[index]) : "";
        const char *location = state.block_image_location[index] ? gtk_editable_get_text(state.block_image_location[index]) : "";
        size_t size = strlen(alt) + strlen(location) + 7; char *markdown = calloc(size, 1);
        snprintf(markdown, size, "![%s](%s)", alt, location); g_free(content); return markdown;
    }
    if (state.block_is_divider[index]) { g_free(content); return strdup("---"); }
    if (state.block_is_bullet[index] || state.block_is_numbered[index]) {
        char prefix[32]; snprintf(prefix, sizeof prefix, state.block_is_bullet[index] ? "- " : "%d. ",
            state.block_numbers[index] > 0 ? state.block_numbers[index] : 1);
        size_t size = strlen(prefix) + strlen(content) + 1; char *markdown = calloc(size, 1);
        snprintf(markdown, size, "%s%s", prefix, content); g_free(content); return markdown;
    }
    if (state.block_is_quote[index]) {
        StringBuilder markdown = {0}; const char *line = content;
        while (true) {
            const char *end = strchr(line, '\n'); if (!end) end = line + strlen(line);
            if (markdown.length) builder_append(&markdown, "\n");
            builder_append(&markdown, "> ");
            builder_append_n(&markdown, line, (size_t)(end - line)); if (!*end) break; line = end + 1;
        }
        g_free(content); return markdown.value ? markdown.value : strdup("> ");
    }
    char *copy = strdup(content); g_free(content); return copy;
}

static void task_toggled(Ptr check, Ptr user_data) {
    (void)check; size_t index = (size_t)user_data; if (index >= state.block_count) return;
    blocks_changed(state.block_buffers[index], NULL);
}

static void heading_level_changed(Ptr dropdown, Ptr unused, Ptr user_data) {
    (void)dropdown; (void)unused; size_t index = (size_t)user_data; if (index >= state.block_count) return;
    blocks_changed(state.block_buffers[index], NULL);
}

static void code_language_changed(Ptr entry, Ptr user_data) {
    (void)entry; size_t index = (size_t)user_data; if (index >= state.block_count) return;
    blocks_changed(state.block_buffers[index], NULL);
}

static void image_field_changed(Ptr entry, Ptr user_data) {
    (void)entry; size_t index = (size_t)user_data; if (index >= state.block_count) return;
    blocks_changed(state.block_buffers[index], NULL);
}

static gboolean refresh_visual_editor(Ptr unused) {
    (void)unused; rebuild_block_editor(); return false;
}

static void table_action(Ptr button, Ptr user_data) {
    (void)button; size_t encoded = (size_t)user_data, index = encoded / 2; bool add_column = (encoded % 2) != 0;
    if (index >= state.block_count) return;
    char *table = buffer_text(state.block_buffers[index]); StringBuilder result = {0};
    if (!add_column) {
        builder_append(&result, table); size_t columns = 0;
        const char *first_end = strchr(table, '\n'); if (!first_end) first_end = table + strlen(table);
        for (const char *p = table; p < first_end; p++) if (*p == '|') columns++;
        if (columns > 1) columns--;
        builder_append(&result, "\n|"); for (size_t column = 0; column < columns; column++) builder_append(&result, "  |");
    } else {
        char *copy = strdup(table), *save = NULL; bool first = true;
        for (char *line = strtok_r(copy, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
            if (!first) builder_append(&result, "\n");
            first = false;
            size_t length = strlen(line); while (length && line[length - 1] == ' ') length--;
            bool trailing_pipe = length && line[length - 1] == '|';
            if (trailing_pipe) builder_append_n(&result, line, length - 1); else builder_append_n(&result, line, length);
            builder_append(&result, strstr(line, "---") ? " --- |" : "  |");
        }
        free(copy);
    }
    gtk_text_buffer_set_text(state.block_buffers[index], result.value ? result.value : table, -1);
    g_timeout_add(1, (void *)refresh_visual_editor, NULL);
    free(result.value); g_free(table);
}

static void append_table_cell(StringBuilder *builder, const char *value) {
    builder_append(builder, " ");
    for (const char *cursor = value ? value : ""; *cursor; cursor++) {
        if (*cursor == '|') builder_append(builder, "\\|");
        else builder_append_n(builder, cursor, 1);
    }
    builder_append(builder, " |");
}

static void table_cell_changed(Ptr entry, Ptr user_data) {
    (void)entry; TableEditor *table = user_data;
    if (!table || table->syncing || table->block_index >= state.block_count) return;
    StringBuilder markdown = {0};
    for (size_t row = 0; row < table->rows; row++) {
        if (row) builder_append(&markdown, "\n");
        builder_append(&markdown, "|");
        for (size_t column = 0; column < table->columns; column++)
            append_table_cell(&markdown, gtk_editable_get_text(table->cells[row][column]));
        if (row == 0) {
            builder_append(&markdown, "\n|");
            for (size_t column = 0; column < table->columns; column++) builder_append(&markdown, " --- |");
        }
    }
    table->syncing = true;
    gtk_text_buffer_set_text(state.block_buffers[table->block_index], markdown.value ? markdown.value : "", -1);
    table->syncing = false; free(markdown.value);
}

static size_t table_column_count(const char *line) {
    size_t pipes = 0; for (const char *cursor = line; *cursor; cursor++) if (*cursor == '|' && (cursor == line || cursor[-1] != '\\')) pipes++;
    return pipes > 1 ? pipes - 1 : 1;
}

static void table_cell_text(const char *line, size_t column, char output[512]) {
    const char *cursor = line; if (*cursor == '|') cursor++;
    for (size_t current = 0; current < column; current++) {
        const char *pipe = strchr(cursor, '|'); if (!pipe) { output[0] = '\0'; return; } cursor = pipe + 1;
    }
    const char *end = cursor; while (*end && *end != '|') end++;
    while (cursor < end && isspace((unsigned char)*cursor)) cursor++;
    while (end > cursor && isspace((unsigned char)end[-1])) end--;
    size_t length = (size_t)(end - cursor); if (length >= 512) length = 511;
    memcpy(output, cursor, length); output[length] = '\0';
}

static Ptr table_visual_editor(const char *markdown, size_t block_index) {
    TableEditor *table = calloc(1, sizeof *table); table->block_index = block_index;
    char *copy = strdup(markdown), *save = NULL; char *lines[33] = {0}; size_t line_count = 0;
    for (char *line = strtok_r(copy, "\n", &save); line && line_count < 33; line = strtok_r(NULL, "\n", &save))
        lines[line_count++] = line;
    table->columns = line_count ? table_column_count(lines[0]) : 2;
    if (table->columns > 16) table->columns = 16;
    Ptr grid = gtk_grid_new(); gtk_grid_set_row_spacing(grid, 5); gtk_grid_set_column_spacing(grid, 5);
    gtk_widget_add_css_class(grid, "table-grid");
    for (size_t source_row = 0; source_row < line_count && table->rows < 32; source_row++) {
        if (source_row == 1 && strstr(lines[source_row], "---")) continue;
        size_t row = table->rows++;
        for (size_t column = 0; column < table->columns; column++) {
            char value[512]; table_cell_text(lines[source_row], column, value);
            Ptr cell = gtk_entry_new(); gtk_editable_set_text(cell, value); gtk_entry_set_placeholder_text(cell, tr("Conteúdo"));
            gtk_widget_set_hexpand(cell, true); gtk_widget_add_css_class(cell, row == 0 ? "table-header-cell" : "table-cell");
            table->cells[row][column] = cell; gtk_grid_attach(grid, cell, (int)column, (int)row, 1, 1);
            g_signal_connect_data(cell, "changed", (void *)table_cell_changed, table, NULL, 0);
        }
    }
    if (!table->rows) table->rows = 1;
    state.block_tables[block_index] = table; free(copy); return grid;
}

static Ptr visual_image_widget(const char *block) {
    const char *alt, *location; size_t alt_length, location_length;
    if (!state.active || !markdown_image(block, &alt, &alt_length, &location, &location_length)) return NULL;
    char relative[PATH_MAX], path[PATH_MAX]; snprintf(relative, sizeof relative, "%.*s", (int)location_length, location);
    if (relative[0] == '/') snprintf(path, sizeof path, "%s", relative);
    else snprintf(path, sizeof path, "%s/%s", state.active->directory, relative);
    if (access(path, R_OK) != 0) return NULL;
    Ptr picture = gtk_picture_new_for_filename(path); gtk_picture_set_content_fit(picture, 3);
    gtk_widget_set_size_request(picture, -1, 260); gtk_widget_add_css_class(picture, "visual-image"); return picture;
}

static void blocks_changed(Ptr buffer, Ptr unused) {
    (void)buffer; (void)unused; size_t size = 1;
    for (size_t index = 0; index < state.block_count; index++) {
        char *text = visual_block_markdown(index); size += strlen(text) + 2; free(text);
    }
    char *markdown = calloc(size, 1); size_t used = 0;
    for (size_t index = 0; index < state.block_count; index++) {
        char *text = visual_block_markdown(index);
        used += (size_t)snprintf(markdown + used, size - used, "%s%s", index ? "\n\n" : "", text); free(text);
    }
    state.block_syncing = true;
    gtk_text_buffer_set_text(state.editor_buffer, markdown, -1);
    state.block_syncing = false;
    free(markdown);
}

static void block_action(Ptr button, Ptr user_data) {
    (void)button;
    size_t encoded = (size_t)user_data, target = encoded / 2;
    bool duplicate = (encoded % 2) == 0;
    if (target >= state.block_count) return;
    size_t size = 1;
    for (size_t index = 0; index < state.block_count; index++) {
        char *text = visual_block_markdown(index);
        size += strlen(text) * (duplicate && index == target ? 2 : 1) + 4; free(text);
    }
    char *markdown = calloc(size, 1); size_t used = 0, written = 0;
    for (size_t index = 0; index < state.block_count; index++) {
        if (!duplicate && index == target) continue;
        char *text = visual_block_markdown(index);
        used += (size_t)snprintf(markdown + used, size - used, "%s%s", written++ ? "\n\n" : "", text);
        if (duplicate && index == target)
            used += (size_t)snprintf(markdown + used, size - used, "\n\n%s", text);
        free(text);
    }
    gtk_text_buffer_set_text(state.editor_buffer, markdown, -1); free(markdown);
}

static char *quote_visual_text(const char *markdown) {
    StringBuilder content = {0}; const char *line = markdown;
    while (true) {
        const char *end = strchr(line, '\n'); if (!end) end = line + strlen(line);
        const char *start = strncmp(line, "> ", 2) == 0 ? line + 2 : line;
        if (content.length) builder_append(&content, "\n");
        builder_append_n(&content, start, (size_t)(end - start));
        if (!*end) break;
        line = end + 1;
    }
    return content.value ? content.value : strdup("");
}

static char *code_visual_text(const char *markdown, char language[64]) {
    language[0] = '\0'; const char *first_end = strchr(markdown, '\n');
    if (!first_end) return strdup("");
    const char *language_start = markdown + 3; while (language_start < first_end && isspace((unsigned char)*language_start)) language_start++;
    const char *language_end = first_end; while (language_end > language_start && isspace((unsigned char)language_end[-1])) language_end--;
    snprintf(language, 64, "%.*s", (int)(language_end - language_start), language_start);
    const char *content = first_end + 1, *closing = strstr(content, "\n```");
    if (!closing) closing = content + strlen(content);
    size_t length = (size_t)(closing - content); char *result = calloc(length + 1, 1);
    memcpy(result, content, length); return result;
}

static void rebuild_block_editor(void) {
    if (!state.block_box) return;
    while (gtk_widget_get_first_child(state.block_box))
        gtk_box_remove(state.block_box, gtk_widget_get_first_child(state.block_box));
    for (size_t index = 0; index < 256; index++) { free(state.block_tables[index]); state.block_tables[index] = NULL; }
    memset(state.block_checks, 0, sizeof state.block_checks);
    memset(state.block_headings, 0, sizeof state.block_headings);
    memset(state.block_is_heading, 0, sizeof state.block_is_heading);
    memset(state.block_is_task, 0, sizeof state.block_is_task);
    memset(state.block_is_bullet, 0, sizeof state.block_is_bullet);
    memset(state.block_is_numbered, 0, sizeof state.block_is_numbered);
    memset(state.block_is_quote, 0, sizeof state.block_is_quote);
    memset(state.block_is_code, 0, sizeof state.block_is_code);
    memset(state.block_languages, 0, sizeof state.block_languages);
    memset(state.block_is_image, 0, sizeof state.block_is_image);
    memset(state.block_image_alt, 0, sizeof state.block_image_alt);
    memset(state.block_image_location, 0, sizeof state.block_image_location);
    memset(state.block_is_divider, 0, sizeof state.block_is_divider);
    memset(state.block_numbers, 0, sizeof state.block_numbers);
    state.visual_image_count = 0;
    state.block_count = 0; char *source = editor_text(), *cursor = source;
    do {
        while (*cursor == '\n') cursor++;
        size_t length = visual_block_length(cursor); char *end = cursor + length;
        char *block = calloc(length + 1, 1); memcpy(block, cursor, length);
        Ptr card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3); gtk_widget_add_css_class(card, "block-card"); margins(card, 5, 4);
        Ptr header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        Ptr label = gtk_label_new(block_kind(block)); gtk_label_set_xalign(label, 0.0f); gtk_widget_set_hexpand(label, true);
        gtk_widget_add_css_class(label, "block-kind"); gtk_box_append(header, label);
        size_t block_index = state.block_count;
        Ptr task = NULL, heading_dropdown = NULL, language_entry = NULL, image_alt_entry = NULL, image_location_entry = NULL;
        char code_language[64] = ""; char *code_content = NULL;
        if (block[0] == '#') {
            state.block_is_heading[block_index] = true;
            static const char *levels[] = {"H1", "H2", "H3", "H4", "H5", "H6", NULL};
            guint level = 0; while (level < 6 && block[level] == '#') level++;
            heading_dropdown = gtk_drop_down_new_from_strings(levels); gtk_drop_down_set_selected(heading_dropdown, level ? level - 1 : 0);
            gtk_widget_set_tooltip_text(heading_dropdown, tr("Título")); gtk_box_append(header, heading_dropdown);
        }
        if (block[0] == '|') {
            Ptr add_row = icon_button("list-add-symbolic", tr("Adicionar linha"));
            Ptr add_column = icon_button("insert-table-symbolic", tr("Adicionar coluna"));
            g_signal_connect_data(add_row, "clicked", (void *)table_action, (Ptr)(block_index * 2), NULL, 0);
            g_signal_connect_data(add_column, "clicked", (void *)table_action, (Ptr)(block_index * 2 + 1), NULL, 0);
            gtk_box_append(header, add_row); gtk_box_append(header, add_column);
        }
        if (strncmp(block, "- [ ] ", 6) == 0 || strncmp(block, "- [x] ", 6) == 0 || strncmp(block, "- [X] ", 6) == 0) {
            state.block_is_task[block_index] = true;
            task = gtk_check_button_new_with_label(""); gtk_widget_set_tooltip_text(task, tr("Marcar tarefa como concluída"));
            gtk_check_button_set_active(task, block[3] == 'x' || block[3] == 'X');
        }
        if (!state.block_is_task[block_index] && (strncmp(block, "- ", 2) == 0 || strncmp(block, "* ", 2) == 0))
            state.block_is_bullet[block_index] = true;
        if (isdigit((unsigned char)block[0])) {
            const char *dot = strchr(block, '.'); if (dot && dot[1] == ' ') {
                state.block_is_numbered[block_index] = true; state.block_numbers[block_index] = atoi(block);
            }
        }
        if (strncmp(block, "> ", 2) == 0) state.block_is_quote[block_index] = true;
        if (strncmp(block, "```", 3) == 0) {
            state.block_is_code[block_index] = true; code_content = code_visual_text(block, code_language);
            language_entry = gtk_entry_new(); gtk_entry_set_placeholder_text(language_entry, tr("Linguagem"));
            gtk_editable_set_text(language_entry, code_language); gtk_widget_add_css_class(language_entry, "code-language");
            gtk_widget_set_tooltip_text(language_entry, tr("Linguagem")); gtk_box_append(header, language_entry);
        }
        const char *image_alt = NULL, *image_location = NULL; size_t image_alt_length = 0, image_location_length = 0;
        if (markdown_image(block, &image_alt, &image_alt_length, &image_location, &image_location_length)) {
            state.block_is_image[block_index] = true;
            char alt_value[512], location_value[PATH_MAX];
            snprintf(alt_value, sizeof alt_value, "%.*s", (int)image_alt_length, image_alt);
            snprintf(location_value, sizeof location_value, "%.*s", (int)image_location_length, image_location);
            image_alt_entry = gtk_entry_new(); gtk_entry_set_placeholder_text(image_alt_entry, tr("Descrição da imagem"));
            gtk_editable_set_text(image_alt_entry, alt_value); gtk_widget_add_css_class(image_alt_entry, "image-description");
            image_location_entry = gtk_entry_new(); gtk_entry_set_placeholder_text(image_location_entry, tr("Localização da imagem"));
            gtk_editable_set_text(image_location_entry, location_value); gtk_widget_add_css_class(image_location_entry, "image-location");
        }
        if (strcmp(block, "---") == 0 || strcmp(block, "***") == 0) state.block_is_divider[block_index] = true;
        Ptr duplicate = icon_button("edit-copy-symbolic", tr("Duplicar bloco"));
        Ptr remove = icon_button("user-trash-symbolic", tr("Remover bloco"));
        g_signal_connect_data(duplicate, "clicked", (void *)block_action, (Ptr)(block_index * 2), NULL, 0);
        g_signal_connect_data(remove, "clicked", (void *)block_action, (Ptr)(block_index * 2 + 1), NULL, 0);
        gtk_box_append(header, duplicate); gtk_box_append(header, remove);
        Ptr view = gtk_text_view_new(); gtk_text_view_set_editable(view, true);
        gtk_widget_add_css_class(view, "block-source");
        gtk_text_view_set_wrap_mode(view, GTK_WRAP_WORD_CHAR); gtk_text_view_set_monospace(view, true);
        gtk_widget_set_size_request(view, -1, 74); Ptr buffer = gtk_text_view_get_buffer(view);
        gtk_text_buffer_set_enable_undo(buffer, true);
        const char *visual_text = block;
        char *owned_visual_text = NULL;
        if (state.block_is_heading[block_index]) { while (*visual_text == '#') visual_text++; if (*visual_text == ' ') visual_text++; }
        else if (state.block_is_task[block_index] && strlen(visual_text) >= 6) visual_text += 6;
        else if (state.block_is_bullet[block_index] && strlen(visual_text) >= 2) visual_text += 2;
        else if (state.block_is_numbered[block_index]) { const char *dot = strchr(visual_text, '.'); if (dot && dot[1] == ' ') visual_text = dot + 2; }
        else if (state.block_is_quote[block_index]) { owned_visual_text = quote_visual_text(block); visual_text = owned_visual_text; }
        else if (state.block_is_code[block_index]) visual_text = code_content ? code_content : "";
        else if (state.block_is_divider[block_index]) visual_text = "";
        gtk_text_buffer_set_text(buffer, visual_text, -1); state.block_buffers[state.block_count++] = buffer;
        free(owned_visual_text);
        free(code_content);
        g_signal_connect_data(buffer, "changed", (void *)blocks_changed, NULL, NULL, 0);
        if (heading_dropdown) {
            state.block_headings[block_index] = heading_dropdown;
            g_signal_connect_data(heading_dropdown, "notify::selected", (void *)heading_level_changed, (Ptr)block_index, NULL, 0);
        }
        if (task) {
            state.block_checks[block_index] = task;
            g_signal_connect_data(task, "toggled", (void *)task_toggled, (Ptr)block_index, NULL, 0);
        }
        if (language_entry) {
            state.block_languages[block_index] = language_entry;
            g_signal_connect_data(language_entry, "changed", (void *)code_language_changed, (Ptr)block_index, NULL, 0);
        }
        if (image_alt_entry && image_location_entry) {
            state.block_image_alt[block_index] = image_alt_entry; state.block_image_location[block_index] = image_location_entry;
            g_signal_connect_data(image_alt_entry, "changed", (void *)image_field_changed, (Ptr)block_index, NULL, 0);
            g_signal_connect_data(image_location_entry, "changed", (void *)image_field_changed, (Ptr)block_index, NULL, 0);
        }
        gtk_box_append(card, header);
        if (block[0] == '|') gtk_box_append(card, table_visual_editor(block, block_index));
        else if (state.block_is_divider[block_index]) {
            Ptr divider = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL); gtk_widget_add_css_class(divider, "visual-divider");
            gtk_box_append(card, divider);
        }
        else {
            Ptr visual_image = visual_image_widget(block);
            if (visual_image) { gtk_box_append(card, visual_image); state.visual_image_count++; }
            if (state.block_is_image[block_index]) {
                Ptr image_fields = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5); gtk_widget_add_css_class(image_fields, "image-fields");
                gtk_box_append(image_fields, image_alt_entry); gtk_box_append(image_fields, image_location_entry);
                gtk_box_append(card, image_fields);
            } else if (state.block_is_task[block_index] || state.block_is_bullet[block_index] ||
                state.block_is_numbered[block_index] || state.block_is_quote[block_index]) {
                Ptr content_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 7); gtk_widget_add_css_class(content_row, "structured-line");
                if (task) gtk_box_append(content_row, task);
                else {
                    char marker[24] = "•";
                    if (state.block_is_numbered[block_index]) snprintf(marker, sizeof marker, "%d.", state.block_numbers[block_index]);
                    else if (state.block_is_quote[block_index]) snprintf(marker, sizeof marker, "▌");
                    Ptr marker_label = gtk_label_new(marker); gtk_widget_add_css_class(marker_label,
                        state.block_is_quote[block_index] ? "quote-marker" : "list-marker");
                    gtk_box_append(content_row, marker_label);
                }
                gtk_widget_set_hexpand(view, true); gtk_box_append(content_row, view); gtk_box_append(card, content_row);
            } else gtk_box_append(card, view);
        }
        gtk_box_append(state.block_box, card);
        free(block); cursor = end; while (*cursor == '\n') cursor++; if (!*cursor) break;
    } while (state.block_count < 256);
    g_free(source);
}

static void add_visual_block(Ptr button, Ptr user_data) {
    (void)button; const char *template = user_data; TextIter end; gtk_text_buffer_get_end_iter(state.editor_buffer, &end);
    char insertion[1024]; snprintf(insertion, sizeof insertion, "%s%s", state.block_count ? "\n\n" : "", template);
    gtk_text_buffer_insert(state.editor_buffer, &end, insertion, -1); rebuild_block_editor();
}

static void show_block_editor(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    if (!state.active) return;
    rebuild_block_editor();
    gtk_stack_set_visible_child_name(state.mode_stack, "visual");
    if (state.mode_editor) gtk_widget_remove_css_class(state.mode_editor, "active-mode");
    if (state.mode_split) gtk_widget_remove_css_class(state.mode_split, "active-mode");
    if (state.mode_visual) gtk_widget_add_css_class(state.mode_visual, "active-mode");
}

static void select_editor_mode(Ptr button, Ptr user_data) {
    (void)button; const char *mode = user_data;
    if (!mode) return;
    if (strcmp(mode, "visual") == 0) rebuild_block_editor();
    if (strcmp(mode, "split") == 0) {
        char *text = buffer_text(state.editor_buffer);
        state.split_origin = false; update_views(text); g_free(text);
    }
    gtk_stack_set_visible_child_name(state.mode_stack, mode);
    Ptr buttons[] = {state.mode_editor, state.mode_visual, state.mode_split};
    const char *names[] = {"editor", "visual", "split"};
    for (size_t index = 0; index < 3; index++) {
        if (!buttons[index]) continue;
        gtk_widget_remove_css_class(buttons[index], "active-mode");
        if (strcmp(mode, names[index]) == 0) gtk_widget_add_css_class(buttons[index], "active-mode");
    }
}

static void remove_note_from_model(Note *target) {
    Note **cursor = &state.notes;
    while (*cursor && *cursor != target) cursor = &(*cursor)->next;
    if (*cursor) *cursor = target->next;
    Note *next = state.notes;
    if (state.active == target) state.active = NULL;
    discard_note_draft(target); free(target); rebuild_sidebar();
    rebuild_tabs();
    save_settings();
    if (next) select_note(NULL, next);
    else {
        state.loading = true; gtk_editable_set_text(state.title, ""); gtk_editable_set_text(state.tags, "");
        gtk_text_buffer_set_text(state.editor_buffer, "", -1); update_views(""); state.loading = false;
        gtk_label_set_text(state.status, "Crie ou abra uma nota");
    }
}

static void delete_note_response(Ptr dialog, int response, Ptr user_data) {
    Note *target = user_data;
    if (response == GTK_RESPONSE_YES && target) {
        bool removed = target->external;
        if (!target->external) {
            Ptr file = g_file_new_for_path(target->directory);
            removed = g_file_trash(file, NULL, NULL); g_object_unref(file);
        }
        if (removed) remove_note_from_model(target);
        else gtk_label_set_text(state.status, "Não foi possível mover a nota para o Lixo");
    }
    gtk_window_destroy(dialog);
}

static void delete_active_note(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    if (!state.active) return;
    Ptr dialog = gtk_message_dialog_new(state.window, GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING,
        GTK_BUTTONS_YES_NO, state.active->external ? "Fechar este ficheiro?" : "Remover esta nota?");
    gtk_message_dialog_format_secondary_text(dialog, "%s", state.active->external
        ? "O ficheiro original não será apagado."
        : "A nota e as suas imagens serão movidas para o Lixo.");
    g_signal_connect_data(dialog, "response", (void *)delete_note_response, state.active, NULL, 0);
    gtk_window_present(dialog);
}

static void choose_theme(Ptr button, Ptr user_data) {
    (void)button; apply_theme((const char *)user_data);
    gtk_label_set_text(state.status, "Tema aplicado");
}

static void autosave_changed(Ptr check, Ptr unused) {
    (void)unused; state.autosave = gtk_check_button_get_active(check); save_settings();
    gtk_label_set_text(state.status, state.autosave ? "Gravação automática ativada" : "Gravação automática desativada");
}

static void language_changed(Ptr button, Ptr user_data) {
    (void)button; snprintf(state.language, sizeof state.language, "%s", (const char *)user_data); save_settings();
    gtk_label_set_text(state.status, "Idioma guardado; será aplicado integralmente ao reiniciar");
}

static void note_color_changed(Ptr button, Ptr user_data) {
    (void)button; if (!state.active || state.active->external) return;
    snprintf(state.active->color, sizeof state.active->color, "%s", (const char *)user_data);
    save_active(NULL); rebuild_sidebar();
}

static void notebook_color_changed(Ptr button, Ptr user_data) {
    (void)button; if (!state.active_notebook || state.active_notebook->external) return;
    snprintf(state.active_notebook->color, sizeof state.active_notebook->color, "%s", (const char *)user_data);
    save_notebook_metadata(state.active_notebook); rebuild_sidebar();
}

static bool move_note_between_notebooks(Note *note, Notebook *destination) {
    if (!note || note->external || !destination || destination->external || note->notebook == destination) return false;
    char target[PATH_MAX]; snprintf(target, sizeof target, "%s/%s", destination->path, note->title);
    if (access(target, F_OK) == 0) { gtk_label_set_text(state.status, "Já existe uma nota com esse nome no destino"); return false; }
    Notebook *source = note->notebook;
    if (rename(note->directory, target) != 0) { gtk_label_set_text(state.status, "Não foi possível mover a nota"); return false; }
    snprintf(note->directory, sizeof note->directory, "%s", target);
    snprintf(note->markdown_path, sizeof note->markdown_path, "%s/note.md", target);
    note->notebook = destination; if (state.active == note) state.active_notebook = destination;
    save_notebook_metadata(source); save_notebook_metadata(destination); rebuild_sidebar();
    gtk_label_set_text(state.status, "Nota movida"); return true;
}

static void move_note_to_notebook(Ptr button, Ptr user_data) {
    (void)button; move_note_between_notebooks(state.active, user_data);
}

static Ptr note_drag_prepare(Ptr source, double x, double y, Ptr user_data) {
    (void)source; (void)x; (void)y; Note *note = user_data;
    if (!note || note->external || !note->id[0]) return NULL;
    return gdk_content_provider_new_typed(g_type_from_name("gchararray"), note->id);
}

static gboolean note_dropped_on_notebook(Ptr target, const void *value, double x, double y, Ptr user_data) {
    (void)target; (void)x; (void)y; const char *id = g_value_get_string(value); if (!id) return false;
    for (Note *note = state.notes; note; note = note->next)
        if (strcmp(note->id, id) == 0) return move_note_between_notebooks(note, user_data);
    return false;
}

static bool reorder_note_before(Note *source, Note *target) {
    if (!source || !target || source == target || source->external || target->external) return false;
    if (source->notebook != target->notebook && !move_note_between_notebooks(source, target->notebook)) return false;
    Note **remove = &state.notes; while (*remove && *remove != source) remove = &(*remove)->next;
    if (!*remove) return false;
    *remove = source->next;
    Note **place = &state.notes; while (*place && *place != target) place = &(*place)->next;
    if (!*place) return false;
    source->next = target; *place = source;
    save_notebook_metadata(target->notebook); rebuild_sidebar(); gtk_label_set_text(state.status, "Nota reordenada"); return true;
}

static gboolean note_dropped_on_note(Ptr target_widget, const void *value, double x, double y, Ptr user_data) {
    (void)target_widget; (void)x; (void)y; const char *id = g_value_get_string(value); if (!id) return false;
    Note *target = user_data;
    for (Note *source = state.notes; source; source = source->next)
        if (strcmp(source->id, id) == 0) return reorder_note_before(source, target);
    return false;
}

static void show_preferences(Ptr button, Ptr unused) {
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

typedef struct { Ptr window, entry; Notebook *notebook; } RenameNotebookData;

static void rename_notebook_apply(Ptr button, Ptr user_data) {
    (void)button; RenameNotebookData *data = user_data;
    char title[256]; safe_title(gtk_editable_get_text(data->entry), title);
    char destination[PATH_MAX]; snprintf(destination, sizeof destination, "%s/%s", state.root, title);
    size_t old_length = strlen(data->notebook->path);
    if (strcmp(destination, data->notebook->path) == 0 ||
        (access(destination, F_OK) != 0 && rename(data->notebook->path, destination) == 0)) {
        for (Note *note = state.notes; note; note = note->next) if (note->notebook == data->notebook) {
            char old_directory[PATH_MAX]; snprintf(old_directory, sizeof old_directory, "%s", note->directory);
            snprintf(note->directory, sizeof note->directory, "%s%s", destination, old_directory + old_length);
            snprintf(note->markdown_path, sizeof note->markdown_path, "%s/note.md", note->directory);
        }
        snprintf(data->notebook->path, sizeof data->notebook->path, "%s", destination);
        snprintf(data->notebook->title, sizeof data->notebook->title, "%s", title);
        rebuild_sidebar(); gtk_label_set_text(state.status, "Notebook renomeado");
        gtk_window_destroy(data->window); free(data);
    } else gtk_label_set_text(state.status, "Já existe um notebook com esse nome");
}

static void rename_selected_notebook(Ptr button, Ptr unused) {
    (void)button; (void)unused;
    if (!state.active_notebook || state.active_notebook->external) return;
    RenameNotebookData *data = calloc(1, sizeof *data); data->notebook = state.active_notebook;
    data->window = gtk_window_new(); gtk_window_set_title(data->window, "Renomear notebook");
    gtk_window_set_default_size(data->window, 420, 150); gtk_window_set_transient_for(data->window, state.window);
    gtk_window_set_modal(data->window, true); Ptr box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8); margins(box, 16, 16);
    data->entry = gtk_entry_new(); gtk_editable_set_text(data->entry, data->notebook->title);
    Ptr apply = gtk_button_new_with_label("Renomear"); gtk_widget_add_css_class(apply, "suggested-action");
    g_signal_connect_data(apply, "clicked", (void *)rename_notebook_apply, data, NULL, 0);
    gtk_box_append(box, data->entry); gtk_box_append(box, apply); gtk_window_set_child(data->window, box); gtk_window_present(data->window);
}

static int note_compare(Note *left, Note *right, bool recent) {
    if (!recent) return strcasecmp(left->title, right->title);
    struct stat a = {0}, b = {0}; stat(left->markdown_path, &a); stat(right->markdown_path, &b);
    if (a.st_mtime == b.st_mtime) return strcasecmp(left->title, right->title);
    return a.st_mtime > b.st_mtime ? -1 : 1;
}

static void sort_notes(bool recent) {
    Note *sorted = NULL;
    while (state.notes) {
        Note *moving = state.notes; state.notes = moving->next;
        Note **place = &sorted;
        while (*place && note_compare(*place, moving, recent) <= 0) place = &(*place)->next;
        moving->next = *place; *place = moving;
    }
    state.notes = sorted;
    for (Notebook *book = state.notebooks; book; book = book->next) save_notebook_metadata(book);
    rebuild_sidebar();
}

static void sort_title(Ptr button, Ptr unused) { (void)button; (void)unused; sort_notes(false); }
static void sort_recent(Ptr button, Ptr unused) { (void)button; (void)unused; sort_notes(true); }

static void reorder_active_note(int direction) {
    if (!state.active || state.active->external) return;
    Note *items[512]; size_t count = 0, current = 0;
    for (Note *note = state.notes; note && count < 512; note = note->next) if (note->notebook == state.active->notebook) {
        items[count] = note; if (note == state.active) current = count; note->order = (int)count++;
    }
    if (!count || (direction < 0 && current == 0) || (direction > 0 && current + 1 >= count)) return;
    size_t other = direction < 0 ? current - 1 : current + 1;
    int value = items[current]->order; items[current]->order = items[other]->order; items[other]->order = value;
    Note *sorted = NULL;
    while (state.notes) {
        Note *moving = state.notes; state.notes = moving->next; Note **place = &sorted;
        while (*place) {
            int books = strcmp((*place)->notebook->title, moving->notebook->title);
            if (books > 0 || (books == 0 && (*place)->order > moving->order)) break;
            place = &(*place)->next;
        }
        moving->next = *place; *place = moving;
    }
    state.notes = sorted; save_notebook_metadata(state.active->notebook); rebuild_sidebar();
}

static void move_note_up(Ptr button, Ptr unused) { (void)button; (void)unused; reorder_active_note(-1); }
static void move_note_down(Ptr button, Ptr unused) { (void)button; (void)unused; reorder_active_note(1); }

static void delete_notebook_response(Ptr dialog, int response, Ptr user_data) {
    Notebook *target = user_data;
    if (response == GTK_RESPONSE_YES && target) {
        bool removed = target->external;
        if (!target->external) {
            Ptr file = g_file_new_for_path(target->path); removed = g_file_trash(file, NULL, NULL); g_object_unref(file);
        }
        if (removed) {
            Note **note = &state.notes;
            while (*note) {
                if ((*note)->notebook == target) { Note *old = *note; *note = old->next; discard_note_draft(old); free(old); }
                else note = &(*note)->next;
            }
            Notebook **book = &state.notebooks;
            while (*book && *book != target) book = &(*book)->next;
            if (*book) *book = target->next;
            if (state.active_notebook == target) state.active_notebook = state.notebooks;
            state.active = NULL; free(target); rebuild_sidebar();
            if (state.notes) select_note(NULL, state.notes);
            else { state.loading = true; gtk_editable_set_text(state.title, ""); gtk_editable_set_text(state.tags, "");
                gtk_text_buffer_set_text(state.editor_buffer, "", -1); update_views(""); state.loading = false; }
        } else gtk_label_set_text(state.status, "Não foi possível mover o notebook para o Lixo");
    }
    gtk_window_destroy(dialog);
}

static void delete_selected_notebook(Ptr button, Ptr unused) {
    (void)button; (void)unused; if (!state.active_notebook) return;
    Ptr dialog = gtk_message_dialog_new(state.window, GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING,
        GTK_BUTTONS_YES_NO, state.active_notebook->external ? "Fechar ficheiros externos?" : "Remover este notebook?");
    gtk_message_dialog_format_secondary_text(dialog, "%s", state.active_notebook->external
        ? "Os ficheiros originais não serão apagados."
        : "Todas as notas e imagens serão movidas para o Lixo.");
    g_signal_connect_data(dialog, "response", (void *)delete_notebook_response, state.active_notebook, NULL, 0);
    gtk_window_present(dialog);
}

static Ptr context_action(const char *icon, const char *label, void *callback, Ptr data) {
    Ptr button = navigation_button(icon, label); gtk_widget_add_css_class(button, "context-action");
    g_signal_connect_data(button, "clicked", callback, data, NULL, 0); return button;
}

static Ptr color_actions(bool notebook) {
    static const char *names[] = {"Sem cor", "Vermelho", "Laranja", "Amarelo", "Verde", "Azul", "Roxo"};
    static const char *values[] = {"", "#d1242f", "#bc4c00", "#9a6700", "#1a7f37", "#0969da", "#8250df"};
    static const char *classes[] = {"color-none", "color-red", "color-orange", "color-yellow", "color-green", "color-blue", "color-purple"};
    Ptr row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3); gtk_widget_add_css_class(row, "color-palette");
    for (size_t index = 0; index < sizeof values / sizeof values[0]; index++) {
        Ptr choice = icon_button(index ? "media-record-symbolic" : "edit-clear-symbolic", names[index]);
        gtk_widget_add_css_class(choice, "color-swatch"); gtk_widget_add_css_class(choice, classes[index]);
        g_signal_connect_data(choice, "clicked", (void *)(notebook ? notebook_color_changed : note_color_changed),
            (Ptr)values[index], NULL, 0); gtk_box_append(row, choice);
    }
    return row;
}

static void library_menu_clicked(Ptr button, Ptr user_data) {
    (void)button; ContextTarget *context = user_data;
    if (!context || !context->target) return;
    Ptr anchor = context->widget;
    Ptr popover = gtk_popover_new(); Ptr box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(box, "context-menu");
    if (context->notebook) {
        Notebook *book = context->target; state.active_notebook = book;
        Ptr heading = gtk_label_new(book->title); gtk_label_set_xalign(heading, 0.0f);
        gtk_widget_add_css_class(heading, "context-title"); gtk_box_append(box, heading);
        if (!book->external) {
            gtk_box_append(box, context_action("document-new-symbolic", "Nova nota", (void *)create_note_in_notebook, book));
            gtk_box_append(box, context_action("document-properties-symbolic", "Criar a partir de template", (void *)show_templates, NULL));
            gtk_box_append(box, context_action("edit-rename-symbolic", "Renomear notebook", (void *)rename_selected_notebook, NULL));
            Ptr color_label = gtk_label_new("Cor do notebook"); gtk_label_set_xalign(color_label, 0.0f);
            gtk_widget_add_css_class(color_label, "context-section"); gtk_box_append(box, color_label);
            gtk_box_append(box, color_actions(true));
            gtk_box_append(box, context_action("view-sort-ascending-symbolic", "Ordenar por título", (void *)sort_title, NULL));
            gtk_box_append(box, context_action("document-open-recent-symbolic", "Mais recentes", (void *)sort_recent, NULL));
        }
        Ptr remove = context_action("user-trash-symbolic", book->external ? "Fechar ficheiros externos" : "Remover notebook",
            (void *)delete_selected_notebook, NULL); gtk_widget_add_css_class(remove, "destructive-action"); gtk_box_append(box, remove);
    } else {
        Note *note = context->target; select_note(NULL, note);
        Ptr heading = gtk_label_new(note_display_title(note)); gtk_label_set_xalign(heading, 0.0f);
        gtk_widget_add_css_class(heading, "context-title"); gtk_box_append(box, heading);
        if (!note->external) {
            Ptr color_label = gtk_label_new("Cor da nota"); gtk_label_set_xalign(color_label, 0.0f);
            gtk_widget_add_css_class(color_label, "context-section"); gtk_box_append(box, color_label);
            gtk_box_append(box, color_actions(false));
            gtk_box_append(box, context_action("go-up-symbolic", "Mover para cima", (void *)move_note_up, NULL));
            gtk_box_append(box, context_action("go-down-symbolic", "Mover para baixo", (void *)move_note_down, NULL));
            Ptr move_label = gtk_label_new("Mover para notebook"); gtk_label_set_xalign(move_label, 0.0f);
            gtk_widget_add_css_class(move_label, "context-section"); gtk_box_append(box, move_label);
            for (Notebook *book = state.notebooks; book; book = book->next)
                if (!book->external && book != note->notebook)
                    gtk_box_append(box, context_action("folder-symbolic", book->title, (void *)move_note_to_notebook, book));
        }
        Ptr remove = context_action("user-trash-symbolic", note->external ? "Fechar ficheiro" : "Remover nota",
            (void *)delete_active_note, NULL); gtk_widget_add_css_class(remove, "destructive-action"); gtk_box_append(box, remove);
    }
    margins(box, 10, 10); gtk_popover_set_child(popover, box); present_popover(popover, anchor);
}

static Ptr library_menu_button(Ptr target, bool notebook) {
    Ptr button = icon_button("view-more-symbolic", notebook ? "Ações do notebook" : "Ações da nota");
    gtk_widget_add_css_class(button, "item-menu");
    ContextTarget *context = calloc(1, sizeof *context); context->widget = button; context->target = target; context->notebook = notebook;
    g_signal_connect_data(button, "clicked", (void *)library_menu_clicked, context, (void *)free_signal_data, 0);
    state.context_menu_count++; return button;
}

static void library_context_pressed(Ptr gesture, int presses, double x, double y, Ptr user_data) {
    (void)gesture; (void)presses; (void)x; (void)y; library_menu_clicked(NULL, user_data);
}

static Ptr attach_library_context_menu(Ptr widget, Ptr target, bool notebook) {
    Ptr gesture = gtk_gesture_click_new(); gtk_gesture_single_set_button(gesture, 3);
    ContextTarget *context = calloc(1, sizeof *context);
    context->widget = widget; context->target = target; context->notebook = notebook;
    g_signal_connect_data(gesture, "pressed", (void *)library_context_pressed, context, (void *)free_signal_data, 0);
    gtk_widget_add_controller(widget, gesture);
    return gesture;
}

static void show_export_menu(Ptr button, Ptr unused) {
    (void)unused; Ptr popover = gtk_popover_new(); Ptr box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(box, "context-menu");
    Ptr heading = gtk_label_new("Exportar nota"); gtk_label_set_xalign(heading, 0.0f);
    gtk_widget_add_css_class(heading, "context-title"); gtk_box_append(box, heading);
    gtk_box_append(box, context_action("text-html-symbolic", "HTML", (void *)export_html, NULL));
    gtk_box_append(box, context_action("application-pdf-symbolic", "PDF", (void *)export_office, "pdf"));
    gtk_box_append(box, context_action("x-office-document-symbolic", "DOCX", (void *)export_office, "docx"));
    margins(box, 10, 10); gtk_popover_set_child(popover, box); present_popover(popover, button);
}

static char *editor_text(void) {
    TextIter start, end;
    gtk_text_buffer_get_bounds(state.editor_buffer, &start, &end);
    return gtk_text_buffer_get_text(state.editor_buffer, &start, &end, true);
}

static void preview_line(Ptr buffer, const char *text, const char *tag) {
    int start_offset = gtk_text_buffer_get_char_count(buffer);
    TextIter cursor; gtk_text_buffer_get_end_iter(buffer, &cursor);
    gtk_text_buffer_insert(buffer, &cursor, text, -1); gtk_text_buffer_insert(buffer, &cursor, "\n", 1);
    if (tag) {
        TextIter start, end; gtk_text_buffer_get_iter_at_offset(buffer, &start, start_offset);
        gtk_text_buffer_get_end_iter(buffer, &end); gtk_text_buffer_apply_tag_by_name(buffer, tag, &start, &end);
    }
}

static void preview_tagged_segment(Ptr buffer, const char *text, size_t length, const char *tag) {
    int start_offset = gtk_text_buffer_get_char_count(buffer); TextIter cursor;
    gtk_text_buffer_get_end_iter(buffer, &cursor); gtk_text_buffer_insert(buffer, &cursor, text, (int)length);
    if (tag) {
        TextIter start, end; gtk_text_buffer_get_iter_at_offset(buffer, &start, start_offset);
        gtk_text_buffer_get_end_iter(buffer, &end); gtk_text_buffer_apply_tag_by_name(buffer, tag, &start, &end);
    }
}

static void preview_inline_line(Ptr buffer, const char *text, const char *line_tag) {
    int line_start = gtk_text_buffer_get_char_count(buffer); const char *cursor = text;
    while (*cursor) {
        const char *close = NULL;
        if (strncmp(cursor, "<span style=\"color:#", 20) == 0) {
            const char *content = strchr(cursor + 20, '>'), *end = content ? strstr(content + 1, "</span>") : NULL;
            if (content && end && content - cursor >= 27) {
                char color[8]; memcpy(color, cursor + 19, 7); color[7] = '\0'; const char *tag = NULL;
                if (strcasecmp(color, "#d1242f") == 0) tag = "text-red";
                else if (strcasecmp(color, "#bc4c00") == 0) tag = "text-orange";
                else if (strcasecmp(color, "#9a6700") == 0) tag = "text-yellow";
                else if (strcasecmp(color, "#1a7f37") == 0) tag = "text-green";
                else if (strcasecmp(color, "#0969da") == 0) tag = "text-blue";
                else if (strcasecmp(color, "#8250df") == 0) tag = "text-purple";
                if (tag) { preview_tagged_segment(buffer, content + 1, (size_t)(end - content - 1), tag); cursor = end + 7; continue; }
            }
        }
        if (strncmp(cursor, "**", 2) == 0 && (close = strstr(cursor + 2, "**"))) {
            preview_tagged_segment(buffer, cursor + 2, (size_t)(close - cursor - 2), "strong"); cursor = close + 2; continue;
        }
        if (strncmp(cursor, "~~", 2) == 0 && (close = strstr(cursor + 2, "~~"))) {
            preview_tagged_segment(buffer, cursor + 2, (size_t)(close - cursor - 2), "strike"); cursor = close + 2; continue;
        }
        if (*cursor == '_' && (close = strchr(cursor + 1, '_'))) {
            preview_tagged_segment(buffer, cursor + 1, (size_t)(close - cursor - 1), "emphasis"); cursor = close + 1; continue;
        }
        if (*cursor == '`' && (close = strchr(cursor + 1, '`'))) {
            preview_tagged_segment(buffer, cursor + 1, (size_t)(close - cursor - 1), "inline-code"); cursor = close + 1; continue;
        }
        if (*cursor == '[' && cursor[1] != '[' && (close = strstr(cursor + 1, "]("))) {
            const char *end = strchr(close + 2, ')');
            if (end) { preview_tagged_segment(buffer, cursor + 1, (size_t)(close - cursor - 1), "link"); cursor = end + 1; continue; }
        }
        if (strncmp(cursor, "[[", 2) == 0 && (close = strstr(cursor + 2, "]]"))) {
            preview_tagged_segment(buffer, cursor + 2, (size_t)(close - cursor - 2), "link"); cursor = close + 2; continue;
        }
        const char *next = cursor + 1;
        while (*next && *next != '*' && *next != '~' && *next != '_' && *next != '`' && *next != '[' && *next != '<') next++;
        preview_tagged_segment(buffer, cursor, (size_t)(next - cursor), NULL); cursor = next;
    }
    preview_tagged_segment(buffer, "\n", 1, NULL);
    if (line_tag) {
        TextIter start, end; gtk_text_buffer_get_iter_at_offset(buffer, &start, line_start);
        gtk_text_buffer_get_end_iter(buffer, &end); gtk_text_buffer_apply_tag_by_name(buffer, line_tag, &start, &end);
    }
}

static void preview_table_line(Ptr buffer, const char *line, bool header) {
    char *copy = strdup(line), *cell = copy; while (*cell == ' ' || *cell == '|') cell++;
    size_t length = strlen(cell); while (length && (cell[length - 1] == ' ' || cell[length - 1] == '|')) cell[--length] = '\0';
    StringBuilder row = {0}; char *save = NULL; bool first = true;
    for (char *value = strtok_r(cell, "|", &save); value; value = strtok_r(NULL, "|", &save)) {
        while (*value == ' ') value++;
        size_t value_length = strlen(value);
        while (value_length && value[value_length - 1] == ' ') value[--value_length] = '\0';
        if (!first) builder_append(&row, "   │   ");
        builder_append(&row, value); first = false;
    }
    preview_inline_line(buffer, row.value ? row.value : "", header ? "table-header" : "table-row");
    free(row.value); free(copy);
}

static bool preview_image(Ptr buffer, const char *line) {
    if (!state.active || strncmp(line, "![", 2) != 0) return false;
    const char *open = strstr(line, "]("), *close = open ? strrchr(open + 2, ')') : NULL;
    if (!open || !close || close <= open + 2) return false;
    char relative[PATH_MAX], path[PATH_MAX];
    snprintf(relative, sizeof relative, "%.*s", (int)(close - open - 2), open + 2);
    if (relative[0] == '/') snprintf(path, sizeof path, "%s", relative);
    else snprintf(path, sizeof path, "%s/%s", state.active->directory, relative);
    if (access(path, R_OK) != 0) return false;
    Ptr view = buffer == state.preview_buffer ? state.preview_view :
        buffer == state.split_preview_buffer ? state.split_preview_view : NULL;
    if (!view) return false;
    TextIter end; gtk_text_buffer_get_end_iter(buffer, &end);
    Ptr anchor = gtk_text_buffer_create_child_anchor(buffer, &end);
    Ptr picture = gtk_picture_new_for_filename(path); gtk_picture_set_content_fit(picture, 3);
    gtk_widget_set_size_request(picture, 520, 280); gtk_text_view_add_child_at_anchor(view, picture, anchor);
    gtk_text_buffer_get_end_iter(buffer, &end); gtk_text_buffer_insert(buffer, &end, "\n", 1); return true;
}

static void render_markdown(Ptr buffer, const char *source) {
    gtk_text_buffer_set_text(buffer, "", -1); char *copy = strdup(source), *save = NULL; bool code = false, table = false;
    for (char *line = strtok_r(copy, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        if (!code && preview_image(buffer, line)) continue;
        if (!code && strncmp(line, "![", 2) == 0) {
            const char *alt, *location; size_t alt_length, location_length;
            if (markdown_image(line, &alt, &alt_length, &location, &location_length)) {
                char unavailable[1024]; snprintf(unavailable, sizeof unavailable, "▧  %.*s  ·  imagem indisponível", (int)alt_length, alt);
                preview_inline_line(buffer, unavailable, "quote"); continue;
            }
        }
        if (strncmp(line, "```", 3) == 0) { code = !code; continue; }
        if (code) preview_line(buffer, line, "code");
        else if (line[0] == '|' && strstr(line, "---")) { table = true; continue; }
        else if (line[0] == '|') { preview_table_line(buffer, line, !table); table = true; }
        else if (strncmp(line, "###### ", 7) == 0) preview_inline_line(buffer, line + 7, "h3");
        else if (strncmp(line, "##### ", 6) == 0) preview_inline_line(buffer, line + 6, "h3");
        else if (strncmp(line, "#### ", 5) == 0) preview_inline_line(buffer, line + 5, "h3");
        else if (strncmp(line, "### ", 4) == 0) preview_inline_line(buffer, line + 4, "h3");
        else if (strncmp(line, "## ", 3) == 0) preview_inline_line(buffer, line + 3, "h2");
        else if (strncmp(line, "# ", 2) == 0) preview_inline_line(buffer, line + 2, "h1");
        else if (strncmp(line, "> ", 2) == 0) preview_inline_line(buffer, line + 2, "quote");
        else if (strncmp(line, "- [ ] ", 6) == 0) { char value[2048]; snprintf(value, sizeof value, "☐  %s", line + 6); preview_inline_line(buffer, value, "list"); }
        else if (strncmp(line, "- [x] ", 6) == 0 || strncmp(line, "- [X] ", 6) == 0) {
            char value[2048]; snprintf(value, sizeof value, "☑  %s", line + 6); preview_inline_line(buffer, value, "done");
        } else if (strncmp(line, "- ", 2) == 0 || strncmp(line, "* ", 2) == 0) {
            char value[2048]; snprintf(value, sizeof value, "•  %s", line + 2); preview_inline_line(buffer, value, "list");
        } else if (line[0] >= '0' && line[0] <= '9' && strchr(line, '.') && strchr(line, '.')[1] == ' ')
            preview_inline_line(buffer, line, "list");
        else if (strcmp(line, "---") == 0 || strcmp(line, "***") == 0)
            preview_line(buffer, "────────────────────────────", "divider");
        else { table = false; preview_inline_line(buffer, line, NULL); }
    }
    free(copy);
}

static void update_views(const char *text) {
    bool previous_sync = state.view_syncing; state.view_syncing = true;
    if (state.line_buffer) {
        size_t lines = 1; for (const char *p = text; *p; p++) if (*p == '\n') lines++;
        size_t size = lines * 16 + 1, used = 0; char *numbers = calloc(size, 1);
        for (size_t line = 1; line <= lines; line++) used += (size_t)snprintf(numbers + used, size - used, "%zu\n", line);
        gtk_text_buffer_set_text(state.line_buffer, numbers, -1); free(numbers); apply_active_line_tag();
    }
    if (state.preview_buffer) render_markdown(state.preview_buffer, text);
    if (state.split_buffer && !state.split_origin) gtk_text_buffer_set_text(state.split_buffer, text, -1);
    if (state.split_preview_buffer) render_markdown(state.split_preview_buffer, text);
    rebuild_links(text);
    state.view_syncing = previous_sync;
}

static void split_content_changed(Ptr buffer, Ptr unused) {
    (void)unused;
    if (state.loading || state.view_syncing || !state.active) return;
    char *text = buffer_text(buffer); state.loading = true;
    gtk_text_buffer_set_text(state.editor_buffer, text, -1); state.loading = false;
    g_free(text); state.split_origin = true; content_changed(state.editor_buffer, NULL); state.split_origin = false;
}

static void apply_editor_tag(const char *source, const char *start_pointer, const char *end_pointer, const char *tag) {
    TextIter start, end; gtk_text_buffer_get_iter_at_offset(state.editor_buffer, &start, utf8_offset(source, start_pointer));
    gtk_text_buffer_get_iter_at_offset(state.editor_buffer, &end, utf8_offset(source, end_pointer));
    gtk_text_buffer_apply_tag_by_name(state.editor_buffer, tag, &start, &end);
}

static void highlight_editor(const char *source) {
    TextIter document_start, document_end; gtk_text_buffer_get_bounds(state.editor_buffer, &document_start, &document_end);
    gtk_text_buffer_remove_all_tags(state.editor_buffer, &document_start, &document_end);
    bool code = false; const char *line = source;
    while (*line) {
        const char *end = strchr(line, '\n'); if (!end) end = line + strlen(line);
        if (end - line >= 3 && strncmp(line, "```", 3) == 0) { apply_editor_tag(source, line, end, "syntax-code"); code = !code; }
        else if (code) apply_editor_tag(source, line, end, "syntax-code");
        else if (end - line >= 2 && line[0] == '#' && line[1] == ' ') apply_editor_tag(source, line, end, "syntax-heading");
        else if (end - line >= 3 && line[0] == '#' && line[1] == '#' && line[2] == ' ') apply_editor_tag(source, line, end, "syntax-heading");
        else if (end - line >= 2 && ((line[0] == '-' || line[0] == '*') && line[1] == ' ')) apply_editor_tag(source, line, end, "syntax-list");
        else if (end - line >= 2 && line[0] == '>' && line[1] == ' ') apply_editor_tag(source, line, end, "syntax-quote");
        const char *cursor = line;
        while (!code && (cursor = strstr(cursor, "**")) && cursor < end) {
            const char *close = strstr(cursor + 2, "**"); if (!close || close >= end) break;
            apply_editor_tag(source, cursor, close + 2, "syntax-emphasis"); cursor = close + 2;
        }
        cursor = line;
        while (!code && (cursor = strstr(cursor, "[[")) && cursor < end) {
            const char *close = strstr(cursor + 2, "]]" ); if (!close || close >= end) break;
            apply_editor_tag(source, cursor, close + 2, "syntax-link"); cursor = close + 2;
        }
        line = *end ? end + 1 : end;
    }
}

static Note *note_named(const char *title) {
    for (Note *note = state.notes; note; note = note->next)
        if (strcasecmp(note_display_title(note), title) == 0) return note;
    return NULL;
}

static void open_linked_note(Ptr button, Ptr user_data) {
    (void)button; select_note(NULL, user_data);
}

static void create_linked_note(Ptr button, Ptr user_data) {
    (void)button; const char *title = user_data;
    if (!title || !title[0] || !state.active_notebook) return;
    Note *existing = note_named(title); if (existing) { select_note(NULL, existing); return; }
    new_note(NULL, NULL); if (!state.active) return;
    char markdown[320]; snprintf(markdown, sizeof markdown, "# %s\n", title);
    state.loading = true; gtk_editable_set_text(state.title, title);
    gtk_text_buffer_set_text(state.editor_buffer, markdown, -1); state.loading = false;
    state.active->dirty = true; capture_active_draft(); save_active(NULL);
}

static void rebuild_links(const char *text) {
    if (!state.linksbar) return;
    while (gtk_widget_get_first_child(state.linksbar))
        gtk_box_remove(state.linksbar, gtk_widget_get_first_child(state.linksbar));
    if (!state.active) {
        gtk_widget_set_visible(state.linksbar, false);
        return;
    }
    size_t visible_count = 0;
    const char *cursor = text; char outgoing[128][256]; size_t outgoing_count = 0;
    while ((cursor = strstr(cursor, "[["))) {
        const char *end = strstr(cursor + 2, "]]" ); if (!end) break;
        char title[256]; size_t length = (size_t)(end - cursor - 2);
        if (length >= sizeof title) length = sizeof title - 1;
        memcpy(title, cursor + 2, length); title[length] = '\0';
        bool duplicate = false; for (size_t index = 0; index < outgoing_count; index++)
            if (strcasecmp(outgoing[index], title) == 0) { duplicate = true; break; }
        if (duplicate || !title[0]) { cursor = end + 2; continue; }
        if (outgoing_count < 128) snprintf(outgoing[outgoing_count++], sizeof outgoing[0], "%s", title);
        Note *linked = note_named(title);
        Ptr button = gtk_button_new_with_label(title);
        if (linked)
            g_signal_connect_data(button, "clicked", (void *)open_linked_note, linked, NULL, 0);
        else {
            char *new_title = strdup(title); gtk_widget_add_css_class(button, "missing-link");
            gtk_widget_set_tooltip_text(button, tr("Criar esta nota"));
            g_signal_connect_data(button, "clicked", (void *)create_linked_note, new_title, (void *)free_signal_data, 0);
        }
        gtk_box_append(state.linksbar, button);
        visible_count++;
        cursor = end + 2;
    }
    if (!state.active) return;
    char needle[300]; snprintf(needle, sizeof needle, "[[%s]]", note_display_title(state.active));
    for (Note *note = state.notes; note; note = note->next) {
        if (note == state.active) continue;
        char *source = note_current_markdown(note);
        if (strstr(source, needle)) {
            char backlink[300]; snprintf(backlink, sizeof backlink, "← %s", note_display_title(note));
            Ptr button = gtk_button_new_with_label(backlink);
            g_signal_connect_data(button, "clicked", (void *)open_linked_note, note, NULL, 0);
            gtk_box_append(state.linksbar, button);
            visible_count++;
        }
        free(source);
    }
    gtk_widget_set_visible(state.linksbar, visible_count > 0);
}

static gboolean save_active(Ptr unused) {
    (void)unused;
    state.autosave_source = 0;
    if (!state.active) return 0;
    char *text = editor_text();
    update_views(text);
    bool ok = save_note_payload(state.active, text, gtk_editable_get_text(state.title), gtk_editable_get_text(state.tags), false);
    gtk_label_set_text(state.status, ok ? tr("Guardado") : tr("Erro ao guardar"));
    g_free(text);
    return 0;
}

static void apply_active_line_tag(void) {
    if (!state.line_buffer || state.active_line < 1) return;
    int start_offset = 0; char number[32];
    for (int line = 1; line < state.active_line; line++) start_offset += snprintf(number, sizeof number, "%d", line) + 1;
    int end_offset = start_offset + snprintf(number, sizeof number, "%d", state.active_line);
    TextIter document_start, document_end, start, end;
    gtk_text_buffer_get_bounds(state.line_buffer, &document_start, &document_end);
    gtk_text_buffer_remove_all_tags(state.line_buffer, &document_start, &document_end);
    gtk_text_buffer_get_iter_at_offset(state.line_buffer, &start, start_offset);
    gtk_text_buffer_get_iter_at_offset(state.line_buffer, &end, end_offset);
    gtk_text_buffer_apply_tag_by_name(state.line_buffer, "active-line", &start, &end);
}

static void cursor_mark_set(Ptr buffer, const void *location, Ptr mark, Ptr unused) {
    (void)unused; if (!state.line_buffer || mark != gtk_text_buffer_get_insert(buffer)) return;
    state.active_line = gtk_text_iter_get_line(location) + 1; apply_active_line_tag();
}

static void update_statistics(const char *text) {
    if (!text) text = "";
    size_t words = 0, lines = 1; bool inside = false;
    for (const char *p = text; *p; p++) {
        bool now = !strchr(" \t\r\n", *p);
        if (now && !inside) words++;
        if (*p == '\n') lines++;
        inside = now;
    }
    size_t characters = (size_t)utf8_offset(text, text + strlen(text));
    if (state.stats) {
        char value[160]; snprintf(value, sizeof value, "%zu %s  •  %zu %s", words, tr("palavras"), characters, tr("carateres"));
        gtk_label_set_text(state.stats, value);
    }
    if (state.line_badge) {
        char value[80]; snprintf(value, sizeof value, "%zu %s", lines, tr("linhas")); gtk_label_set_text(state.line_badge, value);
    }
}

static void content_changed(Ptr object, Ptr unused) {
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

static void select_note(Ptr button, Ptr user_data) {
    (void)button; Note *target = user_data; if (!target) return;
    ensure_metadata_loaded(target);
    if (state.active == target) {
        state.active_notebook = target->notebook;
        if (target->button) gtk_widget_add_css_class(target->button, "selected-item");
        return;
    }
    if (state.active && state.active->button) gtk_widget_remove_css_class(state.active->button, "selected-item");
    if (state.active) {
        if (state.autosave_source) { g_source_remove(state.autosave_source); state.autosave_source = 0; }
        if (state.autosave) save_active(NULL); else capture_active_draft();
    }
    state.active = target; state.loading = true;
    if (state.active && state.active->button) gtk_widget_add_css_class(state.active->button, "selected-item");
    state.active->opened = true; rebuild_tabs();
    state.active_notebook = state.active->notebook;
    char *content = state.active->dirty && state.active->draft ? strdup(state.active->draft) : read_file(state.active->markdown_path);
    gtk_editable_set_text(state.title, state.active->dirty && state.active->draft ? state.active->draft_title : state.active->title);
    gtk_editable_set_text(state.tags, state.active->dirty && state.active->draft ? state.active->draft_tags : state.active->tags);
    rebuild_note_tags(); gtk_editable_set_text(state.tag_input, "");
    gtk_text_buffer_set_text(state.editor_buffer, content, -1);
    highlight_editor(content);
    update_views(content);
    rebuild_block_editor();
    update_statistics(content); gtk_label_set_text(state.status, tr("Pronto"));
    free(content); state.loading = false;
}

static void insert_markdown(Ptr button, Ptr user_data) {
    (void)button;
    if (!state.active) return;
    const char *format = user_data, *separator = strchr(format, '|');
    size_t prefix_length = separator ? (size_t)(separator - format) : strlen(format);
    const char *suffix = separator ? separator + 1 : "";
    TextIter start, end;
    if (gtk_text_buffer_get_selection_bounds(state.editor_buffer, &start, &end)) {
        char *selected = gtk_text_buffer_get_text(state.editor_buffer, &start, &end, true);
        size_t size = prefix_length + strlen(selected) + strlen(suffix) + 1; char *replacement = calloc(size, 1);
        memcpy(replacement, format, prefix_length); strcat(replacement, selected); strcat(replacement, suffix);
        gtk_text_buffer_delete(state.editor_buffer, &start, &end); gtk_text_buffer_insert(state.editor_buffer, &start, replacement, -1);
        free(replacement); g_free(selected);
    } else {
        size_t size = prefix_length + strlen(suffix) + 1; char *insertion = calloc(size, 1);
        memcpy(insertion, format, prefix_length); strcat(insertion, suffix);
        gtk_text_buffer_insert_at_cursor(state.editor_buffer, insertion, -1); free(insertion);
    }
}

static void heading_format_changed(Ptr dropdown, Ptr unused, Ptr user_data) {
    (void)unused; (void)user_data; if (!state.active || state.loading) return;
    guint level = gtk_drop_down_get_selected(dropdown); if (level == 0 || level > 6) return; char format[16];
    memset(format, '#', level); format[level] = ' '; format[level + 1] = '|'; format[level + 2] = '\0';
    insert_markdown(NULL, format); state.loading = true; gtk_drop_down_set_selected(dropdown, 0); state.loading = false;
}

static void text_color_changed(Ptr dropdown, Ptr unused, Ptr user_data) {
    (void)unused; (void)user_data; if (!state.active || state.loading) return;
    static const char *colors[] = {NULL, "#d1242f", "#bc4c00", "#9a6700", "#1a7f37", "#0969da", "#8250df"};
    guint selected = gtk_drop_down_get_selected(dropdown); if (selected == 0 || selected >= sizeof colors / sizeof colors[0]) return;
    char format[96]; snprintf(format, sizeof format, "<span style=\"color:%s\">|</span>", colors[selected]);
    insert_markdown(NULL, format); state.loading = true; gtk_drop_down_set_selected(dropdown, 0); state.loading = false;
}

static void insert_literal(Ptr button, Ptr user_data) {
    (void)button; if (state.active) gtk_text_buffer_insert_at_cursor(state.editor_buffer, (const char *)user_data, -1);
}

static Ptr editor_scroller(Ptr view) {
    Ptr scroll = gtk_scrolled_window_new(); gtk_widget_set_hexpand(scroll, true);
    gtk_widget_set_vexpand(scroll, true); gtk_scrolled_window_set_child(scroll, view);
    return scroll;
}

static Ptr readonly_markdown_view(Ptr *buffer, const char *css_class) {
    Ptr view = gtk_text_view_new(); gtk_text_view_set_editable(view, false);
    gtk_text_view_set_cursor_visible(view, false); gtk_text_view_set_wrap_mode(view, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(view, 30); gtk_text_view_set_right_margin(view, 30);
    gtk_text_view_set_top_margin(view, 22); gtk_widget_add_css_class(view, css_class);
    *buffer = gtk_text_view_get_buffer(view);
    if (strcmp(css_class, "preview") == 0) {
        gtk_text_buffer_create_tag(*buffer, "h1", "scale", 1.8, "weight", 700, "pixels-above-lines", 14, "pixels-below-lines", 7, NULL);
        gtk_text_buffer_create_tag(*buffer, "h2", "scale", 1.45, "weight", 700, "pixels-above-lines", 11, NULL);
        gtk_text_buffer_create_tag(*buffer, "h3", "scale", 1.2, "weight", 700, "pixels-above-lines", 8, NULL);
        gtk_text_buffer_create_tag(*buffer, "code", "family", "monospace", "background", "#252a34", NULL);
        gtk_text_buffer_create_tag(*buffer, "quote", "style", 2, "left-margin", 22, "foreground", "#8992a8", NULL);
        gtk_text_buffer_create_tag(*buffer, "list", "left-margin", 12, NULL);
        gtk_text_buffer_create_tag(*buffer, "done", "strikethrough", true, "foreground", "#737d93", NULL);
        gtk_text_buffer_create_tag(*buffer, "strong", "weight", 700, NULL);
        gtk_text_buffer_create_tag(*buffer, "emphasis", "style", 2, NULL);
        gtk_text_buffer_create_tag(*buffer, "strike", "strikethrough", true, "foreground", "#8992a8", NULL);
        gtk_text_buffer_create_tag(*buffer, "inline-code", "family", "monospace", "background", "#252a34", "foreground", "#c29df1", NULL);
        gtk_text_buffer_create_tag(*buffer, "link", "foreground", "#78a9ff", "underline", 1, NULL);
        gtk_text_buffer_create_tag(*buffer, "table-header", "weight", 700, "background", "#202530", "pixels-above-lines", 5, "pixels-below-lines", 5, NULL);
        gtk_text_buffer_create_tag(*buffer, "table-row", "family", "monospace", "pixels-above-lines", 3, "pixels-below-lines", 3, NULL);
        gtk_text_buffer_create_tag(*buffer, "divider", "foreground", "#596174", "justification", 2, NULL);
        gtk_text_buffer_create_tag(*buffer, "text-red", "foreground", "#ff657a", NULL);
        gtk_text_buffer_create_tag(*buffer, "text-orange", "foreground", "#ff9f66", NULL);
        gtk_text_buffer_create_tag(*buffer, "text-yellow", "foreground", "#e7c664", NULL);
        gtk_text_buffer_create_tag(*buffer, "text-green", "foreground", "#68d391", NULL);
        gtk_text_buffer_create_tag(*buffer, "text-blue", "foreground", "#78a9ff", NULL);
        gtk_text_buffer_create_tag(*buffer, "text-purple", "foreground", "#c29df1", NULL);
    }
    return view;
}

static void toggle_sidebar(Ptr button, Ptr unused) {
    (void)button; (void)unused; if (!state.sidebar_panel) return;
    state.sidebar_hidden = !state.sidebar_hidden; gtk_widget_set_visible(state.sidebar_panel, !state.sidebar_hidden);
    if (state.sidebar_toggle) {
        gtk_button_set_icon_name(state.sidebar_toggle, state.sidebar_hidden ? "sidebar-show-symbolic" : "sidebar-hide-symbolic");
        gtk_widget_set_tooltip_text(state.sidebar_toggle, tr(state.sidebar_hidden ? "Mostrar biblioteca" : "Ocultar biblioteca"));
    }
}

static gboolean key_pressed(Ptr controller, guint keyval, guint keycode, guint modifiers, Ptr unused) {
    (void)controller; (void)keycode; (void)unused;
    if (!(modifiers & GDK_CONTROL_MASK)) return false;
    switch (keyval) {
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

static gboolean files_dropped(Ptr target, const void *value, double x, double y, Ptr unused) {
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

static void external_conflict_response(Ptr dialog, int response, Ptr user_data) {
    Note *note = user_data; state.external_conflict_dialog = NULL;
    if (note && note == state.active) {
        if (response == 1) {
            save_active(NULL); gtk_label_set_text(state.status, "Ficheiro externo substituído");
        } else if (response == 2) {
            char *content = read_file(note->markdown_path); state.loading = true;
            gtk_text_buffer_set_text(state.editor_buffer, content, -1); highlight_editor(content); update_views(content);
            note->dirty = false; clear_recovery(note); state.loading = false; free(content);
            gtk_label_set_text(state.status, "Alterações externas recarregadas");
        } else gtk_label_set_text(state.status, "Conflito externo mantido sem alterações");
    }
    gtk_window_destroy(dialog);
}

static gboolean poll_external_file(Ptr unused) {
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
    gtk_text_buffer_set_text(state.editor_buffer, content, -1); highlight_editor(content); update_views(content); state.loading = false;
    gtk_label_set_text(state.status, "Alterações externas recarregadas"); free(content); return true;
}

static void quit_response(Ptr dialog, int response, Ptr unused) {
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

static gboolean close_requested(Ptr window, Ptr unused) {
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

static char *buffer_text(Ptr buffer) {
    TextIter start, end; gtk_text_buffer_get_bounds(buffer, &start, &end);
    return gtk_text_buffer_get_text(buffer, &start, &end, true);
}

static gboolean run_ui_assertions(Ptr unused) {
    (void)unused; const char *needle = "Conteúdo dividido";
    struct { const char *name; Ptr buffer; bool exact; } checks[] = {
        {"editor", state.editor_buffer, true}, {"dividido-fonte", state.split_buffer, true},
        {"dividido-preview", state.split_preview_buffer, false},
        {"numeros-linha", state.line_buffer, false}
    };
    bool ok = state.window && state.sidebar && state.tag_box && state.tabbar && state.title && state.tags && state.linksbar && state.stats && state.line_badge;
    printf("UI widgets: %s\n", ok ? "OK" : "FALHOU");
    char original_theme[32]; snprintf(original_theme, sizeof original_theme, "%s", state.theme);
    const char *theme_ids[] = {"white", "black", "system", "monokai", "tokyo-night", "dracula", "solarized-light"};
    bool themes_ok = state.syntax_heading && state.syntax_emphasis && state.syntax_code && state.syntax_link && state.syntax_list && state.syntax_quote;
    for (size_t index = 0; index < sizeof theme_ids / sizeof theme_ids[0]; index++) {
        apply_theme(theme_ids[index]); themes_ok = themes_ok && strcmp(state.theme, theme_ids[index]) == 0 && state.theme_provider;
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
    bool sidebar_ok = state.sidebar_panel && gtk_widget_get_visible(state.sidebar_panel);
    toggle_sidebar(NULL, NULL); sidebar_ok = sidebar_ok && !gtk_widget_get_visible(state.sidebar_panel) && state.sidebar_hidden;
    toggle_sidebar(NULL, NULL); sidebar_ok = sidebar_ok && gtk_widget_get_visible(state.sidebar_panel) && !state.sidebar_hidden;
    printf("UI ocultar-biblioteca: %s\n", sidebar_ok ? "OK" : "FALHOU"); ok = ok && sidebar_ok;
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

    state.loading = true; gtk_text_buffer_set_text(state.editor_buffer, "texto", -1); state.loading = false;
    TextIter selection_start, selection_end; gtk_text_buffer_get_iter_at_offset(state.editor_buffer, &selection_start, 0);
    gtk_text_buffer_get_iter_at_offset(state.editor_buffer, &selection_end, 5);
    gtk_text_buffer_select_range(state.editor_buffer, &selection_start, &selection_end); insert_markdown(NULL, "**|**");
    char *formatted = editor_text(); bool format_ok = strcmp(formatted, "**texto**") == 0; g_free(formatted);
    state.loading = true; gtk_text_buffer_set_text(state.editor_buffer, "título", -1); state.loading = false;
    gtk_text_buffer_get_bounds(state.editor_buffer, &selection_start, &selection_end);
    gtk_text_buffer_select_range(state.editor_buffer, &selection_start, &selection_end); gtk_drop_down_set_selected(state.format_heading, 4);
    formatted = editor_text(); format_ok = format_ok && strcmp(formatted, "#### título") == 0; g_free(formatted);
    state.loading = true; gtk_text_buffer_set_text(state.editor_buffer, "apagado", -1); state.loading = false;
    gtk_text_buffer_get_bounds(state.editor_buffer, &selection_start, &selection_end);
    gtk_text_buffer_select_range(state.editor_buffer, &selection_start, &selection_end); insert_markdown(NULL, "~~|~~");
    formatted = editor_text(); format_ok = format_ok && strcmp(formatted, "~~apagado~~") == 0; g_free(formatted);
    state.loading = true; gtk_text_buffer_set_text(state.editor_buffer, "colorido", -1); state.loading = false;
    gtk_text_buffer_get_bounds(state.editor_buffer, &selection_start, &selection_end);
    gtk_text_buffer_select_range(state.editor_buffer, &selection_start, &selection_end); gtk_drop_down_set_selected(state.format_color, 5);
    formatted = editor_text(); format_ok = format_ok && strcmp(formatted, "<span style=\"color:#0969da\">colorido</span>") == 0; g_free(formatted);
    printf("UI formatacao-selecao: %s\n", format_ok ? "OK" : "FALHOU"); ok = ok && format_ok;

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
            strcmp(gtk_editable_get_text(state.block_languages[2]), "c") == 0;
        g_free(code_content); gtk_editable_set_text(state.block_languages[2], "rust");
        char *code_markdown = editor_text(); blocks_ok = blocks_ok && strstr(code_markdown, "```rust\nlinha 1"); g_free(code_markdown);
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
    if (access(source_icon, R_OK) != 0) snprintf(source_icon, sizeof source_icon, "%s/../Assets/NoteMD-AppIcon-1024.png", state.data_dir);
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
    state.ui_test_failed = !ok; g_application_quit(state.app); return false;
}

static gboolean run_empty_ui_assertions(Ptr unused) {
    (void)unused;
    bool ok = !state.notebooks && !state.notes && !state.active && state.side_new_note;
    printf("UI vazio-estado-inicial: %s\n", ok ? "OK" : "FALHOU");
    g_signal_emit_by_name(state.mode_split, "clicked");
    bool split_without_note = strcmp(gtk_stack_get_visible_child_name(state.mode_stack), "split") == 0;
    printf("UI vazio-clique-dividido: %s\n", split_without_note ? "OK" : "FALHOU"); ok = ok && split_without_note;
    g_signal_emit_by_name(state.side_new_note, "clicked");
    bool created = state.notebooks && state.active_notebook && state.active && state.active->notebook == state.active_notebook &&
        access(state.active->markdown_path, R_OK) == 0;
    printf("UI vazio-nova-nota-cria-tudo: %s\n", created ? "OK" : "FALHOU"); ok = ok && created;
    if (created) {
        gtk_text_buffer_set_text(state.editor_buffer, "# Primeira nota\n\nTexto visível no dividido", -1);
        g_signal_emit_by_name(state.mode_split, "clicked"); char *preview = buffer_text(state.split_preview_buffer);
        bool visible = strstr(preview, "Primeira nota") && strstr(preview, "Texto visível no dividido");
        printf("UI vazio-dividido-com-conteudo: %s\n", visible ? "OK" : "FALHOU"); ok = ok && visible; g_free(preview);
    }
    state.ui_test_failed = !ok; g_application_quit(state.app); return false;
}

static void activate(Ptr app, Ptr unused) {
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
    Ptr history = icon_button("document-open-recent-symbolic", tr("Histórico"));
    Ptr export = icon_button("x-office-document-symbolic", "Exportar nota");
    g_signal_connect_data(state.sidebar_toggle, "clicked", (void *)toggle_sidebar, NULL, NULL, 0);
    g_signal_connect_data(history, "clicked", (void *)show_history, NULL, NULL, 0);
    g_signal_connect_data(export, "clicked", (void *)show_export_menu, NULL, NULL, 0);

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
    state.title = gtk_entry_new(); gtk_entry_set_placeholder_text(state.title, tr("Título da nota"));
    gtk_widget_add_css_class(state.title, "note-title"); gtk_widget_set_size_request(state.title, 320, -1);
    gtk_widget_set_visible(state.title, false);
    gtk_widget_set_hexpand(state.title, true);
    Ptr tag_area = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5); gtk_widget_add_css_class(tag_area, "note-tag-area");
    state.note_tags_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    Ptr tags_scroll = gtk_scrolled_window_new(); gtk_widget_set_size_request(tags_scroll, 180, -1);
    gtk_scrolled_window_set_policy(tags_scroll, GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    gtk_scrolled_window_set_child(tags_scroll, state.note_tags_box); gtk_box_append(tag_area, tags_scroll);
    state.tag_input = gtk_entry_new(); gtk_entry_set_placeholder_text(state.tag_input, tr("Adicionar tag"));
    gtk_widget_add_css_class(state.tag_input, "tag-input"); gtk_widget_set_size_request(state.tag_input, 90, -1);
    g_signal_connect_data(state.tag_input, "activate", (void *)add_note_tag, NULL, NULL, 0); gtk_box_append(tag_area, state.tag_input);
    Ptr add_tag = icon_button("list-add-symbolic", tr("Adicionar tag")); gtk_widget_add_css_class(add_tag, "tag-add");
    g_signal_connect_data(add_tag, "clicked", (void *)add_note_tag_clicked, NULL, NULL, 0); gtk_box_append(tag_area, add_tag);
    state.tags = gtk_entry_new(); gtk_widget_set_visible(state.tags, false); gtk_box_append(tag_area, state.tags);
    gtk_box_append(identity, state.title); gtk_box_append(workspace, identity);
    g_signal_connect_data(state.title, "changed", (void *)content_changed, NULL, NULL, 0);
    g_signal_connect_data(state.tags, "changed", (void *)content_changed, NULL, NULL, 0);
    state.linksbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5); margins(state.linksbar, 16, 3);
    gtk_widget_set_visible(state.linksbar, false);
    gtk_widget_add_css_class(state.linksbar, "linksbar"); gtk_box_append(workspace, state.linksbar);
    Ptr formatbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3); margins(formatbar, 12, 4);
    gtk_widget_add_css_class(formatbar, "formatbar");
    gtk_box_append(formatbar, state.sidebar_toggle);
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
    gtk_box_append(formatbar, history);
    gtk_box_append(formatbar, export);
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
    gtk_widget_add_css_class(state.status, "status");
    state.stats = gtk_label_new("0 palavras  •  0 carateres"); gtk_widget_add_css_class(state.stats, "document-stats");
    state.line_badge = gtk_label_new("1 linha"); gtk_widget_add_css_class(state.line_badge, "line-badge");
    Ptr markdown_badge = gtk_label_new("Markdown"); gtk_widget_add_css_class(markdown_badge, "markdown-badge");
    gtk_box_append(statusbar, state.status);
    gtk_box_append(statusbar, tag_area);
    Ptr status_spacer = gtk_label_new(""); gtk_widget_set_hexpand(status_spacer, true);
    gtk_box_append(statusbar, status_spacer);
    gtk_box_append(statusbar, state.stats);
    gtk_box_append(statusbar, state.line_badge); gtk_box_append(statusbar, markdown_badge);
    gtk_box_append(workspace, statusbar); gtk_paned_set_end_child(paned, workspace);
    gtk_box_append(outer, paned); gtk_window_set_child(state.window, outer); gtk_window_present(state.window);
    if (state.notes) select_note(NULL, state.notes);
    if (state.ui_test) {
        if (state.empty_ui_test) g_timeout_add(350, (void *)run_empty_ui_assertions, NULL);
        else { select_editor_mode(NULL, "split"); g_timeout_add(350, (void *)run_ui_assertions, NULL); }
    }
}

static void open_application_files(Ptr app, Ptr *files, int count, const char *hint, Ptr unused) {
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

static void resolve_root(void) {
    const char *argument = getenv("NOTEMD_NOTES_DIR");
    if (argument && *argument) snprintf(state.root, sizeof state.root, "%s", argument);
    else if (!state.root[0]) {
        argument = getenv("HOME"); if (!argument) argument = ".";
        snprintf(state.root, sizeof state.root, "%s/Documents/NoteMD", argument);
    }
}

static void resolve_data_dir(void) {
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

static int self_test(void) {
    char temporary[] = "/tmp/notemd-selftest-XXXXXX";
    if (!mkdtemp(temporary)) { fprintf(stderr, "self-test: mkdtemp falhou\n"); return 1; }
    char notebook_path[PATH_MAX], note_path[PATH_MAX], markdown_path[PATH_MAX];
    snprintf(notebook_path, sizeof notebook_path, "%s/Testes", temporary);
    snprintf(note_path, sizeof note_path, "%s/Nota", notebook_path);
    snprintf(markdown_path, sizeof markdown_path, "%s/note.md", note_path);
    bool ok = make_directory(note_path) && write_atomic(markdown_path, "# Teste\n");
    char *restored = read_file(markdown_path); ok = ok && strcmp(restored, "# Teste\n") == 0; free(restored);
    Notebook notebook = {0}; Note note = {0}; snprintf(notebook.title, sizeof notebook.title, "Testes");
    snprintf(notebook.path, sizeof notebook.path, "%s", notebook_path); snprintf(note.id, sizeof note.id, "self-test-id");
    note.notebook = &notebook; state.notes = &note; save_notebook_metadata(&notebook);
    char metadata_path[PATH_MAX]; snprintf(metadata_path, sizeof metadata_path, "%s/.notebook.json", notebook_path);
    char *metadata = read_file(metadata_path); ok = ok && strstr(metadata, "self-test-id") != NULL; free(metadata);
    snprintf(state.language, sizeof state.language, "en");
    ok = ok && strcmp(tr("Pesquisar e substituir"), "Find and replace") == 0 && strcmp(tr("Duplicar bloco"), "Duplicate block") == 0;
    snprintf(state.language, sizeof state.language, "fr");
    ok = ok && strcmp(tr("Sem tags"), "Aucun tag") == 0 && strcmp(tr("Exportar PDF"), "Exporter en PDF") == 0;
    snprintf(state.language, sizeof state.language, "pt");
    state.notes = NULL; unlink(markdown_path); unlink(metadata_path); rmdir(note_path); rmdir(notebook_path); rmdir(temporary);
    printf("NoteMD self-test: %s\n", ok ? "OK" : "FALHOU"); return ok ? 0 : 1;
}

static int export_self_test(void) {
    const char *markdown = "## Secção\n\nTexto com **negrito**, _itálico_, `código`, <span style=\"color:#0969da\">azul</span> e [ligação](https://example.com).\n\n"
        "- item\n1. primeiro\n- [ ] tarefa\n- [x] concluída\n\n> citação\n\n"
        "| Nome | Valor |\n| --- | --- |\n| Um | Dois |\n\n```c\nint main(void);\n```\n\n![Imagem](assets/teste.png)\n";
    char pdf[] = "/tmp/notemd-export-test.pdf", docx[] = "/tmp/notemd-export-test.docx";
    bool ok = pdf_export_content(pdf, "Teste NoteMD", markdown) && docx_export_content(docx, markdown);
    char *html = html_document("Teste NoteMD", markdown);
    const char *html_markers[] = {"<h2>", "<strong>", "<em>", "<code>", "<a href=", "<ul>", "<ol>",
        "class=\"task\"", "<s>", "<span style=\"color:#0969da\">", "<blockquote>", "<table>", "<pre><code>", "<img src="};
    for (size_t index = 0; index < sizeof html_markers / sizeof html_markers[0]; index++) {
        bool present = strstr(html, html_markers[index]) != NULL;
        if (!present) fprintf(stderr, "export-self-test: HTML em falta: %s\n", html_markers[index]);
        ok = ok && present;
    }
    char *document_xml = docx_document_xml(markdown);
    const char *docx_markers[] = {"<w:b/>", "<w:i/>", "Consolas", "<w:numId w:val=\"1\"/>",
        "<w:numId w:val=\"2\"/>", "<w:strike/>", "<w:color w:val=\"0969da\"/>", "<w:pBdr>", "<w:tbl>"};
    for (size_t index = 0; index < sizeof docx_markers / sizeof docx_markers[0]; index++) {
        bool present = strstr(document_xml, docx_markers[index]) != NULL;
        if (!present) fprintf(stderr, "export-self-test: DOCX em falta: %s\n", docx_markers[index]);
        ok = ok && present;
    }
    char *pdf_data = read_file(pdf), *docx_data = read_file(docx);
    ok = ok && strncmp(pdf_data, "%PDF-", 5) == 0 && (unsigned char)docx_data[0] == 'P' && (unsigned char)docx_data[1] == 'K';
    free(document_xml); free(html); free(pdf_data); free(docx_data); unlink(pdf); unlink(docx);
    printf("NoteMD export self-test: %s\n", ok ? "OK" : "FALHOU"); return ok ? 0 : 1;
}

static bool prepare_ui_test(void) {
    if (!state.ui_test_dir[0]) {
        snprintf(state.ui_test_dir, sizeof state.ui_test_dir, "/tmp/notemd-ui-test-XXXXXX");
        if (!mkdtemp(state.ui_test_dir)) return false;
    }
    char library[PATH_MAX], notebook[PATH_MAX], note[PATH_MAX], markdown[PATH_MAX], metadata[PATH_MAX];
    snprintf(library, sizeof library, "%s/library", state.ui_test_dir); if (!make_directory(library)) return false;
    if (state.empty_ui_test) { snprintf(state.root, sizeof state.root, "%s", library); return true; }
    snprintf(notebook, sizeof notebook, "%s/Testes", library);
    snprintf(note, sizeof note, "%s/Nota dividida", notebook); if (!make_directory(note)) return false;
    snprintf(markdown, sizeof markdown, "%s/note.md", note); snprintf(metadata, sizeof metadata, "%s/.note.json", note);
    if (!write_atomic(markdown, "# Teste\n\nConteúdo dividido\n\n- item\n")) return false;
    if (!write_atomic(metadata, "{\"id\":\"ui-test-note\",\"colorHex\":null,\"tags\":[\"teste\"]}")) return false;
    snprintf(state.root, sizeof state.root, "%s", library); return true;
}

static void remove_test_tree(const char *path) {
    struct stat info; if (lstat(path, &info) != 0) return;
    if (!S_ISDIR(info.st_mode)) { unlink(path); return; }
    DIR *directory = opendir(path); if (!directory) return; struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char child[PATH_MAX]; snprintf(child, sizeof child, "%s/%s", path, entry->d_name); remove_test_tree(child);
    }
    closedir(directory); rmdir(path);
}

static void cleanup_ui_test(void) {
    if (strncmp(state.ui_test_dir, "/tmp/notemd-ui-test-", 20) == 0) remove_test_tree(state.ui_test_dir);
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--self-test") == 0) return self_test();
    if (argc == 2 && strcmp(argv[1], "--export-self-test") == 0) { load_gtk(); return export_self_test(); }
    bool empty_ui_test = argc == 2 && strcmp(argv[1], "--empty-ui-self-test") == 0;
    bool ui_test = (argc == 2 && strcmp(argv[1], "--ui-self-test") == 0) || empty_ui_test;
    bool new_instance = argc == 2 && strcmp(argv[1], "--new-instance") == 0;
    if (new_instance) setenv("GSK_RENDERER", "gl", 0);
    if (ui_test) {
        snprintf(state.ui_test_dir, sizeof state.ui_test_dir, "/tmp/notemd-ui-test-XXXXXX");
        if (!mkdtemp(state.ui_test_dir)) return 1;
        char config[PATH_MAX], data[PATH_MAX]; snprintf(config, sizeof config, "%s/xdg-config", state.ui_test_dir);
        snprintf(data, sizeof data, "%s/xdg-data", state.ui_test_dir); setenv("XDG_CONFIG_HOME", config, 1); setenv("XDG_DATA_HOME", data, 1);
    }
    load_gtk(); load_settings(); resolve_root(); resolve_data_dir();
    if (ui_test) { state.ui_test = true; state.empty_ui_test = empty_ui_test; state.external_count = 0; if (!prepare_ui_test()) return 1; }
    load_library(); if (!ui_test) restore_recovery();
    if (ui_test) {
        for (Notebook *book = state.notebooks; book; book = book->next) extract_notebook_metadata(book);
        for (Note *note = state.notes; note; note = note->next) {
            extract_metadata(note);
            note->metadata_loaded = true;
        }
        resort_library_notes();
    } else {
        start_background_metadata_indexer();
    }
    for (size_t index = 0; index < state.external_count; index++) add_external_note(state.external_paths[index]);
    int application_flags = G_APPLICATION_HANDLES_OPEN | (new_instance ? G_APPLICATION_NON_UNIQUE : 0);
    state.app = gtk_application_new("pt.notemd.NoteMD", application_flags);
    g_signal_connect_data(state.app, "activate", (void *)activate, NULL, NULL, 0);
    g_signal_connect_data(state.app, "open", (void *)open_application_files, NULL, NULL, 0);
    int run_argc = (ui_test || new_instance) ? 1 : argc;
    int result = g_application_run(state.app, run_argc, argv);
    g_object_unref(state.app);
    if (ui_test) { cleanup_ui_test(); return state.ui_test_failed ? 1 : result; }
    return result;
}

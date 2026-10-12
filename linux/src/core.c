#include "internal.h"

State state;

const char *tr(const char *pt) {
    struct Translation { const char *pt, *en, *fr; };
    static const struct Translation values[] = {
        {"Biblioteca", "Library", "Bibliothèque"}, {"Menu da aplicação", "Application menu", "Menu de l’application"},
        {"Claro", "Light", "Clair"}, {"Escuro", "Dark", "Sombre"},
        {"Tamanho do texto", "Text size", "Taille du texte"},
        {"Editor e pré-visualização", "Editor and preview", "Éditeur et aperçu"},
        {"Aumentar: Ctrl++   ·   Diminuir: Ctrl+−   ·   Repor: Ctrl+0", "Larger: Ctrl++   ·   Smaller: Ctrl+−   ·   Reset: Ctrl+0", "Agrandir : Ctrl++   ·   Réduire : Ctrl+−   ·   Réinitialiser : Ctrl+0"},
        {"Sem resultados", "No results", "Aucun résultat"},
        {"Experimente outra pesquisa ou remova o filtro de tags.", "Try another search or clear the tag filter.", "Essayez une autre recherche ou effacez le filtre de tags."},
        {"Detalhes da nota", "Note details", "Détails de la note"}, {"Mais formatação", "More formatting", "Plus de mise en forme"},
        {"Um espaço para as suas ideias", "A space for your ideas", "Un espace pour vos idées"},
        {"Crie uma nota ou abra um ficheiro Markdown para começar.", "Create a note or open a Markdown file to get started.", "Créez une note ou ouvrez un fichier Markdown pour commencer."},
        {"Nova nota  Ctrl+N   ·   Abrir  Ctrl+O", "New note  Ctrl+N   ·   Open  Ctrl+O", "Nouvelle note  Ctrl+N   ·   Ouvrir  Ctrl+O"},
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
        {"Estilo do texto", "Text style", "Style du texte"},
        {"Cor do notebook", "Notebook color", "Couleur du carnet"},
        {"Ordenar notas", "Sort notes", "Trier les notes"},
        {"Título 1", "Heading 1", "Titre 1"}, {"Título 2", "Heading 2", "Titre 2"},
        {"Título 3", "Heading 3", "Titre 3"}, {"Título 4", "Heading 4", "Titre 4"},
        {"Título 5", "Heading 5", "Titre 5"}, {"Título 6", "Heading 6", "Titre 6"},
        {"Descrição da imagem", "Image description", "Description de l’image"},
        {"Localização da imagem", "Image location", "Emplacement de l’image"}
    };
    int column = strcmp(state.language, "en") == 0 ? 1 : strcmp(state.language, "fr") == 0 ? 2 : 0;
    for (size_t index = 0; index < sizeof values / sizeof values[0]; index++) if (strcmp(values[index].pt, pt) == 0)
        return column == 1 ? values[index].en : column == 2 ? values[index].fr : values[index].pt;
    return pt;
}

#define FN(ret, name, args) ret (*name) args
FN(Ptr, gtk_application_new, (const char *, int));
FN(int, g_application_run, (Ptr, int, char **));
FN(void, g_application_quit, (Ptr));
FN(void, g_object_unref, (Ptr));
FN(Ptr, g_object_ref, (Ptr));
FN(void, g_object_set, (Ptr, const char *, ...));
FN(void, g_object_get, (Ptr, const char *, ...));
FN(Ptr, g_object_bind_property, (Ptr, const char *, Ptr, const char *, int));
FN(Ptr, gtk_settings_get_default, (void));
FN(unsigned long, g_signal_connect_data, (Ptr, const char *, void *, Ptr, void *, int));
FN(void, g_signal_emit_by_name, (Ptr, const char *, ...));
FN(Ptr, gtk_application_window_new, (Ptr));
FN(void, gtk_window_set_title, (Ptr, const char *));
FN(void, gtk_window_set_icon_name, (Ptr, const char *));
FN(void, gtk_window_set_default_size, (Ptr, int, int));
FN(void, gtk_window_set_resizable, (Ptr, gboolean));
FN(gboolean, gtk_window_is_maximized, (Ptr));
FN(gboolean, gtk_window_is_fullscreen, (Ptr));
FN(Ptr, gtk_native_get_surface, (Ptr));
FN(void, gdk_toplevel_begin_resize, (Ptr, int, Ptr, int, double, double, guint));
FN(Ptr, gdk_event_get_device, (Ptr));
FN(guint, gdk_event_get_time, (Ptr));
FN(gboolean, gdk_event_get_position, (Ptr, double *, double *));
FN(void, gtk_window_set_child, (Ptr, Ptr));
FN(void, gtk_window_set_titlebar, (Ptr, Ptr));
FN(Ptr, gtk_window_handle_new, (void));
FN(void, gtk_window_handle_set_child, (Ptr, Ptr));
FN(Ptr, gtk_window_controls_new, (int));
FN(Ptr, gtk_header_bar_new, (void));
FN(void, gtk_header_bar_pack_start, (Ptr, Ptr));
FN(void, gtk_header_bar_pack_end, (Ptr, Ptr));
FN(void, gtk_header_bar_set_title_widget, (Ptr, Ptr));
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
FN(Ptr, gtk_widget_get_parent, (Ptr));
FN(Ptr, gtk_widget_get_next_sibling, (Ptr));
FN(Ptr, gtk_paned_new, (int));
FN(void, gtk_paned_set_start_child, (Ptr, Ptr));
FN(void, gtk_paned_set_end_child, (Ptr, Ptr));
FN(void, gtk_paned_set_position, (Ptr, int));
FN(void, gtk_paned_set_resize_start_child, (Ptr, gboolean));
FN(void, gtk_paned_set_resize_end_child, (Ptr, gboolean));
FN(void, gtk_paned_set_shrink_start_child, (Ptr, gboolean));
FN(void, gtk_paned_set_shrink_end_child, (Ptr, gboolean));
FN(void, gtk_paned_set_wide_handle, (Ptr, gboolean));
FN(Ptr, gtk_scrolled_window_new, (void));
FN(void, gtk_scrolled_window_set_child, (Ptr, Ptr));
FN(void, gtk_scrolled_window_set_policy, (Ptr, int, int));
FN(void, gtk_scrolled_window_set_max_content_height, (Ptr, int));
FN(void, gtk_scrolled_window_set_propagate_natural_height, (Ptr, gboolean));
FN(Ptr, gtk_scrolled_window_get_vadjustment, (Ptr));
FN(void, gtk_scrolled_window_set_vadjustment, (Ptr, Ptr));
FN(void, gtk_widget_set_hexpand, (Ptr, gboolean));
FN(void, gtk_widget_set_vexpand, (Ptr, gboolean));
FN(void, gtk_widget_set_size_request, (Ptr, int, int));
FN(void, gtk_widget_set_visible, (Ptr, gboolean));
FN(void, gtk_widget_set_sensitive, (Ptr, gboolean));
FN(void, gtk_widget_set_valign, (Ptr, int));
FN(void, gtk_widget_set_halign, (Ptr, int));
FN(gboolean, gtk_widget_grab_focus, (Ptr));
FN(void, gtk_widget_set_can_focus, (Ptr, gboolean));
FN(Ptr, gtk_widget_pick, (Ptr, double, double, int));
FN(Ptr, gtk_event_controller_focus_new, (void));
FN(gboolean, gtk_widget_get_visible, (Ptr));
FN(void, gtk_widget_add_css_class, (Ptr, const char *));
FN(void, gtk_widget_remove_css_class, (Ptr, const char *));
FN(gboolean, gtk_widget_has_css_class, (Ptr, const char *));
FN(void, gtk_widget_set_tooltip_text, (Ptr, const char *));
FN(void, gtk_widget_set_cursor_from_name, (Ptr, const char *));
FN(int, gtk_widget_get_width, (Ptr));
FN(void, gtk_widget_measure, (Ptr, int, int, int *, int *, int *, int *));
FN(void, gtk_widget_add_controller, (Ptr, Ptr));
FN(Ptr, gtk_event_controller_key_new, (void));
FN(Ptr, gtk_event_controller_get_current_event, (Ptr));
FN(gboolean, gtk_gesture_set_state, (Ptr, int));
FN(void, gtk_event_controller_set_propagation_phase, (Ptr, int));
FN(Ptr, gtk_gesture_click_new, (void));
FN(void, gtk_gesture_single_set_button, (Ptr, unsigned));
FN(void, gtk_widget_set_parent, (Ptr, Ptr));
FN(void, gtk_widget_unparent, (Ptr));
FN(Ptr, gtk_popover_new, (void));
FN(void, gtk_popover_set_child, (Ptr, Ptr));
FN(void, gtk_popover_set_pointing_to, (Ptr, const void *));
FN(void, gtk_popover_set_has_arrow, (Ptr, gboolean));
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
FN(void, gtk_label_set_wrap, (Ptr, gboolean));
FN(Ptr, gtk_entry_new, (void));
FN(Ptr, gtk_search_entry_new, (void));
FN(void, gtk_search_entry_set_placeholder_text, (Ptr, const char *));
FN(void, gtk_entry_set_placeholder_text, (Ptr, const char *));
FN(const char *, gtk_editable_get_text, (Ptr));
FN(void, gtk_editable_set_text, (Ptr, const char *));
FN(void, gtk_editable_set_editable, (Ptr, gboolean));
FN(gboolean, gtk_editable_get_editable, (Ptr));
FN(void, gtk_editable_select_region, (Ptr, int, int));
FN(Ptr, gtk_button_new_with_label, (const char *));
FN(Ptr, gtk_button_new_from_icon_name, (const char *));
FN(Ptr, gtk_menu_button_new, (void));
FN(void, gtk_menu_button_set_icon_name, (Ptr, const char *));
FN(void, gtk_menu_button_set_label, (Ptr, const char *));
FN(void, gtk_menu_button_set_always_show_arrow, (Ptr, gboolean));
FN(void, gtk_menu_button_set_popover, (Ptr, Ptr));
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
FN(int, gtk_text_iter_get_line_offset, (const void *));
FN(void, gtk_text_iter_set_line_offset, (void *, int));
FN(gboolean, gtk_text_iter_forward_to_line_end, (void *));
FN(gboolean, gtk_text_iter_ends_line, (const void *));
FN(gboolean, gtk_text_iter_backward_char, (void *));
FN(void, gtk_text_buffer_begin_user_action, (Ptr));
FN(void, gtk_text_buffer_end_user_action, (Ptr));
FN(void, gtk_text_buffer_undo, (Ptr));
FN(Ptr, gtk_text_buffer_create_tag, (Ptr, const char *, const char *, ...));
FN(Ptr, gtk_text_buffer_get_tag_table, (Ptr));
FN(Ptr, gtk_text_tag_table_lookup, (Ptr, const char *));
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
FN(void, gtk_stack_set_hhomogeneous, (Ptr, gboolean));
FN(void, gtk_stack_set_vhomogeneous, (Ptr, gboolean));
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
FN(void, gtk_label_set_ellipsize, (Ptr, int));
FN(void, gtk_label_set_max_width_chars, (Ptr, int));
FN(void, gtk_editable_set_width_chars, (Ptr, int));
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

FN(void, gtk_native_dialog_set_modal, (Ptr, gboolean));
FN(Ptr, gtk_file_filter_new, (void));
FN(void, gtk_file_filter_set_name, (Ptr, const char *));
FN(void, gtk_file_filter_add_pattern, (Ptr, const char *));
FN(void, gtk_file_filter_add_mime_type, (Ptr, const char *));
FN(void, gtk_file_chooser_add_filter, (Ptr, Ptr));
FN(void, gtk_file_chooser_set_filter, (Ptr, Ptr));
FN(Ptr, g_object_ref_sink, (Ptr));

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

FN(void, cairo_save, (Ptr));
FN(void, cairo_restore, (Ptr));
FN(void, cairo_set_source_rgb, (Ptr, double, double, double));
FN(void, cairo_rectangle, (Ptr, double, double, double, double));
FN(void, cairo_fill, (Ptr));
FN(void, cairo_set_line_width, (Ptr, double));
FN(void, cairo_line_to, (Ptr, double, double));
FN(void, cairo_stroke, (Ptr));
FN(void, cairo_translate, (Ptr, double, double));
FN(void, cairo_scale, (Ptr, double, double));
FN(void, cairo_paint, (Ptr));

FN(Ptr, cairo_image_surface_create_from_png, (const char *));
FN(Ptr, cairo_image_surface_create, (int, int, int));
FN(int, cairo_image_surface_get_width, (Ptr));
FN(int, cairo_image_surface_get_height, (Ptr));
FN(int, cairo_surface_set_mime_data, (Ptr, const char *, const unsigned char *, unsigned long, void (*)(void *), void *));
FN(void, cairo_set_source_surface, (Ptr, Ptr, double, double));

FN(void, cairo_pdf_surface_set_metadata, (Ptr, int, const char *));
FN(int, cairo_pdf_surface_add_outline, (Ptr, int, const char *, const char *, int));
FN(void, cairo_tag_begin, (Ptr, const char *, const char *));
FN(void, cairo_tag_end, (Ptr, const char *));
FN(int, g_unichar_to_utf8, (unsigned, char *));
FN(char *, g_uri_unescape_string, (const char *, const char *));
FN(Ptr, pango_cairo_create_layout, (Ptr));
FN(void, pango_cairo_context_set_resolution, (Ptr, double));
FN(void, pango_cairo_show_layout, (Ptr, Ptr));
FN(void, pango_cairo_show_layout_line, (Ptr, Ptr));
FN(Ptr, pango_layout_get_context, (Ptr));
FN(void, pango_layout_set_width, (Ptr, int));
FN(void, pango_layout_set_wrap, (Ptr, int));
FN(void, pango_layout_set_alignment, (Ptr, int));
FN(void, pango_layout_set_markup, (Ptr, const char *, int));
FN(void, pango_layout_set_text, (Ptr, const char *, int));
FN(void, pango_layout_set_font_description, (Ptr, Ptr));
FN(Ptr, pango_font_description_from_string, (const char *));
FN(void, pango_font_description_set_absolute_size, (Ptr, double));
FN(void, pango_font_description_free, (Ptr));
FN(void, pango_layout_get_size, (Ptr, int *, int *));
FN(int, pango_layout_get_line_count, (Ptr));
FN(Ptr, pango_layout_get_iter, (Ptr));
FN(Ptr, pango_layout_iter_get_line_readonly, (Ptr));
FN(void, pango_layout_iter_get_line_extents, (Ptr, Ptr, Ptr));
FN(int, pango_layout_iter_get_baseline, (Ptr));
FN(gboolean, pango_layout_iter_next_line, (Ptr));
FN(void, pango_layout_iter_free, (Ptr));
FN(int, pango_layout_line_get_start_index, (Ptr));
FN(int, pango_layout_line_get_length, (Ptr));
FN(void, pango_layout_line_get_x_ranges, (Ptr, int, int, int **, int *));
FN(Ptr, gdk_pixbuf_new_from_file_at_scale, (const char *, int, int, gboolean, Ptr *));
FN(int, gdk_pixbuf_get_width, (Ptr));
FN(int, gdk_pixbuf_get_height, (Ptr));
FN(void, gdk_cairo_set_source_pixbuf, (Ptr, Ptr, double, double));

FN(Ptr, gtk_source_view_new, (void));
FN(Ptr, gtk_source_buffer_new, (Ptr));
FN(void, gtk_source_buffer_set_language, (Ptr, Ptr));
FN(Ptr, gtk_source_buffer_get_language, (Ptr));
FN(void, gtk_source_buffer_set_highlight_syntax, (Ptr, gboolean));
FN(gboolean, gtk_source_buffer_get_highlight_syntax, (Ptr));
FN(void, gtk_source_buffer_set_highlight_matching_brackets, (Ptr, gboolean));
FN(void, gtk_source_buffer_ensure_highlight, (Ptr, const void *, const void *));
FN(void, gtk_source_buffer_set_style_scheme, (Ptr, Ptr));
FN(Ptr, gtk_source_buffer_get_style_scheme, (Ptr));
FN(Ptr, gtk_source_language_manager_get_default, (void));
FN(void, gtk_source_language_manager_prepend_search_path, (Ptr, const char *));
FN(Ptr, gtk_source_language_manager_get_language, (Ptr, const char *));
FN(Ptr, gtk_source_style_scheme_manager_get_default, (void));
FN(void, gtk_source_style_scheme_manager_prepend_search_path, (Ptr, const char *));
FN(Ptr, gtk_source_style_scheme_manager_get_scheme, (Ptr, const char *));
FN(Ptr, gtk_source_style_scheme_get_style, (Ptr, const char *));
FN(GSList *, gtk_text_iter_get_tags, (const void *));

FN(int, gtk_text_iter_get_offset, (const void *));
FN(gboolean, gtk_text_iter_forward_to_tag_toggle, (void *, Ptr));
FN(void, gtk_text_buffer_apply_tag, (Ptr, Ptr, const void *, const void *));

FN(void, g_slist_free, (Ptr));
FN(void, gdk_rgba_free, (Ptr));

#undef FN

void die(const char *message) {
    fprintf(stderr, "NoteMD: %s\n", message);
    exit(EXIT_FAILURE);
}

void load_symbol(void **target, const char *name) {
    *target = dlsym(RTLD_DEFAULT, name);
    if (!*target) die(name);
}

#define LOAD(name) load_symbol((void **)&name, #name)
void load_gtk(void) {
    state.gtk = dlopen("libgtk-4.so.1", RTLD_NOW | RTLD_GLOBAL);
    if (!state.gtk) die("GTK 4 não está instalada (libgtk-4.so.1)");
    if (!dlopen("libgtksourceview-5.so.0", RTLD_NOW | RTLD_GLOBAL)) die("GtkSourceView 5 não está instalado (libgtksourceview-5.so.0)");
    LOAD(gtk_source_view_new); LOAD(gtk_source_buffer_new); LOAD(gtk_source_buffer_set_language); LOAD(gtk_source_buffer_get_language);
    LOAD(gtk_source_buffer_set_highlight_syntax); LOAD(gtk_source_buffer_get_highlight_syntax); LOAD(gtk_source_buffer_set_highlight_matching_brackets); LOAD(gtk_source_buffer_ensure_highlight);
    LOAD(gtk_source_buffer_set_style_scheme); LOAD(gtk_source_buffer_get_style_scheme); LOAD(gtk_source_language_manager_get_default); LOAD(gtk_source_language_manager_prepend_search_path);
    LOAD(gtk_source_language_manager_get_language); LOAD(gtk_source_style_scheme_manager_get_default); LOAD(gtk_source_style_scheme_manager_prepend_search_path); LOAD(gtk_source_style_scheme_manager_get_scheme);
    LOAD(gtk_source_style_scheme_get_style); LOAD(gtk_text_iter_get_tags); LOAD(gtk_text_iter_get_offset); LOAD(gtk_text_iter_forward_to_tag_toggle); LOAD(gtk_text_buffer_apply_tag); LOAD(g_slist_free); LOAD(gdk_rgba_free);
    LOAD(gtk_application_new); LOAD(g_application_run); LOAD(g_application_quit); LOAD(g_object_unref); LOAD(g_object_ref); LOAD(g_object_set);
    LOAD(g_object_get); LOAD(gtk_settings_get_default);
    LOAD(g_signal_connect_data); LOAD(g_signal_emit_by_name); LOAD(gtk_application_window_new); LOAD(gtk_window_new);
    LOAD(gtk_window_set_transient_for); LOAD(gtk_window_set_modal); LOAD(gtk_window_set_title);
    LOAD(gtk_window_destroy);
    LOAD(gtk_message_dialog_new); LOAD(gtk_message_dialog_format_secondary_text);
    LOAD(gtk_dialog_add_button);
    LOAD(gtk_window_set_default_size); LOAD(gtk_window_set_child); LOAD(gtk_window_present);
    LOAD(gtk_window_set_icon_name);
    LOAD(gtk_window_set_resizable);
    LOAD(gtk_window_is_maximized); LOAD(gtk_window_is_fullscreen); LOAD(gtk_native_get_surface);
    LOAD(gdk_toplevel_begin_resize); LOAD(gdk_event_get_device); LOAD(gdk_event_get_time); LOAD(gdk_event_get_position);
    LOAD(gtk_window_set_titlebar); LOAD(gtk_header_bar_new); LOAD(gtk_header_bar_pack_start);
    LOAD(gtk_window_handle_new); LOAD(gtk_window_handle_set_child); LOAD(gtk_window_controls_new); LOAD(g_object_bind_property);
    LOAD(gtk_header_bar_pack_end); LOAD(gtk_header_bar_set_title_widget);
    LOAD(gtk_box_new); LOAD(gtk_box_append); LOAD(gtk_box_remove); LOAD(gtk_widget_get_first_child); LOAD(gtk_widget_get_next_sibling);
    LOAD(gtk_widget_get_parent);
    LOAD(gtk_grid_new); LOAD(gtk_grid_attach); LOAD(gtk_grid_set_row_spacing); LOAD(gtk_grid_set_column_spacing);
    LOAD(gtk_expander_new); LOAD(gtk_expander_set_child); LOAD(gtk_expander_set_label_widget);
    LOAD(gtk_expander_set_expanded); LOAD(gtk_expander_get_expanded);
    LOAD(gtk_paned_new); LOAD(gtk_paned_set_start_child);
    LOAD(gtk_paned_set_resize_start_child); LOAD(gtk_paned_set_resize_end_child);
    LOAD(gtk_paned_set_shrink_start_child); LOAD(gtk_paned_set_shrink_end_child); LOAD(gtk_paned_set_wide_handle);
    LOAD(gtk_paned_set_end_child); LOAD(gtk_paned_set_position); LOAD(gtk_scrolled_window_new);
    LOAD(gtk_scrolled_window_set_child); LOAD(gtk_scrolled_window_set_policy);
    LOAD(gtk_scrolled_window_set_max_content_height); LOAD(gtk_scrolled_window_set_propagate_natural_height);
    LOAD(gtk_scrolled_window_get_vadjustment); LOAD(gtk_scrolled_window_set_vadjustment);
    LOAD(gtk_widget_set_hexpand); LOAD(gtk_widget_set_vexpand);
    LOAD(gtk_widget_set_size_request); LOAD(gtk_widget_set_visible); LOAD(gtk_widget_get_visible);
    LOAD(gtk_widget_set_sensitive); LOAD(gtk_widget_set_valign); LOAD(gtk_widget_set_halign);
    LOAD(gtk_widget_grab_focus);
    LOAD(gtk_menu_button_set_label); LOAD(gtk_menu_button_set_always_show_arrow);
    LOAD(gtk_text_iter_get_line_offset); LOAD(gtk_text_iter_set_line_offset); LOAD(gtk_text_iter_forward_to_line_end);
    LOAD(gtk_text_iter_ends_line); LOAD(gtk_text_iter_backward_char); LOAD(gtk_text_buffer_begin_user_action); LOAD(gtk_text_buffer_end_user_action); LOAD(gtk_text_buffer_undo);
    LOAD(gtk_widget_set_can_focus); LOAD(gtk_widget_pick); LOAD(gtk_event_controller_focus_new);
    LOAD(gtk_editable_set_editable); LOAD(gtk_editable_get_editable); LOAD(gtk_editable_select_region);
    LOAD(gtk_widget_add_css_class); LOAD(gtk_widget_remove_css_class); LOAD(gtk_widget_has_css_class);
    LOAD(gtk_widget_set_tooltip_text);
    LOAD(gtk_widget_set_cursor_from_name); LOAD(gtk_event_controller_get_current_event); LOAD(gtk_gesture_set_state);
    LOAD(gtk_widget_get_width); LOAD(gtk_widget_measure); LOAD(gtk_label_new);
    LOAD(gtk_widget_add_controller); LOAD(gtk_event_controller_key_new); LOAD(gtk_event_controller_set_propagation_phase);
    LOAD(gtk_gesture_click_new); LOAD(gtk_gesture_single_set_button);
    LOAD(gtk_widget_set_parent); LOAD(gtk_widget_unparent);
    LOAD(gtk_popover_set_has_arrow); LOAD(gtk_popover_new); LOAD(gtk_popover_set_child); LOAD(gtk_popover_set_pointing_to); LOAD(gtk_popover_popup); LOAD(gtk_popover_popdown);
    LOAD(gtk_drop_target_new); LOAD(gtk_drag_source_new); LOAD(gtk_drag_source_set_actions);
    LOAD(gdk_file_list_get_type); LOAD(g_value_get_boxed); LOAD(g_value_get_string); LOAD(g_type_from_name);
    LOAD(gdk_content_provider_new_typed);
    LOAD(gtk_label_set_text); LOAD(gtk_label_set_xalign); LOAD(gtk_label_set_wrap); LOAD(gtk_entry_new); LOAD(gtk_search_entry_new);
    LOAD(gtk_search_entry_set_placeholder_text);
    LOAD(gtk_entry_set_placeholder_text); LOAD(gtk_editable_get_text); LOAD(gtk_editable_set_text);
    LOAD(gtk_button_new_with_label); LOAD(gtk_button_new_from_icon_name); LOAD(gtk_button_set_child);
    LOAD(gtk_menu_button_new); LOAD(gtk_menu_button_set_icon_name); LOAD(gtk_menu_button_set_popover);
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
    LOAD(gtk_text_buffer_get_tag_table); LOAD(gtk_text_tag_table_lookup);
    LOAD(gtk_text_buffer_apply_tag_by_name);
    LOAD(gtk_text_buffer_remove_all_tags);
    LOAD(gtk_text_buffer_set_enable_undo);
    LOAD(gtk_text_buffer_create_child_anchor); LOAD(gtk_text_view_add_child_at_anchor);
    LOAD(gtk_picture_new_for_filename); LOAD(gtk_picture_set_content_fit);
    LOAD(gtk_text_view_set_editable); LOAD(gtk_text_view_set_cursor_visible); LOAD(gtk_stack_new);
    LOAD(gtk_stack_set_hhomogeneous); LOAD(gtk_stack_set_vhomogeneous);
    LOAD(gtk_stack_add_titled); LOAD(gtk_stack_set_visible_child_name); LOAD(gtk_stack_get_visible_child_name); LOAD(gtk_stack_switcher_new);
    LOAD(gtk_stack_switcher_set_stack); LOAD(gtk_drop_down_new_from_strings); LOAD(gtk_drop_down_set_selected);
    LOAD(gtk_drop_down_get_selected); LOAD(g_free); LOAD(g_error_free);
    LOAD(g_timeout_add); LOAD(g_idle_add); LOAD(g_source_remove); LOAD(gtk_file_chooser_native_new);
    LOAD(gtk_label_set_ellipsize); LOAD(gtk_label_set_max_width_chars); LOAD(gtk_editable_set_width_chars);
    LOAD(gtk_file_chooser_get_file); LOAD(g_file_get_path); LOAD(gtk_native_dialog_show);
    LOAD(gtk_native_dialog_set_modal); LOAD(gtk_file_filter_new); LOAD(gtk_file_filter_set_name); LOAD(gtk_file_filter_add_pattern);
    LOAD(gtk_file_filter_add_mime_type); LOAD(gtk_file_chooser_add_filter); LOAD(gtk_file_chooser_set_filter); LOAD(g_object_ref_sink);
    LOAD(gtk_file_chooser_set_current_name);
    LOAD(g_file_new_for_path); LOAD(g_file_trash);
    LOAD(cairo_pdf_surface_create); LOAD(cairo_create); LOAD(cairo_select_font_face);
    LOAD(cairo_set_font_size); LOAD(cairo_move_to); LOAD(cairo_show_text); LOAD(cairo_show_page);
    LOAD(cairo_destroy); LOAD(cairo_surface_finish); LOAD(cairo_surface_destroy); LOAD(cairo_surface_status);
    LOAD(cairo_save); LOAD(cairo_restore); LOAD(cairo_set_source_rgb); LOAD(cairo_rectangle);
    LOAD(cairo_fill); LOAD(cairo_set_line_width); LOAD(cairo_line_to); LOAD(cairo_stroke);
    LOAD(cairo_translate); LOAD(cairo_scale); LOAD(cairo_paint);
    LOAD(cairo_image_surface_create_from_png); LOAD(cairo_image_surface_create); LOAD(cairo_image_surface_get_width); LOAD(cairo_image_surface_get_height);
    LOAD(cairo_surface_set_mime_data); LOAD(cairo_set_source_surface); LOAD(cairo_pdf_surface_set_metadata);
    LOAD(cairo_pdf_surface_add_outline); LOAD(cairo_tag_begin); LOAD(cairo_tag_end); LOAD(g_unichar_to_utf8);
    LOAD(g_uri_unescape_string); LOAD(pango_cairo_create_layout); LOAD(pango_cairo_context_set_resolution); LOAD(pango_cairo_show_layout);
    LOAD(pango_cairo_show_layout_line); LOAD(pango_layout_get_context); LOAD(pango_layout_set_width); LOAD(pango_layout_set_wrap);
    LOAD(pango_layout_set_alignment); LOAD(pango_layout_set_markup); LOAD(pango_layout_set_text); LOAD(pango_layout_set_font_description);
    LOAD(pango_font_description_from_string); LOAD(pango_font_description_set_absolute_size); LOAD(pango_font_description_free); LOAD(pango_layout_get_size);
    LOAD(pango_layout_get_line_count); LOAD(pango_layout_get_iter); LOAD(pango_layout_iter_get_line_readonly); LOAD(pango_layout_iter_get_line_extents);
    LOAD(pango_layout_iter_get_baseline); LOAD(pango_layout_iter_next_line); LOAD(pango_layout_iter_free); LOAD(pango_layout_line_get_start_index);
    LOAD(pango_layout_line_get_length); LOAD(pango_layout_line_get_x_ranges); LOAD(gdk_pixbuf_new_from_file_at_scale); LOAD(gdk_pixbuf_get_width);
    LOAD(gdk_pixbuf_get_height); LOAD(gdk_cairo_set_source_pixbuf);
    LOAD(gtk_image_new_from_file); LOAD(gtk_image_new_from_icon_name); LOAD(gtk_image_set_pixel_size);
    LOAD(gtk_widget_set_margin_start);
    LOAD(gtk_widget_set_margin_end); LOAD(gtk_widget_set_margin_top); LOAD(gtk_widget_set_margin_bottom);
    LOAD(gtk_separator_new); LOAD(gtk_css_provider_new); LOAD(gtk_css_provider_load_from_path); LOAD(gtk_css_provider_load_from_string);
    LOAD(gdk_display_get_default); LOAD(gdk_display_get_clipboard); LOAD(gdk_clipboard_get_formats);
    LOAD(gdk_content_formats_contain_gtype); LOAD(gdk_texture_get_type); LOAD(gdk_clipboard_read_texture_async);
    LOAD(gdk_clipboard_read_texture_finish); LOAD(gdk_texture_save_to_png); LOAD(gtk_style_context_add_provider_for_display);
    LOAD(gtk_style_context_remove_provider_for_display);
}

void margins(Ptr widget, int horizontal, int vertical) {
    gtk_widget_set_margin_start(widget, horizontal); gtk_widget_set_margin_end(widget, horizontal);
    gtk_widget_set_margin_top(widget, vertical); gtk_widget_set_margin_bottom(widget, vertical);
}

Ptr symbolic_image(const char *icon) {
    if (strcmp(icon, "insert-table-symbolic") == 0) {
        char path[PATH_MAX]; snprintf(path, sizeof path, "%s/assets/notemd-insert-table-symbolic.svg", state.data_dir);
        return gtk_image_new_from_file(path);
    }
    return gtk_image_new_from_icon_name(icon);
}

Ptr icon_button(const char *icon, const char *tooltip) {
    Ptr button = gtk_button_new_from_icon_name(icon); gtk_widget_set_tooltip_text(button, tooltip);
    if (strcmp(icon, "insert-table-symbolic") == 0) gtk_button_set_child(button, symbolic_image(icon));
    gtk_widget_add_css_class(button, "icon-button"); return button;
}

Ptr navigation_button(const char *icon, const char *text) {
    Ptr button = gtk_button_new_with_label(""); Ptr row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    Ptr image = symbolic_image(icon); Ptr label = gtk_label_new(text); gtk_label_set_xalign(label, 0.0f);
    gtk_label_set_ellipsize(label, 3); gtk_label_set_max_width_chars(label, 24);
    gtk_widget_set_hexpand(label, true); gtk_box_append(row, image); gtk_box_append(row, label); gtk_button_set_child(button, row);
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

void apply_theme(const char *theme);

void config_path(char output[PATH_MAX]) {
    const char *config = getenv("XDG_CONFIG_HOME"), *home = getenv("HOME");
    if (config && *config) snprintf(output, PATH_MAX, "%s/notemd/settings.conf", config);
    else snprintf(output, PATH_MAX, "%s/.config/notemd/settings.conf", home ? home : ".");
}

void save_settings(void) {
    char path[PATH_MAX], directory[PATH_MAX]; config_path(path);
    snprintf(directory, sizeof directory, "%s", path); char *slash = strrchr(directory, '/');
    if (slash) { *slash = '\0'; make_directory(directory); }
    size_t size = PATH_MAX + 256;
    for (Note *note = state.notes; note; note = note->next) if (note->external) size += strlen(note->markdown_path) + 12;
    char *contents = calloc(size, 1); size_t used = (size_t)snprintf(contents, size,
        "root=%s\ntheme=%s\nlanguage=%s\nautosave=%d\neditor_font_size=%d\n", state.root, state.theme, state.language,
        state.autosave ? 1 : 0, state.editor_font_size);
    for (Note *note = state.notes; note && used < size; note = note->next) if (note->external)
        used += (size_t)snprintf(contents + used, size - used, "external=%s\n", note->markdown_path);
    write_atomic(path, contents); free(contents);
}

void load_settings(void) {
    snprintf(state.theme, sizeof state.theme, "system");
    snprintf(state.language, sizeof state.language, "pt");
    state.autosave = true;
    state.editor_font_size = EDITOR_FONT_DEFAULT;
    char path[PATH_MAX]; config_path(path); char *contents = read_file(path), *line = contents;
    while (line && *line) {
        char *next = strchr(line, '\n'); if (next) *next = '\0';
        if (strncmp(line, "root=", 5) == 0) snprintf(state.root, sizeof state.root, "%s", line + 5);
        else if (strncmp(line, "theme=", 6) == 0 && line[6])
            snprintf(state.theme, sizeof state.theme, "%s", line + 6);
        else if (strncmp(line, "autosave=", 9) == 0) state.autosave = line[9] != '0';
        else if (strncmp(line, "language=", 9) == 0 && line[9]) snprintf(state.language, sizeof state.language, "%s", line + 9);
        else if (strncmp(line, "editor_font_size=", 17) == 0) {
            char *end; long size = strtol(line + 17, &end, 10);
            if (end != line + 17 && !*end && size >= EDITOR_FONT_MIN && size <= EDITOR_FONT_MAX)
                state.editor_font_size = (int)size;
        }
        else if (strncmp(line, "external=", 9) == 0 && state.external_count < 32)
            snprintf(state.external_paths[state.external_count++], PATH_MAX, "%s", line + 9);
        line = next ? next + 1 : NULL;
    }
    free(contents);
}

bool is_system_dark(void) {
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

void color_buffer_tag(Ptr buffer, const char *name, const char *foreground, const char *background) {
    if (!buffer) return;
    Ptr tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(buffer), name);
    if (!tag) return;
    if (foreground) g_object_set(tag, "foreground", foreground, NULL);
    if (background) g_object_set(tag, "background", background, NULL);
}


void apply_preview_theme(Ptr extra_buffer) {
    const char *heading = "#8db8ff", *code = "#c7a1ff", *emphasis = "#f0bd79";
    const char *code_background = "#2d3847", *link = "#76d5df", *list = "#9bdaa0", *quote = "#b2bfd2";
    bool use_light = source_is_light();
    if (use_light) {
        heading = "#245fbc"; code = "#7b49b5"; emphasis = "#9b4c16";
        code_background = "#e8edf3"; link = "#146d7b"; list = "#217849"; quote = "#53667f";
        if (!strcmp(state.theme, "solarized-light")) {
            heading = "#256b87"; code = "#78509b"; emphasis = "#9d4c27";
            code_background = "#eae4d1"; link = "#187272"; list = "#577423"; quote = "#53666a";
        } else if (!strcmp(state.theme, "catppuccin-latte")) {
            heading = "#8839ef"; code = "#d20f39"; emphasis = "#9b4b0a";
            code_background = "#e6e9ef"; link = "#1e66f5"; list = "#287d1e"; quote = "#5c5f77";
        }
    } else if (!strcmp(state.theme, "dracula")) {
        heading = "#bd93f9"; code = "#ff94cc"; emphasis = "#ffc688"; code_background = "#36384c";
        link = "#8be9fd"; list = "#a1e8b1"; quote = "#c1b9d7";
    } else if (!strcmp(state.theme, "monokai")) {
        heading = "#a9dc76"; code = "#c5adff"; emphasis = "#ffd08a"; code_background = "#383f2f";
        link = "#78dce8"; list = "#a9dc76"; quote = "#bfc5ad";
    } else if (!strcmp(state.theme, "tokyo-night")) {
        heading = "#82aaff"; code = "#c099ff"; emphasis = "#ffc777"; code_background = "#2a3650";
        link = "#86e1fc"; list = "#b4e08b"; quote = "#b2bfdc";
    }
    Ptr scheme = source_scheme(use_light), body_style = gtk_source_style_scheme_get_style(scheme, "text");
    char *body = NULL; if (body_style) g_object_get(body_style, "foreground", &body, NULL);
    Ptr previews[] = {state.preview_buffer, state.split_preview_buffer, extra_buffer};
    for (size_t index = 0; index < sizeof previews / sizeof previews[0]; index++) {
        Ptr buffer = previews[index];
        color_buffer_tag(buffer, "h1", heading, NULL);
        color_buffer_tag(buffer, "h2", link, NULL);
        color_buffer_tag(buffer, "h3", code, NULL);
        color_buffer_tag(buffer, "strong", emphasis, NULL);
        color_buffer_tag(buffer, "emphasis", code, NULL);
        color_buffer_tag(buffer, "list", list, NULL);
        color_buffer_tag(buffer, "code", body, NULL);
        color_buffer_tag(buffer, "code-header", heading, NULL);
        color_buffer_tag(buffer, "inline-code", code, code_background);
        color_buffer_tag(buffer, "quote", quote, NULL);
        color_buffer_tag(buffer, "done", quote, NULL);
        color_buffer_tag(buffer, "strike", quote, NULL);
        color_buffer_tag(buffer, "link", link, NULL);
        color_buffer_tag(buffer, "divider", quote, NULL);
        color_buffer_tag(buffer, "table-header", heading, code_background);
        const char *panels[] = {"code", "code-header", "code-footer", "table-header"};
        for (size_t panel = 0; buffer && panel < sizeof panels / sizeof panels[0]; panel++) {
            Ptr tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(buffer), panels[panel]);
            if (tag) g_object_set(tag, "paragraph-background", code_background, NULL);
        }
        color_buffer_tag(buffer, "text-red", use_light ? "#bb3447" : "#ff8b9a", NULL);
        color_buffer_tag(buffer, "text-orange", use_light ? "#a64e18" : "#ffb27c", NULL);
        color_buffer_tag(buffer, "text-yellow", use_light ? "#886512" : "#edd17b", NULL);
        color_buffer_tag(buffer, "text-green", use_light ? "#217849" : "#8bd9aa", NULL);
        color_buffer_tag(buffer, "text-blue", use_light ? "#245fbc" : "#8db8ff", NULL);
        color_buffer_tag(buffer, "text-purple", use_light ? "#7b49b5" : "#c7a1ff", NULL);
    }
    g_free(body); color_buffer_tag(state.line_buffer, "active-line", heading, code_background);
}

void apply_syntax_theme(void) {
    apply_source_scheme(source_is_light());
    apply_preview_theme(NULL);
    /* Syntax runs in rendered code are regenerated when the palette changes. */
    if (state.editor_buffer && (state.preview_buffer || state.split_preview_buffer)) {
        char *markdown = editor_text();
        if (state.preview_buffer) render_markdown(state.preview_buffer, markdown);
        if (state.split_preview_buffer) render_markdown(state.split_preview_buffer, markdown);
        g_free(markdown);
    }
}

void apply_theme(const char *theme) {
    if (!theme || !*theme) theme = "system";
    if (theme != state.theme) snprintf(state.theme, sizeof state.theme, "%s", theme);

    char theme_path[PATH_MAX], style_path[PATH_MAX];
    snprintf(theme_path, sizeof theme_path, "%s/themes/%s.css", state.data_dir, theme);
    snprintf(style_path, sizeof style_path, "%s/style.css", state.data_dir);

    char *theme_css = read_file(theme_path);
    char *style_css = read_file(style_path);

    size_t theme_len = theme_css ? strlen(theme_css) : 0;
    size_t style_len = style_css ? strlen(style_css) : 0;
    char typography[2048];
    int font = state.editor_font_size;
    if (font < EDITOR_FONT_MIN || font > EDITOR_FONT_MAX) font = EDITOR_FONT_DEFAULT;
    snprintf(typography, sizeof typography,
        ".notemd-window textview, .notemd-window textview text, "
        ".notemd-window textview.markdown-source, .notemd-window textview.markdown-source text, "
        ".notemd-window textview.line-numbers, .notemd-window textview.line-numbers text, "
        ".notemd-window .block-card textview, .notemd-window .block-card textview text { font-size: %dpx; }\n"
        ".notemd-window textview.preview, .notemd-window textview.preview text { font-size: %dpx; }\n"
        ".notemd-window .block-card textview.block-code, .notemd-window .block-card textview.block-code text { font-size: %dpx; }\n"
        ".notemd-window .block-card textview.block-heading, .notemd-window .block-card textview.block-heading text { font-size: %dpx; }\n",
        font, font + 1, font, font + 4);
    /* Saturated library colors, with separate values for readable light/dark text. */
    const char *library_colors = source_is_light() ?
        "@define-color notemd_red #d50032;\n@define-color notemd_orange #b83b00;\n"
        "@define-color notemd_yellow #8f6200;\n@define-color notemd_green #00772b;\n"
        "@define-color notemd_blue #0057e6;\n@define-color notemd_purple #791fe8;\n" :
        "@define-color notemd_red #ff3159;\n@define-color notemd_orange #ff8717;\n"
        "@define-color notemd_yellow #ffcc00;\n@define-color notemd_green #0ede67;\n"
        "@define-color notemd_blue #3994ff;\n@define-color notemd_purple #b065ff;\n";
    /* Context menus deliberately use neutral surfaces, independent of desktop accents. */
    const char *menu_colors = source_is_light() ?
        "@define-color notemd_menu_surface #ffffff;\n@define-color notemd_menu_fg #303030;\n"
        "@define-color notemd_menu_border #dddddd;\n@define-color notemd_menu_hover #f0f0f0;\n"
        "@define-color notemd_menu_muted #666666;\n" :
        "@define-color notemd_menu_surface #252525;\n@define-color notemd_menu_fg #f0f0f0;\n"
        "@define-color notemd_menu_border #454545;\n@define-color notemd_menu_hover #353535;\n"
        "@define-color notemd_menu_muted #b6b6b6;\n";
    size_t combined_len = theme_len + style_len + strlen(library_colors) + strlen(menu_colors) + strlen(typography) + 16;
    char *combined = malloc(combined_len);
    snprintf(combined, combined_len, "%s\n%s\n%s\n%s\n%s", style_css ? style_css : "", theme_css ? theme_css : "", library_colors, menu_colors, typography);

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

void set_editor_font_size(int size) {
    if (size < EDITOR_FONT_MIN) size = EDITOR_FONT_MIN;
    if (size > EDITOR_FONT_MAX) size = EDITOR_FONT_MAX;
    if (size == state.editor_font_size) return;
    state.editor_font_size = size;
    apply_theme(state.theme);
}

void editor_font_changed(Ptr dropdown, Ptr unused, Ptr user_data) {
    (void)unused; (void)user_data;
    guint selected = gtk_drop_down_get_selected(dropdown);
    if (selected <= EDITOR_FONT_MAX - EDITOR_FONT_MIN) set_editor_font_size(EDITOR_FONT_MIN + (int)selected);
}


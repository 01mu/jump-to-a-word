/*
   Jump to a Word - Move the cursor to a word in Geany
   Copyright (C) 2025 01mu <github.com/01mu>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef JUMP_TO_A_WORD_H
#define JUMP_TO_A_WORD_H

#include <geanyplugin.h>

typedef struct {
    gint starting;
    gint starting_doc;
    GString *word;
    GString *shortcut;
    gint line;
    gint padding;
    gint bytes;
    gint replace_pos;
    gint replace_pos_start;
    gboolean valid_search;
    gboolean shortcut_marked;
    gboolean is_hidden_neighbor;
} Word;

typedef enum {
    INDICATOR_TAG = 2,
    INDICATOR_HIGHLIGHT = 3,
    INDICATOR_TEXT = 4,
    INDICATOR_MULTICURSOR = 5,
} Indicator;

typedef enum {
    KB_JUMP_TO_A_WORD_SHORTCUT,
    KB_JUMP_TO_A_WORD_SEARCH,
    KB_REPLACE_SEARCH,
    KB_JUMP_TO_PREVIOUS_CARET,
    KB_JUMP_TO_A_CHAR_SHORTCUT,
    KB_JUMP_TO_LINE,
    KB_OPEN_LINE_OPTIONS,
    KB_OPEN_TEXT_OPTIONS,
    KB_OPEN_REPLACE_OPTIONS,
    KB_JUMP_TO_A_SUBSTRING,
    KB_MULTICURSOR,
    KB_REPEAT_ACTION,
    KB_COUNT,
} KB;

typedef enum {
    JM_SHORTCUT_WORD,
    JM_SHORTCUT_CHAR_JUMPING,
    JM_SHORTCUT_CHAR_ACCEPTING,
    JM_SHORTCUT_CHAR_REPLACING,
    JM_SEARCH,
    JM_REPLACE_SEARCH,
    JM_SUBSTRING,
    JM_REPLACE_SUBSTRING,
    JM_LINE,
    JM_INSERTING_LINE,
    JM_INSERTING_LINE_MULTICURSOR,
    JM_REPLACE_MULTICURSOR,
    JM_TRANSPOSE_MULTICURSOR,
    JM_NONE,
} JumpMode;

typedef enum {
    MC_DISABLED,
    MC_ACCEPTING,
} MulticusrorMode;

typedef enum {
    TX_DO_NOTHING,
    TX_SELECT_TEXT,
    TX_SELECT_TO_TEXT,
    TX_SELECT_TEXT_RANGE,
    TX_COUNT,
} TextAfter;

typedef enum {
    LA_DO_NOTHING,
    LA_SELECT_LINE,
    LA_SELECT_TO_LINE,
    LA_SELECT_LINE_RANGE,
    LA_JUMP_TO_WORD_SHORTCUT,
    LA_JUMP_TO_CHARACTER_SHORTCUT,
    LA_JUMP_TO_WORD_SEARCH,
    LA_JUMP_TO_SUBSTRING_SEARCH,
    LA_COUNT,
} LineAfter;

typedef enum {
    RA_REPLACE,
    RA_INSERT_START,
    RA_INSERT_END,
    RA_INSERT_PREVIOUS_LINE,
    RA_INSERT_NEXT_LINE,
    RA_TRANSPOSE_STRING,
    RA_COUNT,
} ReplaceAction;

typedef enum {
    SOURCE_SETTINGS_CHANGE,
    SOURCE_OPTION_MENU,
} SettingSource;

typedef enum {
    OM_TEXT,
    OM_LINE,
    OM_REPLACE,
} OptionMod;

typedef gboolean (*KeyPressCallback)(GtkWidget *, GdkEventKey *, gpointer);
typedef gboolean (*ClickCallback)(GtkWidget *, GdkEventButton *, gpointer);

typedef struct {
    gboolean move_marker_to_line;
    gboolean center_shortcut;
    gboolean wait_for_enter;
    gboolean only_tag_current_line;
    gboolean hide_word_shortcut_jump;
    gboolean cancel_on_mouse_move;
    gboolean search_start_from_beginning;
    gboolean search_case_sensitive;
    gboolean shortcut_all_caps;
    gboolean wrap_search;
    gboolean show_annotations;
    gboolean use_selected_word_or_char;
    gboolean search_from_selection;
    gboolean match_whole_word;
    gboolean search_selection_if_line;
    gboolean shortcuts_include_single_char;
    gboolean select_when_shortcut_char;
    gboolean search_smart_case;
    gboolean instant_transpose;
    gboolean disable_live_replace;

    gint tag_color;
    gint text_color;
    gint highlight_color;
    gint search_annotation_bg_color;

    LineAfter line_after;
    TextAfter text_after;
    ReplaceAction replace_action;
} Settings;

typedef struct {
    GtkWidget *move_marker_to_line;
    GtkWidget *center_shortcut;
    GtkWidget *wait_for_enter;
    GtkWidget *only_tag_current_line;
    GtkWidget *hide_word_shortcut_jump;
    GtkWidget *cancel_on_mouse_move;
    GtkWidget *search_start_from_beginning;
    GtkWidget *search_case_sensitive;
    GtkWidget *shortcut_all_caps;
    GtkWidget *wrap_search;
    GtkWidget *show_annotations;
    GtkWidget *use_current_word;
    GtkWidget *use_selected_word_or_char;
    GtkWidget *search_from_selection;
    GtkWidget *match_whole_word;
    GtkWidget *search_selection_if_line;
    GtkWidget *tag_color;
    GtkWidget *text_color;
    GtkWidget *highlight_color;
    GtkWidget *search_annotation_bg_color;
    GtkWidget *shortcuts_include_single_char;
    GtkWidget *line_after;
    GtkWidget *text_after;
    GtkWidget *replace_action;
    GtkWidget *select_when_shortcut_char;
    GtkWidget *search_smart_case;
    GtkWidget *instant_transpose;
    GtkWidget *disable_live_replace;
} Widgets;

typedef struct {
    GdkColor tag_color_gdk;
    GdkColor text_color_gdk;
    GdkColor highlight_color_gdk;
    GdkColor search_annotation_bg_color_gdk;
} Colors;

typedef struct {
    GtkWidget *panel;
    GtkWidget *entry;
    GtkWidget *view;
    GtkListStore *store;
    GtkTreeModel *sort;
    GtkTreePath *last_path;
} TextLineWindow;

typedef struct {
    gint *previous_cursor_pos;
    GtkWidget *submenu;
} PCMenuSensitivity;

typedef struct {
    gboolean *has_previous_action;
    GtkWidget *submenu;
} PAMenuSensitivity;

typedef struct {
    Settings *config_settings;
    Widgets *config_widgets;
    Colors *gdk_colors;
    TextLineWindow *tl_window;

    ScintillaObject *sci;
    GtkWidget *main_menu_item;
    gchar *config_file;
    GeanyData *geany_data;

    GtkCheckMenuItem *multicursor_menu_checkbox;
    gulong multicursor_menu_checkbox_signal_id;

    PCMenuSensitivity *pc_menu_sensitivity;
    PAMenuSensitivity *pa_menu_sensitivity;

    gint wrapped_lines;
    gint first_line_on_screen;
    gint lines_on_screen;
    gint last_line_on_screen;
    gint first_position;
    gint last_position;

    gint cursor_moved_to_eol;
    gint current_cursor_pos;
    gint previous_cursor_pos;

    GString *cache;
    GString *buffer;
    GString *replace_cache;

    GArray *markers;
    GArray *words;
    GArray *multicursor_words;
    GArray *searched_words_for_line_insert;

    GArray *lf_positions;

    gboolean replace_instant;

    gboolean range_is_set;
    gint range_first_pos;
    gint range_word_length;

    GString *search_query;
    gint search_results_count;
    gint shortcut_single_pos;

    GString *eol_message;
    gint eol_message_line;

    gint search_word_pos;
    gint search_word_pos_first;
    gint search_word_pos_last;

    gboolean cursor_in_word;
    gboolean search_change_made;
    gint replace_len;

    gulong paste_key_release_id;
    gulong kp_handler_id;
    gulong click_handler_id;

    gboolean in_selection;
    gboolean selection_is_a_word;
    gboolean selection_is_a_char;
    gboolean selection_is_within_a_line;
    gint selection_start;
    gint selection_end;

    gboolean option_mod;
    JumpMode current_mode;

    MulticusrorMode multicursor_mode;
    GString *multicursor_eol_message;
    gint multicusor_eol_message_line;
    gint multicursor_first_pos;
    gint multicursor_last_pos;

    GString *replace_query;

    GString *previous_search_query;
    GString *previous_replace_query;
    JumpMode previous_mode;
    ReplaceAction previous_replace_action;
    gboolean has_previous_action;

    gint added_new_line_insert;

    gchar *clipboard_text;
    gboolean inserting_clipboard;

    gboolean waiting_after_single_instance;
} ShortcutJump;

#endif

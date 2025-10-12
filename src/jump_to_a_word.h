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
    gboolean replacing;
    gboolean valid_search;
    gboolean shortcut_marked;
    gboolean is_hidden_neighbor;
} Word;

typedef enum {
    INDICATOR_TAG,
    INDICATOR_HIGHLIGHT,
    INDICATOR_TEXT,
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
    KB_JUMP_TO_A_SUBSTRING,
    KB_COUNT,
} KB;

typedef enum {
    JM_SEARCH,
    JM_SHORTCUT,
    JM_REPLACE_SEARCH,
    JM_SHORTCUT_CHAR_JUMPING,
    JM_SHORTCUT_CHAR_WAITING,
    JM_SHORTCUT_CHAR_REPLACING,
    JM_LINE,
    JM_SUBSTRING,
    JM_REPLACE_SUBSTRING,
    JM_NONE,
} JumpMode;

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
    LA_COUNT,
} LineAfter;

typedef enum {
    SOURCE_SETTINGS_CHANGE,
    SOURCE_OPTION_MENU,
} SettingSource;

typedef enum {
    OM_TEXT,
    OM_LINE,
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
    gboolean search_case_sensitive_smart_case;

    gint tag_color;
    gint text_color;
    gint highlight_color;

    gint tag_color_store_style;
    gint tag_color_store_fore;
    gint tag_color_store_outline;
    gint text_color_store_style;
    gint text_color_store_fore;
    gint text_color_store_outline;
    gint highlight_color_store_style;
    gint highlight_color_store_fore;
    gint highlight_color_store_outline;

    gint search_annotation_bg_color;
    LineAfter line_after;
    TextAfter text_after;
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
    GtkWidget *select_when_shortcut_char;
    GtkWidget *search_case_sensitive_smart_case;
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
    Settings *config_settings;
    Widgets *config_widgets;
    Colors *gdk_colors;
    TextLineWindow *tl_window;

    ScintillaObject *sci;
    GtkWidget *main_menu_item;
    gchar *config_file;
    GeanyData *geany_data;

    gint first_line_on_screen;
    gint lines_on_screen;
    gint last_line_on_screen;
    gint first_position;
    gint last_position;

    gint current_cursor_pos;
    gint previous_cursor_pos;

    GString *cache;         // original text on screen before shortcuts are displayed
    GString *buffer;        // text with shortcuts positioned over words
    GString *replace_cache; // the cached text being updated when chars are added or removed during replacement

    GArray *markers;      // the markers on the screen
    GArray *words;        // every word on screen
    GArray *lf_positions; /* the number of times a "\n" was shifted forward before a line
line        original string len: 9  replaced (pattern len of 2 overwrites [LF] so we shift) len: 12     total LF shifts
1           a[LF]                   DE[LF]                                                               1
2           a[LF]                   DF[LF]                                                               2
3           aa[LF]                  DG[LF] (no shift)                                                    2
4           a[LF]                   DH[LF]                                                               3
5                                                                                                        3
This is necessary because we need to know how many LFs were shifted up until a certain line in order to accurately
set the cursor position after clicking somewhere on the screen to cancel a shortcut jump.
*/

    gboolean replace_instant; // whether we are performing an instant replace

    gboolean line_range_set;     // whether we are performing a line range selection jump
    gint line_range_first;       // the first line in the selection
    gint text_range_word_length; // the length of the first word used in a selection range

    GString *search_query;     // query used during a jump
    gint search_results_count; // marked words during search
    gint shortcut_single_pos;  // index of highlighted word

    GString *eol_message;  // end of line annotation message used during a word search
    gint eol_message_line; // line end of line annotation will appear on

    gint search_word_pos;       // index of currently highlighted word during a word search
    gint search_word_pos_first; // index of first word in words array that matches a query
    gint search_word_pos_last;  // index of last word in words array that matches a query

    gboolean cursor_in_word;     // whether the cursor appears within a word (used for resetting the cursor position)
    gboolean search_change_made; // check if a change has been made to the search string
    gint replace_len;            // the length of the replacement for a set of serach words

    gboolean delete_added_bracket;

    gulong kp_handler_id;
    gulong click_handler_id;

    gboolean in_selection;
    gboolean selection_is_a_word;
    gboolean selection_is_a_char;
    gboolean selection_is_a_line;
    gint selection_start;
    gint selection_end;

    gboolean option_mod;
    JumpMode current_mode;
} ShortcutJump;

#endif

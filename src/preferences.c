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

#include <plugindata.h>

#include "jump_to_a_word.h"

/**
 * @brief Returns the int value from a GTK color type. Used when updaing the settings file. The color is in BGR.
 *
 * @param const GdkColor *color: GTK color struct

 * @return guint32 val: Integer value of the color
 */
static guint32 configure_color_to_int(const GdkColor *color) {
    return (((color->blue / 0x101) << 16) | ((color->green / 0x101) << 8) | ((color->red / 0x101) << 0));
}

/**
 * @brief Updates the settings file. This can either be triggered from the main configuration window or when changing
 * the line or text action from their menus.
 *
 * @param SettingSource source: Either from configration or from a line or text action change
 * @param ShortcutJump *sj: The plugin object
 */

void update_settings(SettingSource source, ShortcutJump *sj) {
#define UPDATE_BOOL(name, name_str, category)                                                                          \
    G_STMT_START {                                                                                                     \
        if (source == SOURCE_SETTINGS_CHANGE) {                                                                        \
            sj->config_settings->name = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(sj->config_widgets->name));     \
        }                                                                                                              \
                                                                                                                       \
        g_key_file_set_boolean(config, category, name_str, sj->config_settings->name);                                 \
    }                                                                                                                  \
    G_STMT_END

#define UPDATE_INTEGER(name, name_str, category)                                                                       \
    G_STMT_START {                                                                                                     \
        if (source == SOURCE_SETTINGS_CHANGE) {                                                                        \
            sj->config_settings->name = gtk_combo_box_get_active(GTK_COMBO_BOX(sj->config_widgets->name));             \
        }                                                                                                              \
                                                                                                                       \
        g_key_file_set_integer(config, category, name_str, sj->config_settings->name);                                 \
    }                                                                                                                  \
    G_STMT_END

#define UPDATE_COLOR(name, name_str, gdk)                                                                              \
    G_STMT_START {                                                                                                     \
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS                                                                               \
        if (source == SOURCE_SETTINGS_CHANGE) {                                                                        \
            gtk_color_button_get_color(GTK_COLOR_BUTTON(sj->config_widgets->name), &sj->gdk_colors->gdk);              \
            sj->config_settings->name = configure_color_to_int(&sj->gdk_colors->gdk);                                  \
        }                                                                                                              \
                                                                                                                       \
        g_key_file_set_integer(config, "colors", name_str, sj->config_settings->name);                                 \
        G_GNUC_END_IGNORE_DEPRECATIONS                                                                                 \
    }                                                                                                                  \
    G_STMT_END

    GKeyFile *config = g_key_file_new();
    gchar *config_dir = g_path_get_dirname(sj->config_file);
    gchar *data;

    g_key_file_load_from_file(config, sj->config_file, G_KEY_FILE_NONE, NULL);

    UPDATE_BOOL(show_annotations, "show_annotations", "general");
    UPDATE_BOOL(use_selected_word_or_char, "use_selected_word_or_char", "general");
    UPDATE_BOOL(wait_for_enter, "wait_for_enter", "general");
    UPDATE_BOOL(only_tag_current_line, "only_tag_current_line", "general");
    UPDATE_BOOL(move_marker_to_line, "move_marker_to_line", "general");
    UPDATE_BOOL(cancel_on_mouse_move, "cancel_on_mouse_move", "general");
    UPDATE_BOOL(search_from_selection, "search_from_selection", "general");
    UPDATE_BOOL(search_selection_if_line, "search_selection_if_line", "general");

    UPDATE_BOOL(select_when_shortcut_char, "select_when_shortcut_char", "shortcut");
    UPDATE_BOOL(shortcut_all_caps, "shortcut_all_caps", "shortcut");
    UPDATE_BOOL(shortcuts_include_single_char, "shortcuts_include_single_char", "shortcut");
    UPDATE_BOOL(hide_word_shortcut_jump, "hide_word_shortcut_jump", "shortcut");
    UPDATE_BOOL(center_shortcut, "center_shortcut", "shortcut");

    UPDATE_BOOL(wrap_search, "wrap_search", "search");
    UPDATE_BOOL(search_start_from_beginning, "search_start_from_beginning", "search");
    UPDATE_BOOL(match_whole_word, "match_whole_word", "search");
    UPDATE_BOOL(search_case_sensitive, "search_case_sensitive", "search");
    UPDATE_BOOL(search_case_sensitive_smart_case, "search_case_sensitive_smart_case", "search");

    UPDATE_INTEGER(text_after, "text_after", "text_after");
    UPDATE_INTEGER(line_after, "line_after", "line_after");

    UPDATE_COLOR(text_color, "text_color", text_color_gdk);
    UPDATE_COLOR(search_annotation_bg_color, "search_annotation_bg_color", search_annotation_bg_color_gdk);
    UPDATE_COLOR(tag_color, "tag_color", tag_color_gdk);
    UPDATE_COLOR(highlight_color, "highlight_color", highlight_color_gdk);

    if (!g_file_test(config_dir, G_FILE_TEST_IS_DIR) && utils_mkdir(config_dir, TRUE) != 0) {
        dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("Plugin configuration directory could not be created."));
    } else {
        data = g_key_file_to_data(config, NULL, NULL);
        utils_write_file(sj->config_file, data);
        g_free(data);
    }

    g_free(config_dir);
    g_key_file_free(config);
}

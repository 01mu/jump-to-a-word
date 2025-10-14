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

#include "annotation.h"
#include "jump_to_a_word.h"
#include "line_options.h"
#include "preferences.h"
#include "previous_cursor.h"
#include "replace_instant.h"
#include "search_common.h"
#include "search_substring.h"
#include "search_word.h"
#include "selection.h"
#include "shortcut_char.h"
#include "shortcut_common.h"
#include "shortcut_line.h"
#include "shortcut_word.h"
#include "util.h"

/**
 * @brief Line settings used in the plugin configuration and line options windows.
 */
const struct {
    gchar *label;
    LineAfter type;
} line_conf[] = {{"Do nothing", LA_DO_NOTHING},
                 {"Select line", LA_SELECT_LINE},
                 {"Select to line", LA_SELECT_TO_LINE},
                 {"Select line range", LA_SELECT_LINE_RANGE},
                 {"Jump to word (shortcut)", LA_JUMP_TO_WORD_SHORTCUT},
                 {"Jump to character (shortcut)", LA_JUMP_TO_CHARACTER_SHORTCUT},
                 {"Jump to word (search)", LA_JUMP_TO_WORD_SEARCH}};

/**
 * @brief Text settings used in the plugin configuration and text options windows.
 */
const struct {
    gchar *label;
    TextAfter type;
} text_conf[] = {{"Do nothing", TX_DO_NOTHING},
                 {"Select text", TX_SELECT_TEXT},
                 {"Select to text", TX_SELECT_TO_TEXT},
                 {"Select text range", TX_SELECT_TEXT_RANGE}};

/**
 * @brief Provides a callback for either saving or closing a document, or quitting. This is necessary for shortcut
 * mode because we don't want to save the version of the file with the buffered shortcut text replacement. Canceling
 * the shortcut jump replaces the buffered text with the original cached text.
 *
 * @param GObject *obj: (unused)
 * @param GeanyDocument *doc: (unused)
 * @param gpointer user_data: The plugin data
 */
static void on_cancel(GObject *obj, GeanyDocument *doc, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    sj->line_range_set = FALSE;

    if (sj->current_mode == JM_SHORTCUT || sj->current_mode == JM_SHORTCUT_CHAR_JUMPING ||
        sj->current_mode == JM_LINE) {
        shortcut_cancel(sj);
    }

    if (sj->current_mode == JM_SHORTCUT_CHAR_REPLACING) {
        shortcut_char_replacing_complete(sj);
    }

    if (sj->current_mode == JM_SHORTCUT_CHAR_WAITING) {
        shortcut_char_waiting_cancel(sj);
    }

    if (sj->current_mode == JM_REPLACE_SEARCH || sj->current_mode == JM_REPLACE_SUBSTRING) {
        search_replace_complete(sj);
    }

    if (sj->current_mode == JM_SEARCH || sj->current_mode == JM_SUBSTRING) {
        search_cancel(sj);
    }
}

/**
 * @brief Provides a callback for when a reload is triggered either from a manual reload or from the file being
 * edited from an outside program. In shortcut mode we end the jump and free memory instead of canceling. This is
 * necessary because we don't want to reinsert the cached text at a certain location if we don't know what edits were
 * made from the other program.
 *
 * @param GObject *obj: (unused)
 * @param GeanyDocument *doc: (unused)
 * @param gpointer user_data: The plugin data
 */
static void on_document_reload(GObject *obj, GeanyDocument *doc, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (sj->current_mode == JM_SHORTCUT_CHAR_WAITING) {
        shortcut_char_waiting_cancel(sj);
    }

    if (sj->current_mode == JM_SHORTCUT || sj->current_mode == JM_SHORTCUT_CHAR_JUMPING ||
        sj->current_mode == JM_SHORTCUT_CHAR_REPLACING || sj->current_mode == JM_LINE) {
        shortcut_end(sj, FALSE);
    }

    if (sj->current_mode == JM_SEARCH || sj->current_mode == JM_REPLACE_SEARCH || sj->current_mode == JM_SUBSTRING ||
        sj->current_mode == JM_REPLACE_SUBSTRING) {
        search_cancel(sj);
    }
}

/**
 * @brief Provides a callback for when the editor is modified. Checks if an additional bracket was added, either by
 * Geany or the Auto-close plugin and marks it for deletion during a character jump or substring search.
 *
 * @param GObject *obj: (unused)
 * @param GeanyEditor *editor:(unused)
 * @param SCNotification *nt: Notification
 * @param gpointer user_data: The plugin object
 *
 * @return gboolean: FALSE if nothing was triggered
 */
static gboolean on_editor_notify(GObject *obj, GeanyEditor *editor, SCNotification *nt, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (nt->nmhdr.code == SCN_MODIFYATTEMPTRO) {
        return TRUE;
    }

    if (nt->modificationType & (SC_MOD_INSERTTEXT)) {
        if (sj->current_mode == JM_SHORTCUT_CHAR_WAITING || sj->current_mode == JM_SUBSTRING) {
            if (strcmp(nt->text, "}") == 0 || strcmp(nt->text, ">") == 0 || strcmp(nt->text, "]") == 0 ||
                strcmp(nt->text, "\'") == 0 || strcmp(nt->text, "\"") == 0 || strcmp(nt->text, "`") == 0 ||
                strcmp(nt->text, ")") == 0) {
                sj->delete_added_bracket = TRUE;
                return TRUE;
            }
        }
    }

    return FALSE;
}

/**
 * @brief Configures a GTK color type from an int value. Used when retrieving the value from the settings file,
 * applying the colors to indicators, and updating the color of the box in the settings panel. The color is in BGR.
 *
 * @param GdkColor *color: GTK color struct
 * @param guint32 val: Integer value of the color
 */
static void configure_color_from_int(GdkColor *color, guint32 val) {
    color->blue = ((val & 0xff0000) >> 16) * 0x101;
    color->green = ((val & 0x00ff00) >> 8) * 0x101;
    color->red = ((val & 0x0000ff) >> 0) * 0x101;
}

/**
 * @brief Sets the items that fill the menu bar listing as well as their keybindings.
 *
 * @param GeanyPlugin *plugin: Geany plugin
 * @param ShortcutJump *sj: The plugin object
 */
static void setup_menu_and_keybindings(GeanyPlugin *plugin, ShortcutJump *sj) {
#define SET_MENU_ITEM(description, callback, data)                                                                     \
    G_STMT_START {                                                                                                     \
        item = gtk_menu_item_new_with_mnemonic(_(description));                                                        \
        g_signal_connect(item, "activate", G_CALLBACK(callback), data);                                                \
        gtk_widget_show(item);                                                                                         \
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);                                                          \
    }                                                                                                                  \
    G_STMT_END

#define SET_MENU_SEPERATOR()                                                                                           \
    G_STMT_START {                                                                                                     \
        item = gtk_separator_menu_item_new();                                                                          \
        gtk_widget_show(item);                                                                                         \
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);                                                          \
    }                                                                                                                  \
    G_STMT_END

#define SET_KEYBINDING(description, name, callback, w, data, menu)                                                     \
    G_STMT_START { keybindings_set_item_full(key_group, w, 0, 0, name, _(description), menu, callback, data, NULL); }  \
    G_STMT_END

    GeanyKeyGroup *key_group = plugin_set_key_group(plugin, "jump-to-a-word", KB_COUNT, NULL);

    GtkWidget *submenu = gtk_menu_new();
    GtkWidget *item;

    sj->main_menu_item = gtk_menu_item_new_with_mnemonic("Jump to a Word");

    gtk_widget_show(submenu);
    gtk_widget_show(sj->main_menu_item);

    SET_MENU_ITEM("Jump to Word (Shortcut)", shortcut_cb, sj);
    SET_KEYBINDING("Jump to word (shortcut)", "jump_to_a_word_shortcut", shortcut_kb, KB_JUMP_TO_A_WORD_SHORTCUT, sj,
                   item);

    SET_MENU_ITEM("Jump to Character (Shortcut)", shortcut_char_cb, sj);
    SET_KEYBINDING("Jump to character (shortcut)", "jump_to_a_char_shortcut", shortcut_char_kb,
                   KB_JUMP_TO_A_CHAR_SHORTCUT, sj, item);

    SET_MENU_ITEM("Jump to Line (Shortcut)", jump_to_line_cb, sj);
    SET_KEYBINDING("Jump to line (shortcut)", "jump_to_a_line", jump_to_line_kb, KB_JUMP_TO_LINE, sj, item);

    SET_MENU_ITEM("Jump to Word (Search)", search_cb, sj);
    SET_KEYBINDING("Jump to word (search)", "jump_to_a_word_search", search_kb, KB_JUMP_TO_A_WORD_SEARCH, sj, item);

    SET_MENU_ITEM("Jump to Substring (Search)", substring_cb, sj);
    SET_KEYBINDING("Jump to substring (search)", "jump_to_a_substring", substring_kb, KB_JUMP_TO_A_SUBSTRING, sj, item);

    SET_MENU_ITEM("Jump to Previous Cursor Position", jump_to_previous_cursor_cb, sj);
    SET_KEYBINDING("Jump to previous cursor position", "jump_to_previous_cursor", jump_to_previous_cursor_kb,
                   KB_JUMP_TO_PREVIOUS_CARET, sj, item);

    SET_MENU_SEPERATOR();

    SET_MENU_ITEM("Replace Selected Text", replace_search_cb, sj);
    SET_KEYBINDING("Replace selected text", "replace_search", replace_search_kb, KB_REPLACE_SEARCH, sj, item);

    SET_MENU_SEPERATOR();

    SET_MENU_ITEM("Open Text Options Window", open_text_options_cb, sj);
    SET_KEYBINDING("Open text options window", "open_text_options", open_text_options_kb, KB_OPEN_TEXT_OPTIONS, sj,
                   item);

    SET_MENU_ITEM("Open Line Options Window", open_line_options_cb, sj);
    SET_KEYBINDING("Open line options window", "open_line_options", open_line_options_kb, KB_OPEN_LINE_OPTIONS, sj,
                   item);

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(sj->main_menu_item), submenu);
    gtk_container_add(GTK_CONTAINER(sj->geany_data->main_widgets->tools_menu), sj->main_menu_item);
}

/**
 * @brief Sets the configuration settings and defaults from the file.
 *
 * @param GeanyPlugin *plugin: The plugin
 * @param gpointer pdata: (unused)
 * @param ShortcutJump *sj: The plugin object
 *
 * @return gboolean: TRUE
 */
static gboolean setup_config_settings(GeanyPlugin *plugin, gpointer pdata, ShortcutJump *sj) {
#define SET_SETTING_BOOL(name, name_str, category, default)                                                            \
    G_STMT_START { sj->config_settings->name = utils_get_setting_boolean(config, category, name_str, default); }       \
    G_STMT_END

#define SET_SETTING_INTEGER(name, name_str, category, default)                                                         \
    G_STMT_START { sj->config_settings->name = utils_get_setting_integer(config, category, name_str, default); }       \
    G_STMT_END

#define SET_SETTING_COLOR(name, name_str, default)                                                                     \
    G_STMT_START { sj->config_settings->name = utils_get_setting_integer(config, "colors", name_str, default); }       \
    G_STMT_END

    GKeyFile *config = g_key_file_new();

    sj->config_file = g_strconcat(sj->geany_data->app->configdir, G_DIR_SEPARATOR_S, "plugins", G_DIR_SEPARATOR_S,
                                  "jump-to-a-word", G_DIR_SEPARATOR_S, "jump-to-a-word.conf", NULL);

    g_key_file_load_from_file(config, sj->config_file, G_KEY_FILE_NONE, NULL);

    SET_SETTING_BOOL(show_annotations, "show_annotations", "general", TRUE);
    SET_SETTING_BOOL(use_selected_word_or_char, "use_selected_word_or_char", "general", TRUE);
    SET_SETTING_BOOL(wait_for_enter, "wait_for_enter", "general", FALSE);
    SET_SETTING_BOOL(only_tag_current_line, "only_tag_current_line", "general", FALSE);
    SET_SETTING_BOOL(move_marker_to_line, "move_marker_to_line", "general", FALSE);
    SET_SETTING_BOOL(cancel_on_mouse_move, "cancel_on_mouse_move", "general", FALSE);
    SET_SETTING_BOOL(search_from_selection, "search_from_selection", "general", TRUE);
    SET_SETTING_BOOL(search_selection_if_line, "search_selection_if_line", "general", TRUE);

    SET_SETTING_BOOL(select_when_shortcut_char, "select_when_shortcut_char", "shortcut", FALSE);
    SET_SETTING_BOOL(shortcut_all_caps, "shortcut_all_caps", "shortcut", FALSE);
    SET_SETTING_BOOL(shortcuts_include_single_char, "shortcuts_include_single_char", "shortcut", FALSE);
    SET_SETTING_BOOL(hide_word_shortcut_jump, "hide_word_shortcut_jump", "shortcut", FALSE);
    SET_SETTING_BOOL(center_shortcut, "center_shortcut", "shortcut", FALSE);

    SET_SETTING_BOOL(wrap_search, "wrap_search", "search", TRUE);
    SET_SETTING_BOOL(search_start_from_beginning, "search_start_from_beginning", "search", TRUE);
    SET_SETTING_BOOL(match_whole_word, "match_whole_word", "search", FALSE);
    SET_SETTING_BOOL(search_case_sensitive, "search_case_sensitive", "search", TRUE);
    SET_SETTING_BOOL(search_smart_case, "search_smart_case", "search", TRUE);

    SET_SETTING_INTEGER(text_after, "text_after", "text_after", TX_SELECT_TEXT);
    SET_SETTING_INTEGER(line_after, "line_after", "line_after", LA_SELECT_TO_LINE);

    SET_SETTING_COLOR(text_color, "text_color", 0xD4D4D4);
    SET_SETTING_COLOR(search_annotation_bg_color, "search_annotation_bg_color", 0x1E1E1E);
    SET_SETTING_COLOR(tag_color, "tag_color", 0xFFFFFF);
    SET_SETTING_COLOR(highlight_color, "highlight_color", 0x00FF00);

    g_key_file_free(config);

    return TRUE;
}

/**
 * @brief Inits plugin; configures menu items, sets keybindings, and loads settings from the configuration file.
 *
 * @param GeanyPlugin *plugin: Geany plugin
 * @param gpointer pdata: The plugin data
 *
 * @return gboolean: TRUE
 */
static gboolean init(GeanyPlugin *plugin, gpointer pdata) {
    ShortcutJump *sj = (ShortcutJump *)pdata;

    setup_menu_and_keybindings(plugin, sj);
    setup_config_settings(plugin, pdata, sj);

    return TRUE;
}

/**
 * @brief Cancels jumps and frees the menu and configuration files.
 *
 * @param GeanyPlugin *plugin: (unused)
 * @param gpointer pdata: The plugin data
 */
static void cleanup(GeanyPlugin *plugin, gpointer pdata) {
    ShortcutJump *sj = (ShortcutJump *)pdata;

    if (sj->current_mode == JM_SHORTCUT || sj->current_mode == JM_SHORTCUT_CHAR_JUMPING ||
        sj->current_mode == JM_LINE) {
        shortcut_cancel(sj);
    }

    if (sj->current_mode == JM_SHORTCUT_CHAR_WAITING) {
        shortcut_char_waiting_cancel(sj);
    }

    if (sj->current_mode == JM_SHORTCUT_CHAR_REPLACING) {
        shortcut_char_replacing_cancel(sj);
    }

    if (sj->current_mode == JM_SEARCH || sj->current_mode == JM_REPLACE_SEARCH || sj->current_mode == JM_SUBSTRING ||
        sj->current_mode == JM_REPLACE_SUBSTRING) {
        search_cancel(sj);
    }

    if (sj->tl_window->panel) {
        gtk_widget_destroy(sj->tl_window->panel);
    }

    if (sj->tl_window->last_path) {
        gtk_tree_path_free(sj->tl_window->last_path);
    }

    g_free(sj->config_settings);
    g_free(sj->config_widgets);
    g_free(sj->gdk_colors);
    g_free(sj->tl_window);
    g_free(sj->config_file);

    gtk_widget_destroy(sj->main_menu_item);

    g_free(sj);
}

/**
 * @brief Provides a callback for when the settings are updated from the settings panel.
 *
 * @param GtkDialog *dialog: (unused)
 * @param gint *response: Response type is OK or APPLY
 * @param gpointer user_data: The plugin data
 */
static void configure_response_cb(GtkDialog *dialog, gint response, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY) {
        update_settings(SOURCE_SETTINGS_CHANGE, sj);
    }
}

/**
 * @brief Toggles availability of "Even if it only exists on a single line".
 *
 * @param GtkToggleButton *toggle_button: The button that triggers the toggle
 * @param gpointer data: The dialog
 */
static void single_line_toggle_cb(GtkToggleButton *toggle_button, gpointer data) {
    gtk_widget_set_sensitive(g_object_get_data(G_OBJECT(data), "search_selection_if_line"),
                             gtk_toggle_button_get_active(toggle_button));
}

/**
 * @brief Toggles availability of "Smart casing".
 *
 * @param GtkToggleButton *toggle_button: The button that triggers the toggle
 * @param gpointer data: The dialog
 */
static void smart_case_toggle_cb(GtkToggleButton *toggle_button, gpointer data) {
    gtk_widget_set_sensitive(g_object_get_data(G_OBJECT(data), "search_smart_case"),
                             gtk_toggle_button_get_active(toggle_button));
}

/**
 * @brief Creates and displays the configuration menu.
 *
 * @param GeanyPlugin *plugin: The plugin
 * @param GtkDialog *dialog: Set dialog for menu
 * @param gpointer pdata: The plugin object
 *
 * @return GtkWidget *: Pointer to configuration menu container
 */
static GtkWidget *configure(GeanyPlugin *plugin, GtkDialog *dialog, gpointer pdata) {
    ShortcutJump *sj = (ShortcutJump *)pdata;

#define HORIZONTAL_FRAME()                                                                                             \
    G_STMT_START {                                                                                                     \
        hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);                                                             \
        gtk_box_set_homogeneous(GTK_BOX(hbox), TRUE);                                                                  \
        gtk_container_add(GTK_CONTAINER(vbox), hbox);                                                                  \
    }                                                                                                                  \
    G_STMT_END

#define WIDGET_FRAME(description, orientation)                                                                         \
    G_STMT_START {                                                                                                     \
        container = gtk_box_new(orientation, 0);                                                                       \
        frame = gtk_frame_new(NULL);                                                                                   \
        gtk_frame_set_label(GTK_FRAME(frame), description);                                                            \
        gtk_container_add(GTK_CONTAINER(frame), container);                                                            \
        gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);                                                     \
    }                                                                                                                  \
    G_STMT_END

#define WIDGET_FRAME_COLOR(description)                                                                                \
    G_STMT_START {                                                                                                     \
        container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);                                                          \
        frame = gtk_frame_new(NULL);                                                                                   \
        gtk_frame_set_label(GTK_FRAME(frame), description);                                                            \
        gtk_container_add(GTK_CONTAINER(frame), container);                                                            \
    }                                                                                                                  \
    G_STMT_END

#define WIDGET_CONF_BOOL(name, description, tooltip)                                                                   \
    G_STMT_START {                                                                                                     \
        sj->config_widgets->name = gtk_check_button_new_with_label(description);                                       \
        gtk_widget_set_tooltip_text(sj->config_widgets->name, tooltip);                                                \
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sj->config_widgets->name), sj->config_settings->name);          \
        gtk_box_pack_start(GTK_BOX(container), sj->config_widgets->name, FALSE, FALSE, 0);                             \
    }                                                                                                                  \
    G_STMT_END

#define WIDGET_CONF_BOOL_TOGGLE(name, description, tooltip)                                                            \
    G_STMT_START {                                                                                                     \
        sj->config_widgets->name = gtk_check_button_new_with_label(description);                                       \
        gtk_widget_set_tooltip_text(sj->config_widgets->name, tooltip);                                                \
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sj->config_widgets->name), sj->config_settings->name);          \
    }                                                                                                                  \
    G_STMT_END

#define WIDGET_COLOR(type, type_gdk)                                                                                   \
    G_STMT_START {                                                                                                     \
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS                                                                               \
        sj->config_widgets->type = gtk_color_button_new();                                                             \
        configure_color_from_int(&sj->gdk_colors->type_gdk, sj->config_settings->type);                                \
        gtk_color_button_set_color(GTK_COLOR_BUTTON(sj->config_widgets->type), &sj->gdk_colors->type_gdk);             \
        gtk_box_pack_start(GTK_BOX(container), sj->config_widgets->type, FALSE, FALSE, 0);                             \
        gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 0);                                                       \
        G_GNUC_END_IGNORE_DEPRECATIONS                                                                                 \
    }                                                                                                                  \
    G_STMT_END

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *scrollbox = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *hbox;
    GtkWidget *container;
    GtkWidget *frame;

    gtk_widget_set_size_request(GTK_WIDGET(scrollbox), 0, 400);

#if GTK_CHECK_VERSION(3, 8, 0)
    gtk_container_add(GTK_CONTAINER(scrollbox), vbox);
#else
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrollbox), vbox);
#endif

    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollbox), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    /*
     * General
     */

    WIDGET_FRAME("General", GTK_ORIENTATION_VERTICAL);

    gchar *tt = "Show the text that appears at the end of the current line when performing a jump";
    WIDGET_CONF_BOOL(show_annotations, "Display end of line annotations", tt);

    tt = "Use the selected word or character for searching, jumping, or replacing";
    WIDGET_CONF_BOOL(use_selected_word_or_char, "Use selected word or character for search", tt);

    tt = "Wait for the Enter key to be pressed before jumping to a shortcut or text";
    WIDGET_CONF_BOOL(wait_for_enter, "Wait for Enter key to be pressed before jump", tt);

    tt = "Use the line the cursor is on for a shortcut jump or text search";
    WIDGET_CONF_BOOL(only_tag_current_line, "Only tag current line", tt);

    tt = "Set the arrow marker on the markers margin after jumping to a shortcut or text";
    WIDGET_CONF_BOOL(move_marker_to_line, "Set marker to current line after jump", tt);

    tt = "Cancel the shortcut jump or text search when the mouse moves";
    WIDGET_CONF_BOOL(cancel_on_mouse_move, "Cancel jump on mouse movement", tt);

    tt = "Use the currently selected text as the range instead of the visible page";
    WIDGET_CONF_BOOL_TOGGLE(search_from_selection, "Search or replace within current selection", tt);

    tt = "Use the selected text even if it only spans a single line";
    WIDGET_CONF_BOOL_TOGGLE(search_selection_if_line, "Even if it only exists on a single line", tt);

    g_signal_connect(sj->config_widgets->search_from_selection, "toggled", G_CALLBACK(single_line_toggle_cb), dialog);
    g_object_set_data(G_OBJECT(dialog), "search_selection_if_line", sj->config_widgets->search_selection_if_line);
    single_line_toggle_cb(GTK_TOGGLE_BUTTON(sj->config_widgets->search_from_selection), dialog);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_label_widget(GTK_FRAME(frame), sj->config_widgets->search_from_selection);
    gtk_container_add(GTK_CONTAINER(frame), sj->config_widgets->search_selection_if_line);
    gtk_box_pack_start(GTK_BOX(container), frame, FALSE, FALSE, 0);

    /*
     * Jumping to a word, character, or line using shortcuts
     */

    WIDGET_FRAME("Jumping to a word, character, or line using shortcuts", GTK_ORIENTATION_VERTICAL);

    tt = "Always select the text between the cursor and the character being jumped to";
    WIDGET_CONF_BOOL(select_when_shortcut_char, "Select to text during a character jump", tt);

    tt = "Display the shortcuts in all caps for visibility";
    WIDGET_CONF_BOOL(shortcut_all_caps, "Display shortcuts in all caps", tt);

    tt = "Include the tags A-Z when jumping to a shortcut";
    WIDGET_CONF_BOOL(shortcuts_include_single_char, "Include single character tags", tt);

    tt = "Place blank characters in the place of the words with shortcut tags";
    WIDGET_CONF_BOOL(hide_word_shortcut_jump, "Hide words when jumping to a shortcut", tt);

    tt = "Place shortcuts in the middle of words instead of the left";
    WIDGET_CONF_BOOL(center_shortcut, "Position shortcuts in middle of words", tt);

    /*
     * Jumping to a word or substring using search
     */

    WIDGET_FRAME("Jumping to a word or substring using search", GTK_ORIENTATION_VERTICAL);

    tt = "Return to the opposite side of the selected text range after moving out of range";
    WIDGET_CONF_BOOL(wrap_search, "Always wrap search", tt);

    tt = "Only mark words that match the query from the beginning";
    WIDGET_CONF_BOOL(search_start_from_beginning, "Match from start of word", tt);

    tt = "Only mark words if every character matches";
    WIDGET_CONF_BOOL(match_whole_word, "Match only a whole word", tt);

    tt = "Use proper case matching when jumping to or searching for text";
    WIDGET_CONF_BOOL_TOGGLE(search_case_sensitive, "Case sensitive", tt);

    tt = "Lower case chars match both lower and upper case chars, upper case chars only match upper case chars";
    WIDGET_CONF_BOOL_TOGGLE(search_smart_case, "Use smartcase matching", tt);

    g_signal_connect(sj->config_widgets->search_case_sensitive, "toggled", G_CALLBACK(smart_case_toggle_cb), dialog);
    g_object_set_data(G_OBJECT(dialog), "search_smart_case", sj->config_widgets->search_smart_case);
    smart_case_toggle_cb(GTK_TOGGLE_BUTTON(sj->config_widgets->search_case_sensitive), dialog);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_label_widget(GTK_FRAME(frame), sj->config_widgets->search_case_sensitive);
    gtk_container_add(GTK_CONTAINER(frame), sj->config_widgets->search_smart_case);
    gtk_box_pack_start(GTK_BOX(container), frame, FALSE, FALSE, 0);

    /*
     * After jumping to a word, character, or substring
     */

    WIDGET_FRAME("After jumping to a word, character, or substring", GTK_ORIENTATION_VERTICAL);
    sj->config_widgets->text_after = gtk_combo_box_text_new();

    for (gint i = 0; i < TX_COUNT; i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sj->config_widgets->text_after), text_conf[i].label);
    }

    gtk_box_pack_start(GTK_BOX(container), sj->config_widgets->text_after, FALSE, FALSE, 0);
    gtk_combo_box_set_active(GTK_COMBO_BOX(sj->config_widgets->text_after), sj->config_settings->text_after);

    /*
     * After jumping to a line
     */

    WIDGET_FRAME("After jumping to a line", GTK_ORIENTATION_VERTICAL);
    sj->config_widgets->line_after = gtk_combo_box_text_new();

    for (gint i = 0; i < LA_COUNT; i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sj->config_widgets->line_after), line_conf[i].label);
    }

    gtk_box_pack_start(GTK_BOX(container), sj->config_widgets->line_after, FALSE, FALSE, 0);
    gtk_combo_box_set_active(GTK_COMBO_BOX(sj->config_widgets->line_after), sj->config_settings->line_after);

    /*
     * Colors
     */

    HORIZONTAL_FRAME();
    WIDGET_FRAME_COLOR("Text color");
    WIDGET_COLOR(text_color, text_color_gdk);
    WIDGET_FRAME_COLOR("Annotation background color");
    WIDGET_COLOR(search_annotation_bg_color, search_annotation_bg_color_gdk);

    HORIZONTAL_FRAME();
    WIDGET_FRAME_COLOR("Tag color");
    WIDGET_COLOR(tag_color, tag_color_gdk);
    WIDGET_FRAME_COLOR("Highlight color");
    WIDGET_COLOR(highlight_color, highlight_color_gdk);

    // Set
    gtk_widget_show_all(scrollbox);
    g_signal_connect(dialog, "response", G_CALLBACK(configure_response_cb), sj);

    return scrollbox;
}

/**
 * @brief The plugin's callbacks which are used to ensure that the shortcuts written to the screen during a jump
 * are not actually saved to the  file when it is saved or edited from an outside source.
 */
static PluginCallback callbacks[] = {{"document-before-save", (GCallback)&on_cancel, TRUE, NULL},
                                     {"document-before-save-as", (GCallback)&on_cancel, TRUE, NULL},
                                     {"document-activate", (GCallback)&on_cancel, TRUE, NULL},
                                     {"document-reload", (GCallback)&on_document_reload, TRUE, NULL},
                                     {"editor-notify", (GCallback)&on_editor_notify, TRUE, NULL},
                                     {NULL, NULL, FALSE, NULL}};

void help(GeanyPlugin *plugin, void *data) { utils_open_browser("https://www.github.com/01mu/jump-to-a-word"); }

/**
 * @brief Inits the plugin object that persits throughout the plugin's lifetime.
 *
 * @param GeanyPlugin *plugin: The plugin
 *
 * @return ShortcutJump *: The plugin object
 */
ShortcutJump *init_data(GeanyPlugin *plugin) {
    ShortcutJump *sj = g_new0(ShortcutJump, 1);

    sj->geany_data = plugin->geany_data;

    sj->config_settings = g_new0(Settings, 1);
    sj->config_widgets = g_new0(Widgets, 1);
    sj->gdk_colors = g_new0(Colors, 1);
    sj->tl_window = g_new0(TextLineWindow, 1);

    sj->tl_window->panel = NULL;
    sj->tl_window->entry = NULL;
    sj->tl_window->view = NULL;
    sj->tl_window->store = NULL;
    sj->tl_window->sort = NULL;
    sj->tl_window->last_path = NULL;

    sj->sci = NULL;
    sj->in_selection = FALSE;
    sj->selection_is_a_word = FALSE;
    sj->line_range_set = FALSE;
    sj->previous_cursor_pos = -1;
    sj->delete_added_bracket = FALSE;
    sj->current_mode = JM_NONE;

    return sj;
}

/**
 * @brief Loads the plugin.
 *
 * @param GeanyPlugin *plugin: The plugin
 */
G_MODULE_EXPORT
void geany_load_module(GeanyPlugin *plugin) {
    ShortcutJump *sj = init_data(plugin);

    plugin->info->name = "Jump to a Word";
    plugin->info->description = "Move the cursor to a word in Geany";
    plugin->info->version = "1.0";
    plugin->info->author = "01mu <github.com/01mu>";

    plugin->funcs->init = init;
    plugin->funcs->cleanup = cleanup;
    plugin->funcs->configure = configure;
    plugin->funcs->callbacks = callbacks;
    plugin->funcs->help = help;

    GEANY_PLUGIN_REGISTER_FULL(plugin, 225, sj, NULL);
}

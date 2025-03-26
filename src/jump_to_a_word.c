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
#include "line_options.h"
#include "previous_cursor.h"
#include "replace_instant.h"
#include "search_substring.h"
#include "search_word.h"
#include "selection.h"
#include "shortcut_char.h"
#include "shortcut_common.h"
#include "shortcut_line.h"
#include "shortcut_word.h"
#include "util.h"

struct {
    gchar *label;
    LineAfter type;
} line_conf[] = {{"Do nothing", LA_DO_NOTHING},
                 {"Select line", LA_SELECT_LINE},
                 {"Select to line", LA_SELECT_TO_LINE},
                 {"Select line range", LA_SELECT_LINE_RANGE},
                 {"Jump to word (shortcut)", LA_JUMP_TO_WORD_SHORTCUT},
                 {"Jump to character (shortcut)", LA_JUMP_TO_CHARACTER_SHORTCUT},
                 {"Jump to word (search)", LA_JUMP_TO_WORD_SEARCH}};

struct {
    gchar *label;
    TextAfter type;
} text_conf[] = {{"Do nothing", TX_DO_NOTHING},
                 {"Select text", TX_SELECT_TEXT},
                 {"Select to text", TX_SELECT_TO_TEXT},
                 {"Select text range", TX_SELECT_TEXT_RANGE}};

struct {
    GtkWidget *panel;
    GtkWidget *entry;
    GtkWidget *view;
    GtkListStore *store;
    GtkTreeModel *sort;
    GtkTreePath *last_path;
} plugin_data = {NULL, NULL, NULL, NULL, NULL, NULL};

GeanyData *geany_data;
ShortcutJump *sj_global;

void set_key_press_action(ShortcutJump *sj, KeyPressCallback function) {
    sj->kp_handler_id = g_signal_connect(sj->sci, "key-press-event", G_CALLBACK(function), sj);
}

void set_click_action(ShortcutJump *sj, ClickCallback function) {
    sj->click_handler_id = g_signal_connect(geany_data->main_widgets->window, "event", G_CALLBACK(function), sj);
}

void block_key_press_action(ShortcutJump *sj) { g_signal_handler_block(sj->sci, sj->kp_handler_id); }

void block_click_action(ShortcutJump *sj) {
    g_signal_handler_block(geany_data->main_widgets->window, sj->click_handler_id);
}

/**
 * @brief Provides a callback for either saving or closing a document, or quitting. This is necessary for shortcut
 * mode because we don't want to save the version of the file with the buffered shortcut text replacement. Canceling
 * the shortcut jump replaces the buffered text with the original cached text.
 *
 * @param GObject *obj: (unused)
 * @param GeanyDocument *doc: (unused)
 * @param gpointer user_data: (unused)
 */
static void on_cancel(GObject *obj, GeanyDocument *doc, gpointer user_data) {
    if (sj_global->current_mode == JM_SHORTCUT || sj_global->current_mode == JM_SHORTCUT_CHAR_JUMPING ||
        sj_global->current_mode == JM_LINE) {
        shortcut_cancel(sj_global);
    }

    if (sj_global->current_mode == JM_SEARCH || sj_global->current_mode == JM_REPLACE_SEARCH) {
        search_cancel(sj_global);
    }
}

/**
 * @brief Provides a callback for when a reload is triggered either from a manual reload or from the file being
 * edited from an outside program. In shortcut mode we end the jump and clear the cache instead of canceling. This
 * is necessary because we don't want to reinsert the cached text at a certain location if we don't know what edits
 * were made from the other program.
 *
 * @param GObject *obj: (unused)
 * @param GeanyDocument *doc: (unused)
 * @param gpointer user_data: (unused)
 */
static void on_document_reload(GObject *obj, GeanyDocument *doc, gpointer user_data) {
    if (sj_global->current_mode == JM_SHORTCUT || sj_global->current_mode == JM_SHORTCUT_CHAR_JUMPING ||
        sj_global->current_mode == JM_LINE) {
        shortcut_end(sj_global, FALSE);
        ui_set_statusbar(TRUE, _("Clearing jump indicators"));
    }

    if (sj_global->current_mode == JM_SEARCH || sj_global->current_mode == JM_REPLACE_SEARCH) {
        search_cancel(sj_global);
    }
}

/**
 * @brief Provides a callback for when the editor is modified.
 *
 * @param GObject *obj: (unused)
 * @param GeanyEditor *editor: Click event
 * @param SCNotification *nt: Notification
 * @param gpointer user_data: (unused)
 *
 * @return gboolean: False if nothing was triggered
 */
static gboolean on_editor_notify(GObject *obj, GeanyEditor *editor, SCNotification *nt, gpointer user_data) {
    if (nt->nmhdr.code == SCN_MODIFYATTEMPTRO) {
        ui_set_statusbar(TRUE, _("Mod attempt while read-only"));
        return TRUE;
    }

    if (nt->nmhdr.code == SCN_MODIFIED) {
        gboolean in_mode =
            sj_global->current_mode == JM_REPLACE_SEARCH || sj_global->current_mode == JM_SHORTCUT_CHAR_REPLACING;
        if (in_mode && sj_global->search_change_made) {
            if (nt->modificationType & (SC_PERFORMED_UNDO) || nt->modificationType & (SC_PERFORMED_REDO)) {
                search_replace_complete(sj_global);
                return TRUE;
            }
        }
    }

    if (nt->modificationType & (SC_MOD_INSERTTEXT)) {
        if (sj_global->current_mode == JM_SHORTCUT_CHAR_WAITING || sj_global->current_mode == JM_SUBSTRING) {
            if (strcmp(nt->text, "}") == 0 || strcmp(nt->text, ">") == 0 || strcmp(nt->text, "]") == 0 ||
                strcmp(nt->text, "\'") == 0 || strcmp(nt->text, "\"") == 0 || strcmp(nt->text, "`") == 0 ||
                strcmp(nt->text, ")") == 0) {
                sj_global->delete_added_bracket = TRUE;
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
 * @brief Get the int value from GTK color type. Used when updaing the settings file. The color is in BGR.
 *
 * @param const GdkColor *color: GTK color struct

 * @return guint32 val: Integer value of the color
 */
static guint32 configure_color_to_int(const GdkColor *color) {
    return (((color->blue / 0x101) << 16) | ((color->green / 0x101) << 8) | ((color->red / 0x101) << 0));
}

void update_settings(SettingSource source) {
#define UPDATE_BOOL(name, name_str, category)                                                                          \
    G_STMT_START {                                                                                                     \
        if (source == SOURCE_SETTINGS_CHANGE) {                                                                        \
            sj_global->config_settings->name =                                                                         \
                gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(sj_global->config_widgets->name));                      \
        }                                                                                                              \
                                                                                                                       \
        g_key_file_set_boolean(config, category, name_str, sj_global->config_settings->name);                          \
    }                                                                                                                  \
    G_STMT_END

#define UPDATE_INTEGER(name, name_str, category)                                                                       \
    G_STMT_START {                                                                                                     \
        if (source == SOURCE_SETTINGS_CHANGE) {                                                                        \
            sj_global->config_settings->name =                                                                         \
                gtk_combo_box_get_active(GTK_COMBO_BOX(sj_global->config_widgets->name));                              \
        }                                                                                                              \
                                                                                                                       \
        g_key_file_set_integer(config, category, name_str, sj_global->config_settings->name);                          \
    }                                                                                                                  \
    G_STMT_END

#define UPDATE_COLOR(name, name_str, gdk)                                                                              \
    G_STMT_START {                                                                                                     \
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS                                                                               \
        if (source == SOURCE_SETTINGS_CHANGE) {                                                                        \
            gtk_color_button_get_color(GTK_COLOR_BUTTON(sj_global->config_widgets->name),                              \
                                       &sj_global->gdk_colors->gdk);                                                   \
            sj_global->config_settings->name = configure_color_to_int(&sj_global->gdk_colors->gdk);                    \
        }                                                                                                              \
                                                                                                                       \
        g_key_file_set_integer(config, "colors", name_str, sj_global->config_settings->name);                          \
        G_GNUC_END_IGNORE_DEPRECATIONS                                                                                 \
    }                                                                                                                  \
    G_STMT_END

    GKeyFile *config = g_key_file_new();
    gchar *config_dir = g_path_get_dirname(sj_global->config_file);
    gchar *data;

    g_key_file_load_from_file(config, sj_global->config_file, G_KEY_FILE_NONE, NULL);

    UPDATE_BOOL(show_annotations, "show_annotations", "general");
    UPDATE_BOOL(use_selected_word_or_char, "use_selected_word_or_char", "general");
    UPDATE_BOOL(wait_for_enter, "wait_for_enter", "general");
    UPDATE_BOOL(only_tag_current_line, "only_tag_current_line", "general");
    UPDATE_BOOL(move_marker_to_line, "move_marker_to_line", "general");
    UPDATE_BOOL(cancel_on_mouse_move, "cancel_on_mouse_move", "general");
    UPDATE_BOOL(search_from_selection, "search_from_selection", "general");
    UPDATE_BOOL(search_selection_if_line, "search_selection_if_line", "general");

    UPDATE_BOOL(shortcut_all_caps, "shortcut_all_caps", "shortcut");
    UPDATE_BOOL(shortcuts_include_single_char, "shortcuts_include_single_char", "shortcut");
    UPDATE_BOOL(hide_word_shortcut_jump, "hide_word_shortcut_jump", "shortcut");
    UPDATE_BOOL(center_shortcut, "center_shortcut", "shortcut");

    UPDATE_BOOL(search_start_from_beginning, "search_start_from_beginning", "search");
    UPDATE_BOOL(search_case_sensitive, "search_case_sensitive", "search");
    UPDATE_BOOL(match_whole_word, "match_whole_word", "search");
    UPDATE_BOOL(wrap_search, "wrap_search", "search");

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
        utils_write_file(sj_global->config_file, data);
        g_free(data);
    }

    g_free(config_dir);
    g_key_file_free(config);
}

static void setup_menu(ShortcutJump *sj) {
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

    GtkWidget *submenu = gtk_menu_new();
    GtkWidget *item;

    sj->main_menu_item = gtk_menu_item_new_with_mnemonic("Jump to a Word");

    gtk_widget_show(submenu);
    gtk_widget_show(sj->main_menu_item);

    SET_MENU_ITEM("Jump to Word (Shortcut)", shortcut_cb, sj);
    SET_MENU_ITEM("Jump to Character (Shortcut)", shortcut_char_cb, sj);
    SET_MENU_ITEM("Jump to Line (Shortcut)", jump_to_line_cb, sj);
    SET_MENU_ITEM("Jump to Word (Search)", search_cb, sj);
    SET_MENU_ITEM("Jump to Substring (Search)", substring_cb, sj);
    SET_MENU_ITEM("Jump to Previous Caret Position", jump_to_previous_cursor_cb, sj);
    SET_MENU_SEPERATOR();
    SET_MENU_ITEM("Replace Selected Text", replace_search_cb, sj);
    SET_MENU_SEPERATOR();
    SET_MENU_ITEM("Open Text Options Window", open_text_options_cb, sj);
    SET_MENU_ITEM("Open Line Options Window", open_line_options_cb, sj);

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(sj->main_menu_item), submenu);
    gtk_container_add(GTK_CONTAINER(geany_data->main_widgets->tools_menu), sj->main_menu_item);
}

static void setup_keybindings(GeanyPlugin *plugin, ShortcutJump *sj) {
#define SET_KEYBINDING(description, name, callback, w, data)                                                           \
    G_STMT_START { keybindings_set_item_full(key_group, w, 0, 0, name, _(description), NULL, callback, data, NULL); }  \
    G_STMT_END

    GeanyKeyGroup *key_group;
    key_group = plugin_set_key_group(plugin, "jump-to-a-word", KB_COUNT, NULL);
    SET_KEYBINDING("Jump to word (shortcut)", "jump_to_a_word_shortcut", shortcut_kb, KB_JUMP_TO_A_WORD_SHORTCUT, sj);
    SET_KEYBINDING("Jump to character (shortcut)", "jump_to_a_char_shortcut", shortcut_char_kb,
                   KB_JUMP_TO_A_CHAR_SHORTCUT, sj);
    SET_KEYBINDING("Jump to line (shortcut)", "jump_to_a_line", jump_to_line_kb, KB_JUMP_TO_LINE, sj);
    SET_KEYBINDING("Jump to word (search)", "jump_to_a_word_search", search_kb, KB_JUMP_TO_A_WORD_SEARCH, sj);
    SET_KEYBINDING("Jump to substring (search)", "jump_to_a_substring", substring_kb, KB_JUMP_TO_A_SUBSTRING, sj);
    SET_KEYBINDING("Jump to previous cursor position", "jump_to_previous_cursor", jump_to_previous_cursor_kb,
                   KB_JUMP_TO_PREVIOUS_CARET, sj);
    SET_KEYBINDING("Replace selected text", "replace_search", replace_search_kb, KB_REPLACE_SEARCH, sj);
    SET_KEYBINDING("Open text options window", "open_text_options", open_text_options_kb, KB_OPEN_TEXT_OPTIONS, sj);
    SET_KEYBINDING("Open line options window", "open_line_options", open_line_options_kb, KB_OPEN_LINE_OPTIONS, sj);
}

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

    sj->config_file = g_strconcat(geany_data->app->configdir, G_DIR_SEPARATOR_S, "plugins", G_DIR_SEPARATOR_S,
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

    SET_SETTING_BOOL(shortcut_all_caps, "shortcut_all_caps", "shortcut", TRUE);
    SET_SETTING_BOOL(shortcuts_include_single_char, "shortcuts_include_single_char", "shortcut", TRUE);
    SET_SETTING_BOOL(hide_word_shortcut_jump, "hide_word_shortcut_jump", "shortcut", FALSE);
    SET_SETTING_BOOL(center_shortcut, "center_shortcut", "shortcut", FALSE);

    SET_SETTING_BOOL(wrap_search, "wrap_search", "search", TRUE);
    SET_SETTING_BOOL(search_case_sensitive, "search_case_sensitive", "search", TRUE);
    SET_SETTING_BOOL(match_whole_word, "match_whole_word", "search", FALSE);
    SET_SETTING_BOOL(search_start_from_beginning, "search_start_from_beginning", "search", TRUE);

    SET_SETTING_INTEGER(text_after, "text_after", "text_after", TX_SELECT_TEXT);
    SET_SETTING_INTEGER(line_after, "line_after", "line_after", LA_SELECT_TO_LINE);

    SET_SETTING_COLOR(text_color, "text_color", 0xFFFFFF);
    SET_SETTING_COLOR(search_annotation_bg_color, "search_annotation_bg_color", 0x311F24);
    SET_SETTING_COLOR(tag_color, "tag_color", 0xFFFFFF);
    SET_SETTING_COLOR(highlight_color, "highlight_color", 0x00FF00);

    g_key_file_free(config);

    return TRUE;
}

/**
 * @brief Inits plugin; configures menu items, sets keybindings, and loads settings from the configuration file.
 *
 * @param GeanyPlugin *plugin: Geany plugin
 * @param gpointer pdata: (unused)
 *
 * @return gboolean: True
 */
static gboolean init(GeanyPlugin *plugin, gpointer pdata) {
    ShortcutJump *sj = g_new0(ShortcutJump, 1);

    geany_data = plugin->geany_data;

    sj->config_settings = g_new0(Settings, 1);
    sj->config_widgets = g_new0(Widgets, 1);
    sj->gdk_colors = g_new0(Colors, 1);

    sj->sci = NULL;
    sj->in_selection = FALSE;
    sj->selection_is_a_word = FALSE;
    sj->line_range_set = FALSE;
    sj->previous_cursor_pos = -1;
    sj->delete_added_bracket = FALSE;
    sj->current_mode = JM_NONE;

    sj_global = sj;

    setup_menu(sj);
    setup_keybindings(plugin, sj);
    setup_config_settings(plugin, pdata, sj);

    return TRUE;
}

/**
 * @brief Cancels jumps and frees the menu and configuration files.
 *
 * @param gpointer pdata: (unused)
 * @param GeanyPlugin *plugin: (unused)
 */
static void cleanup(GeanyPlugin *plugin, gpointer pdata) {
    if (sj_global->current_mode == JM_SHORTCUT) {
        shortcut_cancel(sj_global);
    }

    if (sj_global->current_mode == JM_SEARCH || sj_global->current_mode == JM_REPLACE_SEARCH) {
        search_cancel(sj_global);
    }

    if (plugin_data.panel) {
        gtk_widget_destroy(plugin_data.panel);
    }

    if (plugin_data.last_path) {
        gtk_tree_path_free(plugin_data.last_path);
    }

    g_free(sj_global->config_settings);
    g_free(sj_global->config_widgets);
    g_free(sj_global->gdk_colors);
    g_free(sj_global->config_file);

    gtk_widget_destroy(sj_global->main_menu_item);

    g_free(sj_global);
}

/**
 * @brief Provides a callback for when the settings are updated from the settings panel.
 *
 * @param GtkDialog *dialog: (unused)
 * @param gint *response: Response type is OK or APPLY
 * @param gpointer user_data: (unused)
 */
static void configure_response_cb(GtkDialog *dialog, gint response, gpointer user_data) {
    if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY) {
        update_settings(SOURCE_SETTINGS_CHANGE);
    }
}

static void ao_configure_markword_toggled_cb(GtkToggleButton *togglebutton, gpointer data) {
    gtk_widget_set_sensitive(g_object_get_data(G_OBJECT(data), "search_selection_if_line"),
                             gtk_toggle_button_get_active(togglebutton));
}

/**
 * @brief Creates and displays the configuration menu.
 *
 * @param GeanyPlugin *plugin: (unused)
 * @param GtkDialog *dialog: Set dialog for menu
 * @param gpointer pdata: (unused)
 *
 * @return GtkWidget *: Pointer to configuration menu container
 */
static GtkWidget *configure(GeanyPlugin *plugin, GtkDialog *dialog, gpointer pdata) {
#define HORIZONTAL_FRAME()                                                                                             \
    G_STMT_START {                                                                                                     \
        hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);                                                             \
        gtk_box_set_homogeneous(GTK_BOX(hbox), TRUE);                                                                  \
        gtk_container_add(GTK_CONTAINER(vbox), hbox);                                                                  \
    }                                                                                                                  \
    G_STMT_END

#define WIDGET_FRAME(description, orientation)                                                                         \
    G_STMT_START {                                                                                                     \
        container = gtk_box_new(orientation, 1);                                                                       \
        frame = gtk_frame_new(NULL);                                                                                   \
        gtk_frame_set_label(GTK_FRAME(frame), description);                                                            \
        gtk_container_add(GTK_CONTAINER(frame), container);                                                            \
        gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 1);                                                     \
    }                                                                                                                  \
    G_STMT_END

#define WIDGET_FRAME_COLOR(description, orientation)                                                                   \
    G_STMT_START {                                                                                                     \
        container = gtk_box_new(orientation, 1);                                                                       \
        frame = gtk_frame_new(NULL);                                                                                   \
        gtk_frame_set_label(GTK_FRAME(frame), description);                                                            \
        gtk_widget_set_hexpand(frame, TRUE);                                                                           \
        gtk_widget_set_halign(frame, GTK_ALIGN_FILL);                                                                  \
        gtk_widget_set_size_request(frame, 200, -1);                                                                   \
        gtk_container_add(GTK_CONTAINER(frame), container);                                                            \
    }                                                                                                                  \
    G_STMT_END

#define WIDGET_CONF_BOOL(name, description)                                                                            \
    G_STMT_START {                                                                                                     \
        sj_global->config_widgets->name = gtk_check_button_new_with_label(description);                                \
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sj_global->config_widgets->name),                               \
                                     sj_global->config_settings->name);                                                \
        gtk_box_pack_start(GTK_BOX(container), sj_global->config_widgets->name, FALSE, FALSE, 1);                      \
    }                                                                                                                  \
    G_STMT_END

#define WIDGET_COLOR(type, type_gdk)                                                                                   \
    G_STMT_START {                                                                                                     \
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS                                                                               \
        sj_global->config_widgets->type = gtk_color_button_new();                                                      \
        configure_color_from_int(&sj_global->gdk_colors->type_gdk, sj_global->config_settings->type);                  \
        gtk_color_button_set_color(GTK_COLOR_BUTTON(sj_global->config_widgets->type),                                  \
                                   &sj_global->gdk_colors->type_gdk);                                                  \
        gtk_box_pack_start(GTK_BOX(container), sj_global->config_widgets->type, FALSE, FALSE, 1);                      \
        gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 1);                                                       \
        G_GNUC_END_IGNORE_DEPRECATIONS                                                                                 \
    }                                                                                                                  \
    G_STMT_END

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    GtkWidget *scrollbox = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *hbox;
    GtkWidget *container;
    GtkWidget *frame;

    gtk_widget_set_size_request(GTK_WIDGET(scrollbox), -1, 400);

#if GTK_CHECK_VERSION(3, 8, 0)
    gtk_container_add(GTK_CONTAINER(scrollbox), vbox);
#else
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrollbox), vbox);
#endif

    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollbox), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    WIDGET_FRAME(_("General"), GTK_ORIENTATION_VERTICAL);

    WIDGET_CONF_BOOL(show_annotations, _("Display end of line annotations"));
    WIDGET_CONF_BOOL(use_selected_word_or_char, _("Use selected word or character for search"));
    WIDGET_CONF_BOOL(wait_for_enter, _("Wait for Enter key to be pressed before jump"));
    WIDGET_CONF_BOOL(only_tag_current_line, _("Only tag current line"));
    WIDGET_CONF_BOOL(move_marker_to_line, _("Set marker to current line after jump"));
    WIDGET_CONF_BOOL(cancel_on_mouse_move, _("Cancel jump on mouse movement"));

    sj_global->config_widgets->search_from_selection =
        gtk_check_button_new_with_label("Search or replace within current selection");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sj_global->config_widgets->search_from_selection),
                                 sj_global->config_settings->search_from_selection);
    g_signal_connect(sj_global->config_widgets->search_from_selection, "toggled",
                     G_CALLBACK(ao_configure_markword_toggled_cb), dialog);

    sj_global->config_widgets->search_selection_if_line =
        gtk_check_button_new_with_label("Even if it only exists on a single line");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sj_global->config_widgets->search_selection_if_line),
                                 sj_global->config_settings->search_selection_if_line);

    g_object_set_data(G_OBJECT(dialog), "search_selection_if_line",
                      sj_global->config_widgets->search_selection_if_line);
    ao_configure_markword_toggled_cb(GTK_TOGGLE_BUTTON(sj_global->config_widgets->search_from_selection), dialog);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_label_widget(GTK_FRAME(frame), sj_global->config_widgets->search_from_selection);
    gtk_container_add(GTK_CONTAINER(frame), sj_global->config_widgets->search_selection_if_line);
    gtk_box_pack_start(GTK_BOX(container), frame, FALSE, FALSE, 1);

    WIDGET_FRAME(_("Jumping to a word, character, or line using shortcuts"), GTK_ORIENTATION_VERTICAL);
    WIDGET_CONF_BOOL(shortcut_all_caps, _("Display shortcuts in all caps"));
    WIDGET_CONF_BOOL(shortcuts_include_single_char, _("Include single character tags"));
    WIDGET_CONF_BOOL(hide_word_shortcut_jump, _("Hide words when jumping to a shortcut"));
    WIDGET_CONF_BOOL(center_shortcut, _("Position shortcuts in middle of words"));

    WIDGET_FRAME(_("Jumping to a word or substring using search"), GTK_ORIENTATION_VERTICAL);
    WIDGET_CONF_BOOL(wrap_search, _("Always wrap search"));
    WIDGET_CONF_BOOL(search_case_sensitive, _("Case sensitive"));
    WIDGET_CONF_BOOL(match_whole_word, _("Match only a whole word"));
    WIDGET_CONF_BOOL(search_start_from_beginning, _("Match from start of word"));

    WIDGET_FRAME(_("After jumping to a word, character, or substring"), GTK_ORIENTATION_VERTICAL);
    sj_global->config_widgets->text_after = gtk_combo_box_text_new();

    for (gint i = 0; i < TX_COUNT; i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sj_global->config_widgets->text_after),
                                       _(text_conf[i].label));
    }

    gtk_box_pack_start(GTK_BOX(container), sj_global->config_widgets->text_after, FALSE, FALSE, 1);
    gtk_combo_box_set_active(GTK_COMBO_BOX(sj_global->config_widgets->text_after),
                             sj_global->config_settings->text_after);

    WIDGET_FRAME(_("After jumping to a line"), GTK_ORIENTATION_VERTICAL);
    sj_global->config_widgets->line_after = gtk_combo_box_text_new();

    for (gint i = 0; i < LA_COUNT; i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sj_global->config_widgets->line_after),
                                       _(line_conf[i].label));
    }

    gtk_box_pack_start(GTK_BOX(container), sj_global->config_widgets->line_after, FALSE, FALSE, 1);
    gtk_combo_box_set_active(GTK_COMBO_BOX(sj_global->config_widgets->line_after),
                             sj_global->config_settings->line_after);

    HORIZONTAL_FRAME();
    WIDGET_FRAME_COLOR("Text color", GTK_ORIENTATION_VERTICAL);
    WIDGET_COLOR(text_color, text_color_gdk);
    WIDGET_FRAME_COLOR("Annotation background color", GTK_ORIENTATION_VERTICAL);
    WIDGET_COLOR(search_annotation_bg_color, search_annotation_bg_color_gdk);

    HORIZONTAL_FRAME();
    WIDGET_FRAME_COLOR("Tag color", GTK_ORIENTATION_VERTICAL);
    WIDGET_COLOR(tag_color, tag_color_gdk);
    WIDGET_FRAME_COLOR("Highlight color", GTK_ORIENTATION_VERTICAL);
    WIDGET_COLOR(highlight_color, highlight_color_gdk);

    gtk_widget_show_all(scrollbox);
    g_signal_connect(dialog, "response", G_CALLBACK(configure_response_cb), NULL);

    return scrollbox;
}

/**
 * @brief Loads the plugin.
 *
 * @param GeanyPlugin *plugin: The plugin
 */
static PluginCallback callbacks[] = {{"document-before-save", (GCallback)&on_cancel, TRUE, NULL},
                                     {"document-before-save-as", (GCallback)&on_cancel, TRUE, NULL},
                                     {"document-activate", (GCallback)&on_cancel, TRUE, NULL},
                                     {"document-close", (GCallback)&on_cancel, TRUE, NULL},
                                     {"geany-before-quit", (GCallback)&on_cancel, TRUE, NULL},
                                     {"document-reload", (GCallback)&on_document_reload, TRUE, NULL},
                                     {"editor-notify", (GCallback)&on_editor_notify, TRUE, NULL},
                                     {NULL, NULL, FALSE, NULL}};

void help() { utils_open_browser("https://www.github.com/01mu/jump-to-a-word"); }

G_MODULE_EXPORT
void geany_load_module(GeanyPlugin *plugin) {
    plugin->info->name = "Jump to a Word";
    plugin->info->description = "Move the cursor to a word in Geany";
    plugin->info->version = "1.0";
    plugin->info->author = "01mu <github.com/01mu>";

    plugin->funcs->init = init;
    plugin->funcs->cleanup = cleanup;
    plugin->funcs->configure = configure;
    plugin->funcs->callbacks = callbacks;
    plugin->funcs->help = help;

    GEANY_PLUGIN_REGISTER(plugin, 225);
}

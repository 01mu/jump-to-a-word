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

#include "insert_line.h"
#include "jump_to_a_word.h"
#include "line_options.h"
#include "multicursor.h"
#include "preferences.h"
#include "previous_cursor.h"
#include "replace_instant.h"
#include "search_substring.h"
#include "search_word.h"
#include "selection.h"
#include "shortcut_char.h"
#include "shortcut_line.h"
#include "shortcut_word.h"
#include "transpose_string.h"
#include "util.h"
#include "values.h"

const struct {
    gchar *label;
    LineAfter type;
} line_conf[] = {{"Do nothing", LA_DO_NOTHING},
                 {"Select line", LA_SELECT_LINE},
                 {"Select to line", LA_SELECT_TO_LINE},
                 {"Select line range", LA_SELECT_LINE_RANGE},
                 {"Jump to word (shortcut)", LA_JUMP_TO_WORD_SHORTCUT},
                 {"Jump to character (shortcut)", LA_JUMP_TO_CHARACTER_SHORTCUT},
                 {"Jump to word (search)", LA_JUMP_TO_WORD_SEARCH},
                 {"Jump to substring (search)", LA_JUMP_TO_SUBSTRING_SEARCH}};

const struct {
    gchar *label;
    TextAfter type;
} text_conf[] = {{"Do nothing", TX_DO_NOTHING},
                 {"Select text", TX_SELECT_TEXT},
                 {"Select to text", TX_SELECT_TO_TEXT},
                 {"Select text range", TX_SELECT_TEXT_RANGE}};

const struct {
    gchar *label;
    ReplaceAction type;
} replace_conf[] = {{"Replace string", RA_REPLACE},
                    {"Insert at start of string", RA_INSERT_START},
                    {"Insert at end of string", RA_INSERT_END},
                    {"Insert at previous line", RA_INSERT_PREVIOUS_LINE},
                    {"Insert at next line", RA_INSERT_NEXT_LINE},
                    {"Transpose string", RA_TRANSPOSE_STRING}};

void handle_action(gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    ReplaceAction ra = sj->config_settings->replace_action;
    MulticusrorMode mm = sj->multicursor_enabled;
    JumpMode jm = sj->current_mode;

    if (ra == RA_REPLACE || ra == RA_INSERT_START || ra == RA_INSERT_END) {
        if (mm == MC_DISABLED) {
            if (jm == JM_SEARCH) {
                replace_word_init(sj, FALSE);
                return;
            } else if (jm == JM_SHORTCUT_WORD) {
                shortcut_word_cancel(sj);
                return;
            } else if (jm == JM_REPLACE_SEARCH) {
                search_word_replace_cancel(sj);
                return;
            } else if (jm == JM_SHORTCUT_CHAR_JUMPING) {
                shortcut_char_jumping_cancel(sj);
                return;
            } else if (jm == JM_SHORTCUT_CHAR_ACCEPTING) {
                shortcut_char_waiting_cancel(sj);
                return;
            } else if (jm == JM_SHORTCUT_CHAR_REPLACING) {
                shortcut_char_replacing_cancel(sj);
                return;
            } else if (jm == JM_LINE) {
                shortcut_word_cancel(sj);
                return;
            } else if (jm == JM_SUBSTRING) {
                replace_substring_init(sj);
                return;
            } else if (jm == JM_REPLACE_SUBSTRING) {
                search_substring_replace_cancel(sj);
                return;
            } else if (jm == JM_INSERTING_LINE) {
                line_insert_cancel(sj);
                return;
            } else if (jm == JM_NONE) {
                replace_instant_init(sj);
                return;
            }
        } else if (mm == MC_ACCEPTING) {
            if (jm == JM_NONE) {
                multicursor_replace(sj);
                return;
            }
        } else if (mm == MC_REPLACING) {
            multicursor_cancel(sj);
            return;
        }
    } else if (ra == RA_INSERT_NEXT_LINE || ra == RA_INSERT_PREVIOUS_LINE) {
        if (mm == MC_DISABLED) {
            if (jm == JM_NONE) {
                set_sj_scintilla_object(sj);
                set_selection_info(sj);
                line_insert_set_query(sj);
                define_indicators(sj->sci, sj);
                line_insert_from_search(sj);
                return;
            } else if (jm == JM_SEARCH || jm == JM_SUBSTRING) {
                disconnect_key_press_action(sj);
                disconnect_click_action(sj);
                line_insert_from_search(sj);
                return;
            }
        } else if (mm == MC_ACCEPTING) {
            if (jm == JM_NONE) {
                line_insert_from_multicursor(sj);
                return;
            }
        } else if (mm == MC_REPLACING) {
            multicursor_cancel(sj);
            return;
        }
    } else if (ra == RA_TRANSPOSE_STRING) {
        if (mm == MC_ACCEPTING) {
            if (jm == JM_NONE) {
                transpose_string(sj);
                return;
            }
        }
    }

    ui_set_statusbar(TRUE, _("No action available."));
}

void replace_search_cb(GtkMenuItem *menu_item, gpointer user_data) { handle_action(user_data); }

gboolean replace_search_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    handle_action(user_data);
    return TRUE;
}

static void on_cancel(GObject *obj, GeanyDocument *doc, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (sj->current_mode == JM_SEARCH) {
        search_word_jump_cancel(sj);
    } else if (sj->current_mode == JM_SHORTCUT_WORD) {
        shortcut_word_cancel(sj);
    } else if (sj->current_mode == JM_REPLACE_SEARCH) {
        search_word_replace_cancel(sj);
    } else if (sj->current_mode == JM_SHORTCUT_CHAR_JUMPING) {
        shortcut_char_jumping_cancel(sj);
    } else if (sj->current_mode == JM_SHORTCUT_CHAR_ACCEPTING) {
        shortcut_char_waiting_cancel(sj);
    } else if (sj->current_mode == JM_SHORTCUT_CHAR_REPLACING) {
        shortcut_char_replacing_cancel(sj);
    } else if (sj->current_mode == JM_LINE) {
        shortcut_line_cancel(sj);
    } else if (sj->current_mode == JM_SUBSTRING) {
        search_substring_jump_cancel(sj);
    } else if (sj->current_mode == JM_REPLACE_SUBSTRING) {
        search_substring_replace_cancel(sj);
    } else if (sj->current_mode == JM_INSERTING_LINE) {
        line_insert_cancel(sj);
    }

    if (sj->multicursor_enabled == MC_ACCEPTING) {
        multicursor_end(sj);
    } else if (sj->multicursor_enabled == MC_REPLACING) {
        multicursor_cancel(sj);
    }
}

static gboolean on_editor_notify(GObject *obj, GeanyEditor *editor, const SCNotification *nt, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (nt->nmhdr.code == SCN_UPDATEUI && nt->updated == SC_UPDATE_SELECTION &&
        sj->multicursor_enabled == MC_ACCEPTING) {
        if (!sj->sci) {
            set_sj_scintilla_object(sj);
        }

        gint selection_start = scintilla_send_message(sj->sci, SCI_GETSELECTIONSTART, 0, 0);
        gint selection_end = scintilla_send_message(sj->sci, SCI_GETSELECTIONEND, 0, 0);

        if (selection_start == selection_end) {
            return TRUE;
        }

        multicursor_add_word_selection(sj, selection_start, selection_end);
        return TRUE;
    }

    if (nt->nmhdr.code == SCN_MODIFYATTEMPTRO) {
        return TRUE;
    }

    if (nt->modificationType & (SC_MOD_INSERTTEXT)) {
        if (sj->current_mode == JM_SHORTCUT_CHAR_ACCEPTING || sj->current_mode == JM_SUBSTRING) {
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

static void configure_color_from_int(GdkColor *color, guint32 val) {
    color->blue = ((val & 0xff0000) >> 16) * 0x101;
    color->green = ((val & 0x00ff00) >> 8) * 0x101;
    color->red = ((val & 0x0000ff) >> 0) * 0x101;
}

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

    sj->main_menu_item = gtk_menu_item_new_with_mnemonic("_Jump to a Word");

    gtk_widget_show(submenu);
    gtk_widget_show(sj->main_menu_item);

    SET_MENU_ITEM("Jump to _Word (Shortcut)", shortcut_word_cb, sj);
    SET_KEYBINDING("Jump to word (shortcut)", "jump_to_a_word_shortcut", shortcut_word_kb, KB_JUMP_TO_A_WORD_SHORTCUT,
                   sj, item);

    SET_MENU_ITEM("Jump to _Character (Shortcut)", shortcut_char_cb, sj);
    SET_KEYBINDING("Jump to character (shortcut)", "jump_to_a_char_shortcut", shortcut_char_kb,
                   KB_JUMP_TO_A_CHAR_SHORTCUT, sj, item);

    SET_MENU_ITEM("Jump to _Line (Shortcut)", shortcut_line_cb, sj);
    SET_KEYBINDING("Jump to line (shortcut)", "jump_to_a_line", shortcut_line_kb, KB_JUMP_TO_LINE, sj, item);

    SET_MENU_ITEM("Jump to W_ord (Search)", search_word_cb, sj);
    SET_KEYBINDING("Jump to word (search)", "jump_to_a_word_search", search_word_kb, KB_JUMP_TO_A_WORD_SEARCH, sj,
                   item);

    SET_MENU_ITEM("Jump to _Substring (Search)", search_substring_cb, sj);
    SET_KEYBINDING("Jump to substring (search)", "jump_to_a_substring", search_substring_kb, KB_JUMP_TO_A_SUBSTRING, sj,
                   item);

    SET_MENU_ITEM("Jump to _Previous Cursor Position", jump_to_previous_cursor_cb, sj);
    SET_KEYBINDING("Jump to previous cursor position", "jump_to_previous_cursor", jump_to_previous_cursor_kb,
                   KB_JUMP_TO_PREVIOUS_CARET, sj, item);

    SET_MENU_SEPERATOR();

    SET_MENU_ITEM("_Replace Selected Text", replace_search_cb, sj);
    SET_KEYBINDING("Replace selected text", "replace_search", replace_search_kb, KB_REPLACE_SEARCH, sj, item);

    SET_MENU_ITEM("Toggle _Multicursor Mode", multicursor_cb, sj);
    SET_KEYBINDING("Toggle multicursor mode", "multicursor", multicursor_kb, KB_MULTICURSOR, sj, item);

    SET_MENU_SEPERATOR();

    SET_MENU_ITEM("Open _Text Options Window", open_text_options_cb, sj);
    SET_KEYBINDING("Open text options window", "open_text_options", open_text_options_kb, KB_OPEN_TEXT_OPTIONS, sj,
                   item);

    SET_MENU_ITEM("Open _Line Options Window", open_line_options_cb, sj);
    SET_KEYBINDING("Open line options window", "open_line_options", open_line_options_kb, KB_OPEN_LINE_OPTIONS, sj,
                   item);

    SET_MENU_ITEM("Open _Replacement Options Window", open_replace_options_cb, sj);
    SET_KEYBINDING("Open replacement options window", "open_replace_options", open_replace_options_kb,
                   KB_OPEN_REPLACE_OPTIONS, sj, item);

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(sj->main_menu_item), submenu);
    gtk_container_add(GTK_CONTAINER(sj->geany_data->main_widgets->tools_menu), sj->main_menu_item);
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

    SET_SETTING_BOOL(select_when_shortcut_char, "select_when_shortcut_char", "shortcut", TRUE);
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
    SET_SETTING_INTEGER(replace_action, "replace_action", "replace_action", RA_REPLACE);

    SET_SETTING_COLOR(text_color, "text_color", 0xFFFFFF);
    SET_SETTING_COLOR(search_annotation_bg_color, "search_annotation_bg_color", 0x311F24);
    SET_SETTING_COLOR(tag_color, "tag_color", 0xFFFFFF);
    SET_SETTING_COLOR(highlight_color, "highlight_color", 0x69A226);

    g_key_file_free(config);

    return TRUE;
}

static gboolean init(GeanyPlugin *plugin, gpointer pdata) {
    ShortcutJump *sj = (ShortcutJump *)pdata;

    setup_menu_and_keybindings(plugin, sj);
    setup_config_settings(plugin, pdata, sj);

    return TRUE;
}

static void cleanup(GeanyPlugin *plugin, gpointer pdata) {
    ShortcutJump *sj = (ShortcutJump *)pdata;
    end_actions(sj);

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

static void configure_response_cb(GtkDialog *dialog, gint response, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY) {
        update_settings(SOURCE_SETTINGS_CHANGE, sj);
    }
}

static void single_line_toggle_cb(GtkToggleButton *toggle_button, gpointer data) {
    gtk_widget_set_sensitive(g_object_get_data(G_OBJECT(data), "search_selection_if_line"),
                             gtk_toggle_button_get_active(toggle_button));
}

static void smart_case_toggle_cb(GtkToggleButton *toggle_button, gpointer data) {
    gtk_widget_set_sensitive(g_object_get_data(G_OBJECT(data), "search_smart_case"),
                             gtk_toggle_button_get_active(toggle_button));
}

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
        sj->config_widgets->name = gtk_check_button_new_with_mnemonic(description);                                    \
        gtk_widget_set_tooltip_text(sj->config_widgets->name, tooltip);                                                \
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sj->config_widgets->name), sj->config_settings->name);          \
        gtk_box_pack_start(GTK_BOX(container), sj->config_widgets->name, FALSE, FALSE, 0);                             \
    }                                                                                                                  \
    G_STMT_END

#define WIDGET_CONF_BOOL_TOGGLE(name, description, tooltip)                                                            \
    G_STMT_START {                                                                                                     \
        sj->config_widgets->name = gtk_check_button_new_with_mnemonic(description);                                    \
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
    WIDGET_CONF_BOOL(show_annotations, "_Display end of line annotations", tt);

    tt = "Use the selected word or character for searching, jumping, or replacing";
    WIDGET_CONF_BOOL(use_selected_word_or_char, "_Use selected word or character for search", tt);

    tt = "Wait for the Enter key to be pressed before jumping to a shortcut or text";
    WIDGET_CONF_BOOL(wait_for_enter, "_Wait for Enter key to be pressed before jump", tt);

    tt = "Use the line the cursor is on for a shortcut jump or text search";
    WIDGET_CONF_BOOL(only_tag_current_line, "O_nly tag current line", tt);

    tt = "Set the arrow marker on the markers margin after jumping to a shortcut or text";
    WIDGET_CONF_BOOL(move_marker_to_line, "_Set marker to current line after jump", tt);

    tt = "Cancel the shortcut jump or text search when the mouse moves";
    WIDGET_CONF_BOOL(cancel_on_mouse_move, "Cancel _jump on mouse movement", tt);

    tt = "Use the currently selected text as the range instead of the visible page";
    WIDGET_CONF_BOOL_TOGGLE(search_from_selection, "Search o_r replace within current selection", tt);

    tt = "Use the selected text even if it only spans a single line";
    WIDGET_CONF_BOOL_TOGGLE(search_selection_if_line, "Even if it only e_xists on a single line", tt);

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
    WIDGET_CONF_BOOL(select_when_shortcut_char, "S_elect to text during a character jump", tt);

    tt = "Display the shortcuts in all caps for visibility";
    WIDGET_CONF_BOOL(shortcut_all_caps, "D_isplay shortcuts in all caps", tt);

    tt = "Include the tags A-Z when jumping to a shortcut";
    WIDGET_CONF_BOOL(shortcuts_include_single_char, "Inc_lude single character tags", tt);

    tt = "Place blank characters in the place of the words with shortcut tags";
    WIDGET_CONF_BOOL(hide_word_shortcut_jump, "_Hide words when jumping to a shortcut", tt);

    tt = "Place shortcuts in the middle of words instead of the left";
    WIDGET_CONF_BOOL(center_shortcut, "_Position shortcuts in middle of words", tt);

    /*
     * Jumping to a word or substring using search
     */

    WIDGET_FRAME("Jumping to a word or substring using search", GTK_ORIENTATION_VERTICAL);

    tt = "Return to the opposite side of the selected text range after moving out of range";
    WIDGET_CONF_BOOL(wrap_search, "Alwa_ys wrap search", tt);

    tt = "Only mark words that match the query from the beginning";
    WIDGET_CONF_BOOL(search_start_from_beginning, "_Match from start of word", tt);

    tt = "Only mark words if every character matches";
    WIDGET_CONF_BOOL(match_whole_word, "Ma_tch only a whole word", tt);

    tt = "Use proper case matching when jumping to or searching for text";
    WIDGET_CONF_BOOL_TOGGLE(search_case_sensitive, "Case sensiti_ve", tt);

    tt = "Lower case chars match both lower and upper case chars, upper case "
         "chars only match upper case chars";
    WIDGET_CONF_BOOL_TOGGLE(search_smart_case, "Use smartcase matchin_g", tt);

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
     * Replacement action
     */

    WIDGET_FRAME("Replacement action", GTK_ORIENTATION_VERTICAL);
    sj->config_widgets->replace_action = gtk_combo_box_text_new();

    for (gint i = 0; i < RA_COUNT; i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sj->config_widgets->replace_action), replace_conf[i].label);
    }

    gtk_box_pack_start(GTK_BOX(container), sj->config_widgets->replace_action, FALSE, FALSE, 0);
    gtk_combo_box_set_active(GTK_COMBO_BOX(sj->config_widgets->replace_action), sj->config_settings->replace_action);

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

    /*
     * Set
     */

    gtk_widget_show_all(scrollbox);
    g_signal_connect(dialog, "response", G_CALLBACK(configure_response_cb), sj);

    return scrollbox;
}

static PluginCallback callbacks[] = {{"document-before-save", (GCallback)&on_cancel, TRUE, NULL},
                                     {"document-before-save-as", (GCallback)&on_cancel, TRUE, NULL},
                                     {"document-activate", (GCallback)&on_cancel, TRUE, NULL},
                                     {"document-reload", (GCallback)&on_cancel, TRUE, NULL},
                                     {"editor-notify", (GCallback)&on_editor_notify, TRUE, NULL},
                                     {NULL, NULL, FALSE, NULL}};

void help(GeanyPlugin *plugin, void *data) { utils_open_browser("https://www.github.com/01mu/jump-to-a-word"); }

ShortcutJump *init_data(const GeanyPlugin *plugin) {
    ShortcutJump *sj = g_new0(ShortcutJump, 1);

    sj->geany_data = plugin->geany_data;

    sj->config_settings = g_new0(Settings, 1);
    sj->config_widgets = g_new0(Widgets, 1);
    sj->gdk_colors = g_new0(Colors, 1);
    sj->tl_window = g_new0(TextLineWindow, 1);

    sj->multicursor_enabled = MC_DISABLED;
    sj->multicursor_first_pos = 0;
    sj->multicursor_last_pos = 0;

    sj->tl_window->panel = NULL;
    sj->tl_window->entry = NULL;
    sj->tl_window->view = NULL;
    sj->tl_window->store = NULL;
    sj->tl_window->sort = NULL;
    sj->tl_window->last_path = NULL;

    sj->sci = NULL;
    sj->in_selection = FALSE;
    sj->selection_is_a_word = FALSE;
    sj->range_is_set = FALSE;
    sj->previous_cursor_pos = -1;
    sj->delete_added_bracket = FALSE;
    sj->current_mode = JM_NONE;

    return sj;
}

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

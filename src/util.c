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
#include "search_substring.h"
#include "search_word.h"
#include "shortcut_char.h"
#include "shortcut_word.h"

gint get_lfs(ShortcutJump *sj, gint current_line) {
    if (sj->in_selection && sj->selection_is_a_line) {
        return 0;
    }

    gint line;

    if (sj->in_selection && sj->config_settings->search_from_selection) {
        line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->first_position, 0) + 1;
    } else {
        line = sj->first_line_on_screen;
    }

    return g_array_index(sj->lf_positions, gint, current_line - line);
}

gint set_cursor_position_with_lfs(ShortcutJump *sj) {
    gint current_line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->current_cursor_pos, 0);
    gint lfs = get_lfs(sj, current_line);
    return sj->current_cursor_pos - lfs;
}

gint get_indent_width() {
    GeanyDocument *doc = document_get_current();
    if (!doc->is_valid) {
        exit(1);
    }
    return doc->editor->indent_width;
}

gboolean mod_key_pressed(const GdkEventKey *event) {
    return event->keyval == GDK_KEY_Shift_L || event->keyval == GDK_KEY_Shift_R || event->keyval == GDK_KEY_Caps_Lock ||
           event->keyval == GDK_KEY_Control_L || event->keyval == GDK_KEY_Control_R;
}

gboolean mouse_movement_performed(ShortcutJump *sj, const GdkEventButton *event) {
    return (sj->config_settings->cancel_on_mouse_move && event->type == GDK_MOTION_NOTIFY) ||
           event->type == GDK_LEAVE_NOTIFY || event->type == GDK_SCROLL || event->type == GDK_BUTTON_PRESS ||
           event->type == GDK_BUTTON_RELEASE || event->type == GDK_SELECTION_CLEAR ||
           event->type == GDK_SELECTION_REQUEST || event->type == GDK_SELECTION_NOTIFY ||
           event->type == GDK_2BUTTON_PRESS || event->type == GDK_3BUTTON_PRESS;
}

void define_indicators(ScintillaObject *sci, ShortcutJump *sj) {
    scintilla_send_message(sci, SCI_INDICSETSTYLE, INDICATOR_TAG, INDIC_FULLBOX);
    scintilla_send_message(sci, SCI_INDICSETOUTLINEALPHA, INDICATOR_TAG, 120);
    scintilla_send_message(sci, SCI_INDICSETFORE, INDICATOR_TAG, sj->config_settings->tag_color);

    scintilla_send_message(sci, SCI_INDICSETSTYLE, INDICATOR_HIGHLIGHT, INDIC_FULLBOX);
    scintilla_send_message(sci, SCI_INDICSETALPHA, INDICATOR_HIGHLIGHT, 120);
    scintilla_send_message(sci, SCI_INDICSETFORE, INDICATOR_HIGHLIGHT, sj->config_settings->highlight_color);

    scintilla_send_message(sci, SCI_INDICSETSTYLE, INDICATOR_TEXT, INDIC_TEXTFORE);
    scintilla_send_message(sci, SCI_INDICSETALPHA, INDICATOR_TEXT, 120);
    scintilla_send_message(sci, SCI_INDICSETFORE, INDICATOR_TEXT, sj->config_settings->text_color);

    scintilla_send_message(sci, SCI_INDICSETSTYLE, INDICATOR_MULTICURSOR, INDIC_PLAIN);
    scintilla_send_message(sci, SCI_INDICSETALPHA, INDICATOR_MULTICURSOR, 0);
    scintilla_send_message(sci, SCI_INDICSETFORE, INDICATOR_MULTICURSOR, sj->config_settings->highlight_color);
}

void connect_key_press_action(ShortcutJump *sj, KeyPressCallback function) {
    sj->kp_handler_id = g_signal_connect(sj->sci, "key-press-event", G_CALLBACK(function), sj);
}

void connect_click_action(ShortcutJump *sj, ClickCallback function) {
    sj->click_handler_id = g_signal_connect(sj->geany_data->main_widgets->window, "event", G_CALLBACK(function), sj);
}

void disconnect_key_press_action(ShortcutJump *sj) { g_signal_handler_disconnect(sj->sci, sj->kp_handler_id); }

void disconnect_click_action(ShortcutJump *sj) {
    g_signal_handler_disconnect(sj->geany_data->main_widgets->window, sj->click_handler_id);
}

void end_actions(ShortcutJump *sj) {
    if (sj->current_mode == JM_SEARCH) {
        search_word_jump_cancel(sj);
    } else if (sj->current_mode == JM_SHORTCUT_WORD) {
        shortcut_word_cancel(sj);
    } else if (sj->current_mode == JM_REPLACE_SEARCH) {
        search_word_replace_complete(sj);
    } else if (sj->current_mode == JM_SHORTCUT_CHAR_JUMPING) {
        shortcut_char_jumping_cancel(sj);
    } else if (sj->current_mode == JM_SHORTCUT_CHAR_ACCEPTING) {
        shortcut_char_waiting_cancel(sj);
    } else if (sj->current_mode == JM_SHORTCUT_CHAR_REPLACING) {
        shortcut_char_replacing_cancel(sj);
    } else if (sj->current_mode == JM_LINE) {
        shortcut_word_cancel(sj);
    } else if (sj->current_mode == JM_SUBSTRING) {
        search_substring_end(sj);
    } else if (sj->current_mode == JM_REPLACE_SUBSTRING) {
        search_substring_end(sj);
    } else if (sj->current_mode == JM_INSERTING_LINE) {
        line_insert_cancel(sj);
    }
}

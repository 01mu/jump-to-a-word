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
#include "multicursor.h"
#include "search_common.h"
#include "search_word.h"
#include "shortcut_char.h"
#include "shortcut_common.h"

/**
 * @brief Sets indicators for a given range.
 *
 * @param ShortcutJump *sj: The plugin object
 * @param Indicator type: Which indicator
 * @param gint starting: The starting point
 * @param gint length: The length of the indicator
 */
void set_indicator_for_range(ScintillaObject *sci, Indicator type, gint starting, gint length) {
    scintilla_send_message(sci, SCI_SETINDICATORCURRENT, type, 0);
    scintilla_send_message(sci, SCI_INDICATORFILLRANGE, starting, length);
}

/**
 * @brief Clears indicators for a given range.
 *
 * @param ShortcutJump *sj: The plugin object
 * @param Indicator type: Which indicator
 * @param gint starting: The starting point
 * @param gint length: The length of the indicator
 */
void clear_indicator_for_range(ScintillaObject *sci, Indicator type, gint starting, gint length) {
    scintilla_send_message(sci, SCI_SETINDICATORCURRENT, type, 0);
    scintilla_send_message(sci, SCI_INDICATORCLEARRANGE, starting, length);
}

/**
 * @brief Sets the indicators for valid shortcuts.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void set_shortcut_indicators(ShortcutJump *sj) {
    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);

        if (!word.is_hidden_neighbor) {
            set_indicator_for_range(sj->sci, INDICATOR_TAG, word.starting + word.padding, word.shortcut->len);
            set_indicator_for_range(sj->sci, INDICATOR_TEXT, word.starting + word.padding, word.shortcut->len);
        }
    }
}

/**
 * @brief Sets the indicators for valid words.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void set_word_indicators(ShortcutJump *sj) {
    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);

        if (!word.is_hidden_neighbor) {
            set_indicator_for_range(sj->sci, INDICATOR_TAG, word.starting + word.padding, word.word->len);
            set_indicator_for_range(sj->sci, INDICATOR_TEXT, word.starting + word.padding, word.word->len);
        }
    }
}

/**
 * @brief Returns the number of line endings that were added during a shortcut placement at a given line. This is
 * needed so we can maintain the correct cursor position.
 *
 * @param ShortcutJump *sj: The plugin object
 * @param gint current_line: The current line
 *
 * @return gint: The number of line endings at a certain line
 */
gint get_lfs(ShortcutJump *sj, gint current_line) {
    if (sj->in_selection && sj->selection_is_a_line) {
        return 0;
    }

    gint line = sj->in_selection && sj->config_settings->search_from_selection
                    ? scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->first_position, 0) + 1
                    : sj->first_line_on_screen;

    return g_array_index(sj->lf_positions, gint, current_line - line);
}

/**
 * @brief Returns the current cursor position controlling for line endings.
 *
 * @param ShortcutJump *sj: The plugin object
 *
 * @return gint: The new position
 */
gint set_cursor_position_with_lfs(ShortcutJump *sj) {
    gint current_line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->current_cursor_pos, 0);
    gint lfs = get_lfs(sj, current_line);

    return sj->current_cursor_pos - lfs;
}

/**
 * @brief Resets the marker locations after displaying the buffered text. This is necessary because the current markers
 * are displaced when the original text is replaced with the buffer during shortcut or search replacement mode.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void margin_markers_reset(ShortcutJump *sj) {
    gint last_line_in_doc = scintilla_send_message(sj->sci, SCI_GETLINECOUNT, 0, 0);

    for (gint i = 0; i < sj->lines_on_screen; i++) {
        gint marker = g_array_index(sj->markers, gint, i);
        scintilla_send_message(sj->sci, SCI_MARKERADDSET, i + sj->first_line_on_screen, marker);
    }

    if (sj->last_line_on_screen == last_line_in_doc) {
        gint ll_markers = scintilla_send_message(sj->sci, SCI_MARKERGET, last_line_in_doc, 0);

        for (gint i = 0; i < sj->lines_on_screen - 1; i++) {
            gint marker = g_array_index(sj->markers, gint, i);
            scintilla_send_message(sj->sci, SCI_MARKERADDSET, i + sj->first_line_on_screen, marker);
        }

        scintilla_send_message(sj->sci, SCI_MARKERDELETE, last_line_in_doc - 1, -1);
        scintilla_send_message(sj->sci, SCI_MARKERADDSET, last_line_in_doc, ll_markers);
    } else {
        scintilla_send_message(sj->sci, SCI_MARKERDELETE, sj->last_line_on_screen, -1);
    }
}

/**
 * @brief Returns the indent width of the current document to control for tabs during a line jump.
 *
 * @return gint: The indent width
 */
gint get_indent_width() {
    GeanyDocument *doc = document_get_current();

    if (!doc->is_valid) {
        exit(1);
    }

    return doc->editor->indent_width;
}

/**
 * @brief Clears the tag and highlight indicators that were created when searching for a word.
 *
 * @param ScintillaObject *sci: The Scintilla object
 * @param gint eol_message_line: The word array
 */
void search_clear_indicators(ScintillaObject *sci, GArray *words) {
    for (gint i = 0; i < words->len; i++) {
        Word word = g_array_index(words, Word, i);

        clear_indicator_for_range(sci, INDICATOR_TAG, word.starting, word.word->len);
        clear_indicator_for_range(sci, INDICATOR_HIGHLIGHT, word.starting, word.word->len);
        clear_indicator_for_range(sci, INDICATOR_TEXT, word.starting, word.word->len);
        clear_indicator_for_range(sci, INDICATOR_MULTICURSOR, word.starting, word.word->len);
    }
}

/**
 * @brief Checks if a special key is pressed so we can continue the text replacement and accept new characters.
 *
 * @param const GdkEventKey *event: The key press event
 *
 * @return gboolean: If it was pressed
 */
gboolean mod_key_pressed(const GdkEventKey *event) {
    return event->keyval == GDK_KEY_Shift_L || event->keyval == GDK_KEY_Shift_R || event->keyval == GDK_KEY_Caps_Lock ||
           event->keyval == GDK_KEY_Control_L || event->keyval == GDK_KEY_Control_R;
}

/**
 * @brief Checks if a mouse movement was performed so we can cancel the jump and clear memory.
 *
 * @param ShortcutJump *sj: The plugin object
 * @param GdkEventButton *event: The mouse event
 *
 * @return gboolean: If it was activated
 */
gboolean mouse_movement_performed(ShortcutJump *sj, const GdkEventButton *event) {
    return (sj->config_settings->cancel_on_mouse_move && event->type == GDK_MOTION_NOTIFY) ||
           event->type == GDK_LEAVE_NOTIFY || event->type == GDK_SCROLL || event->type == GDK_BUTTON_PRESS ||
           event->type == GDK_BUTTON_RELEASE || event->type == GDK_SELECTION_CLEAR ||
           event->type == GDK_SELECTION_REQUEST || event->type == GDK_SELECTION_NOTIFY ||
           event->type == GDK_2BUTTON_PRESS || event->type == GDK_3BUTTON_PRESS;
}

/**
 * @brief Defines the indicators used for shortcut tags. The first one is for the box that appears around the tag, the
 * second for the highlight, the third defined the text color.
 *
 * @param ScintillaObject *sci: Scintilla object
 */
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

/**
 * @brief Saves the cusor position. This is necessary because we have to keep the cursor in its original position on
 * the page while the text is updating during a replacement.
 *
 * @param ShortcutJump *sj: The plugin object
 *
 * @return gint: The cursor's 'position
 */
gint save_cursor_position(ShortcutJump *sj) { return scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0); }

/**
 * @brief Sets the key press action for a jump.
 *
 * @param ShortcutJump *sj: The plugin object
 * @param KeyPressCallback function: The callback
 */
void connect_key_press_action(ShortcutJump *sj, KeyPressCallback function) {
    sj->kp_handler_id = g_signal_connect(sj->sci, "key-press-event", G_CALLBACK(function), sj);
}

/**
 * @brief Sets the click action for a jump.
 *
 * @param ShortcutJump *sj: The plugin object
 * @param ClickCallback function: The callback
 */
void connect_click_action(ShortcutJump *sj, ClickCallback function) {
    sj->click_handler_id = g_signal_connect(sj->geany_data->main_widgets->window, "event", G_CALLBACK(function), sj);
}

/**
 * @brief Destroys a key press action previously set during a jump.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void disconnect_key_press_action(ShortcutJump *sj) {
    // g_signal_handler_block(sj->sci, sj->kp_handler_id);
    g_signal_handler_disconnect(sj->sci, sj->kp_handler_id);
}

/**
 * @brief Destroys a click action previously set during a jump.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void disconnect_click_action(ShortcutJump *sj) {
    // g_signal_handler_block(sj->geany_data->main_widgets->window, sj->click_handler_id);
    g_signal_handler_disconnect(sj->geany_data->main_widgets->window, sj->click_handler_id);
}

/**
 * @brief Handles the action performed after jumping to a word or character using a shortcut.
 *
 * @param ShortcutJump *sj: The plugin object
 * @param gint pos: The position of the word or text on screen
 * @param gint word_length: The length of the text
 * @param gint line: The line the text is on
 *
 * @return gboolean: Whether a text line range jump was completed, used to clear the marker
 */
gboolean handle_text_after_action(ShortcutJump *sj, gint pos, gint word_length, gint line) {
    gboolean text_range_jumped = FALSE;

    if (sj->config_settings->text_after == TX_DO_NOTHING) {
        scintilla_send_message(sj->sci, SCI_GOTOPOS, pos, 0);
    }

    if (sj->config_settings->text_after == TX_SELECT_TEXT) {
        scintilla_send_message(sj->sci, SCI_SETSEL, pos, pos + word_length);
    }

    gboolean select_when_shortcut_char = sj->config_settings->select_when_shortcut_char;
    gboolean mode_shortcut_char_jumping = sj->current_mode == JM_SHORTCUT_CHAR_JUMPING;
    gboolean char_jump_enabled = select_when_shortcut_char && mode_shortcut_char_jumping;

    if (sj->config_settings->text_after == TX_SELECT_TO_TEXT || char_jump_enabled) {
        if (sj->current_cursor_pos > pos) {
            scintilla_send_message(sj->sci, SCI_SETSEL, sj->current_cursor_pos, pos);
        } else {
            scintilla_send_message(sj->sci, SCI_SETSEL, sj->current_cursor_pos, pos + word_length);
        }
    }

    if (sj->config_settings->text_after == TX_SELECT_TEXT_RANGE && sj->range_is_set) {
        if (pos > sj->range_first_pos) {
            scintilla_send_message(sj->sci, SCI_SETSEL, sj->range_first_pos, pos + word_length);
        } else {
            scintilla_send_message(sj->sci, SCI_SETSEL, sj->range_first_pos + sj->range_word_length, pos);
        }

        text_range_jumped = TRUE;
    }

    if (sj->config_settings->text_after == TX_SELECT_TEXT_RANGE && !sj->range_is_set) {
        scintilla_send_message(sj->sci, SCI_MARKERDEFINE, 0, SC_MARK_SHORTARROW);
        scintilla_send_message(sj->sci, SCI_MARKERADD, line, 0);
        sj->range_first_pos = pos;
        sj->range_word_length = word_length;
        g_string_erase(sj->search_query, 0, sj->search_query->len);
        sj->range_is_set = TRUE;
    }

    if (text_range_jumped) {
        sj->range_is_set = FALSE;
        return TRUE;
    }

    return FALSE;
}

void end_actions(ShortcutJump *sj) {
    if (sj->current_mode == JM_SEARCH) {
        search_cancel(sj);
    } else if (sj->current_mode == JM_SHORTCUT) {
        shortcut_cancel(sj);
    } else if (sj->current_mode == JM_REPLACE_SEARCH) {
        search_replace_complete(sj);
    } else if (sj->current_mode == JM_SHORTCUT_CHAR_JUMPING) {
        shortcut_cancel(sj);
    } else if (sj->current_mode == JM_SHORTCUT_CHAR_WAITING) {
        shortcut_char_waiting_cancel(sj);
    } else if (sj->current_mode == JM_SHORTCUT_CHAR_REPLACING) {
        shortcut_char_replace_complete(sj);
    } else if (sj->current_mode == JM_LINE) {
        shortcut_cancel(sj);
    } else if (sj->current_mode == JM_SUBSTRING) {
        search_cancel(sj);
    } else if (sj->current_mode == JM_REPLACE_SUBSTRING) {
        search_replace_complete(sj);
    } else if (sj->current_mode == JM_MULTICURSOR_REPLACING) {
    }
}

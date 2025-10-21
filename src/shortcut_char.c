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
#include "replace_handle_input.h"
#include "selection.h"
#include "shortcut_common.h"
#include "util.h"
#include "values.h"

/**
 * @brief Gets every char that matches the search query and assigns them tags for jumping.
 *
 * @param ShortcutJump *sj: The plugin object
 * @param gchar search_char: The char to search for
 */
void shrtct_char_get_chars(ShortcutJump *sj, gchar search_char) {
    if (sj->current_mode != JM_SHORTCUT_CHAR_WAITING) {
        return;
    }

    gint lfs_added = 0;
    gint toggle = 1;
    gint added = 0;
    gint prev_line;
    gchar prev_char;

    if (sj->in_selection && sj->config_settings->search_from_selection) {
        prev_line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->first_position, 0);
    } else {
        prev_line = sj->first_line_on_screen - 1;
    }

    if (sj->delete_added_bracket) {
        scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->current_cursor_pos, 1);
        sj->current_cursor_pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
        scintilla_send_message(sj->sci, SCI_SETCURRENTPOS, sj->current_cursor_pos, 0);
        sj->delete_added_bracket = FALSE;
    }

    for (gint i = sj->first_position; i < sj->last_position; i++) {
        if (added == shrtct_get_max_words(sj)) {
            break;
        }

        gchar current_char = scintilla_send_message(sj->sci, SCI_GETCHARAT, i, TRUE);

        if (current_char != search_char) {
            continue;
        }

        gchar char_to_the_left = scintilla_send_message(sj->sci, SCI_GETCHARAT, i - 1, TRUE);

        if (char_to_the_left != prev_char) {
            toggle = 1;
        }

        Word word;

        if (current_char == search_char && prev_char == current_char && toggle == 0) {
            GString *ch = g_string_new("");
            g_string_insert_c(ch, 0, current_char);
            word.word = ch;
            word.valid_search = TRUE;
            word.is_hidden_neighbor = TRUE;
            word.starting = i + lfs_added;
            word.starting_doc = i;
            word.bytes = shrtct_utf8_char_length(word.word->str[0]);
            word.line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, i, 0);
            word.padding = shrtct_set_padding(sj, word.word->len);
            word.replace_pos = i - sj->first_position;
            g_array_append_val(sj->words, word);
            toggle ^= 1;
            continue;
        }

        GString *ch = g_string_new("");
        g_string_insert_c(ch, 0, current_char);
        word.word = ch;
        word.valid_search = TRUE;
        word.is_hidden_neighbor = FALSE;
        word.starting = i + lfs_added;
        word.starting_doc = i;
        word.bytes = shrtct_utf8_char_length(word.word->str[0]);
        word.line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, i, 0);
        word.padding = shrtct_set_padding(sj, word.word->len);
        word.replace_pos = i - sj->first_position;
        word.shortcut = shrtct_make_tag(sj, added++);
        toggle ^= 1;

        gchar line_ending_char = scintilla_send_message(sj->sci, SCI_GETCHARAT, i + 1, TRUE);

        if (line_ending_char == '\n' && word.shortcut->len == 2) {
            g_string_insert_c(sj->buffer, lfs_added + i - sj->first_position, '\n');
            gint line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, i, 0);

            if (line != prev_line) {
                for (gint i = prev_line; i < line; i++) {
                    g_array_append_val(sj->lf_positions, lfs_added);
                }

                prev_line = line;
            }

            lfs_added += 1;
        }

        g_array_append_val(sj->words, word);
        prev_char = current_char;
    }

    for (gint i = prev_line; i < sj->last_line_on_screen; i++) {
        g_array_append_val(sj->lf_positions, lfs_added);
    }

    sj->search_results_count = sj->words->len;
}

/**
 * @brief Clears the memory allocated during an incomplete shortcut jump (when a search query was not provided). This
 * is needed because the words array and other variables that are usually allocated during a char search are not.
 * During JM_SHORTCUT_CHAR_JUMPING we call the standard shortcut_cancel.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void shrtct_char_waiting_cancel(ShortcutJump *sj) {
    annotation_clear(sj->sci, sj->eol_message_line);
    g_string_free(sj->eol_message, TRUE);
    g_string_free(sj->search_query, TRUE);
    g_string_free(sj->cache, TRUE);
    g_string_free(sj->buffer, TRUE);
    g_array_free(sj->lf_positions, TRUE);
    g_array_free(sj->words, TRUE);
    disconnect_key_press_action(sj);
    disconnect_click_action(sj);
    sj->current_mode = JM_NONE;
    ui_set_statusbar(TRUE, _("Shortcut jump canceled (no option)"));
}

/**
 * @brief Clears the indicators used during a character replacement and writes the buffer to the screen.
 *
 * @param ShortcutJump *sj: The plugin object
 */
static void shtct_char_replace_end(ShortcutJump *sj) {
    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);

        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);

        if (word.valid_search) {
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.replace_pos + sj->first_position,
                                   sj->replace_len + 2);
        }
    }

    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->first_position, sj->replace_cache->len);
    scintilla_send_message(sj->sci, SCI_INSERTTEXT, sj->first_position, (sptr_t)sj->replace_cache->str);
    scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);

    if (!sj->search_change_made) {
        scintilla_send_message(sj->sci, SCI_UNDO, 0, 0);
    }

    sj->cursor_in_word = FALSE;
    sj->replace_len = 0;
    sj->search_change_made = FALSE;

    scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);
}

/**
 * @brief Displays  message for canceled shortcut char replacement and frees memory.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void shrtct_char_replace_cancel(ShortcutJump *sj) {
    shtct_char_replace_end(sj);
    shrtct_set_to_first_visible_line(sj);
    shrtct_end(sj, FALSE);
    ui_set_statusbar(TRUE, _("Shortcut replacement canceled"));
}

/**
 * @brief Displays message for completed shortcut jump and frees memory.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void shrtct_char_replace_complete(ShortcutJump *sj) {
    shtct_char_replace_end(sj);
    shrtct_set_to_first_visible_line(sj);
    shrtct_end(sj, FALSE);
    ui_set_statusbar(TRUE, _("Shortcut replacement completed"));
}

/**
 * @brief Handles click event for shortcut char jump. If the search was no completed we clear the initalized values and
 * free memory.
 *
 * @param GtkWidget *widget: (unused)
 * @param GdkEventButton *event: Click event
 * @param gpointer user_data: The plugin data
 *
 * @return gboolean: FALSE if uncontrolled for click or wrong mode
 */
gboolean shrtct_char_on_click_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (mouse_movement_performed(sj, event)) {
        if (sj->current_mode == JM_SHORTCUT_CHAR_WAITING) {
            shrtct_char_waiting_cancel(sj);
            annotation_clear(sj->sci, sj->eol_message_line);
        }

        if (sj->current_mode == JM_SHORTCUT_CHAR_JUMPING) {
            sj->current_cursor_pos = save_cursor_position(sj);
            sj->current_cursor_pos = set_cursor_position_with_lfs(sj);
            shrtct_cancel(sj);
        }

        if (sj->current_mode == JM_SHORTCUT_CHAR_REPLACING) {
            sj->current_cursor_pos = save_cursor_position(sj);
            shrtct_char_replace_cancel(sj);
        }

        return TRUE;
    }

    return FALSE;
}

/**
 * @brief Begins shortcut jump to character mode.
 *
 * @param ShortcutJump *sj: The plugin object
 * @param gboolean init_set: Whether we are using an initial value (see replace_instant_init)
 * @param gchar init: The highlighted character
 */
void shrtct_char_init(ShortcutJump *sj, gboolean init_set, gchar init) {
    sj->current_mode = JM_SHORTCUT_CHAR_WAITING;
    set_sj_scintilla_object(sj);

    if (!init_set) {
        set_selection_info(sj);
    }

    init_sj_values(sj);
    define_indicators(sj->sci, sj);

    if (sj->in_selection && sj->selection_is_a_char && sj->config_settings->use_selected_word_or_char) {
        gchar search_char;

        if (init_set) {
            search_char = init;
        } else {
            search_char = scintilla_send_message(sj->sci, SCI_GETCHARAT, sj->selection_start, sj->selection_end);
        }

        shrtct_char_get_chars(sj, search_char);
        sj->buffer = shrtct_mask_bytes(sj->words, sj->buffer, sj->first_position);
        sj->buffer = shrtct_set_tags_in_buffer(sj->words, sj->buffer, sj->first_position);
        sj->current_cursor_pos = save_cursor_position(sj);
        shrtct_set_after_placement(sj);
        set_shortcut_indicators(sj);
        sj->current_mode = JM_SHORTCUT_CHAR_JUMPING;
        ui_set_statusbar(TRUE, _("%i character%s in view"), sj->words->len, sj->words->len == 1 ? "" : "s");
    }

    if (!sj->in_selection) {
        annotation_display_shortcut_char(sj);
    }

    connect_key_press_action(sj, shrtct_char_on_key_press);
    connect_click_action(sj, shrtct_char_on_click_event);
}

/**
 * @brief Provides a menu callback for performing a shortcut character jump.
 *
 * @param GtkMenuItem *menu_item: (unused)
 * @param gpointer user_data: The plugin data
 */
void shrtct_char_cb(GtkMenuItem *menu_item, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (sj->current_mode == JM_NONE) {
        shrtct_char_init(sj, FALSE, '0');
    }
}

/**
 * @brief Provides a keybinding callback for performing a shortcut character jump.
 *
 * @param GtkMenuItem *kb: (unused)
 * @param guint key_id: (unused)
 * @param gpointer user_data: The plugin data
 *
 * @return gboolean: TRUE
 */
gboolean shrtct_char_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (sj->current_mode == JM_NONE) {
        shrtct_char_init(sj, FALSE, '0');
        return TRUE;
    }

    return TRUE;
}

/**
 * @brief Handles key presses for a shortcut char jump: if we are waiting for a search query we set that char to
 * search_char and proceed to find every instance of the char with shortcut_char_get_chars. If we have a completed
 * search query, we are in jumping mode, and we perform the standard tag jump.
 *
 * @param GtkWidget *widget: (unused)
 * @param GdkEventKey *event: Keypress event
 * @param gpointer user_data: The plugin data
 *
 * @return gboolean: FALSE if uncontrolled for key press
 */
gboolean shrtct_char_on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    gunichar search_char = gdk_keyval_to_unicode(event->keyval);

    if (sj->current_mode == JM_SHORTCUT_CHAR_WAITING) {
        if (mod_key_pressed(event)) {
            return TRUE;
        }

        shrtct_char_get_chars(sj, search_char);

        if (sj->words->len == 0) {
            shrtct_char_waiting_cancel(sj);
            annotation_clear(sj->sci, sj->eol_message_line);
            return FALSE;
        }

        sj->buffer = shrtct_mask_bytes(sj->words, sj->buffer, sj->first_position);
        sj->buffer = shrtct_set_tags_in_buffer(sj->words, sj->buffer, sj->first_position);
        sj->current_cursor_pos = save_cursor_position(sj);
        shrtct_set_after_placement(sj);
        set_shortcut_indicators(sj);
        annotation_display_char_search(sj);
        sj->current_mode = JM_SHORTCUT_CHAR_JUMPING;
        ui_set_statusbar(TRUE, _("%i character%s in view"), sj->words->len, sj->words->len == 1 ? "" : "s");
        return TRUE;
    }

    if (sj->current_mode == JM_SHORTCUT_CHAR_JUMPING) {
        annotation_clear(sj->sci, sj->eol_message_line);
        return shrtct_on_key_press_action(event, sj);
    }

    if (sj->current_mode == JM_SHORTCUT_CHAR_REPLACING) {
        annotation_clear(sj->sci, sj->eol_message_line);
        return replace_handle_input(sj, event, search_char);
    }

    return FALSE;
}

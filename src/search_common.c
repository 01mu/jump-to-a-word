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
#include "search_substring.h"
#include "search_word.h"
#include "util.h"

gboolean valid_smart_case(char haystack_char, char needle_char) {
    gboolean g1 = g_unichar_islower(haystack_char) && g_unichar_islower(needle_char) && needle_char == haystack_char;
    gboolean g2 = g_unichar_isupper(haystack_char) && g_unichar_islower(needle_char) &&
                  g_ascii_tolower(haystack_char) == needle_char;
    gboolean g3 = g_unichar_isupper(haystack_char) && g_unichar_isupper(needle_char) && needle_char == haystack_char;
    return g1 || g2 || g3 || haystack_char == needle_char;
}

gint get_search_word_pos(ShortcutJump *sj) {
    gint closest_to_left = 0;
    gint closest_to_right = 0;
    gint closest_to_left_idx = -1;
    gint closest_to_right_idx = -1;
    gint left_len = 0;
    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        if (word.valid_search && word.starting >= sj->current_cursor_pos) {
            closest_to_right = word.starting;
            closest_to_right_idx = i;
            break;
        }
    }
    for (gint i = sj->words->len - 1; i >= 0; i--) {
        Word word = g_array_index(sj->words, Word, i);
        if (word.valid_search && word.starting < sj->current_cursor_pos) {
            closest_to_left = word.starting;
            closest_to_left_idx = i;
            left_len = word.word->len;
            break;
        }
    }
    if (abs(sj->current_cursor_pos - closest_to_left - left_len) < abs(sj->current_cursor_pos - closest_to_right)) {
        return closest_to_left_idx;
    } else {
        return closest_to_right_idx;
    }
}

gint get_search_word_pos_first(ShortcutJump *sj) {
    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        if (word.valid_search) {
            return i;
        }
    }
    return 0;
}

gint get_search_word_pos_last(ShortcutJump *sj) {
    for (gint i = sj->words->len - 1; i >= 0; i--) {
        Word word = g_array_index(sj->words, Word, i);
        if (word.valid_search) {
            return i;
        }
    }
    return 0;
}

gboolean set_search_word_pos_right_key(ShortcutJump *sj) {
    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        if (sj->search_word_pos == sj->search_word_pos_last && word.valid_search && sj->config_settings->wrap_search) {
            sj->search_word_pos = sj->search_word_pos_first;
            scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, word.starting, word.word->len);
            return TRUE;
        }
        if (i > sj->search_word_pos && word.valid_search) {
            sj->search_word_pos = i;
            scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, word.starting, word.word->len);
            return TRUE;
        }
    }
    return FALSE;
}

gboolean set_search_word_pos_left_key(ShortcutJump *sj) {
    for (gint i = sj->words->len - 1; i >= 0; i--) {
        Word word = g_array_index(sj->words, Word, i);
        if (sj->search_word_pos == sj->search_word_pos_first && word.valid_search && sj->config_settings->wrap_search) {
            sj->search_word_pos = i;
            scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, word.starting, word.word->len);
            return TRUE;
        }
        if (i < sj->search_word_pos && word.valid_search) {
            sj->search_word_pos = i;
            scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, word.starting, word.word->len);
            return TRUE;
        }
    }
    return FALSE;
}

gboolean on_click_event_search(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    if (mouse_movement_performed(sj, event)) {
        sj->current_cursor_pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
        scintilla_send_message(sj->sci, SCI_SETCURRENTPOS, sj->current_cursor_pos, 0);
        if (sj->current_mode == JM_SEARCH) {
            search_word_jump_cancel(sj);
            return TRUE;
        } else if (sj->current_mode == JM_SUBSTRING) {
            search_substring_jump_cancel(sj);
            return TRUE;
        } else if (sj->current_mode == JM_REPLACE_SEARCH) {
            search_word_replace_cancel(sj);
            return TRUE;
        } else if (sj->current_mode == JM_REPLACE_SUBSTRING) {
            search_substring_replace_cancel(sj);
            return TRUE;
        }
    }
    return FALSE;
}

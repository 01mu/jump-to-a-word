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
#include "multicursor.h"
#include "search_substring.h"
#include "search_word.h"
#include "shortcut_char.h"

/**
 * @brief Controls for the user pressing a single backspace and deletes every occurance of the selected text.
 *
 * @param ShortcutJump *sj: The plugin object
 */
static void handle_single_backspace(ShortcutJump *sj) {
    gint chars_removed = 0;
    gint rem_to_left = 0;

    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->first_position, sj->replace_cache->len);

    for (gint i = 0; i < sj->words->len; i++) {
        Word *word = &g_array_index(sj->words, Word, i);

        if (word->valid_search) {
            word->replace_pos -= chars_removed;
            g_string_erase(sj->replace_cache, word->replace_pos, word->word->len);
            chars_removed += word->word->len;

            if (word->starting < sj->current_cursor_pos) {
                rem_to_left += word->word->len;
            }
        }
    }

    scintilla_send_message(sj->sci, SCI_INSERTTEXT, sj->first_position, (sptr_t)sj->replace_cache->str);

    sj->search_change_made = TRUE;
    sj->current_cursor_pos -= rem_to_left;

    scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);
}

/**
 * @brief Deletes every occurance of the text if no change has been made previously and determines if the cursor is
 * within a word to ensure that it maintains the correct position while the text is being updated.
 *
 * @param ShortcutJump *sj: The plugin object
 */
static void clear_occurances(ShortcutJump *sj) {
    gint chars_removed = 0;
    gint removed_to_left = 0;
    gint into = 0;

    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->first_position, sj->replace_cache->len);

    for (gint i = 0; i < sj->words->len; i++) {
        Word *word = &g_array_index(sj->words, Word, i);

        if (word->valid_search) {
            if (sj->current_cursor_pos >= word->starting_doc &&
                sj->current_cursor_pos < word->starting_doc + word->word->len) {
                sj->cursor_in_word = TRUE;
            }

            word->replace_pos -= chars_removed;
            g_string_erase(sj->replace_cache, word->replace_pos, word->word->len);
            chars_removed += word->word->len;

            if (word->starting_doc < sj->current_cursor_pos) {
                if (sj->cursor_in_word) {
                    into = word->word->len - (sj->current_cursor_pos - word->starting_doc);
                }

                removed_to_left = chars_removed;
            }
        }
    }

    scintilla_send_message(sj->sci, SCI_INSERTTEXT, sj->first_position, (sptr_t)sj->replace_cache->str);
    sj->current_cursor_pos -= removed_to_left - into;
    scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);
}

/**
 * @brief Adds a new character to the replacement buffer for each occurance, maintains the correct cursor position, and
 * updates the indicators.
 *
 * @param ShortcutJump *sj: The plugin object
 */
static void add_character(ShortcutJump *sj, gunichar keychar) {
    gint chars_added = 0;
    gint c = 0;

    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->first_position, sj->replace_cache->len);

    for (gint i = 0; i < sj->words->len; i++) {
        Word *word = &g_array_index(sj->words, Word, i);

        if (word->valid_search) {
            int t = word->replace_pos;

            word->replace_pos += chars_added;

            g_string_insert_c(sj->replace_cache, word->replace_pos + sj->replace_len, keychar);
            chars_added += 1;

            if (t + sj->first_position < sj->current_cursor_pos) {
                c = chars_added;
            }
        }
    }

    scintilla_send_message(sj->sci, SCI_INSERTTEXT, sj->first_position, (sptr_t)sj->replace_cache->str);

    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);

        if (word.valid_search) {
            gint start = sj->first_position + word.replace_pos;
            gint len = sj->replace_len + 1;

            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, start, len);
            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, start, len);
        }
    }

    sj->current_cursor_pos += c;

    if (sj->cursor_in_word) {
        scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos + sj->replace_len + 1, 0);
    } else {
        scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);
    }

    sj->replace_len += 1;
    sj->search_change_made = TRUE;
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 1, 0);
}
/**
 * @brief Removes the last character from the replacement buffer for every occurance, maintains the correct cursor
 * position, and updates the indicators.
 *
 * @param ShortcutJump *sj: The plugin object
 */

static void remove_character(ShortcutJump *sj) {
    gint chars_removed = 0;
    gint c = 0;

    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->first_position, sj->replace_cache->len);

    for (gint i = 0; i < sj->words->len; i++) {
        Word *word = &g_array_index(sj->words, Word, i);

        if (word->valid_search) {
            gint t = word->replace_pos;

            g_string_erase(sj->replace_cache, word->replace_pos - chars_removed + sj->replace_len - 1, 1);
            word->replace_pos -= chars_removed;
            chars_removed += 1;

            if (t + sj->first_position < sj->current_cursor_pos) {
                c = chars_removed;
            }
        }
    }

    scintilla_send_message(sj->sci, SCI_INSERTTEXT, sj->first_position, (sptr_t)sj->replace_cache->str);

    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);

        if (word.valid_search) {
            gint start = sj->first_position + word.replace_pos;
            gint len = sj->replace_len - 1;

            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, start, len);
            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, start, len);
        }
    }

    sj->current_cursor_pos -= c;

    if (sj->cursor_in_word) {
        scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos + sj->replace_len - 1, 0);
    } else {
        scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);
    }

    sj->replace_len -= 1;
    sj->search_change_made = TRUE;
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 1, 0);
}

/**
 * @brief Handles inputs for reaplce search word mode, updates indicators, deletes the last input char when Backspace
 * is pressed and inputs a new char if it is valid. Also works to maintain the correct cursor position when updating.
 *
 * @param ShortcutJump *sj: The plugin object
 * @param GdkEventKey *event: A struct containing the key event
 * @param gunichar keychar: The key input value
 *
 * @return gboolean: TRUE if controlled for input occurs
 */
gboolean replace_handle_input(ShortcutJump *sj, GdkEventKey *event, gunichar keychar) {
    gint pos_cache = sj->current_cursor_pos;

    gboolean is_other_char =
        strchr("[]\\;'.,/-=_+{`_+|}:<>?\"~)(*&^%$#@!) ", (gchar)gdk_keyval_to_unicode(event->keyval)) ||
        (event->keyval >= GDK_KEY_0 && event->keyval <= GDK_KEY_9);

    scintilla_send_message(sj->sci, SCI_SETREADONLY, 1, 0);
    scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);

    if (keychar != 0 && (event->keyval == GDK_KEY_BackSpace || event->keyval == GDK_KEY_Delete) &&
        !sj->search_change_made) {
        handle_single_backspace(sj);

        if (sj->current_mode == JM_REPLACE_MULTICURSOR) {
            multicursor_replace_complete(sj);
        } else if (sj->current_mode == JM_REPLACE_SEARCH) {
            search_word_replace_complete(sj);
        } else if (sj->current_mode == JM_REPLACE_SUBSTRING) {
            search_substring_replace_complete(sj);
        } else if (sj->current_mode == JM_INSERTING_LINE) {
            line_insert_complete(sj);
        } else if (sj->current_mode == JM_INSERTING_LINE_MULTICURSOR) {
            multicursor_line_insert_complete(sj);
        } else if (sj->current_mode == JM_SHORTCUT_CHAR_REPLACING) {
            shortcut_char_replacing_complete(sj);
        }

        return TRUE;
    }

    if (keychar != 0 && (g_unichar_isalpha(keychar) || is_other_char) && sj->replace_len >= 0) {
        if (sj->config_settings->replace_action == RA_REPLACE && !sj->search_change_made) {
            clear_occurances(sj);
        }

        add_character(sj, keychar);
        return TRUE;
    }

    if (keychar != 0 && (event->keyval == GDK_KEY_BackSpace || event->keyval == GDK_KEY_Delete) &&
        sj->replace_len >= 0) {
        if (sj->replace_len == 0) {
            if (sj->current_mode == JM_REPLACE_MULTICURSOR) {
                multicursor_replace_complete(sj);
            } else if (sj->current_mode == JM_REPLACE_SEARCH) {
                search_word_replace_complete(sj);
            } else if (sj->current_mode == JM_REPLACE_SUBSTRING) {
                search_substring_replace_complete(sj);
            } else if (sj->current_mode == JM_INSERTING_LINE) {
                line_insert_complete(sj);
            } else if (sj->current_mode == JM_INSERTING_LINE_MULTICURSOR) {
                multicursor_line_insert_complete(sj);
            } else if (sj->current_mode == JM_SHORTCUT_CHAR_REPLACING) {
                shortcut_char_replacing_complete(sj);
            }

            return TRUE;
        }

        remove_character(sj);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_Shift_L || event->keyval == GDK_KEY_Shift_R || event->keyval == GDK_KEY_Caps_Lock ||
        event->keyval == GDK_KEY_Control_L || event->keyval == GDK_KEY_Control_R) {
        return TRUE;
    }

    if (event->keyval == GDK_KEY_Return) {
        if (sj->current_mode == JM_REPLACE_MULTICURSOR) {
            multicursor_replace_complete(sj);
        } else if (sj->current_mode == JM_REPLACE_SEARCH) {
            search_word_replace_complete(sj);
        } else if (sj->current_mode == JM_REPLACE_SUBSTRING) {
            search_substring_replace_complete(sj);
        } else if (sj->current_mode == JM_INSERTING_LINE) {
            line_insert_complete(sj);
        } else if (sj->current_mode == JM_INSERTING_LINE_MULTICURSOR) {
            multicursor_line_insert_complete(sj);
        } else if (sj->current_mode == JM_SHORTCUT_CHAR_REPLACING) {
            shortcut_char_replacing_complete(sj);
        }

        return TRUE;
    }

    if (sj->replace_len > 0) {
        sj->current_cursor_pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
    } else {
        sj->current_cursor_pos = pos_cache;
    }

    if (sj->search_change_made) {
        if (sj->current_mode == JM_REPLACE_MULTICURSOR) {
            multicursor_replace_complete(sj);
        } else if (sj->current_mode == JM_REPLACE_SEARCH) {
            search_word_replace_complete(sj);
        } else if (sj->current_mode == JM_REPLACE_SUBSTRING) {
            search_substring_replace_complete(sj);
        } else if (sj->current_mode == JM_INSERTING_LINE) {
            line_insert_complete(sj);
        } else if (sj->current_mode == JM_INSERTING_LINE_MULTICURSOR) {
            multicursor_line_insert_complete(sj);
        } else if (sj->current_mode == JM_SHORTCUT_CHAR_REPLACING) {
            shortcut_char_replacing_complete(sj);
        }
    } else {
        if (sj->current_mode == JM_REPLACE_MULTICURSOR) {
            multicursor_replace_cancel(sj);
        } else if (sj->current_mode == JM_REPLACE_SEARCH) {
            search_word_replace_cancel(sj);
        } else if (sj->current_mode == JM_REPLACE_SUBSTRING) {
            search_substring_replace_cancel(sj);
        } else if (sj->current_mode == JM_INSERTING_LINE) {
            line_insert_cancel(sj);
        } else if (sj->current_mode == JM_INSERTING_LINE_MULTICURSOR) {
            multicursor_line_insert_cancel(sj);
        } else if (sj->current_mode == JM_SHORTCUT_CHAR_REPLACING) {
            shortcut_char_replacing_cancel(sj);
        }
    }

    return FALSE;
}

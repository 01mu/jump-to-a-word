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

void clear_occurances(ShortcutJump *sj) {
    gint chars_removed = 0;
    gint removed_to_left = 0;

    if (!sj->config_settings->disable_live_replace) {
        scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->first_position, sj->replace_cache->len);
    }

    for (gint i = 0; i < sj->words->len; i++) {
        Word *word = &g_array_index(sj->words, Word, i);

        if (word->valid_search) {
            word->replace_pos -= chars_removed;
            g_string_erase(sj->replace_cache, word->replace_pos, word->word->len);
            chars_removed += word->word->len;

            if (word->starting_doc < sj->cursor_moved_to_eol) {
                removed_to_left = chars_removed;
            }
        }
    }

    if (!sj->config_settings->disable_live_replace) {
        scintilla_send_message(sj->sci, SCI_INSERTTEXT, sj->first_position, (sptr_t)sj->replace_cache->str);

        sj->current_cursor_pos -= removed_to_left;
        sj->cursor_moved_to_eol -= removed_to_left;
    }

    scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->cursor_moved_to_eol, 0);
}

static void add_character(ShortcutJump *sj, gunichar keychar) {
    gint chars_added = 0;
    gint c = -1;
    gint prev;

    if (!sj->config_settings->disable_live_replace) {
        scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->first_position, sj->replace_cache->len);
    }

    for (gint i = 0; i < sj->words->len; i++) {
        Word *word = &g_array_index(sj->words, Word, i);

        if (word->valid_search) {
            gint v = chars_added + sj->cursor_moved_to_eol;

            word->replace_pos += chars_added;
            g_string_insert_c(sj->replace_cache, word->replace_pos + sj->replace_len, keychar);
            chars_added += 1;

            if (c == -1 && word->replace_pos + sj->first_position > v) {
                c = prev;
            } else {
                prev = chars_added;
            }
        }
    }

    if (c == -1) {
        c = chars_added;
    }

    if (!sj->config_settings->disable_live_replace) {
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
        sj->cursor_moved_to_eol += c;
    }

    scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->cursor_moved_to_eol, 0);

    g_string_insert_c(sj->replace_query, sj->replace_len, keychar);
    sj->replace_len += 1;
    sj->search_change_made = TRUE;

    if (sj->config_settings->disable_live_replace) {
        annotation_display_replace_string(sj);
    }
}

static void backspace_character(ShortcutJump *sj) {
    gint chars_removed = 0;
    gint c = -1;
    gint prev;

    if (!sj->config_settings->disable_live_replace) {
        scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->first_position, sj->replace_cache->len);
    }

    for (gint i = 0; i < sj->words->len; i++) {
        Word *word = &g_array_index(sj->words, Word, i);

        if (word->valid_search) {
            gint v = sj->cursor_moved_to_eol - chars_removed;

            g_string_erase(sj->replace_cache, word->replace_pos - chars_removed + sj->replace_len - 1, 1);
            word->replace_pos -= chars_removed;
            chars_removed += 1;

            if (c == -1 && word->replace_pos + sj->first_position > v) {
                c = prev;
            } else {
                prev = chars_removed;
            }
        }
    }

    if (c == -1) {
        c = chars_removed;
    }

    if (!sj->config_settings->disable_live_replace) {
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
        sj->cursor_moved_to_eol -= c;
    }

    scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->cursor_moved_to_eol, 0);

    sj->replace_len -= 1;
    g_string_erase(sj->replace_query, sj->replace_len, 1);
    sj->search_change_made = TRUE;

    if (sj->config_settings->disable_live_replace) {
        annotation_display_replace_string(sj);
    }
}

static gboolean delete_character(ShortcutJump *sj) {
    gint chars_removed = 0;
    gint c = -1;
    gint prev;

    if (!sj->config_settings->disable_live_replace) {
        scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->first_position, sj->replace_cache->len);
    }

    for (gint i = 0; i < sj->words->len; i++) {
        Word *word = &g_array_index(sj->words, Word, i);

        if (word->valid_search) {
            gint v = sj->cursor_moved_to_eol - chars_removed;
            gint p = word->replace_pos + sj->replace_len - chars_removed;

            if (p < sj->replace_cache->len) {
                g_string_erase(sj->replace_cache, p, 1);
            } else {
                return TRUE;
            }

            word->replace_pos -= chars_removed;
            chars_removed += 1;

            if (c == -1 && word->replace_pos + sj->first_position > v) {
                c = prev;
            } else {
                prev = chars_removed;
            }
        }
    }

    if (c == -1) {
        c = chars_removed;
    }

    if (!sj->config_settings->disable_live_replace) {
        scintilla_send_message(sj->sci, SCI_INSERTTEXT, sj->first_position, (sptr_t)sj->replace_cache->str);

        for (gint i = 0; i < sj->words->len; i++) {
            Word word = g_array_index(sj->words, Word, i);

            if (word.valid_search) {
                gint start = sj->first_position + word.replace_pos;
                gint len = sj->replace_len;

                scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);
                scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, start, len);
                scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);
                scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, start, len);
            }
        }

        sj->current_cursor_pos -= c;
        sj->cursor_moved_to_eol -= c;
    }

    scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->cursor_moved_to_eol, 0);

    sj->search_change_made = TRUE;

    if (sj->config_settings->disable_live_replace) {
        annotation_display_replace_string(sj);
    }

    return FALSE;
}

gboolean replace_handle_input(ShortcutJump *sj, GdkEventKey *event, gunichar keychar,
                              void complete_func(ShortcutJump *), void cancel_func(ShortcutJump *)) {
    if (keychar != 0) {
        if (event->keyval == GDK_KEY_BackSpace) {
            if (sj->search_change_made) {
                if (sj->replace_len == 0) {
                    complete_func(sj);
                    return TRUE;
                }

                backspace_character(sj);
            } else {
                clear_occurances(sj);
                complete_func(sj);
            }

            return TRUE;
        } else if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_Tab) {
            if (sj->config_settings->replace_action == RA_REPLACE && !sj->search_change_made) {
                clear_occurances(sj);
            }

            add_character(sj, keychar);
            return TRUE;
        } else if (event->keyval == GDK_KEY_Delete) {
            gboolean aborted = delete_character(sj);

            if (aborted) {
                complete_func(sj);
            }

            return TRUE;
        }

        if ((g_unichar_isalpha(keychar) ||
             (strchr("[]\\;'.,/-=_+{`_+|}:<>?\"~)(*&^%$#@!) ", (gchar)gdk_keyval_to_unicode(event->keyval)) ||
              (event->keyval >= GDK_KEY_0 && event->keyval <= GDK_KEY_9))) &&
            sj->replace_len >= 0) {
            if (sj->config_settings->replace_action == RA_REPLACE && !sj->search_change_made) {
                clear_occurances(sj);
            }

            add_character(sj, keychar);
            return TRUE;
        }
    }

    if (event->keyval == GDK_KEY_Shift_L || event->keyval == GDK_KEY_Shift_R || event->keyval == GDK_KEY_Caps_Lock ||
        event->keyval == GDK_KEY_Control_L || event->keyval == GDK_KEY_Control_R) {
        annotation_display_replace_string(sj);
        return TRUE;
    }

    if (sj->search_change_made) {
        complete_func(sj);
    } else {
        cancel_func(sj);
    }

    return FALSE;
}

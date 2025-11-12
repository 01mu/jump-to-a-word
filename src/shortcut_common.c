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

#include <math.h>
#include <plugindata.h>

#include "jump_to_a_word.h"
#include "multicursor.h"
#include "search_substring.h"
#include "search_word.h"
#include "shortcut_char.h"
#include "shortcut_line.h"
#include "shortcut_word.h"
#include "util.h"
#include "values.h"

void shortcut_end(ShortcutJump *sj, gboolean was_canceled) {
    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        g_string_free(word.word, TRUE);
        if (!word.is_hidden_neighbor) {
            g_string_free(word.shortcut, TRUE);
        }
    }

    gboolean in_line_jump_mode;

    if (sj->current_mode == JM_LINE) {
        in_line_jump_mode = TRUE;
    } else {
        in_line_jump_mode = FALSE;
    }

    margin_markers_reset(sj);

    sj->current_mode = JM_NONE;

    g_string_free(sj->search_query, TRUE);
    g_string_free(sj->cache, TRUE);
    g_string_free(sj->buffer, TRUE);
    g_string_free(sj->replace_cache, TRUE);
    g_array_free(sj->lf_positions, TRUE);
    g_array_free(sj->words, TRUE);
    g_array_free(sj->markers, TRUE);

    disconnect_key_press_action(sj);
    disconnect_click_action(sj);

    if (sj->multicursor_enabled == MC_ACCEPTING) {
        for (gint i = 0; i < sj->multicursor_words->len; i++) {
            Word word = g_array_index(sj->multicursor_words, Word, i);
            if (word.valid_search) {
                scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_MULTICURSOR, 0);
                scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, word.starting_doc, word.word->len);
            }
        }
    }

    if (in_line_jump_mode && !was_canceled) {
        if (sj->config_settings->line_after == LA_JUMP_TO_CHARACTER_SHORTCUT) {
            shortcut_char_init(sj);
        } else if (sj->config_settings->line_after == LA_JUMP_TO_WORD_SHORTCUT) {
            shortcut_word_init(sj);
        } else if (sj->config_settings->line_after == LA_JUMP_TO_WORD_SEARCH) {
            search_init(sj, FALSE);
        } else if (sj->config_settings->line_after == LA_JUMP_TO_SUBSTRING_SEARCH) {
            substring_init(sj, FALSE);
        }
    }
}

void shortcut_set_to_first_visible_line(ShortcutJump *sj) {
    if (!sj->in_selection || sj->selection_is_a_word || sj->selection_is_a_char) {
        scintilla_send_message(sj->sci, SCI_SETFIRSTVISIBLELINE, sj->first_line_on_screen, 0);
    }
}

gint shortcut_get_max_words(ShortcutJump *sj) {
    if (sj->config_settings->shortcuts_include_single_char) {
        return 720;
    } else {
        return 676;
    }
}

GString *shortcut_mask_bytes(GArray *words, GString *buffer, gint first_position) {
    for (gint i = 0; i < words->len; i++) {
        Word word = g_array_index(words, Word, i);
        gint starting = word.starting - first_position;
        for (gint j = 0; j < word.bytes; j++) {
            buffer->str[starting + j + word.padding] = ' ';
        }
    }
    return buffer;
}

GString *shortcut_set_tags_in_buffer(GArray *words, GString *buffer, gint first_position) {
    for (gint i = 0; i < words->len; i++) {
        Word word = g_array_index(words, Word, i);
        gint starting = word.starting - first_position;
        if (!word.is_hidden_neighbor) {
            for (gint j = 0; j < word.shortcut->len; j++) {
                buffer->str[starting + j + word.padding] = word.shortcut->str[j];
            }
        }
    }
    return buffer;
}

GString *shortcut_make_tag(ShortcutJump *sj, gint position) {
    if (!sj->config_settings->shortcuts_include_single_char) {
        position += 26;
    }
    if (position < 0) {
        return NULL;
    }
    gint word_length = 1;
    gint temp = position;
    while (temp >= 26) {
        temp = (temp / 26) - 1;
        word_length++;
    }
    GString *result = g_string_new("");
    if (!result) {
        return NULL;
    }
    for (gint i = word_length - 1; i >= 0; i--) {
        gchar ch = (sj->config_settings->shortcut_all_caps ? 'A' : 'a') + (position % 26);
        g_string_prepend_c(result, ch);
        position = (position / 26) - 1;
    }
    return result;
}

static gint shortcut_get_search_results_count(ScintillaObject *sci, GArray *words) {
    gint search_results_count = 0;
    for (gint i = 0; i < words->len; i++) {
        Word *word = &g_array_index(words, Word, i);
        if (word->shortcut_marked) {
            search_results_count += 1;
        }
    }
    return search_results_count;
}

static gint shortcut_get_highlighted_pos(ScintillaObject *sci, GArray *words) {
    for (gint i = 0; i < words->len; i++) {
        Word word = g_array_index(words, Word, i);
        if (word.valid_search) {
            return i;
        }
    }
    return -1;
}

static GArray *shortcut_mark_indicators(ScintillaObject *sci, GArray *words, GString *search_query) {
    for (gint i = 0; i < words->len; i++) {
        Word *word = &g_array_index(words, Word, i);
        word->shortcut_marked = FALSE;
        word->valid_search = FALSE;
        scintilla_send_message(sci, SCI_INDICATORCLEARRANGE, word->starting, 2);
    }
    for (gint i = 0; i < words->len; i++) {
        Word *word = &g_array_index(words, Word, i);
        if (word->is_hidden_neighbor) {
            continue;
        }
        if (g_str_has_prefix(word->shortcut->str, search_query->str) && search_query->len > 0) {
            word->shortcut_marked = TRUE;
        }
        if (g_strcmp0(word->shortcut->str, search_query->str) == 0) {
            word->valid_search = TRUE;
        }
    }
    return words;
}

gint shortcut_get_utf8_char_length(gchar c) {
    if ((c & 0x80) == 0) {
        return 1;
    } else if ((c & 0xE0) == 0xC0) {
        return 2;
    } else if ((c & 0xF0) == 0xE0) {
        return 3;
    } else if ((c & 0xF8) == 0xF0) {
        return 4;
    }
    return -1;
}

gint shortcut_set_padding(ShortcutJump *sj, gint word_length) {
    if (sj->config_settings->center_shortcut) {
        return word_length >= 3 ? floor((float)word_length / 2) : 0;
    }
    return 0;
}

void shortcut_set_after_placement(ShortcutJump *sj) {
    gint current_line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->current_cursor_pos, 0);
    gint lfs_added = get_lfs(sj, current_line);
    shortcut_set_to_first_visible_line(sj);
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    scintilla_send_message(sj->sci, SCI_BEGINUNDOACTION, 0, 0);
    scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->first_position, sj->last_position - sj->first_position);
    scintilla_send_message(sj->sci, SCI_INSERTTEXT, sj->first_position, (sptr_t)sj->buffer->str);
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 1, 0);
    scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos + lfs_added, 0);
}

gint shortcut_on_key_press_action(GdkEventKey *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    gunichar keychar = gdk_keyval_to_unicode(event->keyval);

    scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_HIGHLIGHT, 0);
    sj->words = shortcut_mark_indicators(sj->sci, sj->words, sj->search_query);
    sj->search_results_count = shortcut_get_search_results_count(sj->sci, sj->words);
    sj->shortcut_single_pos = shortcut_get_highlighted_pos(sj->sci, sj->words);

    if (keychar >= 96 && keychar <= 122 && sj->config_settings->shortcut_all_caps) {
        keychar -= 32;
    }

    if (event->keyval == GDK_KEY_BackSpace) {
        if (sj->search_query->len == 0) {
            if (sj->current_mode == JM_SHORTCUT_CHAR_JUMPING) {
                shortcut_char_jumping_cancel(sj);
            } else if (sj->current_mode == JM_LINE) {
                shortcut_line_cancel(sj);
            } else if (sj->current_mode == JM_SHORTCUT_WORD) {
                shortcut_word_cancel(sj);
            }
            return TRUE;
        }

        g_string_truncate(sj->search_query, sj->search_query->len - 1);
        sj->words = shortcut_mark_indicators(sj->sci, sj->words, sj->search_query);

        for (gint i = 0; i < sj->words->len; i++) {
            Word word = g_array_index(sj->words, Word, i);
            gint start = word.starting + word.padding;

            if (word.shortcut_marked) {
                scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, start, sj->search_query->len);
            }
        }
        return TRUE;
    }

    if (event->keyval == GDK_KEY_Return) {
        Word word;

        if (sj->shortcut_single_pos == -1 && sj->current_mode != JM_LINE) {
            if (sj->current_mode == JM_SHORTCUT_CHAR_JUMPING) {
                shortcut_char_jumping_cancel(sj);
            } else if (sj->current_mode == JM_LINE) {
                shortcut_line_cancel(sj);
            } else if (sj->current_mode == JM_SHORTCUT_WORD) {
                shortcut_word_cancel(sj);
            }
            return TRUE;
        }

        if (strcmp(sj->search_query->str, "") == 0 && sj->current_mode == JM_LINE) {
            gint current_pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
            gint current_line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, current_pos, 0);
            gint lfs = get_lfs(sj, current_line);

            current_line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->current_cursor_pos + lfs, 0);

            if (current_line - sj->first_line_on_screen >= sj->words->len) {
                shortcut_line_cancel(sj);
                return TRUE;
            }

            word = g_array_index(sj->words, Word, current_line - sj->first_line_on_screen);
        } else {
            if (sj->shortcut_single_pos >= sj->words->len) {
                shortcut_line_cancel(sj);
                return TRUE;
            }

            word = g_array_index(sj->words, Word, sj->shortcut_single_pos);
        }

        if (sj->multicursor_enabled == MC_ACCEPTING && sj->current_mode != JM_LINE) {
            multicursor_add_word(sj, word);
        }

        if (sj->current_mode == JM_SHORTCUT_CHAR_JUMPING) {
            shortcut_char_jumping_complete(sj, word.starting_doc, word.word->len, word.line);
        } else if (sj->current_mode == JM_LINE) {
            shortcut_line_complete(sj, word.starting_doc, word.word->len, word.line);
        } else if (sj->current_mode == JM_SHORTCUT_WORD) {
            shortcut_word_complete(sj, word.starting_doc, word.word->len, word.line);
        }
        return TRUE;
    }

    if (keychar != 0 && g_unichar_isalnum(keychar)) {
        g_string_append_c(sj->search_query, keychar);

        sj->words = shortcut_mark_indicators(sj->sci, sj->words, sj->search_query);
        sj->search_results_count = shortcut_get_search_results_count(sj->sci, sj->words);
        sj->shortcut_single_pos = shortcut_get_highlighted_pos(sj->sci, sj->words);

        if (sj->search_results_count == 0) {
            if (sj->current_mode == JM_SHORTCUT_CHAR_JUMPING) {
                shortcut_char_jumping_cancel(sj);
            } else if (sj->current_mode == JM_LINE) {
                shortcut_line_cancel(sj);
            } else if (sj->current_mode == JM_SHORTCUT_WORD) {
                shortcut_word_cancel(sj);
            }
            return TRUE;
        }

        for (gint i = 0; i < sj->words->len; i++) {
            Word word = g_array_index(sj->words, Word, i);
            gint start = word.starting + word.padding;

            if (word.shortcut_marked) {
                scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, start, sj->search_query->len);
            }
        }

        if (sj->search_results_count == 1 && !sj->config_settings->wait_for_enter) {
            Word word = g_array_index(sj->words, Word, sj->shortcut_single_pos);

            if (sj->multicursor_enabled == MC_ACCEPTING && sj->current_mode != JM_LINE) {
                multicursor_add_word(sj, word);
            }

            if (sj->current_mode == JM_SHORTCUT_CHAR_JUMPING) {
                shortcut_char_jumping_complete(sj, word.starting_doc, word.word->len, word.line);
            } else if (sj->current_mode == JM_LINE) {
                shortcut_line_complete(sj, word.starting_doc, word.word->len, word.line);
            } else if (sj->current_mode == JM_SHORTCUT_WORD) {
                shortcut_word_complete(sj, word.starting_doc, word.word->len, word.line);
            }
        }

        return TRUE;
    }

    if (sj->current_mode == JM_SHORTCUT_CHAR_JUMPING) {
        shortcut_char_jumping_cancel(sj);
    } else if (sj->current_mode == JM_LINE) {
        shortcut_line_cancel(sj);
    } else if (sj->current_mode == JM_SHORTCUT_WORD) {
        shortcut_word_cancel(sj);
    }

    return FALSE;
}

void shortcut_set_indicators(ShortcutJump *sj) {
    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        if (!word.is_hidden_neighbor) {
            set_indicator_for_range(sj->sci, INDICATOR_TAG, word.starting + word.padding, word.shortcut->len);
            set_indicator_for_range(sj->sci, INDICATOR_TEXT, word.starting + word.padding, word.shortcut->len);
        }
    }
}

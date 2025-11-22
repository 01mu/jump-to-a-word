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

#include "action_text_after.h"
#include "annotation.h"
#include "jump_to_a_word.h"
#include "multicursor.h"
#include "search_common.h"
#include "selection.h"
#include "util.h"
#include "values.h"

static void search_word_clear_replace_indicators(ShortcutJump *sj) {
    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        if (word.valid_search) {
            gint start = sj->first_position + word.replace_pos;
            gint len = sj->replace_len;
            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, start, len);
            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_HIGHLIGHT, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, start, len);
            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TEXT, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, start, len);
        }
    }
}

static void search_word_clear_jump_indicators(ShortcutJump *sj) {
    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        if (word.valid_search) {
            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, word.word->len);
            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_HIGHLIGHT, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, word.word->len);
            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TEXT, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, word.word->len);
        }
    }
}

void search_word_end(ShortcutJump *sj) {
    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        g_string_free(word.word, TRUE);
    }

    g_string_free(sj->replace_query, TRUE);

    g_string_free(sj->eol_message, TRUE);
    g_string_free(sj->search_query, TRUE);

    sj->search_results_count = 0;
    sj->search_word_pos = -1;
    sj->search_word_pos_first = -1;
    sj->search_word_pos_last = -1;
    sj->search_change_made = FALSE;
    sj->cursor_in_word = FALSE;
    sj->delete_added_bracket = FALSE;
    sj->replace_len = 0;
    sj->replace_instant = FALSE;

    g_string_free(sj->cache, TRUE);
    g_string_free(sj->buffer, TRUE);
    g_string_free(sj->replace_cache, TRUE);

    g_array_free(sj->lf_positions, TRUE);
    g_array_free(sj->words, TRUE);
    g_array_free(sj->markers, TRUE);

    sj->current_mode = JM_NONE;
}

void search_word_replace_complete(ShortcutJump *sj) {
    search_word_clear_replace_indicators(sj);
    search_word_clear_jump_indicators(sj);
    scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);
    ui_set_statusbar(TRUE, _("Word replacement completed (%i change%s made)."), sj->search_results_count,
                     sj->search_results_count == 1 ? "" : "s");
    margin_markers_reset(sj);
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    annotation_clear(sj->sci, sj->eol_message_line);
    disconnect_key_press_action(sj);
    disconnect_click_action(sj);

    if (sj->has_previous_action) {
        g_string_free(sj->previous_search_query, TRUE);
        g_string_free(sj->previous_replace_query, TRUE);
    }

    sj->previous_search_query = g_string_new(sj->search_query->str);
    sj->previous_replace_query = g_string_new(sj->replace_query->str);
    sj->previous_mode = sj->current_mode;
    sj->previous_replace_action = sj->config_settings->replace_action;
    sj->has_previous_action = TRUE;

    search_word_end(sj);
}

void search_word_replace_cancel(ShortcutJump *sj) {
    search_word_clear_replace_indicators(sj);
    search_word_clear_jump_indicators(sj);
    scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);
    margin_markers_reset(sj);
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    annotation_clear(sj->sci, sj->eol_message_line);
    disconnect_key_press_action(sj);
    disconnect_click_action(sj);
    search_word_end(sj);
    ui_set_statusbar(TRUE, _("Word replacement canceled."));
}

void search_word_jump_complete(ShortcutJump *sj) {
    search_word_clear_jump_indicators(sj);

    if (sj->multicursor_mode == MC_ACCEPTING) {
        for (gint i = 0; i < sj->words->len; i++) {
            Word word = g_array_index(sj->words, Word, i);
            if (word.valid_search) {
                scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_MULTICURSOR, 0);
                scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, word.starting_doc, word.word->len);
                multicursor_add_word(sj, word);
            }
        }
    }

    Word word = g_array_index(sj->words, Word, sj->search_word_pos);
    gint pos = word.starting;
    gint line = word.line;
    gint word_length = word.word->len;

    sj->previous_cursor_pos = sj->current_cursor_pos;
    scintilla_send_message(sj->sci, SCI_GOTOPOS, word.starting, 0);

    if (sj->config_settings->move_marker_to_line) {
        GeanyDocument *doc = document_get_current();

        if (!doc->is_valid) {
            exit(1);
        } else {
            navqueue_goto_line(doc, doc, word.line + 1);
        }
    }

    gboolean clear_previous_marker = FALSE;

    if (sj->multicursor_mode == MC_DISABLED) {
        clear_previous_marker = handle_text_after_action(sj, pos, word_length, line);
    }

    if (sj->multicursor_mode == MC_ACCEPTING) {
        scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);
    }

    margin_markers_reset(sj);

    if (sj->multicursor_mode != MC_ACCEPTING) {
        scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    }

    annotation_clear(sj->sci, sj->eol_message_line);
    disconnect_key_press_action(sj);
    disconnect_click_action(sj);
    search_word_end(sj);

    if (clear_previous_marker) {
        gint line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->range_first_pos, 0);
        scintilla_send_message(sj->sci, SCI_MARKERDELETE, line, -1);
    }

    ui_set_statusbar(TRUE, _("Word search completed."));
}

void search_word_jump_cancel(ShortcutJump *sj) {
    search_word_clear_jump_indicators(sj);
    margin_markers_reset(sj);
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    annotation_clear(sj->sci, sj->eol_message_line);
    disconnect_key_press_action(sj);
    disconnect_click_action(sj);
    search_word_end(sj);
    ui_set_statusbar(TRUE, _("Word search canceled."));
}

void search_word_mark_words(ShortcutJump *sj, gboolean instant_replace) {
    for (gint i = 0; i < sj->words->len; i++) {
        Word *word = &g_array_index(sj->words, Word, i);
        word->valid_search = FALSE;
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word->starting, word->word->len);
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_HIGHLIGHT, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word->starting, word->word->len);
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TEXT, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word->starting, word->word->len);
    }

    sj->search_results_count = 0;
    sj->search_word_pos = -1;

    for (gint i = 0; i < sj->words->len; i++) {
        Word *word = &g_array_index(sj->words, Word, i);

        if (instant_replace) {
            if (strcmp(word->word->str, sj->search_query->str) == 0) {
                word->valid_search = TRUE;
            }
            continue;
        }

        if (sj->config_settings->match_whole_word) {
            if (strcmp(word->word->str, sj->search_query->str) == 0) {
                word->valid_search = TRUE;
            }
            continue;
        }

        if (sj->config_settings->search_case_sensitive && sj->config_settings->search_smart_case) {
            if (sj->config_settings->search_start_from_beginning) {
                for (gchar *p = word->word->str; *p != '\0'; p++) {
                    gint matched_chars = 0;

                    for (gint z = 0; z < sj->search_query->len; z++) {
                        gchar haystack_char = word->word->str[z];
                        gchar needle_char = sj->search_query->str[z];

                        if (valid_smart_case(haystack_char, needle_char)) {
                            matched_chars += 1;
                        }
                    }

                    if (matched_chars == sj->search_query->len) {
                        word->valid_search = TRUE;
                    }
                }
            }

            if (!sj->config_settings->search_start_from_beginning) {
                for (gchar *p = word->word->str; *p != '\0'; p++) {
                    const gchar *z;
                    gint k = 0;
                    gchar haystack_char;
                    gchar needle_char;

                    do {
                        z = sj->search_query->str + k;
                        const gchar *d = p + k;
                        haystack_char = d[0];
                        needle_char = z[0];
                        k++;
                    } while (valid_smart_case(haystack_char, needle_char));

                    if (k - 1 >= 1) {
                        word->valid_search = TRUE;
                    }
                }
            }
        }

        if (sj->config_settings->search_case_sensitive && !sj->config_settings->search_smart_case) {
            if (sj->config_settings->search_start_from_beginning) {
                if (g_str_has_prefix(word->word->str, sj->search_query->str)) {
                    word->valid_search = TRUE;
                }
            }

            if (!sj->config_settings->search_start_from_beginning) {
                if (g_strstr_len(word->word->str, -1, sj->search_query->str)) {
                    word->valid_search = TRUE;
                }
            }
        }

        if (!sj->config_settings->search_case_sensitive && sj->config_settings->search_start_from_beginning) {
            if (g_ascii_strncasecmp(word->word->str, sj->search_query->str, strlen(sj->search_query->str)) == 0) {
                word->valid_search = TRUE;
            }
        }

        if (!sj->config_settings->search_case_sensitive && !sj->config_settings->search_start_from_beginning) {
            gchar *word_lower = g_ascii_strdown(word->word->str, -1);
            gchar *query_lower = g_ascii_strdown(sj->search_query->str, -1);

            if (g_strrstr(word_lower, query_lower)) {
                word->valid_search = TRUE;
            }

            g_free(word_lower);
            g_free(query_lower);
        }
    }

    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        if (word.valid_search) {
            sj->search_results_count += 1;
            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, word.starting, word.word->len);
            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TEXT, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, word.starting, word.word->len);
        }
    }

    gint search_word_pos = get_search_word_pos(sj);
    sj->search_word_pos_first = get_search_word_pos_first(sj);
    sj->search_word_pos = search_word_pos == -1 ? sj->search_word_pos_first : search_word_pos;
    if (sj->search_results_count > 0) {
        Word word = g_array_index(sj->words, Word, sj->search_word_pos);
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_HIGHLIGHT, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, word.starting, word.word->len);
    }
    sj->search_word_pos_last = get_search_word_pos_last(sj);

    ui_set_statusbar(TRUE, _("%i word%s in view."), sj->search_results_count, sj->search_results_count == 1 ? "" : "s");
}

static gboolean on_key_press_search_word(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    gunichar keychar = gdk_keyval_to_unicode(event->keyval);

    gboolean is_other_char =
        strchr("[]\\;'.,/-=_+{`_+|}:<>?\"~)(*&^%$#@!)", (gchar)gdk_keyval_to_unicode(event->keyval)) ||
        (event->keyval >= GDK_KEY_0 && event->keyval <= GDK_KEY_9);

    if (event->keyval == GDK_KEY_Return) {
        if (sj->search_word_pos != -1) {
            search_word_jump_complete(sj);
            return TRUE;
        }
    }

    if (event->keyval == GDK_KEY_BackSpace) {
        if (sj->search_query->len == 0) {
            search_word_jump_cancel(sj);
            return TRUE;
        }

        g_string_truncate(sj->search_query, sj->search_query->len - 1);

        if (sj->search_query->len > 0) {
            search_word_mark_words(sj, FALSE);
        } else {
            sj->search_results_count = 0;

            for (gint i = 0; i < sj->words->len; i++) {
                Word word = g_array_index(sj->words, Word, i);
                scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);
                scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, word.word->len);
                scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_HIGHLIGHT, 0);
                scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, word.word->len);
                scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TEXT, 0);
                scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, word.word->len);
                scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_MULTICURSOR, 0);
                scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, word.word->len);
            }
        }

        annotation_display_search(sj);
        return TRUE;
    }

    if (keychar != 0 && (g_unichar_isalpha(keychar) || is_other_char)) {
        g_string_append_c(sj->search_query, keychar);

        search_word_mark_words(sj, FALSE);
        annotation_display_search(sj);

        if (sj->search_results_count == 1 && !sj->config_settings->wait_for_enter) {
            search_word_jump_complete(sj);
        }

        return TRUE;
    }

    scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_HIGHLIGHT, 0);

    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, word.word->len);
    }

    if (event->keyval == GDK_KEY_Left && sj->search_query->len > 0) {
        if (set_search_word_pos_left_key(sj)) {
            return TRUE;
        }
    }

    if (event->keyval == GDK_KEY_Right && sj->search_query->len > 0) {
        if (set_search_word_pos_right_key(sj)) {
            return TRUE;
        }
    }

    if (mod_key_pressed(event)) {
        return TRUE;
    }

    search_word_jump_cancel(sj);
    return FALSE;
}

void search_word_get_words(ShortcutJump *sj) {
    for (gint i = 0; i < sj->last_position - sj->first_position; i++) {
        gint start = scintilla_send_message(sj->sci, SCI_WORDSTARTPOSITION, sj->first_position + i, TRUE);
        gint end = scintilla_send_message(sj->sci, SCI_WORDENDPOSITION, sj->first_position + i, TRUE);

        if (start == end || start < sj->first_position || end > sj->last_position) {
            continue;
        }

        Word data;

        data.word = g_string_new(sci_get_contents_range(sj->sci, start, end));
        data.starting_doc = start;
        data.starting = start;
        data.replace_pos = i;
        data.line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, start, 0);

        g_array_append_val(sj->words, data);
        i += data.word->len;
    }
}

void search_word_set_query(ShortcutJump *sj, gboolean instant_replace) {
    gint start = scintilla_send_message(sj->sci, SCI_WORDSTARTPOSITION, sj->current_cursor_pos, TRUE);
    gint end = scintilla_send_message(sj->sci, SCI_WORDENDPOSITION, sj->current_cursor_pos, TRUE);
    gchar *current_word = start != end ? sci_get_contents_range(sj->sci, start, end) : "";
    gboolean ps = sj->config_settings->use_selected_word_or_char && sj->in_selection && sj->selection_is_a_word;

    current_word = ps ? sci_get_contents_range(sj->sci, sj->selection_start, sj->selection_end) : current_word;

    if ((current_word && strlen(current_word) > 0) && (ps || instant_replace)) {
        g_string_append(sj->search_query, current_word);
        search_word_mark_words(sj, instant_replace);
    }
}

void search_word_init(ShortcutJump *sj, gboolean instant_replace) {
    sj->current_mode = JM_SEARCH;
    set_sj_scintilla_object(sj);
    set_selection_info(sj);
    define_indicators(sj->sci, sj);
    init_sj_values(sj);
    search_word_get_words(sj);
    search_word_set_query(sj, instant_replace);
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 1, 0);
    connect_key_press_action(sj, on_key_press_search_word);
    connect_click_action(sj, on_click_event_search);
    annotation_display_search(sj);
}

void search_word_cb(GtkMenuItem *menu_item, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    if (sj->current_mode == JM_NONE) {
        search_word_init(sj, FALSE);
    }
}

gboolean search_word_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    if (sj->current_mode == JM_NONE) {
        search_word_init(sj, FALSE);
        return TRUE;
    }
    return FALSE;
}

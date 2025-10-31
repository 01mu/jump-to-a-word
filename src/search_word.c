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
#include "search_common.h"
#include "selection.h"
#include "util.h"
#include "values.h"

/**
 * @brief Provides a filter used in word search when the setting is set to case insensitive.

  * @param const gchar *word: The word we are searching (haystack)
  * @param const gchar *search_query: The search query (needle)
  *
  * @returns gboolean: TRUE if the needle exists FALSE otherwise
 */
gboolean search_case_insensitive_match(const gchar *word, const gchar *search_query) {
    return g_ascii_strncasecmp(word, search_query, strlen(search_query)) == 0;
}

/**
 * @brief Resets indicators and info from the previous search, begins a new search, sets the highlight indicator for the
 * first result, and sets the indexes for the first and last valid search words in the words array (used for wrapping).
 *
 * @param ShortcutJump *sj: The plugin object
 * @param gboolean instant_replace: If instant replace mode is enabled
 */
void search_mark_words(ShortcutJump *sj, gboolean instant_replace) {
    for (gint i = 0; i < sj->words->len; i++) {
        Word *word = &g_array_index(sj->words, Word, i);

        word->valid_search = FALSE;

        clear_indicator_for_range(sj->sci, INDICATOR_TAG, word->starting, word->word->len);
        clear_indicator_for_range(sj->sci, INDICATOR_HIGHLIGHT, word->starting, word->word->len);
        clear_indicator_for_range(sj->sci, INDICATOR_TEXT, word->starting, word->word->len);
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

                    if (k - 1 == sj->search_query->len) {
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
            if (search_case_insensitive_match(word->word->str, sj->search_query->str)) {
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

            set_indicator_for_range(sj->sci, INDICATOR_TAG, word.starting, word.word->len);
            set_indicator_for_range(sj->sci, INDICATOR_TEXT, word.starting, word.word->len);
        }
    }

    ui_set_statusbar(TRUE, _("%i word%s in view."), sj->search_results_count, sj->search_results_count == 1 ? "" : "s");
    gint search_word_pos = get_search_word_pos(sj);

    sj->search_word_pos_first = get_search_word_pos_first(sj);
    sj->search_word_pos = search_word_pos == -1 ? sj->search_word_pos_first : search_word_pos;

    if (sj->search_results_count > 0) {
        Word word = g_array_index(sj->words, Word, sj->search_word_pos);

        set_indicator_for_range(sj->sci, INDICATOR_HIGHLIGHT, word.starting, word.word->len);
    }

    sj->search_word_pos_last = get_search_word_pos_last(sj);
}

/**
 * @brief Handles key presses for a search jump: jumps to selected word with Return, removes the last char from the
 * search query with Backspace, performs navigation of tagged words with Left and Right, and handles search replace
 * mode key key input.
 *
 * @param GtkWidget *widget: (unused)
 * @param GdkEventKey *event: Keypress event
 * @param gpointer user_data: The plugin data
 *
 * @return gint: FALSE if no controlled for key press action was found
 */
static gboolean on_key_press_search_word(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    gunichar keychar = gdk_keyval_to_unicode(event->keyval);

    gboolean is_other_char =
        strchr("[]\\;'.,/-=_+{`_+|}:<>?\"~)(*&^%$#@!)", (gchar)gdk_keyval_to_unicode(event->keyval)) ||
        (event->keyval >= GDK_KEY_0 && event->keyval <= GDK_KEY_9);

    if (event->keyval == GDK_KEY_Return) {
        if (sj->search_word_pos != -1) {
            search_complete(sj);
            return TRUE;
        }
    }

    if (event->keyval == GDK_KEY_BackSpace) {
        if (sj->search_query->len == 0) {
            search_cancel(sj);
            return TRUE;
        }

        g_string_truncate(sj->search_query, sj->search_query->len - 1);

        if (sj->search_query->len > 0) {
            search_mark_words(sj, FALSE);
        } else {
            sj->search_results_count = 0;
            search_clear_indicators(sj->sci, sj->words);
        }

        annotation_display_search(sj);
        return TRUE;
    }

    if (keychar != 0 && (g_unichar_isalpha(keychar) || is_other_char)) {
        g_string_append_c(sj->search_query, keychar);

        search_mark_words(sj, FALSE);
        annotation_display_search(sj);

        if (sj->search_results_count == 1 && !sj->config_settings->wait_for_enter) {
            search_complete(sj);
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

    search_cancel(sj);
    return FALSE;
}

/**
 * @brief Sets all the words within a range to the words array.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void search_get_words(ShortcutJump *sj) {
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

/**
 * @brief Sets the inital query used in a word search based on selection. If a word is highlighted we use it for
 * searching if use_selected_word_or_char is enabled. During an instant replace the word under the cursor is replaced.
 *
 * @param ShortcutJump *sj: The plugin object
 * @param gboolean instant_replace: Whether we are instant replacing
 */
void search_set_initial_query(ShortcutJump *sj, gboolean instant_replace) {
    gint start = scintilla_send_message(sj->sci, SCI_WORDSTARTPOSITION, sj->current_cursor_pos, TRUE);
    gint end = scintilla_send_message(sj->sci, SCI_WORDENDPOSITION, sj->current_cursor_pos, TRUE);
    gchar *current_word = start != end ? sci_get_contents_range(sj->sci, start, end) : "";
    gboolean ps = sj->config_settings->use_selected_word_or_char && sj->in_selection && sj->selection_is_a_word;

    current_word = ps ? sci_get_contents_range(sj->sci, sj->selection_start, sj->selection_end) : current_word;

    if ((current_word && strlen(current_word) > 0) && (ps || instant_replace)) {
        g_string_append(sj->search_query, current_word);
        search_mark_words(sj, instant_replace);
    }
}

/**
 * @brief Assigns every word on the screen to the words struct, sets configuration for indicators, and activates key
 * press and click signals. In the event that we are performing an instant search, we set the appropriate starting
 * and ending positions (the earliest and latest indexes of words in the array that match a query) and the initial
 * position. This process is also triggered if we are using use_current_word.
 *
 * @param ShortcutJump *sj: The plugin object
 * @param gboolean instant_replace: If instant replace mode is enabled
 */
void search_init(ShortcutJump *sj, gboolean instant_replace) {
    if (sj->current_mode != JM_NONE) {
        return;
    }

    sj->current_mode = JM_SEARCH;

    set_sj_scintilla_object(sj);

    if (!instant_replace) {
        set_selection_info(sj);
    }

    init_sj_values(sj);
    define_indicators(sj->sci, sj);

    search_get_words(sj);
    search_set_initial_query(sj, instant_replace);

    connect_key_press_action(sj, on_key_press_search_word);
    connect_click_action(sj, on_click_event_search);

    annotation_display_search(sj);
}

/**
 * @brief Provides a menu callback for performing a search jump.
 *
 * @param GtkMenuItem *menu_item: (unused)
 * @param gpointer user_data: The plugin data
 */
void search_cb(GtkMenuItem *menu_item, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (sj->current_mode == JM_NONE) {
        search_init(sj, FALSE);
    }
}

/**
 * @brief Provides a keybinding callback for performing a search jump.
 *
 * @param GtkMenuItem *kb: (unused)
 * @param guint key_id: (unused)
 * @param gpointer user_data: The plugin data
 *
 * @return gboolean: TRUE
 */
gboolean search_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (sj->current_mode == JM_NONE) {
        search_init(sj, FALSE);
        return TRUE;
    }

    return FALSE;
}

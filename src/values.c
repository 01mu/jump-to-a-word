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

ScintillaObject *get_scintilla_object() {
    GeanyDocument *doc = document_get_current();

    if (!doc->is_valid) {
        exit(1);
    }

    return doc->editor->sci;
}

static gint get_first_line_on_screen(ShortcutJump *sj) {
    gboolean z = sj->in_selection && sj->replace_instant && sj->current_mode == JM_SUBSTRING;

    if (!z && sj->in_selection && sj->config_settings->search_from_selection && !sj->selection_is_a_char) {
        if (sj->selection_is_within_a_line && sj->config_settings->search_selection_if_line) {
            gint first_line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->selection_start, 0);
            return scintilla_send_message(sj->sci, SCI_DOCLINEFROMVISIBLE, first_line, 0);
        }

        if (!sj->selection_is_a_word && !sj->selection_is_within_a_line) {
            gint first_line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->selection_start, 0);
            return scintilla_send_message(sj->sci, SCI_DOCLINEFROMVISIBLE, first_line, 0);
        }
    }

    gint first_visible_line = scintilla_send_message(sj->sci, SCI_GETFIRSTVISIBLELINE, 0, 0);
    gint doc_line = scintilla_send_message(sj->sci, SCI_DOCLINEFROMVISIBLE, first_visible_line, 0);

    return doc_line;
}

static gint get_number_of_lines_on_screen(ShortcutJump *sj) {
    gboolean z = sj->in_selection && sj->replace_instant && sj->current_mode == JM_SUBSTRING;

    if (!z && sj->in_selection && sj->config_settings->search_from_selection && !sj->selection_is_a_char) {
        if (sj->selection_is_within_a_line && sj->config_settings->search_selection_if_line) {
            gint first_line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->selection_start, 0);
            gint last_line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->selection_end, 0);
            return last_line - first_line;
        }

        if (!sj->selection_is_a_word && !sj->selection_is_within_a_line) {
            gint first_line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->selection_start, 0);
            gint last_line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->selection_end, 0);
            return last_line - first_line;
        }
    }

    return scintilla_send_message(sj->sci, SCI_LINESONSCREEN, 0, 0);
}

static gint get_first_position(ShortcutJump *sj, gint first_line_on_screen) {
    gboolean z = sj->in_selection && sj->replace_instant && sj->current_mode == JM_SUBSTRING;

    if (!z && sj->in_selection && sj->config_settings->search_from_selection && !sj->selection_is_a_char) {
        if (sj->selection_is_within_a_line && sj->config_settings->search_selection_if_line) {
            return sj->selection_start;
        }

        if (!sj->selection_is_a_word && !sj->selection_is_within_a_line) {
            return sj->selection_start;
        }
    }

    if (sj->config_settings->only_tag_current_line && sj->current_mode != JM_LINE) {
        gint pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
        gint current_line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, pos, 0);

        return scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, current_line, 0);
    }

    return scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, first_line_on_screen, 0);
}

static gint get_last_position(ShortcutJump *sj, gint last_line_on_screen) {
    gboolean z = sj->in_selection && sj->replace_instant && sj->current_mode == JM_SUBSTRING;

    if (!z && sj->in_selection && sj->config_settings->search_from_selection && !sj->selection_is_a_char) {
        if (sj->selection_is_within_a_line && sj->config_settings->search_selection_if_line) {
            return sj->selection_end;
        }

        if (!sj->selection_is_a_word && !sj->selection_is_within_a_line) {
            return sj->selection_end;
        }
    }

    if (sj->config_settings->only_tag_current_line && sj->current_mode != JM_LINE) {
        gint pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
        gint current_line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, pos, 0);

        return scintilla_send_message(sj->sci, SCI_GETLINEENDPOSITION, current_line, 0);
    }

    gint p = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, last_line_on_screen, 0);

    while (p == -1) {
        p = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, --last_line_on_screen, 0);
    }

    return p;
}

static gint get_cursor_position(ScintillaObject *sci, gint first_position, gint last_position) {
    gint current_cursor_pos = scintilla_send_message(sci, SCI_GETCURRENTPOS, 0, 0);

    if (current_cursor_pos < first_position || current_cursor_pos > last_position) {
        current_cursor_pos = first_position + floor(((gfloat)last_position - (gfloat)first_position) / 2);
    }

    return current_cursor_pos;
}

static GArray *markers_margin_get(ShortcutJump *sj, gint first_line_on_screen, gint lines_on_screen) {
    GArray *markers = g_array_new(FALSE, FALSE, sizeof(gint));

    if (sj->current_mode == JM_SHORTCUT_WORD || sj->current_mode == JM_SHORTCUT_CHAR_JUMPING) {
        if (sj->range_is_set) {
            gint line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->range_first_pos, 0);

            scintilla_send_message(sj->sci, SCI_MARKERDELETE, line, 0);
        }
    } else {
        if (sj->range_is_set) {
            scintilla_send_message(sj->sci, SCI_MARKERDELETE, sj->range_first_pos, 0);
        }
    }

    for (gint i = 0; i < lines_on_screen; i++) {
        scintilla_send_message(sj->sci, SCI_MARKERDELETE, i + first_line_on_screen, SC_MARK_SHORTARROW);

        gint marker = scintilla_send_message(sj->sci, SCI_MARKERGET, i + first_line_on_screen, 0);

        g_array_append_val(markers, marker);
    }

    return markers;
}

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

static gint get_wrapped_lines(ScintillaObject *sci, gint first_line_on_screen) {
    gint wrapped_lines = 0;

    for (gint i = 0; i < first_line_on_screen; i++) {
        wrapped_lines += scintilla_send_message(sci, SCI_WRAPCOUNT, i, 0) - 1;
    }

    return wrapped_lines;
}

void get_view_positions(ShortcutJump *sj) {
    gint first_line_on_screen = get_first_line_on_screen(sj);
    gint lines_on_screen = get_number_of_lines_on_screen(sj);
    gint last_line_on_screen = first_line_on_screen + lines_on_screen;
    gint first_position = get_first_position(sj, first_line_on_screen);
    gint last_position = get_last_position(sj, last_line_on_screen);
    gint current_cursor_pos = get_cursor_position(sj->sci, first_position, last_position);
    gint wrapped_lines = get_wrapped_lines(sj->sci, first_line_on_screen);

    sj->wrapped_lines = wrapped_lines;
    sj->first_line_on_screen = first_line_on_screen;
    sj->lines_on_screen = lines_on_screen;
    sj->last_line_on_screen = last_line_on_screen;
    sj->first_position = first_position;
    sj->last_position = last_position;
    sj->current_cursor_pos = current_cursor_pos;
}

static void set_common_vals(ShortcutJump *sj) {
    sj->eol_message_line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->current_cursor_pos, 0);
    sj->shortcut_single_pos = 0;
    sj->search_results_count = 0;
    sj->search_word_pos = -1;
    sj->search_word_pos_first = -1;
    sj->search_word_pos_last = -1;
    sj->search_change_made = FALSE;
    sj->cursor_in_word = FALSE;
    sj->replace_len = 0;
    sj->replace_instant = FALSE;
    sj->inserting_clipboard = FALSE;
}

void free_sj_values(ShortcutJump *sj) {
    g_string_free(sj->cache, TRUE);
    g_string_free(sj->buffer, TRUE);
    g_string_free(sj->replace_cache, TRUE);

    g_string_free(sj->eol_message, TRUE);
    g_string_free(sj->search_query, TRUE);

    g_free(sj->clipboard_text);
    g_string_free(sj->replace_query, TRUE);

    g_array_free(sj->lf_positions, TRUE);
    g_array_free(sj->words, TRUE);
    g_array_free(sj->markers, TRUE);

    set_common_vals(sj);
}

void init_sj_values(ShortcutJump *sj) {
    get_view_positions(sj);

    gchar *screen_lines;

    if (sj->first_position < sj->last_position) {
        screen_lines = sci_get_contents_range(sj->sci, sj->first_position, sj->last_position);
    } else {
        screen_lines = g_strdup("");
    }

    sj->cache = g_string_new(screen_lines);
    sj->buffer = g_string_new(screen_lines);
    sj->replace_cache = g_string_new(screen_lines);

    g_free(screen_lines);

    sj->eol_message = g_string_new("");
    sj->search_query = g_string_new("");

    sj->clipboard_text = g_strdup("");
    sj->replace_query = g_string_new("");

    sj->lf_positions = g_array_new(FALSE, FALSE, sizeof(gint));
    sj->words = g_array_new(FALSE, FALSE, sizeof(Word));
    sj->markers = markers_margin_get(sj, sj->first_line_on_screen, sj->lines_on_screen);

    set_common_vals(sj);

    gint chars_in_doc = scintilla_send_message(sj->sci, SCI_GETLENGTH, 0, 0);
    gchar last_char = scintilla_send_message(sj->sci, SCI_GETCHARAT, sj->last_position - 1, 0);

    if (chars_in_doc == sj->last_position && last_char != '\n') {
        g_string_append_c(sj->buffer, '\n');
    }

    // scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);
}

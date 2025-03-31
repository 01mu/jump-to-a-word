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

#ifndef SHORTCUT_COMMON_H_
#define SHORTCUT_COMMON_H_

#include <geanyplugin.h>

#include "jump_to_a_word.h"

void shortcut_end(ShortcutJump *sj, gboolean was_canceled);
void shortcut_char_waiting_cancel(ShortcutJump *sj);
void shortcut_char_get_chars(ShortcutJump *sj, gchar search_char);
void shortcut_cancel(ShortcutJump *sj);
void set_after_shortcut_placement(ShortcutJump *sj);
GString *shortcut_set_tags_in_buffer(GArray *words, GString *buffer, gint first_position);
GString *shortcut_mask_bytes(GArray *words, GString *buffer, gint first_position);
GString *shortcut_make_tag(ShortcutJump *sj, gint position);
gint shortcut_utf8_char_length(gchar c);
gint shortcut_set_word_padding(ShortcutJump *sj, gint word_length);
gint get_max_words(ShortcutJump *sj);
gboolean on_key_press_shortcut(GtkWidget *widget, GdkEventKey *event, gpointer user_data);
gboolean on_key_press_shortcut_char(GtkWidget *widget, GdkEventKey *event, gpointer user_data);
gboolean on_click_event_shortcut(GtkWidget *widget, GdkEventButton *event, gpointer user_data);

#endif

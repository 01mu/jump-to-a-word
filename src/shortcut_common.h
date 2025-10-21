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

void shrtct_end(ShortcutJump *sj, gboolean was_canceled);
void shrtct_cancel(ShortcutJump *sj);
void shrtct_set_to_first_visible_line(ShortcutJump *sj);
void shrtct_set_after_placement(ShortcutJump *sj);
GString *shrtct_set_tags_in_buffer(GArray *words, GString *buffer, gint first_position);
GString *shrtct_mask_bytes(GArray *words, GString *buffer, gint first_position);
GString *shrtct_make_tag(ShortcutJump *sj, gint position);
gint shrtct_utf8_char_length(gchar c);
gint shrtct_set_padding(ShortcutJump *sj, gint word_length);
gint shrtct_get_max_words(ShortcutJump *sj);
gboolean shrtct_on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data);
gboolean shrtct_char_on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data);
gboolean shrtct_on_click_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
gint shrtct_on_key_press_action(GdkEventKey *event, gpointer user_data);

#endif

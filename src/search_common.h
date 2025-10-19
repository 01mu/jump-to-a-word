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

#ifndef SEARCH_COMMON_H_
#define SEARCH_COMMON_H_

#include <geanyplugin.h>

#include "jump_to_a_word.h"

void search_replace_complete(ShortcutJump *sj);
void search_replace_cancel(ShortcutJump *sj);
void search_end(ShortcutJump *sj);
void search_complete(ShortcutJump *sj);
void search_cancel(ShortcutJump *sj);
gint get_search_word_pos(ShortcutJump *sj);
gint get_search_word_pos_last(ShortcutJump *sj);
gint get_search_word_pos_first(ShortcutJump *sj);
gboolean valid_smart_case(char haystack_char, char needle_char);
gboolean set_search_word_pos_right_key(ShortcutJump *sj);
gboolean set_search_word_pos_left_key(ShortcutJump *sj);
gboolean on_click_event_search(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
gboolean on_key_press_search_replace(GtkWidget *widget, GdkEventKey *event, gpointer user_data);

#endif

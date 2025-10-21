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

#ifndef SHORTCUT_CHAR_H_
#define SHORTCUT_CHAR_H_

#include <geanyplugin.h>

#include "jump_to_a_word.h"

void shrtct_char_get_chars(ShortcutJump *sj, gchar search_char);
void shrtct_char_waiting_cancel(ShortcutJump *sj);
void shrtct_char_replace_complete(ShortcutJump *sj);
void shrtct_char_replace_cancel(ShortcutJump *sj);
void shrtct_char_init(ShortcutJump *sj, gboolean init_set, gchar init);
void shrtct_char_cb(GtkMenuItem *menu_item, gpointer user_data);
gboolean shrtct_char_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data);
gboolean shrtct_char_on_click_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data);

#endif

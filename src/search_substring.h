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

#ifndef SEARCH_SUBSTRING_H_
#define SEARCH_SUBSTRING_H_

#include <geanyplugin.h>

#include "jump_to_a_word.h"

void substring_init(ShortcutJump *sj, gboolean instant_replace);
void substring_cb(GtkMenuItem *menu_item, gpointer user_data);
gboolean substring_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data);

#endif

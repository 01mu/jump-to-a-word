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

#ifndef REPLACE_INSTANT_H
#define REPLACE_INSTANT_H

#include <geanyplugin.h>

void replace_search_cb(GtkMenuItem *menu_item, gpointer user_data);
gboolean replace_search_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data);

#endif

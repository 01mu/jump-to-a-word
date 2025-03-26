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

#ifndef LINE_OPTIONS_H
#define LINE_OPTIONS_H

#include <geanyplugin.h>

void open_text_options_cb(GtkMenuItem *menuitem, gpointer user_data);
void open_line_options_cb(GtkMenuItem *menuitem, gpointer user_data);
gboolean open_text_options_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data);
gboolean open_line_options_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data);

#endif

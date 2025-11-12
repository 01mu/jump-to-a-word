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

#ifndef UTIL_H_
#define UTIL_H_

#include <geanyplugin.h>

#include "jump_to_a_word.h"

void connect_key_press_action(ShortcutJump *sj, KeyPressCallback function);
void set_indicator_for_range(ScintillaObject *sci, Indicator type, gint starting, gint length);
void connect_click_action(ShortcutJump *sj, ClickCallback function);
void search_clear_indicators(ScintillaObject *sci, GArray *words);
void margin_markers_reset(ShortcutJump *sj);
void define_indicators(ScintillaObject *sci, ShortcutJump *sj);
void clear_indicator_for_range(ScintillaObject *sci, Indicator type, gint starting, gint length);
void disconnect_key_press_action(ShortcutJump *sj);
void disconnect_click_action(ShortcutJump *sj);
gint set_cursor_position_with_lfs(ShortcutJump *sj);
gint get_lfs(ShortcutJump *sj, gint current_line);
gint get_indent_width();
gboolean mouse_movement_performed(ShortcutJump *sj, GdkEventButton *event);
gboolean mod_key_pressed(GdkEventKey *event);
void reset_indicators(ShortcutJump *sj);
gboolean handle_text_after_action(ShortcutJump *sj, gint pos, gint word_length, gint line);
void end_actions(ShortcutJump *sj);

#endif

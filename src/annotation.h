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

#ifndef ANNOTATION_H
#define ANNOTATION_H

#include "jump_to_a_word.h"

void annotation_clear(ScintillaObject *sci, gint eol_message_line);
void annotation_show(ShortcutJump *sj);
void annotation_display_search(ShortcutJump *sj);
void annotation_display_substring(ShortcutJump *sj);
void annotation_display_char_search(ShortcutJump *sj);
void annotation_display_replace(ShortcutJump *sj);
void annotation_display_inserting_line_from_search(ShortcutJump *sj);
void annotation_display_replace_substring(ShortcutJump *sj);
void annotation_display_accepting_multicursor(ShortcutJump *sj);
void annotation_display_inserting_line_multicursor(ShortcutJump *sj);
void annotation_display_replace_multicursor(ShortcutJump *sj);
void annotation_display_replace_char(ShortcutJump *sj);
void annotation_display_shortcut_char(ShortcutJump *sj);

#endif

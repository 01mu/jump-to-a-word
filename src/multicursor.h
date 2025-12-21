/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#ifndef MULTICURSOR_H_
#define MULTICURSOR_H_

#include "jump_to_a_word.h"
#include <geanyplugin.h>

void multicursor_accepting_cancel(ShortcutJump *sj);
void multicursor_cb(GtkMenuItem *menu_item, gpointer user_data);
gboolean multicursor_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data);
void multicursor_end(ShortcutJump *sj);
void multicursor_add_word(ShortcutJump *sj, Word word);
void multicursor_add_word_from_selection(ShortcutJump *sj, gint start, gint end);
void multicursor_replace_cancel(ShortcutJump *sj);
void multicursor_replace_complete(ShortcutJump *sj);
gboolean on_click_event_multicursor_replace(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
void multicursor_replace_clear_indicators(ShortcutJump *sj);
void multicursor_toggle(ShortcutJump *sj);

#endif

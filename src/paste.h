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

#ifndef PASTE_H_
#define PASTE_H_

#include "jump_to_a_word.h"
#include <geanyplugin.h>

void paste_get_clipboard_text(ShortcutJump *sj);
gboolean on_paste_key_release_replace(GtkWidget *widget, GdkEventKey *event, gpointer user_data);
gboolean on_paste_key_release_word_search(GtkWidget *widget, GdkEventKey *event, gpointer user_data);
gboolean on_paste_key_release_substring_search(GtkWidget *widget, GdkEventKey *event, gpointer user_data);

#endif

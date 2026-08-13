#ifndef UTIL_H_
#define UTIL_H_

#include <geanyplugin.h>

#include "jump_to_a_word.h"

void connect_key_press_action(ShortcutJump *sj, KeyPressCallback function);
void connect_click_action(ShortcutJump *sj, ClickCallback function);
void define_indicators(ScintillaObject *sci, gint tag_color, gint highlight_color, gint text_color);
void disconnect_key_press_action(ShortcutJump *sj);
void disconnect_click_action(ShortcutJump *sj);
gint set_cursor_position_with_lfs(ShortcutJump *sj);
gint get_lfs(ShortcutJump *sj, gint current_line);
gint get_indent_width();
gboolean mouse_movement_performed(ShortcutJump *sj, GdkEventButton *event);
gboolean mod_key_pressed(GdkEventKey *event);
void cancel_actions(ShortcutJump *sj);
void end_actions(ShortcutJump *sj);
gint sort_words_by_starting_doc(gconstpointer a, gconstpointer b);
void multicursor_menu_toggled(GtkMenuItem *menuitem, gpointer data);
void whole_document_menu_toggled(GtkMenuItem *menuitem, gpointer data);
void toggle_multicursor_menu(ShortcutJump *sj, gboolean type);
void attempt_line_end_for_char(ShortcutJump *sj);
void move_to_end_of_line(ShortcutJump *sj);
void get_strings_for_instant_action(ShortcutJump *sj);
gboolean whole_document_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data);

#endif

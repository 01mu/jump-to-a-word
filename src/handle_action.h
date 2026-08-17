#ifndef HANDLE_ACTION_H_
#define HANDLE_ACTION_H_

#include <geanyplugin.h>

void cb_action_replace(GtkMenuItem *menu_item, gpointer user_data);
void cb_action_insert_start(GtkMenuItem *menu_item, gpointer user_data);
void cb_action_insert_end(GtkMenuItem *menu_item, gpointer user_data);
void cb_action_insert_previous_line(GtkMenuItem *menu_item, gpointer user_data);
void cb_action_insert_next_line(GtkMenuItem *menu_item, gpointer user_data);
void cb_action_transpose(GtkMenuItem *menu_item, gpointer user_data);
void cb_action_duplicate(GtkMenuItem *menu_item, gpointer user_data);
gboolean action_replace_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data);
gboolean action_insert_start_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data);
gboolean action_insert_end_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data);
gboolean action_insert_previous_line_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data);
gboolean action_insert_next_line_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data);
gboolean action_transpose_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data);
gboolean action_duplicate_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data);
void replace_search_cb(GtkMenuItem *menu_item, gpointer user_data);
gboolean replace_search_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data);

#endif

#ifndef REPEAT_ACTION_H_
#define REPEAT_ACTION_H_

#include <geanyplugin.h>

gboolean repeat_action_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data);
void repeat_action_cb(GtkMenuItem *menu_item, gpointer user_data);

#endif

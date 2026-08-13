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

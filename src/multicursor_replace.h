#ifndef MULTICURSOR_REPLACE_H_
#define MULTICURSOR_REPLACE_H_

#include "jump_to_a_word.h"

void multicursor_replace_start(ShortcutJump *sj);
void multicursor_replace_cancel(ShortcutJump *sj);
void multicursor_replace_complete(ShortcutJump *sj);
gboolean on_click_event_multicursor_replace(GtkWidget *widget, GdkEventButton *event, gpointer user_data);

#endif

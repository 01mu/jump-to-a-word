#ifndef VALUES_H_
#define VALUES_H_

#include "jump_to_a_word.h"

void init_sj_values(ShortcutJump *sj);
ScintillaObject *get_scintilla_object();
void margin_markers_reset(ShortcutJump *sj);
void get_view_positions(ShortcutJump *sj);
void free_sj_values(ShortcutJump *sj);

#endif

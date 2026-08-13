#ifndef DUPLICATE_H_
#define DUPLICATE_H_

#include "jump_to_a_word.h"

void multicursor_duplicate_cancel(ShortcutJump *sj);
void duplicate_string(ShortcutJump *sj);
void duplicate_string_for_multicursor(ShortcutJump *sj);
void duplicate_end(ShortcutJump *sj);
void duplicate_cancel(ShortcutJump *sj);

#endif

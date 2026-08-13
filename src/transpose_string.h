#ifndef TRANSPOSE_STRING_H_
#define TRANSPOSE_STRING_H_

#include "jump_to_a_word.h"

void transpose_string(ShortcutJump *sj, gboolean is_instant_transpose);
void multicursor_transpose_cancel(ShortcutJump *sj);
void multicursor_transpose_complete(ShortcutJump *sj);
void transpose_string_attempt(ShortcutJump *sj);

#endif

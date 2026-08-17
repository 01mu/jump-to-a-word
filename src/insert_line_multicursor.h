#ifndef INSERT_LINE_MULTICURSOR_H_
#define INSERT_LINE_MULTICURSOR_H_

#include "jump_to_a_word.h"

void multicursor_line_insert_cancel(ShortcutJump *sj);
void multicursor_line_insert_complete(ShortcutJump *sj);
void multicursor_line_insert_end(ShortcutJump *sj);
void line_insert_from_multicursor_init(ShortcutJump *sj);

#endif

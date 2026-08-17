#ifndef INSERT_LINE_COMMON_H_
#define INSERT_LINE_COMMON_H_

#include "jump_to_a_word.h"

void line_insert_done_common(ShortcutJump *sj);
void line_insert_common(ShortcutJump *sj, GArray *unique_lines, GArray *dummy_lines, GArray *anchors);

#endif

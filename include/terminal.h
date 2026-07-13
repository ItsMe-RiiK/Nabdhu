#pragma once

namespace terminal {

void init();
void restore();
int get_width();
int get_height();
bool is_resized();
void clear_resized_flag();

} // namespace terminal

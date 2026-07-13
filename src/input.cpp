#include "input.h"
#include "terminal.h"

#include <unistd.h>
#include <poll.h>
#include <iostream>
#include <vector>

namespace input {

bool has_event() {
    if (terminal::is_resized()) return true;
    struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
    return poll(&pfd, 1, 0) > 0;
}

bool wait_for_event(int timeout_ms) {
    if (terminal::is_resized()) return true;
    struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
    return poll(&pfd, 1, timeout_ms) > 0;
}

Event read_event() {
    Event ev = { KeyCode::Unknown, 0, 0, 0 };

    if (terminal::is_resized()) {
        terminal::clear_resized_flag();
        ev.key = KeyCode::Resize;
        return ev;
    }

    char c;
    if (read(STDIN_FILENO, &c, 1) != 1) {
        return ev;
    }

    if (c == '\x1b') {
        struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
        if (poll(&pfd, 1, 0) == 0) {
            ev.key = KeyCode::Escape;
            return ev;
        }

        char seq[32];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return ev;

        if (seq[0] == '[') {
            if (poll(&pfd, 1, 0) == 0) return ev;
            if (read(STDIN_FILENO, &seq[1], 1) != 1) return ev;

            if (seq[1] == 'A') { ev.key = KeyCode::Up; return ev; }
            if (seq[1] == 'B') { ev.key = KeyCode::Down; return ev; }
            if (seq[1] == 'C') { ev.key = KeyCode::Right; return ev; }
            if (seq[1] == 'D') { ev.key = KeyCode::Left; return ev; }
            
            if (seq[1] == '<') {
                // SGR Mouse Event: \x1b[<button;x;yM or \x1b[<button;x;ym
                std::string sgr;
                while (true) {
                    if (poll(&pfd, 1, 0) == 0) break;
                    char mc;
                    if (read(STDIN_FILENO, &mc, 1) != 1) break;
                    sgr += mc;
                    if (mc == 'M' || mc == 'm') break;
                }
                
                if (sgr.back() == 'M') { // Pressed
                    int btn = 0, x = 0, y = 0;
                    sscanf(sgr.c_str(), "%d;%d;%d", &btn, &x, &y);
                    if (btn == 0) ev.key = KeyCode::MouseClickLeft;
                    else if (btn == 2) ev.key = KeyCode::MouseClickRight;
                    else if (btn == 64) ev.key = KeyCode::MouseScrollUp;
                    else if (btn == 65) ev.key = KeyCode::MouseScrollDown;
                    ev.mouse_x = x;
                    ev.mouse_y = y;
                }
                return ev;
            }
        }
        return ev;
    }
    
    if (c == '\r' || c == '\n') { ev.key = KeyCode::Enter; return ev; }
    if (c == '\t') { ev.key = KeyCode::Tab; return ev; }
    if (c == 127 || c == '\b') { ev.key = KeyCode::Backspace; return ev; }
    
    ev.key = KeyCode::Char;
    ev.ch = c;
    return ev;
}

} // namespace input

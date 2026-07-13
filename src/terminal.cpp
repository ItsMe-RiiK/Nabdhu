#include "terminal.h"

#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <iostream>

namespace terminal {

namespace {
    struct termios original_termios;
    bool resized = false;
    int current_width = 0;
    int current_height = 0;

    void sigwinch_handler(int) {
        resized = true;
    }

    void update_size() {
        struct winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        current_width = w.ws_col;
        current_height = w.ws_row;
    }
}

void init() {
    tcgetattr(STDIN_FILENO, &original_termios);
    struct termios raw = original_termios;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    // Enter alternate screen, hide cursor, enable mouse tracking (SGR 1006)
    std::cout << "\x1b[?1049h\x1b[?25l\x1b[?1002h\x1b[?1006h\x1b[?1015h" << std::flush;

    struct sigaction sa;
    sa.sa_handler = sigwinch_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGWINCH, &sa, NULL);

    update_size();
}

void restore() {
    // Exit alternate screen, show cursor, disable mouse tracking
    std::cout << "\x1b[?1049l\x1b[?25h\x1b[?1002l\x1b[?1006l\x1b[?1015l" << std::flush;
    tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
}

int get_width() {
    if (resized) update_size();
    return current_width;
}

int get_height() {
    if (resized) update_size();
    return current_height;
}

bool is_resized() {
    return resized;
}

void clear_resized_flag() {
    resized = false;
    update_size();
}

} // namespace terminal

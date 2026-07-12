#!/bin/bash
# Wrapper script to run TaskManager in terminal

# 1. Single Instance GUI Enforcer
if command -v xdotool >/dev/null 2>&1; then
    EXISTING_WINS=$(xdotool search --name "Nabdhu" 2>/dev/null)
    WIN_COUNT=$(echo "$EXISTING_WINS" | grep -v '^$' | wc -l)
    if [ "$WIN_COUNT" -gt 1 ]; then
        # Find the oldest window and activate it
        OLD_WIN=$(echo "$EXISTING_WINS" | head -n1)
        xdotool windowactivate "$OLD_WIN" 2>/dev/null
        exit 0
    fi
fi

if [ "$EUID" -ne 0 ]; then
    if command -v xdotool >/dev/null 2>&1 && command -v xprop >/dev/null 2>&1; then
        # Wait a tiny bit for the window to actually map
        sleep 0.1
        WIN_IDS=$(xdotool search --name "Nabdhu" 2>/dev/null)
        for win in $WIN_IDS; do
            xprop -id "$win" -f WM_CLASS 8s -set WM_CLASS "Nabdhu,Nabdhu"
            xprop -id "$win" -remove _NET_WM_ICON 2>/dev/null
            
            # 2. Window Centering Logic
            SCREEN_W=$(xdotool getdisplaygeometry | awk '{print $1}')
            SCREEN_H=$(xdotool getdisplaygeometry | awk '{print $2}')
            WIN_GEO=$(xdotool getwindowgeometry "$win" | grep Geometry | awk '{print $2}')
            WIN_W=$(echo "$WIN_GEO" | cut -d'x' -f1)
            WIN_H=$(echo "$WIN_GEO" | cut -d'x' -f2)
            
            if [ -n "$SCREEN_W" ] && [ -n "$WIN_W" ]; then
                NEW_X=$(( (SCREEN_W - WIN_W) / 2 ))
                NEW_Y=$(( (SCREEN_H - WIN_H) / 2 ))
                xdotool windowmove "$win" "$NEW_X" "$NEW_Y" 2>/dev/null
            fi
        done
        # Fallback to active window if search fails
        ACTIVE_WIN=$(xdotool getactivewindow 2>/dev/null)
        if [ -n "$ACTIVE_WIN" ] && [ -z "$WIN_IDS" ]; then
            xprop -id "$ACTIVE_WIN" -f WM_CLASS 8s -set WM_CLASS "Nabdhu,Nabdhu"
            xprop -id "$ACTIVE_WIN" -remove _NET_WM_ICON 2>/dev/null
        fi
    fi
    echo 1234 | sudo -S -p "" sh -c 'exec /usr/local/bin/nabdhu "$@" < /dev/tty'
else
    exec /usr/local/bin/nabdhu "$@"
fi

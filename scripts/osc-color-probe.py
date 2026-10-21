#!/usr/bin/env python3
"""Print actual OSC 10/11 replies from the controlling terminal (two-second limit)."""
import os
import re
import select
import termios
import time
import tty

fd = os.open('/dev/tty', os.O_RDWR)
saved = termios.tcgetattr(fd)
replies = {}
raw = b''
try:
    tty.setraw(fd)
    start = time.monotonic()
    os.write(fd, b'\x1b]10;?\x1b\\\x1b]11;?\x1b\\')
    while len(replies) < 2:
        remaining = start + 2 - time.monotonic()
        if remaining <= 0 or not select.select([fd], [], [], remaining)[0]:
            break
        raw += os.read(fd, 4096)
        for match in re.finditer(rb'\x1b\](10|11);(rgb:[0-9a-fA-F/]+)(?:\x1b\\|\x07)', raw):
            replies.setdefault(int(match[1]), (match[2].decode(), (time.monotonic() - start) * 1000))
finally:
    termios.tcsetattr(fd, termios.TCSANOW, saved)
    os.close(fd)

for slot, name in ((10, 'foreground'), (11, 'background')):
    if slot in replies:
        color, elapsed = replies[slot]
        print(f'OSC {slot} {name}: {color} ({elapsed:.1f} ms)')
    else:
        print(f'OSC {slot} {name}: no response')
print(f'Raw replies: {raw!r}')
raise SystemExit(0 if len(replies) == 2 else 1)

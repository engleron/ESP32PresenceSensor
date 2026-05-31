// Placeholder sketch file.
// Real setup()/loop() are implemented in PresenceRuntime.cpp.

#include <Arduino.h>

// Enlarge the Arduino loop task stack (default 8 KB). homeSpan.poll() runs on
// this task and performs HAP TLS handshakes on every controller (re)connect —
// stack-hungry, and the device roams across multiple WiFi APs, so handshakes
// happen often. A too-small loop stack can overflow into neighbouring memory
// during a handshake and corrupt the heap, surfacing later as an idle-task
// stack-canary panic. 16 KB gives mbedTLS comfortable headroom.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

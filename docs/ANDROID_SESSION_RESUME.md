# Live session resume and remaining save-state work

Phone0.14.13 fixes the launcher killing its existing :game process on Resume.
Only a live process is offered as RESUME; the previous .session_running marker
is not evidence of a restorable snapshot. The automatic Enter presses through
startup were removed. Restarting ends the live session only after confirmation.

GameActivity is singleTask. Resume brings that activity forward without launching
a second runtime. A process-lifetime GuestBootWorker also keeps its thread context
alive if the activity is destroyed and recreated. A foreground special-use service
with a notification reduces the chance of Android reclaiming the paused process;
it is not a guarantee. Notification End session deliberately ends the process and
unsaved state. App pause retains the existing native freeze/audio/input handling;
return opens the game's pause menu through the existing implementation.

On the Samsung phone, Home→explicit launcher→RESUME kept PID31233, continued
presenting frames, and logged exactly one lifted CRT entry. After intentionally
ending that test process, the launcher refused a stale Resume and explained that
the previous live session ended. A two-second foreground-only refresh updates the
button if the process disappears while the launcher is visible. Real mid-race
position/progress preservation still needs a user test. No career files were
restored or replaced in this verification.

## This is not a durable snapshot

The user's requested disk save state remains unimplemented. A reboot, force-stop,
OS process reclamation or APK update loses the live session. Ordinary career saves
and backup ZIPs do not include exact race/car position, timers or unsaved progress.

Persisting raw guest memory alone cannot restore this native runtime. Guest calls
execute through native C continuations on pthread stacks (guest_threads.inc), while
the bridge owns Vulkan/D3D resources, audio streams, file handles, timers, sockets
and synchronization objects outside guest memory. Addresses and handles cannot be
reused blindly in another process. Correct durable states need explicit guest
continuation checkpoints plus versioned serialization/reconstruction of these
host resources, coordinated suspension of workers, validation against the exact
build/game data, and atomic state-file publication. Network sessions need a defined
policy. That is runtime work, not a launcher flag or career auto-load.

Regression tests/guest_boot_worker.cpp checks process-lifetime ownership and
idempotent starts using a fake runtime; existing source regressions still apply.

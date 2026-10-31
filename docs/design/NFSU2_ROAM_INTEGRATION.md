# Native free roam with other players

Status: design, not implemented. This is the path for nfsu2.xyz free roam in
the native Android app, reusing the browser/Mac free-roam code instead of
rewriting it.

## What already exists

The browser port's free roam is not one file: it is the upstream **portable**
free-roam implementation, also used by the Mac native port and the Windows
ASI companion:

| File | Lines | Role |
|---|---:|---|
| `runtime/roam_game.c` | 7,206 | game-side hooks: local car state, remote car slots, poses, names, horn/audio, outruns, races, quick chat |
| `runtime/roam_core.c` / `.h` | 1,679 / 646 | portable session: `N2FR` wire format, HELLO/PEERS/SNAP/CFG, clock sync, interpolation, timeouts |
| `runtime/roam_plat.h` | 150 | transport API: `roam_plat_start/stop`, `io.send`, `io.broadcast`, stats, options |
| `runtime/roam_bsd.c` | ~380 | macOS/BSD UDP transport on port 47210 |
| `runtime/roam_host.h` | 150 | host API for a native recomp: guest memory (`MR32`), guest calls (`rh_call`), clock, options, HUD/audio hooks |
| `runtime/horn.c`, `carsfx.c`, `names.h`, `drift.c`, `language.c` | various | game-side helpers used by `roam_game.c` |

The Windows ASI plugin (`win/roam_asi`) builds the same `roam_game.c` against
the original x86 game; the Mac native port builds it against a recomp. Both
use a small host header. **The Android port is the same kind of host as the
Mac port**, so the work is a host layer, a per-frame hook, and a transport, not
a rewrite of the protocol or the car logic.

The game is the same v1.2 NA `SPEED2.EXE` the addresses in `roam_game.c` name
(`G_FLOW`, `G_WORLD`, `G_PLAYER1`, `FN_CAR_SETRIDE`, etc.). Verify the
executable SHA on the device before relying on every address.

## Architecture

```
 guest game (lifted x86 -> ARM)
        |
        |  per-frame hook (Present first, 0x581475 later)
        v
 roam_game_frame(CPU*)  --- guest memory --->  game state, CreateCar, SetRideInfo
        |
        |  rc_session (roam_core)
        v
 roam_plat (transport)  <--- UDP 47210 real LAN
        |               <--- nfsu2.xyz kind-1 frames via Java /lan bridge
        v
 other players (native Android, Mac native, browser)
```

### 1. Host layer (`roam_host_android.h` / `.cpp`)

Implement the `roam_host.h` names for the Android runtime:

- `g_mem` -> `memory->base()` (the guest arena).
- `MR8/MR16/MR32/MRF32` -> lvalue macros over `g_mem`.
- `rh_committed(a)` -> `memory->readable(a,n) && memory->writable(a,n)`.
- `rh_call(c, fn, ecx, edx, n, args)` -> a new `callGuestEx` that sets
  `g_ecx/g_edx`, pushes the arguments, calls the lifted function, restores
  state, and returns `g_eax`. Only from the game thread.
- `rlog` -> `NFS_RUNTIME_LOG`.
- `rh_opt` -> `getenv` plus `debug.nfs.*` system properties.
- `rh_frame`, `rh_ns`, `rh_heap_mb` -> runtime counters/clock (heap can be 0).
- `rh_hud_marker`, `horn_*`, `carsfx_*`, `language_no_profile`,
  `drift_kept_tuning` -> stubs first; port the real helpers after remote cars
  are visible.
- `rh_file`/pose log -> optional; off in production.

`roam_game.c` only uses a `CPU*` as a handle for `rh_call`; the Android host
can define an empty `CPU` struct and ignore the pointer.

### 2. Per-frame hook

- **PoC:** call `roam_game_frame()` from `d3dDispatch` at Present
  (`0x7e002110`), on the guest thread, after the game has updated the world.
  Remote car poses then appear one frame later; acceptable.
- **Final:** add a `recomp_hook` site at `0x00581475` (the browser/Mac hook,
  between World::DoTimestep and the camera/render) through `config/hooks.txt`
  and the lift/build pipeline. This removes the one-frame lag and matches the
  tested upstream timing.

The hook must do nothing unless the game is in free roam:
`G_FLOW == 6`, `G_TRACK % 1000 == 0`, `G_RACETYPE == 0`.

### 3. Transport

- **Real LAN first:** port `roam_bsd.c` to Android (POSIX; drop `sin_len` and
  the Mac `nettest` bits, stub test mode). One UDP socket on port 47210,
  broadcast discovery, network thread, `roam_plat_lock`.
- **nfsu2.xyz second:** the free-roam hub uses the same `/lan` WebSocket as
  LAN play, but `NET_KIND_UDP` (kind 1) frames to `10.77.0.253:47210` and to
  peers. `switch_socket`/`Nfsu2LanBridge` already carries raw frames:
  - add a kind-1 route in native before the LAN parser: kind 1 -> `roam`
    session; kind 3/4/5 -> the existing virtual LAN;
  - add a `roam_plat` backend whose `io.send` enqueues a kind-1 frame through
    the Java outbox, and whose RX is fed by `nativeSwitchFrame`;
  - broadcast/discovery use the switch's hub/peer list, as the page does.
- The protocol is identical over both transports; only `roam_plat` changes.

### 4. UI

- Launcher: a **Free roam with others** toggle and a player name, under Game
  or a new Online section. The name is sent in HELLO.
- In-game: a small status line/toast ("free roam: 3 players") and the
  existing horn/quick-chat controls once audio/names are ported.
- Start the session only in free roam; stop on races/shops/menus (the core
  parks the cars; `roam_plat_stop` sends BYE).

## Milestones

0. **Compile the shared code** into the Android target with the host shim and
   stubs. No runtime behavior. (First check: the v1.2 NA addresses match.)
1. **Hook + local state:** log the local car position/heading and the frame
   cost at Present; verify it only runs in free roam.
2. **Real-LAN session:** two clients on one Wi-Fi (S24 + Amigo, or S24 + Mac)
   see each other in the peer list; no cars yet.
3. **One remote car:** spawn the nearest peer's car in a free slot and set its
   pose each frame. Visual check.
4. **Fill in the experience:** all remote cars, names/minimap, collisions or
   ghost mode, horn/carsfx, hydraulics/nitrous, outruns, crews/quick chat.
5. **nfsu2.xyz transport:** kind-1 frames through the switch; verify with a
   browser client in free roam.
6. **Online races:** the browser's LOBBY/RACEREP path (optional; the native
   Online menu still uses EA Nation/eahub).

## Risks

- **Addresses/build:** the roam code is tied to v1.2 NA; verify the lifted
  `SPEED2.EXE` SHA and the first few addresses (`G_FLOW`, `G_WORLD`,
  `G_PLAYER1`) before porting the car slots.
- **Guest calls:** `rh_call` is thiscall/cdecl with arbitrary ECX/EDX; the
  current `callGuest` has no ECX/EDX variant, so add `callGuestEx` and tests.
- **Hook timing:** Present is safe but one frame late; the 0x581475 site needs
  a re-lift or a generated-code patch.
- **Transport:** nfsu2.xyz's switch routes by VIP; the hub is not a normal UDP
  server. Keep the real-LAN path as the reference test.
- **Performance:** the TV box runs free roam at ~8 FPS; use the S24 for visual
  testing and the box only for protocol tests.

## Immediate next step

Import `roam_core.c/h`, `roam_game.c`, `roam_plat.h`, `roam_bsd.c`,
`horn.c/h`, `carsfx.c/h`, `names.h` and `roam_host.h` into
`android/app/src/main/cpp/roam/`, add `roam_host_android` and
`callGuestEx`, and compile. The first compile tells us which host names need
the most work before any runtime test.

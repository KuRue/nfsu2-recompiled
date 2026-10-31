# Portable free-roam code

`roam_core.c/.h`, `roam_game.c`, `roam_plat.h`, `roam_bsd_android.c`, `horn.h`,
`carsfx.h` and `names.h` come from [th0mk/nfsu2-web](https://github.com/th0mk/nfsu2-web)
(MIT), the same portable free-roam implementation the browser, the Mac native
port and the Windows ASI companion use. `roam_host.h`,
`roam_host_android.cpp` and this Android transport adaptation are this fork's
host layer. The upstream code is unmodified except for the host include branch
in `roam_game.c`, the Windows-compatible build path (`ROAM_HOST_ANDROID`,
`ROAM_HOST_WIN`) the Android nameplate visibility policy (the portable labels setting takes precedence over
the single-player career's legacy player-names flag), and the Android/BSD socket differences in
`roam_bsd_android.c` (no `sin_len`, no Mac test harness).

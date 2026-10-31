# Installation, updates and saves

## Requirements

- ARM64 Android device running Android 10/API 29 or newer.
- Vulkan 1.1-capable GPU/driver. Version support alone does not guarantee every
  device supports the features used by DXVK; use **Graphics setup → Test driver**
  if startup fails.
- Your own supported PC game installation and room for its files, optional packs,
  shader caches and save backups. Game data is not part of the APK.

The v0.14.14 download is ARM64 only. The source also has an experimental
armeabi-v7a target; it is not a second download for this release.

## Install and copy data

Download `NFSU2-AndroidEvolved-v0.14.14-arm64.apk` from
[the fork release](https://github.com/KuRue/nfsu2-recompiled/releases/tag/v0.14.14).
Allow your installer/file manager to install the APK when Android requests it.

Copy the **contents** of the PC game directory, preserving folder structure:

```text
Internal storage/nfsu2/
  SPEED2.EXE
  CARS/
  CREDITS/
  FRONTEND/
  GLOBAL/
  LANGUAGES/
  MOVIES/
  NIS/
  SOUND/
  SUBTITLES/
  TRACKS/
```

Avoid an extra nested `nfsu2/Need for Speed Underground 2/` layer. The location is
`/sdcard/nfsu2/`, outside the APK's private storage.

The supported executable is 4,800,512 bytes, SHA-256:

```text
f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7
```

The port is tied to this build; a filename match alone is insufficient. Source
builds for other executables need their own validated layout/hooks.

Open **NFSU2 Native Dev**, grant file access, and inspect the Game Files checklist.
Select an installed language under **Game → Game language**. Its language file
must exist in `LANGUAGES/`. Optional HD packs/HUD hook data are not a substitute
for the base game folders. Press **Play** when the required checks pass.

## Update an existing installation

This fork uses `com.nfsu2.androidevolved.dev` and the existing development signing
certificate. It can coexist with upstream's production package. Install a new
fork APK **over the existing fork app**, using the same package/signature; do not
uninstall or clear app data as an update step. v0.14.14 is versionCode 29.

Use **Support → Check updates** or the
[fork releases page](https://github.com/KuRue/nfsu2-recompiled/releases). Review
notes and install manually. The release includes SHA256SUMS and a build manifest.
A production upstream APK belongs to a different channel and does not update this app.

## Career saves and backups

Career saves live under:

```text
/sdcard/nfsu2/AppData/Local/NFS Underground 2/
```

Close the game before using **Saves → Backup saves** or **Restore saves**. The
launcher refuses these operations while the game is running to avoid racing an
autosave. Backup ZIPs are written to:

```text
/sdcard/Download/NFSU2/saves/
```

Restore replaces career-save contents; keep a backup of the current saves first.
App preferences/control layouts are separate from career-save ZIPs. Copy important
backups off the phone as well. APK rollback does not undo save-file changes.

## Pause, Resume and Quit

Switching away pauses the game. Resume returns to the retained live session when
its process is still available. Android can reclaim that process; a notification
or keep-alive service cannot guarantee indefinite retention. After process death,
start the game and load the regular career save.

**No durable game snapshot/save-state feature is enabled in the released APK.**
Experimental continuation/capture work in the source does not change this limit.
Use the game's **Quit Game** action for a clean return to the launcher. In game
menus, right stick moves the pointer and R3 clicks mouse-only controls.

[Controls](CONTROLS.md) · [Troubleshooting](TROUBLESHOOTING.md) · [Index](README.md)

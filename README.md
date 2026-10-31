# Crazy Error V2 - Qt 6 / C++ port

This is a native Qt 6 Widgets + Qt Multimedia application. It does not embed or
invoke Python. MIDI parsing is implemented in C++ in `src/main.cpp`; WAV playback
uses `QMediaPlayer`.

## Arch Linux dependencies

```bash
sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-multimedia
```

For exact top-level window positioning under Hyprland/Wayland, install and use
XWayland:

```bash
sudo pacman -S --needed xorg-xwayland
```

To Run:

```bash
./build/crazy-error
```

This requires an active XWayland server (normally provided by Hyprland).

## Build

Place these two assets next to the executable after building:

- `CRAZY-ERROR-170BPM.mid`
- `CRAZY-ERROR-170BPM.wav`

Then:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cp CRAZY-ERROR-170BPM.mid CRAZY-ERROR-170BPM.wav build/
./build/crazy-error
```

The program resolves its assets relative to the executable directory.

## Notes on behavioral compatibility

- Main window and dialogs use Qt 6 Widgets, retaining the original Qt layout,
  dialog dimensions, labels, button labels, and positioning calculations.
- The 4 ms timer and 80 ms event look-ahead are retained.
- MIDI tempo changes are applied, and note-on events are sorted by
  `(time, instrument index)`.
- Pitch 0 closes all dialogs and resets positions; pitch 41 triggers a beep.
- The WAV player is Qt Multimedia rather than pygame.

### MIDI parser compatibility

The bundled parser supports Standard MIDI File formats 0 and 1, PPQN timing,
running status, tempo changes, program changes, and note-on events. Format 2
and SMPTE-time division are rejected. Instrument identities are grouped by
MIDI channel and current program. This matches common MIDI arrangements, but
may differ from `pretty_midi` in unusual files where tracks/program changes
create instrument grouping differently.

### Wayland positioning limitation

The original program explicitly positions many independent windows. Wayland
does not provide ordinary clients with unrestricted global window placement.
XWayland/X11 is therefore required if those coordinates must be honored.

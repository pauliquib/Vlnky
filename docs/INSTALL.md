# Vlnky 1.5 — build, install and usage guide

A KDE Plasma 6 wallpaper that renders the moving wave of the PSP menu (XMB) from a
`system_plugin_bg.rco` file. It also offers XMB month colours, a PS2 (PSX DESR) style wave,
a Svec Studio wave and post-processing for Full HD, 1440p and 4K.

License: GPL-2.0-or-later (`COPYING`). Author: Pavel Švec,
<https://svec-elektro.cz/projekty/psp-xmb-wave/>. A Czech version of this guide is in
`docs/NAVOD.cs.md`.

---

## 1. What the archive contains and what it does not

**Contains:** plugin source code (C++/Qt), shaders, the Plasma package (QML + config), tests,
a preview tool, install and uninstall scripts, and a script to import the original wave.

**Does not contain:** any Sony firmware, any `system_plugin_bg.rco` file or data extracted
from one (meshes, textures, animations). The wallpaper renders files you supply. You can
extract the original wave from an official PSP update yourself (section 5.2). Whoever uses RCO
files from community collections is responsible for their legality.

---

## 2. Requirements

- KDE Plasma 6 (Wayland or X11), Qt **6.7 or newer** (for `QQuickRhiItem`).
- A GPU with OpenGL 3.3 or Vulkan. Plasma uses OpenGL by default.
- To build: a C++17 compiler, CMake 3.21+, Qt development packages.

### Fedora 42+ (tested on Fedora 44)

```bash
sudo dnf install gcc-c++ cmake zlib-devel \
  qt6-qtbase-devel qt6-qtbase-private-devel qt6-qtdeclarative-devel qt6-qtshadertools-devel \
  kf6-kpackage python3
```

`qt6-qtbase-private-devel` provides the `rhi/qrhi.h` headers. Without it the build uses the
bundled copy in `third_party/rhi` (from Qt 6.6) and CMake prints a warning. That works, but an
exact match with the installed Qt is safer.

### Other distributions (package names, untested)

| Distribution | Packages |
|---|---|
| Arch / Manjaro | `base-devel cmake zlib qt6-base qt6-declarative qt6-shadertools python` |
| Debian 13 / Ubuntu 24.10+ / KDE neon | `build-essential cmake zlib1g-dev qt6-base-dev qt6-base-private-dev qt6-declarative-dev qt6-shadertools-dev python3` |
| openSUSE Tumbleweed | `gcc-c++ cmake zlib-devel qt6-base-devel qt6-gui-private-devel qt6-declarative-devel qt6-shadertools-devel python3` |

`kpackagetool6` (installs the Plasma package) usually ships with Plasma.

---

## 3. Build

```bash
tar xzf vlnky-1.5.0.tar.gz
cd vlnky-1.5.0
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

Output:

- `build/libvlnkywave.so` — QML plugin with the renderer,
- `build/plasma-wallpaper/package/` — the assembled Plasma package,
- `build/vlnky-preview` — preview a wave in a window,
- `build/vlnky-extract` — extract the texture from an RCO (for checking).

`-DVLNKY_COPY_USER_QML_MODULE=OFF` stops the build from copying the QML module to
`~/.local/lib64/qml` (useful for packaging).

Optional tests over a whole wave collection:

```bash
VLNKY_WAVE_COLLECTION="/path/to/collection" cmake -B build
cmake --build build && ctest --test-dir build
```

---

## 4. Install (per user)

```bash
bash scripts/install.sh
systemctl --user restart plasma-plasmashell.service
```

The script:

1. builds the project if needed,
2. installs the Plasma package to `~/.local/share/plasma/wallpapers/org.psvec.vlnky/`,
3. copies the QML module to `~/.local/lib64/qml/org/psvec/vlnky/`,
4. if it finds a wave collection, records it as the default and extracts preview textures.

Environment variables:

- `WAVE_COLLECTION=/path/to/collection` — default collection,
- `SKIP_TEXTURE_EXTRACT=1` — skip extracting preview PNGs.

To update, unpack the new archive, run `scripts/install.sh` again and restart Plasma.

---

## 5. Where to get waves

A collection is a plain folder where every wave has its own subfolder with a
`system_plugin_bg.rco` file:

```
My waves/
├── PSP Original (OFW 6.61)/system_plugin_bg.rco
├── My blue/system_plugin_bg.rco
└── …
```

### 5.1 Your own RCO file

Put it into a subfolder of the collection, or use **Import custom RCO…** in the settings.

### 5.2 The original Sony wave from an official update

Download an official PSP update (`EBOOT.PBP`, 6.61 recommended — archived e.g. at
<https://archive.org/details/psp_ofw_firmwares>) and run:

```bash
bash scripts/import-original-wave.sh /path/to/EBOOT.PBP "/path/to/collection"
```

On first use the script downloads and builds
[pspdecrypt](https://github.com/John-K/pspdecrypt) (GPL-3.0) into
`~/.cache/vlnky/tools`; it needs `git`, `gcc-c++`, `zlib-devel` and `openssl-devel`.
It extracts `flash0:/vsh/resource/system_plugin_bg.rco` from the update and stores it in the
collection as `PSP Original (OFW 6.61)`.

The original wave is white. The colour you remember from the PSP comes from the **XMB colour**
option (section 7).

### 5.3 The PS2 style wave

Needs no file: choose **Wave style → PS2 – PSX (DESR) XMB wave** in the settings.

### 5.4 The Svec Studio wave

Needs no file either: choose **Wave style → Svec Studio wave**. It draws the four layered
"hero waves" of the Sencurio landing page (the same waves used on the svec-studio desktop):
filled silhouettes at the bottom of the screen that slowly scroll sideways and bob.

Pick **Wave colour → Custom colour** to tint it (and the soft aurora glow over the
background) with any colour, like the wave settings in svec-studio. The background itself
comes from the usual *Background* options — e.g. a solid custom colour or a gradient.

---

## 6. Enable the wallpaper

1. Right-click the desktop → **Configure Desktop and Wallpaper…**
2. **Wallpaper type:** *Vlnky*.
3. **Wave collection:** your collection folder (*Browse…*).
4. **Wave:** pick a wave; a preview appears below.
5. **Apply / OK.**

---

## 7. All settings

### Wave and colours

| Option | Meaning |
|---|---|
| **Wave collection** | Folder with the waves (under *Advanced*; shown automatically when no waves are found). |
| **Wave** | Selected wave (subfolder of the collection). |
| **Wave style** | *PSP* — wave from the RCO file; *PS2* — procedural PSX (DESR) wave, no RCO needed; *Svec Studio* — the layered hero waves from svec-studio, no RCO needed. |
| **Wave colour** | *Automatic* — theme/style default; *Custom colour* — freely chosen colour for any wave style. |
| **XMB colour** | *Off* — background from the options below; *Automatic* — colour of the current month as on PSP/PS3; or a fixed month (January silver … December red). |
| **Dim at night** | With an XMB colour: full brightness 12:00–15:00, darkest 22:00–06:00 (by 50%). |
| **Import…** (next to the wave list) | Uses any `.rco` file outside the collection. |
| **Background** | When *XMB colour* is off: solid, gradient (start, end, angle), image, or an image from the wave folder (`screen3.bmp`, `preview.png`, `screenshot.png`). Colours are picked with a colour button (opens a colour picker); *Colour preset* offers ready-made combinations and the arrows swap gradient start and end. |
| **Opacity** | Strength of the wave (0.3–1.0). |
| **Height** | PSP/PS2 styles: vertical position of the wave on screen. Svec Studio style: height of the wave band, 30–250 % of the landing-page default. |
| **Speed** | Animation speed, 10–400 % (100 % = original PSP speed). Every slider has a button that resets it to the default. |

### Render quality

Presets set several options at once:

| Preset | Mesh | Supersampling | MSAA | Glow | FPS |
|---|---|---|---|---|---|
| Performance | Medium | 1× | off | off | 30 |
| Balanced | High | 1× | 4× | 30% | 60 |
| Beautiful (Full HD+) | Ultra | 1.5× | 4× | 35% | 60 |
| Maximum (4K) | Ultra | 2× | 8× | 40% | 60 |

| Option | Meaning |
|---|---|
| **Mesh detail** | Surface tessellation: Low 96×28, Medium 168×48, High 264×80, Ultra 400×120 samples. Computed on the CPU. |
| **Supersampling** | The wave is drawn at 1.5× or 2× the resolution and scaled down. The heaviest GPU option (2× at 4K = a 7680×4320 buffer). |
| **Antialiasing** | MSAA 2×/4×/8× for smooth strand edges. |
| **Preset** | Quick quality setup: Performance, Balanced, Beautiful, Maximum. *Custom* = your own combination (shows the advanced options). |
| **Glow** | Soft glow around bright strands. |
| **Vignette** | Darkens the corners. |
| **Smooth (bicubic) reflection texture** | Smooth magnification of the 128×128 texture; without it large monitors show diamond artefacts. |
| **Dithering** | Removes colour banding in dark gradients. |
| **Max FPS** | 15 / 30 / 60 / 120 / 144. On 120–144 Hz monitors the animation is smoother but costs more CPU. |

Recommendation: Full HD → *Balanced* or *Beautiful*; 1440p → *Beautiful*;
4K with a strong GPU → *Maximum*, on integrated graphics *Balanced*.

### Power and performance

| Option | Meaning |
|---|---|
| **Pause animation on battery** | Stops the animation on battery. |
| **Battery saver** | Max 15 FPS, lower mesh, no supersampling, MSAA or bloom. |
| **Adaptive quality** | Lowers the mesh automatically when a frame takes longer than 6 ms. |

### Diagnostics

| Option | Meaning |
|---|---|
| **Show debug overlay** | Shows wave, frame, FPS, tessellation and CPU time top-left. Also `VLNKY_WAVE_DEBUG=1`. |
| **Wireframe mesh** | Shows the surface as a wireframe. |
| **Capture debug screenshots (F12)** | Saves a screenshot to `~/.cache/vlnky/screenshots/`. |

---

## 8. Preview outside Plasma

```bash
build/vlnky-preview -r "/path/to/collection/Wave/system_plugin_bg.rco" --size 1920x1080
build/vlnky-preview --size 1920x1080 --set waveStyle=1 --set backgroundMode=2 --set backgroundTop=#0c76c0
build/vlnky-preview -r wave.rco --set renderScale=2 --set msaaSamples=8 --set bloom=0.4 -g shot.png
```

`--set property=value` sets any `XmbWaveItem` property (`tessLevel`, `renderScale`,
`msaaSamples`, `bloom`, `bicubicTexture`, `dither`, `vignette`, `waveStyle`, `backgroundMode`,
`backgroundTop`, `backgroundBottom`, `ps2WaveColor`, `svecWaveColor`, `svecWaveCustom`,
`waveTint`, `waveOpacity`, `waveCenterY`, `speed`).
`-g file.png` saves a screenshot and exits.

---

## 9. Uninstall

```bash
bash scripts/uninstall.sh            # keeps the cache
bash scripts/uninstall.sh --purge    # also removes cache, screenshots and helper tools
systemctl --user restart plasma-plasmashell.service
```

Switch to another wallpaper first, otherwise Plasma shows an empty desktop.

---

## 10. Troubleshooting

| Problem | Fix |
|---|---|
| Only the background, no wave | Check the RCO path in settings. Enable *Show debug overlay* and read the log (below). |
| Wave looks wrong after an update | `rm -rf ~/.cache/vlnky/` and restart Plasma. |
| Stutter or high CPU | *Performance* preset, or lower *Mesh detail* and *Max FPS*. |
| GPU struggles at 4K | Lower *Supersampling* to 1× or 1.5×. |
| Plugin does not load | Was plasmashell restarted after install? Does `~/.local/share/plasma/wallpapers/org.psvec.vlnky/contents/ui/org/psvec/vlnky/libvlnkywave.so` exist? |
| Settings page shows only "Colour" | plasmashell still holds an old plugin in memory. Run `scripts/install.sh` and `systemctl --user restart plasma-plasmashell.service`. |
| CMake warns "rhi/qrhi.h not found" | Install `qt6-qtbase-private-devel` (Fedora) and rebuild. |

Log:

```bash
QT_LOGGING_RULES="xmb.*=true" journalctl --user -f -t plasmashell
```

Categories: `xmb.rco`, `xmb.mesh`, `xmb.anim`, `xmb.render`, `xmb.perf`.

---

## 11. License

This program is free software under the GNU GPL version 2 or (at your option) any later
version (`COPYING`). Third-party parts and attributions are listed in `NOTICE`. PlayStation,
PSP, PS2, PSX and XMB are trademarks of Sony Interactive Entertainment; this project is not
affiliated with Sony.

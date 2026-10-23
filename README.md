# CAM-ASCII-cpp

A C++ webcam-to-ASCII project that turns a live camera feed into a real-time character-based visual display. The project evolved through multiple versions, from earlier webcam ASCII implementations to a more configurable, terminal-inspired visual engine with several rendering styles and image/video export.

**Repository:** [kakadd172-lab/CAM-ASCII-cpp](https://github.com/kakadd172-lab/CAM-ASCII-cpp)
**Created by:** [Divy179](https://github.com/kakadd172-lab)

## Overview

The application captures frames from a webcam, processes the image, and represents the image as ASCII characters. Depending on the version, the output can include colour palettes, edge-based characters, retro terminal effects, and other visual styles.

The newer `webcam_ascii4` version is the most feature-rich version documented in this repository. It uses **C++17**, **Raylib** for windowing/rendering, and **OpenCV 4** for camera capture and image processing.

## Features

Features documented for the newer version include:

- Live webcam feed rendered as ASCII art.
- Nine visual modes:
  1. **RAW_RGB** — colour ASCII using source RGB values.
  2. **MATRIX_LINK** — green Matrix-style terminal look.
  3. **GEOMETRY** — Sobel-based edge rendering using directional characters.
  4. **NEON_PUNK** — cyberpunk-inspired colours.
  5. **TERM_AMBER** — amber CRT-terminal look.
  6. **RETRO_8BIT** — Game Boy-inspired green palette.
  7. **THERMAL_IR** — simulated thermal/infrared palette.
  8. **RAW_BINARY** — binary `0`/`1` rendering.
  9. **SIGNAL_LOST** — glitch-style distortion.
- Brightness and contrast adjustments.
- CRT scanline/vignette effect and a hideable HUD.
- Screenshot export as **PNG** and the character grid as **TXT**.
- MP4 recording of the rendered application output.
- A keyboard-operated pause/settings menu in `webcam_ascii4`.
- Window and ASCII-density settings in the newer version.

Exact features and controls can differ between versions. Refer to the documentation inside the corresponding version folder before using or building that version.

## Repository structure

```text
CAM-ASCII-cpp/
├── README.md                 # Project overview and navigation
├── README(pre).txt           # Earlier/preliminary project notes
├── LICENSE                   # Repository license
├── .gitignore                # Excludes individual DLLs and MP4 recordings
├── media/                    # Top-level sample ASCII captures
│   ├── ascii_snap_*.png
│   └── ascii_snap_*.txt
├── ol_modls/                 # Older/previous model files and source
│   ├── webcam_ascii4.cpp
│   └── webcam_ascii*.exe
├── webcam_ascii3/            # Version 3 source, media, and documentation
│   ├── webcam_ascii.cpp
│   ├── README.md
│   ├── DOCUMENTATION.txt
│   └── media/
└── webcam_ascii4/             # Version 4 source, media, and documentation
    ├── webcam_ascii4.cpp
    ├── README.txt
    ├── DOCUMENTATION.docx
    ├── dll(s).zip
    └── media/
```

This is a guide to the intended contents; filenames may change as the project develops. Executables are included in the repository as convenience builds. The C++ source files are provided for inspection and further development.

## Versions and documentation

Each version has its own documentation because controls, features, and implementation details may differ. **Use the README/documentation in the version folder you intend to run** rather than assuming every instruction applies to every version.

| Location | What it contains |
|---|---|
| `README(pre).txt` | Preliminary/earlier project notes from the root folder. |
| `ol_modls/` | Older model/source and executable files retained for reference. |
| `webcam_ascii3/` | Version 3 source, sample media, `README.md`, and `DOCUMENTATION.txt`. |
| `webcam_ascii4/` | Version 4 source, sample media, `README.txt`, `DOCUMENTATION.docx`, and the DLL archive. |
| `media/` | Sample PNG screenshots and TXT ASCII output. |

- **Version 3:** start with [`webcam_ascii3/README.md`](webcam_ascii3/README.md), then consult [`webcam_ascii3/DOCUMENTATION.txt`](webcam_ascii3/DOCUMENTATION.txt).
- **Version 4:** start with [`webcam_ascii4/README.txt`](webcam_ascii4/README.txt), then consult [`webcam_ascii4/DOCUMENTATION.docx`](webcam_ascii4/DOCUMENTATION.docx).
- **Earlier notes:** [`README(pre).txt`](README%28pre%29.txt).

## Running the project

### Option 1: Run a provided executable

Prebuilt `.exe` files are included. On a compatible Windows setup:

1. Open the version folder you want to try.
2. Read that version's README and documentation first.
3. Run the matching executable.
4. Allow the application to access your webcam if Windows requests permission.

The application depends on the runtime libraries required by its build. The `webcam_ascii4/dll(s).zip` archive is included for the version 4 runtime files. If the executable reports a missing DLL, consult the version's documentation and make sure the required runtime libraries are available.

### Option 2: Build from source

The documented stack for the newer engine is:

- A C++17-compatible compiler (the included manual documents an MSYS2/MinGW-w64 workflow).
- Raylib.
- OpenCV 4, including the required video I/O and image-processing libraries.
- The appropriate Windows graphics/system libraries for the chosen toolchain.

The version 2.0 technical manual in `webcam_ascii4/DOCUMENTATION.docx` documents a MinGW-style compile command and controls. Follow the build instructions that match the exact source file and libraries installed on your machine; library paths and linker options may need to be adjusted for your environment.

## Controls in the newer version

The version 4 documentation describes the following controls:

| Key | Action |
|---|---|
| `1`–`9` | Select a visual mode directly. |
| `SPACE` | Cycle through visual modes. |
| `P` or `ESC` | Open/close the pause/settings menu. |
| `C` | Toggle the CRT effect. |
| `H` | Hide/show the HUD. |
| `S` | Save a PNG snapshot and TXT ASCII grid. |
| `R` | Start/stop MP4 recording. |
| `UP` / `DOWN` | Adjust contrast while live. |
| `LEFT` / `RIGHT` | Adjust brightness while live. |
| `BACKSPACE` | Reset brightness and contrast. |

In the pause menu, use `UP`/`DOWN` to select an item, `LEFT`/`RIGHT` to adjust it, and `ENTER` to resume when the Resume option is selected. The exact control set can vary by version, so check that version's own documentation.

## Output files

The newer version can export:

- **PNG** — screenshot of the rendered application output.
- **TXT** — text representation of the ASCII character grid.
- **MP4** — recording of the rendered application output.

Exports are written to the application's media directory according to the version's implementation. Screenshots and recordings may include visible overlays such as the HUD or CRT effects because they capture the rendered output.

## Notes

- A working webcam and compatible Windows runtime environment are needed to run the included Windows executables.
- The source is included so the project can be inspected and modified.
- Individual `.dll` files and `.mp4` recordings are excluded by `.gitignore`; the DLL archive `webcam_ascii4/dll(s).zip` is included.
- The DLL archive is relatively large. GitHub accepted it, but recommends keeping individual files below 50 MB; GitHub warns on files above that recommendation.
- Features described here are based on the documentation available for the repository's versions. Check the matching version documentation for the most accurate details.

## License

See [`LICENSE`](LICENSE) for the license that applies to this repository.

## Acknowledgements and project status

CAM-ASCII-cpp is a personal C++ project exploring real-time image-to-ASCII rendering, visual filters, and retro terminal presentation. The repository keeps earlier versions alongside the newer implementation so readers can compare their evolution and consult version-specific documentation.

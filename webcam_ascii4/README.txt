ASCII Webcam / Tactical Optic Engine
====================================
Project Report — Current Feature Set, Controls, Architecture, Build and Development History

This report documents the current state of the webcam ASCII project as developed so far. It combines the current webcam_ascii4.cpp implementation with the development history from this project session. The program is a real-time webcam-to-ASCII visual engine built around Raylib for the window/rendering layer and OpenCV for camera capture and image processing.

1. COMPLETE FEATURE LIST
- Live webcam ASCII conversion: Real-time camera feed converted to an ASCII character grid.
- 16:9 source handling: Camera image is center-cropped to 16:9 before being resized, preventing the earlier stretch/distortion problem.
- Resizable window: Window is resizable while maintaining a locked 16:9 aspect ratio.
- Resolution presets: 960x540, 1280x720, 1600x900, and 1920x1080 available from the pause menu.
- Variable ASCII font size: 8–24 px range; smaller values create a denser grid and more visual detail.
- Monospace font support: Attempts to load Windows Consolas, then Lucida Console; falls back to Raylib default font.
- Brightness control: Live keyboard tuning and pause-menu slider; range -100 to +100.
- Contrast control: Live keyboard tuning and pause-menu slider; range 0.50x to 2.50x.
- Detail enhancement: Optional sharpening/high-frequency enhancement before grayscale conversion.
- CLAHE local contrast: OpenCV CLAHE is applied to the grayscale image for stronger local tonal separation.
- Temporal smoothing: A small amount of previous-frame blending reduces flicker and unstable ASCII changes.
- Dithered character mapping: 4x4 Bayer-style dithering improves tonal transitions in the limited ASCII ramp.
- Nine visual modes: RGB, Matrix, Edge, Cyberpunk, Amber, GameBoy, Thermal, Binary, and Glitch.
- CRT overlay: Scanlines, moving scan bar, and edge/vignette darkening for a display-terminal look.
- HUD: Mode, contrast, brightness, font size, FPS, control hints, recording status, and notifications.
- Snapshots: PNG screenshot plus TXT ASCII grid saved to the media folder.
- MP4 recording: Records the rendered application output at a 30 FPS video writer rate.
- Pause menu: Keyboard-driven settings menu for resolution, font, brightness, contrast, detail, CRT and resume.
- Camera fallback: Attempts DirectShow first and then falls back to the default OpenCV camera backend.
- Double-click EXE workflow: Release build is designed to run without keeping a terminal window open when built with -mwindows.

2. KEYBOARD CONTROLS
- 1–9: Select a visual mode directly.
- SPACE: Advance to the next visual mode.
- P: Open/close the pause menu.
- ESC: Open/close the pause menu.
- C: Toggle CRT overlay.
- H: Hide/show HUD.
- S: Save a snapshot: PNG + TXT.
- R: Start or stop MP4 recording.
- UP / DOWN: While live: increase/decrease contrast continuously.
- LEFT / RIGHT: While live: decrease/increase brightness continuously.
- BACKSPACE: Reset live brightness and contrast to defaults.

3. PAUSE / SETTINGS MENU
Open with P or ESC. UP/DOWN select. LEFT/RIGHT adjust. ENTER resumes when RESUME is selected.
- RESOLUTION: LEFT / RIGHT -> Cycles 960x540 → 1280x720 → 1600x900 → 1920x1080.
- FONT SIZE: LEFT / RIGHT -> 8–24 px. Smaller = denser ASCII and more grid cells; larger = bigger, less dense glyphs.
- BRIGHTNESS: LEFT / RIGHT -> -100 to +100; changes OpenCV brightness offset.
- CONTRAST: LEFT / RIGHT -> 0.50x to 2.50x; changes OpenCV contrast scale.
- DETAIL ENHANCEMENT: LEFT / RIGHT -> Toggles the sharpening/detail pass.
- CRT SCANLINES: LEFT / RIGHT -> Toggles CRT scanline/scan-bar/vignette overlay.
- RESUME: ENTER -> Leaves the pause menu.
- BACKSPACE in pause: reset brightness and contrast.

4. VISUAL MODES
- 1 — RAW_RGB: Direct color ASCII using the source pixel RGB values for glyph color.
- 2 — MATRIX_LINK: Green terminal-style rendering with a subtle animated pulse tied to row/time.
- 3 — GEOMETRY: Sobel-based edge detection. Characters are selected from edge direction (/, |, \, -).
- 4 — NEON_PUNK: Cyberpunk palette using dark violet, cyan, magenta/pink and bright highlights.
- 5 — TERM_AMBER: Amber/orange CRT-terminal appearance.
- 6 — RETRO_8BIT: Game Boy-like four-level green palette using #, x, :, and space.
- 7 — THERMAL_IR: Luma-to-color pseudo-thermal palette moving through blue/cyan, red/orange and pale highlights.
- 8 — RAW_BINARY: Two-state ASCII representation using 0 and 1 with dark areas suppressed.
- 9 — SIGNAL_LOST: Glitch mode with row offsets, chromatic displacement, animated green treatment, and random glyph corruption.

5. IMAGE PROCESSING PIPELINE
- 1. Capture: OpenCV reads the webcam frame. DirectShow (CAP_DSHOW) is attempted first, then a default backend fallback.
- 2. Mirror: The frame is flipped horizontally so the preview behaves like a mirror.
- 3. Crop: The source is center-cropped to 16:9.
- 4. Grid sizing: The current window size and font size determine cell dimensions and grid width/height.
- 5. Resize: The 16:9 crop is resized to the current ASCII grid using INTER_AREA.
- 6. Brightness/contrast: OpenCV convertTo applies the current contrast multiplier and brightness offset.
- 7. Detail pass: Optional Gaussian-blur subtraction sharpens the frame.
- 8. Grayscale/CLAHE: The tuned frame is converted to grayscale and locally contrast-enhanced with CLAHE.
- 9. Temporal smoothing: Current frame is blended 90/10 with the previous tuned frame to reduce flicker.
- 10. Mode transform: The selected mode changes glyph selection and/or glyph color; Edge additionally uses Sobel gradients.
- 11. ASCII raster: Luma is mapped through a 10-character ramp with a 4x4 Bayer-style dither.
- 12. Overlay: CRT effects, HUD, recording indicator, notifications and pause UI are drawn by Raylib.
- 13. Output: The screen can be captured to PNG/TXT or sampled into a 30 FPS MP4 recording.

6. QUALITY / DENSITY
- ASCII ramp: “ .:-=+*#%@”. 10 tonal steps from empty space to dense fill.
- Font size: 8–24 px. Smaller font produces a denser, more detailed ASCII field.
- Default font: Windows Consolas / Lucida Console / Raylib fallback. Monospace is preferred for regular cell geometry.
- Grid model: Window-dependent. Grid width/height are recalculated from measured glyph width and font size.
- Source aspect: 16:9. Center crop instead of non-uniform stretching.
- Camera request: 1280x720 @ 30 FPS. Actual camera support still depends on hardware/driver.
- Render target: 60 FPS. Raylib target FPS is 60; actual FPS is shown in the HUD.
- Recording: 30 FPS. Frames are sampled with an accumulator to match the video writer rate.

7. WINDOW / RESOLUTION
- Initial window: 1280x720
- Minimum window: 640x360
- Aspect ratio: locked to 16:9 while resizing
- Presets: 960x540, 1280x720, 1600x900, 1920x1080

8. FONT SYSTEM
- Font size: 8–24 px; default 14 px.
- Prefers Consolas, then Lucida Console, then Raylib default.
- Grid dimensions are recalculated from measured glyph width and font size.

9. OUTPUT FILES
- media/ascii_snap_N.png: Full rendered window screenshot at capture time.
- media/ascii_snap_N.txt: ASCII character grid corresponding to the current rendered frame.
- media/ascii_rec_N.mp4: Rendered output video captured through OpenCV VideoWriter at 30 FPS.

10. BUILD
g++ webcam_ascii4.cpp -o webcam_ascii4.exe -std=c++17 -O2 -I/mingw64/include/opencv4 -lraylib -lopencv_core -lopencv_videoio -lopencv_imgproc -lopengl32 -lgdi32 -lwinmm -mwindows

11. DEVELOPMENT HISTORY
- Initial prototype: Webcam image was resized directly to an ASCII grid. Early implementation used 120x60 cells, per-character drawing, direct frame stretching, frame-rate-dependent tuning and an initial 30 FPS recording setup.
- Foundation rewrite: The project moved to a 16:9 center-crop, a less distorted grid, time-based controls, expanded visual modes, better CRT/HUD structure, snapshot export and improved recording timing.
- Professionalization pass: A proper pause/settings system was added, with resolution presets, brightness/contrast sliders, detail enhancement, CRT toggling and a resizable 16:9 window.
- Font-quality pass: Font size became a user-controlled setting. Grid dimensions are recalculated from measured glyph width so smaller fonts increase ASCII density instead of simply scaling the same grid.
- Desktop EXE workflow: The project was prepared for double-click execution using a Windows subsystem build and local runtime DLL deployment.
- Runtime debugging: The build encountered a missing ShowMessageBox helper, then OpenCV DLL naming differences, then transitive FFmpeg DLL requirements. A later attempt to use windows.h caused symbol conflicts with Raylib, so the project returned to a Raylib/OpenCV-only implementation.
- Pause-input correction: A pause-state input bug was fixed so P/ESC toggles pause from the main loop, rather than only handling input after the state was already paused.
- Current state: The current source is webcam_ascii4.cpp with the requested settings, visual modes, capture/recording features, resizable 16:9 window, font control and desktop-oriented build flow.

12. CURRENT SOURCE SUBSYSTEMS
- AppState: Central state and buffers.
- CropTo16x9: Aspect-safe camera crop.
- RebuildGridLayout: Font/window dependent grid.
- MaintainWindowAspect: Resizable 16:9 window.
- ApplyImageQuality: Image-quality pipeline.
- LumaToCharIndex: Luma to ASCII + dithering.
- RenderAscii: Mode-specific rendering.
- DrawCRT: CRT overlay.
- DrawHUD / DrawPauseMenu: UI layers.
- HandleLiveControls / HandlePauseMenu: Input.
- StartRecording / RecordScreenFrame / StopRecording: Video.
- SaveSnapshot: PNG/TXT capture.

13. OPERATIONAL NOTES
- The pause menu is keyboard-driven; the current implementation does not provide mouse slider dragging.
- Changing a resolution preset while recording stops the recording so the video dimensions do not change mid-file.
- The camera preview pauses processing while the settings menu is open; the last rendered frame remains visible underneath the menu.
- The recording path is screen-output capture, not a raw-camera recording. The HUD/CRT and other rendered effects can therefore appear in recorded video.
- Snapshot PNG capture is also screen-output capture, so it includes the current visual overlays that are visible at the moment of capture.
- The current report describes the software as implemented in the latest source reviewed; it does not claim features that are not represented in that source.

14. QUICK START
1. Launch webcam_ascii4.exe.
2. 1–9 choose mode; SPACE cycles.
3. UP/DOWN contrast; LEFT/RIGHT brightness.
4. P or ESC opens/closes pause menu.
5. C toggles CRT; H toggles HUD; S saves; R records; BACKSPACE resets tuning.
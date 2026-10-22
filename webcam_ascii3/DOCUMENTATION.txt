===============================================================================
               ASCII OS // TACTICAL OPTIC ENGINE v2.0
                     SYSTEM OPERATIONS & TECHNICAL MANUAL
===============================================================================

1. SYSTEM OVERVIEW
-------------------------------------------------------------------------------
The Tactical Optic Engine is a C++17 real-time video processing application that 
transforms live optical feed from a webcam into an accelerated, multi-mode ASCII 
art array. By combining OpenCV for image analysis/frame capture and Raylib for 
low-latency OpenGL rendering, the system processes video pixels into high-density 
text representations at up to 60 FPS.

2. ARCHITECTURE & SPECIFICATIONS
-------------------------------------------------------------------------------
* Program Language:        C++17
* Graphics Framework:      Raylib (OpenGL Backend)
* Computer Vision:         OpenCV 4 (DirectShow Capture & MP4 Encoding)
* ASCII Grid Size:         120 columns x 60 rows (7,200 active characters/frame)
* Render Resolution:       1080 x 780 pixels (9x13 monospaced font geometry)
* Color Pipeline:          24-bit RGB, Custom Palette Mapping, Sobel Gradients
* Export Directory:        /media (Auto-creates on launch)

3. TACTICAL CONTROLS & KEYBINDINGS
-------------------------------------------------------------------------------
KEY INPUT               ACTION / FUNCTION
-------------------------------------------------------------------------------
[1] - [9]               Directly switch to Render Modes 1 through 9
[SPACE]                 Cycle forward through all 9 Render Modes
[UP ARROW]              Increase Image Contrast (+0.02 step)
[DOWN ARROW]            Decrease Image Contrast (-0.02 step)
[RIGHT ARROW]           Increase Image Brightness (+2 step)
[LEFT ARROW]            Decrease Image Brightness (-2 step)
[BACKSPACE]             Reset Contrast & Brightness to Defaults (1.0 / 0)
[C]                     Toggle CRT Scanlines & Vignette Shading ON/OFF
[H]                     Toggle Tactical HUD Overlay ON/OFF (Clear Screen)
[S]                     Capture Snapshot (Generates .PNG + .TXT in /media)
[R]                     Toggle Live Video Recording (Outputs .MP4 in /media)

4. OPTIC RENDER MODES (FILTERS)
-------------------------------------------------------------------------------
MODE ID   NAME          DESCRIPTION & ALGORITHMIC BEHAVIOR
-------------------------------------------------------------------------------
[1]       RAW_RGB       Direct color-mapping. Converts luminance to ASCII math 
                        while retaining original webcam RGB values.
[2]       MATRIX_LINK   Terminal Matrix style. Forces high-contrast green-on-black 
                        palette with luminance amplification.
[3]       GEOMETRY      Edge detection engine using OpenCV Sobel filters. Calculates 
                        gradient angles and draws physical boundaries using 
                        geometry characters (/, \, |, -) in cyan.
[4]       NEON_PUNK     Cyberpunk color ramp mapping low luma to dark violet, 
                        mid-range to cyan/magenta, and highlights to yellow.
[5]       TERM_AMBER    Classic 1980s monochrome phosphor CRT display output.
[6]       RETRO_8BIT    Simulates 4-shade Gameboy matrix (#, x, :, space) 
                        using custom olive green palettes.
[7]       THERMAL_IR    Simulated FLIR infrared scope mapping low values to blue, 
                        mid values to orange/red, and hotspots to white.
[8]       RAW_BINARY    Renders stream as dynamic 1s and 0s based on light threshold.
[9]       SIGNAL_LOST   Glitch mode introducing random line displacement, RGB 
                        channel separation, and spatial tearing.

5. MEDIA EXPORT ENGINE
-------------------------------------------------------------------------------
All captures are auto-indexed to prevent overwriting existing media files:

* Screenshot Export (.PNG):
  Triggered via [S]. Captures the post-processed frame buffer directly after 
  EndDrawing() to guarantee a non-black, fully rendered output image.

* Text Matrix Export (.TXT):
  Triggered via [S]. Dumps the exact 120x60 array of ASCII characters visible on 
  screen into a plain text file for copy-pasting.

* Video Uplink (.MP4):
  Triggered via [R]. Feeds captured Raylib screen data directly into an OpenCV 
  VideoWriter instance encoded at 30 FPS using the MP4V codec.

6. COMPILATION INSTRUCTIONS (MSYS2 / MinGW-w64)
-------------------------------------------------------------------------------
Run the following build command in your terminal:

g++ webcam_ascii.cpp -o webcam_ascii.exe -std=c++17 -I/mingw64/include/opencv4 \
-lraylib -lopencv_core -lopencv_videoio -lopencv_imgproc -lopengl32 -lgdi32 -lwinmm

===============================================================================
                     END OF SYSTEM MANUAL // ASCII OS v2.0
===============================================================================
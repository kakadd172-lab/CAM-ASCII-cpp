#include "raylib.h"
#include <opencv2/opencv.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

enum RenderMode {
    MODE_RGB = 1,
    MODE_MATRIX,
    MODE_EDGE,
    MODE_CYBERPUNK,
    MODE_AMBER,
    MODE_GAMEBOY,
    MODE_THERMAL,
    MODE_BINARY,
    MODE_GLITCH
};

struct Resolution {
    int w;
    int h;
    const char* name;
};

static constexpr float ASPECT = 16.0f / 9.0f;
static constexpr int GRID_W = 120;
static constexpr int GRID_H = 47;

static const std::array<Resolution, 4> RESOLUTIONS = {{
    {960, 540, "960 x 540"},
    {1280, 720, "1280 x 720"},
    {1600, 900, "1600 x 900"},
    {1920, 1080, "1920 x 1080"}
}};

static const char* MODE_NAMES[] = {
    "",
    "RAW RGB",
    "MATRIX LINK",
    "GEOMETRY",
    "NEON PUNK",
    "TERM AMBER",
    "RETRO 8BIT",
    "THERMAL IR",
    "RAW BINARY",
    "SIGNAL GLITCH"
};

static const char* ASCII_RAMP =
    " .'`^\",:;Il!i~+_-?][}{1)(|/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";

static const int BAYER4[4][4] = {
    {0, 8, 2, 10},
    {12, 4, 14, 6},
    {3, 11, 1, 9},
    {15, 7, 13, 5}
};

static Color WithAlpha(Color c, unsigned char a) {
    c.a = a;
    return c;
}

static Rectangle AspectViewport(int w, int h) {
    float targetH = w / ASPECT;
    float targetW = h * ASPECT;

    if (targetH <= h) {
        float y = (h - targetH) * 0.5f;
        return {0, y, (float)w, targetH};
    }

    float x = (w - targetW) * 0.5f;
    return {x, 0, targetW, (float)h};
}

static Color LerpColor(Color a, Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);

    return {
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        (unsigned char)(a.a + (b.a - a.a) * t)
    };
}

static Color ThermalColor(float v) {
    v = std::clamp(v, 0.0f, 1.0f);

    Color c1 = {0, 0, 40, 255};
    Color c2 = {0, 80, 255, 255};
    Color c3 = {0, 255, 255, 255};
    Color c4 = {255, 255, 0, 255};
    Color c5 = {255, 50, 0, 255};
    Color c6 = {255, 255, 255, 255};

    if (v < 0.20f) return LerpColor(c1, c2, v / 0.20f);
    if (v < 0.40f) return LerpColor(c2, c3, (v - 0.20f) / 0.20f);
    if (v < 0.60f) return LerpColor(c3, c4, (v - 0.40f) / 0.20f);
    if (v < 0.80f) return LerpColor(c4, c5, (v - 0.60f) / 0.20f);
    return LerpColor(c5, c6, (v - 0.80f) / 0.20f);
}

static std::string FormatFloat(float v, int decimals = 2) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(decimals) << v;
    return ss.str();
}

static void DrawSlider(
    Rectangle r,
    float value01,
    const std::string& label,
    const std::string& valueText,
    Font font
) {
    value01 = std::clamp(value01, 0.0f, 1.0f);

    DrawTextEx(
        font,
        label.c_str(),
        {r.x, r.y - 23},
        18,
        1,
        Color{180, 210, 190, 255}
    );

    DrawTextEx(
        font,
        valueText.c_str(),
        {r.x + r.width - MeasureTextEx(font, valueText.c_str(), 18, 1).x, r.y - 23},
        18,
        1,
        Color{80, 255, 150, 255}
    );

    DrawRectangleRounded(
        r,
        0.25f,
        8,
        Color{10, 18, 15, 255}
    );

    DrawRectangleRounded(
        {
            r.x,
            r.y,
            r.width * value01,
            r.height
        },
        0.25f,
        8,
        Color{20, 180, 90, 255}
    );

    DrawRectangleLinesEx(
        r,
        1.0f,
        Color{50, 110, 80, 180}
    );

    DrawCircle(
        r.x + r.width * value01,
        r.y + r.height * 0.5f,
        7,
        Color{220, 255, 230, 255}
    );
}

static void DrawPanel(Rectangle r) {
    DrawRectangleRounded(
        r,
        0.035f,
        12,
        Color{4, 10, 8, 245}
    );

    DrawRectangleRounded(
        {r.x + 2, r.y + 2, r.width - 4, r.height - 4},
        0.03f,
        12,
        Color{6, 15, 11, 245}
    );

    DrawRectangleLinesEx(
        r,
        1.0f,
        Color{45, 160, 95, 210}
    );

    DrawRectangle(
        (int)r.x,
        (int)r.y,
        (int)r.width,
        4,
        Color{40, 255, 120, 220}
    );
}

static void StopRecording(
    bool& isRecording,
    cv::VideoWriter& writer,
    std::string& notification,
    float& notificationTimer
) {
    if (isRecording) {
        isRecording = false;
        if (writer.isOpened())
            writer.release();

        notification = "RECORDING STOPPED";
        notificationTimer = 2.0f;
    }
}

static void ApplyWindowResolution(
    const Resolution& res,
    bool& isRecording,
    cv::VideoWriter& writer,
    std::string& notification,
    float& notificationTimer,
    int& lastWindowW,
    int& lastWindowH
) {
    StopRecording(
        isRecording,
        writer,
        notification,
        notificationTimer
    );

    SetWindowSize(res.w, res.h);
    lastWindowW = res.w;
    lastWindowH = res.h;
}

static void MaintainWindowAspect(
    bool& isRecording,
    cv::VideoWriter& writer,
    std::string& notification,
    float& notificationTimer,
    int& lastWindowW,
    int& lastWindowH
) {
    int w = GetScreenWidth();
    int h = GetScreenHeight();

    if (w <= 0 || h <= 0)
        return;

    if (!IsWindowResized())
        return;

    int dw = std::abs(w - lastWindowW);
    int dh = std::abs(h - lastWindowH);

    int newW = w;
    int newH = h;

    if (dw >= dh) {
        newH = (int)std::round(w / ASPECT);
    } else {
        newW = (int)std::round(h * ASPECT);
    }

    newW = std::max(newW, 854);
    newH = std::max(newH, 480);

    if (newW != w || newH != h) {
        StopRecording(
            isRecording,
            writer,
            notification,
            notificationTimer
        );

        SetWindowSize(newW, newH);

        w = newW;
        h = newH;
    }

    lastWindowW = w;
    lastWindowH = h;
}

int main() {
    fs::create_directories("media");

    cv::VideoCapture cap(0, cv::CAP_DSHOW);

    if (!cap.isOpened()) {
        std::cerr << "ERROR: Camera could not be opened.\n";
        return 1;
    }

    cap.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
    cap.set(cv::CAP_PROP_FPS, 60);

    SetConfigFlags(
        FLAG_WINDOW_RESIZABLE |
        FLAG_VSYNC_HINT |
        FLAG_MSAA_4X_HINT
    );

    InitWindow(
        1280,
        720,
        "ASCII OPTIC // PROFESSIONAL TERMINAL"
    );

    SetWindowMinSize(854, 480);
    SetTargetFPS(60);

    Font font = GetFontDefault();

    RenderMode mode = MODE_CYBERPUNK;

    float contrast = 1.0f;
    float brightness = 0.0f;

    bool crtEnabled = true;
    bool hudEnabled = true;
    bool detailBoost = true;
    bool paused = false;

    bool isRecording = false;
    bool triggerSnapshot = false;

    int selectedResolution = 1;
    int menuItem = 0;

    int lastWindowW = GetScreenWidth();
    int lastWindowH = GetScreenHeight();

    float notificationTimer = 4.0f;
    std::string notification =
        "OPTIC ENGINE INITIALIZED";

    std::string lastSnapshotName = "NONE";

    cv::VideoWriter videoWriter;

    cv::Mat frame;
    cv::Mat cropped;
    cv::Mat smallFrame;
    cv::Mat tunedFrame;
    cv::Mat gray;
    cv::Mat blurredGray;
    cv::Mat edgeX;
    cv::Mat edgeY;
    cv::Mat absX;
    cv::Mat absY;
    cv::Mat edgeMag;
    cv::Mat previousGray;
    cv::Mat smoothGray;
    cv::Mat enhancedGray;

    char charGrid[GRID_H][GRID_W]{};

    cv::Ptr<cv::CLAHE> clahe =
        cv::createCLAHE(2.2, cv::Size(4, 4));

    float frameAccumulator = 0.0f;
    float fpsTimer = 0.0f;
    int fpsFrames = 0;
    float actualFPS = 60.0f;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        float timeNow = (float)GetTime();

        MaintainWindowAspect(
            isRecording,
            videoWriter,
            notification,
            notificationTimer,
            lastWindowW,
            lastWindowH
        );

        if (notificationTimer > 0.0f)
            notificationTimer -= dt;

        fpsTimer += dt;
        fpsFrames++;

        if (fpsTimer >= 0.5f) {
            actualFPS = fpsFrames / fpsTimer;
            fpsFrames = 0;
            fpsTimer = 0.0f;
        }

        // ---------------------------------------------------------
        // GLOBAL CONTROLS
        // ---------------------------------------------------------

        if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) {
            paused = !paused;
        }

        if (!paused) {
            if (IsKeyPressed(KEY_ONE))   mode = MODE_RGB;
            if (IsKeyPressed(KEY_TWO))   mode = MODE_MATRIX;
            if (IsKeyPressed(KEY_THREE)) mode = MODE_EDGE;
            if (IsKeyPressed(KEY_FOUR))  mode = MODE_CYBERPUNK;
            if (IsKeyPressed(KEY_FIVE))  mode = MODE_AMBER;
            if (IsKeyPressed(KEY_SIX))   mode = MODE_GAMEBOY;
            if (IsKeyPressed(KEY_SEVEN)) mode = MODE_THERMAL;
            if (IsKeyPressed(KEY_EIGHT)) mode = MODE_BINARY;
            if (IsKeyPressed(KEY_NINE))  mode = MODE_GLITCH;

            if (IsKeyPressed(KEY_SPACE)) {
                int next = ((int)mode % 9) + 1;
                mode = (RenderMode)next;
            }

            if (IsKeyPressed(KEY_C))
                crtEnabled = !crtEnabled;

            if (IsKeyPressed(KEY_H))
                hudEnabled = !hudEnabled;

            if (IsKeyPressed(KEY_Q))
                detailBoost = !detailBoost;

            if (IsKeyPressed(KEY_S))
                triggerSnapshot = true;

            if (IsKeyPressed(KEY_R)) {
                if (!isRecording) {
                    int index = 1;

                    while (fs::exists(
                        "media/ascii_rec_" +
                        std::to_string(index) +
                        ".mp4"
                    )) {
                        index++;
                    }

                    int recW = GetScreenWidth();
                    int recH = GetScreenHeight();

                    std::string path =
                        "media/ascii_rec_" +
                        std::to_string(index) +
                        ".mp4";

                    videoWriter.open(
                        path,
                        cv::VideoWriter::fourcc(
                            'm', 'p', '4', 'v'
                        ),
                        30.0,
                        cv::Size(recW, recH)
                    );

                    if (videoWriter.isOpened()) {
                        isRecording = true;
                        notification =
                            "RECORDING // " + path;
                        notificationTimer = 3.0f;
                    }
                } else {
                    StopRecording(
                        isRecording,
                        videoWriter,
                        notification,
                        notificationTimer
                    );
                }
            }

            if (IsKeyPressed(KEY_BACKSPACE)) {
                contrast = 1.0f;
                brightness = 0.0f;

                notification =
                    "IMAGE TUNING RESET";
                notificationTimer = 1.5f;
            }
        }

        // ---------------------------------------------------------
        // PAUSE MENU INPUT
        // ---------------------------------------------------------

        if (paused) {
            if (IsKeyPressed(KEY_UP)) {
                menuItem--;
                if (menuItem < 0)
                    menuItem = 4;
            }

            if (IsKeyPressed(KEY_DOWN)) {
                menuItem++;
                if (menuItem > 4)
                    menuItem = 0;
            }

            if (menuItem == 0) {
                if (IsKeyPressed(KEY_LEFT) ||
                    IsKeyPressed(KEY_RIGHT) ||
                    IsKeyPressed(KEY_ENTER)) {

                    int direction = 0;

                    if (IsKeyPressed(KEY_LEFT))
                        direction = -1;

                    if (IsKeyPressed(KEY_RIGHT))
                        direction = 1;

                    if (IsKeyPressed(KEY_ENTER))
                        direction = 1;

                    selectedResolution =
                        std::clamp(
                            selectedResolution + direction,
                            0,
                            (int)RESOLUTIONS.size() - 1
                        );

                    ApplyWindowResolution(
                        RESOLUTIONS[selectedResolution],
                        isRecording,
                        videoWriter,
                        notification,
                        notificationTimer,
                        lastWindowW,
                        lastWindowH
                    );
                }
            }

            if (menuItem == 1) {
                if (IsKeyDown(KEY_LEFT))
                    brightness -= 70.0f * dt;

                if (IsKeyDown(KEY_RIGHT))
                    brightness += 70.0f * dt;

                brightness =
                    std::clamp(
                        brightness,
                        -100.0f,
                        100.0f
                    );
            }

            if (menuItem == 2) {
                if (IsKeyDown(KEY_LEFT))
                    contrast -= 0.8f * dt;

                if (IsKeyDown(KEY_RIGHT))
                    contrast += 0.8f * dt;

                contrast =
                    std::clamp(
                        contrast,
                        0.50f,
                        2.50f
                    );
            }

            if (menuItem == 3 && IsKeyPressed(KEY_ENTER)) {
                crtEnabled = !crtEnabled;
            }

            if (menuItem == 4 && IsKeyPressed(KEY_ENTER)) {
                detailBoost = !detailBoost;
            }

            if (IsKeyPressed(KEY_R)) {
                brightness = 0.0f;
                contrast = 1.0f;

                notification =
                    "IMAGE TUNING RESET";

                notificationTimer = 1.5f;
            }
        }

        // ---------------------------------------------------------
        // CAMERA PROCESSING
        // ---------------------------------------------------------

        if (!paused) {
            cap >> frame;

            if (frame.empty()) {
                notification =
                    "CAMERA SIGNAL LOST";
                notificationTimer = 2.0f;

                BeginDrawing();
                ClearBackground(BLACK);
                DrawText(
                    "CAMERA SIGNAL LOST",
                    40,
                    40,
                    30,
                    RED
                );
                EndDrawing();

                continue;
            }

            cv::flip(frame, frame, 1);

            // Force camera image into 16:9 without stretching.
            float sourceAspect =
                (float)frame.cols /
                (float)frame.rows;

            cv::Rect cropRect;

            if (sourceAspect > ASPECT) {
                int cropW =
                    (int)(frame.rows * ASPECT);

                int x =
                    (frame.cols - cropW) / 2;

                cropRect =
                    cv::Rect(
                        x,
                        0,
                        cropW,
                        frame.rows
                    );
            } else {
                int cropH =
                    (int)(frame.cols / ASPECT);

                int y =
                    (frame.rows - cropH) / 2;

                cropRect =
                    cv::Rect(
                        0,
                        y,
                        frame.cols,
                        cropH
                    );
            }

            cropped = frame(cropRect);

            cv::resize(
                cropped,
                smallFrame,
                cv::Size(GRID_W, GRID_H),
                0,
                0,
                cv::INTER_AREA
            );

            // Image tuning.
            smallFrame.convertTo(
                tunedFrame,
                -1,
                contrast,
                brightness
            );

            // Slight intelligent sharpening.
            if (detailBoost) {
                cv::Mat blurColor;

                cv::GaussianBlur(
                    tunedFrame,
                    blurColor,
                    cv::Size(0, 0),
                    1.0
                );

                cv::addWeighted(
                    tunedFrame,
                    1.22,
                    blurColor,
                    -0.22,
                    0,
                    tunedFrame
                );
            }

            cv::cvtColor(
                tunedFrame,
                gray,
                cv::COLOR_BGR2GRAY
            );

            // Noise cleanup before character conversion.
            cv::GaussianBlur(
                gray,
                blurredGray,
                cv::Size(3, 3),
                0.0
            );

            // Temporal smoothing reduces ASCII flicker.
            if (previousGray.empty()) {
                blurredGray.convertTo(
                    smoothGray,
                    CV_32F
                );
            } else {
                cv::Mat currentFloat;

                blurredGray.convertTo(
                    currentFloat,
                    CV_32F
                );

                cv::addWeighted(
                    previousGray,
                    0.52,
                    currentFloat,
                    0.48,
                    0.0,
                    smoothGray
                );
            }

            smoothGray.copyTo(previousGray);

            smoothGray.convertTo(
                gray,
                CV_8U
            );

            // Adaptive local contrast.
            if (detailBoost) {
                clahe->apply(
                    gray,
                    enhancedGray
                );
            } else {
                gray.copyTo(enhancedGray);
            }

            // Edge field.
            cv::Sobel(
                enhancedGray,
                edgeX,
                CV_16S,
                1,
                0,
                3
            );

            cv::Sobel(
                enhancedGray,
                edgeY,
                CV_16S,
                0,
                1,
                3
            );

            cv::convertScaleAbs(
                edgeX,
                absX
            );

            cv::convertScaleAbs(
                edgeY,
                absY
            );

            cv::addWeighted(
                absX,
                0.5,
                absY,
                0.5,
                0,
                edgeMag
            );
        }

        // ---------------------------------------------------------
        // DRAW
        // ---------------------------------------------------------

        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();

        Rectangle viewport =
            AspectViewport(
                screenW,
                screenH
            );

        float cellW =
            viewport.width / GRID_W;

        float cellH =
            viewport.height / GRID_H;

        float fontSize =
            std::max(
                8.0f,
                cellH * 1.32f
            );

        BeginDrawing();

        ClearBackground(
            Color{2, 4, 4, 255}
        );

        // Content letterbox.
        DrawRectangle(
            0,
            0,
            screenW,
            screenH,
            Color{2, 3, 3, 255}
        );

        if (!enhancedGray.empty()) {
            for (int y = 0; y < GRID_H; ++y) {
                int glitchOffset = 0;

                if (mode == MODE_GLITCH) {
                    if (GetRandomValue(0, 100) > 94)
                        glitchOffset =
                            GetRandomValue(-4, 4);

                    if (
                        y % 9 == 0 &&
                        GetRandomValue(0, 100) > 82
                    ) {
                        glitchOffset =
                            GetRandomValue(-10, 10);
                    }
                }

                for (int x = 0; x < GRID_W; ++x) {
                    int sx =
                        std::clamp(
                            x + glitchOffset,
                            0,
                            GRID_W - 1
                        );

                    cv::Vec3b pixel =
                        tunedFrame.at<cv::Vec3b>(
                            y,
                            sx
                        );

                    unsigned char b = pixel[0];
                    unsigned char g = pixel[1];
                    unsigned char r = pixel[2];

                    float luma =
                        enhancedGray.at<unsigned char>(
                            y,
                            sx
                        );

                    // Ordered dithering.
                    float dither =
                        ((float)BAYER4[y & 3][x & 3] -
                         7.5f) *
                        1.7f;

                    float lumaDithered =
                        std::clamp(
                            luma + dither,
                            0.0f,
                            255.0f
                        );

                    int rampIndex =
                        (int)(
                            (lumaDithered / 255.0f) *
                            (ASCII_RAMP[0] ? 67 : 1)
                        );

                    rampIndex =
                        std::clamp(
                            rampIndex,
                            0,
                            (int)std::strlen(ASCII_RAMP) - 1
                        );

                    char displayChar =
                        ASCII_RAMP[rampIndex];

                    Color color = {
                        r,
                        g,
                        b,
                        255
                    };

                    float n =
                        std::clamp(
                            luma / 255.0f,
                            0.0f,
                            1.0f
                        );

                    switch (mode) {
                        case MODE_RGB: {
                            float intensity =
                                0.45f + n * 0.80f;

                            color = {
                                (unsigned char)
                                    std::clamp(
                                        (int)(r * intensity),
                                        0,
                                        255
                                    ),
                                (unsigned char)
                                    std::clamp(
                                        (int)(g * intensity),
                                        0,
                                        255
                                    ),
                                (unsigned char)
                                    std::clamp(
                                        (int)(b * intensity),
                                        0,
                                        255
                                    ),
                                255
                            };
                            break;
                        }

                        case MODE_MATRIX: {
                            color = {
                                20,
                                (unsigned char)
                                    std::clamp(
                                        70.0f + luma * 0.8f,
                                        0.0f,
                                        255.0f
                                    ),
                                30,
                                255
                            };

                            if (n < 0.06f)
                                displayChar = ' ';

                            break;
                        }

                        case MODE_EDGE: {
                            float e =
                                edgeMag.at<unsigned char>(
                                    y,
                                    x
                                ) / 255.0f;

                            if (e > 0.18f) {
                                float gx =
                                    (float)
                                    edgeX.at<short>(
                                        y,
                                        x
                                    );

                                float gy =
                                    (float)
                                    edgeY.at<short>(
                                        y,
                                        x
                                    );

                                float angle =
                                    std::atan2(
                                        gy,
                                        gx
                                    ) *
                                    180.0f /
                                    3.14159265f;

                                if (angle < 0)
                                    angle += 180.0f;

                                if (
                                    angle >= 22.5f &&
                                    angle < 67.5f
                                )
                                    displayChar = '/';
                                else if (
                                    angle >= 67.5f &&
                                    angle < 112.5f
                                )
                                    displayChar = '|';
                                else if (
                                    angle >= 112.5f &&
                                    angle < 157.5f
                                )
                                    displayChar = '\\';
                                else
                                    displayChar = '-';

                                color = {
                                    20,
                                    255,
                                    255,
                                    255
                                };
                            } else {
                                displayChar = ' ';
                            }

                            break;
                        }

                        case MODE_CYBERPUNK: {
                            if (n > 0.82f)
                                color = {
                                    255,
                                    255,
                                    170,
                                    255
                                };
                            else if (n > 0.58f)
                                color = {
                                    255,
                                    40,
                                    160,
                                    255
                                };
                            else if (n > 0.28f)
                                color = {
                                    0,
                                    230,
                                    255,
                                    255
                                };
                            else
                                color = {
                                    50,
                                    15,
                                    70,
                                    255
                                };

                            break;
                        }

                        case MODE_AMBER: {
                            color = {
                                (unsigned char)
                                    std::clamp(
                                        100.0f + luma,
                                        0.0f,
                                        255.0f
                                    ),
                                (unsigned char)
                                    std::clamp(
                                        luma * 0.57f,
                                        0.0f,
                                        180.0f
                                    ),
                                10,
                                255
                            };

                            break;
                        }

                        case MODE_GAMEBOY: {
                            if (luma > 190.0f) {
                                displayChar = '#';
                                color = {
                                    180,
                                    210,
                                    40,
                                    255
                                };
                            } else if (luma > 120.0f) {
                                displayChar = 'x';
                                color = {
                                    130,
                                    175,
                                    25,
                                    255
                                };
                            } else if (luma > 55.0f) {
                                displayChar = ':';
                                color = {
                                    70,
                                    115,
                                    50,
                                    255
                                };
                            } else {
                                displayChar = ' ';
                                color = {
                                    20,
                                    60,
                                    25,
                                    255
                                };
                            }

                            break;
                        }

                        case MODE_THERMAL:
                            color =
                                ThermalColor(n);
                            break;

                        case MODE_BINARY: {
                            if (luma < 25.0f) {
                                displayChar = ' ';
                            } else {
                                displayChar =
                                    luma > 128.0f
                                        ? '1'
                                        : '0';
                            }

                            color = {
                                20,
                                (unsigned char)
                                    std::clamp(
                                        80.0f + luma,
                                        0.0f,
                                        255.0f
                                    ),
                                40,
                                255
                            };

                            break;
                        }

                        case MODE_GLITCH: {
                            color = {
                                20,
                                (unsigned char)
                                    std::clamp(
                                        50.0f + luma,
                                        0.0f,
                                        255.0f
                                    ),
                                40,
                                255
                            };

                            if (
                                GetRandomValue(
                                    0,
                                    100
                                ) > 97
                            ) {
                                color = {
                                    255,
                                    20,
                                    80,
                                    255
                                };
                            }

                            break;
                        }
                    }

                    if (displayChar != ' ') {
                        charGrid[y][x] =
                            displayChar;

                        char text[2] = {
                            displayChar,
                            '\0'
                        };

                        float px =
                            viewport.x +
                            x * cellW;

                        float py =
                            viewport.y +
                            y * cellH;

                        DrawTextEx(
                            font,
                            text,
                            {px, py},
                            fontSize,
                            0.0f,
                            color
                        );
                    } else {
                        charGrid[y][x] = ' ';
                    }
                }
            }
        }

        // ---------------------------------------------------------
        // CRT
        // ---------------------------------------------------------

        if (crtEnabled) {
            for (
                int y = (int)viewport.y;
                y < viewport.y + viewport.height;
                y += 4
            ) {
                DrawRectangle(
                    (int)viewport.x,
                    y,
                    (int)viewport.width,
                    1,
                    Color{0, 0, 0, 80}
                );
            }

            float scanY =
                fmodf(
                    timeNow * 240.0f,
                    viewport.height + 120.0f
                ) - 60.0f;

            DrawRectangle(
                (int)viewport.x,
                (int)(viewport.y + scanY),
                (int)viewport.width,
                18,
                Color{255, 255, 255, 10}
            );

            // Vignette.
            DrawRectangleGradientV(
                (int)viewport.x,
                (int)viewport.y,
                (int)viewport.width,
                80,
                Color{0, 0, 0, 180},
                Color{0, 0, 0, 0}
            );

            DrawRectangleGradientV(
                (int)viewport.x,
                (int)(viewport.y +
                    viewport.height - 80),
                (int)viewport.width,
                80,
                Color{0, 0, 0, 0},
                Color{0, 0, 0, 190}
            );

            DrawRectangleGradientH(
                (int)viewport.x,
                (int)viewport.y,
                70,
                (int)viewport.height,
                Color{0, 0, 0, 170},
                Color{0, 0, 0, 0}
            );

            DrawRectangleGradientH(
                (int)(
                    viewport.x +
                    viewport.width - 70
                ),
                (int)viewport.y,
                70,
                (int)viewport.height,
                Color{0, 0, 0, 0},
                Color{0, 0, 0, 170}
            );
        }

        // ---------------------------------------------------------
        // HUD
        // ---------------------------------------------------------

        if (hudEnabled) {
            int barH = 42;

            DrawRectangle(
                (int)viewport.x,
                (int)viewport.y,
                (int)viewport.width,
                barH,
                Color{3, 12, 8, 225}
            );

            DrawRectangle(
                (int)viewport.x,
                (int)(viewport.y + barH - 1),
                (int)viewport.width,
                1,
                Color{60, 255, 130, 130}
            );

            std::string title =
                "ASCII OPTIC // " +
                std::string(MODE_NAMES[(int)mode]);

            DrawTextEx(
                font,
                title.c_str(),
                {
                    viewport.x + 16,
                    viewport.y + 11
                },
                18,
                1,
                Color{90, 255, 150, 255}
            );

            std::string tuning =
                "BRT " +
                FormatFloat(brightness, 0) +
                "   CNT " +
                FormatFloat(contrast, 2);

            float tuningWidth =
                MeasureTextEx(
                    font,
                    tuning.c_str(),
                    16,
                    1
                ).x;

            DrawTextEx(
                font,
                tuning.c_str(),
                {
                    viewport.x +
                        viewport.width -
                        tuningWidth -
                        150,
                    viewport.y + 12
                },
                16,
                1,
                Color{100, 180, 125, 255}
            );

            std::string fps =
                "FPS " +
                FormatFloat(
                    actualFPS,
                    0
                );

            DrawTextEx(
                font,
                fps.c_str(),
                {
                    viewport.x +
                        viewport.width -
                        125,
                    viewport.y + 12
                },
                16,
                1,
                Color{130, 255, 160, 255}
            );

            int bottomH = 34;

            DrawRectangle(
                (int)viewport.x,
                (int)(
                    viewport.y +
                    viewport.height -
                    bottomH
                ),
                (int)viewport.width,
                bottomH,
                Color{3, 12, 8, 225}
            );

            std::string controls =
                "1-9 MODE | SPACE NEXT | P PAUSE | "
                "C CRT | H HUD | Q DETAIL | "
                "S SNAP | R REC";

            DrawTextEx(
                font,
                controls.c_str(),
                {
                    viewport.x + 14,
                    viewport.y +
                        viewport.height -
                        23
                },
                14,
                1,
                Color{80, 160, 110, 255}
            );

            if (isRecording) {
                DrawCircle(
                    (int)(
                        viewport.x +
                        viewport.width -
                        88
                    ),
                    (int)(
                        viewport.y +
                        viewport.height -
                        18
                    ),
                    5,
                    RED
                );

                DrawTextEx(
                    font,
                    "REC",
                    {
                        viewport.x +
                            viewport.width -
                            76,
                        viewport.y +
                            viewport.height -
                            26
                    },
                    16,
                    1,
                    RED
                );
            }
        }

        // ---------------------------------------------------------
        // NOTIFICATION
        // ---------------------------------------------------------

        if (
            notificationTimer > 0.0f &&
            !paused
        ) {
            float fade =
                std::clamp(
                    notificationTimer,
                    0.0f,
                    1.0f
                );

            Rectangle nr = {
                viewport.x + 18,
                viewport.y +
                    viewport.height - 76,
                std::min(
                    viewport.width - 36.0f,
                    520.0f
                ),
                34
            };

            DrawRectangleRounded(
                nr,
                0.25f,
                8,
                WithAlpha(
                    Color{3, 30, 15, 240},
                    (unsigned char)(
                        220.0f * fade +
                        35.0f
                    )
                )
            );

            DrawRectangleLinesEx(
                nr,
                1,
                WithAlpha(
                    GREEN,
                    (unsigned char)(
                        255.0f * fade
                    )
                )
            );

            DrawTextEx(
                font,
                notification.c_str(),
                {
                    nr.x + 12,
                    nr.y + 8
                },
                15,
                1,
                WithAlpha(
                    Color{110, 255, 155, 255},
                    (unsigned char)(
                        255.0f * fade
                    )
                )
            );
        }

        // ---------------------------------------------------------
        // PAUSE MENU
        // ---------------------------------------------------------

        if (paused) {
            DrawRectangle(
                0,
                0,
                screenW,
                screenH,
                Color{0, 0, 0, 135}
            );

            float panelW =
                std::min(
                    720.0f,
                    screenW * 0.78f
                );

            float panelH =
                std::min(
                    590.0f,
                    screenH * 0.82f
                );

            Rectangle panel = {
                (screenW - panelW) * 0.5f,
                (screenH - panelH) * 0.5f,
                panelW,
                panelH
            };

            DrawPanel(panel);

            DrawTextEx(
                font,
                "SYSTEM PAUSED",
                {
                    panel.x + 32,
                    panel.y + 28
                },
                28,
                1,
                Color{110, 255, 155, 255}
            );

            DrawTextEx(
                font,
                "TACTICAL OPTIC CONTROL",
                {
                    panel.x + 34,
                    panel.y + 62
                },
                14,
                1,
                Color{80, 150, 105, 255}
            );

            const float left = panel.x + 36;
            const float width = panel.width - 72;

            float y = panel.y + 118;

            // Resolution.
            bool selected = menuItem == 0;

            DrawTextEx(
                font,
                "WINDOW RESOLUTION",
                {left, y},
                18,
                1,
                selected
                    ? Color{120, 255, 170, 255}
                    : Color{175, 195, 180, 255}
            );

            std::string resolutionText =
                RESOLUTIONS[selectedResolution].name;

            DrawRectangleRounded(
                {
                    left,
                    y + 30,
                    width,
                    44
                },
                0.18f,
                8,
                selected
                    ? Color{20, 65, 40, 255}
                    : Color{12, 28, 20, 255}
            );

            DrawTextEx(
                font,
                resolutionText.c_str(),
                {
                    left + 16,
                    y + 42
                },
                18,
                1,
                Color{210, 255, 220, 255}
            );

            DrawTextEx(
                font,
                "LEFT / RIGHT",
                {
                    left + width - 140,
                    y + 42
                },
                13,
                1,
                Color{80, 180, 110, 255}
            );

            y += 105;

            // Brightness.
            float brightness01 =
                (brightness + 100.0f) /
                200.0f;

            DrawSlider(
                {
                    left,
                    y + 27,
                    width,
                    14
                },
                brightness01,
                "BRIGHTNESS",
                FormatFloat(
                    brightness,
                    0
                ),
                font
            );

            y += 90;

            // Contrast.
            float contrast01 =
                (contrast - 0.5f) /
                2.0f;

            DrawSlider(
                {
                    left,
                    y + 27,
                    width,
                    14
                },
                contrast01,
                "CONTRAST",
                FormatFloat(
                    contrast,
                    2
                ),
                font
            );

            y += 90;

            // CRT.
            selected = menuItem == 3;

            DrawTextEx(
                font,
                "CRT SCANLINES",
                {left, y},
                18,
                1,
                selected
                    ? Color{120, 255, 170, 255}
                    : Color{175, 195, 180, 255}
            );

            std::string crtState =
                crtEnabled ? "ON" : "OFF";

            DrawTextEx(
                font,
                crtState.c_str(),
                {
                    left + width - 50,
                    y
                },
                18,
                1,
                crtEnabled
                    ? Color{120, 255, 170, 255}
                    : Color{255, 100, 100, 255}
            );

            y += 58;

            // Detail.
            selected = menuItem == 4;

            DrawTextEx(
                font,
                "DETAIL ENHANCEMENT",
                {left, y},
                18,
                1,
                selected
                    ? Color{120, 255, 170, 255}
                    : Color{175, 195, 180, 255}
            );

            std::string detailState =
                detailBoost ? "ON" : "OFF";

            DrawTextEx(
                font,
                detailState.c_str(),
                {
                    left + width - 50,
                    y
                },
                18,
                1,
                detailBoost
                    ? Color{120, 255, 170, 255}
                    : Color{255, 100, 100, 255}
            );

            DrawRectangle(
                (int)panel.x,
                (int)(
                    panel.y +
                    panel.height -
                    74
                ),
                (int)panel.width,
                1,
                Color{45, 120, 75, 140}
            );

            DrawTextEx(
                font,
                "UP / DOWN SELECT   LEFT / RIGHT ADJUST   ENTER TO TOGGLE",
                {
                    panel.x + 30,
                    panel.y +
                        panel.height -
                        55
                },
                13,
                1,
                Color{95, 165, 110, 255}
            );

            DrawTextEx(
                font,
                "P / ESC RESUME    R RESET IMAGE TUNING",
                {
                    panel.x + 30,
                    panel.y +
                        panel.height -
                        32
                },
                13,
                1,
                Color{95, 165, 110, 255}
            );

            // Selection indicator.
            float markerY;

            if (menuItem == 0)
                markerY = panel.y + 127;
            else if (menuItem == 1)
                markerY = panel.y + 232;
            else if (menuItem == 2)
                markerY = panel.y + 322;
            else if (menuItem == 3)
                markerY = panel.y + 410;
            else
                markerY = panel.y + 468;

            DrawRectangle(
                (int)(left - 12),
                (int)markerY,
                4,
                24,
                Color{70, 255, 130, 255}
            );
        }

        EndDrawing();

        // ---------------------------------------------------------
        // SNAPSHOT
        // ---------------------------------------------------------

        if (triggerSnapshot) {
            int index = 1;

            while (
                fs::exists(
                    "media/ascii_snap_" +
                    std::to_string(index) +
                    ".png"
                ) ||
                fs::exists(
                    "media/ascii_snap_" +
                    std::to_string(index) +
                    ".txt"
                )
            ) {
                index++;
            }

            std::string base =
                "media/ascii_snap_" +
                std::to_string(index);

            Image image =
                LoadImageFromScreen();

            ExportImage(
                image,
                (base + ".png").c_str()
            );

            UnloadImage(image);

            std::ofstream txt(
                base + ".txt"
            );

            if (txt.is_open()) {
                for (int y = 0; y < GRID_H; ++y) {
                    for (int x = 0; x < GRID_W; ++x) {
                        txt << charGrid[y][x];
                    }
                    txt << '\n';
                }
            }

            lastSnapshotName =
                base;

            notification =
                "SNAPSHOT SAVED // " +
                base;

            notificationTimer = 3.0f;

            triggerSnapshot = false;
        }

        // ---------------------------------------------------------
        // VIDEO RECORDING
        // ---------------------------------------------------------

        if (
            isRecording &&
            videoWriter.isOpened()
        ) {
            frameAccumulator += dt;

            const float recordInterval =
                1.0f / 30.0f;

            if (frameAccumulator >= recordInterval) {
                frameAccumulator -=
                    recordInterval;

                Image screenImage =
                    LoadImageFromScreen();

                if (
                    screenImage.data &&
                    screenImage.width > 0 &&
                    screenImage.height > 0
                ) {
                    cv::Mat rgba(
                        screenImage.height,
                        screenImage.width,
                        CV_8UC4,
                        screenImage.data
                    );

                    cv::Mat bgr;

                    cv::cvtColor(
                        rgba,
                        bgr,
                        cv::COLOR_RGBA2BGR
                    );

                    videoWriter.write(bgr);
                }

                UnloadImage(screenImage);
            }
        }
    }

    if (isRecording && videoWriter.isOpened())
        videoWriter.release();

    cap.release();

    CloseWindow();

    return 0;
}

/*
MSYS2 / MinGW64 COMPILE:

g++ webcam_ascii.cpp -o webcam_ascii.exe -std=c++17 -I/mingw64/include/opencv4 -lraylib -lopencv_core -lopencv_videoio -lopencv_imgproc -lopengl32 -lgdi32 -lwinmm

RUN:

./webcam_ascii.exe
*/
#include "raylib.h"
#include <opencv2/opencv.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr int WINDOW_W = 1280;
constexpr int WINDOW_H = 720;
constexpr int MIN_WINDOW_W = 640;
constexpr int MIN_WINDOW_H = 360;
constexpr int MAX_FONT_SIZE = 24;
constexpr int MIN_FONT_SIZE = 8;
constexpr int RECORD_FPS = 30;
constexpr float ASPECT = 16.0f / 9.0f;

const char* ASCII_RAMP = " .:-=+*#%@";

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
    int width;
    int height;
    const char* label;
};

const Resolution RESOLUTIONS[] = {
    {960, 540, "960 x 540"},
    {1280, 720, "1280 x 720"},
    {1600, 900, "1600 x 900"},
    {1920, 1080, "1920 x 1080"}
};

constexpr int RESOLUTION_COUNT = static_cast<int>(sizeof(RESOLUTIONS) / sizeof(RESOLUTIONS[0]));

struct AppState {
    int fontSize = 14;
    int gridW = 0;
    int gridH = 0;
    int cellW = 0;
    int cellH = 0;

    double contrast = 1.0;
    int brightness = 0;

    bool crtScanlines = true;
    bool hideHUD = false;
    bool detailEnhancement = true;
    bool paused = false;

    int resolutionIndex = 1;
    int menuSelection = 0;

    RenderMode mode = MODE_CYBERPUNK;

    bool isRecording = false;
    float recordAccumulator = 0.0f;

    float notificationTimer = 0.0f;
    std::string notification = "SYSTEM BOOT // OPTIC ENGINE READY";

    std::string baseDirectory;
    fs::path mediaDirectory;

    cv::VideoWriter videoWriter;
    cv::Ptr<cv::CLAHE> clahe;
    int recordWidth = 0;
    int recordHeight = 0;

    cv::Mat frame;
    cv::Mat sourceCrop;
    cv::Mat smallFrame;
    cv::Mat tunedFrame;
    cv::Mat previousTunedFrame;
    cv::Mat gray;
    cv::Mat claheGray;
    cv::Mat gradX;
    cv::Mat gradY;
    cv::Mat absGradX;
    cv::Mat absGradY;
    cv::Mat edgeMag;

    std::vector<std::string> charGrid;

    Font asciiFont{};
    bool customFontLoaded = false;
};

void Notify(AppState& app, const std::string& message, float seconds = 2.5f) {
    app.notification = message;
    app.notificationTimer = seconds;
}

std::string FormatFloat(double value, int precision = 2) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    return oss.str();
}

std::string ModeName(RenderMode mode) {
    switch (mode) {
        case MODE_RGB: return "RAW_RGB";
        case MODE_MATRIX: return "MATRIX_LINK";
        case MODE_EDGE: return "GEOMETRY";
        case MODE_CYBERPUNK: return "NEON_PUNK";
        case MODE_AMBER: return "TERM_AMBER";
        case MODE_GAMEBOY: return "RETRO_8BIT";
        case MODE_THERMAL: return "THERMAL_IR";
        case MODE_BINARY: return "RAW_BINARY";
        case MODE_GLITCH: return "SIGNAL_LOST";
        default: return "UNKNOWN";
    }
}

cv::Mat CropTo16x9(const cv::Mat& input) {
    if (input.empty()) return cv::Mat();

    const double inputAspect = static_cast<double>(input.cols) / static_cast<double>(input.rows);

    if (std::abs(inputAspect - ASPECT) < 0.01) {
        return input.clone();
    }

    cv::Rect roi;

    if (inputAspect > ASPECT) {
        const int newWidth = static_cast<int>(std::round(input.rows * ASPECT));
        const int x = (input.cols - newWidth) / 2;
        roi = cv::Rect(std::max(0, x), 0, std::min(newWidth, input.cols), input.rows);
    } else {
        const int newHeight = static_cast<int>(std::round(input.cols / ASPECT));
        const int y = (input.rows - newHeight) / 2;
        roi = cv::Rect(0, std::max(0, y), input.cols, std::min(newHeight, input.rows));
    }

    return input(roi).clone();
}

void RebuildGridLayout(AppState& app) {
    const int screenW = std::max(1, GetScreenWidth());
    const int screenH = std::max(1, GetScreenHeight());

    const float measuredWidth = MeasureTextEx(
        app.asciiFont,
        "M",
        static_cast<float>(app.fontSize),
        0.0f
    ).x;

    const int newCellH = std::max(1, app.fontSize);
    const int newCellW = std::max(1, static_cast<int>(std::ceil(measuredWidth)));
    const int newGridW = std::max(1, screenW / newCellW);
    const int newGridH = std::max(1, screenH / newCellH);

    if (app.cellW == newCellW && app.cellH == newCellH &&
        app.gridW == newGridW && app.gridH == newGridH &&
        !app.charGrid.empty()) {
        return;
    }

    app.cellW = newCellW;
    app.cellH = newCellH;
    app.gridW = newGridW;
    app.gridH = newGridH;
    app.charGrid.assign(app.gridH, std::string(app.gridW, ' '));
}


void MaintainWindowAspect(AppState& app) {
    static bool correcting = false;
    static int lastW = -1;
    static int lastH = -1;

    const int width = GetScreenWidth();
    const int height = GetScreenHeight();

    if (lastW < 0) {
        lastW = width;
        lastH = height;
        return;
    }

    if (!IsWindowResized() || correcting) {
        lastW = width;
        lastH = height;
        return;
    }

    if (width == lastW && height == lastH) return;

    correcting = true;

    const int dw = width - lastW;
    const int dh = height - lastH;

    if (std::abs(dw) >= std::abs(dh)) {
        const int newH = static_cast<int>(std::round(width / ASPECT));
        SetWindowSize(std::max(MIN_WINDOW_W, width), std::max(MIN_WINDOW_H, newH));
    } else {
        const int newW = static_cast<int>(std::round(height * ASPECT));
        SetWindowSize(std::max(MIN_WINDOW_W, newW), std::max(MIN_WINDOW_H, height));
    }

    correcting = false;
    lastW = GetScreenWidth();
    lastH = GetScreenHeight();

    RebuildGridLayout(app);
}

void StopRecording(AppState& app, const std::string& reason = "RECORDING STOPPED") {
    if (app.videoWriter.isOpened()) {
        app.videoWriter.release();
    }

    app.isRecording = false;
    app.recordAccumulator = 0.0f;
    app.recordWidth = 0;
    app.recordHeight = 0;

    Notify(app, reason);
}

void ApplyResolution(AppState& app, int index) {
    index = std::clamp(index, 0, RESOLUTION_COUNT - 1);
    if (index == app.resolutionIndex &&
        GetScreenWidth() == RESOLUTIONS[index].width &&
        GetScreenHeight() == RESOLUTIONS[index].height) {
        return;
    }

    if (app.isRecording) {
        StopRecording(app, "RECORDING STOPPED // WINDOW RESIZED");
    }

    app.resolutionIndex = index;
    SetWindowSize(RESOLUTIONS[index].width, RESOLUTIONS[index].height);
    RebuildGridLayout(app);
    Notify(app, std::string("RESOLUTION // ") + RESOLUTIONS[index].label);
}

void UpdateResolutionIndexFromWindow(AppState& app) {
    const int w = GetScreenWidth();
    const int h = GetScreenHeight();

    int bestIndex = app.resolutionIndex;
    int bestDistance = 1'000'000'000;

    for (int i = 0; i < RESOLUTION_COUNT; ++i) {
        const int d = std::abs(w - RESOLUTIONS[i].width) + std::abs(h - RESOLUTIONS[i].height);
        if (d < bestDistance) {
            bestDistance = d;
            bestIndex = i;
        }
    }

    app.resolutionIndex = bestIndex;
}

void ApplyImageQuality(AppState& app) {
    if (app.frame.empty()) return;

    app.sourceCrop = CropTo16x9(app.frame);
    if (app.sourceCrop.empty()) return;

    cv::resize(
        app.sourceCrop,
        app.smallFrame,
        cv::Size(app.gridW, app.gridH),
        0.0,
        0.0,
        cv::INTER_AREA
    );

    app.smallFrame.convertTo(app.tunedFrame, -1, app.contrast, app.brightness);

    if (app.detailEnhancement) {
        cv::Mat blurred;
        cv::GaussianBlur(app.tunedFrame, blurred, cv::Size(0, 0), 1.0);
        cv::addWeighted(app.tunedFrame, 1.30, blurred, -0.30, 0.0, app.tunedFrame);
    }

    cv::cvtColor(app.tunedFrame, app.gray, cv::COLOR_BGR2GRAY);

    if (!app.clahe) {
        app.clahe = cv::createCLAHE(2.0, cv::Size(4, 4));
    }
    app.clahe->apply(app.gray, app.claheGray);

    if (!app.previousTunedFrame.empty() &&
        app.previousTunedFrame.size() == app.tunedFrame.size()) {
        cv::addWeighted(app.tunedFrame, 0.90, app.previousTunedFrame, 0.10, 0.0, app.tunedFrame);
        cv::cvtColor(app.tunedFrame, app.gray, cv::COLOR_BGR2GRAY);
        app.clahe->apply(app.gray, app.claheGray);
    }

    app.previousTunedFrame = app.tunedFrame.clone();

    if (app.mode == MODE_EDGE) {
        cv::Sobel(app.claheGray, app.gradX, CV_16S, 1, 0, 3);
        cv::Sobel(app.claheGray, app.gradY, CV_16S, 0, 1, 3);
        cv::convertScaleAbs(app.gradX, app.absGradX);
        cv::convertScaleAbs(app.gradY, app.absGradY);
        cv::addWeighted(app.absGradX, 0.5, app.absGradY, 0.5, 0.0, app.edgeMag);
    }
}

int LumaToCharIndex(float luma, int x, int y) {
    static const int bayer4[4][4] = {
        {0, 8, 2, 10},
        {12, 4, 14, 6},
        {3, 11, 1, 9},
        {15, 7, 13, 5}
    };

    const float dither = (static_cast<float>(bayer4[y & 3][x & 3]) / 15.0f - 0.5f) * 12.0f;
    const float adjusted = std::clamp(luma + dither, 0.0f, 255.0f);
    constexpr int rampCount = static_cast<int>(sizeof(" .:-=+*#%@") - 1);

    return std::clamp(
        static_cast<int>(std::round((adjusted / 255.0f) * static_cast<float>(rampCount - 1))),
        0,
        rampCount - 1
    );
}

void RenderAscii(AppState& app) {
    if (app.tunedFrame.empty()) return;

    const int screenW = GetScreenWidth();
    const int screenH = GetScreenHeight();

    const int contentW = app.gridW * app.cellW;
    const int contentH = app.gridH * app.cellH;
    const int offsetX = std::max(0, (screenW - contentW) / 2);
    const int offsetY = std::max(0, (screenH - contentH) / 2);

    const float timeVal = static_cast<float>(GetTime());

    for (int y = 0; y < app.gridH; ++y) {
        int rowOffset = 0;

        if (app.mode == MODE_GLITCH) {
            if (GetRandomValue(0, 100) > 95) {
                rowOffset = GetRandomValue(-3, 3);
            }
            if ((y % 11) == 0 && GetRandomValue(0, 100) > 82) {
                rowOffset = GetRandomValue(-8, 8);
            }
        }

        for (int x = 0; x < app.gridW; ++x) {
            const int sampleX = std::clamp(x + rowOffset, 0, app.gridW - 1);
            const cv::Vec3b pixel = app.tunedFrame.at<cv::Vec3b>(y, sampleX);

            int b = pixel[0];
            int g = pixel[1];
            int r = pixel[2];

            if (app.mode == MODE_GLITCH && rowOffset != 0) {
                const int redSampleX = std::clamp(x + rowOffset + 3, 0, app.gridW - 1);
                r = app.tunedFrame.at<cv::Vec3b>(y, redSampleX)[2];
            }

            const float luma = std::clamp(
                0.299f * static_cast<float>(r) +
                0.587f * static_cast<float>(g) +
                0.114f * static_cast<float>(b),
                0.0f,
                255.0f
            );

            char displayChar = ASCII_RAMP[LumaToCharIndex(luma, x, y)];
            Color charColor = {
                static_cast<unsigned char>(r),
                static_cast<unsigned char>(g),
                static_cast<unsigned char>(b),
                255
            };

            switch (app.mode) {
                case MODE_MATRIX:
                case MODE_GLITCH: {
                    const int pulse = static_cast<int>(30.0f + 20.0f * std::sin(timeVal * 2.0f + y * 0.07f));
                    charColor = {
                        20,
                        static_cast<unsigned char>(std::clamp(static_cast<int>(luma) + 70 + pulse, 0, 255)),
                        35,
                        255
                    };
                    break;
                }

                case MODE_BINARY:
                    displayChar = (luma > 112.0f) ? '1' : '0';
                    if (luma < 24.0f) displayChar = ' ';
                    charColor = {
                        0,
                        static_cast<unsigned char>(std::max(100.0f, luma)),
                        10,
                        255
                    };
                    break;

                case MODE_THERMAL:
                    if (luma < 55.0f) {
                        charColor = {0, 20, static_cast<unsigned char>(std::clamp(luma * 4.0f, 0.0f, 255.0f)), 255};
                    } else if (luma < 125.0f) {
                        charColor = {
                            static_cast<unsigned char>(std::clamp((luma - 55.0f) * 3.6f, 0.0f, 255.0f)),
                            15,
                            static_cast<unsigned char>(std::clamp(255.0f - (luma - 55.0f) * 2.7f, 0.0f, 255.0f)),
                            255
                        };
                    } else if (luma < 205.0f) {
                        charColor = {
                            255,
                            static_cast<unsigned char>(std::clamp((luma - 125.0f) * 3.0f, 0.0f, 255.0f)),
                            0,
                            255
                        };
                    } else {
                        charColor = {255, 255, 200, 255};
                    }
                    break;

                case MODE_AMBER:
                    charColor = {
                        static_cast<unsigned char>(std::clamp(luma + 95.0f, 0.0f, 255.0f)),
                        static_cast<unsigned char>(std::clamp(luma * 0.62f, 0.0f, 255.0f)),
                        0,
                        255
                    };
                    break;

                case MODE_GAMEBOY:
                    if (luma > 190.0f) {
                        charColor = {155, 188, 15, 255};
                        displayChar = '#';
                    } else if (luma > 120.0f) {
                        charColor = {139, 172, 15, 255};
                        displayChar = 'x';
                    } else if (luma > 60.0f) {
                        charColor = {48, 98, 48, 255};
                        displayChar = ':';
                    } else {
                        charColor = {15, 56, 15, 255};
                        displayChar = ' ';
                    }
                    break;

                case MODE_EDGE: {
                    if (app.edgeMag.at<unsigned char>(y, x) > 38) {
                        float angle = std::atan2(
                            static_cast<float>(app.gradY.at<short>(y, x)),
                            static_cast<float>(app.gradX.at<short>(y, x))
                        ) * (180.0f / 3.14159265359f);

                        if (angle < 0.0f) angle += 180.0f;

                        if (angle >= 22.5f && angle < 67.5f) displayChar = '/';
                        else if (angle >= 67.5f && angle < 112.5f) displayChar = '|';
                        else if (angle >= 112.5f && angle < 157.5f) displayChar = '\\';
                        else displayChar = '-';

                        charColor = {0, 255, 255, 255};
                    } else {
                        displayChar = ' ';
                        charColor = {0, 30, 30, 255};
                    }
                    break;
                }

                case MODE_CYBERPUNK:
                    if (luma > 190.0f) charColor = {255, 255, 120, 255};
                    else if (luma > 120.0f) charColor = {255, 25, 165, 255};
                    else if (luma > 50.0f) charColor = {0, 225, 255, 255};
                    else charColor = {35, 10, 55, 255};
                    break;

                case MODE_RGB:
                default:
                    break;
            }

            if (app.mode == MODE_GLITCH && GetRandomValue(0, 100) > 99) {
                displayChar = static_cast<char>(GetRandomValue(33, 126));
            }

            app.charGrid[y][x] = displayChar;

            char glyph[2] = {displayChar, '\0'};
            const float drawX = static_cast<float>(offsetX + x * app.cellW);
            const float drawY = static_cast<float>(offsetY + y * app.cellH);

            DrawTextEx(
                app.asciiFont,
                glyph,
                Vector2{drawX, drawY},
                static_cast<float>(app.fontSize),
                0.0f,
                charColor
            );
        }
    }
}

void DrawCRT(const AppState& app) {
    if (!app.crtScanlines) return;

    const int screenW = GetScreenWidth();
    const int screenH = GetScreenHeight();
    const float timeVal = static_cast<float>(GetTime());

    for (int y = 0; y < screenH; y += 3) {
        DrawRectangle(0, y, screenW, 1, Color{0, 0, 0, 135});
    }

    float scanY = std::fmod(timeVal * 250.0f, static_cast<float>(screenH) + 100.0f) - 50.0f;
    DrawRectangle(0, static_cast<int>(scanY), screenW, 18, Color{255, 255, 255, 10});
    DrawRectangle(0, static_cast<int>(scanY + 16.0f), screenW, 2, Color{255, 255, 255, 22});

    DrawRectangleGradientV(0, 0, screenW, 60, Color{0, 0, 0, 165}, Color{0, 0, 0, 0});
    DrawRectangleGradientV(0, screenH - 60, screenW, 60, Color{0, 0, 0, 0}, Color{0, 0, 0, 195});
    DrawRectangleGradientH(0, 0, 60, screenH, Color{0, 0, 0, 165}, Color{0, 0, 0, 0});
    DrawRectangleGradientH(screenW - 60, 0, 60, screenH, Color{0, 0, 0, 0}, Color{0, 0, 0, 165});
}

void DrawHUD(const AppState& app) {
    if (app.hideHUD) return;

    const int screenW = GetScreenWidth();
    const int screenH = GetScreenHeight();
    const float timeVal = static_cast<float>(GetTime());

    DrawRectangle(0, 0, screenW, 38, Color{0, 10, 5, 225});
    DrawRectangle(0, 38, screenW, 1, Color{0, 255, 100, 70});

    std::string modeText = std::string("OPTIC_MODE // ") + ModeName(app.mode);
    DrawText(modeText.c_str(), 15, 10, 16, GREEN);

    std::string tuning =
        "CNT " + FormatFloat(app.contrast, 2) +
        " | BRT " + std::to_string(app.brightness) +
        " | FONT " + std::to_string(app.fontSize);
    DrawText(tuning.c_str(), 250, 12, 12, DARKGREEN);

    DrawFPS(std::max(0, screenW - 82), 11);

    DrawRectangle(0, screenH - 27, screenW, 27, Color{0, 10, 5, 225});
    DrawRectangle(0, screenH - 28, screenW, 1, Color{0, 255, 100, 70});

    const char* controls = "1-9 FILTERS | SPACE NEXT | P/ESC PAUSE | C CRT | H HUD | S SNAP | R REC";
    DrawText(controls, 15, screenH - 19, 12, DARKGREEN);

    if (app.isRecording) {
        if ((static_cast<int>(timeVal * 2.0f) & 1) == 0) {
            DrawCircle(screenW - 115, 18, 6, RED);
        }
        DrawText("REC MP4", screenW - 100, 11, 14, RED);
    }

    if (app.notificationTimer > 0.0f) {
        const int boxW = std::min(520, screenW - 40);
        DrawRectangle(20, screenH - 78, boxW, 34, Color{0, 25, 10, 232});
        DrawRectangleLines(20, screenH - 78, boxW, 34, GREEN);
        DrawText(app.notification.c_str(), 30, screenH - 69, 13, GREEN);
    }
}

void DrawSlider(int x, int y, int width, float normalized, const char* label, const std::string& value) {
    DrawText(label, x, y - 24, 18, RAYWHITE);

    DrawRectangle(x, y, width, 10, Color{35, 40, 45, 255});
    DrawRectangle(x, y, static_cast<int>(width * std::clamp(normalized, 0.0f, 1.0f)), 10, GREEN);
    DrawRectangleLines(x, y, width, 10, Color{110, 120, 125, 255});

    DrawText(value.c_str(), x + width + 18, y - 6, 18, RAYWHITE);
}

void DrawPauseMenu(const AppState& app) {
    const int screenW = GetScreenWidth();
    const int screenH = GetScreenHeight();

    DrawRectangle(0, 0, screenW, screenH, Color{0, 0, 0, 125});

    const int panelW = std::min(820, screenW - 40);
    const int panelH = std::min(570, screenH - 40);
    const int panelX = (screenW - panelW) / 2;
    const int panelY = (screenH - panelH) / 2;

    DrawRectangle(panelX, panelY, panelW, panelH, Color{5, 10, 12, 245});
    DrawRectangleLines(panelX, panelY, panelW, panelH, GREEN);
    DrawRectangle(panelX, panelY, panelW, 46, Color{0, 25, 15, 255});

    DrawText("TACTICAL OPTIC // PAUSED", panelX + 24, panelY + 13, 20, GREEN);
    DrawText("UP/DOWN SELECT   LEFT/RIGHT ADJUST   P/ESC RESUME", panelX + 24, panelY + 55, 13, DARKGREEN);

    const int x = panelX + 44;
    const int labelW = panelW - 88;
    const int y = panelY + 75;
    const int rowGap = std::clamp((panelH - 115) / 6, 33, 68);

    const char* labels[] = {
        "RESOLUTION",
        "FONT SIZE",
        "BRIGHTNESS",
        "CONTRAST",
        "DETAIL ENHANCEMENT",
        "CRT SCANLINES",
        "RESUME"
    };

    for (int i = 0; i < 7; ++i) {
        const bool selected = (i == app.menuSelection);
        const int rowY = y + i * rowGap;

        if (selected) {
            DrawRectangle(panelX + 22, rowY - 8, panelW - 44, 51, Color{0, 55, 32, 115});
            DrawRectangleLines(panelX + 22, rowY - 8, panelW - 44, 51, Color{0, 255, 110, 125});
        }

        DrawText(labels[i], x, rowY, 18, selected ? GREEN : LIGHTGRAY);

        if (i == 0) {
            DrawText(RESOLUTIONS[app.resolutionIndex].label, x + labelW - 220, rowY, 18, RAYWHITE);
        } else if (i == 1) {
            const float t = static_cast<float>(app.fontSize - MIN_FONT_SIZE) /
                            static_cast<float>(MAX_FONT_SIZE - MIN_FONT_SIZE);
            DrawSlider(x + 250, rowY + 8, std::max(160, panelW - 410), t, "", std::to_string(app.fontSize) + " px");
        } else if (i == 2) {
            const float t = static_cast<float>(app.brightness + 100) / 200.0f;
            DrawSlider(x + 250, rowY + 8, std::max(160, panelW - 410), t, "", std::to_string(app.brightness));
        } else if (i == 3) {
            const float t = static_cast<float>(app.contrast - 0.50) / 2.00f;
            DrawSlider(x + 250, rowY + 8, std::max(160, panelW - 410), t, "", FormatFloat(app.contrast, 2) + "x");
        } else if (i == 4) {
            DrawText(app.detailEnhancement ? "ON" : "OFF", x + labelW - 60, rowY, 18, app.detailEnhancement ? GREEN : LIGHTGRAY);
        } else if (i == 5) {
            DrawText(app.crtScanlines ? "ON" : "OFF", x + labelW - 60, rowY, 18, app.crtScanlines ? GREEN : LIGHTGRAY);
        } else if (i == 6) {
            DrawText("ENTER", x + labelW - 75, rowY, 18, GREEN);
        }
    }

    if (panelH >= 400) {
        const int footerY = panelY + panelH - 35;
        DrawText("FONT: smaller = more detail / denser ASCII", panelX + 24, footerY, 12, DARKGREEN);
    }
}

void ShowErrorMessage(const char* title, const char* message) {
    TraceLog(LOG_ERROR, "%s: %s", title, message);
}

void AdjustMenuSelection(AppState& app, int delta) {
    app.menuSelection += delta;
    if (app.menuSelection < 0) app.menuSelection = 6;
    if (app.menuSelection > 6) app.menuSelection = 0;
}

void AdjustMenuValue(AppState& app, int delta) {
    switch (app.menuSelection) {
        case 0:
            app.resolutionIndex = std::clamp(app.resolutionIndex + delta, 0, RESOLUTION_COUNT - 1);
            ApplyResolution(app, app.resolutionIndex);
            break;

        case 1: {
            const int oldFont = app.fontSize;
            app.fontSize = std::clamp(app.fontSize + delta, MIN_FONT_SIZE, MAX_FONT_SIZE);
            if (oldFont != app.fontSize) {
                RebuildGridLayout(app);
                app.previousTunedFrame.release();
                if (!app.frame.empty()) ApplyImageQuality(app);
                Notify(app, "FONT SIZE // " + std::to_string(app.fontSize) + " PX", 1.2f);
            }
            break;
        }

        case 2: {
            const int oldBrightness = app.brightness;
            app.brightness = std::clamp(app.brightness + delta * 2, -100, 100);
            if (oldBrightness != app.brightness && !app.frame.empty()) {
                app.previousTunedFrame.release();
                ApplyImageQuality(app);
            }
            break;
        }

        case 3: {
            const double oldContrast = app.contrast;
            app.contrast = std::clamp(app.contrast + delta * 0.05, 0.50, 2.50);
            if (oldContrast != app.contrast && !app.frame.empty()) {
                app.previousTunedFrame.release();
                ApplyImageQuality(app);
            }
            break;
        }

        case 4:
            if (delta != 0) {
                app.detailEnhancement = !app.detailEnhancement;
                app.previousTunedFrame.release();
                if (!app.frame.empty()) ApplyImageQuality(app);
            }
            break;

        case 5:
            if (delta != 0) app.crtScanlines = !app.crtScanlines;
            break;

        case 6:
            app.paused = false;
            break;

        default:
            break;
    }
}

void HandlePauseMenu(AppState& app) {
    if (!app.paused) return;

    if (IsKeyPressed(KEY_UP)) AdjustMenuSelection(app, -1);
    if (IsKeyPressed(KEY_DOWN)) AdjustMenuSelection(app, 1);

    if (IsKeyPressed(KEY_LEFT)) AdjustMenuValue(app, -1);
    if (IsKeyPressed(KEY_RIGHT)) AdjustMenuValue(app, 1);

    if (IsKeyPressed(KEY_ENTER) && app.menuSelection == 6) {
        app.paused = false;
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
        app.contrast = 1.0;
        app.brightness = 0;
        app.previousTunedFrame.release();
        if (!app.frame.empty()) ApplyImageQuality(app);
        Notify(app, "IMAGE TUNING // RESET");
    }
}

bool StartRecording(AppState& app) {
    if (app.isRecording) return true;

    int recIndex = 1;
    while (fs::exists(app.mediaDirectory / ("ascii_rec_" + std::to_string(recIndex) + ".mp4"))) {
        ++recIndex;
    }

    const int screenW = GetScreenWidth() & ~1;
    const int screenH = GetScreenHeight() & ~1;
    if (screenW < 2 || screenH < 2) return false;

    const fs::path path = app.mediaDirectory / ("ascii_rec_" + std::to_string(recIndex) + ".mp4");

    if (!app.videoWriter.open(
            path.string(),
            cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
            RECORD_FPS,
            cv::Size(screenW, screenH))) {
        Notify(app, "ERROR // VIDEO WRITER OFFLINE", 3.0f);
        return false;
    }

    app.recordWidth = screenW;
    app.recordHeight = screenH;
    app.recordAccumulator = 0.0f;
    app.isRecording = true;

    Notify(app, "UPLINK ESTABLISHED // " + path.filename().string());
    return true;
}

void RecordScreenFrame(AppState& app) {
    if (!app.isRecording || !app.videoWriter.isOpened()) return;

    Image image = LoadImageFromScreen();
    if (!image.data) {
        UnloadImage(image);
        return;
    }

    cv::Mat rgba(image.height, image.width, CV_8UC4, image.data);
    cv::Mat bgr;
    cv::cvtColor(rgba, bgr, cv::COLOR_RGBA2BGR);

    if (bgr.cols != app.recordWidth || bgr.rows != app.recordHeight) {
        cv::resize(bgr, bgr, cv::Size(app.recordWidth, app.recordHeight), 0.0, 0.0, cv::INTER_AREA);
    }

    app.videoWriter.write(bgr);
    UnloadImage(image);
}

void SaveSnapshot(AppState& app) {
    int snapIndex = 1;

    while (
        fs::exists(app.mediaDirectory / ("ascii_snap_" + std::to_string(snapIndex) + ".png")) ||
        fs::exists(app.mediaDirectory / ("ascii_snap_" + std::to_string(snapIndex) + ".txt"))
    ) {
        ++snapIndex;
    }

    const std::string baseName = "ascii_snap_" + std::to_string(snapIndex);
    const fs::path pngPath = app.mediaDirectory / (baseName + ".png");
    const fs::path txtPath = app.mediaDirectory / (baseName + ".txt");

    Image image = LoadImageFromScreen();
    if (image.data) {
        ExportImage(image, pngPath.string().c_str());
    }
    UnloadImage(image);

    std::ofstream out(txtPath);
    if (out.is_open()) {
        for (const std::string& row : app.charGrid) {
            out << row << '\n';
        }
    }

    Notify(app, "DATA CAPTURED // " + baseName, 3.0f);
}

void HandleLiveControls(AppState& app) {
    if (IsKeyPressed(KEY_ONE)) app.mode = MODE_RGB;
    if (IsKeyPressed(KEY_TWO)) app.mode = MODE_MATRIX;
    if (IsKeyPressed(KEY_THREE)) app.mode = MODE_EDGE;
    if (IsKeyPressed(KEY_FOUR)) app.mode = MODE_CYBERPUNK;
    if (IsKeyPressed(KEY_FIVE)) app.mode = MODE_AMBER;
    if (IsKeyPressed(KEY_SIX)) app.mode = MODE_GAMEBOY;
    if (IsKeyPressed(KEY_SEVEN)) app.mode = MODE_THERMAL;
    if (IsKeyPressed(KEY_EIGHT)) app.mode = MODE_BINARY;
    if (IsKeyPressed(KEY_NINE)) app.mode = MODE_GLITCH;
    if (IsKeyPressed(KEY_SPACE)) {
        app.mode = static_cast<RenderMode>((static_cast<int>(app.mode) % 9) + 1);
    }

    if (IsKeyPressed(KEY_C)) app.crtScanlines = !app.crtScanlines;
    if (IsKeyPressed(KEY_H)) app.hideHUD = !app.hideHUD;

    if (IsKeyDown(KEY_UP)) app.contrast = std::clamp(app.contrast + 0.60 * GetFrameTime(), 0.50, 2.50);
    if (IsKeyDown(KEY_DOWN)) app.contrast = std::clamp(app.contrast - 0.60 * GetFrameTime(), 0.50, 2.50);
    if (IsKeyDown(KEY_RIGHT)) app.brightness = std::clamp(app.brightness + static_cast<int>(80.0f * GetFrameTime()), -100, 100);
    if (IsKeyDown(KEY_LEFT)) app.brightness = std::clamp(app.brightness - static_cast<int>(80.0f * GetFrameTime()), -100, 100);

    if (IsKeyPressed(KEY_BACKSPACE)) {
        app.contrast = 1.0;
        app.brightness = 0;
        Notify(app, "IMAGE TUNING // RESET", 1.5f);
    }

    if (IsKeyPressed(KEY_R)) {
        if (app.isRecording) {
            StopRecording(app, "UPLINK SEVERED // MP4 SAVED");
        } else {
            StartRecording(app);
        }
    }

    if (IsKeyPressed(KEY_S)) {
        SaveSnapshot(app);
    }
}

} // namespace

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(WINDOW_W, WINDOW_H, "ASCII OS // TACTICAL OPTIC ENGINE v3.0");
    SetExitKey(KEY_NULL);
    SetWindowMinSize(MIN_WINDOW_W, MIN_WINDOW_H);
    SetTargetFPS(60);

    AppState app;
    app.baseDirectory = GetApplicationDirectory();
    app.mediaDirectory = fs::path(app.baseDirectory) / "media";

    std::error_code ec;
    fs::create_directories(app.mediaDirectory, ec);

    if (ec) {
        Notify(app, "ERROR // MEDIA DIRECTORY FAILED", 4.0f);
    }

    app.asciiFont = GetFontDefault();
#ifdef _WIN32
    const char* monoFonts[] = {
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/lucon.ttf"
    };

    for (const char* fontPath : monoFonts) {
        Font loaded = LoadFontEx(fontPath, 48, nullptr, 0);
        if (loaded.texture.id > 0) {
            app.asciiFont = loaded;
            app.customFontLoaded = true;
            break;
        }
    }
#endif

    RebuildGridLayout(app);
    app.clahe = cv::createCLAHE(2.0, cv::Size(4, 4));
    UpdateResolutionIndexFromWindow(app);

    cv::VideoCapture cap;
    if (!cap.open(0, cv::CAP_DSHOW)) {
        cap.open(0);
    }

    if (!cap.isOpened()) {
        ShowErrorMessage(
            "ASCII Optic Engine",
            "Camera initialization failed. Check your webcam connection and Windows camera permissions."
        );
        if (app.customFontLoaded) UnloadFont(app.asciiFont);
        CloseWindow();
        return 1;
    }

    cap.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
    cap.set(cv::CAP_PROP_FPS, 30);

    while (!WindowShouldClose()) {
        MaintainWindowAspect(app);

        // Pause / resume
        if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) {
            app.paused = !app.paused;
            if (app.paused) app.menuSelection = 0;
        }

        if (!app.paused) {
            HandleLiveControls(app);

            cap >> app.frame;
            if (app.frame.empty()) {
                Notify(app, "ERROR // OPTIC FEED LOST", 4.0f);
                break;
            }

            cv::flip(app.frame, app.frame, 1);

            RebuildGridLayout(app);
            ApplyImageQuality(app);
        } else {
            HandlePauseMenu(app);
        }

        if (app.notificationTimer > 0.0f) {
            app.notificationTimer = std::max(0.0f, app.notificationTimer - GetFrameTime());
        }

        BeginDrawing();
        ClearBackground(Color{5, 5, 10, 255});

        RenderAscii(app);
        DrawCRT(app);
        DrawHUD(app);

        if (app.paused) {
            DrawPauseMenu(app);
        }

        EndDrawing();

        if (app.isRecording && !app.paused) {
            app.recordAccumulator += GetFrameTime();
            const float frameInterval = 1.0f / static_cast<float>(RECORD_FPS);

            if (app.recordAccumulator >= frameInterval) {
                app.recordAccumulator -= frameInterval;
                RecordScreenFrame(app);
            }
        }
    }

    if (app.isRecording) {
        app.videoWriter.release();
    }

    cap.release();
    if (app.customFontLoaded) {
        UnloadFont(app.asciiFont);
    }
    CloseWindow();
    return 0;
}

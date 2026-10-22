#include "raylib.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <random>

namespace fs = std::filesystem;

enum RenderMode {
    MODE_RGB = 1,
    MODE_MATRIX = 2,
    MODE_EDGE = 3,
    MODE_CYBERPUNK = 4,
    MODE_AMBER = 5,
    MODE_GAMEBOY = 6,
    MODE_THERMAL = 7,
    MODE_BINARY = 8,
    MODE_GLITCH = 9
};

int main() {
    fs::create_directories("media");

    cv::VideoCapture cap(0, cv::CAP_DSHOW);
    if (!cap.isOpened()) {
        std::cout << "CRITICAL ERROR: Optic Feed Offline!\n";
        return -1;
    }

    const int GRID_W = 120;
    const int GRID_H = 60;
    const int CHAR_W = 9;
    const int CHAR_H = 13;
    const int FONT_SIZE = 14;
    const int SCREEN_W = GRID_W * CHAR_W;
    const int SCREEN_H = GRID_H * CHAR_H;

    InitWindow(SCREEN_W, SCREEN_H, "ASCII OS // TACTICAL OPTIC ENGINE v2.0");
    SetTargetFPS(60);

    std::string asciiChars = " .:-=+*#%@";
    RenderMode mode = MODE_CYBERPUNK; 

    double contrast = 1.0;
    int brightness = 0;
    bool crtScanlines = true; 
    bool hideHUD = false;

    bool isRecording = false;
    bool triggerSnapshot = false;
    float notificationTimer = 0.0f;
    std::string lastSavedFilename = "SYSTEM.BOOT_SEQUENCE_INITIALIZED...";
    notificationTimer = 4.0f;

    cv::VideoWriter videoWriter;
    char charGrid[GRID_H][GRID_W];

    cv::Mat frame, smallFrame, tunedFrame, gray, gradX, gradY, absGradX, absGradY, edgeMag;

    while (!WindowShouldClose()) {
        cap >> frame;
        if (frame.empty()) break;

        cv::flip(frame, frame, 1);
        cv::resize(frame, smallFrame, cv::Size(GRID_W, GRID_H));

        // --- TACTICAL CONTROLS ---
        if (IsKeyPressed(KEY_ONE)) mode = MODE_RGB;
        if (IsKeyPressed(KEY_TWO)) mode = MODE_MATRIX;
        if (IsKeyPressed(KEY_THREE)) mode = MODE_EDGE;
        if (IsKeyPressed(KEY_FOUR)) mode = MODE_CYBERPUNK;
        if (IsKeyPressed(KEY_FIVE)) mode = MODE_AMBER;
        if (IsKeyPressed(KEY_SIX)) mode = MODE_GAMEBOY;
        if (IsKeyPressed(KEY_SEVEN)) mode = MODE_THERMAL;
        if (IsKeyPressed(KEY_EIGHT)) mode = MODE_BINARY;
        if (IsKeyPressed(KEY_NINE)) mode = MODE_GLITCH;
        if (IsKeyPressed(KEY_SPACE)) mode = (RenderMode)((mode % 9) + 1);

        if (IsKeyPressed(KEY_C)) crtScanlines = !crtScanlines;
        if (IsKeyPressed(KEY_H)) hideHUD = !hideHUD; // Press H to hide/show HUD!

        if (IsKeyDown(KEY_UP)) contrast += 0.02;
        if (IsKeyDown(KEY_DOWN)) contrast = std::max(0.1, contrast - 0.02);
        if (IsKeyDown(KEY_RIGHT)) brightness += 2;
        if (IsKeyDown(KEY_LEFT)) brightness -= 2;
        if (IsKeyPressed(KEY_BACKSPACE)) { contrast = 1.0; brightness = 0; }

        smallFrame.convertTo(tunedFrame, -1, contrast, brightness);

        if (mode == MODE_EDGE) {
            cv::cvtColor(tunedFrame, gray, cv::COLOR_BGR2GRAY);
            cv::Sobel(gray, gradX, CV_16S, 1, 0);
            cv::Sobel(gray, gradY, CV_16S, 0, 1);
            cv::convertScaleAbs(gradX, absGradX);
            cv::convertScaleAbs(gradY, absGradY);
            cv::addWeighted(absGradX, 0.5, absGradY, 0.5, 0, edgeMag);
        }

        // --- RECORDING & SNAPSHOTS ---
        if (IsKeyPressed(KEY_R)) {
            if (!isRecording) {
                int recIndex = 1;
                while (fs::exists("media/ascii_rec_" + std::to_string(recIndex) + ".mp4")) recIndex++;
                
                std::string videoPath = "media/ascii_rec_" + std::to_string(recIndex) + ".mp4";
                videoWriter.open(videoPath, cv::VideoWriter::fourcc('m', 'p', '4', 'v'), 30, cv::Size(SCREEN_W, SCREEN_H));
                
                if (videoWriter.isOpened()) {
                    isRecording = true;
                    lastSavedFilename = "UPLINK ESTABLISHED: " + videoPath;
                    notificationTimer = 2.5f;
                }
            } else {
                isRecording = false;
                videoWriter.release();
                lastSavedFilename = "UPLINK SEVERED. MP4 SAVED.";
                notificationTimer = 3.0f;
            }
        }

        if (IsKeyPressed(KEY_S)) triggerSnapshot = true;
        if (notificationTimer > 0.0f) notificationTimer -= GetFrameTime();

        float timeVal = GetTime();

        // --- GPU RENDER LOOP ---
        BeginDrawing();
            ClearBackground(Color{5, 5, 10, 255});

            for (int y = 0; y < GRID_H; y++) {
                int x_offset = 0;
                if (mode == MODE_GLITCH) {
                    if (GetRandomValue(0, 100) > 95) x_offset = GetRandomValue(-3, 3);
                    if (y % 10 == 0 && GetRandomValue(0, 100) > 80) x_offset = GetRandomValue(-8, 8);
                }

                for (int x = 0; x < GRID_W; x++) {
                    int sample_x = std::clamp(x + x_offset, 0, GRID_W - 1);
                    
                    cv::Vec3b pixel = tunedFrame.at<cv::Vec3b>(y, sample_x);
                    unsigned char b = pixel[0], g = pixel[1], r = pixel[2];

                    if (mode == MODE_GLITCH && x_offset != 0) {
                        int r_sample_x = std::clamp(x + x_offset + 3, 0, GRID_W - 1);
                        r = tunedFrame.at<cv::Vec3b>(y, r_sample_x)[2];
                    }

                    float luma = std::clamp((r * 0.299f + g * 0.587f + b * 0.114f), 0.0f, 255.0f);
                    int charIndex = (int)((luma / 255.0f) * (asciiChars.length() - 1));

                    Color charColor = { r, g, b, 255 };
                    char displayChar = asciiChars[charIndex];

                    switch (mode) {
                        case MODE_MATRIX:
                        case MODE_GLITCH:
                            charColor = Color{ 10, (unsigned char)std::min(255.0f, luma + 60.0f), 20, 255 };
                            break;
                        case MODE_BINARY:
                            displayChar = (luma > 100) ? '1' : '0';
                            if (luma < 30) displayChar = ' ';
                            charColor = Color{ 0, (unsigned char)std::max(100.0f, luma), 0, 255 };
                            break;
                        case MODE_THERMAL:
                            if (luma < 60) charColor = Color{0, 0, (unsigned char)(luma*4), 255};
                            else if (luma < 130) charColor = {(unsigned char)((luma-60)*3), 0, (unsigned char)(255 - (luma-60)*2), 255};
                            else if (luma < 200) charColor = {255, (unsigned char)((luma-130)*3), 0, 255};
                            else charColor = {255, 255, 200, 255};
                            break;
                        case MODE_AMBER:
                            charColor = Color{ (unsigned char)std::min(255.0f, luma + 100.0f), (unsigned char)(luma * 0.7f), 0, 255 };
                            break;
                        case MODE_GAMEBOY:
                            if (luma > 190) { charColor = Color{ 155, 188, 15, 255 }; displayChar = '#'; }
                            else if (luma > 120) { charColor = Color{ 139, 172, 15, 255 }; displayChar = 'x'; }
                            else if (luma > 60) { charColor = Color{ 48, 98, 48, 255 }; displayChar = ':'; }
                            else { charColor = Color{ 15, 56, 15, 255 }; displayChar = ' '; }
                            break;
                        case MODE_EDGE:
                            if (edgeMag.at<unsigned char>(y, x) > 40) {
                                float angle = std::atan2((float)gradY.at<short>(y, x), (float)gradX.at<short>(y, x)) * (180.0f / 3.14159f);
                                if (angle < 0) angle += 180.0f;
                                if (angle >= 22.5f && angle < 67.5f) displayChar = '/';
                                else if (angle >= 67.5f && angle < 112.5f) displayChar = '|';
                                else if (angle >= 112.5f && angle < 157.5f) displayChar = '\\';
                                else displayChar = '-';
                                charColor = Color{ 0, 255, 255, 255 }; 
                            } else {
                                displayChar = ' ';
                            }
                            break;
                        case MODE_CYBERPUNK:
                            if (luma > 180) charColor = Color{ 255, 255, 100, 255 };
                            else if (luma > 110) charColor = Color{ 255, 20, 147, 255 };
                            else if (luma > 45) charColor = Color{ 0, 230, 255, 255 };
                            else charColor = Color{ 30, 10, 50, 255 };
                            break;
                        case MODE_RGB: default: break;
                    }

                    charGrid[y][x] = displayChar;
                    char str[2] = { displayChar, '\0' };
                    DrawText(str, x * CHAR_W, y * CHAR_H, FONT_SIZE, charColor);
                }
            }

            // --- ANIMATED CRT OVERLAY ---
            if (crtScanlines) {
                for (int i = 0; i < SCREEN_H; i += 3) {
                    DrawRectangle(0, i, SCREEN_W, 1, Color{ 0, 0, 0, 150 });
                }
                float scanY = fmod(timeVal * 250.0f, (float)SCREEN_H + 100) - 50;
                DrawRectangle(0, (int)scanY, SCREEN_W, 20, Color{ 255, 255, 255, 12 });
                DrawRectangle(0, (int)scanY + 18, SCREEN_W, 2, Color{ 255, 255, 255, 25 });

                DrawRectangleGradientV(0, 0, SCREEN_W, 60, Color{0,0,0,180}, Color{0,0,0,0});
                DrawRectangleGradientV(0, SCREEN_H - 60, SCREEN_W, 60, Color{0,0,0,0}, Color{0,0,0,200});
                DrawRectangleGradientH(0, 0, 60, SCREEN_H, Color{0,0,0,180}, Color{0,0,0,0});
                DrawRectangleGradientH(SCREEN_W - 60, 0, 60, SCREEN_H, Color{0,0,0,0}, Color{0,0,0,180});
            }

            // --- TACTICAL HUD ---
            if (!hideHUD) {
                // Top Bar
                DrawRectangle(0, 0, SCREEN_W, 35, Color{ 0, 10, 5, 220 });
                DrawRectangle(0, 35, SCREEN_W, 1, Color{ 0, 255, 100, 50 });
                
                std::string modeStr = "OPTIC_MODE // ";
                if (mode == MODE_RGB) modeStr += "RAW_RGB"; else if (mode == MODE_MATRIX) modeStr += "MATRIX_LINK";
                else if (mode == MODE_EDGE) modeStr += "GEOMETRY"; else if (mode == MODE_CYBERPUNK) modeStr += "NEON_PUNK";
                else if (mode == MODE_AMBER) modeStr += "TERM_AMBER"; else if (mode == MODE_GAMEBOY) modeStr += "RETRO_8BIT";
                else if (mode == MODE_THERMAL) modeStr += "THERMAL_IR"; else if (mode == MODE_BINARY) modeStr += "RAW_BINARY";
                else if (mode == MODE_GLITCH) modeStr += "SIGNAL_LOST";
                
                DrawText(modeStr.c_str(), 15, 10, 16, GREEN);

                // Stats
                std::string tuningStr = "CNT: " + std::to_string(contrast).substr(0,4) + " | BRT: " + std::to_string(brightness);
                DrawText(tuningStr.c_str(), 250, 12, 12, DARKGREEN);
                DrawFPS(SCREEN_W - 80, 12);

                // Bottom Bar
                DrawRectangle(0, SCREEN_H - 25, SCREEN_W, 25, Color{ 0, 10, 5, 220 });
                DrawRectangle(0, SCREEN_H - 26, SCREEN_W, 1, Color{ 0, 255, 100, 50 });
                DrawText("1-9:FILTERS | ARROWS:TUNE | BACKSPACE:RESET | C:CRT | H:HUD | S:SNAP | R:REC", 15, SCREEN_H - 18, 12, DARKGREEN);
                
                // Recording Flasher
                if (isRecording) {
                    if ((int)(timeVal * 2) % 2 == 0) { 
                        DrawCircle(SCREEN_W - 120, 17, 6, RED);
                    }
                    DrawText("REC MP4", SCREEN_W - 105, 12, 14, RED);
                }

                // Notification Popups
                if (notificationTimer > 0.0f) {
                    DrawRectangle(20, SCREEN_H - 75, 450, 30, Color{ 0, 25, 10, 230 });
                    DrawRectangleLines(20, SCREEN_H - 75, 450, 30, GREEN);
                    DrawText(lastSavedFilename.c_str(), 30, SCREEN_H - 67, 14, GREEN);
                }
            }

        EndDrawing();

        // --- DIRECT SCREENSHOT EXPORT ---
        if (triggerSnapshot) {
            int snapIndex = 1;
            while (fs::exists("media/ascii_snap_" + std::to_string(snapIndex) + ".png") || 
                   fs::exists("media/ascii_snap_" + std::to_string(snapIndex) + ".txt")) {
                snapIndex++;
            }
            
            std::string snapName = "ascii_snap_" + std::to_string(snapIndex);
            std::string pngPath = "media/" + snapName + ".png";
            std::string txtPath = "media/" + snapName + ".txt";

            Image snapImg = LoadImageFromScreen();
            ExportImage(snapImg, pngPath.c_str());
            UnloadImage(snapImg);

            std::ofstream txtFile(txtPath);
            if (txtFile.is_open()) {
                for (int y = 0; y < GRID_H; y++) {
                    for (int x = 0; x < GRID_W; x++) txtFile << charGrid[y][x];
                    txtFile << "\n";
                }
                txtFile.close();
            }

            lastSavedFilename = "DATA CAPTURED: " + snapName;
            notificationTimer = 3.0f;
            triggerSnapshot = false;
        }

        // --- VIDEO RECORDING EXPORT ---
        if (isRecording && videoWriter.isOpened()) {
            Image screenImg = LoadImageFromScreen();
            cv::Mat matRGBA(SCREEN_H, SCREEN_W, CV_8UC4, screenImg.data);
            cv::Mat matBGR;
            cv::cvtColor(matRGBA, matBGR, cv::COLOR_RGBA2BGR);
            videoWriter.write(matBGR);
            UnloadImage(screenImg);
        }
    }

    if (isRecording) videoWriter.release();
    cap.release();
    CloseWindow();
    return 0;
}
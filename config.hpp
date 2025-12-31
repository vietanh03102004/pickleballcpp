#pragma once 

#include <string>

namespace Config {

    // === ĐƯỜNG DẪN ===
    // Lưu ý: Trong C++, dấu gạch chéo ngược '\' cần được viết là '\\'
    // Hoặc dùng dấu gạch chéo '/' để tương thích đa nền tảng tốt hơn.
    
    // Đường dẫn video nguồn mới của bạn
    const std::string SOURCE_VIDEO_PATH = "in.mp4";
    
    const std::string TARGET_VIDEO_PATH = "Out.mp4";

    // LƯU Ý: Code C++ chạy OpenCV DNN bắt buộc phải dùng file .ONNX
    // Bạn hãy convert 'model_ver2.pt' -> 'model_ver2.onnx' và để vào thư mục data
    const std::string MODEL_PATH = "data/model_ver2.onnx"; 

    // === THAM SỐ MÔ HÌNH ===
    // Cập nhật threshold lên 0.5 theo file config.py mới
    const float CONF_THRESHOLD = 0.5f;
    
    const int BALL_CLASS_ID = 0;
    const int LINE_CLASS_ID = 1;

    // === THAM SỐ KALMAN ===
    const float PROCESS_NOISE = 0.5f;
    const float MEASUREMENT_NOISE = 5.0f;

}
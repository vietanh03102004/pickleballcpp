#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>

// Include file cấu hình mới
#include "config.hpp" 

// Include file header của detector
#include "detectors/ball_detector.hpp" 

int main() {
    // ====================================================
    // 1. KHỞI TẠO HỆ THỐNG
    // ====================================================
    std::cout << "[INFO] Đang khởi tạo Pickleball Detector (New Update)..." << std::endl;
    
    // Hàm này sẽ load model từ Config::MODEL_PATH (model_ver2.onnx)
    initialize_detector();

    // ====================================================
    // 2. MỞ VIDEO NGUỒN
    // ====================================================
    std::cout << "[INFO] Đang mở video: " << Config::SOURCE_VIDEO_PATH << std::endl;
    cv::VideoCapture cap(Config::SOURCE_VIDEO_PATH);
    
    if (!cap.isOpened()) {
        std::cerr << "[ERROR] Không thể mở video nguồn! " << std::endl;
        std::cerr << " -> Hãy kiểm tra lại đường dẫn trong config.hpp" << std::endl;
        std::cerr << " -> Đường dẫn hiện tại: " << Config::SOURCE_VIDEO_PATH << std::endl;
        return -1;
    }

    // Lấy thông số video
    int frame_width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int frame_height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    double fps = cap.get(cv::CAP_PROP_FPS);
    int total_frames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));

    std::cout << "[INFO] Video: " << frame_width << "x" << frame_height 
              << " @ " << fps << " FPS. Tổng frame: " << total_frames << std::endl;

    // ====================================================
    // 3. TẠO VIDEO ĐÍCH
    // ====================================================
    int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
    cv::VideoWriter writer(Config::TARGET_VIDEO_PATH, fourcc, fps, 
                           cv::Size(frame_width, frame_height));

    if (!writer.isOpened()) {
        std::cerr << "[ERROR] Không thể tạo file video đích: " 
                  << Config::TARGET_VIDEO_PATH << std::endl;
        cap.release();
        return -1;
    }

    // ====================================================
    // 4. VÒNG LẶP XỬ LÝ (Thay thế sv.process_video)
    // ====================================================
    std::cout << "[INFO] Bắt đầu xử lý..." << std::endl;

    cv::Mat frame;
    int frame_idx = 0;

    while (true) {
        cap >> frame;

        if (frame.empty()) {
            break;
        }

        // Gọi hàm xử lý chính (update) từ ball_detector
        cv::Mat processed_frame = update(frame, frame_idx);

        // Ghi vào video đích
        writer.write(processed_frame);

        // In tiến độ
        if (frame_idx % 50 == 0) {
            float progress = (float)frame_idx / total_frames * 100.0f;
            std::cout << "Processing: " << frame_idx << "/" << total_frames 
                      << " (" << (int)progress << "%)" << "\r" << std::flush;
        }

        frame_idx++;
    }

    std::cout << std::endl << "[INFO] Hoàn tất! Video đã lưu tại: " 
              << Config::TARGET_VIDEO_PATH << std::endl;

    // Dọn dẹp
    cap.release();
    writer.release();

    return 0;
}
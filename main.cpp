#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>

// Include file cấu hình mới
#include "config.hpp" 

// Include file header của detector
#include "detectors/ball_detector.hpp"

// Include VLC video reader
#include "utils/vlc_reader.hpp" 

int main() {
    // ====================================================
    // 1. KHỞI TẠO HỆ THỐNG
    // ====================================================
    std::cout << "[INFO] Đang khởi tạo Pickleball Detector (New Update)..." << std::endl;
    
    // Hàm này sẽ load model từ Config::MODEL_PATH (model_ver2.onnx)
    initialize_detector();

    // ====================================================
    // 2. MỞ VIDEO NGUỒN (SỬ DỤNG VLC)
    // ====================================================
    std::cout << "[INFO] Đang mở video bằng VLC: " << Config::SOURCE_VIDEO_PATH << std::endl;
    
    // Sử dụng VLC để đọc video
    VLCVideoReader cap;
    if (!cap.open(Config::SOURCE_VIDEO_PATH)) {
        std::cerr << "[ERROR] Không thể mở video nguồn bằng VLC! " << std::endl;
        std::cerr << " -> Hãy kiểm tra lại đường dẫn trong config.hpp" << std::endl;
        std::cerr << " -> Đường dẫn hiện tại: " << Config::SOURCE_VIDEO_PATH << std::endl;
        std::cerr << " -> Kiểm tra xem VLC đã được cài đặt chưa (libvlc-dev trên Linux, vlc trên Windows)" << std::endl;
        return -1;
    }
    
    std::cout << "[INFO] Video đã mở thành công với VLC" << std::endl;

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
        if (!cap.read(frame)) {
            break;
        }

        if (frame.empty()) {
            break;
        }

        // Gọi hàm xử lý chính (update) từ ball_detector
        cv::Mat processed_frame = update(frame, frame_idx);

        // Ghi vào video đích
        writer.write(processed_frame);

        // In tiến độ
        if (frame_idx % 50 == 0) {
            if (total_frames > 0) {
                float progress = (float)frame_idx / total_frames * 100.0f;
                std::cout << "Processing: " << frame_idx << "/" << total_frames 
                          << " (" << (int)progress << "%)" << "\r" << std::flush;
            } else {
                std::cout << "Processing: " << frame_idx << " frames" << "\r" << std::flush;
            }
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
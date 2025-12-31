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

    // Lấy FPS và tổng số frame từ properties
    double fps = cap.get(cv::CAP_PROP_FPS);
    int total_frames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
    
    if (fps <= 0) {
        std::cerr << "[WARNING] FPS không hợp lệ: " << fps << ", sử dụng FPS mặc định: 30.0" << std::endl;
        fps = 30.0;
    }

    // ====================================================
    // 3. ĐỌC FRAME ĐẦU TIÊN ĐỂ LẤY KÍCH THƯỚC CHÍNH XÁC
    // ====================================================
    std::cout << "[INFO] Đang đọc frame đầu tiên để lấy kích thước..." << std::endl;
    
    cv::Mat first_frame;
    cap >> first_frame;
    
    if (first_frame.empty()) {
        std::cerr << "[ERROR] Không thể đọc frame đầu tiên từ video!" << std::endl;
        cap.release();
        return -1;
    }
    
    // Lấy kích thước thực tế từ frame (chính xác hơn properties)
    int frame_width = first_frame.cols;
    int frame_height = first_frame.rows;
    
    std::cout << "[INFO] Video: " << frame_width << "x" << frame_height 
              << " @ " << fps << " FPS. Tổng frame: " << total_frames << std::endl;

    // Kiểm tra thông số video hợp lệ
    if (frame_width <= 0 || frame_height <= 0) {
        std::cerr << "[ERROR] Kích thước video không hợp lệ: " << frame_width 
                  << "x" << frame_height << std::endl;
        cap.release();
        return -1;
    }

    // ====================================================
    // 4. TẠO VIDEO ĐÍCH
    // ====================================================
    int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
    cv::VideoWriter writer(Config::TARGET_VIDEO_PATH, fourcc, fps, 
                           cv::Size(frame_width, frame_height));

    if (!writer.isOpened()) {
        std::cerr << "[ERROR] Không thể tạo file video đích: " 
                  << Config::TARGET_VIDEO_PATH << std::endl;
        std::cerr << "[ERROR] Kích thước: " << frame_width << "x" << frame_height 
                  << ", FPS: " << fps << std::endl;
        cap.release();
        return -1;
    }
    
    std::cout << "[INFO] VideoWriter đã được tạo thành công" << std::endl;

    // ====================================================
    // 5. VÒNG LẶP XỬ LÝ (Thay thế sv.process_video)
    // ====================================================
    std::cout << "[INFO] Bắt đầu xử lý..." << std::endl;

    // Xử lý frame đầu tiên đã đọc
    cv::Mat processed_frame = update(first_frame, 0);
    writer.write(processed_frame);
    
    // In tiến độ cho frame đầu tiên
    if (total_frames > 0) {
        float progress = (float)1 / total_frames * 100.0f;
        std::cout << "Processing: 1/" << total_frames 
                  << " (" << (int)progress << "%)" << "\r" << std::flush;
    }

    cv::Mat frame;
    int frame_idx = 1;  // Bắt đầu từ 1 vì đã xử lý frame 0

    while (true) {
        cap >> frame;

        if (frame.empty()) {
            break;
        }

        // Gọi hàm xử lý chính (update) từ ball_detector
        cv::Mat processed_frame = update(frame, frame_idx);

        // Kiểm tra kích thước frame có khớp với VideoWriter không
        if (processed_frame.cols != frame_width || processed_frame.rows != frame_height) {
            std::cerr << std::endl << "[ERROR] Frame " << frame_idx 
                      << " có kích thước " << processed_frame.cols << "x" << processed_frame.rows
                      << " không khớp với VideoWriter " << frame_width << "x" << frame_height << std::endl;
            break;
        }

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
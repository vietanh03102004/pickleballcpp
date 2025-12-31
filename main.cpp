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

    // Kiểm tra thông số video hợp lệ
    if (frame_width <= 0 || frame_height <= 0) {
        std::cerr << "[ERROR] Kích thước video không hợp lệ: " << frame_width 
                  << "x" << frame_height << std::endl;
        cap.release();
        return -1;
    }
    
    if (fps <= 0) {
        std::cerr << "[WARNING] FPS không hợp lệ: " << fps << ", sử dụng FPS mặc định: 30.0" << std::endl;
        fps = 30.0;
    }

    // ====================================================
    // 3. TẠO VIDEO ĐÍCH
    // ====================================================
    // Thử nhiều fourcc code khác nhau cho tương thích tốt hơn
    int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v'); // MP4V codec
    cv::VideoWriter writer;
    
    // Mở writer với thử lại nếu cần
    bool writer_opened = writer.open(Config::TARGET_VIDEO_PATH, fourcc, fps, 
                                     cv::Size(frame_width, frame_height));
    
    // Nếu không mở được với MP4V, thử H264
    if (!writer_opened) {
        std::cout << "[WARNING] Không mở được với MP4V, thử H264..." << std::endl;
        fourcc = cv::VideoWriter::fourcc('H', '2', '6', '4');
        writer_opened = writer.open(Config::TARGET_VIDEO_PATH, fourcc, fps, 
                                    cv::Size(frame_width, frame_height));
    }
    
    // Nếu vẫn không được, thử XVID
    if (!writer_opened) {
        std::cout << "[WARNING] Không mở được với H264, thử XVID..." << std::endl;
        fourcc = cv::VideoWriter::fourcc('X', 'V', 'I', 'D');
        writer_opened = writer.open(Config::TARGET_VIDEO_PATH, fourcc, fps, 
                                    cv::Size(frame_width, frame_height));
    }
    
    // Nếu vẫn không được, thử MJPG
    if (!writer_opened) {
        std::cout << "[WARNING] Không mở được với XVID, thử MJPG..." << std::endl;
        fourcc = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
        writer_opened = writer.open(Config::TARGET_VIDEO_PATH, fourcc, fps, 
                                    cv::Size(frame_width, frame_height));
    }

    if (!writer_opened || !writer.isOpened()) {
        std::cerr << "[ERROR] Không thể tạo file video đích: " 
                  << Config::TARGET_VIDEO_PATH << std::endl;
        std::cerr << " -> Kích thước: " << frame_width << "x" << frame_height << std::endl;
        std::cerr << " -> FPS: " << fps << std::endl;
        std::cerr << " -> Kiểm tra codec được hỗ trợ hoặc đường dẫn file output" << std::endl;
        cap.release();
        return -1;
    }
    
    std::cout << "[INFO] Video output đã được tạo thành công" << std::endl;

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
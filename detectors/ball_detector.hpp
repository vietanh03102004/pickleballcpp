#pragma once

#include <opencv2/opencv.hpp>

/**
 * @brief Khởi tạo các tài nguyên (Load Model YOLO, Reset Kalman).
 * Cần gọi hàm này 1 lần trong main trước khi chạy vòng lặp.
 */
void initialize_detector();

/**
 * @brief Hàm xử lý chính cho từng frame (tương đương logic update trong Python).
 * * @param frame Ảnh đầu vào từ video.
 * @param frame_idx Số thứ tự của frame (để debug hoặc hiển thị).
 * @return cv::Mat Frame đã được vẽ các thông tin (bbox, đường bóng, bounce, line...).
 */
cv::Mat update(const cv::Mat& frame, int frame_idx);
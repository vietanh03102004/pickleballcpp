#pragma once
#include <opencv2/opencv.hpp>
#include <string>

namespace LineDetector {
    /**
     * @brief Kiểm tra bóng In hay Out khi có va chạm
     * @param cx Tọa độ x bóng
     * @param cy Tọa độ y bóng
     * @param frame Ảnh frame hiện tại (để vẽ kết quả lên)
     */
    void execute(int cx, int cy, cv::Mat& frame);
}
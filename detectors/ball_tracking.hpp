#pragma once
#include <opencv2/opencv.hpp>
#include <map>
#include <vector>

namespace BallTracking {
    struct TrackedObj {
        cv::Point pos;
        int miss;
        double dist;
    };

    // Hàm chính: nhận vào danh sách detection từ YOLO, trả về tọa độ bóng chính (nếu có)
    std::optional<cv::Point> try_get_main_ball(const std::vector<cv::Rect>& detections);
}
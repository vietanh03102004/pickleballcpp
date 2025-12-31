#pragma once
#include <opencv2/opencv.hpp>
#include <map>
#include <vector>
#include <deque>
#include <optional>

namespace BallTracking {
    struct TrackedObj {
        cv::Point pos;
        int miss;
        double dist;  // Tổng quãng đường di chuyển
        float confidence;  // Confidence score trung bình
        float size;  // Kích thước bóng (diện tích hoặc đường kính)
        std::deque<cv::Point> recent_positions;  // Lưu các vị trí gần đây để tính vận tốc
        double recent_speed;  // Vận tốc trung bình gần đây
    };

    // Hàm chính: nhận vào danh sách detection và confidence scores từ YOLO
    // Trả về tọa độ bóng chính (nếu có)
    std::optional<cv::Point> try_get_main_ball(
        const std::vector<cv::Rect>& detections, 
        const std::vector<float>& confidences
    );
}
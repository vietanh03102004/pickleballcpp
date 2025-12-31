#pragma once

#include <opencv2/core.hpp>
#include <optional> // Cần C++17 trở lên để dùng std::optional

namespace Geometry {

    /**
     * Tìm giao điểm của hai đường thẳng đi qua (p0, p1) và (p2, p3).
     * Trả về std::nullopt nếu hai đường thẳng song song.
     */
    std::optional<cv::Point2f> line_intersection(const cv::Point2f& p0, const cv::Point2f& p1, 
                                                 const cv::Point2f& p2, const cv::Point2f& p3);

    /**
     * Tính góc tại đỉnh p1 tạo bởi vector (p1->p0) và (p1->p2).
     * Đơn vị: Độ (Degrees).
     */
    double compute_angle(const cv::Point2f& p0, const cv::Point2f& p1, const cv::Point2f& p2);

}
#include "geometry.hpp"
#include <cmath>
#include <algorithm> // cho std::clamp

namespace Geometry {

    std::optional<cv::Point2f> line_intersection(const cv::Point2f& p0, const cv::Point2f& p1, 
                                                 const cv::Point2f& p2, const cv::Point2f& p3) {
        float x0 = p0.x, y0 = p0.y;
        float x1 = p1.x, y1 = p1.y;
        float x2 = p2.x, y2 = p2.y;
        float x3 = p3.x, y3 = p3.y;

        // Tính mẫu số (denominator)
        float denom = (x0 - x1) * (y2 - y3) - (y0 - y1) * (x2 - x3);

        // Nếu mẫu số bằng 0 (hoặc rất gần 0), hai đường thẳng song song
        if (std::abs(denom) < 1e-6) {
            return std::nullopt; 
        }

        float px = ((x0 * y1 - y0 * x1) * (x2 - x3) - (x0 - x1) * (x2 * y3 - y2 * x3)) / denom;
        float py = ((x0 * y1 - y0 * x1) * (y2 - y3) - (y0 - y1) * (x2 * y3 - y2 * x3)) / denom;

        return cv::Point2f(px, py);
    }

    double compute_angle(const cv::Point2f& p0, const cv::Point2f& p1, const cv::Point2f& p2) {
        // Vector v1: p1 -> p2
        cv::Point2f v1 = p2 - p1;
        // Vector v2: p1 -> p0
        cv::Point2f v2 = p0 - p1;

        // Tích vô hướng (Dot product)
        float dot = v1.dot(v2); // OpenCV Point hỗ trợ hàm .dot()

        // Độ dài vector (Norm)
        double norm_v1 = cv::norm(v1);
        double norm_v2 = cv::norm(v2);

        if (norm_v1 > 0 && norm_v2 > 0) {
            double cos_angle = dot / (norm_v1 * norm_v2);
            
            // Kẹp giá trị trong khoảng [-1, 1] để tránh lỗi hàm acos do sai số số học
            cos_angle = std::clamp(cos_angle, -1.0, 1.0);
            
            double angle_rad = std::acos(cos_angle);
            return angle_rad * (180.0 / CV_PI); // Chuyển sang độ
        }

        return 0.0;
    }
}
#pragma once

#include <opencv2/video/tracking.hpp> // Thư viện Kalman của OpenCV
#include <opencv2/core.hpp>
#include <optional>

namespace KalmanUtils {

    /**
     * Cập nhật bộ lọc Kalman với tọa độ đo được (cx, cy).
     * Nếu Kalman chưa tồn tại, nó sẽ tự khởi tạo.
     */
    cv::Mat update_kalman(float cx, float cy);

    /**
     * Cố gắng dự đoán vị trí tiếp theo mà không cần dữ liệu đo mới.
     * Trả về std::nullopt nếu Kalman chưa được khởi tạo.
     */
    std::optional<cv::Mat> try_predict();

    /**
     * Kiểm tra xem bộ lọc Kalman đã được khởi tạo hay chưa.
     */
    bool is_kfExist();

    /**
     * Reset bộ lọc (hủy trạng thái hiện tại).
     */
    void reset_kalman();

}
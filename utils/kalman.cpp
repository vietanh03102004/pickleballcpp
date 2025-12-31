#include "kalman.hpp"
#include "../config.hpp" // Include file config chứa tham số Noise

namespace KalmanUtils {

    // --- Biến nội bộ (ẩn trong file .cpp này) ---
    static cv::KalmanFilter kf;
    static bool initialized = false;

    // Hàm nội bộ để khởi tạo (tương đương def create_kalman)
    void create_kalman(float start_x, float start_y) {
        // 4 biến trạng thái (x, y, dx, dy), 2 biến đo lường (x, y)
        kf = cv::KalmanFilter(4, 2, 0, CV_32F);

        // Transition Matrix (F)
        // [1 0 1 0]
        // [0 1 0 1]
        // [0 0 1 0]
        // [0 0 0 1]
        cv::setIdentity(kf.transitionMatrix);
        kf.transitionMatrix.at<float>(0, 2) = 1;
        kf.transitionMatrix.at<float>(1, 3) = 1;

        // Measurement Matrix (H)
        // [1 0 0 0]
        // [0 1 0 0]
        cv::setIdentity(kf.measurementMatrix);

        // Process Noise Covariance (Q)
        cv::setIdentity(kf.processNoiseCov, cv::Scalar::all(Config::PROCESS_NOISE));

        // Measurement Noise Covariance (R)
        cv::setIdentity(kf.measurementNoiseCov, cv::Scalar::all(Config::MEASUREMENT_NOISE));

        // Error Covariance Post (P)
        cv::setIdentity(kf.errorCovPost, cv::Scalar::all(1));

        // State Post (Trạng thái ban đầu)
        kf.statePost.at<float>(0) = start_x;
        kf.statePost.at<float>(1) = start_y;
        kf.statePost.at<float>(2) = 0;
        kf.statePost.at<float>(3) = 0;

        initialized = true;
    }

    cv::Mat update_kalman(float cx, float cy) {
        // Nếu chưa có Kalman, tạo mới ngay lập tức
        if (!initialized) {
            create_kalman(cx, cy);
            // Gọi predict lần đầu để khởi động ma trận
            kf.predict();
        }

        // 1. Correct (Hiệu chỉnh với giá trị đo được)
        cv::Mat measurement(2, 1, CV_32F);
        measurement.at<float>(0) = cx;
        measurement.at<float>(1) = cy;
        kf.correct(measurement);

        // 2. Predict (Dự đoán bước tiếp theo)
        cv::Mat prediction = kf.predict();
        return prediction;
    }

    std::optional<cv::Mat> try_predict() {
        if (!initialized) {
            return std::nullopt;
        }
        cv::Mat prediction = kf.predict();
        return prediction;
    }

    bool is_kfExist() {
        return initialized;
    }

    void reset_kalman() {
        initialized = false;
        // Không cần delete kf vì nó là object tĩnh, 
        // lần tới gọi create_kalman nó sẽ được ghi đè.
    }

}
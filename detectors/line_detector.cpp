#include "line_detector.hpp"
#include "../config.hpp"
#include <iostream>
#include <cmath>
#include <vector>

namespace LineDetector {

    void execute(int cx, int cy, cv::Mat& frame) {
        // 1. Đọc frame đầu tiên của video để tìm line (Giống logic Python)
        cv::VideoCapture cap(Config::SOURCE_VIDEO_PATH);
        if (!cap.isOpened()) {
            std::cerr << "[LineDetector] Không mở được video để tìm line!" << std::endl;
            return;
        }
        cv::Mat img;
        cap >> img; // Đọc 1 frame
        cap.release();

        if (img.empty()) return;

        // 2. Xử lý ảnh tìm Line (Màu trắng)
        cv::Mat hsv, mask;
        cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);

        // Lower/Upper từ Python: [0, 0, 130] -> [180, 80, 255]
        cv::inRange(hsv, cv::Scalar(0, 0, 130), cv::Scalar(180, 80, 255), mask);

        // Morphology
        cv::Mat kernel = cv::Mat::ones(7, 7, CV_8U);
        cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);
        cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);

        // 3. Tìm đường thẳng (HoughLinesP thay thế cho findContours phức tạp)
        std::vector<cv::Vec4i> lines;
        // Tham số: 1 pixel, 1 độ, threshold 50, minLength 50, maxGap 10
        cv::HoughLinesP(mask, lines, 1, CV_PI/180, 50, 50, 10);

        // Lọc line theo góc (80 - 85 độ)
        std::vector<cv::Vec4i> filtered_lines;
        for (const auto& l : lines) {
            double dx = l[2] - l[0];
            double dy = l[3] - l[1];
            double angle = std::atan2(dy, dx) * 180.0 / CV_PI;
            angle = std::fmod(angle + 360.0, 180.0); // Giữ trong [0, 180)

            if (angle >= 80 && angle <= 85) {
                filtered_lines.push_back(l);
            }
        }

        // Vẽ line tìm được lên frame hiện tại
        for (const auto& l : filtered_lines) {
            cv::line(frame, cv::Point(l[0], l[1]), cv::Point(l[2], l[3]), cv::Scalar(0, 255, 0), 2);
        }

        // 4. Kiểm tra In/Out (Chỉ lấy line đầu tiên tìm được)
        std::string status = "Out";
        cv::Scalar color(0, 0, 255); // Red

        if (!filtered_lines.empty()) {
            cv::Vec4i line = filtered_lines[0];
            cv::Point pt1(line[0], line[1]);
            cv::Point pt2(line[2], line[3]);

            double dx = pt2.x - pt1.x;
            double dy = pt2.y - pt1.y;

            // Tích chéo xác định vị trí điểm so với vector
            double value = (cx - pt1.x) * dy - (cy - pt1.y) * dx;

            // Python: value > 0 là In (tùy thuộc hệ trục, giả sử theo code gốc)
            if (value > 0) {
                status = "In";
                color = cv::Scalar(0, 255, 0); // Green
            }
        }

        // Vẽ kết quả
        cv::putText(frame, status, cv::Point(cx - 20, cy - 20), 
                    cv::FONT_HERSHEY_SIMPLEX, 1.0, color, 3);
        cv::circle(frame, cv::Point(cx, cy), 10, color, -1);
    }
}
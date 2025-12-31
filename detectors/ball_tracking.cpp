#include "ball_tracking.hpp"
#include <cmath>
#include <iostream>

namespace BallTracking {

    // Các tham số cấu hình (từ ball_tracking.py)
    const double DISTANCE_THRESHOLD = 150.0;
    const int MAX_MISS = 10;
    const double MAIN_THRESHOLD = 50.0;

    // Biến toàn cục quản lý tracking
    static std::map<int, TrackedObj> tracking_objects;
    static int next_id = 0;

    std::optional<cv::Point> try_get_main_ball(const std::vector<cv::Rect>& detections) {
        // 1. Chuyển đổi Rect sang Point (tâm)
        std::vector<cv::Point> centers;
        for (const auto& rect : detections) {
            centers.push_back(cv::Point(rect.x + rect.width / 2, rect.y + rect.height / 2));
        }

        std::vector<int> used_ids;

        // 2. Gán ID (Matching)
        for (const auto& center : centers) {
            int matched_id = -1;
            double min_dist = 99999.0;

            // Tìm object cũ gần nhất
            for (auto& [id, obj] : tracking_objects) {
                double d = std::hypot(center.x - obj.pos.x, center.y - obj.pos.y);
                if (d < min_dist && d < DISTANCE_THRESHOLD) {
                    min_dist = d;
                    matched_id = id;
                }
            }

            if (matched_id == -1) {
                // Tạo mới
                matched_id = next_id++;
                tracking_objects[matched_id] = {center, 0, 0.0};
            } else {
                // Cập nhật cũ
                double d = std::hypot(center.x - tracking_objects[matched_id].pos.x, 
                                      center.y - tracking_objects[matched_id].pos.y);
                tracking_objects[matched_id].dist += d;
                tracking_objects[matched_id].pos = center;
                tracking_objects[matched_id].miss = 0;
            }
            used_ids.push_back(matched_id);
        }

        // 3. Xử lý object mất tín hiệu (Missing)
        auto it = tracking_objects.begin();
        while (it != tracking_objects.end()) {
            bool found = false;
            for (int uid : used_ids) {
                if (uid == it->first) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                it->second.miss++;
                if (it->second.miss > MAX_MISS) {
                    it = tracking_objects.erase(it);
                    continue;
                }
            }
            ++it;
        }

        // 4. Tìm Main Ball (Quãng đường xa nhất)
        int main_id = -1;
        double max_dist = 0.0;

        for (const auto& [id, obj] : tracking_objects) {
            if (obj.dist > max_dist) {
                max_dist = obj.dist;
                main_id = id;
            }
        }

        if (main_id != -1 && max_dist > MAIN_THRESHOLD) {
            return tracking_objects[main_id].pos;
        }

        return std::nullopt;
    }
}
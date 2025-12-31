#include "ball_tracking.hpp"
#include <cmath>
#include <iostream>
#include <algorithm>

namespace BallTracking {

    // Các tham số cấu hình
    const double DISTANCE_THRESHOLD = 150.0;
    const int MAX_MISS = 10;
    const double MAIN_THRESHOLD = 50.0;
    
    // Tham số filter mới
    const float MIN_BALL_SIZE = 50.0f;   // Kích thước bóng tối thiểu (pixels^2)
    const float MAX_BALL_SIZE = 5000.0f; // Kích thước bóng tối đa (pixels^2)
    const double MIN_SPEED = 2.0;        // Vận tốc tối thiểu để coi là bóng đang di chuyển
    const int RECENT_POSITIONS_COUNT = 5; // Số vị trí gần đây để tính vận tốc

    // Biến toàn cục quản lý tracking
    static std::map<int, TrackedObj> tracking_objects;
    static int next_id = 0;

    // Hàm tính kích thước từ Rect (diện tích)
    static float calculate_size(const cv::Rect& rect) {
        return static_cast<float>(rect.width * rect.height);
    }

    // Hàm tính vận tốc trung bình từ các vị trí gần đây
    static double calculate_recent_speed(const std::deque<cv::Point>& positions) {
        if (positions.size() < 2) return 0.0;
        
        double total_dist = 0.0;
        for (size_t i = 1; i < positions.size(); ++i) {
            total_dist += std::hypot(positions[i].x - positions[i-1].x, 
                                     positions[i].y - positions[i-1].y);
        }
        return total_dist / (positions.size() - 1);
    }

    std::optional<cv::Point> try_get_main_ball(
        const std::vector<cv::Rect>& detections, 
        const std::vector<float>& confidences
    ) {
        // Kiểm tra số lượng detections và confidences phải bằng nhau
        if (detections.size() != confidences.size()) {
            std::cerr << "[WARNING] Detections và confidences không khớp!" << std::endl;
            return std::nullopt;
        }

        // 1. Chuyển đổi Rect sang Point (tâm) và filter theo kích thước
        std::vector<cv::Point> centers;
        std::vector<float> valid_confidences;
        std::vector<float> valid_sizes;
        std::vector<cv::Rect> valid_detections;
        
        for (size_t i = 0; i < detections.size(); ++i) {
            float size = calculate_size(detections[i]);
            
            // FILTER 1: Lọc theo kích thước (loại bỏ bóng quá to/quá nhỏ)
            if (size >= MIN_BALL_SIZE && size <= MAX_BALL_SIZE) {
                centers.push_back(cv::Point(
                    detections[i].x + detections[i].width / 2, 
                    detections[i].y + detections[i].height / 2
                ));
                valid_confidences.push_back(confidences[i]);
                valid_sizes.push_back(size);
                valid_detections.push_back(detections[i]);
            }
        }

        if (centers.empty()) {
            // Không có bóng nào hợp lệ
            return std::nullopt;
        }

        std::vector<int> used_ids;

        // 2. Gán ID (Matching) - Cải thiện với confidence và size
        for (size_t idx = 0; idx < centers.size(); ++idx) {
            const cv::Point& center = centers[idx];
            float conf = valid_confidences[idx];
            float size = valid_sizes[idx];
            
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
                TrackedObj new_obj;
                new_obj.pos = center;
                new_obj.miss = 0;
                new_obj.dist = 0.0;
                new_obj.confidence = conf;
                new_obj.size = size;
                new_obj.recent_positions.push_back(center);
                new_obj.recent_speed = 0.0;
                tracking_objects[matched_id] = new_obj;
            } else {
                // Cập nhật object cũ
                double d = std::hypot(center.x - tracking_objects[matched_id].pos.x, 
                                      center.y - tracking_objects[matched_id].pos.y);
                tracking_objects[matched_id].dist += d;
                tracking_objects[matched_id].pos = center;
                tracking_objects[matched_id].miss = 0;
                
                // Cập nhật confidence trung bình (exponential moving average)
                tracking_objects[matched_id].confidence = 
                    0.7f * tracking_objects[matched_id].confidence + 0.3f * conf;
                
                // Cập nhật size trung bình
                tracking_objects[matched_id].size = 
                    0.7f * tracking_objects[matched_id].size + 0.3f * size;
                
                // Cập nhật recent positions để tính vận tốc
                tracking_objects[matched_id].recent_positions.push_back(center);
                if (tracking_objects[matched_id].recent_positions.size() > RECENT_POSITIONS_COUNT) {
                    tracking_objects[matched_id].recent_positions.pop_front();
                }
                tracking_objects[matched_id].recent_speed = 
                    calculate_recent_speed(tracking_objects[matched_id].recent_positions);
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

        // 4. Tìm Main Ball - CẢI THIỆN LOGIC CHỌN BÓNG
        // Thay vì chỉ dựa vào quãng đường, kết hợp nhiều yếu tố:
        // - Vận tốc gần đây (bóng thi đấu di chuyển nhanh)
        // - Confidence (bóng thi đấu có confidence cao)
        // - Quãng đường (tích lũy theo thời gian)
        // - Vị trí (ưu tiên vùng trung tâm frame - có thể thêm sau)
        
        int main_id = -1;
        double max_score = -1.0;

        for (const auto& [id, obj] : tracking_objects) {
            // Bỏ qua object quá mới (chưa có đủ dữ liệu)
            if (obj.recent_positions.size() < 2) continue;
            
            // FILTER 2: Loại bỏ bóng tĩnh (vận tốc quá thấp)
            if (obj.recent_speed < MIN_SPEED && obj.dist > 200.0) {
                // Bóng đã track lâu nhưng vận tốc thấp -> có thể là bóng ngoài sân
                continue;
            }
            
            // Tính điểm số tổng hợp (score)
            // Công thức: kết hợp vận tốc, confidence, và quãng đường
            double speed_score = std::min(obj.recent_speed / 20.0, 1.0) * 3.0;  // Max 3 điểm
            double conf_score = obj.confidence * 2.0;  // Max 2 điểm (với confidence max = 1.0)
            double dist_score = std::min(obj.dist / 500.0, 1.0) * 2.0;  // Max 2 điểm
            
            double total_score = speed_score + conf_score + dist_score;
            
            if (total_score > max_score) {
                max_score = total_score;
                main_id = id;
            }
        }

        // Kiểm tra threshold
        if (main_id != -1 && tracking_objects[main_id].dist > MAIN_THRESHOLD) {
            return tracking_objects[main_id].pos;
        }

        return std::nullopt;
    }
}
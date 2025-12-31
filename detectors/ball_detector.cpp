#include "ball_detector.hpp"
#include "../config.hpp"
#include "../utils/geometry.hpp"
#include "../utils/kalman.hpp"
#include "ball_tracking.hpp"
#include "line_detector.hpp"

#include <iostream>
#include <deque>
#include <optional>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

// --- Biến toàn cục ---
static cv::dnn::Net net;
static std::deque<cv::Point> ball_positions;
static bool bounce_flag = false;
static std::optional<cv::Point2f> previous_predict = std::nullopt;

// Hàm helper: Kiểm tra file tồn tại
static bool file_exists(const std::string& path) {
    std::ifstream file(path);
    return file.good();
}

// Hàm helper: Tìm đường dẫn model (thử nhiều vị trí)
static std::string find_model_path(const std::string& relative_path) {
    // Danh sách các đường dẫn có thể
    std::vector<std::string> possible_paths = {
        relative_path,  // Từ thư mục hiện tại
        "../" + relative_path,  // Từ build/ -> ../data/model_ver2.onnx
        "../../" + relative_path  // Nếu chạy từ build/xxx
    };
    
    // Thử lấy tên file từ đường dẫn (nếu có dấu /)
    size_t last_slash = relative_path.find_last_of('/');
    if (last_slash != std::string::npos && last_slash + 1 < relative_path.length()) {
        std::string filename = relative_path.substr(last_slash + 1);
        possible_paths.push_back(filename);
    }
    
    for (const auto& path : possible_paths) {
        if (file_exists(path)) {
            std::cout << "[INFO] Tìm thấy model tại: " << path << std::endl;
            return path;
        }
    }
    
    return relative_path; // Trả về đường dẫn gốc nếu không tìm thấy
}

// --- Hàm khởi tạo ---
void initialize_detector() {
    std::string model_path = find_model_path(Config::MODEL_PATH);
    
    std::cout << "[INFO] Loading YOLO ONNX model: " << model_path << std::endl;
    
    // Kiểm tra file tồn tại
    if (!file_exists(model_path)) {
        std::cerr << "[ERROR] Không tìm thấy file ONNX model!" << std::endl;
        std::cerr << " -> Đường dẫn đã thử: " << model_path << std::endl;
        std::cerr << " -> Kiểm tra file có tồn tại tại: " << Config::MODEL_PATH << std::endl;
        std::cerr << " -> Hoặc thử đường dẫn tuyệt đối trong config.hpp" << std::endl;
        
        // In thư mục hiện tại để debug
        #ifdef _WIN32
        char current_dir[1024];
        if (GetCurrentDirectoryA(1024, current_dir)) {
            std::cerr << " -> Thư mục hiện tại: " << current_dir << std::endl;
        }
        #else
        char current_dir[1024];
        if (getcwd(current_dir, sizeof(current_dir)) != nullptr) {
            std::cerr << " -> Thư mục hiện tại: " << current_dir << std::endl;
        }
        #endif
        
        exit(1);
    }
    
    try {
        net = cv::dnn::readNet(model_path);
        
        // Kiểm tra model đã load thành công
        if (net.empty()) {
            std::cerr << "[ERROR] Không thể load model ONNX (net.empty())" << std::endl;
            exit(1);
        }
        
        if (cv::cuda::getCudaEnabledDeviceCount() > 0) {
            net.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
            net.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
            std::cout << "[INFO] Sử dụng CUDA backend" << std::endl;
        } else {
            net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
            net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
            std::cout << "[INFO] Sử dụng CPU backend" << std::endl;
        }
        
        std::cout << "[INFO] Model đã load thành công!" << std::endl;
    } catch (const cv::Exception& e) {
        std::cerr << "[ERROR] OpenCV Exception khi load model: " << e.what() << std::endl;
        std::cerr << " -> File: " << model_path << std::endl;
        exit(1);
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception khi load model: " << e.what() << std::endl;
        std::cerr << " -> File: " << model_path << std::endl;
        exit(1);
    }
    KalmanUtils::reset_kalman();
}

// --- Hàm update hoàn chỉnh (Thay thế hàm update cũ) ---
cv::Mat update(const cv::Mat& frame, int frame_idx) {
    cv::Mat annotated_frame = frame.clone();

    // ====================================================
    // 1. YOLO INFERENCE & POST-PROCESSING
    // ====================================================
    
    // a. Pre-process
    cv::Mat blob;
    cv::dnn::blobFromImage(frame, blob, 1.0/255.0, cv::Size(640, 640), cv::Scalar(), true, false);
    net.setInput(blob);
    
    // b. Inference
    std::vector<cv::Mat> outputs;
    net.forward(outputs, net.getUnconnectedOutLayersNames());

    // c. Post-process (ĐÃ SỬA ĐỂ TỰ ĐỘNG NHẬN DIỆN SIZE)
    
    // Lấy kích thước thực tế từ output của model
    // Output chuẩn YOLOv8 thường là [1, channels, anchors] (ví dụ: [1, 5, 8400] hoặc [1, 84, 8400])
    int dimensions = outputs[0].size[1]; 
    int rows_count = outputs[0].size[2];
    
    // In ra để debug (bạn sẽ thấy nó in ra 5, 6 hoặc 84...)
    // std::cout << "[DEBUG] Model output dimensions: " << dimensions << ", Anchors: " << rows_count << std::endl;

    // Reshape về đúng kích thước thực tế của model thay vì fix cứng 84
    cv::Mat output_t = outputs[0].reshape(1, dimensions).t();
    
    float x_scale = (float)frame.cols / 640.0f;
    float y_scale = (float)frame.rows / 640.0f;

    std::vector<cv::Rect> boxes;
    std::vector<float> confidences;

    float* data = (float*)output_t.data;

    for (int i = 0; i < rows_count; ++i) {
        // Mỗi hàng dữ liệu
        float* row = data + (i * dimensions);
        
        // 4 giá trị đầu tiên luôn là cx, cy, w, h
        // Các giá trị từ index 4 trở đi là score của các class
        float* classes_scores = row + 4;
        
        // Tạo ma trận scores với kích thước đúng bằng (dimensions - 4)
        cv::Mat scores(1, dimensions - 4, CV_32F, classes_scores);
        
        cv::Point class_id_point;
        double max_class_score;
        cv::minMaxLoc(scores, 0, &max_class_score, 0, &class_id_point);

        // Debug: Nếu điểm số cao bất thường ở góc, in ra xem
        // if (max_class_score > 0.8) std::cout << "Box: " << row[0] << "," << row[1] << " Score: " << max_class_score << std::endl;

        if (max_class_score > Config::CONF_THRESHOLD) {
            // Lưu ý: Nếu model chỉ có 1 class (bóng), class_id_point.x sẽ luôn là 0
            // Nếu model nhiều class, cần check class_id
            if (dimensions > 5) { 
                 // Model nhiều class -> Check đúng class ID
                 if (class_id_point.x != Config::BALL_CLASS_ID) continue;
            }
            // Nếu model chỉ có 1 class (dimensions == 5), mặc định lấy luôn

            float cx = row[0]; 
            float cy = row[1];
            float w = row[2];  
            float h = row[3];
            
            int left = int((cx - 0.5 * w) * x_scale);
            int top = int((cy - 0.5 * h) * y_scale);
            int width = int(w * x_scale);
            int height = int(h * y_scale);
            
            boxes.push_back(cv::Rect(left, top, width, height));
            confidences.push_back((float)max_class_score);
        }
    }
    
    // d. NMS (Non-Maximum Suppression) -> Tạo ra 'indices'
    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, confidences, Config::CONF_THRESHOLD, 0.4f, indices);
    
    // Lọc ra các box cuối cùng và confidence scores tương ứng
    std::vector<cv::Rect> ball_detections;
    std::vector<float> ball_confidences;
    for (int idx : indices) {
        ball_detections.push_back(boxes[idx]);
        ball_confidences.push_back(confidences[idx]);
    }

    // ====================================================
    // ... (Phần 1: YOLO Inference giữ nguyên) ...

    // ====================================================
    // 2. LOGIC TRACKING & KALMAN FILTER
    // ====================================================

    std::optional<cv::Point> center = BallTracking::try_get_main_ball(ball_detections, ball_confidences);
    
    int cx, cy;
    bool is_measurement = false; 

    if (center.has_value()) {
        // [CASE 1]: Tìm thấy bóng bằng YOLO
        cx = center->x;
        cy = center->y;
        is_measurement = true;
    } 
    else {
        // [CASE 2]: Không thấy bóng -> Dùng Kalman
        // SỬA LỖI: Thêm điều kiện !ball_positions.empty()
        // Nghĩa là: Chỉ dự đoán nếu trước đó đã từng có bóng.
        if (!ball_positions.empty() && KalmanUtils::is_kfExist() && previous_predict.has_value()) {
            cx = (int)previous_predict->x;
            cy = (int)previous_predict->y;
            is_measurement = false;

            cv::putText(annotated_frame, "KALMAN PREDICTED", cv::Point(50, 100),
                        cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 2);
        } else {
            // Chưa từng thấy bóng bao giờ HOẶC Kalman chưa sẵn sàng -> Bỏ qua
            return annotated_frame;
        }
    }

    // 3. VALIDATE (Kiểm tra khoảng cách)
    if (!ball_positions.empty()) {
        cv::Point last_pos = ball_positions.back();
        // ... (Giữ nguyên logic validate) ...
        double dist = std::hypot(cx - last_pos.x, cy - last_pos.y);
        
        if (dist < 5.0) {
             for (const auto& pos : ball_positions) cv::circle(annotated_frame, pos, 4, cv::Scalar(0, 255, 0), -1);
             return annotated_frame;
        }
        else if (dist > 400.0) {
            // Nếu nhảy quá xa -> Coi như bóng mới -> Reset lại từ đầu
            KalmanUtils::reset_kalman();
            ball_positions.clear();
            
            // QUAN TRỌNG: Nếu đây là bóng dự đoán (is_measurement == false) mà lại nhảy xa 
            // thì chứng tỏ dự đoán sai -> Return luôn, không lưu điểm này.
            if (!is_measurement) return annotated_frame; 
            
            // Nếu là bóng thật (is_measurement == true) -> Tiếp tục chạy xuống để init Kalman mới
        }
    }



    // 4. UPDATE / PREDICT KALMAN
    if (is_measurement) {
        // Có bóng thực -> Update (Correct phase)
        cv::Mat kf_res = KalmanUtils::update_kalman((float)cx, (float)cy);
        previous_predict = cv::Point2f(kf_res.at<float>(0), kf_res.at<float>(1));
    } else {
        // Bóng dự đoán -> Predict phase cho frame tiếp theo
        auto pred_mat = KalmanUtils::try_predict();
        if (pred_mat.has_value()) {
            previous_predict = cv::Point2f(pred_mat.value().at<float>(0), pred_mat.value().at<float>(1));
        }
    }

    // 5. LƯU VỊ TRÍ
    ball_positions.push_back(cv::Point(cx, cy));
    if (ball_positions.size() > 4) ball_positions.pop_front();

    // 6. VẼ TRAIL (Đuôi bóng)
    for (const auto& pos : ball_positions) {
        cv::circle(annotated_frame, pos, 4, cv::Scalar(0, 255, 0), -1);
    }

    // 7. DETECT BOUNCE (Xử lý nảy bóng)
    bool skip_detect_bounce = false;

    // --- Cách 1: Giao điểm (Line Intersection) ---
    if (bounce_flag && ball_positions.size() == 4) {
        auto inter = Geometry::line_intersection(ball_positions[0], ball_positions[1], ball_positions[2], ball_positions[3]);
        if (inter.has_value()) {
            skip_detect_bounce = true;
            bounce_flag = false;
            cv::Point inter_pt = inter.value();
            
            cv::circle(annotated_frame, inter_pt, 6, cv::Scalar(0, 0, 255), -1);
            cv::putText(annotated_frame, "BOUNCE POINT", cv::Point(inter_pt.x + 10, inter_pt.y),
                        cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 2);
            
            LineDetector::execute(inter_pt.x, inter_pt.y, annotated_frame);
        }
    }

    // --- Cách 2: Góc (Angle Change) ---
    if (ball_positions.size() >= 3 && !skip_detect_bounce) {
        cv::Point p0 = ball_positions[ball_positions.size() - 3];
        cv::Point p1 = ball_positions[ball_positions.size() - 2];
        cv::Point p2 = ball_positions[ball_positions.size() - 1];

        double angle_deg = Geometry::compute_angle(p0, p1, p2);

        if (angle_deg < 150.0 && !bounce_flag) {
            bounce_flag = true;
            cv::putText(annotated_frame, "BOUNCE", cv::Point(50, 50),
                        cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 255), 2);
            // Có thể gọi LineDetector ở đây nếu muốn bắt điểm nảy ngay lập tức
             LineDetector::execute(p1.x, p1.y, annotated_frame);
        } else if (angle_deg >= 150.0) {
            bounce_flag = false;
        }

        // Visualize góc để debug
        std::string angle_str = cv::format("%.1f deg", angle_deg);
        cv::putText(annotated_frame, angle_str, cv::Point(p1.x + 10, p1.y - 10),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 255), 1);
        
        cv::arrowedLine(annotated_frame, p1, p0, cv::Scalar(200, 220, 100), 2);
        cv::arrowedLine(annotated_frame, p1, p2, cv::Scalar(120, 255, 160), 2);
    }

    return annotated_frame;
}
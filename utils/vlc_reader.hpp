#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <vlc/vlc.h>
#include <mutex>
#include <atomic>

/**
 * @brief VLC Video Reader - Wrapper class để đọc video bằng VLC thay vì OpenCV
 * 
 * Class này cung cấp interface tương tự cv::VideoCapture nhưng sử dụng libVLC
 * để đọc video. Frames được convert sang cv::Mat để tương thích với code hiện tại.
 */
class VLCVideoReader {
public:
    VLCVideoReader();
    ~VLCVideoReader();
    
    /**
     * @brief Mở video file
     * @param video_path Đường dẫn đến file video
     * @return true nếu mở thành công, false nếu thất bại
     */
    bool open(const std::string& video_path);
    
    /**
     * @brief Kiểm tra video đã mở thành công chưa
     * @return true nếu đã mở, false nếu chưa
     */
    bool isOpened() const;
    
    /**
     * @brief Đọc frame tiếp theo từ video
     * @param frame Mat để lưu frame đọc được
     * @return true nếu đọc thành công, false nếu hết video
     */
    bool read(cv::Mat& frame);
    
    /**
     * @brief Lấy property của video (tương tự cv::VideoCapture::get)
     * @param prop Property cần lấy (CAP_PROP_FRAME_WIDTH, CAP_PROP_FPS, etc.)
     * @return Giá trị property, -1 nếu không hỗ trợ
     */
    double get(int prop) const;
    
    /**
     * @brief Đóng video và giải phóng tài nguyên
     */
    void release();
    
    /**
     * @brief Set video property (seek, etc.)
     * @param prop Property cần set
     * @param value Giá trị
     * @return true nếu thành công
     */
    bool set(int prop, double value);
    
    // Operator để tương thích với OpenCV VideoCapture
    VLCVideoReader& operator>>(cv::Mat& frame);

private:
    libvlc_instance_t* vlc_instance_;
    libvlc_media_player_t* media_player_;
    libvlc_media_t* media_;
    
    bool is_opened_;
    unsigned int frame_width_;
    unsigned int frame_height_;
    double fps_;
    int total_frames_;
    
    // Buffer để lưu frame data
    cv::Mat frame_buffer_;
    std::mutex frame_mutex_;
    std::atomic<bool> frame_ready_;
    std::atomic<bool> format_setup_;
    
    // Callback functions
    static void* lock(void* data, void** p_pixels);
    static void unlock(void* data, void* id, void* const* p_pixels);
    static void display(void* data, void* id);
    static unsigned format_setup(void** opaque, char* chroma, unsigned* width, unsigned* height, unsigned* pitches, unsigned* lines);
    static void format_cleanup(void* opaque);
    
    /**
     * @brief Khởi tạo và lấy thông tin video
     */
    bool initializeVideoInfo();
};


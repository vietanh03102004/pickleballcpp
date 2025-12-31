#include "vlc_reader.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>

VLCVideoReader::VLCVideoReader() 
    : vlc_instance_(nullptr)
    , media_player_(nullptr)
    , media_(nullptr)
    , is_opened_(false)
    , frame_width_(0)
    , frame_height_(0)
    , fps_(0.0)
    , total_frames_(0)
    , frame_ready_(false)
    , format_setup_(false)
{
}

VLCVideoReader::~VLCVideoReader() {
    release();
}

bool VLCVideoReader::open(const std::string& video_path) {
    release(); // Đóng video cũ nếu có
    
    // Khởi tạo VLC instance
    vlc_instance_ = libvlc_new(0, nullptr);
    if (!vlc_instance_) {
        std::cerr << "[ERROR] Không thể khởi tạo VLC instance" << std::endl;
        return false;
    }
    
    // Tạo media từ file path
    media_ = libvlc_media_new_path(vlc_instance_, video_path.c_str());
    if (!media_) {
        std::cerr << "[ERROR] Không thể tạo VLC media từ: " << video_path << std::endl;
        libvlc_release(vlc_instance_);
        vlc_instance_ = nullptr;
        return false;
    }
    
    // Parse media để lấy thông tin
    libvlc_media_parse_with_options(media_, libvlc_media_parse_local, -1);
    
    // Tạo media player
    media_player_ = libvlc_media_player_new_from_media(media_);
    if (!media_player_) {
        std::cerr << "[ERROR] Không thể tạo VLC media player" << std::endl;
        libvlc_media_release(media_);
        libvlc_release(vlc_instance_);
        media_ = nullptr;
        vlc_instance_ = nullptr;
        return false;
    }
    
    // Lấy thông tin video
    if (!initializeVideoInfo()) {
        std::cerr << "[ERROR] Không thể lấy thông tin video" << std::endl;
        release();
        return false;
    }
    
    is_opened_ = true;
    return true;
}

bool VLCVideoReader::initializeVideoInfo() {
    if (!media_player_ || !media_) {
        return false;
    }
    
    // Đợi một chút để media parse xong
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Lấy kích thước video
    unsigned int width = 0, height = 0;
    libvlc_video_get_size(media_player_, 0, &width, &height);
    
    if (width == 0 || height == 0) {
        // Thử parse lại media
        libvlc_media_parse_with_options(media_, libvlc_media_parse_local, -1);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        libvlc_video_get_size(media_player_, 0, &width, &height);
    }
    
    if (width == 0 || height == 0) {
        std::cerr << "[WARNING] Không thể lấy kích thước video, sử dụng giá trị mặc định" << std::endl;
        frame_width_ = 1920;
        frame_height_ = 1080;
    } else {
        frame_width_ = width;
        frame_height_ = height;
    }
    
    // Lấy FPS từ media tracks
    libvlc_media_track_t** tracks = nullptr;
    unsigned int track_count = 0;
    track_count = libvlc_media_tracks_get(media_, &tracks);
    
    fps_ = 30.0; // Default FPS
    if (tracks && track_count > 0) {
        for (unsigned int i = 0; i < track_count; i++) {
            if (tracks[i]->i_type == libvlc_track_video) {
                libvlc_video_track_t* video_track = tracks[i]->video;
                if (video_track) {
                    if (video_track->i_frame_rate_num > 0 && video_track->i_frame_rate_den > 0) {
                        fps_ = static_cast<double>(video_track->i_frame_rate_num) / 
                               static_cast<double>(video_track->i_frame_rate_den);
                    }
                    // Lấy kích thước từ track nếu chưa có
                    if (frame_width_ == 0 || frame_height_ == 0) {
                        frame_width_ = video_track->i_width;
                        frame_height_ = video_track->i_height;
                    }
                }
                break;
            }
        }
        libvlc_media_tracks_release(tracks, track_count);
    }
    
    // Tính tổng số frame dựa trên duration và fps
    libvlc_time_t duration_ms = libvlc_media_get_duration(media_);
    if (duration_ms > 0 && fps_ > 0) {
        total_frames_ = static_cast<int>((duration_ms / 1000.0) * fps_);
    }
    
    // Set callback để lấy frame (format sẽ được setup trong format_setup callback)
    libvlc_video_set_callbacks(media_player_, lock, unlock, display, this);
    libvlc_video_set_format_callbacks(media_player_, format_setup, format_cleanup);
    
    return true;
}

bool VLCVideoReader::isOpened() const {
    return is_opened_ && media_player_ != nullptr;
}

unsigned VLCVideoReader::format_setup(void** opaque, char* chroma, unsigned* width, unsigned* height, unsigned* pitches, unsigned* lines) {
    VLCVideoReader* reader = static_cast<VLCVideoReader*>(*opaque);
    
    // Set chroma format (RGB24 - reversed byte order for VLC)
    memcpy(chroma, "RV24", 4);
    
    // Use dimensions provided by VLC (these are the actual video dimensions)
    reader->frame_width_ = *width;
    reader->frame_height_ = *height;
    
    // Allocate frame buffer
    {
        std::lock_guard<std::mutex> lock(reader->frame_mutex_);
        reader->frame_buffer_ = cv::Mat(reader->frame_height_, reader->frame_width_, CV_8UC3);
    }
    
    // Set pitches and lines (pitch = bytes per line)
    unsigned pitch = reader->frame_width_ * 3; // RGB24 = 3 bytes per pixel
    *pitches = pitch;
    *lines = reader->frame_height_;
    
    reader->format_setup_ = true;
    
    return 1; // Return 1 to indicate success
}

void VLCVideoReader::format_cleanup(void* opaque) {
    // Cleanup if needed
}

void* VLCVideoReader::lock(void* data, void** p_pixels) {
    VLCVideoReader* reader = static_cast<VLCVideoReader*>(data);
    reader->frame_mutex_.lock();
    
    if (!reader->frame_buffer_.empty()) {
        *p_pixels = reader->frame_buffer_.data;
    }
    
    return data;
}

void VLCVideoReader::unlock(void* data, void* id, void* const* p_pixels) {
    VLCVideoReader* reader = static_cast<VLCVideoReader*>(data);
    reader->frame_ready_ = true;
    reader->frame_mutex_.unlock();
}

void VLCVideoReader::display(void* data, void* id) {
    // Frame displayed, can be used for synchronization if needed
}

bool VLCVideoReader::read(cv::Mat& frame) {
    if (!isOpened()) {
        return false;
    }
    
    // Start playing if paused or stopped
    libvlc_state_t state = libvlc_media_player_get_state(media_player_);
    if (state == libvlc_Stopped) {
        libvlc_media_player_play(media_player_);
        
        // Wait for format setup
        int setup_attempts = 0;
        while (!format_setup_ && setup_attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            setup_attempts++;
        }
        
        // Wait a bit for first frame
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } else if (state == libvlc_Paused) {
        // Resume playback to get next frame
        libvlc_media_player_set_pause(media_player_, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(30)); // Small delay for frame
    }
    
    // Check if video ended
    state = libvlc_media_player_get_state(media_player_);
    if (state == libvlc_Ended) {
        return false;
    }
    
    // Wait for frame to be ready
    frame_ready_ = false;
    int attempts = 0;
    while (!frame_ready_ && attempts < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        attempts++;
        
        // Check if ended while waiting
        state = libvlc_media_player_get_state(media_player_);
        if (state == libvlc_Ended) {
            return false;
        }
    }
    
    if (frame_ready_ && !frame_buffer_.empty()) {
        std::lock_guard<std::mutex> lock(frame_mutex_);
        // VLC provides RGB24, convert to BGR for OpenCV
        cv::cvtColor(frame_buffer_, frame, cv::COLOR_RGB2BGR);
        
        // Pause video after reading frame to allow step-by-step reading
        libvlc_media_player_set_pause(media_player_, 1);
        
        return true;
    }
    
    return false;
}

double VLCVideoReader::get(int prop) const {
    if (!isOpened()) {
        return -1.0;
    }
    
    switch (prop) {
        case cv::CAP_PROP_FRAME_WIDTH:
            return static_cast<double>(frame_width_);
        case cv::CAP_PROP_FRAME_HEIGHT:
            return static_cast<double>(frame_height_);
        case cv::CAP_PROP_FPS:
            return fps_;
        case cv::CAP_PROP_FRAME_COUNT:
            return static_cast<double>(total_frames_);
        case cv::CAP_PROP_POS_FRAMES: {
            if (!media_player_) return -1.0;
            libvlc_time_t time_ms = libvlc_media_player_get_time(media_player_);
            if (time_ms > 0 && fps_ > 0) {
                return (time_ms / 1000.0) * fps_;
            }
            return -1.0;
        }
        case cv::CAP_PROP_POS_MSEC: {
            if (!media_player_) return -1.0;
            return static_cast<double>(libvlc_media_player_get_time(media_player_));
        }
        default:
            return -1.0;
    }
}

bool VLCVideoReader::set(int prop, double value) {
    if (!isOpened()) {
        return false;
    }
    
    switch (prop) {
        case cv::CAP_PROP_POS_FRAMES: {
            if (fps_ > 0) {
                libvlc_time_t time_ms = static_cast<libvlc_time_t>((value / fps_) * 1000.0);
                libvlc_media_player_set_time(media_player_, time_ms);
                // Reset frame ready flag after seeking
                frame_ready_ = false;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                return true;
            }
            return false;
        }
        case cv::CAP_PROP_POS_MSEC: {
            libvlc_media_player_set_time(media_player_, static_cast<libvlc_time_t>(value));
            frame_ready_ = false;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            return true;
        }
        default:
            return false;
    }
}

void VLCVideoReader::release() {
    if (media_player_) {
        libvlc_media_player_stop(media_player_);
        libvlc_media_player_release(media_player_);
        media_player_ = nullptr;
    }
    
    if (media_) {
        libvlc_media_release(media_);
        media_ = nullptr;
    }
    
    if (vlc_instance_) {
        libvlc_release(vlc_instance_);
        vlc_instance_ = nullptr;
    }
    
    is_opened_ = false;
    frame_ready_ = false;
    format_setup_ = false;
    
    {
        std::lock_guard<std::mutex> lock(frame_mutex_);
        frame_buffer_.release();
    }
}

VLCVideoReader& VLCVideoReader::operator>>(cv::Mat& frame) {
    read(frame);
    return *this;
}


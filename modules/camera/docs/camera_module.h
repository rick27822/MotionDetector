#ifndef MOTION_DETECTOR_CAMERA_MODULE_H_
#define MOTION_DETECTOR_CAMERA_MODULE_H_

#include <array>
#include <cstdint>
#include <string>
#include <opencv2/core/mat.hpp>

// 4.2 摄像头与图像处理模块 v1.1，C++11+。
// 这是接口约定，没有 C++ 实现/工厂。现有实现是 Python CameraEngine，
// 不能直接链接本头文件；团队采用 C++ 时需移植或增加跨语言适配。
namespace motion_detector {
constexpr const char* kCameraApiVersion = "1.1";
enum class CameraSourceType { ComputerCamera, VideoFile, Phone, Demo };
enum class ColorFormat { BGR, RGB, Gray };
enum class PixelType { UInt8, Float32Normalized };
enum class Rotation { Deg0, Deg90Clockwise, Deg180, Deg270Clockwise };
enum class CameraState { Idle, Opening, Waiting, Running, Ended, Error, Stopping };
enum class CameraResult {
    Ok, Timeout, EndOfStream, NotRunning, InvalidArgument,
    SourceUnavailable, Disconnected, SessionChanged, Busy, InternalError
};

struct CaptureConfig {
    int width = 1280; // 整数16..3840；请求值，实际设备可能不支持。
    int height = 720; // 16..2160
    int fps = 30;    // 1..60；视频文件优先使用自身帧率。
};
struct PreprocessConfig {
    int width = 640;  // 16..1920
    int height = 640; // 16..1920
    Rotation rotation = Rotation::Deg0;
    bool mirror_horizontal = false;
    ColorFormat color = ColorFormat::RGB;
    PixelType pixel_type = PixelType::UInt8;
    std::uint8_t padding_value = 24;
    // 顺序：顺时针旋转 -> 水平镜像 -> 等比例缩放/居中补边 -> 颜色 -> 归一化。
    // padding_value 用于补边的全部三个通道。
};
struct CameraConfig {
    CameraSourceType source_type = CameraSourceType::ComputerCamera;
    int camera_index = 0;    // >=0，电脑摄像头使用。
    std::string video_path; // 视频文件使用；UTF-8，编码转换由实现负责。
    CaptureConfig capture;
    PreprocessConfig processing;
    // Phone 通过 BeginExternal/PushFrame 接入，不使用 Start。
};

struct CameraFrame {
    // RGB/BGR: CV_8UC3或CV_32FC3；Gray: CV_8UC1或CV_32FC1，二维单通道。
    // UInt8: [0,255]；Float32Normalized: [0,1]。
    cv::Mat image;
    cv::Mat source_bgr;  // 未处理原图，CV_8UC3，供叠加/录像。
    cv::Mat preview_bgr; // 处理后几何形状，彩色CV_8UC3。
    // 返回后生产者不再改写；数据生命周期独立于采集器。
    // 消费者修改像素前必须clone()，不得修改共享Mat。
    std::uint64_t frame_id = 0; // 从1开始，对象生命周期内递增，重启不清零。
    std::string session_id;     // 每次开始产生新的非空会话编号。
    std::string source;
    std::uint64_t config_revision = 0;
    std::int64_t received_unix_us = 0;      // 本机进入处理时Unix微秒。
    std::int64_t received_monotonic_us = 0; // 本机单调时钟微秒。
    double media_time_seconds = 0.0;       // 来源时间轴，不是跨设备同步时间。
    bool has_media_time = false;
    int source_width = 0;
    int source_height = 0;
    ColorFormat color = ColorFormat::RGB;
    PixelType pixel_type = PixelType::UInt8;
    double processing_ms = 0.0;
    // 行优先3×3仿射矩阵，原图 -> 输出图，整数(0,0)为首像素中心。
    // 包含旋转、镜像、缩放半像素偏移、补边。逆矩阵可还原坐标；
    // 模型归一化坐标须先按照模型定义转换为输出图像素坐标。
    std::array<double, 9> source_to_output = {{1,0,0, 0,1,0, 0,0,1}};
};
struct CameraStats {
    CameraState state = CameraState::Idle;
    CameraResult last_result = CameraResult::Ok;
    std::string message;
    std::string session_id;
    std::string source;
    double processing_fps = 0.0; // 实测，不是目标采集帧率。
    double frame_age_seconds = 0.0;
    bool has_frame_age = false;
    std::uint64_t total_frames = 0;
    std::uint64_t session_frames = 0;
    std::uint64_t config_revision = 0;
};
struct FrameRequest {
    std::uint64_t after_frame_id = 0;
    int timeout_ms = 0;              // >=0；0非阻塞。
    std::string expected_session_id; // 空表示接受当前会话。
};

class ICameraModule {
public:
    virtual ~ICameraModule() = default;
    // 先完整校验；无效参数不得停止当前采集。通过后停止旧源并异步打开
    // 新源。Ok仅表示已提交，后台失败见GetStats/ReadFrame。
    // 旧资源无法释放返回Busy，禁止启动第二条采集线程。
    virtual CameraResult Start(const CameraConfig& config) = 0;
    // 原子替换处理配置；在途帧仍可能带旧revision返回。无效配置保持原值。
    virtual CameraResult Configure(const PreprocessConfig& config) = 0;
    virtual PreprocessConfig GetProcessingConfig() const = 0;
    // 最新帧槽位，慢消费者允许跳帧，不保证录像逐帧无损。
    // 各消费者独立保存after_frame_id。文件末帧未读时先Ok，再EndOfStream。
    // 非Ok时out_frame不变。Stop/断开/切换会话必须唤醒等待者。
    // 停止返回NotRunning；断开返回Disconnected；会话改变返回SessionChanged。
    virtual CameraResult ReadFrame(const FrameRequest& request, CameraFrame& out_frame) = 0;
    // 保留原草案的便捷调用。
    CameraResult ReadFrame(std::uint64_t after_id, int timeout_ms, CameraFrame& out_frame) {
        FrameRequest request;
        request.after_frame_id = after_id;
        request.timeout_ms = timeout_ms;
        return ReadFrame(request, out_frame);
    }
    virtual CameraStats GetStats() const = 0;
    // 手机HTTPS/WebRTC由连接层实现。BeginExternal停止旧源、使用当前处理
    // 配置并生成令牌。PushFrame由单个生产者同步调用，输入必须CV_8UC3。
    // 调用期间不得改写bgr；返回后模块持有独立副本。
    virtual CameraResult BeginExternal(std::string& out_session_id) = 0;
    virtual CameraResult PushFrame(const cv::Mat& bgr, const std::string& session_id,
                                   bool has_media_time = false,
                                   double media_time_seconds = 0.0) = 0;
    // 空/旧令牌返回SessionChanged，不改变新会话。有效断开清空最新帧。
    virtual CameraResult EndExternal(const std::string& session_id) = 0;
    // 幂等停止、清空帧、令会话失效、释放资源、唤醒读者。
    // 读取/状态查询可并发；Start/Stop/BeginExternal/EndExternal由调用方串行。
    virtual CameraResult Stop() = 0;
    // 当前调用线程的同步错误补充文本；后台错误请查GetStats().message。
    virtual std::string LastError() const = 0;
};
} // namespace motion_detector
#endif // MOTION_DETECTOR_CAMERA_MODULE_H_

#include <fstream>
#include <deque>
#include <Poco/Stopwatch.h>
#include <Poco/Util/Application.h>
#include <Poco/Util/HelpFormatter.h>

#include "Common/Promise.h"
#include "Common/AbstractLogger.h"
#include "Common/LogMessage.h"
#include "Common/ThreadPool.h"
#include "Common/DmaHeapAllocateMethod.h"
#include "GPU/GL/GLDrawContex.h"
#include "GPU/Windows/AbstractWindows.h"
#include "GPU/Windows/WindowFactory.h"
#include "GPU/PG/AbstractSceneItem.h"
#include "GPU/PG/AbstractSceneLayer.h"
#include "GPU/PG/AbstractSceneCompositor.h"
#include "GPU/PG/Utility/CommonUtility.h"
#include "Codec/StreamFrame.h"
#include "Codec/CodecConfig.h"
#include "Codec/CodecFactory.h"
#include "Common/ImmutableVectorAllocateMethod.h"

#include "Display/AbstractDisplay.h"

using namespace Mmp;
using namespace Poco::Util;

constexpr uint32_t kBufSize = 1024 * 1024;

/**
 * @todo 好像挺烧 CPU, 用 `mmap` 并且优化 NAL UINT 的查找方式可能好一些
 */
class RkCacheFileByteReader
{
public:
    explicit RkCacheFileByteReader(const std::string& path);
    ~RkCacheFileByteReader();
public:
    NormalPack::ptr GetNalUint();
public:
    size_t Read(void* data, size_t bytes);
    bool Seek(size_t offset);
    size_t Tell();
    bool eof();
private:
    std::ifstream _ifs;
private:
    uint8_t* _buf;
    uint32_t _offset;
    uint32_t _cur;
    uint32_t _len;
};

NormalPack::ptr RkCacheFileByteReader::GetNalUint()
{
    std::vector<uint8_t> bufs;
    bufs.reserve(1024 * 1024);
    uint32_t next_24_bits = 0;
    bool isFirst = true;
    while (!(next_24_bits == 0x000001 && !isFirst))
    {
        if (next_24_bits == 0x000001)
        {
            isFirst = false;
            bufs.push_back(0);
            bufs.push_back(0);
            bufs.push_back(0);
            bufs.push_back(1);
        }
        uint8_t byte = 0;
        if (Read(&byte, 1) != 1 && eof())
        {
            return nullptr;
        }
        if (!isFirst)
        {
            bufs.push_back(byte);
        }
        next_24_bits = (next_24_bits << 8) | byte;
        next_24_bits = next_24_bits & 0xFFFFFF;
    }
    Seek(Tell() - 3);
    if (bufs.size() >= 3)
    {
        bufs.resize(bufs.size() - 3);
        if (!bufs.empty() && bufs[bufs.size()-1] == 0)
        {
            bufs.pop_back();
        }
    }
    std::shared_ptr<ImmutableVectorAllocateMethod<uint8_t>> alloc = std::make_shared<ImmutableVectorAllocateMethod<uint8_t>>();
    alloc->container.swap(bufs);
    return std::make_shared<NormalPack>(alloc->container.size(), alloc);
}

RkCacheFileByteReader::RkCacheFileByteReader(const std::string& path)
{
    _ifs.open(path, std::ios::in | std::ios::binary);
    if (!_ifs.is_open())
    {
        assert(false);
        exit(255);
    }
    _buf = new uint8_t[kBufSize];
    _offset = _ifs.tellg();
    _ifs.read((char*)_buf, kBufSize);
    _cur = 0;
    _len = _ifs.gcount();
}

RkCacheFileByteReader::~RkCacheFileByteReader()
{
    delete[] _buf;
    _ifs.close();
}

size_t RkCacheFileByteReader::Read(void* data, size_t bytes)
{
    if (_cur + bytes <= _len)
    {
        memcpy(data, _buf + _cur, bytes);
        _cur += bytes;
        return bytes;
    }
    else if (_ifs.eof())
    {
        memcpy(data, _buf + _cur, _len -  _cur);
        return _len -  _cur;
    }
    else
    {
        _offset = _ifs.tellg();
        _ifs.read((char*)_buf, kBufSize);
        _cur = 0;
        _len = _ifs.gcount();
        if (_len == 0) /* eof */
        {
            return 0;
        }
        else
        {
            return Read(data, bytes);
        }
    }
}

bool RkCacheFileByteReader::Seek(size_t offset)
{
    if (offset < _offset)
    {
        _ifs.seekg(offset);
        _offset = _ifs.tellg();
        _ifs.readsome((char*)_buf, kBufSize);
        _cur = 0;
        _len = _ifs.gcount(); 
        return _offset == offset;
    }
    else if (offset > _offset + kBufSize)
    {
        _ifs.seekg(offset);
        _offset = _ifs.tellg();
        _ifs.readsome((char*)_buf, kBufSize);
        _cur = 0;
        _len = _ifs.gcount(); 
        return _offset == offset;
    }
    else
    {
        _cur = offset - _offset;
        return true;
    }
}

size_t RkCacheFileByteReader::Tell()
{
    return _offset + _cur;
}

bool RkCacheFileByteReader::eof()
{
    return _len == 0 || (_ifs.eof() && _cur == _len);
}

/**
 * @sa MMP-Core/Extension/poco/Util/samples/SampleApp/src/SampleApp.cpp 
 */
class App : public Application
{
public:
    App();
public:
    void defineOptions(OptionSet& options) override;
protected:
    void initialize(Application& self);
    void uninitialize();
    void reinitialize(Application& self);
    void defineProperty(const std::string& def);
    int main(const ArgVec& args);
private:
    void HandleHelp(const std::string& name, const std::string& value);
    void HandleSrcCodecType(const std::string& name, const std::string& value);
    void HandleDstCodecType(const std::string& name, const std::string& value);
    void HandleInput(const std::string& name, const std::string& value);
    void HandleOutput(const std::string& name, const std::string& value);
    void HandleShow(const std::string& name, const std::string& value);
    void HandleFps(const std::string& name, const std::string& value);
    void HandleRateControlMode(const std::string& name, const std::string& value);
    void HandleBps(const std::string& name, const std::string& value);
    void HandleGop(const std::string& name, const std::string& value);
    void HandleCompositorWidth(const std::string& name, const std::string& value);
    void HandleCompositorHeight(const std::string& name, const std::string& value);
    void displayHelp();
public:
    std::string              decoderClassName;
    std::string              encoderClassName;
    std::string              inputFile;
    std::string              outputFile;
    uint32_t                 gop;
    Codec::RateControlMode   rcMode;
    uint64_t                 bps;
    bool                     show;
    uint32_t                 fps;
    uint32_t                 compositorWidth;
    uint32_t                 compositorHeight;
private: /* gpu */
    std::atomic<bool> _gpuInited;
    std::thread _renderThread;
    AbstractWindows::ptr _window;
    GLDrawContex::ptr    _draw;
public: /* decoder */
    std::mutex _decoderMtxs[4];
    std::condition_variable _decoderConds[4];
    Codec::StreamFrame::ptr _decoderStreamFrames[4];
    Codec::AbstractDecoder::ptr _decoders[4];
public:
    std::mutex _encoderMtx;
    std::condition_variable _encoderCond;
    Codec::StreamFrame::ptr _curEncoderFrame;
    Codec::AbstractEncoder::ptr _encoder;
public:
    std::mutex _displayMtx;
    std::condition_variable _displayCond;
    AbstractFrame::ptr _curDisplayFrame;
    AbstractDisplay::ptr _display;
public:
    Gpu::AbstractSceneCompositor::ptr compositor;
    Gpu::AbstractSceneLayer::ptr layer;
    Gpu::AbstractSceneItem::ptr items[4];
};

App::App()
{
    _gpuInited = false;
    bps = 4 * 1024 * 1024;
    gop = 60;
    rcMode = Codec::RateControlMode::CBR;
    show = true;
    fps = 30;
    compositorWidth = 1920;
    compositorHeight = 1080;
}

void App::displayHelp()
{
    AbstractLogger::LoggerSingleton()->Enable(AbstractLogger::Direction::CONSLOE);
    std::stringstream ss;
    HelpFormatter helpFormatter(options());
    helpFormatter.setWidth(1024);
    helpFormatter.setCommand(commandName());
    helpFormatter.setUsage("OPTIONS");
    helpFormatter.setHeader("Simple program to test rockchip 2x2 Compositor using MMP-Core.");
    helpFormatter.format(ss);
    MMP_LOG_INFO << ss.str();
    exit(0);
}

void App::HandleHelp(const std::string& name, const std::string& value)
{
    displayHelp();
}

void App::HandleRateControlMode(const std::string& name, const std::string& value)
{
    static std::map<std::string, Codec::RateControlMode> kLookup = 
    {
        {"vbr", Codec::RateControlMode::VBR},
        {"cbr", Codec::RateControlMode::CBR},
        {"fixqp", Codec::RateControlMode::FIXQP},
        {"avbr", Codec::RateControlMode::AVBR}
    };
    if (kLookup.count(value))
    {
        rcMode = kLookup[value];
    }
    else
    {
        assert(false);
        exit(-1);
    }
}

void App::HandleBps(const std::string& name, const std::string& value)
{
    bps = std::stoi(value);
}

void App::HandleGop(const std::string& name, const std::string& value)
{
    gop = std::stoi(value);
}

void App::HandleCompositorWidth(const std::string& name, const std::string& value)
{
    compositorWidth = std::stoi(value);
}

void App::HandleCompositorHeight(const std::string& name, const std::string& value)
{
    compositorHeight = std::stoi(value);
}

void App::HandleShow(const std::string& name, const std::string& value)
{
    if (value == "true")
    {
        show = true;
    }
    else if (value == "false")
    {
        show = false;
    }
}

void App::HandleSrcCodecType(const std::string& name, const std::string& value)
{
    static std::map<std::string, std::string> kLookup = 
    {
        {"h264", "RKH264Decoder"},
        {"hevc", ""}, // todo
        {"vp8", ""}, // todo
        {"vp9", ""}, // todo
        {"av1", ""} // av1
    };
    if (kLookup.count(value))
    {
        decoderClassName = kLookup[value];
    }
    else
    {
        assert(false);
        exit(-1);
    }
}

void App::HandleDstCodecType(const std::string& name, const std::string& value)
{
    static std::map<std::string, std::string> kLookup = 
    {
        {"h264", "RKH264Encoder"},
        {"hevc", ""}, // todo
        {"vp8", ""}, // todo
        {"vp9", ""}, // todo
        {"av1", ""} // av1
    };
    if (kLookup.count(value))
    {
        encoderClassName = kLookup[value];
    }
    else
    {
        assert(false);
        exit(-1);
    }
}

void App::HandleInput(const std::string& name, const std::string& value)
{
    inputFile = value;
}

void App::HandleOutput(const std::string& name, const std::string& value)
{
    outputFile = value;
}

void App::initialize(Application& self)
{
    loadConfiguration(); 
    Application::initialize(self);
    Codec::CodecConfig::Instance()->Init();
    // AbstractLogger::LoggerSingleton()->SetThreshold(AbstractLogger::Level::L_TRACE);
    AbstractLogger::LoggerSingleton()->Enable(AbstractLogger::Direction::CONSLOE);
    {
        _renderThread = std::thread([this]() -> void
        {
                GLDrawContex::SetGPUBackendType(GPUBackend::OPENGL_ES);
                _window = WindowFactory::DefaultFactory().createWindow("EGLWindowDefault");
                _window->SetRenderMode(false);
                _window->Open();
                _window->BindRenderThread(true);
                _draw = GLDrawContex::Instance();
                _draw->SetWindows(_window);
                _gpuInited = true;
                _draw->ThreadStart();
                while (true)
                {
                    GpuTaskStatus status;
                    status = _draw->ThreadFrame();
                    if (status == GpuTaskStatus::EXIT)
                    {
                        break;
                    }
                    else if (status == GpuTaskStatus::PRESENT)
                    {
                        _window->Swap();
                    }
                    else
                    {
                        continue;
                    }
                };
                _draw->ThreadEnd();
                _window->BindRenderThread(false);
                _window->Close();
        });
        while (!_gpuInited)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    }
}

void App::uninitialize()
{
    Codec::CodecConfig::Instance()->Uninit();
    Application::uninitialize();
    {
        _draw->ThreadStop();
        _renderThread.join();
    }
}

void App::reinitialize(Application& self)
{
    Application::reinitialize(self);
}

void App::defineOptions(OptionSet& options)
{
    Application::defineOptions(options);

    options.addOption(Option("help", "help", "帮助")
        .required(false)
        .repeatable(false)
        .callback(OptionCallback<App>(this, &App::HandleHelp))
    );
    options.addOption(Option("src_codec", "src_codec", "源编码类型, 可选 : h264, hevc, vp8, vp9, av1")
        .required(true)
        .repeatable(false)
        .argument("[codec_type]")
        .callback(OptionCallback<App>(this, &App::HandleSrcCodecType))
    );
    options.addOption(Option("dst_codec", "dst_codec", "目标编码类型, 可选 : h264, hevc, vp8, vp9, av1")
        .required(true)
        .repeatable(false)
        .argument("[codec_type]")
        .callback(OptionCallback<App>(this, &App::HandleDstCodecType))
    );
    options.addOption(Option("input", "i", "输入文件")
        .required(true)
        .repeatable(false)
        .argument("[filepath]")
        .callback(OptionCallback<App>(this, &App::HandleInput))
    );
    options.addOption(Option("output", "o", "输出文件")
        .required(true)
        .repeatable(false)
        .argument("[filepath]")
        .callback(OptionCallback<App>(this, &App::HandleOutput))
    );
    options.addOption(Option("group_of_picture", "gop", "I 帧间隔帧数, default 60")
        .required(false)
        .repeatable(false)
        .callback(OptionCallback<App>(this, &App::HandleGop))
        .argument("[num]")
    );
    options.addOption(Option("bitrate_per_second", "bps", "编码码率(bit), default 4MB/s")
        .required(false)
        .repeatable(false)
        .callback(OptionCallback<App>(this, &App::HandleBps))
        .argument("[num]")
    );
    options.addOption(Option("rate_control_mode", "rcmode", "码率控制模式, 可选: vbr, cbr, fixqp, avbr, default cbr")
        .required(false)
        .repeatable(false)
        .argument("[rcmode]")
        .callback(OptionCallback<App>(this, &App::HandleRateControlMode))
    );
    options.addOption(Option("display", "display", "是否显示, default true")
        .required(false)
        .repeatable(false)
        .argument("[show]")
        .callback(OptionCallback<App>(this, &App::HandleShow))
    );
    options.addOption(Option("compositor_width", "compositor_width", "合成宽度, default 1920")
        .required(false)
        .repeatable(false)
        .argument("[width]")
        .callback(OptionCallback<App>(this, &App::HandleCompositorWidth))
    );
    options.addOption(Option("compositor_height", "compositor_height", "合成高度, default 1080")
        .required(false)
        .repeatable(false)
        .argument("[height]")
        .callback(OptionCallback<App>(this, &App::HandleCompositorHeight))
    );
}

void App::defineProperty(const std::string& def)
{
    std::string name;
    std::string value;
    std::string::size_type pos = def.find('=');
    if (pos != std::string::npos)
    {
        name.assign(def, 0, pos);
        value.assign(def, pos + 1, def.length() - pos);
    }
    else name = def;
    config().setString(name, value);
}

/********************************************************* TEST(BEGIN) *****************************************************/

int App::main(const ArgVec& args)
{
    //
    // Hint : Compositor 涉及的多线程上下文比较复杂, context 统一放到 App number,
    //        跟 test_decoder、test_encoder、test_transcode 有所区别,
    //        但是整体调用流程还是完整写在 main 中, 便于理解查看
    //

    {
        MMP_LOG_INFO << "Compositor config";
        MMP_LOG_INFO << "-- encoder name : " << encoderClassName;
        MMP_LOG_INFO << "-- decoder name : " << decoderClassName;
        MMP_LOG_INFO << "-- input is: " << inputFile;
        MMP_LOG_INFO << "-- output is: " << outputFile;
        MMP_LOG_INFO << "-- bit per second is: " << bps;
        MMP_LOG_INFO << "-- rate control mode : " << rcMode;
        MMP_LOG_INFO << "-- gop is: " << gop;
        MMP_LOG_INFO << "-- show is: " << show;
        MMP_LOG_INFO << "-- compositor width is: " << compositorWidth;
        MMP_LOG_INFO << "-- compositor height is: " << compositorHeight;
    }
    std::atomic<bool> running(true);
    std::atomic<uint32_t> _decoderReachFileEndNum(0);
    constexpr uint32_t decoderNum = 4;

    std::vector<std::thread*> _threads;

    uint32_t curSlot = 0;

    //
    // 流水线结构
    // Input File Read -> VDEC PUSH (*4)
    //                    VDEC POP -> Send FRAME (*4)
    //                                RECEIVE FRAME And COMPOSITOR ->  SEND FRAME
    //                                                                 RECEIVE FRAME and DISPLAY
    //                                                                 RECEIVE FRAME -> VENC PUSH
    //                                                                                  VENC POP -> Output File Write
    //
    // 流水线最大长度为 5, 整体使用线程数量 12(4 + 4 + 1 + 1 + 1 + 1)条；
    // 流控由 COMPOSITOR 控制,按照固定 fps 合成, 其他环节由 mtx 和 cond 形成正向或反向压制
    // (实际上如果场景更为复杂, 最好是由统一线程池管理调度, 不过单独起线程便于理解逻辑行为)
    // 
    // 其他:
    // 本示例还是挺有意思的,展示了一种通过 DMA BUF 实现无拷贝的画面合成方式,
    // 数据链路为 VDEC -> GPU -> VENC
    // 而非 VDEC -> CPU -> GPU -> CPU -> VENC
    // 同时依托于 ARM MALI 的 GPU 涉及, 可以全链路使用 NV12 进行传输, 避免 YUV 与 RGB 的互转
    // 

    /*********************************** 解码线程(Begin) ******************************/
    for (uint32_t i=0; i<decoderNum; i++)
    {
        _decoders[i] =  Codec::DecoderFactory::DefaultFactory().CreateDecoder(decoderClassName);
    }
    // Decoder Push
    for (uint32_t i=0; i<decoderNum; i++)
    {
        std::thread* thread = new std::thread([this, &running, &_decoderReachFileEndNum, slot = i]()
        {
            std::shared_ptr<RkCacheFileByteReader> byteReader = std::make_shared<RkCacheFileByteReader>(inputFile);
            Codec::AbstractDecoder::ptr decoder = _decoders[slot];
            decoder->Init();
            decoder->Start();
            NormalPack::ptr pack = nullptr;
            do
            {
                pack = byteReader->GetNalUint();
                if (pack)
                {
                    decoder->Push(pack);
                }
            } while (pack && running);
            _decoderReachFileEndNum++;
            decoder->Stop();
            decoder->Uninit();
        });
        thread->detach();
        _threads.push_back(thread);
    }
    // Decoder Pop
    for (uint32_t i=0; i<decoderNum; i++)
    {
        std::thread* thread = new std::thread([this, &running, slot = i]()
        {
            Codec::AbstractDecoder::ptr decoder = _decoders[slot];
            std::mutex& decoderMtx = _decoderMtxs[slot];
            std::condition_variable& decoderCond = _decoderConds[slot];
            Codec::StreamFrame::ptr& curframe = _decoderStreamFrames[slot]; 
            while (running)
            {
                AbstractFrame::ptr frame;
                if (decoder->Pop(frame))
                {
                    std::unique_lock<std::mutex> lock(decoderMtx);
                    if (curframe)
                    {
                        //
                        // Hint : 存在两种唤醒条件
                        //        1 - 数据被消费, 需要再生产数据
                        //        2 - 线程退出
                        //
                        decoderCond.wait(lock);
                        if (!running)
                        {
                            break;
                        }
                    }
                    curframe = std::dynamic_pointer_cast<Codec::StreamFrame>(frame);
                }
                else
                {
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
            }
        });
        thread->detach();
        _threads.push_back(thread);
    }
    /*********************************** 解码线程(End) ******************************/
    /***************************************** 渲染线程(Begin) ****************************************/
    {
        std::thread* thread = new std::thread([this, &running]()
        {
            _display = AbstractDisplay::Create();
            if (_display)
            {
                _display->Init();
                bool isFirst = true;
                while (running)
                {
                    AbstractFrame::ptr frame;
                    {
                        std::unique_lock<std::mutex> lock(_displayMtx);
                        if (!_curDisplayFrame)
                        {
                            _displayCond.wait(lock);
                        }
                        frame = _curDisplayFrame;
                        _curDisplayFrame = nullptr;
                    }
                    if (isFirst)
                    {
                        Codec::StreamFrame::ptr streamFrame = std::dynamic_pointer_cast<Codec::StreamFrame>(frame);
                        AbstractPicture::ptr pictureFrame = std::dynamic_pointer_cast<AbstractPicture>(frame);
                        if (streamFrame)
                        {
                            _display->Open(streamFrame->info);
                        }
                        isFirst = false;
                    }
                    if (frame)
                    {
                        _display->UpdateWindow((const uint32_t*)frame->GetData(0));
                    }
                }
                _display->Close();
                _display->UnInit();
            }
        });
        thread->detach();
        _threads.push_back(thread);
    }
    /***************************************** 渲染线程(End) ****************************************/
    /***************************************** 编码线程(Begin) ****************************************/
    _encoder = Codec::EncoderFactory::DefaultFactory().CreateEncoder(encoderClassName);
    // Enoder Push
    {
        std::thread* thread = new std::thread([this, &running]()
        {
            {
                _encoder->SetParameter(rcMode, Codec::kRateControlMode);
                _encoder->SetParameter(gop, Codec::kGop);
                _encoder->SetParameter(bps, Codec::kBps);
            }
            _encoder->Init();
            _encoder->Start();
            while (running)
            {
                Codec::StreamFrame::ptr frame;
                {
                    std::unique_lock<std::mutex> lock(_encoderMtx);
                    if (!_curEncoderFrame)
                    {
                        _encoderCond.wait(lock);
                    }
                    frame = _curEncoderFrame;
                    _curEncoderFrame = nullptr;
                }
                if (frame)
                {
                    _encoder->Push(frame);
                }
            }
            _encoder->Stop();
            _encoder->Uninit();
        });
        thread->detach();
        _threads.push_back(thread);
    }
    // Encoder Pop
    {
        std::thread* thread = new std::thread([this, &running]()
        {
            std::ofstream ofs(outputFile);
            while (running || _encoder->CanPop())
            {
                AbstractPack::ptr pack;
                if (_encoder->Pop(pack))
                {
                    ofs.write((char*)pack->GetData(0), pack->GetSize());
                    // MMP_LOG_INFO << "Pop, addresss is: " << pack->GetData(0) << ", size is: " << pack->GetSize();
                }
                else
                {
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
            }
            ofs.flush();
            ofs.close();
        });
        thread->detach();
        _threads.push_back(thread);
    }
    /***************************************** 编码线程(End) ****************************************/
    /***************************************** 合成线程(End) ****************************************/
    {
        std::thread* thread = new std::thread([this, &running]()
        {
            // 
            // Compositor
            //            -> Layer
            //                     -> Item0
            //                     -> Item1
            //                     -> Item2
            //                     -> Item3
            //
            
            AbstractPicture::ptr frameBuffer;
            {
                compositor = Gpu::AbstractSceneCompositor::Create();
                {
                    {
                        Gpu::SceneCompositorParam param;
                        param.width = compositorWidth;
                        param.height = compositorHeight;
                        param.bufSize = 3;
                        param.flags = GlTextureFlags::TEXTURE_USE_FOR_RENDER | GlTextureFlags::TEXTURE_EXTERNAL | GlTextureFlags::TEXTURE_YUV;
                        compositor->SetParam(param);
                    }
                }
                layer = Gpu::AbstractSceneLayer::Create();
                {
                    {
                        Gpu::SceneLayerParam param;
                        param.height = compositorWidth;
                        param.width = compositorHeight;
                        layer->SetParam(param);
                    }
                    compositor->AddSceneLayer("Layer", layer);
                }
                for (uint32_t i=0; i<decoderNum; i++)
                {
                    items[i] = Gpu::AbstractSceneItem::Create();
                    Gpu::SceneItemParam param = {};
                    param.area = NormalizedRect(0.5f, 0.5f);
                    switch (i)
                    {
                        case 0:
                        {
                            param.location = NormalizedPoint(0.0f, 0.0f);
                            break;
                        }
                        case 1:
                        {
                            param.location = NormalizedPoint(0.0f, 0.5f);
                            break;
                        }
                        case 2:
                        {
                            param.location = NormalizedPoint(0.5f, 0.0f);
                            break;
                        }
                        case 3:
                        {
                            param.location = NormalizedPoint(0.5f, 0.5f);
                            break;
                        }
                        default:
                        {
                            assert(false);
                            break;
                        }
                    }
                    items[i]->SetParam(param);
                    layer->AddSceneItem(std::string() + "item" + "_" + std::to_string(i), items[i]);
                }
            }

            while (running || _encoder->CanPop())
            {
                Poco::Stopwatch sw;
                sw.start();
                Codec::StreamFrame::ptr decodersFrames[4];
                Codec::StreamFrame::ptr compositorFrame;
                // 反向压制
                {
                    uint32_t curFrame = 0;
                    while (running && curFrame != decoderNum)
                    {
                        for (uint32_t i=0; i<decoderNum; i++)
                        {
                            if (!decodersFrames[i])
                            {
                                std::lock_guard<std::mutex> lock(_decoderMtxs[i]);
                                if (_decoderStreamFrames[i])
                                {
                                    decodersFrames[i] = _decoderStreamFrames[i];
                                    _decoderStreamFrames[i] = nullptr;
                                    _decoderConds[i].notify_one();
                                    curFrame++;
                                }
                            }
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                }
                // 合成
                {
                    // MMP_LOG_INFO << "Compositor Begin";
                    for (uint32_t i=0; i<decoderNum; i++)
                    {
                        Texture::ptr texture = Gpu::Create2DTextures(_draw, decodersFrames[i]->info, "", GlTextureFlags::TEXTURE_EXTERNAL | GlTextureFlags::TEXTURE_YUV)[0];
                        {
                            TextureDesc desc;
                            AbstractPicture::ptr picFrame = std::make_shared<NormalPicture>(decodersFrames[i]->info, decodersFrames[i]->GetAllocateMethod());
                            Gpu::Update2DTextures(_draw, {texture}, picFrame);
                        }
                        items[i]->UpdateImage(texture);
                    }
                    compositor->Draw();
                    {

                        DmaHeapAllocateMethod::ptr alloc = std::dynamic_pointer_cast<DmaHeapAllocateMethod>(compositor->GetFrameBuffer()->GetAllocateMethod());
                        PixelsInfo info;
                        info.width = compositorWidth;
                        info.height = compositorHeight;
                        info.format = PixelFormat::NV12;
                        Codec::StreamFrame::ptr frame = std::make_shared<Codec::StreamFrame>(info, alloc);
                        compositorFrame = frame;
                    }
                    // MMP_LOG_INFO << "Compositor End";
                }
                // 正向压制
                {
                    if (compositorFrame)
                    {
                        {
                            std::lock_guard<std::mutex> lock(_displayMtx);
                            _curDisplayFrame = compositorFrame;
                            _displayCond.notify_one();
                        }
                        {
                            std::lock_guard<std::mutex> lock(_encoderMtx);
                            _curEncoderFrame = compositorFrame;
                            _encoderCond.notify_one();
                        }  
                    }
                }
                // 简易流控
                {
                    uint32_t sleepTime = 1000000 / fps;
                    if (sw.elapsed() > sleepTime)
                    {
                        MMP_LOG_WARN << "Compositor process too low";
                    }
                    else
                    {
                        sleepTime -= sw.elapsed();
                        std::this_thread::sleep_for(std::chrono::microseconds(sleepTime));
                    }
                }
            }
            _displayCond.notify_one();
            _encoderCond.notify_one();
            for (uint32_t i=0; i<decoderNum; i++)
            {
                _decoderConds[i].notify_all();
            }
            for (uint32_t i=0; i<decoderNum; i++)
            {
                items[i].reset();
            }
            layer.reset();
            compositor.reset();
        });
        thread->detach();
        _threads.push_back(thread);
    }
    /***************************************** 合成线程(End) ****************************************/


    while (_decoderReachFileEndNum != decoderNum)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    running = true;

    for (auto& thread : _threads)
    {
        thread->join();
        delete thread;
    }

    return 0;
}

/********************************************************* TEST(END) *****************************************************/

POCO_APP_MAIN(App)
#include <fstream>
#include <deque>
#include <Poco/Stopwatch.h>
#include <Poco/Util/Application.h>
#include <Poco/Util/HelpFormatter.h>

#include "Common/Promise.h"
#include "Common/AbstractLogger.h"
#include "Common/LogMessage.h"
#include "Common/ThreadPool.h"
#include "GPU/GL/GLDrawContex.h"
#include "GPU/Windows/AbstractWindows.h"
#include "GPU/Windows/WindowFactory.h"
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
private: /* gpu */
    std::atomic<bool> _gpuInited;
    std::thread _renderThread;
    AbstractWindows::ptr _window;
    GLDrawContex::ptr    _draw;
public:
    std::mutex _decoderMtx;
    std::map<uint32_t, Codec::AbstractDecoder::ptr> _decoders;
    std::map<uint32_t, std::deque<Codec::AbstractEncoder::ptr>> _decodersStreamFrames;
public:
    std::mutex _encoderMtx;
    std::condition_variable _encoderCond;
    Codec::StreamFrame::ptr _curEncoderFrame;
    Codec::AbstractEncoder::ptr _encoder;
public:
    std::mutex _displayMtx;
    std::condition_variable _displayCond;
    Codec::StreamFrame::ptr _curDisplayFrame;
    AbstractDisplay::ptr _display;
public: /* TODO : compositor */
};

App::App()
{
    _gpuInited = false;
    bps = 4 * 1024 * 1024;
    gop = 60;
    rcMode = Codec::RateControlMode::CBR;
    show = true;
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
    ThreadPool::ThreadPoolSingleton()->Init();
    Application::initialize(self);
    Codec::CodecConfig::Instance()->Init();
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
    ThreadPool::ThreadPoolSingleton()->Uninit();
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
    //        但是整体调用流程还是完整卸载 main 中, 便于理解查看
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
    }


    return 0;
}

/********************************************************* TEST(END) *****************************************************/

POCO_APP_MAIN(App)
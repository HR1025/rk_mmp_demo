#include <fstream>
#include <Poco/Stopwatch.h>
#include <Poco/Util/Application.h>
#include <Poco/Util/HelpFormatter.h>

#include "Common/Promise.h"
#include "Common/AbstractLogger.h"
#include "Common/LogMessage.h"
#include "Common/ThreadPool.h"
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
    void HandleCodecType(const std::string& name, const std::string& value);
    void HandleInput(const std::string& name, const std::string& value);
    void HandleShow(const std::string& name, const std::string& value);
    void HandleFps(const std::string& name, const std::string& value);
    void displayHelp();
public:
    std::string              decoderClassName;
    std::string              inputFile;
    bool                     show;
    uint64_t                 fps;
};

App::App()
{
    show = true;
    fps = 30;
}

void App::displayHelp()
{
    AbstractLogger::LoggerSingleton()->Enable(AbstractLogger::Direction::CONSLOE);
    std::stringstream ss;
    HelpFormatter helpFormatter(options());
    helpFormatter.setWidth(1024);
    helpFormatter.setCommand(commandName());
    helpFormatter.setUsage("OPTIONS");
    helpFormatter.setHeader("Simple program to test rockchip encoder using MMP-Core.");
    helpFormatter.format(ss);
    MMP_LOG_INFO << ss.str();
    exit(0);
}

void App::HandleHelp(const std::string& name, const std::string& value)
{
    displayHelp();
}

void App::HandleCodecType(const std::string& name, const std::string& value)
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

void App::HandleFps(const std::string& name, const std::string& value)
{
    fps = std::stoi(value);
}

void App::HandleInput(const std::string& name, const std::string& value)
{
    inputFile = value;
}

void App::initialize(Application& self)
{
    loadConfiguration(); 
    ThreadPool::ThreadPoolSingleton()->Init();
    Application::initialize(self);
    Codec::CodecConfig::Instance()->Init();
    AbstractLogger::LoggerSingleton()->Enable(AbstractLogger::Direction::CONSLOE);
}

void App::uninitialize()
{
    Codec::CodecConfig::Instance()->Uninit();
    Application::uninitialize();
    ThreadPool::ThreadPoolSingleton()->Uninit();
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
    options.addOption(Option("codec", "codec", "编码类型, 可选 : h264, hevc, vp8, vp9, av1")
        .required(true)
        .repeatable(false)
        .argument("[codec_type]")
        .callback(OptionCallback<App>(this, &App::HandleCodecType))
    );
    options.addOption(Option("input", "i", "输入文件")
        .required(true)
        .repeatable(false)
        .argument("[filepath]")
        .callback(OptionCallback<App>(this, &App::HandleInput))
    );
    options.addOption(Option("display", "display", "是否显示, default true")
        .required(false)
        .repeatable(false)
        .argument("[show]")
        .callback(OptionCallback<App>(this, &App::HandleShow))
    );
    options.addOption(Option("fps", "fps", "显示帧率, default 30")
        .required(false)
        .repeatable(false)
        .argument("[num]")
        .callback(OptionCallback<App>(this, &App::HandleFps))
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
    AbstractDisplay::ptr display;
    Codec::AbstractDecoder::ptr decoder = Codec::DecoderFactory::DefaultFactory().CreateDecoder(decoderClassName);
    {
        MMP_LOG_INFO << "Decoder config";
        MMP_LOG_INFO << "-- codec name : " << decoderClassName;
        MMP_LOG_INFO << "-- input :  " << inputFile;
        MMP_LOG_INFO << "-- display : " << (show ? "true" : "false");
        MMP_LOG_INFO << "-- fps : " << fps;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }


    if (!decoder)
    {
        MMP_LOG_INFO << "Rebuild with -DUSE_ROCKCHIP=ON, see README for detail.";
        return 0;
    }
    decoder->Init();
    decoder->Start();

    if (show)
    {
        display = AbstractDisplay::Create();
    }
    if (display)
    {
        display->Init();
    }
    std::shared_ptr<RkCacheFileByteReader> byteReader = std::make_shared<RkCacheFileByteReader>(inputFile);
    NormalPack::ptr pack = nullptr;
    /***************************************** 渲染线程(Begin) ****************************************/
    std::atomic<bool> running(true);
    std::atomic<bool> sync(false);
    Promise<void>::ptr displayTask = std::make_shared<Promise<void>>([&]()
    {
        uint64_t intervalMs = 1000 / fps;
        Poco::Stopwatch sw;
        sw.start();
        bool first = true;
        while (running)
        {
            AbstractFrame::ptr frame;
            if (decoder->Pop(frame))
            {
                MMP_LOG_INFO << "AbstractDisplay Pop";
                Codec::StreamFrame::ptr streamFrame = std::dynamic_pointer_cast<Codec::StreamFrame>(frame);
                if (display && first)
                {
                    display->Open(streamFrame->info);
                    first = false;
                }
                if (display)
                {
                    display->UpdateWindow((const uint32_t*)streamFrame->GetData(0));
                    if (intervalMs > sw.elapsed() / 1000)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs - sw.elapsed() / 1000));
                    }
                    else
                    {
                        MMP_LOG_WARN << "Process too slow!!!";
                    }                    
                    sw.restart();
                }
            }
            sync = true;
        } 
    });
    ThreadPool::ThreadPoolSingleton()->Commit(displayTask);
    /***************************************** 渲染线程(End) ****************************************/
    /*********************************** 解码线程(Begin) ******************************/
    do
    {
        pack = byteReader->GetNalUint();
        if (pack)
        {
            MMP_LOG_INFO << "AbstractDisplay Push";
            decoder->Push(pack);
        }
    } while (pack);
    /*********************************** 解码线程(End) ******************************/

    if (display)
    {
        display->Close();
        display->UnInit();
    }

    running = false;
    while (!sync)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    decoder->Stop();
    decoder->Uninit();
    return 0;
}

/********************************************************* TEST(END) *****************************************************/

POCO_APP_MAIN(App)
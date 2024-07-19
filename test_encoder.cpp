#include <fstream>
#include <Poco/Stopwatch.h>
#include <Poco/Util/Application.h>
#include <Poco/Util/HelpFormatter.h>

#include "Common/AbstractLogger.h"
#include "Common/LogMessage.h"
#include "Common/DmaHeapAllocateMethod.h"
#include "Codec/CodecConfig.h"
#include "Codec/CodecFactory.h"

using namespace Mmp;
using namespace Poco::Util;

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
    void HandleRateControlMode(const std::string& name, const std::string& value);
    void HandleBps(const std::string& name, const std::string& value);
    void HandleWidth(const std::string& name, const std::string& value);
    void HandleHeight(const std::string& name, const std::string& value);
    void HandleOutput(const std::string& name, const std::string& value);
    void HandleGop(const std::string& name, const std::string& value);
    void HandleMemType(const std::string& name, const std::string& value);
    void displayHelp();
public:
    std::string              decoderClassName;
    uint32_t                 gop;
    Codec::RateControlMode   rcMode;
    uint64_t                 bps;
    uint32_t                 width;
    uint32_t                 height;
    uint64_t                 loopTime;
    std::string              outputFile;
    uint32_t                 memtype;
};

App::App()
{
    memtype = 0;
    bps = 4 * 1024 * 1024;
    gop = 60;
    rcMode = Codec::RateControlMode::CBR;
    width = 1920;
    height = 1080;
    loopTime = gop * 20;
    outputFile = "./test_encoder.dmp";
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
        {"h264", "RKH264Encoder"},
        {"hevc", "RKH265Encoder"},
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

void App::HandleWidth(const std::string& name, const std::string& value)
{
    width = std::stol(value);
}

void App::HandleHeight(const std::string& name, const std::string& value)
{
    height = std::stol(value);
}

void App::HandleOutput(const std::string& name, const std::string& value)
{
    outputFile = value;
}

void App::HandleGop(const std::string& name, const std::string& value)
{
    gop = std::stoi(value);
}

void App::HandleMemType(const std::string& name, const std::string& value)
{
    memtype = std::stoi(value);
}

void App::initialize(Application& self)
{
    loadConfiguration(); 
    Application::initialize(self);
    Codec::CodecConfig::Instance()->Init();
    AbstractLogger::LoggerSingleton()->Enable(AbstractLogger::Direction::CONSLOE);
}

void App::uninitialize()
{
    Codec::CodecConfig::Instance()->Uninit();
    Application::uninitialize();
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
    options.addOption(Option("group_of_picture", "gop", "I 帧间隔帧数")
        .required(false)
        .repeatable(false)
        .callback(OptionCallback<App>(this, &App::HandleGop))
        .argument("[num]")
    );
    options.addOption(Option("bitrate_per_second", "bps", "编码码率,单位 bit")
        .required(false)
        .repeatable(false)
        .callback(OptionCallback<App>(this, &App::HandleBps))
        .argument("[num]")
    );
    options.addOption(Option("rate_control_mode", "rcmode", "码率控制模式, 可选: vbr, cbr, fixqp, avbr")
        .required(false)
        .repeatable(false)
        .argument("[rcmode]")
        .callback(OptionCallback<App>(this, &App::HandleRateControlMode))
    );
    options.addOption(Option("width", "width", "宽, default 1920")
        .required(false)
        .repeatable(false)
        .argument("[width]")
        .callback(OptionCallback<App>(this, &App::HandleWidth))
    );
    options.addOption(Option("height", "height", "高, default 1080")
        .required(false)
        .repeatable(false)
        .argument("[height]")
        .callback(OptionCallback<App>(this, &App::HandleHeight))
    );
    options.addOption(Option("output", "o", "输出文件, default ./test_encoder.dmp")
        .required(false)
        .repeatable(false)
        .argument("[filepath]")
        .callback(OptionCallback<App>(this, &App::HandleOutput))
    );
    options.addOption(Option("memtype", "memtype", "内存类型, 0 - normal, 1 - dma, default 0")
        .required(false)
        .repeatable(false)
        .argument("[type]")
        .callback(OptionCallback<App>(this, &App::HandleMemType))
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
    Codec::AbstractEncoder::ptr encoder = Codec::EncoderFactory::DefaultFactory().CreateEncoder(decoderClassName);
    if (!encoder)
    {
        MMP_LOG_INFO << "Rebuild with -DUSE_ROCKCHIP=ON, see README for detail.";
        return 0;
    }
    {
        MMP_LOG_INFO << "Encoder config";
        MMP_LOG_INFO << "-- codec name : " << decoderClassName;
        MMP_LOG_INFO << "-- rate control mode : " << rcMode;
        MMP_LOG_INFO << "-- gop : " << gop;
        MMP_LOG_INFO << "-- bps : " << bps; 
        MMP_LOG_INFO << "-- width : " << width;
        MMP_LOG_INFO << "-- height : " << height;
        MMP_LOG_INFO << "-- output : " << outputFile;
        MMP_LOG_INFO << "-- memtype : " << memtype;
    }
    {
        encoder->SetParameter(rcMode, Codec::kRateControlMode);
        encoder->SetParameter(gop, Codec::kGop);
        encoder->SetParameter(bps, Codec::kBps);
    }
    encoder->Init();
    encoder->Start();

    Codec::StreamFrame::ptr rgbFrame = std::make_shared<Codec::StreamFrame>(PixelsInfo(width, height, 8, PixelFormat::RGB888));
    AbstractAllocateMethod::ptr alloc;
    if (memtype == 1)
    {
        //
        // Hint : DMA-HEAP 是一种特殊的内存分配方式
        // See also : MMP-Core/Common/DmaHeapAllocateMethod.cpp
        //
        alloc = std::make_shared<DmaHeapAllocateMethod>();
    }
    Codec::StreamFrame::ptr yuvFrame = std::make_shared<Codec::StreamFrame>(PixelsInfo(width, height, 8, PixelFormat::NV12), alloc);
    // Init RGB
    {
        uint8_t* rgbImage = (uint8_t*)rgbFrame->GetData(0);
        uint32_t offset = 0;
        for (uint32_t i=0; i<width*height; i++)
        {
            rgbImage[offset++] = 0x66;
            rgbImage[offset++] = 0xCC;
            rgbImage[offset++] = 0xFF;
        }
    }
    // RGB TO NV12
    {
        uint8_t* rgbImage = (uint8_t*)rgbFrame->GetData(0);
        uint8_t* yuvImage = (uint8_t*)yuvFrame->GetData(0);
        uint64_t imageSize = width * height;
        uint64_t rgbIndex = 0;
        uint64_t yIndex = 0;
        uint64_t uvIndex = imageSize;
        for (uint64_t y = 0; y < height; y++) 
        {
            for (uint64_t x = 0; x < width; x++) 
            {
                uint8_t R = rgbImage[rgbIndex++];
                uint8_t G = rgbImage[rgbIndex++];
                uint8_t B = rgbImage[rgbIndex++];
#if 1 // BT709
                uint8_t Y = 0.2126 * R + 0.7152 * G + 0.0722 * B;
                uint8_t U = -0.1146 * R - 0.3854 * G + 0.5 * B + 128;
                uint8_t V = 0.5 * R - 0.4542 * G - 0.0458 * B + 128;
#else // BT601
                uint8_t Y = 0.299 * R + 0.587 * G + 0.114 * B;
                uint8_t U = -0.169 * R - 0.331 * G + 0.5 * B + 128;
                uint8_t V = 0.5 * R - 0.419 * G - 0.081 * B + 128;
#endif
                yuvImage[yIndex++] = Y;
                if (y % 2 == 0 && x % 2 == 0) 
                {

                    yuvImage[uvIndex++] = U;
                    yuvImage[uvIndex++] = V;
                }
            }
        }
    }

    std::ofstream ofs(outputFile);
    for (uint64_t i=0; i<loopTime; i++)
    {
        Poco::Stopwatch sw;
        sw.start();
        encoder->Push(yuvFrame);
        // MMP_LOG_INFO << "Push , cur is: " << i << ", cost time is: " << sw.elapsed() / 1000 << " ms";
        while (encoder->CanPop())
        {
            AbstractPack::ptr pack;
            if (encoder->Pop(pack))
            {
                ofs.write((char*)pack->GetData(0), pack->GetSize());
                // MMP_LOG_INFO << "Pop, addresss is: " << pack->GetData(0) << ", size is: " << pack->GetSize();
            } 
        }
    }
    ofs.flush();
    ofs.close();

    encoder->Stop();
    encoder->Uninit();
    return 0;
}

/********************************************************* TEST(END) *****************************************************/

POCO_APP_MAIN(App)
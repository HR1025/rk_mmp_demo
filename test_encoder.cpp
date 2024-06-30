#include <Poco/Util/Application.h>
#include <Poco/Util/HelpFormatter.h>

#include "Common/AbstractLogger.h"
#include "Common/LogMessage.h"
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
    void HandleGop(const std::string& name, const std::string& value);
    void displayHelp();
public:
    std::string              encoderClassName;
    uint32_t                 gop;
    Codec::RateControlMode   rcMode;
    uint64_t                 bps;
};

App::App()
{
    bps = 4 * 1024 * 1024;
    gop = 60;
    rcMode = Codec::RateControlMode::CBR;
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

    options.addOption(Option("help", "h", "帮助")
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
    Codec::AbstractEncoder::ptr encoder = Codec::EncoderFactory::DefaultFactory().CreateEncoder(encoderClassName);
    if (!encoder)
    {
        MMP_LOG_INFO << "Rebuild with -DUSE_ROCKCHIP=ON, see README for detail.";
        return 0;
    }
    {
        encoder->SetParameter(rcMode, Codec::kRateControlMode);
        encoder->SetParameter(gop, Codec::kGop);
        encoder->SetParameter(bps, Codec::kBps);
        MMP_LOG_INFO << "Encoder config";
        MMP_LOG_INFO << "-- codec name : " << encoderClassName;
        MMP_LOG_INFO << "-- rate control mode : " << rcMode;
        MMP_LOG_INFO << "-- gop : " << gop;
        MMP_LOG_INFO << "-- bps : " << bps; 
    }
    encoder->Init();
    encoder->Start();

    encoder->Stop();
    encoder->Uninit();
    return 0;
}

/********************************************************* TEST(END) *****************************************************/

POCO_APP_MAIN(App)
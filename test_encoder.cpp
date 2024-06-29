#include <Poco/Util/Application.h>

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
    void defineOptions(OptionSet& options) override;
protected:
    void initialize(Application& self);
    void uninitialize();
    void reinitialize(Application& self);
    void defineProperty(const std::string& def);
    int main(const ArgVec& args);
private:
    void HandleCodecType(const std::string& name, const std::string& value);
public:
    std::string encoderClassName;
};

void App::HandleCodecType(const std::string& name, const std::string& value)
{
    if (value == "h264")
    {
        encoderClassName = "RKH264Encoder";
    }
    else
    {
        // TODO
    }
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

    options.addOption(Option("codec", "codec", "allow value : h264, hevc, vp8, vp9, av1")
        .required(true)
        .repeatable(false)
        .argument("codec_type")
        .callback(OptionCallback<App>(this, &App::HandleCodecType))
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
    encoder->Init();
    encoder->Start();

    encoder->Stop();
    encoder->Uninit();
    return 0;
}

/********************************************************* TEST(END) *****************************************************/

POCO_APP_MAIN(App)
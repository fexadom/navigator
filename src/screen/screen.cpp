#include "oled/Edison_OLED.h"
#include "navigator/services/screen/BnScreenService.h"
#include "binder_constants.h"
#include <gpio.h>
#include <stdio.h>

#include <string>
#include <sysexits.h>

#include <base/bind.h>
#include <base/command_line.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <binderwrapper/binder_wrapper.h>
#include <brillo/binder_watcher.h>
#include <brillo/daemons/daemon.h>
#include <brillo/syslog_logging.h>
#include <utils/String16.h>

using android::String16;

class ScreenService : public navigator::services::screen::BnScreenService {
public:
    void InitializeService();
    android::binder::Status DisplayText(const String16& s, int x, int y);
        
private:
    void SetupButtons();
    void SetupOLED();
    void StartScreen();
    // Define an edOLED object:
    edOLED oled;
    mraa_gpio_context BUTTON_UP;
    mraa_gpio_context BUTTON_DOWN;
    mraa_gpio_context BUTTON_LEFT;
    mraa_gpio_context BUTTON_RIGHT;
    mraa_gpio_context BUTTON_SELECT;
    mraa_gpio_context BUTTON_A;
    mraa_gpio_context BUTTON_B;
};

class Daemon final : public brillo::Daemon {
public:
    Daemon() = default;
    
protected:
    int OnInit() override;

private:
    android::sp<ScreenService> screen_service_;
    brillo::BinderWatcher binder_watcher_;

    base::WeakPtrFactory<Daemon> weak_ptr_factory_{this};

    DISALLOW_COPY_AND_ASSIGN(Daemon);
};

void ScreenService::InitializeService()
{
    SetupButtons();
    SetupOLED();
    StartScreen();
}

void ScreenService::SetupButtons()
{
    BUTTON_UP = mraa_gpio_init(46);
    mraa_gpio_dir(BUTTON_UP, MRAA_GPIO_IN);

    BUTTON_DOWN = mraa_gpio_init(31);
    mraa_gpio_dir(BUTTON_DOWN, MRAA_GPIO_IN);

    BUTTON_LEFT = mraa_gpio_init(15);
    mraa_gpio_dir(BUTTON_LEFT, MRAA_GPIO_IN);

    BUTTON_RIGHT = mraa_gpio_init(45);
    mraa_gpio_dir(BUTTON_RIGHT, MRAA_GPIO_IN);

    BUTTON_SELECT = mraa_gpio_init(33);
    mraa_gpio_dir(BUTTON_SELECT, MRAA_GPIO_IN);

    BUTTON_A = mraa_gpio_init(47);
    mraa_gpio_dir(BUTTON_A, MRAA_GPIO_IN);

    BUTTON_B = mraa_gpio_init(32);
    mraa_gpio_dir(BUTTON_B, MRAA_GPIO_IN);
}

void ScreenService::SetupOLED()
{
    oled.begin();
    oled.clear(PAGE);
    oled.display();
    oled.setFontType(0);
}

void ScreenService::StartScreen()
{
	oled.clear(PAGE);
    oled.setCursor(2, 10);
    oled.print("SERVICE UP");
    // Call display to actually draw it on the OLED:
    oled.display();
}

//Implementation of service call to display text on screen
android::binder::Status ScreenService::DisplayText(const String16& s, int x, int y)
{
    oled.clear(PAGE);
    oled.setCursor(x, y);
    oled.print(android::String8(s).string());
    // Call display to actually draw it on the OLED:
    oled.display();
    
    return android::binder::Status::ok();
}

int Daemon::OnInit() {
    int return_code = brillo::Daemon::OnInit();
    if (return_code != EX_OK)
    return return_code;

    android::BinderWrapper::Create();
    if (!binder_watcher_.Init())
    return EX_OSERR;
    
    screen_service_ = new ScreenService();
    screen_service_->InitializeService();
    
    android::BinderWrapper::Get()->RegisterService(
        services::kBinderScreenServiceName,
        screen_service_);
    
    return EX_OK;
}

int main(int argc, char* argv[]) {
    
    base::CommandLine::Init(argc, argv);
    brillo::InitLog(brillo::kLogToSyslog | brillo::kLogHeader);
    
    LOG(INFO) << "Starting locator daemon...";
    Daemon daemon;
    return daemon.Run();
}

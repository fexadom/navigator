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
#include <libweaved/service.h>

#include "binder_constants.h"
#include "navigator/services/screen/IScreenService.h"
#include "navigator/services/bluescan/IBluescanService.h"

using android::String16;

using navigator::services::screen::IScreenService;
using navigator::services::bluescan::IBluescanService;

namespace {
const char kBaseComponent[] = "base";
const char kBaseTrait[] = "base";
}  // anonymous namespace

class Daemon final : public brillo::Daemon {
public:
    Daemon() = default;

protected:
    int OnInit() override;

private:
    void OnWeaveServiceConnected(const std::weak_ptr<weaved::Service>& service);
    void ConnectToScreenService();
    void OnScreenServiceDisconnected();
    void ConnectToBluescanService();
    void OnBluescanServiceDisconnected();
    void OnPairingInfoChanged(const weaved::Service::PairingInfo* pairing_info);

    // Particular command handlers for various commands.
    void OnSetConfig(std::unique_ptr<weaved::Command> command);
    void OnIdentify(std::unique_ptr<weaved::Command> command);

    std::weak_ptr<weaved::Service> weave_service_;

    // Screen service interface.
    android::sp<IScreenService> screen_service_;
    
    // Bluescan service interface.
    android::sp<IBluescanService> bluescan_service_;

    brillo::BinderWatcher binder_watcher_;
    std::unique_ptr<weaved::Service::Subscription> weave_service_subscription_;

    base::WeakPtrFactory<Daemon> weak_ptr_factory_{this};
    DISALLOW_COPY_AND_ASSIGN(Daemon);
};

int Daemon::OnInit() {
    int return_code = brillo::Daemon::OnInit();
    if (return_code != EX_OK)
        return return_code;

    android::BinderWrapper::Create();
    if (!binder_watcher_.Init())
        return EX_OSERR;

    weave_service_subscription_ = weaved::Service::Connect(
      brillo::MessageLoop::current(),
      base::Bind(&Daemon::OnWeaveServiceConnected,
                 weak_ptr_factory_.GetWeakPtr()));
    ConnectToScreenService();
    ConnectToBluescanService();

    LOG(INFO) << "Navigator daemon started...";
    return EX_OK;
}

void Daemon::OnWeaveServiceConnected(
    const std::weak_ptr<weaved::Service>& service) {
    LOG(INFO) << "Daemon::OnWeaveServiceConnected";
    weave_service_ = service;
    auto weave_service = weave_service_.lock();
    if (!weave_service)
        return;

    weave_service->AddCommandHandler(
      kBaseComponent, kBaseTrait, "identify",
      base::Bind(&Daemon::OnIdentify, weak_ptr_factory_.GetWeakPtr()));

    weave_service->SetPairingInfoListener(
      base::Bind(&Daemon::OnPairingInfoChanged,
                 weak_ptr_factory_.GetWeakPtr()));
}

void Daemon::ConnectToScreenService() {
    android::BinderWrapper* binder_wrapper = android::BinderWrapper::Get();
    auto binder = binder_wrapper->GetService(services::kBinderScreenServiceName);
    if (!binder.get()) {
        brillo::MessageLoop::current()->PostDelayedTask(
            base::Bind(&Daemon::ConnectToScreenService,
                       weak_ptr_factory_.GetWeakPtr()),
            base::TimeDelta::FromSeconds(1));
        return;
    }
    binder_wrapper->RegisterForDeathNotifications(
      binder,
      base::Bind(&Daemon::OnScreenServiceDisconnected,
                 weak_ptr_factory_.GetWeakPtr()));
    screen_service_ = android::interface_cast<IScreenService>(binder);
}

void Daemon::ConnectToBluescanService() {
    android::BinderWrapper* binder_wrapper = android::BinderWrapper::Get();
    auto binder = binder_wrapper->GetService(services::kBinderBluescanServiceName);
    if (!binder.get()) {
        brillo::MessageLoop::current()->PostDelayedTask(
            base::Bind(&Daemon::ConnectToBluescanService,
                       weak_ptr_factory_.GetWeakPtr()),
            base::TimeDelta::FromSeconds(1));
        return;
    }
    binder_wrapper->RegisterForDeathNotifications(
      binder,
      base::Bind(&Daemon::OnBluescanServiceDisconnected,
                 weak_ptr_factory_.GetWeakPtr()));
    bluescan_service_ = android::interface_cast<IBluescanService>(binder);
}

void Daemon::OnScreenServiceDisconnected() {
    screen_service_ = nullptr;
    ConnectToScreenService();
}

void Daemon::OnBluescanServiceDisconnected() {
    bluescan_service_ = nullptr;
    ConnectToBluescanService();
}

void Daemon::OnSetConfig(std::unique_ptr<weaved::Command> command) {
    if (!screen_service_.get()) {
        command->Abort("_system_error", "screen service unavailable", nullptr);
        return;
    }

    auto state = command->GetParameter<std::string>("state");
    bool on = (state == "on");
    
    android::binder::Status status;
  
    if(on)
        status = screen_service_->DisplayText(String16("ON"), 20, 10);
    else
        status = screen_service_->DisplayText(String16("OFF"), 20, 10);
        
    if (!status.isOk()) {
        command->AbortWithCustomError(status, nullptr);
        return;
    }
    
    command->Complete({}, nullptr);
}

void Daemon::OnIdentify(std::unique_ptr<weaved::Command> command) {
    if (!screen_service_.get()) {
        command->Abort("_system_error", "screen service unavailable", nullptr);
        return;
    }
    
    if (!bluescan_service_.get()) {
        command->Abort("_system_error", "bluescan service unavailable", nullptr);
        return;
    }

    android::binder::Status status1 = screen_service_->DisplayText(String16("Here"), 20, 10);
    
    android::binder::Status status2 = bluescan_service_->DoScan();

    if (!status1.isOk() || !status2.isOk()) {
        command->AbortWithCustomError(status2, nullptr);
        return;
    }

    command->Complete({}, nullptr);
}

void Daemon::OnPairingInfoChanged(
    const weaved::Service::PairingInfo* pairing_info) {
    LOG(INFO) << "Daemon::OnPairingInfoChanged: " << pairing_info;
}

int main(int argc, char* argv[]) {
    base::CommandLine::Init(argc, argv);
    brillo::InitLog(brillo::kLogToSyslog | brillo::kLogHeader);
    Daemon daemon;
    return daemon.Run();
}

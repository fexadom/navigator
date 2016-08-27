#include <string>
#include <vector>
#include <sysexits.h>

#include <base/bind.h>
#include <base/command_line.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <base/json/json_writer.h>
#include <base/values.h>
#include <base/strings/string_split.h>
#include <binderwrapper/binder_wrapper.h>
#include <brillo/binder_watcher.h>
#include <brillo/daemons/daemon.h>
#include <brillo/syslog_logging.h>
#include <brillo/http/http_utils.h>
#include <brillo/mime_utils.h>
#include <libweaved/service.h>

#include "binder_constants.h"
#include "navigator_constants.h"
#include "navigator/services/screen/IScreenService.h"
#include "navigator/services/bluescan/IBluescanService.h"
#include "navigator/services/bluescan/BnBluescanCallback.h"

using android::String16;

using navigator::services::screen::IScreenService;
using navigator::services::bluescan::IBluescanService;
using navigator::services::bluescan::BnBluescanCallback;


namespace {
const char kBaseComponent[] = "base";
const char kBaseTrait[] = "base";
}  // anonymous namespace

class Daemon final : public brillo::Daemon, public BnBluescanCallback {
public:
    Daemon() = default;

protected:
    int OnInit() override;
    android::binder::Status OnFinishScanCallback(const std::vector<String16>& scanResults);
    void SendHTTPRequest(std::string scanJSON);
    void FindPosition();

private:
    void OnWeaveServiceConnected(const std::weak_ptr<weaved::Service>& service);
    void ConnectToScreenService();
    void OnScreenServiceDisconnected();
    void ConnectToBluescanService();
    void OnBluescanServiceDisconnected();
    void OnPairingInfoChanged(const weaved::Service::PairingInfo* pairing_info);
    void JSONfy(const std::vector<String16>& scanResults, std::string& output_js);
    void HTTP_Success_callback(brillo::http::RequestID id, std::unique_ptr<brillo::http::Response> response);
    void HTTP_Error_callback(brillo::http::RequestID id, const brillo::Error* error);

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
    std::shared_ptr<brillo::http::Transport> transport_;

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
    
    transport_ = brillo::http::Transport::CreateDefault();
    transport_->SetDefaultTimeout(base::TimeDelta::FromMilliseconds(1500)); 

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

    bluescan_service_->RegisterCallback(this);
    
    brillo::MessageLoop::current()->PostDelayedTask(
            base::Bind(&Daemon::FindPosition,
                       weak_ptr_factory_.GetWeakPtr()),
            base::TimeDelta::FromSeconds(2));
}

void Daemon::OnScreenServiceDisconnected() {
    screen_service_ = nullptr;
    ConnectToScreenService();
}

void Daemon::OnBluescanServiceDisconnected() {
    bluescan_service_ = nullptr;
    ConnectToBluescanService();
}

void Daemon::FindPosition()
{
    bluescan_service_->DoScan(3500);
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
    
    android::binder::Status status2 = bluescan_service_->DoScan(3000);

    if (!status1.isOk() || !status2.isOk()) {
        command->AbortWithCustomError(status2, nullptr);
        return;
    }

    command->Complete({}, nullptr);
}


void Daemon::OnPairingInfoChanged(
    const weaved::Service::PairingInfo* pairing_info) {
    LOG(INFO) << "Daemon::OnPairingInfoChanged pairing code: " << pairing_info->pairing_code;
    screen_service_->DisplayCenteredText(String16(pairing_info->pairing_code.c_str()));
}

android::binder::Status Daemon::OnFinishScanCallback(const std::vector<String16>& scanResults){
        
        std::string results_json("{}");
        
        JSONfy(scanResults, results_json);

        LOG(INFO) << "JSON: " << results_json;
        
        SendHTTPRequest(results_json);
        
        
        return android::binder::Status::ok();
}

void Daemon::SendHTTPRequest(std::string scanJSON)
{
    brillo::http::PostText(navigator::FinderURL, scanJSON, brillo::mime::application::kJson, {}, transport_, 
    base::Bind(&Daemon::HTTP_Success_callback, weak_ptr_factory_.GetWeakPtr()),base::Bind(&Daemon::HTTP_Error_callback, weak_ptr_factory_.GetWeakPtr()));
    
    LOG(INFO) << "Watinting for response...";
}

void Daemon::HTTP_Success_callback(brillo::http::RequestID /*id*/, std::unique_ptr<brillo::http::Response> response) {
    int statusCode;
    std::unique_ptr<base::DictionaryValue> jsonResponse;
    
    jsonResponse = brillo::http::ParseJsonResponse(response.get(), &statusCode, nullptr);
    
    if(statusCode == 200){
        if(jsonResponse)
        {
            std::string value("??");
            if(jsonResponse->HasKey("location")){
                jsonResponse->GetString("location",&value);
                LOG(INFO) << "Location: " << value;
                screen_service_->DisplayCenteredText(String16(value.c_str()));
            }else{
                LOG(ERROR) << "UNKNOWN LOCATION";
                screen_service_->TagPositionLost();
            }
        }else
            LOG(ERROR) << "No JSON response";
    }else
        LOG(ERROR) << "Response code: " << statusCode;
        
    brillo::MessageLoop::current()->PostDelayedTask(
            base::Bind(&Daemon::FindPosition,
                       weak_ptr_factory_.GetWeakPtr()),
            base::TimeDelta::FromSeconds(1));
    
}
    
void Daemon::HTTP_Error_callback(brillo::http::RequestID id, const brillo::Error* error) {
    LOG(ERROR) << "Request id: "<< id << " ERROR MSG: " << error->GetMessage();
    screen_service_->TagPositionLost();
    
    brillo::MessageLoop::current()->PostDelayedTask(
            base::Bind(&Daemon::FindPosition,
                       weak_ptr_factory_.GetWeakPtr()),
            base::TimeDelta::FromSeconds(1));
}

void Daemon::JSONfy(const std::vector<String16>& scanResults, std::string& output_js)
{
    int n = scanResults.size();
    
    base::DictionaryValue root_dict;
    root_dict.SetString("group",navigator::JSONGroupName);
    root_dict.SetString("username",navigator::JSONUserName);
    root_dict.SetString("location",navigator::JSONLocation);
    root_dict.SetDouble("time", static_cast<double> (std::time(0)));
    
    scoped_ptr<base::ListValue> list(new base::ListValue());
    for(int i=0;i<n;i++)
    {
        std::string s = android::String8(scanResults[i]).string();
        std::vector<std::string> tokens = base::SplitString(s,navigator::BluescanStringDelimeter,base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
        std::string mac = tokens[0];
        int rssi = std::stoi(tokens[1]);
        
        scoped_ptr<base::DictionaryValue> inner_dict(new base::DictionaryValue());
        inner_dict->SetString("mac", std::move(mac));
        inner_dict->SetInteger("rssi", rssi);
        list->Append(std::move(inner_dict));
    }
    
    root_dict.Set("wifi-fingerprint", std::move(list));
    base::JSONWriter::Write(root_dict, &output_js);
}

int main(int argc, char* argv[]) {
    base::CommandLine::Init(argc, argv);
    brillo::InitLog(brillo::kLogToSyslog | brillo::kLogHeader);
    Daemon daemon;
    return daemon.Run();
}
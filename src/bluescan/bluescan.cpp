#include "callbacks.h"
#include "navigator/services/bluescan/BnBluescanService.h"
#include "binder_constants.h"

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

#include <bluetooth/scan_filter.h>
#include <bluetooth/scan_settings.h>
#include <bluetooth/uuid.h>

using ipc::binder::IBluetooth;
using ipc::binder::IBluetoothLowEnergy;
using android::sp;
using namespace bluescan;

sp<IBluetooth> bt_iface;
sp<IBluetoothLowEnergy> ble_iface;

class BluescanService : public navigator::services::bluescan::BnBluescanService {
public:
    void InitializeService();
    android::binder::Status DoScan(int milliseconds);
    android::binder::Status RegisterCallback(const sp<navigator::services::bluescan::IBluescanCallback>& callback);
        
private:
	void StopScan();
    void RegisterBLEClient();
    
    sp<navigator::services::bluescan::IBluescanCallback> cbo_;
	base::WeakPtrFactory<BluescanService> weak_ptr_factory_{this};
    std::vector<android::String16> scanResults_;
    
    sp<BluescanBluetoothCallback> callbackBT_;
    sp<BluescanBluetoothLowEnergyCallback> callbackBLE_;
};

class Daemon final : public brillo::Daemon {
public:
    Daemon() = default;

protected:
    int OnInit() override;

private:
    sp<BluescanService> bluescan_service_;
    brillo::BinderWatcher binder_watcher_;

    base::WeakPtrFactory<Daemon> weak_ptr_factory_{this};

    DISALLOW_COPY_AND_ASSIGN(Daemon);
};

void BluescanService::InitializeService()
{
    
    // Register Adapter state-change callback
    callbackBT_ = new BluescanBluetoothCallback();
    callbackBLE_ = new BluescanBluetoothLowEnergyCallback();
    bt_iface->RegisterCallback(callbackBT_);
    
    bt_iface->Enable();
    
    RegisterBLEClient();
}

void BluescanService::RegisterBLEClient()
{
    if (state == bluetooth::ADAPTER_STATE_ON)
    {
        //Create and register to bluetooth ble interface
        ble_iface = bt_iface->GetLowEnergyInterface();
        if (!ble_iface.get()) {
            LOG(ERROR) << "Failed to obtain handle to Bluetooth Low Energy interface";
            ble_registered = false;
        }else{
            ble_iface->RegisterClient(callbackBLE_);
        }
    }else
    {
        //Try again in 1 second
        brillo::MessageLoop::current()->PostDelayedTask(
            base::Bind(&BluescanService::RegisterBLEClient,
                       weak_ptr_factory_.GetWeakPtr()),
            base::TimeDelta::FromSeconds(1));
    }
}

android::binder::Status BluescanService::DoScan(int milliseconds)
{
    if(ble_registered)
    {
        LOG(INFO) << "Starting scan...";
        bluetooth::ScanSettings settings;
        std::vector<bluetooth::ScanFilter> filters;
        ble_iface->StartScan(ble_client_id, settings, filters);
		
		//Schedule stop scan
        brillo::MessageLoop::current()->PostDelayedTask(
            base::Bind(&BluescanService::StopScan,
                       weak_ptr_factory_.GetWeakPtr()),
            base::TimeDelta::FromMilliseconds(milliseconds));
    }else{
        LOG(ERROR) << "BLE not registered!";
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
    }
        
    return android::binder::Status::ok();
}

void BluescanService::StopScan()
{
	if(ble_registered)
    {
        LOG(INFO) << "Stopping scan...";
        ble_iface->StopScan(ble_client_id);
        
        scanResults_.clear();
        callbackBLE_->CopyScanResults(scanResults_);
        cbo_->OnFinishScanCallback(scanResults_);
    }else{
        LOG(ERROR) << "BLE not registered!";
    }
}

android::binder::Status BluescanService::RegisterCallback(const sp<navigator::services::bluescan::IBluescanCallback>& callback)
{
    cbo_ = callback;
    return android::binder::Status::ok();
}

int Daemon::OnInit() {
    int return_code = brillo::Daemon::OnInit();
    if (return_code != EX_OK)
        return return_code;

    android::BinderWrapper::Create();
    if (!binder_watcher_.Init())
        return EX_OSERR;
        
    bluescan_service_ = new BluescanService();
    bluescan_service_->InitializeService();
    
    android::BinderWrapper::Get()->RegisterService(
        services::kBinderBluescanServiceName,
        bluescan_service_);
    
    LOG(INFO) << "Bluescan started, waiting for bluetooth to come up...";
    
    return EX_OK;
}

int main(int argc, char* argv[]) {
    
    base::CommandLine::Init(argc, argv);
    brillo::InitLog(brillo::kLogToSyslog | brillo::kLogHeader);
    
    bt_iface = IBluetooth::getClientInterface();
    if (!bt_iface.get()) {
        LOG(ERROR) << "Failed to obtain handle on IBluetooth";
        return EXIT_FAILURE;
    }
    
    LOG(INFO) << "Starting bluescan daemon...";
    Daemon daemon;
    return daemon.Run();
}

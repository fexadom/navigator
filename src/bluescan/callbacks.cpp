#include "callbacks.h"
#include "navigator_constants.h"
#include <base/logging.h>

using ipc::binder::IBluetooth;

volatile bluetooth::AdapterState state = bluetooth::ADAPTER_STATE_DISCONNECTED;
volatile bool ble_registered = false;
volatile int ble_client_id = 0;

namespace bluescan {

// IBluetoothCallback overrides:
void BluescanBluetoothCallback::OnBluetoothStateChange(
    bluetooth::AdapterState /* prev_state */,
    bluetooth::AdapterState new_state) {
    
    state = new_state;
}
   
void BluescanBluetoothLowEnergyCallback::OnScanResult
    (const bluetooth::ScanResult& scan_result) {
    
    //LOG(INFO) << "Scan result: " << "[" << scan_result.device_address() << "] "
     //           << "- RSSI: " << scan_result.rssi();
    
    
    scanResults_[scan_result.device_address()] = scan_result.rssi();
}

void BluescanBluetoothLowEnergyCallback::CopyScanResults(std::vector<android::String16>& copy)
{
    std::map<std::string,int>::iterator myMapIterator;
    
    int index = 0;
    for(myMapIterator = scanResults_.begin(); 
        myMapIterator != scanResults_.end();
        myMapIterator++)
    {
        if(index < navigator::MaxScanBeacons)
        {
            std::string beacon_str = myMapIterator->first + navigator::BluescanStringDelimeter + std::to_string(myMapIterator->second);
            copy.push_back(android::String16(beacon_str.c_str()));
            index++;
        }
    }
    
    scanResults_.clear();
    
}
  
void BluescanBluetoothLowEnergyCallback::OnClientRegistered(int status, int client_id) {
    if (status != bluetooth::BLE_STATUS_SUCCESS) {
        LOG(ERROR) << "Failed to register BLE client";
        ble_registered = false;
    } else {
        ble_registered = true;
        ble_client_id = client_id;
        LOG(INFO) << "Client registered with id: " << client_id;
    }
}
   
} // namespace bluescan
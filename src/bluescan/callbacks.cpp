#include "callbacks.h"

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
    
    if (new_state == bluetooth::ADAPTER_STATE_ON)
    {
        //Create and register to bluetooth ble interface
        ble_iface = bt_iface->GetLowEnergyInterface();
        if (!ble_iface.get()) {
            LOG(ERROR) << "Failed to obtain handle to Bluetooth Low Energy interface";
            ble_registered = false;
        }else{
            sp<BluescanBluetoothLowEnergyCallback> callbackBLE = new BluescanBluetoothLowEnergyCallback();
            ble_iface->RegisterClient(callbackBLE);
        }
    }
}
   
void BluescanBluetoothLowEnergyCallback::OnScanResult
    (const bluetooth::ScanResult& scan_result) {
    
    LOG(INFO) << "Scan result: " << "[" << scan_result.device_address() << "] "
                << "- RSSI: " << scan_result.rssi();
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
#pragma once

#include <bluetooth/binder/IBluetooth.h>
#include <bluetooth/binder/IBluetoothCallback.h>
#include <bluetooth/binder/IBluetoothLowEnergy.h>
#include <bluetooth/binder/IBluetoothLowEnergyCallback.h>
#include <bluetooth/low_energy_constants.h>
#include <bluetooth/adapter_state.h>

using ipc::binder::IBluetooth;
using ipc::binder::IBluetoothLowEnergy;
using android::sp;

extern volatile bluetooth::AdapterState state;
extern volatile bool ble_registered;
extern volatile int ble_client_id;

extern sp<IBluetooth> bt_iface;
extern sp<IBluetoothLowEnergy> ble_iface;

namespace bluescan {
    
class BluescanBluetoothCallback : public ipc::binder::BnBluetoothCallback {
 public:
  BluescanBluetoothCallback() = default;
  ~BluescanBluetoothCallback() override = default;

  // IBluetoothCallback overrides:
  void OnBluetoothStateChange(
      bluetooth::AdapterState prev_state,
      bluetooth::AdapterState new_state) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BluescanBluetoothCallback);
};

class BluescanBluetoothLowEnergyCallback : public ipc::binder::BnBluetoothLowEnergyCallback {
 public:
  BluescanBluetoothLowEnergyCallback() = default;
  ~BluescanBluetoothLowEnergyCallback() override = default;

  void OnScanResult(const bluetooth::ScanResult& scan_result) override;
  void OnClientRegistered(int status, int client_id) override;
  void OnConnectionState(int /*status*/, int /*client_id*/, const char* /*address*/, bool /*connected*/) override {};
  void OnMtuChanged(int /*status*/, const char* /*address*/, int /*mtu*/) override {};
  void OnMultiAdvertiseCallback(int /*status*/, bool /*is_start*/, const bluetooth::AdvertiseSettings& /*settings*/) override {};

 private:
  DISALLOW_COPY_AND_ASSIGN(BluescanBluetoothLowEnergyCallback);
};

} // namespace bluescan
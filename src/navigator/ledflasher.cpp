/*
 * Copyright 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

#include "animation.h"
#include "binder_constants.h"
#include "brillo/examples/ledflasher/ILEDService.h"

using android::String16;

namespace {
const char kLedFlasherComponent[] = "ledflasher";
const char kLedFlasherTrait[] = "_ledflasher";
const char kBaseComponent[] = "base";
const char kBaseTrait[] = "base";
const char kLedComponentPrefix[] = "led";
const char kOnOffTrait[] = "onOff";
const char kLedInfoTrait[] = "_ledInfo";
}  // anonymous namespace

using brillo::examples::ledflasher::ILEDService;

class Daemon final : public brillo::Daemon {
 public:
  Daemon() = default;

 protected:
  int OnInit() override;

 private:
  void OnWeaveServiceConnected(const std::weak_ptr<weaved::Service>& service);
  void ConnectToLEDService();
  void OnLEDServiceDisconnected();
  void OnPairingInfoChanged(const weaved::Service::PairingInfo* pairing_info);
  void CreateLedComponentsIfNeeded();

  // Particular command handlers for various commands.
  void OnSetConfig(size_t led_index, std::unique_ptr<weaved::Command> command);
  void OnAnimate(std::unique_ptr<weaved::Command> command);
  void OnIdentify(std::unique_ptr<weaved::Command> command);

  // Helper methods to propagate device state changes to Buffet and hence to
  // the cloud server or local clients.
  void UpdateDeviceState();

  void StartAnimation(const std::string& type, base::TimeDelta duration);
  void StopAnimation();

  std::weak_ptr<weaved::Service> weave_service_;

  // Device state variables.
  std::string status_{"idle"};

  // LED service interface.
  android::sp<ILEDService> led_service_;

  // Current animation;
  std::unique_ptr<Animation> animation_;

  brillo::BinderWatcher binder_watcher_;
  std::unique_ptr<weaved::Service::Subscription> weave_service_subscription_;

  bool led_components_added_{false};

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
  ConnectToLEDService();

  LOG(INFO) << "Waiting for commands...";
  return EX_OK;
}

void Daemon::OnWeaveServiceConnected(
    const std::weak_ptr<weaved::Service>& service) {
  LOG(INFO) << "Daemon::OnWeaveServiceConnected";
  weave_service_ = service;
  auto weave_service = weave_service_.lock();
  if (!weave_service)
    return;

  weave_service->AddComponent(
      kLedFlasherComponent, {kLedFlasherTrait}, nullptr);
  weave_service->AddCommandHandler(
      kLedFlasherComponent,
      kLedFlasherTrait,
      "animate",
      base::Bind(&Daemon::OnAnimate, weak_ptr_factory_.GetWeakPtr()));
  weave_service->AddCommandHandler(
      kBaseComponent, kBaseTrait, "identify",
      base::Bind(&Daemon::OnIdentify, weak_ptr_factory_.GetWeakPtr()));

  weave_service->SetPairingInfoListener(
      base::Bind(&Daemon::OnPairingInfoChanged,
                 weak_ptr_factory_.GetWeakPtr()));

  led_components_added_ = false;
  CreateLedComponentsIfNeeded();
  UpdateDeviceState();
}

void Daemon::ConnectToLEDService() {
  android::BinderWrapper* binder_wrapper = android::BinderWrapper::Get();
  auto binder = binder_wrapper->GetService(ledservice::kBinderServiceName);
  if (!binder.get()) {
    brillo::MessageLoop::current()->PostDelayedTask(
        base::Bind(&Daemon::ConnectToLEDService,
                   weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(1));
    return;
  }
  binder_wrapper->RegisterForDeathNotifications(
      binder,
      base::Bind(&Daemon::OnLEDServiceDisconnected,
                 weak_ptr_factory_.GetWeakPtr()));
  led_service_ = android::interface_cast<ILEDService>(binder);
  CreateLedComponentsIfNeeded();
  UpdateDeviceState();
}

void Daemon::CreateLedComponentsIfNeeded() {
  if (led_components_added_ || !led_service_.get())
    return;

  auto weave_service = weave_service_.lock();
  if (!weave_service)
    return;

  std::vector<bool> leds;
  std::vector<String16> ledNames;
  if (!led_service_->getAllLEDs(&leds).isOk())
    return;

  if (!led_service_->getAllLEDNames(&ledNames).isOk())
    return;

  for (size_t led_index = 0; led_index < leds.size(); led_index++) {
    std::string led_name = android::String8{ledNames[led_index]}.string();
    std::string component_name =
        kLedComponentPrefix + std::to_string(led_index + 1);
    if (weave_service->AddComponent(
            component_name, {kOnOffTrait, kLedInfoTrait}, nullptr)) {
      weave_service->AddCommandHandler(
          component_name,
          kOnOffTrait,
          "setConfig",
          base::Bind(
              &Daemon::OnSetConfig, weak_ptr_factory_.GetWeakPtr(), led_index));

      weave_service->SetStateProperty(
          component_name,
          kOnOffTrait,
          "state",
          *brillo::ToValue(leds[led_index] ? "on" : "off"),
          nullptr);

      weave_service->SetStateProperty(component_name,
                                      kLedInfoTrait,
                                      "name",
                                      *brillo::ToValue(led_name),
                                      nullptr);
    }
  }
  led_components_added_ = true;
}

void Daemon::OnLEDServiceDisconnected() {
  animation_.reset();
  led_service_ = nullptr;
  ConnectToLEDService();
}

void Daemon::OnSetConfig(size_t led_index,
                         std::unique_ptr<weaved::Command> command) {
  if (!led_service_.get()) {
    command->Abort("_system_error", "ledservice unavailable", nullptr);
    return;
  }

  auto state = command->GetParameter<std::string>("state");
  bool on = (state == "on");
  android::binder::Status status = led_service_->setLED(led_index, on);
  if (!status.isOk()) {
    command->AbortWithCustomError(status, nullptr);
    return;
  }
  if (animation_) {
    animation_.reset();
    status_ = "idle";
    UpdateDeviceState();
  }

  auto weave_service = weave_service_.lock();
  if (weave_service) {
    std::string component_name =
        kLedComponentPrefix + std::to_string(led_index + 1);
    weave_service->SetStateProperty(component_name,
                                    kOnOffTrait,
                                    "state",
                                    *brillo::ToValue(on ? "on" : "off"),
                                    nullptr);
  }
  command->Complete({}, nullptr);
}

void Daemon::OnAnimate(std::unique_ptr<weaved::Command> command) {
  if (!led_service_.get()) {
    command->Abort("_system_error", "ledservice unavailable", nullptr);
    return;
  }

  double duration = command->GetParameter<double>("duration");
  if(duration <= 0.0) {
    command->Abort("_invalid_parameter", "Invalid parameter value", nullptr);
    return;
  }
  std::string type = command->GetParameter<std::string>("type");
  StartAnimation(type, base::TimeDelta::FromSecondsD(duration));
  command->Complete({}, nullptr);
}

void Daemon::OnIdentify(std::unique_ptr<weaved::Command> command) {
  if (!led_service_.get()) {
    command->Abort("_system_error", "ledservice unavailable", nullptr);
    return;
  }
  StartAnimation("blink", base::TimeDelta::FromMilliseconds(500));
  command->Complete({}, nullptr);

  brillo::MessageLoop::current()->PostDelayedTask(
      base::Bind(&Daemon::StopAnimation, weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(2));
}

void Daemon::OnPairingInfoChanged(
    const weaved::Service::PairingInfo* pairing_info) {
  LOG(INFO) << "Daemon::OnPairingInfoChanged: " << pairing_info;
  if (!pairing_info)
    return StopAnimation();
  StartAnimation("blink", base::TimeDelta::FromMilliseconds(500));
}

void Daemon::StartAnimation(const std::string& type, base::TimeDelta duration) {
  animation_ = Animation::Create(led_service_, type, duration);
  if (animation_) {
    status_ = "animating";
    animation_->Start();
  } else {
    status_ = "idle";
  }
  UpdateDeviceState();
}

void Daemon::StopAnimation() {
  if (!animation_)
    return;

  animation_.reset();
  status_ = "idle";
  UpdateDeviceState();
}

void Daemon::UpdateDeviceState() {
  auto weave_service = weave_service_.lock();
  if (!weave_service)
    return;

  weave_service->SetStateProperty(kLedFlasherComponent,
                                  kLedFlasherTrait,
                                  "status",
                                  *brillo::ToValue(status_),
                                  nullptr);
}

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogHeader);
  Daemon daemon;
  return daemon.Run();
}

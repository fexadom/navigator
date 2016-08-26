/*
 * Interface for the bluescan service. The method DoScan scans the environment for
 * eddystone beacons for an interval of time in milliseconds. It calls the OnFinishScanCallback
 * on the callback object with the collected beacons.
 */

package navigator.services.bluescan;

import navigator.services.bluescan.IBluescanCallback;

// Interface for the screen service that accepts a string and displays it on the screen.
interface IBluescanService {

  // Registers a callback object of type IBluescanCallback with the service. Once
  // the service has finished scanning beacons, then the method
  // OnFinishScanCallback on the callback object will be called.
  void RegisterCallback(IBluescanCallback callback);

  // do an eddystone beacons scan for an interval of time in milliseconds
  void DoScan(int milliseconds);
}

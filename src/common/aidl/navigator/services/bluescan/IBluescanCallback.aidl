/*
 * Interface for the callback object of the bluescan service. The OnFinishScanCallback
 * method is called with a vector containing all the scanned eddystone beacons.
 */

package navigator.services.bluescan;

// Interface for a callback object that is to be registered with
// IBluescanService.
interface IBluescanCallback {
  // This should be a oneway call since we don't want services to be blocked on
  // clients.
  oneway void OnFinishScanCallback(in List<String> scanResults);
}

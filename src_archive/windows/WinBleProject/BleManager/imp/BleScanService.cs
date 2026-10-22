using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Bluetooth;
using Windows.Devices.Enumeration;

namespace BleManager.imp
{
    public class BleScanService : IBleScanService
    {
       // private static readonly String TAG = "BleScanService";

        private IScanDeviceCallback mScanCallBack = null;
        private int mScanTimer = 0;
        private String mScanDeviceName = "iFLYMouse Pro";
        private DeviceWatcher mDeviceWatcher = null;
        private readonly string[] requestedProperties = { "System.Devices.Aep.DeviceAddress",
                "System.Devices.Aep.IsConnected",
                "System.Devices.Aep.Bluetooth.Le.IsConnectable" };

        // BT_Code: Example showing paired and non-paired in a single query.
   //     private readonly string aqsAllBluetoothLEDevices = "(System.Devices.Aep.ProtocolId:=\"{bb7bb05e-5972-42b5-94fc-76eaa7084d49}\")";

        public IBleScanService OnSetScanCallback(IScanDeviceCallback callBack)
        {
            mScanCallBack = callBack;
            return this;
        }

        public IBleScanService OnSetScanTimer(int timer)
        {
            mScanTimer = timer;
            return this;
        }

        public IBleScanService OnStartScan(string deviceName, bool isPaired)
        {
            mScanDeviceName = deviceName;
            StartBleDeviceScan(isPaired);
            return this;
        }

        public IBleScanService OnStopScan()
        {
            StopBleDeviceScan();
            return this;
        }

        private void StartBleDeviceScan(bool isPaired)
        {

            if (null == mDeviceWatcher)
            {
                mDeviceWatcher = DeviceInformation.CreateWatcher(
                        BluetoothLEDevice.GetDeviceSelectorFromPairingState(isPaired),
                     // aqsAllBluetoothLEDevices,
                      requestedProperties,
                      DeviceInformationKind.AssociationEndpoint);

                // Register event handlers before starting the watcher.
                mDeviceWatcher.Added += DeviceWatcher_Added;
                mDeviceWatcher.Updated += DeviceWatcher_Updated;
                mDeviceWatcher.Removed += DeviceWatcher_Removed;
                mDeviceWatcher.EnumerationCompleted += DeviceWatcher_EnumerationCompleted;
                mDeviceWatcher.Stopped += DeviceWatcher_Stopped;
            }

            mDeviceWatcher.Start();
        }

        private void StopBleDeviceScan()
        {

            if (null == mDeviceWatcher)
                return;

            if (mDeviceWatcher.Status == DeviceWatcherStatus.Started)
            {
                mDeviceWatcher.Stop();
            }

            mDeviceWatcher.Added -= DeviceWatcher_Added;
            mDeviceWatcher.Updated -= DeviceWatcher_Updated;
            mDeviceWatcher.Removed -= DeviceWatcher_Removed;
            mDeviceWatcher.EnumerationCompleted -= DeviceWatcher_EnumerationCompleted;
            mDeviceWatcher.Stopped -= DeviceWatcher_Stopped;

            mDeviceWatcher = null;
        }

        private void DeviceWatcher_Added(DeviceWatcher sender, DeviceInformation deviceInfo)
        {
            Console.WriteLine(String.Format("Added {0}{1}", deviceInfo.Id, deviceInfo.Name));
          //  Logging.d(TAG, String.Format("Added {0}{1}", deviceInfo.Id, deviceInfo.Name));
            if (sender == mDeviceWatcher)
            {
                if (!string.IsNullOrEmpty(deviceInfo.Name))
                {
                    if (deviceInfo.Name.Equals(mScanDeviceName))
                    {
                        if (null != mScanCallBack)
                        {
                            mScanCallBack.OnScanDevice(deviceInfo);
                        }
                    }
                }
                else
                {
                    if (null != mScanCallBack)
                    {
                        mScanCallBack.OnScanDevice(deviceInfo);
                    }
                }
            }
        }

        private void DeviceWatcher_Updated(DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate)
        {
            Console.WriteLine(String.Format("Updated {0}{1}", deviceInfoUpdate.Id, ""));
        }

        private void DeviceWatcher_Removed(DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate)
        {
            Console.WriteLine(String.Format("Removed {0}{1}", deviceInfoUpdate.Id, ""));
        }

        private void DeviceWatcher_Stopped(DeviceWatcher sender, object args)
        {
            if (sender == mDeviceWatcher)
            {
                Console.WriteLine("Ble Scan Stop!");
            }
        }

        private void DeviceWatcher_EnumerationCompleted(DeviceWatcher sender, object args)
        {
            // Protect against race condition if the task runs after the app stopped the deviceWatcher.
            if (sender == mDeviceWatcher)
            {
                Console.WriteLine("Ble Scan Complete!");
            }
        }
    }
}

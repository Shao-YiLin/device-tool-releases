using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Enumeration;

namespace BleManager
{

    public interface IScanDeviceCallback {
        void OnScanDevice(DeviceInformation device);
    }

    public interface IBleScanService
    {

        IBleScanService OnStartScan(String deviceName, bool isPaired);

        IBleScanService OnStopScan();

        IBleScanService OnSetScanTimer(int timer);

        IBleScanService OnSetScanCallback(IScanDeviceCallback callBack);
    }
}

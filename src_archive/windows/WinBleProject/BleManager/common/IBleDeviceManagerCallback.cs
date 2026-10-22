using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Enumeration;

namespace BleManager.common
{
    #region BLE 管理模块的回调
    public interface IBleDeviceManagerCallback
    {
        /// <summary>
        /// BLE连接状态发生变化
        /// </summary>
        /// <param name="state"></param>
        void ConnectStatusChange(CONNECT_STATE state);

        /// <summary>
        /// 扫描到的设备回调
        /// </summary>
        /// <param name="device"></param>
        void ScanDevice(BleDeviceInfo device);

        /// <summary>
        /// 收到的Notify数据
        /// </summary>
        /// <param name="data"></param>
        /// <param name="len"></param>
        void NotifyData(byte[] data, uint len);
    }
    #endregion
}


using BleManager.common;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Enumeration;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using System.Threading;

namespace BleManager.imp
{
    /// <summary>
    /// ble设备管理
    /// </summary>
    public class BleDeviceManager : IConnectStateCallback, IScanDeviceCallback, IBleCharacteristicChangeCallback
    {
       // private readonly static string TAG = "BleDeviceManager";

        private readonly BleScanService mBleScanService = new BleScanService();
        private readonly BleConnectService mBleConnectService = new BleConnectService();
        private readonly BleCharacteristicService mBleCharacteristic = new BleCharacteristicService();
        private Guid mTxUuid;
        private IBleDeviceManagerCallback mDeviceCallback = null;
        private BleConfigOption mBleConfig = null;
     //   private DeviceInformation mScanDevice = null;

        #region 单例
        private static BleDeviceManager instance = null;
        private readonly static object _lock = new object();
        private BleDeviceManager() {
            mBleConnectService.OnSetConnectStateCallback(this);
            mBleScanService.OnSetScanCallback(this);
            mBleCharacteristic.OnSetCharacteristicCallback(this);
        }

        public static BleDeviceManager GetInstance()
        {
            lock (_lock)
            {
                if (null == instance)
                    instance = new BleDeviceManager();

            }
            return instance;
        }
        #endregion


        private async void InitBleCharacteristicInfo() {
            if (null == mBleConfig)
                return;

            GattDeviceService service = await mBleConnectService.OnGetService(mBleConfig.PrimaryServiceUUID);
            mBleCharacteristic.OnSetBleService(service)
                .OnSetTxCharacteriscUUID(mTxUuid);
             
        }
        public void OnConnectStateChange(CONNECT_STATE state)
        {
            Console.WriteLine(String.Format("device connect status: {0}", state));
            if (state == CONNECT_STATE.connected)
            {
           //     mBleConnectService.OnStartConnect(mScanDevice);
                InitBleCharacteristicInfo();
            }
            else if (state == CONNECT_STATE.disconnected) {
             //   mBleConnectService.OnDisConnect();
            }

            if (null != mDeviceCallback)
                mDeviceCallback.ConnectStatusChange(state);
        }

        public void OnScanDevice(DeviceInformation device)
        {
            if (null == device)
                return;

            BleDeviceInfo bleDevice = new BleDeviceInfo();
            bleDevice.Id = device.Id;
            bleDevice.IsPaired = device.Pairing.IsPaired;

            IReadOnlyDictionary<string, object> pro = device.Properties;
            if (null != pro)
            {
                bleDevice.IsConnected = (bool?)pro["System.Devices.Aep.IsConnected"] == true;
                bleDevice.MacAddress = (string)pro["System.Devices.Aep.DeviceAddress"];
                Console.WriteLine(String.Format("name:{0},mac:{1},connected:{2}", device.Name, bleDevice.MacAddress, bleDevice.IsConnected));
            }

            if (null != mDeviceCallback)
                mDeviceCallback.ScanDevice(bleDevice);

            //if (isConnected) {
            //    mBleScanService.OnStopScan();
            //    mScanDevice = device;
            //    mBleConnectService.OnStartConnect(device);
            //}
        }

        public void OnNotifyData(byte[] data, uint len)
        {
         //   Console.WriteLine(String.Format("notifyData:{0},{1}",len, data.ToString()));

            if (null != mDeviceCallback)
                mDeviceCallback.NotifyData(data, len);
        }

        /// <summary>
        /// 设置ble的基本参数
        /// </summary>
        /// <param name="config"></param>
        /// <returns></returns>
        public BleDeviceManager OnSetDeviceConfig(BleConfigOption config)
        {
            mBleConfig = config;
            mBleConnectService.OnSetConfig(config);
            return this;
        }

        /// <summary>
        /// 设置ble下行特种值的UUID
        /// </summary>
        /// <param name="uuid"></param>
        /// <returns></returns>
        public BleDeviceManager OnSetTxUUID(Guid uuid) {
            mTxUuid = uuid;
            mBleCharacteristic.OnSetTxCharacteriscUUID(uuid);
            return this;
        }

        /// <summary>
        /// 设置设备的状态等变化的回调函数
        /// </summary>
        /// <param name="callback"></param>
        /// <returns></returns>
        public BleDeviceManager OnSetDeviceCallback(IBleDeviceManagerCallback callback) {
            mDeviceCallback = callback;
            return this;
        }

       /// <summary>
       /// 开始扫描设备
       /// </summary>
       /// <param name="filerName">扫描设备的名称</param>
       /// <param name="isPaired">扫描设备的配对状态</param>
        public void OnStartScan(string filerName, bool isPaired) {
            mBleScanService.OnStartScan(filerName, isPaired);
        }

        /// <summary>
        /// 停止设备扫描
        /// </summary>
        public void OnStopScan() {
            mBleScanService.OnStopScan();
        }

        /// <summary>
        /// 连接设备
        /// </summary>
        /// <param name="device">连接设备的基本信息</param>
        public void OnStartConnect(BleDeviceInfo device) {
            mBleConnectService.OnStartConnect(device);
        }

        /// <summary>
        /// 断开连接
        /// </summary>
        public void OnStopConnect() {
            mBleConnectService.OnDisConnect();
        }

        /// <summary>
        /// 获取设备的连接状态
        /// </summary>
        /// <returns></returns>
        public bool IsConnected() {
            return mBleConnectService.IsConnected();
        }

        /// <summary>
        /// 下发数据到ble设备
        /// </summary>
        /// <param name="data"></param>
        public void OnTransimit(byte[] data) {
            if (IsConnected())
                mBleCharacteristic.OnTransmitData(data);
            else
                Console.WriteLine("ble transimit fail { device disconnected }");
        }

        /// <summary>
        /// 获取设备的特种值句柄
        /// </summary>
        /// <param name="serviceUuid">服务的UUID</param>
        /// <param name="characteristicUuid">特种值的UUID</param>
        /// <returns></returns>
        public async Task<GattCharacteristic> OnGetCharacteristic(Guid serviceUuid, Guid characteristicUuid) {

            if (IsConnected()) {
                GattDeviceService service = await mBleConnectService.OnGetService(serviceUuid);
                if(null != service)
                {
                    return await mBleCharacteristic.OnGetCharacteristic(service, characteristicUuid);
                }
            }else
                 Console.WriteLine("get characteristic fail { device disconnected }");
            return null;
        }

        /// <summary>
        /// 申请开启特种值的notify功能属性
        /// </summary>
        /// <param name="characteristic">特种值句柄</param>
        /// <param name="enable">notify功能是否开始</param>
        public async void OnRequestNotify(GattCharacteristic characteristic, bool enable) {
            bool ret = await mBleCharacteristic.RequestNotifyEnable(characteristic, enable);
            if (ret) {
                Console.WriteLine("request notify success");
            }
            else
            {
                Console.WriteLine("request notify fail");
            }
        }
    }
}

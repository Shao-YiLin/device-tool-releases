using BleManager.common;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Devices.Enumeration;
using Windows.Foundation;

namespace BleManager.imp
{
    #region
    /// <summary>
    /// ble 连接服务
    /// </summary>
    public class BleConnectService
    {
  //      private static readonly string TAG = "BleConnectService";

        #region Error Codes
    //    readonly int E_BLUETOOTH_ATT_WRITE_NOT_PERMITTED = unchecked((int)0x80650003);
      //  readonly int E_BLUETOOTH_ATT_INVALID_PDU = unchecked((int)0x80650004);
     //   readonly int E_ACCESSDENIED = unchecked((int)0x80070005);
        readonly int E_DEVICE_NOT_AVAILABLE = unchecked((int)0x800710df); // HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_AVAILABLE)
        #endregion

        private BleConfigOption mBleConfigOption = null;
        private IConnectStateCallback mConnectCallback = null;
        private BluetoothLEDevice mBleDevice = null;
        private CONNECT_STATE mConnectState = CONNECT_STATE.disconnected;
        private GattDeviceService mBlePrimaryService = null;

        public BleConnectService OnSetConfig(BleConfigOption config)
        {
            mBleConfigOption = config;
            return this;
        }

        public BleConnectService OnSetConnectStateCallback(IConnectStateCallback callback)
        {
            mConnectCallback = callback;
            return this;
        }

        private void OnBleConnectStatusChange(BluetoothLEDevice sender, object obj)
        {
            if (null == sender)
                return;

            BluetoothConnectionStatus status = sender.ConnectionStatus;
            Console.WriteLine(String.Format("ble connect status change {0}", status));
            if (status == BluetoothConnectionStatus.Connected)
            {
                if (mConnectState != CONNECT_STATE.connected)
                {
                    OnGetDeviceGattServiceAsync(mBleDevice);
                }
                mConnectState = CONNECT_STATE.connected;
            }
            else if (status == BluetoothConnectionStatus.Disconnected)
            {
                mConnectState = CONNECT_STATE.disconnected;
            }

            if (null != mConnectCallback)
                mConnectCallback.OnConnectStateChange(mConnectState);
        }

        public async void OnStartConnect(BleDeviceInfo deviceInfo)
        {

            if (mBleDevice != null)
                return;

            try
            {
                mBleDevice = await BluetoothLEDevice.FromIdAsync(deviceInfo.Id);
                mBleDevice.ConnectionStatusChanged += OnBleConnectStatusChange;
            }
            catch (Exception ex) when (ex.HResult == E_DEVICE_NOT_AVAILABLE)
            {
                mConnectState = CONNECT_STATE.disconnected;
            }
            finally
            {
                if (null != mBleDevice)
                {
                    mConnectState = CONNECT_STATE.connected;
                    OnGetDeviceGattServiceAsync(mBleDevice);
                    if (null != mConnectCallback)
                        mConnectCallback.OnConnectStateChange(mConnectState);
                }
                else
                {
                    mConnectState = CONNECT_STATE.disconnected;
                }
            }
        }

        private async void OnGetDeviceGattServiceAsync(BluetoothLEDevice device)
        {
            if (null == device || null == mBleConfigOption)
                return;

            GattDeviceServicesResult result = await device.GetGattServicesAsync(BluetoothCacheMode.Uncached);
            if (null == result)
                return;

            if (result.Status == GattCommunicationStatus.Success)
            {
                var services = result.Services;
                //      Console.WriteLine(String.Format("found {0} services", services.Count));
                Guid orgGuid = mBleConfigOption.PrimaryServiceUUID;
                foreach (var service in services)
                {
                    if (null != service)
                    {
                        Guid guid = service.Uuid;
                        if (null != guid && null != orgGuid && guid.Equals(orgGuid))
                        {
                            mBlePrimaryService = service;
                            Console.WriteLine(String.Format("found primary services:{0}", mBlePrimaryService.Uuid));
                            return;
                        }
                    }
                }
            }
        }

        public void OnDisConnect()
        {
            if (null != mBleDevice)
            {
                mBleDevice.ConnectionStatusChanged -= OnBleConnectStatusChange;
                mBleDevice.Dispose();
            }

            mBleDevice = null;
            mConnectState = CONNECT_STATE.disconnected;
        }

        public bool IsConnected()
        {
            return mConnectState == CONNECT_STATE.connected;
        }

        public GattDeviceService OnGetPrimaryService()
        {
            return mBlePrimaryService;
        }

        public async Task<GattDeviceService> OnGetService(Guid uuid)
        {
            if (null == uuid || null == mBleDevice)
                return null;

            if (null != mBleConfigOption && null != mBlePrimaryService)
            {
                if (mBleConfigOption.PrimaryServiceUUID != null && mBleConfigOption.PrimaryServiceUUID.Equals(uuid))
                    return mBlePrimaryService;
            }

            GattDeviceServicesResult result = await mBleDevice.GetGattServicesForUuidAsync(uuid, BluetoothCacheMode.Uncached);
            if (null != result && result.Status == GattCommunicationStatus.Success)
            {
                return result.Services.First();
            }
            return null;
        }
    }
    #endregion
}

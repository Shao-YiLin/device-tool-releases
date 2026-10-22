using BleManager.common;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Devices.Enumeration;
using Windows.Storage.Streams;

namespace BleManager.imp
{
    public class BleCharacteristicService
    {
       // private static readonly string TAG = "BleCharacteristicService";
        private GattDeviceService mGattService = null;
        private GattCharacteristic mTXGattCharacteristic = null;
        private IBleCharacteristicChangeCallback mCharacteristicCallback = null;

        public BleCharacteristicService OnSetBleService(GattDeviceService service)
        {
            mGattService = service;
            return this;
        }

        public BleCharacteristicService OnSetCharacteristicCallback(IBleCharacteristicChangeCallback callback)
        {
            mCharacteristicCallback = callback;
            return this;
        }

        public async Task<GattCharacteristic> OnGetCharacteristic(GattDeviceService service, Guid uuid)
        {
            if (null == service)
                return null;

            if (null != uuid)
            {
                GattCharacteristicsResult result = await service.GetCharacteristicsForUuidAsync(uuid, Windows.Devices.Bluetooth.BluetoothCacheMode.Uncached);
                if (null != result)
                {
                    if (result.Status == GattCommunicationStatus.Success)
                    {
                        return result.Characteristics.First();
                    }
                    else
                    {
                        Console.WriteLine(String.Format("Get Characteristics error: {0}", result.Status));
                    }
                }
            }
            return null;
        }

        public async void OnSetTxCharacteriscUUID(Guid txUUID)
        {
            if (null == txUUID || null == mGattService)
                return;

            GattCharacteristicsResult result = await mGattService.GetCharacteristicsForUuidAsync(txUUID, Windows.Devices.Bluetooth.BluetoothCacheMode.Uncached);
            if (null == result)
                return;

            if (result.Status == GattCommunicationStatus.Success)
            {
                if(null != result.Characteristics && result.Characteristics.Count > 0)
                 mTXGattCharacteristic = result.Characteristics.First();
            }
            else
            {
                Console.WriteLine(String.Format("Get Characteristics error: {0}", result.Status));
            }
        }

        public async void OnTransmitData(GattCharacteristic characteristic, byte[] data)
        {

            if (null == characteristic || null == data)
                return;

            var writer = new DataWriter();
            writer.WriteBytes(data);

            GattCommunicationStatus result = await characteristic.WriteValueAsync(writer.DetachBuffer());
            if (result == GattCommunicationStatus.Success)
            {
                Console.WriteLine("characteristic write success!");
            }
            else
            {
                Console.WriteLine(String.Format("characteristic write fail {0}!", result));
            }
        }

        public async void OnTransmitData(Guid uuid, byte[] data)
        {

            if (null == mGattService)
                return;

            GattCharacteristicsResult result = await mGattService.GetCharacteristicsForUuidAsync(uuid, Windows.Devices.Bluetooth.BluetoothCacheMode.Uncached);
            if (null == result)
                return;

            if (result.Status == GattCommunicationStatus.Success)
            {
                OnTransmitData(result.Characteristics.First(), data);
            }
            else
            {
                Console.WriteLine(String.Format("Get Characteristics error: {0}", result.Status));
            }
        }

        public void OnTransmitData(byte[] data)
        {
            OnTransmitData(mTXGattCharacteristic, data);
        }

        public async Task<bool> RequestNotifyEnable(GattCharacteristic characteristic, bool enable)
        {
            if (null != characteristic)
            {
                characteristic.ValueChanged += CharacteristicValueChanged;
                GattCommunicationStatus status = await characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue.Notify);
                if (status == GattCommunicationStatus.Success)
                    return true;
                else
                    return false;
            }
            return false;
        }

        private void CharacteristicValueChanged(GattCharacteristic sender, GattValueChangedEventArgs args)
        {
            if (null != args)
            {
              //  Console.WriteLine("data receiver");
                var reader = DataReader.FromBuffer(args.CharacteristicValue);
                if (null != mCharacteristicCallback)
                {
                    uint length = args.CharacteristicValue.Length;
                    byte[] data = new byte[length];
                    reader.ReadBytes(data);
                    mCharacteristicCallback.OnNotifyData(data, length);
                }
            }
        }

    }
}

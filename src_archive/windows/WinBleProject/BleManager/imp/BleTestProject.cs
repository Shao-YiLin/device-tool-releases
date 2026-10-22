using BleManager.common;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Enumeration;
using System.Threading;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using System.IO;

namespace BleManager.imp
{
    public class BleTestProject : IBleDeviceManagerCallback
    {

        private BleDeviceInfo mScanDevice = null;
        enum DeviceType
        {
            iFLYMousePro = 1,
            iFLYKeyboard = 2
        }
        private DeviceType mConnectDeviceType = DeviceType.iFLYKeyboard;

        public void ConnectStatusChange(CONNECT_STATE state)
        {
            if (state == CONNECT_STATE.connected)
            {
                Thread.Sleep(2000);
                OnSetRxNotify();
            }
        }

        public void NotifyData(byte[] data, uint len)
        {
            OnParseData(data, len);
        }

        public void ScanDevice(BleDeviceInfo device)
        {
            if (null == device)
                return;

            if (device.IsConnected)
            {
                BleDeviceManager.GetInstance().OnStopScan();
                mScanDevice = device;
                Console.WriteLine("start Connected");
                BleDeviceManager.GetInstance().OnStartConnect(device);
            }

        }

        public void OnTest()
        {
            Test();
        }

        private void Test()
        {
            BleConfigOption config = new BleConfigOption();

            if (DeviceType.iFLYMousePro == mConnectDeviceType)
                config.PrimaryServiceUUID = Constants.MouseCommandServiceUuid;
            else if (DeviceType.iFLYKeyboard == mConnectDeviceType)
                config.PrimaryServiceUUID = Constants.PrimaryServiceUuid;

            config.mMinInterval = 6;
            config.mMaxInterval = 6;
            config.mTimeOut = 60;
            config.mLatency = 0;
            config.mBleMtuSize = 60;

            BleDeviceManager.GetInstance().OnSetDeviceConfig(config)
                .OnSetTxUUID(Constants.MouseCommandTxUuid)
                .OnSetDeviceCallback(this);
            //   .OnStartScan("iFLYMouse Pro");

            if (DeviceType.iFLYMousePro == mConnectDeviceType)
                BleDeviceManager.GetInstance().OnStartScan("M110(5E:15)", true);
            //  BleDeviceManager.GetInstance().OnStartScan("iFLYMouse Pro", true);
            else if (DeviceType.iFLYKeyboard == mConnectDeviceType)
                BleDeviceManager.GetInstance().OnStartScan("M110(5E:15)", true);
            // BleDeviceManager.GetInstance().OnStartScan("iFLYKeyboard", true);

        }

        private static GattCharacteristic mRxCharacteristic = null;
        private async void OnSetRxNotify()
        {
            if (DeviceType.iFLYMousePro == mConnectDeviceType)
                mRxCharacteristic = await BleDeviceManager.GetInstance().OnGetCharacteristic(Constants.MouseAudioServiceUuid, Constants.MouseAudioRxUuid);
            else if (DeviceType.iFLYKeyboard == mConnectDeviceType)
                mRxCharacteristic = await BleDeviceManager.GetInstance().OnGetCharacteristic(Constants.PrimaryServiceUuid, Constants.PrimaryRxUuid);

            if (null != mRxCharacteristic)
            {
                BleDeviceManager.GetInstance().OnRequestNotify(mRxCharacteristic, true);
            }
        }

        //private BinaryWriter mWriter;
        //private FileStream mStream;

        private void OnKeyUp()
        {
            Console.WriteLine("KeyUpAction");

            //   byte[] data = { 0xfe, 0, 0x3, 0x0, 0x0, 0x0, 0x0 };
            //  BleDeviceManager.GetInstance().OnTransimit(data);
            //mWriter.Close();
            //mStream.Close();
        }

        private void OnKeyDown()
        {
            Console.WriteLine("KeyDownAction");
            // byte[] data = { 0xfe, 0, 0x2, 0x0, 0x0, 0x0, 0x0 };
            //mStream = new FileStream("record.pcm", FileMode.OpenOrCreate, FileAccess.Write);
            //mWriter = new BinaryWriter(mStream);
            //    BleDeviceManager.GetInstance().OnTransimit(data);
        }

        private void OnParseData(byte[] data, uint len)
        {
            if (len < 7)
                return;

            byte magic = data[0];
            byte cmd = data[2];

            if (magic == 0xfe)
            {
                if (cmd == 0x9)
                {
                    byte key = data[7];
                    if (key == 1)
                    {
                        byte value = data[8];
                        if (value == 0)
                        {
                            OnKeyUp();
                        }
                        else if (value == 1)
                        {
                            OnKeyDown();
                        }
                    }
                }
                else if (cmd == 0xa0)
                {
                    // 音频数据
                    Console.WriteLine("audio data receiver");
                    //if(null != mWriter)
                    //{
                    //    mWriter.Write(data, 9, 40);
                    //}
                }
            }
        }
    }
}

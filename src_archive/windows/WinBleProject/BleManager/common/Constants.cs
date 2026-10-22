using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Bluetooth.GenericAttributeProfile;

namespace BleManager.common
{
    public class Constants
    {
        // BT_Code: Initializes custom local parameters w/ properties, protection levels as well as common descriptors like User Description. 
        private static readonly GattLocalCharacteristicParameters gattOperandParameters = new GattLocalCharacteristicParameters
        {
            CharacteristicProperties = GattCharacteristicProperties.Write |
                                       GattCharacteristicProperties.WriteWithoutResponse,
            WriteProtectionLevel = GattProtectionLevel.Plain,
            UserDescription = "Operand Characteristic"
        };

        public static readonly GattLocalCharacteristicParameters gattOperatorParameters = new GattLocalCharacteristicParameters
        {
            CharacteristicProperties = GattCharacteristicProperties.Write |
                                       GattCharacteristicProperties.WriteWithoutResponse,
            WriteProtectionLevel = GattProtectionLevel.Plain,
            UserDescription = "Operator Characteristic"
        };

        public static readonly GattLocalCharacteristicParameters gattResultParameters = new GattLocalCharacteristicParameters
        {
            CharacteristicProperties = GattCharacteristicProperties.Read |
                                       GattCharacteristicProperties.Notify,
            WriteProtectionLevel = GattProtectionLevel.Plain,
            UserDescription = "Result Characteristic"
        };

        public static readonly Guid PrimaryServiceUuid = Guid.Parse("0000eae0-0000-1000-8000-00805f9b34fb");
        public static readonly Guid PrimaryTxUuid = Guid.Parse("0000eae1-0000-1000-8000-00805f9b34fb");
        public static readonly Guid PrimaryRxUuid = Guid.Parse("0000eae2-0000-1000-8000-00805f9b34fb");

        public static readonly Guid MouseAudioServiceUuid = Guid.Parse("00010203-0405-0607-0809-0a0b0c0d1911");
        public static readonly Guid MouseAudioRxUuid = Guid.Parse("00010203-0405-0607-0809-0a0b0c0d2b18");

        public static readonly Guid MouseCommandServiceUuid = Guid.Parse("00010203-0405-0607-0809-0a0b0c0d1910");
        public static readonly Guid MouseCommandTxUuid = Guid.Parse("00010203-0405-0607-0809-0a0b0c0d2b10");

    }
}

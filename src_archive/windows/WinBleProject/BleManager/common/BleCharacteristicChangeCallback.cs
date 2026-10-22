using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace BleManager.common
{
    public interface IBleCharacteristicChangeCallback
    {
        void OnNotifyData(byte[] data, uint len);
    }
}

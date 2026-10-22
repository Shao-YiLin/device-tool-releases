using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace BleManager.common
{
    public enum CONNECT_STATE {
           connected = 1,
           disconnected = 0
    }

    public interface IConnectStateCallback
    {
        void OnConnectStateChange(CONNECT_STATE state);
    }
}

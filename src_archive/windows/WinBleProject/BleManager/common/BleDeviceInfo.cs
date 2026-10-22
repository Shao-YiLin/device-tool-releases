using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace BleManager.common
{
    public class BleDeviceInfo
    {
        public String Name { get; set; }
        public String Id { get; set; }
        public bool IsConnected { get; set; }
        public bool IsPaired { get; set; }
        public String MacAddress { get; set; }
    }
}

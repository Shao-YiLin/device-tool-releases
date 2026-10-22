using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace BleManager.common
{
    #region
      /// <summary>
      /// Ble 连接的基本参数配置
      /// </summary>
    public class BleConfigOption
    {
     //   private Guid mPrimaryServiceUUID;
        public Guid PrimaryServiceUUID {
            get;set;
            //get {
            //    return mPrimaryServiceUUID;
            //}
            //set
            //{
            //    if (!value.Equals(mPrimaryServiceUUID))
            //    {
            //        mPrimaryServiceUUID = value;
            //    }
            //}
        }
        public int mBleMtuSize { get; set; }
        public int mMinInterval { get; set; }
        public int mMaxInterval { get; set; }
        public int mLatency { get; set; }
        public int mTimeOut { get; set; }
    }
    #endregion
}

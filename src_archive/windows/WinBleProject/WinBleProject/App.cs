using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;

namespace WinBleProject
{
    class App : Application
    {
        static void Main(string[] args)
        {
            Application app = new Application();
            BleManager.imp.BleTestProject project = new BleManager.imp.BleTestProject();
            project.OnTest();
            app.Run();
        }
    }
}

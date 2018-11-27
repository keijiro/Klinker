using System;
using System.Runtime.InteropServices;

namespace Klinker
{
    internal static class EnumeratorPlugin
    {
        [DllImport("Klinker")]
        public static extern int RetrieveDeviceNames(IntPtr[] names, int maxCount);

        [DllImport("Klinker")]
        public static extern int RetrieveOutputFormatNames(int device, IntPtr[] names, int maxCount);
    }
}

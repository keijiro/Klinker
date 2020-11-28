// Klinker - Blackmagic DeckLink plugin for Unity
// https://github.com/keijiro/Klinker

using System;
using System.Runtime.InteropServices;

namespace Klinker
{
    // Native plugin entry points for enumeration functions
    static class EnumeratorPlugin
    {
        [DllImport("Klinker")]
        public static extern int RetrieveDeviceNames(IntPtr[] names, int maxCount);

        [DllImport("Klinker")]
        public static extern int RetrieveOutputFormatNames(int device, IntPtr[] names, int maxCount);
    }
}

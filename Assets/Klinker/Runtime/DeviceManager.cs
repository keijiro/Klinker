using UnityEngine;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace Klinker
{
    public static class DeviceManager
    {
        #region Enumeration methods

        // Scan available devices and return their names via a newly allocated
        // string array.
        public static string[] GetDeviceNames()
        {
            var count = PluginEntry.RetrieveDeviceNames(_pointers, _pointers.Length);
            var names = new string[count];
            for (var i = 0; i < count; i++) names[i] = Marshal.PtrToStringBSTR(_pointers[i]);
            return names;
        }

        // Scan available devices and store their names into a given collection
        // object.
        public static void GetDeviceNames(ICollection<string> store)
        {
            store.Clear();
            var count = PluginEntry.RetrieveDeviceNames(_pointers, _pointers.Length);
            for (var i = 0; i < count; i++) store.Add(Marshal.PtrToStringBSTR(_pointers[i]));
        }

        // Scan available output formats on a specified device and return their
        // names via a newly allocated string array.
        public static string[] GetOutputFormatNames(int deviceIndex)
        {
            var count = PluginEntry.RetrieveOutputFormatNames(deviceIndex, _pointers, _pointers.Length);
            var names = new string[count];
            for (var i = 0; i < count; i++) names[i] = Marshal.PtrToStringBSTR(_pointers[i]);
            return names;
        }

        // Scan available output formats on a specified device and return their
        // names into a given collection object.
        public static void GetDeviceNames(int deviceIndex, ICollection<string> store)
        {
            store.Clear();
            var count = PluginEntry.RetrieveOutputFormatNames (deviceIndex, _pointers, _pointers.Length);
            for (var i = 0; i < count; i++) store.Add(Marshal.PtrToStringBSTR(_pointers[i]));
        }

        #endregion

        #region Private members

        static System.IntPtr[] _pointers = new System.IntPtr[256];

        #endregion
    }
}

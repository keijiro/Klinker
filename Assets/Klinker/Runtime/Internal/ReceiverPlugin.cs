using UnityEngine;
using System;
using System.Runtime.InteropServices;

namespace Klinker
{
    sealed class ReceiverPlugin : IDisposable
    {
        #region Disposable pattern

        public ReceiverPlugin(int device, int format)
        {
            _plugin = CreateReceiver(device, format);
        }

        ~ReceiverPlugin()
        {
            if (_plugin != IntPtr.Zero)
                Debug.LogError("Receiver instance should be disposed before finalization.");
        }

        public void Dispose()
        {
            if (_plugin != IntPtr.Zero)
            {
                DestroyReceiver(_plugin);
                _plugin = IntPtr.Zero;
            }
        }

        #endregion

        #region Public properties

        public uint ID { get {
            return GetReceiverID(_plugin);
        } }

        public Vector2Int FrameDimensions { get {
            return new Vector2Int(
                GetReceiverFrameWidth(_plugin),
                GetReceiverFrameHeight(_plugin)
            );
        } }

        public string FormatName { get {
            var bstr = GetReceiverFormatName(_plugin);
            if (bstr == IntPtr.Zero) return null;
            return Marshal.PtrToStringBSTR(bstr);
        } }

        public IntPtr TextureUpdateCallback { get {
            return GetTextureUpdateCallback();
        } }

        #endregion

        #region Unmanaged code entry points

        IntPtr _plugin;

        [DllImport("Klinker")]
        public static extern IntPtr CreateReceiver(int device, int format);

        [DllImport("Klinker")]
        public static extern void DestroyReceiver(IntPtr receiver);

        [DllImport("Klinker")]
        public static extern uint GetReceiverID(IntPtr receiver);

        [DllImport("Klinker")]
        public static extern int GetReceiverFrameWidth(IntPtr receiver);

        [DllImport("Klinker")]
        public static extern int GetReceiverFrameHeight(IntPtr receiver);

        [DllImport("Klinker")]
        public static extern IntPtr GetReceiverFormatName(IntPtr receiver);

        [DllImport("Klinker")]
        public static extern IntPtr GetTextureUpdateCallback();

        #endregion
    }
}

// Klinker - Blackmagic DeckLink plugin for Unity
// https://github.com/keijiro/Klinker

using UnityEngine;
using System;
using System.Runtime.InteropServices;

namespace Klinker
{
    // Wrapper class for native plugin receiver functions
    sealed class ReceiverPlugin : IDisposable
    {
        #region Disposable pattern

        public ReceiverPlugin(int device, int format)
        {
            _plugin = CreateReceiver(device, format);
            CheckError();
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

        public long FrameDuration { get {
            return GetReceiverFrameDuration(_plugin);
        } }

        public bool IsProgressive { get {
            return IsReceiverProgressive(_plugin) != 0;
        } }

        public string FormatName { get {
            var bstr = GetReceiverFormatName(_plugin);
            if (bstr == IntPtr.Zero) return null;
            return Marshal.PtrToStringBSTR(bstr);
        } }

        public int QueuedFrameCount { get {
            return CountReceiverQueuedFrames(_plugin);
        } }

        public IntPtr TextureUpdateCallback { get {
            return GetTextureUpdateCallback();
        } }

        public int DropCount { get {
            return CountDroppedReceiverFrames(_plugin);
        } }

        #endregion

        #region Public methods

        public void DequeueFrame()
        {
            DequeueReceiverFrame(_plugin);
            CheckError();
        }

        #endregion

        #region Error handling

        void CheckError()
        {
            if (_plugin == IntPtr.Zero) return;
            var error = GetReceiverError(_plugin);
            if (error == IntPtr.Zero) return;
            var message = Marshal.PtrToStringAnsi(error);
            Dispose();
            throw new InvalidOperationException(message);
        }

        #endregion

        #region Unmanaged code entry points

        IntPtr _plugin;

        [DllImport("Klinker")]
        static extern IntPtr CreateReceiver(int device, int format);

        [DllImport("Klinker")]
        static extern void DestroyReceiver(IntPtr receiver);

        [DllImport("Klinker")]
        static extern uint GetReceiverID(IntPtr receiver);

        [DllImport("Klinker")]
        static extern int GetReceiverFrameWidth(IntPtr receiver);

        [DllImport("Klinker")]
        static extern int GetReceiverFrameHeight(IntPtr receiver);

        [DllImport("Klinker")]
        static extern long GetReceiverFrameDuration(IntPtr receiver);

        [DllImport("Klinker")]
        static extern int IsReceiverProgressive(IntPtr receiver);

        [DllImport("Klinker")]
        static extern IntPtr GetReceiverFormatName(IntPtr receiver);

        [DllImport("Klinker")]
        static extern int CountReceiverQueuedFrames(IntPtr receiver);

        [DllImport("Klinker")]
        static extern void DequeueReceiverFrame(IntPtr receiver);

        [DllImport("Klinker")]
        static extern IntPtr GetTextureUpdateCallback();

        [DllImport("Klinker")]
        static extern int CountDroppedReceiverFrames(IntPtr receiver);

        [DllImport("Klinker")]
        static extern IntPtr GetReceiverError(IntPtr sender);

        #endregion
    }
}

using Unity.Collections;
using Unity.Collections.LowLevel.Unsafe;
using UnityEngine;
using System;
using System.Runtime.InteropServices;

namespace Klinker
{
    sealed class SenderPlugin : IDisposable
    {
        #region Disposable pattern

        public SenderPlugin(int device, int format)
        {
            _plugin = CreateSender(device, format);
        }

        ~SenderPlugin()
        {
            if (_plugin != IntPtr.Zero)
                Debug.LogError("Sender instance should be disposed before finalization.");
        }

        public void Dispose()
        {
            if (_plugin != IntPtr.Zero)
            {
                DestroySender(_plugin);
                _plugin = IntPtr.Zero;
            }
        }

        #endregion

        #region Public properties

        public Vector2Int FrameDimensions { get {
            return new Vector2Int(
                GetSenderFrameWidth(_plugin),
                GetSenderFrameHeight(_plugin)
            );
        } }

        public float FrameRate { get {
            return GetSenderFrameRate(_plugin);
        } }

        public bool IsProgressive { get {
            return IsSenderProgressive(_plugin) != 0;
        } }

        public bool IsReferenceLocked { get {
            return IsSenderReferenceLocked(_plugin) != 0;
        } }

        #endregion

        #region Public methods

        public unsafe void EnqueueFrame<T>(NativeArray<T> data) where T : struct
        {
            EnqueueSenderFrame(_plugin, (IntPtr)data.GetUnsafeReadOnlyPtr());
        }

        public void WaitCompletion(ulong frameNumber)
        {
            WaitSenderCompletion(_plugin, frameNumber);
        }

        #endregion

        #region Unmanaged code entry points

        IntPtr _plugin;

        [DllImport("Klinker")]
        static extern IntPtr CreateSender(int device, int format);

        [DllImport("Klinker")]
        static extern void DestroySender(IntPtr sender);

        [DllImport("Klinker")]
        static extern int GetSenderFrameWidth(IntPtr sender);

        [DllImport("Klinker")]
        static extern int GetSenderFrameHeight(IntPtr sender);

        [DllImport("Klinker")]
        static extern float GetSenderFrameRate(IntPtr sender);

        [DllImport("Klinker")]
        static extern int IsSenderProgressive(IntPtr sender);

        [DllImport("Klinker")]
        static extern int IsSenderReferenceLocked(IntPtr sender);

        [DllImport("Klinker")]
        static extern void EnqueueSenderFrame(IntPtr sender, IntPtr data);

        [DllImport("Klinker")]
        static extern void WaitSenderCompletion(IntPtr sender, ulong frame);

        #endregion
    }
}

using System;
using System.Runtime.InteropServices;

namespace Klinker
{
    internal static class PluginEntry
    {
        #region Common functions

        [DllImport("Klinker")]
        public static extern IntPtr GetTextureUpdateCallback();

        #endregion

        #region Enumeration functions

        [DllImport("Klinker")]
        public static extern int
            RetrieveDeviceNames(IntPtr[] names, int maxCount);

        [DllImport("Klinker")]
        public static extern int
            RetrieveOutputFormatNames(int device, IntPtr[] names, int maxCount);

        #endregion

        #region Sender functions

        [DllImport("Klinker")]
        public static extern IntPtr
            CreateSender(int device, int format, int preroll);

        [DllImport("Klinker")]
        public static extern void DestroySender(IntPtr sender);

        [DllImport("Klinker")]
        public static extern int GetSenderFrameWidth(IntPtr sender);

        [DllImport("Klinker")]
        public static extern int GetSenderFrameHeight(IntPtr sender);

        [DllImport("Klinker")]
        public static extern int IsSenderReferenceLocked(IntPtr sender);

        [DllImport("Klinker")]
        public static extern void UpdateSenderFrame(IntPtr sender, IntPtr data);

        #endregion

        #region Receiver functions

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

        #endregion
    }
}

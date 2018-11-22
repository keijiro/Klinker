using UnityEngine;
using UnityEngine.Rendering;
using System.Runtime.InteropServices;

namespace Klinker
{
    public class FrameReceiver : MonoBehaviour
    {
        #region Editable attributes

        [SerializeField] int _deviceSelection = 0;

        #endregion

        #region Private members

        System.IntPtr _receiver;

        Texture2D _receivedTexture;
        RenderTexture _decodedTexture;

        CommandBuffer _command;
        Material _material;

        #endregion

        #region Public property

        static byte[] _nameBuffer = new byte[256];

        public string FormatName {
            get {
                if (_receiver == System.IntPtr.Zero) return "-";
                var bstr = PluginEntry.GetReceiverFormatName(_receiver);
                if (bstr == System.IntPtr.Zero) return "-";
                return Marshal.PtrToStringBSTR(bstr);
            }
        }

        #endregion

        #region MonoBehaviour implementation

        void Start()
        {
            _receiver = PluginEntry.CreateReceiver(_deviceSelection, 0);
            _command = new CommandBuffer();
            _material = new Material(Shader.Find("Hidden/Klinker/Decoder"));
        }

        void OnDestroy()
        {
            if (_receivedTexture != null) Destroy(_receivedTexture);
            if (_decodedTexture != null) Destroy(_decodedTexture);

            _command.Dispose();
            Destroy(_material);

            PluginEntry.DestroyReceiver(_receiver);
        }

        void Update()
        {
            var width = PluginEntry.GetReceiverFrameWidth(_receiver);
            var height = PluginEntry.GetReceiverFrameHeight(_receiver);

            // Destroy texture objects when the dimensions were changed.
            if (_receivedTexture != null &&
                _receivedTexture.width != width / 2 &&
                _receivedTexture.height != height)
            {
                Destroy(_receivedTexture);
                Destroy(_decodedTexture);
                _receivedTexture = null;
                _decodedTexture = null;
            }

            // Lazy texture initialization
            if (_receivedTexture == null)
            {
                _receivedTexture = new Texture2D
                    (width / 2, height, TextureFormat.ARGB32, false);
                _receivedTexture.filterMode = FilterMode.Point;
                _decodedTexture = new RenderTexture(width, height, 0);
                _decodedTexture.wrapMode = TextureWrapMode.Clamp;
            }

            // Request texture update via the command buffer.
            _command.IssuePluginCustomTextureUpdateV2(
                PluginEntry.GetTextureUpdateCallback(), _receivedTexture,
                PluginEntry.GetReceiverID(_receiver)
            );
            Graphics.ExecuteCommandBuffer(_command);
            _command.Clear();

            // Decode the received texture.
            Graphics.Blit(_receivedTexture, _decodedTexture, _material, 0);

            // Set the texture to the renderer with using a property block.
            var prop = new MaterialPropertyBlock();
            prop.SetTexture("_MainTex", _decodedTexture);
            GetComponent<Renderer>().SetPropertyBlock(prop);
        }

        #endregion
    }
}

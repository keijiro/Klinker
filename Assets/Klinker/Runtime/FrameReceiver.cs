using UnityEngine;
using UnityEngine.Rendering;

namespace Klinker
{
    public class FrameReceiver : MonoBehaviour
    {
        #region Device settings

        [SerializeField] int _deviceSelection = 0;

        #endregion

        #region Target settings

        [SerializeField] RenderTexture _targetTexture;

        public RenderTexture targetTexture {
            get { return _targetTexture; }
            set { _targetTexture = value; }
        }

        [SerializeField] Renderer _targetRenderer;

        public Renderer targetRenderer {
            get { return _targetRenderer; }
            set { _targetRenderer = value; }
        }

        [SerializeField] string _targetMaterialProperty;

        public string targetMaterialProperty {
            get { return _targetMaterialProperty; }
            set { _targetMaterialProperty = value; }
        }

        #endregion

        #region Runtime properties

        RenderTexture _receivedTexture;

        public Texture receivedTexture { get {
            return _targetTexture != null ? _targetTexture : _receivedTexture;
        } }

        static byte[] _nameBuffer = new byte[256];

        public string formatName { get { return _plugin?.FormatName ?? "-"; } }

        #endregion

        #region Private members

        ReceiverPlugin _plugin;
        Texture2D _sourceTexture;
        Material _upsampler;
        MaterialPropertyBlock _propertyBlock;
        int _fieldCount;
        ulong _lastFrameCount;

        #endregion

        #region MonoBehaviour implementation

        void Start()
        {
            _plugin = new ReceiverPlugin(_deviceSelection, 0);
            _upsampler = new Material(Shader.Find("Hidden/Klinker/Upsampler"));
        }

        void OnDestroy()
        {
            _plugin.Dispose();
            Util.Destroy(_sourceTexture);
            Util.Destroy(_receivedTexture);
            Util.Destroy(_upsampler);
        }

        void Update()
        {
            var dimensions = _plugin.FrameDimensions;

            // Renew texture objects when the frame dimensions were changed.
            if (_sourceTexture != null &&
                (_sourceTexture.width != dimensions.x / 2 ||
                 _sourceTexture.height != dimensions.y))
            {
                Util.Destroy(_sourceTexture);
                Util.Destroy(_receivedTexture);
                _sourceTexture = null;
                _receivedTexture = null;
            }

            // Source texture lazy initialization
            if (_sourceTexture == null)
            {
                _sourceTexture = new Texture2D(dimensions.x / 2, dimensions.y);
                _sourceTexture.filterMode = FilterMode.Point;
            }

            // Request texture update via the command buffer.
            Util.IssueTextureUpdateEvent(
                _plugin.TextureUpdateCallback, _sourceTexture, _plugin.ID
            );

            // Receiver texture lazy initialization
            if (_targetTexture == null && _receivedTexture == null)
            {
                _receivedTexture = new RenderTexture(dimensions.x, dimensions.y, 0);
                _receivedTexture.wrapMode = TextureWrapMode.Clamp;
            }

            // Field selection
            var frameCount = _plugin.FrameCount;

            if (frameCount == _lastFrameCount)
                _fieldCount ^= 1;
            else
                _fieldCount = 0;

            _lastFrameCount = frameCount;

            // Chroma upsampling
            var receiver = _targetTexture != null ? _targetTexture : _receivedTexture;
            var pass = _plugin.IsProgressive ? 0 : 1 + _fieldCount;
            Graphics.Blit(_sourceTexture, receiver, _upsampler, pass);
            receiver.IncrementUpdateCount();

            // Renderer override
            if (_targetRenderer != null)
            {
                // Material property block lazy initialization
                if (_propertyBlock == null) _propertyBlock = new MaterialPropertyBlock();

                // Read-modify-write
                _targetRenderer.GetPropertyBlock(_propertyBlock);
                _propertyBlock.SetTexture(_targetMaterialProperty, receiver);
                _targetRenderer.SetPropertyBlock(_propertyBlock);
            }
        }

        #endregion
    }
}

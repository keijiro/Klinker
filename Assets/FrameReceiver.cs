using UnityEngine;
using UnityEngine.Rendering;

namespace Klinker
{
    public class FrameReceiver : MonoBehaviour
    {
        Texture2D _receivedTexture;
        RenderTexture _decodedTexture;
        CommandBuffer _command;
        Material _material;
        System.IntPtr _receiver;

        void Start()
        {
            _receiver = PluginEntry.CreateReceiver();
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
            if (_receivedTexture == null)
            {
                var w = PluginEntry.GetReceiverFrameWidth(_receiver);
                var h = PluginEntry.GetReceiverFrameHeight(_receiver);
                if (w == 0 || h == 0) return;

                _receivedTexture = new Texture2D(w / 2, h, TextureFormat.ARGB32, false);
                _receivedTexture.filterMode = FilterMode.Point;

                _decodedTexture = new RenderTexture(w, h, 0);
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
    }
}

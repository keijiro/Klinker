using UnityEngine;
using UnityEngine.Rendering;

public class Klinker : MonoBehaviour
{
    Texture2D _receivedTexture;
    RenderTexture _decodedTexture;
    CommandBuffer _command;
    Material _material;

    [System.Runtime.InteropServices.DllImport("Klinker")]
    static extern System.IntPtr GetTextureUpdateCallback();

    [System.Runtime.InteropServices.DllImport("Klinker")]
    static extern System.IntPtr CreateReceiver();

    [System.Runtime.InteropServices.DllImport("Klinker")]
    static extern void DestroyReceiver(System.IntPtr receiver);

    [System.Runtime.InteropServices.DllImport("Klinker")]
    static extern uint GetReceiverID(System.IntPtr receiver);

    [System.Runtime.InteropServices.DllImport("Klinker")]
    static extern int GetReceiverFrameWidth(System.IntPtr receiver);

    [System.Runtime.InteropServices.DllImport("Klinker")]
    static extern int GetReceiverFrameHeight(System.IntPtr receiver);

    System.IntPtr _receiver;

    void Start()
    {
        _receiver = CreateReceiver();
        _command = new CommandBuffer();
        _material = new Material(Shader.Find("Hidden/Klinker/Decoder"));
    }

    void OnDestroy()
    {
        if (_receivedTexture != null) Destroy(_receivedTexture);
        if (_decodedTexture != null) Destroy(_decodedTexture);

        _command.Dispose();
        Destroy(_material);

        DestroyReceiver(_receiver);
    }

    void Update()
    {
        if (_receivedTexture == null)
        {
            var w = GetReceiverFrameWidth(_receiver);
            var h = GetReceiverFrameHeight(_receiver);
            if (w == 0 || h == 0) return;

            _receivedTexture = new Texture2D(w / 2, h, TextureFormat.ARGB32, false);
            _receivedTexture.filterMode = FilterMode.Point;

            _decodedTexture = new RenderTexture(w, h, 0);
            _decodedTexture.wrapMode = TextureWrapMode.Clamp;
        }

        // Request texture update via the command buffer.
        _command.IssuePluginCustomTextureUpdateV2(
            GetTextureUpdateCallback(), _receivedTexture,
            GetReceiverID(_receiver)
        );
        Graphics.ExecuteCommandBuffer(_command);
        _command.Clear();

        // Decode the received texture.
        Graphics.Blit(_receivedTexture, _decodedTexture, _material, 0);

        // Set the texture to the renderer with using a property block.
        var prop = new MaterialPropertyBlock();
        prop.SetTexture("_MainTex", _decodedTexture);
        GetComponent<Renderer>().SetPropertyBlock(prop);

        // Rotation
        transform.eulerAngles = new Vector3(10, 20, 30) * Time.time;
    }
}

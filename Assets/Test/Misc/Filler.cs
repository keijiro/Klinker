using UnityEngine;
using UnityEngine.UI;

namespace Klinker.Test
{
    public class Filler : MonoBehaviour
    {
        [SerializeField] Text _text = null;
        [SerializeField] bool _flash = true;
        [SerializeField, HideInInspector] Shader _shader = null;

        Material _material;
        int _frameCount;

        void Update()
        {
            if (!Input.GetKey(KeyCode.Space)) _frameCount++;
            _text.text = _frameCount.ToString("000000");
        }

        void OnRenderImage(RenderTexture source, RenderTexture destination)
        {
            if (_material == null) _material = new Material(_shader);

            _material.SetInt("_FrameCount", _frameCount);

            if (_flash)
                _material.EnableKeyword("ENABLE_FLASH");
            else
                _material.DisableKeyword("ENABLE_FLASH");

            Graphics.Blit(source, destination, _material);
        }
    }
}

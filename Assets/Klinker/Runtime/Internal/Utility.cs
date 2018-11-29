// Klinker - Blackmagic DeckLink plugin for Unity
// https://github.com/keijiro/Klinker

using UnityEngine;
using UnityEngine.Rendering;

namespace Klinker
{
    // Internal utility functions
    static class Util
    {
        public static void Destroy(Object obj)
        {
            if (obj == null) return;

            if (Application.isPlaying)
                Object.Destroy(obj);
            else
                Object.DestroyImmediate(obj);
        }

        static CommandBuffer _commandBuffer;

        public static void
            IssueTextureUpdateEvent(System.IntPtr callback, Texture texture, uint userData)
        {
            if (_commandBuffer == null) _commandBuffer = new CommandBuffer();

            _commandBuffer.IssuePluginCustomTextureUpdateV2(callback, texture, userData);
            Graphics.ExecuteCommandBuffer(_commandBuffer);
            texture.IncrementUpdateCount();

            _commandBuffer.Clear();
        }
    }
}

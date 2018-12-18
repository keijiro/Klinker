// Klinker - Blackmagic DeckLink plugin for Unity
// https://github.com/keijiro/Klinker

using UnityEngine;
using UnityEngine.Rendering;

namespace Klinker
{
    // Internal utility functions
    static class Util
    {
        public const long FlicksPerSecond = 705600000;

        public static long DeltaTimeInFlicks { get {
            return (long)((double)Time.deltaTime * FlicksPerSecond);
        } }

        public static long PackedTimecodeToFlicks(uint timecode, long frameDuration)
        {
            if (timecode == 0xffffffffU) return 0;
            var h = (timecode >> 24) & 0xffU;
            var m = (timecode >> 16) & 0xffU;
            var s = (timecode >>  8) & 0xffU;
            var f = (timecode >>  0) & 0xffU;
            return ((h * 60 + m) * 60 + s) * FlicksPerSecond + f * frameDuration;
        }

        public static void Destroy(Object obj)
        {
            if (obj == null) return;

            if (Application.isPlaying)
                Object.Destroy(obj);
            else
                Object.DestroyImmediate(obj);
        }

        static CommandBuffer _commandBuffer;

        public static void IssueTextureUpdateEvent(System.IntPtr callback, Texture texture, uint userData)
        {
            if (_commandBuffer == null) _commandBuffer = new CommandBuffer();

            _commandBuffer.IssuePluginCustomTextureUpdateV2(callback, texture, userData);
            Graphics.ExecuteCommandBuffer(_commandBuffer);
            texture.IncrementUpdateCount();

            _commandBuffer.Clear();
        }
    }
}

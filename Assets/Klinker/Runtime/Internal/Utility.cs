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

        public static long BcdTimecodeToFlicks(uint timecode, long frameDuration)
        {
            if (timecode == 0xffffffffU) return 0;

            // Unpacked elements
            var hour   = (timecode >> 24) & 0xffU;
            var minute = (timecode >> 16) & 0xffU;
            var second = (timecode >>  8) & 0xffU;
            var field  = (timecode >>  7) & 1U;
            var frame  = (timecode >>  0) & 0x7fU;

            // BCD -> integer value
            hour   = (hour   >> 4) * 10U + (hour   & 0xf);
            minute = (minute >> 4) * 10U + (minute & 0xf);
            second = (second >> 4) * 10U + (second & 0xf);
            frame  = (frame  >> 4) * 10U + (frame  & 0xf);

            // Determine if we should count the field flag in.
            var fielding = frameDuration <= FlicksPerSecond / 50;

            // Flicks conversion
            var t1 = ((hour * 60 + minute) * 60 + second) * FlicksPerSecond;
            var t2 = (frame * (fielding ? 2 : 1) + field) * frameDuration;

            return t1 + t2;
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

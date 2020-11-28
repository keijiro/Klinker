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
            var hour   = (timecode >> 24) & 0x3fU;
            var minute = (timecode >> 16) & 0x7fU;
            var second = (timecode >>  8) & 0x7fU;
            var field  = (timecode >>  7) & 1U;
            var drop   = (timecode >>  6) & 1U;
            var frame  = (timecode >>  0) & 0x3fU;

            // BCD -> integer value
            hour   = (hour   >> 4) * 10U + (hour   & 0xf);
            minute = (minute >> 4) * 10U + (minute & 0xf);
            second = (second >> 4) * 10U + (second & 0xf);
            frame  = (frame  >> 4) * 10U + (frame  & 0xf);

            // Determine if we should count the field flag in.
            var fielding = frameDuration <= FlicksPerSecond / 50;

            // H:M:S to seconds
            long total = (hour * 60 + minute) * 60 + second;

            // Seconds to flicks
            var fps = (FlicksPerSecond + frameDuration - 1) / frameDuration;
            total *= fps * frameDuration;

            // Drop frame conversion
            if (drop != 0)
                total -= (hour * 54 + minute - minute / 10) * 4 * frameDuration;

            // Frame count addition
            total += (frame * (fielding ? 2 : 1) + field) * frameDuration;

            return total;
        }

        public static uint FlicksToBcdTimecode(long flicks, long frameDuration)
        {
            if (flicks <= 0) return 0;

            var drop = FlicksPerSecond % frameDuration != 0 ? 4L : 0L;

            // Frames per second (ceiled)
            var fps = (FlicksPerSecond + frameDuration - 1) / frameDuration;

            // Frames per 10 minute
            var fpm10 = fps * 60 * 10 - drop * 9;

            // Convert the input value to a frame count.
            var frames = flicks / frameDuration;

            // Hours
            var hours = frames / (fpm10 * 6);
            frames -= hours * fpm10 * 6;

            // 10 minutes
            var min10s = frames / fpm10;
            frames -= min10s * fpm10;

            // Minutes
            var min01s = (frames - drop) / (fps * 60 - drop);
            frames -= min01s * (fps * 60 - drop) + (min01s > 0 ? drop : 0);

            // Seconds
            var seconds = frames / fps;
            frames -= seconds * fps;

            // 24 hours wrapping around
            hours %= 24;

            // Drop frame offset
            if (min01s > 0) frames += drop;

            // Divide into fields when using a frame rate over 50Hz.
            var field = 0L;
            if (frameDuration <= FlicksPerSecond / 50)
            {
                field = frames & 1;
                frames /= 2;
            }

            // Integer value -> BCD
            var bcd = 0L;
            bcd += (hours   / 10) * 0x10000000 + (hours   % 10) * 0x1000000;
            bcd += (min10s      ) * 0x00100000 + (min01s      ) * 0x0010000;
            bcd += (seconds / 10) * 0x00001000 + (seconds % 10) * 0x0000100;
            bcd += field          * 0x00000080;
            bcd += drop > 0       ? 0x00000040 : 0;
            bcd += (frames  / 10) * 0x00000010 + (frames  % 10) * 0x0000001;

            return (uint)bcd;
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

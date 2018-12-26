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

            var drop = true;

            if (drop)
            {
                var fps = (FlicksPerSecond + frameDuration - 1) / frameDuration;
                var fpm10 = fps * 60 * 10 - 4 * 9;

                var frames = flicks / frameDuration;

                var hours  = frames / (fpm10 * 6);
                frames -= hours * fpm10 * 6;

                var min10s = frames / fpm10;
                frames -= min10s * fpm10;

                var min01s = frames < fps * 60 ? 0 : (frames - 4) / (fps * 60 - 4);
                if (min01s > 0) frames -= fps * 60;
                if (min01s > 1) frames -= (min01s - 1) * (fps * 60 - 4);

                var seconds = frames / fps;
                frames -= seconds * fps;

                if (min01s > 0) frames += 4;

                hours %= 24;

                var field = (long)0U;

                // Divide into fields when using a frame rate over 50Hz.
                if (frameDuration <= FlicksPerSecond / 50)
                {
                    field = frames & 1;
                    frames /= 2;
                }

                // Integer value -> BCD
                var bcd = (long)0U;
                bcd |= (hours   / 10) * 0x10000000U | (hours   % 10) * 0x1000000U;
                bcd |= (min10s      ) * 0x00100000U | (min01s      ) * 0x0010000U;
                bcd |= (seconds / 10) * 0x00001000U | (seconds % 10) * 0x0000100U;
                bcd |= field         * 0x00000080U;
                bcd |= drop          ? 0x00000040U : 0U;
                bcd |= (frames / 10) * 0x00000010U | (frames  % 10) * 0x0000001U;

                return (uint)bcd;
            }
            else
            {
                var t_div_sec = flicks / FlicksPerSecond;
                var t_mod_sec = flicks - FlicksPerSecond * t_div_sec;

                // Time components
                var hour   = (uint)(t_div_sec / 3600 % 24);
                var minute = (uint)(t_div_sec /   60 % 60);
                var second = (uint)(t_div_sec        % 60);
                
                // Frame/field
                var frame = (uint)(t_mod_sec / frameDuration);
                var field = 0U; 

                // Divide into fields when using a frame rate over 50Hz.
                if (frameDuration <= FlicksPerSecond / 50)
                {
                    field = frame & 1;
                    frame /= 2;
                }

                // Integer value -> BCD
                var bcd = 0U;
                bcd |= (hour   / 10) * 0x10000000U | (hour   % 10) * 0x1000000U;
                bcd |= (minute / 10) * 0x00100000U | (minute % 10) * 0x0010000U;
                bcd |= (second / 10) * 0x00001000U | (second % 10) * 0x0000100U;
                bcd |= field         * 0x00000080U;
                bcd |= drop          ? 0x00000040U : 0U;
                bcd |= (frame  / 10) * 0x00000010U | (frame  % 10) * 0x0000001U;

                return bcd;
            }
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

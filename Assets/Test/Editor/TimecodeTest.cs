using NUnit.Framework;

namespace Klinker.Tests
{
    public static class TimecodeTest
    {
        [Test]
        public static void BcdConversion()
        {
            var frameDuration = Util.FlicksPerSecond * 1001 / 60000;
//            var frameDuration = Util.FlicksPerSecond / 60;

            for (long i = 0; i < 2 * 60 * 60 * 60; i += 13)
            {
                var t1 = i * frameDuration;
                var bcd = Util.FlicksToBcdTimecode(t1, frameDuration);
                var t2 = Util.BcdTimecodeToFlicks(bcd, frameDuration);
                Assert.AreEqual(t1, t2, string.Format("{0:X}/{1:X}, Frame = {2}, BCD = {3:X}", t1, t2, i, bcd));
            }
        }
    }
}

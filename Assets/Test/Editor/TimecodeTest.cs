using NUnit.Framework;

namespace Klinker.Tests
{
    public static class TimecodeTest
    {
        [TestCase(1, 60)]        // 60 Hz
        [TestCase(1001, 60000)]  // 59.94 Hz
        public static void BcdConversion(int mul, int div)
        {
            var frameDuration = Util.FlicksPerSecond * mul / div;
            for (long i = 0; i < 2 * 60 * 60 * 60; i += 13)
            {
                var t1 = i * frameDuration;
                var bcd = Util.FlicksToBcdTimecode(t1, frameDuration);
                var t2 = Util.BcdTimecodeToFlicks(bcd, frameDuration);
                Assert.AreEqual(t1, t2, "Frame = {0}, BCD = {1:X}", i, bcd);
            }
        }
    }
}

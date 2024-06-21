using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEngine;

public static class ISOHelper
{
    private static List<(int, GameRegion, OffsetValue[])> _validationChecks = new()
    {
        {
            (4, GameRegion.NTSC,
            new OffsetValue[]
            {
                // level toc
                new OffsetValue(0x200C30, new byte[] { 0x03, 0x04, 0x00, 0x00, 0xF5, 0xA4, 0x00, 0x00, 0x04, 0x04, 0x00, 0x00, 0x3F, 0x12, 0x00, 0x00, 0x06, 0x04, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x0B, 0x04, 0x00, 0x00, 0x8B, 0x8E, 0x00, 0x00 })
            })
        },
        {
            (3, GameRegion.NTSC,
            new OffsetValue[]
            {
                // level toc
                new OffsetValue(0x1FBC18, new byte[] { 0xF9, 0x03, 0x00, 0x00, 0x52, 0x2C, 0x00, 0x00, 0xFD, 0x03, 0x00, 0x00, 0xDF, 0x24, 0x00, 0x00 })
            })
        },
        {
            (3, GameRegion.PAL,
            new OffsetValue[]
            {
                // level toc
                new OffsetValue(0x1FBC18, new byte[] { 0xF9, 0x03, 0x00, 0x00, 0x52, 0x2C, 0x00, 0x00, 0xFD, 0x03, 0x00, 0x00, 0xE9, 0x24, 0x00, 0x00 })
            })
        },
        {
            (2, GameRegion.NTSC,
            new OffsetValue[]
            {
                // level toc -- Greatest Hits
                new OffsetValue(0x1F97F8, new byte[] { 0xF4, 0x03, 0x00, 0x00, 0x80, 0x1D, 0x00, 0x00, 0xF5, 0x03, 0x00, 0x00, 0x64, 0x0A, 0x00, 0x00 })
            })
        },
        {
            (2, GameRegion.NTSC,
            new OffsetValue[]
            {
                // level toc -- Black Label
                new OffsetValue(0x1F97F8, new byte[] { 0xF4, 0x03, 0x00, 0x00, 0x81, 0x1D, 0x00, 0x00, 0xF5, 0x03, 0x00, 0x00, 0x64, 0x0A, 0x00, 0x00 })
            })
        }
    };

    public static bool ValidateISO(string path, int expectedRacVersion, GameRegion expectedRegion)
    {
        if (!File.Exists(path)) return false;

        var validations = _validationChecks.Where(x => x.Item1 == expectedRacVersion && x.Item2 == expectedRegion);

        foreach (var validation in validations)
        {
            var match = true;

            using (var fs = File.OpenRead(path))
            {
                using (var reader = new BinaryReader(fs))
                {
                    foreach (var offsetValue in validation.Item3)
                    {
                        // check bounds
                        if ((offsetValue.Offset + offsetValue.Value.Length) > fs.Length)
                            match = false;

                        // check contents
                        fs.Position = offsetValue.Offset;
                        var isoValue = reader.ReadBytes(offsetValue.Value.Length);
                        if (!isoValue.SequenceEqual(offsetValue.Value))
                            match = false;
                    }
                }
            }

            if (match) return true;
        }

        return false; // unsupported
    }

    private class OffsetValue
    {
        public long Offset;
        public byte[] Value;

        public OffsetValue() { }
        public OffsetValue(long offset, byte[] value)
        {
            Offset = offset;
            Value = value;
        }
    }
}

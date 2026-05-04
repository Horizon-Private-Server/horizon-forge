using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Globalization;

public static class NumberHelper
{
    public static float? ParseFloatInvariant(string value)
    {
        return float.TryParse(value, NumberStyles.Float, CultureInfo.InvariantCulture, out var v) ? v : null;
    }
}

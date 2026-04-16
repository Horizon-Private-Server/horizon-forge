using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Text;

public static class StringHelper
{
    public static string MaxLength(this string str, int maxLength)
    {
        if (string.IsNullOrEmpty(str))
            return str;

        if (str.Length <= maxLength)
            return str;

        return str.Substring(0, maxLength);
    }

    public static string Escape(this string str)
    {
        if (string.IsNullOrEmpty(str))
            return str;

        return str
            .Replace("\"", "\\\"")
            .Replace("\'", "\\\'")
            ;
    }

    public static string ToCDefine(this string str)
    {
        if (string.IsNullOrEmpty(str))
            return str;

        return str
            .Trim()
            .ToUpper()
            .Replace("\"", "")
            .Replace("\'", "")
            .Replace(" ", "_")
            ;
    }

    public static string ToTitleCase(this string str)
    {
        var firstword = System.Globalization.CultureInfo.CurrentCulture.TextInfo.ToTitleCase(str.Split(' ')[0].ToLower());
        str = str.Replace(str.Split(' ')[0], firstword);
        return str;
    }

    public static string ToInvariantCulture(this float value) => value.ToString(System.Globalization.CultureInfo.InvariantCulture);
    public static string ToInvariantCulture(this float value, string format) => value.ToString(format, System.Globalization.CultureInfo.InvariantCulture);
    public static string ToInvariantCulture(this double value) => value.ToString(System.Globalization.CultureInfo.InvariantCulture);
    public static string ToInvariantCulture(this decimal value) => value.ToString(System.Globalization.CultureInfo.InvariantCulture);
}

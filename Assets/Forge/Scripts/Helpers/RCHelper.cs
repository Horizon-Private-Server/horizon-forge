using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Text;
using UnityEditor;
using UnityEngine;
using UnityEngine.UIElements;

public static class RCHelper
{
    private static Color[] guiTeamColors = new Color[]
    {
        new Color(0x00 / 255f, 0x4C / 255f, 0x73 / 255f),
        new Color(0x00 / 255f, 0x72 / 255f, 0xAC / 255f),
        new Color(0x00 / 255f, 0x26 / 255f, 0x39 / 255f),
        new Color(0x7F / 255f, 0xA5 / 255f, 0xB9 / 255f),
        new Color(0x6D / 255f, 0x13 / 255f, 0x15 / 255f),
        new Color(0xA3 / 255f, 0x1C / 255f, 0x1F / 255f),
        new Color(0x36 / 255f, 0x09 / 255f, 0x0A / 255f),
        new Color(0xB6 / 255f, 0x89 / 255f, 0x8A / 255f),
        new Color(0x27 / 255f, 0x4B / 255f, 0x24 / 255f),
        new Color(0x3A / 255f, 0x70 / 255f, 0x36 / 255f),
        new Color(0x13 / 255f, 0x25 / 255f, 0x12 / 255f),
        new Color(0x93 / 255f, 0xA5 / 255f, 0x91 / 255f),
        new Color(0x91 / 255f, 0x2B / 255f, 0x1C / 255f),
        new Color(0xD9 / 255f, 0x40 / 255f, 0x2A / 255f),
        new Color(0x48 / 255f, 0x15 / 255f, 0x0E / 255f),
        new Color(0xC8 / 255f, 0x95 / 255f, 0x8D / 255f),
        new Color(0xBF / 255f, 0x99 / 255f, 0x26 / 255f),
        new Color(0xFF / 255f, 0xE5 / 255f, 0x39 / 255f),
        new Color(0x5F / 255f, 0x4C / 255f, 0x13 / 255f),
        new Color(0xDF / 255f, 0xCC / 255f, 0x92 / 255f),
        new Color(0x73 / 255f, 0x27 / 255f, 0x63 / 255f),
        new Color(0xAC / 255f, 0x3A / 255f, 0x94 / 255f),
        new Color(0x39 / 255f, 0x13 / 255f, 0x31 / 255f),
        new Color(0xB9 / 255f, 0x93 / 255f, 0xB1 / 255f),
        new Color(0x26 / 255f, 0x73 / 255f, 0x73 / 255f),
        new Color(0x39 / 255f, 0xAC / 255f, 0xAC / 255f),
        new Color(0x13 / 255f, 0x39 / 255f, 0x39 / 255f),
        new Color(0x92 / 255f, 0xB9 / 255f, 0xB9 / 255f),
        new Color(0xBF / 255f, 0x73 / 255f, 0x99 / 255f),
        new Color(0xFF / 255f, 0xAC / 255f, 0xE5 / 255f),
        new Color(0x5F / 255f, 0x39 / 255f, 0x4C / 255f),
        new Color(0xDF / 255f, 0xB9 / 255f, 0xCC / 255f),
        new Color(0x4C / 255f, 0x4C / 255f, 0x26 / 255f),
        new Color(0x72 / 255f, 0x72 / 255f, 0x39 / 255f),
        new Color(0x26 / 255f, 0x26 / 255f, 0x13 / 255f),
        new Color(0xA5 / 255f, 0xA5 / 255f, 0x92 / 255f),
        new Color(0x4C / 255f, 0x00 / 255f, 0x00 / 255f),
        new Color(0x72 / 255f, 0x00 / 255f, 0x00 / 255f),
        new Color(0x26 / 255f, 0x00 / 255f, 0x00 / 255f),
        new Color(0xA5 / 255f, 0x7F / 255f, 0x7F / 255f),
        new Color(0x80 / 255f, 0x80 / 255f, 0x80 / 255f),
        new Color(0xC0 / 255f, 0xC0 / 255f, 0xC0 / 255f),
        new Color(0x40 / 255f, 0x40 / 255f, 0x40 / 255f),
        new Color(0xBF / 255f, 0xBF / 255f, 0xBF / 255f),
    };

    public static Color ToColor(this DLTeamIds teamId, int variant)
    {
        var idx = (int)teamId * 4 + variant;
        if (idx < 0 || idx >= guiTeamColors.Length)
            return Color.black;

        return guiTeamColors[idx];
    }

    public static uint GetAbgrHex(Color color, float? overrideAlpha = null)
    {
        return ((uint)Mathf.RoundToInt((overrideAlpha ?? color.a) * 255) << 24)
            | ((uint)Mathf.RoundToInt((color.b) * 255) << 16)
            | ((uint)Mathf.RoundToInt((color.g) * 255) << 8)
            | ((uint)Mathf.RoundToInt((color.r) * 255) << 0)
            ;
    }
}

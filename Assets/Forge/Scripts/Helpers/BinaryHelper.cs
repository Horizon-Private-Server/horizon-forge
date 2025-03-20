using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Text;
using UnityEditor;
using UnityEngine;
using UnityEngine.UIElements;

public static class BinaryHelper
{
    public static string StrToRatchetStr(string str)
    {
        if (str == null) return null;

        return str
            .Replace("\r", "") // remove \r
            .Replace("\t", "  ") // replace \t with 2 spaces
            .Replace("\n", "\x01") // replace \n with special byte 0x01
            .Replace("\\normal\\", "\x08") // color codes
            .Replace("\\blue\\", "\x09")
            .Replace("\\green\\", "\x0A")
            .Replace("\\pink\\", "\x0B")
            .Replace("\\white\\", "\x0C")
            .Replace("\\black\\", "\x0D")
            .Replace("\\red\\", "\x0E")
            .Replace("\\aqua\\", "\x0F")
            .Replace("\\cross\\", "\x10")
            .Replace("\\circle\\", "\x11")
            .Replace("\\triangle\\", "\x12")
            .Replace("\\square\\", "\x13")
            .Replace("\\l1\\", "\x14")
            .Replace("\\r1\\", "\x15")
            .Replace("\\l2\\", "\x16")
            .Replace("\\r2\\", "\x17")
            .Replace("\\l3\\", "\x18")
            .Replace("\\r3\\", "\x19")
            .Replace("\\left\\", "\x1A")
            .Replace("\\right\\", "\x1B")
            .Replace("\\down\\", "\x1C")
            .Replace("\\up\\", "\x1D")
            .Replace("\\select\\", "\x1E")
            .Replace("\\start\\", "\x1F")
            ;
    }

    public static string RatchetStrToStr(string str)
    {
        if (str == null) return null;

        return str
            .Replace("\x08", "\\normal\\")
            .Replace("\x09", "\\blue\\")
            .Replace("\x0A", "\\green\\")
            .Replace("\x0B", "\\pink\\")
            .Replace("\x0C", "\\white\\")
            .Replace("\x0D", "\\black\\")
            .Replace("\x0E", "\\red\\")
            .Replace("\x0F", "\\aqua\\")
            .Replace("\x10", "\\cross\\")
            .Replace("\x11", "\\circle\\")
            .Replace("\x12", "\\triangle\\")
            .Replace("\x13", "\\square\\")
            .Replace("\x14", "\\l1\\")
            .Replace("\x15", "\\r1\\")
            .Replace("\x16", "\\l2\\")
            .Replace("\x17", "\\r2\\")
            .Replace("\x18", "\\l3\\")
            .Replace("\x19", "\\r3\\")
            .Replace("\x1A", "\\left\\")
            .Replace("\x1B", "\\right\\")
            .Replace("\x1C", "\\down\\")
            .Replace("\x1D", "\\up\\")
            .Replace("\x1E", "\\select\\")
            .Replace("\x1F", "\\start\\")
            .Replace("\x01", "\n")
            ;
    }

    public static void WriteString(this BinaryWriter writer, string value, int fixedLength)
    {
        var bytes = Encoding.ASCII.GetBytes(value ?? string.Empty);

        if (bytes.Length < fixedLength)
        {
            writer.Write(bytes);
            writer.Write(new byte[fixedLength - bytes.Length]);
        }
        else
        {
            writer.Write(bytes, 0, fixedLength);
        }
    }

    public static void WriteCString(this BinaryWriter writer, string value)
    {
        var bytes = Encoding.ASCII.GetBytes(value ?? string.Empty);
        writer.Write(bytes);
        writer.Write((byte)0);
    }

    public static void WriteVectorXZY(this BinaryWriter writer, Vector3 value)
    {
        writer.Write(value.x);
        writer.Write(value.z);
        writer.Write(value.y);
    }
}

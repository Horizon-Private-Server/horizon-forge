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
    public static void WriteString(this BinaryWriter writer, string value, int fixedLength)
    {
        var bytes = Encoding.ASCII.GetBytes(value);

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

    public static void WriteVectorXZY(this BinaryWriter writer, Vector3 value)
    {
        writer.Write(value.x);
        writer.Write(value.z);
        writer.Write(value.y);
    }
}

using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEngine;

public static class TfragHelper
{
    public static void SetChunkTextureIndices(byte[] def, byte[] data, IEnumerable<int> texIndices)
    {
        var texOff = BitConverter.ToInt16(def, 0x1c);
        var texCnt = def[0x28];
        var msphereOff = BitConverter.ToInt16(def, 0x2e);
        var msphereCnt = def[0x2c];

        // invalid
        if (texIndices.Count() != texCnt) throw new InvalidOperationException($"Attempting to set tfrag chunk texture indices of size {texIndices.Count()} for chunk with {texCnt} textures");

        using (var ms = new MemoryStream(data, true))
        {
            using (var writer = new BinaryWriter(ms))
            {
                // update texture defs
                var remap = new Dictionary<int, int>();
                for (int i = 0; i < texCnt; i++)
                {
                    ms.Position = texOff + (i * 0x50);
                    var ogValue = BitConverter.ToInt32(data, (int)ms.Position);
                    var newValue = texIndices.ElementAt(i);
                    remap.Add(ogValue, newValue);
                    writer.Write(newValue);
                }

                // update mspheres
                for (int i = 0; i < msphereCnt; ++i)
                {
                    var off = msphereOff + (i * 0x10) + 0xF;
                    if (remap.TryGetValue(data[off], out var newTexIdx))
                        data[off] = (byte)newTexIdx;
                }
            }
        }
    }

    public static void TransformChunk(byte[] def, byte[] data, Matrix4x4 transformationMatrix)
    {
        var defReadStream = new MemoryStream(def.ToArray());
        var defWriteStream = new MemoryStream(def, true);
        var dataReadStream = new MemoryStream(data.ToArray());
        var dataWriteStream = new MemoryStream(data, true);

        var defReader = new BinaryReader(defReadStream);
        var defWriter = new BinaryWriter(defWriteStream);
        var dataReader = new BinaryReader(dataReadStream);
        var dataWriter = new BinaryWriter(dataWriteStream);

        var bSpherePosition = ReadVector3_1024(defReader);
        var bSphereRadius = defReader.ReadSingle() / 1024f;
        var bSpherePost = bSpherePosition;

        var preCenterPositions = new List<Vector3>();
        var postCenterPositions = new List<Vector3>();

        defReader.BaseStream.Position = 0x2C;
        var vCount = (int)defReader.ReadByte();
        defReader.ReadByte();
        var vOffset = (int)defReader.ReadInt16();

        defReader.BaseStream.Position = 0x1E;
        var colorOffset = (int)defReader.ReadInt16();

        defReader.BaseStream.Position = 0x30;
        var pOffset = (int)defReader.ReadInt16();

        defReader.BaseStream.Position = 0x38;
        var cubeOffset = (int)defReader.ReadInt16();

        // write bsphere
        defWriter.BaseStream.Position = 0;
        WriteVector3_1024(defWriter, bSpherePost = transformationMatrix.MultiplyPoint(bSpherePosition));

        // write vertices
        for (int v = 0; v < vCount; v++)
        {
            dataReader.BaseStream.Position = dataWriter.BaseStream.Position = vOffset + (0x10 * v);
            WriteVector3_1024(dataWriter, transformationMatrix.MultiplyPoint(ReadVector3_1024(dataReader)));
        }

        // write position
        dataReader.BaseStream.Position = dataWriter.BaseStream.Position = pOffset;
        WriteVector3_1024(dataWriter, transformationMatrix.MultiplyPoint(ReadVector3_1024(dataReader)));

        // write cube
        for (int c = 0; c < 8; ++c)
        {
            dataReader.BaseStream.Position = dataWriter.BaseStream.Position = cubeOffset + (c * 8);
            WriteVector3_16(dataWriter, transformationMatrix.MultiplyPoint(ReadVector3_16(dataReader)));
        }

        // find positions in packet
        for (int w = 0; w < colorOffset; w += 4)
        {
            dataReader.BaseStream.Position = w;
            var word = dataReader.ReadInt32();

            // STROW
            if (word == 0x30000000)
            {
                // there are multiple STROWs with different data
                // hack: check if read vector3_32 is within bsphere
                // will probably fail when bsphere includes 0,0,0
                // as integer values will likely be read as a vector near 0,0,0
                var strowPosition = ReadVector3_32(dataReader);
                var dist = Vector3.Distance(strowPosition, bSpherePosition);
                if (dist < bSphereRadius)
                {
                    var transformed = transformationMatrix.MultiplyPoint(strowPosition);

                    preCenterPositions.Add(strowPosition);
                    postCenterPositions.Add(transformed);

                    dataWriter.BaseStream.Position = w + 4;
                    WriteVector3_32(dataWriter, transformed);
                }
            }
        }

        // find and transform displacements
        int lod = 0;
        bool match = false;
        for (int w = 0; w < colorOffset; w += 4)
        {
            dataReader.BaseStream.Position = w;
            var word = dataReader.ReadInt32();

            // UNPACK
            if (((word >> 24) & 0b01100000) == 0b01100000)
            {
                var vn = (word >> 26) & 0b11;
                var vl = (word >> 24) & 0b11;
                var num = (word >> 16) & 0b11111111;
                if (num == 0)
                    num = 256;
                var gsize = ((32 >> vl) * (vn + 1)) / 8;
                var size = num * gsize;
                if (size % 4 != 0)
                    size += 4 - (size % 4);

                size = (1 + (size / 4)) * 4;

                if (gsize == 6)
                {
                    for (int di = 0; di < num; ++di)
                    {
                        dataReader.BaseStream.Position = dataWriter.BaseStream.Position = w + 4 + (di * 6);

                        var displacement = ReadVector3_16_1024(dataReader);
                        var realPos = preCenterPositions[lod] + displacement;
                        WriteVector3_16_1024(dataWriter, transformationMatrix.MultiplyPoint(realPos) - postCenterPositions[lod]);

                        if (!match)
                        {
                            match = true;
                            ++lod;
                        }
                    }
                }

                // skip
                w += size - 4;
            }
        }
    }



    private static Vector3 ReadVector3(BinaryReader reader)
    {
        return new Vector3(reader.ReadSingle(), reader.ReadSingle(), reader.ReadSingle());
    }

    private static Vector3 ReadVector3_1024(BinaryReader reader)
    {
        return new Vector3(reader.ReadSingle() / 1024f, reader.ReadSingle() / 1024f, reader.ReadSingle() / 1024f);
    }

    private static Vector3 ReadVector3_32(BinaryReader reader)
    {
        return new Vector3(reader.ReadInt32() / 1024f, reader.ReadInt32() / 1024f, reader.ReadInt32() / 1024f);
    }

    private static Vector3 ReadVector3_16_1024(BinaryReader reader)
    {
        return new Vector3(reader.ReadInt16() / 1024f, reader.ReadInt16() / 1024f, reader.ReadInt16() / 1024f);
    }

    private static Vector3 ReadVector3_16(BinaryReader reader)
    {
        return new Vector3(reader.ReadInt16() / 16f, reader.ReadInt16() / 16f, reader.ReadInt16() / 16f);
    }

    private static void WriteVector3(BinaryWriter writer, Vector3 vector)
    {
        writer.Write(vector.x);
        writer.Write(vector.y);
        writer.Write(vector.z);
    }
    private static void WriteVector3_1024(BinaryWriter writer, Vector3 vector)
    {
        writer.Write(vector.x * 1024f);
        writer.Write(vector.y * 1024f);
        writer.Write(vector.z * 1024f);
    }

    private static void WriteVector3_32(BinaryWriter writer, Vector3 vector)
    {
        writer.Write((int)(vector.x * 1024f));
        writer.Write((int)(vector.y * 1024f));
        writer.Write((int)(vector.z * 1024f));
    }

    private static void WriteVector3_16_1024(BinaryWriter writer, Vector3 vector)
    {
        writer.Write((short)(vector.x * 1024f));
        writer.Write((short)(vector.y * 1024f));
        writer.Write((short)(vector.z * 1024f));
    }

    private static void WriteVector3_16(BinaryWriter writer, Vector3 vector)
    {
        writer.Write((short)(vector.x * 16f));
        writer.Write((short)(vector.y * 16f));
        writer.Write((short)(vector.z * 16f));
    }
}

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

public static class TieHelper
{
    public static byte[] ConvertTie(int fromRacVersion, int toRacVersion, byte[] tieData)
    {
        if (fromRacVersion == toRacVersion) return tieData;

        // create copy of tie
        var fixedTieData = tieData.ToArray();

        // if gc tie, we need to fix the backface culling
        if (fromRacVersion == 2)
            FixTieGc(fixedTieData);

        // dl ties have 16 byte header at the top of the normals data block
        if (toRacVersion == 4)
            FixTieNormals(fixedTieData, true);
        else if (fromRacVersion == 4)
            FixTieNormals(fixedTieData, false);

        return fixedTieData;
    }

    private static void FixTieGc(byte[] data)
    {
        List<(int, uint)> patches = new List<(int, uint)>();

        using (var ms = new MemoryStream(data))
        {
            using (var reader = new BinaryReader(ms))
            {
                for (int packetIndex = 0; packetIndex < 3; ++packetIndex)
                {
                    reader.BaseStream.Position = 0x00 + (packetIndex * 4);
                    var packetDefOffset = reader.ReadInt32();
                    reader.BaseStream.Position = 0x0C + packetIndex;
                    var packetDefCount = reader.ReadByte();

                    // parse only valid packets
                    if (packetDefOffset > 0 && packetDefCount > 0)
                    {
                        for (int packetDefIndex = 0; packetDefIndex < packetDefCount; ++packetDefIndex)
                        {
                            // get offset to packet data
                            reader.BaseStream.Position = packetDefOffset + (0x10 * packetDefIndex);
                            var packetDataOffset = reader.ReadInt32() + packetDefOffset;

                            // set bfc_distance to 0
                            data[packetDefOffset + (0x10 * packetDefIndex) + 5] = 0;

                            // read first int of packet
                            reader.BaseStream.Position = packetDataOffset;
                            var packetData1 = reader.ReadInt32();

                            // read second int of packet
                            reader.BaseStream.Position = packetDataOffset + 4;
                            var packetData2 = reader.ReadInt32();

                            // read third int of packet
                            reader.BaseStream.Position = packetDataOffset + 8;
                            var packetData3 = reader.ReadInt32();

                            // read int at 0x0C in packet data
                            reader.BaseStream.Position = packetDataOffset + 0x0C;
                            var packetDataValue = reader.ReadInt32();

                            // we only need to update the value if the value is 0
                            if (true || packetDataValue == 0)
                            {
                                uint value = 0xFFFFFFF7;

                                // this may be incomplete
                                if (packetData1 == 0)
                                    value = 0xFFFFFFF7;
                                else if (packetData2 > 0 && packetData3 > 0)
                                    value = 0xFFFFFFE8;
                                else if (packetData2 > 0)
                                    value = 0xFFFFFFED;
                                else
                                    value = 0xFFFFFFF2;

                                patches.Add((packetDataOffset + 0x0C, value));
                            }
                            else
                            {
                                switch ((uint)packetDataValue)
                                {
                                    case 0xFFFFFFF7:
                                    case 0xFFFFFFED:
                                    case 0xFFFFFFE8:
                                    case 0xFFFFFFF2:
                                        break;
                                }
                            }
                        }
                    }
                }
            }
        }

        using (var ms = new MemoryStream(data, true))
        {
            using (var writer = new BinaryWriter(ms))
            {
                foreach (var patch in patches)
                {
                    ms.Position = patch.Item1;
                    writer.Write(patch.Item2);
                }
            }
        }
    }

    private static void FixTieNormals(byte[] data, bool header)
    {
        using (var ms = new MemoryStream(data))
        {
            using (var reader = new BinaryReader(ms))
            {
                ms.Position = 0x34;
                var normOffset = reader.ReadUInt32();
                var normCount = reader.ReadInt16();

                ms.Position = normOffset;
                var normHeader = reader.ReadBytes(0x10);
                var hasHeader = normHeader[0x9] == 0xDE && normHeader[0x8] == 0xAD; // header always ends in 0x0000DEAD0000DEAD

                if (header && !hasHeader)
                {
                    // add

                    // must have at least 0x10 bytes worth of normal data for this hack to work
                    // since we shift the data down and lose the last 0x10 bytes (2 normals)
                    // ideally we figure out what references the data after the normal block and increment its offset to account for the new data

                    // read normal data
                    ms.Position = normOffset;
                    var normData = reader.ReadBytes(normCount * 8);

                    // shift normal data down 0x10 bytes
                    // and write the deadlocked header
                    // we lose the last 2 normal entries
                    using (var wms = new MemoryStream(data, true))
                    {
                        using (var writer = new BinaryWriter(wms))
                        {
                            wms.Position = normOffset;
                            writer.Write(0.01f);
                            writer.Write(0);
                            writer.Write(0xDEAD);
                            writer.Write(0xDEAD);
                            writer.Write(normData.Take(normData.Length - 0x10).ToArray());
                        }
                    }
                }
                else if (!header && hasHeader)
                {
                    // remove

                    // read normal data, skipping header
                    reader.BaseStream.Position = normOffset + 0x10;
                    var normData = reader.ReadBytes(normCount * 8);

                    // write back normal data without header
                    using (var wms = new MemoryStream(data, true))
                    {
                        using (var writer = new BinaryWriter(wms))
                        {
                            wms.Position = normOffset;
                            writer.Write(normData);
                        }
                    }
                }
            }
        }
    }
}

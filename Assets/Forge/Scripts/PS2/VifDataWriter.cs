using System;
using System.IO;

/// <summary>
/// Writes a VIF command list into an in-memory byte stream.
/// Provides typed methods for STROW, STMOD, STCYCL, NOP, UNPACK, and padding.
/// </summary>
public class VifDataWriter : IDisposable
{
    private readonly MemoryStream _stream;
    private readonly BinaryWriter _writer;

    // -----------------------------------------------------------------------
    // VIF opcode base constants (upper bytes of the 32-bit command word)
    // -----------------------------------------------------------------------
    private const uint CMD_NOP    = 0x00000000u;
    private const uint CMD_STCYCL = 0x01000000u;
    private const uint CMD_STMOD  = 0x05000000u;
    private const uint CMD_STROW  = 0x30000000u;
    private const uint CMD_UNPACK = 0x60000000u; // bits [30:24] = 0b110_xxxx (cmd = 0x60 | vnvl)

    // -----------------------------------------------------------------------
    // Constructor
    // -----------------------------------------------------------------------

    public VifDataWriter()
    {
        _stream = new MemoryStream();
        _writer = new BinaryWriter(_stream);
    }

    // -----------------------------------------------------------------------
    // Public API
    // -----------------------------------------------------------------------

    /// <summary>Current write position in the stream (bytes).</summary>
    public int Position => (int)_stream.Position;

    /// <summary>
    /// Writes a STROW VIF command (opcode 0x30000000) followed by the
    /// four 32-bit row register values.
    /// </summary>
    public void WriteStrow(VifHelper.Strow strow)
    {
        _writer.Write(CMD_STROW);
        _writer.Write(strow.R0);
        _writer.Write(strow.R1);
        _writer.Write(strow.R2);
        _writer.Write(strow.R3);
    }

    /// <summary>
    /// Writes a STMOD VIF command.
    /// <paramref name="mode"/> = 0 → replace mode off (0x05000000),
    /// <paramref name="mode"/> = 1 → replace mode on  (0x05000001).
    /// </summary>
    public void WriteStmod(int mode)
    {
        _writer.Write(CMD_STMOD | ((uint)mode & 0x3u));
    }

    /// <summary>
    /// Writes a STCYCL VIF command.
    /// Command word: 0x01000000 | (wl &lt;&lt; 8) | cl.
    /// Common: wl=1, cl=2 → 0x01000102; wl=4, cl=4 → 0x01000404.
    /// </summary>
    public void WriteStcycl(int wl, int cl)
    {
        _writer.Write(CMD_STCYCL | (((uint)wl & 0xFFu) << 8) | ((uint)cl & 0xFFu));
    }

    /// <summary>
    /// Writes a NOP VIF command (0x00000000).
    /// </summary>
    public void WriteNop()
    {
        _writer.Write(CMD_NOP);
    }

    /// <summary>
    /// Writes an UNPACK VIF command header followed by the payload bytes.
    /// The element count (NUM field, bits [23:16]) is derived from the
    /// payload length and the per-element size for the given <paramref name="vnvl"/>.
    /// The payload is padded to a 4-byte boundary with zero bytes.
    /// </summary>
    /// <param name="payload">Raw bytes to upload to VU memory.</param>
    /// <param name="vnvl">VN/VL format descriptor.</param>
    /// <param name="usn">Signed / unsigned flag.</param>
    /// <param name="addr">Destination VU memory address (bits [9:0]).</param>
    public void WriteUnpack(byte[] payload, VifHelper.VifVnVl vnvl, VifHelper.VifUsn usn, int addr)
    {
        int elementSize = GetElementSize(vnvl);
        if (elementSize <= 0)
            throw new ArgumentException($"Unsupported VifVnVl value: {vnvl}", nameof(vnvl));

        int elementCount = payload.Length / elementSize;
        if (elementCount < 1 || elementCount > 256)
            throw new ArgumentOutOfRangeException(nameof(payload),
                $"Element count {elementCount} is outside the valid range [1, 256].");

        // NUM field: 256 is encoded as 0 per PS2 spec.
        int numField = elementCount == 256 ? 0 : elementCount;

        // Build the UNPACK command word:
        //   bits [30:24]  = 0b110_xxxx  where xxxx = vnvl (4 bits)
        //   bit  [23]     = interrupt (always 0 here)
        //   bits [22:16]  = NUM
        //   bit  [15]     = FLG (1 = USE_VIF1_TOPS — required for VU1 double-buffering)
        //   bit  [14]     = USN
        //   bits [9:0]    = ADDR
        uint cmd = CMD_UNPACK
                 | (((uint)vnvl & 0x0Fu) << 24)    // bits [27:24] — VN/VL
                 | ((uint)(numField & 0xFF) << 16)  // bits [23:16] — NUM
                 | (1u << 15)                        // bit  [15]    — FLG = USE_VIF1_TOPS
                 | ((uint)((int)usn & 0x1) << 14)   // bit  [14]    — USN
                 | ((uint)(addr & 0x3FF));           // bits [9:0]   — ADDR

        _writer.Write(cmd);
        _writer.Write(payload);

        // Pad payload to 4-byte alignment.
        int remainder = payload.Length % 4;
        if (remainder != 0)
        {
            int pad = 4 - remainder;
            for (int i = 0; i < pad; i++)
                _writer.Write((byte)0);
        }
    }

    /// <summary>
    /// Pads the stream to the next multiple of <paramref name="alignment"/> bytes
    /// by writing <paramref name="fill"/> bytes.
    /// </summary>
    public void Pad(int alignment, byte fill = 0)
    {
        if (alignment <= 0)
            throw new ArgumentOutOfRangeException(nameof(alignment), "Alignment must be positive.");

        int pos = (int)_stream.Position;
        int remainder = pos % alignment;
        if (remainder == 0)
            return;

        int padCount = alignment - remainder;
        for (int i = 0; i < padCount; i++)
            _writer.Write(fill);
    }

    /// <summary>
    /// Returns the accumulated byte array from the internal stream.
    /// </summary>
    public byte[] ToArray()
    {
        _writer.Flush();
        return _stream.ToArray();
    }

    // -----------------------------------------------------------------------
    // IDisposable
    // -----------------------------------------------------------------------

    public void Dispose()
    {
        _writer.Dispose();
        _stream.Dispose();
    }

    // -----------------------------------------------------------------------
    // Private helpers
    // -----------------------------------------------------------------------

    /// <summary>
    /// Returns the byte size of a single element for the given VN/VL format.
    /// Mirrors the logic in <see cref="VifHelper.VifCode.GetElementSize"/>.
    ///   element_size = ((32 >> vl) * (vn + 1)) / 8
    /// where vn = bits[3:2] and vl = bits[1:0] of the VifVnVl value.
    /// </summary>
    private static int GetElementSize(VifHelper.VifVnVl vnvl)
    {
        int raw = (int)vnvl;
        int vn = (raw & 0b1100) >> 2;
        int vl = raw & 0b0011;
        return ((32 >> vl) * (vn + 1)) / 8;
    }
}

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

// copied from
// https://github.com/chaoticgd/wrench/blob/a31675f46fa5b91ed0ce4c3206ebefa6c6f47ec1/src/core/vif.h
// credits chaoticgd

public static class VifHelper
{

	public enum VifCmd
	{
		NOP = 0b0000000,
		STCYCL = 0b0000001,
		OFFSET = 0b0000010,
		BASE = 0b0000011,
		ITOP = 0b0000100,
		STMOD = 0b0000101,
		MSKPATH3 = 0b0000110,
		MARK = 0b0000111,
		FLUSHE = 0b0010000,
		FLUSH = 0b0010001,
		FLUSHA = 0b0010011,
		MSCAL = 0b0010100,
		MSCNT = 0b0010111,
		MSCALF = 0b0010101,
		STMASK = 0b0100000,
		STROW = 0b0110000,
		STCOL = 0b0110001,
		MPG = 0b1001010,
		DIRECT = 0b1010000,
		DIRECTHL = 0b1010001
	};

	public enum VifVnVl
	{
		S_32 = 0b0000,
		S_16 = 0b0001,
		ERR_0010 = 0b0010,
		ERR_0011 = 0b0011,
		V2_32 = 0b0100,
		V2_16 = 0b0101,
		V2_8 = 0b0110,
		ERR_0111 = 0b0111,
		V3_32 = 0b1000,
		V3_16 = 0b1001,
		V3_8 = 0b1010,
		ERR_1011 = 0b1011,
		V4_32 = 0b1100,
		V4_16 = 0b1101,
		V4_8 = 0b1110,
		V4_5 = 0b1111
	};

	public enum VifFlg
	{
		DO_NOT_USE_VIF1_TOPS = 0x0,
		USE_VIF1_TOPS = 0x1
	};

	public enum VifUsn
	{
		SIGNED = 0x0,
		UNSIGNED = 0x1
	};

	public struct Unpack
	{
		public VifVnVl Vnvl;
		public VifFlg Flg;
		public VifUsn Usn;
		public int Addr;
	};

	public struct Direct
	{
		public int Size;
	}

	public struct Directhl
	{
		public int Size;
	}

	public class VifCode
	{
		public uint Raw { get; set; }
		public int Interrupt { get; set; }
		public int Num { get; set; }
		public VifCmd Cmd { get; set; }
		public Unpack Unpack { get; set; }
		public Direct Direct { get; set; }
		public Directhl Directhl { get; set; }


		static ulong bit_range(ulong val, int lo, int hi) => (val >> lo) & (ulong)((1 << (hi - lo + 1)) - 1);

		public static VifCode Parse(uint val)
		{
			var code = new VifCode();
			code.Raw = val;
			code.Interrupt = (int)bit_range(val, 31, 31);
			code.Cmd = (VifCmd)(bit_range(val, 24, 30));
			code.Num = (int)bit_range(val, 16, 23);
			code.Num = code.Num != 0 ? code.Num : 256;

			switch (code.Cmd)
			{
				case VifCmd.NOP:
					break;
				case VifCmd.STCYCL:
					//code.stcycl.wl = bit_range(val, 8, 15);
					//code.stcycl.cl = bit_range(val, 0, 7);
					break;
				case VifCmd.OFFSET:
					//code.offset.offset = bit_range(val, 0, 9);
					break;
				case VifCmd.BASE:
					//code.base.base = bit_range(val, 0, 9);
					break;
				case VifCmd.ITOP:
					//code.itop.addr = bit_range(val, 0, 9);
					break;
				case VifCmd.STMOD:
					//code.stmod.mode = bit_range(val, 0, 1);
					break;
				case VifCmd.MSKPATH3:
					//code.mskpath3.mask = bit_range(val, 15, 15);
					break;
				case VifCmd.MARK:
					//code.mark.mark = bit_range(val, 0, 15);
					break;
				case VifCmd.FLUSHE:
				case VifCmd.FLUSH:
				case VifCmd.FLUSHA:
					break;
				case VifCmd.MSCAL:
					//code.mscal.execaddr = bit_range(val, 0, 15);
					break;
				case VifCmd.MSCNT:
					break;
				case VifCmd.MSCALF:
					//code.mscalf.execaddr = bit_range(val, 0, 15);
					break;
				case VifCmd.STMASK:
				case VifCmd.STROW:
				case VifCmd.STCOL:
					break;
				case VifCmd.MPG:
					//code.mpg.loadaddr = bit_range(val, 0, 15);
					break;
				case VifCmd.DIRECT:
					var dsize = (int)bit_range(val, 0, 15);
					code.Direct = new Direct() { Size = dsize != 0 ? dsize : 65536 };
					break;
				case VifCmd.DIRECTHL:
					var dhlsize = (int)bit_range(val, 0, 15);
					code.Directhl = new Directhl() { Size = dhlsize != 0 ? dhlsize : 65536 };
					break;
				default:
					if (!code.IsUnpack())
						return null;

					code.Unpack = new Unpack()
					{
						Vnvl = (VifVnVl)bit_range(val, 24, 27),
						Flg = (VifFlg)bit_range(val, 15, 15),
						Usn = (VifUsn)bit_range(val, 14, 14),
						Addr = (int)bit_range(val, 0, 9)
					};
					break;
			}

			return code;
		}


		public bool IsUnpack()
		{
			return ((int)Cmd & 0b1100000) == 0b1100000;
		}

		public bool IsStrow()
		{
			return ((int)Cmd & 0b0110000) == 0b0110000;
		}

		public long GetPacketSize()
		{
			long result = 0;

			switch (Cmd)
			{
				case VifCmd.NOP:
				case VifCmd.STCYCL:
				case VifCmd.OFFSET:
				case VifCmd.BASE:
				case VifCmd.ITOP:
				case VifCmd.STMOD:
				case VifCmd.MSKPATH3:
				case VifCmd.MARK:
				case VifCmd.FLUSHE:
				case VifCmd.FLUSH:
				case VifCmd.FLUSHA:
				case VifCmd.MSCAL:
				case VifCmd.MSCNT:
				case VifCmd.MSCALF:
					result = 1;
					break;
				case VifCmd.STMASK:
					result = 2;
					break;
				case VifCmd.STROW:
				case VifCmd.STCOL:
					result = 5;
					break;
				case VifCmd.MPG:
					result = 1 + Num * 2;
					break;
				case VifCmd.DIRECT:
					result = 1 + Direct.Size * 4;
					break;
				case VifCmd.DIRECTHL:
					result = 1 + Directhl.Size * 4;
					break;
				default:
					if (IsUnpack())
					{
						int size = Num * GetElementSize();
						if (size % 4 != 0)
							size += 4 - (size % 4);

						result = 1 + (size / 4);
					}
					break;
			}

			if (result == 0) throw new InvalidOperationException();
			return result * 4;
		}

		public int GetElementSize()
		{
			return ((32 >> vl()) * (vn() + 1)) / 8;
		}

		int vn()
		{
			return ((int)Unpack.Vnvl & 0b1100) >> 2;
		}

		int vl()
		{
			return ((int)Unpack.Vnvl) & 0b11;
		}

	}

	public class VifPacket
	{
		public int Offset { get; set; }
		public VifCode Code { get; set; }
		public byte[] Data { get; set; }
		public string Error { get; set; }
	}

	public static List<VifPacket> ParsePackets(BinaryReader reader, int length)
	{
		var packets = new List<VifPacket>();
		while (reader.BaseStream.Position < length)
		{
			var packet = new VifPacket();
			packets.Add(packet);
			packet.Offset = (int)reader.BaseStream.Position;

			var val = reader.ReadUInt32();

			packet.Code = VifCode.Parse(val);
			if (packet.Code == null)
			{
				packet.Error = "failed to disassemble vif code";
				break;
			}

			var packetSize = packet.Code.GetPacketSize();
			if (packetSize > 0x10000)
			{
				packet.Error = "vif packet too big";
				break;
			}

			if ((packet.Offset + packetSize) > length)
			{
				packet.Error = "vif packet overruns buffer";
				break;
			}

			reader.BaseStream.Position += (int)packetSize - 4;
		}

		return packets;
	}
}

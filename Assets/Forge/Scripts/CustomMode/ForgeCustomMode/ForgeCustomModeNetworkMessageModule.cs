using System;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Text;
using UnityEditor;
using UnityEngine;
using UnityEngine.Internal;

[Serializable]
public class ForgeCustomModeNetworkMessageModule : MonoBehaviour, IForgeCustomModeModule
{
	[Serializable]
	public class NetworkMessageArg
	{
		public string Name;
		public string Type;
		public Int32Override ArraySize;

		public string TypeName => Name.ToCVar();
		public string TypeDeclaration => ArraySize.HasOverride ? $"{Type} {TypeName}[{ArraySize.OverrideValue}]" : $"{Type} {TypeName}";
	}

	[Serializable]
	public class NetworkMessage
	{
		public string Name;
		[Tooltip("If set, broadcasted messages will also be relayed to the local recv handler.")]
		public bool RelayToLocalOnBroadcast;
		[Tooltip("Use UDP or TCP for transport.")]
		public bool SendUnreliably;
		[Tooltip("If only the host is allowed to send/broadcast this message.")]
		public bool HostOnly;
		[Tooltip("Custom message arguments.")]
		public List<NetworkMessageArg> Args = new List<NetworkMessageArg>();
		public List<string> RecvHandlers = new List<string>();

		public string EnumName => $"CGM_NET_MSG_{Name.ToCDefine()}";
		public string StructTypeName => $"{Name.ToCVar()}Message";
		public string RecvFuncName => $"cgmNetMsgOnRecv_{Name.ToCVar()}";
		public string BroadcastFuncName => $"cgmNetMsgBroadcast_{Name.ToCVar()}";
		public string SendFuncName => $"cgmNetMsgSend_{Name.ToCVar()}";
	}

	public List<NetworkMessage> Messages = new List<NetworkMessage>();
	
	[HelpBox("Eg: #include \"mytypes.h\"")]
	public List<string> Includes = new List<string>();

	public int ExecutionOrder => 2;

	public void OnValidate()
	{
		// limit messages to 32
		while (Messages.Count > 32)
			Messages.RemoveAt(32);
	}

	public void Configure(string buildFolder, CodeGenState state)
	{
		if (Messages.Count == 0)
			return;
			
        var srcFolder = Path.Combine(buildFolder, FolderNames.CodeBuildSrcFolder);
        var incFolder = Path.Combine(buildFolder, FolderNames.CodeBuildIncludeFolder);

		// add cgm_netmsg
		File.WriteAllText(Path.Combine(incFolder, "cgm_netmsg.h"), BuildNetMsgHeader());
		File.WriteAllText(Path.Combine(srcFolder, "cgm_netmsg.c"), BuildNetMsgSource());
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/cgm_netmsg.o");
        state.LDFlags.Add("-DFORGE_CGM_NETMSG");
        state.Includes.Add("#include \"cgm_netmsg.h\"");
        state.InitBody.Add("cgmNetMsgInit();");
        state.CleanupBody.Add("cgmNetMsgCleanup();");
	}

	public void WriteExData(BinaryWriter writer)
	{
		
	}

	string BuildNetMsgHeader()
	{
		var sb = new StringBuilder();

		sb.AppendLine("#ifndef FORGE_CGM_NETMSG_H");
		sb.AppendLine("#define FORGE_CGM_NETMSG_H");
		sb.AppendLine();
		sb.AppendLine("#include <libdl/net.h>");
		sb.AppendLine("#include <libdl/utils.h>");
		sb.AppendLine("#include <libdl/math3d.h>");
		sb.AppendLine("#include <libdl/math.h>");
		sb.AppendLine("#include <libdl/player.h>");
		sb.AppendLine("#include <libdl/moby.h>");
		sb.AppendLine("#include <libdl/game.h>");
		sb.AppendLine("#include <libdl/gamesettings.h>");
		sb.AppendLine("#include <libdl/string.h>");
		foreach (var include in Includes)
			sb.AppendLine(include);
		sb.AppendLine();

		// enums
		sb.AppendLine("//--------------------------------------------------------------------------");
		sb.AppendLine("enum CgmNetMsgIds");
		sb.AppendLine("{");
		int msgId = 150;
		foreach (var msg in Messages)
		{
			sb.AppendLine($"\t{msg.EnumName} = {msgId},");
			++msgId;
		}
		sb.AppendLine("};");

		// message data structures
		sb.AppendLine("");
		sb.AppendLine("//--------------------------------------------------------------------------");
		foreach (var msg in Messages)
		{
			sb.AppendLine($"struct {msg.StructTypeName}");
			sb.AppendLine("{");
			foreach (var arg in msg.Args)
				sb.AppendLine($"\t{arg.TypeDeclaration};");
			sb.AppendLine("};");
		}

		// function declarations
		sb.AppendLine("");
		sb.AppendLine("//--------------------------------------------------------------------------");
		foreach (var msg in Messages)
		{
			foreach (var recvHandler in msg.RecvHandlers)
				sb.AppendLine($"void {recvHandler}(int fromClientId, struct {msg.StructTypeName} *msg);");
			sb.AppendLine($"void {msg.BroadcastFuncName}(struct {msg.StructTypeName} *msg);");
			sb.AppendLine($"void {msg.SendFuncName}(int toClientId, struct {msg.StructTypeName} *msg);");
		}
		sb.AppendLine("void cgmNetMsgInit(void);");
		sb.AppendLine("void cgmNetMsgCleanup(void);");

		sb.AppendLine();
		sb.AppendLine("#endif // FORGE_CGM_NETMSG_H");
		return sb.ToString();
	}

	string BuildNetMsgSource()
	{
		var sb = new StringBuilder();

		sb.AppendLine("#include \"cgm_netmsg.h\"");
		sb.AppendLine();

		// write message methods
		foreach (var msg in Messages)
		{
			var transportFlags = msg.SendUnreliably ? "0" : "NET_DELIVERY_CRITICAL";

			sb.AppendLine("//--------------------------------------------------------------------------");
			sb.AppendLine($"// {msg.Name}");
			sb.AppendLine("//--------------------------------------------------------------------------");
			
			// recv
			sb.AppendLine($"int {msg.RecvFuncName}(void* connection, void* data)");
			sb.AppendLine("{");
			sb.AppendLine("\tchar fromClientId;");
			sb.AppendLine($"\tstruct {msg.StructTypeName} msg;");
			sb.AppendLine();
			sb.AppendLine("\t// read parameters from serialized message");
			sb.AppendLine("\tfromClientId = *(char*)(data + 0);");
			sb.AppendLine("\tmemcpy(&msg, data + 1, sizeof(msg));");
			sb.AppendLine();
			sb.AppendLine("\t// invoke recv handlers");
			sb.AppendLine("\tif (isInGame())");
			sb.AppendLine("\t{");
			foreach (var recvHandler in msg.RecvHandlers)
			{
				sb.AppendLine($"\t\t{recvHandler}(fromClientId, &msg);");
			}
			sb.AppendLine("\t}");
			sb.AppendLine();
			sb.AppendLine("\t// return number of bytes read from data");
			sb.AppendLine("\treturn sizeof(msg) + 1;");
			sb.AppendLine("}");
			sb.AppendLine("");

			// broadcast
			sb.AppendLine("//--------------------------------------------------------------------------");
			sb.AppendLine($"void {msg.BroadcastFuncName}(struct {msg.StructTypeName} *msg)");
			sb.AppendLine("{");
			if (msg.HostOnly)
			{
				sb.AppendLine("\t// only host can broadcast");
				sb.AppendLine("\tif (!gameAmIHost()) return;");
				sb.AppendLine();
			}
			sb.AppendLine("\tvoid* connection = netGetDmeServerConnection();");
			sb.AppendLine("\tif (!connection) return;");
			sb.AppendLine();
			sb.AppendLine("\t// construct message");
			sb.AppendLine($"\tchar netmsg[sizeof(struct {msg.StructTypeName}) + 1];");
			sb.AppendLine("\tnetmsg[0] = gameGetMyClientId();");
			sb.AppendLine($"\tmemcpy(netmsg + 1, msg, sizeof(struct {msg.StructTypeName}));");
			sb.AppendLine("");
			sb.AppendLine("\t// broadcast");
			sb.AppendLine($"\tnetBroadcastCustomAppMessage({transportFlags}, connection, {msg.EnumName}, sizeof(netmsg), netmsg);");
			if (msg.RelayToLocalOnBroadcast)
			{
				sb.AppendLine("");
				sb.AppendLine("\t// relay locally");
				sb.AppendLine($"\t{msg.RecvFuncName}(connection, netmsg);");
			}
			sb.AppendLine("}");
			sb.AppendLine("");

			// send
			sb.AppendLine("//--------------------------------------------------------------------------");
			sb.AppendLine($"void {msg.SendFuncName}(int toClientId, struct {msg.StructTypeName} *msg)");
			sb.AppendLine("{");
			if (msg.HostOnly)
			{
				sb.AppendLine("\t// only host can send");
				sb.AppendLine("\tif (!gameAmIHost()) return;");
				sb.AppendLine();
			}
			sb.AppendLine("\tvoid* connection = netGetDmeServerConnection();");
			sb.AppendLine("\tif (!connection) return;");
			sb.AppendLine();
			sb.AppendLine("\t// construct message");
			sb.AppendLine($"\tchar netmsg[sizeof(struct {msg.StructTypeName}) + 1];");
			sb.AppendLine("\tnetmsg[0] = gameGetMyClientId();");
			sb.AppendLine($"\tmemcpy(netmsg + 1, msg, sizeof(struct {msg.StructTypeName}));");
			sb.AppendLine("");
			sb.AppendLine("\t// broadcast");
			sb.AppendLine($"\tnetSendCustomAppMessage({transportFlags}, connection, toClientId, {msg.EnumName}, sizeof(netmsg), &netmsg);");
			sb.AppendLine("}");
			sb.AppendLine("");
		}

		// write cgmNetMsgInit()
		sb.AppendLine("//--------------------------------------------------------------------------");
		sb.AppendLine("void cgmNetMsgInit(void)");
		sb.AppendLine("{");
		sb.AppendLine("\t// hook receipt of custom network messages");
		foreach (var msg in Messages)
		{
			sb.AppendLine($"\tnetInstallCustomMsgHandler({msg.EnumName}, &{msg.RecvFuncName});");
		}
		sb.AppendLine("}");

		// write cgmNetMsgCleanup()
		sb.AppendLine("//--------------------------------------------------------------------------");
		sb.AppendLine("void cgmNetMsgCleanup(void)");
		sb.AppendLine("{");
		sb.AppendLine("\t// unhook receipt of custom network messages");
		foreach (var msg in Messages)
		{
			sb.AppendLine($"\tnetUninstallCustomMsgHandler({msg.EnumName}, &{msg.RecvFuncName});");
		}
		sb.AppendLine("}");

		return sb.ToString();
	}
}

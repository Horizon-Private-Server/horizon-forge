using Newtonsoft.Json;
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.Serialization;
using UnityEditor;
using UnityEngine;

public class PvarOverlay
{
    private static List<PvarOverlay> s_PvarOverlays { get; set; }

    public string Name { get; set; }
    public int RCVersion { get; set; }
    public int OClass { get; set; }
    public int Length { get; set; }
    public string Default { get; set; }
    public string Pointers { get; set; }
    public List<PvarOverlayDef> Overlay { get; set; } = new List<PvarOverlayDef>();

    [JsonIgnore]
    public byte[] DefaultBytes { get; set; }

    [JsonIgnore]
    public int[] PointersInts { get; set; }

    [OnDeserialized]
    internal void OnDeserializedMethod(StreamingContext context)
    {
        DefaultBytes = new byte[Length];
        PointersInts = new int[0];

        var str = Default?.Replace(" ", "");
        if (!String.IsNullOrEmpty(str))
        {
            var bytes = new byte[str.Length];
            var len = 0;

            for (int i = 0; i < bytes.Length && len < Length; i += 2)
                bytes[len++] = Convert.ToByte($"{str[i]}{str[i + 1]}", 16);

            // move into default bytes
            Array.Copy(bytes, 0, DefaultBytes, 0, len);
        }

        var ptrs = Pointers?.Replace(" ", "")?.Split(',', StringSplitOptions.RemoveEmptyEntries);
        var ptrInts = new List<int>();
        if (ptrs != null)
            foreach (var ptr in ptrs)
                if (int.TryParse(ptr, out var ptrInt))
                    ptrInts.Add(ptrInt);

        PointersInts = ptrInts.ToArray();
    }

    [MenuItem("Forge/Utilities/Refresh PVar Overlays")]
    public static void RefreshPvarOverlays()
    {
        GetPvarOverlays(reload: true);
    }

    public static List<PvarOverlay> GetPvarOverlays(bool reload = false)
    {
        if (s_PvarOverlays == null || reload)
        {
            var pvarOverlayFilePath = FolderNames.PvarOverlayFile;
            var json = File.ReadAllText(pvarOverlayFilePath);
            s_PvarOverlays = JsonConvert.DeserializeObject<List<PvarOverlay>>(json);
        }

        return s_PvarOverlays;
    }

    public static PvarOverlay GetPvarOverlay(int oclass, int racVersion)
    {
        var pvarOverlays = GetPvarOverlays();

        return pvarOverlays?.FirstOrDefault(x => x.RCVersion == racVersion && x.OClass == oclass);
    }
}

public class PvarOverlayDef
{
    public string Name { get; set; }
    public string Tooltip { get; set; }
    public string DataType { get; set; }
    public int Offset { get; set; }
    public Dictionary<string, int> Options { get; set; }
    public int? DataSize { get; set; }
    public int? Count { get; set; }
    public float? Min { get; set; }
    public float? Max { get; set; }
}

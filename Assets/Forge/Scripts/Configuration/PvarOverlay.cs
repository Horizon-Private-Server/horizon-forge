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
    public static readonly int VARSTRING_MAX_LENGTH = 0x100;
    public static readonly int VARSTRING_CONTAINER_MAX_COUNT = 16;

    private static List<PvarOverlay> s_PvarOverlays { get; set; }

    public string Name { get; set; }
    public int RCVersion { get; set; }
    public string CustomMode { get; set; }
    public int? MobyOClass { get; set; }
    public int? AmbientSoundType { get; set; }
    public int? CameraType { get; set; }
    public bool ShowRawEditor { get; set; }
    public bool ForceDefaults { get; set; }
    public int Length { get; set; }
    public string Default { get; set; }
    public string Pointers { get; set; }
    public Dictionary<string, long> States { get; set; } = new Dictionary<string, long>();
    public List<PvarOverlayDef> Overlay { get; set; } = new List<PvarOverlayDef>();

    [JsonIgnore]
    public byte[] DefaultBytes { get; set; }

    [JsonIgnore]
    public int[] PointersInts { get; set; }

    public int GetLength(IPVarObject pvarObject)
    {
        int length = Length;
        var pvarData = pvarObject.GetPVarData();
        var pvarValues = pvarObject.GetPVarValues();
        var pvarRefs = pvarObject.GetPVarReferences();
        var strings = pvarObject.GetPVarStrings();
        if (pvarData == null) return length;
        if (Overlay == null) return length;

        // use variable length fields to determine real length from data
        foreach (var def in Overlay)
        {
            switch (def.DataType?.ToLower())
            {
                case "messagecontainer":
                case "varstringcontainer":
                    {
                        // add str length to total length
                        int strCount = strings?.Length ?? 0;
                        if (strCount > VARSTRING_CONTAINER_MAX_COUNT) strCount = VARSTRING_CONTAINER_MAX_COUNT;

                        for (int i = 0; i < strCount; ++i)
                        {
                            int defOff = def.Offset + 4 + (i * 8);
                            if (defOff < pvarData.Length)
                            {
                                int strLen = BinaryHelper.StrToRatchetStr(strings[i])?.Length ?? 0;
                                if (strLen > VARSTRING_MAX_LENGTH) strLen = VARSTRING_MAX_LENGTH;

                                length += strLen + 1;
                            }
                        }

                        length += strCount * 8;
                        if ((length % 4) > 0)
                            length += 4 - (length % 4); // align to 4
                        break;
                    }
                case "mobyrefpvarvalue":
                    {
                        // find respective mobyrefpvar
                        var refPath = $".{def.Ref}";
                        if (pvarValues.ContainsKey(refPath))
                        {
                            var parts = pvarValues[refPath]?.Split('|', StringSplitOptions.RemoveEmptyEntries);
                            var pvarPath = parts?.ElementAtOrDefault(1);
                            int? oClass = int.TryParse(parts?.ElementAtOrDefault(0), out var mobyClass) ? mobyClass : null;
                            var mobyRefPvarOverlay = PvarOverlay.GetPvarOverlay(RCVersion, mobyClass: oClass, customMode: CustomMode);
                            if (mobyRefPvarOverlay != null)
                            {
                                var pvarMetadata = mobyRefPvarOverlay.GetPVarMetadata(pvarPath);
                                if (pvarMetadata != null)
                                {
                                    length += pvarMetadata.Size;
                                }
                            }
                        }
                        break;
                    }
            }
        }

        return length;
    }
    
    public string[] GetPVarPaths(IPVarObject pvarObject)
    {
        var paths = new List<string>();
        foreach (var def in this.Overlay)
        {
            GetPVarPaths(pvarObject, paths, null, def);
        }
        return paths.ToArray();
    }

    private void GetPVarPaths(IPVarObject pvarObject, List<string> paths, string path, PvarOverlayDef def)
    {
        var count = def.Count ?? 1;
        for (int i = 0; i < count; ++i)
        {
            var defPath = path + "." + def.Name;
            if (def.Count.HasValue)
                defPath += $"[{i}]";

            paths.Add(defPath);
            if (def.Fields != null)
            {
                foreach (var childDef in def.Fields)
                {
                    GetPVarPaths(pvarObject, paths, defPath, childDef);
                }
            }
        }
    }

    public string FindPVarAtOffset(int targetOffset)
    {
        return FindPVarAtOffset(this.Overlay, null, 0, targetOffset);
    }

    private string FindPVarAtOffset(List<PvarOverlayDef> defs, string path, int baseOffset, int targetOffset)
    {
        foreach (var def in defs)
        {
            var offset = baseOffset + def.Offset;
            var size = def.GetDataSize();
            var count = def.Count ?? 1;
            var defPath = path + "." + def.Name;

            // target is inside this pvar
            if (targetOffset >= offset && targetOffset < (offset + size*count))
            {
                // get idx
                for (int i = 0; i < count; ++i)
                {
                    var idxOffset = offset + (i * size);
                    if (targetOffset >= idxOffset && targetOffset < (idxOffset + size))
                    {
                        defPath += $"[{i}]";
                        offset = idxOffset;
                        break;
                    }
                }

                if (def.Fields != null && def.Fields.Any())
                    return FindPVarAtOffset(def.Fields, defPath, offset, targetOffset);
                else
                    return defPath;
            }
        }

        return null;
    }

    public object GetPVarValue(string path, SerializableStringDictionary pvarValues, SerializableMonoBehaviourDictionary pvarRefs)
    {
        var parts = path.Split('.', StringSplitOptions.RemoveEmptyEntries);
        var defs = Overlay;
        PvarOverlayDef def = null;
        for (int i = 0; i < parts.Length; ++i)
        {
            var name = parts[i];
            if (name.EndsWith("]") && name.Contains("["))
                name = name.Substring(0, name.IndexOf("["));

            def = defs.FirstOrDefault(x => x.Name == name);
            if (def == null) break;
            defs = def.Fields;
        }

        if (def == null) return null;

        if (def.IsReferenceType()) return pvarRefs[path];
        return def.FromString(pvarValues[path]);
    }

    public PvarOverlayDefMetadata GetPVarMetadata(string path)
    {
        if (path == null) return null;

        var parts = path.Split('.', StringSplitOptions.RemoveEmptyEntries);
        var defs = Overlay;
        var offset = 0;
        PvarOverlayDef def = null;
        for (int i = 0; i < parts.Length; ++i)
        {
            int? idx = null;
            var name = parts[i];
            if (name.EndsWith("]") && name.Contains("["))
            {
                idx = int.TryParse(name.Substring(name.IndexOf("[") + 1, name.IndexOf("]") - name.IndexOf("[") - 1), out var parsedIdx) ? parsedIdx : null;
                name = name.Substring(0, name.IndexOf("["));
            }

            def = defs.FirstOrDefault(x => x.Name == name);
            if (def == null) break;
            defs = def.Fields;

            if (idx.HasValue)
                offset += def.Offset + (idx.Value * def.GetDataSize());
            else
                offset += def.Offset;
        }

        if (def == null) return null;

        return new PvarOverlayDefMetadata()
        {
            Offset = offset,
            Size = def.GetDataSize(),
            Field = def,
        };
    }

    [OnDeserialized]
    internal void OnDeserializedMethod(StreamingContext context)
    {
        // order overlay fields
        if (Overlay != null)
        {
            ComputeOrder(Overlay);
            //CheckForDupes($"rc{this.RCVersion}.{this.Name}", new List<string>(), Overlay);
        }

        // calculate defaults
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

        foreach (var def in this.Overlay)
        {
            if (String.IsNullOrEmpty(str) || def.Default != null || def.IsReferenceType())
            {
                SetDefaultBytes(this, def, DefaultBytes);
            }
        }

        var ptrs = Pointers?.Replace(" ", "")?.Split(',', StringSplitOptions.RemoveEmptyEntries);
        var ptrInts = new List<int>();
        if (ptrs != null)
            foreach (var ptr in ptrs)
                if (int.TryParse(ptr, out var ptrInt))
                    ptrInts.Add(ptrInt);

        PointersInts = ptrInts.ToArray();
    }

    internal void ComputeOrder(List<PvarOverlayDef> defs, PvarOverlayDef parent = null)
    {
        for (int i = 0; i < defs.Count; i++)
        {
            defs[i].ParentDef = parent;
            if (!defs[i].Order.HasValue)
                defs[i].Order = i;

            if (defs[i].Fields != null)
                ComputeOrder(defs[i].Fields, defs[i]);
        }

        defs.Sort((a, b) => a.Order.Value.CompareTo(b.Order.Value));
    }

    internal void CheckForDupes(string path, List<string> paths, List<PvarOverlayDef> defs, PvarOverlayDef parent = null)
    {
        for (int i = 0; i < defs.Count; i++)
        {
            var subPath = path + $".{defs[i].Name}";
            if (paths.Contains(subPath))
            {
                Debug.LogWarning($"FOUND DUPLICATE PVAROVERLAY FIELD AT {subPath}");
            }
            else
            {
                paths.Add(subPath);
            }

            defs[i].ParentDef = parent;
            if (!defs[i].Order.HasValue)
                defs[i].Order = i;

            if (defs[i].Fields != null)
                CheckForDupes(subPath, paths, defs[i].Fields, defs[i]);
        }

        defs.Sort((a, b) => a.Order.Value.CompareTo(b.Order.Value));
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

    public static PvarOverlay GetPvarOverlay(int racVersion, int? mobyClass = null, int? ambientSoundType = null, int? cameraType = null, string? customMode = null)
    {
        var pvarOverlays = GetPvarOverlays();

        if (mobyClass.HasValue)
            return FindPvarOverlay(pvarOverlays, racVersion, mobyClass: mobyClass, customMode: customMode);
        if (ambientSoundType.HasValue)
            return FindPvarOverlay(pvarOverlays, racVersion, ambientSoundType: ambientSoundType, customMode: customMode);
        if (cameraType.HasValue)
            return FindPvarOverlay(pvarOverlays, racVersion, cameraType: cameraType, customMode: customMode);

        return null;
    }

    public static PvarOverlay FindPvarOverlay(List<PvarOverlay> pvarOverlays, int racVersion, int? mobyClass = null, int? ambientSoundType = null, int? cameraType = null, string customMode = null)
    {
        if (mobyClass.HasValue)
            return pvarOverlays?.FirstOrDefault(x => (x.RCVersion == racVersion || x.RCVersion < 0) && x.MobyOClass == mobyClass && x.CustomMode == customMode)
                ?? pvarOverlays?.FirstOrDefault(x => (x.RCVersion == racVersion || x.RCVersion < 0) && x.MobyOClass == mobyClass && x.CustomMode == null);
        if (ambientSoundType.HasValue)
            return pvarOverlays?.FirstOrDefault(x => (x.RCVersion == racVersion || x.RCVersion < 0) && x.AmbientSoundType == ambientSoundType && x.CustomMode == customMode)
                ?? pvarOverlays?.FirstOrDefault(x => (x.RCVersion == racVersion || x.RCVersion < 0) && x.AmbientSoundType == ambientSoundType && x.CustomMode == null);
        if (cameraType.HasValue)
            return pvarOverlays?.FirstOrDefault(x => (x.RCVersion == racVersion || x.RCVersion < 0) && x.CameraType == cameraType && x.CustomMode == customMode)
                ?? pvarOverlays?.FirstOrDefault(x => (x.RCVersion == racVersion || x.RCVersion < 0) && x.CameraType == cameraType && x.CustomMode == null);

        return null;
    }

    private static void SetDefaultBytes(PvarOverlay pvarOverlay, PvarOverlayDef def, byte[] defaultBytes, int defOffsetAdditive = 0)
    {
        var dataSize = def.GetDataSize();
        var offset = def.Offset + defOffsetAdditive;

        switch (def.DataType?.ToLower())
        {
            case "cuboidref":
            case "splineref":
            case "arearef":
            case "mobyref":
            case "pathgraphref":
                {
                    // default values for references is -1

                    var count = def.Count ?? 1;
                    for (int i = 0; i < count; ++i)
                        Array.Copy(BitConverter.GetBytes(-1), 0, defaultBytes, offset + (i * 4), 4);
                    break;
                }
            case "colorrgba":
                {
                    // default enum to first value in list

                    var count = def.Count ?? 1;
                    var defaultValue = long.TryParse(def.Default, out var defVal) ? defVal : (long?)null;
                    var value = defaultValue ?? ((def.Options != null && def.Options.Any()) ? def.Options.FirstOrDefault().Value : 0);
                    var valueBytes = BitConverter.GetBytes(value);
                    for (int i = 0; i < count; ++i)
                        Array.Copy(valueBytes, 0, defaultBytes, offset + (i * dataSize), dataSize);
                    break;
                }
            case "enum":
                {
                    // default enum to first value in list

                    var count = def.Count ?? 1;
                    var defaultValue = long.TryParse(def.Default, out var defVal) ? defVal : (long?)null;
                    var value = defaultValue ?? ((def.Options != null && def.Options.Any()) ? def.Options.FirstOrDefault().Value : 0);
                    var valueBytes = BitConverter.GetBytes(value);
                    for (int i = 0; i < count; ++i)
                        Array.Copy(valueBytes, 0, defaultBytes, offset + (i * dataSize), dataSize);
                    break;
                }
            case "raidsmobid":
                {
                    // default enum to first value in list

                    var count = def.Count ?? 1;
                    var defaultValue = long.TryParse(def.Default, out var defVal) ? defVal : (long?)null;
                    var value = defaultValue ?? 0;
                    var valueBytes = BitConverter.GetBytes(value);
                    for (int i = 0; i < count; ++i)
                        Array.Copy(valueBytes, 0, defaultBytes, offset + (i * dataSize), dataSize);
                    break;
                }
            case "raidsmobbehavior":
                {
                    // default enum to first value in list

                    var count = def.Count ?? 1;
                    var defaultValue = long.TryParse(def.Default, out var defVal) ? defVal : (long?)null;
                    var value = defaultValue ?? 0;
                    var valueBytes = BitConverter.GetBytes(value);
                    for (int i = 0; i < count; ++i)
                        Array.Copy(valueBytes, 0, defaultBytes, offset + (i * dataSize), dataSize);
                    break;
                }
            case "float":
                {
                    // default to 0 clamped to MIN/MAX

                    var count = def.Count ?? 1;
                    var defaultValue = float.TryParse(def.Default, out var defVal) ? defVal : (float?)null;
                    var value = Mathf.Clamp(defaultValue ?? 0f, def.Min ?? float.MinValue, def.Max ?? float.MaxValue);
                    var valueBytes = BitConverter.GetBytes(value);
                    for (int i = 0; i < count; ++i)
                        Array.Copy(valueBytes, 0, defaultBytes, offset + (i * dataSize), dataSize);
                    break;
                }
            case "bool":
                {
                    // default to 0 clamped to MIN/MAX

                    var count = def.Count ?? 1;
                    var defaultValue = bool.TryParse(def.Default, out var defVal) ? defVal : (bool?)null;
                    var valueBytes = new byte[] { (defaultValue??false) ? (byte)1 : (byte)0 };
                    for (int i = 0; i < count; ++i)
                        Array.Copy(valueBytes, 0, defaultBytes, offset + (i * dataSize), dataSize);
                    break;
                }
            case "integer":
                {
                    // default to 0 clamped to MIN/MAX

                    var count = def.Count ?? 1;
                    var defaultValue = int.TryParse(def.Default, out var defVal) ? defVal : (int?)null;
                    var value = (int)Mathf.Clamp(defaultValue ?? 0, def.Min ?? int.MinValue, def.Max ?? int.MaxValue);
                    var valueBytes = BitConverter.GetBytes(value);
                    for (int i = 0; i < count; ++i)
                        Array.Copy(valueBytes, 0, defaultBytes, offset + (i * dataSize), dataSize);
                    break;
                }
            case "mobyrefstate":
                {
                    // default to 0 clamped to MIN/MAX

                    var count = def.Count ?? 1;
                    var defaultValue = int.TryParse(def.Default, out var defVal) ? defVal : (int?)null;
                    var value = (int)Mathf.Clamp(defaultValue ?? 0, def.Min ?? int.MinValue, def.Max ?? int.MaxValue);
                    var valueBytes = BitConverter.GetBytes(value);
                    for (int i = 0; i < count; ++i)
                        Array.Copy(valueBytes, 0, defaultBytes, offset + (i * dataSize), dataSize);
                    break;
                }
            case "mobyrefpvar":
                {
                    // default to 0
                    var count = def.Count ?? 1;
                    for (int i = 0; i < count; ++i)
                    {
                        for (int j = 0; j < dataSize; ++j)
                            defaultBytes[j] = 0;
                    }
                    break;
                }
            case "byte":
            case "sbyte":
                {
                    // default to 0 clamped to MIN/MAX

                    var count = def.Count ?? 1;
                    var defaultValue = int.TryParse(def.Default, out var defVal) ? defVal : (int?)null;
                    var value = (int)Mathf.Clamp(defaultValue ?? 0, def.Min ?? int.MinValue, def.Max ?? int.MaxValue);
                    var valueBytes = BitConverter.GetBytes(value);
                    for (int i = 0; i < count; ++i)
                        Array.Copy(valueBytes, 0, defaultBytes, offset + (i * dataSize), dataSize);
                    break;
                }
            case "struct":
                {
                    // iterate through children and set default

                    if (def.Fields != null)
                    {
                        var count = def.Count ?? 1;
                        for (int i = 0; i < count; ++i)
                        {
                            var elemOffset = offset + (i * dataSize);
                            foreach (var structDef in def.Fields)
                            {
                                SetDefaultBytes(pvarOverlay, structDef, defaultBytes, elemOffset);
                            }
                        }
                    }

                    break;
                }
            default:
                {
                    // try to parse default as a number
                    var count = def.Count ?? 1;
                    var defaultValue = long.TryParse(def.Default, out var defVal) ? defVal : (long?)null;
                    if (defaultValue.HasValue)
                    {
                        var value = (long)Mathf.Clamp(defaultValue ?? 0, def.Min ?? long.MinValue, def.Max ?? long.MaxValue);
                        var valueBytes = BitConverter.GetBytes(value);
                        for (int i = 0; i < count; ++i)
                            Array.Copy(valueBytes, 0, defaultBytes, offset + (i * dataSize), dataSize);
                    }
                    break;
                }
        }
    }
}

public class PvarOverlayDef
{
    public string Name { get; set; }
    public string Tooltip { get; set; }
    public string DataType { get; set; }
    public string Ref { get; set; }
    public int Offset { get; set; }
    public Dictionary<string, long> Options { get; set; }
    public string OptionsLookupKey { get; set; }
    public int? DataSize { get; set; }
    public int? Count { get; set; }
    public float? Min { get; set; }
    public float? Max { get; set; }
    public string Default { get; set; }
    public List<string> Labels { get; set; }
    public List<PvarOverlayDef> Fields { get; set; }
    public List<PvarOverlayDisplayRule> DisplayIf { get; set; }
    public int? Order { get; set; }
    public bool Hidden { get; set; }

    public PvarOverlayDef ParentDef { get; set; }

    private byte[] buffer = new byte[100];

    public int GetDataSize()
    {
        if (DataSize.HasValue) return DataSize.Value;

        switch (DataType?.ToLower())
        {
            case "byte":
            case "sbyte":
            case "bool": return 1;

            case "colorrgb": return 3;

            case "float":
            case "enum":
            case "mask":
            case "mobygroupid":
            case "tiegroupid":
            case "cuboidref":
            case "splineref":
            case "arearef":
            case "mobyref":
            case "pathgraphref":
            case "levelfxtex":
            case "fxtex":
            case "colorrgba":
            case "raidsmobid":
            case "raidsmobbehavior":
            case "screenposition":
            case "mobyrefstate":
            case "integer": return 4;

            case "vector2": return 8;
            case "vector3": return 12;

            case "struct": return Fields?.Max(x => x.Offset + x.GetDataSize()) ?? 0;
                
            case "mobyrefpvar": return 8;
            case "mobyrefpvarvalue": return 0;

            case "varstringcontainer": return 4;
            case "messagecontainer": return 4;

            default: return 4;
        }
    }

    public bool IsDisplayOnly()
    {
        switch (DataType?.ToLower())
        {
            case "header":
            case "space":
            case "label": return true;
            default: return false;
        }
    }

    public bool IsReferenceType()
    {
        switch (DataType?.ToLower())
        {
            case "cuboidref":
            case "arearef":
            case "splineref":
            case "pathgraphref":
            case "mobyref": return true;
            default: return false;
        }
    }

    public object FromBytes(PvarOverlay pvarOverlay, byte[] bytes, int index)
    {
        // don't include the ref types
        // that are stored in CuboidRefs[] etc
        // handle those in UnityHelper.InitializePVarField()

        var dataSize = this.GetDataSize();
        Array.Copy(bytes, index, buffer, 0, dataSize);

        switch (this.DataType?.ToLower())
        {
            case "bool": return buffer[0] != 0;
            case "bliptype":
            case "byte": return buffer[0];
            case "sbyte": return (sbyte)buffer[0];
            case "fxtex":
            case "levelfxtex":
            case "mobygroupid":
            case "tiegroupid":
            case "integer": return BitConverter.ToInt32(buffer);
            case "float": return BitConverter.ToSingle(buffer);
            case "screenposition": return new Vector2(BitConverter.ToInt16(buffer), BitConverter.ToInt16(buffer, 2));
            case "vector2": return new Vector2(BitConverter.ToSingle(buffer), BitConverter.ToSingle(buffer, 4));
            case "vector3": return new Vector3(BitConverter.ToSingle(buffer), BitConverter.ToSingle(buffer, 4), BitConverter.ToSingle(buffer, 8));
            case "colorrgb": return new Color32(buffer[0], buffer[1], buffer[2], 255);
            case "colorrgba": return new Color32(buffer[0], buffer[1], buffer[2], buffer[3]);
            case "raidsmobid":
            case "raidsmobbehavior":
            case "mask":
            case "mobyrefstate":
            case "enum": return BitConverter.ToInt64(buffer);
            case "mobyrefpvar":
                {
                    var offset = BitConverter.ToInt16(buffer, 0);
                    var size = BitConverter.ToInt16(buffer, 2);
                    if (size <= 0) return null; // size is empty so no pvar selected

                    // find path at offset
                    return pvarOverlay.FindPVarAtOffset(offset);
                }
            default: return null;
        }
    }

    public void ToBytes(PvarOverlay pvarOverlay, object value, byte[] bytes, int index, object args = null)
    {
        // don't include the ref types
        // that are stored in CuboidRefs[] etc
        // handle those in UnityHelper.InitializePVarField()

        var dataSize = this.GetDataSize();
        switch (this.DataType?.ToLower())
        {
            case "bool": buffer[0] = (byte)(((bool?)value ?? false) ? 1 : 0); break;
            case "byte": buffer[0] = (byte)((byte?)value ?? 0); break;
            case "sbyte": buffer[0] = (byte)((sbyte?)value ?? 0); break;
            case "mobygroupid":
            case "tiegroupid":
            case "integer": BitConverter.TryWriteBytes(buffer, (int?)value ?? 0); break;
            case "float": BitConverter.TryWriteBytes(buffer, (float?)value ?? 0); break;
            case "screenposition": BitConverter.TryWriteBytes(buffer, (short)((Vector2?)value ?? Vector2.zero).x); BitConverter.TryWriteBytes(buffer.AsSpan(2), (short)((Vector2?)value ?? Vector2.zero).y); break;
            case "vector2": BitConverter.TryWriteBytes(buffer, ((Vector2?)value ?? Vector2.zero).x); BitConverter.TryWriteBytes(buffer.AsSpan(4), ((Vector2?)value ?? Vector2.zero).y); break;
            case "vector3": BitConverter.TryWriteBytes(buffer, ((Vector3?)value ?? Vector3.zero).x); BitConverter.TryWriteBytes(buffer.AsSpan(4), ((Vector3?)value ?? Vector3.zero).y); BitConverter.TryWriteBytes(buffer.AsSpan(8), ((Vector3?)value ?? Vector3.zero).z); break;
            case "colorrgb": buffer[0] = ((Color32)value).r; buffer[1] = ((Color32)value).g; buffer[2] = ((Color32)value).b; break;
            case "colorrgba": buffer[0] = ((Color32)value).r; buffer[1] = ((Color32)value).g; buffer[2] = ((Color32)value).b; buffer[3] = ((Color32)value).a; break;
            case "raidsmobbehavior":
            case "mask":
            case "mobyrefstate":
            case "enum": BitConverter.TryWriteBytes(buffer, (long)value); break;

            case "raidsmobid":
                {
                    // validate mob isn't disabled
                    var mobIdx = (long)value;
                    if (mobIdx >= 0)
                    {
                        var raidsModeData = GameObject.FindObjectOfType<RaidsModeData>();
                        if (raidsModeData != null && raidsModeData?.Mobs != null)
                        {

                            var mob = raidsModeData.Mobs.ElementAtOrDefault((int)mobIdx);
                            if (mob == null || mob.Disabled)
                                mobIdx = -1;
                            else
                                mobIdx = raidsModeData.Mobs.Where(x => !x.Disabled).ToList().IndexOf(mob);
                        }
                        else
                        {
                            mobIdx = -1;
                        }
                    }

                    BitConverter.TryWriteBytes(buffer, mobIdx);
                    break;
                }

            case "mobyrefpvar":
                {
                    var parts = (value as string)?.Split('|', StringSplitOptions.RemoveEmptyEntries);
                    var pvarPath = parts?.ElementAtOrDefault(1);
                    int? oClass = int.TryParse(parts?.ElementAtOrDefault(0), out var mobyClass) ? mobyClass : null;
                    var mobyRefPvarOverlay = PvarOverlay.GetPvarOverlay(pvarOverlay.RCVersion, mobyClass: oClass, customMode: pvarOverlay.CustomMode);
                    if (mobyRefPvarOverlay != null)
                    {
                        var metadata = mobyRefPvarOverlay.GetPVarMetadata(pvarPath);
                        if (metadata != null)
                        {
                            BitConverter.TryWriteBytes(buffer, (short)metadata.Offset);
                            BitConverter.TryWriteBytes(buffer.AsSpan(2), (short)metadata.Size);

                            // todo
                            // add support for child mobyrefs
                            if (metadata.Field.DataType?.ToLower() == "mobyref")
                                buffer[4] = 1;
                        }
                    }
                    break;
                }

            case "mobyref": BitConverter.TryWriteBytes(buffer, Array.IndexOf((args as UnityHelper.PVarMapDataContainer).Mobys, value)); break;
            case "cuboidref": BitConverter.TryWriteBytes(buffer, Array.IndexOf((args as UnityHelper.PVarMapDataContainer).Cuboids, value)); break;
            case "splineref": BitConverter.TryWriteBytes(buffer, Array.IndexOf((args as UnityHelper.PVarMapDataContainer).Splines, value)); break;
            case "arearef": BitConverter.TryWriteBytes(buffer, Array.IndexOf((args as UnityHelper.PVarMapDataContainer).Areas, value)); break;
            case "pathgraphref": BitConverter.TryWriteBytes(buffer, Array.IndexOf((args as UnityHelper.PVarMapDataContainer).PathGraphs, value)); break;
            default: throw new NotImplementedException();
        }

        Array.Copy(buffer, 0, bytes, index, dataSize);
    }

    public string ToString(object value)
    {
        // don't include the ref types
        // that are stored in CuboidRefs[] etc
        // handle those in UnityHelper.InitializePVarField()

        switch (this.DataType?.ToLower())
        {
            case "screenposition": return $"{(int)((Vector2)value).x}|{(int)((Vector2)value).y}";
            case "float": return ((float)value).ToInvariantCulture();
            case "vector2": return $"{((Vector2)value).x.ToInvariantCulture()}|{((Vector2)value).y.ToInvariantCulture()}";
            case "vector3": return $"{((Vector3)value).x.ToInvariantCulture()}|{((Vector3)value).y.ToInvariantCulture()}|{((Vector3)value).z.ToInvariantCulture()}";
            case "colorrgb": return $"{((Color32)value).r},{((Color32)value).g},{((Color32)value).b}";
            case "colorrgba": return $"{((Color32)value).r},{((Color32)value).g},{((Color32)value).b},{((Color32)value).a}";
            default: return value?.ToString();
        }
    }

    public object FromString(string value)
    {
        // don't include the ref types
        // that are stored in CuboidRefs[] etc
        // handle those in UnityHelper.InitializePVarField()

        var v = value ?? Default;
        switch (this.DataType?.ToLower())
        {
            case "bool": return bool.TryParse(v, out var boolValue) ? boolValue : false;
            case "byte": return byte.TryParse(v, out var byteValue) ? byteValue : (byte)0;
            case "sbyte": return sbyte.TryParse(v, out var sbyteValue) ? sbyteValue : (sbyte)0;
            case "mobygroupid":
            case "tiegroupid":
            case "integer": return int.TryParse(v, out var intValue) ? intValue : 0;
            case "float": return StringHelper.FromInvariantCulture(v);
            case "screenposition":
                {
                    try
                    {
                        var parts = v.Split('|');
                        return new Vector2(int.Parse(parts[0]), int.Parse(parts[1]));
                    }
                    catch { }

                    return Vector2.zero;
                }
            case "vector2":
                {
                    try
                    {
                        var parts = v.Split('|');
                        return new Vector2(StringHelper.FromInvariantCulture(parts[0]), StringHelper.FromInvariantCulture(parts[1]));
                    }
                    catch { }

                    return Vector2.zero;
                }
            case "vector3":
                {
                    try
                    {
                        var parts = v.Split('|');
                        return new Vector3(StringHelper.FromInvariantCulture(parts[0]), StringHelper.FromInvariantCulture(parts[1]), StringHelper.FromInvariantCulture(parts[2]));
                    }
                    catch { }

                    return Vector3.zero;
                }
            case "colorrgb":
                {
                    try
                    {
                        var parts = v.Split(',');
                        return new Color32(byte.Parse(parts[0]), byte.Parse(parts[1]), byte.Parse(parts[2]), 255);
                    }
                    catch { }

                    return new Color32(0, 0, 0, 255);
                }
            case "colorrgba":
                {
                    try
                    {
                        var parts = v.Split(',');
                        return new Color32(byte.Parse(parts[0]), byte.Parse(parts[1]), byte.Parse(parts[2]), byte.Parse(parts[3]));
                    }
                    catch { }

                    return new Color32(0, 0, 0, 0);
                }
            case "raidsmobid":
            case "raidsmobbehavior":
            case "mask":
            case "mobyrefstate":
            case "enum": return long.TryParse(v, out var enumValue) ? enumValue : 0;
            case "mobyrefpvar": return value;
            default: return null;
        }
    }

    public Dictionary<string, long> GetOptions()
    {
        if (Options != null) return Options;

        var key = OptionsLookupKey?.ToLower();
        if (key == "challenges")
        {
            var raidsData = GameObject.FindObjectOfType<RaidsModeData>();
            var options = new Dictionary<string, long>();
            if (raidsData)
                options = raidsData.Challenges.ToDictionary(x => string.IsNullOrEmpty(x.Name) ? raidsData.Challenges.IndexOf(x).ToString() : x.Name, x => (long)raidsData.Challenges.IndexOf(x));

            return options;
        }
        else if (key == "raidsmobs")
        {
            var raidsData = GameObject.FindObjectOfType<RaidsModeData>();
            var options = new Dictionary<string, long>();
            var isMask = this.DataType?.ToLower() == "mask";
            var maskCount = this.GetDataSize() * 8;
            if (raidsData)
            {
                if (isMask)
                    options = raidsData.Mobs.Take(maskCount).ToDictionary(x => string.IsNullOrEmpty(x.Name) ? raidsData.Mobs.IndexOf(x).ToString() : x.Name, x => (long)(1 << raidsData.Mobs.IndexOf(x)));
                else
                    options = raidsData.Mobs.ToDictionary(x => string.IsNullOrEmpty(x.Name) ? raidsData.Mobs.IndexOf(x).ToString() : x.Name, x => (long)raidsData.Mobs.IndexOf(x));
            }

            return options;
        }

        return PvarOverlayConstants.PVAR_OPTIONS_LOOKUP.GetValueOrDefault(key) ?? new Dictionary<string, long>();
    }

    public (PvarOverlayDef def, int offset) FindFieldFrom(PvarOverlay pvarOverlay, string fieldName, int offset)
    {
        return FindFieldFrom(pvarOverlay, this, fieldName, offset - this.Offset);
    }

    public static (PvarOverlayDef def, int offset) FindFieldFrom(PvarOverlay pvarOverlay, PvarOverlayDef fromField, string fieldName, int fromFieldParentOffset = 0)
    {
        // we've reached the top of the tree
        // try and find it in the root nodes
        // if can't, it doesn't exist in the ancestry
        if (fromField == null)
        {
            var rootDef = pvarOverlay?.Overlay?.FirstOrDefault(x => x.Name == fieldName);
            return (rootDef, 0);
        }

        // def is field we're search for
        if (fromField.Name == fieldName)
        {
            return (fromField, fromFieldParentOffset);
        }

        // search siblings
        var parentDef = fromField.ParentDef;
        if (parentDef?.Fields != null)
        {
            var siblingDef = parentDef.Fields.FirstOrDefault(x => x.Name == fieldName);
            if (siblingDef != null)
                return (siblingDef, fromFieldParentOffset);
        }

        // recurse upwards
        fromFieldParentOffset -= parentDef?.Offset ?? 0;
        return FindFieldFrom(pvarOverlay, parentDef, fieldName, fromFieldParentOffset);
    }
}

public class PvarOverlayDisplayRule
{
    public string Field { get; set; }
    public string Op { get; set; }
    public string Value { get; set; }
    public string[] Values { get; set; }

    public bool IsMatch(PvarOverlay pvarOverlay, IPVarObject pvarObject, PvarOverlayDef def, string basePath = "")
    {
        //(PvarOverlayDef fieldDef, int fieldDefParentOffset) = PvarOverlayDef.FindFieldFrom(pvarOverlay, def, Field, defOffsetAdditive);

        var pvarValues = pvarObject.GetPVarValues();
        var refPath = $"{basePath}.{Field}";
        var fieldDef = (def.ParentDef?.Fields ?? pvarOverlay.Overlay)?.FirstOrDefault(x => x.Name == Field);



        if (fieldDef != null && pvarValues != null && pvarValues.ContainsKey(refPath))
        {
            var fieldValue = pvarValues[refPath];
            long a, b;

            switch (Op?.ToLower())
            {
                case "==": return fieldValue == Value;
                case "&=": return long.TryParse(fieldValue, out a) && long.TryParse(Value, out b) && (a & b) == b;
                case "&": return long.TryParse(fieldValue, out a) && long.TryParse(Value, out b) && (a & b) != 0;
                case "!=": return fieldValue != Value;
                case "<": return long.TryParse(fieldValue, out a) && long.TryParse(Value, out b) && a < b;
                case ">": return long.TryParse(fieldValue, out a) && long.TryParse(Value, out b) && a > b;
                case "<=": return long.TryParse(fieldValue, out a) && long.TryParse(Value, out b) && a <= b;
                case ">=": return long.TryParse(fieldValue, out a) && long.TryParse(Value, out b) && a >= b;
                case "in": return Values?.Contains(fieldValue) ?? false;
            }
        }

        return false;
    }
}

public class PvarOverlayDefMetadata
{
    public int Offset { get; set; }
    public int Size { get; set; }
    public PvarOverlayDef Field { get; set; }
}

public static class PvarOverlayConstants
{
    public static readonly Dictionary<string, Dictionary<string, long>> PVAR_OPTIONS_LOOKUP = new Dictionary<string, Dictionary<string, long>>()
    {
        { "padmask", FromEnum<DLPadMask>() },
        { "alignment", FromEnum<DLAlignment>() },
        { "raidsdifficulty", FromEnum<DLRaidsDifficulties>() },
        { "raidsdifficultymask", FromEnum<DLRaidsDifficultyMask>() },
        { "musictracks", FromEnum<DLMusicTracks>() },
        { "levelfxtex", FromEnum<DLLevelFXTextureIds>() },
        { "fxtex", FromEnum<DLFXTextureIds>() },
        { "teams", FromEnum<DLTeamIds>() },
        { "bliptypes", FromEnum<DLBlipTypes>() },
        { "numbercomparisons", FromEnum<DLRaidsNumberComparisons>() },
    };

    private static Dictionary<string, long> FromEnum<T>() where T : Enum
    {
        return ((T[])Enum.GetValues(typeof(T))).ToDictionary(x => ObjectNames.NicifyVariableName(x.ToString().Replace("_", " ")), x => Convert.ToInt64(x));
    }

}

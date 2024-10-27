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
    public int? MobyOClass { get; set; }
    public int? AmbientSoundType { get; set; }
    public int? CameraType { get; set; }
    public bool ShowRawEditor { get; set; }
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
        var strings = pvarObject.GetPVarStrings();
        if (pvarData == null) return length;
        if (Overlay == null) return length;

        // use variable length fields to determine real length from data
        foreach (var def in Overlay)
        {
            switch (def.DataType?.ToLower())
            {
                case "varstring":
                    {
                        // add str length to total length
                        if (def.Offset < pvarData.Length)
                        {
                            int strLen = BitConverter.ToInt32(pvarData, def.Offset);
                            if (strLen > VARSTRING_MAX_LENGTH) strLen = VARSTRING_MAX_LENGTH;
                            length += strLen + 1; 
                        }
                        break;
                    }
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
            }
        }

        return length;
    }

    [OnDeserialized]
    internal void OnDeserializedMethod(StreamingContext context)
    {
        // order overlay fields
        if (Overlay != null)
        {
            ComputeOrder(Overlay);
        }

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
            SetDefaultBytes(this, def, DefaultBytes);
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

    public static PvarOverlay GetPvarOverlay(int racVersion, int? mobyClass = null, int? ambientSoundType = null, int? cameraType = null)
    {
        var pvarOverlays = GetPvarOverlays();

        if (mobyClass.HasValue)
            return pvarOverlays?.FirstOrDefault(x => (x.RCVersion == racVersion || x.RCVersion < 0) && x.MobyOClass == mobyClass);
        if (ambientSoundType.HasValue)
            return pvarOverlays?.FirstOrDefault(x => (x.RCVersion == racVersion || x.RCVersion < 0) && x.AmbientSoundType == ambientSoundType);
        if (cameraType.HasValue)
            return pvarOverlays?.FirstOrDefault(x => (x.RCVersion == racVersion || x.RCVersion < 0) && x.CameraType == cameraType);

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
                        Array.Copy(valueBytes, 0, defaultBytes, def.Offset + (i * dataSize), dataSize);
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
                        Array.Copy(valueBytes, 0, defaultBytes, def.Offset + (i * dataSize), dataSize);
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
                        Array.Copy(valueBytes, 0, defaultBytes, def.Offset + (i * dataSize), dataSize);
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
    public int? DataSize { get; set; }
    public int? Count { get; set; }
    public float? Min { get; set; }
    public float? Max { get; set; }
    public string Default { get; set; }
    public List<PvarOverlayDef> Fields { get; set; }
    public List<PvarOverlayDisplayRule> DisplayIf { get; set; }
    public int? Order { get; set; }

    public PvarOverlayDef ParentDef { get; set; }

    public int GetDataSize()
    {
        if (DataSize.HasValue) return DataSize.Value;

        switch (DataType?.ToLower())
        {
            case "byte":
            case "sbyte":
            case "team":
            case "bool": return 1;

            case "padmask": return 2;

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
            case "alignment":
            case "screenposition":
            case "mobyrefstate":
            case "integer": return 4;

            case "vector2": return 8;
            case "vector3": return 12;

            case "struct": return Fields?.Max(x => x.Offset + x.GetDataSize()) ?? 0;

            case "varstring": return 4;
            case "varstringcontainer": return 4;
            case "messagecontainer": return 4;

            default: return 4;
        }
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

    public bool IsMatch(PvarOverlay pvarOverlay, IPVarObject pvarObject, PvarOverlayDef def, int defOffsetAdditive = 0)
    {
        (PvarOverlayDef fieldDef, int fieldDefParentOffset) = PvarOverlayDef.FindFieldFrom(pvarOverlay, def, Field, defOffsetAdditive);
        
        if (fieldDef != null)
        {
            var dataSize = fieldDef.GetDataSize();
            var dataBytes = new byte[8];
            Array.Copy(pvarObject.GetPVarData(), fieldDefParentOffset + fieldDef.Offset, dataBytes, 0, dataSize);
            object dataValue = null;

            switch (fieldDef.DataType?.ToLower())
            {
                default:
                    {
                        dataValue = BitConverter.ToInt64(dataBytes);
                        break;
                    }
            }

            switch (Op?.ToLower())
            {
                case "==": return dataValue?.ToString() == Value;
                case "!=": return dataValue?.ToString() != Value;
                case "in": return Values?.Contains(dataValue?.ToString()) ?? false;
            }
        }

        return false;
    }
}

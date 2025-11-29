using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.AI;

public static class UnityHelper
{
    private static Texture2D _defaultTexture;
    public static Texture2D DefaultTexture => _defaultTexture ? _defaultTexture : (_defaultTexture = CreateDefaultTexture());

    public static void Matrix4x4PropertyField(SerializedProperty property)
    {
        EditorGUI.BeginDisabledGroup(!property.editable);
        property.isExpanded = EditorGUILayout.BeginFoldoutHeaderGroup(property.isExpanded, property.displayName);
        if (property.isExpanded)
        {
            GUILayout.BeginVertical();

            for (int y = 0; y < 4; ++y)
            {
                GUILayout.BeginHorizontal();

                for (int x = 0; x < 4; ++x)
                {
                    var prop = property.FindPropertyRelative("e" + y + x);
                    EditorGUILayout.PropertyField(prop, new GUIContent(""));
                }

                GUILayout.EndHorizontal();
            }

            GUILayout.EndVertical();

            // clear
            if (GUILayout.Button("Reset"))
                SetMatrix4x4PropertyField(property, Matrix4x4.identity);
        }
        EditorGUILayout.EndFoldoutHeaderGroup();
        EditorGUI.EndDisabledGroup();
    }

    public static void SetMatrix4x4PropertyField(SerializedProperty property, Matrix4x4 m)
    {
        for (int y = 0; y < 4; ++y)
        {
            for (int x = 0; x < 4; ++x)
            {
                var prop = property.FindPropertyRelative("e" + y + x);
                prop.floatValue = m[x, y];
            }
        }
    }

    private static Texture2D CreateDefaultTexture()
    {
        var tex = new Texture2D(32, 32, TextureFormat.ARGB32, false);
        for (int y = 0; y < tex.height; ++y)
            for (int x = 0; x < tex.width; ++x)
                tex.SetPixel(x, y, Color.white);

        tex.Apply();
        return tex;
    }

    #region Byte Array Property Field

    enum BYTEARRAY_PROPERTYFIELD_FORMAT
    {
        HEX,
        DEC,
        FLOAT
    }

    const int BYTEARRAY_PROPERTYFIELD_ROW_BYTE_COUNT = 0x10;
    static int ByteArrayPropertyField_GroupSizeSelected = 0;
    static int ByteArrayPropertyField_GroupSize = 1;
    static int ByteArrayPropertyField_FieldWidth = 30;
    static BYTEARRAY_PROPERTYFIELD_FORMAT ByteArrayPropertyField_Format = BYTEARRAY_PROPERTYFIELD_FORMAT.HEX;
    static byte[] ByteArrayPropertyField_Buffer = new byte[BYTEARRAY_PROPERTYFIELD_ROW_BYTE_COUNT];

    public static void ByteArrayPropertyField(SerializedProperty property, bool alwaysExpanded = false, bool showEditLength = false)
    {
        EditorGUI.BeginDisabledGroup(!property.editable);
        if (!alwaysExpanded) property.isExpanded = alwaysExpanded || EditorGUILayout.BeginFoldoutHeaderGroup(property.isExpanded, property.displayName);
        if (alwaysExpanded || property.isExpanded)
        {
            GUILayout.BeginVertical();

            if (showEditLength)
            {
                GUILayout.BeginHorizontal();

                int size = property.arraySize;
                size = EditorGUILayout.IntField(new GUIContent("Byte Count", ""), size);
                if (size != property.arraySize)
                    property.arraySize = size;

                GUILayout.EndHorizontal();
            }

            // draw grouping / format options
            GUILayout.BeginHorizontal();
            ByteArrayPropertyField_GroupSizeSelected = GUILayout.SelectionGrid(ByteArrayPropertyField_GroupSizeSelected, new string[] { "1", "2", "4", "8" }, 4, GUILayout.Width(100));
            GUILayout.Space(20);
            ByteArrayPropertyField_Format = (BYTEARRAY_PROPERTYFIELD_FORMAT)GUILayout.SelectionGrid((int)ByteArrayPropertyField_Format, new string[] { "H", "D", "F" }, 3, GUILayout.Width(100));
            GUILayout.EndHorizontal();

            // validate groupings
            ByteArrayPropertyField_GroupSize = (int)Mathf.Pow(2, ByteArrayPropertyField_GroupSizeSelected);
            if (ByteArrayPropertyField_Format == BYTEARRAY_PROPERTYFIELD_FORMAT.FLOAT)
                ByteArrayPropertyField_GroupSize = 4;

            // draw data fields
            var byteCount = property.arraySize;
            var rowHeaderDigitCount = (int)Mathf.Log(byteCount, 16) + 1;
            var rows = Mathf.CeilToInt(byteCount / (float)BYTEARRAY_PROPERTYFIELD_ROW_BYTE_COUNT);
            for (int y = -1; y < rows; ++y)
            {
                GUILayout.BeginHorizontal();

                if (y < 0)
                {
                    // column header
                    GUILayout.Label("  ", GUILayout.Width(ByteArrayPropertyField_FieldWidth));
                    for (int x = 0; x < BYTEARRAY_PROPERTYFIELD_ROW_BYTE_COUNT; x += ByteArrayPropertyField_GroupSize)
                    {
                        GUILayout.Label($"{x:X2}", GUILayout.MinWidth(ByteArrayPropertyField_FieldWidth));
                    }
                }
                else
                {
                    var idx = (y * BYTEARRAY_PROPERTYFIELD_ROW_BYTE_COUNT);

                    // row header
                    GUILayout.Label(idx.ToString($"X{rowHeaderDigitCount}"), GUILayout.Width(ByteArrayPropertyField_FieldWidth));
                    for (int x = 0; x < BYTEARRAY_PROPERTYFIELD_ROW_BYTE_COUNT && (idx + x) < byteCount; x += ByteArrayPropertyField_GroupSize)
                    {
                        ByteArrayPropertyField_DrawValue(property, idx + x);
                    }
                }

                GUILayout.EndHorizontal();
            }

            GUILayout.EndVertical();
        }
        EditorGUILayout.EndFoldoutHeaderGroup();
        EditorGUI.EndDisabledGroup();
    }

    private static void ByteArrayPropertyField_DrawValue(SerializedProperty property, int offset)
    {
        // read bytes
        for (int i = 0; i < ByteArrayPropertyField_GroupSize; ++i)
            ByteArrayPropertyField_Buffer[i] = (byte)property.GetArrayElementAtIndex(offset + i).intValue;

        // parse format
        string value = null;
        switch (ByteArrayPropertyField_Format)
        {
            case BYTEARRAY_PROPERTYFIELD_FORMAT.HEX:
                {
                    value = BitConverter.ToInt64(ByteArrayPropertyField_Buffer, 0).ToString($"X{ByteArrayPropertyField_GroupSize * 2}");
                    break;
                }
            case BYTEARRAY_PROPERTYFIELD_FORMAT.DEC:
                {
                    value = BitConverter.ToInt64(ByteArrayPropertyField_Buffer, 0).ToString();
                    break;
                }
            case BYTEARRAY_PROPERTYFIELD_FORMAT.FLOAT:
                {
                    value = BitConverter.ToSingle(ByteArrayPropertyField_Buffer, 0).ToString("0.#######");
                    break;
                }
        }

        // render
        var newValue = EditorGUILayout.TextField(value, GUILayout.MinWidth(ByteArrayPropertyField_FieldWidth));
        if (newValue == value)
            return;

        // convert value back to byte
        try
        {
            switch (ByteArrayPropertyField_Format)
            {
                case BYTEARRAY_PROPERTYFIELD_FORMAT.HEX:
                    {
                        var bytes = BitConverter.GetBytes(long.Parse(newValue, System.Globalization.NumberStyles.HexNumber));
                        Array.Copy(bytes, 0, ByteArrayPropertyField_Buffer, 0, bytes.Length);
                        break;
                    }
                case BYTEARRAY_PROPERTYFIELD_FORMAT.DEC:
                    {
                        var bytes = BitConverter.GetBytes(long.Parse(newValue));
                        Array.Copy(bytes, 0, ByteArrayPropertyField_Buffer, 0, bytes.Length);
                        break;
                    }
                case BYTEARRAY_PROPERTYFIELD_FORMAT.FLOAT:
                    {
                        var bytes = BitConverter.GetBytes(float.Parse(newValue));
                        Array.Copy(bytes, 0, ByteArrayPropertyField_Buffer, 0, bytes.Length);
                        break;
                    }
            }
        }
        catch
        {
            // failed to parse
            // stop
            return;
        }

        // write bytes
        for (int i = 0; i < ByteArrayPropertyField_GroupSize; ++i)
            property.GetArrayElementAtIndex(offset + i).intValue = ByteArrayPropertyField_Buffer[i];
    }

    #endregion

    #region PVars Property Field

    private static byte[] PVarsPropertyField_Buffer = new byte[0x100];

    public class PVarsPropertiesContainer
    {
        public SerializedProperty PVars { get; set; }
        public SerializedProperty PVarValues { get; set; }
        public SerializedProperty PVarRefs { get; set; }
        public SerializedProperty Strings { get; set; }
    }

    public class PVarMapDataContainer
    {
        public Moby[] Mobys { get; set; }
        public Cuboid[] Cuboids { get; set; }
        public Spline[] Splines { get; set; }
        public Area[] Areas { get; set; }
        public PathGraph[] PathGraphs { get; set; }
    }

    public static void PVarsPropertyField(PVarsPropertiesContainer properties, IPVarObject pvarObject, int racVersion, int? mobyClass = null, int? ambientSoundType = null, int? cameraType = null, bool alwaysExpanded = false, bool showRawEditorIfNoOverlay = true)
    {
        // pvar overlay
        var pvarOverlay = PvarOverlay.GetPvarOverlay(racVersion, mobyClass: mobyClass, ambientSoundType: ambientSoundType, cameraType: cameraType);
        if (pvarOverlay != null && pvarOverlay.Overlay.Any())
        {
            EditorGUI.BeginDisabledGroup(!properties.PVars.editable);
            if (!alwaysExpanded) properties.PVars.isExpanded = EditorGUILayout.BeginFoldoutHeaderGroup(properties.PVars.isExpanded, properties.PVars.displayName);
            if (alwaysExpanded || properties.PVars.isExpanded)
            {
                GUILayout.BeginVertical();

                try
                {
                    foreach (var def in pvarOverlay.Overlay)
                    {
                        PVarsPropertyField_OverlayField(pvarOverlay, properties, pvarObject, "", def);
                    }
                }
                catch (Exception ex) { Debug.LogError(ex); }

                GUILayout.EndVertical();

                // show byte editor
                if (pvarOverlay.ShowRawEditor)
                {
                    ByteArrayPropertyField(properties.PVars, alwaysExpanded: true);
                }
            }
            EditorGUILayout.EndFoldoutHeaderGroup();
            EditorGUI.EndDisabledGroup();
        }
        else if (showRawEditorIfNoOverlay)
        {
            ByteArrayPropertyField(properties.PVars, alwaysExpanded: alwaysExpanded, showEditLength: true);
        }
    }

    public static void ValidatePVars(MapConfig mapConfig, IPVarObject pvarObject)
    {
        if (!mapConfig) return;

        var pvarData = pvarObject.GetPVarData();
        var pvarOverlay = pvarObject.GetPVarOverlay();
        var pvarValues = pvarObject.GetPVarValues();
        var pvarRefs = pvarObject.GetPVarReferences();

        // validate pvars
        var expectedPvarSize = pvarOverlay?.GetLength(pvarObject) ?? 0;
        if (pvarOverlay != null && (pvarData == null || pvarData.Length != expectedPvarSize))
        {
            // merge update
            if (pvarData != null)
            {
                var lastLength = pvarData.Length;
                Array.Resize(ref pvarData, expectedPvarSize);

                // copy default bytes into newly added pvar data
                if (expectedPvarSize > lastLength && expectedPvarSize <= pvarOverlay.DefaultBytes.Length)
                    Array.Copy(pvarOverlay.DefaultBytes, lastLength, pvarData, lastLength, expectedPvarSize - lastLength);

                pvarObject.SetPVarData(pvarData);

                // find missing fields, set to default
                var paths = pvarOverlay.GetPVarPaths(pvarObject);
                if (paths != null)
                {
                    foreach (var path in paths)
                    {
                        if (!pvarValues.ContainsPath(path) && !pvarRefs.ContainsPath(path))
                        {
                            var def = pvarOverlay.GetPVarMetadata(path);
                            Array.Copy(pvarOverlay.DefaultBytes, def.Offset, pvarData, def.Offset, def.Size);
                            InitializePVarField(mapConfig, pvarOverlay, pvarObject, path, def.Field, def.Offset - def.Field.Offset);
                        }
                    }
                }
            }

            InitializePVars(mapConfig, pvarObject, useDefault: pvarData == null);
        }
    }

    public static void InitializePVars(MapConfig mapConfig, IPVarObject pvarObject, bool useDefault = false)
    {
        if (!mapConfig) return;

        var pvarOverlay = pvarObject.GetPVarOverlay();
        if (pvarOverlay != null)
        {
            try
            {
                if (useDefault)
                {
                    var pvars = new byte[pvarOverlay.Length];
                    Array.Copy(pvarOverlay.DefaultBytes, 0, pvars, 0, Math.Min(pvars.Length, pvarOverlay.DefaultBytes.Length));
                    pvarObject.SetPVarData(pvars);
                }

                foreach (var def in pvarOverlay.Overlay)
                {
                    InitializePVarField(mapConfig, pvarOverlay, pvarObject, "", def);
                }
            }
            catch (Exception ex)
            {
                Debug.LogError(ex);
            }
        }
    }

    public static void UpdatePVars(MapConfig mapConfig, IPVarObject pvarObject, int racVersion)
    {
        if (!mapConfig) return;

        var pvars = pvarObject.GetPVarData();
        var pvarValues = pvarObject.GetPVarValues();
        var pvarRefs = pvarObject.GetPVarReferences();
        var mapData = new PVarMapDataContainer()
        {
            Mobys = mapConfig.GetMobys(racVersion),
            Cuboids = mapConfig.GetCuboids(),
            Areas = mapConfig.GetAreas(),
            Splines = mapConfig.GetSplines(),
            PathGraphs = mapConfig.GetPathGraphs()
        };

        //var pvarsDupe = new byte[pvars.Length];
        //Array.Copy(pvars, 0, pvarsDupe, 0, pvars.Length);

        // update reference types to index
        var pvarOverlay = pvarObject.GetPVarOverlay();
        if (pvarOverlay != null && pvarOverlay.Overlay.Any())
        {
            try
            {
                foreach (var def in pvarOverlay.Overlay)
                {
                    UpdatePVar(pvarOverlay, pvarObject, pvars, pvarValues, pvarRefs, mapData, "", def);
                }
            }
            catch (Exception ex) { Debug.LogError(ex); }
        }

        //for (int i = 0; i < pvars.Length; ++i)
        //{
        //    if (pvars[i] != pvarsDupe[i])
        //    {
        //        Debug.Log($"{(pvarObject as MonoBehaviour).gameObject.name}: 0x{i:X4} {pvars[i]:X2}=>{pvarsDupe[i]:X2}", pvarObject as MonoBehaviour);
        //    }
        //}
    }

    private static void UpdatePVar(PvarOverlay pvarOverlay, IPVarObject pvarObject, byte[] pvars, SerializableStringDictionary pvarValues, SerializableMonoBehaviourDictionary pvarRefs, PVarMapDataContainer mapData, string basePath, PvarOverlayDef def, int defOffsetAdditive = 0)
    {
        var count = def.Count ?? 1;
        var offset = def.Offset + defOffsetAdditive;
        var dataSize = def.GetDataSize();
        var path = basePath + $".{def.Name}";
        var normalizedDataType = def.DataType?.ToLower();

        var display = def.DisplayIf == null || def.DisplayIf.All(x => x.IsMatch(pvarOverlay, pvarObject, def, basePath));
        if (!display) return;

        switch (normalizedDataType)
        {
            case "messagecontainer":
            case "varstringcontainer":
                {
                    string strValue = pvarValues[path];
                    var messageData = strValue?.Split('|');
                    var strings = pvarObject.GetPVarStrings().Select(x => BinaryHelper.StrToRatchetStr(x)).ToArray();
                    var totalSize = 4 + (strings.Length * 8);
                    foreach (var str in strings)
                        totalSize += str.Length + 1;

                    // write strings
                    Array.Copy(BitConverter.GetBytes(strings.Length), 0, pvars, offset, 4);
                    var defOffset = offset + 4;
                    var strOffset = offset + 4 + (strings.Length * 8);
                    for (int i = 0; i < strings.Length; ++i)
                    {
                        var moveTo = 0;
                        var runtime = 0;
                        if (messageData != null && messageData[i] != null)
                        {
                            var parts = messageData[i].Split(',');
                            int.TryParse(parts?.ElementAtOrDefault(0), out runtime);
                            int.TryParse(parts?.ElementAtOrDefault(1), out moveTo);
                        }

                        var str = strings[i] + "\0";
                        Array.Copy(BitConverter.GetBytes((short)strings[i].Length), 0, pvars, defOffset, 2);
                        pvars[defOffset + 2] = (byte)runtime;
                        pvars[defOffset + 3] = (byte)moveTo;
                        Array.Copy(BitConverter.GetBytes(strOffset), 0, pvars, defOffset + 4, 4);
                        Array.Copy(Encoding.ASCII.GetBytes(str), 0, pvars, strOffset, str.Length);

                        strOffset += str.Length;
                        defOffset += 8;
                    }
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
                        var mobyRefPvarOverlay = PvarOverlay.GetPvarOverlay(pvarOverlay.RCVersion, mobyClass: oClass);
                        if (mobyRefPvarOverlay != null)
                        {
                            var pvarMetadata = mobyRefPvarOverlay.GetPVarMetadata(pvarPath);
                            if (pvarMetadata != null)
                            {
                                var defOffset = pvarMetadata.Field.Offset;
                                var defCount = pvarMetadata.Field.Count;
                                try
                                {
                                    pvarMetadata.Field.Offset = 0;
                                    pvarMetadata.Field.Count = null;
                                    UpdatePVar(pvarOverlay, pvarObject, pvars, pvarValues, pvarRefs, mapData, path, pvarMetadata.Field, offset);
                                }
                                finally
                                {
                                    pvarMetadata.Field.Offset = defOffset;
                                    pvarMetadata.Field.Count = defCount;
                                }
                            }
                        }
                    }
                    break;
                }
            case "struct":
                {
                    if (def.Fields != null)
                    {
                        for (int i = 0; i < count; ++i)
                        {
                            var iPath = path;
                            if (count > 1) iPath += $"[{i}]";

                            foreach (var structDef in def.Fields)
                            {
                                UpdatePVar(pvarOverlay, pvarObject, pvars, pvarValues, pvarRefs, mapData, iPath, structDef, offset + (i * dataSize));
                            }
                        }
                    }
                    break;
                }
            default: // normal datatypes
                {
                    for (int i = 0; i < count; ++i)
                    {
                        if (def.IsDisplayOnly()) continue;

                        var iPath = path;
                        if (count > 1) iPath += $"[{i}]";
                        var iOffset = offset + (dataSize * i);

                        if (def.IsReferenceType())
                        {
                            var refValue = pvarRefs[iPath];
                            def.ToBytes(pvarOverlay, refValue, pvars, iOffset, mapData);
                        }
                        else
                        {
                            var strValue = pvarValues[iPath];
                            var value = def.FromString(strValue);
                            def.ToBytes(pvarOverlay, value, pvars, iOffset);
                        }
                    }
                    break;
                }
        }
    }

    private static void InitializePVarField(MapConfig mapConfig, PvarOverlay pvarOverlay, IPVarObject pvarObject, string basePath, PvarOverlayDef def, int defOffsetAdditive = 0)
    {
        var pvars = pvarObject.GetPVarData();
        var pvarValues = pvarObject.GetPVarValues();
        var pvarRefs = pvarObject.GetPVarReferences();
        var strings = pvarObject.GetPVarStrings();

        var count = def.Count ?? 1;
        var dataSize = def.GetDataSize();
        var offset = def.Offset + defOffsetAdditive;
        var path = basePath + $".{def.Name}";

        var display = def.DisplayIf == null || def.DisplayIf.All(x => x.IsMatch(pvarOverlay, pvarObject, def, basePath));
        if (!display) return;

        switch (def.DataType?.ToLower())
        {
            case "messagecontainer":
            case "varstringcontainer":
                {
                    if (pvarObject.GetPVarStrings() == null)
                    {
                        // initialize first value if array doesn't fit
                        // this should only occur on newly imported pathgraphs
                        var strCount = BitConverter.ToInt32(pvars, offset);
                        if (strCount > PvarOverlay.VARSTRING_CONTAINER_MAX_COUNT) strCount = PvarOverlay.VARSTRING_CONTAINER_MAX_COUNT;
                        if (strings == null || strCount > strings.Length)
                        {
                            int lastLen = strings?.Length ?? 0;

                            // increase array size
                            if (strings == null)
                                strings = new string[strCount];
                            else
                                Array.Resize(ref strings, strCount);

                            // find init values
                            for (int i = lastLen; i < strCount; ++i)
                            {
                                strings[i] = null;
                                var strLen = BitConverter.ToInt16(pvars, offset + 4 + (i * 8));
                                var strOff = BitConverter.ToInt32(pvars, offset + 4 + (i * 8) + 4);
                                if (strLen > 0 && strOff > 0)
                                    strings[i] = BinaryHelper.RatchetStrToStr(Encoding.ASCII.GetString(pvars, strOff, strLen));
                            }
                        }

                        pvarObject.SetPVarStrings(strings);
                    }
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
                        var mobyRefPvarOverlay = PvarOverlay.GetPvarOverlay(pvarOverlay.RCVersion, mobyClass: oClass);
                        if (mobyRefPvarOverlay != null)
                        {
                            var pvarMetadata = mobyRefPvarOverlay.GetPVarMetadata(pvarPath);
                            if (pvarMetadata != null)
                            {
                                var defOffset = pvarMetadata.Field.Offset;
                                var defCount = pvarMetadata.Field.Count;
                                try
                                {
                                    pvarMetadata.Field.Offset = 0;
                                    pvarMetadata.Field.Count = null;
                                    InitializePVarField(mapConfig, pvarOverlay, pvarObject, path, pvarMetadata.Field, offset);
                                }
                                finally
                                {
                                    pvarMetadata.Field.Offset = defOffset;
                                    pvarMetadata.Field.Count = defCount;
                                }
                            }
                        }
                    }
                    break;
                }
            case "struct":
                {
                    if (def.Fields != null)
                    {
                        for (int i = 0; i < count; ++i)
                        {
                            var iPath = path;
                            if (count > 1) iPath += $"[{i}]";

                            foreach (var structDef in def.Fields)
                            {
                                InitializePVarField(mapConfig, pvarOverlay, pvarObject, iPath, structDef, offset + (i * dataSize));
                            }
                        }
                    }
                    break;
                }
            default: // normal datatypes
                {
                    for (int i = 0; i < count; ++i)
                    {
                        var iPath = path;
                        if (count > 1) iPath += $"[{i}]";
                        var iOffset = offset + (dataSize * i);

                        if (pvarOverlay.ForceDefaults)
                            Array.Copy(pvarOverlay.DefaultBytes, iOffset, pvars, iOffset, def.GetDataSize());

                        if (def.IsReferenceType() && pvarRefs != null && !pvarRefs.ContainsKey(iPath))
                        {
                            MonoBehaviour refValue = null;
                            switch (def.DataType?.ToLower())
                            {
                                case "mobyref": refValue = mapConfig.GetMobyAtIndex(pvarObject.GetRCVersion(), BitConverter.ToInt32(pvars, iOffset)); break;
                                case "cuboidref": refValue = mapConfig.GetCuboidAtIndex(BitConverter.ToInt32(pvars, iOffset)); break;
                                case "splineref": refValue = mapConfig.GetSplineAtIndex(BitConverter.ToInt32(pvars, iOffset)); break;
                                case "arearef": refValue = mapConfig.GetAreaAtIndex(BitConverter.ToInt32(pvars, iOffset)); break;
                                case "pathgraphref": refValue = mapConfig.GetPathGraphAtIndex(BitConverter.ToInt32(pvars, iOffset)); break;
                                default: throw new NotImplementedException();
                            }

                            pvarRefs[iPath] = refValue;
                            UnityEditor.EditorUtility.SetDirty(pvarObject as MonoBehaviour);
                        }
                        else if (!def.IsReferenceType() && pvarValues != null && !pvarValues.ContainsKey(iPath))
                        {
                            pvarValues[iPath] = def.ToString(def.FromBytes(pvarOverlay, pvars, iOffset) ?? def.FromString(null));
                            UnityEditor.EditorUtility.SetDirty(pvarObject as MonoBehaviour);
                        }
                    }
                    break;
                }
        }
    }

    private static RaidsModeData _raidsModeData = null;
    private static RaidsMobsScriptableObject _raidsMobConfig = null;
    private static void PVarsPropertyField_OverlayField(PvarOverlay pvarOverlay, PVarsPropertiesContainer properties, IPVarObject pvarObject, string basePath, PvarOverlayDef def, int defOffsetAdditive = 0)
    {
        var count = def.Count ?? 1;
        var dataSize = def.GetDataSize();
        var baseOffset = def.Offset + defOffsetAdditive;
        var pvarValues = pvarObject.GetPVarValues();
        var pvarRefs = pvarObject.GetPVarReferences();
        var pvarData = pvarObject.GetPVarData();
        var display = !def.Hidden && (def.DisplayIf == null || def.DisplayIf.All(x => x.IsMatch(pvarOverlay, pvarObject, def, basePath)));
        var path = basePath + $".{def.Name}";
        if (!display) return;

        switch (def.DataType?.ToLower())
        {
            case "bool":
                {
                    Draw(properties, pvarObject, basePath, def, baseOffset, (offset, name, path2) =>
                    {
                        // read value
                        string strValue = pvarValues[path2];
                        bool value = (bool)def.FromString(strValue);
                        if (strValue == null) value = (bool?)def.FromBytes(pvarOverlay, pvarData, offset) ?? value;

                        EditorGUI.BeginChangeCheck();
                        value = EditorGUILayout.Toggle(new GUIContent(name, def.Tooltip), value);
                        if (EditorGUI.EndChangeCheck())
                        {
                            pvarValues.SetPropertyKeyValue(properties.PVarValues, path2, def.ToString(value));
                        }
                    });
                    break;
                }
            case "byte":
                {
                    Draw(properties, pvarObject, basePath, def, baseOffset, (offset, name, path2) =>
                    {
                        // read value
                        string strValue = pvarValues[path2];
                        int value = (byte)def.FromString(strValue);
                        if (strValue == null) value = (byte?)def.FromBytes(pvarOverlay, pvarData, offset) ?? value;

                        EditorGUI.BeginChangeCheck();
                        value = EditorGUILayout.IntField(new GUIContent(name, def.Tooltip), value);
                        if (EditorGUI.EndChangeCheck())
                        {
                            if (value < def.Min) value = (int)def.Min;
                            if (value > def.Max) value = (int)def.Max;
                            if (value > byte.MaxValue) value = byte.MaxValue;
                            if (value < byte.MinValue) value = byte.MinValue;
                            pvarValues.SetPropertyKeyValue(properties.PVarValues, path2, def.ToString(value));
                        }
                    });
                    break;
                }
            case "sbyte":
                {
                    Draw(properties, pvarObject, basePath, def, baseOffset, (offset, name, path2) =>
                    {
                        // read value
                        string strValue = pvarValues[path2];
                        int value = (sbyte)def.FromString(strValue);
                        if (strValue == null) value = (sbyte?)def.FromBytes(pvarOverlay, pvarData, offset) ?? value;

                        EditorGUI.BeginChangeCheck();
                        value = EditorGUILayout.IntField(new GUIContent(name, def.Tooltip), value);
                        if (EditorGUI.EndChangeCheck())
                        {
                            if (value < def.Min) value = (int)def.Min;
                            if (value > def.Max) value = (int)def.Max;
                            if (value > sbyte.MaxValue) value = sbyte.MaxValue;
                            if (value < sbyte.MinValue) value = sbyte.MinValue;
                            pvarValues.SetPropertyKeyValue(properties.PVarValues, path2, def.ToString(value));
                        }
                    });
                    break;
                }
            case "integer":
                {
                    Draw(properties, pvarObject, basePath, def, baseOffset, (offset, name, path2) =>
                    {
                        // read value
                        string strValue = pvarValues[path2];
                        int value = (int)def.FromString(strValue);
                        if (strValue == null) value = (int?)def.FromBytes(pvarOverlay, pvarData, offset) ?? value;

                        EditorGUI.BeginChangeCheck();
                        value = EditorGUILayout.IntField(new GUIContent(name, def.Tooltip), value);
                        if (EditorGUI.EndChangeCheck())
                        {
                            if (value < def.Min) value = (int)def.Min;
                            if (value > def.Max) value = (int)def.Max;
                            pvarValues.SetPropertyKeyValue(properties.PVarValues, path2, def.ToString(value));
                        }
                    });
                    break;
                }
            case "float":
                {
                    Draw(properties, pvarObject, basePath, def, baseOffset, (offset, name, path2) =>
                    {
                        // read value
                        string strValue = pvarValues[path2];
                        float value = (float)def.FromString(strValue);
                        if (strValue == null) value = (float?)def.FromBytes(pvarOverlay, pvarData, offset) ?? value;

                        EditorGUI.BeginChangeCheck();
                        if (def.Min.HasValue && def.Max.HasValue)
                        {
                            value = EditorGUILayout.Slider(new GUIContent(name, def.Tooltip), value, def.Min.Value, def.Max.Value);
                        }
                        else
                        {
                            value = EditorGUILayout.FloatField(new GUIContent(name, def.Tooltip), value);
                        }
                        if (EditorGUI.EndChangeCheck())
                        {
                            if (value < def.Min) value = (float)def.Min;
                            if (value > def.Max) value = (float)def.Max;
                            pvarValues.SetPropertyKeyValue(properties.PVarValues, path2, def.ToString(value));
                        }
                    });
                    break;
                }
            case "vector2":
                {
                    // read value
                    string strValue = pvarValues[path];
                    var value = (Vector2)def.FromString(strValue);
                    if (strValue == null) value = (Vector2?)def.FromBytes(pvarOverlay, pvarData, baseOffset) ?? value;

                    EditorGUI.BeginChangeCheck();
                    value = EditorGUILayout.Vector2Field(new GUIContent(def.Name, def.Tooltip), value);
                    if (EditorGUI.EndChangeCheck())
                    {
                        //if (value < def.Min) value = (float)def.Min;
                        //if (value > def.Max) value = (float)def.Max;
                        pvarValues.SetPropertyKeyValue(properties.PVarValues, path, def.ToString(value));
                    }
                    break;
                }
            case "vector3":
                {
                    // read value
                    string strValue = pvarValues[path];
                    var value = (Vector3)def.FromString(strValue);
                    if (strValue == null) value = (Vector3?)def.FromBytes(pvarOverlay, pvarData, baseOffset) ?? value;

                    EditorGUI.BeginChangeCheck();
                    value = EditorGUILayout.Vector3Field(new GUIContent(def.Name, def.Tooltip), value);
                    if (EditorGUI.EndChangeCheck())
                    {
                        //if (value < def.Min) value = (float)def.Min;
                        //if (value > def.Max) value = (float)def.Max;
                        pvarValues.SetPropertyKeyValue(properties.PVarValues, path, def.ToString(value));
                    }
                    break;
                }
            case "screenposition":
                {
                    // read value
                    string strValue = pvarValues[path];
                    var value = (Vector2)def.FromString(strValue);
                    if (strValue == null) value = (Vector2?)def.FromBytes(pvarOverlay, pvarData, baseOffset) ?? value;
                    
                    EditorGUI.BeginChangeCheck();
                    value = EditorGUILayout.Vector2Field(new GUIContent(def.Name, def.Tooltip), value);
                    if (EditorGUI.EndChangeCheck())
                    {
                        value.x = (short)Mathf.Clamp(value.x, 0, 512);
                        value.y = (short)Mathf.Clamp(value.y, 0, 416);
                        pvarValues.SetPropertyKeyValue(properties.PVarValues, path, def.ToString(value));
                    }
                    break;
                }
            case "colorrgb":
                {
                    // read value
                    string strValue = pvarValues[path];
                    var value = (Color32)def.FromString(strValue);
                    if (strValue == null) value = (Color32?)def.FromBytes(pvarOverlay, pvarData, baseOffset) ?? value;

                    EditorGUI.BeginChangeCheck();
                    value = EditorGUILayout.ColorField(new GUIContent(def.Name, def.Tooltip), value, showEyedropper: true, showAlpha: false, hdr: false);
                    if (EditorGUI.EndChangeCheck())
                    {
                        pvarValues.SetPropertyKeyValue(properties.PVarValues, path, def.ToString(value));
                    }
                    break;
                }
            case "colorrgba":
                {
                    // read value
                    string strValue = pvarValues[path];
                    var value = (Color32)def.FromString(strValue);
                    if (strValue == null) value = (Color32?)def.FromBytes(pvarOverlay, pvarData, baseOffset) ?? value;
                    
                    EditorGUI.BeginChangeCheck();
                    value = EditorGUILayout.ColorField(new GUIContent(def.Name, def.Tooltip), value);
                    if (EditorGUI.EndChangeCheck())
                    {
                        pvarValues.SetPropertyKeyValue(properties.PVarValues, path, def.ToString(value));
                    }
                    break;
                }
            case "enum":
                {
                    Draw(properties, pvarObject, basePath, def, baseOffset, (offset, name, path2) =>
                    {
                        // read value
                        string strValue = pvarValues[path2];
                        var value = (long)def.FromString(strValue);
                        if (strValue == null) value = (long?)def.FromBytes(pvarOverlay, pvarData, offset) ?? value;
                        value &= (long)(Math.Pow(2, dataSize * 8) - 1);

                        EditorGUI.BeginChangeCheck();
                        value = PVarsPropertyField_EnumPopup(new GUIContent(name, def.Tooltip), value, def.GetOptions(), dataSize);
                        if (EditorGUI.EndChangeCheck())
                        {
                            pvarValues.SetPropertyKeyValue(properties.PVarValues, path2, def.ToString(value));
                        }
                    });
                    break;
                }
            case "mask":
                {
                    Draw(properties, pvarObject, basePath, def, baseOffset, (offset, name, path2) =>
                    {
                        // read value
                        string strValue = pvarValues[path2];
                        var value = (long)def.FromString(strValue);
                        if (strValue == null) value = (long?)def.FromBytes(pvarOverlay, pvarData, offset) ?? value;
                        value &= (long)(Math.Pow(2, dataSize * 8) - 1);

                        EditorGUI.BeginChangeCheck();
                        value = PVarsPropertyField_MaskPopup(new GUIContent(name, def.Tooltip), value, def.GetOptions(), dataSize);
                        if (EditorGUI.EndChangeCheck())
                        {
                            pvarValues.SetPropertyKeyValue(properties.PVarValues, path2, def.ToString(value));
                        }
                    });
                    break;
                }
            case "raidsmobid":
                {
                    Draw(properties, pvarObject, basePath, def, baseOffset, (offset, name, path2) =>
                    {
                        // read value
                        string strValue = pvarValues[path2];
                        var value = (long)def.FromString(strValue);
                        if (strValue == null) value = (long?)def.FromBytes(pvarOverlay, pvarData, offset) ?? value;
                        value &= (long)(Math.Pow(2, dataSize * 8) - 1);

                        if (!_raidsModeData)
                            _raidsModeData = GameObject.FindObjectOfType<RaidsModeData>();

                        var mobIds = _raidsModeData ? _raidsModeData.Mobs.ToDictionary(x => x.Disabled ? $"{x.Name} (DISABLED)" : x.Name, x => (long)_raidsModeData.Mobs.IndexOf(x)) : new Dictionary<string, long>();
                        mobIds.Add("None", -1);

                        EditorGUI.BeginChangeCheck();
                        value = PVarsPropertyField_EnumPopup(new GUIContent(name, def.Tooltip), value, mobIds, dataSize);
                        if (EditorGUI.EndChangeCheck())
                        {
                            pvarValues.SetPropertyKeyValue(properties.PVarValues, path2, def.ToString(value));
                        }
                    });
                    break;
                }
            case "raidsmobbehavior":
                {
                    Draw(properties, pvarObject, basePath, def, baseOffset, (offset, name, path2) =>
                    {
                        // read value
                        string strValue = pvarValues[path2];
                        var value = (long)def.FromString(strValue);
                        if (strValue == null) value = (long?)def.FromBytes(pvarOverlay, pvarData, offset) ?? value;
                        value &= (long)(Math.Pow(2, dataSize * 8) - 1);

                        if (!_raidsModeData)
                            _raidsModeData = GameObject.FindObjectOfType<RaidsModeData>();

                        if (!_raidsMobConfig)
                            _raidsMobConfig = RaidsMobsScriptableObject.Load();

                        // find reference field
                        // check if raidsmobid
                        // grab mob id
                        var refPath = basePath + $".{def.Ref}";
                        RaidsMob? mobId = null;
                        if (pvarValues.ContainsKey(refPath))
                        {
                            var refValue = pvarValues[refPath];
                            if (int.TryParse(refValue, out int idx))
                                mobId = _raidsModeData?.Mobs?.ElementAtOrDefault(idx)?.Mob;
                        }

                        var mob = _raidsMobConfig?.Mobs?.FirstOrDefault(x => x.Mob == mobId);
                        if (mob == null)
                            return;

                        if (mob?.Behaviors == null || !mob.Behaviors.Any())
                            return;

                        var behaviors = mob.Behaviors.ToDictionary(x => x, x => (long)mob.Behaviors.IndexOf(x));

                        EditorGUI.BeginChangeCheck();
                        value = PVarsPropertyField_EnumPopup(new GUIContent(name, def.Tooltip), value, behaviors, dataSize);
                        if (EditorGUI.EndChangeCheck())
                        {
                            pvarValues.SetPropertyKeyValue(properties.PVarValues, path2, def.ToString(value));
                        }
                    });
                    break;
                }
            case "mobygroupid":
                {
                    // read value
                    string strValue = pvarValues[path];
                    int value = (int)def.FromString(strValue);
                    if (strValue == null) value = (int?)def.FromBytes(pvarOverlay, pvarData, baseOffset) ?? value;

                    EditorGUI.BeginChangeCheck();
                    value = EditorGUILayout.IntField(new GUIContent(def.Name, def.Tooltip), value);
                    if (EditorGUI.EndChangeCheck())
                    {
                        if (value < def.Min) value = (int)def.Min;
                        if (value > def.Max) value = (int)def.Max;
                        pvarValues.SetPropertyKeyValue(properties.PVarValues, path, def.ToString(value));
                    }
                    break;
                }
            case "tiegroupid":
                {
                    // read value
                    string strValue = pvarValues[path];
                    int value = (int)def.FromString(strValue);
                    if (strValue == null) value = (int?)def.FromBytes(pvarOverlay, pvarData, baseOffset) ?? value;
                    
                    EditorGUI.BeginChangeCheck();
                    value = EditorGUILayout.IntField(new GUIContent(def.Name, def.Tooltip), value);
                    if (EditorGUI.EndChangeCheck())
                    {
                        if (value < def.Min) value = (int)def.Min;
                        if (value > def.Max) value = (int)def.Max;
                        pvarValues.SetPropertyKeyValue(properties.PVarValues, path, def.ToString(value));
                    }
                    break;
                }
            case "mobyrefpvar":
                {
                    Draw(properties, pvarObject, basePath, def, baseOffset, (offset, name, path2) =>
                    {
                        PvarOverlay mobyRefPvarOverlay = null;
                        string[] mobyRefPvarPaths = new string[0];
                        string oClass = null;

                        // find reference field
                        // check if mobyref
                        // grab moby
                        //(PvarOverlayDef refField, int refFieldParentOffset) = def.FindFieldFrom(pvarOverlay, def.Ref, offset);
                        var refPath = basePath + $".{def.Ref}";
                        if (pvarRefs.ContainsKey(refPath))
                        {
                            var mobyRef = pvarRefs[refPath] as Moby;
                            if (mobyRef)
                            {
                                oClass = mobyRef.OClass.ToString();
                                mobyRefPvarOverlay = PvarOverlay.GetPvarOverlay(mobyRef.RCVersion, mobyClass: mobyRef.OClass);
                                if (mobyRefPvarOverlay != null)
                                {
                                    mobyRefPvarPaths = mobyRefPvarOverlay.GetPVarPaths(mobyRef);
                                }
                            }
                        }

                        // read value
                        var parts = pvarValues[path2]?.Split('|', StringSplitOptions.RemoveEmptyEntries);
                        var pvarPath = parts?.ElementAtOrDefault(1);

                        if (parts != null && parts.Length > 0 && parts[0] != oClass)
                        {
                            pvarPath = null;
                            Undo.RecordObject(pvarValues.Owner, "OClass Change");
                            pvarValues.SetPropertyKeyValue(properties.PVarValues, path2, oClass + "|" + pvarPath);
                        }

                        EditorGUI.BeginChangeCheck();
                        pvarPath = PVarsPropertyField_EnumPopup(new GUIContent(name, def.Tooltip), pvarPath, mobyRefPvarPaths, 4);
                        if (EditorGUI.EndChangeCheck())
                        {
                            pvarValues.SetPropertyKeyValue(properties.PVarValues, path2, oClass + "|" + pvarPath);
                        }
                    });
                    break;
                }
            case "mobyrefpvarvalue":
                {
                    Draw(properties, pvarObject, basePath, def, baseOffset, (offset, name, path2) =>
                    {
                        PvarOverlayDef refDef = null;

                        // find reference field
                        // check if mobyref
                        // grab moby
                        //(PvarOverlayDef refField, int refFieldParentOffset) = def.FindFieldFrom(pvarOverlay, def.Ref, offset);
                        var refPath = basePath + $".{def.Ref}";
                        if (pvarValues.ContainsKey(refPath))
                        {
                            var parts = pvarValues[refPath]?.Split('|', StringSplitOptions.RemoveEmptyEntries);
                            var pvarPath = parts?.ElementAtOrDefault(1);
                            int? oClass = int.TryParse(parts?.ElementAtOrDefault(0), out var mobyClass) ? mobyClass : null;
                            var mobyRefPvarOverlay = PvarOverlay.GetPvarOverlay(pvarOverlay.RCVersion, mobyClass: oClass);
                            if (mobyRefPvarOverlay != null)
                            {
                                var pvarMetadata = mobyRefPvarOverlay.GetPVarMetadata(pvarPath);
                                if (pvarMetadata != null)
                                {
                                    refDef = pvarMetadata.Field;
                                }
                            }
                        }

                        if (refDef != null)
                        {
                            var defOffset = refDef.Offset;
                            var defCount = refDef.Count;
                            EditorGUILayout.PrefixLabel(new GUIContent(name, def.Tooltip));
                            EditorGUI.indentLevel++;
                            try
                            {
                                refDef.Offset = 0;
                                refDef.Count = null;
                                PVarsPropertyField_OverlayField(pvarOverlay, properties, pvarObject, path2, refDef, defOffsetAdditive: offset);
                            }
                            finally
                            {
                                EditorGUI.indentLevel--;
                                refDef.Offset = defOffset;
                                refDef.Count = defCount;
                            }
                        }
                    });
                    break;
                }
            case "mobyrefstate":
                {
                    Draw(properties, pvarObject, basePath, def, baseOffset, (offset, name, path2) =>
                    {
                        Dictionary<string, long> stateOptions = null;

                        // find reference field
                        // check if mobyref
                        // grab moby
                        //(PvarOverlayDef refField, int refFieldParentOffset) = def.FindFieldFrom(pvarOverlay, def.Ref, offset);
                        var refPath = basePath + $".{def.Ref}";
                        if (pvarRefs.ContainsKey(refPath))
                        {
                            var mobyRef = pvarRefs[refPath] as Moby;
                            if (mobyRef)
                            {
                                var mobyRefPvarOverlay = PvarOverlay.GetPvarOverlay(mobyRef.RCVersion, mobyClass: mobyRef.OClass);
                                if (mobyRefPvarOverlay != null)
                                {
                                    stateOptions = mobyRefPvarOverlay.States;
                                }
                            }
                        }

                        // read value
                        string strValue = pvarValues[path2];
                        var value = (long)def.FromString(strValue);
                        if (strValue == null) value = (long?)def.FromBytes(pvarOverlay, pvarData, offset) ?? value;
                        value &= (long)(Math.Pow(2, dataSize * 8) - 1);

                        if (stateOptions != null && stateOptions.Any())
                        {
                            EditorGUI.BeginChangeCheck();
                            value = PVarsPropertyField_EnumPopup(new GUIContent(name, def.Tooltip), value, stateOptions, dataSize);
                            if (EditorGUI.EndChangeCheck())
                            {
                                if (value < -1) value = -1;
                                if (value > 127) value = 127;
                                pvarValues.SetPropertyKeyValue(properties.PVarValues, path2, def.ToString(value));
                            }
                        }
                        else
                        {
                            // read value
                            EditorGUI.BeginChangeCheck();
                            value = EditorGUILayout.IntField(new GUIContent(name, def.Tooltip), (int)value);
                            if (EditorGUI.EndChangeCheck())
                            {
                                if (value < def.Min) value = (int)def.Min;
                                if (value > def.Max) value = (int)def.Max;
                                if (value < -1) value = -1;
                                if (value > 127) value = 127;
                                pvarValues.SetPropertyKeyValue(properties.PVarValues, path2, def.ToString(value));
                            }
                        }
                    });
                    break;
                }
            case "cuboidref":
                {
                    Draw(properties, pvarObject, basePath, def, baseOffset, (offset, name, path2) =>
                    {
                        // read value
                        var refObj = pvarRefs.GetPropertyKeyValue(properties.PVarRefs, path2);
                        EditorGUILayout.ObjectField(refObj, typeof(Cuboid), new GUIContent(name, def.Tooltip));
                    });
                    break;
                }
            case "splineref":
                {
                    Draw(properties, pvarObject, basePath, def, baseOffset, (offset, name, path2) =>
                    {
                        var refObj = pvarRefs.GetPropertyKeyValue(properties.PVarRefs, path2);
                        EditorGUILayout.ObjectField(refObj, typeof(Spline), new GUIContent(name, def.Tooltip));
                    });
                    break;
                }
            case "arearef":
                {
                    Draw(properties, pvarObject, basePath, def, baseOffset, (offset, name, path2) =>
                    {
                        var refObj = pvarRefs.GetPropertyKeyValue(properties.PVarRefs, path2);
                        EditorGUILayout.ObjectField(refObj, typeof(Area), new GUIContent(name, def.Tooltip));
                    });
                    break;
                }
            case "mobyref":
                {
                    Draw(properties, pvarObject, basePath, def, baseOffset, (offset, name, path2) =>
                    {
                        var refObj = pvarRefs.GetPropertyKeyValue(properties.PVarRefs, path2);
                        EditorGUILayout.ObjectField(refObj, typeof(Moby), new GUIContent(name, def.Tooltip));
                    });
                    break;
                }
            case "pathgraphref":
                {
                    Draw(properties, pvarObject, basePath, def, baseOffset, (offset, name, path2) =>
                    {
                        var refObj = pvarRefs.GetPropertyKeyValue(properties.PVarRefs, path2);
                        EditorGUILayout.ObjectField(refObj, typeof(PathGraph), new GUIContent(name, def.Tooltip));
                    });
                    break;
                }
            case "struct":
                {
                    Draw(properties, pvarObject, basePath, def, baseOffset, (offset, name, path2) =>
                    {
                        if (def.Fields != null)
                        {
                            EditorGUILayout.PrefixLabel(new GUIContent(name, def.Tooltip));
                            EditorGUI.indentLevel++;
                            foreach (var structDef in def.Fields)
                            {
                                PVarsPropertyField_OverlayField(pvarOverlay, properties, pvarObject, path2, structDef, defOffsetAdditive: offset);
                            }
                            EditorGUI.indentLevel--;
                        }
                    });
                    break;
                }
            case "varstringcontainer":
                {
                    DrawToggleGroup(properties, def, baseOffset, (offset, name) =>
                    {
                        // read string length
                        var messageCount = properties.Strings.arraySize;

                        // count int box
                        EditorGUI.BeginChangeCheck();
                        messageCount = EditorGUILayout.IntField(new GUIContent("Count", def.Tooltip), messageCount);
                        if (messageCount < 0) messageCount = 0;
                        if (messageCount > PvarOverlay.VARSTRING_CONTAINER_MAX_COUNT) messageCount = PvarOverlay.VARSTRING_CONTAINER_MAX_COUNT;

                        if (EditorGUI.EndChangeCheck())
                        {
                            properties.Strings.arraySize = messageCount;
                        }

                        for (int i = 0; i < messageCount; ++i)
                        {
                            var str = properties.Strings.GetArrayElementAtIndex(i).stringValue;

                            EditorGUI.BeginChangeCheck();
                            EditorGUILayout.PrefixLabel(new GUIContent($"[{i}]"));
                            str = EditorGUILayout.TextArea(str);
                            if (EditorGUI.EndChangeCheck())
                            {
                                properties.Strings.GetArrayElementAtIndex(i).stringValue = str;
                            }
                        }
                    });
                    break;
                }
            case "messagecontainer":
                {
                    DrawToggleGroup(properties, def, baseOffset, (offset, name) =>
                    {
                        // read string length
                        var messageCount = properties.Strings.arraySize;

                        // count int box
                        EditorGUI.BeginChangeCheck();
                        messageCount = EditorGUILayout.IntField(new GUIContent("Count", def.Tooltip), messageCount);
                        if (messageCount < 0) messageCount = 0;
                        if (messageCount > PvarOverlay.VARSTRING_CONTAINER_MAX_COUNT) messageCount = PvarOverlay.VARSTRING_CONTAINER_MAX_COUNT;

                        if (EditorGUI.EndChangeCheck())
                        {
                            properties.Strings.arraySize = messageCount;
                        }

                        // read value
                        string strValue = pvarValues[path];
                        var messageData = strValue?.Split('|');
                        var newMessageData = "";

                        EditorGUI.indentLevel++;
                        for (int i = 0; i < messageCount; ++i)
                        {
                            int runtime = 0;
                            int moveTo = 0;
                            if (messageData != null && i < messageData.Length)
                            {
                                if (messageData[i] != null)
                                {
                                    var parts = messageData[i].Split(',');
                                    int.TryParse(parts?.ElementAtOrDefault(0), out runtime);
                                    int.TryParse(parts?.ElementAtOrDefault(1), out moveTo);
                                }
                            }

                            // get string
                            var str = properties.Strings.GetArrayElementAtIndex(i).stringValue;

                            EditorGUI.BeginChangeCheck();
                            EditorGUILayout.PrefixLabel(new GUIContent($"[{i}]"));
                            EditorGUI.indentLevel++;
                            runtime = EditorGUILayout.IntField("Runtime (s)", runtime);
                            bool showMoveTo = EditorGUILayout.Toggle(new GUIContent("On Complete"), moveTo > 0);
                            if (showMoveTo)
                            {
                                if (moveTo <= 0) moveTo = 1;
                                moveTo = EditorGUILayout.IntField(new GUIContent("Show Message Id"), moveTo - 1) + 1;
                            }
                            else
                            {
                                moveTo = 0;
                            }

                            str = EditorGUILayout.TextArea(str);
                            if (EditorGUI.EndChangeCheck())
                            {
                                // write runtime
                                runtime = (byte)Math.Clamp(runtime, 0, byte.MaxValue);
                                moveTo = (byte)Math.Clamp(moveTo, 0, byte.MaxValue);

                                // set str
                                properties.Strings.GetArrayElementAtIndex(i).stringValue = str;
                            }
                            EditorGUI.indentLevel--;

                            newMessageData += $"{runtime},{moveTo}|";
                        }
                        EditorGUI.indentLevel--;

                        newMessageData = newMessageData?.Trim('|');
                        if (strValue != newMessageData)
                        {
                            pvarValues.SetPropertyKeyValue(properties.PVarValues, path, newMessageData);
                        }
                    });
                    break;
                }

            // RENDER ONLY
            case "label":
                {
                    Draw(properties, pvarObject, basePath, def, baseOffset, (offset, name, path2) =>
                    {
                        EditorGUILayout.Space(10);
                        EditorGUILayout.PrefixLabel(new GUIContent(name, def.Tooltip));
                    });
                    break;
                }
            case "space":
                {
                    Draw(properties, pvarObject, basePath, def, baseOffset, (offset, name, path2) =>
                    {
                        EditorGUILayout.Space(10);
                    });
                    break;
                }
        }
    }

    private static void Draw(PVarsPropertiesContainer properties, IPVarObject pvarObject, string path, PvarOverlayDef def, int offset, Action<int, string, string> draw)
    {
        var count = def.Count ?? 1;
        var dataSize = def.GetDataSize();
        if (properties.PVars.arraySize <= offset)
            return;

        var serializedProperty = properties.PVars.GetArrayElementAtIndex(offset);
        var pvarValues = pvarObject.GetPVarValues();
        var pvarRefs = pvarObject.GetPVarReferences();

        if (count > 1)
        {
            serializedProperty.isExpanded = EditorGUILayout.BeginToggleGroup(new GUIContent(def.Name, def.Tooltip), serializedProperty.isExpanded);
            EditorGUI.indentLevel++;
        }

        if (count <= 1 || serializedProperty.isExpanded)
        {
            for (int i = 0; i < count; ++i)
            {
                var defOffset = offset + (i * dataSize);
                var name = def.Name;
                var path2 = path + $".{def.Name}";
                if (count > 1)
                {
                    path2 += $"[{i}]";
                    name = def.Labels?.ElementAtOrDefault(i) ?? $"[{i}]";
                }

                draw(defOffset, name, path2);
                
                if (count > 1)
                {
                    EditorGUILayout.BeginHorizontal();
                    EditorGUILayout.Space(0); // moves buttons to right side
                    GUI.enabled = i > 0;
                    if (GUILayout.Button("Move Up", GUILayout.Width(100)))
                    {
                        Swap(properties, pvarObject, pvarValues, pvarRefs, path + "." + def.Name, i, i - 1);
                    }
                    GUI.enabled = i < (count - 1);
                    if (GUILayout.Button("Move Down", GUILayout.Width(100)))
                    {
                        Swap(properties, pvarObject, pvarValues, pvarRefs, path + "." + def.Name, i, i + 1);
                    }
                    GUI.enabled = true;
                    EditorGUILayout.EndHorizontal();
                }
            }
        }

        if (count > 1)
        {
            EditorGUI.indentLevel--;
            EditorGUILayout.EndToggleGroup();
        }
    }

    private static void Swap(PVarsPropertiesContainer properties, IPVarObject pvarObject, SerializableStringDictionary pvarValues, SerializableMonoBehaviourDictionary pvarRefs, string path, int i0, int i1)
    {
        var path0Swap = path + $"[{i0}]";
        var path1Swap = path + $"[{i1}]";

        // swap values
        if (pvarValues.ContainsPath(path0Swap) || pvarValues.ContainsPath(path1Swap))
        {
            var subkeys = pvarValues.Where(x => x.Key.StartsWith(path0Swap)).Select(x => x.Key.Substring(path0Swap.Length))
                .Union(pvarValues.Where(x => x.Key.StartsWith(path1Swap)).Select(x => x.Key.Substring(path1Swap.Length)))
                .Distinct().ToArray();
            foreach (var subkey in subkeys)
            {
                var t0Path = $"{path0Swap}{subkey}";
                var t1Path = $"{path1Swap}{subkey}";
                var t0 = pvarValues[t0Path];
                var t1 = pvarValues[t1Path];
                pvarValues.SetPropertyKeyValue(properties.PVarValues, t0Path, t1);
                pvarValues.SetPropertyKeyValue(properties.PVarValues, t1Path, t0);
            }
            EditorUtility.SetDirty(pvarObject as MonoBehaviour);
        }

        // swap refs
        if (pvarRefs.ContainsPath(path0Swap) || pvarRefs.ContainsPath(path1Swap))
        {
            var subkeys = pvarRefs.Where(x => x.Key.StartsWith(path0Swap)).Select(x => x.Key.Substring(path0Swap.Length))
                .Union(pvarRefs.Where(x => x.Key.StartsWith(path1Swap)).Select(x => x.Key.Substring(path1Swap.Length)))
                .Distinct().ToArray();
            foreach (var subkey in subkeys)
            {
                var t0Path = $"{path0Swap}{subkey}";
                var t1Path = $"{path1Swap}{subkey}";
                var t0 = pvarRefs[t0Path];
                var t1 = pvarRefs[t1Path];
                pvarRefs.SetPropertyKeyValue(properties.PVarRefs, t0Path, t1);
                pvarRefs.SetPropertyKeyValue(properties.PVarRefs, t1Path, t0);
            }
            EditorUtility.SetDirty(pvarObject as MonoBehaviour);
        }
    }

    private static void DrawToggleGroup(PVarsPropertiesContainer properties, PvarOverlayDef def, int offset, Action<int, string> draw)
    {
        var dataSize = def.GetDataSize();
        var serializedProperty = properties.PVars.GetArrayElementAtIndex(offset);

        serializedProperty.isExpanded = EditorGUILayout.BeginToggleGroup(new GUIContent(def.Name, def.Tooltip), serializedProperty.isExpanded);
        EditorGUI.indentLevel++;

        if (serializedProperty.isExpanded)
        {
            draw(offset, def.Name);
        }

        EditorGUI.indentLevel--;
        EditorGUILayout.EndToggleGroup();
    }

    private static void SerializedPropertySwap(SerializedProperty property, int idx0, int idx1)
    {
        if (property == null) return;
        if (idx0 < 0 || idx1 < 0) return;
        if (idx0 >= property.arraySize) return;
        if (idx1 >= property.arraySize) return;

        var temp = property.GetArrayElementAtIndex(idx0).boxedValue;
        property.GetArrayElementAtIndex(idx0).boxedValue = property.GetArrayElementAtIndex(idx1).boxedValue;
        property.GetArrayElementAtIndex(idx1).boxedValue = temp;
    }

    private static T PVarsPropertyField_EnumPopup<T>(GUIContent label, T value, float? min, float? max) where T : struct, IConvertible
    {
        var options = ((T[])Enum.GetValues(typeof(T))).Where(x => !((int)(object)x < min) && !((int)(object)x > max)).ToArray();
        var names = options.Select(x => Enum.GetName(typeof(T), x)).ToArray();

        return options.ElementAtOrDefault(EditorGUILayout.Popup(label, Array.IndexOf(options, value), names));
    }

    private static long PVarsPropertyField_EnumPopup(GUIContent label, long value, Dictionary<string, long> options, int dataSize)
    {
        if (options == null) return value;

        var mask = (long)(Math.Pow(2, dataSize * 8) - 1);
        var names = options.Select(x => x.Key).ToArray();
        var selectedKey = options.FirstOrDefault(x => (x.Value & mask) == (value & mask)).Key;
        var idx = Array.IndexOf(names, selectedKey);

        idx = EditorGUILayout.Popup(label, idx, names);

        selectedKey = names.ElementAtOrDefault(idx);
        if (selectedKey == null) return value;
        return options.GetValueOrDefault(selectedKey);
    }

    private static string PVarsPropertyField_EnumPopup(GUIContent label, string value, string[] options, int dataSize)
    {
        if (options == null) return value;

        var idx = Array.IndexOf(options, value);
        idx = EditorGUILayout.Popup(label, idx, options);
        return options.ElementAtOrDefault(idx);
    }

    private static long PVarsPropertyField_MaskPopup(GUIContent label, long value, Dictionary<string, long> options, int dataSize)
    {
        if (options == null) return value;

        var mask = (long)(Math.Pow(2, dataSize * 8) - 1);
        var names = options.Select(x => x.Key).ToArray();
        var selectedKeys = options.Where(x => ((x.Value & value) & mask) != 0);
        var selectedMask = selectedKeys.Any() ? selectedKeys.Select(x => 1 << Array.IndexOf(names, x.Key)).Aggregate((a, b) => a | b) : 0;

        selectedMask = EditorGUILayout.MaskField(label, selectedMask, names);

        long finalValue = 0;
        foreach (var option in options)
        {
            var selectedBit = 1 << Array.IndexOf(names, option.Key);
            if ((selectedMask & selectedBit) != 0)
                finalValue |= option.Value;
        }

        return finalValue;
    }

    private static void PVarsPropertyField_ReadPVarData(PVarsPropertiesContainer properties, byte[] dst, int srcOffset, int length)
    {
        for (int i = 0; i < length; ++i)
            dst[i] = (byte)properties.PVars.GetArrayElementAtIndex(i + srcOffset).intValue;
    }

    private static void PVarsPropertyField_WritePVarData(PVarsPropertiesContainer properties, byte[] src, int dstOffset, int length)
    {
        for (int i = 0; i < length; ++i)
            properties.PVars.GetArrayElementAtIndex(i + dstOffset).intValue = src[i];
    }

    #endregion

    public static void OnAfterCreateGameObject(GameObject go)
    {
        // place under selected object
        // or try and spawn on top of scene camera
        if (Selection.activeGameObject)
            go.transform.SetParent(Selection.activeGameObject.transform, false);
        else if (SceneView.lastActiveSceneView.camera)
            go.transform.position = SceneView.lastActiveSceneView.camera.transform.position + (SceneView.lastActiveSceneView.camera.transform.forward * 5);

        Selection.activeGameObject = go;
    }

    public static void MarkActiveSceneDirty()
    {
        Dispatcher.RunOnMainThread(() =>
        {
            EditorSceneManager.MarkSceneDirty(EditorSceneManager.GetActiveScene());
        });
    }

    public static bool IsObjectPrefabFile(UnityEngine.Object target)
    {
        var assetType = PrefabUtility.GetPrefabAssetType(target);
        var instanceStatus = PrefabUtility.GetPrefabInstanceStatus(target);

        if (assetType != PrefabAssetType.NotAPrefab && instanceStatus == PrefabInstanceStatus.NotAPrefab)
        {
            return true;
        }

        return false;
    }

    public static void RecurseHierarchy(Transform root, Action<Transform> onNode)
    {
        if (!root) return;
        onNode(root);

        foreach (Transform child in root)
            RecurseHierarchy(child, onNode);
    }

    public static void CloneHierarchy(Transform srcRoot, Transform dstRoot, Func<Transform, Transform, bool> onNode)
    {
        if (!srcRoot) return;
        if (!dstRoot) return;

        // copy
        dstRoot.transform.localPosition = srcRoot.transform.localPosition;
        dstRoot.transform.localRotation = srcRoot.transform.localRotation;
        dstRoot.transform.localScale = srcRoot.transform.localScale;

        if (!onNode(srcRoot, dstRoot.transform))
        {
            GameObject.DestroyImmediate(dstRoot.gameObject);
            return;
        }

        foreach (Transform child in srcRoot)
        {
            var newNode = new GameObject(child.gameObject.name);
            newNode.transform.SetParent(dstRoot.transform, false);
            CloneHierarchy(child, newNode.transform, onNode);
        }
    }

    public static Transform FindInHierarchy(Transform root, string childName)
    {
        if (root.name == childName)
            return root;

        for (int i = 0; i < root.childCount; ++i)
        {
            var hit = FindInHierarchy(root.GetChild(i), childName);
            if (hit)
                return hit;
        }

        return null;
    }

    public static bool HierarchyIsDifferent(Transform a, Transform b)
    {
        if (!a || !b) return false;

        var prefabChildCount = a.transform.childCount;
        var instanceChildCount = b.transform.childCount;
        if (prefabChildCount != instanceChildCount) return true;

        for (int i = 0; i < prefabChildCount; ++i)
        {
            var aT = a.transform.GetChild(i);
            var bT = b.transform.GetChild(i);

            if (aT.name != bT.name) return true;
            if (HierarchyIsDifferent(aT, bT)) return true;
        }

        return false;
    }

    public static string GetPath(Transform root, Transform t)
    {
        if (root == t)
            return "";

        return (GetPath(root, t.parent) + "/" + t.name).TrimStart('/');
    }

    public static GameObject GetAssetPrefab(string assetType, string oClass, int racVersion = 0, bool includeGlobal = false)
    {
        // always return local asset path
        var path = FolderNames.GetLocalAssetFolder(assetType, racVersion);
        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(Path.Combine(path, oClass, $"{oClass}.fbx"));

        if (includeGlobal && !prefab)
        {
            path = FolderNames.GetGlobalAssetFolder(assetType, racVersion);
            prefab = AssetDatabase.LoadAssetAtPath<GameObject>(Path.Combine(path, oClass, $"{oClass}.fbx"));
        }

        return prefab;
    }

    public static GameObject GetAssetColliderPrefab(string assetType, string oClass, int racVersion = 0, bool includeGlobal = false)
    {
        // always return local asset path
        var path = FolderNames.GetLocalAssetFolder(assetType, racVersion);
        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(Path.Combine(path, oClass, $"{oClass}_col.fbx"));
        if (!prefab) prefab = AssetDatabase.LoadAssetAtPath<GameObject>(Path.Combine(path, oClass, $"{oClass}_col.blend"));

        if (includeGlobal && !prefab)
        {
            path = FolderNames.GetGlobalAssetFolder(assetType, racVersion);
            prefab = AssetDatabase.LoadAssetAtPath<GameObject>(Path.Combine(path, oClass, $"{oClass}_col.fbx"));
            if (!prefab) prefab = AssetDatabase.LoadAssetAtPath<GameObject>(Path.Combine(path, oClass, $"{oClass}_col.blend"));
        }

        return prefab;
    }

    public static GameObject GetCuboidPrefab(CuboidMaskType cuboidType)
    {
        var path = FolderNames.GetGlobalPrefabFolder("Cuboid");
        GameObject prefab = null;

        foreach (var type in (CuboidMaskType[])Enum.GetValues(typeof(CuboidMaskType)))
        {
            if (!cuboidType.HasFlag(type)) continue;

            prefab = AssetDatabase.LoadAssetAtPath<GameObject>(Path.Combine(path, $"{type}.prefab"));
        }

        return prefab;
    }

    public static GameObject GetSNDPrefab(string prefabName)
    {
        var path = FolderNames.GetGlobalPrefabFolder("SND");
        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(Path.Combine(path, $"{prefabName}.prefab"));
        return prefab;
    }

    public static GameObject GetRaidsPrefab(string prefabName)
    {
        var path = FolderNames.GetGlobalPrefabFolder("Raids");
        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(Path.Combine(path, $"{prefabName}.prefab"));
        return prefab;
    }

    public static GameObject GetMiscPrefab(string prefabName)
    {
        var path = FolderNames.GetGlobalPrefabFolder("Misc");
        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(Path.Combine(path, $"{prefabName}.prefab"));
        return prefab;
    }

    public static string GetProjectRelativePath(string absolutePath) => Path.GetRelativePath(Environment.CurrentDirectory, absolutePath);

    public static void ImportTexture(string path, TextureWrapMode? wrapu = null, TextureWrapMode? wrapv = null)
    {
        var assetPath = UnityHelper.GetProjectRelativePath(path);
        AssetDatabase.ImportAsset(assetPath);
        TextureImporter importer = (TextureImporter)TextureImporter.GetAtPath(assetPath);
        importer.alphaIsTransparency = true;
        if (wrapu.HasValue) importer.wrapModeU = wrapu.Value;
        if (wrapv.HasValue) importer.wrapModeV = wrapv.Value;
        importer.SaveAndReimport();
    }

    public static List<IOcclusionData> GetAllOcclusionDataInSelection()
    {
        return Selection.gameObjects?.SelectMany(x => x.GetComponentsInChildren<IOcclusionData>())?.ToList();
    }

    public static List<IOcclusionData> GetAllOcclusionData()
    {
        return GameObject.FindObjectsOfType<MonoBehaviour>().Where(x => x is IOcclusionData).Select(x => x as IOcclusionData).ToList();
    }

    public static List<Vector3> GetAllOctants(bool useCache = true)
    {
        var volumes = GameObject.FindObjectsOfType<OcclusionVolume>();
        var rawOctants = GameObject.FindObjectsOfType<OcclusionOctant>();
        var octants = rawOctants.SelectMany(x => x.Octants ?? new List<Vector3>()).ToList();
        foreach (var volume in volumes)
        {
            if (volume.Negate) continue;
            octants.AddRange(useCache ? volume.GetCachedOctants() : volume.GetOctants());
        }

        var negativeOctants = volumes.Where(x => x.Negate).ToList();
        octants.RemoveAll(x => negativeOctants.Any(o => o.Contains(x)));

        return octants.Distinct().ToList();
    }

    public static void DrawLine(Vector3 from, Vector3 to, Color color, float thickness)
    {
        Handles.DrawBezier(from, to, from, to, color, null, thickness);
    }

    public static Texture2D GetMainTexture(this Material mat)
    {
        var tex = mat.mainTexture;
        if (tex) return tex as Texture2D;

        string[] texPropertyNames = { "_BaseMap", "_MainTex", "baseColorTexture" };
        foreach (var texPropertyName in texPropertyNames)
        {
            if (mat.HasProperty(texPropertyName))
            {
                tex = mat.GetTexture(texPropertyName);
                if (tex) return tex as Texture2D;
            }
        }

        return null;
    }

    public static void SaveRenderTexture(RenderTexture rt, string path)
    {
        Texture2D tex = new Texture2D(rt.width, rt.height, TextureFormat.RGBA32, false);
        RenderTexture.active = rt;
        tex.ReadPixels(new Rect(0, 0, rt.width, rt.height), 0, 0);
        tex.Apply();

        byte[] bytes = tex.EncodeToPNG();
        System.IO.File.WriteAllBytes(path, bytes);
        AssetDatabase.ImportAsset(path);
    }

    public static bool SaveTexture(Texture2D tex, string path, Color? tint = null, bool hasAlpha = true, bool forcePowerOfTwo = false, int? maxTexSize = null)
    {
        if (tex)
        {
            // copy file if no operations need be done on the texture
            var assetPath = AssetDatabase.GetAssetPath(tex);
            if (!String.IsNullOrEmpty(assetPath) && Path.GetExtension(assetPath) == ".png")
            {
                if (maxTexSize == null || (tex.width <= maxTexSize && tex.height <= maxTexSize))
                {
                    if (!forcePowerOfTwo || (Mathf.Log(tex.width, 2) == tex.width && Mathf.Log(tex.height, 2) == tex.height))
                    {
                        File.Copy(assetPath, path, true);
                        return true;
                    }
                }
            }

            var width = tex.width;
            var height = tex.height;
            if (forcePowerOfTwo)
            {
                if (width > height && width > maxTexSize)
                {
                    height = Mathf.CeilToInt(height * (maxTexSize.Value / (float)width));
                    width = maxTexSize.Value;
                }
                else if (height > width && height > maxTexSize)
                {
                    width = Mathf.CeilToInt(width * (maxTexSize.Value / (float)height));
                    height = maxTexSize.Value;
                }
                else if (width > maxTexSize)
                {
                    width = maxTexSize.Value;
                    height = maxTexSize.Value;
                }

                // force power of two
                width = ForceDimensionPowerOfTwo(width);
                height = ForceDimensionPowerOfTwo(height);
            }

            var tex2 = CloneTexture(tex, hasAlpha: hasAlpha, tint: tint, resizeWidth: width, resizeHeight: height);
            if (tex2)
            {
                var bytes = tex2.EncodeToPNG();
                File.WriteAllBytes(path, bytes);
            }
        }

        return false;
    }

    public static Texture2D ResizeTexture(Texture2D src, int width, int height)
    {
        if (!src) return null;

        var lastActive = RenderTexture.active;
        RenderTexture rt = new RenderTexture(width, height, 24);
        RenderTexture.active = rt;
        Graphics.Blit(src, rt);
        Texture2D result = new Texture2D(width, height);
        result.ReadPixels(new Rect(0, 0, width, height), 0, 0);
        result.Apply();
        RenderTexture.active = lastActive;
        rt.Release();
        return result;
    }

    public static Texture2D CloneTexture(Texture2D src, bool hasAlpha = true, Color? tint = null, int? resizeWidth = null, int? resizeHeight = null)
    {
        if (!src) return null;

        var width = resizeWidth ?? src.width;
        var height = resizeHeight ?? src.height;
        var rt = new RenderTexture(width, height, 0, RenderTextureFormat.ARGB32);
        rt.Create();
        try
        {
            var mat = new Material(AssetDatabase.LoadAssetAtPath<Material>(Path.Combine(FolderNames.ForgeFolder, "Shaders", "TintBlit.mat")));
            mat.SetColor("_Color", tint ?? Color.white);
            mat.SetTexture("_In", src);
            mat.SetFloat("_ForceAlpha", hasAlpha ? 0 : (tint.HasValue ? tint.Value.a : 1));
            Graphics.Blit(src, rt, mat);

            var oldRt = RenderTexture.active;
            RenderTexture.active = rt;
            var tex2 = new Texture2D(width, height, TextureFormat.ARGB32, false);
            tex2.ReadPixels(new Rect(0, 0, width, height), 0, 0);
            tex2.Apply();
            RenderTexture.active = oldRt;

            return tex2;
        }
        finally
        {
            if (RenderTexture.active == rt)
                RenderTexture.active = null;

            rt.Release();
        }
    }

    public static Hash128 GetHash(this Texture2D tex)
    {
        if (!tex)
            return new Hash128();

        if (tex.imageContentsHash.isValid)
            return tex.imageContentsHash;

        if (!tex.isReadable)
            return new Hash128();

        var pixels = tex.GetPixelData<Color>(0);
        return Hash128.Compute(pixels);
    }

    public static Hash128 Append(this Hash128 hash, Vector3 value)
    {
        hash.Append(value.x);
        hash.Append(value.y);
        hash.Append(value.z);
        return hash;
    }

    public static Hash128 Append(this Hash128 hash, Matrix4x4 value)
    {
        for (int i = 0; i < 16; ++i)
            hash.Append(value[i]);
        return hash;
    }

    public static uint GetColor(this Color32 rgba, byte? forceAlpha = null)
    {
        return (uint)(
            (rgba.r << 0) |
            (rgba.g << 8) |
            (rgba.b << 16) |
            ((forceAlpha ?? rgba.a) << 24)
            );
    }

    public static Color32 GetColor(this uint rgba)
    {
        return new Color32(
            (byte)((rgba >> 0) & 0xff),
            (byte)((rgba >> 8) & 0xff),
            (byte)((rgba >> 16) & 0xff),
            (byte)((rgba >> 24) & 0xff)
            );
    }

    public static Color HalveRGB(this Color color)
    {
        return new Color(color.r * 0.5f, color.g * 0.5f, color.b * 0.5f, color.a);
    }

    public static Color DoubleRGB(this Color color)
    {
        return new Color(color.r * 2f, color.g * 2f, color.b * 2f, color.a);
    }

    public static Color ScaleRGB(this Color color, float factor)
    {
        return new Color(color.r * factor, color.g * factor, color.b * factor, color.a);
    }

    public static Color SetAlpha(this Color color, float alpha)
    {
        return new Color(color.r, color.g, color.b, alpha);
    }

    static int ForceDimensionPowerOfTwo(int dimension)
    {
        float exp = Mathf.Log(dimension, 2);
        if (exp == (int)exp) return dimension;

        return (int)Mathf.Pow(2, Mathf.CeilToInt(exp));
    }

    public static Hash128 Append(this Hash128 hash, Color color)
    {
        hash.Append(color.ToString());
        return hash;
    }

    public static int ComputeHash(this Mesh mesh)
    {
        int hash = 0;
        if (!mesh) return 0;

        mesh.GetHashCode();
        foreach (var v in mesh.vertices)
            hash = hash ^ v.GetHashCode();

        return hash;
    }

    public static Vector2 ClampUV(this Vector2 uv)
    {
        return new Vector2(uv.x % 1, uv.y % 1);
    }

    public static Vector2 Round(this Vector2 uv, int decimals = 3)
    {
        var precision = Mathf.Pow(10, decimals);
        return new Vector2(Mathf.Round(uv.x * precision) / precision, Mathf.Round(uv.y * precision) / precision);
    }

    public static Vector2 ClampUVRelativeTo(this Vector2 uv, Vector2 relativeTo, Vector2 direction)
    {
        var clamped = uv; //.ClampUV();

        var dx = Mathf.Round((clamped.x - relativeTo.x) * 1024) / 1024f;
        var dy = Mathf.Round((clamped.y - relativeTo.y) * 1024) / 1024f;

        if (Math.Sign(dx) != Math.Sign(direction.x) && direction.x != 0)
            clamped.x += 1;
        if (Math.Sign(dy) != Math.Sign(direction.y) && direction.y != 0)
            clamped.y += 1;

        return clamped;
    }

    public static Vector2 RotateAround(this Vector2 point, float radians, Vector2 pivot)
    {
        var dir = point - pivot;
        float cos = Mathf.Cos(radians);
        float sin = Mathf.Sin(radians);

        Vector2 rotatedDir = new Vector2(
            dir.x * cos - dir.y * sin,
            dir.x * sin + dir.y * cos
        );

        return pivot + rotatedDir;
    }

    public static Mesh BuildQuad()
    {
        var m = new Mesh()
        {
            vertices = new[] { new Vector3(-1, -1, 0), new Vector3(1, -1, 0), new Vector3(1, 1, 0), new Vector3(1, 1, 0) },
            triangles = new[] { 0, 2, 1, 2, 3, 1 },
            normals = new[] { -Vector3.forward, -Vector3.forward, -Vector3.forward, -Vector3.forward },
            uv = new[] { new Vector2(0, 0), new Vector2(1, 0), new Vector2(0, 1), new Vector2(1, 1) }
        };

        return m;
    }

    public static Mesh Clone(this Mesh mesh)
    {
        var m = new Mesh()
        {
            name = mesh.name,
            vertices = mesh.vertices,
            normals = mesh.normals,
            tangents = mesh.tangents,
            bounds = mesh.bounds,
            uv = mesh.uv,
            uv2 = mesh.uv2,
            colors = mesh.colors,
            colors32 = mesh.colors32,
            indexBufferTarget = mesh.indexBufferTarget,
            indexFormat = mesh.indexFormat,
            subMeshCount = mesh.subMeshCount,
            boneWeights = mesh.boneWeights,
            bindposes = mesh.bindposes,
        };

        // copy submeshes
        for (int i = 0; i < mesh.subMeshCount; ++i)
        {
            var submesh = mesh.GetSubMesh(i);
            m.SetIndices(mesh.GetIndices(i), submesh.topology, i);
        }

        return m;
    }

    public static void FlipFaces(this Mesh mesh, int subMeshIndex = -1)
    {
        for (int i = 0; i < mesh.subMeshCount; ++i)
        {
            if (subMeshIndex != -1 && i != subMeshIndex) continue;

            var triangles = mesh.GetTriangles(i);
            for (int j = 0; j < triangles.Length; j += 3)
            {
                var t = triangles[j + 2];
                triangles[j + 2] = triangles[j];
                triangles[j] = t;
            }
            mesh.SetTriangles(triangles, i);
        }
    }

    public static void AddBackSideFaces(this Mesh mesh)
    {
        var subMeshCount = mesh.subMeshCount;
        mesh.subMeshCount *= 2;
        for (int i = 0; i < subMeshCount; ++i)
        {
            var triangles = mesh.GetTriangles(i);
            for (int j = 0; j < triangles.Length; j += 3)
            {
                var t = triangles[j + 2];
                triangles[j + 2] = triangles[j];
                triangles[j] = t;
            }
            mesh.SetTriangles(triangles, subMeshCount + i);
        }
    }

    public static void RecalculateFaceNormals(this Mesh mesh, float normalFactor = 1f, bool flip = false)
    {
        var flipNormal = flip ? -1 : 1;
        for (int i = 0; i < mesh.subMeshCount; ++i)
        {
            var triangles = mesh.GetTriangles(i);

            // get center
            var centerSum = Vector3.zero;
            var centerCount = 0f;
            for (int j = 0; j < triangles.Length; j += 3)
            {
                var faceCenter = (mesh.vertices[triangles[j + 0]] + mesh.vertices[triangles[j + 1]] + mesh.vertices[triangles[j + 2]]) / 3;
                var normal = Vector3.Cross(mesh.vertices[triangles[j + 1]] - mesh.vertices[triangles[j + 0]], mesh.vertices[triangles[j + 2]] - mesh.vertices[triangles[j + 0]]);

                centerSum += (faceCenter + normal * normalFactor);
                centerCount += 1;

                //centerSum += mesh.vertices[triangles[j + 0]];
                //centerSum += mesh.vertices[triangles[j + 1]];
                //centerSum += mesh.vertices[triangles[j + 2]];
                //centerCount += 3;
            }

            var center = (centerSum / centerCount);
            for (int j = 0; j < triangles.Length; j += 3)
            {
                var faceCenter = (mesh.vertices[triangles[j + 0]] + mesh.vertices[triangles[j + 1]] + mesh.vertices[triangles[j + 2]]) / 3;
                var normal = flipNormal * Vector3.Cross(mesh.vertices[triangles[j + 1]] - mesh.vertices[triangles[j + 0]], mesh.vertices[triangles[j + 2]] - mesh.vertices[triangles[j + 0]]);
                if (Vector3.Dot(normal, faceCenter - center) < 0)
                {
                    var t = triangles[j + 2];
                    triangles[j + 2] = triangles[j];
                    triangles[j] = t;
                }
            }
            mesh.SetTriangles(triangles, i);
        }
    }

    public static void RunGeneratorsPreBake(BakeType type)
    {
        var assetGenerators = GameObject.FindObjectsOfType<BaseAssetGenerator>();
        foreach (var assetGenerator in assetGenerators)
        {
            assetGenerator.Generate();
            assetGenerator.OnPreBake(type);
        }
    }

    public static void RunGeneratorsPostBake(BakeType type)
    {
        var assetGenerators = GameObject.FindObjectsOfType<BaseAssetGenerator>();
        foreach (var assetGenerator in assetGenerators)
        {
            assetGenerator.OnPostBake(type);
        }
    }

    public static void RunColliderOcclusionPreBake()
    {
        // get instanced collision
        var instancedColliders = new List<IInstancedCollider>();
        instancedColliders.AddRange(GameObject.FindObjectsOfType<Tie>(includeInactive: false) ?? new Tie[0]);
        instancedColliders.AddRange(GameObject.FindObjectsOfType<Shrub>(includeInactive: false) ?? new Shrub[0]);
        instancedColliders.AddRange(GameObject.FindObjectsOfType<InstancedMeshCollider>(includeInactive: false) ?? new InstancedMeshCollider[0]);
        instancedColliders.AddRange(GameObject.FindObjectsOfType<UnityColliderToInstancedCollider>(includeInactive: false) ?? new UnityColliderToInstancedCollider[0]);

        foreach (var instancedCollider in instancedColliders)
        {
            instancedCollider.GetInstancedCollider()?.OnOcclusionPreBake();
        }
    }

    public static void RunColliderOcclusionPostBake()
    {
        // get instanced collision
        var instancedColliders = new List<IInstancedCollider>();
        instancedColliders.AddRange(GameObject.FindObjectsOfType<Tie>(includeInactive: false) ?? new Tie[0]);
        instancedColliders.AddRange(GameObject.FindObjectsOfType<Shrub>(includeInactive: false) ?? new Shrub[0]);
        instancedColliders.AddRange(GameObject.FindObjectsOfType<InstancedMeshCollider>(includeInactive: false) ?? new InstancedMeshCollider[0]);
        instancedColliders.AddRange(GameObject.FindObjectsOfType<UnityColliderToInstancedCollider>(includeInactive: false) ?? new UnityColliderToInstancedCollider[0]);

        foreach (var instancedCollider in instancedColliders)
        {
            instancedCollider.GetInstancedCollider()?.OnOcclusionPostBake();
        }
    }
}

public enum TextureSize
{
    _32,
    _64,
    _128,
    _256,
    _512,
    _1024
}

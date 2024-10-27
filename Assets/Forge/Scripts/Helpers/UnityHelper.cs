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
    public static Texture2D DefaultTexture => _defaultTexture ? _defaultTexture : (_defaultTexture = new Texture2D(32, 32, TextureFormat.ARGB32, false));

    private static readonly Dictionary<string, long> PAD_MASK_OPTIONS = new Dictionary<string, long>()
    {
        { "Up", 0x0010 },
        { "Right", 0x0020 },
        { "Down", 0x0040 },
        { "Left", 0x0080 },

        { "Start", 0x0008 },
        { "Select", 0x0001 },

        { "L3", 0x0002 },
        { "R3", 0x0004 },
        { "L2", 0x0100 },
        { "R2", 0x0200 },
        { "L1", 0x0400 },
        { "R1", 0x0800 },

        { "Triangle", 0x1000 },
        { "Circle", 0x2000 },
        { "Cross", 0x4000 },
        { "Square", 0x8000 },
    };

    private static readonly Dictionary<string, long> ALIGNMENT_OPTIONS = new Dictionary<string, long>()
    {
        { "Top Left", 0 },
        { "Top Center", 1 },
        { "Top Right", 2 },
        { "Middle Left", 3 },
        { "Middle Center", 4 },
        { "Middle Right", 5 },
        { "Bottom Left", 6 },
        { "Bottom Center", 7 },
        { "Bottom Right", 8 },
    };

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

    public static void ByteArrayPropertyField(SerializedProperty property, bool alwaysExpanded = false)
    {
        EditorGUI.BeginDisabledGroup(!property.editable);
        if (!alwaysExpanded) property.isExpanded = alwaysExpanded || EditorGUILayout.BeginFoldoutHeaderGroup(property.isExpanded, property.displayName);
        if (alwaysExpanded || property.isExpanded)
        {
            GUILayout.BeginVertical();

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
        public SerializedProperty CuboidRefs { get; set; }
        public SerializedProperty MobyRefs { get; set; }
        public SerializedProperty SplineRefs { get; set; }
        public SerializedProperty AreaRefs { get; set; }
        public SerializedProperty PathGraphRefs { get; set; }
        public SerializedProperty Strings { get; set; }
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
                        PVarsPropertyField_OverlayField(pvarOverlay, properties, pvarObject, def);
                    }
                }
                catch (Exception ex) { Debug.LogError(ex); }

                GUILayout.EndVertical();

                // show byte editor
                if (pvarOverlay.ShowRawEditor)
                    ByteArrayPropertyField(properties.PVars, alwaysExpanded: true);
            }
            EditorGUILayout.EndFoldoutHeaderGroup();
            EditorGUI.EndDisabledGroup();
        }
        else if (showRawEditorIfNoOverlay)
        {
            ByteArrayPropertyField(properties.PVars, alwaysExpanded: alwaysExpanded);
        }
    }

    public static void ValidatePVars(MapConfig mapConfig, IPVarObject pvarObject)
    {
        if (!mapConfig) return;

        var pvarData = pvarObject.GetPVarData();
        var pvarOverlay = pvarObject.GetPVarOverlay();

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
                    InitializePVarField(mapConfig, pvarOverlay, pvarObject, def);
                }
            }
            catch (Exception ex) { Debug.LogError(ex); }
        }
    }

    public static void UpdatePVars(MapConfig mapConfig, IPVarObject pvarObject, int racVersion)
    {
        if (!mapConfig) return;

        var cuboids = mapConfig.GetCuboids();
        var splines = mapConfig.GetSplines();
        var mobys = mapConfig.GetMobys(racVersion);
        var areas = mapConfig.GetAreas();
        var pathGraphs = mapConfig.GetPathGraphs();
        var pvars = pvarObject.GetPVarData();

        // update reference types to index
        var pvarOverlay = pvarObject.GetPVarOverlay();
        if (pvarOverlay != null && pvarOverlay.Overlay.Any())
        {
            try
            {
                foreach (var def in pvarOverlay.Overlay)
                {
                    UpdatePVar(pvarOverlay, pvarObject, pvars, cuboids, splines, mobys, areas, pathGraphs, def);
                }
            }
            catch (Exception ex) { Debug.LogError(ex); }
        }
    }

    private static void UpdatePVar(PvarOverlay pvarOverlay, IPVarObject pvarObject, byte[] pvars, Cuboid[] cuboids, Spline[] splines, Moby[] mobys, Area[] areas, PathGraph[] pathGraphs, PvarOverlayDef def, int defOffsetAdditive = 0)
    {
        var count = def.Count ?? 1;
        var offset = def.Offset + defOffsetAdditive;
        var dataSize = def.GetDataSize();

        var display = def.DisplayIf == null || def.DisplayIf.All(x => x.IsMatch(pvarOverlay, pvarObject, def, defOffsetAdditive));
        if (!display) return;


        switch (def.DataType?.ToLower())
        {
            case "cuboidref":
                {
                    for (int i = 0; i < count; ++i)
                    {
                        var refIdx = (offset / 4) + i;
                        var refObj = pvarObject.GetPVarCuboidRefs()?.ElementAtOrDefault(refIdx);
                        var value = -1;
                        if (refObj)
                            value = Array.IndexOf(cuboids, refObj);

                        Array.Copy(BitConverter.GetBytes(value), 0, pvars, offset + (i * 4), 4);
                    }
                    break;
                }
            case "splineref":
                {
                    for (int i = 0; i < count; ++i)
                    {
                        var refIdx = (offset / 4) + i;
                        var refObj = pvarObject.GetPVarSplineRefs()?.ElementAtOrDefault(refIdx);
                        var value = -1;
                        if (refObj)
                            value = Array.IndexOf(splines, refObj);

                        Array.Copy(BitConverter.GetBytes(value), 0, pvars, offset + (i * 4), 4);
                    }
                    break;
                }
            case "arearef":
                {
                    for (int i = 0; i < count; ++i)
                    {
                        var refIdx = (offset / 4) + i;
                        var refObj = pvarObject.GetPVarAreaRefs()?.ElementAtOrDefault(refIdx);
                        var value = -1;
                        if (refObj)
                            value = Array.IndexOf(areas, refObj);

                        Array.Copy(BitConverter.GetBytes(value), 0, pvars, offset + (i * 4), 4);
                    }
                    break;
                }
            case "mobyref":
                {
                    for (int i = 0; i < count; ++i)
                    {
                        var refIdx = (offset / 4) + i;
                        var refObj = pvarObject.GetPVarMobyRefs()?.ElementAtOrDefault(refIdx);
                        var value = -1;
                        if (refObj)
                            value = Array.IndexOf(mobys, refObj);

                        Array.Copy(BitConverter.GetBytes(value), 0, pvars, offset + (i * 4), 4);
                    }
                    break;
                }
            case "pathgraphref":
                {
                    for (int i = 0; i < count; ++i)
                    {
                        var refIdx = (offset / 4) + i;
                        var refObj = pvarObject.GetPVarPathGraphRefs()?.ElementAtOrDefault(refIdx);
                        var value = -1;
                        if (refObj)
                            value = Array.IndexOf(pathGraphs, refObj);

                        Array.Copy(BitConverter.GetBytes(value), 0, pvars, offset + (i * 4), 4);
                    }
                    break;
                }
            case "messagecontainer":
            case "varstringcontainer":
                {
                    var strings = pvarObject.GetPVarStrings().Select(x => BinaryHelper.StrToRatchetStr(x)).ToArray();
                    var totalSize = 4 + (strings.Length * 8);
                    foreach (var str in strings)
                        totalSize += str.Length + 1;

                    // write strings
                    var defOffset = offset + 4;
                    var strOffset = offset + 4 + (strings.Length * 8);
                    for (int i = 0; i < strings.Length; ++i)
                    {
                        var str = strings[i] + "\0";
                        Array.Copy(BitConverter.GetBytes((short)strings[i].Length), 0, pvars, defOffset, 2);
                        Array.Copy(BitConverter.GetBytes(strOffset), 0, pvars, defOffset + 4, 4);
                        Array.Copy(Encoding.ASCII.GetBytes(str), 0, pvars, strOffset, str.Length);

                        strOffset += str.Length;
                        defOffset += 8;
                    }
                    break;
                }
            case "struct":
                {
                    if (def.Fields != null)
                    {
                        for (int i = 0; i < count; ++i)
                        {
                            foreach (var structDef in def.Fields)
                            {
                                UpdatePVar(pvarOverlay, pvarObject, pvars, cuboids, splines, mobys, areas, pathGraphs, structDef, offset + (i * dataSize));
                            }
                        }
                    }
                    break;
                }
        }
    }

    private static void InitializePVarField(MapConfig mapConfig, PvarOverlay pvarOverlay, IPVarObject pvarObject, PvarOverlayDef def, int defOffsetAdditive = 0)
    {
        var pvars = pvarObject.GetPVarData();
        var mobyRefs = pvarObject.GetPVarMobyRefs();
        var cuboidRefs = pvarObject.GetPVarCuboidRefs();
        var areaRefs = pvarObject.GetPVarAreaRefs();
        var splineRefs = pvarObject.GetPVarSplineRefs();
        var pathGraphRefs = pvarObject.GetPVarPathGraphRefs();
        var strings = pvarObject.GetPVarStrings();

        var count = def.Count ?? 1;
        var dataSize = def.GetDataSize();
        var offset = def.Offset + defOffsetAdditive;

        var display = def.DisplayIf == null || def.DisplayIf.All(x => x.IsMatch(pvarOverlay, pvarObject, def, defOffsetAdditive));
        if (!display) return;

        switch (def.DataType?.ToLower())
        {
            case "cuboidref":
                {
                    // initialize first value if array doesn't fit
                    // this should only occur on newly imported mobys
                    for (int i = 0; i < count; ++i)
                    {
                        var refIdx = (offset / 4) + i;
                        if (cuboidRefs == null || refIdx >= cuboidRefs.Length)
                        {
                            // increase array size
                            if (cuboidRefs == null)
                                cuboidRefs = new Cuboid[refIdx + 1];
                            else
                                Array.Resize(ref cuboidRefs, refIdx + 1);

                            cuboidRefs[refIdx] = null;

                            // find init value
                            var cuboidIdx = BitConverter.ToInt32(pvars, offset + (i * 4));
                            if (cuboidIdx >= 0)
                            {
                                var cuboid = mapConfig.GetCuboidAtIndex(cuboidIdx);
                                if (cuboid)
                                {
                                    cuboidRefs[refIdx] = cuboid;
                                }
                            }
                        }
                    }

                    pvarObject.SetPVarCuboidRefs(cuboidRefs);
                    break;
                }
            case "splineref":
                {
                    // initialize first value if array doesn't fit
                    // this should only occur on newly imported mobys
                    for (int i = 0; i < count; ++i)
                    {
                        var refIdx = (offset / 4) + i;
                        if (splineRefs == null || refIdx >= splineRefs.Length)
                        {
                            // increase array size
                            if (splineRefs == null)
                                splineRefs = new Spline[refIdx + 1];
                            else
                                Array.Resize(ref splineRefs, refIdx + 1);

                            splineRefs[refIdx] = null;

                            // find init value
                            var splineIdx = BitConverter.ToInt32(pvars, offset + (i * 4));
                            if (splineIdx >= 0)
                            {
                                var spline = mapConfig.GetSplineAtIndex(splineIdx);
                                if (spline)
                                {
                                    splineRefs[refIdx] = spline;
                                }
                            }
                        }
                    }

                    pvarObject.SetPVarSplineRefs(splineRefs);
                    break;
                }
            case "arearef":
                {
                    // initialize first value if array doesn't fit
                    // this should only occur on newly imported mobys
                    for (int i = 0; i < count; ++i)
                    {
                        var refIdx = (offset / 4) + i;
                        if (areaRefs == null || refIdx >= areaRefs.Length)
                        {
                            // increase array size
                            if (areaRefs == null)
                                areaRefs = new Area[refIdx + 1];
                            else
                                Array.Resize(ref areaRefs, refIdx + 1);

                            areaRefs[refIdx] = null;

                            // find init value
                            var areaIdx = BitConverter.ToInt32(pvars, offset + (i * 4));
                            if (areaIdx >= 0)
                            {
                                var area = mapConfig.GetAreaAtIndex(areaIdx);
                                if (area)
                                {
                                    areaRefs[refIdx] = area;
                                }
                            }
                        }
                    }

                    pvarObject.SetPVarAreaRefs(areaRefs);
                    break;
                }
            case "mobyref":
                {
                    // initialize first value if array doesn't fit
                    // this should only occur on newly imported mobys
                    for (int i = 0; i < count; ++i)
                    {
                        var refIdx = (offset / 4) + i;
                        if (mobyRefs == null || refIdx >= mobyRefs.Length)
                        {
                            // increase array size
                            if (mobyRefs == null)
                                mobyRefs = new Moby[refIdx + 1];
                            else
                                Array.Resize(ref mobyRefs, refIdx + 1);

                            mobyRefs[refIdx] = null;

                            // find init value
                            var mobyIdx = BitConverter.ToInt32(pvars, offset + (i * 4));
                            if (mobyIdx >= 0)
                            {
                                var mobyRef = mapConfig.GetMobyAtIndex(pvarObject.GetRCVersion(), mobyIdx);
                                if (mobyRef)
                                {
                                    mobyRefs[refIdx] = mobyRef;
                                }
                            }
                        }
                    }

                    pvarObject.SetPVarMobyRefs(mobyRefs);
                    break;
                }
            case "pathgraphref":
                {
                    // initialize first value if array doesn't fit
                    // this should only occur on newly imported pathgraphs
                    for (int i = 0; i < count; ++i)
                    {
                        var refIdx = (offset / 4) + i;
                        if (pathGraphRefs == null || refIdx >= pathGraphRefs.Length)
                        {
                            // increase array size
                            if (pathGraphRefs == null)
                                pathGraphRefs = new PathGraph[refIdx + 1];
                            else
                                Array.Resize(ref pathGraphRefs, refIdx + 1);

                            pathGraphRefs[refIdx] = null;

                            // find init value
                            var pathGraphIdx = BitConverter.ToInt32(pvars, offset + (i * 4));
                            if (pathGraphIdx >= 0)
                            {
                                var pathGraphRef = mapConfig.GetPathGraphAtIndex(pathGraphIdx);
                                if (pathGraphRef)
                                {
                                    pathGraphRefs[refIdx] = pathGraphRef;
                                }
                            }
                        }
                    }

                    pvarObject.SetPVarPathGraphRefs(pathGraphRefs);
                    break;
                }
            case "messagecontainer":
            case "varstringcontainer":
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
                    break;
                }
            case "struct":
                {
                    if (def.Fields != null)
                    {
                        for (int i = 0; i < count; ++i)
                        {
                            foreach (var structDef in def.Fields)
                            {
                                InitializePVarField(mapConfig, pvarOverlay, pvarObject, structDef, offset + (i * dataSize));
                            }
                        }
                    }
                    break;
                }
        }
    }

    private static void PVarsPropertyField_OverlayField(PvarOverlay pvarOverlay, PVarsPropertiesContainer properties, IPVarObject pvarObject, PvarOverlayDef def, int defOffsetAdditive = 0)
    {
        var count = def.Count ?? 1;
        var dataSize = def.GetDataSize();
        var baseOffset = def.Offset + defOffsetAdditive;
        var display = def.DisplayIf == null || def.DisplayIf.All(x => x.IsMatch(pvarOverlay, pvarObject, def, defOffsetAdditive));
        if (!display) return;

        switch (def.DataType?.ToLower())
        {
            case "bool":
                {
                    Draw(properties, def, baseOffset, (offset, name) =>
                    {
                        // read value
                        PVarsPropertyField_ReadPVarData(properties, PVarsPropertyField_Buffer, offset, dataSize);
                        var value = PVarsPropertyField_Buffer[0] != 0;
                        EditorGUI.BeginChangeCheck();
                        value = EditorGUILayout.Toggle(new GUIContent(name, def.Tooltip), value);
                        if (EditorGUI.EndChangeCheck())
                        {
                            PVarsPropertyField_Buffer[0] = (byte)(value ? 1 : 0);
                            PVarsPropertyField_WritePVarData(properties, PVarsPropertyField_Buffer, offset, dataSize);
                        }
                    });
                    break;
                }
            case "byte":
                {
                    Draw(properties, def, baseOffset, (offset, name) =>
                    {
                        // read value
                        PVarsPropertyField_ReadPVarData(properties, PVarsPropertyField_Buffer, offset, dataSize);
                        var value = (int)PVarsPropertyField_Buffer[0];
                        EditorGUI.BeginChangeCheck();
                        value = EditorGUILayout.IntField(new GUIContent(name, def.Tooltip), value);
                        if (EditorGUI.EndChangeCheck())
                        {
                            if (value < def.Min) value = (int)def.Min;
                            if (value > def.Max) value = (int)def.Max;
                            if (value > byte.MaxValue) value = byte.MaxValue;
                            if (value < byte.MinValue) value = byte.MinValue;
                            PVarsPropertyField_Buffer[0] = (byte)value;
                            PVarsPropertyField_WritePVarData(properties, PVarsPropertyField_Buffer, offset, dataSize);
                        }
                    });
                    break;
                }
            case "sbyte":
                {
                    Draw(properties, def, baseOffset, (offset, name) =>
                    {
                        // read value
                        PVarsPropertyField_ReadPVarData(properties, PVarsPropertyField_Buffer, offset, dataSize);
                        var value = (int)(sbyte)PVarsPropertyField_Buffer[0];
                        EditorGUI.BeginChangeCheck();
                        value = EditorGUILayout.IntField(new GUIContent(name, def.Tooltip), value);
                        if (EditorGUI.EndChangeCheck())
                        {
                            if (value < def.Min) value = (int)def.Min;
                            if (value > def.Max) value = (int)def.Max;
                            if (value > sbyte.MaxValue) value = sbyte.MaxValue;
                            if (value < sbyte.MinValue) value = sbyte.MinValue;
                            PVarsPropertyField_Buffer[0] = (byte)(sbyte)value;
                            PVarsPropertyField_WritePVarData(properties, PVarsPropertyField_Buffer, offset, dataSize);
                        }
                    });
                    break;
                }
            case "integer":
                {
                    Draw(properties, def, baseOffset, (offset, name) =>
                    {
                        // read value
                        PVarsPropertyField_ReadPVarData(properties, PVarsPropertyField_Buffer, offset, dataSize);
                        var value = BitConverter.ToInt32(PVarsPropertyField_Buffer, 0);
                        EditorGUI.BeginChangeCheck();
                        value = EditorGUILayout.IntField(new GUIContent(name, def.Tooltip), value);
                        if (EditorGUI.EndChangeCheck())
                        {
                            if (value < def.Min) value = (int)def.Min;
                            if (value > def.Max) value = (int)def.Max;
                            var b = BitConverter.GetBytes(value);
                            PVarsPropertyField_WritePVarData(properties, b, offset, dataSize);
                        }
                    });
                    break;
                }
            case "float":
                {
                    Draw(properties, def, baseOffset, (offset, name) =>
                    {
                        // read value
                        PVarsPropertyField_ReadPVarData(properties, PVarsPropertyField_Buffer, offset, dataSize);
                        var value = BitConverter.ToSingle(PVarsPropertyField_Buffer, 0);
                        EditorGUI.BeginChangeCheck();
                        value = EditorGUILayout.FloatField(new GUIContent(name, def.Tooltip), value);
                        if (EditorGUI.EndChangeCheck())
                        {
                            if (value < def.Min) value = (float)def.Min;
                            if (value > def.Max) value = (float)def.Max;
                            var b = BitConverter.GetBytes(value);
                            PVarsPropertyField_WritePVarData(properties, b, offset, dataSize);
                        }
                    });
                    break;
                }
            case "vector2":
                {
                    // read value
                    PVarsPropertyField_ReadPVarData(properties, PVarsPropertyField_Buffer, baseOffset, 8);
                    var value = new Vector2(BitConverter.ToSingle(PVarsPropertyField_Buffer, 0), BitConverter.ToSingle(PVarsPropertyField_Buffer, 4));
                    EditorGUI.BeginChangeCheck();
                    value = EditorGUILayout.Vector2Field(new GUIContent(def.Name, def.Tooltip), value);
                    if (EditorGUI.EndChangeCheck())
                    {
                        //if (value < def.Min) value = (float)def.Min;
                        //if (value > def.Max) value = (float)def.Max;
                        PVarsPropertyField_WritePVarData(properties, BitConverter.GetBytes(value.x), baseOffset, 4);
                        PVarsPropertyField_WritePVarData(properties, BitConverter.GetBytes(value.y), baseOffset + 4, 4);
                    }
                    break;
                }
            case "vector3":
                {
                    // read value
                    PVarsPropertyField_ReadPVarData(properties, PVarsPropertyField_Buffer, baseOffset, 12);
                    var value = new Vector3(BitConverter.ToSingle(PVarsPropertyField_Buffer, 0), BitConverter.ToSingle(PVarsPropertyField_Buffer, 4), BitConverter.ToSingle(PVarsPropertyField_Buffer, 8));
                    EditorGUI.BeginChangeCheck();
                    value = EditorGUILayout.Vector3Field(new GUIContent(def.Name, def.Tooltip), value);
                    if (EditorGUI.EndChangeCheck())
                    {
                        //if (value < def.Min) value = (float)def.Min;
                        //if (value > def.Max) value = (float)def.Max;
                        PVarsPropertyField_WritePVarData(properties, BitConverter.GetBytes(value.x), baseOffset, 4);
                        PVarsPropertyField_WritePVarData(properties, BitConverter.GetBytes(value.y), baseOffset + 4, 4);
                        PVarsPropertyField_WritePVarData(properties, BitConverter.GetBytes(value.z), baseOffset + 8, 4);
                    }
                    break;
                }
            case "screenposition":
                {
                    // read value
                    PVarsPropertyField_ReadPVarData(properties, PVarsPropertyField_Buffer, baseOffset, 4);
                    var value = new Vector2(BitConverter.ToInt16(PVarsPropertyField_Buffer, 0), BitConverter.ToInt16(PVarsPropertyField_Buffer, 2));
                    EditorGUI.BeginChangeCheck();
                    value = EditorGUILayout.Vector2Field(new GUIContent(def.Name, def.Tooltip), value);
                    if (EditorGUI.EndChangeCheck())
                    {
                        //if (value < def.Min) value = (float)def.Min;
                        //if (value > def.Max) value = (float)def.Max;
                        PVarsPropertyField_WritePVarData(properties, BitConverter.GetBytes((short)Mathf.Clamp(value.x, 0, 512)), baseOffset, 2);
                        PVarsPropertyField_WritePVarData(properties, BitConverter.GetBytes((short)Mathf.Clamp(value.y, 0, 416)), baseOffset + 2, 2);
                    }

                    break;
                }
            case "colorrgb":
                {
                    // read value
                    PVarsPropertyField_ReadPVarData(properties, PVarsPropertyField_Buffer, baseOffset, 3);
                    var value = new Color32(PVarsPropertyField_Buffer[0], PVarsPropertyField_Buffer[1], PVarsPropertyField_Buffer[2], 255);
                    EditorGUI.BeginChangeCheck();
                    value = EditorGUILayout.ColorField(new GUIContent(def.Name, def.Tooltip), value, showEyedropper: true, showAlpha: false, hdr: false);
                    if (EditorGUI.EndChangeCheck())
                    {
                        PVarsPropertyField_Buffer[0] = value.r;
                        PVarsPropertyField_Buffer[1] = value.g;
                        PVarsPropertyField_Buffer[2] = value.b;
                        PVarsPropertyField_WritePVarData(properties, PVarsPropertyField_Buffer, baseOffset, 3);
                    }
                    break;
                }
            case "colorrgba":
                {
                    // read value
                    PVarsPropertyField_ReadPVarData(properties, PVarsPropertyField_Buffer, baseOffset, 4);
                    var value = new Color32(PVarsPropertyField_Buffer[0], PVarsPropertyField_Buffer[1], PVarsPropertyField_Buffer[2], PVarsPropertyField_Buffer[3]);
                    EditorGUI.BeginChangeCheck();
                    value = EditorGUILayout.ColorField(new GUIContent(def.Name, def.Tooltip), value);
                    if (EditorGUI.EndChangeCheck())
                    {
                        PVarsPropertyField_Buffer[0] = value.r;
                        PVarsPropertyField_Buffer[1] = value.g;
                        PVarsPropertyField_Buffer[2] = value.b;
                        PVarsPropertyField_Buffer[3] = value.a;
                        PVarsPropertyField_WritePVarData(properties, PVarsPropertyField_Buffer, baseOffset, 4);
                    }
                    break;
                }
            case "team":
                {
                    // read value
                    PVarsPropertyField_ReadPVarData(properties, PVarsPropertyField_Buffer, baseOffset, def.DataSize ?? 1);
                    var value = (DLTeamIds)PVarsPropertyField_Buffer[0];
                    EditorGUI.BeginChangeCheck();
                    value = PVarsPropertyField_EnumPopup(new GUIContent(def.Name, def.Tooltip), value, def.Min, def.Max);
                    if (EditorGUI.EndChangeCheck())
                    {
                        var b = BitConverter.GetBytes((int)value);
                        PVarsPropertyField_WritePVarData(properties, b, baseOffset, def.DataSize ?? 1);
                    }
                    break;
                }
            case "fxtex":
                {
                    // read value
                    PVarsPropertyField_ReadPVarData(properties, PVarsPropertyField_Buffer, baseOffset, 4);
                    var value = (DLFXTextureIds)BitConverter.ToInt32(PVarsPropertyField_Buffer, 0);
                    EditorGUI.BeginChangeCheck();
                    value = PVarsPropertyField_EnumPopup(new GUIContent(def.Name, def.Tooltip), value, def.Min, def.Max);
                    if (EditorGUI.EndChangeCheck())
                    {
                        var b = BitConverter.GetBytes((int)value);
                        PVarsPropertyField_WritePVarData(properties, b, baseOffset, 4);
                    }
                    break;
                }
            case "levelfxtex":
                {
                    // read value
                    PVarsPropertyField_ReadPVarData(properties, PVarsPropertyField_Buffer, baseOffset, 4);
                    var value = (DLLevelFXTextureIds)BitConverter.ToInt32(PVarsPropertyField_Buffer, 0);
                    EditorGUI.BeginChangeCheck();
                    value = PVarsPropertyField_EnumPopup(new GUIContent(def.Name, def.Tooltip), value, def.Min, def.Max);
                    if (EditorGUI.EndChangeCheck())
                    {
                        var b = BitConverter.GetBytes((int)value);
                        PVarsPropertyField_WritePVarData(properties, b, baseOffset, 4);
                    }
                    break;
                }
            case "enum":
                {
                    Draw(properties, def, baseOffset, (offset, name) =>
                    {
                        // read value
                        PVarsPropertyField_ReadPVarData(properties, PVarsPropertyField_Buffer, offset, dataSize);
                        var value = (long)(BitConverter.ToInt64(PVarsPropertyField_Buffer, 0) & (long)(Math.Pow(2, dataSize * 8) - 1));
                        EditorGUI.BeginChangeCheck();
                        value = PVarsPropertyField_EnumPopup(new GUIContent(name, def.Tooltip), value, def.Options, dataSize);
                        if (EditorGUI.EndChangeCheck())
                        {
                            var b = BitConverter.GetBytes(value);
                            PVarsPropertyField_WritePVarData(properties, b, offset, dataSize);
                        }
                    });
                    break;
                }
            case "alignment":
                {
                    Draw(properties, def, baseOffset, (offset, name) =>
                    {
                        // read value
                        PVarsPropertyField_ReadPVarData(properties, PVarsPropertyField_Buffer, offset, dataSize);
                        var value = (long)(BitConverter.ToInt64(PVarsPropertyField_Buffer, 0) & (long)(Math.Pow(2, dataSize * 8) - 1));
                        EditorGUI.BeginChangeCheck();
                        value = PVarsPropertyField_EnumPopup(new GUIContent(name, def.Tooltip), value, ALIGNMENT_OPTIONS, dataSize);
                        if (EditorGUI.EndChangeCheck())
                        {
                            var b = BitConverter.GetBytes(value);
                            PVarsPropertyField_WritePVarData(properties, b, offset, dataSize);
                        }
                    });
                    break;
                }
            case "mask":
                {
                    Draw(properties, def, baseOffset, (offset, name) =>
                    {
                        // read value
                        PVarsPropertyField_ReadPVarData(properties, PVarsPropertyField_Buffer, offset, dataSize);
                        var value = (long)(BitConverter.ToInt64(PVarsPropertyField_Buffer, 0) & (long)(Math.Pow(2, dataSize * 8) - 1));
                        EditorGUI.BeginChangeCheck();
                        value = PVarsPropertyField_MaskPopup(new GUIContent(name, def.Tooltip), value, def.Options, dataSize);
                        if (EditorGUI.EndChangeCheck())
                        {
                            var b = BitConverter.GetBytes(value);
                            PVarsPropertyField_WritePVarData(properties, b, offset, dataSize);
                        }
                    });
                    break;
                }
            case "padmask":
                {
                    Draw(properties, def, baseOffset, (offset, name) =>
                    {
                        // read value
                        PVarsPropertyField_ReadPVarData(properties, PVarsPropertyField_Buffer, offset, dataSize);
                        var value = (long)(BitConverter.ToInt64(PVarsPropertyField_Buffer, 0) & (long)(Math.Pow(2, dataSize * 8) - 1));
                        EditorGUI.BeginChangeCheck();
                        value = PVarsPropertyField_MaskPopup(new GUIContent(name, def.Tooltip), value, PAD_MASK_OPTIONS, dataSize);
                        if (EditorGUI.EndChangeCheck())
                        {
                            var b = BitConverter.GetBytes(value);
                            PVarsPropertyField_WritePVarData(properties, b, offset, dataSize);
                        }
                    });
                    break;
                }
            case "mobygroupid":
                {
                    // read value
                    PVarsPropertyField_ReadPVarData(properties, PVarsPropertyField_Buffer, baseOffset, 4);
                    var value = BitConverter.ToInt32(PVarsPropertyField_Buffer, 0);
                    EditorGUI.BeginChangeCheck();
                    value = EditorGUILayout.IntField(new GUIContent(def.Name, def.Tooltip), value);
                    if (EditorGUI.EndChangeCheck())
                    {
                        if (value < def.Min) value = (int)def.Min;
                        if (value > def.Max) value = (int)def.Max;
                        var b = BitConverter.GetBytes(value);
                        PVarsPropertyField_WritePVarData(properties, b, baseOffset, 4);
                    }
                    break;
                }
            case "tiegroupid":
                {
                    // read value
                    PVarsPropertyField_ReadPVarData(properties, PVarsPropertyField_Buffer, baseOffset, 4);
                    var value = BitConverter.ToInt32( PVarsPropertyField_Buffer, 0);
                    EditorGUI.BeginChangeCheck();
                    value = EditorGUILayout.IntField(new GUIContent(def.Name, def.Tooltip), value);
                    if (EditorGUI.EndChangeCheck())
                    {
                        if (value < def.Min) value = (int)def.Min;
                        if (value > def.Max) value = (int)def.Max;
                        var b = BitConverter.GetBytes(value);
                        PVarsPropertyField_WritePVarData(properties, b, baseOffset, 4);
                    }
                    break;
                }
            case "mobyrefstate":
                {
                    Draw(properties, def, baseOffset, (offset, name) =>
                    {
                        Dictionary<string, long> stateOptions = null;

                        // find reference field
                        // check if mobyref
                        // grab moby
                        (PvarOverlayDef refField, int refFieldParentOffset) = def.FindFieldFrom(pvarOverlay, def.Ref, offset);
                        if (refField != null && refField.DataType?.ToLower() == "mobyref")
                        {
                            var mobyRefs = pvarObject.GetPVarMobyRefs();
                            var mobyRef = mobyRefs.ElementAtOrDefault((refField.Offset + refFieldParentOffset) / 4);
                            if (mobyRef)
                            {
                                var mobyRefPvarOverlay = PvarOverlay.GetPvarOverlay(mobyRef.RCVersion, mobyClass: mobyRef.OClass);
                                if (mobyRefPvarOverlay != null)
                                {
                                    stateOptions = mobyRefPvarOverlay.States;
                                }
                            }
                        }

                        if (stateOptions != null && stateOptions.Any())
                        {
                            // read value
                            PVarsPropertyField_ReadPVarData(properties, PVarsPropertyField_Buffer, offset, dataSize);
                            var value = (long)(BitConverter.ToInt64(PVarsPropertyField_Buffer, 0) & (long)(Math.Pow(2, dataSize * 8) - 1));
                            EditorGUI.BeginChangeCheck();
                            value = PVarsPropertyField_EnumPopup(new GUIContent(name, def.Tooltip), value, stateOptions, dataSize);
                            if (EditorGUI.EndChangeCheck())
                            {
                                if (value < -1) value = -1;
                                if (value > 127) value = 127;
                                var b = BitConverter.GetBytes(value);
                                PVarsPropertyField_WritePVarData(properties, b, offset, dataSize);
                            }
                        }
                        else
                        {
                            // read value
                            PVarsPropertyField_ReadPVarData(properties, PVarsPropertyField_Buffer, offset, dataSize);
                            var value = BitConverter.ToInt32(PVarsPropertyField_Buffer, 0);
                            EditorGUI.BeginChangeCheck();
                            value = EditorGUILayout.IntField(new GUIContent(name, def.Tooltip), value);
                            if (EditorGUI.EndChangeCheck())
                            {
                                if (value < def.Min) value = (int)def.Min;
                                if (value > def.Max) value = (int)def.Max;
                                if (value < -1) value = -1;
                                if (value > 127) value = 127;
                                var b = BitConverter.GetBytes(value);
                                PVarsPropertyField_WritePVarData(properties, b, offset, dataSize);
                            }
                        }
                    });
                    break;
                }
            case "cuboidref":
                {
                    Draw(properties, def, baseOffset, (offset, name) =>
                    {
                        var refIdx = offset / 4;
                        var refObj = properties.CuboidRefs.GetArrayElementAtIndex(refIdx);
                        EditorGUILayout.ObjectField(refObj, typeof(Cuboid), new GUIContent(name, def.Tooltip));
                    });
                    break;
                }
            case "splineref":
                {
                    Draw(properties, def, baseOffset, (offset, name) =>
                    {
                        var refIdx = offset / 4;
                        var refObj = properties.SplineRefs.GetArrayElementAtIndex(refIdx);
                        EditorGUILayout.ObjectField(refObj, typeof(Spline), new GUIContent(name, def.Tooltip));
                    });
                    break;
                }
            case "arearef":
                {
                    Draw(properties, def, baseOffset, (offset, name) =>
                    {
                        var refIdx = offset / 4;
                        var refObj = properties.AreaRefs.GetArrayElementAtIndex(refIdx);
                        EditorGUILayout.ObjectField(refObj, typeof(Area), new GUIContent(name, def.Tooltip));
                    });
                    break;
                }
            case "mobyref":
                {
                    Draw(properties, def, baseOffset, (offset, name) =>
                    {
                        var refIdx = offset / 4;
                        var refObj = properties.MobyRefs.GetArrayElementAtIndex(refIdx);
                        EditorGUILayout.ObjectField(refObj, typeof(Moby), new GUIContent(name, def.Tooltip));
                    });
                    break;
                }
            case "pathgraphref":
                {
                    Draw(properties, def, baseOffset, (offset, name) =>
                    {
                        var refIdx = offset / 4;
                        var refObj = properties.PathGraphRefs.GetArrayElementAtIndex(refIdx);
                        EditorGUILayout.ObjectField(refObj, typeof(PathGraph), new GUIContent(name, def.Tooltip));
                    });
                    break;
                }
            case "struct":
                {
                    Draw(properties, def, baseOffset, (offset, name) =>
                    {
                        if (def.Fields != null)
                        {
                            EditorGUILayout.PrefixLabel(new GUIContent(name, def.Tooltip));
                            EditorGUI.indentLevel++;
                            foreach (var structDef in def.Fields)
                            {
                                PVarsPropertyField_OverlayField(pvarOverlay, properties, pvarObject, structDef, defOffsetAdditive: offset);
                            }
                            EditorGUI.indentLevel--;
                        }
                    });
                    break;
                }
            case "varstring":
                {
                    DrawToggleGroup(properties, def, baseOffset, (offset, name) =>
                    {
                        // read string length
                        PVarsPropertyField_ReadPVarData(properties, PVarsPropertyField_Buffer, offset, 4);
                        var length = BitConverter.ToInt32(PVarsPropertyField_Buffer, 0);
                        if (length > PvarOverlay.VARSTRING_MAX_LENGTH)
                            length = PvarOverlay.VARSTRING_MAX_LENGTH;

                        // read string
                        PVarsPropertyField_ReadPVarData(properties, PVarsPropertyField_Buffer, offset + 4, length);
                        var str = BinaryHelper.RatchetStrToStr(Encoding.UTF8.GetString(PVarsPropertyField_Buffer, 0, length));

                        EditorGUI.BeginChangeCheck();
                        EditorGUILayout.PrefixLabel(new GUIContent(name, def.Tooltip));
                        str = EditorGUILayout.TextArea(str);
                        if (EditorGUI.EndChangeCheck())
                        {
                            str = BinaryHelper.StrToRatchetStr(str) + "\0";
                            length = str.Length;
                            var bLen = BitConverter.GetBytes(length);
                            var bStr = Encoding.UTF8.GetBytes(str);

                            // adjust pvar size
                            var writeUntil = offset + 4 + length;
                            if (properties.PVars.arraySize != writeUntil)
                                properties.PVars.arraySize = writeUntil;

                            PVarsPropertyField_WritePVarData(properties, bLen, offset, 4);
                            PVarsPropertyField_WritePVarData(properties, bStr, offset + 4, length);
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
                            var b = BitConverter.GetBytes(messageCount);
                            PVarsPropertyField_WritePVarData(properties, b, offset, 4);
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
                            var b = BitConverter.GetBytes(messageCount);
                            PVarsPropertyField_WritePVarData(properties, b, offset, 4);
                            properties.Strings.arraySize = messageCount;
                        }

                        EditorGUI.indentLevel++;
                        for (int i = 0; i < messageCount; ++i)
                        {
                            // read runtime
                            PVarsPropertyField_ReadPVarData(properties, PVarsPropertyField_Buffer, offset + 4 + (i * 8) + 2, 2);
                            int runtime = PVarsPropertyField_Buffer[0];
                            int moveTo = (sbyte)PVarsPropertyField_Buffer[1];

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
                                var bRuntimeMoveTo = new byte[] { (byte)runtime, (byte)moveTo };
                                PVarsPropertyField_WritePVarData(properties, bRuntimeMoveTo, offset + 4 + (i * 8) + 2, 2);

                                // set str
                                properties.Strings.GetArrayElementAtIndex(i).stringValue = str;
                            }
                            EditorGUI.indentLevel--;
                        }
                        EditorGUI.indentLevel--;
                    });
                    break;
                }

            // RENDER ONLY
            case "label":
                {
                    Draw(properties, def, baseOffset, (offset, name) =>
                    {
                        EditorGUILayout.Space(10);
                        EditorGUILayout.PrefixLabel(new GUIContent(name, def.Tooltip));
                    });
                    break;
                }
            case "space":
                {
                    Draw(properties, def, baseOffset, (offset, name) =>
                    {
                        EditorGUILayout.Space(10);
                    });
                    break;
                }
        }
    }

    private static void Draw(PVarsPropertiesContainer properties, PvarOverlayDef def, int offset, Action<int, string> draw)
    {
        var count = def.Count ?? 1;
        var dataSize = def.GetDataSize();
        var serializedProperty = properties.PVars.GetArrayElementAtIndex(offset);


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
                if (count > 1) name = $"[{i}]";

                draw(defOffset, name);
                
                if (count > 1)
                {
                    EditorGUILayout.BeginHorizontal();
                    EditorGUILayout.Space(0); // moves buttons to right side
                    GUI.enabled = i > 0;
                    if (GUILayout.Button("Move Up", GUILayout.Width(100)))
                    {
                        // swap
                        var b0 = new byte[dataSize];
                        var b1 = new byte[dataSize];
                        PVarsPropertyField_ReadPVarData(properties, b0, defOffset, dataSize);
                        PVarsPropertyField_ReadPVarData(properties, b1, defOffset - dataSize, dataSize);
                        PVarsPropertyField_WritePVarData(properties, b0, defOffset - dataSize, dataSize);
                        PVarsPropertyField_WritePVarData(properties, b1, defOffset, dataSize);

                        // must be 4 byte aligned for refs
                        if ((defOffset % 4) == 0 && (dataSize % 4) == 0)
                        {
                            for (int refOfs = 0; refOfs < dataSize; refOfs += 4)
                            {
                                var refIdx0 = (defOffset + refOfs) / 4;
                                var refIdx1 = (defOffset + refOfs - dataSize) / 4;

                                SerializedPropertySwap(properties.MobyRefs, refIdx0, refIdx1);
                                SerializedPropertySwap(properties.CuboidRefs, refIdx0, refIdx1);
                                SerializedPropertySwap(properties.SplineRefs, refIdx0, refIdx1);
                                SerializedPropertySwap(properties.AreaRefs, refIdx0, refIdx1);
                                SerializedPropertySwap(properties.PathGraphRefs, refIdx0, refIdx1);
                            }
                        }
                    }
                    GUI.enabled = i < (count - 1);
                    if (GUILayout.Button("Move Down", GUILayout.Width(100)))
                    {
                        // swap
                        var b0 = new byte[dataSize];
                        var b1 = new byte[dataSize];
                        PVarsPropertyField_ReadPVarData(properties, b0, defOffset, dataSize);
                        PVarsPropertyField_ReadPVarData(properties, b1, defOffset + dataSize, dataSize);
                        PVarsPropertyField_WritePVarData(properties, b0, defOffset + dataSize, dataSize);
                        PVarsPropertyField_WritePVarData(properties, b1, defOffset, dataSize);

                        // must be 4 byte aligned for refs
                        if ((defOffset % 4) == 0 && (dataSize % 4) == 0)
                        {
                            for (int refOfs = 0; refOfs < dataSize; refOfs += 4)
                            {
                                var refIdx0 = (defOffset + refOfs) / 4;
                                var refIdx1 = (defOffset + refOfs + dataSize) / 4;

                                SerializedPropertySwap(properties.MobyRefs, refIdx0, refIdx1);
                                SerializedPropertySwap(properties.CuboidRefs, refIdx0, refIdx1);
                                SerializedPropertySwap(properties.SplineRefs, refIdx0, refIdx1);
                                SerializedPropertySwap(properties.AreaRefs, refIdx0, refIdx1);
                                SerializedPropertySwap(properties.PathGraphRefs, refIdx0, refIdx1);
                            }
                        }
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

    public static void MarkActiveSceneDirty()
    {
        Dispatcher.RunOnMainThread(() =>
        {
            EditorSceneManager.MarkSceneDirty(EditorSceneManager.GetActiveScene());
        });
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

    public static List<Vector3> GetAllOctants()
    {
        var volumes = GameObject.FindObjectsOfType<OcclusionVolume>();
        var rawOctants = GameObject.FindObjectsOfType<OcclusionOctant>();
        var octants = volumes.Where(x => !x.Negate).SelectMany(x => x.GetOctants()).Union(rawOctants.SelectMany(x => x.Octants ?? new List<Vector3>())).Distinct().ToList();
        var negativeOctants = volumes.Where(x => x.Negate).ToList();
        octants.RemoveAll(x => negativeOctants.Any(o => o.Contains(x)));

        return octants;
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

            var rt = new RenderTexture(width, height, 0, RenderTextureFormat.ARGB32);
            rt.Create();
            try
            {
                var mat = new Material(AssetDatabase.LoadAssetAtPath<Material>(Path.Combine(FolderNames.ForgeFolder, "Shaders", "TintBlit.mat")));
                mat.SetColor("_Color", tint ?? Color.white);
                mat.SetTexture("_In", tex);
                mat.SetTexture("_Out", rt);
                mat.SetColor("_Alpha", hasAlpha ? Color.clear : Color.white);
                Graphics.Blit(tex, rt, mat);

                var oldRt = RenderTexture.active;
                RenderTexture.active = rt;
                var tex2 = new Texture2D(width, height, TextureFormat.ARGB32, false);
                tex2.ReadPixels(new Rect(0, 0, width, height), 0, 0);
                tex2.Apply();
                RenderTexture.active = oldRt;

                var bytes = tex2.EncodeToPNG();
                File.WriteAllBytes(path, bytes);
                return true;
            }
            finally
            {
                if (RenderTexture.active == rt)
                    RenderTexture.active = null;

                rt.Release();
            }
        }

        return false;
    }

    public static Texture2D ResizeTexture(Texture2D src, int width, int height)
    {
        if (!src) return null;

        RenderTexture rt = new RenderTexture(width, height, 24);
        RenderTexture.active = rt;
        Graphics.Blit(src, rt);
        Texture2D result = new Texture2D(width, height);
        result.ReadPixels(new Rect(0, 0, width, height), 0, 0);
        result.Apply();
        rt.Release();
        return result;
    }

    public static Texture2D CloneTexture(Texture2D src, bool hasAlpha = true, Color? tint = null)
    {
        if (!src) return null;

        var width = src.width;
        var height = src.height;
        var rt = new RenderTexture(width, height, 0, RenderTextureFormat.ARGB32);
        rt.Create();
        try
        {
            var mat = new Material(AssetDatabase.LoadAssetAtPath<Material>(Path.Combine(FolderNames.ForgeFolder, "Shaders", "TintBlit.mat")));
            mat.SetColor("_Color", tint ?? Color.white);
            mat.SetTexture("_In", src);
            mat.SetTexture("_Out", rt);
            mat.SetColor("_Alpha", hasAlpha ? Color.clear : Color.white);
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
        if (tex.imageContentsHash.isValid)
            return tex.imageContentsHash;

        if (!tex.isReadable)
            return new Hash128();

        var pixels = tex.GetPixelData<Color>(0);
        return Hash128.Compute(pixels);
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

    static int ForceDimensionPowerOfTwo(int dimension)
    {
        float exp = Mathf.Log(dimension, 2);
        if (exp == (int)exp) return dimension;

        return (int)Mathf.Pow(2, Mathf.CeilToInt(exp));
    }

    public static void Append(this Hash128 hash, Color color)
    {
        hash.Append(color.ToString());
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

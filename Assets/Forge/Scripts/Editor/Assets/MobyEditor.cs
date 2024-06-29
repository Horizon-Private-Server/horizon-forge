using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(Moby)), CanEditMultipleObjects]
public class MobyEditor : Editor
{
    private byte[] _buffer = new byte[256];
    private List<(MaterialEditor matEditor, bool canEdit)> _materialEditors = new List<(MaterialEditor, bool)>();
    private SerializedProperty _rcVersionProperty;
    private SerializedProperty _pvarData;
    private SerializedProperty _pvarCuboidRefs;
    private SerializedProperty _pvarMobyRefs;
    private SerializedProperty _pvarSplineRefs;
    private SerializedProperty _pvarAreaRefs;
    private static Moby _clipboardMoby = null;

    private bool HasOneTarget => targets == null || targets.Length == 1;
    private bool TargetsShareOClass => targets?.All(x => (x as Moby).OClass == (target as Moby).OClass) ?? false;

    private void OnEnable()
    {
        var moby = (Moby)serializedObject.targetObject;

        _rcVersionProperty = serializedObject.FindProperty("RCVersion");
        _pvarData = serializedObject.FindProperty("PVars");
        _pvarCuboidRefs = serializedObject.FindProperty("PVarCuboidRefs");
        _pvarMobyRefs = serializedObject.FindProperty("PVarMobyRefs");
        _pvarSplineRefs = serializedObject.FindProperty("PVarSplineRefs");
        _pvarAreaRefs = serializedObject.FindProperty("PVarAreaRefs");

        _materialEditors.Clear();
        var materials = moby.GetComponentsInChildren<MeshRenderer>()?.SelectMany(x => x.sharedMaterials)?.ToArray();
        if (materials != null)
        {
            // Create an instance of the default MaterialEditor
            for (int i = 0; i < materials.Length; i++)
            {
                var mat = materials[i];
                if (!mat || mat.shader.name == "Horizon Forge/Collider") continue;

                var matEditor = (MaterialEditor)CreateEditor(mat);
                var canEdit = AssetDatabase.GetAssetPath(mat).StartsWith("Assets");
                _materialEditors.Add((matEditor, canEdit));
            }
        }
    }

    public override void OnInspectorGUI()
    {
        serializedObject.Update();

        // draw oclass
        if (TargetsShareOClass)
        {
            var moby = (Moby)target;
            EditorGUI.BeginDisabledGroup(true);
            EditorGUILayout.TextField("OClass", $"{moby?.OClass} ({moby?.OClass:X4})");
            EditorGUILayout.PropertyField(_rcVersionProperty);
            EditorGUI.EndDisabledGroup();
        }

        base.OnInspectorGUI();

        // draw pvar overlay
        if (HasOneTarget)
        {
            var moby = target as Moby;
            var mapConfig = GameObject.FindObjectOfType<MapConfig>();
            if (mapConfig)
            {
                GUILayout.Space(20);
                EditorGUILayout.LabelField("PVars");

                // pvar overlay
                var pvarOverlay = PvarOverlay.GetPvarOverlay(moby.OClass, moby.RCVersion);
                if (pvarOverlay != null && pvarOverlay.Overlay.Any())
                {
                    try
                    {
                        foreach (var def in pvarOverlay.Overlay)
                        {
                            OverlayField(mapConfig, moby, def);
                        }
                    }
                    catch (Exception ex) { Debug.LogError(ex); }
                }

                // copy/paste
                GUILayout.BeginHorizontal();
                if (GUILayout.Button("Copy PVars"))
                {
                    _clipboardMoby = moby;
                }
                EditorGUI.BeginDisabledGroup(!_clipboardMoby || _clipboardMoby.OClass != moby.OClass);
                if (GUILayout.Button($"Paste PVars{(_clipboardMoby ? $" ({_clipboardMoby.name})" : "")}"))
                {
                    // copy refs
                    Undo.RecordObject(moby, "Paste PVars");
                    moby.PVarAreaRefs = _clipboardMoby.PVarAreaRefs.ToArray();
                    moby.PVarCuboidRefs = _clipboardMoby.PVarCuboidRefs.ToArray();
                    moby.PVarMobyRefs = _clipboardMoby.PVarMobyRefs.ToArray();
                    moby.PVarSplineRefs = _clipboardMoby.PVarSplineRefs.ToArray();
                    moby.PVars = _clipboardMoby.PVars.ToArray();
                }
                EditorGUI.EndDisabledGroup();
                GUILayout.EndHorizontal();
            }
        }

        if (TargetsShareOClass)
        { 
            // selection
            EditorGUILayout.Space(20);
            if (GUILayout.Button("Select children"))
            {
                var selected = Selection.objects ?? new UnityEngine.Object[0];
                foreach (var targetMoby in targets) SelectChildren(targetMoby as Moby, ref selected);
                Selection.objects = selected;
            }
            if (GUILayout.Button("Select moby children"))
            {
                var selected = Selection.objects ?? new UnityEngine.Object[0];
                foreach (var targetMoby in targets) SelectMobyChildren(targetMoby as Moby, ref selected);
                Selection.objects = selected;
            }

            // draw materials
            foreach (var matEditor in _materialEditors)
            {
                // Draw the material's foldout and the material shader field
                // Required to call _materialEditor.OnInspectorGUI ();
                matEditor.matEditor.DrawHeader();

                using (new EditorGUI.DisabledGroupScope(!matEditor.canEdit))
                {
                    // Draw the material properties
                    // Works only if the foldout of _materialEditor.DrawHeader () is open
                    matEditor.matEditor.OnInspectorGUI();
                }
            }
        }

        serializedObject.ApplyModifiedProperties();
    }

    private void OverlayField(MapConfig mapConfig, Moby moby, PvarOverlayDef def)
    {
        switch (def.DataType?.ToLower())
        {
            case "bool":
                {
                    // read value
                    ReadPVarData(_buffer, def.Offset, 1);
                    var value = _buffer[0] != 0;
                    value = EditorGUILayout.Toggle(new GUIContent(def.Name, def.Tooltip), value);
                    _buffer[0] = (byte)(value ? 1 : 0);
                    WritePVarData(_buffer, def.Offset, 1);
                    break;
                }
            case "byte":
                {
                    // read value
                    ReadPVarData(_buffer, def.Offset, 1);
                    var value = (int)_buffer[0];
                    value = EditorGUILayout.IntField(new GUIContent(def.Name, def.Tooltip), value);
                    if (value < def.Min) value = (int)def.Min;
                    if (value > def.Max) value = (int)def.Max;
                    if (value > byte.MaxValue) value = byte.MaxValue;
                    if (value < byte.MinValue) value = byte.MinValue;
                    _buffer[0] = (byte)value;
                    WritePVarData(_buffer, def.Offset, 1);
                    break;
                }
            case "sbyte":
                {
                    // read value
                    ReadPVarData(_buffer, def.Offset, 1);
                    var value = (int)(sbyte)_buffer[0];
                    value = EditorGUILayout.IntField(new GUIContent(def.Name, def.Tooltip), value);
                    if (value < def.Min) value = (int)def.Min;
                    if (value > def.Max) value = (int)def.Max;
                    if (value > sbyte.MaxValue) value = sbyte.MaxValue;
                    if (value < sbyte.MinValue) value = sbyte.MinValue;
                    _buffer[0] = (byte)(sbyte)value;
                    WritePVarData(_buffer, def.Offset, 1);
                    break;
                }
            case "integer":
                {
                    // read value
                    ReadPVarData(_buffer, def.Offset, 4);
                    var value = BitConverter.ToInt32(_buffer, 0);
                    value = EditorGUILayout.IntField(new GUIContent(def.Name, def.Tooltip), value);
                    if (value < def.Min) value = (int)def.Min;
                    if (value > def.Max) value = (int)def.Max;
                    var b = BitConverter.GetBytes(value);
                    WritePVarData(b, def.Offset, 4);
                    break;
                }
            case "float":
                {
                    // read value
                    ReadPVarData(_buffer, def.Offset, 4);
                    var value = BitConverter.ToSingle(_buffer, 0);
                    value = EditorGUILayout.FloatField(new GUIContent(def.Name, def.Tooltip), value);
                    if (value < def.Min) value = (float)def.Min;
                    if (value > def.Max) value = (float)def.Max;
                    var b = BitConverter.GetBytes(value);
                    WritePVarData(b, def.Offset, 4);
                    break;
                }
            case "vector2":
                {
                    // read value
                    ReadPVarData(_buffer, def.Offset, 8);
                    var value = new Vector2(BitConverter.ToSingle(_buffer, 0), BitConverter.ToSingle(_buffer, 4));
                    value = EditorGUILayout.Vector2Field(new GUIContent(def.Name, def.Tooltip), value);
                    //if (value < def.Min) value = (float)def.Min;
                    //if (value > def.Max) value = (float)def.Max;
                    WritePVarData(BitConverter.GetBytes(value.x), def.Offset, 4);
                    WritePVarData(BitConverter.GetBytes(value.y), def.Offset + 4, 4);
                    break;
                }
            case "colorrgb":
                {
                    // read value
                    ReadPVarData(_buffer, def.Offset, 3);
                    var value = new Color32(_buffer[0], _buffer[1], _buffer[2], 255);
                    value = EditorGUILayout.ColorField(new GUIContent(def.Name, def.Tooltip), value, showEyedropper: true, showAlpha: false, hdr: false);
                    _buffer[0] = value.r;
                    _buffer[1] = value.g;
                    _buffer[2] = value.b;
                    WritePVarData(_buffer, def.Offset, 3);
                    break;
                }
            case "colorrgba":
                {
                    // read value
                    ReadPVarData(_buffer, def.Offset, 4);
                    var value = new Color32(_buffer[0], _buffer[1], _buffer[2], _buffer[3]);
                    value = EditorGUILayout.ColorField(new GUIContent(def.Name, def.Tooltip), value);
                    _buffer[0] = value.r;
                    _buffer[1] = value.g;
                    _buffer[2] = value.b;
                    _buffer[3] = value.a;
                    WritePVarData(_buffer, def.Offset, 4);
                    break;
                }
            case "team":
                {
                    // read value
                    ReadPVarData(_buffer, def.Offset, def.DataSize ?? 1);
                    var value = (DLTeamIds)_buffer[0];
                    value = EnumPopup(new GUIContent(def.Name, def.Tooltip), value, def.Min, def.Max);
                    var b = BitConverter.GetBytes((int)value);
                    WritePVarData(b, def.Offset, def.DataSize ?? 1);
                    break;
                }
            case "fxtex":
                {
                    // read value
                    ReadPVarData(_buffer, def.Offset, 4);
                    var value = (DLFXTextureIds)BitConverter.ToInt32(_buffer, 0);
                    value = EnumPopup(new GUIContent(def.Name, def.Tooltip), value, def.Min, def.Max);
                    var b = BitConverter.GetBytes((int)value);
                    WritePVarData(b, def.Offset, 4);
                    break;
                }
            case "levelfxtex":
                {
                    // read value
                    ReadPVarData(_buffer, def.Offset, 4);
                    var value = (DLLevelFXTextureIds)BitConverter.ToInt32(_buffer, 0);
                    value = EnumPopup(new GUIContent(def.Name, def.Tooltip), value, def.Min, def.Max);
                    var b = BitConverter.GetBytes((int)value);
                    WritePVarData(b, def.Offset, 4);
                    break;
                }
            case "enum":
                {
                    // read value
                    ReadPVarData(_buffer, def.Offset, def.DataSize ?? 4);
                    var value = (int)(BitConverter.ToInt64(_buffer, 0) & (long)(Math.Pow(2, (def.DataSize ?? 4) * 8) - 1));
                    value = EnumPopup(new GUIContent(def.Name, def.Tooltip), value, def.Options);
                    var b = BitConverter.GetBytes(value);
                    WritePVarData(b, def.Offset, def.DataSize ?? 4);
                    break;
                }
            case "cuboidref":
                {
                    var refIdx = def.Offset / 4;
                    var refObj = _pvarCuboidRefs.GetArrayElementAtIndex(refIdx);
                    EditorGUILayout.ObjectField(refObj, typeof(Cuboid), new GUIContent(def.Name, def.Tooltip));
                    break;
                }
            case "splineref":
                {
                    var refIdx = def.Offset / 4;
                    var refObj = _pvarSplineRefs.GetArrayElementAtIndex(refIdx);
                    EditorGUILayout.ObjectField(refObj, typeof(Spline), new GUIContent(def.Name, def.Tooltip));
                    break;
                }
            case "arearef":
                {
                    var refIdx = def.Offset / 4;
                    var refObj = _pvarAreaRefs.GetArrayElementAtIndex(refIdx);
                    EditorGUILayout.ObjectField(refObj, typeof(Area), new GUIContent(def.Name, def.Tooltip));
                    break;
                }
            case "mobyref":
                {
                    var refIdx = def.Offset / 4;
                    var refObj = _pvarMobyRefs.GetArrayElementAtIndex(refIdx);
                    EditorGUILayout.ObjectField(refObj, typeof(Moby), new GUIContent(def.Name, def.Tooltip));
                    break;
                }
            case "mobyrefarray":
                {
                    for (int i = 0; i < def.Count; ++i)
                    {
                        var refIdx = (def.Offset / 4) + i;
                        var refObj = _pvarMobyRefs.GetArrayElementAtIndex(refIdx);
                        EditorGUILayout.ObjectField(refObj, typeof(Moby), new GUIContent(def.Name + $" #{i+1}", def.Tooltip));
                    }
                    break;
                }
        }
    }

    private T EnumPopup<T>(GUIContent label, T value, float? min, float? max) where T : struct, IConvertible
    {
        var options = ((T[])Enum.GetValues(typeof(T))).Where(x => !((int)(object)x < min) && !((int)(object)x > max)).ToArray();
        var names = options.Select(x => Enum.GetName(typeof(T), x)).ToArray();

        return options.ElementAtOrDefault(EditorGUILayout.Popup(label, Array.IndexOf(options, value), names));
    }

    private int EnumPopup(GUIContent label, int value, Dictionary<string, int> options)
    {
        var names = options.Select(x => x.Key).ToArray();
        var selectedKey = options.FirstOrDefault(x => x.Value == value).Key;
        var idx = Array.IndexOf(names, selectedKey);

        idx = EditorGUILayout.Popup(label, idx, names);

        selectedKey = names.ElementAtOrDefault(idx);
        if (selectedKey == null) return value;
        return options.GetValueOrDefault(selectedKey);
    }

    private void ReadPVarData(byte[] dst, int srcOffset, int length)
    {
        for (int i = 0; i < length; ++i)
            dst[i] = (byte)_pvarData.GetArrayElementAtIndex(i + srcOffset).intValue;

        //for (int i = 0; i < length; ++i)
        //    dst[i] = (target as Moby).PVars[srcOffset + i];
    }

    private void WritePVarData(byte[] src, int dstOffset, int length)
    {
        //var change = false;
        //for (int i = 0; i < length; ++i)
        //    change |= src[i] != (target as Moby).PVars[i + dstOffset];

        //if (change)
        //{
        //    Undo.RecordObject(target, "WritePVarData");
        //    Array.Copy(src, 0, (target as Moby).PVars, dstOffset, length);
        //}

        for (int i = 0; i < length; ++i)
            _pvarData.GetArrayElementAtIndex(i + dstOffset).intValue = src[i];
    }

    private void SelectChildren(Moby moby, ref UnityEngine.Object[] selected)
    {
        if (moby.PVarMobyRefs != null)
        {
            foreach (var childMoby in moby.PVarMobyRefs)
            {
                if (!childMoby) continue;
                if (!selected.Contains(childMoby.gameObject))
                    Array.Resize(ref selected, selected.Length + 1);

                selected[selected.Length - 1] = childMoby.gameObject;
                SelectChildren(childMoby, ref selected);
            }
        }

        SelectCuboidChildren(moby, ref selected);
        SelectSplineChildren(moby, ref selected);
        SelectAreaChildren(moby, ref selected);
    }

    private void SelectMobyChildren(Moby moby, ref UnityEngine.Object[] selected)
    {
        if (moby.PVarMobyRefs != null)
        {
            foreach (var childMoby in moby.PVarMobyRefs)
            {
                if (!childMoby) continue;
                if (!selected.Contains(childMoby.gameObject))
                    Array.Resize(ref selected, selected.Length + 1);

                selected[selected.Length - 1] = childMoby.gameObject;
                SelectMobyChildren(childMoby, ref selected);
            }
        }
    }

    private void SelectCuboidChildren(Moby moby, ref UnityEngine.Object[] selected)
    {
        if (moby.PVarCuboidRefs != null)
        {
            foreach (var childCuboid in moby.PVarCuboidRefs)
            {
                if (!childCuboid) continue;
                if (!selected.Contains(childCuboid.gameObject))
                    Array.Resize(ref selected, selected.Length + 1);

                selected[selected.Length - 1] = childCuboid.gameObject;
            }
        }
    }

    private void SelectSplineChildren(Moby moby, ref UnityEngine.Object[] selected)
    {
        if (moby.PVarSplineRefs != null)
        {
            foreach (var childSpline in moby.PVarSplineRefs)
            {
                if (!childSpline) continue;
                if (!selected.Contains(childSpline.gameObject))
                    Array.Resize(ref selected, selected.Length + 1);

                selected[selected.Length - 1] = childSpline.gameObject;
            }
        }
    }

    private void SelectAreaChildren(Moby moby, ref UnityEngine.Object[] selected)
    {
        if (moby.PVarAreaRefs != null)
        {
            foreach (var childArea in moby.PVarAreaRefs)
            {
                if (!childArea) continue;
                if (!selected.Contains(childArea.gameObject))
                    Array.Resize(ref selected, selected.Length + 1);

                selected[selected.Length - 1] = childArea.gameObject;
            }
        }
    }
}

using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEngine;

[ExecuteInEditMode, SelectionBase]
public class Moby : RenderSelectionBase, IAsset
{
    private static readonly Color mobyLinkColor = new Color(1, 0.75f, 0.75f);
    private static readonly Color cuboidLinkColor = new Color(0.5f, 0.25f, 1f);
    private static readonly Color splineLinkColor = new Color(0.5f, 0.25f, 1f);
    private static readonly Color areaLinkColor = new Color(0.25f, 0.5f, 1f);

    [HideInInspector, SerializeField]
    public int RCVersion = 0;

    /* 0x04 */
    [SerializeField]
    public int Mission = -1;
    /* 0x08 */
    [ReadOnly, SerializeField]
    public int Uid = -1;
    /* 0x0c */
    public int Bolts;
    /* 0x10 */
    [HideInInspector]
    public int OClass;
    /* 0x18 */
    [Range(0, 1023)]
    public int DrawDistance = 64;
    /* 0x1c */
    [Range(0, 255)]
    public int UpdateDistance = 64;
    /* 0x40 */
    public int GroupId = -1;
    /* 0x44 */
    [HideInInspector, SerializeField]
    public int IsRooted;
    /* 0x48 */
    [HideInInspector, SerializeField]
    public float RootedDistance;
    /* 0x50 */
    [HideInInspector, SerializeField]
    public int PvarIndex;
    /* 0x54 */
    public int Occlusion;
    /* 0x58 */
    public int ModeBits;
    /* 0x5c */
    public Color Color = Color.white;
    /* 0x68 */
    public byte Light1;
    /* 0x69 */
    public byte Light2;
    /* 0x6A */
    public short Light3;

    [HideInInspector] public int[] PVarPointers;
    [HideInInspector] public byte[] PVars;
    [HideInInspector] public Cuboid[] PVarCuboidRefs;
    [HideInInspector] public Moby[] PVarMobyRefs;
    [HideInInspector] public Spline[] PVarSplineRefs;
    [HideInInspector] public Area[] PVarAreaRefs;

    public GameObject GameObject => this ? this.gameObject : null;
    public bool IsHidden => renderHandle?.IsHidden ?? false;

    public Renderer[] GetRenderers() => renderHandle?.AssetInstance?.GetComponentsInChildren<Renderer>();
    public GameObject GetAssetInstance() => renderHandle?.AssetInstance;

    private RenderHandle renderHandle = null;
    private Color _tintColor = Color.white;

    private void OnEnable()
    {
        AssetUpdater.RegisterAsset(this);
        UpdateAsset();
    }

    private void OnDisable()
    {
        AssetUpdater.UnregisterAsset(this);

        renderHandle?.DestroyAsset();
    }

    private void OnValidate()
    {
        renderHandle?.UpdateMaterials();
        InitializePVarReferences();
    }

    private void OnDrawGizmosSelected()
    {
        if (PVarMobyRefs != null)
        {
            foreach (var moby in PVarMobyRefs)
            {
                if (!moby) continue;

                UnityHelper.DrawLine(transform.position, moby.transform.position, mobyLinkColor, 5);

                moby.OnDrawGizmosSelected();
            }
        }

        if (PVarCuboidRefs != null)
        {
            foreach (var cuboid in PVarCuboidRefs)
            {
                if (!cuboid) continue;

                UnityHelper.DrawLine(transform.position, cuboid.transform.position, cuboidLinkColor, 5);
            }
        }

        if (PVarSplineRefs != null)
        {
            foreach (var spline in PVarSplineRefs)
            {
                if (!spline) continue;

                // spline.DrawGizmos();
            }
        }

        if (PVarAreaRefs != null)
        {
            foreach (var area in PVarAreaRefs)
            {
                if (!area) continue;

                // area.DrawGizmos();
                foreach (var cuboid in area.Cuboids)
                {
                    if (!cuboid) continue;

                    UnityHelper.DrawLine(transform.position, cuboid.transform.position, areaLinkColor, 5);
                }
            }
        }
    }

    public void UpdateAsset()
    {
        if (renderHandle == null) renderHandle = new RenderHandle(UpdateRenderHandleMaterial);

        this.transform.localScale = Vector3.one * this.transform.localScale.x;
        renderHandle.IsHidden = SceneVisibilityManager.instance.IsHidden(this.gameObject);
        renderHandle.IsSelected = Selection.activeGameObject == this.gameObject || Selection.gameObjects.Contains(this.gameObject);
        renderHandle.IsPicking = !SceneVisibilityManager.instance.IsPickingDisabled(this.gameObject);
        renderHandle.WorldLightIndex = Light1;
        renderHandle.Update(this.gameObject, GetPrefab());
        UpdateMaterials();

        InitializePVarReferences();
    }

    public void UpdateMaterials()
    {
        renderHandle?.UpdateMaterials();
    }

    public void MakeUidUnique()
    {
        var mobys = FindObjectsOfType<Moby>();
        if (this.Uid >= 0 && !mobys.Any(x => x.Uid == this.Uid && x != this)) return;

        int uid = 1;
        while (mobys.Any(x => x.Uid == uid))
            ++uid;

        this.Uid = uid;
    }

    public void Read(BinaryReader reader)
    {
        var dataSize = reader.ReadInt32();
        Mission = reader.ReadInt32();
        Uid = reader.ReadInt32();
        Bolts = reader.ReadInt32();
        OClass = reader.ReadInt32();
        var scale = reader.ReadSingle();
        DrawDistance = reader.ReadInt32();
        UpdateDistance = reader.ReadInt32();
        _ = reader.ReadInt32();
        _ = reader.ReadInt32();
        this.transform.position = new Vector3(reader.ReadSingle(), reader.ReadSingle(), reader.ReadSingle()).SwizzleXZY();
        this.transform.rotation = Quaternion.Euler(new Vector3(reader.ReadSingle(), reader.ReadSingle(), reader.ReadSingle()).SwizzleXZY() * -Mathf.Rad2Deg);
        GroupId = reader.ReadInt32();
        IsRooted = reader.ReadInt32();
        RootedDistance = reader.ReadSingle();
        _ = reader.ReadInt32();
        PvarIndex = reader.ReadInt32();
        Occlusion = reader.ReadInt32();
        ModeBits = reader.ReadInt32();
        Color = new Color(reader.ReadInt32() / 128f, reader.ReadInt32() / 128f, reader.ReadInt32() / 128f, 1);
        Light1 = reader.ReadByte();
        Light2 = reader.ReadByte();
        Light3 = reader.ReadInt16();
        _ = reader.ReadInt32();

        this.transform.localScale = Vector3.one * scale;
    }

    public void Write(BinaryWriter writer)
    {
        var euler = this.transform.rotation.eulerAngles * -Mathf.Deg2Rad;

        writer.Write(0x70); // datasize
        writer.Write(Mission);
        writer.Write(Uid);
        writer.Write(Bolts);
        writer.Write(OClass);
        writer.Write(this.transform.localScale.x);
        writer.Write(DrawDistance);
        writer.Write(UpdateDistance);
        writer.Write(0);
        writer.Write(0);
        writer.Write(this.transform.position.x);
        writer.Write(this.transform.position.z);
        writer.Write(this.transform.position.y);
        writer.Write(euler.x);
        writer.Write(euler.z);
        writer.Write(euler.y);
        writer.Write(GroupId);
        writer.Write(IsRooted);
        writer.Write(RootedDistance);
        writer.Write(0);
        writer.Write(PvarIndex);
        writer.Write(Occlusion);
        writer.Write(ModeBits);
        writer.Write((int)(Color.r * 128));
        writer.Write((int)(Color.g * 128));
        writer.Write((int)(Color.b * 128));
        writer.Write((byte)Light1);
        writer.Write((byte)Light2);
        writer.Write(Light3);
        writer.Write(0);
    }

    #region Occlusion Bake

    public void OnPreBake(Color32 uidColor)
    {
        if (renderHandle == null) UpdateAsset();
        renderHandle.IdColor = uidColor;
        UpdateAsset();
    }

    public void OnPostBake()
    {

    }

    #endregion

    #region Moby PVars

    public void InitializePVarReferences(bool useDefault = false)
    {
        var mapConfig = GameObject.FindObjectOfType<MapConfig>();
        if (!mapConfig) return;

        var pvarOverlay = PvarOverlay.GetPvarOverlay(OClass, RCVersion);
        if (pvarOverlay != null && pvarOverlay.Overlay.Any())
        {
            try
            {
                if (useDefault)
                {
                    this.name = pvarOverlay.Name;
                    this.PVars = new byte[pvarOverlay.Length];
                    Array.Copy(pvarOverlay.DefaultBytes, 0, this.PVars, 0, Math.Min(this.PVars.Length, pvarOverlay.DefaultBytes.Length));
                }

                foreach (var def in pvarOverlay.Overlay)
                {
                    InitializeOverlayField(mapConfig, def);
                }
            }
            catch (Exception ex) { Debug.LogError(ex); }
        }
    }

    private void InitializeOverlayField(MapConfig mapConfig, PvarOverlayDef def)
    {
        switch (def.DataType?.ToLower())
        {
            case "cuboidref":
                {
                    // initialize first value if array doesn't fit
                    // this should only occur on newly imported mobys
                    var refIdx = def.Offset / 4;
                    if (PVarCuboidRefs == null || refIdx >= PVarCuboidRefs.Length)
                    {
                        // increase array size
                        if (PVarCuboidRefs == null)
                            PVarCuboidRefs = new Cuboid[refIdx + 1];
                        else
                            Array.Resize(ref PVarCuboidRefs, refIdx + 1);

                        PVarCuboidRefs[refIdx] = null;

                        // find init value
                        var cuboidIdx = BitConverter.ToInt32(PVars, def.Offset);
                        if (cuboidIdx >= 0)
                        {
                            var cuboid = mapConfig.GetCuboidAtIndex(cuboidIdx);
                            if (cuboid)
                            {
                                PVarCuboidRefs[refIdx] = cuboid;
                            }
                        }
                    }
                    break;
                }
            case "splineref":
                {
                    // initialize first value if array doesn't fit
                    // this should only occur on newly imported mobys
                    var refIdx = def.Offset / 4;
                    if (PVarSplineRefs == null || refIdx >= PVarSplineRefs.Length)
                    {
                        // increase array size
                        if (PVarSplineRefs == null)
                            PVarSplineRefs = new Spline[refIdx + 1];
                        else
                            Array.Resize(ref PVarSplineRefs, refIdx + 1);

                        PVarSplineRefs[refIdx] = null;

                        // find init value
                        var splineIdx = BitConverter.ToInt32(PVars, def.Offset);
                        if (splineIdx >= 0)
                        {
                            var spline = mapConfig.GetSplineAtIndex(splineIdx);
                            if (spline)
                            {
                                PVarSplineRefs[refIdx] = spline;
                            }
                        }
                    }
                    break;
                }
            case "arearef":
                {
                    // initialize first value if array doesn't fit
                    // this should only occur on newly imported mobys
                    var refIdx = def.Offset / 4;
                    if (PVarAreaRefs == null || refIdx >= PVarAreaRefs.Length)
                    {
                        // increase array size
                        if (PVarAreaRefs == null)
                            PVarAreaRefs = new Area[refIdx + 1];
                        else
                            Array.Resize(ref PVarAreaRefs, refIdx + 1);

                        PVarAreaRefs[refIdx] = null;

                        // find init value
                        var areaIdx = BitConverter.ToInt32(PVars, def.Offset);
                        if (areaIdx >= 0)
                        {
                            var area = mapConfig.GetAreaAtIndex(areaIdx);
                            if (area)
                            {
                                PVarAreaRefs[refIdx] = area;
                            }
                        }
                    }
                    break;
                }
            case "mobyref":
                {
                    var refIdx = def.Offset / 4;
                    if (PVarMobyRefs == null || refIdx >= PVarMobyRefs.Length)
                    {
                        // increase array size
                        if (PVarMobyRefs == null)
                            PVarMobyRefs = new Moby[refIdx + 1];
                        else
                            Array.Resize(ref PVarMobyRefs, refIdx + 1);

                        PVarMobyRefs[refIdx] = null;

                        // find init value
                        var mobyIdx = BitConverter.ToInt32(PVars, def.Offset);
                        if (mobyIdx >= 0)
                        {
                            var mobyRef = mapConfig.GetMobyAtIndex(mobyIdx);
                            if (mobyRef)
                            {
                                PVarMobyRefs[refIdx] = mobyRef;
                            }
                        }
                    }
                    break;
                }
            case "mobyrefarray":
                {
                    // initialize first value if array doesn't fit
                    // this should only occur on newly imported mobys
                    for (int i = 0; i < def.Count; ++i)
                    {
                        var refIdx = (def.Offset / 4) + i;
                        if (PVarMobyRefs == null || refIdx >= PVarMobyRefs.Length)
                        {
                            // increase array size
                            if (PVarMobyRefs == null)
                                PVarMobyRefs = new Moby[refIdx + 1];
                            else
                                Array.Resize(ref PVarMobyRefs, refIdx + 1);

                            PVarMobyRefs[refIdx] = null;

                            // find init value
                            var mobyIdx = BitConverter.ToInt32(PVars, def.Offset + (i * 4));
                            if (mobyIdx >= 0)
                            {
                                var mobyRef = mapConfig.GetMobyAtIndex(mobyIdx);
                                if (mobyRef)
                                {
                                    PVarMobyRefs[refIdx] = mobyRef;
                                }
                            }
                        }
                    }

                    break;
                }
        }
    }

    public void UpdatePVars()
    {
        var mapConfig = GameObject.FindObjectOfType<MapConfig>();
        if (!mapConfig) return;

        var cuboids = mapConfig.GetCuboids();
        var splines = mapConfig.GetSplines();
        var mobys = mapConfig.GetMobys();
        var areas = mapConfig.GetAreas();

        // handle special moby class pvars
        switch (OClass)
        {
            case 0x106a: // mp config
                UpdateMPConfigPVars(cuboids);
                break;
        }

        // update reference types to index
        var pvarOverlay = PvarOverlay.GetPvarOverlay(OClass, RCVersion);
        if (pvarOverlay != null && pvarOverlay.Overlay.Any())
        {
            try
            {
                foreach (var def in pvarOverlay.Overlay)
                {
                    switch (def.DataType?.ToLower())
                    {
                        case "cuboidref":
                            {
                                var refIdx = def.Offset / 4;
                                var refObj = PVarCuboidRefs?.ElementAtOrDefault(refIdx);
                                var value = -1;
                                if (refObj)
                                    value = Array.IndexOf(cuboids, refObj);

                                Array.Copy(BitConverter.GetBytes(value), 0, PVars, def.Offset, 4);
                                break;
                            }
                        case "splineref":
                            {
                                var refIdx = def.Offset / 4;
                                var refObj = PVarSplineRefs?.ElementAtOrDefault(refIdx);
                                var value = -1;
                                if (refObj)
                                    value = Array.IndexOf(splines, refObj);

                                Array.Copy(BitConverter.GetBytes(value), 0, PVars, def.Offset, 4);
                                break;
                            }
                        case "arearef":
                            {
                                var refIdx = def.Offset / 4;
                                var refObj = PVarAreaRefs?.ElementAtOrDefault(refIdx);
                                var value = -1;
                                if (refObj)
                                    value = Array.IndexOf(areas, refObj);

                                Array.Copy(BitConverter.GetBytes(value), 0, PVars, def.Offset, 4);
                                break;
                            }
                        case "mobyref":
                            {
                                var refIdx = def.Offset / 4;
                                var refObj = PVarMobyRefs?.ElementAtOrDefault(refIdx);
                                var value = -1;
                                if (refObj)
                                    value = Array.IndexOf(mobys, refObj);

                                Array.Copy(BitConverter.GetBytes(value), 0, PVars, def.Offset, 4);
                                break;
                            }
                        case "mobyrefarray":
                            {
                                for (int i = 0; i < def.Count; ++i)
                                {
                                    var refIdx = (def.Offset / 4) + i;
                                    var refObj = PVarMobyRefs?.ElementAtOrDefault(refIdx);
                                    var value = -1;
                                    if (refObj)
                                        value = Array.IndexOf(mobys, refObj);

                                    Array.Copy(BitConverter.GetBytes(value), 0, PVars, def.Offset + (i*4), 4);
                                }

                                break;
                            }
                    }
                }
            }
            catch (Exception ex) { Debug.LogError(ex); }
        }
    }

    private void UpdateMPConfigPVars(Cuboid[] cuboids)
    {
        using (var ms = new MemoryStream(PVars, true))
        {
            using (var writer = new BinaryWriter(ms))
            {
                // write deathmatch cuboids
                writer.BaseStream.Position = 0x1F8;
                var playerSpawns = cuboids.Where(x => x.Type == CuboidType.Player).ToArray();
                if (playerSpawns.Length > 64) Debug.LogWarning("More than 64 player spawn cuboids detected. Truncating to first 64.");
                if (playerSpawns.Length == 0) Debug.LogWarning("No player spawn cuboids detected. Player spawns will not work.");
                for (int i = 0; i < 64; ++i)
                {
                    var cuboid = playerSpawns.ElementAtOrDefault(i);
                    var idx = cuboid ? Array.IndexOf(cuboids, cuboid) : -1;

                    writer.Write(idx);
                }

                // write flag spawns
                writer.BaseStream.Position = 0;
                var blueFlagSpawns = cuboids.Where(x => (x.Type == CuboidType.Player || x.Type == CuboidType.None) && x.Subtype == CuboidSubType.BlueFlagSpawn).ToArray();
                if (blueFlagSpawns.Length != 3) Debug.LogWarning($"Expected 3 blue flag player spawn cuboids found {blueFlagSpawns.Length}.");
                for (int i = 0; i < 3; ++i)
                {
                    var cuboid = blueFlagSpawns.ElementAtOrDefault(blueFlagSpawns.Length > 0 ? (i % blueFlagSpawns.Length) : 0);
                    var idx = cuboid ? Array.IndexOf(cuboids, cuboid) : -1;
                    writer.Write(idx);
                }

                writer.BaseStream.Position = 0x0c;
                var redFlagSpawns = cuboids.Where(x => (x.Type == CuboidType.Player || x.Type == CuboidType.None) && x.Subtype == CuboidSubType.RedFlagSpawn).ToArray();
                if (redFlagSpawns.Length != 3) Debug.LogWarning($"Expected 3 red flag player spawn cuboids found {redFlagSpawns.Length}.");
                for (int i = 0; i < 3; ++i)
                {
                    var cuboid = redFlagSpawns.ElementAtOrDefault(redFlagSpawns.Length > 0 ? (i % redFlagSpawns.Length) : 0);
                    var idx = cuboid ? Array.IndexOf(cuboids, cuboid) : -1;
                    writer.Write(idx);
                }

                writer.BaseStream.Position = 0x168;
                var greenFlagSpawns = cuboids.Where(x => (x.Type == CuboidType.Player || x.Type == CuboidType.None) && x.Subtype == CuboidSubType.GreenFlagSpawn).ToArray();
                if (greenFlagSpawns.Length != 3) Debug.LogWarning($"Expected 3 green flag player spawn cuboids found {greenFlagSpawns.Length}.");
                for (int i = 0; i < 3; ++i)
                {
                    var cuboid = greenFlagSpawns.ElementAtOrDefault(greenFlagSpawns.Length > 0 ? (i % greenFlagSpawns.Length) : 0);
                    var idx = cuboid ? Array.IndexOf(cuboids, cuboid) : -1;
                    writer.Write(idx);
                }

                writer.BaseStream.Position = 0x174;
                var orangeFlagSpawns = cuboids.Where(x => (x.Type == CuboidType.Player || x.Type == CuboidType.None) && x.Subtype == CuboidSubType.OrangeFlagSpawn).ToArray();
                if (orangeFlagSpawns.Length != 3) Debug.LogWarning($"Expected 3 orange flag player spawn cuboids found {orangeFlagSpawns.Length}.");
                for (int i = 0; i < 3; ++i)
                {
                    var cuboid = orangeFlagSpawns.ElementAtOrDefault(orangeFlagSpawns.Length > 0 ? (i % orangeFlagSpawns.Length) : 0);
                    var idx = cuboid ? Array.IndexOf(cuboids, cuboid) : -1;
                    writer.Write(idx);
                }
            }
        }

    }

    #endregion

    private void UpdateRenderHandleMaterial(Renderer renderer, MaterialPropertyBlock mpb)
    {
        mpb.SetColor("_Color", Color * _tintColor);
    }

    private GameObject GetPrefab()
    {
        GameObject prefab = null;

        switch (OClass)
        {
            case 9278:
                {
                    prefab = GetGadgetPickupPrefab();
                    break;
                }
            case 9838:
                {
                    prefab = GetFlagBasePrefab();
                    break;
                }
            default:
                {
                    var prefabPath = Path.Combine(FolderNames.GetGlobalPrefabFolder(FolderNames.MobyFolder), $"{OClass}", $"{OClass}.prefab");
                    prefab = AssetDatabase.LoadAssetAtPath<GameObject>(prefabPath);
                    break;
                }
        }

        if (!prefab) prefab = UnityHelper.GetAssetPrefab(FolderNames.MobyFolder, OClass.ToString());
        return prefab;
    }

    private GameObject GetFlagBasePrefab()
    {
        if (PVars == null || PVars.Length < 1) return null;

        switch (PVars[0]) // team
        {
            case 0: _tintColor = Color.blue; break;
            case 1: _tintColor = Color.red; break;
            case 2: _tintColor = Color.green; break;
            case 3: _tintColor = (Color.red * 0.75f) + (Color.green * 0.25f); break;
            default: _tintColor = Color.white; break;
        }

        return UnityHelper.GetAssetPrefab(FolderNames.MobyFolder, "8309", includeGlobal: true);
    }

    private GameObject GetGadgetPickupPrefab()
    {
        if (PVars == null || PVars.Length < 1) return null;

        renderHandle.Rotation = Quaternion.Euler(0, 0, 90);
        renderHandle.Offset = Vector3.up * 0.5f;
        renderHandle.Scale = Vector3.one * 2f;

        switch (PVars[0])
        {
            case 0: return UnityHelper.GetAssetPrefab(FolderNames.MobyFolder, "9210", includeGlobal: true);
            case 2: return UnityHelper.GetAssetPrefab(FolderNames.MobyFolder, "4244", includeGlobal: true);
            case 3: return UnityHelper.GetAssetPrefab(FolderNames.MobyFolder, "4231", includeGlobal: true);
            case 4: return UnityHelper.GetAssetPrefab(FolderNames.MobyFolder, "6244", includeGlobal: true);
            case 5: return UnityHelper.GetAssetPrefab(FolderNames.MobyFolder, "4246", includeGlobal: true);
            case 6: return UnityHelper.GetAssetPrefab(FolderNames.MobyFolder, "8340", includeGlobal: true);
            case 7: return UnityHelper.GetAssetPrefab(FolderNames.MobyFolder, "4234", includeGlobal: true);
            case 12: return UnityHelper.GetAssetPrefab(FolderNames.MobyFolder, "8454", includeGlobal: true);
            case 13: return UnityHelper.GetAssetPrefab(FolderNames.MobyFolder, "9773", includeGlobal: true);
            case 16: return UnityHelper.GetAssetPrefab(FolderNames.MobyFolder, "4261", includeGlobal: true);
        }

        return null;
    }

}

using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEngine;

[ExecuteInEditMode, SelectionBase, AddComponentMenu("")]
public class Moby : RenderSelectionBase, IAsset, IPVarObject
{
    private static readonly Color mobyLinkColor = new Color(1, 0.75f, 0.75f);
    private static readonly Color cuboidLinkColor = new Color(0.5f, 0.25f, 1f);
    private static readonly Color splineLinkColor = new Color(0.5f, 0.25f, 1f);
    private static readonly Color areaLinkColor = new Color(0.25f, 0.5f, 1f);
    private static readonly Color groupLinkColor = new Color(0.25f, 1f, 0.5f);
    public static bool DrawPVarMobyTieGroupIdLines = false;
    public static bool DrawPVarMobyLines = false;

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
    [HideInInspector] public SerializableStringDictionary PVarValues;
    [HideInInspector] public SerializableMonoBehaviourDictionary PVarReferences;
    [HideInInspector] public string[] PVarStrings;
    [HideInInspector] public GameObject PrefabOverride;
    [HideInInspector] public Matrix4x4 PrefabOverrideTransformation = Matrix4x4.identity;

    public int GetRCVersion() => RCVersion;
    public byte[] GetPVarData() => PVars;
    public SerializableStringDictionary GetPVarValues() => PVarValues;
    public SerializableMonoBehaviourDictionary GetPVarReferences() => PVarReferences;
    public string[] GetPVarStrings() => PVarStrings;
    public PvarOverlay GetPVarOverlay() => PvarOverlay.GetPvarOverlay(this.RCVersion, mobyClass: OClass);
    public void SetPVarData(byte[] pvarData) => PVars = pvarData;
    public void SetPVarValues(SerializableStringDictionary pvarValues) => PVarValues = pvarValues;
    public void SetPVarReferences(SerializableMonoBehaviourDictionary pvarRefs) => PVarReferences = pvarRefs;
    public void SetPVarStrings(string[] strings) => PVarStrings = strings;
    public object GetPVarValue(string path) => GetPVarOverlay().GetPVarValue(path, PVarValues, PVarReferences);
    public T GetPVarValue<T>(string path)
    {
        var value = GetPVarOverlay().GetPVarValue(path, PVarValues, PVarReferences);
        if (value == null) return default(T);

        return (T)Convert.ChangeType(value, typeof(T));
    }

    public GameObject GameObject => this ? this.gameObject : null;
    public bool IsHidden => renderHandle?.IsHidden ?? false;

    public Renderer[] GetRenderers() => renderHandle?.AssetInstance?.GetComponentsInChildren<Renderer>();
    public GameObject GetAssetInstance() => renderHandle?.AssetInstance;

    private RenderHandle renderHandle = null;
    private Color _tintColor = Color.white;

    private void OnEnable()
    {
        if (PVarValues == null) PVarValues = new SerializableStringDictionary();
        if (PVarReferences == null) PVarReferences = new SerializableMonoBehaviourDictionary();

        if (PVarValues != null) PVarValues.Owner = this;
        if (PVarReferences != null) PVarReferences.Owner = this;

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
        // only render gizmos if gameobject is in actual selection
        if (Selection.gameObjects == null) return;
        if (!Selection.gameObjects.Contains(this.gameObject)) return;

        if (DrawPVarMobyLines)
        {

            DrawMobyRefs(new HashSet<Moby>());

            if (PVarReferences != null)
            {
                foreach (var cuboid in PVarReferences.Select(x => x.Value as Cuboid).Where(x => x))
                {
                    if (!cuboid) continue;

                    UnityHelper.DrawLine(transform.position, cuboid.transform.position, cuboidLinkColor, 5);
                }

                foreach (var spline in PVarReferences.Select(x => x.Value as Spline).Where(x => x))
                {
                    if (!spline) continue;

                    // spline.DrawGizmos();
                }

                foreach (var area in PVarReferences.Select(x => x.Value as Area).Where(x => x))
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
        
        if (PVars != null && DrawPVarMobyTieGroupIdLines)
        {
            var mapConfig = FindObjectOfType<MapConfig>();
            var pvarOverlay = PvarOverlay.GetPvarOverlay(this.RCVersion, mobyClass: this.OClass);
            if (pvarOverlay != null)
            {
                var mobys = mapConfig.GetMobys(this.RCVersion);
                var ties = mapConfig.GetTies();

                // get moby groups and draw lines to all mobys in group
                if (mobys != null)
                {
                    foreach (var mobyGroupId in pvarOverlay.Overlay.Where(x => x.DataType?.ToLower() == "mobygroupid"))
                    {
                        if (int.TryParse(this.PVarValues["." + mobyGroupId.Name], out var groupId) && groupId >= 0)
                        {
                            foreach (var moby in mobys.Where(x => x && x != this && x.GroupId == groupId))
                            {
                                UnityHelper.DrawLine(transform.position, moby.transform.position, groupLinkColor, 5);
                            }
                        }
                    }
                }

                // get tie groups and draw lines to all ties in group
                if (ties != null)
                {
                    foreach (var tieGroupId in pvarOverlay.Overlay.Where(x => x.DataType?.ToLower() == "tiegroupid"))
                    {
                        if (int.TryParse(this.PVarValues["." + tieGroupId.Name], out var groupId) && groupId >= 0)
                        {
                            foreach (var tie in ties.Where(x => x && x.GroupId == groupId))
                            {
                                UnityHelper.DrawLine(transform.position, tie.transform.position, groupLinkColor, 5);
                            }
                        }
                    }
                }
            }
        }
    }

    private void DrawMobyRefs(HashSet<Moby> visited)
    {
        if (PVarReferences != null)
        {
            foreach (var moby in PVarReferences.Select(x => x.Value as Moby).Where(x => x))
            {
                if (!moby) continue;
                if (visited.Contains(moby)) continue;

                visited.Add(moby);
                UnityHelper.DrawLine(transform.position, moby.transform.position, mobyLinkColor, 5);
                moby.DrawMobyRefs(visited);
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
        renderHandle.Layer = LayerMask.NameToLayer("MOBY");
        renderHandle.Update(this.gameObject, GetPrefab());
        if (PrefabOverride && PrefabOverrideTransformation.ValidTRS())
        {
            renderHandle.Offset = PrefabOverrideTransformation.GetPosition();
            renderHandle.Rotation = PrefabOverrideTransformation.GetRotation();
            renderHandle.Scale = PrefabOverrideTransformation.GetScale();
        }

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
        var scale = 1f;

        switch (RCVersion)
        {
            case 3:
                {
                    Mission = reader.ReadInt32();
                    _ = reader.ReadInt32();
                    _ = reader.ReadInt32();
                    Uid = reader.ReadInt32();
                    Bolts = reader.ReadInt32();
                    _ = reader.ReadInt32();
                    _ = reader.ReadInt32();
                    _ = reader.ReadInt32();
                    _ = reader.ReadInt32();
                    OClass = reader.ReadInt32();
                    scale = reader.ReadSingle();
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
                    break;
                }
            case 4:
                {
                    Mission = reader.ReadInt32();
                    Uid = reader.ReadInt32();
                    Bolts = reader.ReadInt32();
                    OClass = reader.ReadInt32();
                    scale = reader.ReadSingle();
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
                    break;
                }
            default: throw new NotImplementedException();
        }

        this.transform.localScale = Vector3.one * scale;
    }

    public void Write(BinaryWriter writer)
    {
        var euler = this.transform.rotation.eulerAngles;
        euler.x = Mathf.DeltaAngle(0, euler.x) * -Mathf.Deg2Rad;
        euler.y = Mathf.DeltaAngle(0, euler.y) * -Mathf.Deg2Rad;
        euler.z = Mathf.DeltaAngle(0, euler.z) * -Mathf.Deg2Rad;

        switch (RCVersion)
        {
            case 3:
                {
                    writer.Write(0x88); // datasize
                    writer.Write(Mission);
                    writer.Write(0);
                    writer.Write(0);
                    writer.Write(Uid);
                    writer.Write(Bolts);
                    writer.Write(0);
                    writer.Write(0);
                    writer.Write(0);
                    writer.Write(0);
                    writer.Write(OClass);
                    writer.Write(this.transform.lossyScale.x);
                    writer.Write(DrawDistance);
                    writer.Write(UpdateDistance);
                    writer.Write(0x20);
                    writer.Write(0x40);
                    writer.Write(this.transform.position.x);
                    writer.Write(this.transform.position.z);
                    writer.Write(this.transform.position.y);
                    writer.Write(euler.x);
                    writer.Write(euler.z);
                    writer.Write(euler.y);
                    writer.Write(GroupId);
                    writer.Write(IsRooted);
                    writer.Write(RootedDistance);
                    writer.Write(1);
                    writer.Write(PvarIndex);
                    writer.Write(Occlusion);
                    writer.Write(ModeBits);
                    writer.Write((int)(Color.r * 128));
                    writer.Write((int)(Color.g * 128));
                    writer.Write((int)(Color.b * 128));
                    writer.Write((byte)Light1);
                    writer.Write((byte)Light2);
                    writer.Write(Light3);
                    writer.Write(-1);
                    break;
                }
            case 4:
                {
                    writer.Write(0x70); // datasize
                    writer.Write(Mission);
                    writer.Write(Uid);
                    writer.Write(Bolts);
                    writer.Write(OClass);
                    writer.Write(this.transform.lossyScale.x);
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
                    break;
                }
            default: throw new NotImplementedException();
        }

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

        var pvarOverlay = PvarOverlay.GetPvarOverlay(RCVersion, mobyClass: this.OClass);
        if (pvarOverlay != null)
        {
            UnityHelper.ValidatePVars(mapConfig, this);

            if (useDefault && !string.IsNullOrEmpty(pvarOverlay.Name))
                this.name = pvarOverlay.Name;

            UnityHelper.InitializePVars(mapConfig, this, useDefault: useDefault); 
            this.PVarPointers = pvarOverlay.PointersInts;
        }
    }

    public void UpdatePVars()
    {
        var mapConfig = GameObject.FindObjectOfType<MapConfig>();
        if (!mapConfig) return;

        var cuboids = mapConfig.GetCuboids();

        UnityHelper.ValidatePVars(mapConfig, this);
        UnityHelper.UpdatePVars(mapConfig, this, this.RCVersion);

        // handle special moby class pvars
        switch ((RCVersion, OClass))
        {
            case (RCVER.UYA, 0x106a): // mp config
                UpdateMPConfigPVars_UYA(cuboids);
                break;
            case (RCVER.DL, 0x106a): // mp config
                UpdateMPConfigPVars_DL(cuboids);
                break;
        }
    }

    private void UpdateMPConfigPVars_DL(Cuboid[] cuboids)
    {
        using (var ms = new MemoryStream(PVars, true))
        {
            using (var writer = new BinaryWriter(ms))
            {
                // write deathmatch cuboids
                writer.BaseStream.Position = 0x1F8;
                var playerSpawns = cuboids.Where(x => x.CuboidType.HasFlag(CuboidMaskType.Player)).ToArray();
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
                var blueFlagSpawns = cuboids.Where(x => x.CuboidType.HasFlag(CuboidMaskType.BlueFlagSpawn)).ToArray();
                if (blueFlagSpawns.Length != 3) Debug.LogWarning($"Expected 3 blue flag player spawn cuboids found {blueFlagSpawns.Length}.");
                for (int i = 0; i < 3; ++i)
                {
                    var cuboid = blueFlagSpawns.ElementAtOrDefault(blueFlagSpawns.Length > 0 ? (i % blueFlagSpawns.Length) : 0);
                    var idx = cuboid ? Array.IndexOf(cuboids, cuboid) : -1;
                    writer.Write(idx);
                }

                writer.BaseStream.Position = 0x0c;
                var redFlagSpawns = cuboids.Where(x => x.CuboidType.HasFlag(CuboidMaskType.RedFlagSpawn)).ToArray();
                if (redFlagSpawns.Length != 3) Debug.LogWarning($"Expected 3 red flag player spawn cuboids found {redFlagSpawns.Length}.");
                for (int i = 0; i < 3; ++i)
                {
                    var cuboid = redFlagSpawns.ElementAtOrDefault(redFlagSpawns.Length > 0 ? (i % redFlagSpawns.Length) : 0);
                    var idx = cuboid ? Array.IndexOf(cuboids, cuboid) : -1;
                    writer.Write(idx);
                }

                writer.BaseStream.Position = 0x168;
                var greenFlagSpawns = cuboids.Where(x => x.CuboidType.HasFlag(CuboidMaskType.GreenFlagSpawn)).ToArray();
                if (greenFlagSpawns.Length != 3) Debug.LogWarning($"Expected 3 green flag player spawn cuboids found {greenFlagSpawns.Length}.");
                for (int i = 0; i < 3; ++i)
                {
                    var cuboid = greenFlagSpawns.ElementAtOrDefault(greenFlagSpawns.Length > 0 ? (i % greenFlagSpawns.Length) : 0);
                    var idx = cuboid ? Array.IndexOf(cuboids, cuboid) : -1;
                    writer.Write(idx);
                }

                writer.BaseStream.Position = 0x174;
                var orangeFlagSpawns = cuboids.Where(x => x.CuboidType.HasFlag(CuboidMaskType.OrangeFlagSpawn)).ToArray();
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

    private void UpdateMPConfigPVars_UYA(Cuboid[] cuboids)
    {
        using (var ms = new MemoryStream(PVars, true))
        {
            using (var writer = new BinaryWriter(ms))
            {
                // write deathmatch cuboids
                writer.BaseStream.Position = 0x200;
                var playerSpawns = cuboids.Where(x => x.CuboidType.HasFlag(CuboidMaskType.Player)).ToArray();
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
                var blueFlagSpawns = cuboids.Where(x => x.CuboidType.HasFlag(CuboidMaskType.BlueFlagSpawn)).ToArray();
                if (blueFlagSpawns.Length < 1) Debug.LogWarning($"Expected 1 blue flag player spawn cuboids found {blueFlagSpawns.Length}.");
                for (int i = 0; i < 1; ++i)
                {
                    var cuboid = blueFlagSpawns.ElementAtOrDefault(blueFlagSpawns.Length > 0 ? (i % blueFlagSpawns.Length) : 0);
                    var idx = cuboid ? Array.IndexOf(cuboids, cuboid) : -1;
                    writer.Write(idx);
                }

                writer.BaseStream.Position = 4;
                var redFlagSpawns = cuboids.Where(x => x.CuboidType.HasFlag(CuboidMaskType.RedFlagSpawn)).ToArray();
                if (redFlagSpawns.Length < 1) Debug.LogWarning($"Expected 1 red flag player spawn cuboids found {redFlagSpawns.Length}.");
                for (int i = 0; i < 1; ++i)
                {
                    var cuboid = redFlagSpawns.ElementAtOrDefault(redFlagSpawns.Length > 0 ? (i % redFlagSpawns.Length) : 0);
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

        if (PrefabOverride) return PrefabOverride;

        switch ((RCVersion, OClass))
        {
            case (0, 0):
                {
                    return null;
                }
            case (RCVER.DL, 9278):
                {
                    prefab = GetDLGadgetPickupPrefab();
                    break;
                }
            case (RCVER.DL, 9838):
                {
                    prefab = GetDLFlagBasePrefab();
                    break;
                }
            default:
                {
                    var prefabPath = Path.Combine(FolderNames.GetGlobalPrefabFolder(FolderNames.MobyFolder, RCVersion), $"{OClass}", $"{OClass}.prefab");
                    prefab = AssetDatabase.LoadAssetAtPath<GameObject>(prefabPath);
                    break;
                }
        }

        if (!prefab) prefab = UnityHelper.GetAssetPrefab(FolderNames.MobyFolder, OClass.ToString(), this.RCVersion);
        return prefab;
    }

    private GameObject GetDLFlagBasePrefab()
    {
        var teamId = GetPVarValue<int>(".Team");
        switch (teamId) // team
        {
            case 0: _tintColor = Color.blue; break;
            case 1: _tintColor = Color.red; break;
            case 2: _tintColor = Color.green; break;
            case 3: _tintColor = (Color.red * 0.75f) + (Color.green * 0.25f); break;
            default: _tintColor = Color.white; break;
        }

        return UnityHelper.GetAssetPrefab(FolderNames.MobyFolder, "8309", this.RCVersion, includeGlobal: true);
    }

    private GameObject GetDLGadgetPickupPrefab()
    {
        renderHandle.Rotation = Quaternion.Euler(0, 0, 90);
        renderHandle.Offset = Vector3.up * 0.5f;
        renderHandle.Scale = Vector3.one * 2f;

        var gadgetId = GetPVarValue<long>(".Gadget");
        switch (gadgetId)
        {
            case 0: return UnityHelper.GetAssetPrefab(FolderNames.MobyFolder, "9210", this.RCVersion, includeGlobal: true);
            case 2: return UnityHelper.GetAssetPrefab(FolderNames.MobyFolder, "4244", this.RCVersion, includeGlobal: true);
            case 3: return UnityHelper.GetAssetPrefab(FolderNames.MobyFolder, "4231", this.RCVersion, includeGlobal: true);
            case 4: return UnityHelper.GetAssetPrefab(FolderNames.MobyFolder, "6244", this.RCVersion, includeGlobal: true);
            case 5: return UnityHelper.GetAssetPrefab(FolderNames.MobyFolder, "4246", this.RCVersion, includeGlobal: true);
            case 6: return UnityHelper.GetAssetPrefab(FolderNames.MobyFolder, "8340", this.RCVersion, includeGlobal: true);
            case 7: return UnityHelper.GetAssetPrefab(FolderNames.MobyFolder, "4234", this.RCVersion, includeGlobal: true);
            case 12: return UnityHelper.GetAssetPrefab(FolderNames.MobyFolder, "8454", this.RCVersion, includeGlobal: true);
            case 13: return UnityHelper.GetAssetPrefab(FolderNames.MobyFolder, "9773", this.RCVersion, includeGlobal: true);
            case 16: return UnityHelper.GetAssetPrefab(FolderNames.MobyFolder, "4261", this.RCVersion, includeGlobal: true);
        }

        return null;
    }
}

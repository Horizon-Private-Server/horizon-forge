using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEngine;

[ExecuteInEditMode]
public class MapRender : MonoBehaviour
{
    const int MAP_RENDER_VERSION = 1;

    public int RenderScale = 512;
    public Color BackgroundColor = new Color32(0x66, 0x66, 0x66, 0);
    public Color Tint = new Color(0.5f, 0.5f, 0.5f, 1);
    [SerializeField, HideInInspector] public string SavePath = null;
    [SerializeField, HideInInspector] private int _version = 0;

    private void OnValidate()
    {
        Upgrade();
        UpdateCamera();
    }

    public void UpdateCamera()
    {
        // setup camera
        var camera = GetComponent<Camera>();
        camera.backgroundColor = BackgroundColor;
        camera.transform.localRotation = Quaternion.LookRotation(Vector3.down, Vector3.forward);
        camera.orthographicSize = transform.localScale.z / 2f;
        camera.nearClipPlane = 0.01f;
        camera.farClipPlane = transform.localScale.y;

        Shader.SetGlobalVector("_MapRenderCameraZRange", new Vector2(this.transform.position.y - this.transform.localScale.y, this.transform.position.y));
    }

    public void Write(BinaryWriter writer, int baseMapId, int racVersion, GameRegion region)
    {
        switch (racVersion)
        {
            case RCVER.UYA:
                {
                    // maps after bwcity use index 0
                    var mapIdx = baseMapId - (int)UYAMapIds.MP_Bakisi_Isles;
                    if (mapIdx > 6) mapIdx = 0;

                    ComputeRadarTransformation_UYA(transform.position, transform.localScale, out var offset, out var scale);

                    writer.BaseStream.Position = GetUYAMinimapTableOffset((UYAMapIds)baseMapId, region) + (0x20 * mapIdx);
                    writer.Write(scale.x);
                    writer.Write(scale.y);
                    writer.BaseStream.Position += 8;
                    writer.Write(offset.x);
                    writer.Write(offset.y);
                    break;
                }
            case RCVER.DL:
                {
                    var mapIdx = baseMapId - (int)DLMapIds.MP_Battledome;
                    writer.BaseStream.Position = 0x175C8 + (0x10 * mapIdx);

                    // write map render pos/scale
                    writer.Write(transform.position.x);
                    writer.Write(transform.position.z);
                    writer.Write(transform.localScale.x);
                    writer.Write(transform.localScale.z);
                    break;
                }
        }
    }

    public void Read(BinaryReader reader, int baseMapId, int racVersion, GameRegion region)
    {
        var mapConfig = FindObjectOfType<MapConfig>();
        var cuboids = mapConfig.GetCuboids();
        var yMin = cuboids.Min(x => x.transform.position.y);
        var yMax = cuboids.Max(x => x.transform.position.y);

        switch (racVersion)
        {
            case RCVER.UYA:
                {
                    // maps after bwcity use index 0
                    var mapIdx = baseMapId - (int)UYAMapIds.MP_Bakisi_Isles;
                    if (mapIdx < 0) break;
                    if (mapIdx > 6) mapIdx = 0;

                    reader.BaseStream.Position = GetUYAMinimapTableOffset((UYAMapIds)baseMapId, region) + (0x20 * mapIdx);
                    var scale = new Vector2(reader.ReadSingle(), reader.ReadSingle());
                    reader.BaseStream.Position += 8;
                    var offset = new Vector2(reader.ReadSingle(), reader.ReadSingle());

                    var min = TransformSSToWS_UYA(offset, scale, new Vector2(0, 1));
                    var max = TransformSSToWS_UYA(offset, scale, new Vector2(1, 0));

                    var rScale = new Vector2((max.x - min.x) * 0.8f, (max.z - min.z) * 0.7f);
                    var rOffset = new Vector2((min.x + max.x) * 0.5f, (max.z + min.z) * 0.5f);
                    rOffset.y += -rScale.y * 0.03f;

                    transform.position = new Vector3(rOffset.x, yMax + 50, rOffset.y);
                    transform.localScale = new Vector3(rScale.x, (yMax - yMin) + 100, rScale.y);
                    break;
                }
            case RCVER.DL:
                {
                    var mapIdx = baseMapId - (int)DLMapIds.MP_Battledome;
                    reader.BaseStream.Position = 0x175C8 + (0x10 * mapIdx);
                    transform.position = new Vector3(reader.ReadSingle(), yMax + 50, reader.ReadSingle());
                    transform.localScale = new Vector3(reader.ReadSingle(), (yMax - yMin) + 100, reader.ReadSingle());
                    break;
                }
        }

        InitializeVersion();
        UpdateCamera();
    }

    private int GetUYAMinimapTableOffset(UYAMapIds mapId, GameRegion region)
    {
        // code.0002.bin
        switch ((region, mapId))
        {
            case (GameRegion.NTSC, UYAMapIds.MP_Bakisi_Isles): return 0x203A0;
            case (GameRegion.NTSC, UYAMapIds.MP_Hoven_Gorge): return 0x20460;
            case (GameRegion.NTSC, UYAMapIds.MP_Outpost_X12): return 0x20460;
            case (GameRegion.NTSC, UYAMapIds.MP_Korgon_Outpost): return 0x203A0;
            case (GameRegion.NTSC, UYAMapIds.MP_Metropolis): return 0x20320;
            case (GameRegion.NTSC, UYAMapIds.MP_Blackwater_City): return 0x20360;
            case (GameRegion.NTSC, UYAMapIds.MP_Command_Center): return 0x201E0;
            case (GameRegion.NTSC, UYAMapIds.MP_Blackwater_Docks): return 0x20260;
            case (GameRegion.NTSC, UYAMapIds.MP_Aquatos_Sewers): return 0x201E0;
            case (GameRegion.NTSC, UYAMapIds.MP_Marcadia_Palace): return 0x201E0;

            case (GameRegion.PAL, UYAMapIds.MP_Bakisi_Isles): return 0x203E0;
            case (GameRegion.PAL, UYAMapIds.MP_Hoven_Gorge): return 0x204A0;
            case (GameRegion.PAL, UYAMapIds.MP_Outpost_X12): return 0x204A0;
            case (GameRegion.PAL, UYAMapIds.MP_Korgon_Outpost): return 0x203E0;
            case (GameRegion.PAL, UYAMapIds.MP_Metropolis): return 0x20360;
            case (GameRegion.PAL, UYAMapIds.MP_Blackwater_City): return 0x203A0;
            case (GameRegion.PAL, UYAMapIds.MP_Command_Center): return 0x20220;
            case (GameRegion.PAL, UYAMapIds.MP_Blackwater_Docks): return 0x202A0;
            case (GameRegion.PAL, UYAMapIds.MP_Aquatos_Sewers): return 0x20220;
            case (GameRegion.PAL, UYAMapIds.MP_Marcadia_Palace): return 0x20220;

            default: return 0;
        }
    }

    private Vector2 TransformWSToSS_UYA(Vector2 offset, Vector2 scale, Vector3 pos)
    {
        var x = ((pos.x - scale.x) / (offset.x - scale.x)) * 0.8f + 0.1f;
        var y = (pos.z - scale.y) / (offset.y - scale.y);
        y = (0.35f - y * 0.7f) + 0.35f + 0.17f;

        return new Vector2(Mathf.Clamp01(x), Mathf.Clamp01(y));
    }

    private Vector3 TransformSSToWS_UYA(Vector2 offset, Vector2 scale, Vector2 pos)
    {
        var x = (pos.x - 0.1f) / 0.8f;
        x = (x * (offset.x - scale.x)) + scale.x;

        var y = -((pos.y - 0.17f - 0.35f) - 0.35f) / 0.7f;
        y = (y * (offset.y - scale.y)) + scale.y;

        return new Vector3(x, 0, y);
    }

    private void ComputeRadarTransformation_UYA(Vector3 center, Vector3 size, out Vector2 offset, out Vector2 scale)
    {
        var scaledSize = new Vector2(size.x / 0.8f, size.z / 0.7f);
        var min = new Vector2(center.x - scaledSize.x * 0.5f, (center.z + scaledSize.y * 0.02f) - scaledSize.y * 0.5f);
        var max = new Vector2(center.x + scaledSize.x * 0.5f, (center.z + scaledSize.y * 0.02f) + scaledSize.y * 0.5f);

        var scaleX = (max.x + 9 * min.x) / 10f;
        var scaleY = (1.0316f * min.y + 0.1544f * max.y) / 1.1857f;
        var offsetX = (9 * max.x + min.x) / 10f;
        var offsetY = (1.1857f * max.y + 0.2429f * min.y) / 1.4266f;

        scale = new Vector2(scaleX, scaleY);
        offset = new Vector2(offsetX, offsetY);
    }

    private void InvertRadarTransformation_UYA(Vector2 offset, Vector2 scale, out Vector3 center, out Vector3 size)
    {
        center = new Vector3(0.5f * scale.x + 0.5f * offset.x, 0, (0.4999870287f * scale.y) + (0.49918663997f * offset.y));
        size = new Vector3(-1f * scale.x + 1f * offset.x, -(1.0000500597f * scale.y) + (0.99890269115f * offset.y));
    }

    #region Versioning

    public void InitializeVersion()
    {
        _version = MAP_RENDER_VERSION;
    }

    private void Upgrade()
    {
        // wait for import to finish before upgrading
        if (LevelImporterWindow.IsImporting) return;

        // upgrade
        if (_version < MAP_RENDER_VERSION)
        {
            while (_version < MAP_RENDER_VERSION)
            {
                RunMigration(_version + 1);
                ++_version;
            }

            Debug.Log($"Map Render upgraded to v{_version}");
            UnityHelper.MarkActiveSceneDirty();
        }
        else if (_version > MAP_RENDER_VERSION)
        {
            _version = MAP_RENDER_VERSION;
        }
    }

    private void RunMigration(int version)
    {
        switch (version)
        {
            case 1: // USE TIE/TFRAG/SHRUB layers
                {
                    var camera = GetComponent<Camera>();
                    if (camera)
                    {
                        camera.cullingMask = LayerMask.GetMask("TIE", "TFRAG", "SHRUB", "MAPRENDER");
                    }
                    break;
                }
        }
    }

    #endregion
}

using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using UnityEditor;
using UnityEngine;

public class CollectathonModeData : CustomModeData, IBuildHook
{
    public static readonly int COLLECTATHON_VERSION = 0;
    public static readonly int GOLDBOLT_OCLASS = 0x400E;

    public override DLCustomModeIds CustomMode => DLCustomModeIds.Collectathon;
    public override bool IsEnabled => Enabled && this.isActiveAndEnabled;
    public int CodeGenOrder => 99999999;

    public bool Enabled = true;

    public void Configure(BuildState state)
    {
        state.MobyOClasses.Add(13); // bolt (for gold bolt)
        state.MobyOClasses.Add(GOLDBOLT_OCLASS); // custom bolt
    }

    public override void Write(BinaryWriter writer)
    {
        var mapConfig = FindObjectOfType<MapConfig>();
        var mobys = mapConfig.GetMobys(RCVER.DL);
        var goldBoltMobys = mobys.Where(x => x.OClass == GOLDBOLT_OCLASS);
        var easyBolts = goldBoltMobys.Where(x => x.GetPVarValue<byte>(".Difficulty") == 0).ToArray();
        var mediumBolts = goldBoltMobys.Where(x => x.GetPVarValue<byte>(".Difficulty") == 1).ToArray();
        var hardBolts = goldBoltMobys.Where(x => x.GetPVarValue<byte>(".Difficulty") == 2).ToArray();
        var veryHardBolts = goldBoltMobys.Where(x => x.GetPVarValue<byte>(".Difficulty") == 3).ToArray();

        // write version
        writer.Write(COLLECTATHON_VERSION);

        // write bolt counts
        writer.Write((byte)easyBolts.Length);
        writer.Write((byte)mediumBolts.Length);
        writer.Write((byte)hardBolts.Length);
        writer.Write((byte)veryHardBolts.Length);

        // write bolts
        foreach (var bolt in easyBolts)
            writer.Write(bolt.Uid);
        foreach (var bolt in mediumBolts)
            writer.Write(bolt.Uid);
        foreach (var bolt in hardBolts)
            writer.Write(bolt.Uid);
        foreach (var bolt in veryHardBolts)
            writer.Write(bolt.Uid);
    }

    #region Menu Items

    [MenuItem("GameObject/Forge/Deadlocked/Collectathon/Create Bolt", priority = 10)]
    public static void CreateBolt()
    {
        var go = new GameObject("Gold Bolt Moby");
        var moby = go.AddComponent<Moby>();
        moby.OClass = GOLDBOLT_OCLASS;
        moby.RCVersion = RCVER.DL;
        moby.DrawDistance = 128;
        moby.UpdateDistance = 128;
        moby.Color = new Color(1, 1, 1, 0.5f);
        moby.PrefabOverride = UnityHelper.GetAssetPrefab(FolderNames.MobyFolder, "13", RCVER.DL, includeGlobal: true);
        moby.PrefabOverrideTransformation = Matrix4x4.TRS(Vector3.zero, Quaternion.FromToRotation(Vector3.right, Vector3.up), Vector3.one * 3);
        moby.InitializePVarReferences();
        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Deadlocked/Collectathon/Create Collectathon Data", priority = 10)]
    public static void CreateDataData()
    {
        var go = new GameObject("Collectathon");
        var survivalData = go.AddComponent<CollectathonModeData>();

        OnAfterCreateGameObject(go);
    }

    private static void OnAfterCreateGameObject(GameObject go)
    {
        // place under selected object
        // or try and spawn on top of scene camera
        if (Selection.activeGameObject)
            go.transform.SetParent(Selection.activeGameObject.transform, false);
        else if (SceneView.lastActiveSceneView.camera)
            go.transform.position = SceneView.lastActiveSceneView.camera.transform.position + (SceneView.lastActiveSceneView.camera.transform.forward * 5);

        Selection.activeGameObject = go;
    }

    #endregion

}

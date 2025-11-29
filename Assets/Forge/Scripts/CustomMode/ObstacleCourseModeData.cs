using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using UnityEditor;
using UnityEngine;

public class ObstacleCourseModeData : CustomModeData, ICodeGen, IBuildHook
{
    public static readonly int OBSTACLE_COURSE_VERSION = 0;


    public override DLCustomModeIds CustomMode => DLCustomModeIds.ObstacleCourse;
    public override bool IsEnabled => Enabled && this.isActiveAndEnabled;
    public int CodeGenOrder => 99999999;

    public bool Enabled = true;

    [Header("Game Settings")]
    public DLGameRules GameRule = DLGameRules.Deathmatch;
    public DLGadgetMask Gadgets;
    public bool Vehicles;

    [Range(0, 60)] public int Timelimit = 0;
    [Range(0, 10)] public int RespawnTime = 0;
    public bool Survivor = false;
    public bool SpawnWithChargeboots = true;
    public bool UnlimitedAmmo = true;
    public bool AutospawnWeapons = true;
    public bool CQLockdown = false;
    public bool CQHomenodes = false;
    public bool CQBoltCranks = false;

    private void OnValidate()
    {
        // make sure we always have the common code
        var commonCodeGen = FindObjectOfType<CommonCodeGen>(true);
        if (!commonCodeGen)
        {
            this.gameObject.AddComponent<CommonCodeGen>();
        }
    }

    #region CodeGen

    public void Configure(string buildFolder, CodeGenState state)
    {
        var mapConfig = FindObjectOfType<MapConfig>();
        var srcFolder = Path.Combine(buildFolder, FolderNames.CodeBuildSrcFolder);
        var includeFolder = Path.Combine(buildFolder, FolderNames.CodeBuildIncludeFolder);

        state.SeparateCodeFile = true;

        // copy base code
        CodeManager.CopySourceFilesIntoWorkingDirectory(FolderNames.GetCodeGenFolder(RCVER.DL, "obstacle"), buildFolder);

        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/config.o");
        state.LDFlags.Add("-DOBSTACLE");
    }

    public void Configure(BuildState state)
    {

    }
    
    public override void Write(BinaryWriter writer)
    {
        var mapConfig = FindObjectOfType<MapConfig>();
        var mobys = mapConfig.GetMobys(RCVER.DL);

        writer.Write(OBSTACLE_COURSE_VERSION);
        writer.Write((uint)Gadgets);
        writer.Write((byte)GameRule);
        writer.Write(Vehicles);
        writer.Write((byte)Timelimit);
        writer.Write((byte)RespawnTime);
        writer.Write(Survivor);
        writer.Write(SpawnWithChargeboots);
        writer.Write(UnlimitedAmmo);
        writer.Write(AutospawnWeapons);
        writer.Write(CQLockdown);
        writer.Write(CQHomenodes);
        writer.Write(CQBoltCranks);
    }

    #endregion

    #region Menu Items

    [MenuItem("GameObject/Forge/Deadlocked/Obstacle Course/Create Obstacle Course Data", priority = 10)]
    public static void CreateDataData()
    {
        var go = new GameObject("Obstacle Course");
        var survivalData = go.AddComponent<ObstacleCourseModeData>();

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

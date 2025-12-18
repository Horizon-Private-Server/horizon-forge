using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using UnityEditor;
using UnityEngine;

public class PropHuntModeData : CustomModeData, IBuildHook, ICodeGen
{
    private const string PROP_HELP_TEXT =
        "Configure available Prop Mobys by placing them at 0,0,0 with Group Id 123.\n\n"
        + "Adjust Moby around 0,0,0 to set the default spawn size/rotation/offset."
        ;

    private const string AREA_HELP_TEXT =
        "Props will be randomly spawned in each Cuboid configured in the given Area.\n\n"
        + "The spawning algorithm picks a random point at the top of the Cuboid and raycasts downwards. If it hits a walkable surface before it reaches the bottom of the Cuboid, it will place a cluster of Props."
        ;

    private const string MOBY_HELP_TEXT =
        "If you crash when loading it can be for one of the following reasons:\n"
        + "1. You have too many Mobys. Try disabling some and repacking.\n"
        + "2. You have one or more unstable Mobys (Hovership, Node Base, Hacker Orb, and more).\n"
        + "3. Skill issue. Try disabling Prop Mobys and repacking until the map loads."
        ;


    public override DLCustomModeIds CustomMode => DLCustomModeIds.HideAndSeek;
    public override bool IsEnabled => Enabled && this.isActiveAndEnabled;
    public int CodeGenOrder => 99999999;

    public bool Enabled = true;

    [Header("Spawn Rules")]
    [Min(1), Tooltip("Number of Props to randomly place when loading the map.")] public int NumberOfPropsToSpawn = 750;
    [Min(1), Tooltip("Increasing this value will result in more concentrated Prop placement.")] public int SpawnClusterSizeMin = 1;
    [Min(1), Tooltip("Increasing this value will result in more concentrated Prop placement.")] public int SpawnClusterSizeMax = 3;
    [Min(0), Tooltip("Increasing this value will spread out the Prop clusters.")] public float SpawnClusterSpread = 5;
    [Tooltip("If true, will sometimes spawn Props vertical magnet walls.")] public bool SpawnClusterAllowVerticalMagnetWalls = false;

    [Header("Prop Rules")]
    [Min(1), Tooltip("Maximum number of Mobys spawned/drawn at once. A large value may result in crashes.")] public int MaxNumberOfPropMobysCanDraw = 80;
    public float PropScaleMin = 2 / 3f;
    public float PropScaleMax = 3 / 2f;

    [Header("Scene Configuration")]
    [ReadOnly, Tooltip("Configure all Prop Mobys in scene with this Group Id.")]
    [HelpBox(PROP_HELP_TEXT, MessageType.Info, false)]
    public int PropMobyGroupId = 123;

    [Space(20)]
    [ReadOnly, Tooltip("Configure Area #0 with Cuboids defining where Props can spawn.")]
    [HelpBox(AREA_HELP_TEXT, MessageType.Info, false)]
    public int PropSpawnAreaIdx = 0;

    [Space(20)]
    [ReadOnly]
    [HelpBox(MOBY_HELP_TEXT, MessageType.Info, false)]
    public int PropMobyHelp = 0;

    public void Configure(BuildState state, BuildStateStage stage)
    {
        if (stage != BuildStateStage.BeforeBuild) return;

        var mapConfig = FindObjectOfType<MapConfig>();
        var mobys = mapConfig.GetMobys(RCVER.DL);

        // find all prop mobys
        // make sure their modebits are set to 15
        foreach (var moby in mobys)
        {
            if (moby.GroupId != PropMobyGroupId) continue;

            // disable update on spawn
            // fixes crashing on mobys with unstable update functions
            moby.ModeBits = 15;
        }
    }

    public override void Write(BinaryWriter writer)
    {

    }

    #region CodeGen

    public void Configure(string buildFolder, CodeGenState state)
    {
        var mapConfig = FindObjectOfType<MapConfig>();
        var area = mapConfig.GetAreaAtIndex(0);
        var srcFolder = Path.Combine(buildFolder, FolderNames.CodeBuildSrcFolder);
        var includeFolder = Path.Combine(buildFolder, FolderNames.CodeBuildIncludeFolder);

        // check that we have a spawn area
        if (!area)
        {
            Debug.LogError("Prop Hunt missing Area at index 0.");
            return;
        }

        // check that we have spawn cuboids
        if (!area.Cuboids.Any(c => c && c.isActiveAndEnabled))
        {
            Debug.LogError("Prop Hunt Area has no active Cuboids.");
            return;
        }

        // copy base code
        CodeManager.CopySourceFilesIntoWorkingDirectory(FolderNames.GetCodeGenFolder(RCVER.DL, "prophunt"), buildFolder);

        // 
        var octantSizeBits = GetTreeOctantSizeBits(area);
        var octantSize = (int)Math.Pow(2, octantSizeBits);
        Debug.Log($"Using prop hunt moby tree size {octantSize}x{octantSize}");

        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/prophunt.o");
        
        state.LDFlags.Add("-DPROPHUNT");
        state.LDFlags.Add($"-DPROP_MOBY_GROUP_ID={PropMobyGroupId}");
        state.LDFlags.Add($"-DTREE_OCTANT_SIZE_BITS={octantSizeBits}");
        state.LDFlags.Add("-DTREE_BITS_PER_AXIS=5");
        state.LDFlags.Add($"-DMAX_SPAWN_PROPS={NumberOfPropsToSpawn}");
        state.LDFlags.Add($"-DPROP_SPAWN_CLUSTER_MIN={SpawnClusterSizeMin}");
        state.LDFlags.Add($"-DPROP_SPAWN_CLUSTER_MAX={SpawnClusterSizeMax}");
        state.LDFlags.Add($"-DPROP_SPAWN_CLUSTER_SPREAD={SpawnClusterSpread}");
        if (!SpawnClusterAllowVerticalMagnetWalls) state.LDFlags.Add("-DPROP_SPAWN_CLUSTER_STRAIGHT");
        state.LDFlags.Add($"-DMAX_DRAW_PROPS={MaxNumberOfPropMobysCanDraw}");
        state.LDFlags.Add($"-DPROP_MAX_SPAWN_DIST={0x40}");
        state.LDFlags.Add($"-DPROP_MOBY_DRAW_DIST={0x30}");
        state.LDFlags.Add($"-DPROP_SCALE_RAND_MIN={PropScaleMin}");
        state.LDFlags.Add($"-DPROP_SCALE_RAND_MAX={PropScaleMax}");
        state.LDFlags.Add($"-DPROP_SOUND_PERIOD_SEC={15}");
        state.LDFlags.Add($"-DPROP_SOUND_PERIOD_DEC={3}");

        //state.LDFlags.Add("-DDEBUG_OCTANTS");
        //state.LDFlags.Add("-DDEBUG_PLAYER");
        //state.LDFlags.Add("-DDEBUG_DRAW_PROPS");
        //state.LDFlags.Add("-DDEBUG_FIND_PROPS");

        state.InitBody.Add($"propInit();");
        state.CleanupBody.Add("propCleanup();");
        state.MainBodyReady.Add("propTick();");
    }

    #endregion

    #region Menu Items

    [MenuItem("GameObject/Forge/Deadlocked/HnS/Create Prop Hunt Data", priority = 10)]
    public static void CreateDataData()
    {
        var go = new GameObject("Prop Hunt");
        var survivalData = go.AddComponent<PropHuntModeData>();

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

    private int GetTreeOctantSizeBits(Area area)
    {
        int minX = int.MaxValue, maxX = int.MinValue;
        int minZ = int.MaxValue, maxZ = int.MinValue;

        // get min/max for x/z axis
        foreach (var cuboid in area.Cuboids.Where(c => c && c.isActiveAndEnabled))
        {
            const float step = 0.01f;
            for (float z = -1f; z <= 1f; z += step)
            {
                for (float x = -1f; x <= 1f; x += step)
                {
                    var posTop = cuboid.transform.localToWorldMatrix.MultiplyPoint(new Vector3(x, 1, z));
                    var posBot = cuboid.transform.localToWorldMatrix.MultiplyPoint(new Vector3(x, -1, z));

                    // check for ground
                    if (!CollisionHelper.Raycast(posTop, posBot - posTop, Vector3.Distance(posTop, posBot), out var hitInfo, out var collisionId))
                        continue;

                    // check for walkable ground
                    if (collisionId != 2 && collisionId != 3 && collisionId != 7 && collisionId != 9 && collisionId != 10 && collisionId != 14 && collisionId != 15)
                        continue;

                    // if not mag wall, check slope
                    if (collisionId != 2)
                    {
                        var slope = Mathf.Abs(Mathf.Acos(Vector3.Dot(hitInfo.normal, Vector3.up))) * Mathf.Rad2Deg;
                        if (slope > 55) continue;
                    }

                    if (posTop.x < minX)
                        minX = (int)posTop.x;
                    if (posTop.x > maxX)
                        maxX = (int)(posTop.x + 1);
                    if (posTop.z < minZ)
                        minZ = (int)posTop.z;
                    if (posTop.z > maxZ)
                        maxZ = (int)(posTop.z + 1);
                }
            }
        }

        // increase octant size until they fit inside 5 bits
        int maxRange = Mathf.Max(maxX - minX, maxZ - minZ);
        int bits = 3;
        while ((maxRange >> bits) > 31)
            ++bits;

        return bits;
    }

}

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

    public static readonly int MOVER_OCLASS = 0x4002;
    public static readonly int CONTROLLER_OCLASS = 0x4003;
    public static readonly int GATE_OCLASS = 0x4004;
    public static readonly int MESSAGER_OCLASS = 0x4005;
    public static readonly int CHECKPOINT_MANAGER_OCLASS = 0x4007;
    public static readonly int CHECKPOINT_OCLASS = 0x4008;
    public static readonly int LASERBEAM_OCLASS = 0x4009;
    public static readonly int LASER_OCLASS = 0x400A;
    public static readonly int PVARPOKE_OCLASS = 0x400B;
    public static readonly int BLIP_OCLASS = 0x400C;
    public static readonly int DUMMY_OCLASS = 0x400D;
    public static readonly int COUNTER_OCLASS = 0x400F;

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
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/gate.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/checkpoint.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/messager.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/mover.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/controller.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/laserbeam.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/laser.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/pvarpoke.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/maputils.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/hackerorb.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/blip.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/dummy.o");

        state.LDFlags.Add("-DGATE");
        state.LDFlags.Add("-DOBSTACLE");

        state.Includes.Add("#include \"maputils.h\"");
        state.Includes.Add("#include \"gate.h\"");
        state.Includes.Add("#include \"messager.h\"");
        state.Includes.Add("#include \"checkpoint.h\"");
        state.Includes.Add("#include \"laser.h\"");
        state.Includes.Add("#include \"controller.h\"");
        state.Includes.Add("#include \"pvarpoke.h\"");
        state.Includes.Add("#include \"mover.h\"");
        state.Includes.Add("#include \"hackerorb.h\"");
        state.Includes.Add("#include \"blip.h\"");
        state.Includes.Add("#include \"dummy.h\"");

        state.Functions.Add($"//--------------------------------------------------------------------------\r\nvoid onBeforeUpdateHeroes(void)\r\n{{\r\n  gateSetCollision(1);\r\n  ((void (*)())0x005ce1d8)();\r\n}}\r\n");
        state.Functions.Add($"//--------------------------------------------------------------------------\r\nvoid onBeforeUpdateHeroes2(u32 a0)\r\n{{\r\n  gateSetCollision(1);\r\n  ((void (*)(u32))0x0059b320)(a0);\r\n}}\r\n");

        state.InitBody.Add($"moverInit();");
        state.InitBody.Add($"controllerInit();");
        state.InitBody.Add($"gateInit();");
        state.InitBody.Add($"messagerInit();");
        state.InitBody.Add($"checkpointInit();");
        state.InitBody.Add($"laserInit();");
        state.InitBody.Add($"pvarpokeInit();");
        state.InitBody.Add($"hackerorbInit();");
        state.InitBody.Add($"blipInit();");
        state.InitBody.Add($"dummyInit();");

        // get gubers
        state.GetGuberCase.Add("case GATE_OCLASS: return gateGetGuber(moby);");
        state.GetGuberCase.Add("case MOVER_OCLASS: return moverGetGuber(moby);");
        state.GetGuberCase.Add("case CONTROLLER_OCLASS: return controllerGetGuber(moby);");
        state.GetGuberCase.Add("case CHECKPOINT_MANAGER_OCLASS: return checkpointGetGuber(moby);");
        state.GetGuberCase.Add("case CHECKPOINT_OCLASS: return checkpointGetGuber(moby);");
        state.GetGuberCase.Add("case DUMMY_OCLASS: return dummyGetGuber(moby);");
        state.GetGuberCase.Add("case MOBY_ID_HACKER_ORB: return hackerorbGetGuber(moby);");

        // handle events
        state.HandleGuberEventCase.Add("case GATE_OCLASS: gateHandleEvent(moby, event); break;");
        state.HandleGuberEventCase.Add("case MOVER_OCLASS: moverHandleEvent(moby, event); break;");
        state.HandleGuberEventCase.Add("case CONTROLLER_OCLASS: controllerHandleEvent(moby, event); break;");
        state.HandleGuberEventCase.Add("case CHECKPOINT_MANAGER_OCLASS: checkpointHandleEvent(moby, event); break;");
        state.HandleGuberEventCase.Add("case CHECKPOINT_OCLASS: checkpointHandleEvent(moby, event); break;");
        state.HandleGuberEventCase.Add("case DUMMY_OCLASS: dummyHandleEvent(moby, event); break;");
        state.HandleGuberEventCase.Add("case MOBY_ID_HACKER_ORB: hackerorbHandleEvent(moby, event); break;");

        state.InitBody.Add($"HOOK_JAL(0x003bd854, &onBeforeUpdateHeroes);");
        state.InitBody.Add($"HOOK_JAL(0x0051f648, &onBeforeUpdateHeroes2);");

        state.MainBodyReady.Add("checkpointStart();");
        state.MainBodyReady.Add("moverStart();");
        state.MainBodyReady.Add("controllerStart();");
        state.MainBodyReady.Add("gateStart();");
        state.MainBodyReady.Add("laserStart();");
        state.MainBodyReady.Add("pvarpokeStart();");
        state.MainBodyReady.Add("dummyStart();");
    }

    public void Configure(BuildState state)
    {
        state.MobyOClasses.Add(RaidsModeData.LASERBEAM_OCLASS);
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

    [MenuItem("GameObject/Forge/Obstacle Course/Create Obstacle Course Data", priority = 10)]
    public static void CreateDataData()
    {
        var go = new GameObject("Obstacle Course");
        var survivalData = go.AddComponent<ObstacleCourseModeData>();

        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Obstacle Course/Mover Moby", priority = 10)]
    public static void CreateMoverMoby()
    {
        var go = new GameObject("Mover");
        var moby = go.AddComponent<Moby>();
        moby.OClass = MOVER_OCLASS;
        moby.RCVersion = RCVER.DL;
        moby.UpdateDistance = 255;
        moby.PrefabOverride = UnityHelper.GetRaidsPrefab("Mover");
        moby.InitializePVarReferences();
        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Obstacle Course/Controller Moby", priority = 10)]
    public static void CreateControllerMoby()
    {
        var go = new GameObject("Controller");
        var moby = go.AddComponent<Moby>();
        moby.OClass = CONTROLLER_OCLASS;
        moby.RCVersion = RCVER.DL;
        moby.UpdateDistance = 255;
        moby.PrefabOverride = UnityHelper.GetRaidsPrefab("Controller");
        moby.InitializePVarReferences();
        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Obstacle Course/Counter Moby", priority = 10)]
    public static void CreateCounterMoby()
    {
        var go = new GameObject("Counter");
        var moby = go.AddComponent<Moby>();
        moby.OClass = COUNTER_OCLASS;
        moby.RCVersion = RCVER.DL;
        moby.PrefabOverride = UnityHelper.GetRaidsPrefab("Counter");
        moby.InitializePVarReferences();
        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Obstacle Course/PVar Poke Moby", priority = 10)]
    public static void CreatePVarPokeMoby()
    {
        var go = new GameObject("PVar Poke");
        var moby = go.AddComponent<Moby>();
        moby.OClass = PVARPOKE_OCLASS;
        moby.RCVersion = RCVER.DL;
        moby.UpdateDistance = 255;
        moby.PrefabOverride = UnityHelper.GetRaidsPrefab("PVarPoke");
        moby.InitializePVarReferences();
        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Obstacle Course/Gate Moby", priority = 10)]
    public static void CreateGateMoby()
    {
        var mobyBin = Convert.FromBase64String("sAAAAAEAAAEAAAAAAQD/AMAAAAAAAQAAAAEAAAABAABgAgAAq6qqPAAAAAAABQAAnFNJvZxTybycU8m8PMQHR3h4eIAAAAAQYAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAgqh7vgAAAACcU8m8N8QHRwH/AAAAAAAAJAAAADAAAAACABQAAAAAAAAAAAAAAAAAAgIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQAQAACAAEAJABAAANAwIIAAAAAAAAAAAQAAAAIAAAAAAAAKAAYAAAAAAAYABgAAAAAABgAKAAAAAAAKAAoAAAAAIBbwIAA28BAwBvAwECbwAAAAAAAAAAAAAAAAAAAADCgAh1AAAAEAAQABAAEAAAAAAAAAAAABAAEAAQABAAAAAAAAAtgQRu/gQBAACCBAOFhggHAQEBAAAAAAAxgQRskv8AAAQAAAAEAKBBAAAAAAAAAAAAAAAACAAAAAAAAAD/////AAAAAAYAAAAAAAAAAAAAAAAAAAA0AAAAAAAAAAEAAAAAAAgAAAAIACAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA9AAAAAAAAAAAAGAAYAAAAPQAAAAAAAAAAACgAGAAAAD0AAAAAAAAAAAAoACgAAAA9AAAAAAAAAAAAGAAoAAAAPQAAAAAgAAAAACgAGAAAAD0AAAAAIAAAAAAYABgAAAA9AAAAACAAAAAAGAAoP8AAPQAAAAAgAAAAACgAKD/AAAAAAAAAAAAAAAAAAAA/wAAAAAAAAAAAAAAAAAAAP8AAAD/AP8A/wD/AAAAAAD///////////////9AAQCA");

        var go = new GameObject("Gate");
        var moby = go.AddComponent<Moby>();
        moby.OClass = GATE_OCLASS;
        moby.RCVersion = RCVER.DL;
        moby.UpdateDistance = 255;
        moby.Color = new Color(1, 0, 0, 0.5f);
        moby.PrefabOverride = UnityHelper.GetRaidsPrefab("Gate");
        moby.InitializePVarReferences();

        // create gate core.bin (has collision)
        var mobyFolder = FolderNames.GetLocalAssetFolder(FolderNames.MobyFolder, RCVER.DL);
        if (Directory.Exists(mobyFolder))
        {
            var gateMobyFolder = Path.Combine(mobyFolder, GATE_OCLASS.ToString());
            if (!Directory.Exists(gateMobyFolder))
            {
                Directory.CreateDirectory(gateMobyFolder);
                File.WriteAllBytes(Path.Combine(gateMobyFolder, "core.bin"), mobyBin);
            }
        }

        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Obstacle Course/Messager Moby", priority = 10)]
    public static void CreateMessagerMoby()
    {
        var go = new GameObject("Messager");
        var moby = go.AddComponent<Moby>();
        moby.OClass = MESSAGER_OCLASS;
        moby.RCVersion = RCVER.DL;
        moby.UpdateDistance = 255;
        moby.PrefabOverride = UnityHelper.GetRaidsPrefab("Messager");
        moby.InitializePVarReferences();
        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Obstacle Course/Checkpoint Manager Moby", priority = 10)]
    public static void CreateCheckpointManagerMoby()
    {
        var go = new GameObject("Checkpoint Manager");
        var moby = go.AddComponent<Moby>();
        moby.OClass = CHECKPOINT_MANAGER_OCLASS;
        moby.RCVersion = RCVER.DL;
        moby.UpdateDistance = 255;
        //moby.PrefabOverride = UnityHelper.GetRaidsPrefab("NPC Controller");
        moby.InitializePVarReferences();
        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Obstacle Course/Checkpoint Moby", priority = 10)]
    public static void CreateCheckpointMoby()
    {
        var go = new GameObject("Checkpoint");
        var moby = go.AddComponent<Moby>();
        moby.OClass = CHECKPOINT_OCLASS;
        moby.RCVersion = RCVER.DL;
        moby.UpdateDistance = 255;
        moby.Color = new Color(1, 1, 1, 0.5f);
        moby.PrefabOverride = UnityHelper.GetRaidsPrefab("Checkpoint");
        moby.InitializePVarReferences();
        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Obstacle Course/Laser (Tripwire) Moby", priority = 10)]
    public static void CreateLaserMoby()
    {
        var go = new GameObject("Laser (Tripwire)");
        var moby = go.AddComponent<Moby>();
        moby.OClass = LASER_OCLASS;
        moby.RCVersion = RCVER.DL;
        moby.UpdateDistance = 255;
        moby.Color = new Color(1, 1, 1, 0.5f);
        moby.PrefabOverride = UnityHelper.GetRaidsPrefab("Laser");
        moby.InitializePVarReferences();
        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Obstacle Course/Radar Blip Moby", priority = 10)]
    public static void CreateRadarBlipMoby()
    {
        var go = new GameObject("Radar Blip Moby");
        var moby = go.AddComponent<Moby>();
        moby.OClass = BLIP_OCLASS;
        moby.RCVersion = RCVER.DL;
        moby.UpdateDistance = 255;
        moby.Color = new Color(1, 1, 1, 0.5f);
        moby.PrefabOverride = UnityHelper.GetRaidsPrefab("Radar Blip");
        moby.InitializePVarReferences();
        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Obstacle Course/Health Proxy Moby", priority = 10)]
    public static void CreateHealthProxyMoby()
    {
        var go = new GameObject("Health Proxy Moby");
        var moby = go.AddComponent<Moby>();
        moby.OClass = DUMMY_OCLASS;
        moby.RCVersion = RCVER.DL;
        moby.UpdateDistance = 255;
        moby.Color = new Color(1, 1, 1, 0.5f);
        moby.PrefabOverride = UnityHelper.GetRaidsPrefab("Health Proxy");
        moby.InitializePVarReferences();
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

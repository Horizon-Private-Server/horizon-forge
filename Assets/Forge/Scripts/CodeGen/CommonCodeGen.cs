using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEngine;

public class CommonCodeGen : MonoBehaviour, ICodeGen, IBuildHook
{
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
    public static readonly int SOULCOLLECTOR_OCLASS = 0x4010;
    public static readonly int LAUNCHSTREAM_OCLASS = 0x4011;
    public static readonly int HOLDER_OCLASS = 0x4012;


    public bool IsEnabled => true;
    public int CodeGenOrder => 0;

	[Header("Custom Mobys")]
	public bool CheckpointEnabled = true;
	public bool GateEnabled = true;
	public bool MessagerEnabled = true;
	public bool MoverEnabled = true;
	public bool PlatformEnabled = true;
	public bool ControllerEnabled = true;
	public bool LaserbeamEnabled = true;
	public bool LaserEnabled = true;
	public bool PvarPokeEnabled = true;
	public bool HackerorbEnabled = true;
	public bool RadarBlipEnabled = true;
	public bool HealthProxyEnabled = true;
	public bool LaunchStreamEnabled = true;
	public bool HoldableProxyEnabled = true;
	public bool SoulCollectorEnabled = true;

    public void Configure(string buildFolder, CodeGenState state)
    {
        var mapConfig = FindObjectOfType<MapConfig>();
        var srcFolder = Path.Combine(buildFolder, FolderNames.CodeBuildSrcFolder);
        var includeFolder = Path.Combine(buildFolder, FolderNames.CodeBuildIncludeFolder);

        //state.SeparateCodeFile = true;

        // copy base code
        CodeManager.CopySourceFilesIntoWorkingDirectory(FolderNames.GetCodeGenFolder(RCVER.DL, "common"), buildFolder);

        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/maputils.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/window.o");

        state.LDFlags.Add("-DGATE");
        state.LDFlags.Add("-DSOULCOLLECTOR");

        state.Includes.Add("#include \"maputils.h\"");

        if (CheckpointEnabled)
        {
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/checkpoint.o");
            state.Includes.Add("#include \"checkpoint.h\"");
            state.InitBody.Add($"checkpointInit();");
            state.GetGuberCase.Add("case CHECKPOINT_MANAGER_OCLASS: return checkpointGetGuber(moby);");
            state.GetGuberCase.Add("case CHECKPOINT_OCLASS: return checkpointGetGuber(moby);");
            state.HandleGuberEventCase.Add("case CHECKPOINT_MANAGER_OCLASS: checkpointHandleEvent(moby, event); break;");
            state.HandleGuberEventCase.Add("case CHECKPOINT_OCLASS: checkpointHandleEvent(moby, event); break;");
            state.MainBodyReady.Add("checkpointStart();");
        }

        if (MoverEnabled)
        {
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/mover.o");
            state.Includes.Add("#include \"mover.h\"");
            state.InitBody.Add($"moverInit();");
            state.GetGuberCase.Add("case MOVER_OCLASS: return moverGetGuber(moby);");
            state.HandleGuberEventCase.Add("case MOVER_OCLASS: moverHandleEvent(moby, event); break;");
            state.MainBodyReady.Add("moverStart();");
        }

        if (PlatformEnabled)
        {
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/platform.o");
            state.Includes.Add("#include \"platform.h\"");
            state.InitBody.Add($"platformInit();");
            state.GetGuberCase.Add("case PLATFORM_FLIPPER_MOBY_OCLASS: return platformGetGuber(moby);");
            state.GetGuberCase.Add("case PLATFORM_PIVOT_MOBY_OCLASS: return platformGetGuber(moby);");
            state.HandleGuberEventCase.Add("case PLATFORM_FLIPPER_MOBY_OCLASS: platformHandleEvent(moby, event); break;");
            state.HandleGuberEventCase.Add("case PLATFORM_PIVOT_MOBY_OCLASS: platformHandleEvent(moby, event); break;");
        }

        if (ControllerEnabled)
        {
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/controller.o");
            state.Includes.Add("#include \"controller.h\"");
            state.InitBody.Add($"controllerInit();");
            state.GetGuberCase.Add("case CONTROLLER_OCLASS: return controllerGetGuber(moby);");
            state.HandleGuberEventCase.Add("case CONTROLLER_OCLASS: controllerHandleEvent(moby, event); break;");
            state.MainBodyReady.Add("controllerStart();");
        }

        if (GateEnabled)
        {
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/gate.o");
            state.Includes.Add("#include \"gate.h\"");
            state.Functions.Add($"//--------------------------------------------------------------------------\r\nvoid onBeforeUpdateHeroes(void)\r\n{{\r\n  gateSetCollision(1);\r\n  ((void (*)())0x005ce1d8)();\r\n}}\r\n");
            state.Functions.Add($"//--------------------------------------------------------------------------\r\nvoid onBeforeUpdateHeroes2(u32 a0)\r\n{{\r\n  gateSetCollision(1);\r\n  ((void (*)(u32))0x0059b320)(a0);\r\n}}\r\n");
            state.InitBody.Add($"gateInit();");
            state.InitBody.Add($"HOOK_JAL(0x003bd854, &onBeforeUpdateHeroes);");
            state.InitBody.Add($"HOOK_JAL(0x0051f648, &onBeforeUpdateHeroes2);");
            state.GetGuberCase.Add("case GATE_OCLASS: return gateGetGuber(moby);");
            state.HandleGuberEventCase.Add("case GATE_OCLASS: gateHandleEvent(moby, event); break;");
            state.MainBodyReady.Add("gateStart();");
        }

        if (MessagerEnabled)
        {
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/messager.o");
            state.Includes.Add("#include \"messager.h\"");
            state.InitBody.Add($"messagerInit();");
        }

        if (LaserbeamEnabled || LaserEnabled)
        {
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/laserbeam.o");
            state.Includes.Add("#include \"laserbeam.h\"");
        }

        if (LaserEnabled)
        {
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/laser.o");
            state.Includes.Add("#include \"laser.h\"");
            state.InitBody.Add($"laserInit();");
            state.MainBodyReady.Add("laserStart();");
        }

        if (PvarPokeEnabled)
        {
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/pvarpoke.o");
            state.Includes.Add("#include \"pvarpoke.h\"");
            state.InitBody.Add($"pvarpokeInit();");
            state.MainBodyReady.Add("pvarpokeStart();");
        }

        if (HackerorbEnabled)
        {
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/hackerorb.o");
            state.Includes.Add("#include \"hackerorb.h\"");
            state.InitBody.Add($"hackerorbInit();");
            state.GetGuberCase.Add("case MOBY_ID_HACKER_ORB: return hackerorbGetGuber(moby);");
            state.HandleGuberEventCase.Add("case MOBY_ID_HACKER_ORB: hackerorbHandleEvent(moby, event); break;");
        }

        if (RadarBlipEnabled)
        {
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/blip.o");
            state.Includes.Add("#include \"blip.h\"");
            state.InitBody.Add($"blipInit();");
        }

        if (HealthProxyEnabled)
        {
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/dummy.o");
            state.Includes.Add("#include \"dummy.h\"");
            state.InitBody.Add($"dummyInit();");
            state.GetGuberCase.Add("case DUMMY_OCLASS: return dummyGetGuber(moby);");
            state.HandleGuberEventCase.Add("case DUMMY_OCLASS: dummyHandleEvent(moby, event); break;");
            state.MainBodyReady.Add("dummyStart();");
        }

        if (LaunchStreamEnabled)
        {
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/launchstream.o");
            state.Includes.Add("#include \"launchstream.h\"");
            state.InitBody.Add($"launchstreamInit();");
        }

        if (HoldableProxyEnabled)
        {
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/holder.o");
            state.Includes.Add("#include \"holder.h\"");
            state.InitBody.Add($"holderInit();");
            state.GetGuberCase.Add("case HOLDER_OCLASS: return holderGetGuber(moby);");
            state.HandleGuberEventCase.Add("case HOLDER_OCLASS: holderHandleEvent(moby, event); break;");
            state.MainBodyReady.Add("holderStart();");
        }

        if (SoulCollectorEnabled)
        {
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/soulcollector.o");
            state.Includes.Add("#include \"soulcollector.h\"");
            state.InitBody.Add($"soulcollectorInit();");
            state.GetGuberCase.Add("case SOULCOLLECTOR_OCLASS: return soulcollectorGetGuber(moby);");
            state.HandleGuberEventCase.Add("case SOULCOLLECTOR_OCLASS: soulcollectorHandleEvent(moby, event); break;");
            state.MainBodyReady.Add("soulcollectorStart();");
        }

    }

    public void Configure(BuildState state, BuildStateStage stage)
    {
        if (stage != BuildStateStage.BeforeBuild) return;

        state.MobyOClasses.Add(RaidsModeData.LASERBEAM_OCLASS);
    }

    #region Menu Items

    [MenuItem("GameObject/Forge/Deadlocked/Custom Moby/Mover Moby", priority = 10)]
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

    [MenuItem("GameObject/Forge/Deadlocked/Custom Moby/Checkpoint Manager Moby", priority = 10)]
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

    [MenuItem("GameObject/Forge/Deadlocked/Custom Moby/Checkpoint Moby", priority = 10)]
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

    [MenuItem("GameObject/Forge/Deadlocked/Custom Moby/Controller Moby", priority = 10)]
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

    [MenuItem("GameObject/Forge/Deadlocked/Custom Moby/Counter Moby", priority = 10)]
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

    [MenuItem("GameObject/Forge/Deadlocked/Custom Moby/PVar Poke Moby", priority = 10)]
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

    [MenuItem("GameObject/Forge/Deadlocked/Custom Moby/Gate Moby", priority = 10)]
    public static GameObject CreateGateMoby()
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
        return go;
    }

    [MenuItem("GameObject/Forge/Deadlocked/Custom Moby/Messager Moby", priority = 10)]
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

    [MenuItem("GameObject/Forge/Deadlocked/Custom Moby/Laser (Tripwire) Moby", priority = 10)]
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

    [MenuItem("GameObject/Forge/Deadlocked/Custom Moby/Radar Blip Moby", priority = 10)]
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

    [MenuItem("GameObject/Forge/Deadlocked/Custom Moby/Health Proxy Moby", priority = 10)]
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

    [MenuItem("GameObject/Forge/Deadlocked/Custom Moby/Launch Stream Moby", priority = 10)]
    public static void CreateLaunchStreamMoby()
    {
        var go = new GameObject("Launch Stream Moby");
        var moby = go.AddComponent<Moby>();
        moby.OClass = LAUNCHSTREAM_OCLASS;
        moby.RCVersion = RCVER.DL;
        moby.UpdateDistance = 255;
        moby.Color = new Color(1, 1, 1, 0.5f);
        moby.PrefabOverride = UnityHelper.GetRaidsPrefab("Launch Stream");
        moby.InitializePVarReferences();
        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Deadlocked/Custom Moby/Holdable Proxy Moby", priority = 10)]
    public static void CreateHoldableProxyMoby()
    {
        var go = new GameObject("Holdable Proxy Moby");
        var moby = go.AddComponent<Moby>();
        moby.OClass = HOLDER_OCLASS;
        moby.RCVersion = RCVER.DL;
        moby.UpdateDistance = 255;
        moby.Color = new Color(1, 1, 1, 0.5f);
        moby.PrefabOverride = UnityHelper.GetRaidsPrefab("Holdable Proxy");
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

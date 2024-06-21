using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEngine;

public class SNDModeData : CustomModeData
{
    public static GameObject DefendTeamPrefab;

    public override DLCustomModeIds CustomMode => DLCustomModeIds.SearchAndDestroy;
    public override bool IsEnabled => DefendTeamSpawn && AttackTeamSpawn && PackSpawn && BombSite1 && BombSite2;

    public bool Enabled = true;
    public Transform DefendTeamSpawn;
    public Transform AttackTeamSpawn;
    public Transform PackSpawn;
    public Transform BombSite1;
    public Transform BombSite2;

    [MenuItem("Forge/Custom Modes/Create Search and Destroy Data")]
    public static void CreateNewDataObject()
    {
        var go = new GameObject("Search and Destroy");
        var sndData = go.AddComponent<SNDModeData>();

        // update references
        sndData.DefendTeamSpawn = CreateChild("Defend Spawn", go, UnityHelper.GetSNDPrefab("SNDDefendTeamSpawn")).transform;
        sndData.AttackTeamSpawn = CreateChild("Attack Spawn", go, UnityHelper.GetSNDPrefab("SNDAttackTeamSpawn")).transform;
        sndData.PackSpawn = CreateChild("Pack Spawn", go, UnityHelper.GetSNDPrefab("SNDPackSpawn")).transform;
        sndData.BombSite1 = CreateChild("Bombsite 1", go, UnityHelper.GetSNDPrefab("SNDBombsite")).transform;
        sndData.BombSite2 = CreateChild("Bombsite 2", go, UnityHelper.GetSNDPrefab("SNDBombsite")).transform;

        // create default positions
        var mapConfig = FindObjectOfType<MapConfig>();
        if (!mapConfig) return;
        
        var mpConfigMoby = mapConfig.GetMobys().FirstOrDefault(x => x.OClass == 0x106a);
        if (mpConfigMoby)
        {
            CopyTransform(mpConfigMoby.PVarMobyRefs[384 / 4] ? mpConfigMoby.PVarMobyRefs[384 / 4].transform : null, sndData.BombSite1);
            CopyTransform(mpConfigMoby.PVarMobyRefs[388 / 4] ? mpConfigMoby.PVarMobyRefs[388 / 4].transform : null, sndData.BombSite2);
        }

        var cuboids = mapConfig.GetCuboids();
        var blueSpawnCuboid = cuboids?.FirstOrDefault(x => (x.Type == CuboidType.Player || x.Type == CuboidType.None) && x.Subtype == CuboidSubType.BlueFlagSpawn);
        var redSpawnCuboid = cuboids?.FirstOrDefault(x => (x.Type == CuboidType.Player || x.Type == CuboidType.None) && x.Subtype == CuboidSubType.RedFlagSpawn);

        CopyTransform(blueSpawnCuboid ? blueSpawnCuboid.transform : null, sndData.DefendTeamSpawn);
        CopyTransform(redSpawnCuboid ? redSpawnCuboid.transform : null, sndData.AttackTeamSpawn);
        CopyTransform(sndData.AttackTeamSpawn, sndData.PackSpawn);
        sndData.PackSpawn.position += (sndData.PackSpawn.forward * 10f) + (Vector3.up * 2f);
    }

    public override void Write(BinaryWriter writer)
    {
        writer.WriteVectorXZY(DefendTeamSpawn.position);
        writer.Write(-(DefendTeamSpawn.rotation.eulerAngles.y - 90) * Mathf.Deg2Rad);
        writer.WriteVectorXZY(AttackTeamSpawn.position);
        writer.Write(-(AttackTeamSpawn.rotation.eulerAngles.y - 90) * Mathf.Deg2Rad);
        writer.WriteVectorXZY(BombSite1.position);
        writer.Write(0f);
        writer.WriteVectorXZY(BombSite2.position);
        writer.Write(0f);
        writer.WriteVectorXZY(PackSpawn.position);
        writer.Write(0f);
    }

    private static GameObject CreateChild(string name, GameObject parent, GameObject prefab)
    {
        var go = new GameObject(name);
        go.transform.SetParent(parent.transform, false);

        var renderHandle = go.AddComponent<PrefabRenderHandle>();
        renderHandle.Prefab = prefab;

        return go;
    }

    private static void CopyTransform(Transform source, Transform destination)
    {
        destination.position = source ? source.position : Vector3.zero;
        destination.rotation = source ? source.rotation : Quaternion.identity;
    }
}

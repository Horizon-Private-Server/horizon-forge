using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using UnityEngine;

public class SurvivalSpecialRoundDef : MonoBehaviour
{
    public int MinRound = 25;
    public int RepeatEveryNRounds = 25;
    public int RepeatCount = 0;
    public float SpawnCountFactor = 1;
    public float SpawnRateFactor = 1;
    [Range(0, SurvivalModeData.SURVIVAL_MAX_SPAWNED_MOBS)] public int MaxSpawnedAtOnce = SurvivalModeData.SURVIVAL_MAX_SPAWNED_MOBS;
    public bool UnlimitedPostRoundTime = false;
    public bool DisableDrops = false;

    public List<SurvivalMobDef> MobToSpawn = new List<SurvivalMobDef>();

    public string Name => gameObject.name;

    private void OnValidate()
    {
        // limit mobs to 4
        // probably should increase this now
        while (MobToSpawn != null && MobToSpawn.Count > 4) MobToSpawn.RemoveAt(4);
    }

    public string GetDef(SurvivalModeData survivalData)
    {
        var sb = new StringBuilder();
        var enabledMobs = survivalData.GetEnabledMobs();

        var maxSpawnedAtOnce = MaxSpawnedAtOnce <= 0 ? SurvivalModeData.SURVIVAL_MAX_SPAWNED_MOBS : Math.Clamp(MaxSpawnedAtOnce, 1, SurvivalModeData.SURVIVAL_MAX_SPAWNED_MOBS);
        var name = Name?.Replace("\"", "") ?? string.Empty;
        if (name.Length >= 32)
            name = name.Substring(0, 31); 

        var spawnParamIdxs = new List<int>();
        foreach (var mobDef in MobToSpawn)
        {
            // check mob is enabled
            var spawnParam = enabledMobs.FirstOrDefault(x => x == mobDef);
            if (spawnParam == null)
                continue;

            spawnParamIdxs.Add(Array.IndexOf(enabledMobs, spawnParam));
        }

        sb.AppendLine("\t{");
        sb.AppendLine($"\t\t.Name = \"{name}\",");
        sb.AppendLine($"\t\t.MinRound = {MinRound},");
        sb.AppendLine($"\t\t.RepeatEveryNRounds = {RepeatEveryNRounds},");
        sb.AppendLine($"\t\t.RepeatCount = {RepeatCount},");
        sb.AppendLine($"\t\t.UnlimitedPostRoundTime = {(UnlimitedPostRoundTime ? 1 : 0)},");
        sb.AppendLine($"\t\t.DisableDrops = {(DisableDrops ? 1 : 0)},");
        sb.AppendLine($"\t\t.MaxSpawnedAtOnce = {maxSpawnedAtOnce},");
        sb.AppendLine($"\t\t.SpawnCountFactor = {SpawnCountFactor.ToInvariantCulture()},");
        sb.AppendLine($"\t\t.SpawnRateFactor = {SpawnRateFactor.ToInvariantCulture()},");
        sb.AppendLine($"\t\t.SpawnParamCount = {Math.Min(4, spawnParamIdxs.Count)},");
        sb.AppendLine("\t\t.SpawnParamIds = {");
        for (int i = 0; i < 4; ++i)
            sb.AppendLine($"\t\t\t{(i < spawnParamIdxs.Count ? spawnParamIdxs[i] : -1)},");
        sb.AppendLine("\t\t}");
        sb.AppendLine("\t},");

        return sb.ToString();
    }

}

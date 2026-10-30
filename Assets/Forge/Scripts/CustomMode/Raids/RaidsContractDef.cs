using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using UnityEngine;

[AddComponentMenu("Forge/Raids/Raids Contract Definition")]
[DisallowMultipleComponent]
public class RaidsContractDef : MonoBehaviour
{
    public List<RaidsMobDef> MobDefinitions = new List<RaidsMobDef>();

    [Min(1)]
    public int MinCount = 10;
    [Min(1)]
    public int MaxCount = 1000;
    public float BaseXpPerMob = 1;
    public float BaseBoltsPerMob = 1;
    public ushort ExpirationTimeMinutes = 30;

    public string Name => gameObject.name;
    public string ExportName => Name.MaxLength(31);

    public ushort GetMobMask(List<RaidsMobDef> mobs)
    {
        if (string.IsNullOrWhiteSpace(ExportName) || ExportName.Any(char.IsControl))
            throw new InvalidOperationException("Contract groups must have a nonempty name without control characters.");
        if (Encoding.UTF8.GetByteCount(ExportName) > 31)
            throw new InvalidOperationException($"{Name}: exported contract group name exceeds 31 UTF-8 bytes.");
        if (MobDefinitions == null || MobDefinitions.Count == 0)
            throw new InvalidOperationException($"{Name}: contract group must include at least one mob definition.");

        ushort mask = 0;
        foreach (var mob in MobDefinitions)
        {
            var index = mob ? mobs.IndexOf(mob) : -1;
            if (index < 0 || index >= 16)
                throw new InvalidOperationException($"{Name}: contract group references a missing mob definition or one outside the map's enabled export list.");
            mask |= (ushort)(1 << index);
        }
        return mask;
    }

    public string GetDef(List<RaidsMobDef> mobs)
    {
        var sb = new StringBuilder();
        var mask = GetMobMask(mobs);

        sb.AppendLine("\t{");
        sb.AppendLine($"\t\t.MobDefMask = 0x{mask:X4},");
        sb.AppendLine($"\t\t.MinCount = {(ushort)MinCount},");
        sb.AppendLine($"\t\t.MaxCount = {(ushort)MaxCount},");
        sb.AppendLine($"\t\t.XpMult = {BaseXpPerMob.ToInvariantCulture()},");
        sb.AppendLine($"\t\t.BoltMult = {BaseBoltsPerMob.ToInvariantCulture()},");
        sb.AppendLine($"\t\t.ExpirationMinutes = {ExpirationTimeMinutes},");
        sb.AppendLine($"\t\t.Name = \"{ExportName.Escape()}\"");
        sb.AppendLine("\t},");

        return sb.ToString();
    }
}

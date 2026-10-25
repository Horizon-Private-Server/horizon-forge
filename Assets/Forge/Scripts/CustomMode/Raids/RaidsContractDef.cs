using System.Linq;
using System.Text;
using UnityEngine;

[AddComponentMenu("Forge/Raids/Contract Definition")]
[DisallowMultipleComponent]
public class RaidsContractDef : MonoBehaviour
{
    public RaidsMob Mob;

    [Tooltip("Use to specify moby oclass. If variants use same oclass, all will be accepted by the contract.")]
    public int Variant;

    [Min(1)]
    public int MinCount = 10;
    [Min(1)]
    public int MaxCount = 1000;
    public float BaseXpPerMob = 1;
    public float BaseBoltsPerMob = 1;
    public ushort ExpirationTimeMinutes = 30;

    public string GetDef()
    {
        var sb = new StringBuilder();

        // add mob oclasses
        var mobConfig = RaidsMobsScriptableObject.Load();
        var mobDefaults = mobConfig.Mobs.FirstOrDefault(x => x.Mob == Mob);
        var variant = mobDefaults.Variants.ElementAtOrDefault(Variant);

        sb.AppendLine("\t{");
        sb.AppendLine($"\t\t.MobOClass = {variant.OClass},");
        sb.AppendLine($"\t\t.MinCount = {(ushort)MinCount},");
        sb.AppendLine($"\t\t.MaxCount = {(ushort)MaxCount},");
        sb.AppendLine($"\t\t.XpMult = {BaseXpPerMob},");
        sb.AppendLine($"\t\t.BoltMult = {BaseBoltsPerMob},");
        sb.AppendLine($"\t\t.ExpirationMinutes = {ExpirationTimeMinutes}");
        sb.AppendLine("\t},");

        return sb.ToString();
    }
}

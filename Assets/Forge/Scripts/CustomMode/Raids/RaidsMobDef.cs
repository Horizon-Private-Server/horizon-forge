using System.Linq;
using System.Text;
using UnityEngine;

[AddComponentMenu("Forge/Raids/Mob Definition")]
[DisallowMultipleComponent]
public class RaidsMobDef : MonoBehaviour
{
    public RaidsMob Mob;
    public int Variant;
    [Tooltip("For Mobs with team textures only.")]
    public DLTeamIds TexturePalette = DLTeamIds.Blue;

    [Min(0)] public float SizeMultiplier = 1;

    [Min(0)] public float XpMultiplier = 1;
    [Min(0)] public float BoltsMultiplier = 1;

    [Min(0)] public float DamageMultiplier = 1;
    [Min(0), Tooltip("Adjusts the rate at which the mob's damage will scale with respect to the difficulty. A larger value will result in stronger mobs in higher difficulties.")] public float DamageDifficultyRateMultiplier = 1;

    [Min(0)] public float SpeedMultiplier = 1;
    [Min(0), Tooltip("Adjusts the rate at which the mob's speed will scale with respect to the difficulty. A larger value will result in faster mobs in higher difficulties.")] public float SpeedDifficultyRateMultiplier = 1;

    [Min(0)] public float HealthMultiplier = 1;
    [Min(0), Tooltip("Adjusts the rate at which the mob's health will scale with respect to the difficulty. A larger value will result in tougher mobs in higher difficulties.")] public float HealthDifficultyRateMultiplier = 1;

    [Min(0), Tooltip("Increase or decrease turn speed.")] public float TurnSpeedMultiplier = 1;
    [Min(0), Tooltip("For ranged attacks, how far away from the target the mob can be to fire.")] public float RangedAttackDistance = 50;
    [Min(0), Tooltip("How far out a mob can lock onto a target from.")] public float VisionRange = 50;
    [Min(0), Tooltip("How narrow or wide the mob's vision is."), Range(0, 360)] public float PeripheralVisionDegrees = 135;
    [Min(0), Tooltip("Range that a mob will always aggro a target, regardless of their peripheral vision.")] public float ForceAggroRange = 10;
    [Min(0), Tooltip("In seconds, how long after the mob loses sight of its target before it will exit the Aggro state.")] public float OutOfSightDeAggroTime = 60;

    public string Name => gameObject.name;

    public string GetDef()
    {
        var sb = new StringBuilder();

        var mobPrefix = this.Mob.ToString().ToLower();
        var mobConfig = RaidsMobsScriptableObject.Load();
        var defaults = mobConfig.Mobs.FirstOrDefault(x => x.Mob == this.Mob) ?? new RaidsMobsScriptableObject.RaidsMobsConfig();
        var variant = defaults?.Variants?.ElementAtOrDefault(Variant);

        sb.AppendLine("\t{");
        sb.AppendLine($"\t\t.MobCreate = &{mobPrefix}Create,");
        sb.AppendLine($"\t\t.MobVTable = &{this.Mob}VTable,");
        sb.AppendLine($"\t\t.RenderCost = {mobPrefix.ToUpper()}_RENDER_COST,");
        sb.AppendLine($"\t\t.Scale = {SizeMultiplier},");
        sb.AppendLine($"\t\t.OClass = {variant.OClass},");
        sb.AppendLine($"\t\t.BlipType = {(int)defaults.BlipType},");
        sb.AppendLine($"\t\t.BlipTeam = {(int)defaults.BlipTeam},");
        sb.AppendLine($"\t\t.TeamPalette = {(int)TexturePalette},");
        //sb.AppendLine($"\t\t.Name = \"{Name?.Replace("\"", "")}\",");
        sb.AppendLine($"\t\t.Config = {{");
        sb.AppendLine($"\t\t\t.Xp = {(int)(defaults.Xp * XpMultiplier)},");
        sb.AppendLine($"\t\t\t.Bolts = {(int)(defaults.Bolts * BoltsMultiplier)},");
        sb.AppendLine($"\t\t\t.Bangles = 0x{(int)variant.Bangles:X4},");
        sb.AppendLine($"\t\t\t.Damage = {defaults.Damage * DamageMultiplier},");
        sb.AppendLine($"\t\t\t.MaxDamage = {defaults.DamageMax},");
        sb.AppendLine($"\t\t\t.DamageScale = {defaults.DamageScale * DamageDifficultyRateMultiplier},");
        sb.AppendLine($"\t\t\t.Speed = {defaults.Speed * SpeedMultiplier},");
        sb.AppendLine($"\t\t\t.MaxSpeed = {defaults.SpeedMax},");
        sb.AppendLine($"\t\t\t.SpeedScale = {defaults.SpeedScale * SpeedDifficultyRateMultiplier},");
        sb.AppendLine($"\t\t\t.Health = {defaults.Health * HealthMultiplier},");
        sb.AppendLine($"\t\t\t.MaxHealth = {defaults.HealthMax},");
        sb.AppendLine($"\t\t\t.HealthScale = {defaults.HealthScale * HealthDifficultyRateMultiplier},");
        sb.AppendLine($"\t\t\t.TurnSpeed = {defaults.TurnSpeed * TurnSpeedMultiplier},");
        sb.AppendLine($"\t\t\t.AttackRadius = {defaults.AttackRadius * SizeMultiplier},");
        sb.AppendLine($"\t\t\t.HitRadius = {defaults.HitRadius * SizeMultiplier},");
        sb.AppendLine($"\t\t\t.CollRadius = {defaults.CollRadius * SizeMultiplier},");
        sb.AppendLine($"\t\t\t.AutoAggroMaxRange = {ForceAggroRange},");
        sb.AppendLine($"\t\t\t.VisionRange = {VisionRange},");
        sb.AppendLine($"\t\t\t.RangedMaxDistanceToTarget = {RangedAttackDistance},");
        sb.AppendLine($"\t\t\t.PeripheryRangeTheta = {PeripheralVisionDegrees * 0.5f * Mathf.Deg2Rad},");
        sb.AppendLine($"\t\t\t.OutOfSightDeAggroTickCount = {(int)(OutOfSightDeAggroTime * 60)},");
        sb.AppendLine($"\t\t\t.ReactionTickCount = {(int)(defaults.ReactionDelaySeconds * 60)},");
        sb.AppendLine($"\t\t\t.AttackCooldownTickCount = {(int)(defaults.AttackCooldownSeconds * 60)},");
        sb.AppendLine($"\t\t}}");
        sb.AppendLine("\t},");

        return sb.ToString();
    }

}

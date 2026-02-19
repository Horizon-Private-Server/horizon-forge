using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using UnityEngine;

public class SurvivalMobDef : MonoBehaviour
{
    public SurvivalMob Mob;
    public int Variant;
    public int Behavior;
    public SurvivalMobAttributes Attributes;
    public EnumOverride<DLBlipTypes> BlipType;

    [Header("Spawn Parameters")]
    public bool SpecialRoundOnly = false;
    public int MinRound = 0;
    public int MaxSpawnedAtOnce = 0;
    public int MaxSpawnedPerRound = 0;
    [Range(0, 1f)] public float Probability = 1;
    public SurvivalMobSpawnType SpawnType = SurvivalMobSpawnType.Random;
    public int CooldownTicks = 60;
    public float CooldownOffsetPerRoundFactor = 0;

    [Header("Mob Parameters")]
    [Min(0)] public float SizeMultiplier = 1;
    [Min(0), Tooltip("Increase or decrease turn speed.")] public float TurnSpeedMultiplier = 1;
    [Min(0), Tooltip("For ranged attacks, how far away from the target the mob can be to fire.")] public FloatOverride RangedAttackDistance;
    public FloatOverride Xp;
    public FloatOverride Bolts;

    [Header("Damage")]
    public FloatOverride Damage;
    public FloatOverride DamageMax;
    public FloatOverride DamageScale;

    [Header("Speed")]
    public FloatOverride Speed;
    public FloatOverride SpeedMax;
    public FloatOverride SpeedScale;

    [Header("Health")]
    public FloatOverride Health;
    public FloatOverride HealthMax;
    public FloatOverride HealthScale;

    [Header("Colors")]
    public ColorNoAlphaOverride BaseColor;
    public ColorNoAlphaOverride GlowColor;
    public ColorNoAlphaOverride SpriteColor;

    public string Name => gameObject.name;

    public string GetDef(SpriteDef[] spriteDefs, float? probabilityOverride = null)
    {
        var sb = new StringBuilder();

        var mobPrefix = this.Mob.ToString().ToLower();
        var mobConfig = SurvivalMobsScriptableObject.Load();
        var defaults = mobConfig.Mobs.FirstOrDefault(x => x.Mob == this.Mob) ?? new SurvivalMobsScriptableObject.SurvivalMobsConfig();
        var variant = defaults?.Variants?.ElementAtOrDefault(Variant);
        var spriteIdx = Array.FindIndex(spriteDefs, x => x.m_Texture == variant.SpriteTexture);
        var bossSpriteDef = Array.Find(spriteDefs, x => (variant.ExistingBossSpriteUid > 0 && x.m_Uid == variant.ExistingBossSpriteUid && !variant.BossTexture) || x.m_Texture == variant.BossTexture);
        var name = gameObject.name.MaxLength(31).Escape();

        sb.AppendLine("\t{");
        sb.AppendLine($"\t\t.MobVTable = &{this.Mob}VTable,");
        sb.AppendLine($"\t\t.RenderCost = {mobPrefix.ToUpper()}_RENDER_COST,");
        sb.AppendLine($"\t\t.Scale = {SizeMultiplier.ToInvariantCulture()},");
        sb.AppendLine($"\t\t.OClass = {variant.OClass},");
        sb.AppendLine($"\t\t.BlipType = {(BlipType.HasOverride ? (int)BlipType.OverrideValue : (int)defaults.BlipType)},");
        sb.AppendLine($"\t\t.MaxSpawnedAtOnce = {MaxSpawnedAtOnce},");
        sb.AppendLine($"\t\t.MaxSpawnedPerRound = {MaxSpawnedPerRound},");
        sb.AppendLine($"\t\t.MinRound = {Math.Clamp(MinRound, 0, int.MaxValue)},");
        sb.AppendLine($"\t\t.CooldownTicks = {(int)CooldownTicks},");
        sb.AppendLine($"\t\t.CooldownOffsetPerRoundFactor = {CooldownOffsetPerRoundFactor.ToInvariantCulture()},");
        sb.AppendLine($"\t\t.Probability = {(probabilityOverride ?? Probability).ToInvariantCulture()},");
        sb.AppendLine($"\t\t.RangedAttackDistance = {(RangedAttackDistance.HasOverride ? RangedAttackDistance.OverrideValue : defaults.RangedAttackDistance).ToInvariantCulture()},");
        sb.AppendLine($"\t\t.SpawnType = {(int)SpawnType},");
        sb.AppendLine($"\t\t.SpecialRoundOnly = {(SpecialRoundOnly ? 1 : 0)},");
        sb.AppendLine($"\t\t.StatId = {(int)defaults.StatId},");
        sb.AppendLine($"\t\t.BaseColor = 0x{RCHelper.GetAbgrHex(BaseColor.HasOverride ? BaseColor.OverrideValue : defaults.BaseColor, overrideAlpha: 0):X8},");
        sb.AppendLine($"\t\t.GlowColor = 0x{RCHelper.GetAbgrHex(GlowColor.HasOverride ? GlowColor.OverrideValue : defaults.GlowColor, overrideAlpha: 0.5f):X8},");
        sb.AppendLine($"\t\t.SpriteColor = 0x{RCHelper.GetAbgrHex(SpriteColor.HasOverride ? SpriteColor.OverrideValue : defaults.SpriteColor, overrideAlpha: 0):X8},");
        sb.AppendLine($"\t\t.SpriteTexId = {(spriteIdx < 0 ? 127 : spriteIdx)},");
        sb.AppendLine($"\t\t.BossTexUid = {(bossSpriteDef != null ? bossSpriteDef.m_Uid : 30130)},");
        sb.AppendLine($"\t\t.Name = \"{name}\",");
        sb.AppendLine($"\t\t.Config = {{");
        sb.AppendLine($"\t\t\t.Xp = {(ushort)Math.Clamp(Xp.HasOverride ? Xp.OverrideValue : defaults.Xp, 0, ushort.MaxValue)},");
        sb.AppendLine($"\t\t\t.Bolts = {(int)(Bolts.HasOverride ? Bolts.OverrideValue : defaults.Bolts)},");
        sb.AppendLine($"\t\t\t.Bangles = 0x{(int)variant.Bangles:X4},");
        sb.AppendLine($"\t\t\t.Damage = {(Damage.HasOverride ? Damage.OverrideValue : defaults.Damage).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.MaxDamage = {(DamageMax.HasOverride ? DamageMax.OverrideValue : defaults.DamageMax).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.DamageScale = {(DamageScale.HasOverride ? DamageScale.OverrideValue : defaults.DamageScale).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.Speed = {(Speed.HasOverride ? Speed.OverrideValue : defaults.Speed).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.MaxSpeed = {(SpeedMax.HasOverride ? SpeedMax.OverrideValue : defaults.SpeedMax).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.SpeedScale = {(SpeedScale.HasOverride ? SpeedScale.OverrideValue : defaults.SpeedScale).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.Health = {(Health.HasOverride ? Health.OverrideValue : defaults.Health).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.MaxHealth = {(HealthMax.HasOverride ? HealthMax.OverrideValue : defaults.HealthMax).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.HealthScale = {(HealthScale.HasOverride ? HealthScale.OverrideValue : defaults.HealthScale).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.AttackRadius = {(defaults.AttackRadius * SizeMultiplier).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.HitRadius = {(defaults.HitRadius * SizeMultiplier).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.CollRadius = {(defaults.CollRadius * SizeMultiplier).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.ReactionTickCount = {(int)(defaults.ReactionDelaySeconds * 60)},");
        sb.AppendLine($"\t\t\t.AttackCooldownTickCount = {(int)(defaults.AttackCooldownSeconds * 60)},");
        sb.AppendLine($"\t\t\t.DamageCooldownTickCount = {(int)(defaults.DamageCooldownSeconds * 60)},");
        sb.AppendLine($"\t\t\t.MobAttribute = {(int)Attributes},");
        sb.AppendLine($"\t\t\t.Behavior = {Behavior},");
        sb.AppendLine($"\t\t\t.SharedXp = {1},");
        sb.AppendLine($"\t\t}}");
        sb.AppendLine("\t},");

        return sb.ToString();
    }
}

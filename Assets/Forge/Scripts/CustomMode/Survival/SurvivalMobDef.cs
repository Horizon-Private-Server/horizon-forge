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

    public List<ActionOverride> Actions = new List<ActionOverride>();
    public List<ParamOverride> Parameters = new List<ParamOverride>();

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
        sb.AppendLine($"\t\t\t.AttackRadius = {(defaults.AttackRadius + (SizeMultiplier * defaults.CollRadius)).ToInvariantCulture()},");
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

    public string GetActionDefs()
    {
        var sb = new StringBuilder();

        var mobPrefix = this.Mob.ToString().ToLower();
        var mobConfig = SurvivalMobsScriptableObject.Load();
        var defaults = mobConfig.Mobs.FirstOrDefault(x => x.Mob == this.Mob) ?? new SurvivalMobsScriptableObject.SurvivalMobsConfig();
        var variant = defaults?.Variants?.ElementAtOrDefault(Variant);
        var name = gameObject.name.MaxLength(31).Escape();

        for (int i = 0; i < defaults.Actions.Count; ++i)
        {
            var action = defaults.Actions[i];
            var actionOverride = Actions.ElementAtOrDefault(i);

            float minCooldownSec = actionOverride?.MinCooldownSeconds.HasOverride == true ? actionOverride.MinCooldownSeconds.OverrideValue : action.MinCooldownSeconds;
            float maxCooldownSec = actionOverride?.MaxCooldownSeconds.HasOverride == true ? actionOverride.MaxCooldownSeconds.OverrideValue : action.MaxCooldownSeconds;
            float probability = actionOverride?.Probability.HasOverride == true ? actionOverride.Probability.OverrideValue : action.Probability;
            float queuedForTicks = actionOverride?.QueuedForTicks.HasOverride == true ? actionOverride.QueuedForTicks.OverrideValue : action.QueuedForTicks;

            sb.AppendLine($"\t{{ /* {action.Name} */ ");
            sb.AppendLine($"\t\t.MinCooldownTicks = {(int)(60 * minCooldownSec)},");
            sb.AppendLine($"\t\t.MaxCooldownTicks = {(int)(60 * maxCooldownSec)},");
            sb.AppendLine($"\t\t.Probability = {probability.ToInvariantCulture()},");
            sb.AppendLine($"\t\t.QueuedForTicks = {queuedForTicks},");
            sb.AppendLine("\t},");
        }

        return sb.ToString();
    }

    public string GetParameterDefs()
    {
        var sb = new StringBuilder();

        var mobPrefix = this.Mob.ToString().ToLower();
        var mobConfig = SurvivalMobsScriptableObject.Load();
        var defaults = mobConfig.Mobs.FirstOrDefault(x => x.Mob == this.Mob) ?? new SurvivalMobsScriptableObject.SurvivalMobsConfig();
        var variant = defaults?.Variants?.ElementAtOrDefault(Variant);
        var name = gameObject.name.MaxLength(31).Escape();

        for (int i = 0; i < defaults.Parameters.Count; ++i)
        {
            var paramDef = defaults.Parameters[i];
            var paramOverride = Parameters.ElementAtOrDefault(i);
            var value = paramDef.DefaultValue;

            switch (paramDef.ValueType)
            {
                case SurvivalMobsScriptableObject.SurvivalMobParameter.InputType.Float:
                case SurvivalMobsScriptableObject.SurvivalMobParameter.InputType.Probability:
                    if (paramOverride != null && paramOverride.FloatValue.HasOverride)
                        value = paramOverride.FloatValue.OverrideValue.ToInvariantCulture();
                    sb.AppendLine($"\t{{ .FloatValue = {value} /* {paramDef.Name} */ }},");
                    break;
                case SurvivalMobsScriptableObject.SurvivalMobParameter.InputType.Integer:
                    if (paramOverride != null && paramOverride.IntValue.HasOverride)
                        value = paramOverride.IntValue.OverrideValue.ToString();
                    sb.AppendLine($"\t{{ .IntValue = {value} /* {paramDef.Name} */ }},");
                    break;
                case SurvivalMobsScriptableObject.SurvivalMobParameter.InputType.Boolean:
                    var b = (bool.TryParse(paramDef.DefaultValue, out var bValue) && bValue) || (int.TryParse(paramDef.DefaultValue, out var iValue) && iValue != 0);
                    if (paramOverride != null && paramOverride.BoolValue.HasOverride)
                        b = paramOverride.BoolValue.OverrideValue;
                    sb.AppendLine($"\t{{ .IntValue = {(b ? 1 : 0)} /* {paramDef.Name} */ }},");
                    break;
                default: throw new NotImplementedException();
            }
        }

        return sb.ToString();
    }

    [Serializable]
    public class ParamOverride
    {
        public FloatOverride FloatValue;
        public Int32Override IntValue;
        public BoolOverride BoolValue;
    }

    [Serializable]
    public class ActionOverride
    {
        [Tooltip("Minimum threshold for random cooldown time.")]
        public FloatOverride MinCooldownSeconds;
        [Tooltip("Maximum threshold for random cooldown time.")]
        public FloatOverride MaxCooldownSeconds;
        [Tooltip("Probability (0.0 - 1.0) that once the cooldown hits 0 that the action will be queued.")]
        public FloatOverride Probability;
        [Tooltip("How long the action can remain queued before it resets. 0 means it can remain queued forever. 60 ticks in a second.")]
        public Int32Override QueuedForTicks;
    }
}

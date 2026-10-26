using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using UnityEngine;

[AddComponentMenu("Forge/Raids/Mob Definition")]
[DisallowMultipleComponent]
public class RaidsMobDef : MonoBehaviour
{
    public RaidsMob Mob;
    public int Variant;
    [Tooltip("Enables the shared boss health meter and boss combat behavior. Enable explicitly for Reactor or King Leviathan encounters.")]
    public bool IsBoss;
    public EnumOverride<DLBlipTypes> BlipType;
    public ColorNoAlphaOverride BaseColor;
    public ColorNoAlphaOverride GlowColor;
    public ColorNoAlphaOverride SpriteColor;
    public List<ActionOverride> Actions = new List<ActionOverride>();
    public List<ParamOverride> Parameters = new List<ParamOverride>();
    [Tooltip("For Mobs with team textures only.")]
    public DLTeamIds TexturePalette = DLTeamIds.Blue;

    [Min(0)] public float SizeMultiplier = 1;

    public RaidsDifficultyValues<Int32Override> XpOverrides = new RaidsDifficultyValues<Int32Override>();
    public RaidsDifficultyValues<Int32Override> BoltsOverrides = new RaidsDifficultyValues<Int32Override>();

    public RaidsDifficultyValues<FloatOverride> DamageOverrides = new RaidsDifficultyValues<FloatOverride>();
    public RaidsDifficultyValues<FloatOverride> SpeedOverrides = new RaidsDifficultyValues<FloatOverride>();
    public RaidsDifficultyValues<FloatOverride> HealthOverrides = new RaidsDifficultyValues<FloatOverride>();

    [Tooltip("How quickly the mob turns at each difficulty.")]
    public RaidsDifficultyValues<FloatOverride> TurnSpeed = new RaidsDifficultyValues<FloatOverride>();
    [Tooltip("How far out a mob can aggro onto a target from.")] public FloatOverride VisionRange;
    [Tooltip("How narrow or wide the mob's aggro vision is.")] public FloatOverride PeripheralVisionDegrees;
    [Tooltip("Range that a mob will always aggro a target, regardless of their peripheral vision.")] public FloatOverride ForceAggroRange;
    [Tooltip("In seconds, how long after the mob loses sight of its target before it will exit the Aggro state.")] public FloatOverride OutOfSightDeAggroTime;

    public string Name => gameObject.name;

    public string GetDef(SpriteDef[] spriteDefs)
    {
        var sb = new StringBuilder();

        var mobPrefix = this.Mob.ToString().ToLower();
        var defaults = GetDefaults();
        var variant = defaults.Variants[Variant];
        var spriteIdx = FindSprite(spriteDefs, variant.SpriteTexture, variant.SpriteTextureTint);
        var bossIdx = FindSprite(spriteDefs, variant.BossTexture, variant.BossTextureTint);
        var bossUid = bossIdx >= 0 ? spriteDefs[bossIdx].m_Uid : variant.ExistingBossSpriteUid > 0 ? variant.ExistingBossSpriteUid : 30130;

        sb.AppendLine("\t{");
        sb.AppendLine($"\t\t.MobCreate = &{mobPrefix}Create,");
        sb.AppendLine($"\t\t.MobVTable = &{this.Mob}VTable,");
        sb.AppendLine($"\t\t.RenderCost = {mobPrefix.ToUpper()}_RENDER_COST,");
        sb.AppendLine($"\t\t.Scale = {(SizeMultiplier).ToInvariantCulture()},");
        sb.AppendLine($"\t\t.OClass = {variant.OClass},");
        sb.AppendLine($"\t\t.BlipType = {(int)(BlipType.HasOverride ? BlipType.OverrideValue : defaults.BlipType)},");
        sb.AppendLine($"\t\t.BlipTeam = {(int)defaults.BlipTeam},");
        sb.AppendLine($"\t\t.TeamPalette = {(int)TexturePalette},");
        sb.AppendLine($"\t\t.IsBoss = {(IsBoss ? 1 : 0)},");
        sb.AppendLine($"\t\t.BaseColor = 0x{RCHelper.GetAbgrHex(BaseColor.HasOverride ? BaseColor.OverrideValue : defaults.BaseColor, overrideAlpha: 0):X8},");
        sb.AppendLine($"\t\t.GlowColor = 0x{RCHelper.GetAbgrHex(GlowColor.HasOverride ? GlowColor.OverrideValue : defaults.GlowColor, overrideAlpha: 0.5f):X8},");
        sb.AppendLine($"\t\t.SpriteColor = 0x{RCHelper.GetAbgrHex(SpriteColor.HasOverride ? SpriteColor.OverrideValue : defaults.SpriteColor, overrideAlpha: 0):X8},");
        sb.AppendLine($"\t\t.SpriteTexId = {(spriteIdx < 0 ? 127 : spriteIdx)},");
        sb.AppendLine($"\t\t.BossTexUid = {bossUid},");
        sb.AppendLine($"\t\t.Name = \"{Name.MaxLength(31).Escape()}\",");
        sb.AppendLine($"\t\t.Config = {{");
        sb.AppendLine($"\t\t\t.Xp = {{ {GetRewardValues(defaults.Xp, XpOverrides, ushort.MaxValue)} }},");
        sb.AppendLine($"\t\t\t.Bolts = {{ {GetRewardValues(defaults.Bolts, BoltsOverrides, int.MaxValue)} }},");
        sb.AppendLine($"\t\t\t.Bangles = 0x{(int)variant.Bangles:X4},");
        sb.AppendLine($"\t\t\t.Damage = {{ {GetStatValues(defaults.Damage, DamageOverrides)} }},");
        sb.AppendLine($"\t\t\t.Speed = {{ {GetStatValues(defaults.Speed, SpeedOverrides)} }},");
        sb.AppendLine($"\t\t\t.Health = {{ {GetStatValues(defaults.Health, HealthOverrides)} }},");
        sb.AppendLine($"\t\t\t.TurnSpeed = {{ {GetStatValues(defaults.TurnSpeed, TurnSpeed)} }},");
        sb.AppendLine($"\t\t\t.AttackRadius = {(defaults.AttackRadius * SizeMultiplier).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.HitRadius = {(defaults.HitRadius * SizeMultiplier).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.CollRadius = {(defaults.CollRadius * SizeMultiplier).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.AutoAggroMaxRange = {ForceAggroRange.GetValue(defaults.ForceAggroRange).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.VisionRange = {VisionRange.GetValue(defaults.VisionRange).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.PeripheryRangeTheta = {(PeripheralVisionDegrees.GetValue(defaults.PeripheralVisionDegrees) * 0.5f * Mathf.Deg2Rad).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.OutOfSightDeAggroTickCount = {(int)(OutOfSightDeAggroTime.GetValue(defaults.OutOfSightDeAggroTime) * 60)},");
        sb.AppendLine($"\t\t\t.ReactionTickCount = {(int)(defaults.ReactionDelaySeconds * 60)},");
        sb.AppendLine($"\t\t\t.DamageCooldownTickCount = {(int)(defaults.DamageCooldownSeconds * 60)},");
        sb.AppendLine($"\t\t\t.AttackCooldownTickCount = {(int)(defaults.AttackCooldownSeconds * 60)},");
        sb.AppendLine($"\t\t}}");
        sb.AppendLine("\t},");

        return sb.ToString();
    }

    public RaidsMobsScriptableObject.RaidsMobsConfig GetDefaults()
    {
        var defaults = RaidsMobsScriptableObject.Load()?.Mobs.FirstOrDefault(x => x.Mob == Mob);
        if (defaults == null || Variant < 0 || Variant >= defaults.Variants.Count)
            throw new InvalidOperationException($"{Name}: invalid {Mob} variant {Variant}.");
        return defaults;
    }

    public static int FindSprite(SpriteDef[] sprites, Texture2D texture, Color tint)
    {
        if (!texture || sprites == null) return -1;
        return Array.FindIndex(sprites, x => x.m_Texture == texture && (x.m_Tint ?? Color.white) == tint);
    }

    public void ValidateConfig()
    {
        var defaults = GetDefaults();
        if (defaults.Actions.Count > RaidsModeData.MAX_ACTIONS_PER_MOB || defaults.Parameters.Count > RaidsModeData.MAX_PARAMS_PER_MOB)
            throw new InvalidOperationException($"{Name}: mob config exceeds 8 actions or 16 parameters.");
        if ((Actions?.Count ?? 0) > defaults.Actions.Count || (Parameters?.Count ?? 0) > defaults.Parameters.Count)
            throw new InvalidOperationException($"{Name}: overrides do not match the selected mob's config.");
        for (var difficulty = 0; difficulty < 5; ++difficulty)
        {
            GetActionDefs(difficulty);
            GetParameterDefs(difficulty);
        }
        GetStatValues(defaults.Damage, DamageOverrides);
        GetStatValues(defaults.Speed, SpeedOverrides);
        GetStatValues(defaults.Health, HealthOverrides);
        GetStatValues(defaults.TurnSpeed, TurnSpeed);
        GetRewardValues(defaults.Xp, XpOverrides, ushort.MaxValue);
        GetRewardValues(defaults.Bolts, BoltsOverrides, int.MaxValue);
        if (!Finite(defaults.DamageCooldownSeconds) || defaults.DamageCooldownSeconds < 0 || defaults.DamageCooldownSeconds > ushort.MaxValue / 60f)
            throw new InvalidOperationException($"{Name}: damage cooldown is outside the supported range.");
    }

    public string GetActionDefs(int difficulty)
    {
        var sb = new StringBuilder();
        var defaults = GetDefaults();
        for (var i = 0; i < defaults.Actions.Count; ++i)
        {
            var action = defaults.Actions[i];
            var value = Actions?.ElementAtOrDefault(i);
            var min = (value?.MinCooldownSeconds.Get(difficulty) ?? default).GetValue(action.MinCooldownSeconds.Get(difficulty));
            var max = (value?.MaxCooldownSeconds.Get(difficulty) ?? default).GetValue(action.MaxCooldownSeconds.Get(difficulty));
            var probability = (value?.Probability.Get(difficulty) ?? default).GetValue(action.Probability.Get(difficulty));
            var queued = (value?.QueuedForTicks.Get(difficulty) ?? default).GetValue(action.QueuedForTicks.Get(difficulty));
            if (!Finite(min) || !Finite(max) || min < 0 || max < min || max >= int.MaxValue / 60f || !Finite(probability) || probability < 0 || probability > 1 || queued < 0)
                throw new InvalidOperationException($"{Name}: invalid settings for action {action.Name} ({difficulty + 1} stars).");
            sb.AppendLine($"{{ .MinCooldownTicks = {(int)(min * 60)}, .MaxCooldownTicks = {(int)(max * 60)}, .Probability = {probability.ToInvariantCulture()}, .QueuedForTicks = {queued} }},");
        }
        return sb.ToString();
    }

    public string GetParameterDefs(int difficulty)
    {
        var sb = new StringBuilder();
        var defaults = GetDefaults();
        for (var i = 0; i < defaults.Parameters.Count; ++i)
        {
            var param = defaults.Parameters[i];
            var value = Parameters?.ElementAtOrDefault(i);
            var defaultValue = param.DefaultValue.Get(difficulty);
            var floatOverride = value?.FloatValue.Get(difficulty) ?? default;
            var intOverride = value?.IntValue.Get(difficulty) ?? default;
            var boolOverride = value?.BoolValue.Get(difficulty) ?? default;
            switch (param.ValueType)
            {
                case RaidsMobsScriptableObject.RaidsMobParameter.InputType.Float:
                case RaidsMobsScriptableObject.RaidsMobParameter.InputType.Probability:
                    if (!float.TryParse(defaultValue, NumberStyles.Float, CultureInfo.InvariantCulture, out var f) || !Finite(f))
                        throw new InvalidOperationException($"{Name}: invalid float default for {param.Name}.");
                    if (floatOverride.HasOverride) f = floatOverride.OverrideValue;
                    if (!Finite(f) || (param.ValueType == RaidsMobsScriptableObject.RaidsMobParameter.InputType.Probability && (f < 0 || f > 1)))
                        throw new InvalidOperationException($"{Name}: invalid value for {param.Name}.");
                    sb.AppendLine($"{{ .FloatValue = {f.ToInvariantCulture()} }},");
                    break;
                case RaidsMobsScriptableObject.RaidsMobParameter.InputType.Integer:
                    if (!int.TryParse(defaultValue, NumberStyles.Integer, CultureInfo.InvariantCulture, out var n))
                        throw new InvalidOperationException($"{Name}: invalid integer default for {param.Name}.");
                    if (intOverride.HasOverride) n = intOverride.OverrideValue;
                    sb.AppendLine($"{{ .IntValue = {n} }},");
                    break;
                case RaidsMobsScriptableObject.RaidsMobParameter.InputType.Boolean:
                    bool b;
                    if (!bool.TryParse(defaultValue, out b))
                    {
                        if (!int.TryParse(defaultValue, out var numeric))
                            throw new InvalidOperationException($"{Name}: invalid boolean default for {param.Name}.");
                        b = numeric != 0;
                    }
                    if (boolOverride.HasOverride) b = boolOverride.OverrideValue;
                    sb.AppendLine($"{{ .IntValue = {(b ? 1 : 0)} }},");
                    break;
                default: throw new InvalidOperationException($"{Name}: unsupported parameter type for {param.Name}.");
            }
        }
        return sb.ToString();
    }

    private string GetStatValues(RaidsDifficultyValues<float> defaults, RaidsDifficultyValues<FloatOverride> overrides)
    {
        var values = new List<string>();
        for (var difficulty = 0; difficulty < 5; ++difficulty)
        {
            var value = overrides.Get(difficulty).GetValue(defaults.Get(difficulty));
            if (!Finite(value) || value < 0)
                throw new InvalidOperationException($"{Name}: stats must be finite and nonnegative ({difficulty + 1} stars).");
            values.Add(value.ToInvariantCulture("R"));
        }
        return string.Join(", ", values);
    }

    private string GetRewardValues(RaidsDifficultyValues<int> defaults, RaidsDifficultyValues<Int32Override> overrides, int maximum)
    {
        var values = new List<string>();
        for (var difficulty = 0; difficulty < 5; ++difficulty)
        {
            var value = overrides.Get(difficulty).GetValue(defaults.Get(difficulty));
            if (value < 0 || value > maximum)
                throw new InvalidOperationException($"{Name}: reward must be between 0 and {maximum} ({difficulty + 1} stars).");
            values.Add(value.ToString(CultureInfo.InvariantCulture));
        }
        return string.Join(", ", values);
    }

    private static bool Finite(float value) => !float.IsNaN(value) && !float.IsInfinity(value);

    [Serializable]
    public class ParamOverride
    {
        public RaidsDifficultyValues<FloatOverride> FloatValue = new RaidsDifficultyValues<FloatOverride>();
        public RaidsDifficultyValues<Int32Override> IntValue = new RaidsDifficultyValues<Int32Override>();
        public RaidsDifficultyValues<BoolOverride> BoolValue = new RaidsDifficultyValues<BoolOverride>();
    }

    [Serializable]
    public class ActionOverride
    {
        public RaidsDifficultyValues<FloatOverride> MinCooldownSeconds = new RaidsDifficultyValues<FloatOverride>();
        public RaidsDifficultyValues<FloatOverride> MaxCooldownSeconds = new RaidsDifficultyValues<FloatOverride>();
        public RaidsDifficultyValues<FloatOverride> Probability = new RaidsDifficultyValues<FloatOverride>();
        public RaidsDifficultyValues<Int32Override> QueuedForTicks = new RaidsDifficultyValues<Int32Override>();
    }

}

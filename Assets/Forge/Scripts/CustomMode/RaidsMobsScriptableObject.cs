using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using UnityEditor;
using UnityEngine;

public class RaidsMobsScriptableObject : ScriptableObject
{
    public static readonly string PATH = "Assets/Forge/Prefabs/Raids/Raids Mob Config.asset";
    public static readonly string[] DifficultyFields = { "OneStar", "TwoStars", "ThreeStars", "FourStars", "FiveStars" };
    public static readonly string[] DifficultyNames = { "1 Star", "2 Stars", "3 Stars", "4 Stars", "5 Stars" };
    private const int BASE_XP = 5;
    private const int BASE_BOLTS = 50;
    private const int BASE_DAMAGE = 10;
    private const int BASE_SPEED = 3;
    private const int BASE_HEALTH = 30;

    public List<RaidsMobsConfig> Mobs = new List<RaidsMobsConfig>();


    [Serializable]
    public class RaidsMobsConfig
    {
        [ReadOnly] public RaidsMob Mob;
        [Multiline] public string Description;
        public DLBlipTypes BlipType = DLBlipTypes.CircleSmallDark;
        public DLTeamIds BlipTeam = DLTeamIds.Red;
        public List<RaidsMobVariant> Variants = new List<RaidsMobVariant>();
        public List<RaidsMobBehavior> Behaviors = new List<RaidsMobBehavior>();
        public List<RaidsMobAction> Actions = new List<RaidsMobAction>();
        public List<RaidsMobParameter> Parameters = new List<RaidsMobParameter>();

        [ColorUsage(false)] public Color BaseColor = new Color(0.25f, 0.25f, 0.25f);
        [ColorUsage(false)] public Color GlowColor = new Color(0.5f, 0.5f, 0.5f);
        [ColorUsage(false)] public Color SpriteColor = new Color(0.5f, 0.5f, 0.5f);

        public RaidsDifficultyValues<int> Xp = new RaidsDifficultyValues<int>(BASE_XP);
        public RaidsDifficultyValues<int> Bolts = new RaidsDifficultyValues<int>(BASE_BOLTS);

        public RaidsDifficultyValues<float> Damage = new RaidsDifficultyValues<float>(BASE_DAMAGE);
        public RaidsDifficultyValues<float> Speed = new RaidsDifficultyValues<float>(BASE_SPEED);
        public RaidsDifficultyValues<float> Health = new RaidsDifficultyValues<float>(BASE_HEALTH);

        public RaidsDifficultyValues<float> TurnSpeed = new RaidsDifficultyValues<float>(1);
        [Min(0)] public float VisionRange = 50;
        [Range(0, 360)] public float PeripheralVisionDegrees = 135;
        [Min(0)] public float ForceAggroRange = 10;
        [Min(0)] public float OutOfSightDeAggroTime = 60;
        public float AttackRadius = 5;
        public float HitRadius = 0.5f;
        public float CollRadius = 0.5f;

        public float ReactionDelaySeconds = 0.25f;
        public float AttackCooldownSeconds = 2;
        public float DamageCooldownSeconds = 0;
    }

    [Serializable]
    public class RaidsMobVariant
    {
        public string Name;
        [Multiline] public string Description;
        public int OClass;
        public RaidsMobBangle Bangles;
        [Tooltip("Show the team texture palette selector for this variant.")]
        public bool SupportsTeamPalette;
        public Texture2D SpriteTexture;
        public Color SpriteTextureTint = Color.white;
        public Texture2D BossTexture;
        public Color BossTextureTint = Color.white;
        public ushort ExistingBossSpriteUid;
        public List<RaidsMobDependency> Dependencies = new List<RaidsMobDependency>();

        public RaidsMobVariant() { }
        public RaidsMobVariant(string name, int oClass, DLMapIds sourceMapId, int sourceMissionId)
        {
            Name = name;
            OClass = oClass;
            Dependencies.Add(new RaidsMobDependency()
            {
                OClass = oClass,
                SourceMapId = sourceMapId,
                SourceMissionId = sourceMissionId
            });
        }
    }

    [Serializable]
    public class RaidsMobBehavior
    {
        public string Name;
        [Multiline] public string Description;
    }

    [Serializable]
    public class RaidsMobAction
    {
        public string Name;
        [Multiline] public string Description;
        public RaidsDifficultyValues<float> MinCooldownSeconds = new RaidsDifficultyValues<float>(0);
        public RaidsDifficultyValues<float> MaxCooldownSeconds = new RaidsDifficultyValues<float>(1);
        public RaidsDifficultyValues<float> Probability = new RaidsDifficultyValues<float>(1);
        public RaidsDifficultyValues<int> QueuedForTicks = new RaidsDifficultyValues<int>(0);
    }

    [Serializable]
    public class RaidsMobParameter
    {
        public enum InputType { Float, Integer, Boolean, Probability }
        public string Name;
        [Multiline] public string Description;
        public InputType ValueType;
        public RaidsDifficultyValues<string> DefaultValue = new RaidsDifficultyValues<string>("0");
    }

    [Serializable]
    public class RaidsMobDependency
    {
        public int OClass;
        public DLMapIds SourceMapId;
        public int SourceMissionId;
    }

    public static RaidsMobsScriptableObject Load()
    {
        return AssetDatabase.LoadAssetAtPath<RaidsMobsScriptableObject>(RaidsMobsScriptableObject.PATH);
    }
}

[Serializable]
public class RaidsDifficultyValues<T>
{
    [InspectorName("1 Star")] public T OneStar;
    [InspectorName("2 Stars")] public T TwoStars;
    [InspectorName("3 Stars")] public T ThreeStars;
    [InspectorName("4 Stars")] public T FourStars;
    [InspectorName("5 Stars")] public T FiveStars;

    public RaidsDifficultyValues() { }
    public RaidsDifficultyValues(T value)
    {
        OneStar = TwoStars = ThreeStars = FourStars = FiveStars = value;
    }

    public T Get(int difficulty)
    {
        switch (difficulty)
        {
            case 0: return OneStar;
            case 1: return TwoStars;
            case 2: return ThreeStars;
            case 3: return FourStars;
            case 4: return FiveStars;
            default: throw new ArgumentOutOfRangeException(nameof(difficulty));
        }
    }
}

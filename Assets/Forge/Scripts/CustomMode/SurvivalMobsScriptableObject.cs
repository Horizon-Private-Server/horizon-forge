using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using UnityEditor;
using UnityEngine;

//[CreateAssetMenu(fileName = "Survival Mob Config", menuName = "Forge/Survival/Mob Config")]
public class SurvivalMobsScriptableObject : ScriptableObject
{
    public static readonly string PATH = "Assets/Forge/Prefabs/Survival/Survival Mob Config.asset";
    private const int BASE_XP = 5;
    private const int BASE_BOLTS = 220;
    private const int BASE_DAMAGE = 10;
    private const int BASE_SPEED = 3;
    private const int BASE_HEALTH = 30;

    public List<SurvivalMobsConfig> Mobs = new List<SurvivalMobsConfig>();
    public List<SurvivalPatch> Patches = new List<SurvivalPatch>();
    public List<SurvivalWeaponStats> WeaponStats = new List<SurvivalWeaponStats>();
    public List<SpriteDef> SurvivalMysteryBoxSprites = new List<SpriteDef>();
    public List<SpriteDef> SurvivalStackableSprites = new List<SpriteDef>();
    public List<SurvivalDefaultItem> SurvivalDefaultItems = new List<SurvivalDefaultItem>();
    
    [Serializable]
    public class SurvivalMobsConfig
    {
        [ReadOnly] public SurvivalMob Mob;
		[Multiline]
		public string Description;
        public DLBlipTypes BlipType = DLBlipTypes.CircleSmallDark;
        public SurvivalMobStatIds StatId = SurvivalMobStatIds.None;
        public List<SurvivalMobVariant> Variants = new List<SurvivalMobVariant>();
        public List<SurvivalMobBehavior> Behaviors = new List<SurvivalMobBehavior>();
		public List<SurvivalMobAction> Actions = new List<SurvivalMobAction>();

        [ColorUsage(false)] public Color BaseColor = new Color(0.25f, 0.25f, 0.25f);
        [ColorUsage(false)] public Color GlowColor = new Color(0.5f, 0.5f, 0.5f);
        [ColorUsage(false)] public Color SpriteColor = new Color(0.5f, 0.5f, 0.5f);

        public int Xp = BASE_XP;
        public int Bolts = BASE_BOLTS;

        public float Damage = BASE_DAMAGE;
        public float DamageMax = 0;
        public float DamageScale = 1;

        public float Speed = BASE_SPEED;
        public float SpeedMax = BASE_SPEED * 5;
        public float SpeedScale = 1;

        public float Health = BASE_HEALTH;
        public float HealthMax = 1e+08f;
        public float HealthScale = 1;

        public float TurnSpeed = 1;
        public float AttackRadius = 5;
        public float RangedAttackDistance = 50;
        public float HitRadius = 0.5f;
        public float CollRadius = 0.5f;

        public float ReactionDelaySeconds = 0.25f;
        public float AttackCooldownSeconds = 2;
        public float DamageCooldownSeconds = 0;
    }

    [Serializable]
    public class SurvivalMobVariant
    {
        public string Name;
		[Multiline]
		public string Description;
        public int OClass;
        public SurvivalMobBangle Bangles;
        public Texture2D SpriteTexture;
        public Color SpriteTextureTint = Color.white;
        public Texture2D BossTexture;
        public Color BossTextureTint = Color.white;
        public ushort ExistingBossSpriteUid;
        public List<SurvivalMobDependency> Dependencies = new List<SurvivalMobDependency>();

        public SurvivalMobVariant() { }
        public SurvivalMobVariant(string name, int oClass, DLMapIds sourceMapId, int sourceMissionId)
        {
            Name = name;
            OClass = oClass;
            Dependencies.Add(new SurvivalMobDependency()
            {
                OClass = oClass,
                SourceMapId = sourceMapId,
                SourceMissionId = sourceMissionId
            });
        }
    }

    [Serializable]
    public class SurvivalMobBehavior
    {
        public string Name;

		[Multiline]
        public string Description;
    }

    [Serializable]
    public class SurvivalMobAction
    {
        public string Name;
		[Multiline] public string Description;

		[Header("Cooldown")]
		[Min(0)] public float MinCooldownSeconds = 0;
		[Min(0)] public float MaxCooldownSeconds = 1;
		[Range(0f, 1f)] public float Probability = 1;
		[Min(0)] public int QueuedForTicks = 0;

		[Header("Parameters")]
		public List<SurvivalMobActionParameter> Parameters = new List<SurvivalMobActionParameter>();
    }

    [Serializable]
    public class SurvivalMobActionParameter
    {
		public enum InputType
		{
			Float,
			Integer,
			Boolean,
			Probability
		}

        public string Name;
		[Multiline] public string Description;
		public InputType ValueType;
		public string DefaultValue;
    }

    [Serializable]
    public class SurvivalMobDependency
    {
        public int OClass;
        public DLMapIds SourceMapId;
        public int SourceMissionId;
    }

    [Serializable]
    public class SurvivalPatch
    {
        public string Name;
        public bool Disabled;
        public int CodeSegIndex;
        public string CodeSegOffsetHex;
        public string Hex;
    }

    [Serializable]
    public class SurvivalWeaponStats
    {
        public string Name;
        public DLGadgetIds Gadget;
        public int BaseAmmo = 16;
        public int AmmoModAmount = 5;
        public List<Vector4> Damages = new List<Vector4>();
    }

    [System.Serializable]
    public class SurvivalDefaultItem
    {
        [ReadOnly] public SurvivalDefaultItems Item;
        public SurvivalItemEntry Def = new SurvivalItemEntry();
    }

    public static SurvivalMobsScriptableObject Load()
    {
        return AssetDatabase.LoadAssetAtPath<SurvivalMobsScriptableObject>(SurvivalMobsScriptableObject.PATH);
    }
}

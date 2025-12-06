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

    public List<SurvivalMobsConfig> Mobs;


    [Serializable]
    public class SurvivalMobsConfig
    {
        [ReadOnly] public SurvivalMob Mob;
        public DLBlipTypes BlipType = DLBlipTypes.CircleSmallDark;
        public SurvivalMobStatIds StatId = SurvivalMobStatIds.None;
        public List<SurvivalMobVariant> Variants = new List<SurvivalMobVariant>();
        public List<string> Behaviors = new List<string>();

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
        public float HitRadius = 0.5f;
        public float CollRadius = 0.5f;

        public float ReactionDelaySeconds = 0.25f;
        public float AttackCooldownSeconds = 2;
    }

    [Serializable]
    public class SurvivalMobVariant
    {
        public string Name;
        public int OClass;
        public SurvivalMobBangle Bangles;
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
    public class SurvivalMobDependency
    {
        public int OClass;
        public DLMapIds SourceMapId;
        public int SourceMissionId;
    }

    public static SurvivalMobsScriptableObject Load()
    {
        return AssetDatabase.LoadAssetAtPath<SurvivalMobsScriptableObject>(SurvivalMobsScriptableObject.PATH);
    }
}

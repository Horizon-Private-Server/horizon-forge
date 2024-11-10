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
    private const int BASE_XP = 5;
    private const int BASE_BOLTS = 50;
    private const int BASE_DAMAGE = 10;
    private const int BASE_SPEED = 3;
    private const int BASE_HEALTH = 30;

    public List<RaidsMobsConfig> Mobs;


    [Serializable]
    public class RaidsMobsConfig
    {
        [ReadOnly] public RaidsMob Mob;
        public List<RaidsMobVariant> Variants;

        public int Xp = BASE_XP;
        public int Bolts = BASE_BOLTS;

        public float Damage = BASE_DAMAGE;
        public float DamageMax = 0;
        public float DamageScale = 1;

        public float Speed = BASE_SPEED;
        public float SpeedMax = BASE_SPEED * 5;
        public float SpeedScale = 1;

        public float Health = BASE_HEALTH;
        public float HealthMax = 0;
        public float HealthScale = 1;

        public float AttackRadius = 5;
        public float HitRadius = 0.5f;
        public float CollRadius = 0.5f;

        public float ReactionDelaySeconds = 0.25f;
        public float AttackCooldownSeconds = 2;
    }

    [Serializable]
    public class RaidsMobVariant
    {
        public string Name;
        public int OClass;
        public DLMapIds SourceMapId;
        public int SourceMissionId;

        public RaidsMobVariant() { }
        public RaidsMobVariant(string name, int oClass, DLMapIds sourceMapId, int sourceMissionId)
        {
            OClass = oClass;
            SourceMapId = sourceMapId;
            SourceMissionId = sourceMissionId;
        }
    }


    public static RaidsMobsScriptableObject Load()
    {
        return AssetDatabase.LoadAssetAtPath<RaidsMobsScriptableObject>(RaidsMobsScriptableObject.PATH);
    }
}

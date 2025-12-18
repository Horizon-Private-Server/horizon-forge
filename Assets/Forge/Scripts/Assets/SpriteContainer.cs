using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEngine;

public class SpriteContainer : MonoBehaviour
{
    [HelpBox("Override or add sprites on top of the base set.\nTo override, set the Uid of the sprite to the one you'd like to override.\nTo add, set a unique uid.", MessageType.Info, placeAbove = false)]
    [ReadOnly] public int RacVersion = RCVER.DL;

    public List<SpriteDef> Sprites = new List<SpriteDef>();
}

[Serializable]
public class SpriteDef
{
    public enum SpriteDefBank
    {
        Bank1,
        Bank2
    };

    public Texture2D m_Texture;
    public SpriteDefBank m_Bank = SpriteDefBank.Bank1;
    public ushort m_Uid;
    public ushort m_Unknown = 1;
    public TextureSize? m_TextureSizeOverride;
    public Color? m_Tint;
}

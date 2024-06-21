using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;

public class TieDatabase : ScriptableObject
{
    public List<TieData> Ties = new List<TieData>();


    #region Accessors

    public TieData Get(int oclass)
    {
        return Ties.FirstOrDefault(x => x.OClass == oclass);
    }

    public TieData Create(int oclass)
    {
        var data = new TieData()
        {
            OClass = oclass,
            OClassStr = $"{oclass}_{oclass:X4}"
        };

        Ties.Add(data);
        return data;
    }

    #endregion

}

[Serializable]
public class TieData
{
    [HideInInspector] public string OClassStr;
    [HideInInspector] public int OClass;
    [Min(0.01f)] public float MipDistanceMultiplier = 1;
    [Min(0.01f)] public float LODDistanceMultiplier = 1;
}

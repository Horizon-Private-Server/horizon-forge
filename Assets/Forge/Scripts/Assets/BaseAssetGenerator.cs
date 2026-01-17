using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public abstract class BaseAssetGenerator : MonoBehaviour
{
    public abstract void Generate();

    public virtual void OnPreBake(BakeType type) { }
    public virtual void OnPostBake(BakeType type) { }
}

public enum BakeType
{
    BUILD,
    OCCLUSION,
    COLLISION,
    MAPRENDER
}

using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

public abstract class CustomModeData : MonoBehaviour
{
    public abstract DLCustomModeIds CustomMode { get; }
    public abstract bool IsEnabled { get; }

    public abstract void Write(BinaryWriter writer);
}

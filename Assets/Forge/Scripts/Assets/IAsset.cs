using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public interface IAsset
{
    GameObject GameObject { get; }
    bool IsHidden { get; }
    void UpdateAsset();
}

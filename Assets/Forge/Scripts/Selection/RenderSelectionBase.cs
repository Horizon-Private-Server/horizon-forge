using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;

public class RenderSelectionBase : MonoBehaviour
{
    Renderer[] _cache { get; set; } = null;

    public virtual Matrix4x4 GetSelectionReflectionMatrix() => Matrix4x4.identity;

    public virtual Renderer[] GetSelectionRenderers()
    {
        if (_cache != null && !_cache.Any(x => !x)) return _cache;
        return _cache = this.GetComponentsInChildren<Renderer>();
    }
}

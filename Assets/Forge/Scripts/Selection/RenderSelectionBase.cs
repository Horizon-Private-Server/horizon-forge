using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class RenderSelectionBase : MonoBehaviour
{
    public virtual Matrix4x4 GetSelectionReflectionMatrix() => Matrix4x4.identity;

    public virtual Renderer[] GetSelectionRenderers()
    {
        return this.GetComponentsInChildren<Renderer>();
    }
}

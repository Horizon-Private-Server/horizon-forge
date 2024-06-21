using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class SkyLayer : MonoBehaviour
{
    public bool Bloom = false;
    public Vector3 AngularRotation = Vector3.zero;

    private MeshRenderer _renderer = null;
    private MeshFilter _meshFilter = null;
    //private Material[] _materials = null;

    private void OnValidate()
    {
        //_materials = null;
        GetMaterials();
    }

    public MeshRenderer GetRenderer()
    {
        if (_renderer) return _renderer;
        return _renderer = GetComponent<MeshRenderer>();
    }

    public Material[] GetMaterials()
    {
        var renderer = GetRenderer();
        if (!renderer) return null;

        return renderer.sharedMaterials;

        //if (_materials != null && _materials.Length == renderer.sharedMaterials.Length) return _materials;

        //_materials = new Material[renderer.sharedMaterials.Length];
        //for (int i = 0; i < renderer.sharedMaterials.Length; ++i)
        //    _materials[i] = Instantiate(renderer.sharedMaterials[i]);

        //return _materials;
    }

    public MeshFilter GetMeshFilter()
    {
        if (_meshFilter) return _meshFilter;
        return _meshFilter = GetComponent<MeshFilter>();
    }
}

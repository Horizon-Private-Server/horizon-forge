using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using UnityEditor;
using UnityEngine;

public enum CollisionRenderHandleNormalMode
{
    FrontSide,
    BackSide,
    DoubleSided,
    RecalculateOutside,
    RecalculateInside
}

public class CollisionRenderHandle
{
    const string CHILD_ASSET_COLLIDER_NAME = "renderHandleColliderInstance";

    // collider meshes
    private static Dictionary<GameObject, Mesh> _colliderCache = new Dictionary<GameObject, Mesh>();

    public GameObject AssetInstance { get; private set; }

    public Matrix4x4 Reflection { get => _reflection; set { _changed |= value != _reflection; _reflection = value; } }
    public Quaternion Rotation { get => _rotation; set { _changed |= value != _rotation; _rotation = value; } }
    public Vector3 Offset { get => _offset; set { _changed |= value != _offset; _offset = value; } }
    public Vector3 Scale { get => _scale; set { _changed |= value != _scale; _scale = value; } }
    public bool IsSelected { get => _isSelected; set { _changed |= value != _isSelected; _isSelected = value; } }
    public bool IsHidden { get => _isHidden; set { _changed |= value != _isHidden; _isHidden = value; } }
    public bool IsPicking { get => _isPicking; set { _changed |= value != _isPicking; _isPicking = value; } }
    public Color IdColor { get => _idColor; set { _changed |= value != _idColor; _idColor = value; } }
    public CollisionRenderHandleNormalMode Normals { get => _normals; set { _regenerate |= value != _normals; _normals = value; } }
    public float RecalculateNormalsFactor { get => _recalculateNormalsFactor; set { _regenerate |= value != _recalculateNormalsFactor; _recalculateNormalsFactor = value; } }
    public IEnumerable<ColliderIdOverride> CollisionIdOverrides { get => _idOverrides; set { _regenerate |= value != _idOverrides; _idOverrides = value; } }

    private bool _changed = false;
    private bool _regenerate = false;
    private Matrix4x4 _reflection = Matrix4x4.identity;
    private Quaternion _rotation = Quaternion.identity;
    private Vector3 _offset = Vector3.zero;
    private Vector3 _scale = Vector3.one;
    private bool _isSelected = false;
    private bool _isHidden = false;
    private bool _isPicking = false;
    private CollisionRenderHandleNormalMode _normals = CollisionRenderHandleNormalMode.FrontSide;
    private float _recalculateNormalsFactor = 1f;
    private Color _idColor = Color.clear;
    private IEnumerable<ColliderIdOverride> _idOverrides = null;
    private GameObject _prefab = null;
    private Mesh _mesh = null;
    private Action<Renderer, MaterialPropertyBlock> _updateRenderer;
    private Dictionary<Renderer, Material[]> _instancedMaterials = null;
    private Dictionary<Material, string> _instancedMaterialsNameBackup = null;
    private int? _colliderGeneratedMaterialId = null;

    public CollisionRenderHandle(Action<Renderer, MaterialPropertyBlock> updateRenderer)
    {
        _updateRenderer = updateRenderer;
    }

    public void DestroyAsset()
    {
        DestroyColliderAsset();
    }

    public void UpdateMaterials()
    {
        UpdateColliderAssetMaterials();

        _changed = false;
    }

    #region Collider Asset

    public void Update(GameObject parent, GameObject prefab)
    {
        if (_mesh || prefab != _prefab || !AssetInstance || PrefabUtility.GetCorrespondingObjectFromSource(AssetInstance) != _prefab)
        {
            _prefab = prefab;
            _mesh = null;
            _regenerate = true;
        }

        if (AssetInstance && !AssetInstance.hideFlags.HasFlag(HideFlags.HideInInspector))
        {
            UnityHelper.RecurseHierarchy(AssetInstance.transform, (t) =>
            {
                Hide(t.gameObject);
            });
        }

        if (_regenerate)
        {
            CreateColliderAsset(parent);
            _regenerate = false;
        }

        if (_changed)
        {
            UpdateMaterials();
            _changed = false;
        }

        if (AssetInstance)
        {
            AssetInstance.transform.localRotation = Rotation;
            AssetInstance.transform.localPosition = Offset;
            AssetInstance.transform.localScale = Scale;
        }
    }

    public void Update(GameObject parent, Mesh mesh, int matId)
    {
        if (_colliderGeneratedMaterialId != matId)
        {
            _colliderGeneratedMaterialId = matId;
            _regenerate = true;
        }

        if (_prefab || !AssetInstance || mesh != _mesh)
        {
            _prefab = null;
            _mesh = mesh;
            _regenerate = true;
        }

        if (AssetInstance && !AssetInstance.hideFlags.HasFlag(HideFlags.HideInInspector))
        {
            UnityHelper.RecurseHierarchy(AssetInstance.transform, (t) =>
            {
                Hide(t.gameObject);
            });
        }

        if (_regenerate)
        {
            CreateColliderAsset(parent);
            _regenerate = false;
        }

        if (_changed)
        {
            UpdateMaterials();
            _changed = false;
        }

        if (AssetInstance)
        {
            AssetInstance.transform.localRotation = Rotation;
            AssetInstance.transform.localPosition = Offset;
            AssetInstance.transform.localScale = Scale;
        }
    }

    private void CreateColliderAsset(GameObject parent)
    {
        var layer = LayerMask.NameToLayer("COLLISION");

        DestroyColliderAsset();

        // destroy all hidden children
        for (int i = 0; i < parent.transform.childCount; ++i)
        {
            var child = parent.transform.GetChild(i);
            if (child.name == CHILD_ASSET_COLLIDER_NAME)
            {
                GameObject.DestroyImmediate(child.gameObject);
                --i;
            }
        }

        if (_mesh && _colliderGeneratedMaterialId.HasValue)
        {
            // generate instance
            var assetInstance = AssetInstance = new GameObject(CHILD_ASSET_COLLIDER_NAME);
            var mesh = GenerateColliderFromMesh(_mesh);

            UnityHelper.RecurseHierarchy(assetInstance.transform, (t) =>
            {
                t.gameObject.layer = layer;
                Hide(t.gameObject);
            });

            // renderer
            var meshFilter = assetInstance.AddComponent<MeshFilter>();
            meshFilter.sharedMesh = mesh;

            var meshRenderer = assetInstance.AddComponent<MeshRenderer>();
            var materials = new Material[mesh.subMeshCount];
            for (int i = 0; i < mesh.subMeshCount; ++i)
            {
                var mat = new Material(Shader.Find("Horizon Forge/Collider"));
                mat.name = $"col_{_colliderGeneratedMaterialId.Value:x}";
                mat.SetInteger("_ColId", _colliderGeneratedMaterialId.Value);
                materials[i] = mat;
            }
            meshRenderer.sharedMaterials = materials;

            // collider
            var meshCollider = assetInstance.AddComponent<MeshCollider>();
            meshCollider.sharedMesh = mesh;

            // position
            assetInstance.transform.localRotation = Rotation;
            assetInstance.transform.localPosition = Offset;
            assetInstance.transform.localScale = Scale;
            assetInstance.transform.SetParent(parent.transform, false);
        }
        else if (_prefab)
        {
            try
            {
                var assetInstance = AssetInstance = (GameObject)PrefabUtility.InstantiatePrefab(_prefab);
                if (assetInstance)
                {
                    UnityHelper.RecurseHierarchy(assetInstance.transform, (t) =>
                    {
                        t.gameObject.layer = layer;
                        Hide(t.gameObject);

                        if (t.GetComponent<MeshFilter>() is MeshFilter mf && mf && mf.sharedMesh && mf.sharedMesh.triangles.Length == 0)
                            GameObject.DestroyImmediate(mf);
                    });

                    _instancedMaterials = new Dictionary<Renderer, Material[]>();
                    var renderers = assetInstance.GetComponentsInChildren<Renderer>();
                    foreach (var renderer in renderers)
                    {
                        _instancedMaterials[renderer] = new Material[renderer.sharedMaterials.Length];
                        for (int i = 0; i < renderer.sharedMaterials.Length; i++)
                        {
                            var newMat = Material.Instantiate(renderer.sharedMaterials[i]);
                            newMat.name = renderer.sharedMaterials[i].name;
                            _instancedMaterials[renderer][i] = newMat;
                        }
                        renderer.sharedMaterials = _instancedMaterials[renderer];
                    }

                    assetInstance.name = CHILD_ASSET_COLLIDER_NAME;
                    assetInstance.transform.localRotation = Rotation;
                    assetInstance.transform.localPosition = Offset;
                    assetInstance.transform.localScale = Scale;
                    assetInstance.transform.SetParent(parent.transform, false);
                }

                UpdateColliderAssetMaterials();
            }
            catch (Exception ex)
            {
                Debug.LogError(ex);
                DestroyAsset();
            }
        }
    }

    private void UpdateColliderAssetMaterials()
    {
        if (!AssetInstance) return;

        var mpb = new MaterialPropertyBlock();
        var renderers = AssetInstance.GetComponentsInChildren<Renderer>();
        foreach (var renderer in renderers)
        {
            var materialsAffected = new List<Material>();
            if (_idOverrides != null && _idOverrides.Any() && _instancedMaterials.TryGetValue(renderer, out var materials))
            {
                foreach (var idOverride in _idOverrides)
                {
                    if (int.TryParse(idOverride.OverrideId, System.Globalization.NumberStyles.HexNumber, CultureInfo.InvariantCulture, out var colIdInt))
                    {
                        var mat = materials.FirstOrDefault(x => x.name == idOverride.MaterialName && !materialsAffected.Contains(x));
                        if (mat)
                        {
                            mat.SetInteger("_ColId", colIdInt);
                            materialsAffected.Add(mat);
                        }
                    }
                }
            }
            
            // fix reflection causing objects to clip early
            // expands bounds so renderer renders even in periphery
            renderer.ResetLocalBounds();
            renderer.ResetBounds();
            var b = renderer.bounds;
            b.size += 1000f * Vector3.one;
            //var b = new Bounds(_reflection.MultiplyPoint(renderer.bounds.center)/100f, _reflection.GetScale().magnitude * renderer.bounds.size);
            renderer.bounds = b;
            renderer.enabled = !IsHidden;

            renderer.GetPropertyBlock(mpb);
            mpb.SetMatrix("_Reflection2", Reflection);
            mpb.SetInteger("_Selected", IsSelected ? 1 : 0);
            mpb.SetInteger("_Faded2", IsHidden ? 1 : 0);
            mpb.SetInteger("_Picking", IsPicking ? 1 : 0);
            mpb.SetColor("_IdColor", _idColor);

            // pass to parent
            if (_updateRenderer != null)
                _updateRenderer(renderer, mpb);

            renderer.SetPropertyBlock(mpb);
            renderer.allowOcclusionWhenDynamic = false;
        }
    }

    private void DestroyColliderAsset()
    {
        if (AssetInstance) GameObject.DestroyImmediate(AssetInstance);
        if (_prefab) _colliderCache[_prefab] = null;
        AssetInstance = null;
    }

    private Mesh GenerateColliderFromPrefab(GameObject prefab, int materialId)
    {
        // get meshes
        Mesh[] meshes = prefab.GetComponentsInChildren<MeshFilter>().Select(x => x.sharedMesh).ToArray();

        // build combine args
        List<CombineInstance> combine = new List<CombineInstance>();
        for (int i = 0; i < meshes.Length; i++)
        {
            for (int m = 0; m < meshes[i].subMeshCount; ++m)
            {
                var combineInstance = new CombineInstance()
                {
                    mesh = meshes[i],
                    transform = Matrix4x4.identity,
                    subMeshIndex = m
                };

                combine.Add(combineInstance);
            }
        }

        // combine
        Mesh combinedMesh = new Mesh { indexFormat = UnityEngine.Rendering.IndexFormat.UInt32 };
        combinedMesh.CombineMeshes(combine.ToArray(), true, true, false);

        var normals = new List<Vector3>();
        var vertices = new List<Vector3>();
        combinedMesh.GetNormals(normals);
        combinedMesh.GetVertices(vertices);
        for (int i = 0; i < combinedMesh.subMeshCount; ++i)
        {
            var triangles = combinedMesh.GetTriangles(i);
            for (int j = 0; j < triangles.Length; j += 3)
            {
                var v0 = vertices[triangles[j + 0]];
                var v1 = vertices[triangles[j + 1]];
                var v2 = vertices[triangles[j + 2]];
                var n0 = normals[triangles[j + 0]];
                var n1 = normals[triangles[j + 1]];
                var n2 = normals[triangles[j + 2]];
                var n = (n0 + n1 + n2) / 3;
                var cn = Vector3.Cross(v1 - v0, v2 - v0);

                // flip
                if (Vector3.Dot(n, cn) < 0)
                {
                    var t = triangles[j + 2];
                    triangles[j + 2] = triangles[j];
                    triangles[j] = t;
                }
            }
            combinedMesh.SetTriangles(triangles, i);
        }

        //switch (Normals)
        //{

        //}

        //if (FlipNormals)
        //{
        //    for (int i = 0; i < combinedMesh.subMeshCount; ++i)
        //    {
        //        var triangles = combinedMesh.GetTriangles(i);
        //        for (int j = 0; j < triangles.Length; j += 3)
        //        {
        //            var t = triangles[j + 2];
        //            triangles[j + 2] = triangles[j];
        //            triangles[j] = t;
        //        }
        //        combinedMesh.SetTriangles(triangles, i);
        //    }
        //}

        //combinedMesh.RecalculateNormals();
        //combinedMesh.RecalculateTangents();

        return combinedMesh;
    }

    private Mesh GenerateColliderFromMesh(Mesh mesh)
    {
        if (Normals == CollisionRenderHandleNormalMode.FrontSide) return mesh;

        var newMesh = new Mesh()
        {
            vertices = mesh.vertices,
            triangles = mesh.triangles,
            normals = mesh.normals,
            tangents = mesh.tangents,
            bounds = mesh.bounds,
            uv = mesh.uv
        };

        switch (Normals)
        {
            case CollisionRenderHandleNormalMode.BackSide:
                {
                    // flip normals
                    newMesh.FlipFaces();
                    break;
                }
            case CollisionRenderHandleNormalMode.DoubleSided:
                {
                    newMesh.AddBackSideFaces();
                    break;
                }
            case CollisionRenderHandleNormalMode.RecalculateOutside:
                {
                    newMesh.RecalculateFaceNormals(normalFactor: _recalculateNormalsFactor, flip: false);
                    break;
                }
            case CollisionRenderHandleNormalMode.RecalculateInside:
                {
                    newMesh.RecalculateFaceNormals(normalFactor: _recalculateNormalsFactor, flip: true);
                    break;
                }
        }


        return newMesh;
    }

    #endregion

    private void Hide(GameObject go)
    {
        go.hideFlags = HideFlags.DontSave | HideFlags.NotEditable | HideFlags.HideInInspector | HideFlags.HideInHierarchy;
        //go.hideFlags = HideFlags.DontSave | HideFlags.NotEditable | HideFlags.HideInInspector;
        //go.hideFlags = HideFlags.DontSave;
    }

    #region Collision Bake

    public void OnPreBake()
    {
        _instancedMaterialsNameBackup = null;
        if (_instancedMaterials != null)
        {
            _instancedMaterialsNameBackup = new Dictionary<Material, string>();
            foreach (var material in _instancedMaterials.SelectMany(x => x.Value))
            {
                var newName = $"col_{material.GetInteger("_ColId"):x}";
                _instancedMaterialsNameBackup[material] = material.name;
                material.name = newName;
            }
        }
    }

    public void OnPostBake()
    {
        if (_instancedMaterialsNameBackup != null)
        {
            foreach (var item in _instancedMaterialsNameBackup)
                item.Key.name = item.Value;
        }

        _instancedMaterialsNameBackup = null;
    }

    #endregion

}

public interface IInstancedCollider
{
    bool HasInstancedCollider();
    CollisionRenderHandle GetInstancedCollider();
}

[Serializable]
public class ColliderIdOverride
{
    public string MaterialName;
    public string OverrideId;

    public ColliderIdOverride() { }
    public ColliderIdOverride(string materialName, string overrideId) { this.MaterialName = materialName; this.OverrideId = overrideId; }
    public ColliderIdOverride(ColliderIdOverride copyFrom) { this.MaterialName = copyFrom.MaterialName; this.OverrideId = copyFrom.OverrideId; }
}

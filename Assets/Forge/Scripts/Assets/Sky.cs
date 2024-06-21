using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEngine;
using UnityEngine.SceneManagement;

[ExecuteInEditMode]
public class Sky : MonoBehaviour
{
    public int MaxSpriteCount = 0;
    public SkyLayer[] Layers => layers;

    SkyLayer[] layers;
    Material emptySkyMat;
    Mesh sphereMesh;
    MapConfig mapConfig;

    void OnEnable()
    {
        layers = this.GetComponentsInChildren<SkyLayer>(true);
        emptySkyMat = AssetDatabase.LoadAssetAtPath<Material>(Path.Combine(FolderNames.ForgeFolder, "Sky", "Skymesh.mat"));
        var skySphereGo = AssetDatabase.LoadAssetAtPath<GameObject>(Path.Combine(FolderNames.ForgeFolder, "Sky", "SkySphere.fbx"));
        sphereMesh = skySphereGo.GetComponent<MeshFilter>().sharedMesh;

        Camera.onPreCull -= OnRender;
        Camera.onPreCull += OnRender;
    }

    private void OnDisable()
    {
        Camera.onPreCull -= OnRender;
    }

    private void OnRender(Camera camera)
    {
        if (layers == null) return;
        if (mapConfig == null) mapConfig = FindObjectOfType<MapConfig>();

        RenderShell(camera, -1, emptySkyMat, sphereMesh, 0, Quaternion.identity, Vector3.zero, false, mapConfig.BackgroundColor);
        for (int idx = 0; idx < layers.Length; ++idx)
        {
            var layer = layers[idx];
            if (!layer) continue;
            if (!layer.gameObject.activeInHierarchy) continue;

            var renderer = layer.GetRenderer();
            var materials = layer.GetMaterials();
            var meshFilter = layer.GetMeshFilter();
            if (!meshFilter || !meshFilter.sharedMesh || !renderer || materials == null) continue;

            var matIdx = 0;
            foreach (var mat in materials)
                RenderShell(camera, idx, mat, meshFilter.sharedMesh, matIdx++ % meshFilter.sharedMesh.subMeshCount, layer.transform.rotation, layer.AngularRotation, layer.Bloom, null);
            renderer.enabled = false;
        }
    }

    private void RenderShell(Camera camera, int idx, Material material, Mesh mesh, int submeshIdx, Quaternion rotation, Vector3 angularVelocity, bool bloom, Color? color)
    {
        var rot = Quaternion.Euler(90, 0, 0) * rotation * GetAngularRotation(angularVelocity);
        //material.SetPass(0);
        //Graphics.DrawMeshNow(mesh, Matrix4x4.TRS(Vector3.zero, rot, Vector3.one * 1000));
        //Graphics.DrawMesh(mesh, Matrix4x4.TRS(Vector3.zero, rot, Vector3.one * 1000), material, 0, camera, 0);

        RenderParams rp = new RenderParams(material);
        rp.camera = camera;
        rp.worldBounds = new Bounds(camera.transform.position, Vector3.one * 10000);
        var mpb = rp.matProps = new MaterialPropertyBlock();

        if (color.HasValue) mpb.SetColor("_Color", color.Value);
        //else if (material.name.Contains("gouraud")) mpb.SetColor("_Color", mapConfig.BackgroundColor);
        mpb.SetFloat("_Bloom", bloom ? 1 : 0);
        material.renderQueue = 1000 + idx;

        Graphics.RenderMesh(rp, mesh, submeshIdx, Matrix4x4.TRS(Vector3.zero, rot, Vector3.one * 10));
    }

    private Quaternion GetAngularRotation(Vector3 angularVelocity)
    {
        return Quaternion.Euler(
            Mathf.DeltaAngle(0, (float)((angularVelocity.x * EditorApplication.timeSinceStartup) % 360)),
            Mathf.DeltaAngle(0, (float)((angularVelocity.y * EditorApplication.timeSinceStartup) % 360)),
            Mathf.DeltaAngle(0, (float)((angularVelocity.z * EditorApplication.timeSinceStartup) % 360))
            );
    }
}

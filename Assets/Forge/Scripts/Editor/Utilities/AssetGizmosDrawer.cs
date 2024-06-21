using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using UnityEditor.Search;
using UnityEngine;
using UnityEngine.Profiling;
using UnityEngine.SceneManagement;

public static class AssetGizmosDrawer
{
    static Dictionary<Mesh, Mesh> _meshFlipCache = new Dictionary<Mesh, Mesh>();
    static Material mat;

    [InitializeOnLoadMethod]
    static void Hook()
    {
        SceneManager.activeSceneChanged -= SceneManager_activeSceneChanged;
        SceneManager.activeSceneChanged += SceneManager_activeSceneChanged;
    }

    private static void SceneManager_activeSceneChanged(Scene arg0, Scene arg1)
    {
        // clear mesh cache when scene loads
        // just in case the cache is ever wrong
        // we want a way to easily reset it
        _meshFlipCache.Clear();
    }

    [DrawGizmo(GizmoType.NotInSelectionHierarchy | GizmoType.Pickable | GizmoType.Selected | GizmoType.NonSelected)]
    static void DrawMeshGizmos(RenderSelectionBase selectionBase, GizmoType gizmoType)
    {
        var selected = false; // gizmoType.HasFlag(GizmoType.Selected) | gizmoType.HasFlag(GizmoType.InSelectionHierarchy);
        Gizmos.color = new Color(0, 0, 1, selected ? 0.25f : 0f);

        var renderers = selectionBase.GetSelectionRenderers();
        var reflection = selectionBase.GetSelectionReflectionMatrix();
        reflection[15] = 1;
        if (renderers != null)
        {
            foreach (var renderer in renderers)
            {
                var meshFilter = renderer.GetComponent<MeshFilter>();
                if (!meshFilter || !meshFilter.sharedMesh || meshFilter.sharedMesh.vertexCount == 0) continue;

                Gizmos.matrix = reflection * renderer.transform.localToWorldMatrix;
                //Gizmos.DrawMesh(meshFilter.sharedMesh, Vector3.zero, Quaternion.identity, Vector3.one * 1f);

                // Gizmos.DrawMesh draws the mesh one-sided
                // since we render everything two-sided
                // we need to create a copy of the mesh with duplicated and flipped faces
                // in order for it to be selectable from the backside
                if (!_meshFlipCache.TryGetValue(meshFilter.sharedMesh, out var flippedMesh) || !flippedMesh)
                    _meshFlipCache[meshFilter.sharedMesh] = flippedMesh = DupeFlipMesh(meshFilter.sharedMesh);

                if (flippedMesh)
                    Gizmos.DrawMesh(flippedMesh, Vector3.zero, Quaternion.identity, Vector3.one * 1f);
            }
        }

        Gizmos.matrix = Matrix4x4.identity;
    }

    static Mesh DupeFlipMesh(Mesh mesh)
    {
        var newMesh = new Mesh()
        {
            vertices = mesh.vertices,
            normals = mesh.normals
        };

        var triangles = new int[mesh.triangles.Length * 2];
        for (int i = 0; i < mesh.triangles.Length; i += 3)
        {
            triangles[i * 2 + 0] = mesh.triangles[i + 0];
            triangles[i * 2 + 1] = mesh.triangles[i + 1];
            triangles[i * 2 + 2] = mesh.triangles[i + 2];
            triangles[i * 2 + 3] = mesh.triangles[i + 2];
            triangles[i * 2 + 4] = mesh.triangles[i + 1];
            triangles[i * 2 + 5] = mesh.triangles[i + 0];
        }
        newMesh.SetTriangles(triangles, 0);

        return newMesh;
    }
}

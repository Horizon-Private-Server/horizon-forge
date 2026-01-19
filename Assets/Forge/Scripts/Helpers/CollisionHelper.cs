using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnityEngine;

public static class CollisionHelper
{
    public static Color GetColor(int colId)
    {
        return new Color(
            ((colId & 0x03) << 6) / 255f,
            ((colId & 0x0C) << 4) / 255f,
            ((colId & 0xF0) << 0) / 255f,
            1
            );
    }

    public static int ParseId(string colStr, int? defaultColId = null)
    {
        if (int.TryParse(colStr, System.Globalization.NumberStyles.HexNumber, CultureInfo.InvariantCulture, out var id))
            return id;

        return defaultColId ?? 0x2f; // default
    }

    public static bool Raycast(Vector3 position, Vector3 direction, float maxDistance, out RaycastHit hitInfo, out int collisionId)
    {
        collisionId = 0;
        var layerMask = LayerMask.GetMask("COLLISION");

        if (Physics.Raycast(position, direction.normalized, out hitInfo, maxDistance, layerMask))
        {
            var mc = hitInfo.collider as MeshCollider;
            var mr = hitInfo.transform.GetComponent<MeshRenderer>();
            if (!mr) mr = hitInfo.transform.GetComponentInChildren<MeshRenderer>();

            if (mr && mc)
            {
                Mesh mesh = mc.sharedMesh;
                int triangleOffset = 0;

                // Find which submesh contains the triangle index
                for (int submeshIndex = 0; submeshIndex < mesh.subMeshCount; submeshIndex++)
                {
                    var submeshInfo = mesh.GetSubMesh(submeshIndex);
                    int triangleCount = submeshInfo.indexCount / 3;

                    if (hitInfo.triangleIndex >= triangleOffset && hitInfo.triangleIndex < triangleOffset + triangleCount)
                    {
                        // Found the submesh that contains this triangle
                        Material hitMaterial = mr.sharedMaterials[submeshIndex];
                        if (hitMaterial.shader.name != "Horizon Forge/Collider") break;

                        collisionId = hitMaterial.GetInteger("_ColId") & 0x1f;
                        return true;
                    }

                    triangleOffset += triangleCount;
                }
            }

            return true;
        }

        return false;
    }
}

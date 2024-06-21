using System.Collections;
using System.Collections.Generic;
using Unity.VisualScripting;
using UnityEditor;
using UnityEngine;

[ExecuteInEditMode]
public class CameraCullFix : MonoBehaviour
{
    Camera m_Camera;

    private void Start()
    {
        m_Camera = GetComponent<Camera>();
    }

    private void OnPreRender()
    {
        if (m_Camera)
            m_Camera.cullingMatrix = Matrix4x4.Perspective(m_Camera.fieldOfView, m_Camera.aspect, 0, 10000) * m_Camera.worldToCameraMatrix * Matrix4x4.Translate(m_Camera.cameraToWorldMatrix * Vector3.forward * -100f);
    }
}

using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class MapRender : MonoBehaviour
{
    public int RenderScale = 512;
    public Color BackgroundColor = new Color32(0x66, 0x66, 0x66, 0);

    private void OnValidate()
    {
        UpdateCamera();
    }

    public void UpdateCamera()
    {
        // setup camera
        var camera = GetComponent<Camera>();
        camera.backgroundColor = BackgroundColor;
        camera.transform.localRotation = Quaternion.LookRotation(Vector3.down, Vector3.forward);
        camera.orthographicSize = transform.localScale.z / 2f;
        camera.nearClipPlane = 0.01f;
        camera.farClipPlane = transform.localScale.y;
    }
}

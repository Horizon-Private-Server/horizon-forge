using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEngine;
using UnityEngine.SceneManagement;

[ExecuteInEditMode, AddComponentMenu("")]
public class RaidsGateMoby : MonoBehaviour, IRenderHandlePrefab
{
    private Moby m_Moby;
    private float m_Height;
    private float m_Length;

    void Start()
    {
        m_Moby = GetComponentInParent<Moby>();

        UpdateMaterial();
    }

    // Start is called before the first frame update
    void OnEnable()
    {
        m_Moby = GetComponentInParent<Moby>();

        UpdateMaterial();
    }

    public void UpdateMaterials()
    {
        UpdateMaterial();
    }

    private void UpdateMaterial()
    {
        if (!m_Moby) return;
        if (m_Moby.OClass != 0x4004) return;

        float.TryParse(m_Moby.PVarValues[".Length"], out m_Length);
        float.TryParse(m_Moby.PVarValues[".Height"], out m_Height);

        m_Moby.transform.localScale = Vector3.one;
        this.transform.localScale = new Vector3(1, m_Height, m_Length);
    }
}

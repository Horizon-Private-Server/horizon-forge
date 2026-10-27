using UnityEditor;

[CustomEditor(typeof(RaidsContractDef))]
public class RaidsContractDefEditor : Editor
{
    public override void OnInspectorGUI()
    {
        serializedObject.Update();

        DrawPropertiesExcluding(serializedObject, "m_Script");
        serializedObject.ApplyModifiedProperties();
    }
}

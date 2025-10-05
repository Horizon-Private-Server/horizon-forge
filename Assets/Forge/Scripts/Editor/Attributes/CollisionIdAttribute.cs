using UnityEditor;
using UnityEngine;

public class CollisionIdAttribute : PropertyAttribute
{

}

[CustomPropertyDrawer(typeof(CollisionIdAttribute))]
public class CollisionIdDrawer : PropertyDrawer
{
    static readonly string[] COL_OPTIONS = new[]
    {
        "Swimmable Water (0x00)",
        "Acid (0x01)",
        "Magnet Wall (0x02)",
        "Water (0x03)",
        "DreadZone OOB (ring of fire) (0x04)",
        "Electricity (0x05)",
        "Nonwalkable Magnet Wall (0x06)",
        "Walkable Surface 1 (0x07)",
        "Nonwalkable Surface 1 (0x08)",
        "Walkable Surface 2 (0x09)",
        "Walkable Surface 3 (0x0A)",
        "Lethal Water (0x0B)",
        "Nonwalkable Surface 2 (0x0C)",
        "Lethal Water Ice Cube (0x0D)",
        "Water Trail (0x0E)",
        "Walkable Surface 4 (0x0F)",
        "Player Only"
    };

    static readonly string[] SOUND_OPTIONS = new[]
    {
        "Stone",
        "Level Sound 1",
        "Level Sound 2",
        "Level Sound 3",
        "Level Sound 4",
        "Level Sound 5",
        "Level Sound 6",
        "Level Sound 7"
    };

    public override float GetPropertyHeight(SerializedProperty property,
                                            GUIContent label)
    {
        return EditorGUI.GetPropertyHeight(property, label, true);
    }

    public override void OnGUI(Rect position,
                               SerializedProperty property,
                               GUIContent label)
    {

        var id = CollisionHelper.ParseId(property.stringValue);
        var colId = id & 0x1f;
        var soundId = (id >> 5) & 0x7;
        if (id == 256)
        {
            soundId = 0;
            colId = COL_OPTIONS.Length - 1;
        }

        EditorGUI.BeginProperty(position, label, property);
        var rect = EditorGUI.PrefixLabel(position, label);
        var xSplit = rect.width * 1f;
        var xOff = rect.width - xSplit;
        var strWidth = 24;

        var isHeroCollision = colId == (COL_OPTIONS.Length-1);
        var indentation = EditorGUI.indentLevel;
        EditorGUI.indentLevel = 0;
        if (!isHeroCollision)
            soundId = EditorGUI.Popup(EditorGUI.IndentedRect(new Rect(rect.x + xOff, rect.y, (xSplit * 0.5f) - strWidth, rect.height)), soundId, SOUND_OPTIONS);
        
        colId = EditorGUI.Popup(new Rect(rect.x + xOff + ((xSplit - strWidth) * 0.5f), rect.y, (xSplit * 0.5f) - strWidth, rect.height), colId, COL_OPTIONS);
        isHeroCollision = colId == (COL_OPTIONS.Length-1);

        GUI.enabled = false;
        EditorGUI.TextField(new Rect(position.x + position.width - strWidth, position.y, strWidth, position.height), property.stringValue);
        GUI.enabled = true;

        var colStr = isHeroCollision ? "100" : ((colId & 0x1f) | ((soundId & 0x7) << 5)).ToString("x2");
        if (colStr != property.stringValue)
            property.stringValue = colStr;

        EditorGUI.indentLevel = indentation;
        EditorGUI.EndProperty();
    }
}

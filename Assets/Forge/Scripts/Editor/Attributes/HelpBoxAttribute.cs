using UnityEditor;
using UnityEngine;
using UnityEngine.UIElements;

public class HelpBoxAttribute : PropertyAttribute
{
    public string text;
    public MessageType messageType;
    public bool placeAbove = false;

    public HelpBoxAttribute(string text, MessageType messageType = MessageType.None, bool placeAbove = false)
    {
        this.text = text;
        this.messageType = messageType;
        this.placeAbove = placeAbove;
    }
}

[CustomPropertyDrawer(typeof(HelpBoxAttribute))]
public class HelpBoxDrawer : PropertyDrawer
{
    private const int INSPECTOR_WIDTH_PADDING = 30;

    public override void OnGUI(Rect position, SerializedProperty property, GUIContent label)
    {
        var guiEnabled = GUI.enabled;
        var height = base.GetPropertyHeight(property, label);
        var heightWithPadding = height + EditorGUIUtility.standardVerticalSpacing;

        // draw base if no attr
        var helpBoxAttribute = attribute as HelpBoxAttribute;
        if (helpBoxAttribute == null)
        {
            EditorGUI.PropertyField(position, property, label);
            return;
        }

        // calculate helpbox height
        var style = GUI.skin.GetStyle("HelpBox");
        float helpBoxHeight = style.CalcHeight(new GUIContent(helpBoxAttribute.text), EditorGUIUtility.currentViewWidth - INSPECTOR_WIDTH_PADDING);
        var helpBoxHeightWithPadding = helpBoxHeight + EditorGUIUtility.standardVerticalSpacing;

        if (helpBoxAttribute.placeAbove)
        {
            var propertyRect = new Rect(position.x, position.y + helpBoxHeightWithPadding, position.width, height);
            var helpBoxRect = new Rect(position.x, position.y, position.width, helpBoxHeight);

            // draw the help box
            GUI.enabled = true;
            EditorGUI.HelpBox(helpBoxRect, helpBoxAttribute.text, helpBoxAttribute.messageType);
            GUI.enabled = guiEnabled;

            // draw the default property field
            EditorGUI.PropertyField(propertyRect, property, label);
        }
        else
        {
            var propertyRect = new Rect(position.x, position.y, position.width, height);
            var helpBoxRect = new Rect(position.x, position.y + heightWithPadding, position.width, helpBoxHeight);

            // draw the default property field
            EditorGUI.PropertyField(propertyRect, property, label);

            // draw the help box
            GUI.enabled = true;
            EditorGUI.HelpBox(helpBoxRect, helpBoxAttribute.text, helpBoxAttribute.messageType);
            GUI.enabled = guiEnabled;
        }

    }

    public override float GetPropertyHeight(SerializedProperty property, GUIContent label)
    {
        var height = base.GetPropertyHeight(property, label);
        var helpBoxAttribute = attribute as HelpBoxAttribute;
        if (helpBoxAttribute == null) return height;

        var style = GUI.skin.GetStyle("HelpBox");
        var helpBoxHeight = style.CalcHeight(new GUIContent(helpBoxAttribute.text), EditorGUIUtility.currentViewWidth - INSPECTOR_WIDTH_PADDING);
        return height + helpBoxHeight + EditorGUIUtility.standardVerticalSpacing;
    }
}

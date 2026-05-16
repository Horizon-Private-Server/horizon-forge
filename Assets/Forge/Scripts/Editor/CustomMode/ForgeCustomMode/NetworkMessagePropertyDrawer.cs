using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnityEditor;
using UnityEngine;
using UnityEngine.SceneManagement;
using UnityEngine.UIElements;

[CustomPropertyDrawer(typeof(ForgeCustomModeNetworkMessageModule.NetworkMessage))]
public class NetworkMessagePropertyDrawer : PropertyDrawer
{
    public override float GetPropertyHeight(SerializedProperty property, GUIContent label)
    {
		var height = EditorGUI.GetPropertyHeight(property, label, includeChildren: true);

        SerializedProperty handlersProp = property.FindPropertyRelative("RecvHandlers");
		if (handlersProp.isExpanded)
			height += 3 * EditorGUIUtility.singleLineHeight;
		
		// info
		height += 3 * EditorGUIUtility.singleLineHeight;
        
        return height;
    }

    public override void OnGUI(Rect position, SerializedProperty property, GUIContent label)
    {
		var networkMessage = property.boxedValue as ForgeCustomModeNetworkMessageModule.NetworkMessage;

        // Begin property drawing
        EditorGUI.BeginProperty(position, label, property);
        
        // Draw label
        // position = EditorGUI.PrefixLabel(position, GUIUtility.GetControlID(FocusType.Passive), label);
        
        // Calculate rects for each field
        float lineHeight = EditorGUIUtility.singleLineHeight;
        float verticalSpacing = EditorGUIUtility.standardVerticalSpacing;
        
        float currentY = position.y;
        
        // Name field
        Rect nameRect = new Rect(position.x, currentY, position.width, lineHeight);
        EditorGUI.PropertyField(nameRect, property.FindPropertyRelative("Name"), new GUIContent("Name"));
        currentY += lineHeight + verticalSpacing;
        
        // RelayToLocalOnBroadcast field
        Rect relayRect = new Rect(position.x, currentY, position.width, lineHeight);
        EditorGUI.PropertyField(relayRect, property.FindPropertyRelative("RelayToLocalOnBroadcast"), new GUIContent("Relay Local"));
        currentY += lineHeight + verticalSpacing;
        
        // SendUnreliably field
        Rect unreliableRect = new Rect(position.x, currentY, position.width, lineHeight);
        EditorGUI.PropertyField(unreliableRect, property.FindPropertyRelative("SendUnreliably"), new GUIContent("Unreliable"));
        currentY += lineHeight + verticalSpacing;
        
        // HostOnly field
        Rect hostOnlyRect = new Rect(position.x, currentY, position.width, lineHeight);
        EditorGUI.PropertyField(hostOnlyRect, property.FindPropertyRelative("HostOnly"), new GUIContent("Host Only"));
        currentY += lineHeight + verticalSpacing;
        
        // Args list
        SerializedProperty argsProp = property.FindPropertyRelative("Args");
        Rect argsRect = new Rect(position.x, currentY, position.width, EditorGUI.GetPropertyHeight(argsProp, true));
		EditorGUI.PropertyField(argsRect, argsProp, true);
        currentY += argsRect.height + verticalSpacing;
        
        // RecvHandlers list
        SerializedProperty handlersProp = property.FindPropertyRelative("RecvHandlers");
		Rect handlersRect = new Rect(position.x, currentY, position.width, EditorGUI.GetPropertyHeight(handlersProp, true));
		EditorGUI.PropertyField(handlersRect, handlersProp, true);
        currentY += handlersRect.height + verticalSpacing;

		if (handlersProp.isExpanded)
		{
			Rect handlersHelpRect = new Rect(position.x, currentY, position.width, lineHeight * 3);
			EditorGUI.HelpBox(handlersHelpRect, $"Enter name of function to be invoked when message is received. Function signature must match the following:\nvoid yourFunctionName(int fromClientId, struct {networkMessage.StructTypeName} *msg)", MessageType.Info);
        	currentY += handlersHelpRect.height + verticalSpacing;
		}

		//
		Rect infoHelpRect = new Rect(position.x, currentY, position.width, lineHeight * 3);
		EditorGUI.HelpBox(infoHelpRect, $"struct {networkMessage.StructTypeName}\n{networkMessage.BroadcastFuncName}(&msg);\n{networkMessage.SendFuncName}(toClientId, &msg);", MessageType.Info);
		currentY += infoHelpRect.height + verticalSpacing;
        
        // End property drawing
        EditorGUI.EndProperty();
    }
}
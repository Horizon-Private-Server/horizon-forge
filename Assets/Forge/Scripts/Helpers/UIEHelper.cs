using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEngine;
using UnityEngine.UIElements;

public static class UIEHelper
{
    public static void BuildHeading(this VisualElement root, string heading)
    {
        var titleLabel = new Label(heading);
        titleLabel.style.unityTextAlign = TextAnchor.MiddleCenter;
        titleLabel.style.fontSize = 18;
        root.Add(titleLabel);
    }

    public static void BuildPadding(this VisualElement root)
    {
        var padding = new VisualElement();
        padding.style.height = 10;
        root.Add(padding);
    }

    public static void BuildRow(this VisualElement root, string title, Action<VisualElement> buildElement)
    {
        var row = new VisualElement();
        var content = new VisualElement();
        var label = new Label(title);
        row.Add(label);
        row.Add(content);
        row.style.flexDirection = FlexDirection.Row;
        row.style.width = new Length(100, LengthUnit.Percent);
        content.style.width = new Length(50, LengthUnit.Percent);
        label.style.width = new Length(50, LengthUnit.Percent);
        buildElement(content);
        root.Add(row);
    }

    public static void BuildFileBrowser(this VisualElement root, string title, string value, Action<string> valueChanged, string[] filters = null)
    {
        root.BuildRow(title, (container) =>
        {
            var textField = new TextField();
            textField.SetValueWithoutNotify(value);
            textField.RegisterValueChangedCallback((e) => valueChanged?.Invoke(e.newValue));
            textField.style.flexGrow = 1;
            textField.style.flexShrink = 1;

            var browseButton = new Button(() =>
            {
                var selectedFile = EditorUtility.OpenFilePanelWithFilters(title, value, filters);
                if (!String.IsNullOrEmpty(selectedFile)) { textField.SetValueWithoutNotify(selectedFile); valueChanged?.Invoke(selectedFile); }
            });
            browseButton.text = "Browse";
            browseButton.style.width = 100;

            container.style.flexDirection = FlexDirection.Row;
            container.Add(textField);
            container.Add(browseButton);
        });
    }
}

using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEngine;
using UnityEngine.UIElements;

public class ForgeStartupWindow : EditorWindow
{
    static readonly Color unhoverColor = new Color(0.8f, 0.8f, 0.8f, 1f);
    static readonly Color hoverColor = Color.white;

    // populated by inspector
    public Texture2D LogoBlender;
    public Texture2D IconCheck;
    public Texture2D IconCross;
    public Texture2D IconWrench;
    public Texture2D IconDotNetCore;

    float scrollPosition = 0;
    ForgeSettings forgeSettings;

    bool dlIsoValid = false;
    bool uyaNtscIsoValid = false;
    bool uyaPalIsoValid = false;
    bool gcNtscIsoValid = false;

    [MenuItem("Forge/Startup", priority = 10000)]
    public static void CreateNewWindow()
    {
        var wnd = GetWindow<ForgeStartupWindow>();
        wnd.titleContent = new GUIContent("Forge Startup");
        wnd.minSize = new Vector2(700, 500);
        wnd.forgeSettings = wnd.GetOrCreateForgeSettings();
        wnd.ValidateISOs();
        wnd.CreateGUI();
    }

    private void CreateGUI()
    {
        if (!forgeSettings) return;

        VisualElement root = rootVisualElement;
        root.Clear();

        // scroll
        var scrollregion = new ScrollView(ScrollViewMode.Vertical);
        scrollregion.verticalScroller.value = scrollPosition;
        scrollregion.verticalScroller.valueChanged += (e) => scrollPosition = e;
        root.Add(scrollregion);

        // title
        var pageHeading = new Label($"Horizon Forge {Constants.ForgeVersion}");
        pageHeading.style.unityTextAlign = TextAnchor.MiddleCenter;
        pageHeading.style.unityFontStyleAndWeight = FontStyle.Bold;
        pageHeading.style.fontSize = 24;
        scrollregion.Add(pageHeading);

        // subheading
        var pageSubheading = new Label($"A Map Editor for Ratchet and Clank UYA & Deadlocked Multiplayer");
        pageSubheading.style.unityTextAlign = TextAnchor.MiddleCenter;
        pageSubheading.style.fontSize = 18;
        scrollregion.Add(pageSubheading);

        CreateGroupHeader(scrollregion, "Welcome!");
        var groupBox = CreateGroupBox(scrollregion, width: 600);
        CreateLabel(groupBox, "Before you can begin making your own maps you will need to provide a couple things.");
        CreateLabel(groupBox, "Please complete each step before continuing.");
        CreateLabel(groupBox, $"Please refer to our <a href=\"{Constants.WikiUrl}\">wiki for a more complete Setup Guide</a>.");
        CreateLabel(groupBox, $"If you have any questions please make an issue on our <a href=\"{Constants.RepoUrl}\">github</a> or ask in our <a href=\"{Constants.DiscordInviteUrl}\">discord</a>.");

        // STEP1 INSTALLS
        CreateGroupHeader(scrollregion, "Step 1");
        groupBox = CreateGroupBox(scrollregion, width: 600);
        CreateLabel(groupBox, "Forge requires a few things be installed in order to run properly.");
        CreateLabel(groupBox, ""); // padding

        // Blender
        var hasBlender = HasBlender();
        CreateDependencyBox(groupBox, LogoBlender, hasBlender, hasBlender ? "Blender installation detected." : "Blender installation not detected. Please install the latest Blender at <a href=\"https://www.blender.org/download/\">https://www.blender.org/download/</a>.");

        // Tools
        var hasTools = HasPacker() && HasWrench();
        CreateDependencyBox(groupBox, IconWrench, hasTools, hasTools ? "Tools package detected." : $"Tools package not detected. Please download and install the tools package at <a href=\"{Constants.RepoUrl}/releases\">{Constants.RepoUrl}/releases</a>.");

        // .NETCore 3.1
        var hasDotNetCore = HasPacker() && HasDotNetCore();
        CreateDependencyBox(groupBox, IconDotNetCore, hasDotNetCore, hasDotNetCore ? ".NET Desktop Runtime 3.1 installation detected." : $".NET Desktop Runtime 3.1 installation not detected. Please ensure the Tools package is installed first. Then download and install at <a href=\"https://dotnet.microsoft.com/en-us/download/dotnet/3.1#runtime-desktop-3.1.32\">https://dotnet.microsoft.com/en-us/download/dotnet/3.1#runtime-desktop-3.1.32</a>.");

        // refresh
        CreateLabel(groupBox, ""); // padding
        CreateButton(groupBox, "Refresh Installations", () => { CreateGUI(); });

        // STEP2 ISO PATHS
        CreateGroupHeader(scrollregion, "Step 2");
        groupBox = CreateGroupBox(scrollregion, width: 600);
        CreateLabel(groupBox, "Please provide the path to each of the following Ratchet (PS2) games (ISO only). These will be used throughout the editor.");
        CreateLabel(groupBox, ""); // padding

        // clean isos
        CreateOpenFileBrowser(CreateValidationRow(groupBox, dlIsoValid), $"Deadlocked ISO (NTSC) (required *)", forgeSettings.PathToCleanDeadlockedIso, (v) => OnDeadlockedCleanISOPathUpdated(forgeSettings, v), "ISO", "iso");
        CreateOpenFileBrowser(CreateValidationRow(groupBox, uyaNtscIsoValid || string.IsNullOrEmpty(forgeSettings.PathToCleanUyaNtscIso)), $"Up Your Arsenal (NTSC) ISO (recommended)", forgeSettings.PathToCleanUyaNtscIso, (v) => OnUyaNtscCleanISOPathUpdated(forgeSettings, v), "ISO", "iso");
        CreateOpenFileBrowser(CreateValidationRow(groupBox, uyaPalIsoValid || string.IsNullOrEmpty(forgeSettings.PathToCleanUyaPalIso)), $"Ratchet and Clank 3 (PAL) ISO (recommended)", forgeSettings.PathToCleanUyaPalIso, (v) => OnUyaPalCleanISOPathUpdated(forgeSettings, v), "ISO", "iso");
        CreateOpenFileBrowser(CreateValidationRow(groupBox, gcNtscIsoValid || string.IsNullOrEmpty(forgeSettings.PathToCleanGcIso)), "Going Commando (NTSC) ISO (recommended)", forgeSettings.PathToCleanGcIso, (v) => OnGcNtscCleanISOPathUpdated(forgeSettings, v), "ISO", "iso");

        // output iso
        CreateLabel(groupBox, ""); // padding
        CreateHelpBox(groupBox, $"<b>This ISO is auto-generated by Forge. When building your custom map, it will be patched with your map such that you can load and test your custom map from within Local Play with PCSX2.</b>", HelpBoxMessageType.Warning);
        CreateSaveFileBrowser(groupBox, $"Output Deadlocked ISO", forgeSettings.PathToOutputDeadlockedIso, (v) => { forgeSettings.PathToOutputDeadlockedIso = v; CreateGUI(); }, "iso");
        CreateSaveFileBrowser(groupBox, $"Output Up Your Arsenal (NTSC) ISO", forgeSettings.PathToOutputUyaNtscIso, (v) => { forgeSettings.PathToOutputUyaNtscIso = v; CreateGUI(); }, "iso");
        CreateSaveFileBrowser(groupBox, $"Output Ratchet and Clank 3 (PAL) ISO", forgeSettings.PathToOutputUyaPalIso, (v) => { forgeSettings.PathToOutputUyaPalIso = v; CreateGUI(); }, "iso");

        // STEP3 SAVE & IMPORT
        var hasRequiredIsos =
            (dlIsoValid && !string.IsNullOrEmpty(forgeSettings.PathToCleanDeadlockedIso) && !string.IsNullOrEmpty(forgeSettings.PathToOutputDeadlockedIso)) &&
            (string.IsNullOrEmpty(forgeSettings.PathToCleanUyaNtscIso) || (uyaNtscIsoValid && !string.IsNullOrEmpty(forgeSettings.PathToOutputUyaNtscIso))) && 
            (string.IsNullOrEmpty(forgeSettings.PathToCleanUyaPalIso) || (uyaPalIsoValid && !string.IsNullOrEmpty(forgeSettings.PathToOutputUyaPalIso))) &&
            (string.IsNullOrEmpty(forgeSettings.PathToCleanGcIso) || gcNtscIsoValid)
            ;
        var canImport = HasPacker() && HasWrench() && HasDotNetCore() && hasRequiredIsos;
        var step3 = CreateDisabledElement(scrollregion, !canImport);
        CreateGroupHeader(step3, "Step 3");
        groupBox = CreateGroupBox(step3, width: 600);
        CreateLabel(groupBox, "You are ready to finish setting up Forge. The final step is to import all assets from each of the provided ISOs above, for use in your custom maps. This process will take awhile and will require many disk operations. We highly recommend installing Forge on an SSD to speed up this process.");
        CreateButton(groupBox, "Import Assets", ImportAssetsAndClose);

        CreatePadding(groupBox, 40);
        CreateHelpBox(groupBox, $"If you do not wish to import any assets, you may simply save and exit.\nPlease refer to our <a href=\"{Constants.RepoUrl}\">github repo</a> for how to begin using Forge to make custom maps.", HelpBoxMessageType.Warning);
        CreateButton(groupBox, "Save and Close", () => SaveAndClose());

        // bottom padding
        CreatePadding(scrollregion, 50);
    }

    private ForgeSettings GetOrCreateForgeSettings()
    {
        if (forgeSettings) return forgeSettings;

        // check for the existent of ForgeSettings scriptableobject
        var settings = ForgeSettings.Load();
        if (!settings)
            settings = ScriptableObject.CreateInstance<ForgeSettings>();

        return forgeSettings = settings;
    }

    private void SaveAndClose()
    {
        var assetPath = AssetDatabase.GetAssetPath(forgeSettings);
        if (string.IsNullOrEmpty(assetPath))
        {
            AssetDatabase.CreateAsset(forgeSettings, ForgeSettings.FORGE_SETTINGS_PATH);
        }
        else
        {
            AssetDatabase.SaveAssetIfDirty(forgeSettings);
        }

        this.Close();
    }

    private void ValidateISOs()
    {
        if (!forgeSettings) return;

        dlIsoValid = !string.IsNullOrEmpty(forgeSettings.PathToCleanDeadlockedIso) && ISOHelper.ValidateISO(forgeSettings.PathToCleanDeadlockedIso, 4, GameRegion.NTSC);
        uyaNtscIsoValid = !string.IsNullOrEmpty(forgeSettings.PathToCleanUyaNtscIso) && ISOHelper.ValidateISO(forgeSettings.PathToCleanUyaNtscIso, 3, GameRegion.NTSC);
        uyaPalIsoValid = !string.IsNullOrEmpty(forgeSettings.PathToCleanUyaPalIso) && ISOHelper.ValidateISO(forgeSettings.PathToCleanUyaPalIso, 3, GameRegion.PAL);
        gcNtscIsoValid = !string.IsNullOrEmpty(forgeSettings.PathToCleanGcIso) && ISOHelper.ValidateISO(forgeSettings.PathToCleanGcIso, 2, GameRegion.NTSC);
    }

    private void OnDeadlockedCleanISOPathUpdated(ForgeSettings forgeSettings, string newValue)
    {
        dlIsoValid = ISOHelper.ValidateISO(newValue, 4, GameRegion.NTSC);
        forgeSettings.PathToCleanDeadlockedIso = newValue;

        // print validation error
        if (!dlIsoValid && !string.IsNullOrEmpty(newValue))
            Debug.LogError($"ISO is not a valid Deadlocked NTSC ISO: \"{newValue}\"");

        // create default output iso path
        if (!string.IsNullOrEmpty(newValue) && string.IsNullOrEmpty(forgeSettings.PathToOutputDeadlockedIso) && dlIsoValid)
        {
            forgeSettings.PathToOutputDeadlockedIso = Path.Combine(Path.GetDirectoryName(newValue), Path.GetFileNameWithoutExtension(newValue) + ".modded.iso");
        }

        // update gui
        CreateGUI();
    }

    private void OnUyaNtscCleanISOPathUpdated(ForgeSettings forgeSettings, string newValue)
    {
        uyaNtscIsoValid = ISOHelper.ValidateISO(newValue, 3, GameRegion.NTSC);
        forgeSettings.PathToCleanUyaNtscIso = newValue;

        // print validation error
        if (!uyaNtscIsoValid && !string.IsNullOrEmpty(newValue))
            Debug.LogError($"ISO is not a valid UYA NTSC ISO: \"{newValue}\"");

        // create default output iso path
        if (!string.IsNullOrEmpty(newValue) && string.IsNullOrEmpty(forgeSettings.PathToOutputUyaNtscIso) && uyaNtscIsoValid)
        {
            forgeSettings.PathToOutputUyaNtscIso = Path.Combine(Path.GetDirectoryName(newValue), Path.GetFileNameWithoutExtension(newValue) + ".modded.iso");
        }

        // update gui
        CreateGUI();
    }

    private void OnUyaPalCleanISOPathUpdated(ForgeSettings forgeSettings, string newValue)
    {
        uyaPalIsoValid = ISOHelper.ValidateISO(newValue, 3, GameRegion.PAL);
        forgeSettings.PathToCleanUyaPalIso = newValue;

        // print validation error
        if (!uyaPalIsoValid && !string.IsNullOrEmpty(newValue))
            Debug.LogError($"ISO is not a valid R&C3 PAL ISO: \"{newValue}\"");

        // create default output iso path
        if (!string.IsNullOrEmpty(newValue) && string.IsNullOrEmpty(forgeSettings.PathToOutputUyaPalIso) && uyaPalIsoValid)
        {
            forgeSettings.PathToOutputUyaPalIso = Path.Combine(Path.GetDirectoryName(newValue), Path.GetFileNameWithoutExtension(newValue) + ".modded.iso");
        }

        // update gui
        CreateGUI();
    }

    private void OnGcNtscCleanISOPathUpdated(ForgeSettings forgeSettings, string newValue)
    {
        gcNtscIsoValid = ISOHelper.ValidateISO(newValue, 2, GameRegion.NTSC);
        forgeSettings.PathToCleanGcIso = newValue;

        // print validation error
        if (!gcNtscIsoValid && !string.IsNullOrEmpty(newValue))
            Debug.LogError($"ISO is not a valid Going Commando NTSC ISO: \"{newValue}\"");

        // update gui
        CreateGUI();
    }

    private void ImportAssetsAndClose()
    {
        var isos = new[] { forgeSettings.PathToCleanDeadlockedIso, forgeSettings.GetPathToCleanUyaIso(), forgeSettings.PathToCleanGcIso };
        var racVersions = new[] { 4, 3, 2 };

        // save forgeSettings
        var assetPath = AssetDatabase.GetAssetPath(forgeSettings);
        if (string.IsNullOrEmpty(assetPath))
        {
            AssetDatabase.CreateAsset(forgeSettings, ForgeSettings.FORGE_SETTINGS_PATH);
        }
        else
        {
            AssetDatabase.SaveAssetIfDirty(forgeSettings);
        }

        // import isos
        for (int i = 0; i < isos.Length; ++i)
        {
            var iso = isos[i];
            var racVersion = racVersions[i];
            if (string.IsNullOrEmpty(iso)) continue;

            ISOImporterWindow.ImportISO(iso, racVersion, true, true, true, true);
        }

        // complete
        EditorUtility.DisplayDialog("Startup Complete", $"Please refer to our github for documentation (link in Forge/About page).", "Okay");
        this.Close();
    }

    #region Install Validation Helpers

    private bool HasBlender() => !string.IsNullOrEmpty(BlenderHelper.GetBlenderPath());

    private bool HasPacker() => PackerHelper.IsInstalled();

    private bool HasWrench() => WrenchHelper.IsInstalled();

    private bool HasDotNetCore() => PackerHelper.CanRun();

    #endregion

    #region UI Helpers

    public VisualElement CreateDisabledElement(VisualElement root, bool disabled)
    {
        var element = new VisualElement();
        element.SetEnabled(!disabled);
        root.Add(element);
        return element;
    }

    private void CreateGroupHeader(VisualElement root, string text)
    {
        var header = new Label(text);
        header.style.unityTextAlign = TextAnchor.MiddleCenter;
        header.style.unityFontStyleAndWeight = FontStyle.Bold;
        header.style.fontSize = 16;
        header.style.marginTop = 20;
        root.Add(header);
    }

    private void CreatePadding(VisualElement root, int padding)
    {
        var elem = new VisualElement();
        elem.style.paddingTop = padding;
        root.Add(elem);
    }

    private VisualElement CreateGroupBox(VisualElement root, int width = 500)
    {
        var box = new VisualElement();
        box.style.alignSelf = Align.Center;
        box.style.width = width;
        root.Add(box);
        return box;
    }

    private void CreateHelpBox(VisualElement root, string text, HelpBoxMessageType type)
    {
        var helpbox = new HelpBox(text, type);
        root.Add(helpbox);
    }

    private void CreateButton(VisualElement root, string text, Action onClick)
    {
        var button = new Button(onClick);
        button.text = text;
        root.Add(button);
    }

    private void CreateLabel(VisualElement root, string text)
    {
        var label = new Label(text);
        label.style.unityTextAlign = TextAnchor.UpperLeft;
        label.style.color = unhoverColor;
        label.style.whiteSpace = WhiteSpace.Normal;
        root.Add(label);
    }

    private void CreateLink(VisualElement root, string text, string link)
    {
        var label = new Label(text);
        label.style.unityTextAlign = TextAnchor.MiddleCenter;
        label.style.color = unhoverColor;

        if (!string.IsNullOrEmpty(link))
        {
            label.RegisterCallback<ClickEvent>(e => Application.OpenURL(link));
            label.RegisterCallback<MouseEnterEvent>(e => { label.style.color = hoverColor; });
            label.RegisterCallback<MouseLeaveEvent>(e => { label.style.color = unhoverColor; });
        }

        root.Add(label);
    }

    private void CreateImageLink(VisualElement root, Texture2D tex, string link, string tooltip)
    {
        var image = new UnityEngine.UIElements.Image();
        image.image = tex;
        image.tooltip = tooltip;
        image.scaleMode = ScaleMode.ScaleToFit;
        image.style.width = image.style.height = 30;
        image.style.marginLeft = image.style.marginRight = 10;
        image.tintColor = unhoverColor;

        if (!string.IsNullOrEmpty(link))
        {
            image.RegisterCallback<ClickEvent>(e => Application.OpenURL(link));
            image.RegisterCallback<MouseEnterEvent>(e => { image.tintColor = hoverColor; });
            image.RegisterCallback<MouseLeaveEvent>(e => { image.tintColor = unhoverColor; });
        }

        root.Add(image);
    }

    private VisualElement CreateValidationRow(VisualElement root, bool validated)
    {
        var row = new VisualElement();
        row.style.flexDirection = FlexDirection.RowReverse;

        var checkIcon = new Image();
        checkIcon.image = validated ? IconCheck : IconCross;
        checkIcon.tintColor = validated ? Color.green : Color.red;
        checkIcon.style.height = 30;
        checkIcon.style.width = 30;
        checkIcon.style.minWidth = 30;
        checkIcon.style.minHeight = 30;
        checkIcon.style.marginLeft = 10;
        checkIcon.style.alignSelf = Align.Center;
        checkIcon.scaleMode = ScaleMode.ScaleToFit;
        row.Add(checkIcon);

        var content = new VisualElement();
        content.style.flexDirection = FlexDirection.Row;
        content.style.flexGrow = 1;
        content.style.justifyContent = Justify.SpaceBetween;
        content.style.alignItems = Align.Auto;
        content.style.alignSelf = Align.Center;
        row.Add(content);

        root.Add(row);
        return content;
    }

    private void CreateOpenFileBrowser(VisualElement root, string title, string path, Action<string> pathChanged, params string[] extensions)
    {
        var row = new VisualElement();
        row.style.flexDirection = FlexDirection.Row;

        // label
        var label = new Label(title);
        label.style.width = new Length(50, LengthUnit.Percent);
        row.Add(label);

        // path textbox
        var textbox = new TextField();
        textbox.SetValueWithoutNotify(path);
        textbox.RegisterValueChangedCallback((e) => pathChanged?.Invoke(e.newValue));
        textbox.isDelayed = true;
        textbox.style.width = new Length(40, LengthUnit.Percent);
        row.Add(textbox);

        // browse button
        var browse = new Button();
        browse.text = "Browse";
        browse.style.width = new Length(10, LengthUnit.Percent);
        browse.clicked += () =>
        {
            var newPath = EditorUtility.OpenFilePanelWithFilters(title, textbox.value, extensions);
            if (!string.IsNullOrEmpty(newPath)) textbox.value = newPath;
        };
        row.Add(browse);

        root.Add(row);
    }

    private void CreateSaveFileBrowser(VisualElement root, string title, string path, Action<string> pathChanged, string extension)
    {
        var row = new VisualElement();
        row.style.flexDirection = FlexDirection.Row;

        // label
        var label = new Label(title);
        label.style.width = new Length(50, LengthUnit.Percent);
        row.Add(label);

        // path textbox
        var textbox = new TextField();
        textbox.SetValueWithoutNotify(path);
        textbox.RegisterValueChangedCallback((e) => pathChanged?.Invoke(e.newValue));
        textbox.isDelayed = true;
        textbox.style.width = new Length(40, LengthUnit.Percent);
        row.Add(textbox);

        // browse button
        var browse = new Button();
        browse.text = "Browse";
        browse.style.width = new Length(10, LengthUnit.Percent);
        browse.clicked += () =>
        {
            var dir = string.IsNullOrEmpty(textbox.value) ? "" : Path.GetDirectoryName(textbox.value);
            var filename = string.IsNullOrEmpty(textbox.value) ? "" : Path.GetFileName(textbox.value);

            var newPath = EditorUtility.SaveFilePanel(title, dir, filename, extension);
            if (!string.IsNullOrEmpty(newPath)) textbox.value = newPath;
        };
        row.Add(browse);

        root.Add(row);
    }

    private void CreateFolderBrowser(VisualElement root, string title, string path, Action<string> pathChanged)
    {
        var row = new VisualElement();
        row.style.flexDirection = FlexDirection.Row;

        // label
        var label = new Label(title);
        label.style.width = new Length(50, LengthUnit.Percent);
        row.Add(label);

        // path textbox
        var textbox = new TextField();
        textbox.SetValueWithoutNotify(path);
        textbox.RegisterValueChangedCallback((e) => pathChanged?.Invoke(e.newValue));
        textbox.isDelayed = true;
        textbox.style.width = new Length(40, LengthUnit.Percent);
        row.Add(textbox);

        // browse button
        var browse = new Button();
        browse.text = "Browse";
        browse.style.width = new Length(10, LengthUnit.Percent);
        browse.clicked += () =>
        {
            var newPath = EditorUtility.OpenFolderPanel(title, textbox.value, "");
            if (!string.IsNullOrEmpty(newPath)) textbox.value = newPath;
        };
        row.Add(browse);

        root.Add(row);
    }

    private void CreateDependencyBox(VisualElement root, Texture2D image, bool installed, string text)
    {
        var row = new VisualElement();
        row.style.flexDirection = FlexDirection.Row;
        row.style.marginBottom = 5;
        root.Add(row);

        var box = new VisualElement();
        box.style.alignSelf = Align.Center;
        box.style.paddingBottom = box.style.paddingRight = box.style.paddingLeft = box.style.paddingTop = 5;
        box.style.flexDirection = FlexDirection.Row;
        box.style.width = 600;
        box.style.borderRightWidth = box.style.borderBottomWidth = box.style.borderLeftWidth = box.style.borderTopWidth = 1;
        box.style.borderTopColor = box.style.borderBottomColor = box.style.borderLeftColor = box.style.borderRightColor = Color.black;
        box.style.borderBottomLeftRadius = box.style.borderBottomRightRadius = box.style.borderTopLeftRadius = box.style.borderTopRightRadius = 5;
        box.style.backgroundColor = EditorGUIUtility.isProSkin ? (Color.gray*0.25f) : Color.clear;
        row.Add(box);

        var icon = new Image();
        icon.image = image;
        icon.style.height = 30;
        icon.style.width = 30;
        icon.style.minWidth = 30;
        icon.style.minHeight = 30;
        icon.style.marginRight = 10;
        icon.scaleMode = ScaleMode.ScaleToFit;
        box.Add(icon);

        var label = new Label(text);
        label.style.unityTextAlign = TextAnchor.MiddleLeft;
        label.style.whiteSpace = WhiteSpace.Normal;
        box.Add(label);

        var checkIcon = new Image();
        checkIcon.image = installed ? IconCheck : IconCross;
        checkIcon.tintColor = installed ? Color.green : Color.red;
        checkIcon.style.height = 30;
        checkIcon.style.width = 30;
        checkIcon.style.minWidth = 30;
        checkIcon.style.minHeight = 30;
        checkIcon.style.marginLeft = 10;
        checkIcon.style.alignSelf = Align.Center;
        checkIcon.scaleMode = ScaleMode.ScaleToFit;
        row.Add(checkIcon);
    }

    #endregion

}

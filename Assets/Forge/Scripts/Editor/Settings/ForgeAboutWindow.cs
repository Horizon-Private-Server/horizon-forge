using System.Collections;
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;
using UnityEngine.UIElements;

public class ForgeAboutWindow : EditorWindow
{
    static readonly Color unhoverColor = new Color(0.8f, 0.8f, 0.8f, 1f);
    static readonly Color hoverColor = Color.white;
    static readonly Dictionary<string, string> contributors = new()
    {
        // keep it alphabetical
        { "Badger41", "https://github.com/badger41" },
        { "Dnawrkshp", "https://github.com/dnawrkshp" }
    };
    static readonly Dictionary<string, string> specialThanks = new()
    {
        { "chaoticgd", "https://github.com/chaoticgd/wrench" }
    };

    // populated by inspector
    public Texture2D LogoDiscord;
    public Texture2D LogoGithub;
    public Texture2D LogoWiki;
    public Texture2D LogoYouTube;
    public Texture2D LogoHorizon;

    [MenuItem("Forge/About", priority = 10010)]
    public static void CreateNewWindow()
    {
        var wnd = GetWindow<ForgeAboutWindow>();
        wnd.titleContent = new GUIContent("Forge About");
        wnd.minSize = new Vector2(700, 500);
    }

    private void CreateGUI()
    {

        VisualElement root = rootVisualElement;
        root.Clear();

        // title
        var pageHeading = new Label($"Horizon Forge {Constants.ForgeVersion}");
        pageHeading.style.unityTextAlign = TextAnchor.MiddleCenter;
        pageHeading.style.unityFontStyleAndWeight = FontStyle.Bold;
        pageHeading.style.fontSize = 24;
        root.Add(pageHeading);

        // subheading
        var pageSubheading = new Label($"A Map Editor for Ratchet and Clank UYA & Deadlocked Multiplayer");
        pageSubheading.style.unityTextAlign = TextAnchor.MiddleCenter;
        pageSubheading.style.fontSize = 18;
        root.Add(pageSubheading);

        // contributors
        CreateGroupHeader(root, "Contributors");
        foreach (var contributor in contributors)
        {
            CreateLink(root, contributor.Key, contributor.Value);
        }

        // special thanks
        CreateGroupHeader(root, "Special Thanks To");
        foreach (var specialThank in specialThanks)
        {
            CreateLink(root, specialThank.Key, specialThank.Value);
        }

        // links
        CreateGroupHeader(root, "Links");
        var row = new VisualElement();
        row.style.marginTop = 5;
        row.style.flexDirection = FlexDirection.Row;
        row.style.justifyContent = Justify.Center;
        CreateImageLink(row, LogoDiscord, Constants.DiscordInviteUrl, "Join our Discord");
        CreateImageLink(row, LogoWiki, Constants.WikiUrl, "Wiki");
        CreateImageLink(row, LogoGithub, Constants.RepoUrl, "Contribute");
        CreateImageLink(row, LogoYouTube, "https://www.youtube.com/@HorizonPrivateServer", "Check out our YouTube");

        root.Add(row);
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
}

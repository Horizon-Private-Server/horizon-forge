using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

public static class IOHelper
{
    // adapted from https://stackoverflow.com/a/3822913
    public static void CopyDirectory(string sourcePath, string targetPath, Func<string, bool> filter = null)
    {
        if (!Directory.Exists(targetPath)) Directory.CreateDirectory(targetPath);

        //Now Create all of the directories
        foreach (string dirPath in Directory.GetDirectories(sourcePath, "*", SearchOption.AllDirectories))
        {
            if (filter != null && !filter(dirPath)) continue;
            Directory.CreateDirectory(dirPath.Replace(sourcePath, targetPath));
        }

        //Copy all the files & Replaces any files with the same name
        foreach (string newPath in Directory.GetFiles(sourcePath, "*.*", SearchOption.AllDirectories))
        {
            if (filter != null && !filter(newPath)) continue;
            File.Copy(newPath, newPath.Replace(sourcePath, targetPath), true);
        }
    }
}

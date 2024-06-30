using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnityEngine;

public static class CollisionHelper
{
    public static Color GetColor(int colId)
    {
        return new Color(
            ((colId & 0x03) << 6) / 255f,
            ((colId & 0x0C) << 4) / 255f,
            ((colId & 0xF0) << 0) / 255f,
            1
            );
    }
}

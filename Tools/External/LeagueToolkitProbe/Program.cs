using System.Reflection;
using System.Globalization;
using LeagueToolkit.Core.Meta;

static IEnumerable<(uint nameHash, string value)> FindStrings(BinTreeProperty property)
{
    var type = property.GetType();
    var valueProperty = type.GetProperty("Value");
    if (valueProperty?.GetValue(property) is string stringValue)
    {
        yield return (property.NameHash, stringValue);
    }

    var elementsProperty = type.GetProperty("Elements");
    if (elementsProperty?.GetValue(property) is System.Collections.IEnumerable elements)
    {
        foreach (var element in elements.OfType<BinTreeProperty>())
        {
            foreach (var found in FindStrings(element))
            {
                yield return found;
            }
        }
    }

    var propertiesProperty = type.GetProperty("Properties");
    if (propertiesProperty?.GetValue(property) is System.Collections.IDictionary properties)
    {
        foreach (var value in properties.Values.OfType<BinTreeProperty>())
        {
            foreach (var found in FindStrings(value))
            {
                yield return found;
            }
        }
    }

    var valuesProperty = type.GetProperty("Values");
    if (valuesProperty?.GetValue(property) is System.Collections.IEnumerable values)
    {
        foreach (var value in values.OfType<BinTreeProperty>())
        {
            foreach (var found in FindStrings(value))
            {
                yield return found;
            }
        }
    }
}

static IEnumerable<(uint nameHash, System.Numerics.Vector3 value)> FindVectors(BinTreeProperty property)
{
    var type = property.GetType();
    var valueProperty = type.GetProperty("Value");
    if (valueProperty?.GetValue(property) is System.Numerics.Vector3 vectorValue)
    {
        yield return (property.NameHash, vectorValue);
    }

    var elementsProperty = type.GetProperty("Elements");
    if (elementsProperty?.GetValue(property) is System.Collections.IEnumerable elements)
    {
        foreach (var element in elements.OfType<BinTreeProperty>())
        {
            foreach (var found in FindVectors(element))
            {
                yield return found;
            }
        }
    }

    var propertiesProperty = type.GetProperty("Properties");
    if (propertiesProperty?.GetValue(property) is System.Collections.IDictionary properties)
    {
        foreach (var value in properties.Values.OfType<BinTreeProperty>())
        {
            foreach (var found in FindVectors(value))
            {
                yield return found;
            }
        }
    }

    var valuesProperty = type.GetProperty("Values");
    if (valuesProperty?.GetValue(property) is System.Collections.IEnumerable values)
    {
        foreach (var value in values.OfType<BinTreeProperty>())
        {
            foreach (var found in FindVectors(value))
            {
                yield return found;
            }
        }
    }
}

static IEnumerable<(uint nameHash, System.Numerics.Matrix4x4 value)> FindMatrices(BinTreeProperty property)
{
    var type = property.GetType();
    var valueProperty = type.GetProperty("Value");
    if (valueProperty?.GetValue(property) is System.Numerics.Matrix4x4 matrixValue)
    {
        yield return (property.NameHash, matrixValue);
    }

    var elementsProperty = type.GetProperty("Elements");
    if (elementsProperty?.GetValue(property) is System.Collections.IEnumerable elements)
    {
        foreach (var element in elements.OfType<BinTreeProperty>())
        {
            foreach (var found in FindMatrices(element))
            {
                yield return found;
            }
        }
    }

    var propertiesProperty = type.GetProperty("Properties");
    if (propertiesProperty?.GetValue(property) is System.Collections.IDictionary properties)
    {
        foreach (var value in properties.Values.OfType<BinTreeProperty>())
        {
            foreach (var found in FindMatrices(value))
            {
                yield return found;
            }
        }
    }

    var valuesProperty = type.GetProperty("Values");
    if (valuesProperty?.GetValue(property) is System.Collections.IEnumerable values)
    {
        foreach (var value in values.OfType<BinTreeProperty>())
        {
            foreach (var found in FindMatrices(value))
            {
                yield return found;
            }
        }
    }
}

static IEnumerable<(string name, System.Numerics.Matrix4x4 m)> FindNamedTransforms(BinTreeProperty property)
{
    var type = property.GetType();

    var propertiesProperty = type.GetProperty("Properties");
    if (propertiesProperty?.GetValue(property) is System.Collections.IDictionary directProps)
    {
        string? name = null;
        System.Numerics.Matrix4x4? matrix = null;
        foreach (var child in directProps.Values.OfType<BinTreeProperty>())
        {
            var childValue = child.GetType().GetProperty("Value")?.GetValue(child);
            if (childValue is string s && name is null)
            {
                name = s;
            }
            if (childValue is System.Numerics.Matrix4x4 m)
            {
                matrix = m;
            }
        }

        if (name is not null && matrix is not null)
        {
            yield return (name, matrix.Value);
        }

        foreach (var child in directProps.Values.OfType<BinTreeProperty>())
        {
            foreach (var found in FindNamedTransforms(child))
            {
                yield return found;
            }
        }
    }

    var elementsProperty = type.GetProperty("Elements");
    if (elementsProperty?.GetValue(property) is System.Collections.IEnumerable elements)
    {
        foreach (var element in elements.OfType<BinTreeProperty>())
        {
            foreach (var found in FindNamedTransforms(element))
            {
                yield return found;
            }
        }
    }

    var valuesProperty = type.GetProperty("Values");
    if (valuesProperty?.GetValue(property) is System.Collections.IEnumerable values)
    {
        foreach (var value in values.OfType<BinTreeProperty>())
        {
            foreach (var found in FindNamedTransforms(value))
            {
                yield return found;
            }
        }
    }
}

if (args.Length >= 2 && args[0].Equals("levelprops", StringComparison.OrdinalIgnoreCase))
{
    using var lpStream = File.OpenRead(args[1]);
    var lpTree = new BinTree(lpStream);
    var lpFilters = args.Length >= 3
        ? args[2].Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries)
        : Array.Empty<string>();

    foreach (var pair in lpTree.Objects.OrderBy(pair => pair.Key))
    {
        foreach (var (name, m) in pair.Value.Properties.Values.SelectMany(FindNamedTransforms))
        {
            if (lpFilters.Length > 0 &&
                !lpFilters.Any(filter => name.Contains(filter, StringComparison.OrdinalIgnoreCase)))
            {
                continue;
            }

            Console.WriteLine(string.Create(CultureInfo.InvariantCulture,
                $"{name},{m.M41:F3},{m.M42:F3},{m.M43:F3},{m.M11:F4},{m.M12:F4},{m.M13:F4},{m.M21:F4},{m.M22:F4},{m.M23:F4},{m.M31:F4},{m.M32:F4},{m.M33:F4}"));
        }
    }

    return;
}

if (args.Length >= 3 && args[0].Equals("placements", StringComparison.OrdinalIgnoreCase))
{
    using var placementStream = File.OpenRead(args[1]);
    var placementTree = new BinTree(placementStream);
    var filters = args[2].Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);

    foreach (var pair in placementTree.Objects.OrderBy(pair => pair.Key))
    {
        var strings = pair.Value.Properties.Values.SelectMany(FindStrings).ToList();
        var matches = strings
            .Where(item => filters.Any(filter => item.value.Contains(filter, StringComparison.OrdinalIgnoreCase)))
            .Select(item => item.value)
            .Distinct()
            .ToList();
        if (matches.Count == 0)
        {
            continue;
        }

        Console.WriteLine($"object=0x{pair.Key:X8} class=0x{pair.Value.ClassHash:X8} ref={string.Join(";", matches.Take(4))}");
        foreach (var (nameHash, vec) in pair.Value.Properties.Values.SelectMany(FindVectors))
        {
            Console.WriteLine(string.Create(CultureInfo.InvariantCulture,
                $"  vec3 prop=0x{nameHash:X8} {vec.X:F3},{vec.Y:F3},{vec.Z:F3}"));
        }

        foreach (var (nameHash, m) in pair.Value.Properties.Values.SelectMany(FindMatrices))
        {
            Console.WriteLine(string.Create(CultureInfo.InvariantCulture,
                $"  mat44 prop=0x{nameHash:X8} row4={m.M41:F3},{m.M42:F3},{m.M43:F3} col4={m.M14:F3},{m.M24:F3},{m.M34:F3} scale={m.M11:F3},{m.M22:F3},{m.M33:F3}"));
        }
    }

    return;
}

if (args.Length >= 1 && args[0].Equals("types", StringComparison.OrdinalIgnoreCase))
{
    var filter = args.Length >= 2 ? args[1] : string.Empty;
    foreach (var assembly in AppDomain.CurrentDomain.GetAssemblies()
                 .Concat(new[] { typeof(BinTree).Assembly })
                 .Distinct())
    {
        Type[] types;
        try
        {
            types = assembly.GetTypes();
        }
        catch (ReflectionTypeLoadException ex)
        {
            types = ex.Types.Where(type => type is not null).Cast<Type>().ToArray();
        }

        foreach (var type in types
                     .Where(type => string.IsNullOrWhiteSpace(filter)
                         || type.FullName?.Contains(filter, StringComparison.OrdinalIgnoreCase) == true)
                     .OrderBy(type => type.FullName))
        {
            Console.WriteLine($"{assembly.GetName().Name}: {type.FullName}");
        }
    }

    return;
}

if (args.Length >= 2 && args[0].Equals("describe", StringComparison.OrdinalIgnoreCase))
{
    var typeName = args[1];
    var type = AppDomain.CurrentDomain.GetAssemblies()
        .Concat(new[] { typeof(BinTree).Assembly })
        .Select(assembly => assembly.GetType(typeName, throwOnError: false, ignoreCase: false))
        .FirstOrDefault(type => type is not null);

    if (type is null)
    {
        Console.Error.WriteLine($"Type not found: {typeName}");
        return;
    }

    Console.WriteLine(type.FullName);
    Console.WriteLine("Constructors:");
    foreach (var ctor in type.GetConstructors(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance))
    {
        Console.WriteLine($"  {ctor}");
    }

    Console.WriteLine("Properties:");
    foreach (var property in type.GetProperties(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static)
                 .OrderBy(property => property.Name))
    {
        Console.WriteLine($"  {property.PropertyType.FullName} {property.Name}");
    }

    Console.WriteLine("Fields:");
    foreach (var field in type.GetFields(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static)
                 .OrderBy(field => field.Name))
    {
        var suffix = string.Empty;
        if (type.IsEnum && field.IsLiteral)
        {
            var value = field.GetRawConstantValue();
            suffix = $" = {Convert.ToInt64(value, CultureInfo.InvariantCulture)}";
        }

        Console.WriteLine($"  {field.FieldType.FullName} {field.Name}{suffix}");
    }

    Console.WriteLine("Methods:");
    foreach (var method in type.GetMethods(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static | BindingFlags.DeclaredOnly)
                 .OrderBy(method => method.Name))
    {
        Console.WriteLine($"  {method}");
    }

    return;
}

if (args.Length == 2 && args[0].Equals("overlay", StringComparison.OrdinalIgnoreCase))
{
    var overlayType = typeof(BinTree).Assembly.GetType("LeagueToolkit.IO.NavigationGridOverlay.NavigationGridOverlay");
    if (overlayType is null)
    {
        Console.Error.WriteLine("NavigationGridOverlay type not found.");
        return;
    }

    var overlay = Activator.CreateInstance(overlayType, args[1]);
    var regions = (System.Collections.IEnumerable?)overlayType.GetProperty("Regions")?.GetValue(overlay);
    if (regions is null)
    {
        Console.Error.WriteLine("No Regions property found.");
        return;
    }

    var totalRegions = 0;
    var totalCells = 0;
    var totalGrass = 0;
    foreach (var region in regions)
    {
        totalRegions++;
        var regionType = region.GetType();
        var x = Convert.ToUInt32(regionType.GetProperty("X")?.GetValue(region));
        var y = Convert.ToUInt32(regionType.GetProperty("Y")?.GetValue(region));
        var width = Convert.ToUInt32(regionType.GetProperty("Width")?.GetValue(region));
        var height = Convert.ToUInt32(regionType.GetProperty("Height")?.GetValue(region));
        var cellFlags = (System.Collections.IEnumerable?)regionType.GetProperty("CellFlags")?.GetValue(region);
        var regionCells = 0;
        var regionGrass = 0;

        if (cellFlags is not null)
        {
            foreach (var row in cellFlags)
            {
                if (row is not System.Collections.IEnumerable flags)
                {
                    continue;
                }

                foreach (var flag in flags)
                {
                    regionCells++;
                    if (flag.ToString()?.Contains("HAS_GRASS", StringComparison.OrdinalIgnoreCase) == true)
                    {
                        regionGrass++;
                    }
                }
            }
        }

        totalCells += regionCells;
        totalGrass += regionGrass;
        Console.WriteLine($"region[{totalRegions - 1}] x={x} y={y} w={width} h={height} cells={regionCells} grass={regionGrass}");
    }

    Console.WriteLine($"regions={totalRegions} cells={totalCells} grass={totalGrass}");
    return;
}

if (args.Length == 2 && args[0].Equals("aimesh", StringComparison.OrdinalIgnoreCase))
{
    var aiMeshType = typeof(BinTree).Assembly.GetType("LeagueToolkit.IO.AiMesh.AiMeshFile");
    if (aiMeshType is null)
    {
        Console.Error.WriteLine("AiMeshFile type not found.");
        return;
    }

    var aiMesh = Activator.CreateInstance(aiMeshType, args[1]);
    var cells = (System.Collections.IEnumerable?)aiMeshType.GetProperty("Cells")?.GetValue(aiMesh);
    if (cells is null)
    {
        Console.Error.WriteLine("No Cells property found.");
        return;
    }

    var count = 0;
    var min = new System.Numerics.Vector3(float.MaxValue, float.MaxValue, float.MaxValue);
    var max = new System.Numerics.Vector3(float.MinValue, float.MinValue, float.MinValue);
    foreach (var cell in cells)
    {
        count++;
        var vertices = (System.Numerics.Vector3[]?)cell.GetType().GetProperty("Vertices")?.GetValue(cell);
        if (vertices is null)
        {
            continue;
        }

        foreach (var vertex in vertices)
        {
            min = System.Numerics.Vector3.Min(min, vertex);
            max = System.Numerics.Vector3.Max(max, vertex);
        }
    }

    Console.WriteLine($"cells={count} boundsMin={min.X:F3},{min.Y:F3},{min.Z:F3} boundsMax={max.X:F3},{max.Y:F3},{max.Z:F3}");
    return;
}

if (args.Length == 2 && args[0].Equals("ngrid-summary", StringComparison.OrdinalIgnoreCase))
{
    using var ngridStream = File.OpenRead(args[1]);
    using var ngridReader = new BinaryReader(ngridStream);

    var major = ngridReader.ReadByte();
    var minor = ngridReader.ReadInt16();
    var minX = ngridReader.ReadSingle();
    var minY = ngridReader.ReadSingle();
    var minZ = ngridReader.ReadSingle();
    var maxX = ngridReader.ReadSingle();
    var maxY = ngridReader.ReadSingle();
    var maxZ = ngridReader.ReadSingle();
    var cellSize = ngridReader.ReadSingle();
    var xCount = ngridReader.ReadUInt32();
    var zCount = ngridReader.ReadUInt32();
    var cellCount = checked((int)(xCount * zCount));

    const short brushFlag = 0x1;
    const short wallFlag = 0x2;
    const short alwaysVisibleFlag = 0x100;

    var brushCount = 0;
    var wallCount = 0;
    var alwaysVisibleCount = 0;
    var minBrushX = int.MaxValue;
    var minBrushZ = int.MaxValue;
    var maxBrushX = int.MinValue;
    var maxBrushZ = int.MinValue;
    var firstBrushes = new List<(int x, int z, short flags, float height)>();
    var flagCounts = new SortedDictionary<int, int>();
    var bitCounts = new int[16];

    for (var i = 0; i < cellCount; ++i)
    {
        var height = ngridReader.ReadSingle();
        _ = ngridReader.ReadUInt16();
        _ = ngridReader.ReadSingle();
        _ = ngridReader.ReadUInt16();
        _ = ngridReader.ReadSingle();
        _ = ngridReader.ReadUInt32();
        var cellX = ngridReader.ReadInt16();
        var cellZ = ngridReader.ReadInt16();
        _ = ngridReader.ReadSingle();
        _ = ngridReader.ReadSingle();
        _ = ngridReader.ReadUInt32();
        _ = ngridReader.ReadSingle();
        _ = ngridReader.ReadInt16();
        var flags = ngridReader.ReadInt16();
        _ = ngridReader.ReadInt16();
        _ = ngridReader.ReadInt16();

        var flagKey = flags & 0xFFFF;
        flagCounts.TryGetValue(flagKey, out var existingFlagCount);
        flagCounts[flagKey] = existingFlagCount + 1;
        for (var bit = 0; bit < bitCounts.Length; ++bit)
        {
            if ((flagKey & (1 << bit)) != 0)
            {
                bitCounts[bit]++;
            }
        }

        if ((flags & wallFlag) != 0)
        {
            wallCount++;
        }

        if ((flags & alwaysVisibleFlag) != 0)
        {
            alwaysVisibleCount++;
        }

        if ((flags & brushFlag) == 0)
        {
            continue;
        }

        brushCount++;
        minBrushX = Math.Min(minBrushX, cellX);
        minBrushZ = Math.Min(minBrushZ, cellZ);
        maxBrushX = Math.Max(maxBrushX, cellX);
        maxBrushZ = Math.Max(maxBrushZ, cellZ);
        if (firstBrushes.Count < 16)
        {
            firstBrushes.Add((cellX, cellZ, flags, height));
        }
    }

    Console.WriteLine(
        string.Create(
            CultureInfo.InvariantCulture,
            $"version={major}.{minor} min={minX:F3},{minY:F3},{minZ:F3} max={maxX:F3},{maxY:F3},{maxZ:F3} cellSize={cellSize:F3} xCount={xCount} zCount={zCount} cells={cellCount}"));
    Console.WriteLine($"brush={brushCount} wall={wallCount} alwaysVisible={alwaysVisibleCount} streamOffsetAfterCells={ngridStream.Position}");
    Console.WriteLine("flagHistogramTop:");
    foreach (var pair in flagCounts.OrderByDescending(pair => pair.Value).Take(32))
    {
        Console.WriteLine($"  flags=0x{pair.Key:X4} count={pair.Value}");
    }

    Console.WriteLine("bitCounts:");
    for (var bit = 0; bit < bitCounts.Length; ++bit)
    {
        if (bitCounts[bit] != 0)
        {
            Console.WriteLine($"  bit{bit}=0x{(1 << bit):X4} count={bitCounts[bit]}");
        }
    }

    if (brushCount > 0)
    {
        Console.WriteLine($"brushCellBounds=x[{minBrushX},{maxBrushX}] z[{minBrushZ},{maxBrushZ}]");
        foreach (var brush in firstBrushes)
        {
            Console.WriteLine($"sample x={brush.x} z={brush.z} flags=0x{brush.flags:X4} height={brush.height.ToString("F3", CultureInfo.InvariantCulture)}");
        }
    }

    return;
}

if (args.Length == 3 && args[0].Equals("ngrid-mask", StringComparison.OrdinalIgnoreCase))
{
    using var ngridStream = File.OpenRead(args[1]);
    using var ngridReader = new BinaryReader(ngridStream);

    _ = ngridReader.ReadByte();
    _ = ngridReader.ReadInt16();
    _ = ngridReader.ReadSingle();
    _ = ngridReader.ReadSingle();
    _ = ngridReader.ReadSingle();
    _ = ngridReader.ReadSingle();
    _ = ngridReader.ReadSingle();
    _ = ngridReader.ReadSingle();
    _ = ngridReader.ReadSingle();
    var xCount = checked((int)ngridReader.ReadUInt32());
    var zCount = checked((int)ngridReader.ReadUInt32());
    var flagsGrid = new ushort[xCount, zCount];

    for (var i = 0; i < xCount * zCount; ++i)
    {
        _ = ngridReader.ReadSingle();
        _ = ngridReader.ReadUInt16();
        _ = ngridReader.ReadSingle();
        _ = ngridReader.ReadUInt16();
        _ = ngridReader.ReadSingle();
        _ = ngridReader.ReadUInt32();
        var cellX = ngridReader.ReadInt16();
        var cellZ = ngridReader.ReadInt16();
        _ = ngridReader.ReadSingle();
        _ = ngridReader.ReadSingle();
        _ = ngridReader.ReadUInt32();
        _ = ngridReader.ReadSingle();
        _ = ngridReader.ReadInt16();
        var flags = (ushort)ngridReader.ReadInt16();
        _ = ngridReader.ReadInt16();
        _ = ngridReader.ReadInt16();

        if (cellX >= 0 && cellX < xCount && cellZ >= 0 && cellZ < zCount)
        {
            flagsGrid[cellX, cellZ] = flags;
        }
    }

    static (byte r, byte g, byte b) ColorForFlags(ushort flags)
    {
        return flags switch
        {
            0x0000 => (28, 28, 28),
            0x0001 => (72, 170, 82),
            0x0002 => (190, 68, 66),
            0x0003 => (222, 152, 58),
            0x0004 => (68, 108, 210),
            0x0005 => (83, 206, 202),
            0x0006 => (196, 82, 214),
            0x0007 => (232, 232, 232),
            0x0008 => (110, 110, 110),
            0x0009 => (148, 190, 104),
            _ => (255, 255, 0),
        };
    }

    var rowStride = ((xCount * 3 + 3) / 4) * 4;
    var pixelBytes = rowStride * zCount;
    var fileBytes = 14 + 40 + pixelBytes;
    Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(args[2])) ?? ".");
    using var bmpStream = File.Create(args[2]);
    using var bw = new BinaryWriter(bmpStream);
    bw.Write((byte)'B');
    bw.Write((byte)'M');
    bw.Write(fileBytes);
    bw.Write((ushort)0);
    bw.Write((ushort)0);
    bw.Write(14 + 40);
    bw.Write(40);
    bw.Write(xCount);
    bw.Write(zCount);
    bw.Write((ushort)1);
    bw.Write((ushort)24);
    bw.Write(0);
    bw.Write(pixelBytes);
    bw.Write(0);
    bw.Write(0);
    bw.Write(0);
    bw.Write(0);

    var padding = new byte[rowStride - xCount * 3];
    for (var z = 0; z < zCount; ++z)
    {
        for (var x = 0; x < xCount; ++x)
        {
            var (r, g, b) = ColorForFlags(flagsGrid[x, zCount - 1 - z]);
            bw.Write(b);
            bw.Write(g);
            bw.Write(r);
        }

        bw.Write(padding);
    }

    Console.WriteLine($"wrote {args[2]} width={xCount} height={zCount}");
    return;
}

if (args.Length == 5 && args[0].Equals("ngrid-filter-mask", StringComparison.OrdinalIgnoreCase))
{
    using var ngridStream = File.OpenRead(args[1]);
    using var ngridReader = new BinaryReader(ngridStream);

    static ushort ParseUshortFlag(string text)
    {
        return text.StartsWith("0x", StringComparison.OrdinalIgnoreCase)
            ? Convert.ToUInt16(text[2..], 16)
            : Convert.ToUInt16(text, CultureInfo.InvariantCulture);
    }

    var mask = ParseUshortFlag(args[3]);
    var value = ParseUshortFlag(args[4]);

    _ = ngridReader.ReadByte();
    _ = ngridReader.ReadInt16();
    _ = ngridReader.ReadSingle();
    _ = ngridReader.ReadSingle();
    _ = ngridReader.ReadSingle();
    _ = ngridReader.ReadSingle();
    _ = ngridReader.ReadSingle();
    _ = ngridReader.ReadSingle();
    _ = ngridReader.ReadSingle();
    var xCount = checked((int)ngridReader.ReadUInt32());
    var zCount = checked((int)ngridReader.ReadUInt32());
    var matched = new bool[xCount, zCount];
    var matchCount = 0;

    for (var i = 0; i < xCount * zCount; ++i)
    {
        _ = ngridReader.ReadSingle();
        _ = ngridReader.ReadUInt16();
        _ = ngridReader.ReadSingle();
        _ = ngridReader.ReadUInt16();
        _ = ngridReader.ReadSingle();
        _ = ngridReader.ReadUInt32();
        var cellX = ngridReader.ReadInt16();
        var cellZ = ngridReader.ReadInt16();
        _ = ngridReader.ReadSingle();
        _ = ngridReader.ReadSingle();
        _ = ngridReader.ReadUInt32();
        _ = ngridReader.ReadSingle();
        _ = ngridReader.ReadInt16();
        var flags = (ushort)ngridReader.ReadInt16();
        _ = ngridReader.ReadInt16();
        _ = ngridReader.ReadInt16();

        if (cellX < 0 || cellX >= xCount || cellZ < 0 || cellZ >= zCount)
        {
            continue;
        }

        if ((flags & mask) == value)
        {
            matched[cellX, cellZ] = true;
            matchCount++;
        }
    }

    var rowStride = ((xCount * 3 + 3) / 4) * 4;
    var pixelBytes = rowStride * zCount;
    var fileBytes = 14 + 40 + pixelBytes;
    Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(args[2])) ?? ".");
    using var bmpStream = File.Create(args[2]);
    using var bw = new BinaryWriter(bmpStream);
    bw.Write((byte)'B');
    bw.Write((byte)'M');
    bw.Write(fileBytes);
    bw.Write((ushort)0);
    bw.Write((ushort)0);
    bw.Write(14 + 40);
    bw.Write(40);
    bw.Write(xCount);
    bw.Write(zCount);
    bw.Write((ushort)1);
    bw.Write((ushort)24);
    bw.Write(0);
    bw.Write(pixelBytes);
    bw.Write(0);
    bw.Write(0);
    bw.Write(0);
    bw.Write(0);

    var padding = new byte[rowStride - xCount * 3];
    for (var z = 0; z < zCount; ++z)
    {
        for (var x = 0; x < xCount; ++x)
        {
            var on = matched[x, zCount - 1 - z];
            bw.Write(on ? (byte)230 : (byte)16);
            bw.Write(on ? (byte)230 : (byte)16);
            bw.Write(on ? (byte)230 : (byte)16);
        }

        bw.Write(padding);
    }

    Console.WriteLine($"wrote {args[2]} mask=0x{mask:X4} value=0x{value:X4} matches={matchCount}");
    return;
}

if (args.Length != 1)
{
    Console.Error.WriteLine("Usage: LeagueToolkitProbe <file.bin>");
    Console.Error.WriteLine("       LeagueToolkitProbe types [filter]");
    Console.Error.WriteLine("       LeagueToolkitProbe describe <fully.qualified.TypeName>");
    Console.Error.WriteLine("       LeagueToolkitProbe overlay <file.rg_overlay>");
    Console.Error.WriteLine("       LeagueToolkitProbe aimesh <file.aimesh_ngrid>");
    Console.Error.WriteLine("       LeagueToolkitProbe ngrid-summary <file.aimesh_ngrid>");
    Console.Error.WriteLine("       LeagueToolkitProbe ngrid-mask <file.aimesh_ngrid> <output.bmp>");
    Console.Error.WriteLine("       LeagueToolkitProbe ngrid-filter-mask <file.aimesh_ngrid> <output.bmp> <mask> <value>");
    return;
}

using var stream = File.OpenRead(args[0]);
var tree = new BinTree(stream);
Console.WriteLine($"objects={tree.Objects.Count} dependencies={tree.Dependencies.Count} overrides={tree.DataOverrides.Count} isOverride={tree.IsOverride}");

var needles = new[]
{
    "brush", "grass", "foliage", "firefly", "bird", "duck", "critter",
    "mapgeometry", "navgrid", "particles", "objectcfg", "ambient"
};

foreach (var pair in tree.Objects.OrderBy(pair => pair.Key))
{
    var strings = pair.Value.Properties.Values.SelectMany(FindStrings).ToList();
    var hits = strings
        .Where(item => needles.Any(needle => item.value.Contains(needle, StringComparison.OrdinalIgnoreCase)))
        .ToList();
    if (hits.Count == 0)
    {
        continue;
    }

    Console.WriteLine($"object=0x{pair.Key:X8} class=0x{pair.Value.ClassHash:X8} strings={strings.Count}");
    foreach (var hit in hits.Take(80))
    {
        Console.WriteLine($"  prop=0x{hit.nameHash:X8} {hit.value}");
    }
}

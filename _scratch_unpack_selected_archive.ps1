$ErrorActionPreference = 'Stop'

Add-Type @'
using System;

public sealed class VTProgress : IProgress<ValueTuple<double, string>>
{
    public void Report(ValueTuple<double, string> value)
    {
        Console.WriteLine("{0:0.000} {1}", value.Item1, value.Item2);
    }
}
'@

$uxmRoot = 'C:\Users\user\Desktop\EldenRingExtract\UXM_Limgrave_m60_42_36_MapPieces'
$uxm = Join-Path $uxmRoot 'UXM Selective Unpack.exe'
$souls = Join-Path $uxmRoot 'soulsformats.dll'
$dictionaryPath = Join-Path $uxmRoot 'res\EldenRingDictionary.txt'
$gameDir = 'C:\Program Files (x86)\Steam\steamapps\common\ELDEN RING\Game'
$archive = 'Data2'

[System.Reflection.Assembly]::LoadFrom($souls) | Out-Null
$uxmAsm = [System.Reflection.Assembly]::LoadFrom($uxm)

$soulsAsm = [AppDomain]::CurrentDomain.GetAssemblies() | Where-Object { $_.GetName().Name -eq 'SoulsFormats' } | Select-Object -First 1
$gameType = $soulsAsm.GetType('SoulsFormats.BHD5+Game')
$game = [System.Enum]::Parse($gameType, 'EldenRing')

$keysType = $uxmAsm.GetType('UXM.ArchiveKeys')
$keys = $keysType.GetField('EldenRingKeys', [System.Reflection.BindingFlags]'Public,Static').GetValue($null)
$key = $keys[$archive]

$dictType = $uxmAsm.GetType('UXM.ArchiveDictionary')
$dictionaryText = [System.IO.File]::ReadAllText($dictionaryPath)
$ctor = $dictType.GetConstructor(@([string], $gameType))
$archiveDictionary = $ctor.Invoke([object[]]@($dictionaryText, $game))

$unpackerType = $uxmAsm.GetType('UXM.ArchiveUnpacker')
$setSkip = $unpackerType.GetMethod('SetSkip', [System.Reflection.BindingFlags]'Public,Static')
$setSkip.Invoke($null, @($true))
$method = $unpackerType.GetMethod('UnpackArchive', [System.Reflection.BindingFlags]'NonPublic,Static')
$progress = (New-Object VTProgress).PSObject.BaseObject
$ct = [System.Threading.CancellationToken]::None

$task = $method.Invoke($null, @($gameDir, $archive, $key, 1, 1, $game, $archiveDictionary, $progress, $ct))
$result = $task.GetAwaiter().GetResult()
"RESULT=$result"

[CmdletBinding()]
param(
    [string]$EngineRoot = 'E:\UE Source\UnrealEngine-5.8',
    [ValidateRange(2, 4)][int]$Clients = 2,
    [ValidateRange(0, 500)][int]$PacketLag = 0,
    [ValidateRange(0, 20)][int]$PacketLoss = 0,
    [int]$Port = 17777,
    [string]$ServerExecutable,
    [string]$ClientExecutable,
    [switch]$Screenshots,
    [switch]$RespawnRegression,
    [switch]$KeyInput
)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$projectFile = Join-Path $projectRoot 'RPGDemo.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
foreach ($requestedExecutable in @($ServerExecutable, $ClientExecutable)) {
    if ($requestedExecutable -and -not (Test-Path -LiteralPath $requestedExecutable -PathType Leaf)) {
        throw "Test executable was not found: $requestedExecutable"
    }
}
$runRoot = Join-Path $projectRoot ("Saved\Automation\Lifecycle\{0}-{1}clients-lag{2}-loss{3}" -f (Get-Date -Format 'yyyyMMdd-HHmmss'), $Clients, $PacketLag, $PacketLoss)
New-Item -ItemType Directory -Path $runRoot -Force | Out-Null
$shared = @('-log', '-unattended', '-NoSplash', '-RPGDemoLifecycleTest', "-RPGDemoLifecycleClients=$Clients", '-RPGDemoCombatTestTimeout=240')
if ($RespawnRegression) { $shared += '-RPGDemoRespawnRegression' }
if ($KeyInput) { $shared += '-RPGDemoLifecycleKeyInput' }
$render = '-ExecCmds="rpgdemo.Combat.Debug 1,sg.ViewDistanceQuality 1,sg.ShadowQuality 0,sg.PostProcessQuality 0,sg.TextureQuality 1,sg.EffectsQuality 0,sg.FoliageQuality 0,r.Streaming.PoolSize 512,r.ScreenPercentage 70,t.MaxFPS 30"'
$network = @("-PktLag=$PacketLag", "-PktLoss=$PacketLoss")
$serverArgs = @('/Game/Maps/MultiplayerLobbyMap?game=/Script/RPGDemo.RPGDemoLobbyGameMode', '-server', '-nullrhi', '-multihome=127.0.0.1', '-ExecCmds="rpgdemo.Combat.Debug 1"', "-port=$Port", ('-abslog="' + (Join-Path $runRoot 'Server.log') + '"')) + $shared + $network
if (-not $ServerExecutable) { $ServerExecutable = $editor; $serverArgs = @('"' + $projectFile + '"') + $serverArgs }
$server = Start-Process -FilePath $ServerExecutable -ArgumentList $serverArgs -PassThru -WindowStyle Hidden
$processes = @($server)
for ($role = 1; $role -le $Clients; ++$role) {
    $clientArgs = @("127.0.0.1:$Port`?Name=LocalClient$role", '-game', '-windowed', '-ResX=960', '-ResY=540', $render, "-RPGDemoCombatRole=$role", ('-abslog="' + (Join-Path $runRoot "Client$role.log") + '"')) + $shared + $network
    $exe = $ClientExecutable
    if (-not $exe) { $exe = $editor; $clientArgs = @('"' + $projectFile + '"') + $clientArgs }
    if ($Screenshots -and $role -eq 1) { $clientArgs += '-RPGDemoLifecycleScreenshots' }
    $processes += Start-Process -FilePath $exe -ArgumentList $clientArgs -PassThru -WindowStyle Hidden
}
[pscustomobject]@{RunDirectory=$runRoot; ProcessIds=@($processes.Id); Clients=$Clients; PacketLag=$PacketLag; PacketLoss=$PacketLoss} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $runRoot 'run.json')
[pscustomobject]@{RunDirectory=$runRoot; ProcessIds=@($processes.Id); Clients=$Clients; PacketLag=$PacketLag; PacketLoss=$PacketLoss}
# Development test processes exit through the engine's bounded timeout; no forced termination.

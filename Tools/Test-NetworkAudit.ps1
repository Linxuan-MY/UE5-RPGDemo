[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
function Invoke-AuditTool([string]$Name, [hashtable]$Arguments) {
    $request = @{toolset_name='AutomationTestToolset.AutomationTestToolset';tool_name=$Name;arguments=$Arguments} | ConvertTo-Json -Depth 20 -Compress
    $response = & (Join-Path $PSScriptRoot 'Invoke-UnrealMcp.ps1') -Tool call_tool -Arguments $request | ConvertFrom-Json
    if ($response.error -or $response.result.isError) { throw ($response | ConvertTo-Json -Depth 30) }
    $value = ($response.result.content[0].text | ConvertFrom-Json).returnValue
    return ($value | ConvertFrom-Json)
}
# The C++ test owns its disposable PIE worlds; no editor assets are edited.
Invoke-AuditTool 'DiscoverTests' @{} | Out-Null
$result = Invoke-AuditTool 'RunTests' @{testNames=@('RPGDemo.Network.SpawnAbilityPolicies','RPGDemo.Network.ListenServerContracts','RPGDemo.Network.BlueprintEnemyDeath')}
$reportDirectory = Join-Path (Split-Path -Parent $PSScriptRoot) 'Saved/Automation/NetworkAudit'
New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null
$result | ConvertTo-Json -Depth 30 | Set-Content -LiteralPath (Join-Path $reportDirectory 'results.json')
$result | ConvertTo-Json -Depth 30
if ($result.failed -ne 0 -or $result.passed -ne 3) { exit 1 }

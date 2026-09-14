param([string]$Tool, [string]$Arguments = '{}')
$ErrorActionPreference = 'Stop'
$uri = 'http://127.0.0.1:8000/mcp'
$init = Invoke-WebRequest -Uri $uri -Method Post -ContentType 'application/json' -Body '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"codex-network-audit","version":"1.0"}}}'
$headers = @{'Mcp-Session-Id' = [string]$init.Headers['Mcp-Session-Id'][0]}
$body = @{jsonrpc='2.0'; id=2; method='tools/call'; params=@{name=$Tool; arguments=($Arguments | ConvertFrom-Json)}} | ConvertTo-Json -Depth 40 -Compress
Invoke-RestMethod -Uri $uri -Method Post -ContentType 'application/json' -Headers $headers -Body $body -TimeoutSec 60 | ConvertTo-Json -Depth 50

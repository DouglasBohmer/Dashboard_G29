$ErrorActionPreference = 'Stop'
$raizProjeto = Split-Path -Parent $PSScriptRoot
$diretorioSaida = Join-Path $raizProjeto '.pio/testes'
New-Item -ItemType Directory -Path $diretorioSaida -Force | Out-Null
$diretorioInclude = Join-Path $raizProjeto 'include'
$arquivoTeste = Join-Path $raizProjeto 'test/teste_demonstracao.cpp'
$executavel = Join-Path $diretorioSaida 'teste_demonstracao.exe'
$compiladorGnu = Get-Command 'g++.exe' -ErrorAction SilentlyContinue
$compiladorZig = Get-Command 'zig.exe' -ErrorAction SilentlyContinue
$zigLocal = Join-Path $diretorioSaida 'ferramentas/ziglang/zig.exe'
if ($compiladorGnu) {
    & $compiladorGnu.Source '-std=c++17' '-Wall' '-Wextra' '-Werror' '-I' $diretorioInclude $arquivoTeste '-o' $executavel
} elseif ($compiladorZig -or (Test-Path -LiteralPath $zigLocal)) {
    $zig = if ($compiladorZig) { $compiladorZig.Source } else { $zigLocal }
    $env:ZIG_GLOBAL_CACHE_DIR = Join-Path $diretorioSaida 'zig-cache'
    & $zig c++ '-std=c++17' '-Wall' '-Wextra' '-Werror' '-I' $diretorioInclude $arquivoTeste '-o' $executavel
} else {
    $padraoVcVars = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/*/*/VC/Auxiliary/Build/vcvars64.bat'
    $vcvars = Get-ChildItem -Path $padraoVcVars -File -ErrorAction SilentlyContinue | Select-Object -Last 1
    if (-not $vcvars) { throw 'Instale g++, Zig ou Visual Studio Build Tools com C++ e Windows SDK para executar este teste no computador.' }
    $objeto = Join-Path $diretorioSaida 'teste_demonstracao.obj'
    $comando = 'call "{0}" >nul && cl.exe /nologo /std:c++17 /EHsc /W4 /I"{1}" "{2}" /Fo"{3}" /Fe"{4}" /link /INCREMENTAL:NO' -f $vcvars.FullName, $diretorioInclude, $arquivoTeste, $objeto, $executavel
    & $env:ComSpec /d /s /c $comando
}
if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar o teste de demonstracao.' }
& $executavel
if ($LASTEXITCODE -ne 0) { throw 'Falha nos testes da demonstracao.' }

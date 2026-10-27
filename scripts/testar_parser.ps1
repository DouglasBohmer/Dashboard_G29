$ErrorActionPreference = 'Stop'
$raizProjeto = Split-Path -Parent $PSScriptRoot
$compilador = Join-Path $env:USERPROFILE '.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-g++.exe'
if (-not (Test-Path -LiteralPath $compilador)) {
    throw 'Compile um ambiente ESP32 no PlatformIO para instalar o compilador.'
}
$diretorioSaida = Join-Path $raizProjeto '.pio/testes'
New-Item -ItemType Directory -Path $diretorioSaida -Force | Out-Null
$diretorioInclude = Join-Path $raizProjeto 'include'
$arquivoTeste = Join-Path $raizProjeto 'test/teste_parser_f1.cpp'
$arquivoObjeto = Join-Path $diretorioSaida 'teste_parser_f1.o'
& $compilador '-std=c++17' '-Wall' '-Wextra' '-Werror' '-I' $diretorioInclude '-c' $arquivoTeste '-o' $arquivoObjeto
if ($LASTEXITCODE -ne 0) {
    throw 'Falha nas verificacoes do parser de telemetria.'
}
Write-Output '26 verificacoes do parser aprovadas em tempo de compilacao. Nenhuma gravacao no ESP32.'

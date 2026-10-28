$ErrorActionPreference = 'Stop'
$raizProjeto = Split-Path -Parent $PSScriptRoot
$compilador = Join-Path $env:USERPROFILE '.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-g++.exe'
if (-not (Test-Path -LiteralPath $compilador)) {
    throw 'Compile um ambiente ESP32 no PlatformIO para instalar o compilador.'
}
$diretorioSaida = Join-Path $raizProjeto '.pio/testes'
New-Item -ItemType Directory -Path $diretorioSaida -Force | Out-Null
$diretorioInclude = Join-Path $raizProjeto 'include'
$arquivoTeste = Join-Path $raizProjeto 'test/teste_sensor_luminosidade.cpp'
$arquivoObjeto = Join-Path $diretorioSaida 'teste_sensor_luminosidade.o'
& $compilador '-std=c++17' '-Wall' '-Wextra' '-Werror' '-I' $diretorioInclude '-c' $arquivoTeste '-o' $arquivoObjeto
if ($LASTEXITCODE -ne 0) {
    throw 'Falha nas verificacoes do sensor de luminosidade.'
}
Write-Output '22 verificacoes de luminosidade aprovadas em tempo de compilacao. Nenhuma gravacao no ESP32.'

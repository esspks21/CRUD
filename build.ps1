# g++로 CRUD 앱 빌드
param(
    [switch]$Clean
)

Set-Location $PSScriptRoot

if ($Clean -and (Test-Path "crud_app.exe")) {
    Remove-Item "crud_app.exe" -Force
    Write-Host "기존 빌드 파일 삭제 완료" -ForegroundColor Yellow
}

Write-Host "=== CRUD 앱 빌드 중 ===" -ForegroundColor Cyan
g++ -std=c++17 -O2 -Wall -o crud_app.exe main.cpp
if ($LASTEXITCODE -eq 0) {
    Write-Host "빌드 성공: crud_app.exe" -ForegroundColor Green
    Write-Host "실행: .\crud_app.exe" -ForegroundColor Green
} else {
    Write-Host "빌드 실패" -ForegroundColor Red
    exit 1
}

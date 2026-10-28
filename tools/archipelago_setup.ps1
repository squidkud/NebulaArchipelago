# Cree (ou remplace) le fichier archipelago.ini de Nebula en vous posant 3 questions.
# Usage : clic droit sur le fichier > "Executer avec PowerShell", ou dans PowerShell : .\tools\archipelago_setup.ps1
$ErrorActionPreference = "Stop"
$folder = Join-Path $env:LOCALAPPDATA "Nebula"
New-Item -ItemType Directory -Force -Path $folder | Out-Null
$path = Join-Path $folder "archipelago.ini"

Write-Host ""
Write-Host "Configuration d'Archipelago pour Nebula"
Write-Host "Le port change a chaque salle : copiez l'adresse complete depuis la page de la salle (ex. archipelago.gg:58396)."
Write-Host "Pour un serveur lance sur ce PC, tapez : localhost:38281"
Write-Host ""
$server = (Read-Host "Adresse du serveur").Trim()
$slot = (Read-Host "Nom du joueur (le name: de votre YAML)").Trim()
$password = Read-Host "Mot de passe de la salle (laissez vide s'il n'y en a pas)"

if ($server -eq "" -or $slot -eq "") {
    Write-Host "L'adresse et le nom du joueur sont obligatoires. Rien n'a ete ecrit."
    exit 1
}

$text = "server=$server`r`nslot=$slot`r`npassword=$password`r`n"
# UTF-8 sans BOM, avec l'extension .ini exacte.
[System.IO.File]::WriteAllText($path, $text, (New-Object System.Text.UTF8Encoding($false)))
if (Test-Path ($path + ".txt")) { Remove-Item ($path + ".txt") -Force }

Write-Host ""
Write-Host "Ecrit : $path"
Write-Host $text
Write-Host "Relancez Nebula. Pour jouer sans Archipelago, supprimez ou renommez ce fichier."

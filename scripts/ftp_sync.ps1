param(
    [string]$VitaIp = "192.168.1.88",
    [int]$Port = 1337,
    [switch]$UploadVPK = $true,
    [switch]$DownloadLogs = $true,
    [switch]$DownloadDumps = $true
)

$client = New-Object System.Net.WebClient

Write-Host "========================================"
Write-Host "PSVitaman FTP Sync with PS Vita (${VitaIp}:${Port})"
Write-Host "========================================"

# 1. Download logs & config
if ($DownloadLogs) {
    Write-Host "`n[1/3] Downloading logs and configuration..."
    try {
        $client.DownloadFile("ftp://$($VitaIp):$Port/ux0:/data/psvitaman/psvitaman.log", "build/psvitaman_device.log")
        Write-Host "  -> psvitaman.log saved to build/psvitaman_device.log"
    } catch {
        Write-Warning "  -> Could not download psvitaman.log: $_"
    }

    try {
        $client.DownloadFile("ftp://$($VitaIp):$Port/ux0:/data/psvitaman/config.ini", "build/config_device.ini")
        Write-Host "  -> config.ini saved to build/config_device.ini"
    } catch {
        Write-Warning "  -> Could not download config.ini: $_"
    }
}

# 2. Dynamically discover and download all crash dumps
if ($DownloadDumps) {
    Write-Host "`n[2/3] Checking for crash dumps in ux0:data/ ..."
    try {
        $req = [System.Net.FtpWebRequest]::Create("ftp://$($VitaIp):$Port/ux0:/data/")
        $req.Method = [System.Net.WebRequestMethods+Ftp]::ListDirectoryDetails
        $req.Timeout = 7000
        $res = $req.GetResponse()
        $reader = New-Object System.IO.StreamReader($res.GetResponseStream())
        $dirListing = $reader.ReadToEnd()
        $reader.Close()
        $res.Close()

        $files = @()
        foreach ($line in ($dirListing -split "[\r\n]+")) {
            if ($line -match "(psp2core-[^\s]+(\.tmp)?)") {
                $files += $matches[1]
            }
        }

        if ($files.Count -eq 0) {
            Write-Host "  -> No crash dumps found in ux0:data/."
        } else {
            Write-Host "  -> Found $($files.Count) crash dump(s) on Vita:"
            foreach ($file in $files) {
                $file = $file.Trim()
                if ([string]::IsNullOrWhiteSpace($file)) { continue }
                $localTarget = "build/$file"
                Write-Host "     Downloading $file ..."
                try {
                    $client.DownloadFile("ftp://$($VitaIp):$Port/ux0:/data/$file", $localTarget)
                    $size = (Get-Item $localTarget).Length
                    Write-Host "     Saved $localTarget ($size bytes)"
                } catch {
                    Write-Warning "     Failed to download ${file}: $_"
                }
            }
        }
    } catch {
        Write-Warning "  -> Failed to list ux0:data/: $_"
    }
}

# 3. Upload VPK
if ($UploadVPK) {
    Write-Host "`n[3/3] Uploading VPK to PS Vita..."
    $vpkPath = "build/PSVitaman.vpk"
    if (Test-Path $vpkPath) {
        $vpkItem = Get-Item $vpkPath
        Write-Host "  -> Found $vpkPath ($($vpkItem.Length) bytes, $($vpkItem.LastWriteTime))"
        Write-Host "  -> Uploading to ftp://$($VitaIp):$Port/ux0:/data/PSVitaman.vpk ..."
        try {
            $client.UploadFile("ftp://$($VitaIp):$Port/ux0:/data/PSVitaman.vpk", "STOR", $vpkItem.FullName)
            Write-Host "  -> PSVitaman.vpk uploaded successfully to ux0:data/PSVitaman.vpk!"
        } catch {
            Write-Error "  -> Failed to upload VPK: $_"
        }
    } else {
        Write-Warning "  -> build/PSVitaman.vpk does not exist locally. Skipping upload."
    }
}

Write-Host "`nSync complete!"

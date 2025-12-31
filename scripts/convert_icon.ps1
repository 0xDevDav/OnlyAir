Add-Type -AssemblyName System.Drawing

$pngPath = 'C:\Users\DevDav\Desktop\OnlyCaller\icon.png'
$icoPath = 'C:\Users\DevDav\Desktop\OnlyCaller\resources\OnlyAir.ico'

# Create resources folder if not exists
if (-not (Test-Path 'C:\Users\DevDav\Desktop\OnlyCaller\resources')) {
    New-Item -ItemType Directory -Path 'C:\Users\DevDav\Desktop\OnlyCaller\resources' -Force
}

# Load original image
$original = [System.Drawing.Image]::FromFile($pngPath)

# Create icon sizes
$sizes = @(16, 32, 48, 256)
$images = @()

foreach ($size in $sizes) {
    $bmp = New-Object System.Drawing.Bitmap($size, $size)
    $graphics = [System.Drawing.Graphics]::FromImage($bmp)
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $graphics.DrawImage($original, 0, 0, $size, $size)
    $graphics.Dispose()
    $images += $bmp
}

# Write ICO file manually
$fs = [System.IO.File]::Create($icoPath)
$bw = New-Object System.IO.BinaryWriter($fs)

# ICO header
$bw.Write([Int16]0)      # Reserved
$bw.Write([Int16]1)      # Type (1 = ICO)
$bw.Write([Int16]$sizes.Count)  # Number of images

# Calculate offsets
$headerSize = 6 + (16 * $sizes.Count)
$offset = $headerSize

# Write directory entries and collect PNG data
$pngDataList = @()
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $ms = New-Object System.IO.MemoryStream
    $images[$i].Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngData = $ms.ToArray()
    $pngDataList += ,$pngData
    $ms.Dispose()

    $size = $sizes[$i]
    if ($size -ge 256) { $widthByte = 0 } else { $widthByte = $size }
    if ($size -ge 256) { $heightByte = 0 } else { $heightByte = $size }

    $bw.Write([Byte]$widthByte)   # Width
    $bw.Write([Byte]$heightByte)  # Height
    $bw.Write([Byte]0)    # Color palette
    $bw.Write([Byte]0)    # Reserved
    $bw.Write([Int16]1)   # Color planes
    $bw.Write([Int16]32)  # Bits per pixel
    $bw.Write([Int32]$pngData.Length)  # Size of image data
    $bw.Write([Int32]$offset)          # Offset to image data
    $offset += $pngData.Length
}

# Write image data
foreach ($pngData in $pngDataList) {
    $bw.Write($pngData)
}

$bw.Close()
$fs.Close()

foreach ($img in $images) { $img.Dispose() }
$original.Dispose()

Write-Host "Icon created: $icoPath"

# 调用Windows系统定位服务获取精确地理位置（街道级别）
# 使用 WinRT Geolocation API，精确到街道级别

# 加载 System.Runtime.WindowsRuntime.dll
$winrtDllPath = "C:\Windows\Microsoft.NET\Assembly\GAC_MSIL\System.Runtime.WindowsRuntime\v4.0_4.0.0.0__b77a5c561934e089\System.Runtime.WindowsRuntime.dll"
if (-not (Test-Path $winrtDllPath)) {
    $winrtDllPath = (Get-ChildItem "C:\Windows\Microsoft.NET\Assembly\GAC_MSIL\System.Runtime.WindowsRuntime" -Recurse -Filter "System.Runtime.WindowsRuntime.dll" -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName)
}
if ($winrtDllPath -and (Test-Path $winrtDllPath)) {
    [System.Reflection.Assembly]::LoadFrom($winrtDllPath) | Out-Null
}

# 加载 WinRT Geolocation 类型
[Windows.Devices.Geolocation.Geolocator, Windows.Devices.Geolocation, ContentType = WindowsRuntime] | Out-Null
[Windows.Devices.Geolocation.Geoposition, Windows.Devices.Geolocation, ContentType = WindowsRuntime] | Out-Null

# 获取 AsTask 泛型方法
$asTaskGeneric = ([System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object { $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1' })[0]

try {
    # 创建定位器
    $locator = New-Object Windows.Devices.Geolocation.Geolocator

    # 使用反射调用 GetGeopositionAsync()
    $methods = $locator.GetType().GetMethods() | Where-Object { $_.Name -eq 'GetGeopositionAsync' }
    $method = $null
    foreach ($m in $methods) {
        if ($m.GetParameters().Count -eq 0) { $method = $m; break }
    }

    if ($method -eq $null) {
        Write-Output "ERROR:No GetGeopositionAsync method"
        return
    }

    $operation = $method.Invoke($locator, $null)
    if ($operation -eq $null) {
        Write-Output "ERROR:GetGeopositionAsync returned null"
        return
    }

    # 使用泛型反射调用 AsTask<Geoposition>
    $geopositionType = [Windows.Devices.Geolocation.Geoposition]
    $asTaskMethod = $asTaskGeneric.MakeGenericMethod($geopositionType)
    $task = $asTaskMethod.Invoke($null, @($operation))

    if ($task -ne $null -and $task.Wait(15000)) {
        $position = $task.Result
        $coord = $position.Coordinate.Point.Position
        Write-Output "RESULT:$($coord.Longitude),$($coord.Latitude)"
    } else {
        Write-Output "ERROR:TIMEOUT"
    }
} catch {
    Write-Output "ERROR:$($_.Exception.Message)"
}

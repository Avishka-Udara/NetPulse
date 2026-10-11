param(
    [Parameter(Mandatory=$true)][int]$ProcessId,
    [ValidateRange(1,1440)][int]$Minutes=10,
    [string]$OutputPath='resource-samples.csv',
    [switch]$IncludeCollector
)
$ErrorActionPreference='Stop'
$initial=Get-Process -Id $ProcessId
if ($initial.ProcessName -notin 'NetPulse','test_capture') { throw 'Select a NetPulse or test_capture process.' }
$started=$initial.StartTime
$previousCpu=@{}
$previousCpu[$initial.Id]=$initial.TotalProcessorTime.TotalSeconds
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$rows=[Collections.Generic.List[object]]::new()
try {
    while ($watch.Elapsed.TotalSeconds -lt $Minutes*60) {
        Start-Sleep -Seconds 5
        $sample=Get-Process -Id $ProcessId
        if ($sample.StartTime -ne $started) { throw 'Process identity changed; stopping measurement.' }
        $time=$watch.Elapsed.TotalSeconds
        $processes=@($sample)
        if ($IncludeCollector) {
            $helpers=Get-CimInstance Win32_Process -Filter "ParentProcessId=$ProcessId" | Where-Object { $_.Name -eq "$($initial.ProcessName).exe" -and $_.CommandLine -match ' --collector ' }
            foreach ($helper in $helpers) { $child=Get-Process -Id $helper.ProcessId -ErrorAction SilentlyContinue;if ($child) { $processes+= $child } }
        }
        $cpuDelta=0.0;$working=0L;$private=0L;$handles=0;$threads=0
        foreach ($process in $processes) {
            $cpu=$process.TotalProcessorTime.TotalSeconds
            if ($previousCpu.ContainsKey($process.Id)) { $cpuDelta += [math]::Max(0,$cpu-$previousCpu[$process.Id]) }
            $previousCpu[$process.Id]=$cpu
            $working+=$process.WorkingSet64;$private+=$process.PrivateMemorySize64;$handles+=$process.HandleCount;$threads+=$process.Threads.Count
        }
        $rows.Add([pscustomobject]@{
            Timestamp=(Get-Date).ToString('o')
            ElapsedSeconds=[math]::Round($time,2)
            CpuOneCorePercent=[math]::Round(100*$cpuDelta/($time-$lastTime),4)
            WorkingSetMiB=[math]::Round($working/1MB,3)
            PrivateMiB=[math]::Round($private/1MB,3)
            Processes=$processes.Count
            Handles=$handles
            Threads=$threads
        })
        $lastTime=$time
    }
} finally {
    if ($rows.Count) { $rows | Export-Csv -LiteralPath $OutputPath -NoTypeInformation }
}
Write-Output "Saved $($rows.Count) samples. This script does not start, stop, or change NetPulse."

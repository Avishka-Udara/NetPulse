param(
    [Parameter(Mandatory=$true)][int]$ProcessId,
    [ValidateRange(1,1440)][int]$Minutes=10,
    [string]$OutputPath='resource-samples.csv'
)
$ErrorActionPreference='Stop'
$initial=Get-Process -Id $ProcessId
if ($initial.ProcessName -ne 'NetPulse') { throw 'Select a NetPulse process.' }
$started=$initial.StartTime
$lastCpu=$initial.TotalProcessorTime.TotalSeconds
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$rows=[Collections.Generic.List[object]]::new()
try {
    while ($watch.Elapsed.TotalSeconds -lt $Minutes*60) {
        Start-Sleep -Seconds 5
        $sample=Get-Process -Id $ProcessId
        if ($sample.StartTime -ne $started) { throw 'Process identity changed; stopping measurement.' }
        $time=$watch.Elapsed.TotalSeconds
        $cpu=$sample.TotalProcessorTime.TotalSeconds
        $rows.Add([pscustomobject]@{
            Timestamp=(Get-Date).ToString('o')
            ElapsedSeconds=[math]::Round($time,2)
            CpuOneCorePercent=[math]::Round(100*($cpu-$lastCpu)/($time-$lastTime),4)
            WorkingSetMiB=[math]::Round($sample.WorkingSet64/1MB,3)
            PrivateMiB=[math]::Round($sample.PrivateMemorySize64/1MB,3)
            Handles=$sample.HandleCount
            Threads=$sample.Threads.Count
        })
        $lastCpu=$cpu;$lastTime=$time
    }
} finally {
    if ($rows.Count) { $rows | Export-Csv -LiteralPath $OutputPath -NoTypeInformation }
}
Write-Output "Saved $($rows.Count) samples. This script does not start, stop, or change NetPulse."

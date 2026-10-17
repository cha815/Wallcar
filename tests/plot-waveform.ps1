param([string]$InputCsv='docs/evidence/tuning-arm-fault.csv',[string]$OutputPng='docs/evidence/motor-feedback.png')
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Windows.Forms.DataVisualization
Add-Type -AssemblyName System.Drawing
$waveRows=@(Import-Csv -LiteralPath $InputCsv)
if($waveRows.Count -eq 0) { throw 'No waveform samples' }
# DAPLink can retain one old startup packet. Keep the final contiguous segment.
$startRow=0
for($i=1;$i -lt $waveRows.Count;$i++) {
    if(([double]$waveRows[$i].time_s-[double]$waveRows[$i-1].time_s) -gt 0.5) { $startRow=$i }
}
$waveRows=@($waveRows[$startRow..($waveRows.Count-1)])
$chart=[System.Windows.Forms.DataVisualization.Charting.Chart]::new()
try {
    $chart.Width=1400;$chart.Height=900
    $chart.BackColor=[System.Drawing.Color]::White
    [void]$chart.Titles.Add('ID1 MIT feedback - actual recorded samples')
    $groups=@(
        @{Name='position';Unit='rad';Fields=@('target_pos','actual_pos')},
        @{Name='velocity';Unit='rad/s';Fields=@('target_vel','actual_vel')},
        @{Name='torque';Unit='Nm';Fields=@('actual_torque')},
        @{Name='status';Unit='state / fault';Fields=@('state','fault','enabled','stop_confirmed')}
    )
    $legend=[System.Windows.Forms.DataVisualization.Charting.Legend]::new('Legend')
    $legend.Docking='Right';$chart.Legends.Add($legend)
    $firstTime=[double]$waveRows[0].time_s
    for($g=0;$g -lt $groups.Count;$g++) {
        $group=$groups[$g]
        $area=[System.Windows.Forms.DataVisualization.Charting.ChartArea]::new($group.Name)
        $area.Position.Auto=$false;$area.Position.X=3;$area.Position.Y=7+22*$g;$area.Position.Width=78;$area.Position.Height=21
        $area.AxisX.Title='Time (s)';$area.AxisY.Title=$group.Unit
        $area.AxisX.LabelStyle.Format='0.00';$area.AxisY.LabelStyle.Format='0.###'
        $area.AxisX.MajorGrid.LineColor=[System.Drawing.Color]::LightGray
        $area.AxisY.MajorGrid.LineColor=[System.Drawing.Color]::LightGray
        $chart.ChartAreas.Add($area)
        foreach($field in $group.Fields) {
            $series=[System.Windows.Forms.DataVisualization.Charting.Series]::new($field)
            $series.ChartType='Line';$series.BorderWidth=2;$series.ChartArea=$group.Name
            $chart.Series.Add($series)
            foreach($row in $waveRows) { [void]$series.Points.AddXY(([double]$row.time_s-$firstTime),[double]$row.$field) }
        }
    }
    $chart.SaveImage((Join-Path (Get-Location) $OutputPng),'Png')
} finally { $chart.Dispose() }
Write-Output "Saved $OutputPng from $($waveRows.Count) real samples"

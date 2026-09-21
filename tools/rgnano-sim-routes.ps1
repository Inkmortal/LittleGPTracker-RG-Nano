function Get-RGNanoSimRoutes {
  $routes = @{}

  $routes["combo.r.up"] = @("down n", "press u 80", "up n")
  $routes["combo.r.down"] = @("down n", "press d 80", "up n")
  $routes["combo.r.left"] = @("down n", "press l 80", "up n")
  $routes["combo.r.right"] = @("down n", "press r 80", "up n")
  $routes["combo.r.start"] = @("down n", "press s 80", "up n")

  $routes["combo.l.up"] = @("down m", "press u 80", "up m")
  $routes["combo.l.down"] = @("down m", "press d 80", "up m")
  $routes["combo.l.left"] = @("down m", "press l 80", "up m")
  $routes["combo.l.right"] = @("down m", "press r 80", "up m")
  $routes["combo.l.start"] = @("down m", "press s 80", "up m")

  $routes["combo.a.up"] = @("down a", "press u 80", "up a")
  $routes["combo.a.down"] = @("down a", "press d 80", "up a")
  $routes["combo.a.left"] = @("down a", "press l 80", "up a")
  $routes["combo.a.right"] = @("down a", "press r 80", "up a")

  $routes["combo.b.up"] = @("down b", "press u 80", "up b")
  $routes["combo.b.down"] = @("down b", "press d 80", "up b")
  $routes["combo.b.left"] = @("down b", "press l 80", "up b")
  $routes["combo.b.right"] = @("down b", "press r 80", "up b")

  $routes["boot.new_project_random"] = @(
    "wait 500",
    "press r 80",
    "press a 80",
    "wait 200",
    "press d 80",
    "press a 80",
    "press r 80",
    "press a 80",
    "wait 1000"
  )

  $routes["project.to_song"] = $routes["combo.r.down"]
  $routes["song.to_project"] = $routes["combo.r.up"]
  $routes["song.to_mixer"] = $routes["combo.r.down"]
  $routes["mixer.to_song"] = $routes["combo.r.up"]
  $routes["song.to_chain"] = $routes["combo.r.right"]
  $routes["chain.to_song"] = $routes["combo.r.left"]
  $routes["chain.to_phrase"] = $routes["combo.r.right"]
  $routes["phrase.to_chain"] = $routes["combo.r.left"]
  $routes["phrase.to_instrument"] = $routes["combo.r.right"]
  $routes["phrase.to_table"] = $routes["combo.r.down"]
  $routes["phrase.to_groove"] = $routes["combo.r.up"]
  $routes["groove.to_phrase"] = $routes["combo.r.down"]
  $routes["instrument.to_phrase"] = $routes["combo.r.left"]
  $routes["instrument.to_table"] = $routes["combo.r.down"]
  $routes["table.to_parent"] = $routes["combo.r.up"]
  $routes["table.to_instrument_table"] = $routes["combo.r.right"]
  $routes["instrument_table.to_table"] = $routes["combo.r.left"]
  $routes["instrument.make_sample"] = @("press u 80", "down a", "press l 80", "up a", "wait 120", "press d 80")
  $routes["instrument.open_sample_import"] = @("press k 80", "wait 300")
  $routes["sample_import.to_import"] = @("press r 80")
  $routes["sample_import.to_first_file"] = @("press d 80", "wait 120")
  $routes["sample_import.import_selected"] = @("press r 80", "press a 80", "wait 500")
  $routes["sample_import.quick_import_selected"] = @("down s", "press r 80", "up s", "wait 500")
  $routes["sample_import.exit"] = @("press r 80", "press r 80", "press r 80", "press a 80", "wait 200")

  return $routes
}

function Expand-RGNanoSimScript {
  param(
    [Parameter(Mandatory = $true)][string]$ScriptPath,
    [Parameter(Mandatory = $true)][string]$OutputPath
  )

  $routes = Get-RGNanoSimRoutes
  $expanded = New-Object System.Collections.Generic.List[string]
  $lineNumber = 0

  foreach ($line in Get-Content -LiteralPath $ScriptPath) {
    $lineNumber += 1
    $trimmed = $line.Trim()

    if ($trimmed -match '^route\s+([A-Za-z0-9_.-]+)\s*$') {
      $routeName = $Matches[1]
      if (-not $routes.ContainsKey($routeName)) {
        throw "Unknown RG Nano simulator route '$routeName' at ${ScriptPath}:$lineNumber"
      }
      $expanded.Add("# route $routeName")
      foreach ($command in $routes[$routeName]) {
        $expanded.Add($command)
      }
      continue
    }

    $expanded.Add($line)
  }

  Set-Content -LiteralPath $OutputPath -Value $expanded -Encoding ASCII
}

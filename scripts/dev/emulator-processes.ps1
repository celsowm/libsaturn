Get-Process | Where-Object { $_.ProcessName -match 'kron|medna|yaba' } | Format-Table Id, MainWindowTitle, ProcessName

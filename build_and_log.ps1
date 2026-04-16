$env:PATH = "C:\msys64\home\celso\saturn-tools\bin;" + $env:PATH
$env:PYTHON = "C:\Python\python.exe"
cd C:\Users\celso\Documents\projetos\libsaturn-1
make EXAMPLE=vdp2_infinite_plan IP_PROFILE=current IP_TEMPLATE_KIND=yaul all 2>&1 | Out-File -Encoding Ascii build_log.txt
Get-Content build_log.txt

param([string]$Text = "Hiii! Message from Ahmed!")
Add-Type -AssemblyName System.Speech
$synth = New-Object System.Speech.Synthesis.SpeechSynthesizer
$synth.Speak($Text)

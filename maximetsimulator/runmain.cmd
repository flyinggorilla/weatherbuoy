ampy -p com8 put main.py main.py
rem miniterm com8 115200
echo quit miniterm with [Ctrl-T] then [Q]
python -m serial.tools.miniterm com8 115200

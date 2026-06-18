@echo off
setlocal
python "%~dp0build_champion_game_data.py" --root "%~dp0..\.."
exit /b %ERRORLEVEL%

$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$testBuild = Join-Path $taskRoot 'tests/build'
New-Item -ItemType Directory -Path $testBuild -Force | Out-Null
Push-Location $taskRoot
try {
    & 'D:\D-Tool\cliontoolchain\mingw64\bin\gcc.exe' -std=c11 -Wall -Wextra -Werror -I. tests/test_motor_instances.c First/device/qzb_motor.c First/service/can_service.c First/service/bench_link.c First/service/motor_registry.c First/component/motor_codec.c First/component/mw_canQZB.c First/component/can_frame.c -o tests/build/test_motor_instances.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$testBuild/test_motor_instances.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & 'D:\D-Tool\cliontoolchain\mingw64\bin\gcc.exe' -std=c11 -Wall -Wextra -Werror -IFirst -IFirst/component tests/test_control_pid.c First/component/control_pid.c -o tests/build/test_control_pid.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$testBuild/test_control_pid.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & 'D:\D-Tool\cliontoolchain\mingw64\bin\gcc.exe' -std=c11 -Wall -Wextra -Werror -DMOTOR_RO_PACKET_TEST -IFirst/component -IFirst/bsp -IFirst/device tests/test_core.c First/component/can_frame.c First/device/motor_ro.c -o tests/build/test_core.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$testBuild/test_core.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & 'D:\D-Tool\cliontoolchain\mingw64\bin\gcc.exe' -std=c11 -Wall -Wextra -Werror -Itests/mocks -IFirst/component -IFirst/bsp tests/test_bsp.c First/component/can_frame.c First/bsp/bsp_fdcan.c -o tests/build/test_bsp.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$testBuild/test_bsp.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & 'D:\D-Tool\cliontoolchain\mingw64\bin\gcc.exe' -std=c11 -Wall -Wextra -Werror -IFirst/component -IFirst/bsp -IFirst/device -IFirst/service tests/test_diag.c First/service/fdcan_diag.c -o tests/build/test_diag.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$testBuild/test_diag.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & 'D:\D-Tool\cliontoolchain\mingw64\bin\gcc.exe' -std=c11 -Wall -Wextra -Werror -IFirst/component tests/test_motor_codec.c First/component/motor_codec.c First/component/mw_canQZB.c -o tests/build/test_motor_codec.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$testBuild/test_motor_codec.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & 'D:\D-Tool\cliontoolchain\mingw64\bin\gcc.exe' -std=c11 -Wall -Wextra -Werror -IFirst/service tests/test_tuning_link.c First/service/tuning_link.c -o tests/build/test_tuning_link.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$testBuild/test_tuning_link.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & 'D:\D-Tool\cliontoolchain\mingw64\bin\gcc.exe' -std=c11 -Wall -Wextra -Werror -IFirst -IFirst/app -IFirst/device -IFirst/bsp -IFirst/component -IFirst/service tests/test_motor_tune.c First/app/motor_tune.c First/component/control_pid.c First/component/motor_codec.c First/component/mw_canQZB.c -o tests/build/test_motor_tune.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$testBuild/test_motor_tune.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & 'D:\D-Tool\cliontoolchain\mingw64\bin\gcc.exe' -std=c11 -Wall -Wextra -Werror -IFirst -IFirst/device -IFirst/bsp -IFirst/component tests/test_motor_device.c First/device/motor.c First/device/motor_ro.c First/component/motor_codec.c First/component/can_frame.c First/component/mw_canQZB.c -o tests/build/test_motor_device.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$testBuild/test_motor_device.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally { Pop-Location }

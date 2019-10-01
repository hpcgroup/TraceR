#!/bin/bash

scriptPath="$(cd "$(dirname "$0")"; pwd -P)"
basePath="$(cd ${scriptPath}/../..; pwd -P)"
testPath="$(cd ${scriptPath}/../; pwd -P)"
examplePath="$basePath/examples"

exampleName=$1
confName=$2

echo scriptPath=$scriptPath
echo basePath=$basePath
echo examplePath=$examplePath
echo exampleName=$exampleName
echo confName=$confName

pushd ${examplePath}/${exampleName}
mpirun -np 2 ${basePath}/install-test/bin/traceR --lp-io-dir=${testPath}/output/test-output-${confName} --sync=3 -- ../conf/${confName}.conf tracer_config | sed -n '/START (PARALLEL|SEQUENTIAL)* SIMULATION/,/END SIMULATION/p' | tail -n +2 | head -n -3 > std.out
rv=$?
cat std.out
mv std.out ${testPath}/output/test-output-${confName}/std.out
popd

${scriptPath}/check-files-match.sh ${testPath}/regression/test-output-${confName}
rv2=$?

if [[ $rv == 0 && $rv2 == 0 ]]; then true; else false; fi

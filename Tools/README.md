# Tools

원자: AuthoringTool, ValidationEvidence, CookPipeline, PackagePatchDeploy

Tools는 하나의 원자가 아니다. 사람이 원본을 만드는 도구, 검증 증거, cook pipeline, release pipeline을 구분한다.

현재 의미:
- `SimLab/`: ValidationEvidence
- `EldenAssetPipeline/`: CookPipeline
- `WintersAssetConverter/`: CookPipeline
- `ChampionData/`: AuthoringTool 또는 GameDesignSource. 확인 필요
- `Bin/`, `Intermediate/`: GeneratedOutput
- `External/`: ExternalDependency

추가 후보:
- `Validation/`: `SimLab/`만으로 QA gate를 표현할 수 없을 때만 추가
- `Cook/`: 기존 pipeline과 converter가 cook 계약을 담을 수 없을 때만 추가
- `Release/`: package, patch, deploy manifest 계약이 생길 때만 추가

구조 규칙:
- 기존 `Tools/` 폴더명과 구조를 유지한다.
- tool output을 source처럼 직접 편집하지 않는다.
- 새 tool executable이 생길 때만 build graph 수정을 검토한다.

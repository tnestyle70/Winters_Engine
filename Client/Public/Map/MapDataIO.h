#pragma once
#include "Defines.h"
#include "WintersTypes.h"
#include "Map/MapDataFormats.h"

// CMapDataIO — Stage .dat 파일 I/O orchestrator
//
// v3 설계:
//   - Manager 3개(Structure/Jungle/Minion)가 싱글턴 → 포인터 전달 불필요
//   - 내부에서 CXxx_Manager::Get()->Save_ToFile / Load_FromFile 호출
//
// API 설계 노트 (사용자 질문 응답):
//   - CMapDataIO() = delete / ~CMapDataIO() = delete
//     → 인스턴스 생성 원천 차단. static util 클래스 강제 패턴.
//     → public + delete 이유: 에러 메시지 명확성 ("deleted function" vs "private member")
//   - Get_StagePathW:
//     → EXE 디렉토리 기준 "Data\\StageN.dat" 절대경로 조립
//     → WintersResolveContentPath 는 "파일 존재 시에만 반환" 이라 Save(신규 생성)에 못 씀
//     → GetModuleFileNameW 로 EXE 경로 추출 후 상대 경로 덧붙임
//
class CMapDataIO final
{
public:
    CMapDataIO()  = delete;
    ~CMapDataIO() = delete;

    // 현재 싱글턴 Manager 상태를 파일에 기록
    static HRESULT Save_Stage(const wchar_t* pAbsPath);

    // 파일에서 싱글턴 Manager 로 로드 (실패 시 Manager 들은 Clear 된 상태)
    static HRESULT Load_Stage(const wchar_t* pAbsPath);

    // EXE 디렉토리 기준 "Data\\StageN.dat" 경로 조립
    static bool Get_StagePathW(i32_t stageIndex, wchar_t* pOutBuf, u32_t capacity);
    static bool GetNavGridPathW(i32_t stageIndex, wchar_t* pOutBuf, u32_t capacity);
};

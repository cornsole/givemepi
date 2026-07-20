# PiEngine 개발 자동화 도입 계획

목표:

코드 작성 자체보다 **검증·관리·반복 작업을 자동화**해서 개발 속도를 올리는 것.

현재 `AGENT.md` 규칙은 그대로 유지.

---

# 1단계 — 로컬 개발 자동화

## 1. CTest 적용 (우선순위 ★★★★★)

현재:

```bash
./build/gmp-test
./build/binary-test
./build/memory-test
```

이렇게 하나씩 실행 중.

변경 후:

```bash
ctest
```

한 번 실행하면:

```
GMP 테스트
Binary 테스트
Memory 테스트
Scheduler 테스트
```

전부 자동 실행.

효과:

* 테스트 빼먹는 문제 방지
* CI 붙이기 쉬움
* PR 검증 자동화 기반

---

# 2. 커밋 전 자동 검사 (pre-commit)

흐름:

```
git commit
      |
      v
자동 검사
      |
      + 코드 스타일 확인
      + 빌드 확인
      + 테스트 실행
      |
      v
커밋 생성
```

검사 내용:

* clang-format
* CMake 빌드
* 일부 테스트

효과:

깨진 코드가 커밋되는 것 방지.

---

# 3. GitHub Actions CI (가장 중요)

PR 올리면 자동 실행.

흐름:

```
코드 업로드
      |
      v
GitHub 서버
      |
      + 의존성 설치
      + CMake 생성
      + 빌드
      + 테스트
      |
      v
성공 / 실패 표시
```

효과:

네가 직접:

```
cmake
build
테스트
```

안 눌러도 됨.

---

여기까지 됬음

-----------

# 4. 코드 스타일 자동 검사

도구:

`clang-format`

하는 일:

띄어쓰기, 줄바꿈, 스타일 통일.

예:

누가:

```cpp
int x=10;
```

작성하면

자동으로:

```cpp
int x = 10;
```

형태 유지.

---

# 5. C++ 정적 분석

도구:

`clang-tidy`

검사:

* 위험한 코드
* 메모리 문제
* 이상한 패턴

예:

찾아줌:

```
사용 후 이동(move) 문제
널 포인터 위험
사용하지 않는 코드
```

---

# 6. PR 자동 리뷰

학생 라이선스로 활용 가능.

예:

PR 올림:

```
BinarySplitter 추가
```

자동 리뷰:

```
테스트 부족
경계값 확인 필요
```

같은 피드백.

사람 리뷰 보조.

---

# 7. 문서 규칙 검사 (PiEngine에 중요)

네 AGENT.md 규칙 기반.

PR 완료 조건:

확인:

```
CHECKLIST.md 변경됨?
CHANGELOG.md 추가됨?
ROADMAP 변경 필요?
DECISIONS 필요?
```

없으면 CI 실패.

효과:

문서 누락 방지.

---

# 8. Benchmark 자동화 (중요)

이 프로젝트 목표가 성능이라 필요.

자동 측정:

```
GMP 연산 속도
Binary merge 속도
메모리 사용량
Scheduler 성능
```

예:

```
PR-0012

Binary merge
10ms

PR-0013

Binary merge
8ms
```

비교 가능.

---

# 9. Dependabot

라이브러리 업데이트 자동 관리.

예:

```
toml++ 새 버전 발견
```

자동 PR 생성.

---

# 학생 라이선스로 쓸만한 것

## GitHub Copilot

추천:

* PR 리뷰
* 테스트 보조
* 코드 설명

---

## GitHub Actions

추천:

* 빌드 서버
* 테스트 서버
* 벤치마크 서버

---

# 적용 순서

내 추천:

```
1. CTest 추가
        ↓
2. GitHub Actions 빌드/테스트
        ↓
3. clang-format
        ↓
4. clang-tidy
        ↓
5. 문서 검사 자동화
        ↓
6. Benchmark 자동화
        ↓
7. PR 리뷰 자동화
        ↓
8. Dependabot
```

---

현재 상태 보면:

* CMake 있음
* 테스트 exe 있음
* 문서 규칙 있음
* PR 규칙 있음

이라 **자동화 넣기 딱 좋은 시점**임.

다음 PR 시작 전에 이거 먼저 넣으면 이후 PR-0013부터는 거의:

```
수정
↓
push
↓
자동 검사
↓
결과 확인
```

흐름으로 갈 수 있음.
